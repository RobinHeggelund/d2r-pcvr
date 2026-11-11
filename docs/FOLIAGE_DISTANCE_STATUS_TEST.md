# Verify room expansion and model distance during the moving grass band

The owner observed grass changing only during movement, with its unstable band
coinciding with the appearance of both shrine structures and shrine effects.
Neither exclusion-mask test changed it. Changing model distance to300 or10
also left the boundary unchanged, and rings8 had already been tested after a
full restart. Those results do not yet identify the responsible native system.

This build fixes a verified source-level retry gap: optional room tiling,
append, model-radius and recalculate bindings continue to be checked (at most
twice a second) after the basic room hooks have installed. Late-published
function pointers use atomic publication. The existing settings worker polls
the refresh even after all camera/room hooks are considered installed.
It preserves the one-room-per-tick tiling budget and existing signature guards.
Failed tiling attempts no longer count as successfully loaded rooms.

It also publishes a current status snapshot from the settings worker to a4KiB
per-profile named shared-memory mapping. There are no added disk writes or log
reads. The desktop GUI opens only this status mapping read-only, not game memory;
unavailable, inconsistent, unsupported or stale snapshots are identified.
Existing foliage controls and candidate capacity are unchanged by this change.
This package retains the previous shader diagnostics, both native by default.

Close D2R and D2RLoader and run this package's `Install-Test.ps1 -Mod pcvr-dev`.
The guarded installer updates/backups only the DLL and preserves INI/shader/saves.
For VR use the owner's normal Downloads launcher:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\Downloads\build_and_play_pcvr_with_saves.ps1" -Action Play -SaveProfile Dev
```

The optional `Play-Foliage-Desktop.ps1` intentionally disables headset transport.
Do not use that launcher for headset testing.

Open **this package's** `Launch-Foliage-Controls.cmd`. The runtime readout and
**Copy runtime status** button are at the bottom. The additional
`foliage_runtime_status.py` file must stay beside `foliage_control.py`.

1. Keep both exclusion/mask checkboxes On and retain a stable spacing. Use the
   owner's usual rings8 and object-distance settings. If the earlier distance10
   test is still selected, restore the usual value before this observation;
   the latest previously recorded usual distance was3260. Do not reset presets.
2. Enter an outdoor area and walk for several seconds, then toward the same
   shrine/grass boundary. Note whether behavior changes with this build.
3. Click **Copy runtime status** and paste it into the chat. No historical log,
   binary inspection, or additional capture is needed for this first readback.

How to read the status:

- Requested rings/radius are what the plugin loaded from the settings. **Actual
  object radius** is the native getter's current value, not the saved INI value;
  unavailable means that binding has not resolved.
- Room hooks, tiler, append and recalculate indicate separately which paths
  have resolved. Room hooks ready alone does not prove distant tiling works.
- Expansion state distinguishes missing tiling/append bindings, missing or
  invalid version data, missing hero room, and a running room-graph traversal.
  Its age shows whether that path ran recently.
- Furthest room depth is the deepest eligible node with a non-null ROOM in
  the latest expansion pass. It can be less than the requested rings because
  of level/graph shape or remaining tiling work. **It is not proof that every
  tile/model/grass input is rendered or simulated at that depth.**
- Tiled successfully counts non-null returns from on-demand tiling; attempts
  counts all calls, including failures. Rooms added is the latest list expansion,
  not an accumulated number of entities or shrines spawned.

The retry defect is real in source, but no evidence yet establishes that it
occurred in the owner's session or caused the grass/shrine boundary. The live
readout is intended to distinguish those cases before another rendering change.
