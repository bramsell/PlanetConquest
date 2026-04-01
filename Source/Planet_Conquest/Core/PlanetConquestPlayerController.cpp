// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PlanetConquestPlayerController.h"
#include "CityActor.h"
#include "ResourceActor.h"
#include "VehicleActor.h"
#include "../Entities/Vehicles/ShipActor.h"
#include "PlanetActor.h"
#include "PlanetConquestGameMode.h"
#include "../Buildings/BuildingActor.h"
#include "../Buildings/FactoryBuildingActor.h"
#include "../Buildings/MineActor.h"
#include "../Kaiju/KaijuActor.h"
#include "CityWidget.h"
#include "GameHUDWidget.h"
#include "PlanetCameraPawn.h"
#include "PlanetConquestHUD.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

APlanetConquestPlayerController::APlanetConquestPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void APlanetConquestPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// Force reset BlackSubstrateMode to Efficient if invalid (handles old cached enum values)
	int32 RawModeValue = static_cast<int32>(BlackSubstrateMode);
	if (RawModeValue < 0 || RawModeValue > 2)
	{
		UE_LOG(LogTemp, Error, TEXT("=== BeginPlay: INVALID BlackSubstrateMode detected (raw: %d), forcing to Efficient ==="), RawModeValue);
		BlackSubstrateMode = EBlackSubstrateMode::Efficient;
	}
	
	// Debug: Check initial BlackSubstrateMode
	RawModeValue = static_cast<int32>(BlackSubstrateMode);
	FString ModeName = TEXT("UNKNOWN");
	switch (BlackSubstrateMode)
	{
		case EBlackSubstrateMode::Auxiliary:
			ModeName = TEXT("Auxiliary");
			break;
		case EBlackSubstrateMode::Efficient:
			ModeName = TEXT("Efficient");
			break;
		case EBlackSubstrateMode::Overdrive:
			ModeName = TEXT("Overdrive");
			break;
		default:
			ModeName = FString::Printf(TEXT("INVALID (raw value: %d)"), RawModeValue);
			break;
	}
	UE_LOG(LogTemp, Error, TEXT("=== BeginPlay: BlackSubstrateMode = %s (raw: %d) ==="), *ModeName, RawModeValue);
	
	// Ensure cursor is visible
	bShowMouseCursor = true;
	
	// Create city UI widget
	if (CityWidgetClass)
	{
		// UE_LOG(LogTemp, Warning, TEXT("CityWidgetClass is SET! Attempting to create widget..."));
		CityWidgetInstance = CreateWidget<UCityWidget>(this, CityWidgetClass);
		if (CityWidgetInstance)
		{
			CityWidgetInstance->AddToViewport();
			CityWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
			// UE_LOG(LogTemp, Warning, TEXT("SUCCESS: City UI widget created and hidden"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("FAILED: CreateWidget returned NULL!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CityWidgetClass NOT SET! Please assign WBP_CityUI in editor:"));
		UE_LOG(LogTemp, Error, TEXT("  1. Open World Settings"));
		UE_LOG(LogTemp, Error, TEXT("  2. Set Player Controller Class to PlanetConquestPlayerController"));
		UE_LOG(LogTemp, Error, TEXT("  3. Find the Player Controller in the level"));
		UE_LOG(LogTemp, Error, TEXT("  4. Set City Widget Class to WBP_CityUI"));
	}
	
	// Start income timer (collect $100 per resource/city, $200 per factory every 5 seconds)
	GetWorld()->GetTimerManager().SetTimer(IncomeTimerHandle, this, &APlanetConquestPlayerController::CollectIncome, 5.0f, true);
	// UE_LOG(LogTemp, Warning, TEXT("Income system started - collecting every 5 seconds"));
	
	// DISABLED - Orange Substrate maintenance removed
	// GetWorld()->GetTimerManager().SetTimer(MaintenanceTimerHandle, this, &APlanetConquestPlayerController::PayVehicleMaintenance, 5.0f, true);
	// UE_LOG(LogTemp, Warning, TEXT("Maintenance system started - paying every 5 seconds"));

	// Start Green Substrate consumption timer (consume GS every 1 second based on mode)
	GetWorld()->GetTimerManager().SetTimer(BlackSubstrateConsumptionHandle, this, &APlanetConquestPlayerController::ConsumeBlackSubstrate, 1.0f, true);
	
	// Create game HUD widget
	if (GameHUDWidgetClass)
	{
		// UE_LOG(LogTemp, Warning, TEXT("GameHUDWidgetClass is SET! Attempting to create widget..."));
		GameHUDWidgetInstance = CreateWidget<UGameHUDWidget>(this, GameHUDWidgetClass);
		if (GameHUDWidgetInstance)
		{
			GameHUDWidgetInstance->AddToViewport();
			GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
			// UE_LOG(LogTemp, Warning, TEXT("SUCCESS: Game HUD widget created and shown"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("FAILED: CreateWidget returned NULL for GameHUD!"));
		}
	}
	else
	{
		// UE_LOG(LogTemp, Warning, TEXT("GameHUDWidgetClass NOT SET - HUD will not be shown"));
	}
	
	// UE_LOG(LogTemp, Warning, TEXT("Player Controller initialized with mouse cursor visible"));
}

void APlanetConquestPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (InputComponent)
	{
		// Bind left mouse button directly (doesn't require Input Action setup)
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APlanetConquestPlayerController::HandleLeftClick);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &APlanetConquestPlayerController::HandleLeftClickRelease);
		
		// Bind right mouse button for panning
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &APlanetConquestPlayerController::HandleRightClick);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &APlanetConquestPlayerController::HandleRightClickRelease);
	}
}

void APlanetConquestPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Check if grace period has expired (0.05s after release without re-clicking)
	if (bBoxSelectionGracePeriod && BoxSelectionReleaseTime >= 0.0f)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - BoxSelectionReleaseTime >= 0.05f)
		{
			// Grace period expired, finalize the selection
			FVector2D BoxSize = (BoxSelectionEnd - BoxSelectionStart).GetAbs();
			float DragThreshold = 5.0f; // Pixels
			
			if (BoxSize.X < DragThreshold && BoxSize.Y < DragThreshold)
			{
				// Small movement = single click selection
				SelectActorUnderMouse();
			}
			else
			{
				// Box selection
				PerformBoxSelection();
			}
			
			bIsBoxSelecting = false;
			bBoxSelectionGracePeriod = false;
			BoxSelectionReleaseTime = -1.0f;
			//UE_LOG(LogTemp, Log, TEXT("Grace period expired, selection finalized"));
		}
	}
	
	// Update box selection end position while dragging
	if (bIsBoxSelecting)
	{
		// Hide resource info while box selecting
		if (LastHoveredResource)
		{
			LastHoveredResource->ShowResourceInfo(false);
			LastHoveredResource = nullptr;
		}
		
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			BoxSelectionEnd = FVector2D(MouseX, MouseY);
		}
		
		// Update HUD to draw selection box
		if (APlanetConquestHUD* HUD = Cast<APlanetConquestHUD>(GetHUD()))
		{
			HUD->bDrawSelectionBox = true;
			HUD->SelectionBoxStart = BoxSelectionStart;
			HUD->SelectionBoxEnd = BoxSelectionEnd;
		}
	}
	else
	{
		// Disable box drawing
		if (APlanetConquestHUD* HUD = Cast<APlanetConquestHUD>(GetHUD()))
		{
			HUD->bDrawSelectionBox = false;
		}
	}

	// Check if right mouse button is held for panning cursor
	if (bIsRightMouseHeld)
	{
		CurrentMouseCursor = EMouseCursor::CardinalCross;
		
		// Hide resource info while panning
		if (LastHoveredResource)
		{
			LastHoveredResource->ShowResourceInfo(false);
			LastHoveredResource = nullptr;
		}
	}
	// Hover detection for cursor changes (only when not box selecting)
	else if (!bIsBoxSelecting)
	{
		static int32 DebugFrameCounter = 0;
		DebugFrameCounter++;
		
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			FVector2D MousePos(MouseX, MouseY);
			AActor* ClosestActor = nullptr;
			float ClosestDistance = FLT_MAX;

			// Get camera location for visibility checks
			FVector CameraLocation = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;

			// Check all resources
			TArray<AActor*> FoundResources;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
			
			for (AActor* Actor : FoundResources)
			{
				FVector ActorLocation = Actor->GetActorLocation();
				
				// Check visibility (raycast from camera to actor)
				FHitResult VisibilityHit;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(Actor);
				QueryParams.AddIgnoredActor(this);
				
				bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
					VisibilityHit,
					CameraLocation,
					ActorLocation,
					ECC_Visibility,
					QueryParams
				);

				// Actor is only blocked if we hit something BEFORE reaching it
				float DistanceToActor = FVector::Dist(CameraLocation, ActorLocation);
				float DistanceToHit = VisibilityHit.Distance;
				bool bIsBlocked = bHitSomething && (DistanceToHit < DistanceToActor - 100.0f); // 100 unit tolerance
				
				if (bIsBlocked)
				{
					continue;
				}

				// Convert world position to screen position
				FVector2D ScreenPos;
				if (ProjectWorldLocationToScreen(ActorLocation, ScreenPos))
				{
					float Distance = FVector2D::Distance(MousePos, ScreenPos);
					const float ResourceHoverThreshold = 40.0f;
					
					if (Distance <= ResourceHoverThreshold && Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						ClosestActor = Actor;
					}
				}
			}

			// Check all vehicles
			TArray<AActor*> FoundVehicles;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), FoundVehicles);
			for (AActor* Actor : FoundVehicles)
			{
				AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
				if (!Vehicle)
				{
					continue;
				}

				FVector ActorLocation = Actor->GetActorLocation();
				
				// Check visibility (raycast from camera to actor)
				FHitResult VisibilityHit;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(Actor);
				QueryParams.AddIgnoredActor(this);
				
				bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
					VisibilityHit,
					CameraLocation,
					ActorLocation,
					ECC_Visibility,
					QueryParams
				);

				// Actor is only blocked if we hit something BEFORE reaching it
				float DistanceToActor = FVector::Dist(CameraLocation, ActorLocation);
				float DistanceToHit = VisibilityHit.Distance;
				bool bIsBlocked = bHitSomething && (DistanceToHit < DistanceToActor - 100.0f); // 100 unit tolerance
				
				if (bIsBlocked)
				{
					continue;
				}

				// Convert world position to screen position
				FVector2D ScreenPos;
				if (ProjectWorldLocationToScreen(ActorLocation, ScreenPos))
				{
					float Distance = FVector2D::Distance(MousePos, ScreenPos);
					const float VehicleHoverThreshold = 30.0f; // Reduced from 50.0f
					
					if (Distance <= VehicleHoverThreshold && Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						ClosestActor = Actor;
					}
				}
			}

			// Check all kaiju
			TArray<AActor*> FoundKaiju;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AKaijuActor::StaticClass(), FoundKaiju);
			for (AActor* Actor : FoundKaiju)
			{
				AKaijuActor* Kaiju = Cast<AKaijuActor>(Actor);
				if (!Kaiju)
				{
					continue;
				}

				FVector ActorLocation = Actor->GetActorLocation();
				
				// Check visibility (raycast from camera to actor)
				FHitResult VisibilityHit;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(Actor);
				QueryParams.AddIgnoredActor(this);
				
				bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
					VisibilityHit,
					CameraLocation,
					ActorLocation,
					ECC_Visibility,
					QueryParams
				);

				// Actor is only blocked if we hit something BEFORE reaching it
				float DistanceToActor = FVector::Dist(CameraLocation, ActorLocation);
				float DistanceToHit = VisibilityHit.Distance;
				bool bIsBlocked = bHitSomething && (DistanceToHit < DistanceToActor - 100.0f); // 100 unit tolerance
				
				if (bIsBlocked)
				{
					continue;
				}

				// Convert world position to screen position
				FVector2D ScreenPos;
				if (ProjectWorldLocationToScreen(ActorLocation, ScreenPos))
				{
					float Distance = FVector2D::Distance(MousePos, ScreenPos);
					const float KaijuHoverThreshold = 75.0f; // Larger threshold for bigger kaiju
					
					if (Distance <= KaijuHoverThreshold && Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						ClosestActor = Actor;
					}
				}
			}

			// Check all buildings (only when city editor is open)
			if (CityWidgetInstance && CityWidgetInstance->IsVisible())
			{
				TArray<AActor*> FoundBuildings;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABuildingActor::StaticClass(), FoundBuildings);
				for (AActor* Actor : FoundBuildings)
				{
					ABuildingActor* Building = Cast<ABuildingActor>(Actor);
					if (!Building || Building->OwnerTeam != EOwnerTeam::Player)
					{
						continue;
					}

					FVector ActorLocation = Actor->GetActorLocation();
					
					// Check visibility (raycast from camera to actor)
					FHitResult VisibilityHit;
					FCollisionQueryParams QueryParams;
					QueryParams.AddIgnoredActor(Actor);
					QueryParams.AddIgnoredActor(this);
					
					bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
						VisibilityHit,
						CameraLocation,
						ActorLocation,
						ECC_Visibility,
						QueryParams
					);

					// Actor is only blocked if we hit something BEFORE reaching it
					float DistanceToActor = FVector::Dist(CameraLocation, ActorLocation);
					float DistanceToHit = VisibilityHit.Distance;
					bool bIsBlocked = bHitSomething && (DistanceToHit < DistanceToActor - 100.0f);
					
					if (bIsBlocked)
					{
						continue;
					}

					// Convert world position to screen position
					FVector2D ScreenPos;
					if (ProjectWorldLocationToScreen(ActorLocation, ScreenPos))
					{
						float Distance = FVector2D::Distance(MousePos, ScreenPos);
						const float BuildingHoverThreshold = 60.0f;
						
						if (Distance <= BuildingHoverThreshold && Distance < ClosestDistance)
						{
							ClosestDistance = Distance;
							ClosestActor = Actor;
						}
					}
				}
			}

			// Update cursor based on what's hovered
			HoveredActor = ClosestActor;

			// Handle resource hover highlight (redirect to mine if resource is owned)
			AResourceActor* CurrentHoveredResource = Cast<AResourceActor>(HoveredActor);
			if (CurrentHoveredResource != LastHoveredResource)
			{
				// Hide previous resource's info widget and hover highlight
				if (LastHoveredResource)
				{
					LastHoveredResource->ShowResourceInfo(false);
					// If last resource had a mine, clear its selection only if not already selected by player
					if (LastHoveredResource->Mine && IsValid(LastHoveredResource->Mine))
					{
						// Don't clear selection if the mine is in the SelectedActors array
						if (!SelectedActors.Contains(LastHoveredResource->Mine))
						{
							LastHoveredResource->Mine->SetSelected(false);
						}
					}
					else
					{
						LastHoveredResource->SetHovered(false);
					}
				}
				
				// Show new resource's info widget and hover highlight
				if (CurrentHoveredResource)
				{
					CurrentHoveredResource->ShowResourceInfo(true);
					// If resource has a mine, highlight the mine instead
					if (CurrentHoveredResource->Mine && IsValid(CurrentHoveredResource->Mine))
					{
						CurrentHoveredResource->Mine->SetSelected(true);
					}
					else
					{
						CurrentHoveredResource->SetHovered(true);
					}
				}
				
				LastHoveredResource = CurrentHoveredResource;
			}
			
			// Handle vehicle hover highlight (only for enemy vehicles)
			AVehicleActor* CurrentHoveredVehicle = Cast<AVehicleActor>(HoveredActor);
			if (CurrentHoveredVehicle != LastHoveredVehicle)
			{
				// Hide previous vehicle's hover highlight
				if (LastHoveredVehicle && LastHoveredVehicle->OwnerTeam != EOwnerTeam::Player)
				{
					LastHoveredVehicle->SetHovered(false);
				}
				
				// Show new vehicle's hover highlight (only for enemy vehicles)
				if (CurrentHoveredVehicle && CurrentHoveredVehicle->OwnerTeam != EOwnerTeam::Player)
				{
					CurrentHoveredVehicle->SetHovered(true);
				}
				
				LastHoveredVehicle = CurrentHoveredVehicle;
			}
			
			// Handle kaiju hover highlight
			AKaijuActor* CurrentHoveredKaiju = Cast<AKaijuActor>(HoveredActor);
			if (CurrentHoveredKaiju != LastHoveredKaiju)
			{
				// Hide previous kaiju's hover highlight
				if (LastHoveredKaiju)
				{
					LastHoveredKaiju->SetHovered(false);
				}
				
				// Show new kaiju's hover highlight
				if (CurrentHoveredKaiju)
				{
					CurrentHoveredKaiju->SetHovered(true);
				}
				
				LastHoveredKaiju = CurrentHoveredKaiju;
			}
			
			// Handle building hover highlight (only when city editor is open)
			ABuildingActor* CurrentHoveredBuilding = nullptr;
			if (CityWidgetInstance && CityWidgetInstance->IsVisible())
			{
				CurrentHoveredBuilding = Cast<ABuildingActor>(HoveredActor);
				// Only hover player buildings
				if (CurrentHoveredBuilding && CurrentHoveredBuilding->OwnerTeam != EOwnerTeam::Player)
				{
					CurrentHoveredBuilding = nullptr;
				}
			}
			
			if (CurrentHoveredBuilding != LastHoveredBuilding)
			{
				// Hide previous building's hover highlight
				if (LastHoveredBuilding)
				{
					LastHoveredBuilding->SetHovered(false);
				}
				
				// Show new building's hover highlight
				if (CurrentHoveredBuilding)
				{
					CurrentHoveredBuilding->SetHovered(true);
				}
				
				LastHoveredBuilding = CurrentHoveredBuilding;
			}
			
			if (HoveredActor)
			{
				if (AResourceActor* Resource = Cast<AResourceActor>(HoveredActor))
				{
					// If resource has a mine, treat it as an attackable target
					if (Resource->Mine && IsValid(Resource->Mine))
					{
						CurrentMouseCursor = EMouseCursor::Crosshairs;
						//UE_LOG(LogTemp, Warning, TEXT("HOVER DEBUG: Cursor set to CROSSHAIRS (resource with mine)"));
					}
					else
					{
						// Uncaptured resources are collectible targets
						CurrentMouseCursor = EMouseCursor::GrabHand;
					}
				}
				else if (AVehicleActor* Vehicle = Cast<AVehicleActor>(HoveredActor))
				{
					if (Vehicle->OwnerTeam == EOwnerTeam::Player)
					{
						// Friendly vehicles
						CurrentMouseCursor = EMouseCursor::Hand;
						//UE_LOG(LogTemp, Warning, TEXT("HOVER DEBUG: Cursor set to HAND (friendly vehicle)"));
					}
					else
					{
						// Enemy vehicles
						CurrentMouseCursor = EMouseCursor::Crosshairs;
						//UE_LOG(LogTemp, Warning, TEXT("HOVER DEBUG: Cursor set to CROSSHAIRS (enemy vehicle)"));
					}
				}
				else if (AKaijuActor* Kaiju = Cast<AKaijuActor>(HoveredActor))
				{
					// Kaiju are always targetable
					CurrentMouseCursor = EMouseCursor::Crosshairs;
				}
			}
			else
			{
				// Nothing hovered
				CurrentMouseCursor = EMouseCursor::Default;
			}
		}
	}
	
	// Update resource health based on mining mode
	UpdateResourceHealth(DeltaTime);
}

