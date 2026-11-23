// DriftDash - destructible street props: fence, crate, lamp post, hydrant.
//
// Intact, a prop is 1-4 basic-shape parts that only overlap the car (they never stop it, no physics, no tick).
// When the car touches it, Smash() turns every part into a physics body flying off with the car's impulse, adds
// sparks / water / dust and a combo point; after DebrisLifetime seconds the parts shrink away and the actor is gone.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DriftProp.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EDriftPropKind : uint8
{
	Fence,
	Crate,
	Lamp,
	Hydrant
};

UCLASS()
class DRIFTDASH_API ADriftProp : public AActor
{
	GENERATED_BODY()

public:
	ADriftProp();

	/** Seconds from the smash until the debris is gone. */
	static constexpr float DebrisLifetime = 2.5f;
	/** The last part of that time: parts stop simulating and shrink to nothing. */
	static constexpr float ShrinkTime = 0.4f;

	/** Builds the parts. Call right after spawning; the actor's origin is on the ground. */
	void Setup(EDriftPropKind InKind);

	/** Breaks the prop. HitVelocity: the car's velocity (cm/s). Returns false if it was already smashed. */
	bool Smash(const FVector& HitVelocity, const FVector& HitPoint);

	/** Starts the shrink-out now (debris budget). */
	void ExpireSoon();

	EDriftPropKind GetKind() const { return Kind; }
	bool IsSmashed() const { return bSmashed; }
	float GetSmashAge() const { return SmashAge; }
	/** Parts still simulating physics. */
	int32 GetLiveDebrisParts() const;
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetParts() const { return Parts; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FVector& Scale, const FRotator& Rotation, float MassKg);
	void FreezeParts();

	UPROPERTY(VisibleAnywhere, Category = "Prop")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	TArray<float> PartMass;
	TArray<FVector> PartScale;
	TArray<FVector> FrozenVelocity;

	EDriftPropKind Kind = EDriftPropKind::Crate;
	bool bSmashed = false;
	bool bFrozen = false;
	float SmashAge = 0.f;
};
