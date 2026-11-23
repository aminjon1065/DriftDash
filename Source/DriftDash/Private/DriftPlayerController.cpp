// DriftDash - input.

#include "DriftPlayerController.h"
#include "DriftCarPawn.h"
#include "DriftGameMode.h"
#include "DriftCarMovement.h"
#include "DriftScore.h"
#include "DriftTypes.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ADriftPlayerController::ADriftPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableTouchEvents = false;
}

void ADriftPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(false);
	SetInputMode(Mode);

	const TCHAR* Cmd = FCommandLine::Get();
	bAutopilot = FParse::Param(Cmd, TEXT("DDAutopilot"));
	FString Shots;
	if (FParse::Value(Cmd, TEXT("-DDShots="), Shots, false))
	{
		TArray<FString> Parts;
		Shots.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			ShotTimes.Add(FCString::Atof(*Part));
		}
		ShotTimes.Sort();
	}
	FParse::Value(Cmd, TEXT("-DDQuit="), QuitAfter);
	if (bAutopilot || ShotTimes.Num() > 0 || QuitAfter > 0.f)
	{
		UE_LOG(LogDrift, Display, TEXT("DD_DEV autopilot %d, %d shots, quit after %.1f s"), bAutopilot ? 1 : 0, ShotTimes.Num(), QuitAfter);
	}
}

FDriftInputFrame ADriftPlayerController::AutopilotInput(float T) const
{
	// From the start: the first ramp, a nudge into the crates, a handbrake drift right at the junction (nitro on
	// release), up the middle road, then alternating drifts.
	FDriftInputFrame In;
	if (T >= 2.95f && T < 3.2f)
	{
		In.Steer = 1.f;
	}
	else if (T >= 3.2f && T < 3.45f)
	{
		In.Steer = -1.f;
	}
	else if (T >= 3.7f && T < 4.3f)
	{
		In.Steer = 1.f;
		In.bHandbrake = true;
	}
	else if (T >= 6.f)
	{
		const float Phase = FMath::Fmod(T - 6.f, 3.f);
		In.Steer = Phase < 1.5f ? -1.f : 1.f;
		In.bHandbrake = FMath::Fmod(Phase, 1.5f) < 0.7f;
	}
	return In;
}

void ADriftPlayerController::TickDevChecks(float DeltaTime)
{
	const float Before = PlayTime;
	PlayTime += DeltaTime;
	if (FMath::FloorToInt(Before) != FMath::FloorToInt(PlayTime) && (bAutopilot || ShotTimes.Num() > 0))
	{
		const ADriftCarPawn* Car = Cast<ADriftCarPawn>(GetPawn());
		const UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(this);
		UE_LOG(LogDrift, Display, TEXT("DD_POS t %.1f car %s speed %.0f slip %.1f air %d score %d combo %d smashed %d fps %.0f"), PlayTime,
			Car ? *Car->GetActorLocation().ToCompactString() : TEXT("-"),
			Car ? Car->GetDriftMovement()->GetCarState().Speed() : 0.f,
			Car ? Car->GetDriftMovement()->GetCarState().SlipAngle : 0.f,
			Car && !Car->GetDriftMovement()->IsGrounded() ? 1 : 0,
			Score ? Score->GetScore() : 0, Score ? Score->GetCombo() : 0, Score ? Score->GetSmashCount() : 0,
			DeltaTime > 0.f ? 1.f / DeltaTime : 0.f);
	}
	if (NextShot < ShotTimes.Num() && PlayTime >= ShotTimes[NextShot])
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("DD_%02d_%04.1fs.png"), NextShot, PlayTime));
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		UE_LOG(LogDrift, Display, TEXT("DD_SHOT %s"), *Path);
		++NextShot;
	}
	if (QuitAfter > 0.f && PlayTime >= QuitAfter)
	{
		QuitAfter = -1.f;
		UE_LOG(LogDrift, Display, TEXT("DD_DEV quit"));
		ConsoleCommand(TEXT("quit"));
	}
}

FDriftHudLayout ADriftPlayerController::GetHudLayout() const
{
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	const FVector2D Size(FMath::Max(W, 1), FMath::Max(H, 1));
	return FDriftHudLayout::Make(Size, FDriftHudLayout::CurrentSafeInsets(Size));
}

void ADriftPlayerController::RequestRestart()
{
	if (ADriftGameMode* Mode = GetWorld()->GetAuthGameMode<ADriftGameMode>())
	{
		Mode->RestartRun();
	}
}

void ADriftPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	int32 W = 0;
	int32 H = 0;
	GetViewportSize(W, H);
	if (W <= 0 || H <= 0)
	{
		return;
	}
	const FDriftHudLayout Layout = GetHudLayout();

	// Everything held this frame.
	TMap<int32, FVector2D> Down;
	for (int32 i = ETouchIndex::Touch1; i <= ETouchIndex::Touch10; ++i)
	{
		float X = 0.f;
		float Y = 0.f;
		bool bPressed = false;
		GetInputTouchState(static_cast<ETouchIndex::Type>(i), X, Y, bPressed);
		if (bPressed)
		{
			Down.Add(i, FVector2D(X, Y));
		}
	}
	float MX = 0.f;
	float MY = 0.f;
	if (IsInputKeyDown(EKeys::LeftMouseButton) && GetMousePosition(MX, MY))
	{
		Down.Add(MouseId, FVector2D(MX, MY));
	}
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left))
	{
		Down.Add(KeyLeftId, FVector2D(W * 0.25f, H * 0.5f));
	}
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right))
	{
		Down.Add(KeyRightId, FVector2D(W * 0.75f, H * 0.5f));
	}

	// Keep the press order; new presses on the restart button restart and are not steering.
	PressOrder.RemoveAll([&Down](int32 Id) { return !Down.Contains(Id); });
	for (auto It = ButtonPointers.CreateIterator(); It; ++It)
	{
		if (!Down.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
	bool bRestart = WasInputKeyJustPressed(EKeys::R);
	for (const TPair<int32, FVector2D>& P : Down)
	{
		if (!PressOrder.Contains(P.Key))
		{
			PressOrder.Add(P.Key);
			if (P.Key < KeyLeftId && Layout.HitsRestart(P.Value))
			{
				ButtonPointers.Add(P.Key);
				bRestart = true;
			}
		}
	}

	TArray<FVector2D> Pointers;
	for (const int32 Id : PressOrder)
	{
		if (!ButtonPointers.Contains(Id))
		{
			Pointers.Add(Down[Id]);
		}
	}
	const bool bHandbrakeKey = IsInputKeyDown(EKeys::SpaceBar) || IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	Current = DriftInput::Resolve(Pointers, static_cast<float>(W), bHandbrakeKey);
	TickDevChecks(DeltaTime);
	if (bAutopilot)
	{
		Current = AutopilotInput(PlayTime);
	}

	if (bRestart)
	{
		RequestRestart();
	}
	if (ADriftCarPawn* Car = Cast<ADriftCarPawn>(GetPawn()))
	{
		Car->SetDriftInput(Current);
	}
}
