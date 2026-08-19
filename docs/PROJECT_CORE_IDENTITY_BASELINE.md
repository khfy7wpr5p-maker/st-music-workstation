# ST Music Workstation — Project Core Identity Primitive Contract v0.1

Status: Project Core identity primitive candidate after shadow hardening; allocation adapter/runtime persistence integration remain bounded follow-up work

## 1. Scope

This package is the first production Project Core implementation slice after Stage 0-E baseline acceptance.

It fixes and implements the ST-owned primitive representation required by `docs/DOMAIN_IDENTITIES.md` without adding a framework, database, UI, plugin SDK, audio/MIDI library, network service, or AI dependency.

The package defines:

- one bounded 128-bit opaque payload representation;
- canonical text encoding and strict parsing;
- nominal typed IDs for Project/Track/Clip/MusicalEvent/Score/MIDI/TAB/Audio roles;
- Project-scoped reference values that retain Project scope explicitly;
- the allocation contract that a later secure-random adapter must implement;
- negative tests for malformed/non-canonical/all-zero/cross-project cases;
- a closed construction surface: arbitrary raw bytes are not a public StrongId construction API.

## 2. Canonical identity payload

The ST-owned primitive payload is exactly 128 bits / 16 bytes.

Canonical textual encoding is exactly:

```text
32 lowercase hexadecimal characters
```

Example:

```text
00112233445566778899aabbccddeeff
```

Rules:

1. No separators, braces, prefixes, suffixes, whitespace, or case normalization.
2. Allowed characters are `0-9` and lowercase `a-f` only.
3. Uppercase hexadecimal is rejected rather than silently normalized.
4. The all-zero payload is reserved as invalid and is rejected.
5. Parsing first requires exact length 32, so oversized input is rejected before per-character decoding.
6. Canonical serialization always emits exactly 32 lowercase hex characters.
7. The 128-bit payload is opaque; no bit field encodes time, machine identity, entity type, authorization, filesystem location, or musical meaning.
8. This is an ST-owned identifier format, not a claim of UUID compatibility.

## 3. Nominal type safety and construction boundary

The production primitive exposes nominally distinct types for:

- `ProjectId`;
- `TrackId`;
- `ClipId`;
- `MusicalEventId`;
- `ScoreEntityId`;
- `MidiEntityId`;
- `TabEntityId`;
- `AudioEntityId`.

A common underlying representation does not make these types interchangeable.

There is no implicit ProjectId↔TrackId↔ClipId conversion and no generic raw-ID comparison that erases nominal kind.

The initial public StrongId value-construction surface is deliberately narrow:

- strict canonical text parsing is public;
- raw 16-byte representation helpers remain implementation detail;
- there is no public raw-byte constructor or generic `from_bytes`/`from_candidate_bytes` factory;
- StrongId is not default constructible;
- a future allocator may receive a narrow reviewed internal/friend construction boundary, but that package must not introduce a generic public raw-byte factory merely for convenience.

Strict text parsing is needed for staging/persistence/import boundaries, but **successful parsing does not establish authoritative entity existence, uniqueness, ownership, or mapping validity**. A parsed typed ID is only a syntactically valid value. Project load/import/application validation must still establish Project scope, nominal uniqueness, target existence, relation/cardinality rules, and any required provenance before authoritative state is published.

## 4. Project-scoped identity

Project-scoped entity identity is represented conceptually and in the initial primitive by:

```text
ProjectScopedId<LocalId>
  projectId : ProjectId
  localId   : nominal local ID
```

`ProjectScopedId` is constrained to the reviewed Project-local nominal ID roles; `ProjectId` itself and arbitrary user-defined types are not valid local-ID template arguments.

Equality requires both ProjectId and local ID equality.

Therefore the same local 128-bit payload in two different Projects is not the same scoped entity identity.

The local ID remains useful inside an already validated Project aggregate/context, but cross-project/domain boundaries must retain Project scope explicitly.

## 5. Allocation contract — Random128V1

The concrete allocation strategy reserved for later non-real-time implementation is `Random128V1`.

**Random128V1 is fixed here as a contract only; this package does not implement entropy acquisition, collision lookup, allocation retry, or authoritative insertion.**

Allocation procedure for the later reviewed implementation:

1. Request exactly 16 bytes from a reviewed operating-system cryptographically secure random source through an explicit adapter/application port.
2. If the entropy source reports failure or incomplete output, fail the allocation attempt; do not fall back to wall-clock time, process ID, machine ID, `rand()`, deterministic pseudo-random defaults, or a weaker provider.
3. Reject the all-zero candidate.
4. Check the candidate against the authoritative uniqueness scope required by `docs/DOMAIN_IDENTITIES.md`:
   - ProjectId: collision check against ProjectIds concurrently/authoritatively known to the allocating context;
   - Project-scoped nominal entity ID: collision check within that Project and nominal identity namespace.
