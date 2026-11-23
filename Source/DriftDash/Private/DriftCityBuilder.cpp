// DriftDash - the "City block" test scene.

#include "DriftCityBuilder.h"
#include "DriftLook.h"
#include "DriftTypes.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Components/SkyAtmosphereComponent.h"

namespace
{
	constexpr float HalfRoad = ADriftCityBuilder::RoadWidth * 0.5f;
	// Ramps: 7 m long, 1.6 m high (about 13 degrees), 6 m wide; the rest of the road stays free beside them.
	constexpr float RampLength = 700.f;
	constexpr float RampHeight = 160.f;
	constexpr float RampWidth = 600.f;

	const FLinearColor WallColors[] = {
		FLinearColor(0.85f, 0.52f, 0.4f),  // terracotta
		FLinearColor(0.95f, 0.8f, 0.56f),  // sand
		FLinearColor(0.42f, 0.68f, 0.7f),  // teal
		FLinearColor(0.76f, 0.56f, 0.72f), // lilac
		FLinearColor(0.9f, 0.9f, 0.86f),   // off-white
	};
	constexpr int32 WallStyles = UE_ARRAY_COUNT(WallColors);
}

ADriftCityBuilder::ADriftCityBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

FTransform ADriftCityBuilder::GetStartTransform() const
{
	return FTransform(FRotator::ZeroRotator, FVector(RoadX(0) + 1200.f, RoadY(0), 75.f));
}

TArray<FVector2D> ADriftCityBuilder::GetLoopCorners() const
{
	return {
		FVector2D(RoadX(0), RoadY(0)),
		FVector2D(RoadX(BlocksX), RoadY(0)),
		FVector2D(RoadX(BlocksX), RoadY(BlocksY)),
		FVector2D(RoadX(0), RoadY(BlocksY)),
	};
}

int32 ADriftCityBuilder::CountProps(EDriftPropKind Kind) const
{
	int32 N = 0;
	for (const TWeakObjectPtr<ADriftProp>& P : Props)
	{
		N += P.IsValid() && P->GetKind() == Kind ? 1 : 0;
	}
	return N;
}

UInstancedStaticMeshComponent* ADriftCityBuilder::MakeIsm(UStaticMesh* Mesh, UMaterialInterface* Material, ECollisionChannel ObjectType, bool bCollision, bool bShadows)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Mesh);
	C->SetMaterial(0, Material);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(bShadows);
	if (bCollision)
	{
		C->SetCollisionObjectType(ObjectType);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->SetupAttachment(GetRootComponent());
	Isms.Add(C);
	return C;
}

void ADriftCityBuilder::AddBox(UInstancedStaticMeshComponent* Ism, const FVector& Center, const FVector& Size, float Yaw, float Pitch)
{
	// Basic-shape cube: 100 cm, centred.
	Ism->AddInstance(FTransform(FRotator(Pitch, Yaw, 0.f), Center, Size / 100.f), true);
}

void ADriftCityBuilder::Build()
{
	UDriftLookSubsystem* Look = UDriftLookSubsystem::Get(this);
	if (bBuilt || !Look)
	{
		return;
	}
	bBuilt = true;
	UStaticMesh* Cube = Look->Cube();
	Asphalt = MakeIsm(Cube, Look->Solid(FLinearColor(0.055f, 0.06f, 0.075f), 0.9f), ECC_DriftGround, true, false);
	Sidewalk = MakeIsm(Cube, Look->Solid(FLinearColor(0.4f, 0.37f, 0.35f), 0.9f), ECC_DriftGround, true, false);
	Kerb = MakeIsm(Cube, Look->Solid(FLinearColor(0.7f, 0.68f, 0.64f), 0.8f), ECC_WorldStatic, false, false);
	Marking = MakeIsm(Cube, Look->Solid(FLinearColor(0.9f, 0.86f, 0.72f), 0.6f), ECC_WorldStatic, false, false);
	RampTop = MakeIsm(Cube, Look->Solid(FLinearColor(1.f, 0.6f, 0.08f), 0.5f), ECC_DriftGround, true, true);
	RampBody = MakeIsm(Cube, Look->Solid(FLinearColor(0.22f, 0.19f, 0.18f), 0.8f), ECC_WorldStatic, true, true);
	Roofs = MakeIsm(Cube, Look->Solid(FLinearColor(0.17f, 0.15f, 0.19f), 0.8f), ECC_WorldStatic, false, true);
	Windows = MakeIsm(Cube, Look->Glow(FLinearColor(1.f, 0.6f, 0.28f), 0.75f), ECC_WorldStatic, false, false);
	for (int32 i = 0; i < WallStyles; ++i)
	{
		Walls.Add(MakeIsm(Cube, Look->Solid(WallColors[i], 0.85f), ECC_WorldStatic, true, true));
	}

	BuildGround();
	BuildBuildings();
	BuildRamp(FVector2D(RoadX(0) + HalfRoad + BlockX * 0.5f - RampLength * 0.5f, RoadY(0)), 0.f);
	BuildRamp(FVector2D(RoadX(1), RoadY(0) + HalfRoad + BlockY * 0.5f - RampLength * 0.5f), 90.f);
	BuildMarkings();

	for (UInstancedStaticMeshComponent* Ism : Isms)
	{
		Ism->RegisterComponent();
	}
	BuildLights();
	PlanProps();
}

