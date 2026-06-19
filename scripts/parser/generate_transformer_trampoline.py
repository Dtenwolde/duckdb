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
    LiteralNode,
    OptionalNode,
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

# Pilot scope: TransformStatement is currently hand-written and creates the stack
# for converted statement alternatives. Generated use.gram Internal functions
# remain compatibility entrypoints while both backends coexist.
ROOT_RULES = set()


@dataclass
class StackChild:
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


@dataclass
class FinalizeArg:
    var_name: str
    cpp_type: str
    by_value: bool
    lines: list[str]


def load_identifier_override_rules(grammar_types_file):
    matcher_overrides = load_matcher_rule_overrides(grammar_types_file)
    return {
        rule_name
        for rule_name, info in matcher_overrides.items()
        if isinstance(info, dict) and info.get("matcher") in ("identifier", "reserved_identifier")
    }


def to_upper_snake_case(name):
    value = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value).upper()


def ops_name(rule_name):
    return f"{to_upper_snake_case(rule_name)}_OPS"


def initialize_name(rule_name):
    return f"Initialize{rule_name}"


def finalize_name(rule_name):
    return f"Finalize{rule_name}"


def internal_name(rule_name):
    return f"Transform{rule_name}Internal"


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
    stack_children = []
    repeat_child = None
    static_slot = 0

    for child_idx, child in enumerate(sequence.children):
        if isinstance(child, ReferenceNode) and is_transformer_rule(child.name, rules, rule_types):
            stack_children.append(
                StackChild(
                    parse_expr=f"list_pr.GetChild({child_idx})",
                    rule_name=child.name,
                    slot_idx=static_slot,
                )
            )
            static_slot += 1
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
            )

    if stack_children and repeat_child:
        raise NotImplementedError("mixed static and repeated stack children are not supported yet")
    return stack_children, repeat_child


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


def generate_rule_declarations(rule_name, ast, rules, rule_types, identifier_override_rules):
    return_type = rule_type(rule_name, rule_types)
    lines = [
        f"\tstatic const TransformFrameOps {ops_name(rule_name)};",
        declaration_line(
            f"static unique_ptr<TransformResultValue> {internal_name(rule_name)}"
            f"(PEGTransformer &transformer, ParseResult &parse_result)"
        ),
        declaration_line(
            f"static void {initialize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStack &stack, TransformStackFrame &frame)"
        ),
        declaration_line(
            f"static unique_ptr<TransformResultValue> {finalize_name(rule_name)}"
            f"(PEGTransformer &transformer, TransformStackFrame &frame)"
        ),
    ]
    if not is_pure_reference_choice(ast):
        args = sequence_finalize_args(ensure_sequence(ast), rules, rule_types, identifier_override_rules)
        semantic_args = ["PEGTransformer &transformer"] + format_cpp_args(args)
        lines.append(declaration_line(f"static {return_type} Transform{rule_name}({', '.join(semantic_args)})"))
    return lines


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
    ]
    for idx, alternative in enumerate(alternatives):
        prefix = "\tif" if idx == 0 else "\t} else if"
        lines.extend(
            [
                f'{prefix} (choice_result.name == "{alternative}") {{',
                f"\t\tstack.PushFrame(choice_result, {child_ops(alternative)}, frame, 0);",
            ]
        )
    lines.extend(
        [
            "\t} else {",
            f'\t\tthrow InternalException("Unexpected {rule_name} alternative \'%s\'", choice_result.name);',
            "\t}",
            "}",
        ]
    )
    return "\n".join(lines)


def generate_sequence_initialize(rule_name, ast, rules, rule_types):
    sequence = ensure_sequence(ast)
    stack_children, repeat_child = sequence_stack_children(sequence, rules, rule_types)
    lines = [
        f"void PEGTransformerFactory::{initialize_name(rule_name)}(PEGTransformer &transformer, TransformStack &stack,",
        "                                                TransformStackFrame &frame) {",
    ]
    if not stack_children and repeat_child is None:
        lines.append("}")
        return "\n".join(lines)

    lines.append("\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();")
    if stack_children:
        lines.append(f"\tframe.child_results.resize({len(stack_children)});")
        for child in reversed(stack_children):
            lines.append(
                f"\tstack.PushFrame({child.parse_expr}, {child_ops(child.rule_name)}, frame, {child.slot_idx});"
            )
    if repeat_child is not None:
        lines.extend(
            [
                f"\tauto &{repeat_child.var_name}_opt = {repeat_child.parse_expr}.Cast<OptionalParseResult>();",
                f"\tif (!{repeat_child.var_name}_opt.HasResult()) {{",
                "\t\treturn;",
                "\t}",
                (
                    f"\tauto &{repeat_child.var_name}_repeat = "
                    f"{repeat_child.var_name}_opt.GetResult().Cast<RepeatParseResult>();"
                ),
                f"\tauto {repeat_child.var_name}_children = {repeat_child.var_name}_repeat.GetChildren();",
                f"\tframe.child_results.resize({repeat_child.var_name}_children.size());",
                "\tauto parent_frame_index = frame.frame_index;",
                f"\tfor (idx_t child_idx = {repeat_child.var_name}_children.size(); child_idx > 0; child_idx--) {{",
                "\t\tauto result_idx = child_idx - 1;",
                (
                    f"\t\tstack.PushFrame({repeat_child.var_name}_children[result_idx].get(), "
                    f"{child_ops(repeat_child.rule_name)}, parent_frame_index, result_idx);"
                ),
                "\t}",
            ]
        )
    lines.append("}")
    return "\n".join(lines)


