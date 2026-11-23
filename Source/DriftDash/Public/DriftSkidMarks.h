// DriftDash - two black tyre trails from the rear wheels, drawn as a procedural mesh ring buffer.
//
// The buffer is split into SectionCount sections of QuadsPerSection quads; a new quad rewrites only the section it
// lands in (fixed vertex count, so UpdateMeshSection is enough). When the ring wraps, the oldest marks disappear.
// The component lives in world space (absolute transform at the origin).

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "DriftSkidMarks.generated.h"

UCLASS()
class DRIFTDASH_API UDriftSkidMarks : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UDriftSkidMarks(const FObjectInitializer& ObjectInitializer);

	static constexpr int32 NumTrails = 2;
	static constexpr int32 SectionCount = 8;
	static constexpr int32 QuadsPerSection = 48;

	/** Trail width, cm. */
	float Width = 22.f;
	/** A new quad every this many cm. */
	float MinSegment = 35.f;
	/** A longer jump (teleport, landing far away) starts a new strip. */
	float MaxGap = 250.f;

	/** Feeds the wheel's ground contact point. bActive = false ends the trail's current strip. */
	void AddPoint(int32 Trail, const FVector& Position, bool bActive);

	void ClearMarks();

	/** Quads laid since the last clear. */
	int32 GetTotalQuads() const { return TotalQuads; }

	void SetMarkMaterial(UMaterialInterface* InMaterial);

private:
	struct FSection
	{
		TArray<FVector> Vertices;
		int32 Used = 0;
		bool bCreated = false;
	};

	struct FTrail
	{
		bool bHasPoint = false;
		bool bHasEdge = false;
		FVector Point = FVector::ZeroVector;
		FVector EdgeL = FVector::ZeroVector;
		FVector EdgeR = FVector::ZeroVector;
	};

	void AppendQuad(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1);
	void Upload(int32 SectionIndex);

	TArray<FSection> Sections;
	FTrail Trails[NumTrails];
	int32 Current = 0;
	int32 TotalQuads = 0;

	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MarkMaterial;
};
