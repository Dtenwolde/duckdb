import argparse
import re
import sys
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from grammar_types import load_grammar_types, load_matcher_rule_overrides
from inline_grammar import parse_peg_grammar
from transformer_plan import (
    ChoiceNode,
    FunctionCallNode,
    LiteralNode,
    ListMacroNode,
    OptionalNode,
    ParensNode,
    ReferenceNode,
    RepeatNode,
    SequenceNode,
    is_pure_reference_choice,
    rule_to_ast,
    to_snake_case,
)


scripts_dir = Path(__file__).parent.parent
src_dir = scripts_dir.parent / "src"
peg_dir = src_dir / "parser" / "peg"
statements_dir = peg_dir / "grammar" / "statements"
type_dir = scripts_dir / "parser"
transformer_header = src_dir / "include" / "duckdb" / "parser" / "peg" / "transformer" / "peg_transformer.hpp"
transformer_source = peg_dir / "transformer" / "transform_generated.cpp"
transformer_trampoline_source = peg_dir / "transformer" / "transform_generated_trampoline.cpp"
transformer_dir = peg_dir / "transformer"

TRAMPOLINE_GRAMMARS = [
    "alter.gram",
    "analyze.gram",
    "attach.gram",
    "call.gram",
    "checkpoint.gram",
    "comment.gram",
    "common.gram",
    "connect.gram",
    "copy.gram",
    "create_index.gram",
    "create_macro.gram",
    "create_schema.gram",
    "create_secret.gram",
    "create_sequence.gram",
    "create_table.gram",
    "create_trigger.gram",
    "create_type.gram",
    "create_view.gram",
    "deallocate.gram",
    "delete.gram",
    "describe.gram",
    "detach.gram",
    "drop.gram",
    "execute.gram",
    "explain.gram",
    "export.gram",
    "expression.gram",
    "insert.gram",
    "load.gram",
    "merge_into.gram",
    "pivot.gram",
    "pragma.gram",
    "prepare.gram",
    "select.gram",
    "set.gram",
    "transaction.gram",
    "update.gram",
    "use.gram",
    "vacuum.gram",
]
TRAMPOLINE_HELPER_RULE_SOURCES = {}
TRAMPOLINE_EXCLUDED_RULES = {
    "Program",
}


@dataclass
class StackChild:
    parse_expr: str
    rule_name: str
    slot_idx: int


@dataclass
class OptionalStackChild:
    parse_expr: str
    rule_name: str
    slot_idx: int
    parens: bool = False
    sequence_child_idx: object = None


@dataclass
class RepeatStackChild:
    parse_expr: str
    rule_name: str
    var_name: str
    cpp_type: str
    by_value: bool
    optional: bool


@dataclass
class ListStackChild:
    parse_expr: str
    rule_name: str
    var_name: str
    cpp_type: str
    by_value: bool
    mode: str


@dataclass
class FinalizeArg:
    var_name: str
    cpp_type: str
    by_value: bool
    lines: list[str]


@dataclass
class PrimitiveRule:
    kind: str


IDENTIFIER_OR_KEYWORD_RULES = {
    "PragmaName",
    "TypeName",
    "ColLabel",
    "PlainIdentifier",
    "QuotedIdentifier",
    "ReservedKeyword",
    "UnreservedKeyword",
    "ColumnNameKeyword",
    "FuncNameKeyword",
    "TypeNameKeyword",
    "SettingName",
    "ExplainOptionName",
}
RAW_PARSE_RESULT_RULES = {
    "CommentValue",
}


def load_primitive_override_rules(grammar_types_file):
    matcher_overrides = load_matcher_rule_overrides(grammar_types_file)
    primitive_rules = {}
    for rule_name, info in matcher_overrides.items():
        if not isinstance(info, dict):
            continue
        matcher = info.get("matcher")
        if matcher in ("identifier", "reserved_identifier"):
            primitive_rules[rule_name] = PrimitiveRule("identifier")
        elif matcher == "string_literal":
            primitive_rules[rule_name] = PrimitiveRule("string")
        elif matcher == "number_literal":
            primitive_rules[rule_name] = PrimitiveRule("number")
        elif matcher == "operator":
            primitive_rules[rule_name] = PrimitiveRule("operator")
    for rule_name in IDENTIFIER_OR_KEYWORD_RULES:
        primitive_rules.setdefault(rule_name, PrimitiveRule("identifier_or_keyword"))
    return primitive_rules


def primitive_cpp_type(rule_name, rule_types, primitive_rules):
    if rule_name in rule_types:
        return rule_types[rule_name].cpp_type
    kind = primitive_rules[rule_name].kind
    if kind == "identifier":
        return "Identifier"
    if kind in ("string", "identifier_or_keyword"):
        return "string"
    if kind == "number":
        return "unique_ptr<ParsedExpression>"
    if kind == "operator":
        return "string"
    raise NotImplementedError(f"unsupported primitive rule kind: {kind}")


def primitive_by_value(rule_name, rule_types, primitive_rules):
    return primitive_rules[rule_name].kind == "number"


def primitive_value_expr(rule_name, source_expr, expected_type, primitive_rules):
    kind = primitive_rules[rule_name].kind
    if kind == "identifier":
        if expected_type == "Identifier":
            return f"{source_expr}.Cast<IdentifierParseResult>().identifier"
        if expected_type == "string":
            return f"{source_expr}.Cast<IdentifierParseResult>().identifier.GetIdentifierName()"
    if kind == "string":
        if expected_type == "Identifier":
            return f"Identifier({source_expr}.Cast<StringLiteralParseResult>().result)"
        if expected_type == "string":
            return f"{source_expr}.Cast<StringLiteralParseResult>().result"
    if kind == "number":
        if expected_type == "unique_ptr<ParsedExpression>":
            return f"TransformNumberLiteral(transformer, {source_expr})"
    if kind == "operator":
        if expected_type == "string":
            return f"{source_expr}.Cast<KeywordParseResult>().keyword"
    if kind == "identifier_or_keyword":
        if expected_type == "string":
            return f"TransformIdentifierOrKeyword(transformer, {source_expr})"
        if expected_type == "Identifier":
            return f"Identifier(TransformIdentifierOrKeyword(transformer, {source_expr}))"
    raise NotImplementedError(f"primitive rule {rule_name} cannot produce {expected_type}")


def generate_terminal_choice_extraction(var_name, source_expr, result_type, primitive_alternatives=None, primitive_rules=None):
    def as_result(expr):
        return f"Identifier({expr})" if result_type == "Identifier" else expr

    primitive_alternatives = primitive_alternatives or []
    identifier_value = f"{source_expr}.Cast<IdentifierParseResult>().identifier"
    if result_type != "Identifier":
        identifier_value = f"{identifier_value}.GetIdentifierName()"
    lines = [
        f"\t{result_type} {var_name};",
        f"\tif ({source_expr}.type == ParseResultType::IDENTIFIER) {{",
        f"\t\t{var_name} = {identifier_value};",
        f"\t}} else if ({source_expr}.type == ParseResultType::KEYWORD) {{",
        f"\t\t{var_name} = {as_result(f'{source_expr}.Cast<KeywordParseResult>().keyword')};",
        f"\t}} else if ({source_expr}.type == ParseResultType::STRING) {{",
        f"\t\t{var_name} = {as_result(f'{source_expr}.Cast<StringLiteralParseResult>().result')};",
    ]
    for alternative in primitive_alternatives:
        if primitive_rules is None:
            continue
        value_expr = primitive_value_expr(alternative, source_expr, result_type, primitive_rules)
        lines.extend(
            [
                f'\t}} else if ({source_expr}.name == "{alternative}") {{',
                f"\t\t{var_name} = {value_expr};",
            ]
        )
    lines.extend(
        [
            "\t} else {",
            f"\t\t{var_name} = frame.TakeResult<{result_type}>(0);",
            "\t}",
        ]
    )
    return lines


def to_upper_snake_case(name):
    value = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value).upper()


def ops_name(rule_name):
    return f"{to_upper_snake_case(rule_name)}_OPS"


def initialize_name(rule_name):
    return f"Initialize{rule_name}"


def finalize_name(rule_name):
    return f"Finalize{rule_name}"


def box_result(return_type, by_value):
    result_expr = "std::move(result)" if by_value else "result"
    return f"\treturn make_uniq<TypedTransformResult<{return_type}>>({result_expr});"


def is_transformer_rule(rule_name, rules, rule_types, primitive_rules=None):
    if primitive_rules is not None and rule_name in primitive_rules:
        return False
    return rule_name in rules and rule_name in rule_types


def child_ops(rule_name):
    return ops_name(rule_name)


def rule_type(rule_name, rule_types):
    return rule_types[rule_name].cpp_type


def rule_by_value(rule_name, rule_types):
    info = rule_types[rule_name]
    return info.by_value or "unique_ptr<" in info.cpp_type


def semantic_rule_type(rule_name, rule_types, primitive_rules):
    if rule_name in primitive_rules:
        return primitive_cpp_type(rule_name, rule_types, primitive_rules)
    return rule_type(rule_name, rule_types)


def semantic_rule_by_value(rule_name, rule_types, primitive_rules):
    if rule_name in primitive_rules:
        return primitive_by_value(rule_name, rule_types, primitive_rules)
    return rule_by_value(rule_name, rule_types)


def is_semantic_rule(rule_name, rules, rule_types, primitive_rules):
    return rule_name in primitive_rules or is_transformer_rule(rule_name, rules, rule_types, primitive_rules)