5. On all-zero/collision candidate, request a fresh 16-byte candidate.
6. A single allocation operation performs at most 16 candidate attempts.
7. If no valid non-colliding candidate is obtained within the bounded attempt limit, return an explicit allocation failure and leave authoritative state unchanged.
8. The accepted ID is recorded in the accepted command/state. Deterministic replay reuses the recorded ID and never re-runs random allocation.

`Random128V1` uses the entire opaque payload as random data; it does not reserve UUID version/timestamp/node bits.

The exact OS APIs for Windows/macOS/Linux belong to a later adapter/platform package and require security review. Project Core must depend only on an ST-owned entropy/allocation port, never on an OS/framework type.

## 6. Security properties

IDs are identity values, not secrets, credentials, authorization tokens, capabilities, file paths, URLs, or executable instructions.

Security requirements:

- parse only bounded canonical representations;
- reject invalid data explicitly;
- never dereference or execute identifier text;
- never derive filesystem/network access from an ID;
- never treat external/provider/renderer/plugin IDs as authoritative merely because their text satisfies the ST lexical format;
- provenance/source IDs pass through explicit reconciliation before an ST identity relation/entity is accepted;
- a successfully parsed typed value is not proof that the referenced entity exists or is unique in an authoritative Project;
- collision handling never overwrites/merges an existing authoritative entity;
- secure-random generation occurs outside the real-time callback;
- entropy failure causes allocation failure rather than unsafe fallback;
- arbitrary raw byte buffers cannot construct authoritative StrongId values through a public generic factory in this package.

## 7. Determinism

Random allocation itself is intentionally non-deterministic, but accepted domain behavior is deterministic because the allocated value becomes explicit accepted command/state data.

Rules:

- replay uses recorded IDs;
- ID payload ordering is not musical ordering;
- no ordering comparator is part of the initial primitive contract;
- map/container semantics introduced later must not rely on random payload ordering for musical behavior;
- serialization/parsing round-trip is deterministic and canonical.

## 8. Real-time boundary

ID parsing, string serialization, secure-random allocation, collision lookup, and Project identity mutation are non-real-time operations.

No part of this package authorizes those operations inside the audio callback.

A callback may later receive already validated opaque IDs/snapshots if a concrete RT contract proves the representation/use is bounded, but it must not allocate or parse IDs there.

## 9. Failure semantics

Parsing returns explicit error categories for:

- wrong length;
- non-canonical character/case;
- reserved all-zero value.

Later allocation service failures must distinguish at least entropy-provider failure and bounded-attempt exhaustion/collision failure without partial authoritative mutation.

No parser path silently trims, case-folds, inserts/removes separators, repairs malformed values, generates replacement IDs, establishes Project existence, or resolves duplicate/cross-project references.

## 10. Test requirements

The initial dependency-free CTest must cover:

- valid canonical round trip;
- nominal types are distinct/non-convertible at compile time;
- StrongId is not default constructible;
- raw byte storage is not a public StrongId construction path;
- `ProjectScopedId` rejects ProjectId/arbitrary local roles at compile time and accepts reviewed Project-local nominal roles;
- short and long input rejection;
- uppercase rejection;
- separator/invalid-character rejection;
- all-zero rejection;
- whitespace rejection;
- maximum all-ones payload acceptance;
- oversized input rejection at fixed-length boundary;
- Project-scoped equality includes ProjectId;
- repeated canonical parsing is deterministic;
- strict warnings/build/security CI.

Later allocator implementation must add entropy-source failure, short-read, all-zero candidate, collision, retry-limit, duplicate/replay, and cross-project allocation tests.

Later Project/load validation must separately test that a syntactically valid parsed ID cannot establish nonexistent, duplicate, wrong-scope, wrong-kind, or dangling authoritative references.

## 11. Non-goals

This package does not:

- implement operating-system entropy adapters;
- implement Random128V1 allocation/retry/collision lookup;
- persist a Project file;
- create the Project aggregate;
- establish entity existence/uniqueness merely by parsing an ID;
- create Track/Clip collections;
- implement Musical Time types;
- implement hashing/container policy for IDs;
- implement undo/redo/revision commands;
- activate third-party dependencies;
- make IDs authorization/security tokens;
- add audio/MIDI/Score/TAB/plugin/AI runtime behavior.

## 12. Acceptance criteria

The Project Core identity primitive package is acceptable when:

- the representation is exactly 128-bit opaque data;
- canonical 32-lowercase-hex parsing/serialization is deterministic;
- all-zero and malformed inputs fail explicitly;
- nominal identity types are not implicitly interchangeable;
- arbitrary raw bytes cannot construct StrongId through the public package API;
- Project-scoped references are constrained to reviewed local ID roles and include ProjectId in equality;
- parsing is explicitly separated from authoritative Project existence/uniqueness validation;
- `Random128V1` is fixed as a bounded OS-CSPRNG-backed allocation contract with no weak fallback, while implementation remains deferred;
- deterministic replay uses recorded IDs rather than regenerated random values;
- C++ implementation contains no third-party/framework type;
- negative/unit tests pass under strict C++20 CI;
- Security and Build Baseline candidate/current-main jobs pass;
- no runtime/persistence/entropy-adapter capability is falsely claimed complete.