void ADriftCityBuilder::BuildGround()
{
	const float MinX = RoadX(0) - HalfRoad - SidewalkWidth - BoundaryDepth;
	const float MaxX = RoadX(BlocksX) + HalfRoad + SidewalkWidth + BoundaryDepth;
	const float MinY = RoadY(0) - HalfRoad - SidewalkWidth - BoundaryDepth;
	const float MaxY = RoadY(BlocksY) + HalfRoad + SidewalkWidth + BoundaryDepth;
	// Asphalt under everything, top at Z = 0, in tiles of about 40 m (they cull better than one 230 m slab).
	const int32 TilesX = FMath::CeilToInt((MaxX - MinX) / 4000.f);
	const int32 TilesY = FMath::CeilToInt((MaxY - MinY) / 4000.f);
	const float TileX = (MaxX - MinX) / TilesX;
	const float TileY = (MaxY - MinY) / TilesY;
	for (int32 tx = 0; tx < TilesX; ++tx)
	{
		for (int32 ty = 0; ty < TilesY; ++ty)
		{
			AddBox(Asphalt, FVector(MinX + TileX * (tx + 0.5f), MinY + TileY * (ty + 0.5f), -50.f), FVector(TileX, TileY, 100.f));
		}
	}

	// Raised sidewalk islands (top at SidewalkHeight) with a light kerb line along their edges.
	auto Island = [this](float X0, float Y0, float X1, float Y1)
	{
		const float H = SidewalkHeight + 100.f;
		AddBox(Sidewalk, FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, SidewalkHeight - H * 0.5f), FVector(X1 - X0, Y1 - Y0, H));
		const float K = 24.f;
		const float Z = SidewalkHeight + 0.5f;
		AddBox(Kerb, FVector((X0 + X1) * 0.5f, Y0 + K * 0.5f, Z), FVector(X1 - X0, K, 2.f));
		AddBox(Kerb, FVector((X0 + X1) * 0.5f, Y1 - K * 0.5f, Z), FVector(X1 - X0, K, 2.f));
		AddBox(Kerb, FVector(X0 + K * 0.5f, (Y0 + Y1) * 0.5f, Z), FVector(K, Y1 - Y0 - 2.f * K, 2.f));
		AddBox(Kerb, FVector(X1 - K * 0.5f, (Y0 + Y1) * 0.5f, Z), FVector(K, Y1 - Y0 - 2.f * K, 2.f));
	};
	for (int32 i = 0; i < BlocksX; ++i)
	{
		for (int32 j = 0; j < BlocksY; ++j)
		{
			Island(RoadX(i) + HalfRoad, RoadY(j) + HalfRoad, RoadX(i + 1) - HalfRoad, RoadY(j + 1) - HalfRoad);
		}
	}
	// Outer ring of sidewalk in front of the boundary buildings.
	const float OX0 = RoadX(0) - HalfRoad - SidewalkWidth;
	const float OX1 = RoadX(BlocksX) + HalfRoad + SidewalkWidth;
	const float OY0 = RoadY(0) - HalfRoad - SidewalkWidth;
	const float OY1 = RoadY(BlocksY) + HalfRoad + SidewalkWidth;
	Island(OX0, OY0, OX1, OY0 + SidewalkWidth);
	Island(OX0, OY1 - SidewalkWidth, OX1, OY1);
	Island(OX0, OY0 + SidewalkWidth, OX0 + SidewalkWidth, OY1 - SidewalkWidth);
	Island(OX1 - SidewalkWidth, OY0 + SidewalkWidth, OX1, OY1 - SidewalkWidth);
}

