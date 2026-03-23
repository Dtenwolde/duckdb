# Extensible PEG Grammar — Implementation Plan

All paths are relative to `extension/autocomplete/` unless otherwise noted.

---

## Overview

The goal is to let external DuckDB extensions register new PEG grammar rules and
transformer functions at load time. There are two moving parts:

- **`PEGMatcherCache`** (per-database) — compiles grammar into the `PEGMatcher` tree
  used for tokenized input matching.
- **`PEGTransformerFactory`** (global singleton) — maps rule names to transform
  functions and holds parsed grammar rules.

Extensions supply a grammar text (with `<-/` to append to existing choice rules)
plus a map of transform functions. The registration API lives in the autocomplete
extension itself, not in core DuckDB.

---

## New syntax: `<-/`

```
# Normal: define a new rule
MyStatement <- 'MY' 'KEYWORD' Expression

# Append: extend an existing choice rule with a new alternative
Statement <-/ MyStatement
```

`<-/` appends `/ MyStatement` to the token list of the existing `Statement` rule.
Normal `<-` adds a brand-new rule (throws if it already exists).

---

## Piece 1 — `include/parser/peg_parser.hpp`

Add to `PEGParser`:

```cpp
struct PEGParser {
public:
    void ParseRules(const char *grammar);
    void AddRule(string_t rule_name, PEGRule rule);
    void ApplyExtensionRules();             // NEW: merge extension_rules into rules in-place
    void MergeInto(PEGParser &base) const;  // NEW: merge this parser's rules into another

    case_insensitive_map_t<PEGRule> rules;
    vector<pair<string, PEGRule>> extension_rules; // NEW: collected from '<-/' syntax
};
```

> Use `vector<pair<…>>` (not a map) so multiple extensions can all append to
> the same rule (e.g. two extensions both extend `Statement`).

---

## Piece 2 — `parser/peg_parser.cpp`

### 2a. Track `is_extension_rule` in `ParseRules`

Add next to existing locals:
```cpp
bool is_extension_rule = false;
```

In the **RULE_SEPARATOR** `else` branch, after `c += 2` (for `<-`), add:
```cpp
if (grammar[c] == '/') {
    is_extension_rule = true;
    c++;
}
```

At the **newline completion** block (currently calls `AddRule`), replace with:
```cpp
if (is_extension_rule) {
    extension_rules.push_back({rule_name.GetString(), std::move(rule)});
    is_extension_rule = false;
} else {
    AddRule(rule_name, std::move(rule));
}
```

Apply the same branch at the **EOF completion** block at the bottom of the function.

### 2b. `ApplyExtensionRules()`

```cpp
void PEGParser::ApplyExtensionRules() {
    for (auto &[ext_name, ext_rule] : extension_rules) {
        auto it = rules.find(ext_name);
        if (it == rules.end()) {
            throw InternalException(
                "Extension rule '<-/' references unknown base rule '%s'", ext_name);
        }
        // '/' is 1 byte → stored inline in string_t, no dangling pointer risk
        PEGToken or_token;
        or_token.type = PEGTokenType::OPERATOR;
        or_token.text = string_t("/", 1);
        it->second.tokens.push_back(or_token);
        for (auto &token : ext_rule.tokens) {
            it->second.tokens.push_back(token);
        }
    }
    extension_rules.clear();
}
```

### 2c. `MergeInto()`

Used by `RegisterExtensionGrammarRules` to merge a separately-parsed extension
parser into the singleton's `PEGParser`.

```cpp
void PEGParser::MergeInto(PEGParser &base) const {
    for (auto &[name, rule] : rules) {
        if (base.rules.count(name)) {
            throw InternalException(
                "Extension grammar conflict: rule '%s' already exists", name);
        }
        base.rules.insert({name, rule});
    }
    for (auto &[ext_name, ext_rule] : extension_rules) {
        auto it = base.rules.find(ext_name);
        if (it == base.rules.end()) {
            throw InternalException(
                "Extension rule '<-/' references unknown base rule '%s'", ext_name);
        }
        PEGToken or_token;
        or_token.type = PEGTokenType::OPERATOR;
        or_token.text = string_t("/", 1);
        it->second.tokens.push_back(or_token);
        for (auto &token : ext_rule.tokens) {
            it->second.tokens.push_back(token);
        }
    }
}
```

---

## Piece 3 — `include/matcher.hpp`

Extend `PEGMatcherCache`:

```cpp
struct PEGMatcherCache : ParserExtensionInfo {
    shared_ptr<PEGMatcher> GetMatcher();
    void Invalidate();
    void AddGrammarExtension(const string &extension_name, // NEW
                             const string &grammar_text);

private:
    std::mutex mutex;
    shared_ptr<PEGMatcher> matcher;
    vector<pair<string, string>> grammar_extensions; // NEW: (name, grammar_text)
};
```

---

## Piece 4 — `matcher.cpp`

### 4a. Update `PEGMatcherCache::GetMatcher()`

Build a combined grammar string (base + all extensions) before compiling:

