// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PlanetCameraPawn.h"
#include "PlanetActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "../Entities/Cities/CityActor.h"
#include "../Entities/Buildings/CapitalBuildingActor.h"

APlanetCameraPawn::APlanetCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component at planet center
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// ===== PLANET CAMERA SETUP =====
	PlanetCameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("PlanetCameraArm"));
	PlanetCameraArm->SetupAttachment(RootComponent);
	PlanetCameraArm->TargetArmLength = CurrentOrbitDistance;
	PlanetCameraArm->bDoCollisionTest = false;
	PlanetCameraArm->bEnableCameraLag = true;
	PlanetCameraArm->CameraLagSpeed = 10.0f;

	PlanetCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PlanetCamera"));
	PlanetCamera->SetupAttachment(PlanetCameraArm, USpringArmComponent::SocketName);
	PlanetCamera->bUsePawnControlRotation = false;
	PlanetCamera->SetRelativeRotation(FRotator(CurrentCameraTilt, 180.0f, 0.0f));

	// ===== CITY EDITOR CAMERA SETUP =====
	CityCameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CityCameraArm"));
	CityCameraArm->SetupAttachment(RootComponent);
	CityCameraArm->TargetArmLength = CityEditorStartDistance;
	CityCameraArm->bDoCollisionTest = false;
	CityCameraArm->bEnableCameraLag = true; // Enable lag for smooth controls
	CityCameraArm->CameraLagSpeed = 10.0f;

	CityCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CityCamera"));
	CityCamera->SetupAttachment(CityCameraArm, USpringArmComponent::SocketName);
	CityCamera->bUsePawnControlRotation = false;
	CityCamera->SetRelativeRotation(FRotator(CityCameraTilt, 180.0f, 180.0f)); // Roll 180 to flip right-side up
	CityCamera->SetActive(false); // Start inactive

	// ===== BLEND CAMERA SETUP =====
	BlendCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BlendCamera"));
	BlendCamera->SetupAttachment(RootComponent);
	BlendCamera->bUsePawnControlRotation = false;
	BlendCamera->SetActive(false); // Only active during transitions
}

void APlanetCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	
	// Find the planet in the world
	for (TActorIterator<APlanetActor> It(GetWorld()); It; ++It)
	{
		TargetPlanet = *It;
		break; // Use first planet found
	}

	// If we found a planet, position ourselves at its center (updated later for player city)
	if (TargetPlanet)
	{
		PlanetCenter = TargetPlanet->GetActorLocation();
		SetActorLocation(PlanetCenter);
		
		UE_LOG(LogTemp, Log, TEXT("Camera attached to planet at location: %s"), *PlanetCenter.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No PlanetActor found in level! Camera will orbit around (0,0,0)"));
		SetActorLocation(PlanetCenter);
	}
	
	// Find player's capital city to orient camera toward it initially
	ACityActor* PlayerCity = nullptr;
	for (TActorIterator<ACityActor> It(GetWorld()); It; ++It)
	{
		if (It->OwnerTeam == EOwnerTeam::Player)
		{
			PlayerCity = *It;
			break;
		}
	}
	
	if (PlayerCity && PlayerCity->CapitalBuilding && TargetPlanet)
	{
		// Get capital building location and direction from planet center
		FVector CapitalLocation = PlayerCity->CapitalBuilding->GetActorLocation();
		FVector CapitalDirection = (CapitalLocation - PlanetCenter).GetSafeNormal();
		
		float PlanetRadius = TargetPlanet->PlanetRadius;
		
		// Get the continent center to orient camera toward it
		FVector ContinentCenter = FVector::ZeroVector;
		if (TargetPlanet->ContinentSeeds.Num() > 0)
		{
			// Find largest land continent (same as cluster spawning)
			int32 TargetContinentId = -1;
			float LargestSize = 0.0f;
			for (int32 i = 0; i < TargetPlanet->ContinentSeeds.Num(); i++)
			{
				if (TargetPlanet->ContinentSeeds[i].bIsLand && TargetPlanet->ContinentSeeds[i].Size > LargestSize)
				{
					LargestSize = TargetPlanet->ContinentSeeds[i].Size;
					TargetContinentId = i;
				}
			}
			
			if (TargetContinentId >= 0)
			{
				ContinentCenter = TargetPlanet->ContinentSeeds[TargetContinentId].Position;
			}
		}
		
		// Calculate angular offset to be StartDistanceFromCity units away from capital (on planet surface)
		float AngularOffset = StartDistanceFromCity / PlanetRadius; // Radians
		
		// Calculate direction from continent center to capital (to position camera in opposite direction)
		FVector ContinentToCapital = (CapitalDirection - ContinentCenter).GetSafeNormal();
		
		// Create a tangent vector in the direction away from continent center
		FVector Tangent = FVector::CrossProduct(CapitalDirection, ContinentToCapital).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::CrossProduct(CapitalDirection, FVector::RightVector).GetSafeNormal();
		}
		
		// Rotate around the planet to offset from capital in the direction away from continent center
		FVector RotationAxis = Tangent; // Rotate around this axis
		FQuat OffsetRotation(RotationAxis, AngularOffset);
		FVector CameraDirection = OffsetRotation.RotateVector(CapitalDirection);
		
		// Adjust camera direction to be in opposite direction from continent center
		// We want camera on the far side of capital from continent center
		FVector CapitalToContinentCenter = (ContinentCenter - CapitalDirection).GetSafeNormal();
		FVector OppositeFromCenter = -CapitalToContinentCenter;
		
		// Create rotation axis perpendicular to both capital direction and opposite-from-center direction
		FVector PlacementAxis = FVector::CrossProduct(CapitalDirection, OppositeFromCenter).GetSafeNormal();
		if (PlacementAxis.IsNearlyZero())
		{
			PlacementAxis = FVector::CrossProduct(CapitalDirection, FVector::UpVector).GetSafeNormal();
		}
		
		// Rotate capital direction around placement axis to offset away from continent center
		FQuat FinalOffsetRotation(PlacementAxis, AngularOffset);
		CameraDirection = FinalOffsetRotation.RotateVector(CapitalDirection);
		
		UE_LOG(LogTemp, Warning, TEXT("=== CAMERA POSITIONING DEBUG ==="));
		UE_LOG(LogTemp, Warning, TEXT("Continent Center: %s"), *ContinentCenter.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Capital Direction: %s"), *CapitalDirection.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Offset Camera Direction: %s"), *CameraDirection.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Angular Offset: %.4f radians (%.2f degrees)"), AngularOffset, FMath::RadiansToDegrees(AngularOffset));
		
		// Point the pawn AWAY from camera direction (so SpringArm extends there)
		FVector OppositeDirection = -CameraDirection;
		
		// Calculate actual camera world position (where camera will be at end of spring arm)
		FVector ActualCameraPosition = PlanetCenter + (OppositeDirection * CurrentOrbitDistance);
		
		// Calculate the pawn's rotation with proper roll for surface alignment
		// Pawn forward: extends spring arm to camera position
		FVector PawnForward = OppositeDirection;
		
		// Pawn up: perpendicular to pawn forward, chosen to align camera horizon to surface
		// We want the camera (with yaw=180°) to see the capital with proper roll
		// Camera will look at -PawnForward direction when yaw=180°
		// For proper roll, we need to find the right "up" vector for the pawn
		
		// Calculate vectors for pawn orientation
		FVector CameraToCapital = (CapitalLocation - ActualCameraPosition).GetSafeNormal();
		FVector RadialUp = ActualCameraPosition.GetSafeNormal();  // Surface normal at camera position
		
		// The camera's right vector (when looking at capital) should be perpendicular to both
		// the look direction and the radial up vector
		FVector CameraRight = FVector::CrossProduct(CameraToCapital, RadialUp).GetSafeNormal();
		
		// The pawn's up should be oriented so the camera (at yaw=180°) has this right vector
		// Camera at yaw=180° has its right vector aligned with -PawnRight
		// So PawnRight should equal -CameraRight
		FVector PawnRight = -CameraRight;
		FVector PawnUp = FVector::CrossProduct(PawnForward, PawnRight).GetSafeNormal();
		
		// Create pawn rotation from forward and up vectors
		FMatrix PawnRotationMatrix = FRotationMatrix::MakeFromXZ(PawnForward, PawnUp);
		FRotator PawnRotation = PawnRotationMatrix.Rotator();
		
		CurrentYaw = PawnRotation.Yaw;
		CurrentPitch = PawnRotation.Pitch;
		CurrentCameraRotation = PawnRotation.Roll;
		
		SetActorRotation(PawnRotation);
		SetActorLocation(PlanetCenter);
		
		// Set camera with constant yaw=180° and roll=0°, only pitch varies
		CurrentCameraTilt += StartCameraPitch;
		
		if (PlanetCamera)
		{
			PlanetCamera->SetRelativeRotation(FRotator(CurrentCameraTilt, 180.0f, 0.0f));
			
			UE_LOG(LogTemp, Warning, TEXT(">>> CAMERA POSITIONING COMPLETE"));
			UE_LOG(LogTemp, Warning, TEXT("  Camera Position: %s"), *ActualCameraPosition.ToString());
			UE_LOG(LogTemp, Warning, TEXT("  Capital Position: %s"), *CapitalLocation.ToString());
			UE_LOG(LogTemp, Warning, TEXT("  Pawn Rotation (Pitch/Yaw/Roll): %s"), *PawnRotation.ToString());
			UE_LOG(LogTemp, Warning, TEXT("  Camera Relative Rotation (constant yaw=180, roll=0): %s"), *PlanetCamera->GetRelativeRotation().ToString());
			UE_LOG(LogTemp, Warning, TEXT("  Camera World Rotation: %s"), *PlanetCamera->GetComponentRotation().ToString());
			UE_LOG(LogTemp, Warning, TEXT("  CurrentCameraTilt: %.1f"), CurrentCameraTilt);
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Final Rotation - Pitch: %.1f, Yaw: %.1f, Roll: %.1f"), 
			CurrentPitch, CurrentYaw, CurrentCameraRotation);
		UE_LOG(LogTemp, Warning, TEXT("================================="));
		
		// Log capital building and camera positions/rotations for debugging
		UE_LOG(LogTemp, Warning, TEXT("=== CAPITAL & CAMERA DEBUG ==="));
		UE_LOG(LogTemp, Warning, TEXT("Capital Building Position: %s"), *PlayerCity->CapitalBuilding->GetActorLocation().ToString());
		UE_LOG(LogTemp, Warning, TEXT("Capital Building Quaternion: %s"), *PlayerCity->CapitalBuilding->GetActorQuat().ToString());
		
		if (PlanetCamera)
		{
			UE_LOG(LogTemp, Warning, TEXT("Camera World Position: %s"), *PlanetCamera->GetComponentLocation().ToString());
			UE_LOG(LogTemp, Warning, TEXT("Camera World Quaternion: %s"), *PlanetCamera->GetComponentQuat().ToString());
		}
		UE_LOG(LogTemp, Warning, TEXT("================================="));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Camera fallback: PlayerCity=%s, CapitalBuilding=%s, TargetPlanet=%s"),
			PlayerCity ? TEXT("Valid") : TEXT("NULL"),
			(PlayerCity && PlayerCity->CapitalBuilding) ? TEXT("Valid") : TEXT("NULL"),
			TargetPlanet ? TEXT("Valid") : TEXT("NULL"));
		
		// Fallback: Position pawn at planet center with default rotation
		SetActorLocation(PlanetCenter);
		FRotator InitialRotation = FRotator(CurrentPitch, CurrentYaw, CurrentCameraRotation);
		SetActorRotation(InitialRotation);
		
		// Apply default camera tilt and rotation
		if (PlanetCamera)
		{
			PlanetCamera->SetRelativeRotation(FRotator(CurrentCameraTilt, 180.0f, 0.0f));
		}
	}
	
	// Set initial orbit distance
	if (PlanetCameraArm)
	{
		PlanetCameraArm->TargetArmLength = CurrentOrbitDistance;
	}

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (InputMappingContext)
			{
				Subsystem->AddMappingContext(InputMappingContext, 0);
			}
		}
	}
}

void APlanetCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Pawn always stays at planet center
	SetActorLocation(PlanetCenter);

	// Handle WASD/Arrow key orbital rotation
	if (!MoveInput.IsZero())
	{
		if (bCityEditorMode)
		{
			// In city mode, orbit around the city
			// Camera is rolled 180° to be right-side up, so controls are adjusted
			// Left/Right: Rotate around the city (change yaw)
			CityCameraYaw -= MoveInput.X * OrbitSpeed * DeltaTime;
			
			// Up/Down: Adjust viewing angle (tilt camera arm, inverted due to roll)
			// Range: -75° to -10° gives good overhead viewing without flipping
			CityCameraPitch = FMath::Clamp(CityCameraPitch - (MoveInput.Y * OrbitSpeed * DeltaTime), -75.0f, -10.0f);
		}
		else
		{
			// Planet mode: Camera-relative "drag the ground" movement
			// Get current pawn rotation (determines camera facing direction)
			FRotator CurrentRotation = GetActorRotation();
			
			// Get camera's forward and right directions (horizontal plane only)
			FVector CameraForward = CurrentRotation.Vector();
			FVector CameraRight = FRotationMatrix(CurrentRotation).GetScaledAxis(EAxis::Y);
			
			// Project onto horizontal plane (remove vertical component for surface movement)
			CameraForward.Z = 0;
			CameraForward.Normalize();
			CameraRight.Z = 0;
			CameraRight.Normalize();
			
			// Calculate movement direction in world space
			// Inverted because we're "dragging the ground" not moving the camera
			// MoveInput.Y: positive = forward, negative = backward
			// MoveInput.X: positive = right, negative = left
			FVector WorldMovement = (-MoveInput.Y * CameraForward) + (-MoveInput.X * CameraRight);
			
			// Current camera position in world space
			FVector CurrentCameraPos = PlanetCenter + (CurrentRotation.Vector() * CurrentOrbitDistance);
			
			// Calculate movement speed (scaled by distance)
			float DistanceRange = MaxOrbitDistance - MinOrbitDistance;
			float DistanceRatio = (CurrentOrbitDistance - MinOrbitDistance) / DistanceRange;
			float ScaledOrbitSpeed = OrbitSpeed * (0.3f + (DistanceRatio * 0.7f));
			
			// Move camera position along the surface
			FVector NewCameraPos = CurrentCameraPos + (WorldMovement * ScaledOrbitSpeed * DeltaTime);
			
			// Project back onto sphere at the correct orbit distance
			FVector DirectionFromCenter = NewCameraPos - PlanetCenter;
			DirectionFromCenter.Normalize();
			DirectionFromCenter *= CurrentOrbitDistance;
			
			// Calculate new spherical coordinates (yaw/pitch) from this direction
			// Yaw: angle in the XY plane
			CurrentYaw = FMath::Atan2(DirectionFromCenter.Y, DirectionFromCenter.X) * (180.0f / PI);
			
			// Pitch: angle from XY plane toward Z axis
			float HorizontalDist = FMath::Sqrt(DirectionFromCenter.X * DirectionFromCenter.X + DirectionFromCenter.Y * DirectionFromCenter.Y);
			CurrentPitch = FMath::Atan2(DirectionFromCenter.Z, HorizontalDist) * (180.0f / PI);
			// No clamping - allow going over the poles
			
			// Apply the new rotation
			SetActorRotation(FRotator(CurrentPitch, CurrentYaw, CurrentCameraRotation));
		}
	}
	
	// Apply rotation to appropriate camera system
	if (bCityEditorMode)
	{
		// Orient pawn so local Z = city surface normal; yaw then orbits around that axis
		FVector CityNormal = (CityCenter - PlanetCenter).GetSafeNormal();
		SetActorRotation(FRotationMatrix::MakeFromZ(CityNormal).ToQuat());
		
		if (CityCameraArm)
		{
			// Arm pivot in pawn local space: city is directly along local Z at CityDist
			float CityDist = FVector::Dist(CityCenter, PlanetCenter);
			CityCameraArm->SetRelativeLocation(FVector(0.0f, 0.0f, CityDist));
			CityCameraArm->SetRelativeRotation(FRotator(CityCameraPitch, CityCameraYaw, 0.0f));
		}
	}
	// For planet mode, rotation is handled directly by input handlers
	// Don't apply rotation here - it causes spinning
	
	// Periodic camera diagnostic logging (every 0.2s)
	static float LogTimer = 0.0f;
	LogTimer += DeltaTime;
	if (LogTimer >= 0.2f)
	{
		LogTimer = 0.0f;
		
		if (!bCityEditorMode && PlanetCamera && PlanetCameraArm)
		{
			FVector PawnPos = GetActorLocation();
			FRotator PawnRot = GetActorRotation();
			FVector ArmPos = PlanetCameraArm->GetComponentLocation();
			FRotator ArmRot = PlanetCameraArm->GetComponentRotation();
			FVector CamPos = PlanetCamera->GetComponentLocation();
			FRotator CamWorldRot = PlanetCamera->GetComponentRotation();
			FRotator CamRelRot = PlanetCamera->GetRelativeRotation();
			FVector CamForward = PlanetCamera->GetForwardVector();
			FVector DirectionToPlanet = (PlanetCenter - CamPos).GetSafeNormal();
			float DotProduct = FVector::DotProduct(CamForward, DirectionToPlanet);
			
			// UE_LOG(LogTemp, Warning, TEXT("=== CAMERA DEBUG ==="));
			// UE_LOG(LogTemp, Warning, TEXT("Pawn: Pos=%s Rot=P:%.1f Y:%.1f R:%.1f"), *PawnPos.ToCompactString(), PawnRot.Pitch, PawnRot.Yaw, PawnRot.Roll);
			// UE_LOG(LogTemp, Warning, TEXT("Arm: Pos=%s Rot=P:%.1f Y:%.1f R:%.1f"), *ArmPos.ToCompactString(), ArmRot.Pitch, ArmRot.Yaw, ArmRot.Roll);
			// UE_LOG(LogTemp, Warning, TEXT("Cam: Pos=%s"), *CamPos.ToCompactString());
			// UE_LOG(LogTemp, Warning, TEXT("Cam World Rot: P:%.1f Y:%.1f R:%.1f"), CamWorldRot.Pitch, CamWorldRot.Yaw, CamWorldRot.Roll);
			// UE_LOG(LogTemp, Warning, TEXT("Cam Relative Rot: P:%.1f Y:%.1f R:%.1f"), CamRelRot.Pitch, CamRelRot.Yaw, CamRelRot.Roll);
			// UE_LOG(LogTemp, Warning, TEXT("Cam Forward: %s | DirToPlanet: %s | Dot: %.3f (1.0=facing planet)"), *CamForward.ToCompactString(), *DirectionToPlanet.ToCompactString(), DotProduct);
			// UE_LOG(LogTemp, Warning, TEXT("CurrentYaw=%.1f CurrentPitch=%.1f CurrentCameraTilt=%.1f CurrentCameraRotation=%.1f"), CurrentYaw, CurrentPitch, CurrentCameraTilt, CurrentCameraRotation);
		}
	}

	// Handle right-click drag panning
	if (bIsRightClickPanning)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			if (!PC->IsInputKeyDown(EKeys::RightMouseButton))
			{
				bIsRightClickPanning = false;
			}
			else
			{
				float MouseX, MouseY;
				PC->GetMousePosition(MouseX, MouseY);
				FVector2D CurrentMousePosition(MouseX, MouseY);
				FVector2D MouseDelta = CurrentMousePosition - LastMousePosition;
				
				// Log right-click panning activity
				if (MouseDelta.Size() > 0.1f)
				{
					// UE_LOG(LogTemp, Warning, TEXT("RIGHT CLICK DRAG: Delta X=%f Y=%f | CurrentYaw=%.1f CurrentPitch=%.1f"), 
					// 	MouseDelta.X, MouseDelta.Y, CurrentYaw, CurrentPitch);
				}
				
				if (bCityEditorMode)
				{
					// City camera is rolled 180° - yaw is NOT inverted, pitch IS inverted
					CityCameraYaw += MouseDelta.X * MousePanSensitivity * DeltaTime * 60.0f;
					// Match the WASD limits: -75° to -10°
					CityCameraPitch = FMath::Clamp(CityCameraPitch - (MouseDelta.Y * MousePanSensitivity * DeltaTime * 60.0f), -75.0f, -10.0f);
				}
				else
				{
					// Planet mode: Simple orbital rotation around camera axes
					// Just rotate the pawn - no yaw/pitch conversion needed
					
					float DistanceRange = MaxOrbitDistance - MinOrbitDistance;
					float DistanceRatio = (CurrentOrbitDistance - MinOrbitDistance) / DistanceRange;
					// Scale sensitivity: 10% when zoomed in, 100% when zoomed out
					float ScaledPanSensitivity = MousePanSensitivity * (0.001f + (DistanceRatio * 0.999f));
					
					// Get the active camera
					UCameraComponent* ActiveCamera = bCityEditorMode ? CityCamera : PlanetCamera;
					if (!ActiveCamera)
					{
						return;
					}
					
					// Get the camera's current world orientation
					FRotator CameraRotation = ActiveCamera->GetComponentRotation();
					FVector CameraRight = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
					FVector CameraUp = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);
					
					// Calculate rotation angles
					float VerticalAngle = MouseDelta.Y * ScaledPanSensitivity * DeltaTime * 60.0f;
					float HorizontalAngle = MouseDelta.X * ScaledPanSensitivity * DeltaTime * 60.0f;
					
					// Rotate pawn around camera's right axis (for vertical movement)
					AddActorWorldRotation(FQuat(CameraRight, FMath::DegreesToRadians(VerticalAngle)));
					
					// Rotate pawn around camera's up axis (for horizontal movement)
					AddActorWorldRotation(FQuat(CameraUp, FMath::DegreesToRadians(HorizontalAngle)));
					
					// Update stored yaw/pitch from final rotation (for other systems that need them)
					// Note: FRotator will clamp pitch to ±90, but the actual quaternion rotation is preserved
					FRotator FinalRotation = GetActorRotation();
					CurrentYaw = FinalRotation.Yaw;
					CurrentPitch = FinalRotation.Pitch;
					
					// UE_LOG(LogTemp, Warning, TEXT("RIGHT CLICK UPDATE: MouseDelta(%.1f,%.1f) | Yaw %.1f | Pitch %.1f | ActualQuat= %s"), 
					// 	MouseDelta.X, MouseDelta.Y, CurrentYaw, CurrentPitch, *GetActorQuat().ToString());
				}
				
				LastMousePosition = CurrentMousePosition;
			}
		}
	}

	// Handle middle mouse camera control (tilt and rotate)
	HandleMiddleMouseControl(DeltaTime);
}

void APlanetCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind Enhanced Input actions
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind movement
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlanetCameraPawn::Move);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &APlanetCameraPawn::Move);
		}

		// Bind zoom
		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &APlanetCameraPawn::Zoom);
		}

		// Bind right-click pan
		if (RightClickPanAction)
		{
			EnhancedInputComponent->BindAction(RightClickPanAction, ETriggerEvent::Started, this, &APlanetCameraPawn::StartRightClickPan);
			EnhancedInputComponent->BindAction(RightClickPanAction, ETriggerEvent::Completed, this, &APlanetCameraPawn::StopRightClickPan);
		}

		// Bind middle mouse camera control
		if (MiddleMouseAction)
		{
			EnhancedInputComponent->BindAction(MiddleMouseAction, ETriggerEvent::Started, this, &APlanetCameraPawn::StartMiddleMouseControl);
			EnhancedInputComponent->BindAction(MiddleMouseAction, ETriggerEvent::Completed, this, &APlanetCameraPawn::StopMiddleMouseControl);
		}
	}
}

void APlanetCameraPawn::Move(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
}

void APlanetCameraPawn::Zoom(const FInputActionValue& Value)
{
	float ZoomValue = Value.Get<float>();
	
	// Use different zoom limits based on camera mode
	float ActiveMinDistance = bCityEditorMode ? CityEditorMinOrbitDistance : MinOrbitDistance;
	float ActiveMaxDistance = bCityEditorMode ? CityEditorMaxOrbitDistance : MaxOrbitDistance;
	
	if (bCityEditorMode)
	{
		// City mode: Update city camera distance
		float DistanceRange = ActiveMaxDistance - ActiveMinDistance;
		float DistanceRatio = (CityCameraOrbitDistance - ActiveMinDistance) / DistanceRange;
		float ScaledZoomSpeed = (ZoomSpeed * 0.2f) * (0.3f + (DistanceRatio * 0.7f));
		
		CityCameraOrbitDistance = FMath::Clamp(CityCameraOrbitDistance - (ZoomValue * ScaledZoomSpeed), ActiveMinDistance, ActiveMaxDistance);
		
		if (CityCameraArm)
		{
			CityCameraArm->TargetArmLength = CityCameraOrbitDistance;
		}
	}
	else
	{
		// Planet mode: Update planet camera distance
		float DistanceRange = ActiveMaxDistance - ActiveMinDistance;
		float DistanceRatio = (CurrentOrbitDistance - ActiveMinDistance) / DistanceRange;
		float ScaledZoomSpeed = ZoomSpeed * (0.3f + (DistanceRatio * 0.7f));
		
		CurrentOrbitDistance = FMath::Clamp(CurrentOrbitDistance - (ZoomValue * ScaledZoomSpeed), ActiveMinDistance, ActiveMaxDistance);
		
		if (PlanetCameraArm)
		{
			PlanetCameraArm->TargetArmLength = CurrentOrbitDistance;
		}
	}
}

