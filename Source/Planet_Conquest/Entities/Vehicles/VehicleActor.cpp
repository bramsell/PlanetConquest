// Copyright Benjamin Ramsell. All Rights Reserved.

#include "VehicleActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "../Projectiles/ProjectileActor.h"
#include "../Cities/CityActor.h"
#include "../Buildings/BuildingActor.h"
#include "../Buildings/CapitalBuildingActor.h"
#include "../Buildings/MineActor.h"
#include "../Buildings/TurretBuildingActor.h"
#include "../Resources/ResourceActor.h"
#include "../Kaiju/KaijuActor.h"
#include "../../UI/HealthBarWidget.h"
#include "../../UI/InfoUIWidget.h"
#include "../../Core/AITeamController.h"
#include "../../Core/PlanetConquestGameMode.h"
#include "../../Core/PlanetConquestPlayerController.h"
#include "../../World/PlanetActor.h"
#include "TimerManager.h"

// =============================================================================
// DEBUG CONFIG — toggle these to enable/disable debug visualizations and logs
// =============================================================================
static constexpr bool bDebugDrawWaypoints  = false; // Green spheres at each waypoint in CreatePathTo
static constexpr bool bDebugLogPathfinding = false; // "[PATHFINDING] Path complete" log from FindPath
static constexpr bool bDebugLogP23Movement = false; // Per-second movement log for P2.3 AI vehicles
// =============================================================================

AVehicleActor::AVehicleActor()
{
	PrimaryActorTick.bCanEverTick = true; // Enable tick for movement

	// Create collision sphere as root for proper sweep collision
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(60.0f); // Slightly larger than visual mesh (50 units)
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Overlap other vehicles (no hard collision)
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Block cities
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // Block projectiles
	CollisionSphere->SetCollisionObjectType(ECC_Pawn);
	CollisionSphere->SetHiddenInGame(true); // Don't show collision visualization

	// Create vehicle mesh (using cube, smaller than resource)
	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetupAttachment(RootComponent);
	
	// Load cube mesh from engine content
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		VehicleMesh->SetStaticMesh(CubeMesh.Object);
	}
	
	// Scale smaller for vehicle
	VehicleMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	
	// Try to load team color material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColorMat(TEXT("/Game/M_TeamColor"));
	if (TeamColorMat.Succeeded())
	{
		VehicleMesh->SetMaterial(0, TeamColorMat.Object);
	}
	
	// Enable collision for selection only (not physics blocking)
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VehicleMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	VehicleMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Create selection box (wireframe cube)
	SelectionBox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBox"));
	SelectionBox->SetupAttachment(RootComponent);
	SelectionBox->SetStaticMesh(CubeMesh.Object);
	SelectionBox->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f)); // Slightly larger than vehicle
	SelectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionBox->SetVisibility(false); // Hidden by default
	SelectionBox->SetRenderCustomDepth(true);
	
	// Try to load yellow selection material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SelectionMat(TEXT("/Game/M_SelectionBox_Yellow"));
	if (SelectionMat.Succeeded())
	{
		SelectionBox->SetMaterial(0, SelectionMat.Object);
	}

	// Create target marker (small sphere to show destination)
	TargetMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMarker"));
	TargetMarker->SetupAttachment(RootComponent);
	
	// Load sphere mesh for target marker
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		TargetMarker->SetStaticMesh(SphereMesh.Object);
	}
	
	TargetMarker->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f)); // Small marker
	TargetMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetMarker->SetVisibility(false); // Hidden by default
	
	// Use absolute world transform so it doesn't move with vehicle
	TargetMarker->SetAbsolute(false, true, false); // Absolute rotation only
	TargetMarker->SetUsingAbsoluteLocation(true); // Absolute world location
	TargetMarker->SetUsingAbsoluteRotation(true); // Absolute world rotation
	
	// Try to load yellow material for target marker
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TargetMat(TEXT("/Game/M_SelectionBox_Yellow"));
	if (TargetMat.Succeeded())
	{
		TargetMarker->SetMaterial(0, TargetMat.Object);
	}

	// Create health bar widget
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen); // Billboard to camera
	HealthBarWidget->SetDrawSize(FVector2D(100.0f, 10.0f)); // Small bar
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f)); // Above vehicle
	HealthBarWidget->SetVisibility(false); // Hidden when at full health

	// Load health bar widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> HealthBarClass(TEXT("/Game/WBP_HealthBar"));
	if (HealthBarClass.Succeeded())
	{
		HealthBarWidget->SetWidgetClass(HealthBarClass.Class);
	}

	// Create info widget (shows on hover)
	InfoWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InfoWidget"));
	InfoWidget->SetupAttachment(RootComponent);
	InfoWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InfoWidget->SetDrawSize(FVector2D(150.0f, 40.0f));
	InfoWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f)); // Above health bar
	InfoWidget->SetVisibility(false); // Hidden by default

	// Load info UI widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> InfoUIClass(TEXT("/Game/UI/WBP_InfoUI.WBP_InfoUI_C"));
	if (InfoUIClass.Succeeded())
	{
		InfoWidget->SetWidgetClass(InfoUIClass.Class);
	}

	// Create damage smoke particle effect
	DamageSmoke = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("DamageSmoke"));
	DamageSmoke->SetupAttachment(RootComponent);

	// Load P_Smoke particle system from StarterContent
	static ConstructorHelpers::FObjectFinder<UParticleSystem> SmokeParticle(TEXT("/Game/StarterContent/Particles/P_Smoke"));
	if (SmokeParticle.Succeeded())
	{
		DamageSmoke->SetTemplate(SmokeParticle.Object);
		DamageSmoke->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f)); // Above vehicle
		DamageSmoke->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f)); // Moderate smoke
	}
	DamageSmoke->bAutoActivate = false; // Only show when damaged
	DamageSmoke->SetVisibility(false);
}

void AVehicleActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Note: AlignToPlanet() should be called by spawner after setting correct PlanetRadius
	// Don't auto-align here as it would use the default PlanetRadius value
	
	// Set initial color to cyan (player)
	UpdateColor();

	// Initialize health
	CurrentHealth = MaxHealth;
	UpdateHealthBar();
	
	// Initialize info widget
	UpdateInfoDisplay();
	
	// Verify collision settings
	if (CollisionSphere)
	{
		// bool bBlocksWorldStatic = CollisionSphere->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block;
		// bool bBlocksPawn = CollisionSphere->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
		// ECollisionEnabled::Type CollisionEnabled = CollisionSphere->GetCollisionEnabled();
		
		// UE_LOG(LogTemp, Warning, TEXT("Vehicle spawned. Collision enabled: %d, Blocks WorldStatic: %d, Blocks Pawn: %d"),
		// 	(int32)CollisionEnabled, bBlocksWorldStatic, bBlocksPawn);
	}
}

void AVehicleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Safety check: if we're being destroyed or have no world, don't tick
	if (!GetWorld() || !IsValid(this))
	{
		return;
	}
	
	// Debug: Draw collision sphere
	//if (CollisionSphere)
	//{
	//	DrawDebugSphere(GetWorld(), GetActorLocation(), CollisionSphere->GetScaledSphereRadius(), 
	//		12, FColor::Red, false, -1.0f, 0, 1.0f);
	//}
	
	// Combat: Track attack cooldown
	TimeSinceLastAttack += DeltaTime;

	// Reset firing flag at start of frame
	bIsFiring = false;

	UWorld* World = GetWorld(); // Cache world pointer for safety

	// Refresh actor caches every 2s - avoids per-frame GetAllActorsOfClass (major performance gain)
	ActorCacheTimer += DeltaTime;
	if (ActorCacheTimer >= 2.0f && World)
	{
		ActorCacheTimer = FMath::FRandRange(0.0f, 0.5f); // Stagger refreshes so vehicles don't all hit on the same frame
		UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), CachedAllCities);
		UGameplayStatics::GetAllActorsOfClass(World, AVehicleActor::StaticClass(), CachedAllVehicles);
		UGameplayStatics::GetAllActorsOfClass(World, AResourceActor::StaticClass(), CachedAllResources);
		UGameplayStatics::GetAllActorsOfClass(World, ABuildingActor::StaticClass(), CachedAllBuildings);
		UGameplayStatics::GetAllActorsOfClass(World, AMineActor::StaticClass(), CachedAllMines);
		if (!CachedTeamController && OwnerTeam != EOwnerTeam::Player && OwnerTeam != EOwnerTeam::Neutral)
		{
			TArray<AActor*> FoundControllers;
			UGameplayStatics::GetAllActorsOfClass(World, AAITeamController::StaticClass(), FoundControllers);
			for (AActor* Actor : FoundControllers)
			{
				if (AAITeamController* TC = Cast<AAITeamController>(Actor))
				{
					if (TC->ControlledTeam == OwnerTeam)
					{
						CachedTeamController = TC;
						break;
					}
				}
			}
		}
	}

	// Get fire rate multiplier (affects attack cooldown)
	float FireRateMultiplier = 1.0f;
	if (World)
	{
		if (OwnerTeam == EOwnerTeam::Player)
		{
			APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
			if (PlayerController)
			{
				FireRateMultiplier = PlayerController->GetFireRateMultiplier();
			}
		}
		else // AI teams
		{
			// Check if AI team has Black Substrate available (use cached controller)
			if (CachedTeamController)
			{
				FireRateMultiplier = (CachedTeamController->BlackSubstrate > 0) ? 1.0f : 0.25f;
			}
		}
	}
	
	// Calculate effective attack cooldown (higher fire rate = shorter cooldown)
	float EffectiveAttackCooldown = AttackCooldown / FireRateMultiplier;

	// Combat: Find and attack nearby enemies (vehicles, turrets, or cities)
	if (TimeSinceLastAttack >= EffectiveAttackCooldown)
	{
		// Get GameMode for relationship checks
		APlanetConquestGameMode* GameMode = nullptr;
		if (World)
		{
			GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
		}
		const float HostilityThreshold = -0.5f; // Fire on enemies (relationship <= -0.5)
		
		AActor* TargetToFireAt = nullptr; // The target we'll actually shoot at this frame
		
		// PRIORITY 0: DEFEND CAPITAL - If capital is under attack and we're in same territory, engage attackers (HIGHEST PRIORITY)
		// This overrides AI priority system - vehicles automatically defend their capital when it's attacked
		if (GameMode && OwnerTeam != EOwnerTeam::Neutral)
		{
			// Find all cities belonging to our team
			const TArray<AActor*>& FoundCities = CachedAllCities;
			
			for (AActor* CityActor : FoundCities)
			{
				ACityActor* City = Cast<ACityActor>(CityActor);
				if (!City || City->OwnerTeam != OwnerTeam) continue;
				
				// Check if we're within this city's territory
				float DistanceToCity = FVector::Dist(GetActorLocation(), City->GetActorLocation());
				if (DistanceToCity > GameMode->TERRITORY_RADIUS) continue;
				
				// Check if capital building exists and is under attack
				if (City->CapitalBuilding && City->CapitalBuilding->LastDamagingActor.IsValid())
				{
					AActor* CapitalAttacker = City->CapitalBuilding->LastDamagingActor.Get();
					
					// Verify attacker is still alive and is a vehicle
					AVehicleActor* AttackerVehicle = Cast<AVehicleActor>(CapitalAttacker);
					if (AttackerVehicle && AttackerVehicle->CurrentHealth > 0 && AttackerVehicle->OwnerTeam != OwnerTeam)
					{
						// Find closest enemy vehicle that's attacking the capital
						float DistanceToAttacker = FVector::Dist(GetActorLocation(), AttackerVehicle->GetActorLocation());
						
						// If attacker is in range, engage immediately
						if (DistanceToAttacker <= AttackRange)
						{
							TargetToFireAt = AttackerVehicle;
							UE_LOG(LogTemp, Log, TEXT("Vehicle defending capital - firing on attacker"));
							break; // Found target, stop searching
						}
						else
						{
							// Attacker is out of range - move toward them to engage
							// Set as combat target - combat positioning logic will maintain optimal range
							if (!bHasTarget || CurrentTarget != AttackerVehicle)
							{
								CurrentTarget = AttackerVehicle;
								bHasTarget = true;
								UE_LOG(LogTemp, Log, TEXT("Vehicle defending capital - moving to engage attacker"));
							}
						}
					}
				}
			}
		}
		
		// PRIORITY 1: Retaliate against actor that damaged us (overrides manual targets)
		// This ensures vehicles defend themselves even when manually targeted on cities
		if (!TargetToFireAt && LastDamagingActor.IsValid())
		{
			float DistanceToAttacker = FVector::Dist(GetActorLocation(), LastDamagingActor.Get()->GetActorLocation());
			
			// Check if attacker is in range
			if (DistanceToAttacker <= AttackRange)
			{
				bool bAttackerAlive = false;
				EOwnerTeam AttackerTeam = EOwnerTeam::Neutral;
				
				// Check if it's a vehicle
				AVehicleActor* AttackerVehicle = Cast<AVehicleActor>(LastDamagingActor.Get());
				if (AttackerVehicle && AttackerVehicle->CurrentHealth > 0)
				{
					bAttackerAlive = true;
					AttackerTeam = AttackerVehicle->OwnerTeam;
				}
				
				// Check if it's a building (turret, mine, etc)
				ABuildingActor* AttackerBuilding = Cast<ABuildingActor>(LastDamagingActor.Get());
				if (AttackerBuilding && AttackerBuilding->CurrentHealth > 0)
				{
					bAttackerAlive = true;
					AttackerTeam = AttackerBuilding->OwnerTeam;
				}
				
				// Check if it's a city
				ACityActor* AttackerCity = Cast<ACityActor>(LastDamagingActor.Get());
				if (AttackerCity && AttackerCity->CurrentHealth > 0)
				{
					bAttackerAlive = true;
					AttackerTeam = AttackerCity->OwnerTeam;
				}
				
				// Check if it's a kaiju
				AKaijuActor* AttackerKaiju = Cast<AKaijuActor>(LastDamagingActor.Get());
				if (AttackerKaiju && AttackerKaiju->CurrentHealth > 0)
				{
					// Check if kaiju is hostile to our team using relationship system
					int32 TeamID = static_cast<int32>(OwnerTeam);
					if (AttackerKaiju->IsHostileToTeam(TeamID))
					{
						TargetToFireAt = AttackerKaiju;
						bAttackerAlive = true;
					}
				}
				
				// If attacker is alive, hostile, and in range, RETALIATE (breaks away from manual target)
				// Skip this check if we already set target to kaiju above
				if (!TargetToFireAt && bAttackerAlive && AttackerTeam != OwnerTeam && AttackerTeam != EOwnerTeam::Neutral)
				{
					bool bIsHostile = ForcedHostileTeams.Contains(AttackerTeam);
					if (!bIsHostile && GameMode)
					{
						float Relationship = GameMode->GetRelationship(OwnerTeam, AttackerTeam);
						bIsHostile = (Relationship <= HostilityThreshold);
					}
					
					if (bIsHostile)
					{
						TargetToFireAt = LastDamagingActor.Get(); // RETALIATE!
					}
				}
			}
		}
		
		// PRIORITY 2: Fire on manually-targeted actor (player/AI-commanded target like cities/mines)
		// Only if we're NOT retaliating against an attacker
		if (!TargetToFireAt && CurrentTarget && IsValid(CurrentTarget))
		{
			// Check if target is in range
			float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
			
			if (DistanceToTarget <= AttackRange)
			{
				// Check if target is still alive
				bool bTargetAlive = false;
				
				// Check if it's a vehicle
				AVehicleActor* TargetVehicle = Cast<AVehicleActor>(CurrentTarget);
				if (TargetVehicle && TargetVehicle->CurrentHealth > 0)
				{
					bTargetAlive = true;
				}
				
				// Check if it's a building (includes mines, turrets, capitals)
				ABuildingActor* TargetBuilding = Cast<ABuildingActor>(CurrentTarget);
				if (TargetBuilding && TargetBuilding->CurrentHealth > 0)
				{
					bTargetAlive = true;
				}
				
				// Check if it's a kaiju
				AKaijuActor* TargetKaiju = Cast<AKaijuActor>(CurrentTarget);
				if (TargetKaiju && TargetKaiju->CurrentHealth > 0)
				{
					bTargetAlive = true;
				}
				
				// If target is alive and in range, fire on it
				if (bTargetAlive)
				{
					TargetToFireAt = CurrentTarget;
				}
				else
				{
					// Target is dead, clear it
					CurrentTarget = nullptr;
					bHasTarget = false;
					
					// If this wasn't our primary target, check if we should return to primary objective
					if (PrimaryTarget && IsValid(PrimaryTarget))
					{
						// Check if primary target is still alive
						bool bPrimaryAlive = false;
						AVehicleActor* PrimaryVehicle = Cast<AVehicleActor>(PrimaryTarget);
						ABuildingActor* PrimaryBuilding = Cast<ABuildingActor>(PrimaryTarget);
						ACityActor* PrimaryCity = Cast<ACityActor>(PrimaryTarget);
						AKaijuActor* PrimaryKaiju = Cast<AKaijuActor>(PrimaryTarget);
						
						if ((PrimaryVehicle && PrimaryVehicle->CurrentHealth > 0) ||
							(PrimaryBuilding && PrimaryBuilding->CurrentHealth > 0) ||
							(PrimaryCity && PrimaryCity->CurrentHealth > 0) ||
							(PrimaryKaiju && PrimaryKaiju->CurrentHealth > 0))
						{
							bPrimaryAlive = true;
						}
						
						// Return to primary target
						if (bPrimaryAlive)
						{
							// Check if primary target is a capital building - if so, look for turrets first
							ACapitalBuildingActor* CapitalTarget = Cast<ACapitalBuildingActor>(PrimaryTarget);
							if (CapitalTarget && CapitalTarget->ParentCity)
							{
								// Find remaining turrets in the city
								ATurretBuildingActor* NextTurret = nullptr;
								for (ABuildingActor* Building : CapitalTarget->ParentCity->Buildings)
								{
									ATurretBuildingActor* Turret = Cast<ATurretBuildingActor>(Building);
									if (Turret && Turret->CurrentHealth > 0)
									{
										NextTurret = Turret;
										break;
									}
								}
								
								if (NextTurret)
								{
									// Target next turret before attacking capital
									CurrentTarget = NextTurret;
									bHasTarget = true;
									SetTargetLocation(NextTurret->GetActorLocation());
									UE_LOG(LogTemp, Warning, TEXT("Vehicle targeting next turret before capital: %s"), *NextTurret->GetName());
								}
								else
								{
									// All turrets destroyed, now attack capital
									CurrentTarget = PrimaryTarget;
									bHasTarget = true;
									SetTargetLocation(PrimaryTarget->GetActorLocation());
									UE_LOG(LogTemp, Warning, TEXT("All turrets destroyed - now attacking capital"));
								}
							}
							else
							{
								// Not a capital target - clear CurrentTarget so ExecuteAttackTargetTask
								// re-evaluates and re-navigates with proper angular offset on the next tick
								CurrentTarget = nullptr;
								bHasTarget = false;
								UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Returning to primary target after eliminating distraction"), *GetName());
							}
						}
						else
						{
							// Primary target is also dead, clear it
							PrimaryTarget = nullptr;
						}
					}
				}
			}
		}
		
		// If we have a target to fire at (retaliation or manual), shoot it now
		if (TargetToFireAt && IsValid(TargetToFireAt))
		{
			// Spawn projectile slightly ahead of vehicle to avoid collision with self
			FVector DirectionToTarget = (TargetToFireAt->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			
			// Add upward arc to projectile (30 degrees upward from horizontal)
			FVector UpDirection = (GetActorLocation() - PlanetCenter).GetSafeNormal();
			FVector ArcDirection = (DirectionToTarget + UpDirection * 0.577f).GetSafeNormal(); // tan(30°) ≈ 0.577
			
			FVector SpawnLocation = GetActorLocation() + ArcDirection * 100.0f; // Spawn 100 units ahead
			FRotator SpawnRotation = FRotator::ZeroRotator;

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetInstigator();
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AProjectileActor* Projectile = GetWorld()->SpawnActor<AProjectileActor>(
				AProjectileActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);

			if (Projectile)
			{
				Projectile->TargetLocation = TargetToFireAt->GetActorLocation();
				Projectile->TargetActor = TargetToFireAt;
				Projectile->OwnerTeam = OwnerTeam;
				Projectile->Damage = AttackDamage;
				Projectile->PlanetCenter = PlanetCenter;
				Projectile->PlanetRadius = PlanetRadius;
				if (ProjectileArcHeight > 0.0f)
				{
					Projectile->ArcHeight = ProjectileArcHeight;
					Projectile->ArcMaxFlightTime = FVector::Dist(SpawnLocation, Projectile->TargetLocation) / Projectile->ProjectileSpeed;
				}
				if (Projectile->ProjectileMesh)
				{
					FLinearColor ProjectileColor = FLinearColor(0.5f, 0.5f, 0.5f); // Default gray
					switch (OwnerTeam)
					{
					case EOwnerTeam::Player:
						ProjectileColor = FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
						break;
					case EOwnerTeam::AI1:
						ProjectileColor = FLinearColor(1.0f, 0.0f, 0.0f); // Red
						break;
					case EOwnerTeam::AI2:
						ProjectileColor = FLinearColor(0.0f, 1.0f, 0.0f); // Green
						break;
					case EOwnerTeam::AI3:
						ProjectileColor = FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
						break;
					case EOwnerTeam::AI4:
						ProjectileColor = FLinearColor(0.5f, 0.0f, 1.0f); // Purple
						break;
					case EOwnerTeam::AI5:
						ProjectileColor = FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
						break;
					case EOwnerTeam::AI6:
						ProjectileColor = FLinearColor(0.0f, 0.0f, 0.0f); // Black
						break;
					case EOwnerTeam::AI7:
						ProjectileColor = FLinearColor(0.2f, 0.3f, 0.85f); // Royal blue
						break;
					case EOwnerTeam::AI8:
						ProjectileColor = FLinearColor(1.0f, 1.0f, 1.0f); // White
						break;
					case EOwnerTeam::AI9:
						ProjectileColor = FLinearColor(1.0f, 0.0f, 0.5f); // Hot pink
						break;
					case EOwnerTeam::AI10:
						ProjectileColor = FLinearColor(1.0f, 0.55f, 0.0f); // Orange
						break;
					case EOwnerTeam::AI11:
						ProjectileColor = FLinearColor(0.0f, 0.7f, 0.5f); // Teal
						break;
					case EOwnerTeam::AI12:
						ProjectileColor = FLinearColor(0.5f, 1.0f, 0.0f); // Lime
						break;
					case EOwnerTeam::AI13:
						ProjectileColor = FLinearColor(1.0f, 0.0f, 1.0f); // Magenta
						break;
					case EOwnerTeam::AI14:
						ProjectileColor = FLinearColor(0.55f, 0.27f, 0.0f); // Bronze
						break;
					case EOwnerTeam::AI15:
						ProjectileColor = FLinearColor(0.05f, 0.1f, 0.5f); // Navy
						break;
					case EOwnerTeam::AI16:
						ProjectileColor = FLinearColor(1.0f, 0.75f, 0.0f); // Gold
						break;
					case EOwnerTeam::AI17:
						ProjectileColor = FLinearColor(0.7f, 0.7f, 0.7f); // Silver
						break;
					case EOwnerTeam::AI18:
						ProjectileColor = FLinearColor(0.55f, 0.0f, 0.05f); // Crimson
						break;
					case EOwnerTeam::AI19:
						ProjectileColor = FLinearColor(0.5f, 1.0f, 0.75f); // Mint
						break;
					}
					
					// Make emissive for glow effect (multiply by 3 for brightness)
					FLinearColor EmissiveColor = ProjectileColor * 3.0f;
					
					UMaterialInstanceDynamic* DynMaterial = Projectile->ProjectileMesh->CreateDynamicMaterialInstance(0);
					if (DynMaterial)
					{
						DynMaterial->SetVectorParameterValue(FName("BaseColor"), ProjectileColor);
						DynMaterial->SetVectorParameterValue(FName("Color"), ProjectileColor);
						DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), EmissiveColor);
						DynMaterial->SetVectorParameterValue(FName("Tint"), ProjectileColor);
					}
					Projectile->ProjectileMesh->SetVectorParameterValueOnMaterials(FName("VertexColor"), FVector(ProjectileColor.R, ProjectileColor.G, ProjectileColor.B));
				}
			}
			
			// Mark vehicle as firing this frame
			TimeSinceLastAttack = 0.0f;
			
			// Don't do auto-fire if we fired on priority target (retaliation or manual)
			return;
		}
		
		// PRIORITY 3 & 4: Auto-fire at turrets/vehicles (only if no retaliation or manual target)
		AActor* NearestTarget = nullptr;
		float NearestDistance = AttackRange;
		
		// PRIORITY 3: Find enemy turrets (threats that need to be destroyed)
		{
			ATurretBuildingActor* NearestTurret = nullptr;
			float NearestTurretDistance = AttackRange;
			const TArray<AActor*>& FoundBuildings = CachedAllBuildings;

		for (AActor* Actor : FoundBuildings)
		{
			ATurretBuildingActor* Turret = Cast<ATurretBuildingActor>(Actor);
			if (!Turret) continue;

			// Only target enemy turrets
			if (Turret->OwnerTeam == OwnerTeam) continue;
			
			// Check relationship - only fire if hostile enough OR if team is in forced hostile list
			bool bIsHostile = ForcedHostileTeams.Contains(Turret->OwnerTeam);
			if (!bIsHostile && GameMode)
			{
				float Relationship = GameMode->GetRelationship(OwnerTeam, Turret->OwnerTeam);
				if (Relationship > HostilityThreshold)
				{
					continue; // Not hostile enough, skip this target
				}
			}

			// Check if in range
			float Distance = FVector::Dist(GetActorLocation(), Turret->GetActorLocation());
			if (Distance < NearestTurretDistance)
			{
				NearestTurretDistance = Distance;
				NearestTurret = Turret;
			}
		}

		// If we found a turret, prioritize it over everything else
		if (NearestTurret)
		{
			NearestTarget = NearestTurret;
			NearestDistance = NearestTurretDistance;
		}
		else
		{
			// PRIORITY 3: Find enemy vehicles
			const TArray<AActor*>& FoundVehicles = CachedAllVehicles;

			for (AActor* Actor : FoundVehicles)
			{
				AVehicleActor* OtherVehicle = Cast<AVehicleActor>(Actor);
				if (!OtherVehicle || OtherVehicle == this) continue;

				// Only target enemies
				if (OtherVehicle->OwnerTeam == OwnerTeam) continue;
				
				// Check relationship - only fire if hostile enough OR if team is in forced hostile list
				bool bIsHostile = ForcedHostileTeams.Contains(OtherVehicle->OwnerTeam);
				if (!bIsHostile && GameMode)
				{
					float Relationship = GameMode->GetRelationship(OwnerTeam, OtherVehicle->OwnerTeam);
					if (Relationship > HostilityThreshold)
					{
						continue; // Not hostile enough, skip this target
					}
				}

				// Check if in range
				float Distance = FVector::Dist(GetActorLocation(), OtherVehicle->GetActorLocation());
				if (Distance < NearestDistance)
				{
					NearestDistance = Distance;
					NearestTarget = OtherVehicle;
				}
			}
			
			// PRIORITY 4: Find hostile kaiju (only if no turrets or vehicles nearby)
			if (!NearestTarget)
			{
				TArray<AActor*> FoundKaiju;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AKaijuActor::StaticClass(), FoundKaiju);
				
				for (AActor* Actor : FoundKaiju)
				{
					AKaijuActor* Kaiju = Cast<AKaijuActor>(Actor);
					if (!Kaiju || Kaiju->CurrentHealth <= 0) continue;
					
					// Check if kaiju is hostile to our team using relationship system
					int32 TeamID = static_cast<int32>(OwnerTeam);
					if (!Kaiju->IsHostileToTeam(TeamID)) continue; // Skip if neutral or friendly
					
					// Check if in range
					float Distance = FVector::Dist(GetActorLocation(), Kaiju->GetActorLocation());
					if (Distance < NearestDistance)
					{
						NearestDistance = Distance;
						NearestTarget = Kaiju;
					}
				}
			}
		}
	}
	
	// If we found an auto-fire target (turret or vehicle), shoot at it
	if (NearestTarget)
	{
		// Spawn projectile at weapon fire point
		FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
		FRotator SpawnRotation = (NearestTarget->GetActorLocation() - SpawnLocation).Rotation();
		
		AProjectileActor* Projectile = GetWorld()->SpawnActor<AProjectileActor>(AProjectileActor::StaticClass(), SpawnLocation, SpawnRotation);
		if (Projectile)
		{
			Projectile->SetOwner(this);
			Projectile->TargetLocation = NearestTarget->GetActorLocation();
			Projectile->TargetActor = NearestTarget;
			Projectile->OwnerTeam = OwnerTeam;
			Projectile->Damage = AttackDamage;
			Projectile->PlanetCenter = PlanetCenter;
			Projectile->PlanetRadius = PlanetRadius;
			if (ProjectileArcHeight > 0.0f)
			{
				Projectile->ArcHeight = ProjectileArcHeight;
				FVector AutoFireSpawn = GetActorLocation() + GetActorForwardVector() * 100.0f;
				Projectile->ArcMaxFlightTime = FVector::Dist(AutoFireSpawn, Projectile->TargetLocation) / Projectile->ProjectileSpeed;
			}
			FLinearColor ProjectileColor;
			switch (OwnerTeam)
			{
			case EOwnerTeam::Player:
				ProjectileColor = FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
				break;
			case EOwnerTeam::AI1:
				ProjectileColor = FLinearColor(1.0f, 0.0f, 0.0f); // Red
				break;
			case EOwnerTeam::AI2:
				ProjectileColor = FLinearColor(0.0f, 1.0f, 0.0f); // Green
				break;
			case EOwnerTeam::AI3:
				ProjectileColor = FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
				break;
			case EOwnerTeam::AI4:
				ProjectileColor = FLinearColor(0.5f, 0.0f, 1.0f); // Purple
				break;
			case EOwnerTeam::AI5:
				ProjectileColor = FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
				break;
			case EOwnerTeam::AI6:
				ProjectileColor = FLinearColor(0.0f, 0.0f, 0.0f); // Black
				break;
			case EOwnerTeam::AI7:
				ProjectileColor = FLinearColor(0.2f, 0.3f, 0.85f); // Royal blue
				break;
			case EOwnerTeam::AI8:
				ProjectileColor = FLinearColor(1.0f, 1.0f, 1.0f); // White
				break;
			case EOwnerTeam::AI9:
				ProjectileColor = FLinearColor(1.0f, 0.0f, 0.5f); // Hot pink
				break;
			case EOwnerTeam::AI10:
				ProjectileColor = FLinearColor(1.0f, 0.55f, 0.0f); // Orange
				break;
			case EOwnerTeam::AI11:
				ProjectileColor = FLinearColor(0.0f, 0.7f, 0.5f); // Teal
				break;
			case EOwnerTeam::AI12:
				ProjectileColor = FLinearColor(0.5f, 1.0f, 0.0f); // Lime
				break;
			case EOwnerTeam::AI13:
				ProjectileColor = FLinearColor(1.0f, 0.0f, 1.0f); // Magenta
				break;
			case EOwnerTeam::AI14:
				ProjectileColor = FLinearColor(0.55f, 0.27f, 0.0f); // Bronze
				break;
			case EOwnerTeam::AI15:
				ProjectileColor = FLinearColor(0.05f, 0.1f, 0.5f); // Navy
				break;
			case EOwnerTeam::AI16:
				ProjectileColor = FLinearColor(1.0f, 0.75f, 0.0f); // Gold
				break;
			case EOwnerTeam::AI17:
				ProjectileColor = FLinearColor(0.7f, 0.7f, 0.7f); // Silver
				break;
			case EOwnerTeam::AI18:
				ProjectileColor = FLinearColor(0.55f, 0.0f, 0.05f); // Crimson
				break;
			case EOwnerTeam::AI19:
				ProjectileColor = FLinearColor(0.5f, 1.0f, 0.75f); // Mint
				break;
			default:
				ProjectileColor = FLinearColor(1.0f, 1.0f, 1.0f); // White for any other team
				break;
			}
			
			// Apply color to projectile with emissive glow
			UMaterialInstanceDynamic* DynMaterial = Projectile->ProjectileMesh->CreateDynamicMaterialInstance(0);
			if (DynMaterial)
			{
				// Make emissive for glow effect (multiply by 3 for brightness)
				FLinearColor EmissiveColor = ProjectileColor * 3.0f;
				DynMaterial->SetVectorParameterValue(FName("BaseColor"), ProjectileColor);
				DynMaterial->SetVectorParameterValue(FName("Color"), ProjectileColor);
				DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), EmissiveColor);
				DynMaterial->SetVectorParameterValue(FName("Tint"), ProjectileColor);
			}
			Projectile->ProjectileMesh->SetVectorParameterValueOnMaterials(FName("VertexColor"), FVector(ProjectileColor.R, ProjectileColor.G, ProjectileColor.B));
		}
		
		// Mark vehicle as firing this frame
		bIsFiring = true;
		
		// Reset attack cooldown
		TimeSinceLastAttack = 0.0f;
	}
	} // End of attack cooldown check

	// City healing: Check if near own city and ready to heal
	if (CurrentHealth < MaxHealth && TimeSinceLastDamaged >= HealingStartDelay)
	{
		// Find the player's or enemy's city based on team
		ACityActor* OwnCity = nullptr;
		const TArray<AActor*>& AllCities = CachedAllCities;

		for (AActor* Actor : AllCities)
		{
			ACityActor* City = Cast<ACityActor>(Actor);
			if (City && City->OwnerTeam == OwnerTeam)
			{
				OwnCity = City;
				break;
			}
		}

		// If near own city, heal over time
		if (OwnCity)
		{
			float DistanceToCity = FVector::Dist(GetActorLocation(), OwnCity->GetActorLocation());
			if (DistanceToCity <= CityHealingRange)
			{
				TimeSinceLastHeal += DeltaTime;
				
				if (TimeSinceLastHeal >= HealingInterval)
				{
					// Heal the vehicle
					CurrentHealth = FMath::Min(CurrentHealth + HealingAmount, MaxHealth);
					TimeSinceLastHeal = 0.0f;
					
					// Update health bar
					UpdateHealthBar();
				}
			}
		}
	}
	
	// Update damage smoke effect based on health
	if (DamageSmoke)
	{
		float HealthPercent = CurrentHealth / MaxHealth;
		bool bShouldShowSmoke = (HealthPercent < 0.4f); // Show smoke at <40% health
		
		if (bShouldShowSmoke && !DamageSmoke->IsActive())
		{
			DamageSmoke->SetVisibility(true);
			DamageSmoke->Activate();
		}
		else if (!bShouldShowSmoke && DamageSmoke->IsActive())
		{
			DamageSmoke->Deactivate();
			DamageSmoke->SetVisibility(false);
		}
	}
	
	// Clear completed targets - if resource is already captured by any team (friend or foe), we can't capture it
	if (TargetResource && TargetResource->OwnerTeam != EOwnerTeam::Neutral)
	{
		if (OwnerTeam == EOwnerTeam::Player)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Resource %s already owned by team %d - clearing target"), 
				*TargetResource->GetName(), (int32)TargetResource->OwnerTeam);
		}
		TargetResource = nullptr;
	}
	
	// ========== TASK SYSTEM EXECUTION ==========
	// Route to task-specific execution logic based on CurrentTask.Type
	// These methods will set CurrentTarget, TargetResource, bHasTarget appropriately
	// The rest of Tick() will handle movement and combat based on those fields
	
	switch (CurrentTask.Type)
	{
		case EVehicleTaskType::DefendTerritory:
			ExecuteDefendTerritoryTask(DeltaTime);
			break;
			
		case EVehicleTaskType::SecureIncome:
			ExecuteSecureIncomeTask(DeltaTime);
			break;
			
		case EVehicleTaskType::AttackTarget:
			ExecuteAttackTargetTask(DeltaTime);
			break;
			
		case EVehicleTaskType::AidAlly:
			ExecuteAidAllyTask(DeltaTime);
			break;
			
		case EVehicleTaskType::Retaliate:
			ExecuteRetaliateTask(DeltaTime);
			break;
			
		case EVehicleTaskType::None:
		default:
			// No active task - fall through to legacy autonomous logic below
			break;
	}
	
	// ========== END TASK SYSTEM EXECUTION ==========
	
	// Autonomous AI targeting: If assigned to an action and idle, find next target
	if (bHasAssignment && !bHasTarget && !TargetResource)
	{
		FindNextTarget();
	}

	// AI autonomous targeting: If in autonomous mode and idle, find next target
	if (bAIAutonomousMode && !bHasTarget && !TargetResource)
	{
		FindNextTarget();
	}

	// Player autonomous targeting: If in autonomous mode and idle, find next target
	if (bPlayerAutonomousMode && !bHasTarget && !TargetResource && !CurrentTarget)
	{
		FindNextTarget();
	}
	
	// Reset moving flag at start of movement check
	bIsMoving = false;
	
	// If we have a CurrentTarget that's a vehicle, building, city, or kaiju, continuously update our movement target
	AVehicleActor* TargetVehicle = Cast<AVehicleActor>(CurrentTarget);
	ABuildingActor* TargetBuilding = Cast<ABuildingActor>(CurrentTarget);
	ACityActor* TargetCity = Cast<ACityActor>(CurrentTarget);
	AKaijuActor* TargetKaiju = Cast<AKaijuActor>(CurrentTarget);
	
	if (TargetVehicle)
	{
		// Check if target is still valid and alive
		if (IsValid(TargetVehicle) && TargetVehicle->CurrentHealth > 0)
		{
			float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetVehicle->GetActorLocation());
			
			// Maintain optimal combat distance (75% of max range = 1350 units)
			// This allows vehicles to stay at range while still being able to fire
			float OptimalRange = AttackRange * 0.75f;
			float TooCloseThreshold = AttackRange * 0.5f; // Back away if closer than 900 units
			
			// If outside attack range, move toward the enemy vehicle
			if (DistanceToTarget > AttackRange)
			{
				// Too far - close distance to optimal range
				// Only create new path if we don't have one yet
				if (CurrentPath.Num() == 0)
				{
					FVector VehicleLocation = TargetVehicle->GetActorLocation();
					FVector DirectionToTarget = (VehicleLocation - GetActorLocation()).GetSafeNormal();
					
					FVector OptimalPosition = VehicleLocation - DirectionToTarget * OptimalRange;
					
					// Project target location onto planet surface
					FVector DirectionFromCenter = (OptimalPosition - PlanetCenter).GetSafeNormal();
					OptimalPosition = PlanetCenter + DirectionFromCenter * PlanetRadius;
					OptimalPosition = AdjustPositionForCityExclusion(OptimalPosition);
					
					CreatePathTo(OptimalPosition); // Internal combat repositioning
					CombatRepathTimer = 3.0f; // Start repathing timer
				}
				else
				{
					bHasTarget = true; // Continue following existing path
				}
			}
			else if (DistanceToTarget < TooCloseThreshold)
			{
				// Too close - back away to optimal range
				// Only create new path if we don't have one yet
				if (CurrentPath.Num() == 0)
				{
					FVector VehicleLocation = TargetVehicle->GetActorLocation();
					FVector DirectionAwayFromTarget = (GetActorLocation() - VehicleLocation).GetSafeNormal();
					
					FVector OptimalPosition = VehicleLocation + DirectionAwayFromTarget * OptimalRange;
					
					// Project target location onto planet surface
					FVector DirectionFromCenter = (OptimalPosition - PlanetCenter).GetSafeNormal();
					OptimalPosition = PlanetCenter + DirectionFromCenter * PlanetRadius;
					OptimalPosition = AdjustPositionForCityExclusion(OptimalPosition);
					
					CreatePathTo(OptimalPosition); // Internal combat repositioning
					CombatRepathTimer = 3.0f; // Start repathing timer
				}
				else
				{
					bHasTarget = true; // Continue following existing path
				}
			}
			else
			{
				// Within optimal range: ground vehicles stop, ships keep sailing
				if (!bCanMoveWhileFiring)
				{
					CurrentPath.Empty();
					bHasTarget = false;
				}
			}
		}
		else
		{
			// Target is dead or invalid, clear it
			CurrentTarget = nullptr;
			bHasTarget = false;
		}
	}
	else if (TargetBuilding)
	{
		// Check if building target is still valid and alive
		if (IsValid(TargetBuilding) && TargetBuilding->CurrentHealth > 0)
		{
			float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetBuilding->GetActorLocation());
			
			// If outside attack range, move toward the building
			if (DistanceToTarget > AttackRange)
			{
				// Only create new path if we don't have one yet (buildings don't move)
				if (CurrentPath.Num() == 0)
				{
					// Use angular offset approach (same as city/capital in ExecuteAttackTargetTask)
					// Direct SetTargetLocation(building loc) fails when building is at city center (0 dist from city)
					float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
					FVector TgtDir = (TargetBuilding->GetActorLocation() - PlanetCenter).GetSafeNormal();
					FVector SelfDir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
					float DesiredDist = AttackRange * 0.75f;
					float TotalAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(TgtDir, SelfDir), -1.0f, 1.0f));
					float TotalArc = TotalAngle * PlanetR;
					float T = (TotalArc > KINDA_SMALL_NUMBER) ? FMath::Min(DesiredDist / TotalArc, 1.0f) : 1.0f;
					FVector ApproachDir = FMath::Lerp(TgtDir, SelfDir, T).GetSafeNormal();
					float TerrainH = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(ApproachDir) : 0.0f;
					SetTargetLocation(PlanetCenter + ApproachDir * (PlanetR * (1.0f + TerrainH)));
				}
				else
				{
					bHasTarget = true; // Continue following existing path
				}
			}
			else
			{
				// Within attack range: ground vehicles stop, ships keep sailing
				if (!bCanMoveWhileFiring)
				{
					CurrentPath.Empty();
					bHasTarget = false;
				}
			}
		}
		else
		{
			// Building is destroyed, clear it
			CurrentTarget = nullptr;
			bHasTarget = false;
			
			// If this was a mine and we have a PostMineResource, capture it now
			// Ships can't move onto land, so discard instead of capturing
			if (PostMineResource && IsValid(PostMineResource))
			{
				if (bCanCaptureResources)
				{
					UE_LOG(LogTemp, Warning, TEXT("Mine destroyed! Now capturing the resource it was protecting..."));
					SetTargetResource(PostMineResource);
				}
				PostMineResource = nullptr;
			}
		}
	}
	else if (TargetCity)
	{
		// Check if city target is still valid and alive
		if (IsValid(TargetCity) && TargetCity->CurrentHealth > 0)
		{
			float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetCity->GetActorLocation());
			
			// If outside attack range, navigate to approach position on sphere surface
			if (DistanceToTarget > AttackRange)
			{
				// Only create new path if we don't have one yet (cities don't move)
				if (CurrentPath.Num() == 0)
				{
					// Navigate to AttackRange * 0.75 away from city using angular offset on sphere surface
					float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
					FVector TgtDir = (TargetCity->GetActorLocation() - PlanetCenter).GetSafeNormal();
					FVector SelfDir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
					float DesiredDist = AttackRange * 0.75f;
					float TotalAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(TgtDir, SelfDir), -1.0f, 1.0f));
					float TotalArc = TotalAngle * PlanetR;
					float T = (TotalArc > KINDA_SMALL_NUMBER) ? FMath::Min(DesiredDist / TotalArc, 1.0f) : 1.0f;
					FVector ApproachDir = FMath::Lerp(TgtDir, SelfDir, T).GetSafeNormal();
					float TerrainH = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(ApproachDir) : 0.0f;
					SetTargetLocation(PlanetCenter + ApproachDir * (PlanetR * (1.0f + TerrainH)));
				}
				else
				{
					bHasTarget = true; // Continue following existing path
				}
			}
			else
			{
				// Within attack range: ground vehicles stop, ships keep sailing
				if (!bCanMoveWhileFiring)
				{
					CurrentPath.Empty();
					bHasTarget = false;
				}
			}
		}
		else
		{
			// City is destroyed or captured, clear it
			CurrentTarget = nullptr;
			bHasTarget = false;
		}
	}
	else if (TargetKaiju)
	{
		// Check if kaiju target is still valid and alive
		if (IsValid(TargetKaiju) && TargetKaiju->CurrentHealth > 0)
		{
			float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetKaiju->GetActorLocation());
			
			// Maintain optimal combat distance (75% of max range = 1350 units)
			float OptimalRange = AttackRange * 0.75f;
			
			// If outside attack range, move toward the kaiju
			if (DistanceToTarget > AttackRange)
			{
				// Too far - close distance to optimal range
				// Only create new path if we don't have one yet
				if (CurrentPath.Num() == 0)
				{
					FVector KaijuLocation = TargetKaiju->GetActorLocation();
					FVector DirectionToTarget = (KaijuLocation - GetActorLocation()).GetSafeNormal();
					
					FVector OptimalPosition = KaijuLocation - DirectionToTarget * OptimalRange;
					
					// Project target location onto planet surface
					FVector DirectionFromCenter = (OptimalPosition - PlanetCenter).GetSafeNormal();
					OptimalPosition = PlanetCenter + DirectionFromCenter * PlanetRadius;
					
					CreatePathTo(OptimalPosition); // Internal combat repositioning - preserve CurrentTarget
					CombatRepathTimer = 3.0f; // Start repathing timer (kaiju can move)
				}
				else
				{
					bHasTarget = true; // Continue following existing path
				}
			}
			else
			{
				// Within attack range: ground vehicles stop, ships keep sailing
				if (!bCanMoveWhileFiring)
				{
					CurrentPath.Empty();
					bHasTarget = false;
				}
			}
		}
		else
		{
			// Kaiju is destroyed, clear it
			CurrentTarget = nullptr;
			bHasTarget = false;
		}
	}
	
	// Combat repathing: If targeting a moving target (vehicle or kaiju), repath periodically
	// Buildings don't move, so don't repath for them
	if (bHasTarget && CurrentTarget && (TargetVehicle || TargetKaiju))
	{
		CombatRepathTimer -= DeltaTime;
		if (CombatRepathTimer <= 0.0f)
		{
			CombatRepathTimer = 3.0f; // Repath every 3 seconds
			
			// Get target's current location
			FVector TargetActorLocation = CurrentTarget->GetActorLocation();
			float DistToTarget = FVector::Dist(GetActorLocation(), TargetActorLocation);
			
			// Calculate optimal range (75% of max range for vehicles/kaiju)
			float OptimalRange = AttackRange * 0.75f;
			
			// Only repath if we're outside attack range
			if (DistToTarget > AttackRange)
			{
				// Calculate optimal position
				FVector DirectionToTarget = (TargetActorLocation - GetActorLocation()).GetSafeNormal();
				FVector OptimalPosition = TargetActorLocation - DirectionToTarget * OptimalRange;
				
				// Project onto planet surface
				FVector DirectionFromCenter = (OptimalPosition - PlanetCenter).GetSafeNormal();
				OptimalPosition = PlanetCenter + DirectionFromCenter * PlanetRadius;
				OptimalPosition = AdjustPositionForCityExclusion(OptimalPosition);
				
				// Repath to target's new position
				CreatePathTo(OptimalPosition); // Internal combat repositioning - preserve CurrentTarget
			}
			else
			{
				// We're in range - stop moving, let auto-fire handle combat
				CurrentPath.Empty();
				bHasTarget = false;
			}
		}
	}
	
	// Move towards target if we have one (allow movement toward vehicle/building/city/kaiju targets)
	if (bHasTarget && (!CurrentTarget || TargetVehicle || TargetBuilding || TargetCity || TargetKaiju))
	{
		// DEBUG: Log when P2.3 vehicles move
		if (bDebugLogP23Movement && CurrentTask.SubPriorityLabel == TEXT("P2.3") && OwnerTeam != EOwnerTeam::Player)
		{
			static TMap<AActor*, float> LastLogTime;
			float& LastLog = LastLogTime.FindOrAdd(this);
			float CurrentTime = GetWorld()->GetTimeSeconds();
			if (CurrentTime - LastLog >= 1.0f) // Log once per second
			{
				LastLog = CurrentTime;
				UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: P2.3 Moving - bHasTarget:%d, CurrentPath.Num:%d, TargetBuilding:%d"),
					*GetName(), bHasTarget, CurrentPath.Num(), TargetBuilding != nullptr);
			}
		}
		
		MoveTowardsTarget(DeltaTime);
		
		// DEBUG: Check if we're dangerously close to any city
		const TArray<AActor*>& AllCities = CachedAllCities;
		
		for (AActor* Actor : AllCities)
		{
			ACityActor* City = Cast<ACityActor>(Actor);
			if (City)
			{
				float DistanceToCity = FVector::Dist(GetActorLocation(), City->GetActorLocation());
				float CityCollisionRadius = 400.0f; // Cities have 400 unit radius
				float WarningRadius = CityCollisionRadius + 100.0f; // 500 units
				
}
		}
	}

	// Update health bar visibility every frame
	UpdateHealthBar();
}

