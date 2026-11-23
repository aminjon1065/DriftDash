#!/bin/zsh
# Shared paths for the helper scripts. Override UE_ROOT if the engine lives elsewhere.
export UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export DD_ROOT="$(cd "$(dirname "${0:A}")/.." && pwd)"
export DD_PROJECT="$DD_ROOT/DriftDash.uproject"
export UE_EDITOR_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
export UE_EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"

# Fails if any C++ source is newer than the editor module the engine will load (stale binary guard).
dd_check_fresh() {
	local manifest="$DD_ROOT/Binaries/Mac/UnrealEditor.modules"
	local lib="$DD_ROOT/Binaries/Mac/$(grep -o 'libUnrealEditor-DriftDash[^"]*' "$manifest" 2>/dev/null)"
	if [[ ! -f "$lib" ]]; then echo "ERROR: editor module not built. Run Scripts/build_editor.sh"; return 1; fi
	# Private/IOS and Private/Android are not compiled into the Mac editor module.
	local newer=$(find "$DD_ROOT/Source" -newer "$lib" -type f -not -path "*/Private/IOS/*" -not -path "*/Private/Android/*" | head -1)
	if [[ -n "$newer" ]]; then echo "ERROR: $newer is newer than $lib. Run Scripts/build_editor.sh"; return 1; fi
	return 0
}

# UDID of the iPhone to use: $DD_IOS_DEVICE if set, otherwise the first paired physical iPhone (prefers a connected one).
dd_ios_device() {
	if [[ -n "$DD_IOS_DEVICE" ]]; then echo "$DD_IOS_DEVICE"; return 0; fi
	local json="$(mktemp)"
	xcrun devicectl list devices --json-output "$json" >/dev/null 2>&1
	python3 - "$json" <<'PY'
import json, sys
try:
    devices = json.load(open(sys.argv[1]))["result"]["devices"]
except Exception:
    devices = []
phones = [d for d in devices
          if d.get("hardwareProperties", {}).get("reality") == "physical"
          and d.get("hardwareProperties", {}).get("deviceType") == "iPhone"
          and d.get("connectionProperties", {}).get("pairingState") == "paired"]
phones.sort(key=lambda d: d.get("connectionProperties", {}).get("tunnelState") != "connected")
if phones:
    print(phones[0]["hardwareProperties"]["udid"])
PY
	rm -f "$json"
}
