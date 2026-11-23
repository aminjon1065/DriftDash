// DriftDash - HUD.

#include "DriftHUD.h"
#include "DriftCarMovement.h"
#include "DriftCarPawn.h"
#include "DriftInput.h"
#include "DriftScore.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GlobalRenderResources.h"

void ADriftHUD::Text(const FString& S, float X, float Y, float Size, const FLinearColor& Color, bool bCentre)
{
	UFont* Font = GEngine->GetLargeFont();
	// The large engine font is about 24 px tall.
	const float Scale = Size / 24.f;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(S), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.bCentreX = bCentre;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.65f), FVector2D(1.5f, 1.5f) * Scale);
	Canvas->DrawItem(Item);
}

void ADriftHUD::Disc(const FVector2D& Centre, float Radius, const FLinearColor& Color)
{
	TArray<FCanvasUVTri> Tris;
	const int32 N = 28;
	for (int32 i = 0; i < N; ++i)
	{
		const float A0 = 2.f * PI * i / N;
		const float A1 = 2.f * PI * (i + 1) / N;
		FCanvasUVTri T;
		T.V0_Pos = Centre;
		T.V1_Pos = Centre + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius;
		T.V2_Pos = Centre + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius;
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
		Tris.Add(T);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ADriftHUD::DrawRestartButton(const FBox2D& Box, float Scale)
{
	const FVector2D C = Box.GetCenter();
	const float R = Box.GetSize().X * 0.5f;
	Disc(C, R, FLinearColor(0.f, 0.f, 0.f, 0.45f));

	// Circular arrow: an open arc and an arrow head at its end.
	const float ArcR = R * 0.5f;
	const int32 N = 14;
	const float From = FMath::DegreesToRadians(-60.f);
	const float To = FMath::DegreesToRadians(230.f);
	FVector2D Prev = C + FVector2D(FMath::Cos(From), FMath::Sin(From)) * ArcR;
	for (int32 i = 1; i <= N; ++i)
	{
		const float A = FMath::Lerp(From, To, static_cast<float>(i) / N);
		const FVector2D P = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * ArcR;
		FCanvasLineItem Line(Prev, P);
		Line.LineThickness = 3.5f * Scale;
		Line.SetColor(FLinearColor::White);
		Canvas->DrawItem(Line);
		Prev = P;
	}
	const FVector2D Tip = C + FVector2D(FMath::Cos(From), FMath::Sin(From)) * ArcR;
	const FVector2D Tangent(-FMath::Sin(From), FMath::Cos(From));
	const FVector2D Normal(FMath::Cos(From), FMath::Sin(From));
	const float H = 9.f * Scale;
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri T;
	T.V0_Pos = Tip - Tangent * H;
	T.V1_Pos = Tip + Normal * H * 0.8f + Tangent * H * 0.3f;
	T.V2_Pos = Tip - Normal * H * 0.8f + Tangent * H * 0.3f;
	T.V0_Color = T.V1_Color = T.V2_Color = FLinearColor::White;
	Tris.Add(T);
	FCanvasTriangleItem Head(Tris, GWhiteTexture);
	Canvas->DrawItem(Head);
}

void ADriftHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const FVector2D Size(Canvas->SizeX, Canvas->SizeY);
	const FDriftHudLayout L = FDriftHudLayout::Make(Size, FDriftHudLayout::CurrentSafeInsets(Size));
	const float S = L.Scale;
	const float Left = static_cast<float>(L.Safe.X) + 18.f * S;
	const float Top = static_cast<float>(L.Safe.Y) + 16.f * S;
	HintTime += GetWorld()->GetDeltaSeconds();

	UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(this);
	if (Score)
	{
		Text(FString::Printf(TEXT("%d"), Score->GetScore()), Left, Top, 34.f * S, FLinearColor::White);
		const int32 Combo = Score->GetCombo();
		if (Combo > 0)
		{
			const float Y = Top + 44.f * S;
			const FLinearColor Hot = Score->IsDrifting() ? FLinearColor(0.45f, 0.85f, 1.f) : FLinearColor(1.f, 0.65f, 0.2f);
			Text(FString::Printf(TEXT("COMBO x%d"), Combo), Left, Y, 24.f * S, Hot);
			const float BarW = 150.f * S;
			const float BarH = 6.f * S;
			FCanvasTileItem Back(FVector2D(Left, Y + 32.f * S), FVector2D(BarW, BarH), FLinearColor(0.f, 0.f, 0.f, 0.4f));
			Back.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Back);
			FCanvasTileItem Fill(FVector2D(Left, Y + 32.f * S), FVector2D(BarW * Score->GetComboFraction(), BarH), Hot);
			Fill.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Fill);
		}

		// Event popups rise and fade in the upper middle of the screen.
		const TArray<UDriftScoreSubsystem::FPopup>& Popups = Score->GetPopups();
		for (int32 i = 0; i < Popups.Num(); ++i)
		{
			const UDriftScoreSubsystem::FPopup& P = Popups[i];
			const float A = FMath::Clamp(1.f - P.Age / 1.2f, 0.f, 1.f);
			FLinearColor C = P.Color;
			C.A = A;
			const float Y = Size.Y * 0.28f - (Popups.Num() - 1 - i) * 34.f * S - P.Age * 40.f * S;
			Text(P.Text, Size.X * 0.5f, Y, 26.f * S, C, true);
		}
	}

	// Speed, bottom left.
	if (const ADriftCarPawn* Car = Cast<ADriftCarPawn>(GetOwningPawn()))
	{
		const float Kmh = Car->GetDriftMovement()->GetCarState().Speed() * 0.036f;
		Text(FString::Printf(TEXT("%d km/h"), FMath::RoundToInt(Kmh)), Left, Size.Y - static_cast<float>(L.Safe.W) - 46.f * S, 22.f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f));
	}

	// Control hint for the first seconds.
	if (HintTime < 6.f)
	{
		const float A = FMath::Clamp(6.f - HintTime, 0.f, 1.f);
		const float Y = Size.Y - static_cast<float>(L.Safe.W) - 120.f * S;
		Text(TEXT("HOLD LEFT / RIGHT TO STEER"), Size.X * 0.5f, Y, 18.f * S, FLinearColor(1.f, 1.f, 1.f, A), true);
		Text(TEXT("HOLD BOTH = DRIFT, RELEASE = NITRO"), Size.X * 0.5f, Y + 26.f * S, 18.f * S, FLinearColor(1.f, 0.85f, 0.5f, A), true);
	}

	DrawRestartButton(L.RestartButton(), S);
}