void AVehicleActor::AlignToPlanet()
{
	// Get direction from planet center to this vehicle
	FVector VehicleLocation = GetActorLocation();
	FVector Direction = (VehicleLocation - PlanetCenter).GetSafeNormal();
	
	// Calculate distance from planet center
	float CurrentDistance = FVector::Dist(VehicleLocation, PlanetCenter);
	
	// If vehicle is at planet center, place it on surface at default position
	if (CurrentDistance < 10.0f)
	{
		Direction = FVector(0.0f, 0.0f, 1.0f); // Default to top of planet
	}
	
	// Move vehicle to hover 50 units above planet surface
	FVector NewLocation = PlanetCenter + Direction * (PlanetRadius + 50.0f);
	SetActorLocation(NewLocation);
	
	// Log initial placement for player vehicles
	if (OwnerTeam == EOwnerTeam::Player)
	{
		float RadialDist = FVector::Dist(NewLocation, PlanetCenter);
		UE_LOG(LogTemp, Warning, TEXT("[VEHICLE INIT] %s placed at radial distance: %.1f (PlanetRadius=%.1f, Target=PlanetRadius+50=%.1f)"),
			*GetName(), RadialDist, PlanetRadius, PlanetRadius + 50.0f);
	}
	
	// Orient vehicle so its up (Z-axis) points away from planet center
	FRotator LookRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
	SetActorRotation(LookRotation);
}

void AVehicleActor::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	
	if (SelectionBox)
	{
		SelectionBox->SetVisibility(bSelected);
	}
}

void AVehicleActor::SetHovered(bool bHovered)
{
	// Only show hover highlight for enemy vehicles (player vehicles use SetSelected)
	if (OwnerTeam != EOwnerTeam::Player)
	{
		// Selection box follows hover only when not selected (selected vehicles keep selection box visible)
		if (!bIsSelected && SelectionBox)
		{
			SelectionBox->SetVisibility(bHovered);
		}
		
		// Info widget ALWAYS follows hover state (even if selected as attack target)
		if (InfoWidget)
		{
			InfoWidget->SetVisibility(bHovered);
			if (bHovered)
			{
				UpdateInfoDisplay();
			}
		}
	}
}

void AVehicleActor::SetTargetLocation(FVector NewTarget)
{
	// Safety check: ensure this actor is valid before proceeding
	if (!IsValid(this))
	{
		return; // Silently fail if object is being destroyed
	}
	
	// Now safe to call GetWorld()
	UWorld* World = GetWorld();
	if (!World)
	{
		return; // No world, likely editor or being destroyed
	}
	
	// Validate target is not inside a city's collision sphere
	const TArray<AActor*>& AllCities = CachedAllCities;
	
	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City)
		{
			float DistanceToCity = FVector::Dist(NewTarget, City->GetActorLocation());
			float SafeAttackDistance = 1000.0f; // Minimum distance to attack cities from
			
			// If target is inside safe attack distance, adjust it to outside
			if (DistanceToCity < SafeAttackDistance)
			{
				// Move target to safe attack distance from city center
				FVector ToCityDirection = (NewTarget - City->GetActorLocation()).GetSafeNormal();
				FVector SafeTarget = City->GetActorLocation() + ToCityDirection * SafeAttackDistance;
				
				NewTarget = SafeTarget;
				break; // Only adjust for first city found
			}
		}
	}
	
	// Use pathfinding system to create a path to the target
	CreatePathTo(NewTarget);
}

void AVehicleActor::SetPathToTarget(FVector Destination)
{
	// Blueprint-callable pathfinding function
	// Simply creates a path to the destination without additional validation
	CreatePathTo(Destination);
}

bool AVehicleActor::CreatePathTo(FVector Destination)
{
	// Internal path creation - preserves combat state (CurrentTarget, assignments, etc.)
	// Used by task execution and combat positioning
	
	if (!OwningPlanet)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] CreatePathTo: No OwningPlanet reference"), *GetName());
		return false;
	}

	// Use planet's pathfinding to find route
	TArray<FVector> Path;
	bool bDebugLog = bDebugLogPathfinding && (OwnerTeam == EOwnerTeam::Player);
	FString FailureReason;
	if (OwningPlanet->FindPath(GetActorLocation(), Destination, Path, bDebugLog, &FailureReason))
	{
		CurrentPath = Path;
		
		// Normalize all waypoints to terrain surface at each waypoint's direction
		// This ensures waypoints follow terrain contours (slopes, water, volcanoes)
		FVector PlanetLoc = OwningPlanet->GetActorLocation();
		float BasePlanetRadius = OwningPlanet->PlanetRadius;
		for (int32 i = 0; i < CurrentPath.Num(); i++)
		{
			FVector Direction = (CurrentPath[i] - PlanetLoc).GetSafeNormal();
			float TerrainHeight = OwningPlanet->CalculateHeightAtPoint(Direction);
			float TerrainRadius = BasePlanetRadius * (1.0f + TerrainHeight);
			CurrentPath[i] = PlanetLoc + Direction * TerrainRadius; // Place at terrain surface
		}
		
		// Draw debug spheres at each NORMALIZED waypoint (what vehicle actually moves to)
		if (bDebugDrawWaypoints)
		{
			for (const FVector& Waypoint : CurrentPath)
			{
				DrawDebugSphere(GetWorld(), Waypoint, 100.0f, 8, FColor::Green, false, 10.0f);
			}
		}
		
		CurrentWaypointIndex = 0;
		bHasTarget = true;
		TargetLocation = Destination; // Store final destination

		// NOTE: CombatRepathTimer is NOT reset here — the caller (Tick combat logic) owns that timer.
		// Resetting it here would cause the repath block to immediately re-fire on the next tick.
		
		// Clear any previous error
		LastPathfindingError.Empty();

		// Show target marker at the destination (if vehicle has one)
		if (TargetMarker && IsValid(TargetMarker))
		{
			TargetMarker->SetWorldLocation(Destination);
			TargetMarker->SetVisibility(true);

			// Orient marker to planet surface
			FVector Direction = (Destination - PlanetCenter).GetSafeNormal();
			FRotator MarkerRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
			TargetMarker->SetWorldRotation(MarkerRotation);
		}
		return true; // Path found successfully
	}
	else
	{
		// Path not found - target unreachable
		FVector VehiclePos = GetActorLocation();
		float Distance = FVector::Dist(VehiclePos, Destination);
		
		// Store error for debugging display
		FString NewError = FString::Printf(TEXT("%s (%.0f units)"), *FailureReason, Distance);
		
		// Only update if error changed (prevents HUD spam)
		if (LastPathfindingError != NewError)
		{
			LastPathfindingError = NewError;
			// Error now only shown in HUD, not console
		}
		
		CurrentPath.Empty();
		bHasTarget = false;
		return false; // Pathfinding failed
	}
}