void APlanetCameraPawn::StartRightClickPan(const FInputActionValue& Value)
{
	bIsRightClickPanning = true;
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		float MouseX, MouseY;
		PC->GetMousePosition(MouseX, MouseY);
		LastMousePosition = FVector2D(MouseX, MouseY);
		// UE_LOG(LogTemp, Warning, TEXT("RIGHT CLICK PAN STARTED at position: X=%f, Y=%f"), MouseX, MouseY);
	}
}

void APlanetCameraPawn::StopRightClickPan(const FInputActionValue& Value)
{
	bIsRightClickPanning = false;
	UE_LOG(LogTemp, Warning, TEXT("RIGHT CLICK PAN STOPPED"));
}

void APlanetCameraPawn::StartMiddleMouseControl(const FInputActionValue& Value)
{
	bIsMiddleMouseControlling = true;
	UE_LOG(LogTemp, Warning, TEXT("Middle Mouse Button PRESSED - Starting camera control"));
	UE_LOG(LogTemp, Warning, TEXT("Camera state: Tilt=%f, Rotation=%f"), CurrentCameraTilt, CurrentCameraRotation);
	
	// Use the appropriate camera based on current mode
	UCameraComponent* ActiveCamera = bCityEditorMode ? CityCamera : PlanetCamera;
	if (ActiveCamera)
	{
		FRotator CurrentRot = ActiveCamera->GetRelativeRotation();
		UE_LOG(LogTemp, Warning, TEXT("Camera actual rotation: Pitch=%f, Yaw=%f, Roll=%f"), CurrentRot.Pitch, CurrentRot.Yaw, CurrentRot.Roll);
	}
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		float MouseX, MouseY;
		PC->GetMousePosition(MouseX, MouseY);
		LastMiddleMousePosition = FVector2D(MouseX, MouseY);
		//UE_LOG(LogTemp, Log, TEXT("Initial mouse position: X=%f, Y=%f"), MouseX, MouseY);
	}
}