void APlanetConquestPlayerController::HandleLeftClick()
 {
	// UE_LOG(LogTemp, Warning, TEXT("Left click pressed!"));
	
	// Check if we're in the grace period (0.05s after release)
	if (bBoxSelectionGracePeriod)
	{
		// Continue with the same box selection
		bBoxSelectionGracePeriod = false;
		BoxSelectionReleaseTime = -1.0f;
		// Keep bIsBoxSelecting true and continue dragging
		//UE_LOG(LogTemp, Log, TEXT("Continuing box selection within grace period"));
		return;
	}
	
	// Record starting position for box selection
	float MouseX, MouseY;
	if (GetMousePosition(MouseX, MouseY))
	{
		BoxSelectionStart = FVector2D(MouseX, MouseY);
		BoxSelectionEnd = BoxSelectionStart;
		bIsBoxSelecting = true;
	}
}

void APlanetConquestPlayerController::HandleLeftClickRelease()
{
	// UE_LOG(LogTemp, Warning, TEXT("Left click released!"));
	
	if (!bIsBoxSelecting)
	{
		return;
	}
	
	// Don't process game clicks when City UI is open
	if (CityWidgetInstance && CityWidgetInstance->IsVisible())
	{
		// UE_LOG(LogTemp, Warning, TEXT("Click ignored - City UI is open"));
		bIsBoxSelecting = false;
		return;
	}
	
	// Get final mouse position
	float MouseX, MouseY;
	if (GetMousePosition(MouseX, MouseY))
	{
		BoxSelectionEnd = FVector2D(MouseX, MouseY);
	}
	
	// Start grace period instead of immediately finalizing
	// This allows re-clicking within 0.05s to continue the selection
	bBoxSelectionGracePeriod = true;
	BoxSelectionReleaseTime = GetWorld()->GetTimeSeconds();
	//UE_LOG(LogTemp, Log, TEXT("Box selection released, starting 0.05s grace period"));
}

void APlanetConquestPlayerController::HandleRightClick()
{
	bIsRightMouseHeld = true;
}

void APlanetConquestPlayerController::HandleRightClickRelease()
{
	bIsRightMouseHeld = false;
}

