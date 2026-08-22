# Project Core — Authoritative EventProjection Relation Removal

Status: bounded Project Core implementation slice

## Purpose

This contract defines the first Project-owned authoritative publication boundary for removing one exact `EventProjectionLink` from in-memory Project state.

It builds on the immutable removal transition introduced previously. The immutable transition remains non-authoritative by itself; only `ProjectAggregate` may publish the resulting relation state and Project revision as current authoritative state.

## Inputs

Authoritative removal requires:

- the current owning `ProjectAggregate`;
- an exact `ProjectSnapshotToken` supplied as the caller's expected base snapshot;
- an exact typed `EventProjectionLinkCandidate` used only as the removal key.

The removal key is matched by exact `MusicalEventId` plus exact typed projection identity. Pitch, time, source identity, renderer identity, AI output, display position, or other inferred similarity must not substitute for exact identity.

## Publication rules

`ProjectAggregate::publish_event_projection_link_removal()` must:

1. reject reentrant Project publication before any state transition;
2. reject an expected snapshot from another Project;
3. reject a stale expected snapshot before attempting the removal transition;
4. build the next immutable relation state from the current authoritative relation state and current global Project snapshot;
5. preserve exact transition failures including wrong-Project key, missing link, revision overflow, and allocation failure;
6. publish relation state and the exact next Project snapshot together only after all checks succeed;
7. consume exactly one global Project revision on success;
8. consume no revision and mutate no authoritative state on every failure.

Removal does not require the event or projection endpoint to still exist. This is intentional so a dangling relation can be detached safely after an endpoint has already disappeared. Endpoint deletion/cascade policy remains a separate later contract.

## Reentrancy and ownership

The existing `ProjectAggregate` publication guard is shared by addition and removal publication. A removal attempt made while another Project publication is already in progress fails closed with `reentrant_publication`.

`ProjectAggregate` remains single-owner/control-thread only. This slice does not add mutexes, actors, queues, lock-free publication, or realtime-safe mutation.

## Failure semantics

Top-level removal publication failures distinguish:

- reentrant publication;
- expected-snapshot Project mismatch;
- stale expected snapshot;
- immutable relation-state transition failure;
- internal invariant failure.

When the immutable transition fails, its exact `EventProjectionRelationStateRemovalError` is preserved. Failure never silently converts into a successful no-op.

## Realtime boundary

Authoritative relation removal is not permitted on the realtime audio callback. It may allocate while constructing the next immutable relation vector and therefore belongs to the Project/control side of the architecture.

## Non-goals

This slice does not add:

- an application-level removal command;
- endpoint deletion, cascade, orphan cleanup, or cardinality policy;
- Track/Clip/entity collection mutation;
- persistence, journal, undo/redo, migration, or replay storage;
- synchronization or multi-thread publication semantics;
- UI, network, plugin, renderer, AI, or device behavior;
- realtime mutation or deployment.

The next safe application-layer slice may wrap this publication boundary in an exact expected-snapshot removal command without weakening Project ownership or failure semantics.