def ast_has_semantic_rule(ast, rules, rule_types, primitive_rules):
    if isinstance(ast, ReferenceNode):
        return is_semantic_rule(ast.name, rules, rule_types, primitive_rules)
    if isinstance(ast, SequenceNode):
        return any(ast_has_semantic_rule(child, rules, rule_types, primitive_rules) for child in ast.children)
    if isinstance(ast, ChoiceNode):
        return any(ast_has_semantic_rule(alternative, rules, rule_types, primitive_rules) for alternative in ast.alternatives)
    if isinstance(ast, OptionalNode):
        return ast_has_semantic_rule(ast.child, rules, rule_types, primitive_rules)
    if isinstance(ast, RepeatNode):
        return ast_has_semantic_rule(ast.child, rules, rule_types, primitive_rules)
    if isinstance(ast, ParensNode):
        return ast_has_semantic_rule(ast.inner, rules, rule_types, primitive_rules)
    if isinstance(ast, ListMacroNode):
        return ast_has_semantic_rule(ast.inner, rules, rule_types, primitive_rules)
    if isinstance(ast, FunctionCallNode):
        return ast_has_semantic_rule(ast.argument, rules, rule_types, primitive_rules)
    return False


def ensure_sequence(ast):
    if isinstance(ast, SequenceNode):
        return ast
    return SequenceNode([ast])


def list_reference_node(node):
    if isinstance(node, ListMacroNode) and isinstance(node.inner, ReferenceNode):
        return node.inner
    if (
        isinstance(node, ParensNode)
        and isinstance(node.inner, ListMacroNode)
        and isinstance(node.inner.inner, ReferenceNode)
    ):
        return node.inner.inner
    if (
        isinstance(node, OptionalNode)
        and isinstance(node.child, ListMacroNode)
        and isinstance(node.child.inner, ReferenceNode)
    ):
        return node.child.inner
    if (
        isinstance(node, OptionalNode)
        and isinstance(node.child, ParensNode)
        and isinstance(node.child.inner, ListMacroNode)
        and isinstance(node.child.inner.inner, ReferenceNode)
    ):
        return node.child.inner.inner
    if (
        isinstance(node, ParensNode)
        and isinstance(node.inner, OptionalNode)
        and isinstance(node.inner.child, ListMacroNode)
        and isinstance(node.inner.child.inner, ReferenceNode)
    ):
        return node.inner.child.inner
    return None


def optional_sequence_reference_node(node):
    if not isinstance(node, OptionalNode) or not isinstance(node.child, SequenceNode):
        return None
    semantic_children = [
        (idx, child)
        for idx, child in enumerate(node.child.children)
        if isinstance(child, ReferenceNode)
    ]
    if len(semantic_children) != 1:
        return None
    return semantic_children[0]


def optional_parens_reference_node(node):
    if (
        isinstance(node, OptionalNode)
        and isinstance(node.child, ParensNode)
        and isinstance(node.child.inner, ReferenceNode)
    ):
        return node.child.inner
    return None


def parens_optional_reference_node(node):
    if (
        isinstance(node, ParensNode)
        and isinstance(node.inner, OptionalNode)
        and isinstance(node.inner.child, ReferenceNode)
    ):
        return node.inner.child
    return None


def is_literal_only_choice(ast):
    return isinstance(ast, ChoiceNode) and all(isinstance(alternative, LiteralNode) for alternative in ast.alternatives)


def literal_sequence_text(ast):
    if isinstance(ast, LiteralNode):
        return ast.text
    if isinstance(ast, SequenceNode) and all(isinstance(child, LiteralNode) for child in ast.children):
        return " ".join(child.text for child in ast.children)
    return None


def is_syntax_only_ast(ast, rules, rule_types, primitive_rules):
    return not ast_has_semantic_rule(ast, rules, rule_types, primitive_rules)


def is_syntax_only_reference_choice(ast, excluded_rules, rule_types, primitive_rules):
    return isinstance(ast, ChoiceNode) and all(
        isinstance(alternative, ReferenceNode)
        and alternative.name in excluded_rules
        and alternative.name not in rule_types
        and alternative.name not in primitive_rules
        for alternative in ast.alternatives
    )


def list_stack_mode(node):
    if isinstance(node, ListMacroNode):
        return "list"
    if isinstance(node, OptionalNode) and isinstance(node.child, ListMacroNode):
        return "optional_list"
    if isinstance(node, ParensNode) and isinstance(node.inner, ListMacroNode):
        return "parens_list"
    if isinstance(node, OptionalNode):
        return "optional_parens_list"
    if isinstance(node, ParensNode) and isinstance(node.inner, OptionalNode):
        return "parens_optional_list"
    raise NotImplementedError(f"unsupported list stack node: {node}")


def list_value_expr(parse_expr, mode):
    if mode == "list":
        return parse_expr
    if mode == "parens_list":
        return f"ExtractResultFromParens({parse_expr})"
    raise NotImplementedError(f"list_value_expr is only valid for non-optional modes: {mode}")


def append_list_item_extraction(lines, child_rule, item_expr, target_var, rule_types, primitive_rules, indent):
    cpp_type = semantic_rule_type(child_rule, rule_types, primitive_rules)
    by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
    if child_rule in primitive_rules:
        value_expr = primitive_value_expr(child_rule, item_expr, cpp_type, primitive_rules)
        lines.append(f"{indent}auto {target_var} = {value_expr};")
    else:
        lines.append(f"{indent}auto {target_var} = frame.TakeResult<{cpp_type}>(child_slot + child_idx);")
    push_expr = f"std::move({target_var})" if by_value else target_var
    lines.append(f"{indent}{to_snake_case(child_rule)}.push_back({push_expr});")


def generate_list_finalize_arg(child_idx, child, rules, rule_types, primitive_rules, expected_type=None):
    list_ref = list_reference_node(child)
    if not list_ref or not is_semantic_rule(list_ref.name, rules, rule_types, primitive_rules):
        return None, False

    child_rule = list_ref.name
    child_type = list_element_type(expected_type) or semantic_rule_type(child_rule, rule_types, primitive_rules)
    list_by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
    var_name = to_snake_case(child_rule)
    item_name = f"{var_name}_item"
    value_name = f"{var_name}_child_value"
    source_expr = f"list_pr.GetChild({child_idx})"
    mode = list_stack_mode(child)
    child_by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
    needs_child_slot = child_rule not in primitive_rules

    if mode == "optional_parens_list":
        lines = [
            f"\toptional<vector<{child_type}>> {var_name} {{}};",
            f"\tauto &{var_name}_opt = {source_expr}.Cast<OptionalParseResult>();",
            f"\tif ({var_name}_opt.HasResult()) {{",
            f"\t\tvector<{child_type}> {var_name}_value;",
            (
                f"\t\tauto {var_name}_items = "
                f"ExtractParseResultsFromList(ExtractResultFromParens({var_name}_opt.GetResult()));"
            ),
            f"\t\tfor (idx_t child_idx = 0; child_idx < {var_name}_items.size(); child_idx++) {{",
            f"\t\t\tauto &{item_name} = {var_name}_items[child_idx];",
        ]
        if child_rule in primitive_rules:
            value_expr = primitive_value_expr(child_rule, f"{item_name}.get()", child_type, primitive_rules)
            lines.append(f"\t\t\tauto {value_name} = {value_expr};")
        else:
            lines.append(f"\t\t\tauto {value_name} = frame.TakeResult<{child_type}>(child_slot + child_idx);")
        push_expr = f"std::move({value_name})" if child_by_value else value_name
        lines.extend(
            [
                f"\t\t\t{var_name}_value.push_back({push_expr});",
                "\t\t}",
                f"\t\t{var_name} = std::move({var_name}_value);",
            ]
        )
        if needs_child_slot:
            lines.append(f"\t\tchild_slot += {var_name}_items.size();")
        lines.append("\t}")
        return FinalizeArg(var_name, f"optional<vector<{child_type}>>", list_by_value, lines), needs_child_slot
    if mode == "optional_list":
        lines = [
            f"\toptional<vector<{child_type}>> {var_name} {{}};",
            f"\tauto &{var_name}_opt = {source_expr}.Cast<OptionalParseResult>();",
            f"\tif ({var_name}_opt.HasResult()) {{",
            f"\t\tvector<{child_type}> {var_name}_value;",
            f"\t\tauto {var_name}_items = ExtractParseResultsFromList({var_name}_opt.GetResult());",
            f"\t\tfor (idx_t child_idx = 0; child_idx < {var_name}_items.size(); child_idx++) {{",
            f"\t\t\tauto &{item_name} = {var_name}_items[child_idx];",
        ]
        if child_rule in primitive_rules:
            value_expr = primitive_value_expr(child_rule, f"{item_name}.get()", child_type, primitive_rules)
            lines.append(f"\t\t\tauto {value_name} = {value_expr};")
        else:
            lines.append(f"\t\t\tauto {value_name} = frame.TakeResult<{child_type}>(child_slot + child_idx);")
        push_expr = f"std::move({value_name})" if child_by_value else value_name
        lines.extend(
            [
                f"\t\t\t{var_name}_value.push_back({push_expr});",
                "\t\t}",
                f"\t\t{var_name} = std::move({var_name}_value);",
            ]
        )
        if needs_child_slot:
            lines.append(f"\t\tchild_slot += {var_name}_items.size();")
        lines.append("\t}")
        return FinalizeArg(var_name, f"optional<vector<{child_type}>>", list_by_value, lines), needs_child_slot

    lines = [f"\tvector<{child_type}> {var_name};"]
    if mode == "parens_optional_list":
        lines.extend(
            [
                f"\tauto &{var_name}_opt = ExtractResultFromParens({source_expr}).Cast<OptionalParseResult>();",
                f"\tif ({var_name}_opt.HasResult()) {{",
                f"\t\tauto {var_name}_items = ExtractParseResultsFromList({var_name}_opt.GetResult());",
                f"\t\tfor (idx_t child_idx = 0; child_idx < {var_name}_items.size(); child_idx++) {{",
                f"\t\t\tauto &{item_name} = {var_name}_items[child_idx];",
            ]
        )
        indent = "\t\t\t"
        close_indent = "\t\t"
    else:
        list_expr = list_value_expr(source_expr, mode)
        lines.extend(
            [
                f"\tauto {var_name}_items = ExtractParseResultsFromList({list_expr});",
                f"\tfor (idx_t child_idx = 0; child_idx < {var_name}_items.size(); child_idx++) {{",
                f"\t\tauto &{item_name} = {var_name}_items[child_idx];",
            ]
        )
        indent = "\t\t"
        close_indent = "\t"

    if child_rule in primitive_rules:
        value_expr = primitive_value_expr(child_rule, f"{item_name}.get()", child_type, primitive_rules)
        lines.append(f"{indent}auto {value_name} = {value_expr};")
    else:
        lines.append(f"{indent}auto {value_name} = frame.TakeResult<{child_type}>(child_slot + child_idx);")
    push_expr = f"std::move({value_name})" if child_by_value else value_name
    lines.extend(
        [
            f"{indent}{var_name}.push_back({push_expr});",
            f"{close_indent}}}",
        ]
    )
    if needs_child_slot:
        lines.append(f"\tchild_slot += {var_name}_items.size();")
    if mode == "parens_optional_list":
        lines.append("\t}")
    return FinalizeArg(var_name, f"vector<{child_type}>", list_by_value, lines), needs_child_slot


