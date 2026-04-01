// Copyright Benjamin Ramsell. All Rights Reserved.

#include "BuildingActor.h"
#include "../../World/PlanetActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "../Cities/CityActor.h"
#include "../../UI/HealthBarWidget.h"
#include "../../UI/InfoUIWidget.h"
#include "../../Core/PlanetConquestGameMode.h"
#include "MineActor.h"

ABuildingActor::ABuildingActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create collision sphere as root
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(100.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // Don't block vehicles - pathfinding handles avoidance
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	CollisionSphere->SetCollisionObjectType(ECC_WorldStatic);
	CollisionSphere->SetHiddenInGame(true);

	// Create building mesh
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	BuildingMesh->SetupAttachment(RootComponent);
	
	// Load cube mesh as default (will be customized by subclasses)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		BuildingMesh->SetStaticMesh(CubeMesh.Object);
	}
	
	// Load team color material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColorMat(TEXT("/Game/M_TeamColor"));
	if (TeamColorMat.Succeeded())
	{
		BuildingMesh->SetMaterial(0, TeamColorMat.Object);
	}
	
	// Enable collision on mesh for mouse hover detection (entire building should be hoverable)
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BuildingMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BuildingMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // For mouse cursor traces
	BuildingMesh->SetCollisionObjectType(ECC_WorldStatic);

	// Create selection box (wireframe cube)
	SelectionBox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBox"));
	SelectionBox->SetupAttachment(RootComponent);
	SelectionBox->SetStaticMesh(CubeMesh.Object);
	SelectionBox->SetRelativeScale3D(FVector(1.1f, 1.1f, 1.1f)); // Slightly larger than building
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
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawSize(FVector2D(100.0f, 10.0f));
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	HealthBarWidget->SetVisibility(false);

	// Load health bar widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> HealthBarClass(TEXT("/Game/WBP_HealthBar"));
	if (HealthBarClass.Succeeded())
	{
		HealthBarWidget->SetWidgetClass(HealthBarClass.Class);
	}

	// Create info widget (shows on hover in city editor)
	InfoWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InfoWidget"));
	InfoWidget->SetupAttachment(RootComponent);
	InfoWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InfoWidget->SetDrawSize(FVector2D(150.0f, 40.0f));
	InfoWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f)); // Above building
	InfoWidget->SetVisibility(false); // Hidden by default

	// Load info UI widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> InfoUIClass(TEXT("/Game/UI/WBP_InfoUI.WBP_InfoUI_C"));
	if (InfoUIClass.Succeeded())
	{
		InfoWidget->SetWidgetClass(InfoUIClass.Class);
	}
}

void ABuildingActor::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHealth = MaxHealth;
	// Note: AlignToPlanet() is now called by parent (CityActor) during spawning after setting correct PlanetRadius
	// Don't auto-align here as it would use the default PlanetRadius value
	UpdateColor();
}

void ABuildingActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Remove this building from parent city's Buildings array when destroyed
	if (ParentCity)
	{
		ParentCity->Buildings.Remove(this);
		
		// If this was a turret, reposition remaining turrets
		if (BuildingType == EBuildingType::Turret)
		{
			ParentCity->RepositionTurrets();
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

void ABuildingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Track time since last damaged
	TimeSinceLastDamaged += DeltaTime;
	
	// Healing system - only heal if city hasn't been attacked recently
	if (CurrentHealth < MaxHealth && ParentCity)
	{
		// Check if city has been damage-free long enough
		if (ParentCity->TimeSinceAnyBuildingDamaged >= HealingStartDelay)
		{
			TimeSinceLastHeal += DeltaTime;
			
			if (TimeSinceLastHeal >= HealingInterval)
			{
				TimeSinceLastHeal = 0.0f;
				CurrentHealth = FMath::Min(CurrentHealth + HealingAmount, MaxHealth);
				UpdateHealthBar();
				
				// Hide health bar when fully healed
				if (CurrentHealth >= MaxHealth && HealthBarWidget)
				{
					HealthBarWidget->SetVisibility(false);
				}
			}
		}
	}
}

void ABuildingActor::AlignToPlanet()
{
	if (PlanetRadius > 0.0f)
	{
		FVector CurrentLocation = GetActorLocation();
		FVector DirectionFromCenter = (CurrentLocation - PlanetCenter).GetSafeNormal();

		// PlanetRadius is already set to the terrain-surface radius by PlanetActor at spawn time
		// (ActualTerrainRadius = PlanetRadius * (1 + TerrainHeight)), so just add the stand-on offset.
		FVector SurfacePosition = PlanetCenter + DirectionFromCenter * (PlanetRadius + 50.0f);
		SetActorLocation(SurfacePosition);
		
		// Orient building to stand upright (Z-axis pointing away from planet)
		FRotator SurfaceRotation = FRotationMatrix::MakeFromZ(DirectionFromCenter).Rotator();
		SetActorRotation(SurfaceRotation);
	}
}

void ABuildingActor::UpdateColor()
{
	if (BuildingMesh)
	{
		UMaterialInstanceDynamic* DynMaterial = BuildingMesh->CreateDynamicMaterialInstance(0);
		if (DynMaterial)
		{
			FLinearColor TeamColor;
			switch (OwnerTeam)
			{
				case EOwnerTeam::Player:
					TeamColor = FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
					break;
				case EOwnerTeam::AI1:
					TeamColor = FLinearColor::Red;
					break;
				case EOwnerTeam::AI2:
					TeamColor = FLinearColor::Green;
					break;
				case EOwnerTeam::AI3:
					TeamColor = FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
					break;
				case EOwnerTeam::AI4:
					TeamColor = FLinearColor(0.5f, 0.0f, 1.0f); // Purple
					break;
				case EOwnerTeam::AI5:
				TeamColor = FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
					break;
				case EOwnerTeam::AI6:
				TeamColor = FLinearColor(0.0f, 0.0f, 0.0f); // Black
					break;
				case EOwnerTeam::AI7:
				TeamColor = FLinearColor(0.2f, 0.3f, 0.85f); // Royal blue
					break;
				case EOwnerTeam::AI8:
					TeamColor = FLinearColor(1.0f, 1.0f, 1.0f); // White
					break;
				case EOwnerTeam::AI9:
					TeamColor = FLinearColor(1.0f, 0.0f, 0.5f); // Hot pink
					break;
				case EOwnerTeam::AI10:
					TeamColor = FLinearColor(1.0f, 0.55f, 0.0f); // Orange
					break;
				case EOwnerTeam::AI11:
					TeamColor = FLinearColor(0.0f, 0.7f, 0.5f); // Teal
					break;
				case EOwnerTeam::AI12:
					TeamColor = FLinearColor(0.5f, 1.0f, 0.0f); // Lime
					break;
				case EOwnerTeam::AI13:
					TeamColor = FLinearColor(1.0f, 0.0f, 1.0f); // Magenta
					break;
				case EOwnerTeam::AI14:
					TeamColor = FLinearColor(0.55f, 0.27f, 0.0f); // Bronze
					break;
				case EOwnerTeam::AI15:
					TeamColor = FLinearColor(0.05f, 0.1f, 0.5f); // Navy
					break;
				case EOwnerTeam::AI16:
					TeamColor = FLinearColor(1.0f, 0.75f, 0.0f); // Gold
					break;
				case EOwnerTeam::AI17:
					TeamColor = FLinearColor(0.7f, 0.7f, 0.7f); // Silver
					break;
				case EOwnerTeam::AI18:
					TeamColor = FLinearColor(0.55f, 0.0f, 0.05f); // Crimson
					break;
				case EOwnerTeam::AI19:
					TeamColor = FLinearColor(0.5f, 1.0f, 0.75f); // Mint
					break;
				default:
					TeamColor = FLinearColor::Gray;
					break;
			}
			
			DynMaterial->SetVectorParameterValue(FName("TeamColor"), TeamColor);
			DynMaterial->SetVectorParameterValue(FName("BaseColor"), TeamColor);
		}
	}
}

FLinearColor ABuildingActor::GetTeamColor() const
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
		case EOwnerTeam::AI9:
			return FLinearColor(1.0f, 0.0f, 0.5f); // Hot pink
		case EOwnerTeam::AI10:
			return FLinearColor(1.0f, 0.55f, 0.0f); // Orange
		case EOwnerTeam::AI11:
			return FLinearColor(0.0f, 0.7f, 0.5f); // Teal
		case EOwnerTeam::AI12:
			return FLinearColor(0.5f, 1.0f, 0.0f); // Lime
		case EOwnerTeam::AI13:
			return FLinearColor(1.0f, 0.0f, 1.0f); // Magenta
		case EOwnerTeam::AI14:
			return FLinearColor(0.55f, 0.27f, 0.0f); // Bronze
		case EOwnerTeam::AI15:
			return FLinearColor(0.05f, 0.1f, 0.5f); // Navy
		case EOwnerTeam::AI16:
			return FLinearColor(1.0f, 0.75f, 0.0f); // Gold
		case EOwnerTeam::AI17:
			return FLinearColor(0.7f, 0.7f, 0.7f); // Silver
		case EOwnerTeam::AI18:
			return FLinearColor(0.55f, 0.0f, 0.05f); // Crimson
		case EOwnerTeam::AI19:
			return FLinearColor(0.5f, 1.0f, 0.75f); // Mint
		default:
			return FLinearColor::Gray;
	}
}

