# ST Music Workstation — Dependency & License Contract v0.1

Status: Stage 0-B.1 accepted and merged
Validated research date: 2026-08-12

## 1. Purpose

This document defines the dependency, licensing, version-selection, and activation rules for ST Music Workstation.

It extends `docs/ARCHITECTURE.md` without changing the Stage 0-A architectural direction.

Stage 0-B.1 is documentation-only. It does not install, vendor, link, compile, or activate any third-party dependency.

## 2. Binding dependency principles

1. ST Music Workstation owns its Project, Musical Time, Transport, Audio, MIDI, Score, Guitar TAB, Teacher Review, Validation, and persistence domain contracts.
2. Third-party frameworks and SDKs must remain behind explicit adapter boundaries.
3. Third-party types must not become authoritative core-domain types.
4. No dependency may be activated solely because it is popular or convenient.
5. Every activated dependency requires an explicit version, source, licence, platform, security, and adapter-boundary review.
6. `latest` is not a valid production dependency policy. Exact versions or immutable revisions must be pinned when a dependency is activated.
7. Code licences and content licences are separate. A permissive library licence does not grant redistribution rights for sound packs, samples, presets, fonts, scores, or other assets.
8. Dependencies that touch the real-time audio path require a separate real-time suitability review under Stage 0-D before activation.
9. AI providers and AI SDKs are not DAW Core dependencies and are outside this contract unless explicitly added later through an AI adapter review.

## 3. Dependency states

Dependencies in this contract use one of these states:

- **Approved baseline** — selected for the project direction, but activation may still occur in a later implementation stage.
- **Conditionally approved** — technically acceptable, but activation is blocked until stated licensing, commercial, security, or integration conditions are satisfied.
- **Approved candidate / not activated** — acceptable to evaluate later; not selected as a current implementation dependency.
- **Not selected** — intentionally excluded from the current baseline. Reconsideration requires a separate architecture decision.

## 4. Baseline technology decisions

| Technology | Role | State | Licence / commercial note | Contract rule |
|---|---|---|---|---|
| C++20 | Core implementation language standard | Approved baseline | Language standard; no framework licence | Core domain and engine code may target C++20. A later compiler/platform matrix must define concrete toolchains. |
| CMake | Build-system generator | Approved baseline | BSD 3-Clause | Build configuration will be introduced in Stage 0-E or another explicitly approved build stage. Stage 0-B.1 adds no CMake files. |
| Catch2 v3 | Unit/contract test framework | Approved baseline | BSL-1.0 | May be activated with the test/build baseline. Exact version must be pinned when added. |
| JUCE 9 | UI, device, MIDI, audio-I/O, and future plugin-host adapter framework | Conditionally approved | JUCE 9 EULA; commercial tier obligations apply | JUCE must remain outside authoritative ST domain contracts. Activation requires verification of the correct licence tier and seats for the Product Owner and Framework Users. |
| Verovio 6.x | Notation rendering/import adapter | Approved candidate / not activated | LGPL-family licence; repository includes LGPL/GPL notices | Use only behind a Score/Notation adapter. Verovio output is never the authoritative Score model. Linkage and redistribution obligations must be reviewed before activation. |
| FluidSynth 2.5.x | Optional SoundFont synthesizer adapter | Approved candidate / not activated | LGPL-2.1 | Use only behind a SoundFont/Synth adapter. SoundFont asset licences are separate from FluidSynth's code licence. Exact patched version must be selected at activation. |
| VST 3 SDK 3.8.x | Future plugin-host adapter | Approved candidate / not activated | MIT | May be used only in the future Plugin Host stage. VST SDK types must not propagate into Project or Audio domain contracts. |

## 5. JUCE licensing gate

JUCE is the highest licensing-risk baseline candidate because its licence is not equivalent to a permissive open-source licence.

The JUCE 9 EULA validated on 2026-08-12 defines Starter, Indie, Pro, and Educational licence types. The documented annual revenue/funding limits are up to USD 20,000 for Starter, up to USD 300,000 for Indie, and no revenue/funding limit for Pro. Educational use has separate eligibility requirements and may not be used for commercial, professional, promotional, or other for-profit activity.

Therefore:

1. JUCE must not be activated until the Product Owner's current eligibility and required seat count are verified.
2. The Educational licence must not be assumed merely because ST Music Workstation has educational use cases.
3. A future change in revenue, funding, team size, or distribution model may require a licence-tier change.
4. JUCE source or binary redistribution must follow the JUCE 9 EULA in force for the selected major version.
5. The framework must remain replaceable at the adapter boundary; Project and musical-domain models must not depend on JUCE classes.

