#!/bin/zsh
# Installs the last iOS build on the iPhone and launches it. Ask the owner first: the game takes over the phone.
#   Scripts/install_ios.sh               # install + launch
#   Scripts/install_ios.sh --no-launch   # install only
source "${0:A:h}/env.sh"
DEVICE="$(dd_ios_device)"
[[ -z "$DEVICE" ]] && { echo "No paired iPhone found. Connect it by cable once, trust this Mac, and pair it in Xcode (Window > Devices and Simulators)."; exit 2; }
BUNDLE="com.driftdash.DriftDash"
LOG="$DD_ROOT/Saved/Logs/Package_IOS.log"
mkdir -p "$DD_ROOT/Saved/Logs"

APP=$(find "$DD_ROOT/Packaged/IOS" "$DD_ROOT/Binaries/IOS" -maxdepth 3 -name "*.app" -type d 2>/dev/null | head -1)
IPA=$(find "$DD_ROOT/Packaged/IOS" "$DD_ROOT/Binaries/IOS" -maxdepth 3 -name "*.ipa" 2>/dev/null | head -1)
TARGET="${IPA:-$APP}"
[[ -z "$TARGET" ]] && { echo "No .app or .ipa produced. Log: $LOG"; exit 3; }
echo "== Install $TARGET on $DEVICE"
for i in {1..3}; do
	xcrun devicectl device install app --device "$DEVICE" "$TARGET" > "$DD_ROOT/Saved/Logs/Install_IOS.log" 2>&1 && break
	[[ $i -eq 3 ]] && { echo "Install failed (is the iPhone unlocked and reachable?)"; tail -3 "$DD_ROOT/Saved/Logs/Install_IOS.log"; exit 4; }
	sleep 5
done
echo "Installed."
[[ "$1" == "--no-launch" ]] && { echo "Installed without launching (open the game on the phone)."; exit 0; }

echo "== Launch $BUNDLE"
# Launching needs an unlocked phone; retry for up to 3 minutes.
for i in {1..36}; do
	OUTPUT=$(xcrun devicectl device process launch --device "$DEVICE" --terminate-existing "$BUNDLE" 2>&1)
	if echo "$OUTPUT" | grep -q "Launched application"; then echo "Launched."; exit 0; fi
	[[ $i -eq 1 ]] && echo "Waiting for the iPhone to be unlocked..."
	sleep 5
done
echo "$OUTPUT" | tail -5
exit 5
