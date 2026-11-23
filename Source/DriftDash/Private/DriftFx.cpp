// DriftDash - pooled effects and the debris budget.

#include "DriftFx.h"
#include "DriftLook.h"
#include "DriftProp.h"
#include "DriftTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	int32 PoolSize(EDriftFx Kind)
	{
		switch (Kind)
		{
		case EDriftFx::Smoke: return UDriftFxSubsystem::SmokePool;
		case EDriftFx::Spark: return UDriftFxSubsystem::SparkPool;
		default: return UDriftFxSubsystem::WaterPool;
		}
	}

	/** Hidden instances keep their last place at a tiny scale (zero scale breaks normal transforms). */
	constexpr float HiddenScale = 0.001f;
}

UDriftFxSubsystem* UDriftFxSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDriftFxSubsystem>() : nullptr;
}

bool UDriftFxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UDriftFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDriftFxSubsystem, STATGROUP_Tickables);
}

bool UDriftFxSubsystem::EnsureHost()
{
	if (Host && Isms.Num() == static_cast<int32>(EDriftFx::Count))
	{
		return true;
	}
	UWorld* World = GetWorld();
	UDriftLookSubsystem* Look = UDriftLookSubsystem::Get(World);
	if (!World || !Look)
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), TEXT("DriftFxHost"));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host)
	{
		return false;
	}
	Isms.Reset();
	for (int32 K = 0; K < static_cast<int32>(EDriftFx::Count); ++K)
	{
		const EDriftFx Kind = static_cast<EDriftFx>(K);
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(Host);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCastShadow(false);
		Ism->SetGenerateOverlapEvents(false);
		switch (Kind)
		{
		case EDriftFx::Smoke:
			Ism->SetStaticMesh(Look->Sphere());
			Ism->SetMaterial(0, Look->Solid(FLinearColor(0.88f, 0.86f, 0.84f), 1.f));
			break;
		case EDriftFx::Spark:
			Ism->SetStaticMesh(Look->Cube());
			Ism->SetMaterial(0, Look->Glow(FLinearColor(1.f, 0.55f, 0.15f), 8.f));
			break;
		default:
			Ism->SetStaticMesh(Look->Sphere());
			Ism->SetMaterial(0, Look->Solid(FLinearColor(0.35f, 0.62f, 1.f), 0.2f));
			break;
		}
		if (K == 0)
		{
			Host->SetRootComponent(Ism);
		}
		else
		{
			Ism->SetupAttachment(Host->GetRootComponent());
		}
		Ism->RegisterComponent();

		FPool& Pool = Pools[K];
		const int32 Size = PoolSize(Kind);
		Pool.Particles.SetNum(Size);
		Pool.Transforms.Init(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(HiddenScale)), Size);
		Ism->AddInstances(Pool.Transforms, false, true, false);
		Isms.Add(Ism);
	}
	return true;
}

void UDriftFxSubsystem::Spawn(EDriftFx Kind, const FVector& Pos, const FVector& Vel, float Size, float Life, float FloorZ)
{
	if (Kind == EDriftFx::Count || Life <= 0.f || !EnsureHost())
	{
		return;
	}
	FPool& Pool = Pools[static_cast<int32>(Kind)];
	FParticle& P = Pool.Particles[Pool.Next];
	Pool.Next = (Pool.Next + 1) % Pool.Particles.Num();
	P.Pos = Pos;
	P.Vel = Vel;
	P.Rot = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f);
	P.Spin = FRotator(Rng.FRandRange(-360.f, 360.f), Rng.FRandRange(-360.f, 360.f), 0.f);
	P.Age = 0.f;
	P.Life = Life;
	P.Size = Size;
	P.FloorZ = FloorZ;
	P.bAlive = true;
}

void UDriftFxSubsystem::Burst(EDriftFx Kind, const FVector& Pos, const FVector& BaseVel, int32 Count, float Speed, float Size, float Life, float FloorZ)
{
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Dir = Rng.VRand();
		const float Scale = Rng.FRandRange(0.6f, 1.3f);
		Spawn(Kind, Pos, BaseVel + Dir * Speed * Rng.FRandRange(0.4f, 1.f), Size * Scale, Life * Rng.FRandRange(0.7f, 1.2f), FloorZ);
	}
}

void UDriftFxSubsystem::AddFountain(const FVector& Pos, float Duration)
{
	FFountain& F = Fountains.AddDefaulted_GetRef();
	F.Pos = Pos;
	F.TimeLeft = Duration;
}