def sequence_stack_children(sequence, rules, rule_types, primitive_rules):
    children = []

    for child_idx, child in enumerate(sequence.children):
        optional_parens_ref = optional_parens_reference_node(child)
        if (
            optional_parens_ref
            and optional_parens_ref.name not in primitive_rules
            and is_transformer_rule(optional_parens_ref.name, rules, rule_types, primitive_rules)
        ):
            children.append(
                OptionalStackChild(
                    parse_expr=f"list_pr.GetChild({child_idx})",
                    rule_name=optional_parens_ref.name,
                    slot_idx=0,
                    parens=True,
                )
            )
            continue
        parens_optional_ref = parens_optional_reference_node(child)
        if (
            parens_optional_ref
            and parens_optional_ref.name not in primitive_rules
            and is_transformer_rule(parens_optional_ref.name, rules, rule_types, primitive_rules)
        ):
            children.append(
                OptionalStackChild(
                    parse_expr=f"ExtractResultFromParens(list_pr.GetChild({child_idx}))",
                    rule_name=parens_optional_ref.name,
                    slot_idx=0,
                )
            )
            continue
        optional_sequence_ref = optional_sequence_reference_node(child)
        if optional_sequence_ref:
            sequence_child_idx, sequence_ref = optional_sequence_ref
            if (
                sequence_ref.name not in primitive_rules
                and is_transformer_rule(sequence_ref.name, rules, rule_types, primitive_rules)
            ):
                children.append(
                    OptionalStackChild(
                        parse_expr=f"list_pr.GetChild({child_idx})",
                        rule_name=sequence_ref.name,
                        slot_idx=0,
                        sequence_child_idx=sequence_child_idx,
                    )
                )
                continue
        list_ref = list_reference_node(child)
        if (
            list_ref
            and list_ref.name not in primitive_rules
            and is_transformer_rule(list_ref.name, rules, rule_types, primitive_rules)
        ):
            child_rule = list_ref.name
            children.append(
                ListStackChild(
                    parse_expr=f"list_pr.GetChild({child_idx})",
                    rule_name=child_rule,
                    var_name=to_snake_case(child_rule),
                    cpp_type=rule_type(child_rule, rule_types),
                    by_value=rule_by_value(child_rule, rule_types),
                    mode=list_stack_mode(child),
                )
            )
            continue
        if (
            isinstance(child, ParensNode)
            and isinstance(child.inner, ReferenceNode)
            and is_transformer_rule(child.inner.name, rules, rule_types, primitive_rules)
        ):
            children.append(
                StackChild(
                    parse_expr=f"ExtractResultFromParens(list_pr.GetChild({child_idx}))",
                    rule_name=child.inner.name,
                    slot_idx=0,
                )
            )
            continue
        if isinstance(child, ReferenceNode) and is_transformer_rule(child.name, rules, rule_types, primitive_rules):
            children.append(
                StackChild(
                    parse_expr=f"list_pr.GetChild({child_idx})",
                    rule_name=child.name,
                    slot_idx=0,
                )
            )
            continue
        if (
            isinstance(child, OptionalNode)
            and isinstance(child.child, ReferenceNode)
            and is_transformer_rule(child.child.name, rules, rule_types, primitive_rules)
        ):
            children.append(
                OptionalStackChild(
                    parse_expr=f"list_pr.GetChild({child_idx})",
                    rule_name=child.child.name,
                    slot_idx=0,
                )
            )
            continue
        if (
            isinstance(child, OptionalNode)
            and isinstance(child.child, RepeatNode)
            and isinstance(child.child.child, ReferenceNode)
            and is_transformer_rule(child.child.child.name, rules, rule_types, primitive_rules)
        ):
            child_rule = child.child.child.name
            repeat_child = RepeatStackChild(
                parse_expr=f"list_pr.GetChild({child_idx})",
                rule_name=child_rule,
                var_name=to_snake_case(child_rule),
                cpp_type=rule_type(child_rule, rule_types),
                by_value=rule_by_value(child_rule, rule_types),
                optional=True,
            )
            children.append(repeat_child)
            continue
        if (
            isinstance(child, RepeatNode)
            and isinstance(child.child, ReferenceNode)
            and is_transformer_rule(child.child.name, rules, rule_types, primitive_rules)
        ):
            child_rule = child.child.name
            repeat_child = RepeatStackChild(
                parse_expr=f"list_pr.GetChild({child_idx})",
                rule_name=child_rule,
                var_name=to_snake_case(child_rule),
                cpp_type=rule_type(child_rule, rule_types),
                by_value=rule_by_value(child_rule, rule_types),
                optional=False,
            )
            children.append(repeat_child)

    return children


def generate_ops_definition(rule_name):
    return (
        f"const TransformFrameOps PEGTransformerFactory::{ops_name(rule_name)} = {{\n"
        f'    "{rule_name}", &PEGTransformerFactory::{initialize_name(rule_name)},\n'
        f"    &PEGTransformerFactory::{finalize_name(rule_name)}}};"
    )


def generate_declarations(rule_name):
    return [
        f"\tstatic const TransformFrameOps {ops_name(rule_name)};",
        (
            f"\tstatic void {initialize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame);"
        ),
        (
            f"\tstatic unique_ptr<TransformResultValue> {finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame);"
        ),
    ]


def declaration_line(signature):
    if len(signature) <= 120:
        return f"\t{signature};"
    open_paren = signature.find("(")
    if open_paren == -1:
        return f"\t{signature};"
    return f"\t{signature[:open_paren + 1]}\n\t    {signature[open_paren + 1:]};"


def format_cpp_args(args):
    formatted = []
    for arg in args:
        if arg.by_value:
            formatted.append(f"{arg.cpp_type} {arg.var_name}")
        else:
            formatted.append(f"const {arg.cpp_type} &{arg.var_name}")
    return formatted


def split_cpp_parameters(params):
    result = []
    current = []
    depth = 0
    for char in params:
        if char == "<":
            depth += 1
        elif char == ">":
            depth -= 1
        elif char == "," and depth == 0:
            result.append("".join(current).strip())
            current = []
            continue
        current.append(char)
    if current:
        result.append("".join(current).strip())
    return result


def cpp_parameter_type(param):
    param = param.split("=", 1)[0].strip()
    param = re.sub(r"\s+", " ", param)
    param = re.sub(r"\s*[&*]\s*", " ", param).strip()
    param = re.sub(r"\s+[A-Za-z_][A-Za-z0-9_]*$", "", param).strip()
    if param.startswith("const "):
        param = param[len("const ") :].strip()
    return param


@lru_cache(maxsize=None)
def transform_body_signature(rule_name):
    text = transformer_header.read_text()
    for suffix in ("", "Rule"):
        function_name = f"Transform{rule_name}{suffix}"
        pattern = re.compile(
            rf"\bstatic\s+([\w:<>,\s*&]+?)\s+{re.escape(function_name)}\s*\((.*?)\);",
            re.DOTALL,
        )
        match = pattern.search(text)
        if not match:
            continue
        return_type = re.sub(r"\s+", " ", match.group(1)).strip()
        params_text = match.group(2)
        if return_type == "unique_ptr<TransformResultValue>" or "ParseResult" in params_text:
            continue
        params = []
        for param in split_cpp_parameters(params_text):
            if "PEGTransformer" in param:
                continue
            params.append(cpp_parameter_type(param))
        return function_name, params
    return None, []


def transform_body_name(rule_name, arg_count=None):
    name, params = transform_body_signature(rule_name)
    if arg_count is not None and len(params) != arg_count:
        return None
    return name


def transform_body_param_types(rule_name):
    _, params = transform_body_signature(rule_name)
    return params


def expected_arg_type(expected_types, arg_idx, default_type):
    if arg_idx >= len(expected_types):
        return default_type
    expected_type = expected_types[arg_idx]
    if expected_type == "optional_idx":
        return default_type
    return expected_type


def optional_inner_type(cpp_type):
    if cpp_type.startswith("optional<") and cpp_type.endswith(">"):
        return cpp_type[len("optional<") : -1]
    return None


