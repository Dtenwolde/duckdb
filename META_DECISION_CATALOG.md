# Decision catalog for the PEG parser retrospective

Companion to [META_ANALYSIS.md](META_ANALYSIS.md), frozen at accepted `de3be449c5e` on 2026-10-16. Experiment IDs identify research artifacts, not independent trials. Later final decisions override older “pending” or “parked” text in preserved experiment reports.

Positive percentages below mean reduced whole-parser time unless explicitly labeled otherwise. ARM/ Linux numbers compare the candidate with the **listed historical baseline**, not necessarily its eventual commit parent or today's head. A dash means no figure is asserted here.

## Accepted commits

The first-parent history contains 19 commits. Binding 40's implementation was subsequently replaced by the overlapping upstream representation 79; its effect is not a separate addition to 79. Exact full hashes, parent hashes and trees are in [META_EVIDENCE.json](META_EVIDENCE.json).

| Experiment / commit | Retained change | Evidence and why it survived |
|---|---|---|
| **19** `79b65fd2750` | Scanning and token-iterator foundation; natural inlining, ASCII-classification reuse, bounded literal lookup, batched delimiter scanning, structural atomic metadata and lazy frame reservation. | Linux 6.22% below original. Removes native recursive/pthread/forceinline machinery from the earlier prototype. [Report](RESEARCH_REPORT.md), [heap-frame attribution](HEAP_FRAMES.md). |
| **27** `01f08ee34d2` | Prepare and fold first-child list chains within iterative execution. | ARM 14.90% / Linux 7.67% below 19. Broader useful coverage than 25 and cheaper eligibility than 26; retains required ancestor results. [Ledger](RESEARCH.md), [structure](STRUCTURE_CHAIN27.md). |
| **32** `d57d0f0f990` | Move a small flag to remove ParseResult padding, 72→64 bytes. | ARM 0.36% / Linux 1.40% below 27; Flummi 3.40% / 4.43%. Tiny change with straightforward semantics. [Ledger](RESEARCH.md). |
| **35** `ae1372d9fbb` | Skip FIRST-rejected choice alternatives before constructing child state. | ARM 2.71% / Linux 3.09% below 32. Removes work known to fail; one Linux row 0.79% slower. [Ledger](RESEARCH.md). |
| **40** `03206501bc7` | Bind generated transformer rule references when compiling the grammar. | Measured 38 has identical runtime implementation: ARM 4.84% / Linux 3.38% below 35. Replaces repeated name lookup; 40 simplifies generator initialization. Later integrated with 79. [Ledger](RESEARCH.md). |
| **46** `9722c619042` | Separate temporary transform processes from results; rewind after frame destruction. | Latency approximately flat; Flummi RSS −15.95%, large VALUES−17.30% vs 40. Short lifetime no longer retains all process storage. Ordinary allocation counts increase, an explicit tradeoff. [Ledger](RESEARCH.md). |
| **52** `b4de9eebba6` | Declare absent-prefix TildeExpression collapsible. | ARM 1.62% below 46; malformed input 1.94% slower. Existing identity/zero-depth semantics justify one YAML line plus generated output. [Ledger](RESEARCH.md). |
| **55** `100d55796de` | Own a heap vector of transform frames and reserve 32 entries. | ARM 1.17% below 52, all eleven rows improve; RSS flat. Removes repeated deque block allocations and keeps per-traversal ownership/reentrancy. [Ledger](RESEARCH.md). |
| **59** `4af7f40117e` | Move owned generated transform results into wrappers. | ARM 0.35% /shared Linux 0.56% below 55; effectively a small latency effect. Removes 962 Flummi and 1,422 TPC-DS allocations and expresses ownership directly. [Ledger](RESEARCH.md). |
| **73** `777a358fe76` | Refine operator FIRST token class using the first byte. | ARM 4.44% /noisy Linux 3.70% below 59. VALUES failure counts collapse; avoids 69's whole-token scanning tax. [State history](STATE-history-through75.md). |
| **64** `117c6bf9673` | Reserve generated output vectors from already-known lengths. | Measurements against 59 were flat/mixed, not a stable latency gain. Removes 1,879 Flummi allocations and 339,824 requested bytes with seven generator lines. Committed after 73. [State history](STATE-history-through75.md). |
| **79** `f60529c96f5` | Integrate PR27290 rule IDs and transform-result type-hash prefilter. | ARM 0.81% incremental gain vs 117c, mixed; binding substantially overlaps 40. Removes our duplicate table, retains exact type check after hash prefilter. [Integration](PR27290_INTEGRATION79.md). |
| **95** `9735f46ea8d` | Compact full-width source positions with independent validity flags; 64→48-byte result header. | ARM 0.90% / Linux 3.54% below 79; Flummi RSS −11.77%, large VALUES−15.45%. No truncation of position semantics. Direct field reads migrate to getters. [Report](COMPACT_POSITIONS95.md). |
| **99** `5def1439c34` | Finish collapsible lists when FIRST proves the remaining optional tail absent. | ARM 4.32% / Linux 4.28% below 95; malformed input slightly slower. Flummi RSS −9.32%, large VALUES−29.20%. 22 implementation lines, no extra state/container. [Report](EMPTY_TAIL99.md). |
| **107** `4480fd5f867` | Share structural matcher state and iterative driver frames in one transition implementation. | ARM 13.16% / Linux 12.13% below 99, all eleven improve; RSS/cold roughly flat. Removes 105's duplicate interpreter while preserving the extension interface. [Report](SHARED_DRIVER107.md). |
| **121** `ff2709b04f5` | Token-position directory with short matcher chains, lazy growth and map fallback. | Initial/control comparisons around 3.4% below 107 on both platforms; Flummi control 6.69% / 8.37%. Whole-parser benefit, independently of earlier cache-replay claims. [Report](TOKEN_DIRECTORY121.md). |
| **133** `d1501dccb7e` | Prepared list groups combined with early capture avoidance. | ARM 6.36% / Linux 4.06% below 107; RSS flat. Removes pending records and intermediate completions, includes/supersedes 111/130. [Report](COMBINED_GROUPS133.md). |
| **165** `1301ed8eb9d` | Count transform-list elements through a shared validated visitor instead of allocating a vector to read its length. | Removes 2,420 Flummi and 3,323 TPC-DS allocations. Initial 163: 0.93% / 0.19% below 107; repeat 0.48% / −1.52%, variable. Accepted as a small allocation/ownership cleanup. [Report](COUNT_LIST165.md). |
| **178** `de3be449c5e` | Allocate list-child spans directly in the result arena after early-collapse checks. Adapted from PR26741. | ARM 3.46% / Linux 1.89% below 1301; Linux Flummi 2.87% slower, RSS +3.41% Flummi/ +2.20% large VALUES. Accepted after explicit review of these tradeoffs. [Acceptance](ACCEPTED_DIRECT_STORAGE178.md). |

