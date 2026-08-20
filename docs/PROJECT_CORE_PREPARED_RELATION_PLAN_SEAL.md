# ST Music Workstation — Prepared Relation Plan Construction Seal v0.1

Status: bounded Project Core API-surface hardening candidate

## 1. Purpose

`PreparedEventProjectionLinkAddition` is a staging artifact created only after revision-precondition and relation-validation checks succeed.

A future authoritative transition must not be able to confuse an arbitrary caller-constructed value with a plan that actually passed those checks.

This package therefore seals direct construction of the prepared-plan type before any authoritative apply/storage layer is introduced.

## 2. Construction rule

`PreparedEventProjectionLinkAddition` is no longer an aggregate and is not default constructible.

Its component constructor is private and may be invoked only by the reviewed `prepare_event_projection_link_addition()` function.

External callers cannot construct a prepared plan directly from:

```text
ProjectSnapshotToken
ProjectRevision
EventProjectionLinkCandidate
```

A successful preparation result may still be copied as a value so it can move through ordinary non-real-time application/control code, but copying does not grant additional authority.

## 3. Immutable payload

The prepared payload remains readable through these fields:

- `base_snapshot`;
- `next_revision`;
- `link`.

All three fields are `const`.

After a plan is produced, callers cannot replace its base snapshot, change the proposed next revision, or swap its validated relation candidate through ordinary assignment.

This avoids a pattern such as:

```text
validate relation A against revision N
        ↓
receive prepared plan
        ↓
replace relation/revision fields with unrelated values
        ↓
pass the altered value to a future apply path
```

## 4. Authority boundary

Sealing construction does **not** make a prepared plan authoritative.

A prepared plan remains:

- non-persisted;
- non-reserving;
- non-transactional;
- stale-sensitive;
- invalid as a substitute for current Project-state validation.

Future apply/publication code must still compare `base_snapshot` with current ProjectId/revision and must operate inside the owning Project mutation boundary.

The private constructor is defense in depth against accidental or convenience-based plan forgery; it is not an authorization mechanism.

## 5. Security properties

This hardening:

- removes a public plan-forging construction surface;
- prevents ordinary post-construction field replacement;
- keeps the plan strongly typed;
- introduces no framework or third-party type;
- performs no I/O, network, subprocess, AI, persistence, or Project mutation;
- does not make the type a credential, capability, or security token.

## 6. Required tests

The dependency-free compile-time CTest must prove:

- the prepared type is not an aggregate;
- it is not default constructible;
- it is not publicly constructible from its three component values;
- it remains copy constructible;
- it is not copy assignable because its payload is immutable;
- it is not move assignable because its payload is immutable;
- all existing preparation behavior tests continue to pass unchanged;
- strict Build/Security candidate and current-main CI pass.

## 7. Non-goals

This package does not:

- make prepared plans authoritative;
- implement Project storage/publication;
- implement cryptographic signing or unforgeable capabilities;
- add serialization/persistence;
- add locking, actor, transaction, or thread ownership;
- add Track/Clip schemas;
- change relation validation/cardinality rules;
- add realtime behavior.

## 8. Acceptance criteria

The package is acceptable when direct component construction is rejected at compile time, prepared payload is immutable after creation, existing mutation-preparation semantics remain green, and Build/Security CI pass on candidate and current main.
