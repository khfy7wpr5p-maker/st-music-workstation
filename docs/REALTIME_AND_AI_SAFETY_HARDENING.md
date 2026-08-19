# Stage 0-D — Normative Real-Time & AI Safety Hardening

Status: normative companion to `docs/REALTIME_AND_AI_SAFETY.md` for the Stage 0-D contract baseline

This file records shadow-review hardening that is binding together with the primary Stage 0-D contract. If a statement here is stricter than an earlier general statement in `docs/REALTIME_AND_AI_SAFETY.md`, this hardening rule governs.

## 1. Real-time reclamation is fail-closed

A publication mechanism is not real-time safe merely because pointer acquisition is atomic or lock-free.

The callback must not perform an ownership operation whose last-reference transition can synchronously invoke arbitrary destructor logic, allocator/free logic, file/network/plugin cleanup, or other unbounded reclamation.

Therefore:

- dropping a reference-counted handle on the callback is prohibited when that decrement can become the final owner and reclaim non-trivial state;
- snapshot handoff must guarantee that retirement/reclamation responsibility is transferred to a non-real-time role;
- the callback must not determine safety by hoping another owner still exists;
- ownership counts, epochs, hazard-style mechanisms, fixed snapshot slots, or other implementation techniques are acceptable only after their callback-side operation and reclamation paths are reviewed as bounded;
- shutdown/device changes must preserve the same rule;
- a trivial fixed-size object may be destroyed on the callback only when the implementation proves that destruction is allocation-free, non-blocking, bounded, and contains no hidden external cleanup.

A later implementation test must exercise repeated snapshot replacement under maximum supported publication pressure and verify that non-trivial retirement/destruction occurs outside the callback role.

## 2. External AI data egress defaults to deny

Sending user/project content to an external AI/network provider is disabled by default for each provider/capability until explicit configuration authorizes that data flow.

For every external capability, activation must identify the categories of content allowed to leave the local application. Permission for one provider/capability does not authorize another provider/capability.

At minimum:

- no Project/audio/Score/TAB/MIDI/teacher/user content is uploaded merely because an AI feature exists in the build;
- no background prefetch, embedding, model warm-up, or speculative request may transmit user content before the capability is enabled for that data category;
- local-only inference that performs no external data transfer is a distinct capability and must not silently fall back to a network provider;
- provider/model substitution that would change data egress requires the same reviewed activation/consent boundary as a newly enabled external provider;
- filenames, file paths, metadata, teacher annotations, and provenance can themselves be user data and must not be added to an external request unless required and allowed for that capability;
- revoking/disabling the external capability prevents new submissions; already-sent provider-side data is governed by the concrete provider policy and must not be represented as locally revoked/deleted unless independently confirmed;
- absence, ambiguity, or failure to load the required capability/consent configuration causes the external request to fail closed.

Stage 0-D does not define jurisdiction-specific legal consent language. It defines the technical default-deny data-egress boundary that every external AI adapter must enforce.

## 3. AI semantic mutation requires an explicit acceptance policy

A validated AI candidate still has no mutation authority.

For any AI candidate that would change authoritative musical/project content, the concrete capability contract must specify one of these reviewed acceptance modes before activation:

1. explicit user/teacher acceptance; or
2. a narrowly scoped automatic acceptance policy whose eligible mutation class, validation rules, rollback behavior, audit/provenance, and risk justification are defined and tested.

There is no implicit automatic-accept mode based solely on model confidence, provider identity, or successful schema validation.

Destructive, irreversible, credential-sensitive, financial, release/publish, or permission-changing actions are never made automatic by this Stage 0-D contract.

## 4. Negative-test additions

Later implementation verification must include:

- a snapshot/reference release case that would have been the last owner, proving non-trivial reclamation is deferred off the callback;
- external AI capability disabled → zero user-content submission;
- missing/ambiguous egress configuration → request rejected before content transmission;
- local-only capability provider failure → no silent network fallback;
- provider substitution → no data transmission until separately authorized;
- AI semantic candidate with no configured acceptance mode → no Project mutation;
- high-confidence candidate → still no bypass of acceptance/staleness/domain validation.

These are additive requirements to the primary Stage 0-D verification matrices.