## Rejected or parked implementation families

“Measured loss” rejects the measured implementation. “Small/mixed” means insufficient evidence/value for its complexity; it is not a proof of zero effect. “Semantic” identifies a correctness or API-contract problem. “Superseded” means the idea survives in a different accepted implementation.

### Driver, state and compiler boundaries

| IDs | Disposition | Finding and interpretation |
|---|---|---|
| **01** Borrow child result during Resume | Small/mixed | Hot ExecuteFrame self samples fell substantially, but whole-parser time did not consistently improve; keyword workload regressed around 6%. An interface-wide signature change was not justified by a cooler symbol. |
| **03** Loop immediate steps inside the old frame | Measured loss | Linux 3.12% slower overall. Removing a return boundary did not eliminate the underlying protocol. |
| **04–08,14–18** Native process/dispatch fast path and portability refinements | Removed architectural direction | Material earlier speedup, but violated the desired heap-frame/depth direction. The 17/18 control changed native storage **and** concrete dispatch; it cannot measure stack placement alone. 19 retains independent scanning/iterator work. |
| **20** Shared final/child result slot | Small/mixed; user parked | 112→88-byte frame, Linux 1.93% lower vs 19 but Flummi 1.58% slower. Extra ownership/state coupling was not worth this result. |
| **21** Compact tagged MatchStep | Measured loss | 48→24 bytes, Linux 3.16% slower vs 19; all eleven regress. Smaller return/state representation does not guarantee better generated code. |
| **24,28** Concrete Resume dispatch, then forceinline | Measured loss / no net gain | 24 is 6.45% slower vs 19. 28 recovers 5.73% against that bad control but remains 0.16% slower vs 19. The denominator matters. |
| **29** Driver-local active child result | Small/mixed | 112→64-byte suspended frames, Linux 0.11% improvement vs 19; TPC-H/TPC-DS regress. Large layout reduction, little runtime value. |
| **30** Two-word MatchStep with private sentinel | Semantic | Core and loadable extension had different sentinel identities; ASan global-buffer-overflow. No valid timing claim. |
| **36** Cache check before frame creation | Small/mixed | ARM 0.91% lower / Linux 2.24% slower vs 32. Less frame setup introduced other initialization/return bookkeeping. 107 later addresses the structure differently. |
| **44** Inline transform-frame constructor | Neutral | ARM 0.009% below 40 despite visible constructor samples. |
| **45,49** Outline cold result-cast throw path | Small benefit; parked | ARM 1.67% below 40, then 0.98% below 46 after rebasing; Linux evidence contended. Not an accepted or separately additive gain. |
| **103** General sequence continuations | Measured loss | ARM 4.90% slower vs 95, all eleven rows; tested fingerprints match. Saved-prefix and collection bookkeeping may outweigh removed frames, but no isolated attribution proves which component caused it. |
| **104** Prepared unique parent links | Small/mixed | ARM 0.23% below 99. Hoisting ancestry without eliminating enough runtime completion work had little value. |
| **112** Remove frame max-token snapshot | Measured loss | Linux 1.43% slower vs 107,10/11 rows and all paired aggregates slower. Eight saved bytes were not a useful outcome. |
| **113** Sparse memo-frame bookkeeping stack | Small/mixed | ARM 0.08% slower vs 107; adds exception/lifetime protocol for no demonstrated gain. |
| **115** Change literal-lookup code placement | Mixed across platforms | ARM 0.62% lower / Linux 0.75% slower vs 107. Intended natural inlining was not a broad win. |
| **117** Separate token-storage ownership from cursors | Mixed across platforms | ARM 1.64% lower / Linux 3.30% slower vs 107. Simpler-looking child ownership still changes code generation/API/lifetime boundaries. |
| **148** Direct generated-transform dispatch | Measured loss | ARM 1.26% slower vs 107. Resume inlines, but NextStep remains out of line and code grows. |
| **159** Typed private frame destruction | Mixed across platforms | ARM 1.51% lower / Linux 1.43% slower vs 107, all five aggregate rounds agree within each platform. No claim that Linux noise explains away the result. |
| **160** Remove redundant input matcher field | Measured loss | Saves 8 bytes/frame; ARM 0.16% / Linux 0.67% slower vs 107. |