void AVehicleActor::ClearTarget()
{
	bHasTarget = false;
	CurrentPath.Empty();
	
	// Hide target marker
	if (TargetMarker)
	{
		TargetMarker->SetVisibility(false);
	}
	
	// Note: TargetResource is NOT cleared here - it stays set so capture can continue
	// Only clear TargetResource when given a new command or resource is captured
	
	if (OwnerTeam == EOwnerTeam::Player && TargetResource)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] ClearTarget: bHasTarget cleared but TargetResource preserved (%s)"), *TargetResource->GetName());
	}
}

// ========== NEW TASK SYSTEM IMPLEMENTATION ==========

void AVehicleActor::AssignTask(const FVehicleTask& NewTask)
{
	// OPTIMIZATION: Don't reassign the exact same task if already actively working on it
	// This prevents pathfinding from being regenerated every decision cycle
	// FIXED: Only block if vehicle has a valid path, otherwise allow reassignment for recovery
	if (CurrentTask.Type == NewTask.Type && 
		CurrentTask.Type == EVehicleTaskType::SecureIncome &&
		CurrentTask.Resource == NewTask.Resource &&
		CurrentTask.PrimaryTarget == NewTask.PrimaryTarget &&
		(bHasTarget && CurrentPath.Num() > 0)) // Only block if actively moving with a valid path
	{
		// Already working on this exact task with valid path - don't reinitialize
		UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Ignoring duplicate %s task assignment - already working on it (Resource:%s, PrimaryTarget:%s)"),
			*GetName(), *NewTask.SubPriorityLabel,
			CurrentTask.Resource.IsValid() ? *CurrentTask.Resource->GetName() : TEXT("NULL"),
			CurrentTask.PrimaryTarget.IsValid() ? *CurrentTask.PrimaryTarget->GetName() : TEXT("NULL"));
		return;
	}
	
	// Store the new task atomically
	CurrentTask = NewTask;
	
	// For Phase 1 backward compatibility, also set the legacy fields
	// This allows existing code to keep working while we migrate to task-based execution
	
	// Set AI controller reference
	AIController = NewTask.AssigningController.Get();
	
	// Set loyalty
	LoyalCity = NewTask.LoyalCity.Get();
	
	// Set cluster tracking
	ActiveClusterID = NewTask.ClusterID;
	bClusterOnlyMode = (NewTask.ClusterID != -1);
	
	// Set autonomous mode flags
	if (NewTask.bAutonomousContinue)
	{
		if (NewTask.AssigningController.IsValid())
		{
			// AI task
			bAIAutonomousMode = true;
			AIAutonomousSearchRadius = NewTask.SearchRadius;
			bPlayerAutonomousMode = false;
		}
		else
		{
			// Player task
			bPlayerAutonomousMode = true;
			bTargetNeutralOnly = NewTask.bCaptureNeutralOnly;
			bAIAutonomousMode = false;
		}
	}
	else
	{
		bAIAutonomousMode = false;
		bPlayerAutonomousMode = false;
	}
	
	// Set hostile teams
	ForcedHostileTeams = NewTask.MinesToDestroy;
	
	// Set targets based on task type
	switch (NewTask.Type)
	{
		case EVehicleTaskType::SecureIncome:
			// Income task - may have resource target, mine target, or autonomous search
			if (NewTask.Resource.IsValid())
			{
				// If there's a primary target (mine), attack it first
				if (NewTask.PrimaryTarget.IsValid())
				{
					CurrentTarget = NewTask.PrimaryTarget.Get();
					PrimaryTarget = NewTask.PrimaryTarget.Get();
					// Ships can't capture land resources — only set PostMineResource for land vehicles
					PostMineResource = bCanCaptureResources ? NewTask.Resource.Get() : nullptr;
					
					UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: Assigned %s mine attack task - Target mine: %s, Post-mine resource: %s"),
						*GetName(), *NewTask.SubPriorityLabel, 
						*CurrentTarget->GetName(), 
						PostMineResource ? *PostMineResource->GetName() : TEXT("NULL"));
					
					// Move toward the mine
					SetTargetLocation(CurrentTarget->GetActorLocation());
				}
				else
				{
					// No mine - capture the resource directly using smart waypoint selection
					SetTargetResource(NewTask.Resource.Get());
				}
			}
			else if (NewTask.bAutonomousContinue)
			{
				// Autonomous search mode - vehicle will find targets on its own
				bHasAssignment = true;
				FindNextTarget();
			}
			break;
			
		case EVehicleTaskType::AttackTarget:
			// War/combat task - attack specific target
			if (NewTask.PrimaryTarget.IsValid())
			{
				CurrentTarget = NewTask.PrimaryTarget.Get();
				PrimaryTarget = NewTask.PrimaryTarget.Get();
				SetTargetLocation(CurrentTarget->GetActorLocation());
				bHasAssignment = true;
			}
			break;
			
		case EVehicleTaskType::DefendTerritory:
			// Defensive task - move to location and defend
			if (NewTask.Destination != FVector::ZeroVector)
			{
				CreatePathTo(NewTask.Destination); // Internal task execution
				bHasAssignment = true;
			}
			break;
			
		case EVehicleTaskType::AidAlly:
			// Aid task - move to ally city
			if (NewTask.PrimaryTarget.IsValid())
			{
				DefendedCity = Cast<ACityActor>(NewTask.PrimaryTarget.Get());
				if (DefendedCity)
				{
					SetTargetLocation(DefendedCity->GetActorLocation());
					bHasAssignment = true;
				}
			}
			break;
			
		case EVehicleTaskType::Retaliate:
			// Retaliation task - counterattack
			if (NewTask.PrimaryTarget.IsValid())
			{
				CurrentTarget = NewTask.PrimaryTarget.Get();
				PrimaryTarget = NewTask.PrimaryTarget.Get();
				SetTargetLocation(CurrentTarget->GetActorLocation());
				bHasAssignment = true;
			}
			break;
			
		case EVehicleTaskType::None:
		default:
			// Clear any existing assignment
			bHasAssignment = false;
			break;
	}
	
	// Debug log for task assignment
	if (GEngine && NewTask.Type != EVehicleTaskType::None)
	{
		FString TaskTypeName = UEnum::GetValueAsString(NewTask.Type);
		//UE_LOG(LogTemp, Log, TEXT("Vehicle %s assigned task: %s (Priority %d, Autonomous: %s)"),
		//	*GetName(), *TaskTypeName, NewTask.Priority, NewTask.bAutonomousContinue ? TEXT("Yes") : TEXT("No"));
	}
}

void AVehicleActor::ClearTask()
{
	// Clear the task
	CurrentTask = FVehicleTask(); // Reset to default (None)
	
	// Clear legacy fields for backward compatibility
	bHasAssignment = false;
	bAIAutonomousMode = false;
	bPlayerAutonomousMode = false;
	CurrentTarget = nullptr;
	PrimaryTarget = nullptr;
	TargetResource = nullptr;
	PostMineResource = nullptr;
	DefendedCity = nullptr;
	ActiveClusterID = -1;
	bClusterOnlyMode = false;
	ForcedHostileTeams.Empty();
	
	// Clear movement target
	ClearTarget();
	
	//UE_LOG(LogTemp, Log, TEXT("Vehicle %s task cleared - now idle"), *GetName());
}

bool AVehicleActor::CanPreempt(int32 NewPriority) const
{
	// Lower priority number = higher priority (P1 is highest priority)
	// Can preempt if new task has higher priority (lower number) OR if currently idle
	return (CurrentTask.Type == EVehicleTaskType::None) || (NewPriority < CurrentTask.Priority);
}

// ========== END TASK SYSTEM IMPLEMENTATION ==========

void AVehicleActor::SetTargetResource(AResourceActor* Resource)
{
	if (Resource)
	{
		FVector ResourceLocation = Resource->GetActorLocation();
		FVector VehicleLocation = GetActorLocation();
		float ActualDistance = FVector::Dist(VehicleLocation, ResourceLocation);
		
		if (OwnerTeam == EOwnerTeam::Player)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] SetTargetResource: %s at %.1f units"), *Resource->GetName(), ActualDistance);
		}
		
		// If we're already very close (within 400 units - outside mine radius but within capture range), just set the resource
		if (ActualDistance <= 400.0f)
		{
			if (OwnerTeam == EOwnerTeam::Player)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Already close (%.1f units) - setting TargetResource WITHOUT movement"), ActualDistance);
			}
			TargetResource = Resource;
			bHasTarget = true; // Mark as having target even if not moving - prevents task reassignment loop
			// Don't create movement path - vehicle is already positioned to capture
			return;
		}
		
		// Calculate target position 400 units from resource center (towards vehicle)
		// Use angular offset on the sphere surface to ensure the point is on terrain
		FVector ResourceDir = (ResourceLocation - PlanetCenter).GetSafeNormal();
		FVector VehicleDir = (VehicleLocation - PlanetCenter).GetSafeNormal();
		
		// t = fraction of arc to traverse = desired_arc / total_arc
		float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
		float TotalAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(ResourceDir, VehicleDir), -1.0f, 1.0f));
		float TotalArc = TotalAngle * PlanetR;
		float T = (TotalArc > KINDA_SMALL_NUMBER) ? FMath::Min(400.0f / TotalArc, 1.0f) : 1.0f;
		FVector OffsetDir = FMath::Lerp(ResourceDir, VehicleDir, T).GetSafeNormal();
		
		// Place at actual terrain height so the point is on the surface and on land
		float TerrainHeight = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(OffsetDir) : 0.0f;
		float TerrainRadius = PlanetR * (1.0f + TerrainHeight);
		FVector OffsetTarget = PlanetCenter + OffsetDir * TerrainRadius;
		
		// Navigate to offset position (400 units from resource)
		bool bWasAutonomous = bPlayerAutonomousMode;
		bool bPathCreated = CreatePathTo(OffsetTarget); // Internal path creation - preserve combat state
		bPlayerAutonomousMode = bWasAutonomous;
		
		// Only set TargetResource if pathfinding succeeded
		if (bPathCreated)
		{
			TargetResource = Resource;
			
			if (OwnerTeam == EOwnerTeam::Player)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] SetTargetResource complete: TargetResource=%s, bHasTarget=%s"), 
					*Resource->GetName(), bHasTarget ? TEXT("TRUE") : TEXT("FALSE"));
			}
		}
		// Error already logged and stored by CreatePathTo if pathfinding failed
	}
}

void AVehicleActor::UpdateColor()
{
	if (VehicleMesh)
	{
		// Create dynamic material instance from the current material
		UMaterialInstanceDynamic* DynMaterial = VehicleMesh->CreateDynamicMaterialInstance(0);
		
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
		Color = FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
		break;
	case EOwnerTeam::AI4:
		Color = FLinearColor(0.5f, 0.0f, 1.0f); // Purple
		break;
	case EOwnerTeam::AI5:
		Color = FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
		break;
	case EOwnerTeam::AI6:
		Color = FLinearColor(0.0f, 0.0f, 0.0f); // Black
		break;
	case EOwnerTeam::AI7:
		Color = FLinearColor(0.2f, 0.3f, 0.85f); // Royal blue
		break;
	case EOwnerTeam::AI8:
		Color = FLinearColor(1.0f, 1.0f, 1.0f); // White
		break;
	case EOwnerTeam::AI9:
		Color = FLinearColor(1.0f, 0.0f, 0.5f); // Hot pink
		break;
	case EOwnerTeam::AI10:
		Color = FLinearColor(1.0f, 0.55f, 0.0f); // Orange
		break;
	case EOwnerTeam::AI11:
		Color = FLinearColor(0.0f, 0.7f, 0.5f); // Teal
		break;
	case EOwnerTeam::AI12:
		Color = FLinearColor(0.5f, 1.0f, 0.0f); // Lime
		break;
	case EOwnerTeam::AI13:
		Color = FLinearColor(1.0f, 0.0f, 1.0f); // Magenta
		break;
	case EOwnerTeam::AI14:
		Color = FLinearColor(0.55f, 0.27f, 0.0f); // Bronze
		break;
	case EOwnerTeam::AI15:
		Color = FLinearColor(0.05f, 0.1f, 0.5f); // Navy
		break;
	case EOwnerTeam::AI16:
		Color = FLinearColor(1.0f, 0.75f, 0.0f); // Gold
		break;
	case EOwnerTeam::AI17:
		Color = FLinearColor(0.7f, 0.7f, 0.7f); // Silver
		break;
	case EOwnerTeam::AI18:
		Color = FLinearColor(0.55f, 0.0f, 0.05f); // Crimson
		break;
	case EOwnerTeam::AI19:
		Color = FLinearColor(0.5f, 1.0f, 0.75f); // Mint
		break;
	default:
		Color = FLinearColor(0.5f, 0.5f, 0.5f); // Gray
		break;
	}
	
	// Try multiple common parameter names
	if (DynMaterial)
	{
		DynMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
		DynMaterial->SetVectorParameterValue(FName("Color"), Color);
		DynMaterial->SetVectorParameterValue(FName("Tint"), Color);
	}
	
	// Also set vertex color as fallback
	VehicleMesh->SetVectorParameterValueOnMaterials(FName("VertexColor"), FVector(Color.R, Color.G, Color.B));
	}
}

FVector AVehicleActor::AdjustPositionForCityExclusion(FVector Position) const
{
	// Use the same exclusion radius that FindPath enforces so we never hand it a position
	// that pathfinding would immediately reject (which stalls defenders near their own city).
	const float SafeRadius = 900.0f; // Just above FindPath's 800-unit CityExclusionRadius

	if (!GetWorld()) return Position;

	const TArray<AActor*>& AllCities = CachedAllCities;

	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (!City) continue;
		float Dist = FVector::Dist(Position, City->GetActorLocation());
		if (Dist < SafeRadius)
		{
			// Push position outward from city center to just outside the exclusion zone
			FVector AwayDir = (Position - City->GetActorLocation()).GetSafeNormal();
			if (AwayDir.IsNearlyZero())
			{
				// Fallback: push toward the vehicle itself
				AwayDir = (GetActorLocation() - City->GetActorLocation()).GetSafeNormal();
			}
			Position = City->GetActorLocation() + AwayDir * SafeRadius;
		}
	}
	return Position;
}

