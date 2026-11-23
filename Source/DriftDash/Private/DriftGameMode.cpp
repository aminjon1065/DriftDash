// DriftDash - game mode.

#include "DriftGameMode.h"
#include "DriftCarPawn.h"
#include "DriftCityBuilder.h"
#include "DriftFx.h"
#include "DriftHUD.h"
#include "DriftPlayerController.h"
#include "DriftScore.h"
#include "DriftTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

ADriftGameMode::ADriftGameMode()
{
	DefaultPawnClass = ADriftCarPawn::StaticClass();
	PlayerControllerClass = ADriftPlayerController::StaticClass();
	HUDClass = ADriftHUD::StaticClass();
}

void ADriftGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Built before the player logs in, so the car spawns on the finished road.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	City = GetWorld()->SpawnActor<ADriftCityBuilder>(ADriftCityBuilder::StaticClass(), FTransform::Identity, Params);
	if (!City)
	{
		ErrorMessage = TEXT("DriftDash: could not spawn the city");
		return;
	}
	City->Build();
	City->SpawnProps();
	Start = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), City->GetStartTransform(), Params);
	UE_LOG(LogDrift, Log, TEXT("City built: %d ramps, %d props"), City->GetRampCount(), City->GetProps().Num());
}

AActor* ADriftGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return Start ? Start.Get() : Super::ChoosePlayerStart_Implementation(Player);
}

void ADriftGameMode::RestartRun()
{
	if (UDriftFxSubsystem* Fx = UDriftFxSubsystem::Get(this))
	{
		Fx->Clear();
	}
	if (City)
	{
		City->SpawnProps();
	}
	if (UDriftScoreSubsystem* Score = UDriftScoreSubsystem::Get(this))
	{
		Score->ResetRun();
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADriftCarPawn* Car = Cast<ADriftCarPawn>(It->Get() ? It->Get()->GetPawn() : nullptr))
		{
			Car->ResetCar(City ? City->GetStartTransform() : FTransform::Identity);
		}
	}
	UE_LOG(LogDrift, Log, TEXT("Run restarted"));
}
