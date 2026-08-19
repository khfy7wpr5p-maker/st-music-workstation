# ST Music Workstation — Domain Identity & Mapping Contract v0.1

Status: Stage 0-C.2 contract candidate; production implementation and runtime evidence remain deferred

## 1. Scope

Stage 0-C.2 defines ST-owned identity and cross-domain mapping rules required by `docs/ARCHITECTURE.md`, `docs/SECURITY.md`, and the Stage 0-C.1 Musical Time contract.

This document is documentation-only. It does not add production C++ code, a persistence format, track/audio/MIDI/Score/TAB schemas, build configuration, dependencies, real-time behavior, plugin hosting, or AI implementation.

Stage 0-C.2 establishes:

- stable ST-owned project/domain identity roles;
- typed identity separation between Project, Track, Clip, Score, MIDI, TAB, and shared musical-event concepts;
- explicit cross-domain projection links;
- identity lifecycle rules for move/copy/delete/restore/import operations;
- deterministic command/replay expectations for newly allocated identities;
- validation and failure rules for duplicate, dangling, wrong-type, or cross-project references;
- boundaries preventing external IDs, renderer IDs, plugin IDs, memory addresses, collection indexes, or AI output from becoming authoritative identity.

## 2. Identity ownership invariant

ST Music Workstation owns authoritative domain identity.

External libraries, formats, adapters, renderers, plugins, devices, AI providers, database rows, file paths, collection indexes, and memory addresses must not define Project-domain identity.

The required direction is:

```text
External / Source Identity
          ↓
      Adapter Metadata
          ↓
 Validation / Reconciliation
          ↓
      ST-owned Identity
          ↓
 Authoritative Domain State
```

An external identifier may be preserved as provenance metadata, but equality of external identifiers does not by itself establish equality of ST entities.

## 3. Identity roles

Stage 0-C.2 reserves the following nominal identity roles:

```text
ProjectId
TrackId
ClipId
MusicalEventId
ScoreEntityId
MidiEntityId
TabEntityId
AudioEntityId
```

These are **nominally distinct** identity types even if a later implementation chooses a common underlying byte representation.

Rules:

1. A `TrackId` must not be accepted where a `ClipId` is required.
2. A `ScoreEntityId`, `MidiEntityId`, `TabEntityId`, or `AudioEntityId` must not be implicitly converted into a `MusicalEventId`.
3. Identity values are opaque. Business logic must not infer timing, ordering, pitch, track position, creation time, user identity, or entity kind from the identifier payload.
4. The concrete binary/text encoding and generator are not selected in Stage 0-C.2. Before production implementation, the representation must be ST-owned, bounded, serializable, collision-resistant for the chosen allocation strategy, and platform-independent.
5. Native pointers, object addresses, array/vector indexes, renderer handles, database auto-increment values supplied by an adapter, or SDK handles are prohibited authoritative IDs.

## 4. Project scope

Every authoritative entity belongs to exactly one ST Project context.

Rules:

- references between authoritative entities must resolve inside the same project unless a later reviewed cross-project transfer contract explicitly says otherwise;
- a source project must not hold a live authoritative pointer/reference into another project;
- copying/importing an entity into another project allocates new destination-project ST identities;
- source-project IDs may be retained as provenance metadata but must not be reused as destination authoritative identity merely for convenience;
- a project load must reject or quarantine cross-project references rather than silently retargeting them.

`ProjectId` identifies the project domain instance for persistence/integration purposes but does not grant filesystem, network, or authorization capability.

## 5. Stable identity semantics

For an authoritative entity, its ST identity is immutable for that entity's lifetime.

The following operations normally retain identity because they move or edit the same semantic entity:

- moving an entity to another valid MusicalPosition;
- changing validated musical attributes that do not replace the entity's semantic identity;
- moving a Clip between compatible Tracks where the later Track/Clip contract permits it;
- undo restoring the same deleted entity;
- redo restoring the same command result.

The following operations require new identities for newly created semantic entities:

- copy/duplicate;
- paste as a new entity;
- import as a new project entity;
- split when one entity becomes multiple independent entities;
- generation of a new accepted domain entity from a proposal;
- cross-project copy/import.

An edit that changes semantic cardinality must define its identity consequences explicitly. It must not silently reuse one ID for multiple independent entities.

Deleted IDs must not be intentionally reassigned to unrelated new entities. An allocator may discard a collided candidate before mutation, but authoritative domain mutation must never resolve a collision by replacing an existing entity.

## 6. MusicalEventId

`MusicalEventId` is an ST-owned identity for a musical occurrence that may have authoritative projections in more than one musical domain.