void APlanetConquestPlayerController::SelectActorUnderMouse()
{
	// Get mouse position
	FVector2D MousePosition;
	if (!GetMousePosition(MousePosition.X, MousePosition.Y))
	{
		return;
	}

	// First, check if we're hovering over something (pixel-based detection)
	AActor* HitActor = nullptr;
	FVector HitLocation = FVector::ZeroVector;
	bool bPixelBasedHit = false;

	if (HoveredActor)
	{
		// Use the hovered actor from Tick()'s pixel detection
		HitActor = HoveredActor;
		bPixelBasedHit = true;
		
		// For movement commands, we need a valid location on the planet surface
		// Get camera location and direction toward the hovered actor
		if (PlayerCameraManager)
		{
			FVector CameraLocation = PlayerCameraManager->GetCameraLocation();
			FVector ActorLocation = HitActor->GetActorLocation();
			FVector Direction = (ActorLocation - CameraLocation).GetSafeNormal();
			
			// Trace from camera through actor to find planet surface
			FHitResult SurfaceHit;
			FVector TraceStart = CameraLocation;
			FVector TraceEnd = CameraLocation + (Direction * 1000000.0f);
			
			FCollisionQueryParams TraceParams;
			TraceParams.bTraceComplex = false;
			TraceParams.AddIgnoredActor(HitActor);
			
			if (GetWorld()->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
			{
				HitLocation = SurfaceHit.Location;
			}
			else
			{
				HitLocation = ActorLocation;
			}
		}
		else
		{
			HitLocation = HoveredActor->GetActorLocation();
		}
	}
	else
	{
		// Fall back to line trace if nothing is hovered via pixel detection
		FVector WorldLocation, WorldDirection;
		if (!DeprojectScreenPositionToWorld(MousePosition.X, MousePosition.Y, WorldLocation, WorldDirection))
		{
			return;
		}
		
		// Perform line trace
		FHitResult HitResult;
		FVector TraceStart = WorldLocation;
		FVector TraceEnd = WorldLocation + (WorldDirection * 1000000.0f);
		
		FCollisionQueryParams TraceParams;
		TraceParams.bTraceComplex = false;
		TraceParams.bReturnPhysicalMaterial = false;
		
		bool bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			TraceParams
		);

		if (bHit)
		{
			HitActor = HitResult.GetActor();
			HitLocation = HitResult.Location;
		}
	}
	
	// Check if we have vehicles selected (for move commands)
	bool bHasVehiclesSelected = false;
	for (AActor* Actor : SelectedActors)
	{
		if (Cast<AVehicleActor>(Actor))
		{
			bHasVehiclesSelected = true;
			break;
		}
	}
	
	if (HitActor)
	{
		// PRIORITY CHECK: If vehicles are selected and we clicked an enemy vehicle, send vehicles to attack it
		if (bHasVehiclesSelected && HitActor->IsA(AVehicleActor::StaticClass()))
		{
			AVehicleActor* TargetVehicle = Cast<AVehicleActor>(HitActor);
			if (TargetVehicle && TargetVehicle->OwnerTeam != EOwnerTeam::Player)
			{
				// Show selection box on the target vehicle
				TargetVehicle->SetSelected(true);
				
				// Send player vehicles to attack the enemy vehicle
				for (AActor* Actor : SelectedActors)
				{
					if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
					{
						// Set the enemy vehicle as the combat target
						// Don't use SetTargetLocation as it clears CurrentTarget
						Vehicle->CurrentTarget = TargetVehicle;
						Vehicle->ForcedHostileTeams.Add(TargetVehicle->OwnerTeam);
						
						// Set initial movement target (Tick will update this to follow the enemy)
						Vehicle->TargetLocation = TargetVehicle->GetActorLocation();
						Vehicle->bHasTarget = true;
						
						// Clear autonomous modes
						Vehicle->TargetResource = nullptr;
						Vehicle->bPlayerAutonomousMode = false;
						Vehicle->bHasAssignment = false;
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("Sending %d vehicle(s) to attack enemy vehicle %s"), SelectedActors.Num(), *TargetVehicle->GetName());
				
				// Deselect player vehicles after giving command
				DeselectAllActors();
				
				// Select the target vehicle so player can see what they're attacking
				SelectedActors.Add(TargetVehicle);
				return;
			}
		}
		
		// PRIORITY CHECK: If vehicles are selected and we clicked a kaiju, send vehicles to attack it
		if (bHasVehiclesSelected && HitActor->IsA(AKaijuActor::StaticClass()))
		{
			AKaijuActor* TargetKaiju = Cast<AKaijuActor>(HitActor);
			if (TargetKaiju)
			{
				// Show selection box on the target kaiju
				TargetKaiju->SetHovered(true);
				
				// Send player vehicles to attack the kaiju
				for (AActor* Actor : SelectedActors)
				{
					if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
					{
						// Set the kaiju as the combat target
						// Don't use SetTargetLocation as it clears CurrentTarget
						Vehicle->CurrentTarget = TargetKaiju;
						Vehicle->PrimaryTarget = TargetKaiju;
						
						// Set initial movement target (Tick will update this to follow the kaiju)
						Vehicle->TargetLocation = TargetKaiju->GetActorLocation();
						Vehicle->bHasTarget = true;
						
						// Clear autonomous modes
						Vehicle->TargetResource = nullptr;
						Vehicle->bPlayerAutonomousMode = false;
						Vehicle->bHasAssignment = false;
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("Sending %d vehicle(s) to attack kaiju %s"), SelectedActors.Num(), *TargetKaiju->GetName());
				
				// Deselect player vehicles after giving command
				DeselectAllActors();
				return;
			}
		}
		
		// PRIORITY CHECK: If vehicles are selected and we clicked an enemy building (mine, turret, etc), send vehicles to attack it
		if (bHasVehiclesSelected && HitActor->IsA(ABuildingActor::StaticClass()))
		{
			ABuildingActor* TargetBuilding = Cast<ABuildingActor>(HitActor);
			if (TargetBuilding && TargetBuilding->OwnerTeam != EOwnerTeam::Player)
			{
				// Special case: If this is a mine, use unified autopilot system
				if (AMineActor* TargetMine = Cast<AMineActor>(TargetBuilding))
				{
					// Get selected vehicles
					TArray<AVehicleActor*> SelectedVehicles;
					for (AActor* Actor : SelectedActors)
					{
						if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
						{
							SelectedVehicles.Add(Vehicle);
						}
					}
					
					if (SelectedVehicles.Num() > 0)
					{
						// ============================================================================
						// UNIFIED AUTOPILOT SYSTEM: Enemy Mines (Direct Click)
						// ============================================================================
						// RULE: If mine owned by team X is targeted inside a territory, destroy all 
						//       mines owned by team X in that territory.
						//       If mine owned by team X is targeted outside a territory, destroy all 
						//       mines owned by team X in the same cluster.
						// ============================================================================
						
						EOwnerTeam MineOwnerTeam = TargetMine->OwnerTeam;
						TArray<AMineActor*> TargetMines;
						bool bIsTerritory = false;
						FString GroupType = TEXT("Unknown");
						
						// Find the resource that owns this mine to get cluster/territory info
						AResourceActor* MineResource = nullptr;
						TArray<AActor*> FoundResources;
						UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
						
						for (AActor* ResourceActor : FoundResources)
						{
							if (AResourceActor* Resource = Cast<AResourceActor>(ResourceActor))
							{
								if (Resource->Mine == TargetMine)
								{
									MineResource = Resource;
									break;
								}
							}
						}
						
						if (MineResource)
						{
							// Check if mine is in a territory
							ACityActor* TerritoryCity = FindCityForTerritory(MineResource->GetActorLocation());
							
							if (TerritoryCity)
							{
								// IN TERRITORY: Destroy all mines owned by this team in this territory
								TargetMines = GetMinesInTerritory(TerritoryCity, MineOwnerTeam);
								bIsTerritory = true;
								GroupType = FString::Printf(TEXT("Territory (%s)"), *TerritoryCity->CityName);
							}
							else if (MineResource->ClusterID >= 0)
							{
								// OUTSIDE TERRITORY + IN CLUSTER: Destroy all mines owned by this team in cluster
								TargetMines = GetMinesInCluster(MineResource->ClusterID, MineOwnerTeam);
								GroupType = FString::Printf(TEXT("Cluster %d"), MineResource->ClusterID);
							}
							else
							{
								// OUTSIDE TERRITORY + NO CLUSTER: Just attack this single mine
								TargetMines.Add(TargetMine);
								GroupType = TEXT("Single Mine");
							}
						}
						else
						{
							// Couldn't find resource, just attack this mine
							TargetMines.Add(TargetMine);
							GroupType = TEXT("Single Mine (orphaned)");
						}
						
						// Assign targets to vehicles (ensure every vehicle gets a target)
						// First, ensure the closest vehicle targets the clicked mine
						// Then distribute remaining vehicles across territory/cluster mines
						if (TargetMines.Num() > 0 && SelectedVehicles.Num() > 0)
						{
							// Find the closest vehicle to the originally clicked mine
							int32 ClosestVehicleIndex = 0;
							float ClosestDistance = FLT_MAX;
							FVector ClickedMineLocation = TargetMine->GetActorLocation();
							
							for (int32 i = 0; i < SelectedVehicles.Num(); i++)
							{
								float Distance = FVector::Dist(SelectedVehicles[i]->GetActorLocation(), ClickedMineLocation);
								if (Distance < ClosestDistance)
								{
									ClosestDistance = Distance;
									ClosestVehicleIndex = i;
								}
							}
							
						// Get GameMode to check relationships with all teams
						APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
						
						// All vehicles target the clicked mine and autonomously continue to other mines
						for (AVehicleActor* Vehicle : SelectedVehicles)
						{
							Vehicle->SetTargetLocation(TargetMine->GetActorLocation());
							Vehicle->CurrentTarget = TargetMine;
							Vehicle->PrimaryTarget = TargetMine; // Remember mine as primary target
							
							// Add ALL hostile teams (relationship < -50) to ForcedHostileTeams
							Vehicle->ForcedHostileTeams.Empty();
							if (GameMode)
							{
								for (int32 i = 1; i <= 8; i++) // Check all AI teams
								{
									EOwnerTeam CheckTeam = static_cast<EOwnerTeam>(i);
									if (CheckTeam != EOwnerTeam::Neutral && GameMode->GetDisposition(EOwnerTeam::Player, CheckTeam) < -50.0f)
									{
										Vehicle->ForcedHostileTeams.Add(CheckTeam);
									}
								}
							}
							else
							{
								// Fallback: just add the clicked mine's team
								Vehicle->ForcedHostileTeams.Add(MineOwnerTeam);
							}
							
							Vehicle->bHasTarget = true;
							Vehicle->TargetResource = nullptr;
							Vehicle->PostMineResource = MineResource; // Auto-capture resource after destroying mine
							
							// Enable autonomous mine destruction mode
							Vehicle->bPlayerAutonomousMode = true;
							Vehicle->bHasAssignment = false;
							
							// Store the target mines for autonomous targeting
							if (MineResource->ClusterID >= 0)
							{
								Vehicle->ActiveClusterID = MineResource->ClusterID;
								Vehicle->bClusterOnlyMode = true;
							}
							else
							{
								Vehicle->ActiveClusterID = -1;
								Vehicle->bClusterOnlyMode = false;
							}
						}
					}
					
					UE_LOG(LogTemp, Warning, TEXT("[UNIFIED AUTOPILOT] Sending %d vehicle(s) to destroy %d mine(s) owned by Team %d in %s (direct click)"),
						SelectedVehicles.Num(), TargetMines.Num(), (int32)MineOwnerTeam, *GroupType);
					}
					
					DeselectAllActors();
					TargetMine->SetSelected(true);
					SelectedActors.Add(TargetMine);
					return;
				}
				
				// For other buildings (turrets, etc.), use standard single-target attack
				// Show selection box on the target building
				TargetBuilding->SetSelected(true);
				
				// Send player vehicles to attack the building
				for (AActor* Actor : SelectedActors)
				{
					if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
					{
						// Set the building as the combat target
						Vehicle->CurrentTarget = TargetBuilding;
						Vehicle->PrimaryTarget = TargetBuilding; // Remember as primary target
						Vehicle->ForcedHostileTeams.Add(TargetBuilding->OwnerTeam);
						
						// Set initial movement target (building is stationary)
						Vehicle->TargetLocation = TargetBuilding->GetActorLocation();
						Vehicle->bHasTarget = true;
						
						// Clear autonomous modes
						Vehicle->TargetResource = nullptr;
						Vehicle->bPlayerAutonomousMode = false;
						Vehicle->bHasAssignment = false;
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("Sending %d vehicle(s) to attack enemy building %s"), SelectedActors.Num(), *TargetBuilding->GetName());
				
				// Deselect player vehicles after giving command
				DeselectAllActors();
				
				// Select the target building so player can see what they're attacking
				SelectedActors.Add(TargetBuilding);
				return;
			}
		}
		
		// Check if it's a selectable actor (only player-owned vehicles can be selected)
		bool bIsSelectableActor = false;
		if (HitActor)
		{
			if (HitActor->IsA(ACityActor::StaticClass()) || HitActor->IsA(AResourceActor::StaticClass()))
			{
				bIsSelectableActor = true;
			}
			else if (AVehicleActor* Vehicle = Cast<AVehicleActor>(HitActor))
			{
				// Only allow selecting player-owned vehicles
				bIsSelectableActor = (Vehicle->OwnerTeam == EOwnerTeam::Player);
			}
		}
		
		// If vehicles are selected and we clicked a non-selectable object, move all vehicles
		if (bHasVehiclesSelected && !bIsSelectableActor)
		{
			// Collect vehicles to move, split into ships and ground vehicles
			TArray<AShipActor*> ShipsToMove;
			TArray<AVehicleActor*> GroundVehiclesToMove;
			for (AActor* Actor : SelectedActors)
			{
				if (AShipActor* Ship = Cast<AShipActor>(Actor))
				{
					ShipsToMove.Add(Ship);
				}
				else if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
				{
					GroundVehiclesToMove.Add(Vehicle);
				}
			}

			// Find planet for land/water checks
			APlanetActor* Planet = nullptr;
			FVector PlanetCenter = FVector::ZeroVector;
			TArray<AActor*> FoundPlanets;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlanetActor::StaticClass(), FoundPlanets);
			if (FoundPlanets.Num() > 0)
			{
				Planet = Cast<APlanetActor>(FoundPlanets[0]);
				PlanetCenter = FoundPlanets[0]->GetActorLocation();
			}

			// --- Move ships: only if clicked location is water ---
			if (ShipsToMove.Num() > 0)
			{
				FVector HitDir = (HitLocation - PlanetCenter).GetSafeNormal();
				bool bIsWater = Planet && !Planet->IsPointOnLand(HitDir);
				if (bIsWater)
				{
					for (int32 i = 0; i < ShipsToMove.Num(); i++)
					{
						// Clear combat state so the Tick doesn't immediately re-route to the old target
						ShipsToMove[i]->CurrentTarget = nullptr;
						ShipsToMove[i]->PrimaryTarget = nullptr;
						ShipsToMove[i]->bHasAssignment = false;
						ShipsToMove[i]->SetTargetLocation(HitLocation);
					}
					UE_LOG(LogTemp, Warning, TEXT("Moving %d ship(s) to water target: %s"), ShipsToMove.Num(), *HitLocation.ToString());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Ship move ignored: target is not water"));
				}
			}

			// --- Move ground vehicles: existing formation logic ---
			TArray<AVehicleActor*>& VehiclesToMove = GroundVehiclesToMove;
			if (VehiclesToMove.Num() > 0)
			{
				if (VehiclesToMove.Num() == 1)
				{
					// Single vehicle goes to exact target
					VehiclesToMove[0]->SetTargetLocation(HitLocation);
					VehiclesToMove[0]->PrimaryTarget = nullptr; // Clear attack assignment on movement order
				}
				else
				{
					// Multiple vehicles arranged in expanding spiral (scales with count)
					// First ring: 4 vehicles at 150 units
					// Second ring: 8 vehicles at 300 units
					// Third ring: 12 vehicles at 450 units, etc.
					
					FVector SurfaceNormal = (HitLocation - PlanetCenter).GetSafeNormal();
					FVector Tangent = FVector::CrossProduct(SurfaceNormal, FVector::UpVector).GetSafeNormal();
					if (Tangent.IsNearlyZero())
					{
						Tangent = FVector::CrossProduct(SurfaceNormal, FVector::ForwardVector).GetSafeNormal();
					}
					FVector Bitangent = FVector::CrossProduct(SurfaceNormal, Tangent).GetSafeNormal();
					
					int32 VehicleIndex = 0;
					int32 Ring = 0;
					
					while (VehicleIndex < VehiclesToMove.Num())
					{
						int32 VehiclesInThisRing = FMath::Max(4 + (Ring * 4), 1); // 4, 8, 12, 16...
						float RingRadius = 150.0f + (Ring * 150.0f); // 150, 300, 450...
						
						for (int32 i = 0; i < VehiclesInThisRing && VehicleIndex < VehiclesToMove.Num(); i++)
						{
							float Angle = (2.0f * PI * i) / VehiclesInThisRing;
							
							// Calculate formation position
							FVector Offset = (Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle)) * RingRadius;
							FVector FormationTarget = HitLocation + Offset;
							
							// Project back onto sphere surface
							FVector DirectionFromCenter = (FormationTarget - PlanetCenter).GetSafeNormal();
							float PlanetRadius = (HitLocation - PlanetCenter).Size();
							FormationTarget = PlanetCenter + DirectionFromCenter * PlanetRadius;
							
							VehiclesToMove[VehicleIndex]->SetTargetLocation(FormationTarget);
							VehiclesToMove[VehicleIndex]->PrimaryTarget = nullptr; // Clear attack assignment on movement order
							VehicleIndex++;
						}
						
						Ring++;
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("Moving %d vehicle(s) to formation around: %s"), VehiclesToMove.Num(), *HitLocation.ToString());
			}
			
			// Deselect vehicles after giving move command
			DeselectAllActors();
			return;
		}
		
		// If we clicked on a selectable actor
		if (bIsSelectableActor)
		{
			// Special case: if vehicles are selected and we clicked a resource
			if (bHasVehiclesSelected && HitActor->IsA(AResourceActor::StaticClass()))
			{
				AResourceActor* Resource = Cast<AResourceActor>(HitActor);
				
				// If resource has a mine, use unified autopilot to attack mines
				if (Resource && Resource->Mine && IsValid(Resource->Mine) && bHasVehiclesSelected)
				{
					// Get selected vehicles
					TArray<AVehicleActor*> SelectedVehicles;
					for (AActor* Actor : SelectedActors)
					{
						if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
						{
							SelectedVehicles.Add(Vehicle);
						}
					}
					
					if (SelectedVehicles.Num() == 0)
					{
						return;
					}
					
					// ============================================================================
					// UNIFIED AUTOPILOT SYSTEM: Enemy Mines
					// ============================================================================
					// RULE: If mine owned by team X is targeted inside a territory, destroy all 
					//       mines owned by team X in that territory.
					//       If mine owned by team X is targeted outside a territory, destroy all 
					//       mines owned by team X in the same cluster.
					// ============================================================================
					
					EOwnerTeam MineOwnerTeam = Resource->Mine->OwnerTeam;
					TArray<AMineActor*> TargetMines;
					bool bIsTerritory = false;
					FString GroupType = TEXT("Unknown");
					
					// Check if resource is in a territory
					ACityActor* TerritoryCity = FindCityForTerritory(Resource->GetActorLocation());
					
					if (TerritoryCity)
					{
						// IN TERRITORY: Destroy all mines owned by this team in this territory
						TargetMines = GetMinesInTerritory(TerritoryCity, MineOwnerTeam);
						bIsTerritory = true;
						GroupType = FString::Printf(TEXT("Territory (%s)"), *TerritoryCity->CityName);
					}
					else if (Resource->ClusterID >= 0)
					{
						// OUTSIDE TERRITORY + IN CLUSTER: Destroy all mines owned by this team in cluster
						TargetMines = GetMinesInCluster(Resource->ClusterID, MineOwnerTeam);
						GroupType = FString::Printf(TEXT("Cluster %d"), Resource->ClusterID);
					}
					else
					{
						// OUTSIDE TERRITORY + NO CLUSTER: Just attack this single mine
						TargetMines.Add(Resource->Mine);
						GroupType = TEXT("Single Mine");
					}
					
					// Assign targets to vehicles (ensure every vehicle gets a target)
					// First, ensure the closest vehicle targets the clicked mine
					// Then distribute remaining vehicles across territory/cluster mines
					if (TargetMines.Num() > 0 && SelectedVehicles.Num() > 0)
					{
						// Find the closest vehicle to the originally clicked mine (via resource)
						int32 ClosestVehicleIndex = 0;
						float ClosestDistance = FLT_MAX;
						FVector ClickedMineLocation = Resource->Mine->GetActorLocation();
						
						for (int32 i = 0; i < SelectedVehicles.Num(); i++)
						{
							float Distance = FVector::Dist(SelectedVehicles[i]->GetActorLocation(), ClickedMineLocation);
							if (Distance < ClosestDistance)
							{
								ClosestDistance = Distance;
								ClosestVehicleIndex = i;
							}
						}
						
						// Get GameMode to check relationships with all teams
						APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
						
						// All vehicles target the clicked mine and autonomously continue to other mines
						for (AVehicleActor* Vehicle : SelectedVehicles)
						{
							Vehicle->SetTargetLocation(Resource->Mine->GetActorLocation());
							Vehicle->CurrentTarget = Resource->Mine;
							Vehicle->PrimaryTarget = Resource->Mine; // Remember mine as primary target
							
							// Add ALL hostile teams (relationship < -50) to ForcedHostileTeams
							Vehicle->ForcedHostileTeams.Empty();
							if (GameMode)
							{
								for (int32 i = 1; i <= 8; i++) // Check all AI teams
								{
									EOwnerTeam CheckTeam = static_cast<EOwnerTeam>(i);
									if (CheckTeam != EOwnerTeam::Neutral && GameMode->GetDisposition(EOwnerTeam::Player, CheckTeam) < -50.0f)
									{
										Vehicle->ForcedHostileTeams.Add(CheckTeam);
									}
								}
							}
							else
							{
								// Fallback: just add the clicked mine's team
								Vehicle->ForcedHostileTeams.Add(MineOwnerTeam);
							}
							
							Vehicle->bHasTarget = true;
							Vehicle->TargetResource = nullptr;
							Vehicle->PostMineResource = Resource; // Auto-capture resource after destroying mine
							
							// Enable autonomous mine destruction mode
							Vehicle->bPlayerAutonomousMode = true;
							Vehicle->bHasAssignment = false;
							
							// Store the target mines for autonomous targeting
							if (Resource->ClusterID >= 0)
							{
								Vehicle->ActiveClusterID = Resource->ClusterID;
								Vehicle->bClusterOnlyMode = true;
							}
							else
							{
								Vehicle->ActiveClusterID = -1;
								Vehicle->bClusterOnlyMode = false;
							}
						}
					}
					
					UE_LOG(LogTemp, Warning, TEXT("[UNIFIED AUTOPILOT] Sending %d vehicle(s) to destroy %d mine(s) owned by Team %d in %s"),
						SelectedVehicles.Num(), TargetMines.Num(), (int32)MineOwnerTeam, *GroupType);
					
					DeselectAllActors();
					Resource->Mine->SetSelected(true);
					SelectedActors.Add(Resource->Mine);
					return;
				}
				
				if (Resource)
				{
					// Get selected vehicles
					TArray<AVehicleActor*> SelectedVehicles;
					for (AActor* Actor : SelectedActors)
					{
						if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
						{
							SelectedVehicles.Add(Vehicle);
						}
					}
					
					if (SelectedVehicles.Num() == 0)
					{
						// No vehicles selected, just select the resource
						DeselectAllActors();
						Resource->SetSelected(true);
						SelectedActors.Add(Resource);
						return;
					}

					// ============================================================================
					// UNIFIED AUTOPILOT SYSTEM: Neutral Resources
					// ============================================================================
					// RULE: If neutral resource is targeted inside a territory, capture all neutral 
					//       resources in that territory.
					//       If neutral resource is targeted outside a territory, capture all neutral
					//       resources in the same cluster.
					// ============================================================================
					
					if (Resource->OwnerTeam == EOwnerTeam::Neutral)
					{
					// Check if resource is currently being captured by another team
					if (Resource->CapturingVehicle && IsValid(Resource->CapturingVehicle) && 
					    Resource->CapturingVehicle->OwnerTeam != EOwnerTeam::Player)
					{
						UE_LOG(LogTemp, Warning, TEXT("[PLAYER] Cannot capture resource - already being captured by Team %d"), 
							(int32)Resource->CapturingVehicle->OwnerTeam);
						return; // Cannot override another team's capture
					}
					
					TArray<AResourceActor*> TargetResources;
					bool bIsTerritory = false;
					FString GroupType = TEXT("Unknown");
					// Check if resource is in a territory
					ACityActor* TerritoryCity = FindCityForTerritory(Resource->GetActorLocation());
					
					if (TerritoryCity)
					{
						// IN TERRITORY: Capture all neutral resources in this territory
						TargetResources = GetNeutralResourcesInTerritory(TerritoryCity);
						bIsTerritory = true;
						GroupType = FString::Printf(TEXT("Territory (%s)"), *TerritoryCity->CityName);
					}
					else if (Resource->ClusterID >= 0)
						{
							// OUTSIDE TERRITORY + IN CLUSTER: Capture all neutral resources in cluster
							TargetResources = GetNeutralResourcesInCluster(Resource->ClusterID);
							GroupType = FString::Printf(TEXT("Cluster %d"), Resource->ClusterID);
						}
						else
						{
							// OUTSIDE TERRITORY + NO CLUSTER: Just capture this single resource
							TargetResources.Add(Resource);
							GroupType = TEXT("Single Resource");
						}
						
						// Assign targets to vehicles (ensure every vehicle gets a target)
					// First, ensure the closest vehicle targets the clicked resource
					// Then distribute remaining vehicles across territory/cluster resources
					if (TargetResources.Num() > 0 && SelectedVehicles.Num() > 0)
					{
						// Find the closest vehicle to the originally clicked resource
						int32 ClosestVehicleIndex = 0;
						float ClosestDistance = FLT_MAX;
						FVector ClickedResourceLocation = Resource->GetActorLocation();
						
						for (int32 i = 0; i < SelectedVehicles.Num(); i++)
						{
							float Distance = FVector::Dist(SelectedVehicles[i]->GetActorLocation(), ClickedResourceLocation);
							if (Distance < ClosestDistance)
							{
								ClosestDistance = Distance;
								ClosestVehicleIndex = i;
							}
						}
						
						// Assign the closest vehicle to the clicked resource first
						SelectedVehicles[ClosestVehicleIndex]->SetTargetResource(Resource);
						SelectedVehicles[ClosestVehicleIndex]->bPlayerAutonomousMode = true;
						SelectedVehicles[ClosestVehicleIndex]->bClusterOnlyMode = false;
						
						// Build a list of available resources (excluding the clicked one to avoid duplication)
						TArray<AResourceActor*> AvailableResources;
						for (AResourceActor* Res : TargetResources)
						{
							if (Res != Resource) // Don't include the clicked resource
							{
								AvailableResources.Add(Res);
							}
						}
						
						// Assign remaining vehicles to their closest resources
						for (int32 i = 0; i < SelectedVehicles.Num(); i++)
						{
							if (i == ClosestVehicleIndex)
							{
								continue; // Skip the vehicle we already assigned to the clicked resource
							}
							
							AVehicleActor* Vehicle = SelectedVehicles[i];
							
							// Find the closest available resource to this vehicle
							AResourceActor* ClosestResource = nullptr;
							float MinDistance = FLT_MAX;
							
							// If we have available resources, find the closest one
							if (AvailableResources.Num() > 0)
							{
								FVector VehicleLocation = Vehicle->GetActorLocation();
								for (AResourceActor* Res : AvailableResources)
								{
									float Dist = FVector::Dist(VehicleLocation, Res->GetActorLocation());
									if (Dist < MinDistance)
									{
										MinDistance = Dist;
										ClosestResource = Res;
									}
								}
								
								// Remove the assigned resource from available pool
								AvailableResources.Remove(ClosestResource);
							}
							else
							{
								// If we ran out of resources, cycle through all resources (including clicked one)
								FVector VehicleLocation = Vehicle->GetActorLocation();
								for (AResourceActor* Res : TargetResources)
								{
									float Dist = FVector::Dist(VehicleLocation, Res->GetActorLocation());
									if (Dist < MinDistance)
									{
										MinDistance = Dist;
										ClosestResource = Res;
									}
								}
							}
							
							// Assign the vehicle to the closest resource
							if (ClosestResource)
							{
								Vehicle->SetTargetResource(ClosestResource);
								Vehicle->bPlayerAutonomousMode = true;
								Vehicle->bClusterOnlyMode = false;
							}
						}
						
						UE_LOG(LogTemp, Warning, TEXT("[UNIFIED AUTOPILOT] Sending %d vehicle(s) to capture %d neutral resource(s) in %s"),
							SelectedVehicles.Num(), TargetResources.Num(), *GroupType);
					}
				}
				else
				{
						// Enemy-owned resource: Just send vehicles to capture this one resource
						// (No unified autopilot for enemy resources - too aggressive)
						for (AVehicleActor* Vehicle : SelectedVehicles)
						{
							Vehicle->SetTargetResource(Resource);
							Vehicle->bPlayerAutonomousMode = false;
							Vehicle->bClusterOnlyMode = false;
						}
						
						UE_LOG(LogTemp, Warning, TEXT("Sending %d vehicle(s) to capture enemy resource %s"), 
							SelectedVehicles.Num(), *Resource->GetName());
					}
					
					// Deselect vehicles after giving command
					DeselectAllActors();
					
					// Now select the resource
					Resource->SetSelected(true);
					SelectedActors.Add(Resource);
					return;
				}
			}

			// Special case: if clicking on a city (not a building/resource), keep vehicles selected for attack orders
			if (bHasVehiclesSelected && HitActor->IsA(ACityActor::StaticClass()))
			{
				ACityActor* City = Cast<ACityActor>(HitActor);
				if (City && City->OwnerTeam != EOwnerTeam::Player && City->OwnerTeam != EOwnerTeam::Neutral)
				{
					// Select the city but KEEP vehicles selected
					City->SetSelected(true);
					SelectedActors.Add(City);
					
					UE_LOG(LogTemp, Warning, TEXT("AI city selected: %s (keeping %d vehicle(s) selected)"), 
						*City->CityName, GetSelectedVehicleCount());
					return;
				}
			}

			// Otherwise, deselect previous actors and select the new one
			DeselectAllActors();
		
		// Select new actor
		SelectedActors.Add(HitActor);
		
		// Enable selection visual
		if (ACityActor* City = Cast<ACityActor>(HitActor))
		{
			// Don't allow direct selection of AI cities - use diplomacy widget instead
			if (City->OwnerTeam != EOwnerTeam::Player && City->OwnerTeam != EOwnerTeam::Neutral)
			{
				SelectedActors.Remove(HitActor);
				UE_LOG(LogTemp, Log, TEXT("Skipping selection of AI city: %s - use hover widget instead"), *City->CityName);
				return;
			}
			
			City->SetSelected(true);
			
			// Only show city UI for player-owned cities
			// NOTE: Player cities now use the City Editor button on the DiplomacyWidget instead
			if (City->OwnerTeam == EOwnerTeam::Player && CityWidgetInstance)
			{
				// Don't open city editor on direct click - use the City Editor button on DiplomacyWidget instead
				UE_LOG(LogTemp, Log, TEXT("Player city clicked: %s - use City Editor button on hover widget to open editor"), *City->CityName);
			}
			else if (City->OwnerTeam != EOwnerTeam::Neutral)
			{
				// Non-player, non-neutral city - just select it (diplomacy UI shows on hover)
				UE_LOG(LogTemp, Warning, TEXT("Selected AI city: %s (Team %d)"), *City->CityName, (int32)City->OwnerTeam);
			}
			else
			{
				// Neutral city - just select it, no UI
				UE_LOG(LogTemp, Warning, TEXT("Selected neutral city: %s"), *City->CityName);
			}
		}
		else if (AResourceActor* SelectedResource = Cast<AResourceActor>(HitActor))
		{
			// If resource has a mine, select the mine instead
			if (SelectedResource->Mine && IsValid(SelectedResource->Mine))
			{
				SelectedResource->Mine->SetSelected(true);
				SelectedActors.Remove(HitActor); // Remove resource from selection
				SelectedActors.Add(SelectedResource->Mine); // Add mine instead
			}
			else
			{
				SelectedResource->SetSelected(true);
			}
			
			// Hide city UI when selecting non-city
			if (CityWidgetInstance)
			{
				CityWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
				SetCameraLocked(false);
				
				// Show game HUD when city UI hides
				if (GameHUDWidgetInstance)
				{
					GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
				}
			}
		}
		else if (AVehicleActor* Vehicle = Cast<AVehicleActor>(HitActor))
		{
			Vehicle->SetSelected(true);
			
			// Hide city UI when selecting non-city
			if (CityWidgetInstance)
			{
				CityWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
				SetCameraLocked(false);
				
				// Show game HUD when city UI hides
				if (GameHUDWidgetInstance)
				{
					GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
				}
			}
		}
		else if (ABuildingActor* Building = Cast<ABuildingActor>(HitActor))
		{
			Building->SetSelected(true);
			
			// Hide city UI when selecting non-city
			if (CityWidgetInstance)
			{
				CityWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
				SetCameraLocked(false);
				
				// Show game HUD when city UI hides
				if (GameHUDWidgetInstance)
				{
					GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Selected: %s"), *HitActor->GetName());
	}
}
else if (!bHasVehiclesSelected)
{
	// Clicked on empty space with no vehicles selected, deselect all
	DeselectAllActors();
}
}

void APlanetConquestPlayerController::PerformBoxSelection()
{
	// Deselect all current selections
	DeselectAllActors();
	
	// Get all actors in the world
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
	
	// Check each actor to see if it's in the selection box
	for (AActor* Actor : AllActors)
	{
		// Only select player-owned vehicles with box selection
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (!Vehicle || Vehicle->OwnerTeam != EOwnerTeam::Player)
		{
			continue;
		}
		
		// Project actor location to screen space
		FVector2D ScreenPosition;
		if (ProjectWorldLocationToScreen(Vehicle->GetActorLocation(), ScreenPosition))
		{
			// Calculate box bounds
			FVector2D BoxMin(FMath::Min(BoxSelectionStart.X, BoxSelectionEnd.X), FMath::Min(BoxSelectionStart.Y, BoxSelectionEnd.Y));
			FVector2D BoxMax(FMath::Max(BoxSelectionStart.X, BoxSelectionEnd.X), FMath::Max(BoxSelectionStart.Y, BoxSelectionEnd.Y));
			
			// Check if actor is within box
			if (ScreenPosition.X >= BoxMin.X && ScreenPosition.X <= BoxMax.X &&
				ScreenPosition.Y >= BoxMin.Y && ScreenPosition.Y <= BoxMax.Y)
			{
				SelectedActors.Add(Vehicle);
				Vehicle->SetSelected(true);
			}
		}
	}
	
	// UE_LOG(LogTemp, Warning, TEXT("Box selection: Selected %d vehicle(s)"), SelectedActors.Num());
}

void APlanetConquestPlayerController::DeselectAllActors()
{
	// Deselect all actors
	for (AActor* Actor : SelectedActors)
	{
		if (ACityActor* City = Cast<ACityActor>(Actor))
		{
			City->SetSelected(false);
			// Diplomacy widget is now controlled by mouse hover, not selection
		}
		else if (AResourceActor* Resource = Cast<AResourceActor>(Actor))
		{
			Resource->SetSelected(false);
		}
		else if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
		{
			Vehicle->SetSelected(false);
		}
		else if (ABuildingActor* Building = Cast<ABuildingActor>(Actor))
		{
			Building->SetSelected(false);
		}
	}
	
	SelectedActors.Empty();
	
	// Hide city UI when deselecting
	if (CityWidgetInstance)
	{
		CityWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		SetCameraLocked(false);
		
		// Show game HUD when city UI hides
		if (GameHUDWidgetInstance)
		{
			GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void APlanetConquestPlayerController::SetCameraLocked(bool bLocked)
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		APlanetCameraPawn* CameraPawn = Cast<APlanetCameraPawn>(ControlledPawn);
		if (CameraPawn)
		{
			CameraPawn->bLockCameraInput = bLocked;
			UE_LOG(LogTemp, Warning, TEXT("Camera input %s"), bLocked ? TEXT("LOCKED") : TEXT("UNLOCKED"));
		}
	}
}

int32 APlanetConquestPlayerController::GetSelectedVehicleCount() const
{
	int32 Count = 0;
	for (AActor* Actor : SelectedActors)
	{
		if (Actor->IsA(AVehicleActor::StaticClass()))
		{
			Count++;
		}
	}
	return Count;
}

void APlanetConquestPlayerController::CollectIncome()
{
	// In Sustainable mode, only collect income every 3rd call (15 seconds instead of 5)
	IncomeCollectionCounter++;
	if (MiningMode == EMiningMode::Sustainable)
	{
		if (IncomeCollectionCounter % 3 != 0)
		{
			return; // Skip this collection cycle
		}
	}
	
	// Find all resources in the world
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	int32 PlayerOrangeIncome = 0;
	int32 PlayerBlackIncome = 0;
	
	// Find all cities in the world
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), FoundCities);
	
	int32 PlayerCityCount = 0;
	
	// Initialize each city's green substrate to base value (100 from capital)
	// and count player cities
	for (AActor* Actor : FoundCities)
	{
		if (ACityActor* City = Cast<ACityActor>(Actor))
		{
			City->GreenSubstrate = 100; // Base from capital building
			
			if (City->OwnerTeam == EOwnerTeam::Player)
			{
				PlayerCityCount++;
			}
		}
	}
	
	// Assign green resources to nearest city of same team
	for (AActor* Actor : FoundResources)
	{
		if (AResourceActor* Resource = Cast<AResourceActor>(Actor))
		{
			if (Resource->ResourceType == EResourceType::GreenSubstrate && Resource->OwnerTeam != EOwnerTeam::Neutral)
			{
				// Find closest city owned by same team
				ACityActor* ClosestCity = nullptr;
				float ClosestDistance = FLT_MAX;
				
				for (AActor* CityActor : FoundCities)
				{
					if (ACityActor* City = Cast<ACityActor>(CityActor))
					{
						if (City->OwnerTeam == Resource->OwnerTeam)
						{
							float Distance = FVector::Dist(Resource->GetActorLocation(), City->GetActorLocation());
							if (Distance < ClosestDistance)
							{
								ClosestDistance = Distance;
								ClosestCity = City;
							}
						}
					}
				}
				
				// Add resource income to closest city
				if (ClosestCity)
				{
					ClosestCity->GreenSubstrate += Resource->GetEffectiveIncome();
				}
			}
			else if (Resource->OwnerTeam == EOwnerTeam::Player)
			{
				// Non-green resources owned by player
				if (Resource->ResourceType == EResourceType::OrangeSubstrate)
				{
					PlayerOrangeIncome += Resource->GetEffectiveIncome();
				}
				else if (Resource->ResourceType == EResourceType::BlackSubstrate)
				{
					PlayerBlackIncome += Resource->GetEffectiveIncome();
				}
			}
		}
	}
	
	// Find all buildings in the world
	TArray<AActor*> FoundBuildings;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABuildingActor::StaticClass(), FoundBuildings);
	
	int32 PlayerFactoryCount = 0;
	
	// Count factories owned by player
	for (AActor* Actor : FoundBuildings)
	{
		if (AFactoryBuildingActor* Factory = Cast<AFactoryBuildingActor>(Actor))
		{
			if (Factory->OwnerTeam == EOwnerTeam::Player)
			{
				PlayerFactoryCount++;
			}
		}
	}
	
	// Collect variable income per resource (based on size) and 200 OS/BS per factory
	int32 TotalOrangeIncome = PlayerOrangeIncome + (PlayerFactoryCount * 200);
	int32 TotalBlackIncome = PlayerBlackIncome + (PlayerFactoryCount * 200);
	
	// Store income per cycle for AI trading logic
	PlayerOrangeIncomePerCycle = TotalOrangeIncome;
	PlayerBlackIncomePerCycle = TotalBlackIncome;
	
	// Accumulate orange and black substrate
	PlayerOrangeSubstrate += TotalOrangeIncome;
	PlayerBlackSubstrate += TotalBlackIncome;
}

void APlanetConquestPlayerController::UpdateResourceHealth(float DeltaTime)
{
	// Find all resources owned by player
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	for (AActor* Actor : FoundResources)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource || Resource->OwnerTeam != EOwnerTeam::Player) continue;
		
		// Green substrate doesn't deplete or recover - skip it
		if (Resource->ResourceType == EResourceType::GreenSubstrate) continue;
		
		if (MiningMode == EMiningMode::Aggressive)
		{
			// Aggressive: Deplete 5% health every 5 minutes (300 seconds)
			float DepletionInterval = 300.0f;
			if (CurrentTime - Resource->LastDepletionTime >= DepletionInterval)
			{
				Resource->ResourceHealth = FMath::Max(0.2f, Resource->ResourceHealth - 0.05f);
				Resource->LastDepletionTime = CurrentTime;
				UE_LOG(LogTemp, Warning, TEXT("Resource %s depleted to %.1f%% health"), 
					*Resource->GetName(), Resource->ResourceHealth * 100.0f);
			}
			
			// Force sustainable mode if health drops below 20%
			if (Resource->ResourceHealth <= 0.2f && MiningMode == EMiningMode::Aggressive)
			{
				UE_LOG(LogTemp, Warning, TEXT("Resource health critical! Forcing Sustainable mode"));
				SetMiningMode(EMiningMode::Sustainable);
			}
		}
		else if (MiningMode == EMiningMode::Sustainable)
		{
			// Sustainable: Recover 5% health every 10 minutes (600 seconds)
			float RecoveryInterval = 600.0f;
			if (CurrentTime - Resource->LastRecoveryTime >= RecoveryInterval)
			{
				Resource->ResourceHealth = FMath::Min(1.0f, Resource->ResourceHealth + 0.05f);
				Resource->LastRecoveryTime = CurrentTime;
				UE_LOG(LogTemp, Warning, TEXT("Resource %s recovered to %.1f%% health"), 
					*Resource->GetName(), Resource->ResourceHealth * 100.0f);
			}
			
			// Can switch back to aggressive once health is above 25%
			// (player must manually switch back via UI)
		}
	}
}

void APlanetConquestPlayerController::SetMiningMode(EMiningMode NewMode)
{
	MiningMode = NewMode;
	UE_LOG(LogTemp, Warning, TEXT("=== MINING MODE CHANGED TO: %s ==="), 
		NewMode == EMiningMode::Aggressive ? TEXT("AGGRESSIVE") : TEXT("SUSTAINABLE"));
}

void APlanetConquestPlayerController::ToggleVehicleAutopilotMode()
{
	bVehicleAutopilotMode = !bVehicleAutopilotMode;
	UE_LOG(LogTemp, Warning, TEXT("Vehicle Autopilot Mode: %s"), bVehicleAutopilotMode ? TEXT("ON") : TEXT("OFF"));
}

// DISABLED - Orange Substrate maintenance removed
/*
void APlanetConquestPlayerController::PayVehicleMaintenance()
{
	// Count player-owned vehicles
	TArray<AActor*> FoundVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), FoundVehicles);
	
	int32 PlayerVehicleCount = 0;
	for (AActor* Actor : FoundVehicles)
	{
		if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
		{
			if (Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				PlayerVehicleCount++;
			}
		}
	}
	
	// Deduct 20 OS per vehicle for maintenance
	int32 MaintenanceCost = PlayerVehicleCount * 20;
	PlayerOrangeSubstrate -= MaintenanceCost;
}
*/

void APlanetConquestPlayerController::ConsumeBlackSubstrate()
{
	// Count player-owned vehicles and calculate consumption
	TArray<AActor*> FoundVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), FoundVehicles);
	
	// Base consumption rates per mode
	int32 BaseConsumptionPerVehicle = 0;
	switch (BlackSubstrateMode)
	{
		case EBlackSubstrateMode::Auxiliary:
			BaseConsumptionPerVehicle = 0;  // No consumption
			break;
		case EBlackSubstrateMode::Efficient:
			BaseConsumptionPerVehicle = 10;  // 10 BS per second (default)
			break;
		case EBlackSubstrateMode::Overdrive:
			BaseConsumptionPerVehicle = 20;  // 20 BS per second
			break;
	}
	
	int32 TotalConsumption = 0;
	for (AActor* Actor : FoundVehicles)
	{
		if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
		{
			if (Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				bool bMoving = Vehicle->bIsMoving;
				bool bFiring = Vehicle->bIsFiring;
				
				// If doing both moving AND firing, double the consumption
				if (bMoving && bFiring)
				{
					TotalConsumption += BaseConsumptionPerVehicle * 2;  // 20/sec on Efficient, 40/sec on Overdrive
				}
				else if (bMoving || bFiring)
				{
					TotalConsumption += BaseConsumptionPerVehicle;  // 10/sec on Efficient, 20/sec on Overdrive
				}
				// If neither moving nor firing, no consumption
			}
		}
	}
	PlayerBlackSubstrate -= TotalConsumption;
	
	// Don't let it go negative
	if (PlayerBlackSubstrate < 0)
	{
		PlayerBlackSubstrate = 0;
	}
}

