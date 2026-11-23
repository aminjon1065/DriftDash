// DriftDash - the player's car.

#include "DriftCarPawn.h"
#include "DriftCarMovement.h"
#include "DriftFx.h"
#include "DriftGameMode.h"
#include "DriftLook.h"
#include "DriftProp.h"
#include "DriftScore.h"
#include "DriftSkidMarks.h"
#include "DriftTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

namespace
{
	// Body layout (cm). The actor origin rides UDriftCarMovement::RideHeight (75) over the ground.
	constexpr float GroundZ = -75.f;
	constexpr float WheelRadius = 34.f;
	const FVector2D WheelPos(135.f, 88.f);
}

ADriftCarPawn::ADriftCarPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	// The collision box is the upper hull, 40 cm clear of the ground: kerbs and ramp starts pass under it.
	Body->SetBoxExtent(FVector(190.f, 85.f, 35.f));
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Body->SetGenerateOverlapEvents(true);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetHiddenInGame(true);
	SetRootComponent(Body);

	Chassis = CreateDefaultSubobject<USceneComponent>(TEXT("Chassis"));
	Chassis->SetupAttachment(Body);

	Movement = CreateDefaultSubobject<UDriftCarMovement>(TEXT("Movement"));
	Movement->UpdatedComponent = Body;

	Skids = CreateDefaultSubobject<UDriftSkidMarks>(TEXT("SkidMarks"));
	Skids->SetupAttachment(Body);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Body);
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->bConstrainAspectRatio = false;
	// The engine's default for players is MaintainYFOV (the FOV value follows the longer screen side); the camera
	// sets a horizontal FOV computed from CameraVerticalFov and the screen's aspect, so pin the axis to X.
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV);
	Camera->FieldOfView = 45.f;
}

UPawnMovementComponent* ADriftCarPawn::GetMovementComponent() const
{
	return Movement;
}

void ADriftCarPawn::BeginPlay()
{
	Super::BeginPlay();
	AddTickPrerequisiteComponent(Movement);
	Body->OnComponentBeginOverlap.AddDynamic(this, &ADriftCarPawn::OnBodyOverlap);
	BuildVisuals();
	Movement->ResetState(GetActorRotation().Yaw);
	UpdateCamera(0.f, true);
}

UStaticMeshComponent* ADriftCarPawn::AddVisual(USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Material);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetupAttachment(Parent);
	C->SetRelativeLocationAndRotation(Location, Rotation);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	return C;
}