Sources: [early findings](FINDINGS.md), [heap-frame analysis](HEAP_FRAMES.md), [early ledger](RESEARCH.md), [sequence 103](SEQUENCE_FOLDING103.md), [phase-two ledger](PHASE2_RESEARCH.md), [final disposition records](RESEARCH_STATE.json).

### Speculation, collapse, dispatch and captures

| IDs | Disposition | Finding and interpretation |
|---|---|---|
| **22** Batch absent optionals within list processing | Neutral | Linux 0.14% slower vs 19, same allocation counts. Work shifted between functions rather than disappearing. |
| **23** Deferred empty optional placeholders | Superseded direction | Linux 3.74% below 19 but mixed, with extra accumulator/placeholder state. 99 later removes a proven subset more simply. |
| **25,26** Narrow/broader leading-list chains | Superseded by 27 | Broadening coverage and preparing eligibility strengthened the original result; do not count each revision separately. |
| **43** Drain atomic list children in a loop | Small/mixed | ARM 1.02% below 40 with mixed rows. Atomic children already avoid heap child frames, limiting what the loop can remove. |
| **53,54** Anonymous choice collapse and wrapper forwarding | Small/mixed | Around 1.05% and 0.46% ARM respectively; extra shape/type conditions for limited effect. 118 later corrected the overly broad count of truly same-type forwarding opportunities. |
| **81,82** Wide-choice prediction bitmasks and inlining refinement | Small/mixed | 81 adds 89,400 bytes for 13 plans, ARM 0.34% lower. 82 restores natural inlining: ARM 1.02% lower, Linux 0.52% slower vs 79. Preparation must pay for its runtime use and footprint. |
| **110** Lazy list reserve alone | Neutral | ARM 0.05% slower vs 107. 111's value comes from also avoiding first capture/completion; 111 is retained within 133. |
| **125** Skip redundant leading FIRST checks | Mixed across platforms | ARM 1.31% lower / Linux 0.74% slower vs 107. 148,619 Flummi checks were all true, yet their removal alone did not establish a portable gain. |
| **126–128** Two-token TypeLiteral lookahead | Semantic; not retained | 126 misclassified multi-token custom atomic matchers. 127 fixes consumption capability and gives 3.15% ARM gain vs 107, but 128 finds annotation observability and BUILD+autocomplete differences. Normal AST equality is insufficient. Cold grammar also 13.93% slower. |
| **132** Earlier direct result-arena child storage | Parked, revisited as 178 | ARM 4.37% lower / Linux flat vs 107; Linux Flummi/Stress slower. RSS +6.14% Flummi/ +13.96% large VALUES. 178 measures the later combined background rather than reusing these numbers. |
| **134** Shared heap capture buffer with slices | Small/mixed | ARM 1.89% lower vs 107, worse than simultaneous simpler 111 at 5.34%; extra buffer ownership/slice bookkeeping. |
| **135** Coallocate completed result header and child span | Mixed across platforms | ARM 3.97% lower / Linux 1.52% slower vs 107; RSS flat. Structurally plausible, but not a portable performance win. |
| **138–143,149** Deterministic choice-to-list bridges | Parked | 138 first had incorrect wrapper plumbing; 139 fixes it. 143 reaches 4.05% ARM / 1.79% Linux below 107 with a Linux WideSelect regression. 149 simplification loses value: 1.81% ARM versus simultaneous 143 at 3.83%. No additive comparison against final 133/121 establishes a next gain. |
| **145** Prepared class dispatch without bridges | Small/mixed | ARM 0.67% lower but 4/5 paired aggregates slower; Linux 0.82% slower vs 107. 143's structural bridge matters more than dispatch lookup alone. |
| **146,147** Broader single-child early collapse | Parked beyond 133 | 147 vs simultaneous 133 adds 1.64% ARM / 0.34% Linux, Linux Flummi 2.68% slower. The 8.28% ARM figure against 107 includes the already-selected 133 work. |
| **180,181** Latest upstream lookahead diagnostic port | Semantic | 180 did not compile; 181 fixes the port and reproduces multi-token/custom annotation contract problems on 1301. No latency gain is counted from changing those semantics. |

