# Launch the existing foliage test profile without starting/retrying OpenXR.
# The installed DLL must support D2R_PCVR_DESKTOP_TEST. No INI edits or builds.
$ErrorActionPreference = 'Stop'
$launcher = Join-Path $env:USERPROFILE 'Downloads\build_and_play_pcvr_with_saves.ps1'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Existing game launcher missing: $launcher"
}
$previousDesktopTest = $env:D2R_PCVR_DESKTOP_TEST
try {
    $env:D2R_PCVR_DESKTOP_TEST = '1'
    & $launcher -Action Play -SaveProfile Dev
} finally {
    # D2RLoader inherits the flag; later ordinary VR launches do not.
    $env:D2R_PCVR_DESKTOP_TEST = $previousDesktopTest
}
