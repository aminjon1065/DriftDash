// DriftDash - score and combo.
//
// Every smashed prop and every DriftStep seconds of continuous drift add one to the combo; points are multiplied by
// the combo. The combo ends ComboWindow seconds after the last event (it never runs out while drifting) or at once
// on a hard wall hit.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DriftScore.generated.h"

UCLASS()
class DRIFTDASH_API UDriftScoreSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDriftScoreSubsystem* Get(const UObject* WorldContext);

	static constexpr float ComboWindow = 2.5f;
	static constexpr float DriftStep = 0.6f;
	/** Wall impacts faster than this (cm/s, into the wall) end the combo. */
	static constexpr float CrashSpeed = 900.f;

	struct FPopup
	{
		FString Text;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.f;
	};

	void AddSmash(const FVector& Where);
	/** Called by the car every frame. */
	void SetDrifting(bool bDrifting, float Dt);
	void AddNitro();
	void AddWallHit(float ImpactSpeed);
	void ResetRun();

	int32 GetScore() const { return Score; }
	int32 GetCombo() const { return Combo; }
	int32 GetBestCombo() const { return BestCombo; }
	int32 GetSmashCount() const { return Smashes; }
	bool IsDrifting() const { return bDrifting; }
	/** 1 = full combo window left, 0 = expired. */
	float GetComboFraction() const { return Combo > 0 ? FMath::Clamp(ComboTimer / ComboWindow, 0.f, 1.f) : 0.f; }
	const TArray<FPopup>& GetPopups() const { return Popups; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Bump(int32 Points, const FString& Label, const FLinearColor& Color);
	void Popup(const FString& Text, const FLinearColor& Color);

	int32 Score = 0;
	int32 Combo = 0;
	int32 BestCombo = 0;
	int32 Smashes = 0;
	float ComboTimer = 0.f;
	float DriftAccum = 0.f;
	float DriftPoints = 0.f;
	bool bDrifting = false;
	TArray<FPopup> Popups;
};
