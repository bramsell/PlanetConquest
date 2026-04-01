// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "PlanetCameraPawn.generated.h"

UCLASS()
class PLANET_CONQUEST_API APlanetCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	APlanetCameraPawn();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Enhanced Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ZoomAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* RightClickPanAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* MiddleMouseAction;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USceneComponent* Root;

	// Planet camera setup
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* PlanetCameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* PlanetCamera;

	// City editor camera setup
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CityCameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* CityCamera;

	// Blend camera for smooth transitions
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* BlendCamera;

	// Orbital Movement Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float OrbitSpeed = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float EdgeOrbitSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float EdgePanBorderThickness = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float MousePanSensitivity = 0.35f;

	// Reference to the planet we're orbiting
	UPROPERTY(BlueprintReadOnly, Category = "Camera|Orbit")
	class APlanetActor* TargetPlanet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	FVector PlanetCenter = FVector(0.0f, 0.0f, 0.0f);

	// Zoom Settings (orbit distance from planet)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float ZoomSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MinOrbitDistance = 82000.0f; // Prevents clipping through planet (radius 100000)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float MaxOrbitDistance = 220000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	float CurrentOrbitDistance = 87000.0f; // Start close to planet surface

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Startup")
	float StartDistanceFromCity = 6000.0f; // Distance from player capital at startup (on planet surface)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Startup")
	float StartCameraPitch = -5.0f; // Camera pitch (down in degrees) when looking at capital at startup

	// Camera angle looking down at planet (lower = more angled, shows horizon)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float CameraPitch = 210.0f;  // Match CurrentCameraTilt default

	// Camera local tilt control
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LocalRotation")
	float CameraTiltSpeed = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LocalRotation")
	float MinCameraTilt = 100.0f; // Most horizontal/shallow angle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LocalRotation")
	float MaxCameraTilt = 260.0f; // Most steep/downward angle

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LocalRotation")
	float CameraRotateSpeed = 0.10f;

	// Current camera angles (relative to spring arm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|LocalRotation")
	float CurrentCameraTilt = 220.0f;  // 180 = looking at planet center, higher = angled down toward surface

	UPROPERTY(BlueprintReadOnly, Category = "Camera|LocalRotation")
	float CurrentCameraRotation = 0.0f; // Pawn roll - rotates spring arm to show different parts of horizon (0 = upright)

	// Current orbital position (angles around planet)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float CurrentYaw = 0.0f; // Yaw around planet (0=East, 90=North, 180=West, 270=South)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
	float CurrentPitch = 0.0f; // Pitch around planet (-85 to +85)

	// Lock camera input (useful when UI is open)
	UPROPERTY(BlueprintReadWrite, Category = "Camera|Input")
	bool bLockCameraInput = false;

	// City Editor Camera Mode
	UPROPERTY(BlueprintReadOnly, Category = "Camera|CityEditor")
	bool bCityEditorMode = false;

	UPROPERTY(BlueprintReadWrite, Category = "Camera|CityEditor")
	FVector CityCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CityEditor")
	float CityEditorMinOrbitDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CityEditor")
	float CityEditorMaxOrbitDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CityEditor")
	float CityEditorStartDistance = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CityEditor")
	float CityEditorMinCameraTilt = 170.0f; // Looking slightly up from horizontal

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CityEditor")
	float CityEditorMaxCameraTilt = 230.0f; // Looking down 50 degrees

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|CityEditor")
	float CityEditorBlendTime = 1.0f; // How long to blend between cameras

	// Saved planet camera state (for restoring when exiting city mode)
	FQuat SavedPlanetRotation = FQuat::Identity;
	float SavedPlanetCameraTilt = 180.0f;
	float SavedPlanetOrbitDistance = 150000.0f;

	// Saved city camera state (for restoring when re-entering city mode)
	bool bHasSavedCityState = false;
	FVector SavedCityCameraArmLocation = FVector::ZeroVector;
	FQuat SavedCityCameraArmRotation = FQuat::Identity;
	float SavedCityCameraArmLength = 7000.0f;
	FQuat SavedCityCameraRotation = FQuat::Identity;

	// Camera transition state
	bool bIsTransitioning = false;
	float TransitionProgress = 0.0f;
	FVector TransitionStartLocation = FVector::ZeroVector;
	FQuat TransitionStartRotation = FQuat::Identity;
	FVector TransitionEndLocation = FVector::ZeroVector;
	FQuat TransitionEndRotation = FQuat::Identity;
	bool bTransitionToCity = false; // true if transitioning to city, false if to planet

	// City editor camera state
	float CityCameraYaw = 0.0f;
	float CityCameraPitch = 60.0f; // Arm points up so camera looks down
	float CityCameraOrbitDistance = 10000.0f;
	float CityCameraTilt = 175.0f; // 180=horizontal, 195=15deg down (camera is rolled 180 to be right-side up)
	float CityCameraRotation = 0.0f;

public:
	// City editor camera control
	UFUNCTION(BlueprintCallable, Category = "Camera|CityEditor")
	void EnterCityEditorMode(FVector InCityCenter);

	UFUNCTION(BlueprintCallable, Category = "Camera|CityEditor")
	void ExitCityEditorMode();

private:
	// Input handlers
	void Move(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void StartRightClickPan(const FInputActionValue& Value);
	void StopRightClickPan(const FInputActionValue& Value);
	void StartMiddleMouseControl(const FInputActionValue& Value);
	void StopMiddleMouseControl(const FInputActionValue& Value);

	// Edge scrolling
	void HandleEdgeScrolling(float DeltaTime);

	// Camera controls
	void HandleMiddleMouseControl(float DeltaTime);

	// Right-click drag pan
	bool bIsRightClickPanning = false;
	FVector2D LastMousePosition;

	// Middle mouse camera control
	bool bIsMiddleMouseControlling = false;
	FVector2D LastMiddleMousePosition;

	// Current movement input
	FVector2D MoveInput = FVector2D::ZeroVector;
};
