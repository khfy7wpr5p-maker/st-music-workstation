# ST Music Workstation — Authoritative Project Relation Publication v0.1

Status: bounded Project Core authoritative in-memory publication slice; full Project entity storage, concurrency policy and persistence remain deferred

## 1. Purpose

This package introduces the first owning Project aggregate mutation entry point for EventProjection relations.

It composes the already reviewed pipeline:

```text
prepared relation addition
        ↓
current Project-owned snapshot
+
stable current endpoint view
+
Project-owned authoritative relation state
        ↓
publication revalidation
        ↓
immutable next relation-state construction
        ↓
no-throw commit of relation state + exact next Project revision
```

Before this slice, preparation, publication revalidation and immutable next-state construction all remained non-authoritative. `ProjectAggregate::publish_event_projection_link_addition()` is the first operation that actually updates the owning in-memory Project revision and relation state after every gate succeeds.

## 2. Bounded Project aggregate

`ProjectAggregate` currently owns only the Project Core fields whose contracts have been accepted:

- exact `ProjectSnapshotToken`;
- authoritative EventProjection relation state and its resource policy.

It does not invent Track, Clip, MusicalEvent, Score, MIDI, TAB or Audio entity storage before those schemas exist.

The class is therefore a bounded Project Core aggregate, not a claim that the complete DAW Project model is finished.

Future Project mutations must extend the same global Project revision rather than create per-subsystem revision clocks.

## 3. Authoritative versus candidate values

The existing types retain their roles:

- `EventProjectionLinkCandidate`: untrusted/non-authoritative relation proposal;
- `PreparedEventProjectionLinkAddition`: sealed preparation result, stale-sensitive;
- `RevalidatedEventProjectionLinkAddition`: sealed current-state revalidation result;
- `EventProjectionRelationStateCandidate`: complete immutable next relation-state candidate;
- `ProjectAggregate`: current authoritative in-memory owner for the fields implemented in this slice.

A successful preparation, revalidation or relation-state transition does not change authority by itself.

Authority changes only when `ProjectAggregate` completes its final commit.

## 4. Single-owner mutation contract

`ProjectAggregate` is intentionally a single-owner control-thread object in this version.

Rules:

1. callers must not invoke publication concurrently on the same aggregate;
2. callers must not read the aggregate concurrently with publication without a higher-level reviewed synchronization mechanism;
3. the supplied endpoint validation view must remain stable for the duration of the publication call;
4. this API is not authorized for the realtime audio callback;
5. mutex, actor, queue and transaction scheduler selection remains a separate architecture decision.

This package closes the check→publish gap with respect to the aggregate's own Project revision and relation state under the single-owner contract. It does not claim multi-threaded synchronization.

## 5. Endpoint view versus relation ownership

The current aggregate does not yet own endpoint entity collections because their Project schemas have not been implemented.

The caller therefore supplies a read-only `EventProjectionValidationView` for endpoint existence.

The aggregate uses that view only for:

- Project-scope confirmation;
- MusicalEvent endpoint existence;
- Score/MIDI/TAB/Audio endpoint existence.

The aggregate deliberately does **not** delegate authoritative relation duplicate lookup to that external view.

During publication it constructs a pinned internal validation adapter whose `contains_link()` reads only the aggregate's own authoritative relation state.

This prevents a stale or inconsistently composed external relation lookup from bypassing or overriding Project-owned relation authority.

## 6. Deterministic publication order

`publish_event_projection_link_addition()` applies this order:

1. read the supplied endpoint-view ProjectId exactly once;
2. reject endpoint-view Project mismatch before endpoint/relation lookup;
3. pin the aggregate ProjectId and authoritative relation state into an internal validation adapter;
4. revalidate the sealed prepared addition against the aggregate's exact current global Project revision;
5. reject stale revision, invalid transition, missing/wrong-scope endpoints or authoritative duplicate relation;
6. build a complete immutable next relation-state candidate against the same current Project snapshot;
7. reject relation-state Project mismatch, relation limit, allocation failure or other transition failure;
8. verify returned next-state Project scope matches the owning aggregate;
9. commit the complete next relation state and exact next Project snapshot through no-throw assignments;
10. return the newly authoritative Project snapshot.

