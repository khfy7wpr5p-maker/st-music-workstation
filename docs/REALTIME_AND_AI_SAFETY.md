# ST Music Workstation — Real-Time & AI Safety Contract v0.1

Status: Stage 0-D contract candidate; runtime implementation and measurement evidence remain deferred

## 1. Scope

Stage 0-D makes the real-time and AI boundaries reserved by `docs/ARCHITECTURE.md` and `docs/SECURITY.md` concrete enough to constrain later implementation.

This document is documentation-only. It does not implement an audio callback, DSP graph, device adapter, plugin host, AI provider, network client, persistence system, production C++ code, or build configuration.

Stage 0-D defines:

- execution-role and thread-authority boundaries;
- the information allowed to cross into and out of the real-time audio path;
- immutable snapshot and bounded-command handoff rules;
- real-time memory/lifetime and failure-isolation rules;
- callback overrun/telemetry semantics;
- plugin-processing prerequisites for a future host stage;
- AI input snapshot, data-egress, provenance, staleness, validation, acceptance, retry, and failure rules;
- negative-test and runtime-evidence requirements that later implementation must satisfy.

## 2. Governing invariants

The following remain binding:

1. Authoritative Project state is owned by ST domain/application logic, not by the audio callback, UI, adapters, plugins, AI, renderers, or network services.
2. Musical timing resolves through the Stage 0-C.1 Musical Time contract.
3. Domain identity resolves through the Stage 0-C.2 identity contract.
4. External data and AI output are untrusted until validated.
5. The real-time audio path must remain usable without network or AI services.
6. Security, correctness, bounded behavior, and failure isolation take precedence over convenience.

## 3. Execution roles

Later implementations may use more concrete threads/executors, but they must preserve these logical roles.

### 3.1 Authoritative control/application role

This role validates commands and owns publication of authoritative Project changes.

It may:

- validate user/application commands;
- update authoritative non-real-time Project state;
- prepare immutable real-time snapshots;
- allocate/free ordinary control-side memory;
- schedule bounded background work;
- accept/reject validated AI candidates through explicit commands.

It must not treat audio callback state as a second authoritative Project model.

### 3.2 Real-time audio role

The real-time audio role executes the device/render callback under a bounded callback budget.

It may:

- read already-published immutable real-time state;
- read/write buffers explicitly owned for the callback;
- perform bounded DSP/scheduling work proven suitable for the callback path;
- consume bounded real-time control messages under a reviewed protocol;
- publish bounded non-authoritative telemetry through an RT-safe mechanism.

It must not perform authoritative Project mutation.

### 3.3 UI role

The UI is a presentation/input role.

It may submit commands and read snapshots/telemetry, but it must not:

- mutate the audio graph directly;
- invoke arbitrary code on the callback thread;
- hold locks required by the callback;
- make UI object lifetime a dependency of callback safety.

### 3.4 Background worker/adapters

Filesystem, parser, network, AI, plugin discovery/load, sample preparation, waveform analysis, and other unbounded work belongs outside the real-time audio role.

A background worker result is boundary data until validated and explicitly published through the owning application/domain boundary.

## 4. Real-time callback hard prohibitions

Unless a later dedicated proof explicitly narrows a prohibition for a particular bounded primitive, the callback path must not perform:

- filesystem access;
- network I/O;
- database access;
- blocking IPC;
- subprocess creation or waiting;
- AI inference or provider calls;
- MusicXML/MIDI/project/preset parsing;
- plugin discovery, scan, load, unload, or state deserialization;
- UI framework operations;
- logging that may allocate, lock, format dynamically, flush, or perform I/O;
- mutex, recursive mutex, condition-variable, semaphore, future/promise wait, thread join, sleep, or other potentially blocking wait;
- memory allocation whose completion/cost is not proven bounded for the callback path;
- container growth or hidden allocation;
- uncontrolled destruction/deallocation whose cost may be unbounded;
- exceptions escaping the callback boundary;
- RTTI/dynamic operations whose implementation cost/allocation behavior has not been reviewed for the path;
- unbounded loops, recursion, retries, queues, graph traversal, or data-dependent work without a proven maximum;
- synchronous device reconfiguration;
- source decoding/decompression;
- arbitrary callbacks into UI/application/plugin-management code;
- operations whose worst-case execution behavior is unknown for the supported callback contract.