It exists to express semantic correspondence without making Score, MIDI, TAB, Audio, a renderer, or an external format the source of truth.

A `MusicalEventId`:

- does not contain or imply MusicalPosition;
- does not contain or imply MusicalDuration;
- does not contain or imply pitch;
- does not replace Score/MIDI/TAB entity identities;
- is not a MIDI note number, MusicXML note ID, renderer event ID, TAB string/fret coordinate, sample frame, or UI coordinate;
- may exist with projections in only one domain;
- may later acquire or lose projections through validated commands while retaining the musical occurrence identity when semantic identity is preserved.

Time remains authoritative through Stage 0-C.1 Musical Time. Identity must never become a second clock.

## 7. Projection identities

Score, MIDI, Guitar TAB, and Audio domains retain their own typed entity identities because their structures and cardinalities are not always equivalent.

Examples of why a single shared ID is insufficient:

- one sustained musical occurrence may require multiple notational fragments because of ties;
- one sounding note may correspond to multiple MIDI-level records such as note-on/note-off and later expressive data;
- Score may contain rests or layout/semantic notation that has no sounding `MusicalEventId`;
- MIDI may contain controllers, metadata, or transport-oriented events with no Score/TAB projection;
- Guitar TAB may contain technique/fingering structures distinct from Score notation entities;
- Audio clips/regions generally represent recorded/rendered media rather than one-to-one symbolic musical events.

Therefore `ScoreEntityId`, `MidiEntityId`, `TabEntityId`, and `AudioEntityId` remain independent typed identities.

## 8. EventProjectionLink

Cross-domain semantic correspondence is represented explicitly, conceptually:

```text
EventProjectionLink
  eventId        : MusicalEventId
  projectionKind : Score | MIDI | GuitarTAB | Audio
  projectionId   : matching typed projection identity
```

This is a conceptual contract, not a final storage schema.

Invariants:

1. `eventId` and `projectionId` must resolve in the same Project.
2. `projectionKind` must agree with the nominal projection identity type.
3. Stored duplicate links are invalid.
4. A dangling link is invalid authoritative state.
5. A projection must not be linked merely because pitch/time values happen to compare equal.
6. Link creation/removal occurs only through validated domain/application commands.
7. AI output, parser output, renderer output, or adapter output may propose a link but cannot directly authoritatively attach it.
8. A MusicalEventId may have zero or more projections in each domain where the domain model permits it.
9. A domain entity that is explicitly defined later as a primary event-bearing projection may have at most one primary `MusicalEventId`; containers/groups that represent multiple events must use explicit collections/relationships rather than aliasing one identity as many events.

The exact allowed cardinality of concrete Score/MIDI/TAB entity subtypes belongs to their later domain contracts. Stage 0-C.2 prohibits silent assumptions of one-to-one correspondence.

## 9. No equivalence by coincidence

Two domain entities are not the same semantic musical event merely because they share:

- MusicalPosition;
- MusicalDuration;
- pitch;
- channel;
- voice;
- staff;
- string/fret;
- renderer coordinates;
- source-file local identifiers.

Automatic matching may create a **candidate relation** for review/validation, but authoritative correspondence requires an explicit accepted ST mapping operation.

This prevents accidental identity collapse when chords, unisons, doubled parts, tied notation, repeated notes, overlapping MIDI events, or imported duplicates occupy similar timing/pitch coordinates.

## 10. Track and Clip identity

`TrackId` and `ClipId` are stable Project-domain identities.

Stage 0-C.2 does not define concrete Track/Clip schemas, but it fixes these identity rules:

- collection index is never identity;
- visual lane position is never identity;
- track ordering may change without changing TrackId;
- clip timeline movement may change MusicalPosition without changing ClipId;
- copy/duplicate creates a new ClipId/TrackId for the copy;
- a split operation that produces multiple independent clips must allocate new identities according to the later edit-command contract and must not leave two independent clips claiming one ClipId;
- deletion and restoration through undo restore the same identity when the same entity is restored;
- references to a Track/Clip must be validated after edits and before authoritative commit.

Track/Clip timing must use Stage 0-C.1 Musical Time or later explicitly reviewed playback-domain values; TrackId/ClipId cannot encode time.

## 11. SourceReference provenance metadata

External identities may be preserved in bounded untrusted provenance metadata, conceptually:

```text
SourceReference
  adapterKind
  sourceFingerprint
  externalLocalId
  optionalSourceVersion
```

This is not authoritative identity.

Rules:

- values are untrusted input and require length/encoding/resource limits;
- source IDs must not be interpreted as filesystem paths, URLs, commands, credentials, or executable types merely because they are stored as provenance;
- source paths/URLs, when needed by a later adapter, remain separate validated boundary data;
- a renderer ID or MusicXML/MIDI-local ID may assist re-import/reconciliation but cannot directly overwrite an existing ST mapping;
- a source fingerprint collision or ambiguous match must not silently merge authoritative entities;
- provenance metadata may be absent without invalidating an otherwise valid ST entity.

## 12. Identity allocation boundary

Identity allocation is a controlled service/boundary, not hidden domain magic.

For deterministic command/replay behavior:

1. Newly allocated IDs are resolved before or as explicit inputs to the authoritative mutation operation.
2. Once a command is accepted/recorded, replay uses the recorded ST IDs rather than generating fresh identities.
3. A collision with an existing authoritative ID causes rejection of that candidate allocation; it must not replace or merge an existing entity.
4. Domain behavior must not depend on the lexicographic/numeric ordering of opaque identity values.
5. Randomness, clock time, machine ID, thread scheduling, or memory layout used by an allocator must not leak into musical ordering or deterministic Musical Time calculations.

The exact ID generation algorithm is deferred until the implementation/build baseline and must receive security/provenance review before activation.

## 13. Atomic mapping mutation

A command that changes identity relationships must validate the complete intended result before publication as authoritative state.

Required pattern:

```text
command input
    ↓
resolve typed IDs
    ↓
validate existence + project scope + cardinality
    ↓
construct candidate relation state
    ↓
validate no duplicate/dangling/conflicting relation
    ↓
atomic authoritative publication
```

Failure must leave the prior authoritative relation state intact.

Hidden partial link creation is prohibited.

Deletion behavior must be explicit: a later command contract must choose and validate reject/cascade/detach semantics for each relationship. A storage container or ORM must not silently cascade authoritative domain relationships merely because of framework defaults.

## 14. Persistence/load integrity

Future project persistence must preserve ST identities and mappings stably.

At load/import-to-authoritative-state boundaries, validation must reject at minimum:

- duplicate authoritative IDs in a scope that requires uniqueness;
- malformed/empty/out-of-range identity encodings;
- wrong nominal ID kind;
- dangling EventProjectionLink targets;
- cross-project references;
- projectionKind/type mismatch;
- stored duplicate projection links;
- conflicting primary-event mappings where a later subtype contract allows at most one;
- unsupported identity/mapping schema version;
- source/external IDs masquerading as ST IDs.

Deserialization must build a non-authoritative candidate/staging graph, validate it completely, and only then publish a valid authoritative Project state.

Partial successful parsing must not expose a partially authoritative identity graph.

## 15. Security boundary

Identity data must be treated as untrusted when it crosses a file/network/clipboard/plugin/AI/adapter boundary.

Security requirements:

- bounded encoded length and collection counts;
- explicit parse errors;
- no arbitrary object/type deserialization from identity payloads;
- no path traversal or filesystem dereference through IDs;
- no network fetch triggered merely by resolving an ID;
- no subprocess execution;
- no secret/credential material embedded by design in identifiers;
- no pointer/address serialization;
- no direct plugin/renderer/AI mutation of the identity graph;
- relation validation before authoritative publication.

An identifier grants identity only; it is not an authorization token or capability.

## 16. AI and generated candidates

AI output is untrusted candidate data under `docs/SECURITY.md`.

AI may propose:

- a new candidate musical event;
- a Score↔MIDI↔TAB correspondence;
- a likely duplicate/reconciliation match;
- a fingering or notation relationship.

AI must not:

- allocate and publish authoritative ST identities behind the review/command boundary;
- overwrite an existing ST identity;
- attach/detach EventProjectionLinks directly;
- infer that matching source IDs imply authoritative identity;
- cause project-state mutation merely because a confidence threshold was exceeded.

Accepted AI-derived changes pass through validation and explicit project commands using the same identity rules as non-AI changes.

## 17. Renderer, plugin, and SDK boundaries

Renderer, plugin, device, and SDK identifiers are adapter-local handles.

Rules:

- renderer node IDs do not become ScoreEntityId;
- plugin instance handles/pointers do not become TrackId/ClipId/AudioEntityId;
- MIDI-library object identity does not become MidiEntityId;
- notation-library object identity does not become ScoreEntityId;
- device IDs do not become Project-domain identities;
- adapters maintain explicit translation tables where stable association is required.

Destroying/recreating an adapter object must not silently change authoritative ST identity.

## 18. Timing relationship

Identity and timing are separate axes.

