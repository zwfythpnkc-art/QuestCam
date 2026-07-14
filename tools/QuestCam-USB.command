#!/bin/zsh

set -u

find_adb() {
  if command -v adb >/dev/null 2>&1; then
    command -v adb
    return
  fi
  local candidates=(
    "$HOME/Library/Android/sdk/platform-tools/adb"
    "/opt/homebrew/bin/adb"
    "/usr/local/bin/adb"
    "/Applications/SideQuest.app/Contents/Resources/app.asar.unpacked/build/platform-tools/adb"
  )
  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "$candidate" ]]; then
      print -r -- "$candidate"
      return
    fi
  done
}

ADB="$(find_adb || true)"
if [[ -z "$ADB" ]]; then
  echo "ADB was not found. Install SideQuest Advanced or Android Platform Tools."
  read "?Press Return to close..."
  exit 1
fi

if ! "$ADB" start-server >/dev/null; then
  "$ADB" kill-server >/dev/null 2>&1 || true
  "$ADB" start-server >/dev/null || exit 1
fi

DEVICE_COUNT="$("$ADB" devices | awk 'NR > 1 && $2 == "device" { count++ } END { print count+0 }')"
if [[ "$DEVICE_COUNT" -eq 0 ]]; then
  echo "No authorised Quest found. Allow USB debugging inside the headset."
  read "?Press Return to close..."
  exit 1
fi

"$ADB" forward --remove tcp:5353 >/dev/null 2>&1 || true
"$ADB" forward tcp:5353 tcp:5353 || exit 1
echo "QuestCam USB tunnel is active: http://127.0.0.1:5353/"
open "http://127.0.0.1:5353/"
read "?Press Return when you are finished..."
"$ADB" forward --remove tcp:5353 >/dev/null 2>&1 || true
