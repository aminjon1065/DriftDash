// DriftDash - destructible street props.

#include "DriftProp.h"
#include "DriftFx.h"
#include "DriftLook.h"
#include "DriftScore.h"
#include "DriftTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ADriftProp::ADriftProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void ADriftProp::AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FVector& Scale, const FRotator& Rotation, float MassKg)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetupAttachment(Root);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	// Intact: an overlap trigger for the car only.
	Part->SetCollisionObjectType(ECC_WorldDynamic);
	Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Part->SetCollisionResponseToAllChannels(ECR_Ignore);
	Part->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Part->SetGenerateOverlapEvents(true);
	Part->RegisterComponent();
	Parts.Add(Part);
	PartMass.Add(MassKg);
	PartScale.Add(Scale);
}

void ADriftProp::Setup(EDriftPropKind InKind)
{
	Kind = InKind;
	UDriftLookSubsystem* Look = UDriftLookSubsystem::Get(this);
	if (!Look)
	{
		return;
	}
	UStaticMesh* Cube = Look->Cube();
	switch (Kind)
	{
	case EDriftPropKind::Fence:
	{
		// 3 m picket segment along local X: two posts, two rails.
		UMaterialInterface* Wood = Look->Solid(FLinearColor(0.92f, 0.86f, 0.74f), 0.8f);
		UMaterialInterface* Post = Look->Solid(FLinearColor(0.55f, 0.38f, 0.24f), 0.85f);
		AddPart(Cube, Post, FVector(-140.f, 0.f, 50.f), FVector(0.12f, 0.12f, 1.f), FRotator::ZeroRotator, 8.f);
		AddPart(Cube, Post, FVector(140.f, 0.f, 50.f), FVector(0.12f, 0.12f, 1.f), FRotator::ZeroRotator, 8.f);
		AddPart(Cube, Wood, FVector(0.f, 0.f, 35.f), FVector(3.f, 0.06f, 0.16f), FRotator::ZeroRotator, 10.f);
		AddPart(Cube, Wood, FVector(0.f, 0.f, 75.f), FVector(3.f, 0.06f, 0.16f), FRotator::ZeroRotator, 10.f);
		break;
	}
	case EDriftPropKind::Crate:
	{
		// 90 cm crate in two halves, so it splits open.
		AddPart(Cube, Look->Solid(FLinearColor(0.72f, 0.45f, 0.2f), 0.85f), FVector(0.f, -22.5f, 45.f), FVector(0.9f, 0.45f, 0.9f), FRotator::ZeroRotator, 15.f);
		AddPart(Cube, Look->Solid(FLinearColor(0.62f, 0.37f, 0.16f), 0.85f), FVector(0.f, 22.5f, 45.f), FVector(0.9f, 0.45f, 0.9f), FRotator::ZeroRotator, 15.f);
		break;
	}
	case EDriftPropKind::Lamp:
	{
		// 4.2 m post with an arm and a glowing head over local +X.
		UMaterialInterface* Metal = Look->Solid(FLinearColor(0.14f, 0.16f, 0.2f), 0.5f);
		AddPart(Look->Cylinder(), Metal, FVector(0.f, 0.f, 210.f), FVector(0.14f, 0.14f, 4.2f), FRotator::ZeroRotator, 40.f);
		AddPart(Cube, Metal, FVector(40.f, 0.f, 412.f), FVector(0.9f, 0.08f, 0.08f), FRotator::ZeroRotator, 8.f);
		AddPart(Cube, Look->Glow(FLinearColor(1.f, 0.78f, 0.45f), 3.f), FVector(78.f, 0.f, 400.f), FVector(0.36f, 0.26f, 0.12f), FRotator::ZeroRotator, 5.f);
		break;
	}
	case EDriftPropKind::Hydrant:
	{
		UMaterialInterface* Red = Look->Solid(FLinearColor(0.85f, 0.09f, 0.06f), 0.45f);
		AddPart(Look->Cylinder(), Red, FVector(0.f, 0.f, 32.f), FVector(0.36f, 0.36f, 0.64f), FRotator::ZeroRotator, 30.f);
		AddPart(Look->Sphere(), Red, FVector(0.f, 0.f, 64.f), FVector(0.38f, 0.38f, 0.3f), FRotator::ZeroRotator, 6.f);
		AddPart(Cube, Look->Solid(FLinearColor(0.9f, 0.85f, 0.8f), 0.45f), FVector(0.f, 0.f, 42.f), FVector(0.52f, 0.14f, 0.14f), FRotator::ZeroRotator, 4.f);
		break;
	}
	}
}

