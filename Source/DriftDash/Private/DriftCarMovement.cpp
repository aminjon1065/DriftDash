// DriftDash - car movement in the world.

#include "DriftCarMovement.h"
#include "DriftTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

UDriftCarMovement::UDriftCarMovement()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bConstrainToPlane = false;
}

void UDriftCarMovement::ResetState(float Yaw, float Speed)
{
	DriftCarModel::Reset(State, Yaw, Speed);
	Input = FDriftInputFrame();
	bGrounded = false;
	VerticalSpeed = 0.f;
	AirTime = 0.f;
	GroundNormal = FVector::UpVector;
	GroundNormalTarget = FVector::UpVector;
	SurfaceNormal = FVector::UpVector;
	PendingImpact = 0.f;
	PendingLanding = 0.f;
	bPendingNitro = false;
	Velocity = GetVelocity3D();
}

float UDriftCarMovement::ConsumeWallImpact(FVector& OutPoint)
{
	const float Out = PendingImpact;
	OutPoint = PendingImpactPoint;
	PendingImpact = 0.f;
	return Out;
}

float UDriftCarMovement::ConsumeLanding()
{
	const float Out = PendingLanding;
	PendingLanding = 0.f;
	return Out;
}

bool UDriftCarMovement::ConsumeNitro()
{
	const bool bOut = bPendingNitro;
	bPendingNitro = false;
	return bOut;
}

void UDriftCarMovement::ApplySpeedLoss(float Fraction)
{
	State.Velocity *= FMath::Clamp(1.f - Fraction, 0.f, 1.f);
}

void UDriftCarMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!UpdatedComponent || ShouldSkipUpdate(DeltaTime) || DeltaTime <= 0.f)
	{
		return;
	}
	// Substeps of at most 1/60 s keep the slip, the sweeps and the jumps stable when the frame rate drops.
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt * 60.f - 0.01f), 1, 3);
	for (int32 i = 0; i < Steps; ++i)
	{
		StepOnce(Dt / Steps);
	}
	Velocity = GetVelocity3D();
	UpdateComponentVelocity();
}

FQuat UDriftCarMovement::BodyRotation() const
{
	const FVector2D F2 = State.Forward();
	const FVector Up = GroundNormal.GetSafeNormal();
	FVector Fwd(F2.X, F2.Y, 0.f);
	Fwd = (Fwd - Up * FVector::DotProduct(Fwd, Up)).GetSafeNormal();
	if (Fwd.IsNearlyZero())
	{
		Fwd = FVector(F2.X, F2.Y, 0.f);
	}
	return FRotationMatrix::MakeFromXZ(Fwd, Up).ToQuat();
}

void UDriftCarMovement::StepOnce(float Dt)
{
	DriftCarModel::Step(State, Params, Input, bGrounded, Dt);
	bPendingNitro = bPendingNitro || State.bNitroFired;

	if (bGrounded)
	{
		// Stay on the surface under the centre: climbing a ramp gives an upward speed that the car keeps when it
		// leaves the top (the smoothed, wheel-plane normal is only for the body's tilt).
		const FVector N = SurfaceNormal;
		VerticalSpeed = N.Z > 0.3f ? -(State.Velocity.X * N.X + State.Velocity.Y * N.Y) / N.Z : 0.f;
	}
	else
	{
		VerticalSpeed += Gravity * Dt;
		AirTime += Dt;
	}

	const FVector Delta(State.Velocity.X * Dt, State.Velocity.Y * Dt, VerticalSpeed * Dt);
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, BodyRotation(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		HandleWallHit(Hit);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}
	UpdateGround(Dt);
}

bool UDriftCarMovement::ProbeGround(const FVector& At, FHitResult& OutHit) const
{
	const FVector Start = At + FVector(0.f, 0.f, StepUp);
	const FVector End = At - FVector(0.f, 0.f, RideHeight + SnapDown + 400.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DriftGround), false, GetOwner());
	// Faces steeper than ~53 degrees are not ground (a ramp's end face would otherwise eat the take-off speed).
	return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, FCollisionObjectQueryParams(ECC_DriftGround), Query)
		&& OutHit.ImpactNormal.Z >= 0.6f;
}

