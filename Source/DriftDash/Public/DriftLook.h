// DriftDash - shared meshes and colour materials (engine basic shapes + /Game/Materials/M_Solid, M_Glow).
//
// Everything in the prototype is built from basic shapes tinted by material instances. Instances are cached per
// colour, so every part of one colour shares one material (fewer state changes on mobile). Without the project
// materials (Scripts/build_content.py not run yet) the engine's BasicShapeMaterial is used instead.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DriftLook.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class DRIFTDASH_API UDriftLookSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDriftLookSubsystem* Get(const UObject* WorldContext);

	/** 100 cm basic shapes, centred on their origin. */
	UStaticMesh* Cube();
	UStaticMesh* Cylinder();
	UStaticMesh* Sphere();
	UStaticMesh* Cone();

	/** Lit opaque colour. */
	UMaterialInterface* Solid(const FLinearColor& Color, float Roughness = 0.75f);

	/** Unlit emissive colour (blooms above 1). */
	UMaterialInterface* Glow(const FLinearColor& Color, float Strength = 4.f);

	/** True when /Game/Materials/M_Solid and M_Glow were found. */
	bool HasProjectMaterials();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void LoadBases();
	UMaterialInterface* Instance(bool bGlow, const FLinearColor& Color, float Scalar);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SolidBase;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GlowBase;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Cache;

	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMesh>> Meshes;

	bool bBasesLoaded = false;
	bool bProjectMaterials = false;

	UStaticMesh* Mesh(FName Name);
};