def vector_inner_type(cpp_type):
    if cpp_type and cpp_type.startswith("vector<") and cpp_type.endswith(">"):
        return cpp_type[len("vector<") : -1]
    return None


def list_element_type(cpp_type):
    optional_inner = optional_inner_type(cpp_type) if cpp_type else None
    if optional_inner:
        cpp_type = optional_inner
    return vector_inner_type(cpp_type)


def generate_trampoline_rule_declarations(rule_name):
    return [
        f"\tstatic const TransformFrameOps {ops_name(rule_name)};",
        declaration_line(
            f"static void {initialize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame)"
        ),
        declaration_line(
            f"static unique_ptr<TransformResultValue> {finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame)"
        ),
    ]


def generate_choice_initialize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules):
    alternatives = [alternative.name for alternative in ast.alternatives]
    unsupported = [
        name
        for name in alternatives
        if name not in primitive_rules
        and name not in excluded_rules
        and not is_transformer_rule(name, rules, rule_types, primitive_rules)
    ]
    if unsupported:
        raise NotImplementedError(f"{rule_name}: unsupported choice alternatives: {unsupported}")
    has_transformer_alternative = any(is_transformer_rule(name, rules, rule_types, primitive_rules) for name in alternatives)

    lines = [
        f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,",
        "                                                TransformStackFrame &frame) {",
        "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();",
        "\tauto &choice_pr = list_pr.Child<ChoiceParseResult>(0);",
        "\tauto &choice_result = choice_pr.GetResult();",
    ]
    if not has_transformer_alternative:
        lines.extend(
            [
                "\tframe.child_results.resize(0);",
                "}",
            ]
        )
        return "\n".join(lines)
    lines.extend(
        [
        "\tauto &trampoline_ops = GeneratedTrampolineOps();",
        "\tauto entry = trampoline_ops.find(choice_result.name);",
        "\tif (entry == trampoline_ops.end()) {",
        "\t\tframe.child_results.resize(0);",
        "\t\treturn;",
        "\t}",
        "\tframe.child_results.resize(1);",
        "\tstack.PushFrame(choice_result, *entry->second, frame, 0);",
        "}",
        ]
    )
    return "\n".join(lines)


def generate_sequence_initialize(rule_name, ast, rules, rule_types, primitive_rules):
    sequence = ensure_sequence(ast)
    stack_children = sequence_stack_children(sequence, rules, rule_types, primitive_rules)
    lines = [
        f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,",
        "                                                TransformStackFrame &frame) {",
    ]
    if not stack_children:
        lines.append("}")
        return "\n".join(lines)

    lines.extend(
        [
            "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();",
            "\tidx_t child_result_count = 0;",
        ]
    )
    for child_idx, child in enumerate(stack_children):
        if isinstance(child, StackChild):
            lines.append("\tchild_result_count++;")
            continue
        if isinstance(child, OptionalStackChild):
            var_name = f"{to_snake_case(child.rule_name)}_opt_{child_idx}"
            lines.extend(
                [
                    f"\tauto &{var_name} = {child.parse_expr}.Cast<OptionalParseResult>();",
                    f"\tif ({var_name}.HasResult()) {{",
                    "\t\tchild_result_count++;",
                    "\t}",
                ]
            )
            continue
        if isinstance(child, RepeatStackChild):
            repeat_var = f"{child.var_name}_repeat_{child_idx}"
            children_var = f"{child.var_name}_children_{child_idx}"
            if child.optional:
                opt_var = f"{child.var_name}_opt_{child_idx}"
                lines.extend(
                    [
                        f"\tauto &{opt_var} = {child.parse_expr}.Cast<OptionalParseResult>();",
                        f"\tif ({opt_var}.HasResult()) {{",
                        f"\t\tauto &{repeat_var} = {opt_var}.GetResult().Cast<RepeatParseResult>();",
                        f"\t\tchild_result_count += {repeat_var}.GetChildren().size();",
                        "\t}",
                    ]
                )
            else:
                lines.extend(
                    [
                        f"\tauto &{repeat_var} = {child.parse_expr}.Cast<RepeatParseResult>();",
                        f"\tchild_result_count += {repeat_var}.GetChildren().size();",
                    ]
                )
            continue
        if isinstance(child, ListStackChild):
            list_var = f"{child.var_name}_list_{child_idx}"
            children_var = f"{child.var_name}_children_{child_idx}"
            opt_var = f"{child.var_name}_opt_{child_idx}"
            if child.mode in ("list", "parens_list"):
                lines.extend(
                    [
                        f"\tauto {children_var} = ExtractParseResultsFromList({list_value_expr(child.parse_expr, child.mode)});",
                        f"\tchild_result_count += {children_var}.size();",
                    ]
                )
            elif child.mode == "optional_parens_list":
                lines.extend(
                    [
                        f"\tauto &{opt_var} = {child.parse_expr}.Cast<OptionalParseResult>();",
                        f"\tif ({opt_var}.HasResult()) {{",
                        f"\t\tauto {children_var} = ExtractParseResultsFromList(ExtractResultFromParens({opt_var}.GetResult()));",
                        f"\t\tchild_result_count += {children_var}.size();",
                        "\t}",
                    ]
                )
            elif child.mode == "optional_list":
                lines.extend(
                    [
                        f"\tauto &{opt_var} = {child.parse_expr}.Cast<OptionalParseResult>();",
                        f"\tif ({opt_var}.HasResult()) {{",
                        f"\t\tauto {children_var} = ExtractParseResultsFromList({opt_var}.GetResult());",
                        f"\t\tchild_result_count += {children_var}.size();",
                        "\t}",
                    ]
                )
            elif child.mode == "parens_optional_list":
                lines.extend(
                    [
                        f"\tauto &{opt_var} = ExtractResultFromParens({child.parse_expr}).Cast<OptionalParseResult>();",
                        f"\tif ({opt_var}.HasResult()) {{",
                        f"\t\tauto {children_var} = ExtractParseResultsFromList({opt_var}.GetResult());",
                        f"\t\tchild_result_count += {children_var}.size();",
                        "\t}",
                    ]
                )
            continue
    lines.extend(
        [
            "\tframe.child_results.resize(child_result_count);",
            "\tauto parent_frame_index = frame.frame_index;",
            "\tidx_t child_slot = 0;",
        ]
    )
    for child_idx, child in enumerate(stack_children):
        if isinstance(child, StackChild):
            lines.extend(
                [
                    f"\tstack.PushFrame({child.parse_expr}, {child_ops(child.rule_name)}, parent_frame_index, child_slot);",
                    "\tchild_slot++;",
                ]
            )
            continue
        if isinstance(child, OptionalStackChild):
            var_name = f"{to_snake_case(child.rule_name)}_opt_{child_idx}"
            if child.sequence_child_idx is not None:
                child_result_expr = f"{var_name}.GetResult().Cast<ListParseResult>().GetChild({child.sequence_child_idx})"
            elif child.parens:
                child_result_expr = f"ExtractResultFromParens({var_name}.GetResult())"
            else:
                child_result_expr = f"{var_name}.GetResult()"
            lines.extend(
                [
                    f"\tif ({var_name}.HasResult()) {{",
                    f"\t\tstack.PushFrame({child_result_expr}, {child_ops(child.rule_name)}, parent_frame_index, child_slot);",
                    "\t\tchild_slot++;",
                    "\t}",
                ]
            )
            continue
        if isinstance(child, RepeatStackChild):
            repeat_var = f"{child.var_name}_repeat_{child_idx}"
            children_var = f"{child.var_name}_children_{child_idx}"
            if child.optional:
                opt_var = f"{child.var_name}_opt_{child_idx}"
                lines.extend(
                    [
                        f"\tif ({opt_var}.HasResult()) {{",
                        f"\t\tauto &{repeat_var} = {opt_var}.GetResult().Cast<RepeatParseResult>();",
                        f"\t\tauto {children_var} = {repeat_var}.GetChildren();",
                        f"\t\tfor (idx_t child_idx = {children_var}.size(); child_idx > 0; child_idx--) {{",
                        "\t\t\tauto result_idx = child_idx - 1;",
                        (
                            f"\t\t\tstack.PushFrame({children_var}[result_idx].get(), {child_ops(child.rule_name)}, "
                            "parent_frame_index, child_slot + result_idx);"
                        ),
                        "\t\t}",
                        f"\t\tchild_slot += {children_var}.size();",
                        "\t}",
                    ]
                )
            else:
                lines.extend(
                    [
                        f"\tauto {children_var} = {repeat_var}.GetChildren();",
                        f"\tfor (idx_t child_idx = {children_var}.size(); child_idx > 0; child_idx--) {{",
                        "\t\tauto result_idx = child_idx - 1;",
                        (
                            f"\t\tstack.PushFrame({children_var}[result_idx].get(), {child_ops(child.rule_name)}, "
                            "parent_frame_index, child_slot + result_idx);"
                        ),
                        "\t}",
                        f"\tchild_slot += {children_var}.size();",
                    ]
                )
            continue
        if isinstance(child, ListStackChild):
            children_var = f"{child.var_name}_children_{child_idx}"
            opt_var = f"{child.var_name}_opt_{child_idx}"
            push_lines = [
                f"\tfor (idx_t child_idx = {children_var}.size(); child_idx > 0; child_idx--) {{",
                "\t\tauto result_idx = child_idx - 1;",
                (
                    f"\t\tstack.PushFrame({children_var}[result_idx].get(), {child_ops(child.rule_name)}, "
                    "parent_frame_index, child_slot + result_idx);"
                ),
                "\t}",
                f"\tchild_slot += {children_var}.size();",
            ]
            if child.mode in ("list", "parens_list"):
                lines.extend(push_lines)
            elif child.mode == "optional_parens_list":
                lines.append(f"\tif ({opt_var}.HasResult()) {{")
                lines.append(
                    f"\t\tauto {children_var} = ExtractParseResultsFromList(ExtractResultFromParens({opt_var}.GetResult()));"
                )
                lines.extend([line.replace("\t", "\t\t", 1) for line in push_lines])
                lines.append("\t}")
            elif child.mode == "optional_list":
                lines.append(f"\tif ({opt_var}.HasResult()) {{")
                lines.append(f"\t\tauto {children_var} = ExtractParseResultsFromList({opt_var}.GetResult());")
                lines.extend([line.replace("\t", "\t\t", 1) for line in push_lines])
                lines.append("\t}")
            elif child.mode == "parens_optional_list":
                lines.append(f"\tif ({opt_var}.HasResult()) {{")
                lines.append(f"\t\tauto {children_var} = ExtractParseResultsFromList({opt_var}.GetResult());")
                lines.extend([line.replace("\t", "\t\t", 1) for line in push_lines])
                lines.append("\t}")
            continue
    lines.append("}")
    return "\n".join(lines)


