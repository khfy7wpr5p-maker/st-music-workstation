# ST Music Workstation — Event Projection Mutation Preparation v0.1

Status: bounded Project Core mutation-preparation candidate; authoritative Project storage/publication remains deferred

## 1. Purpose

This package composes the already accepted Project revision-precondition primitive with EventProjectionLink candidate validation into one deterministic preparation gate.

It deliberately stops before authoritative mutation.

The package answers:

> Given a current Project revision, an expected revision, a typed EventProjectionLink candidate, and a read-only validation view, is there a complete relation-addition plan that is valid against that exact base revision, and what exact next revision would publication require?

It does not publish that plan.

## 2. Prepared value

A successful result contains `PreparedEventProjectionLinkAddition`:

```text
baseSnapshot : ProjectSnapshotToken
nextRevision : ProjectRevision
link         : EventProjectionLinkCandidate
```

The base snapshot records the exact ProjectId/revision against which the relation was validated.

The next revision is exactly `base revision + 1` and is produced only after all relation validation succeeds.

The link is the exact typed relation candidate that passed validation.

A prepared value is not:

- a lock;
- a reservation;
- a transaction;
- an authoritative relation;
- a persisted command;
- proof that Project state has not changed since preparation;
- authorization to bypass a later Project mutation boundary.

## 3. Deterministic preparation order

The preparation order is binding:

1. compare `expectedRevision` with `currentRevision`;
2. if stale, fail immediately without reading relation state;
3. validate event/projection Project scope;
4. validate endpoint existence;
5. validate exact duplicate relation absence;
6. compute the exact next revision;
7. if the revision is terminal (`UINT64_MAX`), fail closed with `revision_overflow`;
8. return the prepared value.

This ordering is intentional.

A stale command must not inspect potentially irrelevant/current relation state.

After the revision precondition matches, an invalid relation is reported before revision overflow because no publication revision is needed for a relation that cannot be accepted.

Only a completely valid relation reaches next-revision preparation.

## 4. Error model

`EventProjectionMutationPreparationError` contains:

- `stale_expected_revision`;
- `event_wrong_project`;
- `projection_wrong_project`;
- `event_missing`;
- `projection_missing`;
- `duplicate_link`;
- `revision_overflow`;
- `relation_validation_failure`;
- `none`.

`relation_validation_failure` is a fail-closed fallback for an unknown/unrepresentable relation-validation result. It must never be silently relabeled as a known semantic condition.

Every non-`none` result contains no prepared value.

## 5. Project provenance

The prepared base snapshot uses the already validated candidate event ProjectId rather than re-reading the validation view after relation validation.

This prevents a second ProjectId read from creating a contradictory prepared token if a poorly composed/non-stable view changes between calls.

This does not make an unstable view safe for authoritative use. Production publication still requires one owning serialized/atomic Project boundary whose state cannot change between validation and publication.

## 6. Concurrency and atomic publication

Preparation alone does not close the check→publish race.

A caller must not:

```text
prepare outside Project ownership
        ↓
release ownership / allow state change
        ↓
publish later without revalidation
```

The future authoritative command path must perform preparation within the Project's owning mutation boundary and publish the complete new relation state plus `nextRevision` atomically.

If a prepared value crosses an asynchronous/thread/task boundary, its `baseSnapshot` must be treated as a staleness token; authoritative publication must revalidate the current ProjectId/revision and the complete relation invariants again.

## 7. No revision consumption

Preparation does not consume a revision.

Failed preparation does not increment anything.

Successful preparation only calculates the revision value that a future successful authoritative publication would use.

If publication is abandoned or later validation fails, the Project remains at its existing revision.

A revision becomes authoritative only when the owning Project aggregate atomically publishes the accepted new state and that exact revision together.

## 8. Security boundary

This package:

- does not mutate Project state;
- does not write persistence;
- performs no filesystem or network access;
- launches no subprocess;
- activates no third-party dependency;
- performs no AI inference;
- accepts no raw renderer/plugin/provider identity;
- does not interpret IDs as paths, URLs, capabilities, credentials, or commands;
- does not authorize execution inside the audio callback.

All relation candidate data remains subject to typed parsing and owning Project validation before preparation.

## 9. Realtime boundary

Mutation preparation is a non-real-time control operation.

It may perform endpoint/duplicate lookups through a validation view and therefore is not authorized inside the real-time audio callback.

The `noexcept` status of lower-level validation ports does not imply realtime suitability.

## 10. Required tests

The dependency-free CTest must prove at minimum:

- valid relation + current expected revision produces a prepared value;
- prepared base snapshot records exact ProjectId/current revision;
- next revision is exactly current + 1;
- the exact validated link is preserved;
- preparation does not mutate relation storage;
- successful preparation does not re-read ProjectId after relation validation;
- stale expected revision fails before any relation-state lookup;
- relation validation errors survive composition without semantic rewriting;
- exact duplicate failure survives composition;
- valid relation at `UINT64_MAX` fails with revision overflow;
- relation failure precedes overflow after a matching revision precondition;
- stale revision precedes both relation failure and overflow;
- unknown relation validation code maps to a generic fail-closed validation failure;
- repeated preparation is deterministic and non-mutating;
- strict Build/Security candidate and current-main CI pass.

## 11. Non-goals

This package does not implement:

- Project aggregate storage;
- authoritative EventProjectionLink insertion;
- locks, transactions, actor loops, queues, or scheduler ownership;
- relation removal;
- subtype-specific cardinality;
- cascade/detach deletion policy;
- persistence or migration;
- undo/redo history;
- Track/Clip schemas;
- audio/MIDI scheduling;
- AI proposal acceptance;
- realtime publication.

## 12. Acceptance criteria

The package is acceptable when:

- stale revision short-circuits before relation inspection;
- relation validation remains typed and deterministic;
- next revision is created only after relation success;
- overflow fails closed;
- prepared provenance records the exact validated Project/revision base;
- success is explicitly non-authoritative and non-reserving;
- no authoritative state changes occur;
- strict C++20 Build/CTest and Security Baseline pass;
- future atomic Project publication remains a separate reviewed slice.