void ADriftCarPawn::BuildVisuals()
{
	UDriftLookSubsystem* Look = UDriftLookSubsystem::Get(this);
	if (bVisualsBuilt || !Look)
	{
		return;
	}
	bVisualsBuilt = true;
	UStaticMesh* Cube = Look->Cube();
	UMaterialInterface* Paint = Look->Solid(FLinearColor(0.95f, 0.22f, 0.16f), 0.35f);
	UMaterialInterface* Glass = Look->Solid(FLinearColor(0.06f, 0.08f, 0.12f), 0.15f);
	UMaterialInterface* Dark = Look->Solid(FLinearColor(0.05f, 0.05f, 0.06f), 0.8f);
	UMaterialInterface* Trim = Look->Solid(FLinearColor(0.95f, 0.85f, 0.2f), 0.4f);

	// Body: low wedge-ish hull, cabin set back, stripe, bumpers, spoiler.
	AddVisual(Chassis, Cube, Paint, FVector(0.f, 0.f, GroundZ + 58.f), FVector(4.2f, 1.9f, 0.52f));
	AddVisual(Chassis, Cube, Paint, FVector(120.f, 0.f, GroundZ + 86.f), FVector(1.6f, 1.8f, 0.08f));
	AddVisual(Chassis, Cube, Glass, FVector(-30.f, 0.f, GroundZ + 110.f), FVector(2.f, 1.6f, 0.5f));
	AddVisual(Chassis, Cube, Paint, FVector(-30.f, 0.f, GroundZ + 137.f), FVector(1.8f, 1.5f, 0.06f));
	AddVisual(Chassis, Cube, Trim, FVector(0.f, 0.f, GroundZ + 85.f), FVector(4.22f, 0.3f, 0.02f));
	AddVisual(Chassis, Cube, Dark, FVector(212.f, 0.f, GroundZ + 42.f), FVector(0.2f, 1.94f, 0.24f));
	AddVisual(Chassis, Cube, Dark, FVector(-212.f, 0.f, GroundZ + 42.f), FVector(0.2f, 1.94f, 0.24f));
	AddVisual(Chassis, Cube, Dark, FVector(-195.f, 0.f, GroundZ + 112.f), FVector(0.35f, 1.84f, 0.06f));
	AddVisual(Chassis, Cube, Dark, FVector(-195.f, -70.f, GroundZ + 96.f), FVector(0.1f, 0.06f, 0.3f));
	AddVisual(Chassis, Cube, Dark, FVector(-195.f, 70.f, GroundZ + 96.f), FVector(0.1f, 0.06f, 0.3f));

	UMaterialInterface* Head = Look->Glow(FLinearColor(1.f, 0.92f, 0.7f), 5.f);
	UMaterialInterface* Tail = Look->Glow(FLinearColor(1.f, 0.08f, 0.05f), 4.f);
	for (const float Y : { -62.f, 62.f })
	{
		AddVisual(Chassis, Cube, Head, FVector(211.f, Y, GroundZ + 66.f), FVector(0.04f, 0.42f, 0.12f));
		AddVisual(Chassis, Cube, Tail, FVector(-211.f, Y, GroundZ + 66.f), FVector(0.04f, 0.46f, 0.1f));
	}

	// Wheels: pivots (front ones steer) holding a sideways cylinder and a hub.
	UMaterialInterface* Hub = Look->Solid(FLinearColor(0.75f, 0.75f, 0.78f), 0.3f);
	const float Signs[4][2] = { { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, -1.f }, { -1.f, 1.f } };
	for (int32 i = 0; i < 4; ++i)
	{
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetMobility(EComponentMobility::Movable);
		Pivot->SetupAttachment(Body);
		Pivot->SetRelativeLocation(FVector(Signs[i][0] * WheelPos.X, Signs[i][1] * WheelPos.Y, GroundZ + WheelRadius));
		Pivot->RegisterComponent();
		AddVisual(Pivot, Look->Cylinder(), Dark, FVector::ZeroVector, FVector(0.68f, 0.68f, 0.3f), FRotator(0.f, 0.f, 90.f));
		AddVisual(Pivot, Look->Cylinder(), Hub, FVector(0.f, Signs[i][1] * 15.f, 0.f), FVector(0.36f, 0.36f, 0.02f), FRotator(0.f, 0.f, 90.f));
		if (i < 2)
		{
			FrontWheels.Add(Pivot);
		}
	}

	// Nitro flames (hidden until a nitro burns): cones pointing backwards.
	UMaterialInterface* Flame = Look->Glow(FLinearColor(0.3f, 0.7f, 1.f), 7.f);
	for (const float Y : { -45.f, 45.f })
	{
		UStaticMeshComponent* F = AddVisual(Chassis, Look->Cone(), Flame, FVector(-250.f, Y, GroundZ + 45.f), FVector(0.22f, 0.22f, 0.6f), FRotator(90.f, 0.f, 0.f));
		F->SetCastShadow(false);
		F->SetVisibility(false);
		Flames.Add(F);
	}

	Skids->SetMarkMaterial(Look->Solid(FLinearColor(0.012f, 0.012f, 0.014f), 0.95f));
}

void ADriftCarPawn::SetDriftInput(const FDriftInputFrame& In)
{
	Movement->SetDriftInput(In);
}

void ADriftCarPawn::ResetCar(const FTransform& Transform)
{
	const FRotator Yaw(0.f, Transform.Rotator().Yaw, 0.f);
	SetActorLocationAndRotation(Transform.GetLocation(), Yaw, false, nullptr, ETeleportType::ResetPhysics);
	Movement->ResetState(Yaw.Yaw);
	Skids->ClearMarks();
	BodyRoll = BodyPitch = LandingDip = WheelYaw = 0.f;
	ShakeTime = 0.f;
	UpdateCamera(0.f, true);
}

void ADriftCarPawn::OnBodyOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	ADriftProp* Prop = Cast<ADriftProp>(OtherActor);
	if (Prop && Prop->Smash(Movement->GetVelocity3D(), GetActorLocation()))
	{
		// Props never stop the car; a lamp post costs a little more than a crate.
		Movement->ApplySpeedLoss(Prop->GetKind() == EDriftPropKind::Lamp ? 0.06f : 0.03f);
		ShakeTime = 0.18f;
		ShakeStrength = FMath::Max(ShakeStrength, 10.f);
	}
}

void ADriftCarPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	UpdateVisuals(Dt);
	UpdateEffects(Dt);
	UpdateCamera(Dt, false);

	if (GetActorLocation().Z < -3000.f)
	{
		// Fell out of the city: start over.
		if (ADriftGameMode* Mode = GetWorld()->GetAuthGameMode<ADriftGameMode>())
		{
			Mode->RestartRun();
		}
	}
}

