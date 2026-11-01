# Native iPhone/iPad app

Open `Clayfight.xcodeproj` here and run scheme **Clayfight**. The UIKit host,
AVFoundation camera/video workshop, photo imports, pose guides, drafts and
Application Support storage are native; there is no SDL or Lua on iOS.

The target builds all current `src/game`, `src/support` and CAGE C++ files.
`python3 tools/update_xcode.py` refreshes source references after adding files.
Supply the supported arcade chips in `data/primrage/`; the build phase runs
`tools/ios_bundle.py` and packages them with the default photo creatures as
`GameData`. Choose a development team in Xcode for a physical device.

Two GameController devices and the native touch controls are supported.
`--native-checks` runs the existing capture/photo QA; `--capture` opens the
workshop. Real camera acquisition requires a device. Source photos, videos,
drafts and published fighters stay in Application Support/Clayfight.

Full Simulator builds run in cloud CI; `CLAY_ALLOW_MISSING_ROM=1` allows its
compile-only job to omit private ROMs. Use `make ios-simulator` with local data
when intentionally building locally, after estimating the DerivedData footprint.
