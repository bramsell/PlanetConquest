// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResourceActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "../Vehicles/VehicleActor.h"
#include "../Buildings/MineActor.h"
#include "../../UI/HealthBarWidget.h"
#include "../../UI/InfoUIWidget.h"
#include "Kismet/GameplayStatics.h"
#include "../Cities/CityActor.h"
#include "../../Core/PlanetConquestGameMode.h"
#include "../../Core/PlanetConquestPlayerController.h"
#include "DrawDebugHelpers.h"

AResourceActor::AResourceActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create resource mesh (using cube)
	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(RootComponent);
	
	// Load cube mesh from engine content
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		ResourceMesh->SetStaticMesh(CubeMesh.Object);
	}
	
	// Scale for resource size (1.5x diameter)
	ResourceMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
	
	// Try to load team color material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColorMat(TEXT("/Game/M_TeamColor"));
	if (TeamColorMat.Succeeded())
	{
		ResourceMesh->SetMaterial(0, TeamColorMat.Object);
	}
	
	// Enable collision for selection and obstacle detection (but not physical blocking)
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Query only - no physics blocking!
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ResourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // For selection raycasts
	ResourceMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); // Block for detection sweeps (but QueryOnly means no physical blocking)
	ResourceMesh->SetCollisionObjectType(ECC_WorldStatic); // Act as static obstacle for queries
	// Scale up the collision bounds to make clicking easier (2x larger clickable area)
	ResourceMesh->SetBoundsScale(2.0f);

	// Create selection box (wireframe cube)
	SelectionBox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBox"));
	SelectionBox->SetupAttachment(RootComponent);
	SelectionBox->SetStaticMesh(CubeMesh.Object);
	SelectionBox->SetRelativeScale3D(FVector(2.5f, 2.5f, 2.5f)); // Much larger than 1.5x resource for visibility
	SelectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionBox->SetVisibility(false); // Hidden by default
	SelectionBox->SetRenderCustomDepth(true);
	
	// Try to load yellow selection material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SelectionMat(TEXT("/Game/M_SelectionBox_Yellow"));
	if (SelectionMat.Succeeded())
	{
		SelectionBox->SetMaterial(0, SelectionMat.Object);
	}

	// Create progress bar widget for capture
	ProgressBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ProgressBarWidget"));
	ProgressBarWidget->SetupAttachment(RootComponent);
	ProgressBarWidget->SetWidgetSpace(EWidgetSpace::Screen); // Billboard to camera
	ProgressBarWidget->SetDrawSize(FVector2D(120.0f, 12.0f)); // Progress bar size
	ProgressBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f)); // Above resource
	ProgressBarWidget->SetVisibility(false); // Hidden when not capturing

	// Try to load health bar widget class (we'll reuse it as progress bar)
	static ConstructorHelpers::FClassFinder<UUserWidget> ProgressBarClass(TEXT("/Game/WBP_HealthBar"));
	if (ProgressBarClass.Succeeded())
	{
		ProgressBarWidget->SetWidgetClass(ProgressBarClass.Class);
	}

	// Create resource info widget (shows income per cycle on hover)
	ResourceInfoWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResourceInfoWidget"));
	ResourceInfoWidget->SetupAttachment(RootComponent);
	ResourceInfoWidget->SetWidgetSpace(EWidgetSpace::Screen); // Billboard to camera
	ResourceInfoWidget->SetDrawSize(FVector2D(150.0f, 40.0f)); // Info widget size
	ResourceInfoWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f)); // Above resource
	ResourceInfoWidget->SetVisibility(false); // Hidden by default

	// Try to load info UI widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> InfoUIClass(TEXT("/Game/UI/WBP_InfoUI.WBP_InfoUI_C"));
	if (InfoUIClass.Succeeded())
	{
		ResourceInfoWidget->SetWidgetClass(InfoUIClass.Class);
	}
}

void AResourceActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Auto-align to planet on start
	AlignToPlanet();
	
	// Set initial color based on owner
	UpdateColor();
	
	// Initialize influence
	CurrentInfluence = 0.0f;
	UpdateProgressBar();
	
	// Initialize health tracking timestamps
	LastDepletionTime = GetWorld()->GetTimeSeconds();
	LastRecoveryTime = GetWorld()->GetTimeSeconds();
}

void AResourceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Removed territory circle drawing - mines now visually show ownership via color
	// if (bShowTerritoryCircle && OwnerTeam != EOwnerTeam::Neutral)
	// {
	// 	DrawTerritoryCircle();
	// }

	TimeSinceLastInfluenceChange += DeltaTime;

	if (TimeSinceLastInfluenceChange >= InfluenceInterval)
	{
		TimeSinceLastInfluenceChange = 0.0f;

		// Find all vehicles in the world
		TArray<AActor*> FoundVehicles;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), FoundVehicles);

		// Check if any vehicles are within capture range - count per team
		TMap<EOwnerTeam, int32> VehicleCountPerTeam;
		VehicleCountPerTeam.Add(EOwnerTeam::Neutral, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::Player, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI1, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI2, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI3, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI4, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI5, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI6, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI7, 0);
		VehicleCountPerTeam.Add(EOwnerTeam::AI8, 0);

		for (AActor* Actor : FoundVehicles)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
			if (!Vehicle) continue;

			// Only count vehicles that are explicitly targeting this resource
			if (Vehicle->TargetResource != this) continue;

			float Distance = FVector::Dist(GetActorLocation(), Vehicle->GetActorLocation());
			
			// Log for player vehicles to track capture issues
			if (Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				UE_LOG(LogTemp, Warning, TEXT("[RESOURCE %s] Player vehicle detected: Distance=%.1f, CaptureRange=%.1f, InRange=%s"),
					*GetName(), Distance, CaptureRange, (Distance <= CaptureRange) ? TEXT("YES") : TEXT("NO"));
			}
			
			if (Distance <= CaptureRange)
			{
				VehicleCountPerTeam[Vehicle->OwnerTeam]++;
			}
		}

		// Determine which team is influencing (only ONE team can capture at a time)
		EOwnerTeam NearbyTeam = EOwnerTeam::Neutral;
		int32 MaxVehicles = 0;
		int32 TeamsPresent = 0;

		for (auto& Pair : VehicleCountPerTeam)
		{
			if (Pair.Key != EOwnerTeam::Neutral && Pair.Value > 0)
			{
				TeamsPresent++;
				if (Pair.Value > MaxVehicles)
				{
					MaxVehicles = Pair.Value;
					NearbyTeam = Pair.Key;
				}
			}
		}

		// CAPTURE LOCK SYSTEM: First team to start capturing gets exclusive lock
		// This prevents deadlock and interference from other teams
		if (CapturingVehicle && IsValid(CapturingVehicle))
		{
			// A vehicle has the capture lock - only their team can continue capturing
			NearbyTeam = CapturingVehicle->OwnerTeam;
			// Other teams are blocked from interfering - must find different resources
		}
		// If no lock exists, team with most vehicles present starts capturing
		// NearbyTeam already set to team with MaxVehicles from the loop above

		// If resource is already owned by the nearby team, don't process capture
		if (OwnerTeam == NearbyTeam && NearbyTeam != EOwnerTeam::Neutral)
		{
			// Already owned by this team, reset influence
			CurrentInfluence = 0.0f;
			CapturingTeam = EOwnerTeam::Neutral;
			UpdateProgressBar();
			return;
		}

		// MINE PROTECTION: If resource has an enemy mine, prevent capture
		if (Mine && Mine->OwnerTeam != NearbyTeam && Mine->OwnerTeam != EOwnerTeam::Neutral)
		{
			// Enemy mine is protecting this resource - must destroy mine first!
			CurrentInfluence = 0.0f;
			CapturingTeam = EOwnerTeam::Neutral;
			UpdateProgressBar();
			return;
		}

		// Process influence
		if (NearbyTeam != EOwnerTeam::Neutral)
		{
			// Vehicle(s) capturing
			if (CapturingTeam == NearbyTeam)
			{
				// Same team continuing capture
				// If no vehicle has the lock yet, assign the first one we find
				if (!CapturingVehicle || !IsValid(CapturingVehicle))
				{
					TArray<AActor*> NearbyVehicles;
					UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), NearbyVehicles);
					for (AActor* Actor : NearbyVehicles)
					{
						AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
						float Distance = FVector::Dist(Vehicle->GetActorLocation(), GetActorLocation());
						if (Vehicle && Vehicle->OwnerTeam == NearbyTeam && Distance <= CaptureRange)
						{
							CapturingVehicle = Vehicle;
							//UE_LOG(LogTemp, Log, TEXT("Resource '%s': Team %d vehicle locked capture"), *GetName(), (int32)NearbyTeam);
							break;
						}
					}
				}
				
				CurrentInfluence += InfluenceGainRate;
				if (CurrentInfluence >= MaxInfluence)
				{
					CurrentInfluence = MaxInfluence;
					
					// Clear TargetResource on all nearby vehicles capturing this
					TArray<AActor*> NearbyVehicles;
					UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), NearbyVehicles);
					for (AActor* Actor : NearbyVehicles)
					{
						AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
						if (Vehicle && Vehicle->TargetResource == this && Vehicle->OwnerTeam == CapturingTeam)
						{
							// Track cluster before clearing target
							if (ClusterID >= 0)
							{
								Vehicle->ActiveClusterID = ClusterID;
								if (CapturingTeam == EOwnerTeam::Player)
								{
									const TCHAR* TypeName = (ResourceType == EResourceType::BlackSubstrate) ? TEXT("BLACK") : TEXT("ORANGE");
									UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Captured CLUSTER resource %s (ClusterID=%d, Type=%s)"), 
										*GetName(), ClusterID, TypeName);
								}
							}
							else
							{
								Vehicle->ActiveClusterID = -1;
								if (CapturingTeam == EOwnerTeam::Player)
								{
									UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Captured TERRITORY resource %s"), *GetName());
								}
							}
							
							// Clear the target so vehicle can find next resource autonomously
							Vehicle->TargetResource = nullptr;
							Vehicle->bHasTarget = false; // Also clear movement target flag
							
							if (CapturingTeam == EOwnerTeam::Player)
							{
								UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Cleared TargetResource and bHasTarget, autonomous mode: %s"),
									Vehicle->bPlayerAutonomousMode ? TEXT("ENABLED") : TEXT("DISABLED"));
							}
						}
					}
					
					CaptureForTeam(CapturingTeam);
					// Clear capture lock after successful capture
					CapturingVehicle = nullptr;
				}
			}
			else
			{
				// Different team trying to capture - only allowed if no capture lock exists
				if (!CapturingVehicle || !IsValid(CapturingVehicle))
				{
					// No capture lock, start capturing
					CapturingTeam = NearbyTeam;
					CurrentInfluence = InfluenceGainRate;
				}
				else
				{
					// Another team has the capture lock - cannot interfere
					// Must destroy the capturing vehicle first
				}
			}
		}
		else
		{
			// No vehicles nearby or contested - decay
			CurrentInfluence -= InfluenceDecayRate;
			if (CurrentInfluence <= 0.0f)
			{
				CurrentInfluence = 0.0f;
				CapturingTeam = EOwnerTeam::Neutral;
				CapturingVehicle = nullptr; // Clear capture lock when influence reaches 0
			}
			
			// Also clear lock if capturing vehicle is no longer in range
			if (CapturingVehicle && IsValid(CapturingVehicle))
			{
				float Distance = FVector::Dist(CapturingVehicle->GetActorLocation(), GetActorLocation());
				if (Distance > CaptureRange)
				{
					UE_LOG(LogTemp, Log, TEXT("Resource '%s': Capturing vehicle left range - clearing lock"), *GetName());
					CapturingVehicle = nullptr;
				}
			}
		}

		UpdateProgressBar();
	}
}

