# ST Music Workstation — Architecture Contract v1.0

Status: Stage 0-A baseline contract

## 1. Purpose

ST Music Workstation is a modular music workstation for audio, MIDI, musical notation, Guitar TAB, teacher review, sound libraries, and controlled AI-assisted workflows.

This document defines architectural invariants only. It does not select third-party libraries, implement production code, define licensing policy, or authorize AI models to modify project state.

## 2. Architectural goals

The system must:

- keep real-time audio behavior isolated from UI, network, storage, and AI workloads;
- use one shared musical-time model across audio, MIDI, score, TAB, transport, and cursor views;
- keep score, TAB, sound-library, plugin, and AI integrations behind explicit boundaries;
- preserve deterministic core behavior for transport, timing, routing, persistence, and validation;
- allow teacher-reviewed musical changes without coupling the core to any AI provider;
- remain testable in small, reversible development increments.

## 3. High-level layers

```text
Presentation / UI
       |
Application Commands
       |
Domain / Project Model
       |
Core Engines
       |
Adapter Boundaries
       |
Devices / Files / External Services
```

Dependency direction must point inward toward stable domain contracts. Core domain types must not depend on UI frameworks, AI providers, notation renderers, plugin SDKs, or device-specific APIs.

## 4. Core domains

The architecture reserves the following first-class domains:

- Project
- Musical Time
- Transport
- Audio
- MIDI
- Timeline / Tracks / Clips
- Mixer / Routing
- Score
- Guitar TAB
- Sound Library / Instruments
- Teacher Review
- Validation
- Plugin Adapters
- AI Adapters

The detailed schemas and type constraints for these domains are deferred to Stage 0-C.

## 5. Shared Musical Time invariant

Audio, MIDI, Score, Guitar TAB, playback cursor, transport, tempo, and meter must resolve through one shared musical-time system.

Independent timing models for score, TAB, and MIDI are prohibited.

The system must support deterministic conversion between musical position and playback position. Exact type definitions and conversion rules are deferred to Stage 0-C.

## 6. Real-time audio boundary

The real-time audio execution path must be isolated from non-real-time work.

UI operations must not directly execute inside the audio callback. Network calls, disk access, database access, blocking synchronization, AI inference, and other unbounded operations must not execute on the real-time audio thread.

Concrete real-time safety rules and verification requirements are deferred to Stage 0-D.

## 7. UI and command boundary

Presentation code must communicate with application/core behavior through explicit commands, messages, snapshots, or other bounded interfaces.

Direct UI mutation of real-time engine state is prohibited.

The UI may display engine/project state, but the domain model remains the source of truth.

## 8. Project-state invariant

External interchange formats are not the authoritative internal project state.

In particular:

- MusicXML is an import/export interchange format, not the complete internal project model.
- MIDI files are import/export data, not the complete internal project model.
- rendered notation output is a view, not the source of truth.
- sound-library assets are referenced resources, not embedded core logic.

Project persistence format and versioning are deferred to later contracts.

## 9. Score boundary

Notation parsing and rendering must be isolated behind Score adapters.

The core must retain stable musical identities and mappings needed for editing, playback, validation, and synchronization independently of any specific notation renderer.

A notation renderer must not become the authoritative project model.

## 10. Guitar TAB boundary

Guitar TAB must remain a distinct musical domain linked to shared musical events and time.

Generated string/fret/fingering results must be represented as proposals or accepted domain state, not as hidden mutations inside a rendering component.

TAB generation or ranking engines must be replaceable behind adapters.

## 11. Sound-library boundary

Sound libraries must remain external to DAW Core logic.

Instrument/sample packages may be discovered and referenced through a sound-library abstraction. Large assets must not be coupled to core source code or required for basic domain tests.

Exact library formats and supported engines are deferred to later stages.

## 12. Third-party dependency boundary

Third-party audio, MIDI, notation, plugin, file, instrument, or AI technologies must be integrated through explicit adapters where practical.

No third-party framework may silently become the architectural source of truth for Project, Musical Time, Teacher Review, or Validation.

Dependency selection, version policy, and license compatibility are explicitly deferred to Stage 0-B.

## 13. AI boundary

AI is an optional outer capability, not a DAW Core dependency.

AI systems may analyze inputs and produce candidates, rankings, annotations, restoration outputs, transcription proposals, fingering proposals, or other reviewable results.

AI systems must not directly mutate authoritative project state.

The required flow is:

```text
Input / Project Snapshot
        |
     AI Adapter
        |
Candidate / Proposal
        |
    Validation
        |
Teacher / User Review where required
        |
Accepted Domain Change
```

The DAW must remain usable for its deterministic core functions when AI services are unavailable.

Detailed AI safety, provenance, consent, and failure rules are deferred to Stage 0-D.

## 14. Teacher Review invariant

Teacher Review and Validation must remain independent of any AI provider.

Teacher-approved edits may include notation, rhythm, voice, chord, string, fret, fingering, or related musical corrections as later domain contracts permit.

Teacher approval and validation are explicit state transitions; they must not be inferred merely because an AI candidate exists.

## 15. Validation invariant

Validation must be callable independently from AI generation and UI rendering.

Invalid or unsupported candidate changes must be rejectable before they become authoritative project state.

Detailed validation schemas, ranges, and failure semantics are deferred to Stage 0-C and Stage 0-D.

## 16. Plugin boundary

Plugin hosting is not part of the initial core requirement.

Future plugin SDKs and plugin instances must be isolated behind a host abstraction so Project and Audio domain contracts do not depend directly on a specific plugin standard.

## 17. Development invariants

All subsequent implementation work must follow these rules unless this architecture contract is intentionally revised through review:

1. DAW Core must not depend on AI.
2. UI must not directly control or execute inside the real-time audio thread.
3. Audio, MIDI, Score, and TAB must share one Musical Time model.
4. MusicXML must not be the authoritative internal project state.
5. Sound libraries must not be embedded as DAW Core logic.
6. Third-party engines must be isolated behind explicit boundaries where practical.
7. AI may create candidates or proposals, not direct authoritative mutations.
8. Teacher Review and Validation must operate independently of AI.
9. Core deterministic behavior must remain available without network or AI services.
10. Development must proceed in small, reviewable, testable, reversible increments.

## 18. Stage 0-A non-goals

Stage 0-A does not:

- add C++ source code;
- add an audio or MIDI engine;
- add a UI framework;
- add build or CI configuration;
- choose or vendor third-party libraries;
- add notation, sampler, VST, or AI dependencies;
- define final serialization schemas;
- implement Score, TAB, Teacher Review, or AI behavior.

## 19. Follow-on contracts

The next baseline documents should be developed separately:

- Stage 0-B — Dependency and License Contract
- Stage 0-C — Domain and Musical-Time Contract
- Stage 0-D — Real-Time and AI Safety Contract
- Stage 0-E / later — repository, build, test, and CI baseline as approved

## 20. Stage 0-A acceptance criteria

Stage 0-A is complete when:

- this architecture contract is reviewed and accepted;
- no production DAW code or runtime dependency is introduced by the Stage 0-A change;
- architectural boundaries and prohibited dependency directions are explicit;
- subsequent dependency, domain, real-time, and AI contracts can be developed without changing the core direction defined here.