def generate_initialize(rule_name, ast, rules, rule_types):
    if is_pure_reference_choice(ast):
        return generate_choice_initialize(rule_name, ast, rules, rule_types)
    return generate_sequence_initialize(rule_name, ast, rules, rule_types)


def sequence_finalize_args(sequence, rules, rule_types, identifier_override_rules):
    args = []
    stack_slot = 0
    for child_idx, child in enumerate(sequence.children):
        if isinstance(child, LiteralNode):
            continue
        if isinstance(child, ReferenceNode):
            if child.name in identifier_override_rules:
                var_name = to_snake_case(child.name)
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type="Identifier",
                        by_value=False,
                        lines=[
                            (
                                f"\tauto {var_name} = "
                                f"list_pr.GetChild({child_idx}).Cast<IdentifierParseResult>().identifier;"
                            )
                        ],
                    )
                )
                continue
            if is_transformer_rule(child.name, rules, rule_types):
                var_name = to_snake_case(child.name)
                args.append(
                    FinalizeArg(
                        var_name=var_name,
                        cpp_type=rule_type(child.name, rule_types),
                        by_value=rule_by_value(child.name, rule_types),
                        lines=[
                            (
                                f"\tauto {var_name} = "
                                f"frame.TakeResult<{rule_type(child.name, rule_types)}>({stack_slot});"
                            )
                        ],
                    )
                )
                stack_slot += 1
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
                        "\t\tfor (idx_t child_idx = 0; child_idx < frame.child_results.size(); child_idx++) {",
                        f"\t\t\t{var_name}_value.push_back(frame.TakeResult<{cpp_type}>(child_idx));",
                        "\t\t}",
                        f"\t\t{var_name} = {var_name}_value;",
                        "\t}",
                    ],
                )
            )
            continue
        raise NotImplementedError(f"unsupported finalize child at sequence index {child_idx}: {child}")
    return args


def generate_choice_finalize(rule_name, return_type, return_by_value):
    return (
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}"
        f"(PEGTransformer &transformer, TransformStackFrame &frame) {{\n"
        f"\tauto result = frame.TakeResult<{return_type}>(0);\n"
        f"{box_result(return_type, return_by_value)}\n"
        f"}}"
    )


def generate_sequence_finalize(rule_name, ast, rules, rule_types, identifier_override_rules):
    sequence = ensure_sequence(ast)
    return_type = rule_type(rule_name, rule_types)
    return_by_value = rule_by_value(rule_name, rule_types)
    args = sequence_finalize_args(sequence, rules, rule_types, identifier_override_rules)

    lines = [
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{finalize_name(rule_name)}",
        f"    (PEGTransformer &transformer, TransformStackFrame &frame) {{",
    ]
    if any("list_pr." in line or "list_pr " in line for arg in args for line in arg.lines):
        lines.append("\tauto &list_pr = frame.parse_result.Cast<ListParseResult>();")
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


def generate_finalize(rule_name, ast, rules, rule_types, identifier_override_rules):
    return_type = rule_type(rule_name, rule_types)
    return_by_value = rule_by_value(rule_name, rule_types)
    if is_pure_reference_choice(ast):
        return generate_choice_finalize(rule_name, return_type, return_by_value)
    return generate_sequence_finalize(rule_name, ast, rules, rule_types, identifier_override_rules)


def generate_internal(rule_name, is_root):
    if is_root:
        comment = "// Root trampoline entrypoint: creates the stack for this traversal."
    else:
        comment = "// Compatibility entrypoint: not used by generated trampoline child traversal."
    return (
        f"{comment}\n"
        f"unique_ptr<TransformResultValue> PEGTransformerFactory::{internal_name(rule_name)}"
        f"(PEGTransformer &transformer, ParseResult &parse_result) {{\n"
        "\tTransformStack stack(transformer);\n"
        f"\treturn stack.Execute(parse_result, {ops_name(rule_name)});\n"
        "}"
    )


