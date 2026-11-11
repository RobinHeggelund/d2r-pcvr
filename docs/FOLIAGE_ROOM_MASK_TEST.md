# Moving grass band: room/world mask test

The owner reported no change from bypassing sampled local exclusions, and
confirmed live shader activation by hiding grass with footprint0 and restoring
it. This second experiment tests a different rejection: the packed room/world
mask, which uses fractional world/surface coordinates and an18x18 cell lookup.

The updated GUI adds **Room/world mask**. On/default retains the native mask;
Off clears only the mask's final rejection decision. It keeps coordinates,
height sampling, material lookup, local sampled exclusions (when their own
checkbox is on), camera/density/orientation checks and candidate capacity.
This is diagnostic, not an established fix. Off may allow grass in places the
native mask normally excludes.

Close D2R and D2RLoader before running this package's `Install-Test.ps1 -Mod pcvr-dev`.
It backs up and replaces only the DLL, preserving settings, shader and saves.
For headset VR use the owner's normal launcher:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\Downloads\build_and_play_pcvr_with_saves.ps1" -Action Play -SaveProfile Dev
```

This package also includes `Play-Foliage-Desktop.ps1` for intentionally playing
without headset transport. Do not use that desktop launcher for a headset test.

Open **this package's** `Launch-Foliage-Controls.cmd`. Keep **Local exclusion
checks on**, your current sliders unchanged, and a spacing already found stable
(2,4 or8). Do not use a preset during the comparison.

1. Room/world mask **On**, Apply, walk through the moving band toward a
   recognizable patch.
2. Room/world mask **Off**, Apply, allow about a second, repeat the same walk.
3. Room/world mask **On**, Apply, repeat to see whether the original behavior
   returns. Restore both checkboxes On after testing.

Report whether the moving instability improves, worsens or stays the same.
An isolated hitch at the moment of Apply may be shader creation, not the band.
If the band disappears/reappears with this setting, it implicates this packed
mask path; that alone does not establish which producer/update causes it.

INI diagnostics: `local_exclusion=1` and `room_mask=1` are native defaults.
Only `room_mask=0` is needed for this comparison. Both can update live with
Apply after installing this DLL and fully restarting the game/loader. The GUI
saves ten foliage keys, preserving unrelated settings including candidate_count.