float APlanetConquestPlayerController::GetSpeedMultiplier() const
{
	// If out of Black Substrate, force auxiliary mode performance
	if (PlayerBlackSubstrate <= 0)
	{
		return 0.35f; // Auxiliary: 35% speed
	}
	
	switch (BlackSubstrateMode)
	{
		case EBlackSubstrateMode::Auxiliary:
			return 0.35f;   // 35% speed
		case EBlackSubstrateMode::Efficient:
			return 1.0f;   // 100% speed (normal)
		case EBlackSubstrateMode::Overdrive:
			return 1.5f;   // 150% speed
		default:
			return 1.0f;
	}
}

float APlanetConquestPlayerController::GetFireRateMultiplier() const
{
	// If out of Black Substrate, force auxiliary mode performance
	if (PlayerBlackSubstrate <= 0)
	{
		return 0.35f; // Auxiliary: 35% fire rate
	}
	
	switch (BlackSubstrateMode)
	{
		case EBlackSubstrateMode::Auxiliary:
			return 0.35f;  // 35% fire rate
		case EBlackSubstrateMode::Efficient:
			return 1.0f;   // 100% fire rate (normal)
		case EBlackSubstrateMode::Overdrive:
			return 1.5f;   // 150% fire rate
		default:
			return 1.0f;
	}
}

