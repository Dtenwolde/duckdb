# Structural PEG parser research: architectural findings

Final report for the structural campaign starting from `0b778691581`, closed on 2026-10-16. The latest accepted commit is `0093db96624`. The campaign met its performance objective; the remaining prototypes are parked.

The three accepted structural changes, 191, 204 and 215, reduce parser time a further **17.0% on ARM / 14.0% on Linux** relative to the campaign baseline, combining their recorded conditional comparisons across eleven warm parser workloads. This is a chained estimate, not a fresh end-to-end comparison. They reduce work at grammar boundaries rather than merely changing the storage used for the same work.

**Recommendation: close this campaign.** Recent candidates have produced small, inconsistent or negative latency changes, with increasing complexity. There are larger architectural possibilities, but none has enough evidence to promise another substantial gain. Preserve the experiments as a starting point for a separately scoped redesign rather than continue adding local control mechanisms.

Publication update after campaign closeout: all 23 accepted parser commits have been rebased onto upstream `main` (`c7104330fd28e3f48f324377e2f0989f85739c6b`) and pushed to `lnkuiper/duckdb:parser-profile-cyanoptera` at `418e570277f7cb97f67acdb376b6bfa728b40967`. GitHub confirms 23 commits ahead and zero behind; the review worktree is clean. The performance evidence below remains tied to the pre-rebase measured head `0093db96624`. See [sync validation](SYNC_MAIN20261016.md) and the [cumulative benchmark table](BENCHMARK_TABLE_SYNC20261016.md).

## What the evidence supports

| Change | Structural effect | Outcome against its accepted baseline |
|---|---|---|
|191:first-choice/list sharing|One iterative region shares the entry cursor while retaining ordered-choice retries|5.02%ARM /1.80%Linux|
|204:prepared groups across empty optional prefixes|Removes empty captures, capture spans, two requests and three iterations per folded transition|8.43%ARM /7.66%Linux|
|215:generated zero/one-child transform schedules|Removes general mutable process scheduling for statically known small schedules|4.61%ARM /5.19%Linux|
|218:consumed-prefix list continuations|Retains all captures and a136-byte continuation; saves only one iteration per boundary|ARM flat/slightly slower; parked|
|227:separate forwarding-region stack|Moves redundant transform ancestry into another representation|About1.4–1.5% across platforms; complexity and Linux Flummi regression did not justify it|
|240:pure forwarding elision|Proves that no parent depth/location/ownership work remains, then omits the parent entirely|1.80% on both platforms; parked by the user|
|242:optional/list frame sharing|Removes7,474Flummi /12,565TPC-DS frames and two iterations each, without growing the224-byte list frame|ARM+0.29%, replication−0.11%; parked standalone|
|251:fixed multi-child transform storage|Removes two arena vector buffers but retains the mutable process and scheduling|ARM flat (−0.011%); parked|
|245:reuse repeated list bodies|Shares a cursor and reuses body storage across iterations with an arena checkpoint|ARM+0.94%, replication+0.84%; parked|
|253:separate recognition cache for deferred type captures|Avoids failed-prefix captures but changes which annotation writes are replayed|ASTs agree; token-annotation gate fails; not timed|
|255/256:preserve memoized captures while deferring the remainder|Keeps cache history and reduces capture construction, but adds execution boundaries|ARM 36.55% / 3.83% slower; parked|
|260:restore ordinary atomic dispatch|Moves private capture handling out of the ordinary atomic helper|ARM 3.95% slower; same-run 256 replication 3.41% slower; parked|
|261:direct ownership of deferred capture state|Removes an extra boundary process/frame but adds an owner to the common frame|Broad equivalence checks pass; targeted harness does not compile; untimed and parked|

Physical frame count is a useful mechanism check, not the primary metric. Optional plus repeat sharing removes28,771Flummi frames and58,288driver iterations, yet improves ARM by less than1%. All captures and logical matches remain, and dispatch/initialization work has partly moved into the new control helpers. By contrast,204removed logically unnecessary captures as well as scheduling.

Small inlining experiments are also conditional.235verified that a hot helper call disappeared, but improved ARM1.16% and regressed Linux0.69%.224removed its targeted constructor call and still regressed both platforms. 248 removed its targeted repeat-completion call and added roughly0.59% on ARM, but the combined1.42% gain still did not justify retaining the larger control representation. Such outcomes do not support blanket force-inlining; code size, layout and caller complexity matter.

## Current cost centers

Profile220measured four workloads at accepted215, with about9,400–11,100CPU samples each:

| Workload | Recognition | AST transformation | Tokenization |
|---|---:|---:|---:|
|Flummi|62.17%|24.80%|4.82%|
|TPC-DS|67.28%|20.87%|7.34%|
|Large VALUES|57.69%|31.10%|6.11%|
|Statements|52.44%|32.58%|5.28%|

Recognition and transformation remain the priorities. These profiles do not justify a new broad SIMD/tokenization project. Earlier tokenization work already exists on the branch.

Capture inventory222provides a more structural lead: Flummi allocates42,726keyword results, of which26,585are reachable from the completed capture tree; generated transformation casts only280of those to keyword values. VALUES has12,288reachable keyword results and zero such semantic casts. However, those nodes also contribute source spans, named-rule information and the public extension-visible tree. A low semantic-cast count is not proof that they can simply be deleted.

## State that may be shared, and state that must retain its own lifetime