void AVehicleActor::MoveTowardsTarget(float DeltaTime)
{
	FVector CurrentLocation = GetActorLocation();
	
	// Handle backup maneuver if stuck
	if (bIsBackingUp)
	{
		// Get current direction from planet center
		FVector CurrentDirection = (CurrentLocation - PlanetCenter).GetSafeNormal();
		FVector BackupDirection = (BackupTargetLocation - PlanetCenter).GetSafeNormal();
		
		// Calculate angle to backup target
		float DotProduct = FVector::DotProduct(CurrentDirection, BackupDirection);
		DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f);
		float AngleToBackup = FMath::Acos(DotProduct);
		
		// Get speed multiplier
		float SpeedMultiplier = 1.0f;
		if (OwnerTeam == EOwnerTeam::Player)
		{
			APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
			if (PlayerController)
			{
				SpeedMultiplier = PlayerController->GetSpeedMultiplier();
			}
		}
		else // AI teams
		{
			// Check AI team's Black Substrate using cached controller
			if (CachedTeamController)
			{
				SpeedMultiplier = (CachedTeamController->BlackSubstrate > 0) ? 1.0f : 0.25f;
			}
		}
		
		// Calculate angular movement with speed multiplier applied
		float AngularSpeed = (MovementSpeed * SpeedMultiplier) / PlanetRadius;
		float AngularStepThisFrame = AngularSpeed * DeltaTime;
		
		// Check if we've reached backup target
		if (AngularStepThisFrame >= AngleToBackup || AngleToBackup < 0.01f)
		{
			// Backup complete - now move perpendicular instead of resuming forward
			bIsBackingUp = false;
			bIsMovingPerpendicular = true;
			PerpendicularProgress = 0.0f;
			
			// Calculate perpendicular direction (90 degrees from current forward)
			FVector ForwardDir = GetActorForwardVector();
			FVector PerpendicularDir = FVector::CrossProduct(CurrentDirection, ForwardDir).GetSafeNormal();
			// Project onto planet surface
			PerpendicularDir = (PerpendicularDir - CurrentDirection * FVector::DotProduct(PerpendicularDir, CurrentDirection)).GetSafeNormal();
			
			// Calculate perpendicular target 1000 units away
			float PerpendicularDistance = 1000.0f;
			float PerpendicularAngularDistance = PerpendicularDistance / PlanetRadius;
			FVector PerpendicularRotationAxis = FVector::CrossProduct(CurrentDirection, PerpendicularDir).GetSafeNormal();
			FQuat PerpendicularRotation(PerpendicularRotationAxis, PerpendicularAngularDistance);
			FVector PerpendicularDirection = PerpendicularRotation.RotateVector(CurrentDirection);
			PerpendicularTargetLocation = PlanetCenter + PerpendicularDirection * PlanetRadius;
			
			// UE_LOG(LogTemp, Warning, TEXT("Backup complete! Now moving perpendicular 1000 units..."));
			return;
		}
		
		// Move toward backup target
		float Alpha = AngularStepThisFrame / FMath::Max(AngleToBackup, 0.001f);
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		FVector NewDirection = FMath::Lerp(CurrentDirection, BackupDirection, Alpha).GetSafeNormal();
		
		FVector NewLocation = PlanetCenter + NewDirection * PlanetRadius;
		SetActorLocation(NewLocation, true);
		
		// Orient vehicle to face backward
		FVector MovementDir = (BackupDirection - CurrentDirection).GetSafeNormal();
		FRotator NewRotation = FRotationMatrix::MakeFromZX(NewDirection, MovementDir).Rotator();
		SetActorRotation(NewRotation);
		
		BackupProgress += AngularStepThisFrame;
		return; // Skip normal movement while backing up
	}
	
	// Handle perpendicular movement after backup
	if (bIsMovingPerpendicular)
	{
		// Get current direction from planet center
		FVector CurrentDirection = (CurrentLocation - PlanetCenter).GetSafeNormal();
		FVector PerpendicularDirection = (PerpendicularTargetLocation - PlanetCenter).GetSafeNormal();
		
		// Calculate angle to perpendicular target
		float DotProduct = FVector::DotProduct(CurrentDirection, PerpendicularDirection);
		DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f);
		float AngleToPerpendicular = FMath::Acos(DotProduct);
		
		// Get speed multiplier
		float SpeedMultiplier = 1.0f;
		if (OwnerTeam == EOwnerTeam::Player)
		{
			APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
			if (PlayerController)
			{
				SpeedMultiplier = PlayerController->GetSpeedMultiplier();
			}
		}
		else // AI teams
		{
			// Check AI team's Black Substrate using cached controller
			if (CachedTeamController)
			{
				SpeedMultiplier = (CachedTeamController->BlackSubstrate > 0) ? 1.0f : 0.25f;
			}
		}
		
		// Calculate angular movement
		float AngularSpeed = (MovementSpeed * SpeedMultiplier) / PlanetRadius;
		float AngularStepThisFrame = AngularSpeed * DeltaTime;
		
		// Check if we've reached perpendicular target
		if (AngularStepThisFrame >= AngleToPerpendicular || AngleToPerpendicular < 0.01f)
		{
			// Perpendicular movement complete - resume normal movement
			bIsMovingPerpendicular = false;
			
			// UE_LOG(LogTemp, Warning, TEXT("Perpendicular movement complete! Resuming normal movement."));
			return;
		}
		
		// Move toward perpendicular target
		float Alpha = AngularStepThisFrame / FMath::Max(AngleToPerpendicular, 0.001f);
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		FVector NewDirection = FMath::Lerp(CurrentDirection, PerpendicularDirection, Alpha).GetSafeNormal();
		
		FVector NewLocation = PlanetCenter + NewDirection * PlanetRadius;
		SetActorLocation(NewLocation, true);
		
		// Orient vehicle to face perpendicular direction
		FVector MovementDir = (PerpendicularDirection - CurrentDirection).GetSafeNormal();
		FRotator NewRotation = FRotationMatrix::MakeFromZX(NewDirection, MovementDir).Rotator();
		SetActorRotation(NewRotation);
		
		PerpendicularProgress += AngularStepThisFrame;
		return; // Skip normal movement while moving perpendicular
	}

	// === WAYPOINT FOLLOWING NAVIGATION ===
	// Validate path exists and has waypoints
	if (CurrentPath.Num() == 0 || CurrentWaypointIndex >= CurrentPath.Num())
	{
		// No valid path - stop moving
		bHasTarget = false;
		return;
	}

	// Get current waypoint
	FVector CurrentWaypoint = CurrentPath[CurrentWaypointIndex];

	// DEBUG: Log waypoint following for player vehicles (throttled to once per second)
	if (OwnerTeam == EOwnerTeam::Player)
	{
		DebugLogTimer -= DeltaTime;
		if (DebugLogTimer <= 0.0f)
		{
			DebugLogTimer = 1.0f; // Reset to 1 second
			FVector VehiclePos = GetActorLocation();
			float DistToWaypoint = FVector::Dist(VehiclePos, CurrentWaypoint);
			UE_LOG(LogTemp, Warning, TEXT("[WAYPOINT DEBUG] Vehicle: (%.1f, %.1f, %.1f) | Target Waypoint [%d/%d]: (%.1f, %.1f, %.1f) | Dist: %.1f"),
				VehiclePos.X, VehiclePos.Y, VehiclePos.Z,
				CurrentWaypointIndex, CurrentPath.Num() - 1,
				CurrentWaypoint.X, CurrentWaypoint.Y, CurrentWaypoint.Z,
				DistToWaypoint);
		}
	}

	// Check if we've reached the current waypoint using angular distance
	FVector ActualPlanetCenter = OwningPlanet->GetActorLocation();
	FVector CurrentDirection = (GetActorLocation() - ActualPlanetCenter).GetSafeNormal();
	FVector WaypointDirection = (CurrentWaypoint - ActualPlanetCenter).GetSafeNormal();
	
	float AngularDistToWaypoint = FMath::Acos(FMath::Clamp(
		FVector::DotProduct(CurrentDirection, WaypointDirection), -1.0f, 1.0f));
	
	float BasePlanetRadius = OwningPlanet->PlanetRadius;
	float WaypointReachThreshold = 20.0f / BasePlanetRadius; // 20 world units in angular distance - smooth interpolation between waypoints
	
	// For the final waypoint (destination), use same threshold but with debug logging
	if (CurrentWaypointIndex == CurrentPath.Num() - 1)
	{
		if (OwnerTeam == EOwnerTeam::Player && DebugLogTimer <= 0.01f)
		{
			float AngularDistInUnits = AngularDistToWaypoint * BasePlanetRadius;
			UE_LOG(LogTemp, Warning, TEXT("[FINAL WAYPOINT CHECK] AngularDist=%.1f units (%.6f rad), Threshold=5.0, Reached=%s"),
				AngularDistInUnits, AngularDistToWaypoint, (AngularDistToWaypoint < WaypointReachThreshold) ? TEXT("YES") : TEXT("NO"));
		}
	}
	
	bool bReachedWaypoint = (AngularDistToWaypoint < WaypointReachThreshold);
	
	if (bReachedWaypoint)
	{
		CurrentWaypointIndex++;
		
		// Check if we've reached the end of the path
		if (CurrentWaypointIndex >= CurrentPath.Num())
		{
			// Path complete - arrival is handled in Tick
			CurrentPath.Empty();
			bHasTarget = false;
			return;
		}
		else
		{
			// Advanced to next waypoint, update current waypoint
			CurrentWaypoint = CurrentPath[CurrentWaypointIndex];
			// Recalculate directions for new waypoint
			WaypointDirection = (CurrentWaypoint - ActualPlanetCenter).GetSafeNormal();
			AngularDistToWaypoint = FMath::Acos(FMath::Clamp(
				FVector::DotProduct(CurrentDirection, WaypointDirection), -1.0f, 1.0f));
			
			// Skip movement this frame to avoid direction jitter when transitioning between waypoints
			return;
		}
	}

	// Calculate movement angle toward current waypoint
	float AngleToWaypoint = AngularDistToWaypoint; // Already calculated in waypoint check above

	// Apply speed multiplier
	float SpeedMultiplier = 1.0f;
	if (OwnerTeam == EOwnerTeam::Player)
	{
		// Use player's speed multiplier from controller
		APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
		if (PlayerController)
		{
			SpeedMultiplier = PlayerController->GetSpeedMultiplier();
		}
	}
	else
	{
		// AI team speed multiplier: use cached controller
		if (CachedTeamController)
		{
			SpeedMultiplier = (CachedTeamController->BlackSubstrate > 0) ? 1.0f : 0.25f;
		}
	}

	// Calculate angular movement
	float AngularSpeed = (MovementSpeed * SpeedMultiplier) / BasePlanetRadius;
	float AngularStepThisFrame = AngularSpeed * DeltaTime;

	// Calculate interpolation alpha based on angular step
	float Alpha = 0.0f;
	if (AngleToWaypoint > 0.001f)
	{
		Alpha = FMath::Clamp(AngularStepThisFrame / AngleToWaypoint, 0.0f, 1.0f);
	}
	else
	{
		Alpha = 1.0f; // Already at waypoint
	}

	// DEBUG: Log movement calculations for player vehicles
	if (OwnerTeam == EOwnerTeam::Player && DebugLogTimer <= 0.01f)
	{
		float CurrentRadialDist = FVector::Dist(GetActorLocation(), ActualPlanetCenter);
		float WaypointRadialDist = FVector::Dist(CurrentWaypoint, ActualPlanetCenter);
		
		UE_LOG(LogTemp, Warning, TEXT("[MOVEMENT CALC] Angle=%.6f rad (%.2f deg), Step=%.6f, Alpha=%.4f, MoveSpeed=%.1f, AngSpeed=%.6f, PlanetR=%.1f"),
			AngleToWaypoint, FMath::RadiansToDegrees(AngleToWaypoint), AngularStepThisFrame, Alpha, MovementSpeed, AngularSpeed, PlanetRadius);
		UE_LOG(LogTemp, Warning, TEXT("[DIRECTION] Curr=(%.3f,%.3f,%.3f), Waypoint=(%.3f,%.3f,%.3f)"),
			CurrentDirection.X, CurrentDirection.Y, CurrentDirection.Z,
			WaypointDirection.X, WaypointDirection.Y, WaypointDirection.Z);
		UE_LOG(LogTemp, Warning, TEXT("[RADIAL DIST] Vehicle=%.1f, Waypoint=%.1f, Diff=%.1f"),
			CurrentRadialDist, WaypointRadialDist, CurrentRadialDist - WaypointRadialDist);
	}

	// Great circle interpolation: rotate CurrentDirection toward WaypointDirection
	FVector NewDirection;
	if (AngleToWaypoint > 0.001f)
	{
		FVector RotationAxis = FVector::CrossProduct(CurrentDirection, WaypointDirection);
		
		if (RotationAxis.IsNearlyZero())
		{
			// Directions are parallel (either same or opposite)
			NewDirection = CurrentDirection;
		}
		else
		{
			RotationAxis.Normalize();
			float RotationAngle = AngleToWaypoint * Alpha;
			FQuat StepRotation(RotationAxis, RotationAngle);
			NewDirection = StepRotation.RotateVector(CurrentDirection);
		}
	}
	else
	{
		NewDirection = WaypointDirection; // Already there
	}
	
	// Calculate terrain height at current position dynamically
	float TerrainHeight = OwningPlanet->CalculateHeightAtPoint(NewDirection);
	float TerrainRadius = BasePlanetRadius * (1.0f + TerrainHeight);
	float HoverHeight = 50.0f; // Hover 50 units above terrain
	float NewRadius = TerrainRadius + HoverHeight;
	
	// DEBUG: Log terrain calculation for player vehicles
	if (OwnerTeam == EOwnerTeam::Player && DebugLogTimer <= 0.01f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TERRAIN CALC] TerrainHeight=%.6f (%.1f units), BasePlanetR=%.1f, TerrainRadius=%.1f, NewRadius=%.1f"),
			TerrainHeight, TerrainHeight * BasePlanetRadius, BasePlanetRadius, TerrainRadius, NewRadius);
	}
	
	// Calculate new position: direction along sphere + terrain height + hover
	FVector NewLocation = OwningPlanet->GetActorLocation() + NewDirection * NewRadius;

	// Move to new location
	SetActorLocation(NewLocation, true);

	// Orient vehicle to face movement direction
	FVector MovementDir = (WaypointDirection - CurrentDirection).GetSafeNormal();
	FRotator NewRotation = FRotationMatrix::MakeFromZX(NewDirection, MovementDir).Rotator();
	SetActorRotation(NewRotation);

	// Mark vehicle as actively moving
	bIsMoving = true;
}

void AVehicleActor::ApplyDamage(float DamageAmount, EOwnerTeam AttackerTeam, AActor* AttackingActor)
{
	// Don't apply damage if already dead
	if (CurrentHealth <= 0.0f)
	{
		return;
	}
	
	// Track last damaging team and actor for relationship updates and defense AI
	LastDamagingTeam = AttackerTeam;
	LastDamagingActor = AttackingActor;
	
	// Update kaiju relationship if damaged by a kaiju
	if (AttackingActor)
	{
		AKaijuActor* AttackingKaiju = Cast<AKaijuActor>(AttackingActor);
		if (AttackingKaiju)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				float CurrentTime = World->GetTimeSeconds();
				int32 TeamID = static_cast<int32>(OwnerTeam);
				AttackingKaiju->UpdateRelationshipForDamage(TeamID, CurrentTime);
			}
		}
	}
	
	CurrentHealth -= DamageAmount;
	CurrentHealth = FMath::Max(CurrentHealth, 0.0f);

	// Reset healing timer (interrupt healing)
	TimeSinceLastDamaged = 0.0f;
	TimeSinceLastHeal = 0.0f;

	// Update health bar visibility and value
	UpdateHealthBar();

	// Die if health reaches 0
	if (CurrentHealth <= 0.0f)
	{
		// Clear target if selected
		SetSelected(false);
		
		// Clear this vehicle from any resources it was capturing (release capture lock)
		if (UWorld* World = GetWorld())
		{
			const TArray<AActor*>& AllResources = CachedAllResources;
			for (AActor* Actor : AllResources)
			{
				if (AResourceActor* Resource = Cast<AResourceActor>(Actor))
				{
					if (Resource->CapturingVehicle == this)
					{
						Resource->CapturingVehicle = nullptr;
						UE_LOG(LogTemp, Log, TEXT("Vehicle destroyed - cleared capture lock on resource '%s'"), *Resource->GetName());
					}
				}
			}
		}
		
		// Notify GameMode of vehicle destruction for relationship update
		if (UWorld* World = GetWorld())
		{
			if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode()))
			{
				GameMode->OnVehicleDestroyed(LastDamagingTeam, OwnerTeam);
			}
		}
		
		// Destroy the actor
		Destroy();
	}
}

void AVehicleActor::UpdateHealthBar()
{
	if (!HealthBarWidget) return;

	// Detect health changes
	if (FMath::Abs(CurrentHealth - PreviousHealth) > 0.01f)
	{
		LastHealthChangeTime = GetWorld()->GetTimeSeconds();
		PreviousHealth = CurrentHealth;
	}

	// Check if health bar should be visible based on time since last change
	float TimeSinceHealthChange = GetWorld()->GetTimeSeconds() - LastHealthChangeTime;
	bool bRecentHealthChange = TimeSinceHealthChange <= 5.0f;

	// Check if currently healing (health below max and near city and past damage delay)
	bool bCurrentlyHealing = (CurrentHealth < MaxHealth && TimeSinceLastDamaged >= HealingStartDelay);

	// Show health bar if recently changed or currently healing
	bool bShouldShowHealthBar = (bRecentHealthChange || bCurrentlyHealing);

	// Check if vehicle is behind the planet from camera view
	if (bShouldShowHealthBar)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			FVector VehicleLocation = GetActorLocation();
			
			// Vector from planet center to vehicle
			// Check if vehicle is blocked by planet using raycast
			FHitResult VisibilityHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			
			bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
				VisibilityHit,
				CameraLocation,
				VehicleLocation,
				ECC_Visibility,
				QueryParams
			);
			
			// Vehicle is blocked if we hit something BEFORE reaching it
			float DistanceToVehicle = FVector::Dist(CameraLocation, VehicleLocation);
			float DistanceToHit = VisibilityHit.Distance;
			if (bHitSomething && (DistanceToHit < DistanceToVehicle - 100.0f))
			{
				bShouldShowHealthBar = false; // Hide if behind planet
			}
		}
	}

	HealthBarWidget->SetVisibility(bShouldShowHealthBar);

	// Update the widget's health percentage
	if (UHealthBarWidget* HealthWidget = Cast<UHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
	{
		float HealthPercent = CurrentHealth / MaxHealth;
		HealthWidget->SetHealthPercent(HealthPercent);
	}
}

void AVehicleActor::UpdateInfoDisplay()
{
	if (!InfoWidget) return;
	
	UInfoUIWidget* InfoUIWidget = Cast<UInfoUIWidget>(InfoWidget->GetUserWidgetObject());
	if (!InfoUIWidget) return;
	
	// Line 1: Vehicle type
	FText Line1 = FText::FromString(TEXT("Vehicle"));
	
	// Line 2: Team name
	FText Line2;
	if (OwnerTeam == EOwnerTeam::Player)
	{
		Line2 = FText::FromString(TEXT("Player"));
	}
	else if (OwnerTeam == EOwnerTeam::Neutral)
	{
		Line2 = FText::FromString(TEXT("Neutral"));
	}
	else
	{
		// Neutral=0, Player=1, AI1=2, AI2=3 ... so enum value = display team number
		int32 TeamNumber = (int32)OwnerTeam;
		Line2 = FText::FromString(FString::Printf(TEXT("Team %d"), TeamNumber));
	}
	
	// Line 3: Empty
	FText Line3 = FText::GetEmpty();
	
	InfoUIWidget->SetInfoDisplay(Line1, Line2, Line3);
}

FVector AVehicleActor::CalculateFormationPosition(ACityActor* TargetCity, FVector ApproachDirection)
{
	if (!TargetCity) return GetActorLocation();

	FVector CityLocation = TargetCity->GetActorLocation();
	FVector CurrentLocation = GetActorLocation();

	// Calculate approach angle (0-360 degrees around city)
	FVector ToCity = (CityLocation - CurrentLocation).GetSafeNormal();
	FVector CityUp = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Project approach direction onto city's tangent plane
	FVector TangentApproach = (ToCity - CityUp * FVector::DotProduct(ToCity, CityUp)).GetSafeNormal();
	
	// Get a reference direction (north on city's surface)
	FVector CityNorth = FVector::CrossProduct(CityUp, FVector::RightVector).GetSafeNormal();
	if (CityNorth.IsNearlyZero())
	{
		CityNorth = FVector::CrossProduct(CityUp, FVector::ForwardVector).GetSafeNormal();
	}
	
	// Calculate angle from north
	float DotNorth = FVector::DotProduct(TangentApproach, CityNorth);
	FVector Cross = FVector::CrossProduct(CityNorth, TangentApproach);
	float CrossDot = FVector::DotProduct(Cross, CityUp);
	float ApproachAngle = FMath::Atan2(CrossDot, DotNorth);
	if (ApproachAngle < 0) ApproachAngle += 2.0f * PI;

	// Count existing attackers near the city
	const TArray<AActor*>& AllVehicles = CachedAllVehicles;
	
	// Organize attackers by angle sectors (8 sectors of 45 degrees each)
	const int32 NumSectors = 8;
	const float SectorAngle = (2.0f * PI) / NumSectors;
	TArray<int32> VehiclesPerSector;
	VehiclesPerSector.SetNum(NumSectors);
	
	for (AActor* Actor : AllVehicles)
	{
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (Vehicle && Vehicle != this && Vehicle->OwnerTeam == OwnerTeam)
		{
			float DistToCity = FVector::Dist(Vehicle->GetActorLocation(), CityLocation);
			// Count vehicles positioned for attack (800-1500 units from city)
			if (DistToCity > 800.0f && DistToCity < 1500.0f)
			{
				// Calculate this vehicle's angle
				FVector ToVehicle = (Vehicle->GetActorLocation() - CityLocation).GetSafeNormal();
				FVector TangentToVehicle = (ToVehicle - CityUp * FVector::DotProduct(ToVehicle, CityUp)).GetSafeNormal();
				
				float VehicleDot = FVector::DotProduct(TangentToVehicle, CityNorth);
				FVector VehicleCross = FVector::CrossProduct(CityNorth, TangentToVehicle);
				float VehicleCrossDot = FVector::DotProduct(VehicleCross, CityUp);
				float VehicleAngle = FMath::Atan2(VehicleCrossDot, VehicleDot);
				if (VehicleAngle < 0) VehicleAngle += 2.0f * PI;
				
				int32 SectorIndex = FMath::FloorToInt(VehicleAngle / SectorAngle) % NumSectors;
				VehiclesPerSector[SectorIndex]++;
			}
		}
	}
	
	// Determine which sector we're approaching from
	int32 MySector = FMath::FloorToInt(ApproachAngle / SectorAngle) % NumSectors;
	
	// Find least crowded adjacent sector (prefer our sector, then neighbors)
	int32 BestSector = MySector;
	int32 LowestCount = VehiclesPerSector[MySector];
	
	// Check adjacent sectors
	for (int32 Offset = 1; Offset <= 2; Offset++)
	{
		int32 LeftSector = (MySector - Offset + NumSectors) % NumSectors;
		int32 RightSector = (MySector + Offset) % NumSectors;
		
		if (VehiclesPerSector[LeftSector] < LowestCount)
		{
			BestSector = LeftSector;
			LowestCount = VehiclesPerSector[LeftSector];
		}
		if (VehiclesPerSector[RightSector] < LowestCount)
		{
			BestSector = RightSector;
			LowestCount = VehiclesPerSector[RightSector];
		}
	}
	
	// Calculate position within sector using triangular grid
	// Vehicles are arranged in rows where each row forms triangles with the previous row
	int32 PositionInSector = VehiclesPerSector[BestSector];
	
	const float VehicleSpacing = 250.0f; // Lateral spacing between vehicles in same row
	const float RowDepth = VehicleSpacing * 0.866f; // sqrt(3)/2 for equilateral triangles ≈ 217 units
	const int32 VehiclesPerRow = 5; // Number of vehicles across each row in this sector
	
	int32 Row = PositionInSector / VehiclesPerRow;
	int32 SlotInRow = PositionInSector % VehiclesPerRow;
	
	// Calculate radial distance from city (each row is deeper)
	float BaseDistance = 1000.0f + (Row * RowDepth);
	
	// Calculate lateral position within sector
	float SectorCenterAngle = BestSector * SectorAngle + (SectorAngle * 0.5f);
	
	// Stagger odd/even rows for triangular pattern
	// Even rows (0, 2, 4...): vehicles centered at slots 0, 1, 2, 3, 4
	// Odd rows (1, 3, 5...): vehicles offset by half spacing, forming triangles with row above
	float LateralOffset;
	if (Row % 2 == 0)
	{
		// Even row: standard spacing centered on sector
		LateralOffset = (SlotInRow - (VehiclesPerRow - 1) * 0.5f) * VehicleSpacing;
	}
	else
	{
		// Odd row: offset by half spacing to form triangles
		LateralOffset = (SlotInRow - (VehiclesPerRow - 1) * 0.5f) * VehicleSpacing + (VehicleSpacing * 0.5f);
	}
	
	// Convert lateral offset to angular offset
	// At distance BaseDistance, arc length = angle * radius, so angle = arc / radius
	float AngularOffset = LateralOffset / BaseDistance;
	float FinalAngle = SectorCenterAngle + AngularOffset;
	
	// Convert angle to position on planet surface
	FVector LocalNorth = CityNorth;
	FVector LocalEast = FVector::CrossProduct(CityUp, LocalNorth).GetSafeNormal();
	
	FVector OffsetDirection = (LocalNorth * FMath::Cos(FinalAngle) + LocalEast * FMath::Sin(FinalAngle)).GetSafeNormal();
	FVector FormationPosition = CityLocation + OffsetDirection * BaseDistance;
	
	// Project onto planet surface
	FVector DirectionFromPlanet = (FormationPosition - PlanetCenter).GetSafeNormal();
	FormationPosition = PlanetCenter + DirectionFromPlanet * PlanetRadius;
	
	return FormationPosition;
}

