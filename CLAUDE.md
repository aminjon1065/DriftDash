# DriftDash

Mobile (portrait, iOS first) arcade drift-chase prototype in Unreal Engine 5.8.4 (`/Users/Shared/Epic Games/UE_5.8`).
Gameplay is C++ (`Source/DriftDash`); the map `L_City` is empty and the city is built by code at start.
The pipeline (Scripts/, Config/) was adapted from the sibling project `../StackAndEscape`; no game code was taken from it.

## Rules
- Never claim something compiled, ran, packaged or passed unless it happened in this session.
- iOS signing: only the owner's personal team `4RPQ3AXM9Q`, automatic signing (Config/DefaultEngine.ini). Bundle id `com.driftdash.DriftDash`.
- Do not install or launch on a phone (iOS or Android) without the owner's explicit OK each time. `Scripts/deploy_ios.sh --build-only` is always fine.
- No ChaosVehicles: the car is `FDriftCarModel` (pure, unit-tested) moved by `UDriftCarMovement` (sweeps + ray casts).

## Commands
- `Scripts/build_editor.sh` (after any C++ change; `-NoHotReload`, safe with an editor open)
- `Scripts/run_tests.sh [Filter]` (all `DriftDash.*` tests must pass; headless, -nullrhi)
- `Scripts/play_mac.sh [WxH] [args]` (phone-shaped window, editor binary in -game mode). Dev checks without touching
  the controls: `Scripts/play_mac.sh 500x960 -DDAutopilot -DDShots=1.5,4,7 -DDQuit=8` (scripted ramp / smash / drift /
  nitro input, screenshots to `Saved/Screenshots`, `DD_POS` lines every second); the log of such runs is
  `~/Library/Logs/DriftDash/DriftDash.log` (not `Saved/Logs`).
- `Scripts/run_editor_python.sh Scripts/build_content.py` (M_Solid, M_Glow, L_City; `DD_FORCE=1` rebuilds them)
- `Scripts/deploy_ios.sh --build-only` (build + cook + package for iOS, nothing goes to the phone); without the flag it also runs `install_ios.sh`
- `Scripts/package_android.sh [--install]` (arm64 .apk, NDK r27c, `-NoUBA`)

## Code map (Source/DriftDash)
- `DriftCarModel` — the drift physics: auto-throttle, steer yaw rate, lateral grip (grip / drift / handbrake), slip angle, nitro after a handbrake drift. All tuning in `FDriftCarParams`.
- `DriftCarMovement` — applies the model in the world: box sweep vs WorldStatic walls (slide + nose turned along the wall), ray casts vs `DriftGround` for ride height and tilt, gravity and ramp launches.
- `DriftCarPawn` — body from basic shapes, roll / wheel steer / nitro flames, skid marks + smoke, chase camera (pitch 40, lag, slide offset, vertical FOV kept for any aspect).
- `DriftInput` — `DriftInput::Resolve` (screen halves, keys as pseudo-pointers, both halves = handbrake keeping the first side) and `FDriftHudLayout` (restart button, safe area). `DriftPlayerController` feeds it.
- `DriftCityBuilder` — the "City block": 3x3 road grid, 2 ramps, boundary buildings, lights (sunset sun, sky atmosphere, captured sky light, fog, post process), prop spots.
- `DriftProp` — fence / crate / lamp / hydrant; overlap-only until smashed, then physics debris for 2.5 s (last 0.4 s: frozen + shrinking), FX and combo.
- `DriftFx` — pooled ISM particles (smoke, sparks, water, fountains) and the debris budget (`MaxDebrisParts`).
- `DriftScore` — score, combo (smash +1, every 0.6 s of drift +1, window 2.5 s, hard wall hit ends it), popups.
- `DriftHUD` — canvas HUD; `DriftLook` — basic-shape meshes and cached colour material instances.
- `Private/Tests/DriftTests.cpp` — the automation tests.

## Gotchas
- Materials used on instanced meshes (the whole city, the effects) must have `bUsedWithInstancedStaticMeshes` saved
  (`build_content.py` sets it; `DriftDash.City.Layout` checks it). Without it -game runs and cooked builds silently
  draw the engine's grey grid material (it showed as brown noise on a 230 m slab).
- The engine's player default is `AspectRatio_MaintainYFOV` (the FOV value follows the longer screen side). The car
  camera overrides it to MaintainXFOV and computes the horizontal FOV from `CameraVerticalFov`; without that a
  portrait phone gets a third of the intended view.
- The chase camera sweeps WorldStatic from the car to the eye and pulls in when a wall is between them.
- Sky light: the engine's `/Engine/MapTemplates/Sky/SunsetAmbientCubemap` tinted cool (cooked via
  DirectoriesToAlwaysCook); a captured-scene sky light came out black in -game runs.
- Collision: drivable surfaces are object type `DriftGround` (`ECC_GameTraceChannel1`, Config/DefaultEngine.ini). The car's box ignores them and blocks only WorldStatic; props are WorldDynamic overlap triggers; debris is PhysicsBody and ignores the car and other debris.
- The car box is the upper hull (40-110 cm over the ground): kerbs and the low end of ramps pass under it. Ramp steps stay 25 cm under the ramp slab for that reason.
- Vertical speed comes from the surface normal under the car's centre; wheel probes more than 45 cm off the centre are ignored (they tipped the body nose-down at ramp ends and killed the jump).
- iOS compiles with stricter warnings than Mac (`-Werror`); the Mac editor build also hides missing includes (unity files).
- An Xcode update can remove the Metal Toolchain: `xcodebuild -downloadComponent MetalToolchain`.
- Android: keep `r.Vulkan.UseChunkedPSOCache=0` (Config/Android/AndroidEngine.ini), UE 5.8 asserts at startup without it.
- `Scripts/build_editor.sh` uses `-WaitMutex`: it waits while another project's UBT build runs.