Research snapshot: JUCE's official release list showed 9.0.0 as the latest JUCE 9 release at the validation date. This snapshot is not a permanent pin.

## 6. Notation dependency rule — Verovio

Verovio is acceptable as a notation engine because it supports MusicXML-related conversion/rendering workflows and is written in C++20, but it must remain an adapter.

Rules:

- MusicXML input must first pass ST validation/security gates defined by the Score import pipeline.
- Verovio may render or convert notation, but rendered SVG or Verovio's internal model is not source of truth.
- Stable ST musical identities and Musical Time mappings must live in ST-owned domain types.
- Exact Verovio version, linkage strategy, redistribution notices, and transitive/embedded licence obligations must be reviewed before activation.

Research snapshot: the official release list contained version 6.2.1 on the validation date. This snapshot is not a permanent pin.

## 7. SoundFont dependency rule — FluidSynth

FluidSynth is acceptable as an optional synthesizer adapter for SoundFont-based instruments.

Rules:

- FluidSynth is not DAW Core and must not own Instrument, Track, MIDI, Transport, or Project state.
- FluidSynth must not perform file/network/UI work inside the ST real-time audio callback unless a later Stage 0-D contract explicitly proves that usage safe.
- FluidSynth code licensing and SoundFont asset licensing must be reviewed separately.
- No `.sf2`, `.sf3`, sample pack, preset library, or derived sound asset may enter the repository or distribution without provenance and redistribution rights.
- The exact activated FluidSynth version must be a currently supported patched release. Versions known to predate relevant security fixes must not be selected merely for compatibility.

Research snapshot: FluidSynth 2.5.4 was the latest listed release on the validation date. FluidSynth 2.5.2 release notes included a fix for CVE-2025-68617. These facts are research evidence, not a permanent version pin.

## 8. Future plugin standards

### VST 3

The VST 3 SDK 3.8.x line is an approved future candidate. Upstream documents the SDK under the MIT licence.

Activation rules:

- only through `PluginHostAdapter` or equivalent boundary;
- no VST SDK types in core Project/Audio contracts;
- plugin scanning/loading must be isolated from deterministic project-state logic;
- plugin crash, timeout, state-serialization, and real-time safety rules require a later dedicated plugin-host contract.

### CLAP

CLAP is an approved candidate / not activated. The upstream CLAP repository is MIT licensed and defines a stable plugin-host ABI.

CLAP must not be added until the Plugin Host stage explicitly evaluates host maturity, validation tooling, platform support, and interaction with any JUCE-based host layer.

### LV2

LV2 remains an approved candidate / not activated for possible Linux/plugin expansion. Its exact SDK/specification version, licence obligations, packaging conventions, and host integration strategy must be revalidated from official LV2 sources before activation. Stage 0-B.1 does not approve an implementation dependency on LV2.

## 9. Alternative audio/MIDI/file adapters

The following technologies are retained as alternatives, not current dependencies:

| Technology | Possible role | State | Licence note |
|---|---|---|---|
| RtAudio | Alternative cross-platform audio-I/O adapter | Approved candidate / not activated | Upstream describes a custom permissive licence similar to MIT. Exact licence text must be preserved and re-reviewed before activation. |
| RtMidi | Alternative cross-platform MIDI-I/O adapter | Approved candidate / not activated | Upstream describes a custom permissive licence similar to MIT, with an additional modification-related condition. Exact licence text must be reviewed before activation. |
| libsndfile | Alternative audio-file read/write adapter | Approved candidate / not activated | LGPL-2.1 |

These candidates must not be pulled into the repository merely as fallbacks. Activation requires a specific need that is not safely satisfied by the active framework layer.

## 10. Tracktion Engine decision

Tracktion Engine is **not selected** as the ST Music Workstation core or project model.

Reason: ST Music Workstation already reserves ownership of Project, Musical Time, Transport, Timeline, Mixer, Score, TAB, Teacher Review, and Validation contracts. Adopting a high-level third-party DAW engine as the authoritative model would create avoidable architectural coupling and weaken the Stage 0-A inward-dependency rule.

Reconsidering Tracktion Engine requires a separate architecture review and explicit approval. It must not be introduced opportunistically through another dependency.

## 11. Sound-library and content licence policy