bool ADriftProp::Smash(const FVector& HitVelocity, const FVector& HitPoint)
{
	if (bSmashed)
	{
		return false;
	}
	bSmashed = true;
	SmashAge = 0.f;
	SetActorTickEnabled(true);

	FRandomStream Rng(static_cast<int32>(GetUniqueID()) * 7919 + 13);
	const float CarSpeed = FMath::Min(static_cast<float>(HitVelocity.Size()), 3200.f);
	const FVector Dir = CarSpeed > 50.f ? HitVelocity.GetSafeNormal() : (GetActorLocation() - HitPoint).GetSafeNormal2D();
	const float Push = FMath::Max(CarSpeed, 900.f);

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UStaticMeshComponent* Part = Parts[i];
		Part->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		// Debris: blocks walls and ground, ignores the car and other debris (cheaper, and it never stops the car).
		Part->SetGenerateOverlapEvents(false);
		Part->SetCollisionObjectType(ECC_PhysicsBody);
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Part->SetCollisionResponseToChannel(ECC_DriftGround, ECR_Block);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Part->SetSimulatePhysics(true);
		Part->SetMassOverrideInKg(NAME_None, PartMass[i], true);
		Part->SetLinearDamping(0.15f);
		Part->SetAngularDamping(0.4f);

		FVector Out = (Part->GetComponentLocation() - HitPoint);
		Out.Z = 0.f;
		Out = Out.GetSafeNormal();
		const FVector Vel = Dir * Push * Rng.FRandRange(0.55f, 1.05f)
			+ Out * Push * Rng.FRandRange(0.1f, 0.35f)
			+ FVector::UpVector * Push * Rng.FRandRange(0.25f, 0.5f);
		Part->AddImpulse(Vel, NAME_None, true);
		Part->AddAngularImpulseInDegrees(Rng.VRand() * Rng.FRandRange(300.f, 900.f), NAME_None, true);
	}

	if (UDriftFxSubsystem* Fx = UDriftFxSubsystem::Get(this))
	{
		const FVector Base = GetActorLocation();
		const float Floor = Base.Z + 5.f;
		switch (Kind)
		{
		case EDriftPropKind::Lamp:
			Fx->Burst(EDriftFx::Spark, Base + FVector(78.f, 0.f, 400.f), Dir * 300.f, 14, 650.f, 0.07f, 0.7f, Floor);
			Fx->Burst(EDriftFx::Spark, Base + FVector(0.f, 0.f, 20.f), Dir * 200.f, 10, 500.f, 0.06f, 0.6f, Floor);
			break;
		case EDriftPropKind::Hydrant:
			Fx->Burst(EDriftFx::Water, Base + FVector(0.f, 0.f, 50.f), FVector(0.f, 0.f, 500.f), 16, 500.f, 0.16f, 1.f, Floor);
			Fx->AddFountain(Base + FVector(0.f, 0.f, 40.f), DebrisLifetime);
			break;
		default:
			Fx->Burst(EDriftFx::Smoke, Base + FVector(0.f, 0.f, 40.f), Dir * 150.f, 4, 180.f, 0.5f, 0.6f);
			Fx->Burst(EDriftFx::Spark, Base + FVector(0.f, 0.f, 40.f), Dir * 250.f, 5, 450.f, 0.05f, 0.45f, Floor);
			break;
		}
		Fx->RegisterDebris(this);
	}
	if (UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(this))
	{
		Score->AddSmash(GetActorLocation());
	}
	return true;
}

int32 ADriftProp::GetLiveDebrisParts() const
{
	return bSmashed && !bFrozen && SmashAge < DebrisLifetime - ShrinkTime ? Parts.Num() : 0;
}

void ADriftProp::ExpireSoon()
{
	if (bSmashed)
	{
		SmashAge = FMath::Max(SmashAge, DebrisLifetime - ShrinkTime);
	}
}

void ADriftProp::FreezeParts()
{
	bFrozen = true;
	FrozenVelocity.Reset();
	for (UStaticMeshComponent* Part : Parts)
	{
		FrozenVelocity.Add(Part->GetPhysicsLinearVelocity() * 0.5f);
		Part->SetSimulatePhysics(false);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ADriftProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSmashed)
	{
		return;
	}
	SmashAge += DeltaSeconds;
	if (SmashAge >= DebrisLifetime)
	{
		Destroy();
		return;
	}
	if (SmashAge < DebrisLifetime - ShrinkTime)
	{
		return;
	}
	// Shrink-out: no more physics, parts drift on and scale down to nothing.
	if (!bFrozen)
	{
		FreezeParts();
	}
	const float Left = FMath::Clamp((DebrisLifetime - SmashAge) / ShrinkTime, 0.f, 1.f);
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UStaticMeshComponent* Part = Parts[i];
		Part->AddWorldOffset(FrozenVelocity[i] * DeltaSeconds);
		Part->SetWorldScale3D(PartScale[i] * FMath::Max(Left, 0.01f));
	}
}