void AResourceActor::AlignToPlanet()
{
	// Get direction from planet center to this resource
	FVector ResourceLocation = GetActorLocation();
	FVector Direction = (ResourceLocation - PlanetCenter).GetSafeNormal();
	
	// Calculate distance from planet center
	float CurrentDistance = FVector::Dist(ResourceLocation, PlanetCenter);
	
	// If resource is at planet center, use default direction
	if (CurrentDistance < 10.0f)
	{
		Direction = FVector(0.0f, 0.0f, 1.0f); // Default to top of planet
	}
	
	// Orient resource so its up (Z-axis) points away from planet center
	// NOTE: We only set rotation here, NOT location
	// Location is already set by SpawnResourcesAroundPlanet with proper terrain height + offset
	FRotator LookRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
	SetActorRotation(LookRotation);
	
	// UE_LOG(LogTemp, Log, TEXT("Resource aligned to planet at location: %s"), *GetActorLocation().ToString());
}

void AResourceActor::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	
	if (SelectionBox)
	{
		SelectionBox->SetVisibility(bSelected);
		UE_LOG(LogTemp, Warning, TEXT("Resource %s selection box set to: %s (Box valid: %s)"), 
			*GetName(), 
			bSelected ? TEXT("VISIBLE") : TEXT("HIDDEN"),
			SelectionBox ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Resource %s has no SelectionBox component!"), *GetName());
	}
}

