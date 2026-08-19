# ST Music Workstation — Security Baseline Contract v0.1

Status: Stage 0-S.1 draft security contract

## 1. Purpose and gate order

Security is a prerequisite to functional implementation.

The required development order is:

```text
FRESH-READ
    ↓
SECURITY GATE
    ↓
Architecture / Domain Contracts
    ↓
Implementation
    ↓
Tests + Negative Tests
    ↓
Shadow Review
    ↓
CI
    ↓
Merge
```

This contract extends `docs/ARCHITECTURE.md` and `docs/DEPENDENCIES.md` without changing their ownership or dependency-direction rules.

The governing trust flow is:

```text
UNTRUSTED INPUT
      ↓
VALIDATION / PARSING BOUNDARY
      ↓
NORMALIZED ST-OWNED VALUE / COMMAND
      ↓
CONTROLLED DOMAIN MUTATION
      ↓
CONTROLLED ENGINE BOUNDARIES
      ↓
OUTPUT
```

External data must never become authoritative project state merely because a parser, renderer, plugin, SDK, device, network service, AI provider, or UI component accepted it.

## 2. Current repository security baseline

Validated against the repository state at the start of Stage 0-S.1:

- the repository is public;
- `main` is the default branch;
- branch protection is not currently enabled on `main`;
- no GitHub Actions workflow is present yet;
- no production C++ code or build system is present yet;
- no third-party dependency is installed, vendored, linked, or activated;
- the current tree contains documentation only;
- Stage 0-C.1 exists as a separate open draft PR and is not part of this Security Gate change.

Repository visibility is not changed by this stage. Because the repository is public, committed data must always be treated as externally visible and permanent even if later removed from the current tree.

## 3. Trust classification

Unless a later reviewed contract proves a narrower trust assumption, the following inputs are **untrusted**:

| Input | Default trust | Required boundary |
|---|---|---|
| User text / UI input | Untrusted | Validation → ST command/value |
| Project files | Untrusted | Versioned parser → validation → normalized project data |
| MusicXML | Untrusted | XML security gate → schema/capability validation → ST Score model |
| MIDI files / streams | Untrusted | MIDI parser → range/state validation → ST MIDI model |
| Audio files / metadata | Untrusted | Bounded decoder/metadata adapter → validated audio descriptor/data |
| Plugin binaries | Untrusted native code | Plugin host boundary / isolation policy |
| Plugin metadata/state/presets | Untrusted | Bounded parser → validation → plugin adapter state |
| SoundFonts | Untrusted | Provenance + bounded parser/adapter |
| Sample packs / presets | Untrusted | Provenance + path/content validation |
| AI responses | Untrusted proposals | Validation → deterministic policy → explicit command |
| Network responses | Untrusted | Protocol/schema validation → bounded adapter |
| External SDK objects | Untrusted boundary data | Adapter → ST-owned values |
| Filesystem paths | Untrusted | Canonicalization + allowed-root policy |
| Drag-and-drop content | Untrusted | Type/size/path/content validation |
| Clipboard/imported data | Untrusted | Type/size/content validation |

Trusted code does not imply trusted data. A trusted parser library may still receive malicious input and must be isolated by limits and validation.

## 4. Authoritative-state rule

Authoritative ST project state may be changed only by validated ST-owned commands or domain operations.

The following must not directly mutate authoritative project state:

- AI output;
- MusicXML parser objects;
- MIDI parser objects;
- audio decoder metadata objects;
- plugin instances or plugin state objects;
- notation renderer objects;
- UI widgets;
- external SDK objects;
- network responses;
- raw filesystem data.

Required mutation flow:

```text
external input
    ↓
adapter / parser
    ↓
validation
    ↓
normalized ST-owned value / command
    ↓
controlled mutation
```

Validation failure must be explicit. Silent coercion, guessing, hidden fallback, or partial authoritative mutation is prohibited when semantic meaning may change.

## 5. Secret and credential policy

Secrets must never be committed to this repository.

Prohibited repository content includes real:

- API keys;
- personal access tokens;
- passwords;
- private keys;
- OAuth/client secrets;
- cloud credentials;
- service-account credentials;
- signing secrets;
- production connection strings containing credentials;
- `.env` files containing secret values.

