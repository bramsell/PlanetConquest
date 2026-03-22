// Copyright Epic Games, Inc. All Rights Reserved.

#include "CityActor.h"
#include "VehicleActor.h"
#include "ResourceActor.h"
#include "PlanetConquestPlayerController.h"
#include "../../World/PlanetActor.h"
#include "../../UI/CityWidget.h"
#include "../../UI/DiplomacyWidget.h"
#include "../../UI/TalkDialogueWidget.h"
#include "../Buildings/BuildingActor.h"
#include "../Buildings/CapitalBuildingActor.h"
#include "../Buildings/FactoryBuildingActor.h"
#include "../Buildings/TurretBuildingActor.h"
#include "../Buildings/LabBuildingActor.h"
#include "../../Core/PlanetConquestGameMode.h"
#include "../../Core/AITeamController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/Button.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "DrawDebugHelpers.h"
#include "../../UI/HealthBarWidget.h"
#include "../../UI/DiplomacyWidget.h"

ACityActor::ACityActor()
{
	PrimaryActorTick.bCanEverTick = true; // Enable tick for territory circle drawing

	// Create collision sphere as root
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(400.0f); // Match city ring size (scale 8.0 * 50 unit radius)
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // For selection and mouse hover
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); // Block vehicles	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // For projectiles	CollisionSphere->SetCollisionObjectType(ECC_WorldStatic); // Treat as static obstacle
	CollisionSphere->SetHiddenInGame(true); // Don't show collision visualization

	// NOTE: City no longer has a single mesh - it's composed of building actors
	// Buildings will be spawned in BeginPlay

	// Create selection box (wireframe cube)
	SelectionBox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBox"));
	SelectionBox->SetupAttachment(RootComponent);
	
	// Load cube mesh for selection box
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		SelectionBox->SetStaticMesh(CubeMesh.Object);
	}
	
	// Scale selection box slightly larger than city
	SelectionBox->SetRelativeScale3D(FVector(9.0f, 9.0f, 0.5f));
	SelectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionBox->SetVisibility(false); // Hidden by default
	SelectionBox->SetRenderCustomDepth(true);
	
	// Try to load yellow selection material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SelectionMat(TEXT("/Game/M_SelectionBox_Yellow"));
	if (SelectionMat.Succeeded())
	{
		SelectionBox->SetMaterial(0, SelectionMat.Object);
	}

	// Create health bar widget
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen); // Billboard to camera
	HealthBarWidget->SetDrawSize(FVector2D(150.0f, 15.0f)); // Larger bar for cities
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f)); // Above city
	HealthBarWidget->SetVisibility(false); // Hidden until damaged

	// Try to load health bar widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> HealthBarClass(TEXT("/Game/WBP_HealthBar"));
	if (HealthBarClass.Succeeded())
	{
		HealthBarWidget->SetWidgetClass(HealthBarClass.Class);
	}

	// Create diplomacy widget (shown when hovering over AI cities)
	DiplomacyWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DiplomacyWidget"));
	DiplomacyWidgetComponent->SetupAttachment(RootComponent);
	DiplomacyWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // Screen space - always faces camera
	DiplomacyWidgetComponent->SetDrawSize(FVector2D(450.0f, 320.0f)); // UI size in pixels (larger for better visibility)
	DiplomacyWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f)); // Center the widget on its attachment point
	DiplomacyWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f)); // Directly above city
	DiplomacyWidgetComponent->SetVisibility(false); // Hidden by default

	// Try to load diplomacy widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> DiplomacyClass(TEXT("/Game/WBP_DiplomacyUI"));
	if (DiplomacyClass.Succeeded())
	{
		DiplomacyWidgetComponent->SetWidgetClass(DiplomacyClass.Class);
	}
}

void ACityActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Note: AlignToPlanet() is now called by PlanetActor during spawning after setting correct PlanetRadius
	// Capital building spawning moved to SpawnCapitalBuilding() to be called AFTER properties are set
	
	// Assign random city name (always reassigned on BeginPlay to use updated name list)
	{
		static TArray<FString> CityNames = {
			// User's favorites
			TEXT("Akirion"), TEXT("Vega"), TEXT("Redpoint"), TEXT("Prometheus"), TEXT("Hyperion"),
			TEXT("Helios"), TEXT("Architeuthis"), TEXT("Arcturus"), TEXT("Polaris"), TEXT("Elysium"),
			TEXT("Meridian"), TEXT("Goldcrest"),
			
			// Single-word sci-fi themed names
			TEXT("Crimson"), TEXT("Azure"), TEXT("Epsilon"), TEXT("Sirius"), 
			TEXT("Orion"), TEXT("Nebula"), TEXT("Andromeda"), TEXT("Celestia"), TEXT("Titan"), 
			TEXT("Phoenix"), TEXT("Atlas"), TEXT("Chronos"), TEXT("Horizon"), TEXT("Genesis")	
		};
		
		static TArray<FString> UsedNames;
		TArray<FString> AvailableNames;
		
		// Find names that haven't been used yet
		for (const FString& Name : CityNames)
		{
			if (!UsedNames.Contains(Name))
			{
				AvailableNames.Add(Name);
			}
		}
		
		// If all names used, reset the pool
		if (AvailableNames.Num() == 0)
		{
			UsedNames.Empty();
			AvailableNames = CityNames;
		}
		
		// Pick random name
		int32 RandomIndex = FMath::RandRange(0, AvailableNames.Num() - 1);
		CityName = AvailableNames[RandomIndex];
		UsedNames.Add(CityName);
	}
	
	// Set initial color to cyan (player)
	UpdateColor();
	
	// Initialize health
	CurrentHealth = MaxHealth;
	
	// Update health bar (hidden at full health)
	UpdateHealthBar();
	
	// Verify collision settings
	if (CollisionSphere)
	{
		// bool bBlocksPawn = CollisionSphere->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
		// ECollisionEnabled::Type CollisionEnabled = CollisionSphere->GetCollisionEnabled();
		// ECollisionChannel ObjectType = CollisionSphere->GetCollisionObjectType();
		// UE_LOG(LogTemp, Warning, TEXT("City %s spawned. Collision enabled: %d, ObjectType: %d, Blocks Pawn: %d, Radius: %.0f"),
		// 	*GetName(), (int32)CollisionEnabled, (int32)ObjectType, bBlocksPawn, CollisionSphere->GetScaledSphereRadius());
	}
	
	// Set up population update timer (every 6 seconds for smooth 1% per minute growth)
	if (OwnerTeam == EOwnerTeam::Player)
	{
		GetWorld()->GetTimerManager().SetTimer(PopulationUpdateHandle, [this]()
		{
			UpdatePopulation(6.0f); // Pass delta time of 6 seconds
		}, 6.0f, true); // Repeat every 6 seconds
	}
}