void AResourceActor::SetHovered(bool bHovered)
{
	// Show yellow box when hovering (if not already selected)
	if (!bIsSelected)
	{
		if (SelectionBox)
		{
			SelectionBox->SetVisibility(bHovered);
		}
	}
}

void AResourceActor::CaptureForTeam(EOwnerTeam NewOwner)
{
	EOwnerTeam PreviousOwner = OwnerTeam; // Store previous owner for relationship updates
	
	OwnerTeam = NewOwner;
	CurrentInfluence = 0.0f; // Reset influence after capture
	CapturingTeam = EOwnerTeam::Neutral;
	UpdateColor();
	UpdateProgressBar(); // Hide progress bar after capture
	
	// Discovery system: if player captured this, mark resource type as discovered
	if (NewOwner == EOwnerTeam::Player)
	{
		if (APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
		{
			PlayerController->DiscoverResourceType(ResourceType);
		}
	}
	
	// Spawn mine/farm building when captured (destroy old one if recaptured)
	if (NewOwner != EOwnerTeam::Neutral)
	{
		// Destroy existing mine if this resource is being recaptured
		if (Mine)
		{
			Mine->Destroy();
			Mine = nullptr;
		}
		
		// Calculate mine spawn location (offset from resource on the planet surface)
		FVector ResourceLocation = GetActorLocation();
		FVector ToPlanetCenter = (PlanetCenter - ResourceLocation).GetSafeNormal();
		FVector UpVector = -ToPlanetCenter; // Points away from planet center
		
		// Create a tangent vector for offset (perpendicular to up vector)
		FVector Tangent = FVector::CrossProduct(UpVector, FVector::UpVector).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::CrossProduct(UpVector, FVector::RightVector).GetSafeNormal();
		}
		
		// Offset the mine by 150 units from the resource
		FVector OffsetDirection = Tangent;
		FVector MineSpawnLocation = ResourceLocation + OffsetDirection * 150.0f;
		
		// Project back onto planet surface
		FVector DirectionFromCenter = (MineSpawnLocation - PlanetCenter).GetSafeNormal();
		MineSpawnLocation = PlanetCenter + DirectionFromCenter * PlanetRadius;
		
		// Spawn the mine (rotation handled by AlignToPlanet + mesh's relative rotation)
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		Mine = GetWorld()->SpawnActor<AMineActor>(AMineActor::StaticClass(), MineSpawnLocation, FRotator::ZeroRotator, SpawnParams);
		
		if (Mine)
		{
			Mine->PlanetCenter = PlanetCenter;
			Mine->PlanetRadius = PlanetRadius;
			Mine->OwnerTeam = NewOwner;
			Mine->TargetResource = this;
			Mine->AlignToPlanet();
			Mine->UpdateColor();
		}
	}
	else if (Mine && IsValid(Mine)) // If reverting to neutral, remove the mine (if it still exists)
	{
		Mine->Destroy();
		Mine = nullptr;
	}
	
	// Update relationships based on resource capture
	if (UWorld* World = GetWorld())
	{
		if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode()))
		{
			// If this was an enemy resource, apply direct penalty
			if (PreviousOwner != EOwnerTeam::Neutral && PreviousOwner != NewOwner)
			{
				GameMode->OnEnemyResourceCaptured(NewOwner, PreviousOwner);
			}
			// If this was a neutral resource, check if we violated anyone's territory
			else if (PreviousOwner == EOwnerTeam::Neutral)
			{
				// Get all cities and check if this resource is in their territory
				TArray<AActor*> FoundCities;
				UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), FoundCities);
				
				FVector ResourceLocation = GetActorLocation();
				
				for (AActor* CityActor : FoundCities)
				{
					if (ACityActor* City = Cast<ACityActor>(CityActor))
					{
						// Skip if it's the capturing team's city
						if (City->OwnerTeam == NewOwner)
						{
							continue;
						}
						
						// Check if resource is within this city's territory
						float Distance = FVector::Dist(ResourceLocation, City->GetActorLocation());
						if (Distance <= APlanetConquestGameMode::TERRITORY_RADIUS)
						{
							// Territory violation!
							GameMode->OnNeutralResourceCapturedInTerritory(NewOwner, City->OwnerTeam);
							break; // Only penalize once per capture (multiple territories might overlap)
						}
					}
				}
			}
		}
	}
}