void APlanetConquestPlayerController::SetBlackSubstrateMode(EBlackSubstrateMode NewMode)
{
	BlackSubstrateMode = NewMode;
	UE_LOG(LogTemp, Warning, TEXT("Vehicle Efficiency Mode: %s"), 
		NewMode == EBlackSubstrateMode::Efficient ? TEXT("Efficient") : 
		NewMode == EBlackSubstrateMode::Overdrive ? TEXT("Overdrive") : TEXT("Auxiliary"));
}

void APlanetConquestPlayerController::ToggleVehicleEfficiency()
{
	// Can only toggle between Efficient and Overdrive (not Auxiliary)
	if (BlackSubstrateMode == EBlackSubstrateMode::Efficient)
	{
		SetBlackSubstrateMode(EBlackSubstrateMode::Overdrive);
	}
	else
	{
		SetBlackSubstrateMode(EBlackSubstrateMode::Efficient);
	}
}

void APlanetConquestPlayerController::DiscoverResourceType(EResourceType ResourceType)
{
	bool bWasAlreadyDiscovered = DiscoveredResourceTypes.Contains(ResourceType);
	DiscoveredResourceTypes.Add(ResourceType);
	
	if (!bWasAlreadyDiscovered)
	{
		FString TypeName;
		switch (ResourceType)
		{
			case EResourceType::OrangeSubstrate:
				TypeName = TEXT("Orange Substrate");
				break;
			case EResourceType::BlackSubstrate:
				TypeName = TEXT("Black Substrate");
				break;
			case EResourceType::GreenSubstrate:
				TypeName = TEXT("Green Substrate (Food)");
				break;
			default:
				TypeName = TEXT("Unknown");
				break;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("===> DISCOVERED NEW RESOURCE TYPE: %s <==="), *TypeName);
	}
}

