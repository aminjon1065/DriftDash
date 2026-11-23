#!/bin/zsh
# Builds, cooks and packages for iOS; then installs on the connected iPhone and launches it.
#   Scripts/deploy_ios.sh                 # build + install + launch (ask the owner first: it takes over the phone)
#   Scripts/deploy_ios.sh --build-only    # build + cook + package only, nothing goes to the phone
# Requires: UE 5.8 iOS platform support (Epic Launcher > Options > iOS), signing set in Config/DefaultEngine.ini
# (automatic, team 4RPQ3AXM9Q only).
source "${0:A:h}/env.sh"
CONFIG="${DD_IOS_CONFIG:-Development}"
LOG="$DD_ROOT/Saved/Logs/Package_IOS.log"
BUILD_ONLY=""
[[ "$1" == "--build-only" ]] && { BUILD_ONLY=1; shift; }

if [[ ! -f "$UE_ROOT/Engine/Binaries/IOS/UnrealGame.target" ]]; then
	echo "ERROR: iOS platform support is not installed in UE 5.8 (Epic Games Launcher > Library > 5.8 > Options > iOS)."
	exit 2
fi
if [[ -z "$BUILD_ONLY" ]]; then
	DEVICE="$(dd_ios_device)"
	[[ -z "$DEVICE" ]] && { echo "No paired iPhone found. Connect it by cable once, trust this Mac, and pair it in Xcode (Window > Devices and Simulators)."; exit 2; }
fi

mkdir -p "$DD_ROOT/Saved/Logs"
echo "== Build, cook, package ($CONFIG)"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
	-project="$DD_PROJECT" -platform=IOS -clientconfig="$CONFIG" \
	-build -cook -stage -package -pak -iostore -archive -archivedirectory="$DD_ROOT/Packaged/IOS" \
	-nop4 -utf8output -unattended > "$LOG" 2>&1
CODE=$?
grep -E "BUILD SUCCESSFUL|BUILD FAILED|error:|Error:|ERROR:|AutomationTool exiting" "$LOG" | tail -20
[[ $CODE -ne 0 ]] && { echo "Packaging failed (exit $CODE). Log: $LOG"; exit $CODE; }

if [[ -n "$BUILD_ONLY" ]]; then
	find "$DD_ROOT/Packaged/IOS" "$DD_ROOT/Binaries/IOS" -maxdepth 3 \( -name "*.ipa" -o -name "*.app" \) 2>/dev/null
	echo "Built only (nothing installed). Log: $LOG"
	exit 0
fi
exec "${0:A:h}/install_ios.sh" "$@"
