import argparse
import re
import sys
from dataclasses import dataclass
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

TRAMPOLINE_GRAMMARS = [
    "checkpoint.gram",
    "connect.gram",
    "deallocate.gram",
    "detach.gram",
    "transaction.gram",
    "use.gram",
]
TRAMPOLINE_HELPER_RULE_SOURCES = {}


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


@dataclass
class RepeatStackChild:
    parse_expr: str
    rule_name: str
    var_name: str
    cpp_type: str
    by_value: bool
    optional: bool


@dataclass
class FinalizeArg:
    var_name: str
    cpp_type: str
    by_value: bool
    lines: list[str]


def load_primitive_override_rules(grammar_types_file):
    matcher_overrides = load_matcher_rule_overrides(grammar_types_file)
    primitive_rules = {}
    for rule_name, info in matcher_overrides.items():
        if not isinstance(info, dict):
            continue
        matcher = info.get("matcher")
        if matcher in ("identifier", "reserved_identifier"):
            primitive_rules[rule_name] = ("Identifier", "IdentifierParseResult", "identifier")
        elif matcher == "string_literal":
            primitive_rules[rule_name] = ("string", "StringLiteralParseResult", "GetRawString()")
    return primitive_rules


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


def is_transformer_rule(rule_name, rules, rule_types):
    return rule_name in rules and rule_name in rule_types


def child_ops(rule_name):
    return ops_name(rule_name)


def rule_type(rule_name, rule_types):
    return rule_types[rule_name].cpp_type


def rule_by_value(rule_name, rule_types):
    info = rule_types[rule_name]
    return info.by_value or "unique_ptr<" in info.cpp_type


def ensure_sequence(ast):
    if isinstance(ast, SequenceNode):
        return ast
    return SequenceNode([ast])


def sequence_stack_children(sequence, rules, rule_types):
    children = []

    for child_idx, child in enumerate(sequence.children):
        if isinstance(child, ReferenceNode) and is_transformer_rule(child.name, rules, rule_types):
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
            and is_transformer_rule(child.child.name, rules, rule_types)
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
            and is_transformer_rule(child.child.child.name, rules, rule_types)
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
            and is_transformer_rule(child.child.name, rules, rule_types)
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


def generate_choice_initialize(rule_name, ast, rules, rule_types):
    alternatives = [alternative.name for alternative in ast.alternatives]
    unsupported = [name for name in alternatives if not is_transformer_rule(name, rules, rule_types)]
    if unsupported:
        raise NotImplementedError(f"{rule_name}: unsupported choice alternatives: {unsupported}")

    lines = [
        f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,",
        "                                                TransformStackFrame &frame) {",
        "\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();",
        "\tauto &choice_pr = list_pr.Child<ChoiceParseResult>(0);",
        "\tframe.child_results.resize(1);",
        "\tauto &choice_result = choice_pr.GetResult();",
        "\tauto &trampoline_ops = GeneratedTrampolineOps();",
        "\tauto entry = trampoline_ops.find(choice_result.name);",
        "\tD_ASSERT(entry != trampoline_ops.end());",
        "\tstack.PushFrame(choice_result, *entry->second, frame, 0);",
        "}",
    ]
    return "\n".join(lines)