void ABuildingActor::ApplyDamage(float DamageAmount, EOwnerTeam AttackerTeam, AActor* AttackingActor)
{
	// Track last damaging team and actor
	LastDamagingTeam = AttackerTeam;
	LastDamagingActor = AttackingActor;
	TimeSinceLastDamaged = 0.0f; // Reset damage timer
	
	// Debug log for mine damage
	if (Cast<AMineActor>(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("MINE DAMAGE: Mine owned by Team %d took %.1f damage from Team %d, Attacker=%s"),
			(int32)OwnerTeam, DamageAmount, (int32)AttackerTeam, AttackingActor ? *AttackingActor->GetName() : TEXT("nullptr"));
	}
	
	// Calculate damage percentage for relationship updates
	float PreviousHealth = CurrentHealth;
	
	CurrentHealth -= DamageAmount;
	
	// Notify parent city that a building was damaged (stops all building healing)
	if (ParentCity)
	{
		ParentCity->NotifyBuildingDamaged();
	}
	
	// Update relationships based on damage
	if (UWorld* World = GetWorld())
	{
		if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode()))
		{
			// Calculate damage as percentage of max health
			float DamagePercent = (DamageAmount / MaxHealth) * 100.0f;
			
			// Call relationship update (buildings are not capitals)
			FString BuildingTypeName = GetClass()->GetName();
			GameMode->OnBuildingDamaged(AttackerTeam, OwnerTeam, DamagePercent, false, BuildingTypeName);
			
			// On first attack, trigger enemy-of-enemy bonuses
			if (!bHasBeenAttacked && AttackerTeam != EOwnerTeam::Neutral)
			{
				bHasBeenAttacked = true;
				GameMode->OnTeamAttackedTeam(AttackerTeam, OwnerTeam);
			}
		}
	}
	
	if (CurrentHealth <= 0.0f)
	{
		CurrentHealth = 0.0f;
		
		// Special handling for capital buildings - capture instead of destroy
		if (BuildingType == EBuildingType::Capital && ParentCity && AttackerTeam != EOwnerTeam::Neutral)
		{
			UE_LOG(LogTemp, Warning, TEXT("Capital building destroyed - transferring city to team %d"), (int32)AttackerTeam);
			
			// Transfer city ownership (this also resets city health, updates colors, etc.)
			ParentCity->FlipOwnership(AttackerTeam);
			
			// Restore capital building health and update ownership
			CurrentHealth = MaxHealth;
			OwnerTeam = AttackerTeam;
			UpdateColor(); // Update building color to match new owner
			
			// Hide health bar since we're at full health
			if (HealthBarWidget)
			{
				HealthBarWidget->SetVisibility(false);
			}
		}
		else
		{
			// Normal buildings get destroyed
			Destroy();
		}
	}
	else
	{
		// Show health bar when damaged
		if (HealthBarWidget)
		{
			HealthBarWidget->SetVisibility(true);
			UpdateHealthBar();
		}
	}
}

void ABuildingActor::UpdateHealthBar()
{
	if (HealthBarWidget && HealthBarWidget->GetUserWidgetObject())
	{
		// Check if building is occluded by planet
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			FVector BuildingLocation = GetActorLocation();
			
			FHitResult VisibilityHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			
			bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
				VisibilityHit,
				CameraLocation,
				BuildingLocation,
				ECC_Visibility,
				QueryParams
			);
			
			// Hide if building is blocked by planet
			float DistanceToBuilding = FVector::Dist(CameraLocation, BuildingLocation);
			float DistanceToHit = VisibilityHit.Distance;
			if (bHitSomething && (DistanceToHit < DistanceToBuilding - 100.0f))
			{
				HealthBarWidget->SetVisibility(false);
				return;
			}
		}
		
		UHealthBarWidget* HealthBar = Cast<UHealthBarWidget>(HealthBarWidget->GetUserWidgetObject());
		if (HealthBar)
		{
			float HealthPercent = CurrentHealth / MaxHealth;
			HealthBar->SetHealthPercent(HealthPercent);
		}
	}
}

void ABuildingActor::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	
	if (SelectionBox)
	{
		SelectionBox->SetVisibility(bSelected);
	}
}

void ABuildingActor::SetHovered(bool bHovered)
{
	// Only show InfoWidget on hover (selection box remains controlled by SetSelected)
	if (InfoWidget)
	{
		InfoWidget->SetVisibility(bHovered);
		if (bHovered)
		{
			UpdateInfoDisplay();
		}
	}
}

void ABuildingActor::UpdateInfoDisplay()
{
	// Base implementation - derived classes override this
	if (!InfoWidget) return;
	
	UInfoUIWidget* InfoUIWidget = Cast<UInfoUIWidget>(InfoWidget->GetUserWidgetObject());
	if (!InfoUIWidget) return;
	
	// Default: show "Building" + team name
	FString TeamName;
	switch (OwnerTeam)
	{
		case EOwnerTeam::Player: TeamName = "Player"; break;
		case EOwnerTeam::AI1: TeamName = "Team 1"; break;
		case EOwnerTeam::AI2: TeamName = "Team 2"; break;
		case EOwnerTeam::AI3: TeamName = "Team 3"; break;
		case EOwnerTeam::AI4: TeamName = "Team 4"; break;
		case EOwnerTeam::AI5: TeamName = "Team 5"; break;
		case EOwnerTeam::AI6: TeamName = "Team 6"; break;
		case EOwnerTeam::AI7: TeamName = "Team 7"; break;
		default: TeamName = "Neutral"; break;
	}
	
	InfoUIWidget->SetInfoDisplay(
		FText::FromString("Building"),
		FText::FromString(TeamName),
		FText::FromString("")
	);
}