Sources: [early ledger](RESEARCH.md), [phase-two ledger](PHASE2_RESEARCH.md), [lookahead contract](LOOKAHEAD128_CONTRACT.md), [coallocation audit](COALLOCATION135_AUDIT.md), [upstream follow-up](FINAL_BENCHMARKS182.md).

The annotation observer was a purpose-built extension contract probe, not a discovered production extension failure. Shipped autocomplete uses recognition-only mode; its public SQL function was not shown to regress by the BUILD+autocomplete probe. Both qualifications matter.

### Hashing, allocation policy, memory and strings

| IDs | Disposition | Finding and interpretation |
|---|---|---|
| **31,33** Better hash mixing / flat memo index | Measured loss | Quiet Linux 2.25% / 2.52% slower vs 27; ARM 33 about 0.41% slower. Observed collisions did not imply that mixing/probing was the main removable cost. |
| **37,39,42,51** Transform-frame container/reservation refinements | Superseded by 55 | Owning vector plus 32-entry reserve retains the useful capacity reuse. Arena-backed alternatives need a different lifetime analysis, especially after 46. |
| **41** arena_deque for transform frames | Memory tradeoff; parked | ARM 0.44% lower/flat vs 40. Retained discarded blocks raise Flummi used storage and cross a reservation threshold, 16.77→33.55 MB. |
| **47,48** Local string moves and identifier cleanup | Small/mixed | 47 ARM 0.77% lower vs 46; combined 48 only 0.34%, mixed. In-place modification can also keep a heap allocation where a copied result would fit SSO. |
| **60** Start process arena at 32 KB | Small/mixed | ARM 0.15% lower vs 59; one-statement initial capacity 4 KB→32 KB. Removes growth on larger parses but charges smaller ones. |
| **61** Derive generated counts from child-result slots | Small/mixed; simpler successor 165 | ARM 0.32% lower vs 59. Couples generator scheduling to shape/count bookkeeping. 165 instead shares a validated visitor and stores no new state. |
| **69** Scan all bytes for operator classification | Superseded by 73 | Correct but long-operator counterexample roughly 25% slower than control. First-byte conservative classification obtains the useful rejection without the scan. |
| **71** Lazy general arena adapter | Small/mixed | Removes 8 eager simple-statement allocations but ARM 0.21% slower overall; grows general ArenaAllocator by 8 bytes. Parser-only evidence did not justify the generic change. |
| **78** PrimitiveDictionary | Not implemented; user parked | Default hashing/equality does not match the case-insensitive lookup contract. No measured rejection or performance result. |
| **84,87,89** Chained memo nodes, bucket lifetime, hash alternatives | Modest gains; not retained | 84 around 2.19% ARM / 2.48% Linux below 79. 87 robust indexing 1.52% / 2.96%; 89 shared Hash plus arena buckets 0.72% / 1.11%, with Flummi regressions. Useful research, but neither the largest replay gains nor a generally superior hash table carried through to whole parsing. 121 later uses token-key structure. |
| **86,94** Release packrat storage before transformation | User parked | 94 aggregate flat ARM / 0.4% lower Linux vs 79; RSS −3.9% Flummi/ −1.8% large VALUES; Linux TPC-H 1.8% slower. Valid lifetime reduction, insufficient value for this change. |
| **116** Reserve token vector from query bytes | Small/mixed | ARM 0.29% / Linux 0.22% lower vs 107; unused capacity on long-token inputs. Query bytes are not a reliable estimate for every parser vector. |
| **119** Reserve existing memo map from token count | Superseded by 121 | ARM 2.30% / Linux 1.41% below 107, RSS mixed. Token-directory representation was the stronger independent candidate. |
| **123** Reserve 121's full token bound on first store | Tradeoff; not retained | Saves large VALUES growth-related memory, but 0.29% ARM / 0.77% Linux slower than simultaneous 121. Even an exact upper bound is not always the best eagerly reserved capacity. |
| **155** One reusable process-arena spare block | Measured aggregate loss | ARM 1.40% slower vs 107 despite Flummi 1.27% /TPC-DS 3.41% faster. Per-transform initialization harms short workloads. |
| **158,162** Further string ownership/unquote cleanup | Small/mixed | 158 ARM 0.16% slower; 162 only 0.03% lower. Targeted byte-oracle/allocation results did not imply ordinary-workload latency value. |
| **166,168** Exact extraction-vector reservation / combined visitor variant | Small/mixed | Fewer allocations; 168 ARM 0.99% lower / Linux 0.18% slower vs 107, mixed paired rounds. Extra visitor callback not retained alongside the smaller 165. |
| **169,171,174/175** In-place/batched/direct quoted-identifier decoding | Workload-specific; user parked 175 | Earlier variants traded dense-escape wins for sparse/plain regressions. 175's improved form gives roughly 90% dense-escape wins, but ordinary suite ARM 0.26% slower / Linux 0.16% lower and ARM short-plain 3.99% slower. Preserved outside the pushed branch. |
| **179** Owner-preserving FIRST-set worklist adaptation | Measured cold regression | Cold grammar 7.89% slower on ARM. This version preserves owned bitsets and adds a local index map; it does not measure upstream's packed shared-bitset design. |