void ACityActor::SpawnCapitalBuilding()
{
	if (!GetWorld() || CapitalBuilding)
	{
		return; // Already has a capital or no world
	}

	// Spawn capital building (cities always start with a capital)
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	CapitalBuilding = GetWorld()->SpawnActor<ACapitalBuildingActor>(ACapitalBuildingActor::StaticClass(), GetActorLocation(), GetActorRotation(), SpawnParams);
	
	if (CapitalBuilding)
	{
		CapitalBuilding->OwnerTeam = OwnerTeam;
		CapitalBuilding->PlanetCenter = PlanetCenter;
		CapitalBuilding->PlanetRadius = PlanetRadius;
		CapitalBuilding->ParentCity = this;
		
		// Debug: Log positions before alignment
		FVector CityLocation = GetActorLocation();
		float CityDistanceFromCenter = FVector::Dist(CityLocation, PlanetCenter);
		UE_LOG(LogTemp, Warning, TEXT("City %s: PlanetCenter=%s, City at %s, CityDistance=%.1f, Expected PlanetRadius=%.1f"),
			*CityName, *PlanetCenter.ToString(), *CityLocation.ToString(), CityDistanceFromCenter, PlanetRadius);
		
		CapitalBuilding->AlignToPlanet();
		
		// Debug: Log positions after alignment
		FVector CapitalLocation = CapitalBuilding->GetActorLocation();
		float CapitalDistanceFromCenter = FVector::Dist(CapitalLocation, PlanetCenter);
		UE_LOG(LogTemp, Warning, TEXT("  Capital Building: Before align at %s, After align at %s, ActualDistance=%.1f, ExpectedRadius=%.1f, Diff=%.1f"),
			*GetActorLocation().ToString(), *CapitalLocation.ToString(), CapitalDistanceFromCenter, PlanetRadius, 
			CapitalDistanceFromCenter - PlanetRadius);
		
		CapitalBuilding->UpdateColor();
		Buildings.Add(CapitalBuilding);
		
		// Attach diplomacy widget to capital building so it's centered on the visual building
		if (DiplomacyWidgetComponent)
		{
			DiplomacyWidgetComponent->AttachToComponent(CapitalBuilding->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			DiplomacyWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f)); // Above the building
		}
	}
}

void ACityActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Track time since any building was damaged (for healing system)
	TimeSinceAnyBuildingDamaged += DeltaTime;
	
	// Draw territory circle
	if (bShowTerritoryCircle)
	{
		DrawTerritoryCircle();
	}
	
	// Check for mouse hover on cities (for diplomacy widget)
	if (OwnerTeam != EOwnerTeam::Neutral)
	{
		// Get player controller
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			bool bMouseCurrentlyOver = false;
			
			// Use screen-space distance check - works from any camera angle
			if (CapitalBuilding)
			{
				// Check if building is blocked by the planet (raycast from camera to building)
				FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
				FVector BuildingLocation = CapitalBuilding->GetActorLocation();
				
				FHitResult VisibilityHit;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(CapitalBuilding); // Ignore the building itself
				QueryParams.AddIgnoredActor(this); // Ignore city actor
				
				// Trace from camera to building - if we hit something, building is blocked
				bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
					VisibilityHit,
					CameraLocation,
					BuildingLocation,
					ECC_Visibility, // Use Visibility channel - same as mouse cursor traces
					QueryParams
				);
				
				// Building is only blocked if we hit something BEFORE reaching the building
				float DistanceToBuilding = FVector::Dist(CameraLocation, BuildingLocation);
				float DistanceToHit = VisibilityHit.Distance;
				bool bIsBlocked = bHitSomething && (DistanceToHit < DistanceToBuilding - 100.0f); // 100 unit tolerance
				
				// Only show UI if building is NOT blocked
				if (!bIsBlocked)
				{
					// Project building location to screen coordinates
					FVector2D BuildingScreenPos;
					if (PC->ProjectWorldLocationToScreen(BuildingLocation, BuildingScreenPos))
					{
						// Get mouse position
						float MouseX, MouseY;
						PC->GetMousePosition(MouseX, MouseY);
						
						// Calculate screen-space distance
						float ScreenDistance = FVector2D::Distance(FVector2D(MouseX, MouseY), BuildingScreenPos);
						
						// Hover threshold in pixels - use hysteresis (smaller to open, larger to keep open)
						const float OpenHoverRadiusPixels = 50.0f;  // Reduced threshold to initially open UI
						const float CloseHoverRadiusPixels = 100.0f; // Larger threshold to keep UI open
						float CurrentThreshold = bIsMouseHovering ? CloseHoverRadiusPixels : OpenHoverRadiusPixels;
						
						if (ScreenDistance <= CurrentThreshold)
						{
							bMouseCurrentlyOver = true;
						}
					}
				}
			}
			
			// Use hysteresis: need to move further away to close than to open
			if (bMouseCurrentlyOver && !bIsMouseHovering)
			{
			// Mouse entered hover zone - check if city UI is open
			// Don't show diplomacy widget if player is inside this city's UI
			APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
			bool bCityUIOpen = PlayerController && PlayerController->CityWidgetInstance && 
			                   PlayerController->CityWidgetInstance->GetVisibility() == ESlateVisibility::Visible;
			
			if (!bCityUIOpen)
			{
				bIsMouseHovering = true;
				ShowDiplomacyWidget();
			}
		}
		else if (!bMouseCurrentlyOver && bIsMouseHovering)
		{
			bIsMouseHovering = false;
			HideDiplomacyWidget();
		}
		
		// Update button hover states while widget is visible
		if (bIsMouseHovering && DiplomacyWidgetComponent && DiplomacyWidgetComponent->IsVisible())
			{
				// Get mouse position
				float MouseX, MouseY;
				PC->GetMousePosition(MouseX, MouseY);
				
				// Project widget location to screen
				FVector WidgetWorldLocation = DiplomacyWidgetComponent->GetComponentLocation();
				FVector2D WidgetScreenPosition;
				if (PC->ProjectWorldLocationToScreen(WidgetWorldLocation, WidgetScreenPosition))
				{
					if (UDiplomacyWidget* Widget = Cast<UDiplomacyWidget>(DiplomacyWidgetComponent->GetUserWidgetObject()))
					{
						if (OwnerTeam == EOwnerTeam::Player)
						{
							// For player cities, check if hovering over City Editor button
							// Button size: 100x15 pixels, shifted up by 15 pixels
							float ButtonHalfWidth = 50.0f;   // 100 / 2
							float ButtonHalfHeight = 7.5f;   // 15 / 2
							float ButtonCenterY = WidgetScreenPosition.Y - 15.0f;  // Shift up
							
							bool bHoveringCityEditor = 
								FMath::Abs(MouseX - WidgetScreenPosition.X) <= ButtonHalfWidth && 
								FMath::Abs(MouseY - ButtonCenterY) <= ButtonHalfHeight;
							
							Widget->UpdateCityEditorButtonHoverState(bHoveringCityEditor);
						}
						else
						{
							// For AI cities, calculate relative position within widget
							float HalfWidth = 225.0f;  // 450 / 2
							float RelativeX = (MouseX - WidgetScreenPosition.X) / HalfWidth;
							
							// Determine which button is being hovered
							bool bHoveringTalk = RelativeX < 0.0f;
							bool bHoveringAttack = RelativeX >= 0.0f;
							
							// Update hover visual states
							Widget->UpdateButtonHoverStates(bHoveringAttack, bHoveringTalk);
						}
					}
				}
				else
				{
					// Widget not on screen - reset hover states
					if (UDiplomacyWidget* Widget = Cast<UDiplomacyWidget>(DiplomacyWidgetComponent->GetUserWidgetObject()))
					{
						if (OwnerTeam == EOwnerTeam::Player)
						{
							Widget->UpdateCityEditorButtonHoverState(false);
						}
						else
						{
							Widget->UpdateButtonHoverStates(false, false);
						}
					}
				}
			}
			else if (DiplomacyWidgetComponent && DiplomacyWidgetComponent->IsVisible())
			{
				// Widget is visible but mouse is not hovering - reset hover states
				if (UDiplomacyWidget* Widget = Cast<UDiplomacyWidget>(DiplomacyWidgetComponent->GetUserWidgetObject()))
				{
					if (OwnerTeam == EOwnerTeam::Player)
					{
						Widget->UpdateCityEditorButtonHoverState(false);
					}
					else
					{
						Widget->UpdateButtonHoverStates(false, false);
					}
				}
			}
			
			// Handle button clicks while widget is visible
			if (bIsMouseHovering && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
			{
				HandleDiplomacyWidgetClick();
			}
		}
	}
}

