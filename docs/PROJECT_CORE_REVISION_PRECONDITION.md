# ST Music Workstation — Project Revision Precondition Transition v0.1

Status: bounded Project Core transition primitive candidate; authoritative Project transaction/publication remains deferred

## 1. Purpose

This package adds the pure revision decision used by future authoritative Project commands before publication.

Conceptually:

```text
current revision
+
expected revision from command
        ↓
prepare_revision_advance
        ├─ stale expected revision → reject
        ├─ terminal UINT64_MAX     → reject overflow
        └─ exact match             → candidate next revision
```

The result is a transition decision only. It does not reserve a revision, lock a Project, mutate state, or publish an authoritative transaction.

## 2. Deterministic precedence

Validation order is binding:

1. compare `expected` with current authoritative revision;
2. if they differ, return `stale_expected_revision`;
3. only when they match, attempt checked advancement;
4. if current is `UINT64_MAX`, return `overflow`;
5. otherwise return exactly current + 1.

Therefore a stale command presented while current revision is terminal still reports **stale** rather than overflow. This makes the decision dependent first on the command's precondition and avoids leaking a different acceptance path for stale inputs.

## 3. Authoritative transaction boundary

A future Project owner/application command must perform the revision precondition check inside the same serialized/atomic mutation boundary that validates and publishes Project state.

The safe future flow is:

```text
enter owning Project mutation boundary
        ↓
read current authoritative revision
        ↓
validate command expected revision
        ↓
validate full proposed Project mutation
        ↓
prepare checked next revision
        ↓
atomically publish new state + next revision
```

A revision prepared outside that boundary is not a reservation. Another mutation may make it stale before publication.

## 4. Failure semantics

`stale_expected_revision`:

- no next revision;
- no Project mutation;
- caller may refresh/rebase through a later explicit command policy, but must not silently replace the expected revision and retry as if the original command were current.

`overflow`:

- no next revision;
- no Project mutation;
- no wrap, reset, saturation, clone, or wider hidden counter.

`none`:

- returns a candidate next revision only;
- does not prove the accompanying Project mutation is otherwise valid.

## 5. Concurrency

This primitive does not make concurrent mutation safe.

Two callers can both compute the same candidate next revision if they call the pure function against the same stale snapshot. The owning Project mutation boundary must prevent both from publishing.

No mutex, atomic counter, global registry, database transaction, or framework concurrency primitive is introduced here.

## 6. Realtime / AI boundaries

Revision transition is a control/application operation, not audio-callback work.

AI/background candidates may present an expected revision through a future Project command, but model confidence or provider identity cannot bypass the stale-precondition decision.

## 7. Tests

The existing Project revision CTest is extended to prove:

- exact current/expected match → current + 1;
- stale expected revision → explicit stale error and no next value;
- terminal current with matching expected → explicit overflow;
- terminal current with stale expected → stale takes precedence;
- 10,000 repeated exact transitions remain deterministic.

## 8. Non-goals

This package does not implement:

- Project aggregate storage;
- mutation locking/transactions;
- command schemas;
- automatic retry/rebase;
- persistence;
- undo/redo;
- distributed collaboration revisions;
- realtime mutation.

## 9. Acceptance criteria

The package is acceptable when:

- revision precondition comparison is exact;
- stale-vs-overflow precedence is deterministic;
- no failure returns a next revision;
- successful result is current + 1 only;
- no reservation/concurrency authority is falsely implied;
- strict C++20 Build and Security Baseline CI pass;
- future Project publication remains explicitly required to recheck/use the transition inside its owning mutation boundary.