void UDriftFxSubsystem::RegisterDebris(ADriftProp* Prop)
{
	Debris.RemoveAll([](const TWeakObjectPtr<ADriftProp>& P) { return !P.IsValid(); });
	if (Prop)
	{
		Debris.AddUnique(Prop);
	}
	int32 Parts = 0;
	for (const TWeakObjectPtr<ADriftProp>& P : Debris)
	{
		Parts += P->GetLiveDebrisParts();
	}
	// Oldest first: the ones that started vanishing no longer count.
	for (int32 i = 0; i < Debris.Num() && Parts > MaxDebrisParts; ++i)
	{
		ADriftProp* Old = Debris[i].Get();
		const int32 Live = Old->GetLiveDebrisParts();
		if (Live > 0 && Old != Prop)
		{
			Old->ExpireSoon();
			Parts -= Live;
		}
	}
}

void UDriftFxSubsystem::Clear()
{
	for (int32 K = 0; K < static_cast<int32>(EDriftFx::Count); ++K)
	{
		for (FParticle& P : Pools[K].Particles)
		{
			P.bAlive = false;
		}
	}
	Fountains.Reset();
	Debris.Reset();
}

int32 UDriftFxSubsystem::ActiveCount(EDriftFx Kind) const
{
	if (Kind == EDriftFx::Count)
	{
		return 0;
	}
	int32 N = 0;
	for (const FParticle& P : Pools[static_cast<int32>(Kind)].Particles)
	{
		N += P.bAlive ? 1 : 0;
	}
	return N;
}

void UDriftFxSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Host)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	for (int32 i = Fountains.Num() - 1; i >= 0; --i)
	{
		FFountain& F = Fountains[i];
		F.TimeLeft -= Dt;
		F.Accum += Dt * 45.f;
		while (F.Accum >= 1.f)
		{
			F.Accum -= 1.f;
			const FVector Vel(Rng.FRandRange(-160.f, 160.f), Rng.FRandRange(-160.f, 160.f), Rng.FRandRange(650.f, 900.f));
			Spawn(EDriftFx::Water, F.Pos, Vel, Rng.FRandRange(0.12f, 0.2f), Rng.FRandRange(0.9f, 1.3f), F.Pos.Z - 60.f);
		}
		if (F.TimeLeft <= 0.f)
		{
			Fountains.RemoveAtSwap(i);
		}
	}
	for (int32 K = 0; K < static_cast<int32>(EDriftFx::Count); ++K)
	{
		TickPool(static_cast<EDriftFx>(K), Dt);
	}
}

void UDriftFxSubsystem::TickPool(EDriftFx Kind, float Dt)
{
	const int32 K = static_cast<int32>(Kind);
	FPool& Pool = Pools[K];
	UInstancedStaticMeshComponent* Ism = Isms.IsValidIndex(K) ? Isms[K].Get() : nullptr;
	if (!Ism)
	{
		return;
	}
	// Per kind: drag (1/s), gravity (cm/s^2), bounce.
	const float Drag = Kind == EDriftFx::Smoke ? 2.2f : (Kind == EDriftFx::Spark ? 0.6f : 0.3f);
	const float Gravity = Kind == EDriftFx::Smoke ? 60.f : -1400.f;
	int32 Alive = 0;
	for (int32 i = 0; i < Pool.Particles.Num(); ++i)
	{
		FParticle& P = Pool.Particles[i];
		FTransform& T = Pool.Transforms[i];
		if (!P.bAlive)
		{
			T.SetScale3D(FVector(HiddenScale));
			continue;
		}
		P.Age += Dt;
		if (P.Age >= P.Life)
		{
			P.bAlive = false;
			T.SetScale3D(FVector(HiddenScale));
			continue;
		}
		++Alive;
		P.Vel.Z += Gravity * Dt;
		P.Vel *= FMath::Exp(-Drag * Dt);
		P.Pos += P.Vel * Dt;
		if (P.Pos.Z < P.FloorZ)
		{
			P.Pos.Z = P.FloorZ;
			P.Vel.Z = -P.Vel.Z * 0.3f;
			P.Vel.X *= 0.6f;
			P.Vel.Y *= 0.6f;
		}
		P.Rot += P.Spin * Dt;
		const float A = P.Age / P.Life;
		float S = P.Size;
		if (Kind == EDriftFx::Smoke)
		{
			// Puffs swell quickly, then shrink away (no translucency on phones).
			S *= A < 0.25f ? FMath::Lerp(0.45f, 1.f, A / 0.25f) : FMath::Lerp(1.f, 0.f, (A - 0.25f) / 0.75f);
		}
		else
		{
			S *= A < 0.7f ? 1.f : FMath::Lerp(1.f, 0.f, (A - 0.7f) / 0.3f);
		}
		T = FTransform(P.Rot, P.Pos, FVector(FMath::Max(S, HiddenScale)));
	}
	Pool.Alive = Alive;
	if (Alive > 0 || Pool.bWasAlive)
	{
		Ism->BatchUpdateInstancesTransforms(0, Pool.Transforms, true, true, true);
	}
	Pool.bWasAlive = Alive > 0;
}