void APlanetCameraPawn::StopMiddleMouseControl(const FInputActionValue& Value)
{
	bIsMiddleMouseControlling = false;
	UE_LOG(LogTemp, Warning, TEXT("Middle Mouse Button RELEASED - Stopping camera control"));
}

void APlanetCameraPawn::HandleMiddleMouseControl(float DeltaTime)
{
	if (!bIsMiddleMouseControlling) return;
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	
	// Safety check: if middle mouse button isn't actually held down, reset the flag
	// This fixes the bug where dragging off-screen and releasing doesn't fire the Completed event
	if (!PC->IsInputKeyDown(EKeys::MiddleMouseButton))
	{
		bIsMiddleMouseControlling = false;
		return;
	}
	
	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);
	FVector2D CurrentMousePosition(MouseX, MouseY);
	
	FVector2D MouseDelta = CurrentMousePosition - LastMiddleMousePosition;
	
	// Safety check: ignore huge deltas (first frame issues)
	if (MouseDelta.Size() > 100.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("LARGE DELTA DETECTED: %f - Ignoring this frame"), MouseDelta.Size());
		LastMiddleMousePosition = CurrentMousePosition;
		return;
	}
	
	// Ignore tiny movements (deadzone)
	if (MouseDelta.Size() < 0.1f)
	{
		return;
	}
	
	// Apply horizontal movement to camera rotation (spin to look at different horizons)
	// Inverted: mouse left rotates right, mouse right rotates left (reversed direction)
	float OldCameraRotation = CurrentCameraRotation;
	CurrentCameraRotation += MouseDelta.X * CameraRotateSpeed;
	
	// Wrap rotation to 0-360 range
	while (CurrentCameraRotation >= 360.0f)
	{
		CurrentCameraRotation -= 360.0f;
	}
	while (CurrentCameraRotation < 0.0f)
	{
		CurrentCameraRotation += 360.0f;
	}
	
	// If horizon rotation changed, apply it to the pawn's roll (only in planet mode)
	if (!bCityEditorMode && FMath::Abs(CurrentCameraRotation - OldCameraRotation) > 0.01f)
	{
		// Rotate the pawn around its forward vector to change the horizon orientation
		FQuat CurrentQuat = GetActorQuat();
		FVector Forward = CurrentQuat.GetForwardVector();
		float RotationChange = CurrentCameraRotation - OldCameraRotation;
		FQuat RollChange = FQuat(Forward, FMath::DegreesToRadians(RotationChange));
		SetActorRotation(RollChange * CurrentQuat);
	}
	
	// Apply vertical movement to camera tilt (adjust viewing angle up/down)
	// Inverted: moving mouse up should tilt down (toward planet), mouse down should tilt up (away from planet)
	// Use city editor specific limits when in city mode
	if (bCityEditorMode)
	{
		// City mode: adjust CityCameraTilt (inverted due to 180° camera roll)
		float MinTilt = CityEditorMinCameraTilt;
		float MaxTilt = CityEditorMaxCameraTilt;
		CityCameraTilt = FMath::Clamp(CityCameraTilt - (MouseDelta.Y * CameraTiltSpeed), MinTilt, MaxTilt);
	}
	else
	{
		// Planet mode: adjust CurrentCameraTilt
		float MinTilt = MinCameraTilt;
		float MaxTilt = MaxCameraTilt;
		CurrentCameraTilt = FMath::Clamp(CurrentCameraTilt + (MouseDelta.Y * CameraTiltSpeed), MinTilt, MaxTilt);
	}
	
	// Apply to the appropriate camera based on current mode
	UCameraComponent* ActiveCamera = bCityEditorMode ? CityCamera : PlanetCamera;
	if (ActiveCamera)
	{
		// Camera needs tilt (pitch), look back (yaw=180), and roll
		// City camera gets roll=180 to be right-side up, planet camera gets roll=0
		float CameraRoll = bCityEditorMode ? 180.0f : 0.0f;
		float CameraTilt = bCityEditorMode ? CityCameraTilt : CurrentCameraTilt;
		FRotator NewRotation = FRotator(CameraTilt, 180.0f, CameraRoll);
		ActiveCamera->SetRelativeRotation(NewRotation);
		
		// Debug: verify it was applied
		FRotator ActualRotation = ActiveCamera->GetRelativeRotation();
		//UE_LOG(LogTemp, Log, TEXT("Camera tilt update - Mode: %s, Tilt: %.1f, Roll: %.1f, Actual: %s"), 
		//	bCityEditorMode ? TEXT("CITY") : TEXT("PLANET"), 
		//	CameraTilt, CameraRoll, *ActualRotation.ToString());
	}
	
	LastMiddleMousePosition = CurrentMousePosition;
}

