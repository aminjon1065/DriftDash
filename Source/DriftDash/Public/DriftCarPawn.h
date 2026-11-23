// DriftDash - the player's car: collision box + UDriftCarMovement, a stylised body built from basic shapes (body
// roll in turns, steering front wheels, nitro flames), tyre smoke and skid marks while drifting, and its own chase
// camera (high behind, smooth lag, leaning toward the slide).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DriftCarModel.h"
#include "DriftCarPawn.generated.h"

class UBoxComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UDriftCarMovement;
class UDriftSkidMarks;
class UPawnMovementComponent;

UCLASS()
class DRIFTDASH_API ADriftCarPawn : public APawn
{
	GENERATED_BODY()

public:
	ADriftCarPawn();

	/** Camera pitch below the horizon, deg (35-45 per the brief). */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraPitch = 40.f;

	/** Camera distance from its focus point at cruise speed, cm. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraDistance = 1700.f;

	/** Vertical field of view, deg (the horizontal one follows the screen's aspect). */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraVerticalFov = 68.f;

	/** How far the focus shifts toward the slide at a full drift, cm. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraSlideOffset = 260.f;

	void SetDriftInput(const FDriftInputFrame& In);

	/** Teleports to Transform (yaw only), stops, clears the marks and snaps the camera. */
	void ResetCar(const FTransform& Transform);

	UDriftCarMovement* GetDriftMovement() const { return Movement; }
	UBoxComponent* GetBody() const { return Body; }
	UDriftSkidMarks* GetSkidMarks() const { return Skids; }
	UCameraComponent* GetCamera() const { return Camera; }
	float GetBodyRoll() const { return BodyRoll; }
	float GetFrontWheelYaw() const { return WheelYaw; }

	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void OnBodyOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void BuildVisuals();
	UStaticMeshComponent* AddVisual(USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
	void UpdateVisuals(float Dt);
	void UpdateEffects(float Dt);
	void UpdateCamera(float Dt, bool bSnap);

	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<USceneComponent> Chassis;

	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<UDriftCarMovement> Movement;

	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<UDriftSkidMarks> Skids;

	UPROPERTY(VisibleAnywhere, Category = "Car")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> FrontWheels;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Flames;

	float BodyRoll = 0.f;
	float BodyPitch = 0.f;
	float LandingDip = 0.f;
	float WheelYaw = 0.f;
	float SmokeAccum = 0.f;
	float ShakeTime = 0.f;
	float ShakeStrength = 0.f;
	float CameraYaw = 0.f;
	FVector CameraFocus = FVector::ZeroVector;
	FVector CameraLocation = FVector::ZeroVector;
	bool bCameraSnapped = false;
	bool bVisualsBuilt = false;
};
