#!/bin/zsh
# Builds an Android .apk for testing on a phone: arm64, ASTC + ETC2 textures, game data inside the .apk (one file).
#   Scripts/package_android.sh             # Development build -> Packaged/Android/DriftDash.apk
#   Scripts/package_android.sh --install   # also installs it on a USB-connected phone (USB debugging on); ask first
# Needs: UE 5.8 Android platform support (Epic Games Launcher > Library > 5.8 > Options > Android),
#        Android SDK (platforms;android-36, build-tools;36.0.0, cmake;3.22.1), NDK r27c (ndk;27.2.12479018), JDK 17.
# The .apk is signed with the Android debug key: fine for testing, not for the Play Store.
# -NoUBA: the Unreal Build Accelerator cannot run the NDK's clang on macOS (processes die with "not active but did
# not get exit message"), so Android compiles run as plain local processes.
source "${0:A:h}/env.sh"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export NDKROOT="${NDKROOT:-$ANDROID_HOME/ndk/27.2.12479018}"
export NDK_ROOT="$NDKROOT"
export JAVA_HOME="${JAVA_HOME:-$(/usr/libexec/java_home -v 17 2>/dev/null)}"
CONFIG="${DD_ANDROID_CONFIG:-Development}"
OUT="$DD_ROOT/Packaged/Android"
LOG="$DD_ROOT/Saved/Logs/Package_Android.log"

if [[ ! -d "$UE_ROOT/Engine/Build/Android/Java" ]]; then
	echo "ERROR: Android platform support is not installed in UE 5.8 (Epic Games Launcher > Library > 5.8 > Options > Android)."
	exit 2
fi
[[ -d "$NDKROOT" ]] || { echo "ERROR: NDK r27c missing: $NDKROOT (sdkmanager \"ndk;27.2.12479018\")"; exit 2; }
[[ -d "$ANDROID_HOME/platforms/android-36" ]] || { echo "ERROR: Android SDK platform android-36 missing in $ANDROID_HOME"; exit 2; }
[[ -n "$JAVA_HOME" ]] || { echo "ERROR: JDK 17 not found"; exit 2; }

mkdir -p "$OUT" "$DD_ROOT/Saved/Logs"
echo "== Build, cook (ASTC + ETC2), package ($CONFIG) for Android"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
	-project="$DD_PROJECT" -platform=Android -cookflavor=Multi -clientconfig="$CONFIG" \
	-build -cook -stage -package -pak -archive -archivedirectory="$OUT" \
	-ubtargs="-NoUBA" -nop4 -utf8output -unattended > "$LOG" 2>&1
CODE=$?
grep -E "BUILD SUCCESSFUL|BUILD FAILED|error:|Error:|ERROR:|AutomationTool exiting" "$LOG" | tail -15
[[ $CODE -ne 0 ]] && { echo "Packaging failed (exit $CODE). Log: $LOG"; exit $CODE; }

# The newest .apk UE produced (archive folder first, then Binaries/Android).
APK=""
for f in "$OUT"/**/*.apk(N.om) "$DD_ROOT/Binaries/Android"/*.apk(N.om); do
	[[ "$f" != "$OUT/DriftDash.apk" ]] && { APK="$f"; break; }
done
[[ -z "$APK" ]] && { echo "No .apk found after packaging (see $LOG)"; exit 1; }
cp "$APK" "$OUT/DriftDash.apk"
echo "APK: $OUT/DriftDash.apk ($(du -h "$OUT/DriftDash.apk" | cut -f1))"

if [[ "$1" == "--install" ]]; then
	ADB="$ANDROID_HOME/platform-tools/adb"
	if [[ -z "$("$ADB" devices | sed -n '2,$p' | grep -w device)" ]]; then
		echo "No Android phone connected (plug it in by USB and allow USB debugging)."
		exit 2
	fi
	"$ADB" install -r "$OUT/DriftDash.apk"
fi
