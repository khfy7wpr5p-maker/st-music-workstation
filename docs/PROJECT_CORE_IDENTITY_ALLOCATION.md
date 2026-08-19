# ST Music Workstation — Project Core Identity Candidate Allocation v0.1

Status: Project Core bounded allocation-orchestration candidate; OS entropy adapter and authoritative Project insertion remain deferred

## 1. Purpose

This package implements the deterministic orchestration around the `Random128V1` contract fixed by `docs/PROJECT_CORE_IDENTITY_BASELINE.md` without adding operating-system APIs or Project mutation.

It introduces ST-owned ports for:

- obtaining exactly 16 candidate entropy bytes;
- observing currently known collisions in the relevant nominal identity scope;
- producing a bounded typed ID **candidate** or an explicit failure.

The allocator does not reserve an ID and does not publish authoritative Project state.

## 2. Entropy port

`IdentityEntropySource` is an ST-owned non-real-time port.

Its `read()` operation receives a 16-byte destination span from `Random128IdAllocator` and returns an explicit status plus byte count.

The allocator requires:

```text
status == success
bytes_written == 16
```

Any provider failure, short read, or over-reported byte count fails the allocation operation immediately. There is no fallback entropy source in Core.

A future platform adapter must prove that successful reads come from the reviewed operating-system cryptographically secure random facility. A deterministic/test source is valid only for tests and does not satisfy production composition requirements.

## 3. Candidate allocation

`Random128IdAllocator<Tag>::allocate_candidate()` performs at most 16 attempts.

For each attempt:

```text
request exactly 16 bytes
        ↓
provider success + exact byte count?
        ├─ no → explicit failure
        ↓ yes
all-zero candidate?
        ├─ yes → bounded retry
        ↓ no
known collision in supplied view?
        ├─ yes → bounded retry
        ↓ no
typed StrongId candidate
```

After 16 all-zero/collision attempts, the result is `candidate_exhausted`.

Entropy-provider failure and invalid byte count terminate immediately rather than being treated as normal collision retries.

## 4. Collision view is not a reservation

`IdentityCollisionView<Id>` is deliberately named a **view**, not a uniqueness authority or reservation service.

A successful `allocate_candidate()` result means only:

> this candidate was non-zero and did not appear in the supplied collision view at the moment it was checked.

It does **not** prove that the ID remains unique at later authoritative insertion time. Another command/thread/process may change the authoritative identity set after the view check.

Therefore a later Project aggregate/command implementation must:

1. receive the typed candidate;
2. enter the Project's owning mutation/transaction boundary;
3. revalidate nominal scope and current uniqueness against authoritative state;
4. reject on collision;
5. publish the new entity atomically only after all command validation succeeds.

The allocator must never be used as a substitute for atomic authoritative uniqueness validation.

## 5. Nominal scope

The allocator is specialized by the same StrongId tag that owns the private construction boundary.

Examples:

- `ProjectIdAllocator` returns only `ProjectId` candidates;
- `TrackIdAllocator` returns only `TrackId` candidates;
- `ClipIdAllocator` returns only `ClipId` candidates.

The collision view is typed to the same ID type, preventing a TrackId collision set from being accidentally supplied to a ProjectId allocator.

Project-local candidate allocation still requires the later owning Project command to validate the correct Project scope at insertion.

## 6. Raw-byte construction boundary

StrongId raw-byte construction remains private.

Only `Random128IdAllocator<Tag>` for the matching tag is a friend of `StrongId<Tag>` in this package.

This narrow friend exists solely to convert an entropy candidate that has passed the allocator's all-zero and collision-view checks into a typed candidate value.

It does not create a public `from_bytes` API and does not authorize adapters, UI, parsers, plugins, AI, or callers to construct StrongId directly from arbitrary bytes.

Canonical text parsing remains the public staging/persistence value parser and still does not establish authoritative entity existence or uniqueness.

## 7. Failure semantics

The allocation result reports:

- `entropy_failure` — provider explicitly failed;
- `incomplete_entropy` — provider reported a byte count other than exactly 16;
- `candidate_exhausted` — 16 candidate attempts were consumed by all-zero values and/or known collisions;
- `none` — a typed candidate was returned.

The result also reports the number of attempts consumed.

No failure path mutates authoritative Project state because this package contains no authoritative insertion operation.

## 8. Determinism and replay

Candidate generation is intentionally non-deterministic in production.

Deterministic Project command replay must not call `allocate_candidate()` again for an already accepted creation command. The accepted typed ID must be recorded as command/state data and reused during replay.

The allocator's retry sequence, entropy bytes, wall-clock time, or collision-view iteration order must not become musical ordering or Musical Time.

## 9. Real-time boundary

Identity entropy acquisition, collision lookup, candidate allocation, canonical string conversion, and authoritative identity insertion are non-real-time operations.

`Random128IdAllocator` is not authorized for use in the audio callback.

The entropy/collision interfaces do not imply RT suitability merely because their methods are `noexcept`.

## 10. Security boundary

This package intentionally contains no:

- operating-system entropy API;
- network call;
- filesystem access;
- subprocess;
- third-party dependency;
- framework type;
- global mutable ID registry;
- automatic authoritative Project mutation.

Production composition must fail closed if the reviewed OS CSPRNG adapter is absent or reports failure. A test/deterministic entropy source must not be silently selected as a production fallback.

## 11. Required tests

The dependency-free allocator CTest covers at minimum:

- successful first candidate;
- allocator requests exactly 16 bytes;
- entropy provider failure;
- short read rejection;
- over-reported byte count rejection;
- all-zero retry then success;
- known collision retry then candidate success;
- 16 all-zero attempts → bounded exhaustion;
- 16 known collisions → bounded exhaustion;
- provider failure after a prior rejected candidate → immediate fail-closed;
- nominally typed TrackId allocation;
- no seventeenth candidate attempt.

Later Project aggregate tests must cover the check→insert race boundary by proving authoritative insertion rechecks current uniqueness and rejects a candidate that became colliding after candidate generation.

Later OS-adapter tests must cover provider/API failure and prove there is no weak fallback.

## 12. Non-goals

This package does not:

- implement Windows/macOS/Linux entropy adapters;
- certify any entropy source as cryptographically secure;
- reserve IDs globally;
- guarantee uniqueness after `allocate_candidate()` returns;
- mutate Project state;
- implement Track/Clip collections;
- implement persistence or undo/replay storage;
- add a realtime-safe allocator;
- activate any dependency.

## 13. Acceptance criteria

The package is acceptable when:

- StrongId raw construction remains private and allocator friendship is tag-specific;
- entropy source is an ST-owned port with exact byte-count validation;
- all-zero and known collisions consume bounded retries;
- provider failure/short read fail immediately;
- retry count is capped at exactly 16;
- the API names and documents its collision input as a non-authoritative view;
- successful allocation is explicitly a candidate, not a reservation;
- later authoritative insertion is required to revalidate uniqueness atomically;
- negative tests cover all failure/retry boundaries;
- strict C++20 Build Baseline and Security Baseline pass;
- no OS adapter, third-party dependency, or Project mutation is falsely claimed complete.