void AResourceActor::SetupMesh()
{
	if (ResourceMesh)
	{
		// Load custom meshes for resources
		if (ResourceType == EResourceType::BlackSubstrate)
		{
			UStaticMesh* CrystalMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/BlenderAssets/BlackSubstrate/BlackSubstrate_2.BlackSubstrate_2"));
			if (CrystalMesh)
			{
				ResourceMesh->SetStaticMesh(CrystalMesh);
				ResourceMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f) * SizeScale);
				
				// Restore collision settings after mesh change
				ResourceMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				ResourceMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
				ResourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
				ResourceMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				ResourceMesh->SetCollisionObjectType(ECC_WorldStatic);
				ResourceMesh->SetBoundsScale(2.0f);
				
				// Update selection box scale to match
				if (SelectionBox)
				{
					SelectionBox->SetRelativeScale3D(FVector(2.5f * SizeScale, 2.5f * SizeScale, 2.5f * SizeScale));
				}
				
				// Load and apply the black substrate material
				UMaterialInterface* BlackMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BlackSubstrate.M_BlackSubstrate"));
				if (BlackMaterial)
				{
					ResourceMesh->SetMaterial(0, BlackMaterial);
				}
			}
		}
	}
}

void AResourceActor::UpdateColor()
{
	if (ResourceMesh)
	{
		// Create dynamic material instance from the current material
		UMaterialInstanceDynamic* DynMaterial = ResourceMesh->CreateDynamicMaterialInstance(0);
		if (!DynMaterial)
		{
			UE_LOG(LogTemp, Warning, TEXT("Creating new dynamic material for resource"));
		}
		
		FLinearColor Color;
		switch (OwnerTeam)
		{
		case EOwnerTeam::Player:
			Color = FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
			break;
		case EOwnerTeam::AI1:
			Color = FLinearColor(1.0f, 0.0f, 0.0f); // Red
			break;
		case EOwnerTeam::AI2:
			Color = FLinearColor(0.0f, 1.0f, 0.0f); // Green
			break;
		case EOwnerTeam::AI3:
			Color = FLinearColor(1.0f, 0.5f, 0.0f); // Orange
			break;
		case EOwnerTeam::AI4:
			Color = FLinearColor(0.5f, 0.0f, 1.0f); // Purple
			break;
		case EOwnerTeam::AI5:
			Color = FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
			break;
		case EOwnerTeam::AI6:
			Color = FLinearColor(0.0f, 0.0f, 0.0f); // Black
			break;
		default: // Neutral
			Color = FLinearColor(1.0f, 0.35f, 0.0f); // Reddish-orange (Orange Substrate)
			break;
		}
		
		// Set color based on resource type (ownership shown by territory circle)
		if (ResourceType == EResourceType::BlackSubstrate)
		{
			Color = FLinearColor(0.15f, 0.15f, 0.15f); // Dark gray (Black Substrate)
		}
		else if (ResourceType == EResourceType::GreenSubstrate)
		{
			Color = FLinearColor(0.2f, 0.8f, 0.2f); // Bright green (Green Substrate - food)
		}
		else
		{
			Color = FLinearColor(1.0f, 0.35f, 0.0f); // Reddish-orange (Orange Substrate)
		}
		
		// Try multiple common parameter names
		if (DynMaterial)
		{
			DynMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
			DynMaterial->SetVectorParameterValue(FName("Color"), Color);
			DynMaterial->SetVectorParameterValue(FName("Tint"), Color);
		}
		
		// Also set on all materials
		ResourceMesh->SetVectorParameterValueOnMaterials(FName("VertexColor"), FVector(Color.R, Color.G, Color.B));
	}
}

