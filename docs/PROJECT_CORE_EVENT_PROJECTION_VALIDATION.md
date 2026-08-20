# ST Music Workstation — Event Projection Link Candidate Validation v0.1

Status: bounded Project Core relation-validation candidate; authoritative mapping storage/publication remains deferred

## 1. Purpose

This package implements the smallest production validation slice required by the Stage 0-C.2 `EventProjectionLink` contract without creating a Project aggregate, persistence schema, relation container, AI mutation path, or renderer/plugin dependency.

It validates a typed non-authoritative relation candidate against a read-only Project validation view.

The package answers only:

- does the event belong to the owning Project?
- does the projection belong to the same owning Project?
- does the referenced MusicalEvent currently exist in the supplied view?
- does the referenced projection currently exist in the supplied view?
- is the exact relation already present?

A successful result is **not** authoritative publication.

## 2. Typed projection identity

`ProjectionScopedId` is a closed typed union of:

```text
ScopedScoreEntityId
ScopedMidiEntityId
ScopedTabEntityId
ScopedAudioEntityId
```

TrackId, ClipId, MusicalEventId, ProjectId, arbitrary user-defined IDs, raw strings, renderer IDs, plugin handles, and external-source IDs cannot inhabit this projection union.

The projection kind is derived from the active nominal variant alternative. There is no separately mutable `projectionKind` field that can disagree with the projection ID type.

This implements the Stage 0-C.2 requirement that projection kind and nominal projection identity agree by construction rather than by repair after parsing.

## 3. Candidate only

`EventProjectionLinkCandidate` contains:

```text
eventId      : ScopedMusicalEventId
projectionId : ProjectionScopedId
```

It is a staging value only.

Constructing or successfully validating a candidate does not:

- attach the relation to authoritative Project state;
- reserve either identity;
- prove future validity after concurrent mutation;
- grant mutation authorization;
- create or repair missing entities;
- create a MusicalEvent merely because a Score/MIDI/TAB/Audio entity exists;
- infer semantic identity from equal pitch/time/source values.

## 4. Validation context

`EventProjectionValidationView` is an ST-owned read-only validation port.

It exposes only:

- the owning `ProjectId`;
- existence checks for scoped MusicalEvent/Score/MIDI/TAB/Audio identities;
- exact relation-presence lookup.

The port does not expose a framework container, database row, renderer object, plugin object, AI provider type, filesystem path, network resource, or mutable Project handle.

A concrete future Project aggregate may implement or adapt this view internally, but adapter/framework types must not leak into this Core contract.

## 5. Deterministic validation order

Validation precedence is binding:

1. event Project scope;
2. projection Project scope;
3. event existence;
4. projection existence;
5. exact stored-duplicate relation;
6. success.

Therefore:

- a fully foreign relation reports `event_wrong_project` first;
- a same-Project missing event reports `event_missing` before projection/duplicate checks;
- a missing projection reports `projection_missing` before duplicate lookup;
- only a relation whose endpoints exist in the owning Project can reach duplicate detection.

This order is deterministic and does not depend on collection iteration order, opaque ID ordering, wall-clock time, thread scheduling, AI confidence, or renderer state.

## 6. Failure semantics

The validation result can report:

- `event_wrong_project`;
- `projection_wrong_project`;
- `event_missing`;
- `projection_missing`;
- `duplicate_link`;
- `none`.

Every failure leaves authoritative state unchanged because this package has no mutation operation.

No failure path silently:

- retargets a ProjectId;
- creates an endpoint;
- drops an existing link;
- merges entities;
- regenerates IDs;
- substitutes a different projection kind;
- retries against a newer Project revision.

## 7. Concurrency and revision boundary

This validator operates against a supplied read-only view. A successful decision can become stale immediately after validation if Project state changes.

Therefore future authoritative relation publication must execute the complete validation inside the owning serialized/atomic Project mutation boundary and use the current Project revision/precondition rules.

The safe future pattern is:

```text
enter owning Project mutation boundary
        ↓
validate expected Project revision
        ↓
validate endpoint scope/existence
        ↓
validate relation duplicate/cardinality rules
        ↓
construct complete candidate Project state
        ↓
validate next revision
        ↓
atomically publish state + revision
```

This package performs only the endpoint/duplicate candidate-validation portion of that future flow.

## 8. Cardinality boundary

Stage 0-C.2 explicitly defers exact cardinality rules for concrete Score/MIDI/TAB/Audio entity subtypes.

Accordingly this package rejects only an **exact duplicate relation**. It does not invent unsupported one-to-one rules such as:

- one MusicalEvent must have exactly one Score projection;
- one projection can never participate in a later reviewed grouping relation;
- Audio must map one-to-one to symbolic events.

Primary-event/cardinality constraints must be added only after their owning domain subtype contracts are explicit.

## 9. Security boundary

Relation candidates crossing file/network/clipboard/plugin/AI/adapter boundaries remain untrusted until parsed into typed IDs and validated through the owning Project command path.

This package:

- performs no filesystem access;
- performs no network access;
- launches no subprocess;
- activates no third-party dependency;
- performs no AI inference;
- performs no renderer/plugin lookup;
- does not interpret IDs as paths, URLs, credentials, capabilities, or commands;
- performs no authoritative mutation.

## 10. Realtime boundary

Relation candidate validation and authoritative mapping mutation are non-real-time control operations.

This API is not authorized for execution inside the audio callback merely because its validation methods are bounded by interface shape or declared `noexcept`.

## 11. Required tests

The dependency-free CTest must prove at minimum:

- only Score/MIDI/TAB/Audio scoped IDs inhabit the projection union;
- Track/Clip/MusicalEvent scoped IDs cannot be used as projection identities;
- all four supported projection kinds validate when endpoints exist in the owning Project;
- projection kind is determined by nominal ID type;
- foreign event scope rejection;
- foreign projection scope rejection;
- event-scope precedence when both endpoints are foreign;
- missing event rejection;
- missing projection rejection;
- exact duplicate relation rejection;
- deterministic repeated validation;
- Build and Security Baseline candidate/current-main jobs pass.

## 12. Non-goals

This package does not implement:

- authoritative EventProjectionLink storage;
- Project aggregate mutation;
- relation insertion/removal commands;
- subtype-specific primary-event/cardinality rules;
- cascade/detach deletion behavior;
- persistence encoding or migration;
- automatic Score/MIDI/TAB/Audio matching;
- AI acceptance;
- renderer/plugin adapters;
- Track/Clip schemas;
- realtime relation mutation.

## 13. Acceptance criteria

The package is acceptable when:

- projection identity remains a closed nominal typed union;
- owning Project scope is validated explicitly for both endpoints;
- dangling event/projection references fail explicitly;
- exact duplicate links fail explicitly;
- validation precedence is deterministic;
- success is documented and implemented as a candidate decision only;
- no unsupported cardinality rule is invented;
- no third-party/framework type enters Core;
- strict C++20 Build/CTest and Security Baseline pass;
- authoritative atomic publication remains a separate later Project Core responsibility.