def generate_initialize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules):
    if rule_name in RAW_PARSE_RESULT_RULES:
        return (
            f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,\n"
            "                                                TransformStackFrame &frame) {\n"
            "}"
        )
    if manual_transform_parse_result_exists(rule_name, rule_type(rule_name, rule_types)):
        return (
            f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,\n"
            "                                                TransformStackFrame &frame) {\n"
            "}"
        )
    if (
        manual_transform_exists(rule_name)
        and not transform_body_param_types(rule_name)
        and not ast_has_semantic_rule(ast, rules, rule_types, primitive_rules)
    ):
        return (
            f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,\n"
            "                                                TransformStackFrame &frame) {\n"
            "}"
        )
    if is_pure_reference_choice(ast):
        return generate_choice_initialize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules)
    return generate_sequence_initialize(rule_name, ast, rules, rule_types, primitive_rules)


def sequence_finalize_args(rule_name, sequence, rules, rule_types, excluded_rules, primitive_rules):
    args = []
    stack_slot_expr = "child_slot"
    needs_child_slot = False
    expected_types = transform_body_param_types(rule_name)
    for child_idx, child in enumerate(sequence.children):
        if isinstance(child, LiteralNode):
            continue
        if is_literal_only_choice(child) or is_syntax_only_reference_choice(child, excluded_rules, rule_types, primitive_rules):
            continue
        list_arg, list_needs_child_slot = generate_list_finalize_arg(
            child_idx,
            child,
            rules,
            rule_types,
            primitive_rules,
            expected_arg_type(expected_types, len(args), None),
        )
        if list_arg:
            args.append(list_arg)
            needs_child_slot = needs_child_slot or list_needs_child_slot
            continue
        if (
            isinstance(child, ParensNode)
            and isinstance(child.inner, ReferenceNode)
            and is_semantic_rule(child.inner.name, rules, rule_types, primitive_rules)
        ):
            child_rule = child.inner.name
            var_name = to_snake_case(child_rule)
            cpp_type = expected_arg_type(
                expected_types, len(args), semantic_rule_type(child_rule, rule_types, primitive_rules)
            )
            by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
            if child_rule in primitive_rules:
                lines = [
                    (
                        f"\tauto {var_name} = "
                        f"{primitive_value_expr(child_rule, f'ExtractResultFromParens(list_pr.GetChild({child_idx}))', cpp_type, primitive_rules)};"
                    )
                ]
            else:
                needs_child_slot = True
                lines = [
                    f"\tauto {var_name} = frame.TakeResult<{cpp_type}>({stack_slot_expr});",
                    "\tchild_slot++;",
                ]
            args.append(FinalizeArg(var_name, cpp_type, by_value, lines))
            continue
        if isinstance(child, ReferenceNode):
            if child.name in excluded_rules and child.name not in rule_types and child.name not in primitive_rules:
                continue
            if child.name in primitive_rules:
                cpp_type = expected_arg_type(
                    expected_types, len(args), primitive_cpp_type(child.name, rule_types, primitive_rules)
                )
                by_value = primitive_by_value(child.name, rule_types, primitive_rules)
                var_name = to_snake_case(child.name)
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=cpp_type,
                        by_value=by_value,
                        lines=[
                            (
                                f"\tauto {var_name} = "
                                f"{primitive_value_expr(child.name, f'list_pr.GetChild({child_idx})', cpp_type, primitive_rules)};"
                            )
                        ],
                    )
                )
                continue
            if is_transformer_rule(child.name, rules, rule_types, primitive_rules):
                needs_child_slot = True
                var_name = to_snake_case(child.name)
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=rule_type(child.name, rule_types),
                        by_value=rule_by_value(child.name, rule_types),
                        lines=[
                            (
                                f"\tauto {var_name} = "
                                f"frame.TakeResult<{rule_type(child.name, rule_types)}>({stack_slot_expr});"
                            ),
                            "\tchild_slot++;",
                        ],
                    )
                )
                continue
        if isinstance(child, OptionalNode) and isinstance(child.child, ReferenceNode):
            if child.child.name in excluded_rules and child.child.name not in rule_types and child.child.name not in primitive_rules:
                args.append(
                    FinalizeArg(
                        var_name="has_result",
                        cpp_type="bool",
                        by_value=False,
                        lines=[
                            "\tbool has_result {};",
                            f"\tauto &has_result_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                            "\thas_result = has_result_opt.HasResult();",
                        ],
                    )
                )
                continue
            if child.child.name in primitive_rules:
                expected_type = expected_arg_type(
                    expected_types,
                    len(args),
                    f"optional<{primitive_cpp_type(child.child.name, rule_types, primitive_rules)}>",
                )
                cpp_type = optional_inner_type(expected_type) or primitive_cpp_type(
                    child.child.name, rule_types, primitive_rules
                )
                by_value = primitive_by_value(child.child.name, rule_types, primitive_rules)
                var_name = to_snake_case(child.child.name)
                value_expr = f"std::move({var_name}_value)" if by_value else f"{var_name}_value"
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=f"optional<{cpp_type}>",
                        by_value=by_value,
                        lines=[
                            f"\toptional<{cpp_type}> {var_name} {{}};",
                            f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                            f"\tif ({var_name}_opt.HasResult()) {{",
                            (
                                f"\t\tauto {var_name}_value = "
                                f"{primitive_value_expr(child.child.name, f'{var_name}_opt.GetResult()', cpp_type, primitive_rules)};"
                            ),
                            f"\t\t{var_name} = {value_expr};",
                            "\t}",
                        ],
                    )
                )
                continue
            if is_transformer_rule(child.child.name, rules, rule_types, primitive_rules):
                needs_child_slot = True
                child_rule = child.child.name
                var_name = to_snake_case(child_rule)
                child_cpp_type = rule_type(child_rule, rule_types)
                expected_type = expected_arg_type(expected_types, len(args), f"optional<{child_cpp_type}>")
                optional_inner = optional_inner_type(expected_type)
                cpp_type = optional_inner or expected_type
                by_value = rule_by_value(child_rule, rule_types)
                value_expr = f"std::move({var_name}_value)" if by_value else f"{var_name}_value"
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=f"optional<{cpp_type}>" if optional_inner else cpp_type,
                        by_value=by_value,
                        lines=[
                            f"\t{'optional<' if optional_inner else ''}{cpp_type}{'>' if optional_inner else ''} {var_name} {{}};",
                            f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                            f"\tif ({var_name}_opt.HasResult()) {{",
                            f"\t\tauto {var_name}_value = frame.TakeResult<{child_cpp_type}>({stack_slot_expr});",
                            f"\t\t{var_name} = {value_expr};",
                            "\t\tchild_slot++;",
                            "\t}",
                        ],
                    )
                )
                continue
        if isinstance(child, OptionalNode) and is_syntax_only_ast(child.child, rules, rule_types, primitive_rules):
            args.append(
                FinalizeArg(
                    var_name="has_result",
                    cpp_type="bool",
                    by_value=False,
                    lines=[
                        "\tbool has_result {};",
                        f"\tauto &has_result_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                        "\thas_result = has_result_opt.HasResult();",
                    ],
                )
            )
            continue
        optional_parens_ref = optional_parens_reference_node(child)
        if optional_parens_ref and is_semantic_rule(optional_parens_ref.name, rules, rule_types, primitive_rules):
            child_rule = optional_parens_ref.name
            var_name = to_snake_case(child_rule)
            cpp_type = semantic_rule_type(child_rule, rule_types, primitive_rules)
            by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
            value_expr = f"std::move({var_name}_value)" if by_value else f"{var_name}_value"
            if child_rule in primitive_rules:
                value_line = (
                    f"\t\tauto {var_name}_value = "
                    f"{primitive_value_expr(child_rule, f'ExtractResultFromParens({var_name}_opt.GetResult())', cpp_type, primitive_rules)};"
                )
            else:
                needs_child_slot = True
                value_line = f"\t\tauto {var_name}_value = frame.TakeResult<{cpp_type}>({stack_slot_expr});"
            lines = [
                f"\toptional<{cpp_type}> {var_name} {{}};",
                f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                f"\tif ({var_name}_opt.HasResult()) {{",
                value_line,
                f"\t\t{var_name} = {value_expr};",
            ]
            if child_rule not in primitive_rules:
                lines.append("\t\tchild_slot++;")
            lines.append("\t}")
            args.append(FinalizeArg(var_name, f"optional<{cpp_type}>", by_value, lines))
            continue
        parens_optional_ref = parens_optional_reference_node(child)
        if parens_optional_ref and is_semantic_rule(parens_optional_ref.name, rules, rule_types, primitive_rules):
            child_rule = parens_optional_ref.name
            var_name = to_snake_case(child_rule)
            cpp_type = semantic_rule_type(child_rule, rule_types, primitive_rules)
            by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
            value_expr = f"std::move({var_name}_value)" if by_value else f"{var_name}_value"
            if child_rule in primitive_rules:
                value_line = (
                    f"\t\tauto {var_name}_value = "
                    f"{primitive_value_expr(child_rule, f'{var_name}_opt.GetResult()', cpp_type, primitive_rules)};"
                )
            else:
                needs_child_slot = True
                value_line = f"\t\tauto {var_name}_value = frame.TakeResult<{cpp_type}>({stack_slot_expr});"
            lines = [
                f"\toptional<{cpp_type}> {var_name} {{}};",
                f"\tauto &{var_name}_opt = ExtractResultFromParens(list_pr.GetChild({child_idx})).Cast<OptionalParseResult>();",
                f"\tif ({var_name}_opt.HasResult()) {{",
                value_line,
                f"\t\t{var_name} = {value_expr};",
            ]
            if child_rule not in primitive_rules:
                lines.append("\t\tchild_slot++;")
            lines.append("\t}")
            args.append(FinalizeArg(var_name, f"optional<{cpp_type}>", by_value, lines))
            continue
        optional_sequence_ref = optional_sequence_reference_node(child)
        if optional_sequence_ref:
            sequence_child_idx, sequence_ref = optional_sequence_ref
            if is_semantic_rule(sequence_ref.name, rules, rule_types, primitive_rules):
                child_rule = sequence_ref.name
                var_name = to_snake_case(child_rule)
                cpp_type = semantic_rule_type(child_rule, rule_types, primitive_rules)
                by_value = semantic_rule_by_value(child_rule, rule_types, primitive_rules)
                value_expr = f"std::move({var_name}_value)" if by_value else f"{var_name}_value"
                source_expr = f"{var_name}_opt.GetResult().Cast<ListParseResult>().GetChild({sequence_child_idx})"
                if child_rule in primitive_rules:
                    value_line = (
                        f"\t\tauto {var_name}_value = "
                        f"{primitive_value_expr(child_rule, source_expr, cpp_type, primitive_rules)};"
                    )
                else:
                    needs_child_slot = True
                    value_line = f"\t\tauto {var_name}_value = frame.TakeResult<{cpp_type}>({stack_slot_expr});"
                lines = [
                    f"\toptional<{cpp_type}> {var_name} {{}};",
                    f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                    f"\tif ({var_name}_opt.HasResult()) {{",
                    value_line,
                    f"\t\t{var_name} = {value_expr};",
                ]
                if child_rule not in primitive_rules:
                    lines.append("\t\tchild_slot++;")
                lines.append("\t}")
                args.append(FinalizeArg(var_name, f"optional<{cpp_type}>", by_value, lines))
                continue
        if isinstance(child, OptionalNode) and isinstance(child.child, LiteralNode):
            var_name = f"has_result_{child_idx}"
            args.append(
                FinalizeArg(
                    var_name=var_name,
                    cpp_type="bool",
                    by_value=False,
                    lines=[
                        f"\tbool {var_name} {{}};",
                        f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                        f"\t{var_name} = {var_name}_opt.HasResult();",
                    ],
                )
            )
            continue
        if (
            isinstance(child, OptionalNode)
            and isinstance(child.child, RepeatNode)
            and isinstance(child.child.child, ReferenceNode)
            and is_transformer_rule(child.child.child.name, rules, rule_types, primitive_rules)
        ):
            child_rule = child.child.child.name
            var_name = to_snake_case(child_rule)
            cpp_type = rule_type(child_rule, rule_types)
            vector_by_value = rule_by_value(child_rule, rule_types)
            optional_value_expr = f"std::move({var_name}_value)" if vector_by_value else f"{var_name}_value"
            needs_child_slot = True
            args.append(
                FinalizeArg(
                    var_name=var_name,
                    cpp_type=f"optional<vector<{cpp_type}>>",
                    by_value=vector_by_value,
                    lines=[
                        f"\toptional<vector<{cpp_type}>> {var_name} {{}};",
                        f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                        f"\tif ({var_name}_opt.HasResult()) {{",
                        f"\t\tvector<{cpp_type}> {var_name}_value;",
                        f"\t\tauto &{var_name}_repeat = {var_name}_opt.GetResult().Cast<RepeatParseResult>();",
                        f"\t\tauto {var_name}_children = {var_name}_repeat.GetChildren();",
                        f"\t\tfor (idx_t child_idx = 0; child_idx < {var_name}_children.size(); child_idx++) {{",
                        f"\t\t\t{var_name}_value.push_back(frame.TakeResult<{cpp_type}>(child_slot + child_idx));",
                        "\t\t}",
                        f"\t\t{var_name} = {optional_value_expr};",
                        f"\t\tchild_slot += {var_name}_children.size();",
                        "\t}",
                    ],
                )
            )
            continue
        if (
            isinstance(child, RepeatNode)
            and isinstance(child.child, ReferenceNode)
            and is_transformer_rule(child.child.name, rules, rule_types, primitive_rules)
        ):
            child_rule = child.child.name
            var_name = to_snake_case(child_rule)
            cpp_type = rule_type(child_rule, rule_types)
            vector_by_value = rule_by_value(child_rule, rule_types)
            needs_child_slot = True
            args.append(
                FinalizeArg(
                    var_name=var_name,
                    cpp_type=f"vector<{cpp_type}>",
                    by_value=vector_by_value,
                    lines=[
                        f"\tvector<{cpp_type}> {var_name};",
                        f"\tauto &{var_name}_repeat = list_pr.GetChild({child_idx}).Cast<RepeatParseResult>();",
                        f"\tauto {var_name}_children = {var_name}_repeat.GetChildren();",
                        f"\tfor (idx_t child_idx = 0; child_idx < {var_name}_children.size(); child_idx++) {{",
                        f"\t\t{var_name}.push_back(frame.TakeResult<{cpp_type}>(child_slot + child_idx));",
                        "\t}",
                        f"\tchild_slot += {var_name}_children.size();",
                    ],
                )
            )
            continue
        raise NotImplementedError(f"unsupported finalize child at sequence index {child_idx}: {child}")
    return uniquify_finalize_args(args), needs_child_slot


