// DriftDash - automation tests (editor, headless): Scripts/run_tests.sh
//
//   DriftDash.Car.Steer         steering left / right, auto-throttle, grip; the car in a world turns, leans, steers wheels
//   DriftDash.Car.DriftBoost    handbrake drift angle, nitro after a drift (not after a tap); marks, smoke, combo in a world
//   DriftDash.Input.Halves      screen halves / keys -> steer + handbrake; restart button hit area
//   DriftDash.Props.Smash       the car smashes props without stopping; debris flies, combo counts, debris gone in 2.5 s
//   DriftDash.City.Layout       city: 2 ramps that launch the car, a wall-free outer loop, props, sunset lights

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "DriftCarModel.h"
#include "DriftCarMovement.h"
#include "DriftCarPawn.h"
#include "DriftCityBuilder.h"
#include "DriftFx.h"
#include "DriftInput.h"
#include "DriftLook.h"
#include "DriftProp.h"
#include "DriftScore.h"
#include "DriftSkidMarks.h"
#include "DriftTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/Material.h"

#define DD_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace DriftTest
{
	constexpr float Dt = 1.f / 60.f;

	/** A manually ticked game world with a plain game mode (no DriftDash city). */
	class FWorld
	{
	public:
		bool Init()
		{
			if (!Wrapper.CreateTestWorld(EWorldType::Game))
			{
				return false;
			}
			Wrapper.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			// UWorld::CreateWorld leaves simulation off (map loading normally turns it on).
			Wrapper.GetTestWorld()->bShouldSimulatePhysics = true;
			return Wrapper.BeginPlayInTestWorld();
		}

		UWorld* Get() const { return Wrapper.GetTestWorld(); }

		void Tick(float Seconds)
		{
			const int32 Frames = FMath::Max(1, FMath::RoundToInt(Seconds / Dt));
			for (int32 i = 0; i < Frames; ++i)
			{
				Wrapper.TickTestWorld(Dt);
			}
		}

		/** Ticks until Condition holds or MaxSeconds pass. Returns the simulated time, or -1 on timeout. */
		float TickUntil(TFunctionRef<bool()> Condition, float MaxSeconds)
		{
			float T = 0.f;
			while (T < MaxSeconds)
			{
				if (Condition())
				{
					return T;
				}
				Wrapper.TickTestWorld(Dt);
				T += Dt;
			}
			return Condition() ? T : -1.f;
		}

	private:
		FTestWorldWrapper Wrapper;
	};

	/** Flat DriftGround slab with its top at Z = 0. */
	AActor* SpawnGround(UWorld* World, float HalfSize = 20000.f)
	{
		AActor* A = World->SpawnActor<AActor>();
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
		C->SetStaticMesh(UDriftLookSubsystem::Get(World)->Cube());
		C->SetCollisionObjectType(ECC_DriftGround);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCollisionResponseToAllChannels(ECR_Block);
		A->SetRootComponent(C);
		C->SetWorldLocation(FVector(0.f, 0.f, -50.f));
		C->SetWorldScale3D(FVector(HalfSize / 50.f, HalfSize / 50.f, 1.f));
		C->RegisterComponent();
		return A;
	}

	ADriftCarPawn* SpawnCar(UWorld* World, const FVector& Location, float Yaw, float Speed)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADriftCarPawn* Car = World->SpawnActor<ADriftCarPawn>(ADriftCarPawn::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Location), P);
		if (Car)
		{
			Car->GetDriftMovement()->ResetState(Yaw, Speed);
		}
		return Car;
	}

	ADriftProp* SpawnProp(UWorld* World, EDriftPropKind Kind, const FVector& Location)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADriftProp* Prop = World->SpawnActor<ADriftProp>(ADriftProp::StaticClass(), FTransform(Location), P);
		if (Prop)
		{
			Prop->Setup(Kind);
		}
		return Prop;
	}

	void Run(FDriftCarState& S, const FDriftCarParams& P, const FDriftInputFrame& In, float Seconds, float* MaxAbsSlip = nullptr)
	{
		const int32 Steps = FMath::RoundToInt(Seconds / Dt);
		for (int32 i = 0; i < Steps; ++i)
		{
			DriftCarModel::Step(S, P, In, true, Dt);
			if (MaxAbsSlip)
			{
				*MaxAbsSlip = FMath::Max(*MaxAbsSlip, FMath::Abs(S.SlipAngle));
			}
		}
	}

	FDriftInputFrame Input(float Steer, bool bHandbrake)
	{
		FDriftInputFrame In;
		In.Steer = Steer;
		In.bHandbrake = bHandbrake;
		return In;
	}

	float Heading(const FVector2D& V)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriftCarSteerTest, "DriftDash.Car.Steer", DD_TEST_FLAGS)