void ADriftCityBuilder::AddBuilding(const FVector2D& Min, const FVector2D& Max, float Height, int32 Style, const FVector2D& FacadeDir)
{
	const FVector2D C = (Min + Max) * 0.5f;
	const FVector2D S = Max - Min;
	AddBox(Walls[Style % WallStyles], FVector(C.X, C.Y, Height * 0.5f), FVector(S.X, S.Y, Height));
	AddBox(Roofs, FVector(C.X, C.Y, Height + 20.f), FVector(S.X + 50.f, S.Y + 50.f, 40.f));

	// Warm window strips on the facades that face a road (FacadeDir: +-1 per axis, 0 = none).
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const float Dir = Axis == 0 ? FacadeDir.X : FacadeDir.Y;
		if (Dir == 0.f)
		{
			continue;
		}
		const float Span = (Axis == 0 ? S.Y : S.X) - 240.f;
		if (Span < 200.f)
		{
			continue;
		}
		for (float Z = 320.f; Z + 200.f < Height; Z += 380.f)
		{
			FVector P(C.X, C.Y, Z);
			FVector Size;
			if (Axis == 0)
			{
				P.X = Dir > 0.f ? Max.X + 3.f : Min.X - 3.f;
				Size = FVector(10.f, Span, 80.f);
			}
			else
			{
				P.Y = Dir > 0.f ? Max.Y + 3.f : Min.Y - 3.f;
				Size = FVector(Span, 10.f, 80.f);
			}
			AddBox(Windows, P, Size);
		}
	}
}

void ADriftCityBuilder::BuildBuildings()
{
	FRandomStream Rng(20261123);

	// Inner blocks: 3 x 2 low buildings each (low, so the high chase camera sees over them at corners).
	for (int32 i = 0; i < BlocksX; ++i)
	{
		for (int32 j = 0; j < BlocksY; ++j)
		{
			const FVector2D Min(RoadX(i) + HalfRoad + SidewalkWidth, RoadY(j) + HalfRoad + SidewalkWidth);
			const FVector2D Max(RoadX(i + 1) - HalfRoad - SidewalkWidth, RoadY(j + 1) - HalfRoad - SidewalkWidth);
			const int32 NX = 3;
			const int32 NY = 2;
			const FVector2D Cell = (Max - Min) / FVector2D(NX, NY);
			for (int32 a = 0; a < NX; ++a)
			{
				for (int32 b = 0; b < NY; ++b)
				{
					// A little setback variety; buildings stay inside their cell.
					const float Inset = Rng.FRandRange(0.f, 120.f);
					const FVector2D BMin = Min + Cell * FVector2D(a, b) + FVector2D(a == 0 ? 0.f : Inset, b == 0 ? 0.f : Inset);
					const FVector2D BMax = Min + Cell * FVector2D(a + 1, b + 1) - FVector2D(a == NX - 1 ? 0.f : Inset, b == NY - 1 ? 0.f : Inset);
					const FVector2D Facade(a == 0 ? -1.f : (a == NX - 1 ? 1.f : 0.f), b == 0 ? -1.f : 1.f);
					AddBuilding(BMin, BMax, Rng.FRandRange(500.f, 1300.f), Rng.RandRange(0, WallStyles - 1), Facade);
				}
			}
		}
	}

	// Boundary ring: taller buildings closing the map, facing inward.
	const float X0 = RoadX(0) - HalfRoad - SidewalkWidth;
	const float X1 = RoadX(BlocksX) + HalfRoad + SidewalkWidth;
	const float Y0 = RoadY(0) - HalfRoad - SidewalkWidth;
	const float Y1 = RoadY(BlocksY) + HalfRoad + SidewalkWidth;
	auto Band = [&](bool bAlongX, float From, float To, float Inner, float Outer, const FVector2D& Facade)
	{
		float T = From;
		while (T < To - 1.f)
		{
			const float Len = FMath::Min(Rng.FRandRange(1800.f, 3000.f), To - T);
			const float Depth = Rng.FRandRange(0.75f, 1.f) * FMath::Abs(Outer - Inner);
			const float In = Inner;
			const float Out = Inner + (Outer > Inner ? Depth : -Depth);
			FVector2D Min, Max;
			if (bAlongX)
			{
				Min = FVector2D(T, FMath::Min(In, Out));
				Max = FVector2D(T + Len, FMath::Max(In, Out));
			}
			else
			{
				Min = FVector2D(FMath::Min(In, Out), T);
				Max = FVector2D(FMath::Max(In, Out), T + Len);
			}
			AddBuilding(Min, Max, Rng.FRandRange(1000.f, 2400.f), Rng.RandRange(0, WallStyles - 1), Facade);
			T += Len;
		}
	};
	Band(true, X0 - BoundaryDepth, X1 + BoundaryDepth, Y0, Y0 - BoundaryDepth, FVector2D(0.f, 1.f));
	Band(true, X0 - BoundaryDepth, X1 + BoundaryDepth, Y1, Y1 + BoundaryDepth, FVector2D(0.f, -1.f));
	Band(false, Y0, Y1, X0, X0 - BoundaryDepth, FVector2D(1.f, 0.f));
	Band(false, Y0, Y1, X1, X1 + BoundaryDepth, FVector2D(-1.f, 0.f));
}

