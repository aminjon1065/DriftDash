// DriftDash - game mode: builds the city when the map loads, spawns the car at its start, quick restart.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DriftGameMode.generated.h"

class ADriftCityBuilder;
class APlayerStart;

UCLASS()
class DRIFTDASH_API ADriftGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADriftGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Soft restart (no level reload): props back, car to the start, score and effects cleared. */
	void RestartRun();

	ADriftCityBuilder* GetCity() const { return City; }

private:
	UPROPERTY()
	TObjectPtr<ADriftCityBuilder> City;

	UPROPERTY()
	TObjectPtr<APlayerStart> Start;
};