All authoritative musical timing references defined by later domain entities must use Stage 0-C.1 `MusicalPosition` / `MusicalDuration` or an explicitly reviewed playback representation derived from Musical Time.

Rules:

- ID ordering is not event ordering;
- ID generation time is not MusicalPosition;
- a mapping relation does not create a timing grid;
- external tick/division/sample/frame values remain adapter/playback representations, not identity;
- two projections linked to one MusicalEventId must not maintain conflicting independent authoritative clocks.

Later validation contracts must define how a shared event and its projections reconcile timing edits without introducing multiple sources of truth.

## 19. Determinism

For a persisted valid Project identity graph:

```text
same serialized ST identities
+
same validated relation graph
+
same accepted command inputs
=
same resolved identity/mapping result
```

Determinism requirements:

- map/set iteration order must not define semantics unless an explicit semantic ordering field exists;
- opaque ID sort order is not musical order;
- deterministic replay uses recorded IDs;
- duplicate/collision/conflict handling must produce deterministic rejection;
- source-reconciliation heuristics may produce candidates but must not silently mutate authoritative mappings.

## 20. Validation failure semantics

Validation must reject or explicitly quarantine, as appropriate to the later persistence/import contract:

- duplicate IDs;
- empty/malformed IDs;
- wrong nominal ID type;
- cross-project references;
- dangling links;
- duplicate stored links;
- projection-kind/type mismatch;
- conflicting primary mappings;
- unauthorized direct external/AI mapping mutation;
- source IDs used as authoritative ST IDs without validated allocation/reconciliation;
- collisions that would replace an existing entity;
- copy/split operations that create multiple independent entities while reusing one identity;
- mapping operations that would publish partial authoritative state.

Failure must be explicit. Silent ID regeneration during load, silent relation dropping, silent merge-by-source-ID, or best-effort repair that changes semantic identity is prohibited unless a later dedicated recovery mode presents the change as non-authoritative and reviewable.

## 21. Prohibited shortcuts

The following violate Stage 0-C.2:

- using array/vector index as Track/Clip/Event identity;
- using raw pointers or framework object addresses as persisted IDs;
- making MusicXML/MIDI/renderer/plugin IDs authoritative ST IDs;
- forcing Score, MIDI, TAB, and Audio entities into one shared identity when their semantic cardinalities differ;
- treating equal pitch/time coordinates as proof of identity;
- allowing AI/parser/renderer/plugin code to mutate authoritative mappings directly;
- reusing a deleted ID for an unrelated entity intentionally;
- silently regenerating IDs when a project contains duplicates/conflicts;
- relying on opaque ID ordering for musical ordering;
- encoding filesystem paths, credentials, authorization, or executable instructions as identity semantics;
- publishing a partially validated relation graph.

## 22. Stage 0-C.2 non-goals

Stage 0-C.2 does not define:

- the concrete identifier byte/string format or generator algorithm;
- the platform-independent numeric envelope required by Stage 0-C.1;
- complete Project/Track/Clip field schemas;
- Score notation grammar or Score entity subtypes;
- MIDI event schema/scheduling;
- TAB fingering/technique schema;
- AudioClip/sample/asset storage schema;
- exact EventProjectionLink persistence encoding;
- project file format/version migration;
- undo/redo transaction implementation;
- project revision/concurrency control;
- tempo/meter behavior beyond Stage 0-C.1;
- real-time callback/thread ownership implementation;
- plugin hosting;
- AI models/providers;
- production source code, dependencies, build files, or release behavior.

These require later contracts/stages.

## 23. Stage 0-C.2 acceptance criteria

The Stage 0-C.2 contract baseline is acceptable when:

- ST owns authoritative identity;
- Project/Track/Clip/MusicalEvent/Score/MIDI/TAB/Audio identity roles are nominally separated;
- opaque IDs carry no implicit time/order/business semantics;
- `MusicalEventId` expresses optional semantic occurrence identity without replacing projection identities;
- Score/MIDI/TAB/Audio cardinality differences are preserved rather than forced into one-to-one identity;
- cross-domain correspondence is explicit and validated through EventProjectionLink-style relations;
- equal pitch/time/source IDs do not silently establish identity;
- move/copy/delete/restore/import identity lifecycle rules are explicit;
- deterministic replay uses recorded allocated IDs;
- external/source IDs remain untrusted non-authoritative provenance;
- duplicate, dangling, wrong-type, cross-project, and conflicting relations fail explicitly;
- relation mutations validate completely before atomic publication;
- identity cannot become a second Musical Time clock;
- UI/renderer/plugin/SDK/AI identities remain behind adapters;
- no production implementation or persistence encoding is prematurely selected.