void ADriftCityBuilder::BuildRamp(const FVector2D& Start, float Yaw)
{
	const float Theta = FMath::Atan2(RampHeight, RampLength);
	const float Slope = FMath::Sqrt(RampLength * RampLength + RampHeight * RampHeight);
	const float Thick = 40.f;
	const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const FVector2D Side(-Dir.Y, Dir.X);

	// Drivable slab: its top face runs from the road at Start up to RampHeight at Start + Dir * RampLength.
	const FVector2D C2 = Start + Dir * (RampLength * 0.5f + FMath::Sin(Theta) * Thick * 0.5f);
	const float CZ = RampHeight * 0.5f - FMath::Cos(Theta) * Thick * 0.5f;
	AddBox(RampTop, FVector(C2.X, C2.Y, CZ), FVector(Slope, RampWidth, Thick), Yaw, FMath::RadiansToDegrees(Theta));

	// Body under it (walls): steps kept 25 cm under the slab, so the car body, 40 cm over the slab, never touches
	// them from the front; from the side the low end can be driven onto, the high part and the back stop the car.
	for (int32 k = 1; k < 4; ++k)
	{
		const float A = RampLength * k / 4.f;
		const float Top = RampHeight * k / 4.f - 25.f;
		const FVector2D P = Start + Dir * (A + RampLength / 8.f);
		AddBox(RampBody, FVector(P.X, P.Y, Top * 0.5f), FVector(RampLength / 4.f, RampWidth - 20.f, Top), Yaw);
	}
	const float BackTop = RampHeight - 30.f;
	const FVector2D Back = Start + Dir * (RampLength - 20.f);
	AddBox(RampBody, FVector(Back.X, Back.Y, BackTop * 0.5f), FVector(40.f, RampWidth - 20.f, BackTop), Yaw);

	const FVector2D End = Start + Dir * RampLength;
	FBox Box(ForceInit);
	for (const FVector2D& P : { Start + Side * RampWidth * 0.5f, Start - Side * RampWidth * 0.5f, End + Side * RampWidth * 0.5f, End - Side * RampWidth * 0.5f })
	{
		Box += FVector(P.X, P.Y, 0.f);
	}
	Box += FVector(End.X, End.Y, RampHeight);
	Ramps.Add(Box);
}

void ADriftCityBuilder::BuildMarkings()
{
	// Dashed centre lines on every road segment between junctions.
	const float Dash = 300.f;
	const float Gap = 300.f;
	auto Dashes = [&](const FVector2D& A, const FVector2D& B)
	{
		const FVector2D D = B - A;
		const float Len = D.Size();
		const FVector2D U = D / Len;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(U.Y, U.X));
		for (float T = Gap; T + Dash < Len - Gap * 0.5f; T += Dash + Gap)
		{
			const FVector2D P = A + U * (T + Dash * 0.5f);
			AddBox(Marking, FVector(P.X, P.Y, 0.6f), FVector(Dash, 18.f, 2.f), Yaw);
		}
	};
	for (int32 j = 0; j <= BlocksY; ++j)
	{
		for (int32 i = 0; i < BlocksX; ++i)
		{
			Dashes(FVector2D(RoadX(i) + HalfRoad, RoadY(j)), FVector2D(RoadX(i + 1) - HalfRoad, RoadY(j)));
		}
	}
	for (int32 i = 0; i <= BlocksX; ++i)
	{
		for (int32 j = 0; j < BlocksY; ++j)
		{
			Dashes(FVector2D(RoadX(i), RoadY(j) + HalfRoad), FVector2D(RoadX(i), RoadY(j + 1) - HalfRoad));
		}
	}
}

