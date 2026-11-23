// DriftDash - arcade drift model: a pure 2D step (no world), used by UDriftCarMovement and unit-tested directly.
//
// The car keeps a world-space velocity and a heading (yaw). Steering turns the heading; lateral grip pulls the
// velocity back under the heading, so with low grip the heading runs ahead of the velocity: that difference is the
// slip angle, i.e. the drift. Auto-throttle drives the forward speed toward a cruise speed. The handbrake lowers grip
// and boosts the yaw rate; releasing it after a drift fires a short nitro.

#pragma once

#include "CoreMinimal.h"
#include "DriftCarModel.generated.h"

/** One frame of player intent, already resolved from touch / mouse / keys (DriftInput::Resolve). */
USTRUCT(BlueprintType)
struct DRIFTDASH_API FDriftInputFrame
{
	GENERATED_BODY()

	/** -1 = full left, +1 = full right. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float Steer = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	bool bHandbrake = false;
};

USTRUCT(BlueprintType)
struct DRIFTDASH_API FDriftCarParams
{
	GENERATED_BODY()

	/** Auto-throttle target forward speed, cm/s (2300 = 83 km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive")
	float CruiseSpeed = 2300.f;

	/** Auto-throttle acceleration, cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive")
	float Acceleration = 1500.f;

	/** Deceleration when above the target speed (after a nitro), cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive")
	float OverspeedDrag = 700.f;

	/** Total speed never exceeds the target speed times this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive")
	float MaxSpeedShare = 1.15f;

	/** Yaw rate at full steer and full speed, deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steer")
	float SteerYawRate = 125.f;

	/** How fast the yaw rate follows the input, 1/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steer")
	float SteerResponse = 8.f;

	/** Share of the steering left at a standstill (so the car can always turn away from a wall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steer")
	float MinSteerAuthority = 0.4f;

	/** Lateral grip when not drifting, 1/s: lateral speed decays as exp(-Grip * t). Higher = more planted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float Grip = 14.f;

	/** Grip while the car is drifting with the handbrake released: the slide carries on while steering is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float DriftGrip = 4.f;

	/** Grip with the handbrake held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float HandbrakeGrip = 3.4f;

	/** Share of the speed lost to lateral friction that is kept, redirected forward (arcade: corners keep speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float GripSpeedKeep = 0.8f;

	/** Steering yaw rate multiplier with the handbrake held (the bigger drift angle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handbrake")
	float HandbrakeYawScale = 1.5f;

	/** Forward deceleration with the handbrake held, cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handbrake")
	float HandbrakeDrag = 500.f;

	/** The handbrake never slows the forward speed below this share of the cruise speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handbrake")
	float HandbrakeMinSpeedShare = 0.5f;

	/** Slip angle above which the car counts as drifting, deg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drift")
	float DriftSlipAngle = 15.f;

	/** Minimum speed to count as drifting, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drift")
	float DriftMinSpeed = 700.f;

	/** Drift time within one handbrake hold needed for a nitro on release, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nitro")
	float NitroMinDrift = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nitro")
	float NitroDuration = 1.1f;

	/** Target speed bonus while the nitro burns, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nitro")
	float NitroSpeedBonus = 900.f;

	/** Instant forward kick when the nitro fires, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nitro")
	float NitroKick = 500.f;
};

USTRUCT(BlueprintType)
struct DRIFTDASH_API FDriftCarState
{
	GENERATED_BODY()

	/** World XY velocity, cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	FVector2D Velocity = FVector2D::ZeroVector;

	/** Heading, deg (Unreal yaw: positive turns from +X toward +Y, i.e. to the right). */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float Yaw = 0.f;

	/** deg/s */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float YawRate = 0.f;

	/** Angle between heading and velocity, deg; positive = the car slides toward its right side. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float SlipAngle = 0.f;

	/** Seconds of nitro left. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float NitroTime = 0.f;

	/** Drift time collected during the current handbrake hold. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float HandbrakeDriftTime = 0.f;

	/** Continuous drift time. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	float DriftTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	bool bDrifting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	bool bHandbrakeHeld = false;

	/** True only for the step in which a nitro fired. */
	UPROPERTY(BlueprintReadOnly, Category = "Drift")
	bool bNitroFired = false;

	FVector2D Forward() const;
	FVector2D Right() const;
	float ForwardSpeed() const { return FVector2D::DotProduct(Velocity, Forward()); }
	float LateralSpeed() const { return FVector2D::DotProduct(Velocity, Right()); }
	float Speed() const { return Velocity.Size(); }
};

namespace DriftCarModel
{
	/** Advances the car by Dt seconds. bGrounded = false: no throttle, grip or steering (airborne). */
	DRIFTDASH_API void Step(FDriftCarState& State, const FDriftCarParams& Params, const FDriftInputFrame& Input, bool bGrounded, float Dt);

	/** Clears the state: heading Yaw, moving straight ahead at Speed. */
	DRIFTDASH_API void Reset(FDriftCarState& State, float Yaw, float Speed = 0.f);
}
