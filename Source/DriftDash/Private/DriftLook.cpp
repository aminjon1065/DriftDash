// DriftDash - shared meshes and colour materials.

#include "DriftLook.h"
#include "DriftTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* SolidPath = TEXT("/Game/Materials/M_Solid.M_Solid");
	const TCHAR* GlowPath = TEXT("/Game/Materials/M_Glow.M_Glow");
	const TCHAR* FallbackPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

UDriftLookSubsystem* UDriftLookSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDriftLookSubsystem>() : nullptr;
}

bool UDriftLookSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UStaticMesh* UDriftLookSubsystem::Mesh(FName Name)
{
	if (TObjectPtr<UStaticMesh>* Found = Meshes.Find(Name))
	{
		return *Found;
	}
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), *Name.ToString(), *Name.ToString());
	UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, *Path);
	if (!Loaded)
	{
		UE_LOG(LogDrift, Error, TEXT("Missing engine mesh %s"), *Path);
	}
	Meshes.Add(Name, Loaded);
	return Loaded;
}

UStaticMesh* UDriftLookSubsystem::Cube() { return Mesh(TEXT("Cube")); }
UStaticMesh* UDriftLookSubsystem::Cylinder() { return Mesh(TEXT("Cylinder")); }
UStaticMesh* UDriftLookSubsystem::Sphere() { return Mesh(TEXT("Sphere")); }
UStaticMesh* UDriftLookSubsystem::Cone() { return Mesh(TEXT("Cone")); }

void UDriftLookSubsystem::LoadBases()
{
	if (bBasesLoaded)
	{
		return;
	}
	bBasesLoaded = true;
	SolidBase = LoadObject<UMaterialInterface>(nullptr, SolidPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	GlowBase = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	bProjectMaterials = SolidBase && GlowBase;
	if (!bProjectMaterials)
	{
		UE_LOG(LogDrift, Warning, TEXT("Project materials missing (run Scripts/run_editor_python.sh Scripts/build_content.py); using BasicShapeMaterial"));
		UMaterialInterface* Fallback = LoadObject<UMaterialInterface>(nullptr, FallbackPath);
		SolidBase = SolidBase ? SolidBase.Get() : Fallback;
		GlowBase = GlowBase ? GlowBase.Get() : Fallback;
	}
}

bool UDriftLookSubsystem::HasProjectMaterials()
{
	LoadBases();
	return bProjectMaterials;
}

UMaterialInterface* UDriftLookSubsystem::Instance(bool bGlow, const FLinearColor& Color, float Scalar)
{
	LoadBases();
	UMaterialInterface* Base = bGlow ? GlowBase : SolidBase;
	if (!Base)
	{
		return nullptr;
	}
	const FColor Key8 = Color.ToFColor(false);
	const uint32 Key = HashCombine(HashCombine(GetTypeHash(Key8), GetTypeHash(FMath::RoundToInt(Scalar * 100.f))), bGlow ? 1u : 0u);
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
	// The fallback has only "Color": glow is approximated by a brighter colour there.
	const bool bFallback = !bProjectMaterials;
	Mid->SetVectorParameterValue(TEXT("Color"), bGlow && bFallback ? Color * FMath::Min(Scalar, 2.f) : Color);
	Mid->SetScalarParameterValue(bGlow ? TEXT("Strength") : TEXT("Roughness"), Scalar);
	Cache.Add(Key, Mid);
	return Mid;
}

UMaterialInterface* UDriftLookSubsystem::Solid(const FLinearColor& Color, float Roughness)
{
	return Instance(false, Color, Roughness);
}

UMaterialInterface* UDriftLookSubsystem::Glow(const FLinearColor& Color, float Strength)
{
	return Instance(true, Color, Strength);
}
