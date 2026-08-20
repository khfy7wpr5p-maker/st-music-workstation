# ST Music Workstation — Event Projection Relation-State Transition v0.1

Status: bounded Project Core immutable relation-state candidate; authoritative Project publication remains deferred

## 1. Purpose

This package converts a sealed, revalidated EventProjectionLink addition into a complete next **relation-state candidate** without mutating the prior state.

The result remains non-authoritative. Only a future owning Project mutation/publication boundary may make a complete Project state authoritative.

The package therefore implements:

```text
current relation-state candidate
+
sealed revalidated link addition
        ↓
validate exact base/next snapshot
        ↓
validate Project scope + duplicate absence + resource bound
        ↓
construct complete next immutable relation-state candidate
```

It does not perform the final authoritative publication step.

## 2. Candidate versus accepted relation types

`EventProjectionLinkCandidate` remains a proposal/staging value.

`EventProjectionLink` represents a relation that has passed the reviewed preparation, publication-revalidation, and relation-state transition path.

Direct ordinary construction of `EventProjectionLink` from `EventProjectionLinkCandidate` is prohibited. The transition builder is the only construction path introduced by this package.

This type distinction prevents convenience code from silently treating parser, AI, renderer, UI, or other candidate data as an accepted relation.

`EventProjectionLink` is still not a credential, capability, persistence record, or authorization token.

## 3. Relation-state candidate

`EventProjectionRelationStateCandidate` contains:

- one exact `ProjectSnapshotToken`;
- one immutable relation-limit policy value;
- zero or more accepted `EventProjectionLink` values.

The public `initial()` constructor creates an empty non-authoritative candidate at Project revision 0. Creating this value does not grant authority and does not publish Project state.

The relation collection is exposed read-only. A transition creates a new candidate and leaves the prior candidate unchanged.

## 4. Deterministic validation order

`build_event_projection_relation_state_candidate()` applies this binding order:

1. compare the current candidate snapshot with the revalidated addition base snapshot;
2. reject an exact base mismatch;
3. verify the revalidated next snapshot belongs to the same Project and is exactly current revision + 1;
4. reject link endpoints outside the current Project;
5. reject an exact duplicate relation already present in the current candidate;
6. reject growth at the configured relation-count limit;
7. copy the prior relation collection;
8. append the accepted relation;
9. return a complete next candidate carrying the exact next snapshot and unchanged limit policy.

No failure path mutates the prior candidate.

## 5. Defense in depth

Some checks can be unreachable through correctly composed sealed inputs. They remain intentional defense in depth because a future refactor must not make authoritative publication rely only on historical validation.

The relation-state transition independently checks:

- base snapshot equality;
- exact next-snapshot progression;
- Project scope;
- duplicate relation absence;
- bounded relation count.

A caller must not interpret successful historical preparation/revalidation as permission to skip current-state validation.

## 6. Resource bounds

The implementation defines:

`kAbsoluteMaxEventProjectionLinks = 1,048,576`

This is a reviewed implementation **safety ceiling**, not a musical rule, pricing tier, user quota, UI promise, or recommended project size.

`EventProjectionRelationLimits` carries an immutable per-state limit at or below that ceiling.

Rules:

1. zero is invalid;
2. values above the absolute ceiling are invalid;
3. a candidate at its configured limit rejects another addition before copying/growing the relation vector;
4. the limit value does not change during a relation-state transition;
5. changing the absolute ceiling requires a reviewed/versioned contract change and corresponding tests;
6. if a future persistence format stores or derives this limit, load/migration behavior must resolve it deterministically before authoritative publication.

Tests may use a smaller valid limit to exercise the rejection boundary without allocating large collections.

## 7. Allocation failure

Constructing a next immutable candidate can require memory allocation while copying/growing the relation collection.

`std::bad_alloc` is translated to explicit `allocation_failure`.

This is an operational fail-closed result, not evidence that the musical relation is semantically invalid.

Allocation failure:

- publishes nothing;
- does not change the prior candidate;
- must not consume a Project revision;
- must not trigger a hidden smaller/partial state;
- must not silently drop another relation to make room.

A later application layer may report/retry according to an explicit policy, but it must re-enter the complete current-state validation path.

## 8. Ordering semantics

The current bounded implementation stores accepted links in `std::vector` order for simple immutable candidate construction.

That storage order is **not**:

- musical time;
- semantic relation priority;
- identity ordering;
- Score/MIDI/TAB order;
- rendering order;
- playback order.

Business logic must not derive musical semantics from vector index or opaque ID sort order.

Deterministic replay depends on the same accepted command sequence and recorded ST identities. A future persistence format must either preserve an explicitly documented non-semantic command/storage order or canonicalize relations through its own reviewed deterministic schema; it must not accidentally turn container iteration order into musical meaning.

## 9. Revision and publication boundary

A successful transition produces a candidate carrying the revalidated `next_snapshot`.

It does **not** make that revision authoritative.

The safe future flow remains:

```text
enter owning Project mutation boundary
↓
read exact current Project state
↓
prepare/revalidate command against that state
↓
build complete next relation-state candidate
↓
validate complete Project result
↓
atomically publish complete Project state + exact next revision
↓
leave owning boundary
```

If the candidate crosses an asynchronous/task boundary before publication, it is stale-sensitive. The Project owner must compare/revalidate against current authoritative state again.

## 10. Security boundary

This package:

- accepts only ST-owned typed values;
- exposes no parser, renderer, plugin, SDK, or AI types;
- performs no filesystem access;
- performs no network access;
- launches no subprocess;
- activates no third-party dependency;
- performs no persistence;
- performs no AI inference;
- performs no realtime-audio work;
- performs no authoritative Project mutation.

AI/parser/UI/adapter output remains candidate data and cannot directly construct an accepted `EventProjectionLink`.

## 11. Required tests

The dependency-free CTest must prove at minimum:

- zero and over-ceiling limits are rejected;
- the reviewed production default equals the absolute ceiling;
- direct ordinary candidate→accepted-link construction is rejected at compile time;
- initial relation state is empty at Project revision 0;
- successful transition creates a distinct next candidate at revision +1;
- successful transition preserves the exact typed relation;
- successful transition leaves the prior candidate unchanged;
- stale/reused revalidated input fails with base snapshot mismatch;
- duplicate relation is rejected even if an inconsistent upstream view missed it;
- configured relation limit is enforced before state growth;
- failure leaves the prior candidate unchanged;
- 10,000 repeated transitions from identical input are deterministic;
- strict Build and Security Baseline candidate/current-main jobs pass.

## 12. Non-goals

This package does not implement:

- authoritative Project aggregate publication;
- mutex/actor/queue/transaction ownership;
- atomic pointer/state swap;
- endpoint entity storage;
- relation removal;
- cascade/detach deletion semantics;
- subtype cardinality policy;
- persistence/migration;
- undo/redo;
- Track/Clip schemas;
- realtime publication.

## 13. Acceptance criteria

The package is acceptable when:

- accepted relations cannot be directly forged from raw candidates through the public API introduced here;
- exact base and next snapshot checks are enforced;
- wrong-project and duplicate relations fail closed;
- resource exhaustion is bounded and explicit;
- allocation failure is non-mutating;
- prior relation state remains unchanged on success and failure;
- collection order is explicitly non-semantic;
- successful output remains explicitly non-authoritative;
- strict C++20 Build/CTest and Security Baseline pass;
- authoritative Project publication remains a separate reviewed slice.
