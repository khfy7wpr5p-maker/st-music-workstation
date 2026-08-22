# Project Commands — EventProjection Relation Removal

Status: bounded application-command implementation slice

## Purpose

This contract defines the application-layer command boundary for removing one exact authoritative `EventProjectionLink` from a Project.

The command does not own Project state. It validates command scope and delegates authoritative publication to the Project-owned removal gate in `ProjectAggregate`.

## Command shape

`RemoveEventProjectionLinkCommand` carries:

- an exact expected `ProjectSnapshotToken`;
- one exact typed `EventProjectionLinkCandidate` used only as the removal key.

The removal key is identity-based. No pitch, time, visual position, source metadata, renderer output, AI result, or fuzzy matching may select a relation for removal.

## Execution order

`execute_remove_event_projection_link_command()` must:

1. read the current Project snapshot;
2. reject an expected snapshot belonging to another Project;
3. reject a stale expected snapshot;
4. reject an event identity outside the current Project;
5. reject a projection identity outside the current Project;
6. delegate authoritative removal to `ProjectAggregate` using the exact expected snapshot and exact removal key;
7. preserve Project publication and immutable removal-transition errors when publication fails;
8. return the newly published Project snapshot only on success.

The command does not inspect endpoint existence. Relation removal is allowed after an endpoint has disappeared because detaching an exact stored relation must not depend on the continued presence of either endpoint.

## Failure semantics

Top-level command failures distinguish:

- command Project mismatch;
- stale Project snapshot;
- event Project mismatch;
- projection Project mismatch;
- authoritative publication failure.

A publication failure also preserves:

- `ProjectEventProjectionRemovalPublicationError`;
- `EventProjectionRelationStateRemovalError`.

Missing relations, revision overflow, allocation failure, reentrancy, and invariant failures therefore remain explicit rather than being collapsed into success or a generic no-op.

## Authority and concurrency boundary

The command is not an authority source and does not mutate relation storage directly. `ProjectAggregate` remains the only publication owner for this slice.

The current aggregate remains single-owner/control-thread only. This command does not establish multi-thread synchronization, locking, actor semantics, queue semantics, persistence transactions, or realtime-safe mutation.

## Realtime boundary

The command must not execute from the realtime audio callback. Authoritative removal may allocate while constructing the next immutable relation state.

## Non-goals

This slice does not define:

- endpoint entity deletion or cascade behavior;
- automatic orphan cleanup;
- Track, Clip, MusicalEvent, Score, MIDI, TAB, or Audio entity mutation commands;
- undo/redo, persistence, journaling, replay, migration, or synchronization;
- UI routing, network APIs, plugins, renderers, AI providers, devices, release, or deployment.

Any endpoint lifecycle or cascade policy requires a separate reviewed domain decision. This command only removes one exact relation requested against one exact Project snapshot.