void ACityActor::AlignToPlanet()
{
	// Get direction from planet center to this city
	FVector CityLocation = GetActorLocation();
	FVector Direction = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Calculate distance from planet center
	float CurrentDistance = FVector::Dist(CityLocation, PlanetCenter);
	
	// If city is at planet center, place it on surface at default position
	if (CurrentDistance < 10.0f)
	{
		Direction = FVector(0.0f, 0.0f, 1.0f); // Default to top of planet
		SetActorLocation(PlanetCenter + Direction * PlanetRadius);
	}
	else
	{
		// Move city to planet surface
		SetActorLocation(PlanetCenter + Direction * PlanetRadius);
	}
	
	// Orient city so its up (Z-axis) points away from planet center
	// This makes the flat ring lie on the planet surface
	FRotator LookRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
	SetActorRotation(LookRotation);
	
	// UE_LOG(LogTemp, Log, TEXT("City aligned to planet at location: %s"), *GetActorLocation().ToString());
}

void ACityActor::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	
	if (SelectionBox)
	{
		SelectionBox->SetVisibility(bSelected);
		// UE_LOG(LogTemp, Warning, TEXT("City %s selection box set to: %s (Box valid: %s)"), 
		// 	*GetName(), 
		// 	bSelected ? TEXT("VISIBLE") : TEXT("HIDDEN"),
		// 	SelectionBox ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		// UE_LOG(LogTemp, Error, TEXT("City %s has no SelectionBox component!"), *GetName());
	}
}

void ACityActor::ShowDiplomacyWidget()
{
	if (DiplomacyWidgetComponent)
	{
		DiplomacyWidgetComponent->SetVisibility(true);
		
		// Update widget with current city data
		if (UDiplomacyWidget* Widget = Cast<UDiplomacyWidget>(DiplomacyWidgetComponent->GetUserWidgetObject()))
		{
			Widget->SetCity(this);
			// Reset button hover states when showing widget
			Widget->UpdateButtonHoverStates(false, false);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Diplomacy widget component exists but GetUserWidgetObject returned NULL for city: %s"), *CityName);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DiplomacyWidgetComponent is NULL for city: %s"), *CityName);
	}
}

void ACityActor::HideDiplomacyWidget()
{
	if (DiplomacyWidgetComponent)
	{
		DiplomacyWidgetComponent->SetVisibility(false);
		
		// Reset button hover states when widget is hidden
		if (UDiplomacyWidget* Widget = Cast<UDiplomacyWidget>(DiplomacyWidgetComponent->GetUserWidgetObject()))
		{
			Widget->UpdateButtonHoverStates(false, false);
		}
	}
}