Every sound, sample, SoundFont, impulse response, preset, instrument definition, notation font, demo score, or other content asset must have a separate provenance record.

Before an asset can be bundled or redistributed, the record must establish at minimum:

- source and original publisher/author;
- exact asset/version or immutable identifier where possible;
- licence name and licence text/source;
- permission for redistribution;
- permission for commercial distribution if the product may be commercial;
- attribution requirements;
- modification/derivative-work restrictions;
- whether the licence applies to the asset itself or only to software used to create/read it.

Unknown, ambiguous, personal-use-only, non-redistributable, or unverifiable assets are prohibited from the distributable ST sound library.

Large sound assets must remain outside DAW Core source code and should use a separately versioned/distributable asset mechanism defined in a later Sound Library stage.

## 12. Version and provenance policy

When a dependency is activated, the implementing PR must record:

1. canonical upstream source;
2. exact version/tag/commit;
3. cryptographic or immutable identifier where practical;
4. licence identifier and required notice files;
5. direct and material transitive dependencies;
6. supported target platforms/toolchains;
7. known relevant security advisories at activation time;
8. static/dynamic/header-only or other integration method;
9. adapter boundary and allowed type surface;
10. update/removal strategy.

Unbounded branches such as `main`, `master`, `develop`, or `latest` must not be used as production dependency pins.

## 13. Security and maintenance gate

Before activation of any dependency:

- verify the project is actively maintained or explicitly accept the maintenance risk;
- review current upstream security advisories and recent security-relevant releases;
- reject known-vulnerable versions when a patched supported version exists;
- document unsupported platforms or toolchains;
- avoid abandoned forks unless a separate ownership/maintenance decision is approved;
- review parser/file-format dependencies as untrusted-input boundaries when they process user-provided files.

A clean licence does not imply a secure or suitable dependency.

## 14. Real-time activation gate

Any dependency used on the audio callback path must be reviewed under the Stage 0-D real-time contract before use.

The review must establish at minimum whether the proposed calls can allocate, lock, block, access disk/network, invoke UI, create threads, perform unbounded work, or throw across the callback boundary.

No statement in this dependency contract grants permission to place JUCE, FluidSynth, plugin SDK, file-parser, or other third-party calls directly in the real-time path.

## 15. Stage 0-B.1 non-goals

This stage does not:

- add production C++ code;
- add `CMakeLists.txt` or build presets;
- install or vendor JUCE, Verovio, FluidSynth, VST 3, Catch2, RtAudio, RtMidi, libsndfile, CLAP, LV2, or any other dependency;
- add plugin hosting;
- add SoundFonts or sample packs;
- choose compiler versions or final operating-system support ranges;
- define Musical Time schemas;
- define real-time callback implementation details;
- define AI providers or models.

## 16. Stage 0-B.1 acceptance criteria

Stage 0-B.1 is acceptable when:

- dependency ownership remains consistent with `docs/ARCHITECTURE.md`;
- baseline, conditional, candidate, and not-selected technologies are explicitly distinguished;
- JUCE commercial/licence risk is explicit and activation-gated;
- code licences are separated from sound/content licences;
- no dependency is activated by this documentation-only change;
- every future dependency activation requires version, provenance, licence, security, platform, and adapter review;
- real-time usage remains blocked until Stage 0-D verification;
- no third-party type is permitted to become the authoritative ST domain model.

## 17. Primary research sources

Validated on 2026-08-12. These URLs are evidence sources for this draft; they must be rechecked when a dependency is activated.

- JUCE releases: https://github.com/juce-framework/JUCE/releases
- JUCE 9 EULA: https://juce.com/legal/juce-9-licence/
- JUCE plans/licensing summary: https://juce.com/get-juce/
- Verovio repository: https://github.com/rism-digital/verovio
- Verovio releases: https://github.com/rism-digital/verovio/releases
- FluidSynth repository: https://github.com/FluidSynth/fluidsynth
- FluidSynth releases: https://github.com/FluidSynth/fluidsynth/releases
- VST 3 SDK: https://github.com/steinbergmedia/vst3sdk
- CLAP: https://github.com/free-audio/clap
- RtAudio: https://github.com/thestk/rtaudio
- RtMidi: https://github.com/thestk/rtmidi
- libsndfile: https://github.com/libsndfile/libsndfile
- CMake: https://cmake.org/download/
- Catch2: https://github.com/catchorg/Catch2
- LV2: https://lv2plug.in/
