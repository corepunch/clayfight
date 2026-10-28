# Menus, conquest and the capture workshop

Clayfight targets **native iPhone and iPad**. Open `ios/Clayfight.xcodeproj`
and run the Clayfight scheme. The shared C engine renders at 640×400, exactly
2× the DOS 320×200 frame. UIKit presents the game and touch controls;
AVFoundation captures photographs and video inside the app. The native target
does not link SDL and does not run a browser, web server, Python or subprocess.

Build for Simulator with `make ios-simulator`; `make ios-device` verifies the
unsigned iPhoneOS build. To install on a physical device, select your signing
team in Xcode and run on that device. See `ios/README.md` for the native layout.
`make && ./clayfight` remains the desktop development harness.

## Original reference

The reference is the installed **Primal Rage PC 1.0** data, not another port.
`python3 tools/menu_reference.py` extracts the original menu vocabulary from
`ENGLISH.TXT`, the title layers, temple, stone frame, globe and continent
patches into `build/menu-reference/`. Its JSON records archive/frame/hotspot
information. These derived files, like the original game data, stay out of Git.

The working menu tree is:

- Main menu: Start, Create Fighter, Options.
- Start menu: Arcade, Training, Tug of War, Endurance.
- Options: Config Options, Adjust Volume, Sound Test, Modify Controls.
- Config: number of rounds, round time, CPU difficulty, credits, sound,
  restore defaults.

Settings are saved in `build/options.cfg`. Modify Controls currently displays
the working keyboard/controller bindings; rebinding is not implemented.
Music playback/test, gore controls, high scores and statistics are not yet
ported. Layout, unmapped artwork palettes, CPU logic, arena composition, territory
pins and transitions are reconstructed; this is not a binary-exact port of
the PC menus or the complete original game. Original specials, fatalities,
followers, the refuelling bonus and character ending cinematics remain outside
this implementation. The ending screen currently celebrates the chosen fighter.

## Playing through the game

Pick one fighter in Arcade. With no second player, this starts a conquest:
seven home territories, including the mirror fight for an original character.
A clay fighter faces the same original seven rulers. The map shows the current
ruler, arena and conquered lands before each match. Confirm to start, or wait
six seconds. Each match uses the configured number of rounds.

After seven territories, World Domination leads into seven final opponents in
the Dinosaur Graveyard. There is no clock, each rival needs one defeat, and
the player's remaining health carries between rivals. The final battle starts
at full health. Defeat offers a 10-second continue (15 in the final battle),
using configured credits. Continues keep territories; final-battle continues
also keep the current rival and its remaining health. Winning the final
opponent leads to the victory screen. Conquest progress is session-local.

A second player can join character select using their attack buttons, producing
a local versus match. A new select session resets this join state. Training
lets P1 choose both their fighter and a stationary opponent; health restores
and the clock is disabled. Tug of War uses one shared health pool. Endurance
lets each side select four fighters; the winner keeps their health and faces
the next member of the other team. Without P2, the CPU selects a team.

Esc pauses a fight; Q while paused abandons it and returns to the main menu.

Character select uses the retail temple composition: four stone heads above
three, red/yellow cursors and animated fighters standing on the two original
stone podiums. The full 272×170 temple is centred in the 320×200 view with no
zoom, nonuniform stretch or cropping, so its source pixels appear exactly 2×.
The original tile order is Sauron, Talon, Armadon, Blizzard above Chaos,
Vertigo, Diablo. Original
fighters keep their seven tiles; clay fighters occupy separate seven-tile
temple pages. Left/right wraps between pages, up/down follows the staggered
rows, and desktop Tab jumps to the next page. C opens Create Fighter. Palette
bindings for the temple and original large portraits come from the installed
game's data; the small heads retain their fighter palettes and background
colours. Additional creation/page hints occupy the lower margin below the
complete scene. The desktop window starts at 640×400 and cannot shrink below
that size; larger windows letterbox the complete framebuffer.

## Photographing a fighter

Choose **Create Fighter** in the app. The native camera view opens on the
same iPhone or iPad, with an adjustable pose outline drawn above the live
`AVCaptureVideoPreviewLayer`. Camera permission is requested when the user
taps Enable Camera. Neither HTTPS nor a network connection is needed. Recorded
clips are silent, so the app does not request microphone permission.