Every failure before step 9 leaves both authoritative revision and relation state unchanged.

## 7. Commit safety

All allocation occurs before authoritative state assignment.

The publication code compile-time asserts that:

- `EventProjectionRelationStateCandidate` move assignment is non-throwing;
- `ProjectSnapshotToken` copy assignment is non-throwing.

After validation and candidate construction succeed, the final mutation phase performs no callback, endpoint lookup, network access, filesystem access, third-party call or dynamic relation growth.

Under the single-owner contract, no failed operation can expose a partially accepted relation or consume a Project revision.

## 8. Error preservation

The top-level publication result distinguishes:

- endpoint-view Project mismatch;
- publication revalidation failure;
- relation-state transition failure;
- internal invariant failure.

When revalidation or transition fails, the exact existing lower-level error enum is preserved rather than flattened into an ambiguous generic result.

Examples include:

- stale prepared revision;
- endpoint missing;
- duplicate relation;
- relation limit exceeded;
- allocation failure.

## 9. Resource and failure behavior

The existing relation safety ceiling remains unchanged.

This package does not add a second capacity policy.

If immutable relation-state construction reports `allocation_failure` or `relation_limit_exceeded`:

- no relation is published;
- no Project revision is consumed;
- the prior authoritative state remains intact;
- no fallback relation is dropped;
- no partial state is committed.

## 10. Security boundary

This package:

- uses only ST-owned Core types;
- activates no third-party dependency;
- performs no filesystem access;
- performs no network access;
- launches no subprocess;
- performs no persistence;
- performs no AI inference;
- accepts no renderer/plugin/provider identities as authority;
- performs no realtime-audio work;
- does not expose a direct raw-candidate→authoritative-relation shortcut.

AI/parser/UI/adapter output must still enter through candidate validation and the sealed preparation/revalidation path.

## 11. Required tests

The dependency-free CTest must prove at minimum:

- a Project aggregate starts at revision 0 with empty authoritative relation state;
- valid prepared input publishes one relation and advances the global Project revision exactly once;
- stale prepared input fails without revision consumption;
- authoritative duplicate detection uses Project-owned relation state rather than the external endpoint view;
- an external relation claim cannot override empty Project-owned relation state;
- endpoint disappearance between preparation and publication is detected by revalidation;
- endpoint-view Project mismatch fails before endpoint lookup;
- relation resource-limit failure leaves revision and state unchanged;
- two successful distinct publications advance revisions 0→1→2;
- repeated identical fresh-aggregate publication is deterministic;
- strict Build/CTest and Security Baseline remain green.

## 12. Explicit non-goals

This package does not implement:

- Track/Clip/MusicalEvent/Score/MIDI/TAB/Audio authoritative entity collections;
- endpoint insertion/deletion commands;
- relation removal;
- cascade/detach deletion policy;
- subtype cardinality policy;
- undo/redo;
- persistence or migration;
- mutex/actor/queue/transaction scheduling;
- multi-threaded Project access guarantees;
- realtime publication;
- UI/application command routing;
- deployment or release packaging.

## 13. Acceptance criteria

This slice is acceptable when:

- there is exactly one Project-owned authoritative EventProjection publication entry point in this package;
- current global Project revision is revalidated immediately before next-state construction;
- authoritative relation duplicate state comes from the Project aggregate itself;
- success publishes relation state and the exact next global Project revision together under the single-owner contract;
- every failure is non-mutating and consumes no revision;
- lower-level typed error reasons remain inspectable;
- no second revision clock, framework type, external dependency or realtime capability is introduced;
- strict C++20 Build/CTest and Security Baseline pass;
- full Project entity storage and multi-thread synchronization remain separately reviewed future slices.