void ADriftCityBuilder::BuildLights()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Center((RoadX(0) + RoadX(BlocksX)) * 0.5f, (RoadY(0) + RoadY(BlocksY)) * 0.5f, 3000.f);

	// Golden sunset: low warm sun from behind the start (long shadows run ahead of the car).
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-15.f, 25.f, 0.f), Center), Params))
	{
		UDirectionalLightComponent* L = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(5.5f);
		L->SetLightColor(FLinearColor(1.f, 0.6f, 0.33f));
		L->SetAtmosphereSunLight(true);
		L->SetCastShadows(true);
		L->SetDynamicShadowDistanceMovableLight(7000.f);
		L->SetDynamicShadowCascades(2);
		LightActors.Add(Sun);
	}
	if (AActor* Sky = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Center), Params))
	{
		USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(Sky);
		Sky->SetRootComponent(Atmosphere);
		Atmosphere->RegisterComponent();
		LightActors.Add(Sky);
	}
	// Cool fill from the sky: the engine's sunset ambient cubemap tinted blue (no scene capture: it found nothing
	// to capture in -game runs, and a fixed cubemap is the cheapest sky light on phones).
	if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform(Center), Params))
	{
		USkyLightComponent* L = SkyLight->GetLightComponent();
		L->SetMobility(EComponentMobility::Movable);
		if (UTextureCube* Cube = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/SunsetAmbientCubemap.SunsetAmbientCubemap")))
		{
			L->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
			L->SetCubemap(Cube);
		}
		L->SetIntensity(1.6f);
		L->SetLightColor(FLinearColor(0.6f, 0.72f, 1.f));
		// Warm bounce from the ground instead of black: the shaded sides of buildings keep some colour.
		L->bLowerHemisphereIsBlack = false;
		L->SetLowerHemisphereColor(FLinearColor(0.32f, 0.24f, 0.18f));
		L->RecaptureSky();
		LightActors.Add(SkyLight);
	}
	// Light warm haze in the distance.
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform(FVector(Center.X, Center.Y, 0.f)), Params))
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetFogDensity(0.01f);
		F->SetFogHeightFalloff(0.1f);
		F->SetStartDistance(2500.f);
		F->SetFogInscatteringColor(FLinearColor(0.75f, 0.45f, 0.32f));
		F->SetDirectionalInscatteringColor(FLinearColor(1.f, 0.55f, 0.25f));
		F->SetDirectionalInscatteringExponent(6.f);
		LightActors.Add(Fog);
	}
	// Fixed exposure, soft bloom on glowing parts, a touch of warmth and vignette.
	if (APostProcessVolume* Post = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform(Center), Params))
	{
		Post->bUnbound = true;
		FPostProcessSettings& S = Post->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		S.AutoExposureApplyPhysicalCameraExposure = false;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = 0.f;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = 0.55f;
		S.bOverride_BloomThreshold = true;
		S.BloomThreshold = 1.2f;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(1.1f, 1.1f, 1.1f, 1.f);
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.3f;
		LightActors.Add(Post);
	}
}

