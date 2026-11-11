# Eight-ring server/client coverage experiment

The renderer's room expansion did not extend server simulation or client unit
subscriptions. The owner capture repeatedly showed server-resident monsters
and shrine objects disappearing from the client at a room boundary.

This experiment extends server-room activation and the local client's room
subscriptions to graph depth eight within the current level. The native
immediate neighbors, including any across a level boundary, are preserved.
Traversal is bounded at512 rooms; an ordinary square outdoor layout can reach
289 rooms. Exact reachable depth/count depends on the generated level graph.

The experimental DLL defaults `[foliage] subscription_rings` to8. Set it to0
for the native subscription set. It is independent of `[render] rings` and
model visibility radius. A queued authoritative game-thread callback initializes
coverage around the current room as soon as the offline local player/client is
available, including while stationary. Toggles also apply without walking.
Initial expansion can pause while the server activates many rooms. Farther
enemies can simulate as well as become available to the client. The owner
reported distant enemies/shrines with the first build, but also invisible
attackers. Larger renderer/entity pools activated correctly without eliminating
that symptom. On November 11, 2026, the owner confirmed that the build including
the expanded ongoing unit-update/removal path below works in-game. This is
owner-reported validation of that scenario, not an extended stability result.

## Native integration

Only exact95032 code guards and the offline local server/client associations
enable expansion. All hooks must install before the experiment changes a set.
Missing/deferred dependencies are retried by the existing slow settings poll.
The native room-change routine remains responsible for room packets, unit
create/remove delivery, monster activation and center/level bookkeeping.
Stationary refresh uses its same native per-room subscribe/unsubscribe helpers,
with two additional exact-build guards, without moving the native centre. Both
room sets remain retained until removals finish. Membership is checked before
each operation to avoid duplicate adds or removal of an absent client. The
loader callback resolves fresh identities on the game thread and uses expiring
tickets so discarded session callbacks cannot block future initialization.

During that routine, only its two specific calls to the neighbor getter see
owned expanded old/new arrays. The local client's native dirty-unit update and
unit-removal passes also borrow the retained expanded list at their two guarded
neighbor-getter call sites. Previously these ongoing passes still visited only
native neighbors, so subscription coverage alone did not extend their create,
movement/animation, or removal delivery. They retain native unit filtering,
packet generation, update cadence, and deletion queues. No periodic forced
respawn, duplicate room subscription, or client-side fake enemy is introduced.
Other consumers see their native arrays. The
game's ROOM and DRLG neighbor arrays are never rewritten. Each expanded room
retains native room-status and level references; old references are released
after native subscription removal. Disconnect removes the owned memberships
before releasing references through the native symmetric lifecycle. Invalid
graph/loading results roll back new references and attempt a native-set
contraction. A room graph in another level is not recursively expanded.

## Verification and limitations

The fixture tests exercise eight-ring coverage, repeated boundary crossings,
retained overlap without duplicate references, failed-activation rollback,
contraction, disconnect, unreadable graph rejection and bounded traversal.
Routing fixtures cover delayed events in distant rooms for both update and
removal passes, unrelated call-site/room fallback, and views following coverage
changes without retaining an obsolete room list. Six new update/removal guards
matched the owner's running 95032 image before installation. These tests do
not execute the game's native activation/spawn functions. The owner confirmed
the enemy-visibility fix in-game after installation; headset/runtime details
were not separately recorded for that report. Teleport, death, broader level
travel, disconnect and sustained performance need further validation.
Neither model handles nor subscription counts prove visible
draws, and grass/bush behavior may involve a separate producer.

Runtime status adds `subscription_ready`, `subscription_requested_rings`,
`subscription_state`, `subscription_held_rooms`, `subscription_client_rooms`,
`subscription_depth`, `subscription_capped`, and `subscription_transitions`.
`subscription_update_passes` and `subscription_removal_passes` count expanded
native passes since DLL load; `subscription_update_rooms` reports the most
recent expanded update's room count. These confirm routing, not visible draws.
State0=native/waiting for local game initialization;1=waiting for guards/hooks;
2=activation/transition in progress;3=expanded;4=graph/activation failure;
5=client-membership readback mismatch;6=stale owner discarded;
7=unsupported build. A true ready flag alone is not evidence of activation.

Use `subscription_status.py` beside `foliage_runtime_status.py` to print current
status without opening a GUI or reading game memory. Use
`Set-Subscriptions.ps1 -Rings 0` or `-Rings 8` for comparisons.

Private live disassembly and captures remain under ignored build-frame paths.
They are not source assets or package payloads.