Rules:

1. Secret values must not be copied into source, docs, tests, fixtures, PR descriptions, issue comments, logs, build output, or artifacts.
2. Examples must use unmistakably synthetic placeholders.
3. CI must not expose secrets to untrusted pull-request code.
4. If a real credential is discovered in repository history, do not repeat its value in reports. Treat rotation/revocation as a blocker requiring owner action.
5. Removing a secret from the current tree does not revoke it; leaked credentials must be considered compromised.
6. Generated local secret/config files must be ignored by repository hygiene rules.

## 6. Supply-chain and dependency security

`docs/DEPENDENCIES.md` remains binding. A dependency is not activated merely because it appears in architecture documentation.

Required activation sequence:

```text
NEED
  ↓
THREAT + LICENCE REVIEW
  ↓
OFFICIAL UPSTREAM / PROVENANCE
  ↓
EXACT VERSION OR IMMUTABLE REVISION
  ↓
ADAPTER BOUNDARY
  ↓
TESTS / NEGATIVE TESTS
  ↓
ACTIVATION
```

Every activated dependency must record, as applicable:

- canonical upstream;
- exact version/tag/commit;
- immutable commit or checksum/signature where practical;
- licence and required notices;
- relevant security advisories;
- maintenance status;
- material transitive dependencies;
- binary provenance;
- supported platforms/toolchains;
- adapter boundary and allowed type surface;
- removal/update strategy;
- real-time suitability if it can execute on the audio path.

Mutable dependency references such as `main`, `master`, `latest`, or unpinned action tags are not acceptable production pins.

Unknown binary provenance blocks vendoring or distribution.

## 7. GitHub Actions security

All workflows must follow least privilege.

Required rules:

1. Declare explicit workflow `permissions`.
2. Default to `contents: read` and add write permission only when a reviewed job strictly requires it.
3. Prefer immutable full commit SHAs for external Actions.
4. Do not use `pull_request_target` to execute untrusted pull-request code with privileged credentials.
5. Do not expose repository secrets to untrusted pull-request jobs.
6. Do not interpolate untrusted event fields directly into shell commands.
7. Avoid `curl | sh`, remote script execution, or mutable installer endpoints.
8. Treat downloaded artifacts and caches as untrusted inputs unless provenance is established.
9. Release/deployment credentials must be isolated from ordinary PR validation.
10. CI checks must not be weakened merely to obtain a green result.

Stage 0-S is not complete until an automated security-regression workflow exists and passes on the reviewed head and on `main` after merge.

## 8. File and parser security

All user-provided file formats are hostile-input surfaces.

At minimum, future MusicXML, MIDI, project, preset, plugin-state, audio-metadata, SoundFont, and archive/container parsers must address:

- malformed/truncated input;
- oversized input;
- integer overflow/underflow;
- recursion/depth limits;
- decompression/expansion limits;
- invalid encoding;
- duplicate/conflicting fields;
- unsupported semantic values;
- path traversal;
- symlink/allowed-root escape where filesystem extraction is involved;
- resource exhaustion;
- parser exceptions/errors;
- partial parse and partial mutation;
- deterministic rejection behavior.

A parser may produce an intermediate parse result, but authoritative domain state is created only after validation succeeds.

### 8.1 MusicXML / XML

MusicXML is untrusted XML.

The import boundary must reject or disable unsafe XML features such as external entity/network expansion where the selected XML stack could permit them. Structural/schema validation alone is not sufficient; ST capability validation must reject unsupported semantic constructs when silently dropping them would change musical meaning.

### 8.2 MIDI

MIDI input must validate lengths, event ordering, delta/tick arithmetic, channel/data ranges, tempo/meter values, malformed variable-length data, duplicate/conflicting state, and resource limits before conversion to ST-owned Musical Time/domain values.

Raw PPQ/tick values must never become a second authoritative project clock.

### 8.3 Audio files and metadata

Decoders must enforce bounded lengths/channel counts/sample rates/metadata sizes and must treat declared sizes as untrusted. Decode failure must not produce partially authoritative clips.

### 8.4 Project / preset serialization

Future project and preset formats require:

- explicit schema/version identifiers;
- bounded collection/string/blob sizes;
- deterministic validation;
- unknown/unsupported semantic-field policy;
- atomic or recoverable writes;
- no deserialization into arbitrary executable types;
- no path traversal or unrestricted external-file replacement.

## 9. Filesystem boundary

Filesystem access belongs behind explicit application/adapter boundaries.

Rules:

- normalize and validate user-controlled paths;
- define allowed roots for extraction or managed assets;
- reject traversal outside an allowed root;
- avoid following untrusted symlinks across boundaries unless explicitly validated;
- use atomic/recoverable writes for authoritative project persistence;
- do not overwrite source/import files as an implicit side effect;
- temporary files must have controlled lifetime and permissions appropriate to their contents;
- real-time audio code must not access the filesystem.

## 10. Network boundary

Core deterministic behavior must not require network access.

Network access is allowed only through explicit adapters and must define:

- endpoint/provider ownership;
- authentication handling;
- timeout/cancellation;
- size/rate limits;
- response validation;
- failure semantics;
- retry bounds;
- privacy/data-transfer implications where applicable.

Network responses are untrusted and cannot directly mutate authoritative project state.

The real-time audio thread must never perform network I/O.

## 11. Subprocess / process boundary

Spawning external processes is not a Core capability.

If a later adapter requires a subprocess:

- use explicit executable/argument arrays rather than shell interpolation where possible;
- treat all user-controlled arguments as untrusted;
- define timeout, cancellation, output-size, exit-status, and cleanup behavior;
- do not inherit secrets or broad environment state unnecessarily;
- validate produced files/data before import;
- never invoke subprocesses from the real-time audio callback.

## 12. Real-time audio safety boundary

Real-time safety is a security, integrity, and availability boundary.

Inside the real-time audio callback, unless a later Stage 0-D proof explicitly establishes a bounded alternative, the following are prohibited:

- filesystem access;
- network calls;
- database access;
- heavy parsing;
- AI inference;
- UI work;
- logging that may block/allocate;
- blocking locks/waits;
- uncontrolled allocation/deallocation;
- exceptions escaping the callback;
- plugin scanning/loading;
- unpredictable system calls;
- unbounded loops/work queues;
- any operation whose worst-case execution cost is not bounded for the supported callback budget.

Real-time data exchange must use bounded, reviewed ownership/queue mechanisms with work prepared on non-real-time threads.

## 13. Memory and thread ownership

Future runtime code must make ownership and thread authority explicit.

Required principles:

- no unsynchronized cross-thread mutation of authoritative state;
- bounded queues for real-time/non-real-time communication;
- explicit lifetime/ownership for buffers, project snapshots, plugin state, and device state;
- cancellation must not create use-after-free or stale-pointer access;
- callbacks must not outlive the state they reference;
- data published to the real-time thread must be immutable or transferred through a proven bounded synchronization mechanism;
- shutdown/error paths receive the same ownership review as steady-state paths.

## 14. Plugin-host security boundary

Third-party plugins are untrusted native code even when obtained from reputable vendors.

A future plugin-host stage must address:

- crash and exception containment;
- hangs/timeouts during discovery, load, save, and state restore;
- excessive CPU or memory use;
- malformed metadata/state/preset data;
- unexpected filesystem/network behavior;
- incompatible ABI/architecture;
- real-time violations;
- corrupted state serialization;
- scan-cache invalidation and provenance.

Plugin SDK/types must remain behind the host adapter and must not define ST Project or Audio domain identity.

Where practical, scanning and other non-real-time plugin operations should use process isolation so a faulty plugin cannot crash the main application. The final isolation architecture belongs to the dedicated Plugin Host stage.

## 15. AI security boundary

AI output is always untrusted proposal data.

Required flow:

```text
Project Snapshot / Input
        ↓
     AI Adapter
        ↓
Untrusted Candidate / Proposal
        ↓
 Validation + Deterministic Policy
        ↓
 Explicit Project Command
        ↓
 Controlled Mutation
```

AI must not directly mutate:

- Project state;
- authoritative Musical Time;
- Score state;
- Guitar TAB state;
- audio graph/routing;
- plugin state;
- filesystem state.