ACityActor* AVehicleActor::DeterminePlayerLoyalCity()
{
	// Find the closest player-owned city
	const TArray<AActor*>& AllCities = CachedAllCities;

	ACityActor* ClosestCity = nullptr;
	float ClosestDistance = FLT_MAX;

	FVector VehicleLocation = GetActorLocation();

	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == EOwnerTeam::Player)
		{
			float Distance = FVector::Dist(VehicleLocation, City->GetActorLocation());
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestCity = City;
			}
		}
	}

	return ClosestCity;
}

ACityActor* AVehicleActor::DetermineAILoyalCity()
{
	// Find the closest city owned by the same team as this vehicle
	const TArray<AActor*>& AllCities = CachedAllCities;

	ACityActor* ClosestCity = nullptr;
	float ClosestDistance = FLT_MAX;

	FVector VehicleLocation = GetActorLocation();

	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == OwnerTeam)
		{
			float Distance = FVector::Dist(VehicleLocation, City->GetActorLocation());
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestCity = City;
			}
		}
	}

	return ClosestCity;
}

AResourceActor* AVehicleActor::FindBestPlayerResourceToCapture(ACityActor* NearCity)
{
	if (!NearCity)
	{
		return nullptr;
	}

	const TArray<AActor*>& AllResources = CachedAllResources;

	// Get all player vehicles to check which resources are already targeted
	const TArray<AActor*>& AllVehicles = CachedAllVehicles;

	// Get player's cities to determine proximal resources (within territory radius)
	const TArray<AActor*>& AllCities = CachedAllCities;
	
	// Get only player cities
	TArray<ACityActor*> PlayerCities;
	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (!City) continue;
		
		if (City->OwnerTeam == EOwnerTeam::Player)
		{
			PlayerCities.Add(City);
		}
	}
	
	// Get territory radius from GameMode
	float TerritoryRadius = 5000.0f; // Default
	if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode()))
	{
		TerritoryRadius = GameMode->TERRITORY_RADIUS;
	}

	UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] FindBestPlayerResourceToCapture: %d total resources, %d player cities, territory: %.0f"),
		AllResources.Num(), PlayerCities.Num(), TerritoryRadius);

	AResourceActor* BestResource = nullptr;
	float BestScore = FLT_MAX;

	FVector VehicleLocation = GetActorLocation();
	FVector CityLocation = NearCity->GetActorLocation();

	// Weights for composite scoring (60/40 vehicle to city range)
	const float CityDistanceWeight = 0.4f;
	const float VehicleDistanceWeight = 0.6f;

	int32 FilteredOwned = 0;
	int32 FilteredTeam = 0;
	int32 FilteredNonProximal = 0;
	int32 FilteredTargeted = 0;
	int32 ValidResources = 0;

	for (AActor* Actor : AllResources)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource)
		{
			continue;
		}

		// Skip if already owned by player
		if (Resource->OwnerTeam == EOwnerTeam::Player)
		{
			FilteredOwned++;
			continue;
		}

		// Filter based on targeting mode
		if (bTargetNeutralOnly)
		{
			// Only target neutral resources
			if (Resource->OwnerTeam != EOwnerTeam::Neutral)
			{
				FilteredTeam++;
				continue;
			}
		}
		else
		{
			// Target neutral AND specific enemy team resources
			if (Resource->OwnerTeam != EOwnerTeam::Neutral && Resource->OwnerTeam != TargetEnemyTeam)
			{
				FilteredTeam++;
				continue;
			}
		}
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != EOwnerTeam::Player)
		{
			FilteredTargeted++;
			continue;
		}

		// Continent filter: skip resources on a different continent from our city
		if (Resource->ContinentID >= 0 && NearCity->ContinentID >= 0 &&
			Resource->ContinentID != NearCity->ContinentID)
		{
			continue;
		}
		
		// PROXIMAL CHECK: Only target resources within territory radius of player cities
		FVector ResourceLocation = Resource->GetActorLocation();
		
		// Check if resource is within territory radius of ANY player city
		bool bWithinTerritory = false;
		for (ACityActor* City : PlayerCities)
		{
			float Distance = FVector::Dist(City->GetActorLocation(), ResourceLocation);
			if (Distance <= TerritoryRadius)
			{
				bWithinTerritory = true;
				break;
			}
		}
		
		// Skip resources outside our territory
		if (!bWithinTerritory)
		{
			FilteredNonProximal++;
			continue;
		}

		bool bAlreadyTargeted = false;
		for (AActor* VehicleActor : AllVehicles)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(VehicleActor);
			if (Vehicle && Vehicle != this && Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				if (Vehicle->TargetResource == Resource)
				{
					bAlreadyTargeted = true;
					break;
				}
			}
		}

		if (bAlreadyTargeted)
		{
			FilteredTargeted++;
			continue;
		}

		// Calculate composite score (lower is better)
		float DistanceFromCity = FVector::Dist(CityLocation, ResourceLocation);
		float DistanceFromVehicle = FVector::Dist(VehicleLocation, ResourceLocation);
		float Score = (CityDistanceWeight * DistanceFromCity) + (VehicleDistanceWeight * DistanceFromVehicle);

		ValidResources++;

		if (Score < BestScore)
		{
			BestScore = Score;
			BestResource = Resource;
		}
	}

	return BestResource;
}

AResourceActor* AVehicleActor::FindBestAIResourceToCapture(ACityActor* NearCity)
{
	if (!NearCity)
	{
		return nullptr;
	}

	const TArray<AActor*>& AllResources = CachedAllResources;

	// Get all AI vehicles to check which resources are already targeted
	const TArray<AActor*>& AllVehicles = CachedAllVehicles;

	// Get our team's cities to determine proximal resources (within territory radius)
	const TArray<AActor*>& AllCities = CachedAllCities;
	
	// Get only our team's cities
	TArray<ACityActor*> OurCities;
	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (!City) continue;
		
		if (City->OwnerTeam == OwnerTeam)
		{
			OurCities.Add(City);
		}
	}
	
	// Get territory radius from GameMode
	// Use AIAutonomousSearchRadius if in autonomous mode (set by AI controller), otherwise use base territory radius
	float TerritoryRadius = bAIAutonomousMode ? AIAutonomousSearchRadius : 5000.0f;
	if (!bAIAutonomousMode)
	{
		if (APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode()))
		{
			TerritoryRadius = GameMode->TERRITORY_RADIUS;
		}
	}

	AResourceActor* BestResource = nullptr;
	float BestScore = FLT_MAX;

	FVector VehicleLocation = GetActorLocation();
	FVector CityLocation = NearCity->GetActorLocation();

	// Weights for composite scoring (60/40 vehicle to city range)
	const float CityDistanceWeight = 0.4f;
	const float VehicleDistanceWeight = 0.6f;

	int32 FilteredOwned = 0;
	int32 FilteredNonProximal = 0;
	int32 FilteredTargeted = 0;
	int32 ValidResources = 0;

	for (AActor* Actor : AllResources)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource)
		{
			continue;
		}

		// Skip if already owned by our team
		if (Resource->OwnerTeam == OwnerTeam)
		{
			FilteredOwned++;
			continue;
		}
		
		// AI AUTONOMOUS MODE: Only target neutral (unclaimed) resources
		// Enemy-owned resources should only be targeted through explicit AI strategy (mine attacks)
		if (Resource->OwnerTeam != EOwnerTeam::Neutral)
		{
			continue;
		}
		
		// Filter Kaiju-guarded resources based on sub-priority
		// P2.1 (Easy Income): Skip Kaiju-guarded resources
		// P2.4 (Kaiju Clusters): ONLY target Kaiju-guarded resources
		if (CurrentTask.SubPriorityLabel == TEXT("P2.1"))
		{
			if (Resource->bKaijuGuarded) continue; // P2.1 avoids Kaiju
		}
		else if (CurrentTask.SubPriorityLabel == TEXT("P2.4"))
		{
			if (!Resource->bKaijuGuarded) continue; // P2.4 ONLY targets Kaiju-guarded
		}
		// P2.2 and P2.3 don't use autonomous continue for resources, so no filter needed
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != OwnerTeam)
		{
			FilteredTargeted++;
			continue;
		}

		// Continent filter: skip resources on a different continent from our city
		if (Resource->ContinentID >= 0 && NearCity->ContinentID >= 0 &&
			Resource->ContinentID != NearCity->ContinentID)
		{
			continue;
		}

		// PROXIMAL CHECK: Only target resources within territory radius of our cities
		FVector ResourceLocation = Resource->GetActorLocation();
		
		// Check if resource is within territory radius of ANY of our cities
		bool bWithinTerritory = false;
	float ClosestOwnCityDistance = FLT_MAX;
	for (ACityActor* City : OurCities)
	{
		float Distance = FVector::Dist(City->GetActorLocation(), ResourceLocation);
		if (Distance < ClosestOwnCityDistance)
		{
			ClosestOwnCityDistance = Distance;
		}
		if (Distance <= TerritoryRadius)
		{
			bWithinTerritory = true;
		}
	}
	
	// Skip resources outside our territory
	if (!bWithinTerritory)
	{
		FilteredNonProximal++;
		continue;
	}
	
	// PRIORITY CHECK: Deprioritize resources that are closer to enemy cities than our own cities
	// This prevents AI from targeting resources in contested/overlapping territories
	// Find closest enemy city
	float ClosestEnemyCityDistance = FLT_MAX;
	for (AActor* CityActor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(CityActor);
		if (!City || City->OwnerTeam == OwnerTeam || City->OwnerTeam == EOwnerTeam::Neutral) continue;
		
		float Distance = FVector::Dist(City->GetActorLocation(), ResourceLocation);
		if (Distance < ClosestEnemyCityDistance)
		{
			ClosestEnemyCityDistance = Distance;
		}
	}
	
	// If resource is closer to enemy city than our city, heavily penalize it
	bool bCloserToEnemy = (ClosestEnemyCityDistance < ClosestOwnCityDistance);
	float TerritoryPenalty = bCloserToEnemy ? 10000.0f : 0.0f;

		// Check if already targeted by another vehicle on our team
		bool bAlreadyTargeted = false;
		for (AActor* VehicleActor : AllVehicles)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(VehicleActor);
			if (Vehicle && Vehicle != this && Vehicle->OwnerTeam == OwnerTeam)
			{
				if (Vehicle->TargetResource == Resource)
				{
					bAlreadyTargeted = true;
					break;
				}
			}
		}

		if (bAlreadyTargeted)
		{
			FilteredTargeted++;
			continue;
		}

		// Calculate composite score (lower is better)
		float DistanceFromCity = FVector::Dist(CityLocation, ResourceLocation);
		float DistanceFromVehicle = FVector::Dist(VehicleLocation, ResourceLocation);
		float Score = (CityDistanceWeight * DistanceFromCity) + (VehicleDistanceWeight * DistanceFromVehicle) + TerritoryPenalty;

		ValidResources++;

		if (Score < BestScore)
		{
			BestScore = Score;
			BestResource = Resource;
		}
	}

	// Log resource targeting decision
	if (BestResource)
	{
		float DistToClosestCity = FLT_MAX;
		for (ACityActor* City : OurCities)
		{
			float Dist = FVector::Dist(City->GetActorLocation(), BestResource->GetActorLocation());
			if (Dist < DistToClosestCity) DistToClosestCity = Dist;
		}
		
		float DistToVehicle = FVector::Dist(VehicleLocation, BestResource->GetActorLocation());
		
		//UE_LOG(LogTemp, Warning, TEXT("AI Team %d Vehicle: Targeting resource %s - Distance from closest city: %.0f, from vehicle: %.0f, Score: %.1f (SearchRadius: %.0f)"),
			//(int32)OwnerTeam, *BestResource->GetName(), DistToClosestCity, DistToVehicle, BestScore, TerritoryRadius);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d Vehicle: No valid resource found (Filtered: %d owned, %d non-proximal, %d targeted, Valid: %d)"),
			(int32)OwnerTeam, FilteredOwned, FilteredNonProximal, FilteredTargeted, ValidResources);
	}

	return BestResource;
}

AResourceActor* AVehicleActor::FindClusterResource(int32 ClusterID)
{
	// Find an uncaptured resource with the specified ClusterID
	// Much simpler than distance-based detection!
	
	//UE_LOG(LogTemp, Warning, TEXT("[CLUSTER SEARCH] Looking for resources in cluster %d"), ClusterID);
	
	const TArray<AActor*>& AllResources = CachedAllResources;
	
	// Get all vehicles to check which resources are already targeted
	const TArray<AActor*>& AllVehicles = CachedAllVehicles;
	
	AResourceActor* BestResource = nullptr;
	float ClosestDistance = FLT_MAX;
	int TotalInCluster = 0;
	int SkippedOwned = 0;
	int SkippedTargeted = 0;
	
	for (AActor* Actor : AllResources)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource) continue;
		
		// Skip resources not in this cluster
		if (Resource->ClusterID != ClusterID) continue;
		
		TotalInCluster++;
		
		//Skip if already owned by us
		if (Resource->OwnerTeam == OwnerTeam)
		{
			SkippedOwned++;
			continue;
		}
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != OwnerTeam)
		{
			SkippedTargeted++;
			continue;
		}
		
		// Check if already targeted by another vehicle on our team
		bool bAlreadyTargeted = false;
		for (AActor* VehicleActor : AllVehicles)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(VehicleActor);
			if (Vehicle && Vehicle != this && Vehicle->OwnerTeam == OwnerTeam)
			{
				if (Vehicle->TargetResource == Resource)
				{
					bAlreadyTargeted = true;
					break;
				}
			}
		}
		
		if (bAlreadyTargeted)
		{
			SkippedTargeted++;
			continue;
		}
		
		// Pick the closest uncaptured resource in this cluster
		float Distance = FVector::Dist(GetActorLocation(), Resource->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			BestResource = Resource;
		}
	}
	
	//UE_LOG(LogTemp, Warning, TEXT("[CLUSTER SEARCH] Cluster %d has %d resources: Owned=%d, Targeted=%d"), 
	//	ClusterID, TotalInCluster, SkippedOwned, SkippedTargeted);
	
	if (BestResource)
	{
		const TCHAR* TypeName = (BestResource->ResourceType == EResourceType::BlackSubstrate) ? TEXT("BLACK") : TEXT("ORANGE");
		//UE_LOG(LogTemp, Warning, TEXT("[CLUSTER SEARCH] FOUND %s resource %s at %.0f units away"), 
		//	TypeName, *BestResource->GetName(), ClosestDistance);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CLUSTER SEARCH] NO uncaptured resources left in cluster %d"), ClusterID);
	}
	
	return BestResource;
}