Sources: [early ledger](RESEARCH.md), [chained cache 84](CHAINED_CACHE84.md), [shared hash 89](SHARED_HASH89.md), [directory 121](TOKEN_DIRECTORY121.md), [phase-two ledger](PHASE2_RESEARCH.md), [count 165](COUNT_LIST165.md), [unquote 175](UNQUOTE175.md), [final upstream follow-up](FINAL_BENCHMARKS182.md).

## Diagnostics that changed the research direction

These are not retained/rejected production diffs and must not be counted as performance wins.

| IDs | What was learned | How it changed the decision |
|---|---|---|
| **00, early frame/arena probes** | Matching dominated; many tiny iterator calls and repeated list activations were visible. | Established profiling before implementation and supported 02/27. |
| **50,118** | Apparent forwarding functions often change C++ result types; corrected same-type opportunity is small. | Prevented treating every one-line transformer as a removable wrapper. |
| **58,65,66,153** | Arena block churn, memo occupancy and possible spare-block reuse were quantified. | Motivated 60/155, whose measured results then rejected the naive allocation-count argument. |
| **67,76,150,154,156** | Historical LALR comparison, better stack coverage, simulated instruction counts. | Focus shifted toward interpreter work; no claim that profile fractions alone establish algorithmic speed ratios. |
| **70** | Grammar reuse already exists; Parser retention does not retain per-statement arenas. | Ruled out recompilation as the ordinary warm bottleneck. |
| **75** | Operator refinement removes failed frames while cache counts stay fixed. | Gives a direct mechanism for 73. |
| **80,83,85,88,91,120** | Choice frequency, exact cache traces, pathological strides, ownership/hash controls, success/failure reuse and token geometry. | Separated replay claims from parser claims; led to 121 rather than more generic hash mixing. |
| **93,96,97,100,101** | Full-width position semantics, allocated/reachable nodes, empty-result fate, exact removed resumptions, retained failed work. | Led to 95/99 and identified both the promise and constraints of lookahead. |
| **102,108,109,124,131,151** | Residual phase/assembly profiles, reserve no-ops, collapse streaks and group savings. | Supported 133; narrowed the claim for 147 after comparing with its real control. |
| **114,144** | Query bytes, token counts and execution depth predict different capacities. | Rejected a general “reserve parser vectors from SQL length” policy. |
| **136,137,142,157** | Eligibility must be measured under the actual dispatch/grammar shape. Initial probes were too narrow or had invalid assumptions. | Corrected opportunity counts before coding; small remaining fusion fractions did not justify an automatic new fast path. |
| **161** | Tested growth is roughly linear across 31 cases/seven families; nested workloads benefit from 152. | Supports targeting constant interpreter cost without asserting all PEG inputs are linear. |
| **167,173** | Escaped-string helper dominates specially constructed identifier workloads; libc copying dominates plain long input. | Motivated 175 but did not establish real workload prevalence. |
| **164,170,172,176,177,182** | Exact-combination validation, cumulative comparisons and controlled follow-up timing. | These strengthen provenance; they do not introduce that many new independent optimizations. |