void AResourceActor::RandomizeSize()
{
	// Standardized income tiers: 10, 25, 40, 50 per cycle
	// 10 and 25 are equally most common
	float Roll = FMath::FRandRange(0.0f, 100.0f);
	
	if (Roll < 40.0f) // 40% chance - Tiny (10/cycle)
	{
		IncomePerInterval = 10;
		SizeScale = 0.7f;
	}
	else if (Roll < 80.0f) // 40% chance - Small (25/cycle)
	{
		IncomePerInterval = 25;
		SizeScale = 0.9f;
	}
	else if (Roll < 95.0f) // 15% chance - Medium (40/cycle)
	{
		IncomePerInterval = 40;
		SizeScale = 1.1f;
	}
	else // 5% chance - Large (50/cycle)
	{
		IncomePerInterval = 50;
		SizeScale = 1.3f;
	}
	
	// Apply scale to the mesh (multiply by base 1.5x scale)
	if (ResourceMesh)
	{
		ResourceMesh->SetRelativeScale3D(FVector(1.5f * SizeScale, 1.5f * SizeScale, 1.5f * SizeScale));
	}
	
	// Also scale the selection box proportionally
	if (SelectionBox)
	{
		SelectionBox->SetRelativeScale3D(FVector(2.5f * SizeScale, 2.5f * SizeScale, 2.5f * SizeScale));
	}
}

int32 AResourceActor::GetEffectiveIncome() const
{
	// Return income multiplied by resource health (0.0 to 1.0)
	return FMath::RoundToInt(IncomePerInterval * ResourceHealth);
}

