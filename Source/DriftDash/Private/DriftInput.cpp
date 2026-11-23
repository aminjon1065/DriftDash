// DriftDash - input resolve and HUD layout.

#include "DriftInput.h"
#include "GenericPlatform/GenericApplication.h"

FDriftInputFrame DriftInput::Resolve(TConstArrayView<FVector2D> PointersOldestFirst, float ViewportWidth, bool bHandbrakeKey)
{
	FDriftInputFrame Out;
	const float Half = ViewportWidth * 0.5f;
	bool bLeft = false;
	bool bRight = false;
	float FirstSide = 0.f;
	for (const FVector2D& P : PointersOldestFirst)
	{
		const float Side = P.X < Half ? -1.f : 1.f;
		if (FirstSide == 0.f)
		{
			FirstSide = Side;
		}
		(Side < 0.f ? bLeft : bRight) = true;
	}
	if (bLeft && bRight)
	{
		// Both halves: handbrake, still turning toward the side that was held first.
		Out.Steer = FirstSide;
		Out.bHandbrake = true;
	}
	else
	{
		Out.Steer = bLeft ? -1.f : (bRight ? 1.f : 0.f);
	}
	Out.bHandbrake = Out.bHandbrake || bHandbrakeKey;
	return Out;
}

FDriftHudLayout FDriftHudLayout::Make(const FVector2D& InViewport, const FVector4& SafeInsets)
{
	FDriftHudLayout L;
	L.Viewport = FVector2D(FMath::Max(1.0, InViewport.X), FMath::Max(1.0, InViewport.Y));
	L.Safe = SafeInsets;
	L.Scale = FMath::Clamp(static_cast<float>(FMath::Min(L.Viewport.X, L.Viewport.Y)) / 500.f, 0.5f, 4.f);
	return L;
}

FVector4 FDriftHudLayout::CurrentSafeInsets(const FVector2D& InViewport)
{
	static FVector2D CachedFor = FVector2D::ZeroVector;
	static FVector4 Cached = FVector4(0.f, 0.f, 0.f, 0.f);
	if (CachedFor != InViewport)
	{
		CachedFor = InViewport;
		FDisplayMetrics Metrics;
		FDisplayMetrics::RebuildDisplayMetrics(Metrics);
		const double Height = Metrics.PrimaryDisplayHeight > 0 ? Metrics.PrimaryDisplayHeight : InViewport.Y;
		const double ToViewport = Height > 0.0 ? InViewport.Y / Height : 1.0;
		Cached = Metrics.TitleSafePaddingSize * ToViewport;
	}
	return Cached;
}

FBox2D FDriftHudLayout::RestartButton() const
{
	const float Size = 60.f * Scale;
	const float Margin = 14.f * Scale;
	const float Right = Viewport.X - Safe.Z - Margin;
	const float Top = Safe.Y + Margin;
	return FBox2D(FVector2D(Right - Size, Top), FVector2D(Right, Top + Size));
}

bool FDriftHudLayout::HitsRestart(const FVector2D& Point) const
{
	return RestartButton().ExpandBy(14.f * Scale).IsInside(Point);
}