void AVehicleActor::FindNextTarget()
{
	// Handle player autonomous mode separately
	if (bPlayerAutonomousMode && OwnerTeam == EOwnerTeam::Player)
	{
		// Don't interrupt if we already have an active target (check both bHasTarget AND CurrentTarget)
		// bHasTarget can be false while CurrentTarget is set (e.g., attacking building/mine within range)
		if (bHasTarget || TargetResource || CurrentTarget)
		{
			return;
		}

		// PRIORITY 0: PostMineResource - Capture resource after destroying its mine
		if (PostMineResource && IsValid(PostMineResource))
		{
			if (bCanCaptureResources)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Capturing resource after mine destruction"));
				SetTargetResource(PostMineResource);
			}
			PostMineResource = nullptr;
			// Continue autonomous mode to find next mine after capturing this resource
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] FindNextTarget: Looking for next target..."));

		// Update loyal city (may have changed if we're closer to another player city now)
		LoyalCity = DeterminePlayerLoyalCity();

		if (!LoyalCity)
		{
			// No player cities remaining, disable autonomous mode
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] No loyal city found - DISABLING autonomous mode"));
			bPlayerAutonomousMode = false;
			return;
		}

		// PRIORITY 1: Check if we're in mine destruction mode (have ForcedHostileTeams set)
		if (ForcedHostileTeams.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Looking for enemy mines to destroy..."));
			
			// Find all mines owned by hostile teams
			const TArray<AActor*>& AllMines = CachedAllMines;
			
			TArray<AMineActor*> TargetMines;
			for (AActor* Actor : AllMines)
			{
				AMineActor* Mine = Cast<AMineActor>(Actor);
				if (Mine && ForcedHostileTeams.Contains(Mine->OwnerTeam))
				{
					float DistanceToMine = FVector::Dist(GetActorLocation(), Mine->GetActorLocation());
					
					// Filter by territory or cluster
					if (bClusterOnlyMode && ActiveClusterID >= 0)
					{
						// Cluster mode: only attack mines in this cluster
						if (Mine->TargetResource && Mine->TargetResource->ClusterID == ActiveClusterID)
						{
							TargetMines.Add(Mine);
						}
					}
					else
					{
						// Territory mode: only attack mines within territory radius
						float DistanceFromCity = FVector::Dist(LoyalCity->GetActorLocation(), Mine->GetActorLocation());
						if (DistanceFromCity <= 5000.0f) // TERRITORY_RADIUS
						{
							TargetMines.Add(Mine);
						}
					}
				}
			}
			
			// Find the closest mine
			if (TargetMines.Num() > 0)
			{
				AMineActor* ClosestMine = nullptr;
				float ClosestDistance = FLT_MAX;
				
				for (AMineActor* Mine : TargetMines)
				{
					float Distance = FVector::Dist(GetActorLocation(), Mine->GetActorLocation());
					if (Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						ClosestMine = Mine;
					}
				}
				
				if (ClosestMine)
				{
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Found next mine to attack: %s"), *ClosestMine->GetName());
				SetTargetLocation(ClosestMine->GetActorLocation());
				CurrentTarget = ClosestMine; // Set AFTER SetTargetLocation
				bHasTarget = true;
				return;
			}
		}
		
		// No more mines in the area - now look for unclaimed resources
		UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] All enemy mines destroyed - now looking for unclaimed resources..."));
			AResourceActor* NextResource = nullptr;
			if (bClusterOnlyMode && ActiveClusterID >= 0)
			{
				// Look for unclaimed resources in this cluster
				NextResource = FindClusterResource(ActiveClusterID);
				if (NextResource)
				{
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Found unclaimed resource in cluster %d: %s"), ActiveClusterID, *NextResource->GetName());
					SetTargetResource(NextResource);
					return;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Cluster %d complete (mines + resources) - DISABLING autonomous mode"), ActiveClusterID);
				}
			}
			else if (!bClusterOnlyMode)
			{
				// Look for unclaimed resources in territory
				NextResource = FindBestPlayerResourceToCapture(LoyalCity);
				if (NextResource)
				{
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Found unclaimed resource in territory: %s"), *NextResource->GetName());
					SetTargetResource(NextResource);
					return;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Territory complete (mines + resources) - DISABLING autonomous mode"));
				}
			}
			
			// No more resources found, disable autonomous mode
			bPlayerAutonomousMode = false;
			bClusterOnlyMode = false;
			return;
		}

		// PRIORITY 2: CLUSTER PRIORITY - If we're tracking a cluster, find next resource in same cluster first
		AResourceActor* NextResource = nullptr;
		if (ActiveClusterID >= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Searching for more resources in cluster %d..."), ActiveClusterID);
			NextResource = FindClusterResource(ActiveClusterID);
			if (NextResource)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Found cluster resource: %s - continuing cluster %d capture"), *NextResource->GetName(), ActiveClusterID);
			}
			else
			{
				// No more resources in cluster, clear the cluster ID
				UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Cluster %d complete!"), ActiveClusterID);
				ActiveClusterID = -1;
				
				if (bClusterOnlyMode)
				{
					// Cluster-only mode: Don't switch to proximal mode, just stop
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Cluster-only mode - DISABLING autonomous mode"));
					bPlayerAutonomousMode = false;
					bClusterOnlyMode = false;
					return;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Searching for proximal resources..."));
				}
			}
		}
		
		// If no cluster resource found and NOT in cluster-only mode, search for normal proximal resources
		if (!NextResource && !bClusterOnlyMode)
		{
			NextResource = FindBestPlayerResourceToCapture(LoyalCity);
		}
		
		if (NextResource)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] Found next resource: %s - setting target"), *NextResource->GetName());
			SetTargetResource(NextResource);
		}
		else
		{
			// No more resources available in range, idle at current position
			UE_LOG(LogTemp, Warning, TEXT("[PLAYER VEHICLE] No resources found - DISABLING autonomous mode"));
			bPlayerAutonomousMode = false;
			bClusterOnlyMode = false;
		}

		return;
	}

	// AI autonomous targeting
	if (bAIAutonomousMode && OwnerTeam != EOwnerTeam::Player)
	{
		// Don't interrupt if we already have an active target (check both bHasTarget AND CurrentTarget)
		// bHasTarget can be false while CurrentTarget is set (e.g., attacking building within range)
		if (bHasTarget || TargetResource || CurrentTarget)
		{
			return;
		}
		
		// PRIORITY 0: PostMineResource - Capture resource after destroying its mine
		if (PostMineResource && IsValid(PostMineResource))
		{
			if (bCanCaptureResources)
			{
				UE_LOG(LogTemp, Warning, TEXT("[AI VEHICLE] Capturing resource after mine destruction"));
				SetTargetResource(PostMineResource);
				// Disable autonomous mode so vehicle becomes idle after capture and can be reassigned to next mine
				bAIAutonomousMode = false;
			}
			PostMineResource = nullptr;
			return;
		}

		// Update loyal city (may have changed if we're closer to another AI city now)
		LoyalCity = DetermineAILoyalCity();

		if (!LoyalCity)
		{
			// No AI cities remaining, disable autonomous mode
			UE_LOG(LogTemp, Warning, TEXT("[AI VEHICLE] No loyal city found - DISABLING autonomous mode"));
			bAIAutonomousMode = false;
			return;
		}

		// CLUSTER PRIORITY: If we're tracking a cluster, find next resource in same cluster first
		AResourceActor* NextResource = nullptr;
		if (ActiveClusterID >= 0)
		{
			NextResource = FindClusterResource(ActiveClusterID);
			if (!NextResource)
			{
				// No more resources in cluster, clear the cluster ID
				ActiveClusterID = -1;
			}
		}
		
		// If no cluster resource found, search for normal proximal resources
		if (!NextResource)
		{
			NextResource = FindBestAIResourceToCapture(LoyalCity);
		}
		
		if (NextResource)
		{
			SetTargetResource(NextResource);
		}
		else
		{
			// No more resources available in range - clear task and become idle
			UE_LOG(LogTemp, Log, TEXT("AI Vehicle %s: No more resources in radius %.0f - clearing task"),
				*GetName(), AIAutonomousSearchRadius);
			bAIAutonomousMode = false;
			ClearTask(); // Clear task so vehicle becomes truly idle
		}

		return;
	}

	// Old assignment system (deprecated)
	if (!bHasAssignment || !AIController)
	{
		return;
	}

	// Don't interrupt if we already have an active target
	if (bHasTarget || TargetResource)
	{
		return;
	}
}

void AVehicleActor::FindNextAITaskTarget()
{
	// Task-based autonomous targeting for AI vehicles
	// Uses CurrentTask parameters to find next valid target within mission constraints
	
	if (!GetWorld()) return;
	
	// Verify this is an AI vehicle with a valid task
	if (OwnerTeam == EOwnerTeam::Player || CurrentTask.Type != EVehicleTaskType::SecureIncome)
	{
		UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: FindNextAITaskTarget called but not AI SecureIncome task"), *GetName());
		return;
	}
	
	// Get the AI controller's cities to calculate distances
	AAITeamController* TaskAssigner = Cast<AAITeamController>(CurrentTask.AssigningController.Get());
	if (!TaskAssigner)
	{
		UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: No assigning controller found"), *GetName());
		ClearTask();
		return;
	}
	
	const TArray<AActor*>& AllCities = CachedAllCities;
	
	// Get our team's cities for distance calculations
	TArray<ACityActor*> OurCities;
	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == OwnerTeam)
		{
			OurCities.Add(City);
		}
	}
	
	if (OurCities.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: No cities remaining"), *GetName());
		ClearTask();
		return;
	}
	
	// Helper: Get closest city distance
	auto GetClosestCityDistance = [&](FVector Location) -> float
	{
		float MinDist = FLT_MAX;
		for (ACityActor* City : OurCities)
		{
			float Dist = FVector::Dist(City->GetActorLocation(), Location);
			if (Dist < MinDist) MinDist = Dist;
		}
		return MinDist;
	};
	
	FVector VehicleLocation = GetActorLocation();
	
	// Get all vehicles to check if resources are already targeted by teammates
	const TArray<AActor*>& AllVehicles = CachedAllVehicles;
	
	// OPTION 1: Find mines to destroy (if MinesToDestroy is set)
	if (CurrentTask.MinesToDestroy.Num() > 0)
	{
		const TArray<AActor*>& AllMines = CachedAllMines;
		
		// Collect valid mine+resource combos sorted by distance to vehicle
		TArray<TPair<float, TPair<AMineActor*, AResourceActor*>>> ValidMines;
		
		for (AActor* Actor : AllMines)
		{
			AMineActor* Mine = Cast<AMineActor>(Actor);
			if (!Mine || !Mine->TargetResource) continue;
			
			// Must be owned by a team we're allowed to attack
			if (!CurrentTask.MinesToDestroy.Contains(Mine->OwnerTeam)) continue;

			// Never autonomously capture green substrate (no income value to the AI)
			if (Mine->TargetResource->ResourceType == EResourceType::GreenSubstrate) continue;
			
			// Must be within SearchRadius from closest city
			float DistToCity = GetClosestCityDistance(Mine->GetActorLocation());
			if (DistToCity > CurrentTask.SearchRadius) continue;
			
			// Check if target resource is valid
			AResourceActor* Resource = Mine->TargetResource;
			if (!Resource) continue;
			
			// Skip if resource is being captured by another team
			if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != OwnerTeam)
				continue;
			
			// Skip if resource already owned by us
			if (Resource->OwnerTeam == OwnerTeam)
				continue;
			
			// Skip if another teammate vehicle is already targeting this resource
			bool bAlreadyTargeted = false;
			for (AActor* VehicleActor : AllVehicles)
			{
				AVehicleActor* Vehicle = Cast<AVehicleActor>(VehicleActor);
				if (Vehicle && Vehicle != this && Vehicle->OwnerTeam == OwnerTeam)
				{
					if (Vehicle->TargetResource == Resource)
					{
						bAlreadyTargeted = true;
						break;
					}
				}
			}
			
			if (bAlreadyTargeted)
				continue;
			
			// For P2.3 Mine Attack: Check substrate type filtering
			// If task was assigned with substrate filtering, respect it
			// (AI controller sets this based on income needs)
			// We'll rely on the AI controller to only assign substrate-appropriate tasks
			
			// Calculate distance to vehicle (for sorting)
			float DistToVehicle = FVector::Dist(VehicleLocation, Mine->GetActorLocation());
			
			ValidMines.Add(TPair<float, TPair<AMineActor*, AResourceActor*>>(
				DistToVehicle, 
				TPair<AMineActor*, AResourceActor*>(Mine, Resource)
			));
		}
		
		// Sort by distance to vehicle (closest first)
		ValidMines.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
		
		if (ValidMines.Num() > 0)
		{
			// Found a mine to attack!
			AMineActor* NextMine = ValidMines[0].Value.Key;
			AResourceActor* NextResource = ValidMines[0].Value.Value;
			
			CurrentTask.PrimaryTarget = NextMine;
			CurrentTask.Resource = NextResource;
			
			FString ResourceTypeName = (NextResource->ResourceType == EResourceType::OrangeSubstrate) 
				? TEXT("Orange") : TEXT("Black");
			
			UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Found next mine to attack - %s (%s resource) at %.0f units"),
				*GetName(), *NextMine->GetName(), *ResourceTypeName, ValidMines[0].Key);
			return;
		}
		
		UE_LOG(LogTemp, Log, TEXT("Vehicle %s: No more valid mines found in radius %.0f"),
			*GetName(), CurrentTask.SearchRadius);
		ClearTask();
		return;
	}
	
	// OPTION 2: Find neutral resources (no mines to destroy)
	const TArray<AActor*>& AllResources = CachedAllResources;
	
	TArray<TPair<float, AResourceActor*>> ValidResources;
	
	for (AActor* Actor : AllResources)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource) continue;
		
		// Skip if already owned by us
		if (Resource->OwnerTeam == OwnerTeam) continue;
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != OwnerTeam)
			continue;
		
		// Respect bCaptureNeutralOnly flag
		if (CurrentTask.bCaptureNeutralOnly && Resource->OwnerTeam != EOwnerTeam::Neutral)
			continue;
		
		// Filter Kaiju-guarded resources based on sub-priority
		// P2.1 (Easy Income): Skip Kaiju-guarded resources
		// P2.4 (Kaiju Clusters): ONLY target Kaiju-guarded resources
		if (CurrentTask.SubPriorityLabel == TEXT("P2.1"))
		{
			if (Resource->bKaijuGuarded) continue; // P2.1 avoids Kaiju
		}
		else if (CurrentTask.SubPriorityLabel == TEXT("P2.4"))
		{
			if (!Resource->bKaijuGuarded) continue; // P2.4 ONLY targets Kaiju-guarded
		}
		
		// Must be within SearchRadius from closest city
		float DistToCity = GetClosestCityDistance(Resource->GetActorLocation());
		if (DistToCity > CurrentTask.SearchRadius) continue;
		
		// Check if resource is protected by a mine we can't attack
		// (Find if there's a mine on this resource)
		const TArray<AActor*>& AllMines = CachedAllMines;
		
		bool bProtectedByUntouchableMine = false;
		for (AActor* MineActor : AllMines)
		{
			AMineActor* Mine = Cast<AMineActor>(MineActor);
			if (Mine && Mine->TargetResource == Resource)
			{
				// Resource has a mine - can we attack it?
				if (CurrentTask.MinesToDestroy.Num() == 0 || !CurrentTask.MinesToDestroy.Contains(Mine->OwnerTeam))
				{
					bProtectedByUntouchableMine = true;
					break;
				}
			}
		}
		
		if (bProtectedByUntouchableMine) continue;
		
		// Skip if another teammate vehicle is already targeting this resource
		bool bAlreadyTargeted = false;
		for (AActor* VehicleActor : AllVehicles)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(VehicleActor);
			if (Vehicle && Vehicle != this && Vehicle->OwnerTeam == OwnerTeam)
			{
				if (Vehicle->TargetResource == Resource)
				{
					bAlreadyTargeted = true;
					break;
				}
			}
		}
		
		if (bAlreadyTargeted) continue;
		
		// Calculate distance to vehicle
		float DistToVehicle = FVector::Dist(VehicleLocation, Resource->GetActorLocation());
		ValidResources.Add(TPair<float, AResourceActor*>(DistToVehicle, Resource));
	}
	
	// Sort by distance to vehicle (closest first)
	ValidResources.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
	
	if (ValidResources.Num() > 0)
	{
		// Found a resource to capture!
		AResourceActor* NextResource = ValidResources[0].Value;
		
		CurrentTask.PrimaryTarget = nullptr;
		CurrentTask.Resource = NextResource;
		
		//UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Found next resource to capture - %s at %.0f units"),
		//	*GetName(), *NextResource->GetName(), ValidResources[0].Key);
		return;
	}
	
	//UE_LOG(LogTemp, Log, TEXT("Vehicle %s: No more valid resources found in radius %.0f"),
	//	*GetName(), CurrentTask.SearchRadius);
	ClearTask();

	// TODO: Old AI system - currently disabled, new simple state AI controller handles targeting
	// FVector CurrentLocation = GetActorLocation();
	/*
	switch (AssignedAction)
	{
	case EAIAction::CaptureResources:
		{
			AResourceActor* NextResource = AIController->FindBestResourceToCapture(CurrentLocation);
			if (NextResource)
			{
				SetTargetResource(NextResource);
			}
			else
			{
				// No more resources available, clear assignment
				bHasAssignment = false;
			}
		}
		break;

	case EAIAction::CaptureEnemyResources:
		{
			AResourceActor* NextResource = AIController->FindBestEnemyResourceToCapture(CurrentLocation);
			if (NextResource)
			{
				SetTargetResource(NextResource);
			}
			else
			{
				// No more enemy resources available, clear assignment
				bHasAssignment = false;
			}
		}
		break;

	case EAIAction::CaptureCities:
		{
			ACityActor* NextCity = AIController->FindBestCityToAttack(CurrentLocation);
			if (NextCity)
			{
				// Calculate approach direction for formation positioning
				FVector ApproachDirection = (CurrentLocation - NextCity->GetActorLocation()).GetSafeNormal();
				
				// Get formation position (automatically spreads vehicles in organized arcs)
				FVector FormationPosition = CalculateFormationPosition(NextCity, ApproachDirection);
				
				SetTargetLocation(FormationPosition);
			}
			else
			{
				// No more enemy cities available, clear assignment
				bHasAssignment = false;
			}
		}
		break;

	case EAIAction::AttackVehicles:
		{
			AVehicleActor* NextVehicle = AIController->FindBestEnemyVehicle(CurrentLocation);
			if (NextVehicle)
			{
				// Set as combat target - combat positioning logic will maintain optimal range
				CurrentTarget = NextVehicle;
				bHasTarget = true;
			}
			else
			{
				// No more enemy vehicles available, clear assignment
				bHasAssignment = false;
			}
		}
		break;

	case EAIAction::DefendCities:
		{
			if (DefendedCity && AIController)
			{
				// Actively hunt enemies near the city we're defending
				AVehicleActor* NearbyEnemy = AIController->FindBestEnemyNearCity(DefendedCity, CurrentLocation);
				if (NearbyEnemy)
				{
					// Pursue enemy threatening the city
					SetTargetLocation(NearbyEnemy->GetActorLocation());
				}
				else
				{
					// No enemies nearby, patrol near city
					FVector CityLocation = DefendedCity->GetActorLocation();
					float DistanceToCity = FVector::Dist(CurrentLocation, CityLocation);
					
					// If too far from city, move closer
					if (DistanceToCity > 1500.0f)
					{
						SetTargetLocation(CityLocation);
					}
					else
					{
						// Near city with no threats, clear assignment
						bHasAssignment = false;
						DefendedCity = nullptr;
					}
				}
			}
			else
			{
				// No city assigned, clear assignment
				bHasAssignment = false;
				DefendedCity = nullptr;
			}
		}
		break;

	case EAIAction::BuildVehicles:
		// Building is handled by AI, not vehicle action
		bHasAssignment = false;
		break;
	}
	*/
}