A fast average-case operation is not real-time safe merely because typical tests are fast.

## 5. Real-time state is not authoritative Project state

The callback must not traverse or mutate the live authoritative Project object graph.

The preferred conceptual flow is:

```text
Authoritative Project
        ↓
validated application/control transformation
        ↓
Immutable RealtimeSnapshot
        ↓
atomic/bounded publication boundary
        ↓
Real-time callback
```

`RealtimeSnapshot` is a conceptual role, not a final C++ type.

It must contain only the state required by the callback and must not carry hidden access to mutable Project/UI/adapter objects.

A real-time snapshot may contain derived playback values and references to RT-owned/prepared resources, but its contents must be internally consistent before publication.

## 6. Snapshot publication contract

A later implementation must provide a publication mechanism with these properties:

1. Snapshot construction and structural validation occur outside the callback.
2. Publication is atomic from the callback's perspective: it observes the previous complete snapshot or the new complete snapshot, never a partially constructed mixture.
3. The callback never waits for the producer to finish construction.
4. Publication does not require the callback to acquire a contended blocking lock.
5. Snapshot acquisition has a bounded worst-case operation count under the supported implementation contract.
6. A failed candidate snapshot is not published.
7. Snapshot replacement must not trigger arbitrary destruction/freeing of the retired snapshot on the callback thread.
8. Reclamation of retired state occurs outside the callback through a reviewed lifetime protocol.
9. A snapshot may carry an opaque generation/revision token for consistency diagnostics, but that token is not Musical Time, musical ordering, or Project identity.
10. When no newer valid snapshot is available, the callback may continue with the last-known-good published snapshot if doing so is safe for the current device state; it must not invent missing Project state.

The implementation may use lock-free/wait-free techniques, double/triple buffering, immutable reference publication, epochs, or another reviewed bounded mechanism, but Stage 0-D intentionally does not select a library or primitive.

## 7. Real-time/control messages

Low-latency control that cannot wait for a full structural snapshot may use a separate bounded RT-safe message channel.

Examples may later include transport state, seek intents, automation/control values, or other explicitly defined realtime commands.

Required properties:

- fixed or otherwise proven bounded capacity;
- bounded message representation;
- no pointers to mutable UI/application-owned objects;
- deterministic message validation before enqueue where semantic validation is needed;
- explicit producer/consumer ownership;
- explicit ordering semantics;
- no blocking producer dependency on the callback;
- no silent overwrite of authoritative control commands when the queue is full;
- queue-full behavior must be explicit: reject/coalesce/defer only under a command-specific reviewed policy;
- coalescing must not change semantics for commands where every transition matters;
- sequence/generation metadata, if used, is ordering metadata only and must not become Musical Time.

## 8. Telemetry channel

Callback-to-UI/diagnostic telemetry is non-authoritative.

Examples may include meters, xrun counters, peak levels, or bounded diagnostics.

A dedicated telemetry channel may intentionally drop/coalesce stale telemetry if:

- the loss policy is documented;
- no authoritative Project state depends on delivery;
- no transport/domain command acknowledgement is inferred from telemetry delivery;
- the callback never blocks because a consumer is slow.

Telemetry loss must never cause hidden Project mutation or a second state authority.

## 9. Memory and lifetime ownership

Real-time data ownership must be explicit.

Rules:

- audio buffers used in the callback are allocated/prepared before callback use according to the later Audio contract;
- callback-visible objects must remain alive for the entire interval in which the callback can legally access them;
- cancellation/shutdown/device changes must not invalidate callback-visible memory early;
- mutable cross-thread sharing requires a reviewed RT-safe protocol; ordinary unsynchronized mutation is prohibited;
- callback-visible snapshots should be immutable after publication;
- reference-count decrement/destructor behavior that can free complex objects must not cause uncontrolled deallocation on the callback path;
- object reclamation is transferred to a non-real-time role unless a concrete object has a proven trivial/bounded RT-safe lifetime operation;
- ownership must remain correct on failure and shutdown paths, not only steady-state playback.

## 10. Audio-buffer boundary

A future callback contract must validate or establish before processing:

- channel count and buffer layout expected by the engine;
- frame count bounded by supported device/buffer configuration;
- valid non-null buffer regions where the device API contract requires them;
- no integer overflow when computing frame/channel offsets;
- no out-of-bounds read/write;
- explicit handling for zero-frame callbacks if the chosen device API can produce them;
- deterministic handling of unsupported channel/layout configurations outside or at a bounded adapter boundary.

Unexpected adapter/device metadata must not cause the callback to allocate, parse, or rebuild the graph synchronously.

## 11. Callback failure behavior

The callback is an availability boundary.

Rules:

1. Exceptions must not escape across the device callback ABI boundary.
2. The callback must not attempt unbounded recovery.
3. A detected unusable/invalid RT snapshot must not be partially applied.
4. A bounded fail-safe output policy must be defined by the later Audio implementation for unrecoverable per-block conditions; silence is permitted as a fail-safe only when that policy explicitly requires it and must not be confused with successful processing.
5. Error details that require formatting/I/O are reported outside the callback through bounded flags/counters/telemetry.
6. Repeated callback failure must transition through a control-side error/device state rather than spinning indefinitely inside the callback.
7. Failure of UI, AI, network, storage, or background parsing must not crash the callback merely because those subsystems are unavailable.

## 12. Callback budget and overload

Later Audio implementation must define a supported callback budget based on device sample rate and frames per callback.

The RT path must have bounded work for that contract.

Required evidence before an Audio runtime stage can be complete includes:

- representative and stress callback-duration measurements;
- queue-full and maximum-capacity tests;
- graph/resource maximums or explicit supported limits;
- allocation/blocking instrumentation or equivalent proof for the callback path;
- overload behavior showing controlled xrun/error reporting rather than unbounded recovery;
- tests at the smallest supported callback size and relevant sample rates;
- shutdown/device-change stress tests.

CI timing alone is not proof of hard real-time guarantees because hosted runner scheduling is not deterministic. Timing evidence must be interpreted as regression/engineering evidence, not formal hard-real-time certification.

## 13. Real-time determinism

For deterministic DSP/scheduling components, identical validated inputs, identical published state, identical sample rate/buffer contract, and identical ordered realtime commands should produce the same defined engine result subject to explicitly documented floating-point/DSP platform constraints.

The following must not define semantics:

- thread race timing;
- unordered container iteration;
- pointer addresses;
- wall-clock time;
- random ID ordering;
- UI refresh timing;
- AI/network response timing.

Musical event timing continues to derive from authoritative Musical Time; callback block boundaries are playback execution boundaries, not a second musical clock.

## 14. Device changes and shutdown

Device open/close/reconfiguration is control/adapter work, not arbitrary callback work.

A future implementation must define a state machine for at least:

- stopped/no device;
- preparing;
- running;
- device loss/error;
- stopping/draining as appropriate.

Rules:

- device reconfiguration prepares new resources off the active callback path;
- callback-visible state transitions are atomic/bounded;
- shutdown waits/reclamation occur outside the callback;
- old callback state is not destroyed until callbacks that could observe it are no longer able to run;
- device loss must not mutate authoritative Musical Time or Project content.

Exact device APIs belong to the later Audio/adapter stages.

## 15. Plugin real-time prerequisite