The workshop is in Russian. Name the fighter using 1–16 Latin letters, digits
or underscores, starting with a letter; this matches the original game's font.
Choose tripod or handheld mode, then capture:

1. Ready and breathing poses.
2. Four walking poses.
3. Punch windup, punch contact, kick.
4. Crouch, jump, block, hit, down and victory.
5. A separate close-up portrait.

Every screen overlays the same neutral monster outline, posed for the requested
movement. The original vector artwork has shaped hands/feet, distinct walking
phases, a separate frontal portrait, directional arrows and a highlighted active
limb. White contours with dark edges remain visible on light and dark backdrops.
The overlay has adjustable opacity, corner margins and an explicit foot baseline. The jump photo is taken on the table; the simulation
supplies the actual airborne height. The portrait step asks the user to move
closer and face the camera. Keep the whole head and horns inside the frame;
shoulders may be cropped at the bottom.

The camera captures stills or three-second videos using AVFoundation. Videos
are kept with the fighter; a representative frame supplies the pose. The system
photo/video picker also accepts existing media, including iPhone HEIC images.
For an imported video, use native playback to choose the frame. Motion is
assembled from the guided pose frames, not inferred from arbitrary video.

## Alignment and processing

Use a plain, contrasting paper backdrop and soft lighting; remove hands before
capture. Background flood-fill keeps the main silhouette and preserves pale
features enclosed by clay. Review the cutout and adjust the separation threshold
if eyes, fingers or tail are wrong. A clipped body figure is rejected. Portraits allow the shoulders to touch the
bottom edge while still requiring margins above and beside the head. The processing
is intended for controlled plain backdrops, not arbitrary scenes.

Tripod mode uses one scale measured from the standing pose. Handheld mode asks
for two taps marking the head width in each body frame. That stable measurement
compensates for distance changes; the frame's entire bounding height is never
used to stretch a crouch or kick to standing height. It does not reconstruct
perspective or compensate for changes in camera angle. Both modes align feet
automatically and allow manual zoom and horizontal corrections. Retaking the
stand regenerates all output frames against the new reference.

Review all 16 images before publishing. The native draft lives in the app's
Application Support directory; reopening resumes at the first missing pose.
Publishing removes the background and aligns all frames on the device, writes
a regular creature file in a hidden staging directory and atomically moves the
completed folder into the app's roster. The existing C photo pipeline then
supplies its shared 31-colour palette, 92-world-unit height, 184-pixel standing sprite,
foot hotspots and separate portrait. Selection refreshes as the camera closes.

The app keeps original photos and recorded movies under the fighter's `sources/`
directory, normalized `photos/`, `fighter.txt` and `capture.plist`. Bundled
original/concept fighters and user-created fighters appear in the same roster.
The roster pages after seven cells and supports up to 64 fighters. Original game
data is bundled from the developer's local copy during the Xcode build; it is
never downloaded at runtime or committed to the repository.

The old `tools/capture_server.py` browser workshop is a **desktop development
utility**, not the iPhone/iPad product. Its browser permission/HTTPS constraints
do not apply to the native app.

## Verification

The Debug app accepts `--native-checks` to run pixel/coordinate checks,
handheld scale checks, separate portrait import and a real custom-fighter match
inside the iOS runtime. It writes `native-checks.json` in the app's Application
Support/Clayfight directory. `--capture` opens the native wizard for Simulator
layout inspection. Real camera hardware still requires physical-device QA.

`make check` builds the desktop harness, checks rendering, compares all 266 original move
animations, exercises the complete conquest/continue/final flow and the four
start modes, and imports a photographed fixture through the real engine before
running it against Sauron. `node tests/capture-processing.mjs` separately checks
background segmentation, enclosed white eyes, dust rejection, extended limbs,
foot alignment and handheld scale compensation.

Headless views: `-warp menu`, `-warp start`, `-warp options`, `-warp select`,
`-warp map [-hero boxer]`; combine with the existing `-shots`, `-every`, `-tics`
switches. Example: `./clayfight -warp map -shots build/map- -tics 1`.
