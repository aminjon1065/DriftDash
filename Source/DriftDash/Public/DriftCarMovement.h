// DriftDash - moves the car through the world with FDriftCarModel (no ChaosVehicles, no rigid-body car).
//
// Horizontal: the model's velocity, applied as a sweep of the car's box; walls (WorldStatic) stop and deflect it
// and the car slides along them. Vertical: ray casts against DriftGround (road, sidewalks, ramps): one under the
// centre sets the ride height, four under the wheels tilt the body to the ground. Leaving a ramp's top keeps the
// climbing speed, so the car flies; gravity brings it back.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "DriftCarModel.h"
#include "DriftCarMovement.generated.h"

UCLASS()
class DRIFTDASH_API UDriftCarMovement : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UDriftCarMovement();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drift")
	FDriftCarParams Params;

	/** Height of the actor origin over the ground, cm. */
	UPROPERTY(EditAnywhere, Category = "Drift|Ground")
	float RideHeight = 75.f;

	/** Ground up to this far above the ride height is stepped onto (kerbs, ramp starts). */
	UPROPERTY(EditAnywhere, Category = "Drift|Ground")
	float StepUp = 50.f;

	/** While grounded, ground up to this far below is followed (ramp ends launch the car instead). */
	UPROPERTY(EditAnywhere, Category = "Drift|Ground")
	float SnapDown = 60.f;

	/** cm/s^2; twice the engine's gravity for snappy arcade jumps. */
	UPROPERTY(EditAnywhere, Category = "Drift|Ground")
	float Gravity = -1960.f;

	/** Wheel probe offsets: half wheelbase and half track, cm. */
	UPROPERTY(EditAnywhere, Category = "Drift|Ground")
	FVector2D WheelOffset = FVector2D(135.f, 80.f);

	/** Share of the into-wall speed bounced back. */
	UPROPERTY(EditAnywhere, Category = "Drift|Walls")
	float WallBounce = 0.25f;

	void SetDriftInput(const FDriftInputFrame& In) { Input = In; }
	const FDriftInputFrame& GetDriftInput() const { return Input; }

	/** Puts the model at rest (or at Speed) heading Yaw; ground state is found on the next tick. */
	void ResetState(float Yaw, float Speed = 0.f);

	const FDriftCarState& GetCarState() const { return State; }
	bool IsGrounded() const { return bGrounded; }
	float GetAirTime() const { return AirTime; }
	float GetVerticalSpeed() const { return VerticalSpeed; }
	FVector GetGroundNormal() const { return GroundNormal; }
	FVector GetVelocity3D() const { return FVector(State.Velocity.X, State.Velocity.Y, VerticalSpeed); }

	/** Strongest wall impact (into-wall speed, cm/s) since the last call, 0 if none. */
	float ConsumeWallImpact(FVector& OutPoint);
	/** Landing speed (cm/s, downward) since the last call, 0 if none. */
	float ConsumeLanding();
	/** Nitro fired since the last call. */
	bool ConsumeNitro();

	/** Scales the horizontal speed (smashing a prop costs a little). */
	void ApplySpeedLoss(float Fraction);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void StepOnce(float Dt);
	bool ProbeGround(const FVector& At, FHitResult& OutHit) const;
	void UpdateGround(float Dt);
	void HandleWallHit(const FHitResult& Hit);
	FQuat BodyRotation() const;

	FDriftCarState State;
	FDriftInputFrame Input;
	bool bGrounded = false;
	float VerticalSpeed = 0.f;
	float AirTime = 0.f;
	FVector GroundNormal = FVector::UpVector;
	FVector GroundNormalTarget = FVector::UpVector;
	/** Normal of the surface under the centre (unsmoothed), for the vertical speed. */
	FVector SurfaceNormal = FVector::UpVector;
	float PendingImpact = 0.f;
	FVector PendingImpactPoint = FVector::ZeroVector;
	float PendingLanding = 0.f;
	bool bPendingNitro = false;
};