```cpp
shared_ptr<PEGMatcher> PEGMatcherCache::GetMatcher() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (matcher) { return matcher; }
    }
    auto new_matcher = make_shared_ptr<PEGMatcher>();
    MatcherFactory factory(new_matcher->allocator);

    string combined;
#ifdef PEG_PARSER_SOURCE_FILE
    std::ifstream t(PEG_PARSER_SOURCE_FILE);
    std::stringstream buffer;
    buffer << t.rdbuf();
    combined = buffer.str();
#else
    combined = string(const_char_ptr_cast(INLINED_PEG_GRAMMAR));
#endif
    for (auto &ext : grammar_extensions) {
        combined += "\n";
        combined += ext.second;
    }
    new_matcher->root = factory.CreateMatcher(combined.c_str(), "Statement");

    std::unique_lock<std::mutex> lock(mutex);
    if (!matcher) { matcher = std::move(new_matcher); }
    return matcher;
}
```

### 4b. Add `AddGrammarExtension()`

```cpp
void PEGMatcherCache::AddGrammarExtension(const string &name, const string &grammar_text) {
    grammar_extensions.push_back({name, grammar_text});
}
```

### 4c. Call `ApplyExtensionRules()` in `MatcherFactory::CreateMatcher(const char*, const char*)`

After the existing `parser.ParseRules(grammar);` line, add:
```cpp
parser.ApplyExtensionRules();
```

This processes any `<-/` rules that appear in the combined grammar string.

---

## Piece 5 — `include/transformer/peg_transformer.hpp`

In `class PEGTransformerFactory`, add two public static methods (alongside the
existing `Transform` helpers):

```cpp
static void RegisterExtensionTransform(const string &rule_name,
                                       PEGTransformer::AnyTransformFunction function);
static void RegisterExtensionGrammarRules(const string &grammar_text);
```

---

## Piece 6 — `transformer/peg_transformer_factory.cpp`

Add after `GetInstance()`:

```cpp
void PEGTransformerFactory::RegisterExtensionTransform(
        const string &rule_name, PEGTransformer::AnyTransformFunction function) {
    auto &instance = GetInstance();
    if (instance.sql_transform_functions.count(rule_name)) {
        throw InternalException(
            "Extension transformer conflict: rule '%s' already registered", rule_name);
    }
    instance.sql_transform_functions[rule_name] = std::move(function);
}

void PEGTransformerFactory::RegisterExtensionGrammarRules(const string &grammar_text) {
    PEGParser ext;
    ext.ParseRules(grammar_text.c_str());
    ext.MergeInto(GetInstance().parser);
}
```

---

## Piece 7 — `include/peg_grammar_extension.hpp` (new file)

```cpp
#pragma once
#include "transformer/peg_transformer.hpp"
#include "duckdb/common/case_insensitive_map.hpp"

namespace duckdb {

class DatabaseInstance;

struct PEGGrammarExtension {
    string extension_name;
    //! Grammar text. Use '<-' for new rules, '<-/' to append an alternative to
    //! an existing choice rule (e.g. "Statement <-/ MyStatement").
    string grammar_text;
    case_insensitive_map_t<PEGTransformer::AnyTransformFunction> transform_functions;
};

//! Register a grammar extension. Call from your extension's Load().
//! Throws InternalException eagerly on any grammar or name conflict.
void RegisterPEGGrammarExtension(DatabaseInstance &db, PEGGrammarExtension extension);

} // namespace duckdb
```

---

## Piece 8 — `autocomplete_extension.cpp`

Add includes at the top:
```cpp
#include "peg_grammar_extension.hpp"
#include "inlined_grammar.hpp"
#include "parser/peg_parser.hpp"
```

Add the function before `LoadInternal`:

```cpp
void RegisterPEGGrammarExtension(DatabaseInstance &db, PEGGrammarExtension extension) {
    // 1. Eager validation against the base grammar
    PEGParser base;
    base.ParseRules(const_char_ptr_cast(INLINED_PEG_GRAMMAR));
    base.ApplyExtensionRules();
    PEGParser ext;
    ext.ParseRules(extension.grammar_text.c_str());
    ext.MergeInto(base); // throws on any conflict

    // 2. Register with per-database matcher cache → rebuilds on next query
    auto &cache = GetPEGMatcherCache(DBConfig::GetConfig(db));
    cache.AddGrammarExtension(extension.extension_name, extension.grammar_text);
    cache.Invalidate();

    // 3. Register transform functions into the global singleton
    for (auto &kv : extension.transform_functions) {
        PEGTransformerFactory::RegisterExtensionTransform(kv.first, std::move(kv.second));
    }
    PEGTransformerFactory::RegisterExtensionGrammarRules(extension.grammar_text);
}
```

---

## How an external extension uses this

```cpp
// In MyExtension::Load(ExtensionLoader &loader):
PEGGrammarExtension ext;
ext.extension_name = "my_ext";
ext.grammar_text = R"(
    MyStatement <- 'MY' 'KEYWORD' Expression
    Statement <-/ MyStatement
)";
ext.transform_functions["MyStatement"] = [](PEGTransformer &t, optional_ptr<ParseResult> pr) {
    // ... transform to unique_ptr<SQLStatement> ...
};

RegisterPEGGrammarExtension(loader.GetDatabaseInstance(), std::move(ext));
```

---

## Build

```bash
make reldebug
```

No need to run `build_peg_grammar.sh` — these changes don't touch `.gram` files.