void ACityActor::HandleDiplomacyWidgetClick()
{
	if (!DiplomacyWidgetComponent || !DiplomacyWidgetComponent->IsVisible())
	{
		return;
	}
	
	// Get player controller
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	
	// Get mouse position on screen
	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);
	
	// Project widget location to screen
	FVector WidgetWorldLocation = DiplomacyWidgetComponent->GetComponentLocation();
	FVector2D WidgetScreenPosition;
	if (!PC->ProjectWorldLocationToScreen(WidgetWorldLocation, WidgetScreenPosition))
	{
		return; // Widget not visible on screen
	}
	
	// Check if mouse is within widget bounds (450x320 pixels)
	float HalfWidth = 225.0f;  // 450 / 2
	float HalfHeight = 160.0f; // 320 / 2
	
	if (FMath::Abs(MouseX - WidgetScreenPosition.X) > HalfWidth || 
	    FMath::Abs(MouseY - WidgetScreenPosition.Y) > HalfHeight)
	{
		return; // Click is outside widget
	}
	
	if (UDiplomacyWidget* Widget = Cast<UDiplomacyWidget>(DiplomacyWidgetComponent->GetUserWidgetObject()))
	{
		if (OwnerTeam == EOwnerTeam::Player)
		{
			// Player city - check if click is within City Editor button bounds (100x15 pixels, shifted up by 15 pixels)
			float ButtonHalfWidth = 50.0f;   // 100 / 2
			float ButtonHalfHeight = 7.5f;   // 15 / 2
			float ButtonCenterY = WidgetScreenPosition.Y - 15.0f;  // Shift up
			
			if (FMath::Abs(MouseX - WidgetScreenPosition.X) <= ButtonHalfWidth && 
			    FMath::Abs(MouseY - ButtonCenterY) <= ButtonHalfHeight)
			{
				// Click is within button bounds
				UE_LOG(LogTemp, Log, TEXT("City Editor button clicked for city: %s"), *CityName);
				Widget->OnCityEditorButtonClicked();
				Widget->PlayCityEditorButtonAnimation();
			}
		}
		else
		{
			// AI city - handle Talk/Attack button clicks
			// Calculate relative position within widget (-1 to 1)
			float RelativeX = (MouseX - WidgetScreenPosition.X) / HalfWidth;
			
			// Determine which button is being clicked (left = Talk, right = Attack)
			bool bClickingTalk = RelativeX < 0.0f;
			bool bClickingAttack = RelativeX >= 0.0f;
			
			if (bClickingTalk) // Left side - Talk button
			{
				UE_LOG(LogTemp, Warning, TEXT("Talk button clicked for city: %s"), *CityName);
				Widget->OnTalkButtonClicked();
				
				// Play button click animation/visual feedback
				Widget->PlayTalkButtonAnimation();
			}
			else // Right side - Attack button
			{
				UE_LOG(LogTemp, Warning, TEXT("Attack button clicked for city: %s"), *CityName);
				Widget->OnAttackButtonClicked();
				
				// Play button click animation/visual feedback
				Widget->PlayAttackButtonAnimation();
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to cast widget to UDiplomacyWidget!"));
	}
}

void ACityActor::UpdateColor()
{
	// Update color of all buildings in this city
	for (ABuildingActor* Building : Buildings)
	{
		if (Building)
		{
			Building->OwnerTeam = OwnerTeam;
			Building->UpdateColor();
		}
	}
}

void ACityActor::SpawnVehicle()
{
	if (!GetWorld())
	{
		// UE_LOG(LogTemp, Error, TEXT("Cannot spawn vehicle: No world!"));
		return;
	}

	// Get player controller and check money
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		// UE_LOG(LogTemp, Error, TEXT("Cannot spawn vehicle: No player controller!"));
		return;
	}

	// Check if player has enough Orange Substrate
	if (PC->PlayerOrangeSubstrate < VehicleCost)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Cannot spawn vehicle: Insufficient Orange Substrate! Need %d, have %d"), VehicleCost, PC->PlayerOrangeSubstrate);
		return;
	}

	// Deduct the cost
	PC->PlayerOrangeSubstrate -= VehicleCost;
	// UE_LOG(LogTemp, Warning, TEXT("Spawning vehicle for %d OS. Remaining: %d"), VehicleCost, PC->PlayerOrangeSubstrate);

	// Get city location and direction from planet center
	FVector CityLocation = GetActorLocation();
	FVector CityDirection = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Generate a random offset on the planet surface around the city
	// Create a random tangent vector perpendicular to the city direction
	FVector RandomTangent1 = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
	if (RandomTangent1.IsNearlyZero())
	{
		RandomTangent1 = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
	}
	FVector RandomTangent2 = FVector::CrossProduct(CityDirection, RandomTangent1).GetSafeNormal();
	
	// Random offset in tangent space
	float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	// Spawn vehicles between 1500 and 2500 units from city center
	float MinSpawnDistance = 1500.0f;
	float MaxSpawnDistance = 2500.0f;
	float RandomDistance = FMath::FRandRange(MinSpawnDistance, MaxSpawnDistance);
	
	FVector RandomOffset = (RandomTangent1 * FMath::Cos(RandomAngle) + RandomTangent2 * FMath::Sin(RandomAngle)) * RandomDistance;
	
	// Calculate spawn position on planet surface
	FVector SpawnDirection = (CityDirection * PlanetRadius + RandomOffset).GetSafeNormal();
	
	// Get actual terrain height at spawn location
	float ActualTerrainRadius = PlanetRadius; // Default fallback
	if (OwningPlanet)
	{
		float TerrainHeight = OwningPlanet->CalculateHeightAtPoint(SpawnDirection);
		ActualTerrainRadius = OwningPlanet->PlanetRadius * (1.0f + TerrainHeight);
	}
	
	FVector SpawnLocation = SpawnDirection * ActualTerrainRadius;
	
	// Spawn the vehicle
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	
	AVehicleActor* NewVehicle = GetWorld()->SpawnActor<AVehicleActor>(AVehicleActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	
	if (NewVehicle)
	{
		NewVehicle->PlanetCenter = PlanetCenter;
		NewVehicle->PlanetRadius = ActualTerrainRadius; // Use actual terrain height
		NewVehicle->OwnerTeam = OwnerTeam; // Inherit owner from city
		NewVehicle->OwningPlanet = OwningPlanet; // Pass planet reference for terrain queries
		NewVehicle->AlignToPlanet();
		NewVehicle->UpdateColor();
		
		UE_LOG(LogTemp, Warning, TEXT("Vehicle spawned near city %s at location: %s"), *GetName(), *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn vehicle!"));
	}
}

void ACityActor::ApplyDamage(float DamageAmount, EOwnerTeam AttackingTeam, AActor* AttackingActor)
{
	// Don't damage cities owned by the attacking team
	if (AttackingTeam == OwnerTeam)
	{
		return;
	}

	CurrentHealth -= DamageAmount;
	LastDamagingTeam = AttackingTeam;

	// Update relationships based on damage
	if (UWorld* World = GetWorld())
	{
		if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode()))
		{
			// Calculate damage as percentage of max health
			float DamagePercent = (DamageAmount / MaxHealth) * 100.0f;
			
			// Call relationship update (cities are treated as non-capitals for now)
			GameMode->OnBuildingDamaged(AttackingTeam, OwnerTeam, DamagePercent, false, TEXT("CityActor"));
			
			// On first attack, trigger enemy-of-enemy bonuses
			if (!bHasBeenAttacked && AttackingTeam != EOwnerTeam::Neutral)
			{
				bHasBeenAttacked = true;
				GameMode->OnTeamAttackedTeam(AttackingTeam, OwnerTeam);
			}
		}
	}

	// Update health bar
	UpdateHealthBar();

	// Check if city was captured
	if (CurrentHealth <= 0.0f)
	{
		FlipOwnership(LastDamagingTeam);
	}
}

