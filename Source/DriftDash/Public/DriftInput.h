// DriftDash - one control scheme for touch, mouse and keyboard, plus the HUD layout it hit-tests against.
//
//   hold left half  (or A / Left)              -> steer left
//   hold right half (or D / Right)             -> steer right
//   hold both halves (or Space / Shift)        -> handbrake; the steer keeps the side pressed first
//   restart button (top right corner) or R     -> restart

#pragma once

#include "CoreMinimal.h"
#include "DriftCarModel.h"

namespace DriftInput
{
	/**
	 * Resolves held pointers into steer + handbrake.
	 * @param PointersOldestFirst  screen positions (pixels) of every held touch / mouse button / steer key, oldest press
	 *                             first. Steer keys come in as pseudo pointers in their screen half.
	 * @param ViewportWidth        pixels
	 * @param bHandbrakeKey        Space or Shift held
	 */
	DRIFTDASH_API FDriftInputFrame Resolve(TConstArrayView<FVector2D> PointersOldestFirst, float ViewportWidth, bool bHandbrakeKey);
}

/** Where the HUD puts its pieces, in viewport pixels. Shared by the HUD (drawing) and the controller (hit tests). */
struct DRIFTDASH_API FDriftHudLayout
{
	FVector2D Viewport = FVector2D(500.f, 960.f);
	/** Safe-area insets in viewport pixels: left, top, right, bottom. */
	FVector4 Safe = FVector4(0.f, 0.f, 0.f, 0.f);
	/** 1 at a 500 px wide portrait viewport. */
	float Scale = 1.f;

	static FDriftHudLayout Make(const FVector2D& Viewport, const FVector4& SafeInsets);

	/** Safe-area insets of the current display in viewport pixels (cached; zero on desktop). */
	static FVector4 CurrentSafeInsets(const FVector2D& Viewport);

	/** The visible restart button. */
	FBox2D RestartButton() const;

	/** Touches that start here restart the run (the button plus a finger-sized margin) and do not steer. */
	bool HitsRestart(const FVector2D& Point) const;
};