bool FDriftCarSteerTest::RunTest(const FString& Parameters)
{
	using namespace DriftTest;
	const FDriftCarParams P;
	FDriftCarState S;

	// Auto-throttle from a standstill, straight.
	DriftCarModel::Reset(S, 0.f, 0.f);
	Run(S, P, Input(0.f, false), 3.f);
	TestTrue(TEXT("auto-throttle reaches cruise speed"), S.Speed() >= 0.97f * P.CruiseSpeed);
	TestTrue(TEXT("no input keeps the heading"), FMath::Abs(S.Yaw) < 0.01f);

	// Right and left, one second at full steer.
	float MaxSlip = 0.f;
	DriftCarModel::Reset(S, 0.f, P.CruiseSpeed);
	Run(S, P, Input(1.f, false), 1.f, &MaxSlip);
	AddInfo(FString::Printf(TEXT("DD_TEST steer right 1 s: yaw %.1f, velocity heading %.1f, max slip %.1f, speed %.0f"), S.Yaw, Heading(S.Velocity), MaxSlip, S.Speed()));
	TestTrue(TEXT("steering right turns right (yaw > 60)"), S.Yaw > 60.f);
	TestTrue(TEXT("the velocity follows the heading"), Heading(S.Velocity) > 45.f);
	TestTrue(TEXT("grip turn stays below the drift angle"), MaxSlip < P.DriftSlipAngle);
	TestFalse(TEXT("grip turn is not a drift"), S.bDrifting);
	TestTrue(TEXT("a turn keeps most of the speed"), S.Speed() > 0.85f * P.CruiseSpeed);

	DriftCarModel::Reset(S, 0.f, P.CruiseSpeed);
	Run(S, P, Input(-1.f, false), 1.f);
	TestTrue(TEXT("steering left turns left (yaw < -60)"), S.Yaw < -60.f);
	Run(S, P, Input(0.f, false), 0.8f);
	TestTrue(TEXT("releasing the steer stops the rotation"), FMath::Abs(S.YawRate) < 5.f);

	// The same in a world: ground following, body roll, front wheels.
	FWorld World;
	if (!TestTrue(TEXT("test world"), World.Init()))
	{
		return false;
	}
	SpawnGround(World.Get());
	ADriftCarPawn* Car = SpawnCar(World.Get(), FVector(0.f, 0.f, 120.f), 0.f, P.CruiseSpeed);
	if (!TestNotNull(TEXT("car"), Car))
	{
		return false;
	}
	World.Tick(0.3f);
	UDriftCarMovement* M = Car->GetDriftMovement();
	TestTrue(TEXT("car settles on the ground"), M->IsGrounded());
	TestTrue(TEXT("car rides at its ride height"), FMath::IsNearlyEqual(Car->GetActorLocation().Z, M->RideHeight, 3.f));
	Car->SetDriftInput(Input(-1.f, false));
	World.Tick(1.f);
	AddInfo(FString::Printf(TEXT("DD_TEST world steer left 1 s: yaw %.1f, roll %.1f, wheel %.1f"), Car->GetActorRotation().Yaw, Car->GetBodyRoll(), Car->GetFrontWheelYaw()));
	TestTrue(TEXT("car in the world turns left"), Car->GetActorRotation().Yaw < -45.f);
	TestTrue(TEXT("front wheels steer left"), Car->GetFrontWheelYaw() < -20.f);
	TestTrue(TEXT("body leans out of the turn"), Car->GetBodyRoll() > 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriftCarDriftBoostTest, "DriftDash.Car.DriftBoost", DD_TEST_FLAGS)
bool FDriftCarDriftBoostTest::RunTest(const FString& Parameters)
{
	using namespace DriftTest;
	const FDriftCarParams P;
	FDriftCarState S;

	// Handbrake in a turn: a much bigger slide.
	float MaxSlip = 0.f;
	DriftCarModel::Reset(S, 0.f, P.CruiseSpeed);
	Run(S, P, Input(1.f, true), 0.8f, &MaxSlip);
	AddInfo(FString::Printf(TEXT("DD_TEST handbrake 0.8 s: max slip %.1f, drift time %.2f, speed %.0f"), MaxSlip, S.HandbrakeDriftTime, S.Speed()));
	TestTrue(TEXT("handbrake drift angle > 25 deg"), MaxSlip > 25.f);
	TestTrue(TEXT("drifting"), S.bDrifting);
	TestTrue(TEXT("handbrake slows the car"), S.ForwardSpeed() < P.CruiseSpeed);

	// Release after the drift: nitro.
	DriftCarModel::Step(S, P, Input(1.f, false), true, Dt);
	TestTrue(TEXT("nitro fires on release after a drift"), S.bNitroFired);
	TestTrue(TEXT("nitro burning"), S.NitroTime > 0.f);
	Run(S, P, Input(0.f, false), 0.7f);
	AddInfo(FString::Printf(TEXT("DD_TEST after nitro 0.7 s: forward speed %.0f (cruise %.0f)"), S.ForwardSpeed(), P.CruiseSpeed));
	TestTrue(TEXT("nitro pushes above cruise speed"), S.ForwardSpeed() > P.CruiseSpeed + 300.f);

	// A short tap: no nitro.
	DriftCarModel::Reset(S, 0.f, P.CruiseSpeed);
	Run(S, P, Input(1.f, true), 0.1f);
	DriftCarModel::Step(S, P, Input(1.f, false), true, Dt);
	TestFalse(TEXT("no nitro after a tap"), S.bNitroFired || S.NitroTime > 0.f);

	// Handbrake going straight: no drift, no nitro.
	DriftCarModel::Reset(S, 0.f, P.CruiseSpeed);
	Run(S, P, Input(0.f, true), 1.f);
	DriftCarModel::Step(S, P, Input(0.f, false), true, Dt);
	TestFalse(TEXT("no nitro without a drift"), S.bNitroFired);

	// In a world: marks, smoke, combo, then the nitro.
	FWorld World;
	if (!TestTrue(TEXT("test world"), World.Init()))
	{
		return false;
	}
	SpawnGround(World.Get());
	ADriftCarPawn* Car = SpawnCar(World.Get(), FVector(0.f, 0.f, 75.f), 0.f, P.CruiseSpeed);
	if (!TestNotNull(TEXT("car"), Car))
	{
		return false;
	}
	World.Tick(0.2f);
	Car->SetDriftInput(Input(1.f, true));
	World.Tick(1.2f);
	UDriftFxSubsystem* Fx = UDriftFxSubsystem::Get(World.Get());
	UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(World.Get());
	AddInfo(FString::Printf(TEXT("DD_TEST world drift 1.2 s: slip %.1f, skid quads %d, smoke %d, combo %d, score %d"),
		Car->GetDriftMovement()->GetCarState().SlipAngle, Car->GetSkidMarks()->GetTotalQuads(), Fx ? Fx->ActiveCount(EDriftFx::Smoke) : -1,
		Score ? Score->GetCombo() : -1, Score ? Score->GetScore() : -1));
	TestTrue(TEXT("rear wheels lay skid marks"), Car->GetSkidMarks()->GetTotalQuads() >= 10);
	TestTrue(TEXT("tyre smoke"), Fx && Fx->ActiveCount(EDriftFx::Smoke) > 0);
	TestTrue(TEXT("drift counts for the combo"), Score && Score->GetCombo() >= 1 && Score->GetScore() > 0);
	Car->SetDriftInput(Input(0.f, false));
	World.Tick(0.1f);
	TestTrue(TEXT("nitro after the drift in the world"), Car->GetDriftMovement()->GetCarState().NitroTime > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriftInputHalvesTest, "DriftDash.Input.Halves", DD_TEST_FLAGS)
bool FDriftInputHalvesTest::RunTest(const FString& Parameters)
{
	const float W = 500.f;
	const FVector2D L(100.f, 600.f);
	const FVector2D R(400.f, 600.f);
	auto Check = [this](const TCHAR* What, const FDriftInputFrame& F, float Steer, bool bHandbrake)
	{
		TestEqual(FString::Printf(TEXT("%s: steer"), What), F.Steer, Steer);
		TestEqual(FString::Printf(TEXT("%s: handbrake"), What), F.bHandbrake, bHandbrake);
	};
	Check(TEXT("nothing"), DriftInput::Resolve({}, W, false), 0.f, false);
	Check(TEXT("left half"), DriftInput::Resolve({ L }, W, false), -1.f, false);
	Check(TEXT("right half"), DriftInput::Resolve({ R }, W, false), 1.f, false);
	Check(TEXT("centre line counts as right"), DriftInput::Resolve({ FVector2D(250.f, 10.f) }, W, false), 1.f, false);
	Check(TEXT("both halves, left first"), DriftInput::Resolve({ L, R }, W, false), -1.f, true);
	Check(TEXT("both halves, right first"), DriftInput::Resolve({ R, L }, W, false), 1.f, true);
	Check(TEXT("two fingers on one half"), DriftInput::Resolve({ L, L + FVector2D(50.f, 0.f) }, W, false), -1.f, false);
	Check(TEXT("Space / Shift"), DriftInput::Resolve({}, W, true), 0.f, true);
	Check(TEXT("A + Space"), DriftInput::Resolve({ FVector2D(W * 0.25f, 480.f) }, W, true), -1.f, true);

	const FDriftHudLayout Phone = FDriftHudLayout::Make(FVector2D(500.f, 960.f), FVector4(0.f, 0.f, 0.f, 0.f));
	const FBox2D B = Phone.RestartButton();
	TestTrue(TEXT("restart button in the top right corner"), B.Min.X > 400.f && B.Max.X <= 500.f && B.Min.Y >= 0.f && B.Max.Y < 120.f);
	TestTrue(TEXT("tap on the button restarts"), Phone.HitsRestart(B.GetCenter()));
	TestFalse(TEXT("right half below the button steers"), Phone.HitsRestart(FVector2D(450.f, 400.f)));
	TestFalse(TEXT("top left does not restart"), Phone.HitsRestart(FVector2D(60.f, 40.f)));
	const FDriftHudLayout Notch = FDriftHudLayout::Make(FVector2D(1170.f, 2532.f), FVector4(0.f, 141.f, 0.f, 102.f));
	TestTrue(TEXT("button below the notch"), Notch.RestartButton().Min.Y >= 141.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriftPropsSmashTest, "DriftDash.Props.Smash", DD_TEST_FLAGS)
bool FDriftPropsSmashTest::RunTest(const FString& Parameters)
{
	using namespace DriftTest;
	FWorld World;
	if (!TestTrue(TEXT("test world"), World.Init()))
	{
		return false;
	}
	SpawnGround(World.Get());
	const FDriftCarParams P;
	TWeakObjectPtr<ADriftProp> Crate = SpawnProp(World.Get(), EDriftPropKind::Crate, FVector(900.f, 0.f, 0.f));
	TWeakObjectPtr<ADriftProp> Lamp = SpawnProp(World.Get(), EDriftPropKind::Lamp, FVector(2300.f, 40.f, 0.f));
	TWeakObjectPtr<ADriftProp> Hydrant = SpawnProp(World.Get(), EDriftPropKind::Hydrant, FVector(3700.f, -30.f, 0.f));
	ADriftCarPawn* Car = SpawnCar(World.Get(), FVector(0.f, 0.f, 75.f), 0.f, P.CruiseSpeed);
	UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(World.Get());
	UDriftFxSubsystem* Fx = UDriftFxSubsystem::Get(World.Get());
	if (!TestNotNull(TEXT("car"), Car) || !TestTrue(TEXT("props"), Crate.IsValid() && Lamp.IsValid() && Hydrant.IsValid()) || !TestNotNull(TEXT("score"), Score) || !TestNotNull(TEXT("fx"), Fx))
	{
		return false;
	}

	const float THit = World.TickUntil([&]() { return Crate->IsSmashed(); }, 1.5f);
	TestTrue(TEXT("the car smashes the crate on contact"), THit >= 0.f);
	if (THit < 0.f)
	{
		return false;
	}
	TestEqual(TEXT("combo after one prop"), Score->GetCombo(), 1);
	TestEqual(TEXT("smash count"), Score->GetSmashCount(), 1);
	bool bAllSimulate = true;
	for (UStaticMeshComponent* Part : Crate->GetParts())
	{
		bAllSimulate = bAllSimulate && Part->IsSimulatingPhysics();
	}
	TestTrue(TEXT("crate parts become physics debris"), bAllSimulate);

	World.Tick(0.15f);
	float MaxForward = 0.f;
	float MaxUp = -1.e6f;
	for (UStaticMeshComponent* Part : Crate->GetParts())
	{
		MaxForward = FMath::Max(MaxForward, static_cast<float>(Part->GetPhysicsLinearVelocity().X));
		MaxUp = FMath::Max(MaxUp, static_cast<float>(Part->GetComponentLocation().Z));
	}
	AddInfo(FString::Printf(TEXT("DD_TEST crate hit at %.2f s; 0.15 s later: debris forward speed %.0f, highest part z %.0f, car speed %.0f"),
		THit, MaxForward, MaxUp, Car->GetDriftMovement()->GetCarState().Speed()));
	TestTrue(TEXT("debris flies forward with the impulse"), MaxForward > 600.f);
	TestTrue(TEXT("debris is thrown up"), MaxUp > 70.f);
	TestTrue(TEXT("the car does not stop"), Car->GetDriftMovement()->GetCarState().Speed() > 0.85f * P.CruiseSpeed);

	const float TAll = World.TickUntil([&]() { return Lamp->IsSmashed() && Hydrant->IsSmashed(); }, 2.f);
	TestTrue(TEXT("lamp and hydrant smashed too"), TAll >= 0.f);
	TestEqual(TEXT("combo after three props"), Score->GetCombo(), 3);
	TestTrue(TEXT("sparks and water"), Fx->ActiveCount(EDriftFx::Spark) > 0 && Fx->ActiveCount(EDriftFx::Water) > 0);

	// Debris disappears 2.5 s after the smash.
	const float Elapsed = THit + 0.15f + FMath::Max(TAll, 0.f);
	const float TGone = World.TickUntil([&]() { return !Crate.IsValid(); }, 3.5f);
	const float SinceSmash = Elapsed - THit + TGone;
	AddInfo(FString::Printf(TEXT("DD_TEST crate debris gone %.2f s after the smash"), SinceSmash));
	TestTrue(TEXT("crate debris removed"), TGone >= 0.f);
	TestTrue(TEXT("removed about 2.5 s after the smash"), FMath::IsNearlyEqual(SinceSmash, ADriftProp::DebrisLifetime, 0.1f));
	World.Tick(2.6f);
	TestFalse(TEXT("all debris removed"), Lamp.IsValid() || Hydrant.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriftCityLayoutTest, "DriftDash.City.Layout", DD_TEST_FLAGS)
bool FDriftCityLayoutTest::RunTest(const FString& Parameters)
{
	using namespace DriftTest;
	FWorld World;
	if (!TestTrue(TEXT("test world"), World.Init()))
	{
		return false;
	}
	UWorld* W = World.Get();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADriftCityBuilder* City = W->SpawnActor<ADriftCityBuilder>(ADriftCityBuilder::StaticClass(), FTransform::Identity, SP);
	if (!TestNotNull(TEXT("city"), City))
	{
		return false;
	}
	City->Build();
	City->SpawnProps();

	TestEqual(TEXT("ramps"), City->GetRampCount(), 2);
	TestEqual(TEXT("outer loop corners"), City->GetLoopCorners().Num(), 4);
	AddInfo(FString::Printf(TEXT("DD_TEST props: %d lamps, %d crates, %d fences, %d hydrants"), City->CountProps(EDriftPropKind::Lamp),
		City->CountProps(EDriftPropKind::Crate), City->CountProps(EDriftPropKind::Fence), City->CountProps(EDriftPropKind::Hydrant)));
	TestTrue(TEXT("lamps"), City->CountProps(EDriftPropKind::Lamp) >= 16);
	TestTrue(TEXT("crates"), City->CountProps(EDriftPropKind::Crate) >= 20);
	TestTrue(TEXT("fences"), City->CountProps(EDriftPropKind::Fence) >= 12);
	TestTrue(TEXT("hydrants"), City->CountProps(EDriftPropKind::Hydrant) >= 6);

	int32 Suns = 0;
	int32 Skies = 0;
	bool bBloom = false;
	for (TActorIterator<ADirectionalLight> It(W); It; ++It)
	{
		++Suns;
	}
	for (TActorIterator<ASkyLight> It(W); It; ++It)
	{
		++Skies;
	}
	for (TActorIterator<APostProcessVolume> It(W); It; ++It)
	{
		bBloom = bBloom || (It->bUnbound && It->Settings.bOverride_BloomIntensity && It->Settings.BloomIntensity > 0.f);
	}
	// The city and the effects are instanced: without this flag cooked / -game builds draw the grey grid material.
	for (const TCHAR* Path : { TEXT("/Game/Materials/M_Solid.M_Solid"), TEXT("/Game/Materials/M_Glow.M_Glow") })
	{
		const UMaterial* Mat = LoadObject<UMaterial>(nullptr, Path);
		TestTrue(FString::Printf(TEXT("%s exists and is usable on instanced meshes"), Path), Mat && Mat->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
	}
	TestEqual(TEXT("one sun"), Suns, 1);
	TestEqual(TEXT("one sky light"), Skies, 1);
	TestTrue(TEXT("bloom post process"), bBloom);

	// The start is on the road.
	const FTransform Start = City->GetStartTransform();
	FHitResult Hit;
	const bool bGround = W->LineTraceSingleByObjectType(Hit, Start.GetLocation() + FVector(0.f, 0.f, 50.f), Start.GetLocation() - FVector(0.f, 0.f, 300.f), FCollisionObjectQueryParams(ECC_DriftGround));
	TestTrue(TEXT("road under the start"), bGround && FMath::Abs(Hit.ImpactPoint.Z) < 1.f);

	// A lane of the outer loop is free of walls all the way round (ramps sit in the middle of their roads).
	const TArray<FVector2D> Corners = City->GetLoopCorners();
	const FVector2D Mid = (Corners[0] + Corners[2]) * 0.5f;
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(190.f, 85.f, 35.f));
	for (int32 i = 0; i < 4; ++i)
	{
		auto Inset = [&](const FVector2D& C)
		{
			return C + FVector2D(FMath::Sign(Mid.X - C.X), FMath::Sign(Mid.Y - C.Y)) * 450.f;
		};
		const FVector2D A = Inset(Corners[i]);
		const FVector2D B = Inset(Corners[(i + 1) % 4]);
		const FQuat Rot = FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X)), 0.f).Quaternion();
		FHitResult Block;
		const bool bBlocked = W->SweepSingleByObjectType(Block, FVector(A.X, A.Y, 75.f), FVector(B.X, B.Y, 75.f), Rot, FCollisionObjectQueryParams(ECC_WorldStatic), Box);
		if (bBlocked)
		{
			AddInfo(FString::Printf(TEXT("DD_TEST side %d blocked by %s / %s (type %d) at %s"), i, *GetNameSafe(Block.GetActor()), *GetNameSafe(Block.GetComponent()),
				Block.GetComponent() ? static_cast<int32>(Block.GetComponent()->GetCollisionObjectType()) : -1, *Block.ImpactPoint.ToString()));
		}
		TestFalse(FString::Printf(TEXT("outer loop side %d has a free lane"), i), bBlocked);
	}

	// Driving straight from the start takes the first ramp: the car flies and lands.
	ADriftCarPawn* Car = SpawnCar(W, Start.GetLocation(), Start.Rotator().Yaw, 0.f);
	if (!TestNotNull(TEXT("car"), Car))
	{
		return false;
	}
	float MaxAir = 0.f;
	float MaxZ = 0.f;
	for (int32 i = 0; i < FMath::RoundToInt(4.f / Dt); ++i)
	{
		World.Tick(Dt);
		MaxAir = FMath::Max(MaxAir, Car->GetDriftMovement()->GetAirTime());
		MaxZ = FMath::Max(MaxZ, static_cast<float>(Car->GetActorLocation().Z));
	}
	AddInfo(FString::Printf(TEXT("DD_TEST ramp jump: longest air time %.2f s, highest z %.0f, car now at x %.0f, grounded %d"),
		MaxAir, MaxZ, Car->GetActorLocation().X, Car->GetDriftMovement()->IsGrounded() ? 1 : 0));
	TestTrue(TEXT("the ramp launches the car"), MaxAir > 0.4f && MaxZ > 75.f + 160.f);
	TestTrue(TEXT("the car lands again"), Car->GetDriftMovement()->IsGrounded());
	TestTrue(TEXT("the car keeps going after the landing"), Car->GetActorLocation().X > 7000.f);
	return true;
}

#endif