void ACityActor::FlipOwnership(EOwnerTeam NewTeam)
{
	UE_LOG(LogTemp, Display, TEXT("[FEED] City %s captured by Team %d!"), *GetName(), (int32)NewTeam);
	
	EOwnerTeam OldTeam = OwnerTeam; // Store old owner before changing
	
	OwnerTeam = NewTeam;
	CurrentHealth = MaxHealth; // Reset health on capture
	UpdateColor(); // Update visual color to reflect new owner
	UpdateHealthBar(); // Update health bar (should hide at full health)
	
	// Update player's controlled cities list
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PlayerController)
	{
		// Remove from old owner's list
		if (OldTeam == EOwnerTeam::Player)
		{
			PlayerController->ControlledCities.Remove(this);
		}
		
		// Add to new owner's list
		if (NewTeam == EOwnerTeam::Player)
		{
			if (!PlayerController->ControlledCities.Contains(this))
			{
				PlayerController->ControlledCities.Add(this);
				UE_LOG(LogTemp, Warning, TEXT("Player now controls city: %s"), *GetName());
			}
		}
	}
	
	// Close any open UI widgets for this city to prevent crashes
	HideDiplomacyWidget();
	
	// Find and close any TalkDialogueWidget that's showing this city
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC)
	{
		// Get all widgets on screen
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UTalkDialogueWidget::StaticClass());
		
		for (UUserWidget* Widget : FoundWidgets)
		{
			UTalkDialogueWidget* TalkWidget = Cast<UTalkDialogueWidget>(Widget);
			if (TalkWidget && TalkWidget->CurrentCity == this)
			{
				TalkWidget->CloseDialogue();
				UE_LOG(LogTemp, Warning, TEXT("Closed TalkDialogueWidget for captured city %s"), *GetName());
			}
		}
	}
	
	// Clear all attacking vehicles targeting this city or its buildings
	TArray<AActor*> AllVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
	
	for (AActor* Actor : AllVehicles)
	{
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (!Vehicle) continue;
		
		// Check if this vehicle is targeting this city or any of its buildings
		bool bWasTargetingThisCity = false;
		
		if (Vehicle->CurrentTarget == this || Vehicle->CurrentTarget == CapitalBuilding)
		{
			bWasTargetingThisCity = true;
		}
		else
		{
			// Check if targeting any building in this city
			for (ABuildingActor* Building : Buildings)
			{
				if (Vehicle->CurrentTarget == Building)
				{
					bWasTargetingThisCity = true;
					break;
				}
			}
		}
		
		if (bWasTargetingThisCity)
		{
			// Clear target and remove from war mode
			Vehicle->CurrentTarget = nullptr;
			Vehicle->bHasTarget = false;
			Vehicle->bInP5 = false; // Remove from war party
			Vehicle->bIsFiring = false;
			
			UE_LOG(LogTemp, Warning, TEXT("Cleared target for vehicle %s after city %s was captured"),
				*Vehicle->GetName(), *GetName());
		}
	}
	
	// Close city editor if this city was player-owned and is being captured
	if (OldTeam == EOwnerTeam::Player && NewTeam != EOwnerTeam::Player)
	{
		if (PlayerController && PlayerController->CityWidgetInstance && PlayerController->CityWidgetInstance->GetVisibility() == ESlateVisibility::Visible)
		{
			PlayerController->CityWidgetInstance->OnCloseButtonClicked();
			UE_LOG(LogTemp, Warning, TEXT("City editor force-closed: city captured by enemy"));
		}
	}
	
	// Check if the old team has lost all cities (team elimination)
	if (OldTeam != EOwnerTeam::Neutral && OldTeam != NewTeam)
	{
		// Find all cities in the world
		TArray<AActor*> AllCities;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), AllCities);
		
		// Count cities owned by old team
		int32 RemainingCities = 0;
		for (AActor* Actor : AllCities)
		{
			ACityActor* City = Cast<ACityActor>(Actor);
			if (City && City->OwnerTeam == OldTeam)
			{
				RemainingCities++;
			}
		}
		
		// If old team has no cities left, they are eliminated - neutralize all their resources
		if (RemainingCities == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("[FEED] TEAM %d ELIMINATED! All cities lost."), (int32)OldTeam);
			
			// Neutralize all resources owned by eliminated team
			TArray<AActor*> AllResources;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), AllResources);
			
			for (AActor* Actor : AllResources)
			{
				AResourceActor* Resource = Cast<AResourceActor>(Actor);
				if (Resource && Resource->OwnerTeam == OldTeam)
				{
					Resource->CaptureForTeam(EOwnerTeam::Neutral);
					UE_LOG(LogTemp, Warning, TEXT("Resource %s returned to neutral after team %d elimination"), 
						*Resource->GetName(), (int32)OldTeam);
				}
			}
			
			// Destroy all vehicles owned by eliminated team
			TArray<AActor*> TeamVehicles;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), TeamVehicles);
			
			for (AActor* Actor : TeamVehicles)
			{
				AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
				if (Vehicle && Vehicle->OwnerTeam == OldTeam)
				{
					UE_LOG(LogTemp, Warning, TEXT("Vehicle %s destroyed after team %d elimination"), 
						*Vehicle->GetName(), (int32)OldTeam);
					Vehicle->Destroy();
				}
			}

			// Clean up alliances - remaining teams should become unaffiliated if they have no other allies
			if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GameMode->CleanupAlliancesForEliminatedTeam(OldTeam);
			}
		}
	}
	
	// Notify AI controllers that city counts have changed (affects substrate requirements)
	TArray<AActor*> AllAIControllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), AllAIControllers);
	
	for (AActor* Actor : AllAIControllers)
	{
		AAITeamController* AIController = Cast<AAITeamController>(Actor);
		if (AIController)
		{
			// Notify both the team that gained the city and the team that lost it
			if (AIController->ControlledTeam == NewTeam || AIController->ControlledTeam == OldTeam)
			{
				AIController->OnCityCountChanged();
			}
		}
	}
}

void ACityActor::UpdateHealthBar()
{
	if (!HealthBarWidget) return;

	float HealthPercent = CurrentHealth / MaxHealth;
	
	// Only show health bar when damaged
	bool bShouldShowHealthBar = HealthPercent < 1.0f;
	
	// Check if city is occluded by planet
	if (bShouldShowHealthBar)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			FVector CityLocation = GetActorLocation();
			
			FHitResult VisibilityHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			if (CapitalBuilding)
			{
				QueryParams.AddIgnoredActor(CapitalBuilding);
			}
			
			bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
				VisibilityHit,
				CameraLocation,
				CityLocation,
				ECC_Visibility,
				QueryParams
			);
			
			// Hide if city is blocked by planet
			float DistanceToCity = FVector::Dist(CameraLocation, CityLocation);
			float DistanceToHit = VisibilityHit.Distance;
			if (bHitSomething && (DistanceToHit < DistanceToCity - 100.0f))
			{
				bShouldShowHealthBar = false;
			}
		}
	}
	
	HealthBarWidget->SetVisibility(bShouldShowHealthBar);

	// Update the widget's health percent
	if (UHealthBarWidget* HealthWidget = Cast<UHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
	{
		HealthWidget->SetHealthPercent(HealthPercent);
	}
}

