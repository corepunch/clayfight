# Clayfight for iPhone and iPad

Open [Clayfight.xcodeproj](Clayfight.xcodeproj) and run the **Clayfight** scheme.
Deployment target: iOS/iPadOS 17+. Both device families are supported.

```sh
make ios-simulator
make ios-device      # compile for a real device, without signing/installing
```

The Simulator command applies an ad hoc signature after building, so rebuilding
over an existing app does not leave a stale signature that blocks installation.
It does not require an Apple development team.

For a physical iPhone/iPad, choose your Apple development team in Xcode's
Signing & Capabilities settings, select the device, and Run.

The original Primal Rage PC files must be in `../primalrage/`. The build phase
converts the seven creature definitions and bundles the local `.GRA` data,
INDEX and clay fighters. Generated game data remains ignored by Git.

- `AppDelegate.m`: scene lifecycle, 60 Hz game host, framebuffer presentation,
  touch controls, application storage and bundled-roster installation.
- `NativePlatform.m`: platform functions implemented with UIKit, Core Graphics
  and AVAudioEngine. The iOS target contains no SDL, web view or subprocesses.
- `CaptureController.m`: native AVFoundation photo/video capture and preview,
  16 pose/portrait steps, vector monster guides, Photos picker, video frame
  selection, local drafts, review/correction and fighter publication.
- `PoseGuideView.m`: original vector monster artwork for all 16 steps, with
  distinct walking phases, active limbs, movement arrows and a frontal portrait.
- `PhotoProcessing.m`: background flood-fill, enclosed pale-feature retention,
  common foot baseline, shared scale and handheld head-width calibration.
- `NativeChecks.m`: Debug-only integration checks running in the iOS runtime.

All photos, silent videos, settings, drafts and fighters live on the device.
There is no network/HTTPS setup. The only capture permission requested is the
camera permission; selecting media uses the system Photos picker.

The app opens on the original-resource main menu. Use the on-screen D-pad and
Select button to navigate, then choose Create Fighter to open the camera in
the app. Camera controls support stills and three-second videos. In Simulator,
use imported media; real camera operation must be tested on hardware.

Debug launch arguments:

- `--native-checks`: test foreground pixels and white eyes, image orientation,
  foot alignment, handheld normalization, portrait shoulder cropping, portrait/animation import and a real
  custom fighter versus Sauron. Read `Library/Application Support/Clayfight/native-checks.json`.
  The same run exports `guide-qa/all-poses.png` for visual review.
- `--capture`: open the native creation screen for layout inspection.

The desktop SDL build and browser capture helper remain development tools.
