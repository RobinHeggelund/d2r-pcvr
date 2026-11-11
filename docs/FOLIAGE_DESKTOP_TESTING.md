# Desktop foliage testing

The normal native VR transport retries failed OpenXR initialization every five
seconds from the game's Present callback. With no headset, those synchronous
runtime calls can interrupt desktop play. Windowed mode alone does not disable
the transport.

After installing a DLL containing this change, launch from PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\projects\d2r-pcvr\build-frame\worktrees\folliage-current-main\steamvr\Play-Foliage-Desktop.ps1"
```

This wrapper calls the owner's Downloads launcher with `-Action Play
-SaveProfile Dev`, setting `D2R_PCVR_DESKTOP_TEST=1` only for the launched process
tree. The plugin then omits headset transport callback registration, including
OpenXR initialization, retries and headset submission. The flag is startup-only;
close both D2R and D2RLoader before changing launch mode.

First-person camera behavior, configured VR mode, live foliage controls and the
guarded 4x candidate experiment remain available. INI settings and save files
are not edited by this wrapper. The existing Downloads launcher still selects
the Dev save profile using its normal metadata backup behavior.

For a headset session, use the existing Downloads launcher directly without the
desktop flag. Alt+Enter switches the game's desktop window mode independently.