AFactoryBuildingActor* ACityActor::AddFactory()
{
	if (!GetWorld()) return nullptr;

	// Check if we need to start a new chunk
	if (CurrentChunkIndex == -1 || FactoriesInCurrentChunk >= 4)
	{
		// Need to start a new chunk
		if (AvailableChunkIndices.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("City: No more available factory chunk slots!"));
			return nullptr;
		}
		
		// Prioritize inner ring (0-5) before allowing outer ring (6-15)
		TArray<int32> InnerRingChunks;
		TArray<int32> OuterRingChunks;
		
		for (int32 ChunkIndex : AvailableChunkIndices)
		{
			if (ChunkIndex < 6)
			{
				InnerRingChunks.Add(ChunkIndex);
			}
			else
			{
				OuterRingChunks.Add(ChunkIndex);
			}
		}
		
		// Pick from inner ring if any available, otherwise outer ring
		TArray<int32>& ChunksToUse = (InnerRingChunks.Num() > 0) ? InnerRingChunks : OuterRingChunks;
		int32 RandomIndex = FMath::RandRange(0, ChunksToUse.Num() - 1);
		CurrentChunkIndex = ChunksToUse[RandomIndex];
		
		// Remove from AvailableChunkIndices
		AvailableChunkIndices.Remove(CurrentChunkIndex);
		UsedChunkIndices.Add(CurrentChunkIndex);
		FactoriesInCurrentChunk = 0;
	}
	
	// Calculate position for this factory
	FVector SpawnLocation = CalculateFactoryPosition(CurrentChunkIndex, FactoriesInCurrentChunk);
	
	// Rotation: Face chunk outward from city center
	// Calculate the chunk angle to face outward
	float ChunkAngle;
	if (CurrentChunkIndex < 6)
	{
		// Inner ring: 6 chunks at 60° intervals
		ChunkAngle = CurrentChunkIndex * 60.0f;
	}
	else
	{
		// Outer ring: 10 chunks at 36° intervals
		// Offset by 18° so outer chunks sit between inner chunks
		int32 OuterIndex = CurrentChunkIndex - 6;
		ChunkAngle = (OuterIndex * 36.0f) + 18.0f;
	}
	
	FRotator BaseRotation = GetActorRotation();
	FRotator SpawnRotation = BaseRotation + FRotator(0.0f, ChunkAngle, 0.0f); // Face outward from city
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFactoryBuildingActor* Factory = GetWorld()->SpawnActor<AFactoryBuildingActor>(AFactoryBuildingActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	if (Factory)
	{
		Factory->OwnerTeam = OwnerTeam;
		Factory->PlanetCenter = PlanetCenter;
		Factory->PlanetRadius = PlanetRadius;
		Factory->ParentCity = this;
		Factory->AlignToPlanet();
		Factory->UpdateColor();
		Buildings.Add(Factory);
		FactoriesInCurrentChunk++;
		
		// Expand turret ring if we just added a factory to outer ring
		// (turret radius needs to expand as soon as ANY outer factory exists)
		if (CurrentChunkIndex >= 6)
		{
			RepositionTurrets();
		}
	}
	return Factory;
}

ATurretBuildingActor* ACityActor::AddTurret()
{
	if (!GetWorld()) return nullptr;

	// Maximum 8 turrets per city
	if (GetTurretCount() >= 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("City: Maximum turret limit (8) reached!"));
		return nullptr;
	}

	// Turrets will be positioned by RepositionTurrets(), so spawn at city location temporarily
	FVector SpawnLocation = GetActorLocation();
	FRotator SpawnRotation = GetActorRotation();
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ATurretBuildingActor* Turret = GetWorld()->SpawnActor<ATurretBuildingActor>(ATurretBuildingActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	if (Turret)
	{
		Turret->OwnerTeam = OwnerTeam;
		Turret->PlanetCenter = PlanetCenter;
		Turret->PlanetRadius = PlanetRadius;
		Turret->ParentCity = this;
		Buildings.Add(Turret);
		
		// Reposition all turrets to be evenly distributed
		RepositionTurrets();
	}
	return Turret;
}

int32 ACityActor::GetFactoryCount() const
{
	int32 Count = 0;
	for (ABuildingActor* Building : Buildings)
	{
		if (Building && Building->IsA(AFactoryBuildingActor::StaticClass()))
		{
			Count++;
		}
	}
	return Count;
}

int32 ACityActor::GetTurretCount() const
{
	int32 Count = 0;
	for (ABuildingActor* Building : Buildings)
	{
		if (Building && Building->IsA(ATurretBuildingActor::StaticClass()))
		{
			Count++;
		}
	}
	return Count;
}

ALabBuildingActor* ACityActor::AddLab()
{
	if (!GetWorld()) return nullptr;

	// Labs take up an entire chunk
	if (AvailableChunkIndices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("City: No more available lab chunk slots!"));
		return nullptr;
	}
	
	// Prioritize inner ring (0-5) before allowing outer ring (6-15)
	TArray<int32> InnerRingChunks;
	TArray<int32> OuterRingChunks;
	
	for (int32 ChunkIndex : AvailableChunkIndices)
	{
		if (ChunkIndex < 6)
		{
			InnerRingChunks.Add(ChunkIndex);
		}
		else
		{
			OuterRingChunks.Add(ChunkIndex);
		}
	}
	
	// Pick from inner ring if any available, otherwise outer ring
	TArray<int32>& ChunksToUse = (InnerRingChunks.Num() > 0) ? InnerRingChunks : OuterRingChunks;
	int32 RandomIndex = FMath::RandRange(0, ChunksToUse.Num() - 1);
	int32 SelectedChunk = ChunksToUse[RandomIndex];
	
	// Remove from AvailableChunkIndices and add to UsedChunkIndices
	AvailableChunkIndices.Remove(SelectedChunk);
	UsedChunkIndices.Add(SelectedChunk);
	
	// If we were filling this chunk with factories, reset the tracker
	if (CurrentChunkIndex == SelectedChunk)
	{
		CurrentChunkIndex = -1;
		FactoriesInCurrentChunk = 0;
	}
	
	// Calculate chunk center position (no grid offset - lab uses whole chunk)
	FVector SpawnLocation = CalculateLabPosition(SelectedChunk);
	
	// Rotation: Face chunk outward from city center
	float ChunkAngle;
	if (SelectedChunk < 6)
	{
		// Inner ring: 6 chunks at 60° intervals
		ChunkAngle = SelectedChunk * 60.0f;
	}
	else
	{
		// Outer ring: 10 chunks at 36° intervals
		int32 OuterIndex = SelectedChunk - 6;
		ChunkAngle = (OuterIndex * 36.0f) + 18.0f;
	}
	
	FRotator BaseRotation = GetActorRotation();
	FRotator SpawnRotation = BaseRotation + FRotator(0.0f, ChunkAngle, 0.0f);
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ALabBuildingActor* Lab = GetWorld()->SpawnActor<ALabBuildingActor>(ALabBuildingActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
	if (Lab)
	{
		Lab->OwnerTeam = OwnerTeam;
		Lab->PlanetCenter = PlanetCenter;
		Lab->PlanetRadius = PlanetRadius;
		Lab->ParentCity = this;
		Lab->AlignToPlanet();
		Lab->UpdateColor();
		Buildings.Add(Lab);
		
		// Expand turret ring if we just added a lab to outer ring
		if (SelectedChunk >= 6)
		{
			RepositionTurrets();
		}
	}
	return Lab;
}

