# Foliage work on current upstream main

## Current result (November 11, 2026)

This branch also includes automatic eight-ring offline server activation and
client room subscriptions, with the native ongoing unit-update and removal
passes expanded to the same retained room set. The owner confirmed that the
enemy-visibility fix works in-game. The production build passed all 235 native
CTests; see [the subscription design and validation notes](FOLIAGE_SUBSCRIPTION_EXPERIMENT.md).
The guarded 4x foliage-capacity experiment remains unsupported on game build
95032; enemy visibility does not establish a grass/bush fix.

The integration and package references below describe the earlier local
staging step. They are historical; those packages predate the enemy-delivery
fix and should not be reinstalled as the current build.

## Earlier integration notes

Working checkout: `F:\projects\d2r-pcvr\build-frame\worktrees\folliage-current-main`.
Branch: `feat/folliage-current-main`. Base: upstream main
`9975da7e5c9fe7f723b63f3463e04ba8608b0ba2`, release0.3.33, fetched November11,2026.

The complete foliage work has been applied as uncommitted changes atop that
base: eight live sliders, guarded4x capacity experiment, both diagnostic mask
toggles, headset-free desktop transport opt-out, late draw-distance binding
retry fix and live per-profile runtime status. Upstream's reorganized settings
menu is retained; foliage is under **Graphics > Foliage**. Its existing256-bit
settings change mask is retained, with foliage keys appended after upstream's
keys. No commits, pushes, main-branch edits or game installation were performed.

The old `folliage-distance` checkout and its dirty work remain unchanged. A
source-only patch backup is in that checkout at
`build-frame/foliage-latest-main-20261111.patch`; untracked source/docs were also
copied into the new checkout. Both worktrees use the existing shared deps junction,
but each has its own build output. Continue development in this new checkout.

The staged `build-frame/foliage-latest-stage` package includes the new DLL and
current project-owned ReShade effects/headers and sky textures. Upstream split
the effect into multiple headers, so a DLL-only update would be incomplete.
The guarded installer backs up every replaced payload file, refuses while D2R
or D2RLoader runs, and preserves INI files, ReShade configuration, calibration,
modinfo, saves and the game's on-disk grass shader override. It does not extract
game assets, restage a developer profile or launch the game.

Close D2R and D2RLoader and install:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\projects\d2r-pcvr\build-frame\worktrees\folliage-current-main\build-frame\foliage-latest-stage\Install-Test.ps1" -Mod pcvr-dev
```

Launch headset VR using the existing Downloads script:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\Downloads\build_and_play_pcvr_with_saves.ps1" -Action Play -SaveProfile Dev
```

Always pass `-Action Play`; its default rebuilds the unrelated original checkout.
For intentional desktop-only testing the packaged `Play-Foliage-Desktop.ps1`
disables headset transport. Use the new package's foliage GUI; its runtime reader
module must stay next to it.

The outstanding experiment remains actual distance/room-status readback while
walking toward the shrine/grass band. Both mask bypasses previously made no
visible difference. Leave both checks On and use stable spacing/usual distances,
then **Copy runtime status** and report whether the band changed. No claim of
correct heights,4x runtime activation or a fixed band follows from this merge.
