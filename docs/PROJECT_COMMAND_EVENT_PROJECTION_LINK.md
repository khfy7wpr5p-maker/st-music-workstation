# ST Music Workstation — Add EventProjectionLink Command Boundary v0.1

Status: bounded Project Commands implementation candidate

## 1. Purpose

This package establishes the first explicit application-command path for adding an EventProjectionLink to authoritative Project state.

The architecture requires Presentation/UI and future adapters, including AI adapters, to communicate with Project state through explicit commands rather than treating parser, renderer, UI, SDK, network, or AI objects as mutation authority.

The command path is:

```text
external candidate / application intent
        ↓
AddEventProjectionLinkCommand
        ↓
command handler
        ↓
current Project snapshot check
        ↓
endpoint + Project-owned relation validation
        ↓
sealed preparation
        ↓
ProjectAggregate authoritative revalidation/publication
        ↓
authoritative Project state + next ProjectRevision
```

## 2. Scope

This slice adds:

- `st::application::AddEventProjectionLinkCommand`;
- a required expected `ProjectSnapshotToken` precondition;
- `execute_add_event_projection_link_command(...)` as the reviewed application entry path for this mutation;
- explicit command-level error classification while preserving lower-level preparation/publication reasons;
- fail-closed detection when Project state changes during external validation callbacks;
- negative and determinism tests for stale, cross-project, duplicate, disappearing-endpoint, and callback-mutation cases.

This package does not create a general command bus, queue, dispatcher, undo stack, persistence log, networking API, UI layer, AI adapter, or thread synchronization model.

## 3. Expected snapshot is mandatory

Every command carries the exact `ProjectSnapshotToken` the caller observed.

Before consulting external endpoint state, the handler compares the command snapshot to the ProjectAggregate current snapshot.

Rules:

1. A command for another `ProjectId` is rejected before endpoint-state reads.
2. A stale revision is rejected before endpoint-state reads.
3. Failure consumes no Project revision.
4. The command does not silently rebase itself onto newer Project state.
5. A caller that wants to retry must obtain a fresh snapshot, reconsider the intent, and submit a new command.

This makes optimistic concurrency explicit without choosing a mutex, actor, queue, or multi-thread ownership model.

## 4. External endpoint view is not relation authority

Until Score/MIDI/TAB/Audio endpoint collections receive their own reviewed Project-owned schemas, endpoint existence is supplied through the existing bounded read-only `EventProjectionValidationView`.

However, `contains_link(...)` from that external view is not consulted for authoritative relation ownership.

During command preparation, duplicate lookup is derived only from `ProjectAggregate::event_projection_relations()`.

Therefore an external adapter/view cannot veto or manufacture authoritative relation existence merely by claiming a link exists.

## 5. Preparation and publication are separate gates

The handler first prepares the candidate against:

- the exact expected/current Project revision;
- endpoint existence;
- Project-owned current relation state.

A successful preparation is still not authoritative state.

It is then passed to `ProjectAggregate`, which performs fresh publication revalidation and immutable next-state construction before committing relation state and global ProjectRevision together.

This preserves the existing rule:

```text
candidate
→ validate
→ prepare
→ revalidate current state
→ build complete next state
→ publish atomically inside ProjectAggregate
```

## 6. TOCTOU and callback mutation rule

External endpoint methods are untrusted read-only boundary calls even though their C++ interface is `noexcept`.

The command handler captures the Project snapshot before external endpoint work.

If an external callback mutates Project state before authoritative publication begins, the handler compares the aggregate snapshot again and returns `stale_project_snapshot` rather than continuing with the old command basis.

If state changes during ProjectAggregate publication/revalidation, the existing ProjectAggregate reentrancy and snapshot checks remain authoritative.

No stale command is silently rebased.

## 7. Failure model

Top-level command errors are:

```text
none
command_project_mismatch
stale_project_snapshot
validation_view_project_mismatch
preparation_failed
publication_failed
```

For `preparation_failed`, the exact `EventProjectionMutationPreparationError` is preserved.

For `publication_failed`, the exact Project publication, revalidation, and relation-transition error fields are preserved.

Failure must not:

- consume a Project revision;
- publish a partial relation;
- retarget a command to another Project;
- silently ignore a disappearing endpoint;
- accept a duplicate based on stale state;
- convert an external relation claim into authority.

## 8. Determinism

For the same Project state, command, endpoint view answers, and configuration, command execution must produce the same domain result.

No behavior in this package depends on:

- wall-clock time;
- thread scheduling;
- opaque ID ordering;
- filesystem state;
- network state;
- AI output confidence;
- random number generation.

The command is synchronous and control-thread oriented. It is not realtime-audio safe and must not execute in an audio callback.

## 9. Architecture boundary

The compile-time dependency direction introduced by this package is:

```text
st/application/event_projection_commands.hpp
        ↓
st/core/project_aggregate.hpp
```

Core does not include or depend on the application header.

The existing `ProjectAggregate` mutation member remains part of Core because Core tests and future Core composition still require an authoritative mutation primitive. This package defines the reviewed application path; it does not yet introduce separate binary/module visibility that can make every direct Core call impossible at compile time.

Direct UI, parser, renderer, plugin, network, or AI mutation of ProjectAggregate remains architecturally prohibited.

## 10. Security properties exercised by tests

The focused CTest must demonstrate at least:

- valid command succeeds and advances revision exactly once;
- stale expected snapshot rejects before external endpoint reads;
- wrong command Project rejects before external endpoint reads;
- wrong validation-view Project rejects before endpoint existence reads;
- wrong-project candidate fails preparation;
- external duplicate claim is ignored as relation authority;
- real Project-owned duplicate is rejected;
- endpoint disappearance between preparation and publication fails closed;
- an adversarial endpoint callback that mutates Project state makes the outer command stale rather than applying a second mutation;
- repeated identical isolated executions produce the same authoritative state shape.

## 11. Residual boundaries

This slice intentionally leaves the following work for later reviewed packages:

- Project-owned MusicalEvent/Score/MIDI/TAB/Audio endpoint entity collections;
- a general application command dispatcher or command journal;
- undo/redo integration;
- persistence and replay serialization;
- multi-thread ownership or synchronization;
- Track/Clip command schemas;
- relation removal and cascade/detach policy;
- Teacher Review acceptance commands;
- AI candidate adapters.

The repository's existing branch-protection governance residual also remains unchanged.

## 12. Acceptance criteria

This package is acceptable only when:

- the diff is bounded to the command header, focused test, this contract, and CMake test registration;
- strict C++20 build passes;
- all existing CTests remain green;
- the new command test passes;
- Security Baseline tests and scanner pass;
- current-main observer jobs verify the exact merge base;
- independent shadow review finds no unresolved material architecture/security issue;
- merge uses the exact reviewed head SHA.