int32 ACityActor::GetLabCount() const
{
	int32 Count = 0;
	for (ABuildingActor* Building : Buildings)
	{
		if (Building && Building->IsA(ALabBuildingActor::StaticClass()))
		{
			Count++;
		}
	}
	return Count;
}

FVector ACityActor::CalculateLabPosition(int32 ChunkIndex)
{
	// Calculate the center position of the chunk (labs take the whole chunk)
	float ChunkAngle;
	float ChunkRadius;
	
	if (ChunkIndex < 6)
	{
		// Inner ring: 6 positions at 60° intervals
		ChunkAngle = ChunkIndex * 60.0f;
		ChunkRadius = 500.0f;
	}
	else
	{
		// Outer ring: 10 positions at 36° intervals
		int32 OuterIndex = ChunkIndex - 6;
		ChunkAngle = (OuterIndex * 36.0f) + 18.0f;
		ChunkRadius = 1100.0f;
	}
	
	float ChunkAngleRad = FMath::DegreesToRadians(ChunkAngle);
	
	// Get direction from planet center through capital
	FVector CityLocation = GetActorLocation();
	FVector ToCityFromCenter = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Create a coordinate system on the surface
	FVector ArbitraryUp = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(ToCityFromCenter, ArbitraryUp)) > 0.9f)
	{
		ArbitraryUp = FVector::ForwardVector;
	}
	
	FVector TangentX = FVector::CrossProduct(ArbitraryUp, ToCityFromCenter).GetSafeNormal();
	FVector TangentY = FVector::CrossProduct(ToCityFromCenter, TangentX).GetSafeNormal();
	
	// Calculate chunk center position (no grid offset for labs)
	FVector ChunkOffset = TangentX * FMath::Cos(ChunkAngleRad) * ChunkRadius +
	                      TangentY * FMath::Sin(ChunkAngleRad) * ChunkRadius;
	
	return CityLocation + ChunkOffset;
}

FVector ACityActor::CalculateFactoryPosition(int32 ChunkIndex, int32 PositionInChunk)
{
	// Two-ring chunk system:
	// Inner ring (0-5): 6 chunks at 60° intervals, closer to capital
	// Outer ring (6-15): 10 chunks at 36° intervals, farther from capital
	
	float ChunkAngle;
	float ChunkRadius;
	
	if (ChunkIndex < 6)
	{
		// Inner ring: 6 positions at 60° intervals
		ChunkAngle = ChunkIndex * 60.0f;
		ChunkRadius = 500.0f;
	}
	else
	{
		// Outer ring: 10 positions at 36° intervals
		// Offset by 18° so outer chunks sit between inner chunks
		int32 OuterIndex = ChunkIndex - 6;
		ChunkAngle = (OuterIndex * 36.0f) + 18.0f;
		ChunkRadius = 1100.0f; // Much farther out for second ring (more space between rings)
	}
	
	float ChunkAngleRad = FMath::DegreesToRadians(ChunkAngle);
	
	// Get direction from planet center through capital (this becomes "up" for the city)
	FVector CityLocation = GetActorLocation();
	FVector ToCityFromCenter = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Create a coordinate system on the surface
	// "North" is arbitrary - use cross product to get tangent directions
	FVector ArbitraryUp = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(ToCityFromCenter, ArbitraryUp)) > 0.9f)
	{
		ArbitraryUp = FVector::ForwardVector;
	}
	
	FVector TangentX = FVector::CrossProduct(ArbitraryUp, ToCityFromCenter).GetSafeNormal();
	FVector TangentY = FVector::CrossProduct(ToCityFromCenter, TangentX).GetSafeNormal();
	
	// Calculate chunk center position on the tangent plane
	FVector ChunkOffset = TangentX * FMath::Cos(ChunkAngleRad) * ChunkRadius +
	                      TangentY * FMath::Sin(ChunkAngleRad) * ChunkRadius;
	
	// Within each chunk, arrange 4 factories in a 2x2 grid
	// PositionInChunk: 0=TL, 1=TR, 2=BL, 3=BR
	float FactorySpacing = 80.0f; // Space between factories in a chunk
	FVector GridOffset = FVector::ZeroVector;
	
	switch (PositionInChunk)
	{
		case 0: // Top-left
			GridOffset = TangentX * (-FactorySpacing) + TangentY * FactorySpacing;
			break;
		case 1: // Top-right
			GridOffset = TangentX * FactorySpacing + TangentY * FactorySpacing;
			break;
		case 2: // Bottom-left
			GridOffset = TangentX * (-FactorySpacing) + TangentY * (-FactorySpacing);
			break;
		case 3: // Bottom-right
			GridOffset = TangentX * FactorySpacing + TangentY * (-FactorySpacing);
			break;
	}
	
	return CityLocation + ChunkOffset + GridOffset;
}

float ACityActor::CalculateTurretRadius() const
{
	// Determine if we're using the outer ring of factories
	bool bHasOuterRingFactories = false;
	for (int32 ChunkIndex : UsedChunkIndices)
	{
		if (ChunkIndex >= 6) // Outer ring starts at chunk 6
		{
			bHasOuterRingFactories = true;
			break;
		}
	}
	
	// Base radius: clears inner ring factories (500 radius + factory size + buffer)
	float BaseRadius = 1000.0f; // Increased from 750 to spawn farther away
	
	// Expand if we have outer ring factories (1100 radius + factory size + buffer)
	float OuterRingRadius = 1600.0f; // Increased from 1350 to spawn farther away
	
	return bHasOuterRingFactories ? OuterRingRadius : BaseRadius;
}

void ACityActor::RepositionTurrets()
{
	// Clean up null/destroyed buildings first
	// Check for nullptr before IsValid() to avoid accessing corrupted pointers
	Buildings.RemoveAll([](ABuildingActor* Building) { return Building == nullptr || !IsValid(Building); });
	
	// Get all turrets
	TArray<ATurretBuildingActor*> Turrets;
	for (ABuildingActor* Building : Buildings)
	{
		if (ATurretBuildingActor* Turret = Cast<ATurretBuildingActor>(Building))
		{
			Turrets.Add(Turret);
		}
	}
	
	if (Turrets.Num() == 0) return;
	
	// Calculate ring radius
	float RingRadius = CalculateTurretRadius();
	
	// Position turrets evenly around the ring
	FVector CityLocation = GetActorLocation();
	FVector ToCityFromCenter = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Create tangent coordinate system
	FVector ArbitraryUp = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(ToCityFromCenter, ArbitraryUp)) > 0.9f)
	{
		ArbitraryUp = FVector::ForwardVector;
	}
	
	FVector TangentX = FVector::CrossProduct(ArbitraryUp, ToCityFromCenter).GetSafeNormal();
	FVector TangentY = FVector::CrossProduct(ToCityFromCenter, TangentX).GetSafeNormal();
	
	// Distribute turrets evenly
	float AngleStep = 360.0f / Turrets.Num();
	
	for (int32 i = 0; i < Turrets.Num(); i++)
	{
		float Angle = i * AngleStep;
		float AngleRad = FMath::DegreesToRadians(Angle);
		
		FVector Offset = TangentX * FMath::Cos(AngleRad) * RingRadius +
		                 TangentY * FMath::Sin(AngleRad) * RingRadius;
		
		FVector NewLocation = CityLocation + Offset;
		Turrets[i]->SetActorLocation(NewLocation);
		Turrets[i]->AlignToPlanet();
		Turrets[i]->UpdateColor();
	}
}