bool APlanetConquestPlayerController::IsResourceTypeDiscovered(EResourceType ResourceType) const
{
	return DiscoveredResourceTypes.Contains(ResourceType);
}

// ============================================================================
// Unified Autopilot Helper Functions
// ============================================================================

ACityActor* APlanetConquestPlayerController::FindCityForTerritory(FVector Location) const
{
	// Get all cities in the world
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), FoundCities);
	
	// Find the closest city within territory radius
	ACityActor* ClosestCity = nullptr;
	float ClosestDistance = APlanetConquestGameMode::TERRITORY_RADIUS;
	
	for (AActor* CityActor : FoundCities)
	{
		if (ACityActor* City = Cast<ACityActor>(CityActor))
		{
			float Distance = FVector::Dist(Location, City->GetActorLocation());
			if (Distance <= APlanetConquestGameMode::TERRITORY_RADIUS && Distance < ClosestDistance)
			{
				ClosestCity = City;
				ClosestDistance = Distance;
			}
		}
	}
	
	return ClosestCity;
}

TArray<AResourceActor*> APlanetConquestPlayerController::GetNeutralResourcesInTerritory(ACityActor* City) const
{
	TArray<AResourceActor*> NeutralResources;
	
	if (!City)
	{
		return NeutralResources;
	}
	
	// Get all resources in the world
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	FVector CityLocation = City->GetActorLocation();
	
	for (AActor* ResourceActor : FoundResources)
	{
		if (AResourceActor* Resource = Cast<AResourceActor>(ResourceActor))
		{
			// Only include neutral resources
			if (Resource->OwnerTeam != EOwnerTeam::Neutral)
			{
				continue;
			}
			
			// Skip resources currently being captured by other teams
			if (Resource->CapturingVehicle && IsValid(Resource->CapturingVehicle) && 
			    Resource->CapturingVehicle->OwnerTeam != EOwnerTeam::Player)
			{
				continue; // Another team has capture lock
			}
			
			// Check if resource is within territory radius
			float Distance = FVector::Dist(Resource->GetActorLocation(), CityLocation);
			if (Distance <= APlanetConquestGameMode::TERRITORY_RADIUS)
			{
				NeutralResources.Add(Resource);
			}
		}
	}
	
	return NeutralResources;
}

