# ST Music Workstation — Project Revision & Snapshot Token Contract v0.1

Status: Project Core revision primitive candidate; Project aggregate/command mutation integration remains deferred

## 1. Scope

This package defines the ST-owned Project revision primitive required for deterministic command revisioning and the Stage 0-D stale-snapshot/AI-result boundary.

It implements only:

- `ProjectRevision` as an opaque monotonic unsigned 64-bit revision value;
- `ProjectSnapshotToken` as exact `(ProjectId, ProjectRevision)` snapshot identity;
- checked next-revision derivation with overflow rejection;
- dependency-free tests for overflow, equality, project mismatch, revision mismatch, and deterministic repeated advancement.

It does not implement Project mutation, persistence, undo/redo, concurrency, AI requests, or realtime snapshots.

## 2. Revision semantics

A new logical Project begins with:

```text
ProjectRevision = 0
```

`ProjectRevision` is state-version metadata. It is not:

- Musical Time;
- event ordering;
- wall-clock time;
- a ProjectId;
- a command ID;
- an authorization token;
- a database row version supplied by an external framework.

The only semantic ordering operation reserved by this package is deriving the immediate next revision from the current authoritative revision.

## 3. Authoritative mutation rule

A future Project aggregate/command boundary must apply revisions as follows:

1. validate the complete proposed authoritative mutation;
2. ensure any optimistic/current-revision precondition still matches if the command contract requires one;
3. derive `next()` from the current revision;
4. if revision advancement would overflow, reject the mutation before publication;
5. atomically publish the validated new Project state with exactly the next revision.

A failed command does not advance revision.

A rejected/no-op request that produces no authoritative state change does not advance revision merely because it was attempted.

One accepted atomic authoritative mutation advances revision exactly once, regardless of how many internal fields/relationships changed inside that atomic command.

A compound operation that intentionally commits multiple independent authoritative transactions must expose those transactions explicitly rather than hiding multiple revision increments inside one apparently atomic command.

## 4. Overflow

The initial representation is exactly `std::uint64_t`.

All values from `0` through `UINT64_MAX` are representable persisted revision values. `UINT64_MAX` is a terminal representable revision: `next()` returns failure and must not wrap to zero.

Revision overflow is therefore a fail-closed Project mutation condition. The system must not:

- wrap;
- saturate and continue mutating at the same revision;
- reset revision;
- choose a random replacement revision;
- silently clone the Project to continue.

Any future revision-width migration requires an explicit persistence/compatibility contract.

## 5. Persistence and load

A future Project file format must persist the exact unsigned revision value.

Normal load may construct a `ProjectRevision` from the validated persisted integer value, but load validation must separately establish:

- schema/version validity;
- correct ProjectId;
- complete Project graph validity;
- no unsupported numeric representation;
- no duplicate/conflicting revision field.

`from_persisted()` is a value-construction boundary only. It does not validate an entire Project file and does not make parsed data authoritative by itself.

## 6. Snapshot token

`ProjectSnapshotToken` is:

```text
(ProjectId, ProjectRevision)
```

A snapshot token is current only when **both** values exactly equal the current authoritative Project identity and revision.

This prevents:

- a snapshot from Project A being accepted for Project B merely because both happen to have revision 7;
- an old Project A snapshot being accepted after Project A has advanced from revision 7 to 8.

The token does not prove that a particular payload truly corresponds to that revision. The owning snapshot/request construction boundary must capture the Project data and token consistently before publication/submission.

## 7. AI/background staleness

Stage 0-D AI/background requests may carry a ProjectSnapshotToken.

A returning candidate is not current merely because:

- its ProjectId exists;
- its revision is numerically less/greater;
- its request completed recently;
- model confidence is high.

For the baseline current-state check:

```text
candidate.projectId == current.projectId
AND
candidate.revision  == current.revision
```

must hold before a capability may treat the candidate as based on current Project state.

A later capability may define deterministic revalidation/rebase for a stale candidate, but it must not mutate the stored token or pretend an old snapshot was current.

## 8. Concurrency boundary

`ProjectRevision` itself is a value type; it is not a synchronization primitive.

The primitive does not make concurrent Project mutation safe. A future owning Project/application boundary must serialize or otherwise coordinate authoritative mutations so that two commands cannot both validate against revision N and publish different states both labelled N+1.

Therefore:

- reading a revision does not reserve it;
- calling `next()` does not reserve the returned value;
- atomic/transactional Project publication remains a separate Project Core responsibility;
- no global atomic counter is introduced by this package.

## 9. Determinism

Given the same ProjectRevision value:

- `value()` returns the same unsigned value;
- `next()` returns the same next value or the same overflow failure;
- snapshot-token equality/matching is exact and deterministic.

Wall-clock time, thread scheduling, random values, pointer addresses, unordered iteration, and external provider timing do not affect revision semantics.

## 10. Realtime boundary

Revision advancement and Project mutation are non-real-time control operations.

A realtime snapshot may later carry a precomputed revision/generation token for diagnostics/consistency, but the callback must not mutate the authoritative ProjectRevision or use it as Musical Time.

## 11. Security properties

ProjectRevision is not authorization. A client/provider/plugin/AI cannot gain mutation rights by supplying a newer or larger revision.

External revision values are untrusted input until parsed/validated by the relevant Project/persistence/network adapter.

A stale token fails current-state matching. It must not be silently rewritten to the current revision to make a candidate appear fresh.

## 12. Test requirements

The initial CTest must prove:

- initial revision is zero;
- `next()` increments exactly once;
- original immutable value remains unchanged;
- exact persisted value construction;
- terminal `UINT64_MAX` revision does not wrap;
- ProjectSnapshotToken matches only exact ProjectId + revision;
- same revision in a different Project does not match;
- changed revision in same Project does not match;
- repeated bounded advancement is deterministic;
- strong type is not implicitly constructible from raw uint64.

Future Project aggregate tests must prove:

- failed mutation does not increment;
- accepted atomic mutation increments once;
- revision-overflow mutation leaves state unchanged;
- competing stale precondition is rejected;
- snapshot token and payload are captured consistently.

## 13. Non-goals

This package does not:

- implement Project aggregate state;
- implement mutation transactions/locks;
- implement undo/redo;
- implement Project persistence;
- define command IDs;
- define collaboration/distributed revision vectors;
- define realtime snapshot generation;
- implement AI/background request orchestration;
- provide monotonic wall-clock timestamps.

## 14. Acceptance criteria

The package is acceptable when:

- ProjectRevision is an ST-owned 64-bit value type;
- revision 0 is the initial value;
- advancement is checked and never wraps;
- failed/non-mutating operations are contractually forbidden from consuming revisions;
- one accepted atomic Project mutation consumes exactly one revision;
- snapshot staleness uses exact ProjectId + revision equality;
- revision is explicitly not a synchronization primitive or Musical Time;
- compile/runtime negative tests pass under strict C++20;
- Security and Build Baseline candidate/current-main jobs pass;
- no Project aggregate/concurrency/persistence behavior is falsely claimed complete.