void ACityActor::NotifyBuildingDamaged()
{
	// Reset the timer - no buildings in this city can heal for 30 seconds
	TimeSinceAnyBuildingDamaged = 0.0f;
	
	// Calculate city health from all buildings
	float TotalBuildingHealth = 0.0f;
	float TotalBuildingMaxHealth = 0.0f;
	
	for (ABuildingActor* Building : Buildings)
	{
		if (Building && IsValid(Building))
		{
			TotalBuildingHealth += Building->CurrentHealth;
			TotalBuildingMaxHealth += Building->MaxHealth;
		}
	}
	
	// Update city health based on building health
	if (TotalBuildingMaxHealth > 0.0f)
	{
		CurrentHealth = (TotalBuildingHealth / TotalBuildingMaxHealth) * MaxHealth;
	}
	
	// Update health bar to show damage
	UpdateHealthBar();
}

FLinearColor ACityActor::GetTeamColor() const
{
	switch (OwnerTeam)
	{
		case EOwnerTeam::Player:
			return FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
		case EOwnerTeam::AI1:
			return FLinearColor::Red;
		case EOwnerTeam::AI2:
			return FLinearColor::Green;
		case EOwnerTeam::AI3:
			return FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
		case EOwnerTeam::AI4:
			return FLinearColor(0.5f, 0.0f, 1.0f); // Purple
		case EOwnerTeam::AI5:
			return FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
		case EOwnerTeam::AI6:
			return FLinearColor(0.0f, 0.0f, 0.0f); // Black
		case EOwnerTeam::AI7:
			return FLinearColor(0.2f, 0.3f, 0.85f); // Royal blue
		case EOwnerTeam::AI8:
			return FLinearColor(1.0f, 1.0f, 1.0f); // White
		default:
			return FLinearColor::Gray;
	}
}

void ACityActor::DrawTerritoryCircle()
{
	if (!GetWorld())
	{
		return;
	}
	
	// Get GameMode to access TERRITORY_RADIUS constant
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		return;
	}
	
	FVector CityLocation = GetActorLocation();
	FVector CityDirection = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Get team color
	FLinearColor TeamColor = GetTeamColor();
	
	// Draw circle on planet surface
	// The circle is drawn as a series of line segments around the city on the planet's curved surface
	const int32 NumSegments = 64; // Number of line segments to approximate circle
	const float TerritoryRadius = GameMode->TERRITORY_RADIUS;
	
	// Create a tangent basis on the planet surface at this location
	FVector Tangent1 = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
	if (Tangent1.IsNearlyZero())
	{
		Tangent1 = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
	}
	FVector Tangent2 = FVector::CrossProduct(CityDirection, Tangent1).GetSafeNormal();
	
	// Draw circle as connected line segments
	for (int32 i = 0; i < NumSegments; i++)
	{
		float Angle1 = (i * 2.0f * PI) / NumSegments;
		float Angle2 = ((i + 1) * 2.0f * PI) / NumSegments;
		
		// Calculate offset in tangent space
		FVector Offset1 = (Tangent1 * FMath::Cos(Angle1) + Tangent2 * FMath::Sin(Angle1)) * TerritoryRadius;
		FVector Offset2 = (Tangent1 * FMath::Cos(Angle2) + Tangent2 * FMath::Sin(Angle2)) * TerritoryRadius;
		
		// Project points back onto planet surface
		FVector Direction1 = (CityDirection * PlanetRadius + Offset1).GetSafeNormal();
		FVector Direction2 = (CityDirection * PlanetRadius + Offset2).GetSafeNormal();
		
		// Raise circle 350 units above planet surface
		FVector Point1 = PlanetCenter + Direction1 * (PlanetRadius + 350.0f);
		FVector Point2 = PlanetCenter + Direction2 * (PlanetRadius + 350.0f);
		
		// Draw line segment (persistent for one frame, thickness 30)
		DrawDebugLine(GetWorld(), Point1, Point2, TeamColor.ToFColor(true), false, -1.0f, 0, 30.0f);
	}
}

void ACityActor::UpdatePopulation(float DeltaTime)
{
	// Calculate required food (1 green substrate per 1000 population)
	int32 RequiredFood = Population / 1000;
	
	// Calculate surplus or deficit using city's own green substrate
	int32 Difference = GreenSubstrate - RequiredFood;
	
	if (Difference > 0)
	{
		// Surplus: grow population by max 1% per minute
		// Since this is called every 6 seconds (10 times per minute), growth per call = 0.1%
		float GrowthRate = 0.001f * (DeltaTime / 6.0f); // 0.1% per 6 seconds
		int32 MaxGrowth = FMath::RoundToInt(Population * GrowthRate);
		
		// Don't grow more than surplus allows (each surplus supports 1000 people)
		int32 SurplusCapacity = Difference * 1000;
		int32 ActualGrowth = FMath::Min(MaxGrowth, SurplusCapacity);
		
		Population += ActualGrowth;
	}
	else if (Difference < 0)
	{
		// Deficit: shrink population by 5% of deficit per minute
		int32 AbsDeficit = FMath::Abs(Difference);
		
		// Shrinkage per call (since called 10 times per minute, divide by 10)
		int32 ShrinkagePerCall = FMath::RoundToInt((AbsDeficit * 0.05f * 1000.0f) * (DeltaTime / 6.0f));
		
		Population -= ShrinkagePerCall;
		
		// Never go below 0
		Population = FMath::Max(Population, 0);
	}
	
	// Round population to nearest 100
	Population = FMath::RoundToInt(Population / 100.0f) * 100;
}

FLinearColor ACityActor::GetGreenSubstrateColor() const
{
	// Calculate required food (1 green substrate per 1000 population)
	int32 RequiredFood = Population / 1000;
	int32 Difference = GreenSubstrate - RequiredFood;
	
	if (Difference < 0)
	{
		// Deficit - RED
		return FLinearColor(1.0f, 0.0f, 0.0f);
	}
	else if (Difference > 0)
	{
		// Surplus - GREEN
		return FLinearColor(0.0f, 1.0f, 0.0f);
	}
	else
	{
		// Balanced - WHITE
		return FLinearColor(1.0f, 1.0f, 1.0f);
	}
}