void AResourceActor::UpdateProgressBar()
{
	if (!ProgressBarWidget) return;

	// Show progress bar when being captured (influence > 0)
	bool bShouldShowProgressBar = CurrentInfluence > 0.0f;
	
	// Check if resource is occluded by planet
	if (bShouldShowProgressBar)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			FVector ResourceLocation = GetActorLocation();
			
			FHitResult VisibilityHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			
			bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
				VisibilityHit,
				CameraLocation,
				ResourceLocation,
				ECC_Visibility,
				QueryParams
			);
			
			// Hide if resource is blocked by planet
			float DistanceToResource = FVector::Dist(CameraLocation, ResourceLocation);
			float DistanceToHit = VisibilityHit.Distance;
			if (bHitSomething && (DistanceToHit < DistanceToResource - 100.0f))
			{
				bShouldShowProgressBar = false;
			}
		}
	}
	
	ProgressBarWidget->SetVisibility(bShouldShowProgressBar);

	// Update the widget's progress percent
	if (UHealthBarWidget* ProgressWidget = Cast<UHealthBarWidget>(ProgressBarWidget->GetUserWidgetObject()))
	{
		float ProgressPercent = CurrentInfluence / MaxInfluence;
		ProgressWidget->SetHealthPercent(ProgressPercent);
	}
}

void AResourceActor::ShowResourceInfo(bool bShow)
{
	if (!ResourceInfoWidget)
	{
		return;
	}

	// Only show if not occluded by planet
	if (bShow)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			FVector ResourceLocation = GetActorLocation();
			
			FHitResult VisibilityHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			
			bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
				VisibilityHit,
				CameraLocation,
				ResourceLocation,
				ECC_Visibility,
				QueryParams
			);
			
			// Hide if resource is blocked by planet
			float DistanceToResource = FVector::Dist(CameraLocation, ResourceLocation);
			float DistanceToHit = VisibilityHit.Distance;
			if (bHitSomething && (DistanceToHit < DistanceToResource - 100.0f))
			{
				bShow = false;
			}
		}
	}

	ResourceInfoWidget->SetVisibility(bShow);
	
	// Update values when showing
	if (bShow)
	{
		UpdateResourceInfo();
	}
}

void AResourceActor::UpdateResourceInfo()
{
	if (!ResourceInfoWidget) return;

	// Update the widget's income values
	if (UInfoUIWidget* InfoWidget = Cast<UInfoUIWidget>(ResourceInfoWidget->GetUserWidgetObject()))
	{
		// Check if this resource type has been discovered
		APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
		bool bIsDiscovered = PlayerController && PlayerController->IsResourceTypeDiscovered(ResourceType);
		
		// Line 1: Resource type name
		FText Line1;
		if (!bIsDiscovered)
		{
			Line1 = FText::FromString("Unknown Resource");
		}
		else
		{
			switch (ResourceType)
			{
				case EResourceType::OrangeSubstrate:
					Line1 = FText::FromString("Orange Substrate");
					break;
				case EResourceType::BlackSubstrate:
					Line1 = FText::FromString("Black Substrate");
					break;
				case EResourceType::GreenSubstrate:
					Line1 = FText::FromString("Green Substrate");
					break;
				default:
					Line1 = FText::FromString("Unknown Resource");
					break;
			}
		}
		
		// Line 2: Ownership status
		FText Line2;
		if (OwnerTeam == EOwnerTeam::Neutral)
		{
			Line2 = FText::FromString("Unclaimed");
		}
		else
		{
			FString TeamName;
			switch (OwnerTeam)
			{
				case EOwnerTeam::Player:
					TeamName = "Player";
					break;
				case EOwnerTeam::AI1:
					TeamName = "Team 1";
					break;
				case EOwnerTeam::AI2:
					TeamName = "Team 2";
					break;
				case EOwnerTeam::AI3:
					TeamName = "Team 3";
					break;
				case EOwnerTeam::AI4:
					TeamName = "Team 4";
					break;
				case EOwnerTeam::AI5:
					TeamName = "Team 5";
					break;
				case EOwnerTeam::AI6:
					TeamName = "Team 6";
					break;
				case EOwnerTeam::AI7:
					TeamName = "Team 7";
					break;
				default:
					TeamName = "Unknown";
					break;
			}
			Line2 = FText::FromString(FString::Printf(TEXT("Mined by %s"), *TeamName));
		}
		
		// Line 3: Income stats
		FText Line3;
		int32 CurrentIncome = GetEffectiveIncome();
		int32 MaxIncome = IncomePerInterval;
		
		// Green substrate has different display logic (doesn't deplete, always at max)
		bool bIsGreenSubstrate = (ResourceType == EResourceType::GreenSubstrate);
		if (bIsGreenSubstrate)
		{
		Line3 = FText::FromString(FString::Printf(TEXT("%d/cycle"), CurrentIncome));
	}
	else
	{
		Line3 = FText::FromString(FString::Printf(TEXT("%d/cycle | Max: %d/cycle"), CurrentIncome, MaxIncome));	}
	
	InfoWidget->SetInfoDisplay(Line1, Line2, Line3);
	}}