Third-party plugins remain untrusted native code under `docs/SECURITY.md`.

Stage 0-D does not authorize plugin hosting.

Before plugin processing may execute on the real-time path, the dedicated Plugin Host contract/implementation must define at least:

- discovery/load/state work outside callback;
- plugin processing ABI ownership and exception boundary;
- buffer/range validation;
- timeout/hang strategy limitations;
- CPU/overrun observation;
- plugin state publication/lifetime;
- crash/isolation expectations;
- behavior when a plugin violates the callback budget;
- no SDK type leakage into ST Core/domain.

An in-process third-party plugin cannot be assumed real-time safe merely because its API offers a process callback.

## 16. AI execution boundary

AI is an optional non-real-time adapter capability.

Required flow:

```text
validated Project/Input Snapshot
        ↓
explicit AI request
        ↓
AI Adapter / Provider
        ↓
untrusted bounded response
        ↓
Candidate / Proposal
        ↓
schema + domain validation
        ↓
staleness/current-state validation
        ↓
deterministic policy
        ↓
explicit user/teacher review where required
        ↓
explicit Project command
        ↓
controlled authoritative mutation
```

AI must never execute on the real-time callback thread and must never be required for deterministic core playback/project behavior.

## 17. AI request snapshot

An AI request must operate on an immutable/bounded snapshot of the data authorized for that request.

Rules:

- the AI adapter must not receive a live mutable Project object;
- data selection occurs before network/provider submission;
- only data required for the declared capability should be included;
- identifiers/timing values supplied to AI remain data, not mutation capabilities;
- the snapshot must have a consistency token/revision reference sufficient for later stale-result checking under the future Project revision contract;
- the token is not itself permission to mutate the current Project;
- changes to the live Project after request dispatch do not retroactively alter the request snapshot.

## 18. AI data egress and consent

Network/provider AI use must be explicit and reviewable.

Before an adapter is activated for a capability, its contract/configuration must identify, as applicable:

- capability/purpose;
- provider/endpoint ownership;
- categories of Project/user data that may leave the local application;
- whether audio, notation, metadata, filenames, teacher annotations, or other user content is transferred;
- authentication/secret handling;
- known provider retention/training controls available to the integration;
- timeout and retry policy;
- response-size limits;
- paid/billable-call behavior.

Rules:

- enabling AI must not silently enable unrelated data egress;
- paid/billable external use must not start without explicit product/user configuration appropriate to that service;
- credentials must not be stored in Project content or AI prompts;
- logs/telemetry must not expose prompts, user content, or secrets by default;
- a capability requiring user/teacher consent must fail closed when that consent/configuration is absent;
- provider policy text is not trusted as runtime data validation; adapter-side controls remain required.

Stage 0-D does not choose a provider or define legal/privacy terms for one.

## 19. AI response boundary

AI/provider responses are untrusted input.

Before candidate construction, the adapter must enforce:

- maximum response size;
- expected encoding;
- explicit schema/type validation;
- bounded list/string/blob sizes;
- finite/range-valid numeric values;
- rejection of unknown semantic values when ignoring them could change meaning;
- no path/URL/subprocess execution merely because response text requests it;
- no dynamic code/object deserialization;
- no treating provider IDs as authoritative ST IDs;
- no direct filesystem/plugin/network actions initiated from model text.

Natural-language instructions contained inside source material or model output have no authority over application/tool security policy.

## 20. AI candidate provenance

Every candidate that can reach review/acceptance must carry sufficient non-authoritative provenance to explain its origin.

Conceptually provenance should include, where available/applicable:

- ST adapter/capability identity;
- provider identity;
- model identifier/version/reference reported or configured for the request;
- request/candidate correlation identifier owned by the adapter/application;
- input snapshot/revision reference;
- relevant deterministic configuration/version;
- creation/request metadata needed for audit/debugging.

Provenance is metadata, not Project identity, Musical Time, or authorization.