Failed builds, stale initial freezes, invalid instrumentation and incomplete stacks remain in the underlying ledger. They are not data points against an algorithm. For example, 138's wrapper bug was corrected before 139 timing; 180's compile failure is distinct from 181's semantic probes.

## Upstream work and overlapping claims

PR27290 was incorporated at 79, replacing the earlier rule-binding representation while retaining ownership moves and reservations. Its result-type hash is a rejection prefilter; exact type checking remains authoritative.

[PR26741](https://github.com/duckdb/duckdb/pull/26741) contains several directions already covered by accepted code: indexed transform rules, type hashing and refined token classes. Direct list storage was separately adapted and ultimately accepted as 178. Lookahead and FIRST-set construction were separately investigated; neither the full PR nor its published aggregate gain was applied wholesale.

Its initial benchmark comment included cold grammar in a 12-workload aggregate; this campaign's ordinary aggregate excludes cold grammar. The author's later corrected-base cold measurements also superseded an earlier apparent cold regression. These details prevent combining superficially similar percentages.

The initial literature survey also examined prior heap-frame migration, literal IDs, FIRST sets, per-statement arenas and rewind work (including PRs25204, 25328, 25527, 26015, 26037, 26451, 26906, 26919). Their benchmark reports informed workload selection and architecture, but their gains belong to different baselines. They are not independent contributions to this campaign's final 51.4% / 46.2%.

Sources: [initial PR survey](REPORT.md), [integration 79](PR27290_INTEGRATION79.md), [upstream refresh and corrected cold report](UPSTREAM122.md), [178 acceptance](ACCEPTED_DIRECT_STORAGE178.md).