TArray<AResourceActor*> APlanetConquestPlayerController::GetNeutralResourcesInCluster(int32 ClusterID) const
{
	TArray<AResourceActor*> NeutralResources;
	
	if (ClusterID < 0)
	{
		return NeutralResources;
	}
	
	// Get all resources in the world
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	for (AActor* ResourceActor : FoundResources)
	{
		if (AResourceActor* Resource = Cast<AResourceActor>(ResourceActor))
		{
			// Only include neutral resources in the same cluster
			if (Resource->OwnerTeam != EOwnerTeam::Neutral || Resource->ClusterID != ClusterID)
			{
				continue;
			}
			
			// Skip resources currently being captured by other teams
			if (Resource->CapturingVehicle && IsValid(Resource->CapturingVehicle) && 
			    Resource->CapturingVehicle->OwnerTeam != EOwnerTeam::Player)
			{
				continue; // Another team has capture lock
			}
			
			NeutralResources.Add(Resource);
		}
	}
	
	return NeutralResources;
}

TArray<AMineActor*> APlanetConquestPlayerController::GetMinesInTerritory(ACityActor* City, EOwnerTeam TargetTeam) const
{
	TArray<AMineActor*> Mines;
	
	if (!City)
	{
		return Mines;
	}
	
	// Get all resources in the world (mines are attached to resources)
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	FVector CityLocation = City->GetActorLocation();
	
	for (AActor* ResourceActor : FoundResources)
	{
		if (AResourceActor* Resource = Cast<AResourceActor>(ResourceActor))
		{
			// Check if resource has a mine owned by target team
			if (Resource->Mine && IsValid(Resource->Mine) && Resource->Mine->OwnerTeam == TargetTeam)
			{
				// Check if resource is within territory radius
				float Distance = FVector::Dist(Resource->GetActorLocation(), CityLocation);
				if (Distance <= APlanetConquestGameMode::TERRITORY_RADIUS)
				{
					Mines.Add(Resource->Mine);
				}
			}
		}
	}
	
	return Mines;
}

int32 APlanetConquestPlayerController::SendMilitaryAidToAlly(EOwnerTeam AllyTeam, ACityActor* AllyCityUnderAttack)
{
	if (!AllyCityUnderAttack)
	{
		return 0;
	}
	
	// Find up to 10 idle or low-priority player vehicles
	TArray<AActor*> AllVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
	TArray<AVehicleActor*> IdleVehicles;
	
	for (AActor* Actor : AllVehicles)
	{
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (!Vehicle || !IsValid(Vehicle)) continue;
		if (Vehicle->OwnerTeam != EOwnerTeam::Player) continue;
		
		// Check if vehicle is idle (not assigned to any task)
		if (!Vehicle->bHasTarget && !Vehicle->TargetResource)
		{
			IdleVehicles.Add(Vehicle);
			if (IdleVehicles.Num() >= 10) break; // Max 10 vehicles
		}
	}
	
	if (IdleVehicles.Num() == 0)
	{
		return 0; // No idle vehicles available
	}
	
	// Find attackers at ally's city
	TArray<AVehicleActor*> Attackers;
	float TerritoryRadius = 5000.0f;
	for (AActor* Actor : AllVehicles)
	{
		AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
		if (!EnemyVehicle || !IsValid(EnemyVehicle)) continue;
		if (EnemyVehicle->OwnerTeam == AllyTeam || EnemyVehicle->OwnerTeam == EOwnerTeam::Player) continue;
		
		float DistanceToCity = FVector::Dist(EnemyVehicle->GetActorLocation(), AllyCityUnderAttack->GetActorLocation());
		if (DistanceToCity <= TerritoryRadius)
		{
			Attackers.Add(EnemyVehicle);
		}
	}
	
	if (Attackers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player: No attackers found at ally city - aid not needed"));
		return 0; // No attackers, aid not needed
	}
	
	// Send idle vehicles to attack the attackers
	int32 VehiclesSent = 0;
	for (int32 i = 0; i < FMath::Min(IdleVehicles.Num(), Attackers.Num()); i++)
	{
		AVehicleActor* AidVehicle = IdleVehicles[i];
		AVehicleActor* TargetAttacker = Attackers[i % Attackers.Num()]; // Cycle through attackers
		
		// Send vehicle to attack
		AidVehicle->SetTargetLocation(TargetAttacker->GetActorLocation());
		AidVehicle->CurrentTarget = TargetAttacker;
		AidVehicle->bHasTarget = true;
		AidVehicle->bAIAutonomousMode = false;
		AidVehicle->bInP6 = true; // Mark as Priority 6 (aid mission)
		
		// Store ally city as PrimaryTarget so vehicle knows to return home after mission
		AidVehicle->PrimaryTarget = AllyCityUnderAttack;
		
		VehiclesSent++;
		UE_LOG(LogTemp, Warning, TEXT("Player: Sent aid vehicle to attack enemy at ally city"));
	}
	
	return VehiclesSent;
}

TArray<AMineActor*> APlanetConquestPlayerController::GetMinesInCluster(int32 ClusterID, EOwnerTeam TargetTeam) const
{
	TArray<AMineActor*> Mines;
	
	if (ClusterID < 0)
	{
		return Mines;
	}
	
	// Get all resources in the world (mines are attached to resources)
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	for (AActor* ResourceActor : FoundResources)
	{
		if (AResourceActor* Resource = Cast<AResourceActor>(ResourceActor))
		{
			// Check if resource has a mine owned by target team and is in the same cluster
			if (Resource->Mine && IsValid(Resource->Mine) && 
			    Resource->Mine->OwnerTeam == TargetTeam && 
			    Resource->ClusterID == ClusterID)
			{
				Mines.Add(Resource->Mine);
			}
		}
	}
	
	return Mines;
}
