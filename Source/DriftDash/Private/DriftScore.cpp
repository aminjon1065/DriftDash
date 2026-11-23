// DriftDash - score and combo.

#include "DriftScore.h"
#include "Engine/World.h"

UDriftScoreSubsystem* UDriftScoreSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDriftScoreSubsystem>() : nullptr;
}

bool UDriftScoreSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UDriftScoreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDriftScoreSubsystem, STATGROUP_Tickables);
}

void UDriftScoreSubsystem::Popup(const FString& Text, const FLinearColor& Color)
{
	if (Popups.Num() >= 4)
	{
		Popups.RemoveAt(0);
	}
	FPopup& P = Popups.AddDefaulted_GetRef();
	P.Text = Text;
	P.Color = Color;
}

void UDriftScoreSubsystem::Bump(int32 Points, const FString& Label, const FLinearColor& Color)
{
	++Combo;
	BestCombo = FMath::Max(BestCombo, Combo);
	ComboTimer = ComboWindow;
	Score += Points * Combo;
	Popup(FString::Printf(TEXT("%s  x%d"), *Label, Combo), Color);
}

void UDriftScoreSubsystem::AddSmash(const FVector& /*Where*/)
{
	++Smashes;
	Bump(100, TEXT("SMASH"), FLinearColor(1.f, 0.62f, 0.2f));
}

void UDriftScoreSubsystem::SetDrifting(bool bNowDrifting, float Dt)
{
	bDrifting = bNowDrifting;
	if (!bDrifting)
	{
		DriftAccum = 0.f;
		return;
	}
	ComboTimer = FMath::Max(ComboTimer, 0.5f);
	DriftAccum += Dt;
	// Points flow while drifting, multiplied by the combo.
	DriftPoints += Dt * 60.f * FMath::Max(1, Combo);
	const int32 Whole = FMath::FloorToInt(DriftPoints);
	Score += Whole;
	DriftPoints -= Whole;
	if (DriftAccum >= DriftStep)
	{
		DriftAccum -= DriftStep;
		Bump(50, TEXT("DRIFT"), FLinearColor(0.45f, 0.85f, 1.f));
	}
}

void UDriftScoreSubsystem::AddNitro()
{
	Popup(TEXT("NITRO!"), FLinearColor(0.4f, 0.75f, 1.f));
}

void UDriftScoreSubsystem::AddWallHit(float ImpactSpeed)
{
	if (ImpactSpeed >= CrashSpeed && Combo > 0)
	{
		Combo = 0;
		ComboTimer = 0.f;
		Popup(TEXT("CRASH"), FLinearColor(1.f, 0.3f, 0.25f));
	}
}

void UDriftScoreSubsystem::ResetRun()
{
	Score = 0;
	Combo = 0;
	BestCombo = 0;
	Smashes = 0;
	ComboTimer = 0.f;
	DriftAccum = 0.f;
	DriftPoints = 0.f;
	bDrifting = false;
	Popups.Reset();
}

void UDriftScoreSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Combo > 0 && !bDrifting)
	{
		ComboTimer -= DeltaTime;
		if (ComboTimer <= 0.f)
		{
			Combo = 0;
			ComboTimer = 0.f;
		}
	}
	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		Popups[i].Age += DeltaTime;
		if (Popups[i].Age > 1.2f)
		{
			Popups.RemoveAt(i);
		}
	}
}