def generated_block(begin_label, body):
    return f"// BEGIN {begin_label}\n{body.rstrip()}\n// END {begin_label}\n"


def generate_header_section(rules, rule_asts, rule_types, identifier_override_rules):
    lines = []
    for rule_name, ast in rule_asts.items():
        lines.extend(generate_rule_declarations(rule_name, ast, rules, rule_types, identifier_override_rules))
    return "\n".join(lines) + "\n"


def generate_source_section(rules, rule_asts, rule_types, identifier_override_rules):
    lines = []
    for rule_name in rules:
        lines.append(generate_ops_definition(rule_name))
    lines.append("")
    for rule_name, ast in rule_asts.items():
        lines.append(generate_initialize(rule_name, ast, rules, rule_types))
        lines.append("")
        lines.append(generate_finalize(rule_name, ast, rules, rule_types, identifier_override_rules))
        lines.append("")
    for rule_name in rules:
        lines.append(generate_internal(rule_name, rule_name in ROOT_RULES))
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def load_use_rules():
    grammar_types_file = type_dir / "grammar_types.yml"
    rule_types, excluded_rules = load_grammar_types(grammar_types_file)
    identifier_override_rules = load_identifier_override_rules(grammar_types_file)
    grammar_file = statements_dir / "use.gram"
    rules = parse_peg_grammar(grammar_file.read_text())

    for rule_name, rule in rules.items():
        if rule_name in rule_types:
            rule.return_type = rule_types[rule_name].cpp_type
    return grammar_file, rules, rule_types, excluded_rules, identifier_override_rules


def generate_preview():
    grammar_file, rules, rule_types, excluded_rules, identifier_override_rules = load_use_rules()
    del excluded_rules

    rule_asts = {rule_name: rule_to_ast(rule) for rule_name, rule in rules.items()}
    header_section = generate_header_section(rules, rule_asts, rule_types, identifier_override_rules)
    source_section = generate_source_section(rules, rule_asts, rule_types, identifier_override_rules)

    lines = [
        "// AUTO-GENERATED PREVIEW by scripts/parser/generate_transformer_trampoline.py",
        "// This script is intentionally review-only and does not write generated files.",
        f"// Source grammar: {grammar_file}",
        "",
        "// Header declarations:",
        header_section.rstrip(),
        "",
        "// C++ definitions:",
        source_section.rstrip(),
    ]
    return "\n".join(lines).rstrip() + "\n"


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


def write_generated_files():
    _, rules, rule_types, excluded_rules, identifier_override_rules = load_use_rules()
    del excluded_rules

    rule_asts = {rule_name: rule_to_ast(rule) for rule_name, rule in rules.items()}
    label = "generated trampoline transformer rules for use.gram"
    header_section = generate_header_section(rules, rule_asts, rule_types, identifier_override_rules)
    source_section = generate_source_section(rules, rule_asts, rule_types, identifier_override_rules)

    header_text = transformer_header.read_text()
    header_replacement = generated_block(label, header_section)
    new_header_text = replace_between_markers(header_text, label, header_section)
    if new_header_text is None:
        new_header_text = replace_between(
            header_text,
            "\tstatic const TransformFrameOps USE_STATEMENT_OPS;",
            "\tstatic unique_ptr<TransformResultValue> TransformVacuumStatementInternal",
            header_replacement,
        )
    transformer_header.write_text(new_header_text)

    source_text = transformer_source.read_text()
    source_replacement = generated_block(label, source_section)
    new_source_text = replace_between_markers(source_text, label, source_section)
    if new_source_text is None:
        new_source_text = replace_between(
            source_text,
            "const TransformFrameOps PEGTransformerFactory::USE_STATEMENT_OPS",
            "unique_ptr<TransformResultValue> PEGTransformerFactory::TransformVacuumStatementInternal",
            source_replacement,
        )
    transformer_source.write_text(new_source_text)


def main():
    parser = argparse.ArgumentParser(description="Preview trampoline-style transformer generation for use.gram.")
    parser.add_argument(
        "--grammar",
        default="use.gram",
        choices=["use.gram"],
        help="Grammar file to preview. Only use.gram is supported by this pilot.",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="Write the generated use.gram trampoline blocks into the transformer header/source.",
    )
    args = parser.parse_args()
    if args.write:
        write_generated_files()
        return
    print(generate_preview(), end="")


if __name__ == "__main__":
    main()
