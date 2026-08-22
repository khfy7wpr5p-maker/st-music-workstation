# ST Music Workstation — EventProjection Relation Removal Transition v0.1

Status: bounded Project Core implementation candidate

## 1. Purpose

This package defines the first immutable Core transition for removing one exact authoritative EventProjection relation from a relation-state value.

`docs/DOMAIN_IDENTITIES.md` requires EventProjection link creation and removal to occur through validated domain/application operations. Addition already has an immutable transition and authoritative Project publication path. This package supplies only the removal-state transition primitive required before a separately reviewed authoritative removal publication and application command can exist.

## 2. Input and output

The transition consumes:

- current `EventProjectionRelationStateCandidate`;
- current authoritative `ProjectSnapshotToken` supplied by the future Project owner/publication boundary;
- an exact typed `EventProjectionLinkCandidate` used only as the removal key.

On success it returns:

- a new immutable relation-state candidate containing every prior relation except the exact requested relation;
- a separate Project snapshot with the same `ProjectId` and exactly `current_revision + 1`.

The input relation state is never mutated.

## 3. Exact typed removal key

Removal is by semantic relation identity already represented by the existing typed pair:

```text
MusicalEventId + typed projection ID
```

The key does not use:

- collection index;
- vector position;
- renderer handle;
- external source ID;
- pitch/time coincidence;
- raw untyped identifier bytes.

Both the event and projection identities must belong to the current Project.

## 4. Endpoint independence

Removing a relation does not require a Score/MIDI/TAB/Audio endpoint validation view.

This is deliberate. A stale or damaged higher-level state may need to detach a relation after one endpoint has already disappeared. Requiring endpoint existence would make cleanup of that invalid/dangling relationship impossible.

This package removes only the relation. It does not delete, restore, mutate, or authorize any endpoint entity.

## 5. Deterministic failure precedence

Validation order is fixed:

```text
current relation-state Project matches Project snapshot
        ↓
removal key is scoped to that Project
        ↓
exact relation exists
        ↓
Project revision can advance without overflow
        ↓
build complete next relation-state value
```

The corresponding errors are:

```text
current_state_project_mismatch
link_wrong_project
link_not_found
revision_overflow
allocation_failure
```

Therefore `link_not_found` precedes `revision_overflow` when both conditions are true. This prevents implementation/platform variation from changing the domain result.

## 6. Revision rule

Relation state does not carry an independent revision clock.

The current global `ProjectRevision` comes from the supplied `ProjectSnapshotToken`. A successful transition returns exactly the next Project revision separately from the relation-state value.

Failure returns no next snapshot and consumes no revision.

The transition itself is not authoritative publication. A later Project-owned boundary must revalidate the current authoritative Project snapshot and publish relation state + global revision together.

## 7. Immutability and allocation failure

The transition constructs a new vector containing all existing links except the single exact target.

Rules:

- the input state is unchanged on success;
- the input state is unchanged on every failure;
- relation limits and Project scope are preserved;
- relation ordering is preserved for all surviving links;
- vector order remains storage order, not musical semantics;
- `std::bad_alloc` is mapped to explicit `allocation_failure`;
- allocation failure returns no partial next state and no next Project snapshot.

A deterministic allocator-fault injection seam is not introduced by this package. The existing hard cap and fail-closed exception mapping remain the current resource-safety controls.

## 8. No cascade semantics

This package intentionally defines no cascading deletion behavior.

It does not decide what happens when a future command deletes:

- a MusicalEvent;
- a Score entity;
- a MIDI entity;
- a TAB entity;
- an Audio entity.

Cascade/detach policy belongs to the later owning entity and command contracts. This primitive only removes one explicitly named relation.

## 9. Security and authority boundary

An `EventProjectionLinkCandidate` passed as a removal key is not authority.

The transition verifies Project scope and exact current relation existence. Its output remains a non-authoritative candidate until a Project-owned publication boundary accepts it.

AI, UI, parser, renderer, plugin, network, or external SDK objects must not call this primitive and treat its output as authoritative Project state. Future external/application callers must use a separately reviewed command/publication path.

## 10. Realtime boundary

This operation copies a vector and may allocate. It is therefore control-thread/non-realtime work.

It must not execute inside the realtime audio callback.

## 11. Determinism

For the same current relation state, current Project snapshot, removal key, and configuration, the transition returns the same domain result.

It does not depend on:

- wall clock;
- thread scheduling;
- filesystem;
- network;
- AI;
- opaque ID ordering;
- random generation.

## 12. Test requirements

The focused CTest must demonstrate at least:

- removing an existing relation succeeds;
- exactly one requested relation disappears and unrelated relations remain;
- the prior relation state remains unchanged;
- global Project revision advances exactly once on success;
- Project scope and relation limits are preserved;
- missing relation fails with `link_not_found` and no revision;
- cross-Project key fails with `link_wrong_project`;
- relation-state/snapshot Project mismatch has deterministic first precedence;
- revision overflow fails closed;
- missing-link precedence is stable even at max revision;
- repeated identical transitions produce the same next state and revision.

## 13. Non-goals

This package does not add:

- authoritative Project removal publication;
- `RemoveEventProjectionLinkCommand`;
- endpoint entity collections;
- endpoint deletion or cascade policy;
- undo/redo;
- persistence or replay serialization;
- synchronization/actor/mutex ownership;
- realtime behavior;
- UI;
- network;
- plugin behavior;
- AI integration.

## 14. Acceptance criteria

The package is acceptable only when:

- the diff remains bounded to the relation-state primitive, focused test, CMake registration, and this contract;
- strict C++20 build passes;
- all existing CTests remain green and the new removal CTest passes;
- Security Baseline tests and repository scanner pass;
- current-main Build/Security observer jobs verify the exact merge base;
- independent shadow review finds no unresolved material correctness/security issue;
- branch is mergeable and zero behind;
- merge uses the exact reviewed head SHA.