void ADriftCityBuilder::PlanProps()
{
	PropSpots.Reset();
	FRandomStream Rng(7);
	const float Z = SidewalkHeight;

	// Lamp posts along the inner islands' edges, arms over the road; one hydrant per island near a corner.
	for (int32 i = 0; i < BlocksX; ++i)
	{
		for (int32 j = 0; j < BlocksY; ++j)
		{
			const float X0 = RoadX(i) + HalfRoad;
			const float X1 = RoadX(i + 1) - HalfRoad;
			const float Y0 = RoadY(j) + HalfRoad;
			const float Y1 = RoadY(j + 1) - HalfRoad;
			const float In = 110.f;
			for (float T = 700.f; T < (X1 - X0) - 500.f; T += 4200.f)
			{
				PropSpots.Add({ EDriftPropKind::Lamp, FVector(X0 + T, Y0 + In, Z), -90.f });
				PropSpots.Add({ EDriftPropKind::Lamp, FVector(X1 - T, Y1 - In, Z), 90.f });
			}
			for (float T = 900.f; T < (Y1 - Y0) - 500.f; T += 4200.f)
			{
				PropSpots.Add({ EDriftPropKind::Lamp, FVector(X0 + In, Y1 - T, Z), 180.f });
				PropSpots.Add({ EDriftPropKind::Lamp, FVector(X1 - In, Y0 + T, Z), 0.f });
			}
			const bool bLeft = Rng.FRand() < 0.5f;
			const bool bLow = Rng.FRand() < 0.5f;
			PropSpots.Add({ EDriftPropKind::Hydrant, FVector(bLeft ? X0 + 200.f : X1 - 200.f, bLow ? Y0 + 420.f : Y1 - 420.f, Z), 0.f });
		}
	}

	// Outer sidewalk: hydrants mid-side and fence runs.
	const float OY0 = RoadY(0) - HalfRoad - SidewalkWidth * 0.5f;
	const float OY1 = RoadY(BlocksY) + HalfRoad + SidewalkWidth * 0.5f;
	const float OX0 = RoadX(0) - HalfRoad - SidewalkWidth * 0.5f;
	const float OX1 = RoadX(BlocksX) + HalfRoad + SidewalkWidth * 0.5f;
	PropSpots.Add({ EDriftPropKind::Hydrant, FVector(RoadX(1) + 1800.f, OY0, Z), 0.f });
	PropSpots.Add({ EDriftPropKind::Hydrant, FVector(RoadX(1) - 1800.f, OY1, Z), 0.f });
	PropSpots.Add({ EDriftPropKind::Hydrant, FVector(OX0, RoadY(1) + 1500.f, Z), 0.f });
	PropSpots.Add({ EDriftPropKind::Hydrant, FVector(OX1, RoadY(1) - 1500.f, Z), 0.f });
	auto FenceRun = [&](const FVector& From, float Yaw, int32 Count)
	{
		const FVector Step = FRotator(0.f, Yaw, 0.f).Vector() * 300.f;
		for (int32 k = 0; k < Count; ++k)
		{
			PropSpots.Add({ EDriftPropKind::Fence, From + Step * (k + 0.5f), Yaw });
		}
	};
	FenceRun(FVector(RoadX(0) + 5600.f, OY0, Z), 0.f, 3);
	FenceRun(FVector(RoadX(1) + 3200.f, OY0, Z), 0.f, 3);
	FenceRun(FVector(RoadX(0) + 2600.f, OY1, Z), 0.f, 3);
	FenceRun(FVector(RoadX(1) + 4600.f, OY1, Z), 0.f, 3);
	FenceRun(FVector(OX0, RoadY(0) + 2200.f, Z), 90.f, 3);
	FenceRun(FVector(OX1, RoadY(1) + 2600.f, Z), 90.f, 3);

	// Crate stacks on the roads: three in a row and one on top.
	auto Crates = [&](const FVector2D& At, float Yaw)
	{
		const FVector Across = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		const FVector Base(At.X, At.Y, 0.f);
		PropSpots.Add({ EDriftPropKind::Crate, Base - Across * 95.f, Yaw + Rng.FRandRange(-8.f, 8.f) });
		PropSpots.Add({ EDriftPropKind::Crate, Base, Yaw + Rng.FRandRange(-8.f, 8.f) });
		PropSpots.Add({ EDriftPropKind::Crate, Base + Across * 95.f, Yaw + Rng.FRandRange(-8.f, 8.f) });
		PropSpots.Add({ EDriftPropKind::Crate, Base - Across * 45.f + FVector(0.f, 0.f, 90.f), Yaw + Rng.FRandRange(-15.f, 15.f) });
	};
	Crates(FVector2D(RoadX(1) - 1500.f, RoadY(0) + 350.f), 0.f);
	Crates(FVector2D(RoadX(2) - 300.f, RoadY(1) - 2000.f), 90.f);
	Crates(FVector2D(RoadX(0) + 3200.f, RoadY(2) - 300.f), 0.f);
	Crates(FVector2D(RoadX(0) + 300.f, RoadY(1) + 2600.f), 90.f);
	Crates(FVector2D(RoadX(1) + 3600.f, RoadY(1) + 300.f), 0.f);
	Crates(FVector2D(RoadX(1) - 350.f, RoadY(1) + 2200.f), 90.f);
}

void ADriftCityBuilder::SpawnProps()
{
	for (const TWeakObjectPtr<ADriftProp>& P : Props)
	{
		if (P.IsValid())
		{
			P->Destroy();
		}
	}
	Props.Reset();
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FPropSpot& Spot : PropSpots)
	{
		ADriftProp* Prop = World->SpawnActor<ADriftProp>(ADriftProp::StaticClass(), FTransform(FRotator(0.f, Spot.Yaw, 0.f), Spot.Location), Params);
		if (Prop)
		{
			Prop->Setup(Spot.Kind);
			Props.Add(Prop);
		}
	}
}