void ADriftCarPawn::UpdateVisuals(float Dt)
{
	const FDriftCarState& S = Movement->GetCarState();
	const FDriftInputFrame& In = Movement->GetDriftInput();
	const float Speed = S.Speed();

	// Body leans out of the turn (Unreal roll: positive = right side down), dips on the handbrake and on landing.
	const float TargetRoll = FMath::Clamp(-S.YawRate * 0.05f * FMath::Clamp(Speed / 1500.f, 0.f, 1.f), -8.f, 8.f);
	BodyRoll = FMath::FInterpTo(BodyRoll, Movement->IsGrounded() ? TargetRoll : 0.f, Dt, 7.f);
	const float TargetPitch = In.bHandbrake && Movement->IsGrounded() ? -1.5f : (S.NitroTime > 0.f ? 1.5f : 0.f);
	BodyPitch = FMath::FInterpTo(BodyPitch, TargetPitch, Dt, 6.f);
	LandingDip = FMath::FInterpTo(LandingDip, 0.f, Dt, 5.f);
	Chassis->SetRelativeRotation(FRotator(BodyPitch - LandingDip, 0.f, BodyRoll));

	WheelYaw = FMath::FInterpTo(WheelYaw, In.Steer * 28.f, Dt, 12.f);
	for (USceneComponent* W : FrontWheels)
	{
		W->SetRelativeRotation(FRotator(0.f, WheelYaw, 0.f));
	}

	const bool bNitro = S.NitroTime > 0.f;
	const float Flicker = 0.8f + 0.4f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 37.f));
	for (UStaticMeshComponent* F : Flames)
	{
		F->SetVisibility(bNitro);
		if (bNitro)
		{
			F->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.6f * Flicker));
		}
	}
}

void ADriftCarPawn::UpdateEffects(float Dt)
{
	const FDriftCarState& S = Movement->GetCarState();
	const bool bGrounded = Movement->IsGrounded();
	const bool bSliding = bGrounded && S.Speed() > 400.f && (S.bDrifting || (S.bHandbrakeHeld && FMath::Abs(S.SlipAngle) > 6.f));
	const FTransform Xf = GetActorTransform();
	UDriftFxSubsystem* Fx = UDriftFxSubsystem::Get(this);

	// Rear wheel contacts: skid marks, and smoke while sliding.
	SmokeAccum += bSliding ? Dt * 16.f : 0.f;
	const bool bPuff = SmokeAccum >= 1.f;
	if (bPuff)
	{
		SmokeAccum -= 1.f;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		const float Y = i == 0 ? -WheelPos.Y : WheelPos.Y;
		const FVector Contact = Xf.TransformPosition(FVector(-WheelPos.X, Y, GroundZ)) + FVector(0.f, 0.f, 2.f);
		Skids->AddPoint(i, Contact, bSliding);
		if (bPuff && Fx)
		{
			const FVector V(S.Velocity.X * 0.12f, S.Velocity.Y * 0.12f, 110.f);
			const FVector Jitter(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(5.f, 30.f));
			Fx->Spawn(EDriftFx::Smoke, Contact + Jitter, V + FMath::VRand() * 140.f, FMath::FRandRange(0.35f, 0.75f), FMath::FRandRange(0.5f, 0.85f));
		}
	}

	UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(this);
	if (Score)
	{
		Score->SetDrifting(S.bDrifting && bGrounded, Dt);
	}

	if (Movement->ConsumeNitro())
	{
		if (Score)
		{
			Score->AddNitro();
		}
		if (Fx)
		{
			Fx->Burst(EDriftFx::Spark, Xf.TransformPosition(FVector(-240.f, 0.f, GroundZ + 45.f)), FVector(-S.Velocity.X, -S.Velocity.Y, 0.f) * 0.2f, 10, 400.f, 0.06f, 0.4f, Xf.GetLocation().Z + GroundZ);
		}
		ShakeTime = 0.25f;
		ShakeStrength = FMath::Max(ShakeStrength, 6.f);
	}

	FVector ImpactPoint;
	const float Impact = Movement->ConsumeWallImpact(ImpactPoint);
	if (Impact > 250.f)
	{
		if (Fx)
		{
			Fx->Burst(EDriftFx::Spark, ImpactPoint, FVector(0.f, 0.f, 150.f), FMath::Clamp(static_cast<int32>(Impact / 120.f), 4, 16), 500.f, 0.06f, 0.5f, Xf.GetLocation().Z + GroundZ);
		}
		if (Score)
		{
			Score->AddWallHit(Impact);
		}
		ShakeTime = 0.3f;
		ShakeStrength = FMath::Max(ShakeStrength, FMath::Clamp(Impact / 60.f, 6.f, 30.f));
	}

	const float Landing = Movement->ConsumeLanding();
	if (Landing > 300.f)
	{
		LandingDip = FMath::Clamp(Landing / 150.f, 2.f, 7.f);
		ShakeTime = 0.2f;
		ShakeStrength = FMath::Max(ShakeStrength, FMath::Clamp(Landing / 80.f, 4.f, 18.f));
		if (Fx)
		{
			Fx->Burst(EDriftFx::Smoke, Xf.GetLocation() + FVector(0.f, 0.f, GroundZ + 20.f), FVector::ZeroVector, 6, 220.f, 0.8f, 0.6f);
		}
	}
}