def uniquify_finalize_args(args):
    seen = {}
    result = []
    for arg in args:
        count = seen.get(arg.var_name, 0)
        seen[arg.var_name] = count + 1
        if count == 0:
            result.append(arg)
            continue
        old_name = arg.var_name
        new_name = f"{old_name}_{count}"
        lines = [re.sub(rf"\b{re.escape(old_name)}\b", new_name, line) for line in arg.lines]
        lines = [line.replace(f".{new_name}", f".{old_name}") for line in lines]
        result.append(FinalizeArg(new_name, arg.cpp_type, arg.by_value, lines))
    return result


def generate_syntax_only_string_finalize(rule_name, ast, return_type, return_by_value):
    if return_type != "string":
        return None
    if isinstance(ast, LiteralNode):
        result_line = f'\tstring result = "{ast.text}";'
    elif is_literal_only_choice(ast):
        result_line = "\tauto result = choice_pr.GetResult().Cast<KeywordParseResult>().keyword;"
    else:
        return None
    lines = [
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}",
        f"    (PEGTransformer &transformer, TransformStackFrame &frame) {{",
    ]
    if is_literal_only_choice(ast):
        lines.extend(
            [
                "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();",
                "\tauto &choice_pr = list_pr.Child<ChoiceParseResult>(0);",
            ]
        )
    lines.append(result_line)
    lines.append(box_result(return_type, return_by_value))
    lines.append("}")
    return "\n".join(lines)