The useful unit is a rollback boundary, not necessarily a grammar node. Before a mandatory prefix consumes input, several nested lists and choices can share one entry cursor; the accepted191/204changes exploit that fact. Once a prefix has consumed input, retry must recover the correct inner checkpoint. Merely replacing a full frame with another large snapshot often preserves most of the original cost.

MatchState also contains rule identity and ownership of a suggestion-deduplication set. Token annotations and maximum-token tracking have effects beyond the cursor. Custom processes may retain state references across yields. Therefore a global shared cursor is not a mechanically safe replacement for MatchState copies.

Experiments252–257 expose an additional constraint: memoization preserves effect history. Fresh-cache BUILD/RECOGNIZE comparisons agreed, but separating their caches changed annotations in a whole query. `SELECT foo.bar(1)` is sufficient: a failed type attempt annotates `foo` first as catalog, then schema; the later function path normally hits the catalog-qualification cache without writing again. A separate cache repeats that write. The matching outcomes and final AST can agree while the token stream differs.

Keeping ordinary captures at memoized boundaries fixes that mismatch. Diagnostic 257 then measures 16.4% fewer Flummi capture nodes and 18.3% fewer TPC-DS nodes, with unchanged atomic match counts. However, its added boundary processes increase heap frames by 23.0% and 19.7%, respectively. This representation is still slower. Restoring ordinary atomic dispatch in 260 does not recover the loss: it is 3.95% slower on ARM, versus 3.41% for 256 in the same run. The direct-state prototype 261 passes broad equivalence checks but stopped before timing because its targeted harness called a non-const accessor on a const result. That harness error is not a demonstrated parser failure. Its ownership contracts, sanitizer behavior and performance remain unvalidated.

The245prototype demonstrates a narrower legal region: a repeated list keeps one private working state, commits the caller only after a complete successful body, and rewinds iteration-local process storage before growing persistent repeat captures. Existing extension-facing and single-step execution paths provide useful differential oracles. This establishes legality, but its current speedup is too small to treat the representation as a proven architectural destination.

On the transform side, immutable semantic capabilities are more valuable than another general cache.240uses the actual active generated factory to prove owned-type identity and pure forwarding, checks depth and source-location obligations, and omits the frame when all obligations disappear. Unknown/custom factories retain ordinary execution. A separate immutable compiled-rule descriptor cache229was effectively flat and would have added a stronger API immutability contract without demonstrated value.

## Directions for a separately scoped project

1. **Reduce capture materialization at proven semantic boundaries.** The stronger long-term hypothesis is that the parser spends too much time constructing an intermediate object tree that the generated transformer immediately traverses and partly discards. Explore compact capture records or delayed materialization with an explicit plan derived from the grammar and generated transform semantics. Preserve offsets, lengths, alternative indices, names, alias identity and memoized results. Materialize the ordinary public result representation at opaque/custom boundaries. No prototype yet establishes the performance or complexity of this design.

2. **Prefer execution regions that eliminate work over regions that relocate it.** A new region should predict which logical operations, capture allocations or passes disappear, not just how many frames become smaller. Use counters to test that prediction before extending the control-state machinery. Repeated-body reuse remains a bounded experiment; its current result is not a reason to build a general continuation VM on the same representation.

3. **Extend generated fixed schedules only where current profiles and semantic plans justify it.** Zero/one-child specialization worked because a general process was unnecessary. Inventories249/250 identified180 fixed multi-child schedules;251 proved their inline storage correct but measured flat on ARM. Removing buffers alone was insufficient. Runtime-small dynamic schedules and handwritten literals are different cases and remain unimplemented. Keep ordinary extension processes as the compatibility boundary.

4. **Evaluate composition explicitly.** Capture elimination may subsume transform forwarding; two changes that save the same boundary will not have multiplicative gains. Cursor sharing may expose new call/layout costs, as242/245suggest. Compare a new candidate with the latest accepted commit and, when necessary, measure the relevant combinations in one interleaved run.

These are research hypotheses, not pending tasks or validated improvements. The old LALR comparison uses a different whole-version reference and predates the latest changes. It is useful motivation, not an isolated measurement of the remaining algorithmic gap. Any future current-head/LALR comparison should state its version and feature differences rather than attribute the entire ratio to PEG speculation.

## Evidence and status

Accepted changes retain iterative heap traversal. Candidate validation separately checks ordinary fingerprints, public matcher/transformer interfaces, extension effects, metadata, borrowing and ownership, exceptions, reentry, small-stack behavior, and sanitizer results. These checks prove specific contracts; they do not replace structural review or latency measurements.

The canonical ledger is `STRUCTURAL_CAMPAIGN187_STATE.json`. Reports include `RESIDUAL220.md`, `FORWARD_ELISION238.md`, `SIMPLE_FORWARD240.md`, `STRUCTURE_FORWARD240.md`, `UNARY_REGIONS241_PLAN.md`, `OPTIONAL_LIST242_PLAN.md`, `REPEAT_LIST245_PLAN.md` and `DEFERRED_CAPTURE253259.md`. Raw samples, source trees, binary identities and rejected prototypes are retained separately from the reviewable branch. There are no pending candidate approvals.

The earlier, full-campaign cumulative estimate is recorded in `CUMULATIVE215_ESTIMATE.md`: approximately 62% lower ARM parser time and 57% lower Linux parser time versus the original `c457670e673` baseline. It combines recorded comparisons, includes incorporated upstream changes, and is not a fresh cumulative benchmark.
