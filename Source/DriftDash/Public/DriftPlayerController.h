// DriftDash - reads touches, the mouse and keys every frame and hands the resolved input to the car
// (DriftInput::Resolve); the restart button and R restart the run.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DriftCarModel.h"
#include "DriftInput.h"
#include "DriftPlayerController.generated.h"

UCLASS()
class DRIFTDASH_API ADriftPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADriftPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	const FDriftInputFrame& GetDriftInput() const { return Current; }
	FDriftHudLayout GetHudLayout() const;
	void RequestRestart();

private:
	/** Dev checks on Mac (Scripts/play_mac.sh ... -DDAutopilot -DDShots=1,2.5 -DDQuit=12): scripted input,
	 *  screenshots to Saved/Screenshots at those seconds, quit after N seconds. */
	FDriftInputFrame AutopilotInput(float Time) const;
	void TickDevChecks(float DeltaTime);

	float PlayTime = 0.f;
	bool bAutopilot = false;
	TArray<float> ShotTimes;
	int32 NextShot = 0;
	float QuitAfter = -1.f;

	/** Pointer ids in press order: touch index, MouseId, KeyLeftId, KeyRightId. */
	TArray<int32> PressOrder;
	/** Pointers that started on the restart button (they never steer). */
	TSet<int32> ButtonPointers;
	FDriftInputFrame Current;

	static constexpr int32 MouseId = 100;
	static constexpr int32 KeyLeftId = 200;
	static constexpr int32 KeyRightId = 201;
};