def generate_choice_finalize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules, return_type, return_by_value):
    alternatives = [alternative.name for alternative in ast.alternatives]
    has_primitive_alternative = any(name in primitive_rules for name in alternatives)
    has_excluded_alternative = any(
        name in excluded_rules and name not in rule_types and name not in primitive_rules for name in alternatives
    )
    if not has_primitive_alternative:
        transformer_alternatives = [
            name for name in alternatives if is_transformer_rule(name, rules, rule_types, primitive_rules)
        ]
        alternative_types = {rule_type(name, rule_types) for name in transformer_alternatives}
        body_name = transform_body_name(rule_name, 1)
        body_params = transform_body_param_types(rule_name) if body_name else []
        use_body = bool(body_name and len(body_params) == 1 and len(alternative_types) == 1)
        child_type = next(iter(alternative_types)) if use_body else return_type
        child_by_value = (
            any(rule_by_value(name, rule_types) for name in transformer_alternatives)
            if use_body
            else return_by_value
        )
        result_expr = "std::move(child)" if child_by_value else "child"
        lines = [
            f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}",
            f"    (PEGTransformer &transformer, TransformStackFrame &frame) {{",
        ]
        if has_excluded_alternative:
            lines.extend(
                [
                    "\tif (frame.child_results.empty()) {",
                    f"\t\t{return_type} result {{}};",
                    f"{box_result(return_type, return_by_value)}",
                    "\t}",
                ]
            )
        if use_body:
            lines.extend(
                [
                    f"\tauto child = frame.TakeResult<{child_type}>(0);",
                    f"\tauto result = {body_name}(transformer, {result_expr});",
                    f"{box_result(return_type, return_by_value)}",
                    "}",
                ]
            )
            return "\n".join(lines)
        lines.extend(
            [
                f"\tauto result = frame.TakeResult<{return_type}>(0);",
            ]
        )
        if return_type == "unique_ptr<SQLStatement>":
            lines.extend(
                [
                    "\tif (!transformer.named_parameter_map.empty()) {",
                    "\t\tresult->named_param_map = transformer.named_parameter_map;",
                    "\t}",
                ]
            )
        lines.extend(
            [
                f"{box_result(return_type, return_by_value)}",
                "}",
            ]
        )
        return "\n".join(lines)

    lines = [
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}",
        f"    (PEGTransformer &transformer, TransformStackFrame &frame) {{",
        "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();",
        "\tauto &choice_pr = list_pr.Child<ChoiceParseResult>(0);",
        "\tauto &choice_result = choice_pr.GetResult();",
    ]
    body_name = transform_body_name(rule_name, 1)
    body_params = transform_body_param_types(rule_name) if body_name else []
    primitive_alternatives = [name for name in alternatives if name in primitive_rules]
    if body_name and len(body_params) == 1 and body_params[0] in ("Identifier", "string"):
        child_type = body_params[0]
        lines.extend(
            generate_terminal_choice_extraction(
                "child", "choice_result", child_type, primitive_alternatives, primitive_rules
            )
        )
        lines.append(f"\tauto result = {body_name}(transformer, child);")
    elif return_type in ("Identifier", "string"):
        lines.extend(
            generate_terminal_choice_extraction(
                "result", "choice_result", return_type, primitive_alternatives, primitive_rules
            )
        )
    elif return_type == "unique_ptr<ParsedExpression>":
        lines.extend(
            [
                f"\t{return_type} result;",
                "\tif (choice_result.type == ParseResultType::NUMBER) {",
                "\t\tresult = TransformNumberLiteral(transformer, choice_result);",
                "\t} else {",
                f"\t\tresult = frame.TakeResult<{return_type}>(0);",
                "\t}",
            ]
        )
    else:
        lines.extend(
            [
                "\tif (frame.child_results.empty()) {",
                f'\t\tthrow InternalException("Primitive choice rule {rule_name} cannot produce {return_type}");',
                "\t}",
                f"\tauto result = frame.TakeResult<{return_type}>(0);",
            ]
        )
    lines.append(box_result(return_type, return_by_value))
    lines.append("}")
    return "\n".join(lines)


def generate_sequence_finalize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules):
    sequence = ensure_sequence(ast)
    return_type = rule_type(rule_name, rule_types)
    return_by_value = rule_by_value(rule_name, rule_types)
    args, needs_child_slot = sequence_finalize_args(
        rule_name, sequence, rules, rule_types, excluded_rules, primitive_rules
    )

    lines = [
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}",
        f"    (PEGTransformer &transformer, TransformStackFrame &frame) {{",
    ]
    if any("list_pr." in line or "list_pr " in line for arg in args for line in arg.lines):
        lines.append("\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();")
    if needs_child_slot:
        lines.append("\tidx_t child_slot = 0;")
    for arg in args:
        lines.extend(arg.lines)
    body_name = transform_body_name(rule_name, len(args) + (1 if rule_types[rule_name].pass_location else 0))
    if len(args) == 1 and args[0].cpp_type == return_type and not body_name:
        result_expr = f"std::move({args[0].var_name})" if args[0].by_value else args[0].var_name
        lines.append(f"\tauto result = {result_expr};")
    else:
        call_args = []
        for arg in args:
            call_args.append(f"std::move({arg.var_name})" if arg.by_value else arg.var_name)
        if rule_types[rule_name].pass_location:
            call_args.append("frame.parse_result.offset")
        arg_text = ", ".join(["transformer"] + call_args)
        lines.append(f"\tauto result = {body_name or f'Transform{rule_name}'}({arg_text});")
    lines.append(box_result(return_type, return_by_value))
    lines.append("}")
    return "\n".join(lines)


@lru_cache(maxsize=None)
def manual_transform_exists(rule_name):
    pattern = re.compile(rf"\bPEGTransformerFactory::Transform{re.escape(rule_name)}\s*\(")
    return any(pattern.search(path.read_text()) for path in transformer_dir.glob("transform_*.cpp"))


@lru_cache(maxsize=None)
def manual_transform_parse_result_return_types():
    one_line_pattern = re.compile(
        r"^([A-Za-z_][\w:<>, ]*)\s+PEGTransformerFactory::Transform([A-Za-z0-9_]+)\s*"
        r"\(([^)]*PEGTransformer\s*&[^)]*,[^)]*ParseResult\s*&[^)]*)\)",
        re.MULTILINE,
    )
    split_line_pattern = re.compile(
        r"^([A-Za-z_][\w:<>, ]*)\s*$\n^PEGTransformerFactory::Transform([A-Za-z0-9_]+)\s*"
        r"\(([^)]*PEGTransformer\s*&[^)]*,[^)]*ParseResult\s*&[^)]*)\)",
        re.MULTILINE,
    )
    result = {}
    for path in transformer_dir.glob("transform_*.cpp"):
        if path.name.startswith("transform_generated"):
            continue
        text = path.read_text()
        for pattern in (one_line_pattern, split_line_pattern):
            for match in pattern.finditer(text):
                function_rule_name = match.group(2)
                normalized_return_type = re.sub(r"\s+", " ", match.group(1)).strip()
                result[function_rule_name] = (match.group(0).split("PEGTransformerFactory::", 1)[1].split("(", 1)[0], normalized_return_type)
                if function_rule_name.endswith("Rule"):
                    result.setdefault(function_rule_name[: -len("Rule")], (match.group(0).split("PEGTransformerFactory::", 1)[1].split("(", 1)[0], normalized_return_type))
    return result


def manual_transform_parse_result_return_type(rule_name):
    entry = manual_transform_parse_result_return_types().get(rule_name)
    return entry[1] if entry else None


def manual_transform_parse_result_name(rule_name):
    entry = manual_transform_parse_result_return_types().get(rule_name)
    return entry[0] if entry else None


def manual_transform_parse_result_exists(rule_name, return_type=None):
    manual_return_type = manual_transform_parse_result_return_type(rule_name)
    if manual_return_type is None:
        return False
    return return_type is None or manual_return_type == return_type


def generate_finalize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules):
    return_type = rule_type(rule_name, rule_types)
    return_by_value = rule_by_value(rule_name, rule_types)
    if manual_transform_parse_result_exists(rule_name, return_type):
        body_name = manual_transform_parse_result_name(rule_name)
        parse_result_lines = []
        parse_result_expr = "frame.parse_result"
        if isinstance(ast, ChoiceNode):
            parse_result_lines = [
                "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();",
                "\tauto &choice_pr = list_pr.Child<ChoiceParseResult>(0);",
            ]
            parse_result_expr = "choice_pr.GetResult()"
        return (
            f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
            f"{chr(10).join(parse_result_lines)}\n"
            f"\tauto result = {body_name}(transformer, {parse_result_expr});\n"
            f"{box_result(return_type, return_by_value)}\n"
            f"}}"
        )
    if (
        manual_transform_exists(rule_name)
        and not transform_body_param_types(rule_name)
        and not ast_has_semantic_rule(ast, rules, rule_types, primitive_rules)
    ):
        return (
            f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
            f"\tauto result = Transform{rule_name}(transformer);\n"
            f"{box_result(return_type, return_by_value)}\n"
            f"}}"
        )
    syntax_only_string_finalize = generate_syntax_only_string_finalize(rule_name, ast, return_type, return_by_value)
    if syntax_only_string_finalize:
        return syntax_only_string_finalize
    if is_literal_only_choice(ast) and manual_transform_exists(rule_name):
        return (
            f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
            f"\tauto result = Transform{rule_name}(transformer);\n"
            f"{box_result(return_type, return_by_value)}\n"
            f"}}"
        )
    if rule_name in RAW_PARSE_RESULT_RULES:
        if rule_name == "CommentValue":
            return (
                f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
                f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
                "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();\n"
                "\tauto &choice_pr = list_pr.Child<ChoiceParseResult>(0);\n"
                "\tValue result;\n"
                "\tif (choice_pr.GetResult().type == ParseResultType::STRING) {\n"
                "\t\tresult = Value(choice_pr.GetResult().Cast<StringLiteralParseResult>().result);\n"
                "\t} else {\n"
                "\t\tresult = TransformNullLiteral(transformer);\n"
                "\t}\n"
                f"{box_result(return_type, return_by_value)}\n"
                f"}}"
            )
        return (
            f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
            f"\tauto result = Transform{rule_name}(transformer, frame.parse_result);\n"
            f"{box_result(return_type, return_by_value)}\n"
            f"}}"
        )
    if is_pure_reference_choice(ast):
        return generate_choice_finalize(
            rule_name, ast, rules, rule_types, excluded_rules, primitive_rules, return_type, return_by_value
        )
    return generate_sequence_finalize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules)


def generated_block(begin_label, body):
    return f"// BEGIN {begin_label}\n{body.rstrip()}\n// END {begin_label}\n"


def generate_trampoline_header_section(grammar_names):
    context = load_rule_context(grammar_names)
    lines = []
    for rule_name in context.emit_rule_names:
        lines.extend(generate_trampoline_rule_declarations(rule_name))
    return "\n".join(lines) + "\n"


