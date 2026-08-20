# ST Music Workstation — Event Projection Publication Revalidation v0.1

Status: bounded Project Core publication-gate candidate; authoritative Project state publication remains deferred

## 1. Purpose

This package adds the final pure revalidation gate that must run immediately before a future authoritative EventProjectionLink publication.

A prepared relation plan may be valid when it is created and stale later. Therefore a prepared plan alone is never enough to mutate Project state.

The gate answers only:

> Is this sealed prepared plan still valid against the exact current Project identity, revision, endpoint state, and relation set?

It does not publish Project state.

## 2. Inputs

The revalidation gate receives:

- a sealed `PreparedEventProjectionLinkAddition`;
- the current ST-owned `EventProjectionValidationView`;
- the current authoritative `ProjectRevision`.

The current ProjectId is read exactly once from the validation view and pinned for the complete revalidation decision.

## 3. Deterministic order

The binding decision order is:

1. read current ProjectId once;
2. compare prepared base ProjectId with current ProjectId;
3. reject Project mismatch before relation lookup;
4. compare prepared base revision with current revision;
5. reject stale revision before relation lookup;
6. verify prepared next revision is exactly `current + 1` and that advancement is representable;
7. revalidate relation scope, endpoint existence, and exact duplicate absence using the pinned ProjectId;
8. return a sealed revalidated value.

No step silently repairs, rebases, retargets, retries, or rewrites the prepared plan.

## 4. Pinned Project identity

The existing relation validator now has an internal `detail` helper that accepts an already captured ProjectId.

The ordinary public validator still reads the ProjectId itself and preserves its previous behavior.

Publication revalidation captures the current ProjectId once and passes that same value through all relation checks. This prevents a mutable or incorrectly composed validation view from returning Project A for one check and Project B for another check inside the same decision.

This is defense in depth. A future authoritative Project owner must still hold a stable serialized/atomic state while revalidation and publication occur.

## 5. Failure model

`EventProjectionPublicationRevalidationError` contains:

- `prepared_project_mismatch`;
- `stale_prepared_revision`;
- `invalid_prepared_transition`;
- `event_wrong_project`;
- `projection_wrong_project`;
- `event_missing`;
- `projection_missing`;
- `duplicate_link`;
- `relation_validation_failure`;
- `none`.

Every failure returns no revalidated value and performs no Project mutation.

Unknown relation-validation values fail closed to `relation_validation_failure`.

## 6. Revalidated value

A successful result contains a sealed `RevalidatedEventProjectionLinkAddition`:

```text
baseSnapshot : exact current ProjectId + current revision
nextSnapshot : exact current ProjectId + prepared next revision
link         : exact prepared relation candidate
```

The value is not authoritative state and is not a lock, transaction, reservation, credential, or capability.

It records only that the prepared plan was revalidated against the supplied current state at that decision point.

## 7. TOCTOU boundary

This package reduces stale-plan risk but does not solve concurrency by itself.

Unsafe:

```text
revalidate
↓
release Project ownership
↓
state changes
↓
publish old revalidated value
```

Required future pattern:

```text
enter owning Project mutation boundary
↓
read current ProjectId/revision/state
↓
revalidate sealed prepared plan
↓
construct complete new immutable state
↓
atomically publish new state + exact next revision
↓
leave mutation boundary
```

If the revalidated value crosses an asynchronous boundary, it must be treated as stale-sensitive and the full current-state validation repeated before authoritative publication.

## 8. Security boundary

This package:

- performs no filesystem access;
- performs no network access;
- launches no subprocess;
- activates no third-party dependency;
- performs no AI inference;
- performs no plugin/renderer lookup;
- does not mutate Project state;
- does not persist data;
- does not execute in the realtime audio callback.

External or AI-originated candidates remain untrusted until they pass the full command/validation/publication path.

## 9. Tests

The dependency-free CTest must prove at minimum:

- a current prepared plan revalidates successfully;
- base and next snapshot values are exact;
- current ProjectId is read exactly once during publication revalidation;
- stale prepared revision rejects before relation lookup;
- prepared Project mismatch rejects before relation lookup;
- a duplicate introduced after preparation is rejected;
- an endpoint missing after preparation is rejected;
- repeated revalidation is deterministic;
- existing relation validator behavior remains green;
- strict Build and Security Baseline candidate/current-main jobs pass.

## 10. Non-goals

This package does not implement:

- authoritative Project aggregate storage;
- actual EventProjectionLink insertion;
- atomic pointer/state swap;
- mutex, actor, queue, transaction, scheduler, or thread ownership;
- relation removal;
- cardinality policy;
- Track/Clip schema;
- persistence or migration;
- undo/redo;
- realtime publication.

## 11. Acceptance criteria

The package is acceptable when:

- current Project identity is pinned once per decision;
- prepared Project/revision staleness fails before relation inspection;
- prepared next revision must be exact and checked;
- relation invariants are revalidated against current state;
- all failure paths are non-mutating and fail closed;
- success remains explicitly non-authoritative;
- strict C++20 Build/CTest and Security Baseline pass;
- authoritative immutable state publication remains a separate reviewed Project Core slice.
