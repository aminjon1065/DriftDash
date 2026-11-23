// DriftDash - builds the test scene "City block" at start: a 3 x 3 grid of wide roads around 2 x 2 blocks (90-degree
// corners, T-junctions, a crossing), two ramps, a ring of buildings as the boundary, destructible props, and the
// "golden sunset" light (low warm sun with long shadows, cool sky light, light fog, soft bloom).
//
// Static geometry goes into a handful of instanced meshes (one draw call per mesh and colour). Ground pieces are
// DriftGround (driven on by ray casts), buildings and ramp backs WorldStatic (they stop the car).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DriftProp.h"
#include "DriftCityBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class DRIFTDASH_API ADriftCityBuilder : public AActor
{
	GENERATED_BODY()

public:
	ADriftCityBuilder();

	// Layout, cm.
	static constexpr float RoadWidth = 1400.f;
	static constexpr float BlockX = 7000.f;
	static constexpr float BlockY = 5000.f;
	static constexpr int32 BlocksX = 2;
	static constexpr int32 BlocksY = 2;
	static constexpr float SidewalkWidth = 350.f;
	static constexpr float SidewalkHeight = 12.f;
	static constexpr float BoundaryDepth = 2200.f;

	/** Builds ground, buildings, ramps and lights (once). */
	void Build();

	/** Destroys any props and spawns the full set again (restart). */
	void SpawnProps();

	/** On the bottom road, heading +X toward the first ramp. */
	FTransform GetStartTransform() const;

	/** Centre line X of road column i (0..BlocksX) / Y of road row j (0..BlocksY). */
	static float RoadX(int32 i) { return i * (BlockX + RoadWidth); }
	static float RoadY(int32 j) { return j * (BlockY + RoadWidth); }

	/** The outer loop's corners (road centre lines), counter-clockwise from the start corner. */
	TArray<FVector2D> GetLoopCorners() const;

	int32 GetRampCount() const { return Ramps.Num(); }
	const TArray<FBox>& GetRamps() const { return Ramps; }
	const TArray<TWeakObjectPtr<ADriftProp>>& GetProps() const { return Props; }
	int32 CountProps(EDriftPropKind Kind) const;

private:
	struct FPropSpot
	{
		EDriftPropKind Kind;
		FVector Location;
		float Yaw;
	};

	UInstancedStaticMeshComponent* MakeIsm(UStaticMesh* Mesh, UMaterialInterface* Material, ECollisionChannel ObjectType, bool bCollision, bool bShadows);
	void AddBox(UInstancedStaticMeshComponent* Ism, const FVector& Center, const FVector& Size, float Yaw = 0.f, float Pitch = 0.f);
	void BuildGround();
	void BuildBuildings();
	void BuildRamp(const FVector2D& Start, float Yaw);
	void BuildMarkings();
	void BuildLights();
	void PlanProps();
	void AddBuilding(const FVector2D& Min, const FVector2D& Max, float Height, int32 Style, const FVector2D& FacadeDir);

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Isms;

	UPROPERTY()
	TArray<TWeakObjectPtr<ADriftProp>> Props;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> LightActors;

	TArray<FBox> Ramps;
	TArray<FPropSpot> PropSpots;
	bool bBuilt = false;

	// Instanced meshes by role (filled by Build).
	UInstancedStaticMeshComponent* Asphalt = nullptr;
	UInstancedStaticMeshComponent* Sidewalk = nullptr;
	UInstancedStaticMeshComponent* Kerb = nullptr;
	UInstancedStaticMeshComponent* Marking = nullptr;
	UInstancedStaticMeshComponent* RampTop = nullptr;
	UInstancedStaticMeshComponent* RampBody = nullptr;
	UInstancedStaticMeshComponent* Windows = nullptr;
	UInstancedStaticMeshComponent* Roofs = nullptr;
	TArray<UInstancedStaticMeshComponent*> Walls;
};
