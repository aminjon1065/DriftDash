// DriftDash - cheap effects for phones: tyre smoke, sparks and water as pooled instances (one draw call per kind,
// no translucency, no shadows), plus the debris budget for smashed props.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DriftFx.generated.h"

class UInstancedStaticMeshComponent;
class ADriftProp;

UENUM()
enum class EDriftFx : uint8
{
	Smoke,
	Spark,
	Water,
	Count UMETA(Hidden)
};

UCLASS()
class DRIFTDASH_API UDriftFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDriftFxSubsystem* Get(const UObject* WorldContext);

	/** Pool sizes: the oldest particle is reused when a pool is full. */
	static constexpr int32 SmokePool = 64;
	static constexpr int32 SparkPool = 48;
	static constexpr int32 WaterPool = 64;
	/** Physics parts flying at once; beyond it the oldest smashed prop starts vanishing early. */
	static constexpr int32 MaxDebrisParts = 40;

	/** One particle. Size: scale of the 100 cm shape. FloorZ: it bounces there (sparks, water). */
	void Spawn(EDriftFx Kind, const FVector& Pos, const FVector& Vel, float Size, float Life, float FloorZ = -1.0e6f);

	/** Count particles around BaseVel with random spread of Speed. */
	void Burst(EDriftFx Kind, const FVector& Pos, const FVector& BaseVel, int32 Count, float Speed, float Size, float Life, float FloorZ = -1.0e6f);

	/** A broken hydrant: water shoots up from Pos for Duration seconds. */
	void AddFountain(const FVector& Pos, float Duration);

	/** Smashed props register here so the number of physics parts stays bounded. */
	void RegisterDebris(ADriftProp* Prop);

	/** Removes every particle and fountain (restart). */
	void Clear();

	int32 ActiveCount(EDriftFx Kind) const;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FParticle
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		FRotator Rot = FRotator::ZeroRotator;
		FRotator Spin = FRotator::ZeroRotator;
		float Age = 0.f;
		float Life = 0.f;
		float Size = 1.f;
		float FloorZ = -1.0e6f;
		bool bAlive = false;
	};

	struct FPool
	{
		TArray<FParticle> Particles;
		TArray<FTransform> Transforms;
		int32 Next = 0;
		int32 Alive = 0;
		bool bWasAlive = false;
	};

	struct FFountain
	{
		FVector Pos = FVector::ZeroVector;
		float TimeLeft = 0.f;
		float Accum = 0.f;
	};

	bool EnsureHost();
	void TickPool(EDriftFx Kind, float Dt);

	UPROPERTY()
	TObjectPtr<AActor> Host;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Isms;

	UPROPERTY()
	TArray<TWeakObjectPtr<ADriftProp>> Debris;

	FPool Pools[static_cast<int32>(EDriftFx::Count)];
	TArray<FFountain> Fountains;
	FRandomStream Rng = FRandomStream(4242);
};