AI calls must not run on the real-time audio thread. Provider/network failure must leave deterministic core behavior available.

## 16. Sound-library and content provenance

Code licence and content licence are separate security/provenance concerns.

SoundFonts, samples, impulse responses, presets, notation fonts, scores, and other distributable assets require provenance and redistribution rights before they enter a distributable package.

Unknown or unverifiable provenance blocks bundling. Large content assets must remain outside Core source code and must not be committed opportunistically.

## 17. Failure and crash isolation

Boundary failures must fail closed with respect to authoritative state.

Required behavior:

- parser failure → no partial authoritative mutation;
- dependency/adapter failure → explicit error/state, not silent semantic fallback;
- AI failure → proposal rejected/unavailable, Core remains usable;
- plugin failure → contained/reported as far as the host architecture permits;
- persistence failure → source/current valid project state remains recoverable;
- device failure → transport/audio state transition is explicit and bounded;
- validation failure → rejected command/value, not clamped/guessed meaning.

## 18. Determinism and integrity

For deterministic subsystems:

```text
same validated input
+
same authoritative state
+
same configuration
=
same domain result
```

Musical Time, tempo/meter conversion, routing, serialization, Score/TAB synchronization, MIDI scheduling contracts, and offline rendering must define nondeterminism explicitly if any is unavoidable.

Security fixes must not introduce hidden fallback or silent data loss to preserve apparent compatibility.

## 19. Security severity and merge policy

Security findings use these operational severities:

- **CRITICAL** — immediate credential exposure, arbitrary code execution/privilege boundary break, catastrophic integrity loss, or equivalent severe impact.
- **HIGH** — practical unauthorized mutation/execution/data exposure, unsafe privileged CI, or serious untrusted-input/realtime integrity failure.
- **MEDIUM** — meaningful defense-in-depth, reliability, provenance, validation, or policy gap with limited current exploitability/reach.
- **LOW** — narrow hardening or hygiene issue with low direct impact.

No functional stage may proceed while a verified CRITICAL or HIGH security finding remains unresolved.

A PR must not merge if its reviewed head has an unresolved security blocker, failed required security test, or failed required CI check.

## 20. Security regression strategy

The Security Gate requires automated regression checks that are independent of future application dependencies.

The baseline regression layer must at minimum detect:

- tracked secret/credential file names;
- high-confidence credential/private-key patterns;
- unsafe GitHub Actions privilege/event patterns;
- unpinned external GitHub Actions;
- accidental committed build/output directories or selected binary/artifact types;
- violations of repository-specific security invariants that can be checked statically.

The regression implementation must include negative tests proving that unsafe samples are rejected and safe samples pass.

Stage 0-S.1 defines this contract. A subsequent bounded Security Gate implementation PR will add the regression tool/tests/workflow. Security Gate remains **OPEN** until that implementation passes shadow review and CI on the reviewed head and post-merge `main`.

## 21. Security Gate closure criteria

The Security Gate can close only when fresh evidence supports all applicable items:

```text
Threat boundaries defined                  PASS
Secret exposure review                     PASS
Dependency/supply-chain policy             PASS
GitHub Actions permission policy           PASS
Untrusted input policy                     PASS
External adapter policy                    PASS
AI mutation boundary                       PASS
Realtime safety boundary                   PASS
Plugin boundary                            PASS
Security regression implementation         PASS
Negative security tests                    PASS
Shadow review                              PASS
Required CI                                PASS
Critical unresolved finding                0
High unresolved finding                    0
```

Repository branch protection, richer platform/build security, parser fuzzing, sanitizer matrices, dependency SBOM/scanning, plugin process isolation, and runtime sandboxing may remain later-stage work when the corresponding runtime/build surfaces exist, but their absence must not be misrepresented as implemented protection.

## 22. Stage 0-S.1 non-goals

This contract does not:

- change repository visibility;
- enable/disable branch protection;
- add production C++ code;
- add CMake or compiler configuration;
- activate third-party dependencies;
- implement MusicXML/MIDI/audio/project parsers;
- implement plugin hosting;
- implement audio callback code;
- implement AI/provider integration;
- define Stage 0-C.2 identities;
- replace the dedicated Stage 0-D real-time/AI contract;
- publish or release software.