void APlanetCameraPawn::HandleEdgeScrolling(float DeltaTime)
{
	// Don't pan if camera input is locked (e.g., UI is open)
	if (bLockCameraInput)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Get viewport size
	int32 ViewportSizeX, ViewportSizeY;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

	// Get mouse position
	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);

	FVector2D EdgeInput = FVector2D::ZeroVector;

	// Check edges and convert to orbital rotation
	if (MouseX <= EdgePanBorderThickness)
	{
		EdgeInput.X -= 1.0f; // Orbit left
	}
	else if (MouseX >= ViewportSizeX - EdgePanBorderThickness)
	{
		EdgeInput.X += 1.0f; // Orbit right
	}

	if (MouseY <= EdgePanBorderThickness)
	{
		EdgeInput.Y += 1.0f; // Orbit forward
	}
	else if (MouseY >= ViewportSizeY - EdgePanBorderThickness)
	{
		EdgeInput.Y -= 1.0f; // Orbit backward
	}

	if (!EdgeInput.IsZero())
	{
		EdgeInput.Normalize();
		CurrentYaw += EdgeInput.X * EdgeOrbitSpeed * DeltaTime;
		CurrentPitch += EdgeInput.Y * EdgeOrbitSpeed * DeltaTime; // No clamping - allow going over poles
		
		// Update rotation, preserving horizon rotation (roll)
		FRotator NewRotation = FRotator(CurrentPitch, CurrentYaw, CurrentCameraRotation);
		SetActorRotation(NewRotation);
	}
}