// ========== TASK EXECUTION METHODS (Phase 2 - Currently Stubs) ==========

void AVehicleActor::ExecuteSecureIncomeTask(float DeltaTime)
{
	// Income task - capture resources (may need to destroy mines first)
	// This task uses parameters:
	// - PrimaryTarget: Mine to destroy (if resource is protected)
	// - Resource: Resource to capture
	// - bAutonomousContinue: Whether to find next resource or complete when done
	// - SearchRadius: Area to search for autonomous targets
	// - MinesToDestroy: Which teams' mines can we attack
	// - bCaptureNeutralOnly: Only capture neutral resources, or also fight for enemy-owned?
	
	// PRIORITY 1: If PrimaryTarget (mine) exists and is alive, attack it
	if (CurrentTask.PrimaryTarget.IsValid())
	{
		AActor* MineTarget = CurrentTask.PrimaryTarget.Get();
		ABuildingActor* Mine = Cast<ABuildingActor>(MineTarget);
		
		if (Mine && Mine->CurrentHealth > 0)
		{
			// Mine still alive, ensure we're targeting it
			if (CurrentTarget != MineTarget)
			{
				// Set mine as combat target
				CurrentTarget = MineTarget;
				PrimaryTarget = MineTarget;
				
				// Navigate to attack position: AttackRange away from mine on sphere surface
				FVector MineLocation = MineTarget->GetActorLocation();
				FVector MineDir = (MineLocation - PlanetCenter).GetSafeNormal();
				FVector VehicleDir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
				float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
				float TotalAngleMine = FMath::Acos(FMath::Clamp(FVector::DotProduct(MineDir, VehicleDir), -1.0f, 1.0f));
				float TotalArcMine = TotalAngleMine * PlanetR;
				float TMine = (TotalArcMine > KINDA_SMALL_NUMBER) ? FMath::Min(AttackRange / TotalArcMine, 1.0f) : 1.0f;
				FVector AttackDir = FMath::Lerp(MineDir, VehicleDir, TMine).GetSafeNormal();
				float TerrainHeight = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(AttackDir) : 0.0f;
				FVector AttackPos = PlanetCenter + AttackDir * (PlanetR * (1.0f + TerrainHeight));
				SetTargetLocation(AttackPos);
			}
			return; // Keep attacking mine
		}
		else
		{
			// Mine destroyed! Clear it and proceed to resource capture
			CurrentTask.PrimaryTarget = nullptr;
			PrimaryTarget = nullptr;
			CurrentTarget = nullptr;
			bHasTarget = false;
			
			UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Mine destroyed, proceeding to capture resource"), *GetName());
		}
	}
	
	// PRIORITY 2: If Resource target exists and not owned, capture it
	if (CurrentTask.Resource.IsValid())
	{
		AResourceActor* Resource = CurrentTask.Resource.Get();
		
		// Check if resource is being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != OwnerTeam)
		{
			// Someone else is capturing this resource - abandon it
			UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: Target resource %s being captured by Team %d, finding new target"), 
				*GetName(), *Resource->GetName(), (int32)Resource->CapturingTeam);
			CurrentTask.Resource = nullptr;
			TargetResource = nullptr;
			
			if (CurrentTask.bAutonomousContinue)
			{
				// Use task-based targeting for AI, player autonomous mode for player
				if (OwnerTeam != EOwnerTeam::Player)
				{
					FindNextAITaskTarget();
				}
				else
				{
					FindNextTarget();
					// Check if autonomous mode was disabled (no more valid targets)
					if (!bAIAutonomousMode && !bPlayerAutonomousMode)
					{
						ClearTask();
					}
				}
			}
			else
			{
				ClearTask();
			}
			return;
		}
		
		// Check if resource is still available (not owned by us)
		if (Resource->OwnerTeam != OwnerTeam)
		{
			// Check if resource was stolen by someone else mid-task
			// In autonomous mode, ALWAYS abandon enemy-owned resources (only target neutral)
			// In non-autonomous mode, respect bCaptureNeutralOnly flag
			if (Resource->OwnerTeam != EOwnerTeam::Neutral)
			{
				if (CurrentTask.bAutonomousContinue || CurrentTask.bCaptureNeutralOnly)
				{
					// Resource was captured by enemy - abandon it
					UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: Target resource %s owned by Team %d, finding new target"), 
						*GetName(), *Resource->GetName(), (int32)Resource->OwnerTeam);
					CurrentTask.Resource = nullptr;
					TargetResource = nullptr;
					
					if (CurrentTask.bAutonomousContinue)
					{
						// Use task-based targeting for AI, player autonomous mode for player
						if (OwnerTeam != EOwnerTeam::Player)
						{
							FindNextAITaskTarget();
						}
						else
						{
							FindNextTarget();
							// Check if autonomous mode was disabled (no more valid targets)
							if (!bAIAutonomousMode && !bPlayerAutonomousMode)
							{
								ClearTask();
							}
						}
					}
					else
					{
						ClearTask();
					}
					return;
				}
			}
			
			// Resource available (neutral or enemy we're allowed to fight for), move to capture it
			if (TargetResource != Resource)
			{
				SetTargetResource(Resource);
			}
			return; // Keep capturing
		}
		else
		{
			// Resource captured successfully!
			//UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Resource captured successfully"), *GetName());
			CurrentTask.Resource = nullptr;
			TargetResource = nullptr;
			
			// If autonomous, find next target
			if (CurrentTask.bAutonomousContinue)
			{
				// Use task-based targeting for AI, player autonomous mode for player
				if (OwnerTeam != EOwnerTeam::Player)
				{
					FindNextAITaskTarget();
				}
				else
				{
					FindNextTarget();
				}
			}
			else
			{
				// Task complete
				ClearTask();
			}
		}
	}
	// PRIORITY 3: Autonomous mode - find next target if idle
	else if (CurrentTask.bAutonomousContinue)
	{
		// No specific target, let autonomous logic handle it
		if (!bHasTarget && !TargetResource && !CurrentTarget)
		{
			// Use task-based targeting for AI, player autonomous mode for player
			if (OwnerTeam != EOwnerTeam::Player)
			{
				FindNextAITaskTarget();
			}
			else
			{
				FindNextTarget();
				
				// Check if FindNextTarget disabled autonomous mode (no resources found)
				// If so, clear the task so vehicle becomes idle
				if (!bAIAutonomousMode && !bPlayerAutonomousMode)
				{
					UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Autonomous search exhausted - clearing task"), *GetName());
					ClearTask();
				}
			}
		}
	}
	else
	{
		// No target and not autonomous - task complete
		  	ClearTask();
	}
}

void AVehicleActor::ExecuteAttackTargetTask(float DeltaTime)
{
	// War task - attack a specific target (enemy city, building, or vehicle)
	// This task uses parameters:
	// - PrimaryTarget: Main target (usually capital building)
	// - Destination: Rally point or approach vector
	// - bAutonomousContinue: Whether to find new targets after completion
	
	if (!GetWorld()) return;
	
	// Check if PrimaryTarget still exists and is valid
	if (!CurrentTask.PrimaryTarget.IsValid())
	{
		// Primary target destroyed or invalid - mission complete!
		UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Attack mission complete - target destroyed"), *GetName());
		ClearTask();
		return;
	}
	
	AActor* MainTarget = CurrentTask.PrimaryTarget.Get();
	
	// Check if target is a capital building - need to destroy turrets first
	ACapitalBuildingActor* TargetCapital = Cast<ACapitalBuildingActor>(MainTarget);
	if (TargetCapital &&TargetCapital->CurrentHealth > 0)
	{
		// Check if capital was captured by us - mission success!
		if (TargetCapital->OwnerTeam == OwnerTeam)
		{
			UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: VICTORY! Capital captured!"), *GetName());
			ClearTask();
			return;
		}
		
		// VALIDATE CAPITAL OWNERSHIP - ensure capital is still owned by enemy we were assigned to attack
		if (!CurrentTask.MinesToDestroy.Contains(TargetCapital->OwnerTeam))
		{
			// Capital was captured by another team (possibly our ally) - abort mission
			UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: Capital now owned by Team %d (not our enemy) - aborting attack"),
				*GetName(), (int32)TargetCapital->OwnerTeam);
			ClearTask();
			return;
		}
		
		// Find turrets in the parent city
		if (TargetCapital->ParentCity)
		{
			ATurretBuildingActor* ClosestTurret = nullptr;
			float ClosestDistance = FLT_MAX;
			
			for (ABuildingActor* Building : TargetCapital->ParentCity->Buildings)
			{
				ATurretBuildingActor* Turret = Cast<ATurretBuildingActor>(Building);
				if (Turret && Turret->CurrentHealth > 0)
				{
					// VALIDATE TURRET OWNERSHIP - skip turrets not owned by our enemy
					if (!CurrentTask.MinesToDestroy.Contains(Turret->OwnerTeam))
					{
						continue; // Turret captured by ally - skip it
					}
					
					float Distance = FVector::Dist(GetActorLocation(), Turret->GetActorLocation());
					if (Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						ClosestTurret = Turret;
					}
				}
			}
			
			// If turrets exist, attack them first
			if (ClosestTurret)
			{
				if (CurrentTarget != ClosestTurret)
				{
					// Set turret as combat target
					CurrentTarget = ClosestTurret;
					PrimaryTarget = MainTarget; // Remember capital as ultimate goal
					
					// Navigate to AttackRange * 0.75 away from turret using angular offset on sphere surface
					{
						float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
						FVector TgtDir = (ClosestTurret->GetActorLocation() - PlanetCenter).GetSafeNormal();
						FVector SelfDir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
						float DesiredDist = AttackRange * 0.75f;
						float TotalAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(TgtDir, SelfDir), -1.0f, 1.0f));
						float TotalArc = TotalAngle * PlanetR;
						float T = (TotalArc > KINDA_SMALL_NUMBER) ? FMath::Min(DesiredDist / TotalArc, 1.0f) : 1.0f;
						FVector ApproachDir = FMath::Lerp(TgtDir, SelfDir, T).GetSafeNormal();
						float TerrainH = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(ApproachDir) : 0.0f;
						SetTargetLocation(PlanetCenter + ApproachDir * (PlanetR * (1.0f + TerrainH)));
					}
					UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Attacking turret before capital"), *GetName());
				}
				return; // Keep attacking turret
			}
		}
		
		// No turrets left - attack capital
		if (CurrentTarget != TargetCapital)
		{
			// Set capital as combat target
			CurrentTarget = TargetCapital;
			PrimaryTarget = TargetCapital;
			
			// Navigate to AttackRange * 0.75 away from capital using angular offset on sphere surface
			{
				float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
				FVector TgtDir = (TargetCapital->GetActorLocation() - PlanetCenter).GetSafeNormal();
				FVector SelfDir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
				float DesiredDist = AttackRange * 0.75f;
				float TotalAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(TgtDir, SelfDir), -1.0f, 1.0f));
				float TotalArc = TotalAngle * PlanetR;
				float T = (TotalArc > KINDA_SMALL_NUMBER) ? FMath::Min(DesiredDist / TotalArc, 1.0f) : 1.0f;
				FVector ApproachDir = FMath::Lerp(TgtDir, SelfDir, T).GetSafeNormal();
				float TerrainH = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(ApproachDir) : 0.0f;
				SetTargetLocation(PlanetCenter + ApproachDir * (PlanetR * (1.0f + TerrainH)));
			}
			UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: All turrets destroyed - attacking capital!"), *GetName());
		}
		return;
	}
	
	// For non-capital targets (buildings, vehicles, cities), just attack directly
	ABuildingActor* TargetBuilding = Cast<ABuildingActor>(MainTarget);
	AVehicleActor* TargetVehicle = Cast<AVehicleActor>(MainTarget);
	ACityActor* TargetCity = Cast<ACityActor>(MainTarget);
	
	// VALIDATE CITY OWNERSHIP - ensure city is still owned by enemy we were assigned to attack
	if (TargetCity)
	{
		// Check if city owner is one of the teams we're allowed to attack
		if (!CurrentTask.MinesToDestroy.Contains(TargetCity->OwnerTeam))
		{
			// City was captured by another team (possibly our ally) - abort mission
			UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: City '%s' now owned by Team %d (not our enemy) - aborting attack"),
				*GetName(), *TargetCity->GetName(), (int32)TargetCity->OwnerTeam);
			ClearTask();
			return;
		}
	}
	
	// VALIDATE BUILDING OWNERSHIP - ensure building is still owned by enemy we were assigned to attack
	if (TargetBuilding)
	{
		// Check if building owner is one of the teams we're allowed to attack
		if (!CurrentTask.MinesToDestroy.Contains(TargetBuilding->OwnerTeam))
		{
			// Building was captured by another team (possibly our ally) - abort mission
			UE_LOG(LogTemp, Warning, TEXT("Vehicle %s: Building now owned by Team %d (not our enemy) - aborting attack"),
				*GetName(), (int32)TargetBuilding->OwnerTeam);
			ClearTask();
			return;
		}
	}
	
	// Check if target is destroyed
	bool bTargetDestroyed = false;
	if (TargetBuilding && TargetBuilding->CurrentHealth <= 0) bTargetDestroyed = true;
	if (TargetVehicle && TargetVehicle->CurrentHealth <= 0) bTargetDestroyed = true;
	if (TargetCity && TargetCity->CurrentHealth <= 0) bTargetDestroyed = true;
	
	if (bTargetDestroyed)
	{
		UE_LOG(LogTemp, Log, TEXT("Vehicle %s: Target destroyed - mission complete"), *GetName());
		ClearTask();
		return;
	}
	
	// Target still alive - ensure we're attacking it
	if (CurrentTarget != MainTarget)
	{
		// Set as combat target and navigate to AttackRange * 0.75 away using angular offset on sphere surface
		CurrentTarget = MainTarget;
		PrimaryTarget = MainTarget;
		{
			float PlanetR = OwningPlanet ? OwningPlanet->PlanetRadius : PlanetRadius;
			FVector TgtDir = (MainTarget->GetActorLocation() - PlanetCenter).GetSafeNormal();
			FVector SelfDir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
			float DesiredDist = AttackRange * 0.75f;
			float TotalAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(TgtDir, SelfDir), -1.0f, 1.0f));
			float TotalArc = TotalAngle * PlanetR;
			float T = (TotalArc > KINDA_SMALL_NUMBER) ? FMath::Min(DesiredDist / TotalArc, 1.0f) : 1.0f;
			FVector ApproachDir = FMath::Lerp(TgtDir, SelfDir, T).GetSafeNormal();
			float TerrainH = OwningPlanet ? OwningPlanet->CalculateHeightAtPoint(ApproachDir) : 0.0f;
			SetTargetLocation(PlanetCenter + ApproachDir * (PlanetR * (1.0f + TerrainH)));
		}
	}
}

void AVehicleActor::ExecuteDefendTerritoryTask(float DeltaTime)
{
	// Defensive patrol - scan defensive zone for threats and eliminate them
	// This task uses parameters:
	// - Destination: Defensive position (cluster center or city location)
	// - SearchRadius: Defensive zone size
	// - bAutonomousContinue: Whether to keep defending or complete when zone clear
	
	if (!GetWorld()) return;
	
	FVector DefensePosition = CurrentTask.Destination;
	float DefenseRadius = CurrentTask.SearchRadius;
	
	// Check distance from defensive position
	float DistanceFromPosition = FVector::Dist(GetActorLocation(), DefensePosition);
	
	// Scan for enemy vehicles in the defensive zone
	const TArray<AActor*>& FoundVehicles = CachedAllVehicles;
	
	AVehicleActor* ClosestThreat = nullptr;
	float ClosestDistance = FLT_MAX;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	const float HostilityThreshold = -0.5f;
	
	for (AActor* Actor : FoundVehicles)
	{
		AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
		if (!EnemyVehicle || EnemyVehicle == this || EnemyVehicle->CurrentHealth <= 0) continue;
		if (EnemyVehicle->OwnerTeam == OwnerTeam || EnemyVehicle->OwnerTeam == EOwnerTeam::Neutral) continue;
		
		// Check if hostile relationship
		bool bIsHostile = false;
		if (GameMode)
		{
			float Relationship = GameMode->GetRelationship(OwnerTeam, EnemyVehicle->OwnerTeam);
			bIsHostile = (Relationship <= HostilityThreshold);
		}
		
		if (!bIsHostile) continue;
		
		// Check if threat is within defensive radius from defense position
		float DistFromDefensePos = FVector::Dist(EnemyVehicle->GetActorLocation(), DefensePosition);
		if (DistFromDefensePos > DefenseRadius) continue;
		
		// Found a threat in our defensive zone
		float DistFromMe = FVector::Dist(GetActorLocation(), EnemyVehicle->GetActorLocation());
		if (DistFromMe < ClosestDistance)
		{
			ClosestDistance = DistFromMe;
			ClosestThreat = EnemyVehicle;
		}
	}
	
	// If we found a vehicle threat, engage it
	if (ClosestThreat)
	{
		// Set as current target - existing Tick() combat logic will handle movement and firing
		if (CurrentTarget != ClosestThreat)
		{
			CurrentTarget = ClosestThreat;
			PrimaryTarget = ClosestThreat;
			SetTargetLocation(ClosestThreat->GetActorLocation());
			bHasTarget = true;
		}
		return; // Engaging threat
	}
	
	// No vehicle threats found - defensive zone is clear of active threats
	// P1 Survival Layer only defends against VEHICLES, not buildings/mines (that's P2's job)
	// Return to defensive position if we've wandered too far
	if (DistanceFromPosition > 300.0f)
	{
		SetTargetLocation(DefensePosition);
		CurrentTarget = nullptr;
		PrimaryTarget = nullptr;
		bHasTarget = false;
	}
	else
	{
		// At position and no threats - hold position
		ClearTarget();
		CurrentTarget = nullptr;
		PrimaryTarget = nullptr;
		bHasTarget = false;
		
		// If not autonomous, mission complete (zone secured)
		// Note: AI controller will reassess in next cycle and may reassign or clear task
	}
}

void AVehicleActor::ExecuteAidAllyTask(float DeltaTime)
{
	// TODO Phase 2: Implement task-driven aid logic
	// This will handle:
	// 1. Moving to ally city
	// 2. Defending against nearby hostiles
	// 3. Clearing task when area secure
	
	// For now, fall back to existing behavior via legacy fields set in AssignTask()
}

void AVehicleActor::ExecuteRetaliateTask(float DeltaTime)
{
	// TODO Phase 2: Implement task-driven retaliation logic
	// This will handle:
	// 1. Moving toward retaliatory target
	// 2. Attacking target
	// 3. Clearing task when target destroyed
	
	// For now, fall back to existing behavior via legacy fields set in AssignTask()
}

// ========== END TASK EXECUTION METHODS ==========