void ADriftCarPawn::UpdateCamera(float Dt, bool bSnap)
{
	const FDriftCarState& S = Movement->GetCarState();
	const FVector Car = GetActorLocation();
	const FVector2D Fwd2 = S.Forward();
	const float Speed = S.Speed();
	const FVector2D VelDir = Speed > 300.f ? S.Velocity / Speed : Fwd2;

	// Follow a blend of heading and travel direction, so a drift shows the car sideways on the road.
	const FVector2D Look2 = (Fwd2 * 0.45f + VelDir * 0.55f).GetSafeNormal();
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Look2.Y, Look2.X));
	if (bSnap || !bCameraSnapped)
	{
		CameraYaw = TargetYaw;
	}
	else
	{
		CameraYaw = FMath::FixedTurn(CameraYaw, TargetYaw, FMath::Max(15.f, FMath::Abs(FRotator::NormalizeAxis(TargetYaw - CameraYaw)) * 3.2f) * Dt);
	}
	const FRotator YawRot(0.f, CameraYaw, 0.f);
	const FVector CamFwd = YawRot.Vector();
	const FVector CamRight = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	// Focus ahead of the car (the car sits low on the portrait screen) and toward the slide.
	const float Slide = FMath::Clamp(S.SlipAngle / 40.f, -1.f, 1.f);
	const FVector Focus = FVector(Car.X, Car.Y, Car.Z - 40.f)
		+ FVector(VelDir.X, VelDir.Y, 0.f) * (380.f + Speed * 0.12f)
		+ CamRight * (Slide * CameraSlideOffset);
	const float Pitch = FMath::DegreesToRadians(CameraPitch);
	const float Dist = CameraDistance + Speed * 0.12f;
	const FVector Desired = Focus - CamFwd * (Dist * FMath::Cos(Pitch)) + FVector(0.f, 0.f, Dist * FMath::Sin(Pitch));

	if (bSnap || !bCameraSnapped)
	{
		CameraFocus = Focus;
		CameraLocation = Desired;
		bCameraSnapped = true;
	}
	else
	{
		CameraFocus = FMath::VInterpTo(CameraFocus, Focus, Dt, 7.f);
		CameraLocation = FMath::VInterpTo(CameraLocation, Desired, Dt, 5.f);
	}

	// Never behind a wall: pull the camera in along the line from the car (like a spring arm).
	FVector Eye = CameraLocation;
	if (UWorld* World = GetWorld())
	{
		const FVector From = Car + FVector(0.f, 0.f, 120.f);
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(DriftCamera), false, this);
		if (World->SweepSingleByObjectType(Hit, From, CameraLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(40.f), Query))
		{
			Eye = Hit.Location;
		}
	}

	FVector Shake = FVector::ZeroVector;
	if (ShakeTime > 0.f && Dt > 0.f)
	{
		ShakeTime -= Dt;
		Shake = FMath::VRand() * ShakeStrength * FMath::Clamp(ShakeTime / 0.3f, 0.f, 1.f);
		if (ShakeTime <= 0.f)
		{
			ShakeStrength = 0.f;
		}
	}
	Camera->SetWorldLocationAndRotation(Eye + Shake, (CameraFocus - Eye).Rotation());

	// Keep the vertical field of view on any screen shape (portrait phones, landscape windows).
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			const float Aspect = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
			const float HalfV = FMath::DegreesToRadians(CameraVerticalFov * 0.5f);
			const float Horizontal = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(HalfV) * Aspect));
			Camera->SetFieldOfView(FMath::Clamp(Horizontal, 30.f, 100.f));
		}
	}
}