def generate_sequence_initialize(rule_name, ast, rules, rule_types):
    sequence = ensure_sequence(ast)
    stack_children = sequence_stack_children(sequence, rules, rule_types)
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
            lines.extend(
                [
                    f"\tif ({var_name}.HasResult()) {{",
                    f"\t\tstack.PushFrame({var_name}.GetResult(), {child_ops(child.rule_name)}, parent_frame_index, child_slot);",
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
    lines.append("}")
    return "\n".join(lines)


def generate_initialize(rule_name, ast, rules, rule_types):
    if is_pure_reference_choice(ast):
        return generate_choice_initialize(rule_name, ast, rules, rule_types)
    return generate_sequence_initialize(rule_name, ast, rules, rule_types)


def sequence_finalize_args(sequence, rules, rule_types, excluded_rules, primitive_rules):
    args = []
    stack_slot_expr = "child_slot"
    needs_child_slot = False
    for child_idx, child in enumerate(sequence.children):
        if isinstance(child, LiteralNode):
            continue
        if isinstance(child, ReferenceNode):
            if child.name in excluded_rules:
                continue
            if child.name in primitive_rules:
                cpp_type, parse_result_type, accessor = primitive_rules[child.name]
                var_name = to_snake_case(child.name)
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=cpp_type,
                        by_value=False,
                        lines=[
                            (
                                f"\tauto {var_name} = "
                                f"list_pr.GetChild({child_idx}).Cast<{parse_result_type}>().{accessor};"
                            )
                        ],
                    )
                )
                continue
            if is_transformer_rule(child.name, rules, rule_types):
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
            if child.child.name in excluded_rules:
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
                cpp_type, parse_result_type, accessor = primitive_rules[child.child.name]
                var_name = to_snake_case(child.child.name)
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=f"optional<{cpp_type}>",
                        by_value=False,
                        lines=[
                            f"\toptional<{cpp_type}> {var_name} {{}};",
                            f"\tauto &{var_name}_opt = list_pr.GetChild({child_idx}).Cast<OptionalParseResult>();",
                            f"\tif ({var_name}_opt.HasResult()) {{",
                            (
                                f"\t\tauto {var_name}_value = "
                                f"{var_name}_opt.GetResult().Cast<{parse_result_type}>().{accessor};"
                            ),
                            f"\t\t{var_name} = {var_name}_value;",
                            "\t}",
                        ],
                    )
                )
                continue
            if is_transformer_rule(child.child.name, rules, rule_types):
                needs_child_slot = True
                child_rule = child.child.name
                var_name = to_snake_case(child_rule)
                cpp_type = rule_type(child_rule, rule_types)
                by_value = rule_by_value(child_rule, rule_types)
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
                            f"\t\tauto {var_name}_value = frame.TakeResult<{cpp_type}>({stack_slot_expr});",
                            f"\t\t{var_name} = {value_expr};",
                            "\t\tchild_slot++;",
                            "\t}",
                        ],
                    )
                )
                continue
        if (
            isinstance(child, OptionalNode)
            and isinstance(child.child, RepeatNode)
            and isinstance(child.child.child, ReferenceNode)
            and is_transformer_rule(child.child.child.name, rules, rule_types)
        ):
            child_rule = child.child.child.name
            var_name = to_snake_case(child_rule)
            cpp_type = rule_type(child_rule, rule_types)
            needs_child_slot = True
            args.append(
                FinalizeArg(
                    var_name=var_name,
                    cpp_type=f"optional<vector<{cpp_type}>>",
                    by_value=False,
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
                        f"\t\t{var_name} = {var_name}_value;",
                        f"\t\tchild_slot += {var_name}_children.size();",
                        "\t}",
                    ],
                )
            )
            continue
        if (
            isinstance(child, RepeatNode)
            and isinstance(child.child, ReferenceNode)
            and is_transformer_rule(child.child.name, rules, rule_types)
        ):
            child_rule = child.child.name
            var_name = to_snake_case(child_rule)
            cpp_type = rule_type(child_rule, rule_types)
            needs_child_slot = True
            args.append(
                FinalizeArg(
                    var_name=var_name,
                    cpp_type=f"vector<{cpp_type}>",
                    by_value=False,
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
    return args, needs_child_slot


def generate_choice_finalize(rule_name, return_type, return_by_value):
    return (
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
        f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
        f"\tauto result = frame.TakeResult<{return_type}>(0);\n"
        f"{box_result(return_type, return_by_value)}\n"
        f"}}"
    )


def generate_sequence_finalize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules):
    sequence = ensure_sequence(ast)
    return_type = rule_type(rule_name, rule_types)
    return_by_value = rule_by_value(rule_name, rule_types)
    args, needs_child_slot = sequence_finalize_args(
        sequence, rules, rule_types, excluded_rules, primitive_rules
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
    call_args = []
    for arg in args:
        call_args.append(f"std::move({arg.var_name})" if arg.by_value else arg.var_name)
    arg_text = ", ".join(["transformer"] + call_args)
    lines.append(f"\tauto result = Transform{rule_name}({arg_text});")
    lines.append(box_result(return_type, return_by_value))
    lines.append("}")
    return "\n".join(lines)


def generate_finalize(rule_name, ast, rules, rule_types, excluded_rules, primitive_rules):
    return_type = rule_type(rule_name, rule_types)
    return_by_value = rule_by_value(rule_name, rule_types)
    if is_pure_reference_choice(ast):
        return generate_choice_finalize(rule_name, return_type, return_by_value)
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
        lines.append(generate_initialize(rule_name, ast, context.rules, context.rule_types))
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
    return (
        '#include "duckdb/parser/peg/transformer/peg_transformer.hpp"\n'
        "\n"
        "namespace duckdb {\n"
        "\n"
        f"{generated_block('generated trampoline transformer rules', source_section)}"
        "\n"
        "void PEGTransformerFactory::RegisterGeneratedTrampoline() {\n"
        '\ttrampoline_transform_functions["Statement"] = &PEGTransformerFactory::TransformStatementTrampolineInternal;\n'
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
