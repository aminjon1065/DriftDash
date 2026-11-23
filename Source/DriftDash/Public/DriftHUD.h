// DriftDash - minimal HUD drawn on the canvas (no widgets, no assets): score, combo with its timer bar, popups,
// speed, a short control hint and the restart button (top right, inside the safe area).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DriftHUD.generated.h"

UCLASS()
class DRIFTDASH_API ADriftHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Text(const FString& S, float X, float Y, float Size, const FLinearColor& Color, bool bCentre = false);
	void DrawRestartButton(const FBox2D& Box, float Scale);
	void Disc(const FVector2D& Centre, float Radius, const FLinearColor& Color);
	float HintTime = 0.f;
};
