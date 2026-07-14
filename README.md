# QuestCam 0.3.6

QuestCam is a spectator-camera and OBS streaming mod for **Beat Saber
1.40.8_7379 on Meta Quest**, designed for Quest 3S and OBS on macOS.

## Features

- final-frame capture that matches the headset and supports map effects
- optional wide-FOV camera with position and rotation smoothing
- adjustable resolution, frame rate, and JPEG quality
- built-in MJPEG video server—no Mac companion app or OBS plugin required
- streaming stereo PCM game audio
- in-game settings screen powered by BSML
- settings persisted in JSON

This is a beta build. It compiles and packages successfully, but the streaming
path still needs validation on a physical Quest 3S. The default 960x540 at 15
FPS is designed to reduce gameplay stutter.

Version 0.3.6 raises the FOV control to 170 degrees and the stream frame-rate
control to 60 FPS. 60 FPS is optional because it substantially increases Quest
CPU/GPU load; 960x540 is recommended when using it. Version 0.3.5 renders Wide + Smooth through Beat Saber's real effects camera,
adds explicit scene-change rebuilding, and includes a stalled-frame watchdog.
This targets the grey/black map artifacts and freezes after finishing, failing,
or leaving a level. Version 0.3.4 rebuilt capture when Camera.main changed, but
some Beat Saber transitions reuse that same camera object. Version 0.3.3 starts the proven Wide + Smooth renderer directly and removes a
Quest client-detection gate that could leave Safari and OBS permanently black.
It retains the low 960x540/15 FPS performance profile. Version 0.3.2 automatically switches to the proven Wide + Smooth renderer when
Quest XR does not invoke the fast final-frame callback, preventing a permanently
black Safari/OBS source. Version 0.3.1 restores FOV and smoothing as an optional **Wide + Smooth** mode.
Its extra world render only runs while OBS/Safari is connected and is limited to
the selected stream FPS. Fast mode remains the recommended Vivify-compatible
option. Version 0.3.0 captures Beat Saber's completed headset frame instead of rendering
the world a second time. This removes the grey replacement environment and XR
click rectangles, preserves Vivify map effects, and substantially reduces GPU
work. Encoding only runs while a browser or OBS client is connected.

## Install

1. Patch Beat Saber `1.40.8_7379` with Scotland2 using ModsBeforeFriday.
2. Open **Add Mods**, choose **Upload Files**, and upload `QuestCam.qmod`.
3. Start Beat Saber. Open **Settings → Mod Settings → QuestCam** to adjust it.
4. QuestCam settings apply immediately; no restart is required.

## Connect OBS on Mac

The Quest and Mac must be on the same Wi-Fi network.

1. Find the Quest's IP address in its Wi-Fi network details.
2. In OBS, add a **Browser** source.
3. Set URL to `http://QUEST_IP:5353/`, replacing `QUEST_IP` with the address.
4. Set the Browser source width and height to match QuestCam's settings
   (`960 × 540` by default).
5. Enable **Control audio via OBS** in the Browser source properties.

You can test the connection in Safari or Chrome first by opening the same URL.
The direct endpoints are:

- video: `http://QUEST_IP:5353/stream.mjpg`
- audio: `http://QUEST_IP:5353/audio.wav`

If the page does not load, confirm both devices are on the same non-guest Wi-Fi
and that client isolation is disabled on the router.

## Connect through a USB cable

USB mode uses Android Debug Bridge port forwarding, so the video and audio do
not need to travel over Wi-Fi.

1. Install SideQuest Advanced or Android Platform Tools on the Mac.
2. Connect the Quest with a data-capable USB cable.
3. Put on the headset and accept the **Allow USB debugging** prompt.
4. Start Beat Saber and leave QuestCam enabled.
5. Double-click `tools/QuestCam-USB.command` on the Mac.
6. In OBS, use this Browser Source URL:

   `http://127.0.0.1:5353/`

The equivalent Terminal command is:

```sh
adb forward tcp:5353 tcp:5353
```

Run `adb forward --remove tcp:5353` when finished.

## Optional OBS Lua helper

OBS can create the USB video and audio Media Sources automatically. Add
`obs/QuestCam-OBS.lua` in **OBS → Tools → Scripts**, then click
**Connect Quest + Add Sources**. Lua is bundled with OBS, so no Python setup is
needed.

## Configuration

Settings are stored on Quest at:

`/sdcard/ModData/com.beatgames.beatsaber/Configs/questcam.json`

Default configuration:

```json
{
  "enabled": true,
  "streamAudio": true,
  "wideSmoothMode": true,
  "fieldOfView": 90.0,
  "positionHalfLife": 0.08,
  "rotationHalfLife": 0.12,
  "width": 960,
  "height": 540,
  "fps": 15,
  "jpegQuality": 60,
  "capturePipelineVersion": 2,
  "serverPort": 5353
}
```

Higher JPEG quality and frame rate increase CPU use. Leave `wideSmoothMode`
off for the lowest overhead and full Vivify compatibility. Turn it on to use
the separate FOV and smoothing controls; 960x540 at 15 FPS is recommended on
Quest 3S.

## Build on macOS

Install QPM, PowerShell 7, CMake, and Ninja, then run:

```sh
qpm restore
qpm ndk resolve
qpm s qmod
```

If the template rejects QPM's NDK path because `Application Support` contains a
space, create a no-space symlink to the NDK and put the symlink path in
`ndkpath.txt`.

## Portable smoothing tests

```sh
c++ -std=c++20 -Wall -Wextra -Werror -Iinclude \
  src/PoseSmoother.cpp tests/PoseSmootherTests.cpp \
  -o /tmp/questcam-tests
/tmp/questcam-tests
```

## Technical notes

The current transport uses MJPEG because OBS Browser Source supports it on
macOS without extra software. Hardware H.264 is a future performance upgrade,
not a requirement for OBS connectivity. Audio is streamed as stereo 16-bit PCM
inside a continuous WAV response.

Built from Lauriethefish's Quest mod template and the BSMG Quest modding stack.
