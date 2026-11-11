# Moving grass band: local exclusion test

This is a diagnostic build, not a confirmed fix. It tests whether three native
sampled local-exclusion decisions cause the moving band of unstable grass.
The checkbox is on by default (native checks). Turning it off forces only those
three decisions to accept placement; it does not move candidates, change height
sampling, bypass packed room masks or orientation/camera/density checks, or
increase candidate capacity. The guarded 4x experiment remains as before.

Close D2R and D2RLoader, install this package using `Install-Test.ps1 -Mod pcvr-dev`,
then launch `Play-Foliage-Desktop.ps1`. It calls the owner's existing Downloads
script with `-Action Play -SaveProfile Dev` and disables OpenXR transport for
this process, avoiding its five-second headset initialization retry. Use
Alt+Enter for a window. The installer updates only the DLL, backs it up, and
preserves the installed INI, on-disk shader and saves. No automatic launch.

Run **this package's** `Launch-Foliage-Controls.cmd`; older GUI copies lack the
new checkbox. Select pcvr-dev and keep the current slider values. Use a spacing
the owner already found stable, such as 2, 4 or 8, and do not change it during
the comparison. Do not choose a placement preset, because that resets sliders.

1. Leave **Local exclusion checks** checked and click **Apply to game**.
   Walk toward a recognizable patch that is stable far away and becomes
   unstable when the moving band reaches it.
2. Uncheck **Local exclusion checks**, click **Apply to game**, allow about a
   second for settings/shader activation, and repeat the same short walk.
3. Recheck and Apply, then repeat to see whether the symptom returns. A brief
   one-time shader creation hitch while switching is not the moving-band symptom.

Off is a diagnostic bypass and may allow grass in areas normally excluded.
Restore On and Apply after testing. Do not adjust heightmap controls or other
sliders during the comparison. No restart is needed to toggle checks once this
DLL has been installed and the game restarted.

Report whether the moving band improves, stays unchanged, or gets worse, and
whether switching back restores the original behavior. Improvement implicates
the sampled local-exclusion acceptance path; it does not establish which
texture producer or update boundary is responsible. No improvement means we
should investigate other placement inputs/rejection or later rendering, not
assume these three comparisons caused the band. If no visible difference of
any kind occurs, activation of the new DLL/variant is still an open question.

The desktop GUI owns nine foliage keys: the original eight sliders plus
`local_exclusion` (1 native, 0 bypass). This diagnostic key is read directly
by the existing half-second settings loop and has no new headset-menu row.
Presets restore native local-exclusion checks along with their placement values.
