// DriftDash - arcade drift model.

#include "DriftCarModel.h"

FVector2D FDriftCarState::Forward() const
{
	const float R = FMath::DegreesToRadians(Yaw);
	return FVector2D(FMath::Cos(R), FMath::Sin(R));
}

FVector2D FDriftCarState::Right() const
{
	const float R = FMath::DegreesToRadians(Yaw);
	return FVector2D(-FMath::Sin(R), FMath::Cos(R));
}

void DriftCarModel::Reset(FDriftCarState& State, float Yaw, float Speed)
{
	State = FDriftCarState();
	State.Yaw = FRotator::NormalizeAxis(Yaw);
	State.Velocity = State.Forward() * Speed;
}

void DriftCarModel::Step(FDriftCarState& S, const FDriftCarParams& P, const FDriftInputFrame& In, bool bGrounded, float Dt)
{
	S.bNitroFired = false;
	if (Dt <= 0.f)
	{
		return;
	}
	const float Steer = FMath::Clamp(In.Steer, -1.f, 1.f);
	const FVector2D Fwd = S.Forward();
	const FVector2D Rt = S.Right();
	float VF = FVector2D::DotProduct(S.Velocity, Fwd);
	float VR = FVector2D::DotProduct(S.Velocity, Rt);

	// Handbrake released: a nitro if the car drifted long enough during this hold.
	if (S.bHandbrakeHeld && !In.bHandbrake)
	{
		if (S.HandbrakeDriftTime >= P.NitroMinDrift)
		{
			S.NitroTime = P.NitroDuration;
			VF += P.NitroKick;
			S.bNitroFired = true;
		}
		S.HandbrakeDriftTime = 0.f;
	}
	S.bHandbrakeHeld = In.bHandbrake;
	S.NitroTime = FMath::Max(0.f, S.NitroTime - Dt);
	const float Target = P.CruiseSpeed + (S.NitroTime > 0.f ? P.NitroSpeedBonus : 0.f);

	if (bGrounded)
	{
		// Auto-throttle (or the handbrake's drag).
		if (In.bHandbrake)
		{
			const float Floor = P.CruiseSpeed * P.HandbrakeMinSpeedShare;
			if (VF > Floor)
			{
				VF = FMath::Max(Floor, VF - P.HandbrakeDrag * Dt);
			}
		}
		else if (VF < Target)
		{
			VF = FMath::Min(Target, VF + P.Acceleration * Dt);
		}
		else
		{
			VF = FMath::Max(Target, VF - P.OverspeedDrag * Dt);
		}

		// Steering: the yaw rate follows the input; less authority when slow, more with the handbrake.
		const float SpeedShare = FMath::Clamp(FMath::Abs(VF) / (P.CruiseSpeed * 0.4f), 0.f, 1.f);
		const float Authority = FMath::Lerp(P.MinSteerAuthority, 1.f, SpeedShare);
		float TargetRate = Steer * P.SteerYawRate * Authority;
		if (In.bHandbrake)
		{
			TargetRate *= P.HandbrakeYawScale;
		}
		S.YawRate += (TargetRate - S.YawRate) * (1.f - FMath::Exp(-P.SteerResponse * Dt));

		// Lateral grip pulls the velocity under the heading; GripSpeedKeep of the speed that friction would take is
		// kept (redirected forward), so corners and drifts do not bleed the car dry. It never adds speed.
		const float Grip = In.bHandbrake ? P.HandbrakeGrip : (S.bDrifting ? P.DriftGrip : P.Grip);
		const float SpeedBefore = FMath::Sqrt(VF * VF + VR * VR);
		VR *= FMath::Exp(-Grip * Dt);
		const float SpeedFriction = FMath::Sqrt(VF * VF + VR * VR);
		const float Kept = FMath::Lerp(SpeedFriction, SpeedBefore, P.GripSpeedKeep);
		VF = FMath::Sign(VF) * FMath::Sqrt(FMath::Max(Kept * Kept - VR * VR, 0.f));
	}
	else
	{
		// Airborne: the velocity is kept, the spin slowly dies out.
		S.YawRate *= FMath::Exp(-1.5f * Dt);
	}

	S.Velocity = Fwd * VF + Rt * VR;
	const float MaxSpeed = Target * P.MaxSpeedShare;
	if (S.Velocity.SizeSquared() > FMath::Square(MaxSpeed))
	{
		S.Velocity = S.Velocity.GetSafeNormal() * MaxSpeed;
	}
	S.Yaw = FRotator::NormalizeAxis(S.Yaw + S.YawRate * Dt);

	// Slip relative to the new heading.
	const float Speed = S.Velocity.Size();
	S.SlipAngle = Speed > 50.f ? FMath::RadiansToDegrees(FMath::Atan2(S.LateralSpeed(), S.ForwardSpeed())) : 0.f;
	S.bDrifting = bGrounded && Speed >= P.DriftMinSpeed && FMath::Abs(S.SlipAngle) >= P.DriftSlipAngle;
	S.DriftTime = S.bDrifting ? S.DriftTime + Dt : 0.f;
	if (S.bDrifting && In.bHandbrake)
	{
		S.HandbrakeDriftTime += Dt;
	}
}
