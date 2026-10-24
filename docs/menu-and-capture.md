# Menus, conquest and the capture workshop

Run `make && ./clayfight`. Enter opens the main menu. All SDL screens still
render at 640×480, with the original pixel art enlarged 2×. WASD or arrows
navigate menus; Enter / a face button confirms; Esc / controller Back returns.

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
ported. Layout, palette selection, CPU logic, arena composition, territory
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

## Photographing a fighter

Choose **Create Fighter**, or press C in character select. The game starts a
local Python companion and opens the workshop in a browser. No Python packages
are needed. It lists a second URL for a phone or tablet on the same Wi-Fi;
append the displayed `?token=...` to that address. B reopens the computer's
workshop. The server closes when the game exits.

The workshop is in Russian. Name the fighter using 1–16 Latin letters, digits
or underscores, starting with a letter; this matches the original game's font.
Choose tripod or handheld mode, then capture:

1. Ready and breathing poses.
2. Four walking poses.
3. Punch windup, punch contact, kick.
4. Crouch, jump, block, hit, down and victory.
5. A separate close-up portrait.

Every screen overlays the same neutral monster outline, posed for the requested
movement. The overlay has adjustable opacity, a centre line, safe margins and
an explicit foot baseline. The jump photo is taken on the table; the simulation
supplies the actual airborne height. The portrait step asks the user to move
closer and face the camera.

A live camera can capture a still or record a three-second silent video. The
recording and a representative frame are kept. Alternatively, import a photo
or a short video, scrub to the wanted moment and choose that frame. Videos
are archived; motion is assembled from the guided pose frames, not inferred
from arbitrary video. Each video is limited to 12 MB.

Browser live camera access requires `localhost` or **trusted HTTPS**. A phone
opening the computer's plain HTTP Wi-Fi address can use the native photo
capture/file picker and align the result under the outline afterward; that
native camera UI cannot display the web overlay. For live overlays on iOS,
serve with a certificate trusted by the device:

```sh
python3 tools/capture_server.py --host 0.0.0.0 --port 8765 \
  --cert /path/to/trusted-certificate.pem --key /path/to/key.pem
```

The browser's camera requirements are documented by
[MDN](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia).
The workshop chooses a supported recording format, including MP4 on Safari,
rather than assuming WebM; see
[WebKit's MediaRecorder documentation](https://webkit.org/blog/11353/mediarecorder-api/).
Camera hardware and recording on a physical iPhone/iPad still need device QA.

## Alignment and processing

Use a plain, contrasting paper backdrop and soft lighting; remove hands before
capture. Background flood-fill keeps the main silhouette and preserves pale
features enclosed by clay. Review the cutout and adjust the separation threshold
if eyes, fingers or tail are wrong. A clipped figure is rejected. The processing
is intended for controlled plain backdrops, not arbitrary scenes.

Tripod mode uses one scale measured from the standing pose. Handheld mode asks
for two taps marking the head width in each body frame. That stable measurement
compensates for distance changes; the frame's entire bounding height is never
used to stretch a crouch or kick to standing height. It does not reconstruct
perspective or compensate for changes in camera angle. Both modes align feet
automatically and allow manual zoom and horizontal corrections. Retaking the
stand regenerates all output frames against the new reference.

Review all 16 images before publishing. Accepted frames, alignment and videos
are saved as an IndexedDB draft on that device. Publishing sends them to the
local game host, generates a regular creature file, and runs the actual engine
for background removal, shared 31-colour palette, 92-world-unit height,
640×480 photo detail, foot hotspots and portrait processing. A failed import
never exposes a half-written fighter in the roster: validation happens in a
hidden staging directory, then the whole folder is atomically published.

Outputs are `creatures/custom-NAME-ID/`: `fighter.txt`, normalized `photos/`,
original `sources/` (including recorded videos), `capture.json` and a processed
preview sheet. Return to the game and press Enter to refresh character select;
no restart is needed. The portrait appears alongside the original seven and
the existing clay fighters. The roster pages after 14 cells and supports up to
64 fighters. Live capture is a companion for the desktop SDL game; this does
not add a native iOS game target.

## Verification

`make check` builds the game, checks rendering, compares all 266 original move
animations, exercises the complete conquest/continue/final flow and the four
start modes, and imports a photographed fixture through the real engine before
running it against Sauron. `node tests/capture-processing.mjs` separately checks
background segmentation, enclosed white eyes, dust rejection, extended limbs,
foot alignment and handheld scale compensation.

Headless views: `-warp menu`, `-warp start`, `-warp options`, `-warp select`,
`-warp map [-hero boxer]`; combine with the existing `-shots`, `-every`, `-tics`
switches. Example: `./clayfight -warp map -shots build/map- -tics 1`.