void AResourceActor::DrawTerritoryCircle()
{
	if (!GetWorld())
	{
		return;
	}
	
	FVector ResourceLocation = GetActorLocation();
	FVector ResourceDirection = (ResourceLocation - PlanetCenter).GetSafeNormal();
	
	// Get team color
	FLinearColor TeamColor = GetTeamColor();
	
	// Draw thick circle outline on planet surface
	const int32 NumSegments = 64; // Number of segments for smooth circle
	const float CircleRadius = TerritoryCircleRadius;
	const float Thickness = 15.0f; // Thick line
	
	// Create a tangent basis on the planet surface at this location
	FVector Tangent1 = FVector::CrossProduct(ResourceDirection, FVector::UpVector).GetSafeNormal();
	if (Tangent1.IsNearlyZero())
	{
		Tangent1 = FVector::CrossProduct(ResourceDirection, FVector::RightVector).GetSafeNormal();
	}
	FVector Tangent2 = FVector::CrossProduct(ResourceDirection, Tangent1).GetSafeNormal();
	
	// Draw circle as connected line segments
	for (int32 i = 0; i < NumSegments; i++)
	{
		float Angle1 = (i * 2.0f * PI) / NumSegments;
		float Angle2 = ((i + 1) * 2.0f * PI) / NumSegments;
		
		// Calculate offset in tangent space
		FVector Offset1 = (Tangent1 * FMath::Cos(Angle1) + Tangent2 * FMath::Sin(Angle1)) * CircleRadius;
		FVector Offset2 = (Tangent1 * FMath::Cos(Angle2) + Tangent2 * FMath::Sin(Angle2)) * CircleRadius;
		
		// Project points back onto planet surface
		FVector Direction1 = (ResourceDirection * PlanetRadius + Offset1).GetSafeNormal();
		FVector Direction2 = (ResourceDirection * PlanetRadius + Offset2).GetSafeNormal();
		
		FVector Point1 = PlanetCenter + Direction1 * PlanetRadius;
		FVector Point2 = PlanetCenter + Direction2 * PlanetRadius;
		
		// Draw thick line segment
		DrawDebugLine(GetWorld(), Point1, Point2, TeamColor.ToFColor(true), false, -1.0f, 0, Thickness);
	}
}

FLinearColor AResourceActor::GetTeamColor() const
{
	switch (OwnerTeam)
	{
	case EOwnerTeam::Player:
		return FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
	case EOwnerTeam::AI1:
		return FLinearColor::Red; // Red
	case EOwnerTeam::AI2:
		return FLinearColor::Green; // Green
	case EOwnerTeam::AI3:
		return FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
	case EOwnerTeam::AI4:
		return FLinearColor(0.5f, 0.0f, 1.0f); // Purple
	case EOwnerTeam::AI5:
	return FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
case EOwnerTeam::AI6:
	return FLinearColor(0.8f, 0.8f, 0.8f); // Light gray
case EOwnerTeam::AI7:
	return FLinearColor(0.2f, 0.3f, 0.85f); // Royal blue
	case EOwnerTeam::AI8:
		return FLinearColor(1.0f, 1.0f, 1.0f); // White
	default:
		return FLinearColor::Gray; // Gray (neutral)
	}
}