def generate_trampoline_source_section(grammar_names):
    context = load_rule_context(grammar_names)
    lines = []
    for rule_name in context.emit_rule_names:
        lines.append(generate_ops_definition(rule_name))
    lines.append("")
    for rule_name in context.emit_rule_names:
        ast = rule_to_ast(context.rules[rule_name])
        lines.append(
            generate_initialize(
                rule_name, ast, context.rules, context.rule_types, context.excluded_rules, context.primitive_rules
            )
        )
        lines.append("")
        lines.append(
            generate_finalize(
                rule_name,
                ast,
                context.rules,
                context.rule_types,
                context.excluded_rules,
                context.primitive_rules,
            )
        )
        lines.append("")

    ops_registry = [
        "const case_insensitive_map_t<const TransformFrameOps *> &PEGTransformerFactory::GeneratedTrampolineOps() {",
        "\tstatic const case_insensitive_map_t<const TransformFrameOps *> rule_ops = {",
    ]
    for rule_name in context.emit_rule_names:
        ops_registry.append(f'\t    {{"{rule_name}", &{ops_name(rule_name)}}},')
    ops_registry.extend(
        [
            "\t};",
            "\treturn rule_ops;",
            "}",
            "",
        ]
    )
    return "\n".join(lines + ops_registry).rstrip() + "\n"


@dataclass
class RuleContext:
    rules: dict
    rule_types: dict
    excluded_rules: set
    primitive_rules: dict
    emit_rule_names: list[str]
    source_files: list[Path]


def load_rule_file(grammar_name):
    grammar_file = statements_dir / grammar_name
    rules = parse_peg_grammar(grammar_file.read_text())
    return grammar_file, rules


def load_rule_context(grammar_names):
    grammar_types_file = type_dir / "grammar_types.yml"
    rule_types, excluded_rules = load_grammar_types(grammar_types_file)
    primitive_rules = load_primitive_override_rules(grammar_types_file)
    rules = {}
    emit_rule_names = []
    source_files = []
    loaded_grammars = {}
    rule_sources = build_rule_source_index()

    def load_grammar(grammar_name):
        if grammar_name not in loaded_grammars:
            grammar_file, grammar_rules = load_rule_file(grammar_name)
            loaded_grammars[grammar_name] = (grammar_file, grammar_rules)
            source_files.append(grammar_file)
            for rule_name, rule in grammar_rules.items():
                existing = rules.get(rule_name)
                if existing is not None and existing.expression != rule.expression:
                    raise RuntimeError(f"duplicate grammar rule with different bodies: {rule_name}")
                rules[rule_name] = rule
        return loaded_grammars[grammar_name]

    def ensure_rule_loaded(rule_name):
        if rule_name in rules:
            return
        grammar_name = TRAMPOLINE_HELPER_RULE_SOURCES.get(rule_name) or rule_sources.get(rule_name)
        if grammar_name is None:
            return
        load_grammar(grammar_name)

    def add_emit_rule(rule_name):
        if rule_name in TRAMPOLINE_EXCLUDED_RULES:
            return
        if rule_name in primitive_rules:
            return
        if rule_name not in rule_types or rule_name in emit_rule_names:
            return
        ensure_rule_loaded(rule_name)
        if rule_name not in rules:
            raise RuntimeError(f"typed rule {rule_name} is not defined in any statement grammar")
        emit_rule_names.append(rule_name)

    for grammar_name in grammar_names:
        _, grammar_rules = load_grammar(grammar_name)
        for rule_name in grammar_rules:
            add_emit_rule(rule_name)

    processed_rule_names = set()
    for rule_name in emit_rule_names:
        if rule_name in processed_rule_names:
            continue
        processed_rule_names.add(rule_name)
        ast = rule_to_ast(rules[rule_name])
        for dependency_name in referenced_rule_names(ast):
            if dependency_name in primitive_rules:
                continue
            if dependency_name in rule_types:
                add_emit_rule(dependency_name)
            else:
                ensure_rule_loaded(dependency_name)

    for rule_name, grammar_name in TRAMPOLINE_HELPER_RULE_SOURCES.items():
        _, grammar_rules = load_grammar(grammar_name)
        if rule_name not in grammar_rules:
            raise RuntimeError(f"helper rule {rule_name} not found in {grammar_name}")
        if rule_name in rule_types and rule_name not in emit_rule_names:
            emit_rule_names.append(rule_name)

    for rule_name, rule in rules.items():
        if rule_name in rule_types:
            rule.return_type = rule_types[rule_name].cpp_type
    return RuleContext(rules, rule_types, excluded_rules, primitive_rules, emit_rule_names, source_files)


def build_rule_source_index():
    rule_sources = {}
    for grammar_file in sorted(statements_dir.glob("*.gram")):
        grammar_rules = parse_peg_grammar(grammar_file.read_text())
        for rule_name in grammar_rules:
            existing = rule_sources.get(rule_name)
            if existing is not None:
                raise RuntimeError(f"duplicate grammar rule {rule_name} in {existing} and {grammar_file.name}")
            rule_sources[rule_name] = grammar_file.name
    return rule_sources


def referenced_rule_names(ast):
    if isinstance(ast, ReferenceNode):
        return [ast.name]
    if isinstance(ast, SequenceNode):
        result = []
        for child in ast.children:
            result.extend(referenced_rule_names(child))
        return result
    if isinstance(ast, ChoiceNode):
        result = []
        for alternative in ast.alternatives:
            result.extend(referenced_rule_names(alternative))
        return result
    if isinstance(ast, (OptionalNode, RepeatNode)):
        return referenced_rule_names(ast.child)
    if isinstance(ast, (ParensNode, ListMacroNode, FunctionCallNode)):
        return referenced_rule_names(ast.inner)
    if isinstance(ast, LiteralNode):
        return []
    raise NotImplementedError(f"unsupported grammar node while collecting dependencies: {ast}")


def replace_between_markers(text, label, replacement):
    begin = f"// BEGIN {label}\n"
    end = f"// END {label}\n"
    start = text.find(begin)
    if start == -1:
        return None
    finish = text.find(end, start)
    if finish == -1:
        raise RuntimeError(f"found start marker for {label}, but no end marker")
    finish += len(end)
    return text[:start] + generated_block(label, replacement) + text[finish:]


def replace_between(text, start_text, end_text, replacement):
    start = text.find(start_text)
    if start == -1:
        raise RuntimeError(f"could not find replacement start: {start_text!r}")
    finish = text.find(end_text, start)
    if finish == -1:
        raise RuntimeError(f"could not find replacement end after start: {end_text!r}")
    return text[:start] + replacement + "\n" + text[finish:]


def generate_trampoline_source_file(grammar_names):
    source_section = generate_trampoline_source_section(grammar_names)
    context = load_rule_context(grammar_names)
    registrations = ['\ttrampoline_transform_functions["Statement"] = &PEGTransformerFactory::TransformStatementTrampolineInternal;']
    for rule_name in sorted(context.primitive_rules):
        if rule_name not in context.rule_types:
            continue
        return_type = primitive_cpp_type(rule_name, context.rule_types, context.primitive_rules)
        return_by_value = primitive_by_value(rule_name, context.rule_types, context.primitive_rules)
        registrations.extend(
            [
                f'\ttrampoline_transform_functions["{rule_name}"] = [](PEGTransformer &transformer,',
                "\t                                                ParseResult &parse_result) -> unique_ptr<TransformResultValue> {",
                f"\t\tauto result = {primitive_value_expr(rule_name, 'parse_result', return_type, context.primitive_rules)};",
                f"{box_result(return_type, return_by_value)}",
                "\t};",
            ]
        )
    for rule_name in context.emit_rule_names:
        registrations.extend(
            [
                f'\ttrampoline_transform_functions["{rule_name}"] = [](PEGTransformer &transformer,',
                "\t                                                ParseResult &parse_result) -> unique_ptr<TransformResultValue> {",
                "\t\tTransformStack stack(transformer);",
                f"\t\treturn stack.Execute(parse_result, {ops_name(rule_name)});",
                "\t};",
            ]
        )
    return (
        '#include "duckdb/parser/peg/transformer/peg_transformer.hpp"\n'
        "\n"
        "namespace duckdb {\n"
        "\n"
        f"{generated_block('generated trampoline transformer rules', source_section)}"
        "\n"
        "void PEGTransformerFactory::RegisterGeneratedTrampoline() {\n"
        f"{chr(10).join(registrations)}\n"
        "}\n"
        "\n"
        "} // namespace duckdb\n"
    )


def write_generated_files():
    header_section = generate_trampoline_header_section(TRAMPOLINE_GRAMMARS)
    header_text = transformer_header.read_text()
    new_header_text = replace_between_markers(
        header_text, "generated trampoline transformer declarations", header_section
    )
    if new_header_text is None:
        raise RuntimeError("could not find generated trampoline transformer declarations marker")
    transformer_header.write_text(new_header_text)
    transformer_trampoline_source.write_text(generate_trampoline_source_file(TRAMPOLINE_GRAMMARS))


def main():
    parser = argparse.ArgumentParser(description="Preview trampoline-style transformer generation.")
    parser.add_argument(
        "--grammar",
        default="use.gram",
        choices=TRAMPOLINE_GRAMMARS,
        help="Grammar file to preview. --write always writes the full trampoline backend.",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Write the generated trampoline declarations and dedicated trampoline source.",
    )
    args = parser.parse_args()
    if args.write:
        write_generated_files()
        return
    context = load_rule_context([args.grammar])
    header_section = "\n".join(
        line for rule_name in context.emit_rule_names for line in generate_trampoline_rule_declarations(rule_name)
    )
    source_section = generate_trampoline_source_section([args.grammar])
    source_files = ", ".join(str(source_file) for source_file in context.source_files)
    print(
        "\n".join(
            [
                "// AUTO-GENERATED PREVIEW by scripts/parser/generate_transformer_trampoline.py",
                f"// Source grammars: {source_files}",
                "",
                "// Header declarations:",
                header_section.rstrip(),
                "",
                "// C++ definitions:",
                source_section.rstrip(),
            ]
        )
        + "\n",
        end="",
    )


if __name__ == "__main__":
    main()