void UDriftCarMovement::UpdateGround(float Dt)
{
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	FHitResult Center;
	const bool bHit = ProbeGround(Loc, Center);
	const float Gap = bHit ? static_cast<float>(Loc.Z - Center.ImpactPoint.Z) : TNumericLimits<float>::Max();
	const bool bSnap = bHit && (bGrounded
		? Gap <= RideHeight + SnapDown
		: (Gap <= RideHeight + 2.f && VerticalSpeed <= 0.f));

	if (bSnap)
	{
		if (!bGrounded)
		{
			PendingLanding = FMath::Max(PendingLanding, -VerticalSpeed);
		}
		bGrounded = true;
		AirTime = 0.f;
		UpdatedComponent->SetWorldLocation(FVector(Loc.X, Loc.Y, Center.ImpactPoint.Z + RideHeight));
		SurfaceNormal = Center.ImpactNormal;

		// Tilt to the plane under the four wheels (falls back to the centre's normal).
		GroundNormalTarget = Center.ImpactNormal;
		const FVector2D F2 = State.Forward();
		const FVector2D R2 = State.Right();
		const FVector Fwd(F2.X, F2.Y, 0.f);
		const FVector Rt(R2.X, R2.Y, 0.f);
		FVector Wheels[4];
		int32 Hits = 0;
		const float Signs[4][2] = { { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, -1.f }, { -1.f, 1.f } }; // FL FR RL RR
		for (int32 i = 0; i < 4; ++i)
		{
			FHitResult W;
			const FVector At = Loc + Fwd * (Signs[i][0] * WheelOffset.X) + Rt * (Signs[i][1] * WheelOffset.Y);
			// Wheels over a drop (a ramp's end) do not count: they would tip the plane nose-down before take-off.
			if (ProbeGround(At, W) && FMath::Abs(W.ImpactPoint.Z - Center.ImpactPoint.Z) < 45.f)
			{
				Wheels[i] = W.ImpactPoint;
				++Hits;
			}
		}
		if (Hits == 4)
		{
			FVector N = FVector::CrossProduct(Wheels[0] - Wheels[3], Wheels[1] - Wheels[2]).GetSafeNormal();
			if (N.Z < 0.f)
			{
				N = -N;
			}
			if (N.Z > 0.5f)
			{
				GroundNormalTarget = N;
			}
		}
	}
	else
	{
		bGrounded = false;
		SurfaceNormal = FVector::UpVector;
		GroundNormalTarget = FVector::UpVector;
	}
	const float Rate = bGrounded ? 14.f : 2.5f;
	GroundNormal = FMath::Lerp(GroundNormal, GroundNormalTarget, 1.f - FMath::Exp(-Rate * Dt)).GetSafeNormal();
}

void UDriftCarMovement::HandleWallHit(const FHitResult& Hit)
{
	const FVector N = Hit.Normal;
	if (N.Z > 0.7f)
	{
		VerticalSpeed = FMath::Max(VerticalSpeed, 0.f);
		return;
	}
	if (N.Z < -0.7f)
	{
		VerticalSpeed = FMath::Min(VerticalSpeed, 0.f);
		return;
	}
	const FVector2D N2 = FVector2D(N.X, N.Y).GetSafeNormal();
	if (N2.IsNearlyZero())
	{
		return;
	}
	const float SpeedBefore = State.Velocity.Size();
	const float Into = FVector2D::DotProduct(State.Velocity, N2);
	if (Into >= 0.f || SpeedBefore < 1.f)
	{
		return;
	}
	const float HeadOn = FMath::Clamp(-Into / SpeedBefore, 0.f, 1.f);

	// Remove the into-wall part (with a little bounce) and scrub speed by how head-on the hit was.
	State.Velocity -= N2 * Into * (1.f + WallBounce);
	State.Velocity *= 1.f - 0.3f * HeadOn;

	// Turn the nose along the wall, toward the side it already pointed to, so the car never sticks to it.
	FVector2D Tangent(-N2.Y, N2.X);
	if (FVector2D::DotProduct(Tangent, State.Forward()) < 0.f)
	{
		Tangent = -Tangent;
	}
	const float TangentYaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
	const float DeltaYaw = FRotator::NormalizeAxis(TangentYaw - State.Yaw);
	State.Yaw = FRotator::NormalizeAxis(State.Yaw + DeltaYaw * FMath::Clamp(0.15f + 0.5f * HeadOn, 0.f, 1.f));
	State.YawRate *= 0.5f;

	if (-Into > PendingImpact)
	{
		PendingImpact = -Into;
		PendingImpactPoint = Hit.ImpactPoint;
	}
}