void APlanetCameraPawn::EnterCityEditorMode(FVector InCityCenter)
{
	if (bCityEditorMode)
	{
		return; // Already in city mode
	}

	// Save planet camera state before switching
	SavedPlanetRotation = GetActorQuat();
	SavedPlanetCameraTilt = CurrentCameraTilt;
	SavedPlanetOrbitDistance = CurrentOrbitDistance;

	// Switch to city editor mode
	bCityEditorMode = true;
	CityCenter = InCityCenter;
	
	// Orient pawn so its local Z points from planet center through the city.
	// This makes CityCameraYaw rotate around the city's surface normal instead of world Z.
	FVector CityNormal = (CityCenter - PlanetCenter).GetSafeNormal();
	SetActorRotation(FRotationMatrix::MakeFromZ(CityNormal).ToQuat());
	
	// Restore saved city camera state if available, otherwise use defaults
	if (bHasSavedCityState)
	{
		// Restore saved transforms directly
		if (CityCameraArm)
		{
			// Disable lag so arm snaps instantly
			CityCameraArm->bEnableCameraLag = false;
			
			// Arm pivot: city is directly along local Z (CityNormal) from the pawn at PlanetCenter
			float CityDist = FVector::Dist(CityCenter, PlanetCenter);
			CityCameraArm->SetRelativeLocation(FVector(0.0f, 0.0f, CityDist));
			CityCameraArm->SetRelativeRotation(SavedCityCameraArmRotation);
			CityCameraArm->TargetArmLength = SavedCityCameraArmLength;
			
			// Force the arm to evaluate immediately at the correct position
			CityCameraArm->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
			
			// Re-enable lag for smooth controls
			CityCameraArm->bEnableCameraLag = true;
		}
		if (CityCamera)
		{
			CityCamera->SetRelativeRotation(SavedCityCameraRotation);
		}
	}
	else
	{
		// First time entering city mode - calculate initial orientation
		// DirectionFromPlanet is the city's surface normal (local Z of the pawn after orientation fix)
		FVector DirectionFromPlanet = (CityCenter - PlanetCenter).GetSafeNormal();
		
		// Set initial city camera parameters.
		// Since the pawn is now oriented with local Z = CityNormal, the pitch is simply
		// a fixed elevation angle in city-local space (no world-space correction needed).
		CityCameraPitch = -40.0f;  // 40° above local horizontal = comfortable overhead view
		
		// Derive initial yaw: project the planet camera's forward direction onto the city
		// horizontal plane so the orbit starts facing the same general direction as before.
		FVector PlanetCamForward = PlanetCamera ? PlanetCamera->GetForwardVector() : FVector::ForwardVector;
		FVector CityNormalForYaw = DirectionFromPlanet; // local Z of pawn
		FVector FlatForward = (PlanetCamForward - CityNormalForYaw * FVector::DotProduct(PlanetCamForward, CityNormalForYaw)).GetSafeNormal();
		// Project FlatForward into pawn's local XY plane to get a yaw angle
		// Use the pawn's new local X axis to derive yaw
		{
			FQuat PawnQuat = FRotationMatrix::MakeFromZ(CityNormalForYaw).ToQuat();
			FVector LocalFlat = PawnQuat.UnrotateVector(FlatForward);
			CityCameraYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalFlat.Y, LocalFlat.X));
		}
		CityCameraOrbitDistance = CityEditorStartDistance;
		CityCameraTilt = 185.0f;
		CityCameraRotation = 0.0f;
		
		// Position city camera arm at city center (relative to planet)
		if (CityCameraArm)
		{
			// Disable lag so arm snaps instantly
			CityCameraArm->bEnableCameraLag = false;
			
			// Arm pivot in pawn local space: city is directly along local Z at CityDist
			float CityDist = FVector::Dist(CityCenter, PlanetCenter);
			CityCameraArm->SetRelativeLocation(FVector(0.0f, 0.0f, CityDist));
			CityCameraArm->SetRelativeRotation(FRotator(CityCameraPitch, CityCameraYaw, 0.0f));
			CityCameraArm->TargetArmLength = CityCameraOrbitDistance;
			
			// Force the arm to evaluate immediately at the correct position
			CityCameraArm->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
			
			// Re-enable lag for smooth controls
			CityCameraArm->bEnableCameraLag = true;
		}
		
		// Apply camera rotation (tilt, look back, and roll to be right-side up)
		if (CityCamera)
		{
			FRotator CityRotation = FRotator(CityCameraTilt, 180.0f, 180.0f);
			CityCamera->SetRelativeRotation(CityRotation);
		}
	}
	
	// Instantly switch cameras (no blend)
	if (CityCamera)
	{
		CityCamera->SetActive(true);
	}
	if (PlanetCamera)
	{
		PlanetCamera->SetActive(false);
	}
}

void APlanetCameraPawn::ExitCityEditorMode()
{
	if (!bCityEditorMode)
	{
		return; // Not in city mode
	}

	// Save current city camera state before exiting
	if (CityCameraArm)
	{
		SavedCityCameraArmLocation = CityCameraArm->GetRelativeLocation();
		SavedCityCameraArmRotation = CityCameraArm->GetRelativeRotation().Quaternion();
		SavedCityCameraArmLength = CityCameraArm->TargetArmLength;
	}
	if (CityCamera)
	{
		SavedCityCameraRotation = CityCamera->GetRelativeRotation().Quaternion();
	}
	bHasSavedCityState = true;

	bCityEditorMode = false;
	
	// Restore saved planet camera state
	SetActorRotation(SavedPlanetRotation);
	CurrentCameraTilt = SavedPlanetCameraTilt;
	CurrentOrbitDistance = SavedPlanetOrbitDistance;
	
	// Update derived Euler angles for other systems
	FRotator RestoredRotation = SavedPlanetRotation.Rotator();
	CurrentYaw = RestoredRotation.Yaw;
	CurrentPitch = RestoredRotation.Pitch;
	CurrentCameraRotation = RestoredRotation.Roll;
	
	// Restore spring arm length and camera tilt
	if (PlanetCameraArm)
	{
		// Disable lag so arm snaps instantly
		PlanetCameraArm->bEnableCameraLag = false;
		
		PlanetCameraArm->TargetArmLength = CurrentOrbitDistance;
		
		// Force the arm to evaluate immediately at the correct position
		PlanetCameraArm->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
		
		// Re-enable lag for smooth controls
		PlanetCameraArm->bEnableCameraLag = true;
	}
	if (PlanetCamera)
	{
		PlanetCamera->SetRelativeRotation(FRotator(CurrentCameraTilt, 180.0f, 0.0f));
	}
	
	// Instantly switch cameras (no blend)
	if (PlanetCamera)
	{
		PlanetCamera->SetActive(true);
	}
	if (CityCamera)
	{
		CityCamera->SetActive(false);
	}
}