Missing optional provider metadata may make a candidate less auditable; required provenance fields for a concrete capability must be fixed before that capability is enabled.

## 21. AI staleness and current-state validation

An AI result may arrive after the Project has changed.

Therefore acceptance must compare the candidate's input snapshot/revision reference against current authoritative state according to the future Project revision/command contract.

Rules:

- stale candidates must not be applied blindly;
- a capability may reject a stale candidate outright;
- deterministic revalidation/rebase is allowed only if the capability defines an explicit semantic policy and proves that the intended target entities/relationships still exist and retain the required meaning;
- missing/deleted/replaced identity targets cause rejection unless a reviewed recovery workflow explicitly presents a new candidate;
- AI confidence does not override staleness checks;
- the model/provider must not decide whether its own stale mutation is safe to apply.

## 22. AI acceptance boundary

AI candidate acceptance is an ordinary controlled domain mutation, not a privileged mutation path.

Before authoritative mutation:

1. candidate schema is valid;
2. all referenced ST identities are validated under Stage 0-C.2;
3. Musical Time values are validated under Stage 0-C.1;
4. candidate is still applicable to current Project state;
5. deterministic domain/security policy passes;
6. required user/teacher review is explicit;
7. a normal application/Project command is constructed;
8. the command validates the complete resulting authoritative state before publication.

AI adapters cannot bypass ordinary validation because a provider is trusted or a confidence score is high.

## 23. AI retries, cancellation, and idempotency

Provider/network work must have bounded lifecycle behavior.

A concrete adapter must define:

- connect/request timeout;
- total operation timeout or cancellation policy;
- maximum retry count/backoff bounds;
- maximum request/response sizes;
- cancellation cleanup;
- provider error mapping;
- duplicate-response handling.

Rules:

- retries must not create duplicate authoritative Project mutations;
- timeout/cancel/provider failure leaves authoritative Project state unchanged;
- cancellation must not publish a partially parsed/validated candidate;
- a late response after cancellation is discarded or quarantined according to an explicit policy;
- retries and provider timing must not define Musical Time or event ordering.

## 24. AI failure and offline behavior

The deterministic core must remain usable when:

- network is unavailable;
- credentials are absent/expired;
- provider rejects the request;
- model is unavailable;
- response is malformed;
- request times out;
- user cancels;
- candidate fails validation;
- candidate becomes stale.

Failure results in an explicit unavailable/rejected/error state for the AI capability. It must not silently substitute a different provider/model or mutate the Project using guessed fallback content unless a separately configured reviewed policy explicitly authorizes that behavior as a new candidate flow.

## 25. Agentic/tool-capable AI

Stage 0-D does not authorize an AI model to call arbitrary filesystem, shell, network, plugin, Git, or Project mutation tools.

If a future capability becomes tool-using/agentic:

- every tool is an explicit adapter capability;
- tool input is validated independently of model text;
- permissions are least-privilege and capability-scoped;
- destructive/financial/release/credential-sensitive operations retain their ordinary approval/security gates;
- tool results return as untrusted data to the model;
- model instructions cannot broaden its permissions;
- authoritative mutation still occurs through validated Project commands.

A future agentic capability requires its own reviewed threat model before activation.

## 26. AI and real-time interaction

AI may prepare proposals or non-real-time derived resources, but it must not sit synchronously in the audio rendering dependency chain.

Prohibited patterns include:

```text
Audio callback → network → model → audio output
Audio callback → AI lock/future → wait
Audio callback → mutable AI-owned Project state
```

A future offline-render or precomputation feature may use AI outside the real-time callback only through explicit bounded/adapted workflows; generated resources remain untrusted until validated before RT publication.

## 27. Verification requirements for later real-time implementation

Before any future real-time implementation stage is marked complete, relevant tests/evidence must include:

- callback-path static/code review against the prohibition list;
- allocation/deallocation detection or equivalent instrumentation under representative callback execution;
- blocking-lock/wait detection or architecture proof plus targeted tests;
- bounded queue full/empty/wraparound tests;
- snapshot publication/reclamation race tests;
- repeated start/stop/device-loss/shutdown tests;
- maximum supported buffer/channel/resource boundary tests;
- invalid snapshot/message rejection tests;
- callback exception containment tests where language/ABI paths can throw;
- stress/overload/xrun telemetry tests;
- thread/race sanitization on non-production test paths where toolchain support permits;
- use-after-free/lifetime regression tests;
- candidate/current-main security CI.

Hosted CI duration measurements alone do not satisfy the real-time runtime-evidence requirement.

## 28. Verification requirements for later AI implementation

Before a concrete AI capability is marked complete, relevant tests/evidence must include:

- AI-disabled/no-network core behavior;
- malformed/truncated/oversized response rejection;
- unknown/unsupported semantic value rejection;
- NaN/Infinity/range rejection where numeric output exists;
- stale Project snapshot/result rejection;
- missing/deleted/wrong Project identity target rejection;
- timeout/cancellation/provider-error tests;
- retry/duplicate-response idempotency tests;
- prompt/source text attempting to request tool/filesystem/network policy bypass;
- provenance presence/validation for fields required by that capability;
- no direct mutation before explicit acceptance command;
- consent/configuration/data-egress negative tests;
- secret/log redaction tests where credentials/provider requests exist;
- candidate/current-main security CI.

## 29. Shadow-review checklist

Every PR touching later real-time or AI paths must independently ask at minimum:

- Can this code block, allocate, free, parse, log, call UI/network/filesystem, or perform unbounded work on the callback path?
- Can a callback-visible object die while still referenced?
- Can queue overflow silently lose an authoritative command?
- Can telemetry be mistaken for authoritative acknowledgement?
- Can a plugin/provider/model directly reach Project mutation?
- Can stale AI output target changed/deleted entities?
- Can model/source text expand permissions or trigger tools?
- Can network/provider failure change deterministic core behavior?
- Can any external value become Musical Time/identity without Stage 0-C validation?
- Does failure leave the prior authoritative state intact?

A material unanswered question is a merge blocker for the affected implementation PR.

## 30. Stage 0-D non-goals

Stage 0-D does not:

- implement the real-time audio callback;
- select an audio/device library;
- implement audio graph/DSP nodes;
- set final supported device/sample-rate/buffer matrices;
- authorize plugin hosting;
- select an AI provider/model;
- define provider legal/privacy terms;
- implement Project revision/version types;
- implement concrete RealtimeSnapshot/queue primitives;
- implement AI network clients;
- define concrete Project command schemas;
- define release/deployment behavior;
- change Stage 0-C Musical Time or identity ownership.

## 31. Stage 0-D acceptance criteria

The Stage 0-D contract baseline is acceptable when:

- execution roles and authoritative mutation ownership are explicit;
- callback hard prohibitions are explicit;
- live Project/UI/AI/adapter state cannot be traversed as callback authority;
- immutable snapshot publication and non-RT reclamation requirements are explicit;
- realtime command channels are bounded with explicit overflow/order policy;
- telemetry is explicitly non-authoritative and loss-tolerant only under declared policy;
- callback memory/lifetime/failure/device-change expectations are defined;
- later real-time implementation evidence requirements are concrete;
- plugin processing remains unauthorized until its dedicated host safety stage;
- AI operates only on bounded snapshots outside the callback;
- AI data egress/consent/provider configuration is explicit before activation;
- AI response is untrusted and bounded;
- provenance and stale-result validation are required;
- accepted AI output uses ordinary validated Project commands;
- timeout/retry/cancel/offline failure leaves authoritative state safe;
- agentic/tool-capable AI is not implicitly authorized;
- later AI implementation negative tests are explicit;
- no production implementation/dependency/provider selection is prematurely introduced.
