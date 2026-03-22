// Copyright Epic Games, Inc. All Rights Reserved.

#include "KaijuActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "../Projectiles/ProjectileActor.h"
#include "../Vehicles/VehicleActor.h"
#include "../Cities/CityActor.h"
#include "../Resources/ResourceActor.h"
#include "../Buildings/BuildingActor.h"
#include "../Buildings/MineActor.h"
#include "../../UI/HealthBarWidget.h"
#include "../../UI/InfoUIWidget.h"

AKaijuActor::AKaijuActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create capsule collision as root (for horizontal rectangular prism shape)
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	RootComponent = CollisionCapsule;
	CollisionCapsule->InitCapsuleSize(100.0f, 50.0f); // Radius 100, HalfHeight 50 (will be scaled per size)
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionCapsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Block cities
	CollisionCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // Overlap vehicles (no collision)
	CollisionCapsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // Block projectiles
	CollisionCapsule->SetCollisionObjectType(ECC_Pawn);
	CollisionCapsule->SetHiddenInGame(true);

	// Create kaiju mesh (using cube, will scale to rectangular prism)
	KaijuMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KaijuMesh"));
	KaijuMesh->SetupAttachment(RootComponent);
	
	// Load cube mesh from engine content
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		KaijuMesh->SetStaticMesh(CubeMesh.Object);
	}
	
	// Scale to rectangular prism (longer than tall - default medium size)
	// 2x1x1 ratio (longer in X, shorter in height Z)
	KaijuMesh->SetRelativeScale3D(FVector(2.0f, 1.0f, 1.0f));
	
	// Rotate mesh so it's horizontal (capsule is vertical by default)
	KaijuMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	
	// Try to load team color material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColorMat(TEXT("/Game/M_TeamColor"));
	if (TeamColorMat.Succeeded())
	{
		KaijuMesh->SetMaterial(0, TeamColorMat.Object);
	}
	
	// Disable collision on mesh (collision handled by capsule)
	KaijuMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create selection box (wireframe cube for hover/targeting)
	SelectionBox = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionBox"));
	SelectionBox->SetupAttachment(RootComponent);
	SelectionBox->SetStaticMesh(CubeMesh.Object);
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
	HealthBarWidget->SetDrawSize(FVector2D(200.0f, 30.0f));
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f)); // Above kaiju
	HealthBarWidget->SetVisibility(false); // Hidden by default

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
	InfoWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f)); // Above health bar
	InfoWidget->SetVisibility(false); // Hidden by default

	// Load info UI widget class
	static ConstructorHelpers::FClassFinder<UUserWidget> InfoUIClass(TEXT("/Game/UI/WBP_InfoUI.WBP_InfoUI_C"));
	if (InfoUIClass.Succeeded())
	{
		InfoWidget->SetWidgetClass(InfoUIClass.Class);
	}
}

void AKaijuActor::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHealth = MaxHealth;
	
	// Set guard center to spawn location if not set
	if (GuardCenter.IsZero())
	{
		GuardCenter = GetActorLocation();
	}
	
	// Configure based on size
	ConfigureForSize(KaijuSize);
	
	// Align to planet
	AlignToPlanet();
	
	// Set initial color (red/orange for hostile)
	UpdateHealthBar();
	
	// Initialize info widget
	UpdateInfoDisplay();
	
	if (KaijuMesh)
	{
		UMaterialInstanceDynamic* DynMaterial = KaijuMesh->CreateDynamicMaterialInstance(0);
		if (DynMaterial)
		{
			// Hostile red-orange color
			FLinearColor KaijuColor = FLinearColor(1.0f, 0.3f, 0.1f);
			DynMaterial->SetVectorParameterValue(FName("TeamColor"), KaijuColor);
		}
	}
}

void AKaijuActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Align to planet surface
	AlignToPlanet();
	
	// Update state machine
	switch (CurrentState)
	{
		case EKaijuState::Idle:
			UpdateIdle(DeltaTime);
			break;
		case EKaijuState::Alerted:
			UpdateAlerted(DeltaTime);
			break;
		case EKaijuState::Attacking:
			UpdateAttacking(DeltaTime);
			break;
		case EKaijuState::Pursuing:
			UpdatePursuing(DeltaTime);
			break;
		case EKaijuState::Invading:
			UpdateInvading(DeltaTime);
			break;
		case EKaijuState::Returning:
			UpdateReturning(DeltaTime);
			break;
	}
	
	// Update combat if we have a target
	if (CurrentTarget && IsValid(CurrentTarget))
	{
		UpdateCombat(DeltaTime);
	}
	
	// Move towards target location if set
	if (bHasTarget)
	{
		MoveTowardsTarget(DeltaTime);
	}
	
	// Update team relationships (reset to neutral after 30 seconds)
	UWorld* World = GetWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		UpdateRelationships(CurrentTime);
	}
	
	// Update health bar visibility
	UpdateHealthBar();
	
	// Health regeneration: 5 HP per second (always active)
	if (CurrentHealth < MaxHealth)
	{
		CurrentHealth += 5.0f * DeltaTime;
		CurrentHealth = FMath::Min(CurrentHealth, MaxHealth);
	}
	
	// Mine destruction: Every 30 seconds, 10% chance to destroy a random mine in guard area
	TimeSinceLastMineCheck += DeltaTime;
	if (TimeSinceLastMineCheck >= 30.0f)
	{
		TimeSinceLastMineCheck = 0.0f;
		
		// 10% chance to destroy a mine
		if (FMath::FRand() <= 0.1f)
		{
			// Find all resources in guard area that have mines
			TArray<AResourceActor*> ResourcesWithMines;
			if (World)
			{
				TArray<AActor*> AllResources;
				UGameplayStatics::GetAllActorsOfClass(World, AResourceActor::StaticClass(), AllResources);
				
				for (AActor* Actor : AllResources)
				{
					if (AResourceActor* Resource = Cast<AResourceActor>(Actor))
					{
						// Check if resource is in guard area and has a mine
						if (IsResourceInGuardArea(Resource) && Resource->Mine)
						{
							ResourcesWithMines.Add(Resource);
						}
					}
				}
			}
			
			// Destroy a random mine if any exist
			if (ResourcesWithMines.Num() > 0)
			{
				int32 RandomIndex = FMath::RandRange(0, ResourcesWithMines.Num() - 1);
				AResourceActor* TargetResource = ResourcesWithMines[RandomIndex];
				if (TargetResource && TargetResource->Mine)
				{
					TargetResource->Mine->Destroy();
					TargetResource->Mine = nullptr;
				}
			}
		}
	}
	
	// City invasion: Every 30 seconds, 2% chance to attack a nearby city (only when idle/returning)
	if (CurrentState == EKaijuState::Idle || CurrentState == EKaijuState::Returning)
	{
		TimeSinceLastCityInvasionCheck += DeltaTime;
		if (TimeSinceLastCityInvasionCheck >= 30.0f)
		{
			TimeSinceLastCityInvasionCheck = 0.0f;
			
			// 2% chance to invade
			if (FMath::FRand() <= 0.02f)
			{
				// Find nearest city that qualifies for invasion
				if (World)
				{
					TArray<AActor*> AllCities;
					UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), AllCities);
					
					ACityActor* BestCity = nullptr;
					float BestDistance = FLT_MAX;
					
					for (AActor* Actor : AllCities)
					{
						if (ACityActor* City = Cast<ACityActor>(Actor))
						{
							// Check if city qualifies for invasion based on turret count and kaiju size
							if (CanInvadeCity(City))
							{
								float Distance = FVector::Dist(GetActorLocation(), City->GetActorLocation());
								if (Distance < BestDistance)
								{
									BestDistance = Distance;
									BestCity = City;
								}
							}
						}
					}
					
					// Start invasion if valid city found
					if (BestCity)
					{
						SetupInvasionTargets(BestCity);
						TransitionToInvading(BestCity);
					}
				}
			}
		}
	}
	
	bIsFiring = false;
}

void AKaijuActor::AlignToPlanet()
{
	if (PlanetCenter.IsZero()) return;
	
	FVector CurrentLocation = GetActorLocation();
	FVector DirectionFromCenter = (CurrentLocation - PlanetCenter).GetSafeNormal();
	
	// Position on planet surface
	FVector AlignedLocation = PlanetCenter + DirectionFromCenter * PlanetRadius;
	SetActorLocation(AlignedLocation);
	
	// Orient "up" to point away from planet center
	FVector UpVector = DirectionFromCenter;
	FVector ForwardVector = GetActorForwardVector();
	
	// Make forward tangent to sphere
	ForwardVector = (ForwardVector - UpVector * FVector::DotProduct(ForwardVector, UpVector)).GetSafeNormal();
	
	FRotator NewRotation = UKismetMathLibrary::MakeRotFromXZ(ForwardVector, UpVector);
	SetActorRotation(NewRotation);
}

// ========== STATE TRANSITIONS ==========

void AKaijuActor::TransitionToIdle()
{
	CurrentState = EKaijuState::Idle;
	CurrentTarget = nullptr;
	bHasTarget = false;
	TimeSinceLastWander = 0.0f;
}

void AKaijuActor::TransitionToAlerted(AVehicleActor* DetectedVehicle)
{
	CurrentState = EKaijuState::Alerted;
	
	if (DetectedVehicle)
	{
		TargetLocation = DetectedVehicle->GetActorLocation();
		bHasTarget = true;
	}
}

void AKaijuActor::TransitionToAttacking(AActor* Target)
{
	CurrentState = EKaijuState::Attacking;
	CurrentTarget = Target;
	
	if (Target)
	{
		TargetLocation = Target->GetActorLocation();
		bHasTarget = true;
	}
}

void AKaijuActor::TransitionToPursuing(AActor* Target)
{
	CurrentState = EKaijuState::Pursuing;
	CurrentTarget = Target;
	ProvocationCount++; // Track that we were provoked
	
	if (Target)
	{
		TargetLocation = Target->GetActorLocation();
		bHasTarget = true;
	}
}

void AKaijuActor::TransitionToInvading(ACityActor* TargetCity)
{
	CurrentState = EKaijuState::Invading;
	CurrentTarget = TargetCity;
	InvasionUrge = 0.0f; // Reset invasion urge
	
	if (TargetCity)
	{
		TargetLocation = TargetCity->GetActorLocation();
		bHasTarget = true;
	}
}

void AKaijuActor::TransitionToReturning()
{
	CurrentState = EKaijuState::Returning;
	CurrentTarget = nullptr;
	TargetLocation = GuardCenter;
	bHasTarget = true;
}

// ========== STATE BEHAVIORS ==========

void AKaijuActor::UpdateIdle(float DeltaTime)
{
	// Check for vehicles in detection range
	float DetectionRange = BaseDetectionRadius * 0.6f; // Passive detection when idle
	AVehicleActor* NearbyVehicle = FindNearestVehicle(DetectionRange);
	
	if (NearbyVehicle)
	{
		// Detected vehicle - become alerted
		TransitionToAlerted(NearbyVehicle);
		return;
	}
	
	// Update rest/wander timing
	TimeSinceLastWander += DeltaTime;
	TimeInCurrentRestState += DeltaTime;
	
	// Check if it's time to change rest/wander state
	if (TimeInCurrentRestState >= RestStateDuration)
	{
		// Randomly decide to rest or wander (50/50 chance)
		bIsResting = FMath::RandBool();
		
		if (bIsResting)
		{
			// Rest for a while - clear target so kaiju stops moving
			bHasTarget = false;
			RestStateDuration = FMath::FRandRange(3.0f, 8.0f);
		}
		else
		{
			// Pick a new random point within guard radius to wander to
			FVector RandomOffset = FVector(
				FMath::FRandRange(-GuardRadius, GuardRadius),
				FMath::FRandRange(-GuardRadius, GuardRadius),
				0.0f
			);
			
			// Project onto planet surface
			FVector WanderPoint = GuardCenter + RandomOffset;
			FVector DirectionFromCenter = (WanderPoint - PlanetCenter).GetSafeNormal();
			IdleWanderTarget = PlanetCenter + DirectionFromCenter * PlanetRadius;
			
			TargetLocation = IdleWanderTarget;
			bHasTarget = true;
			RestStateDuration = FMath::FRandRange(3.0f, 8.0f);
		}
		
		TimeInCurrentRestState = 0.0f;
		TimeSinceLastWander = 0.0f;
	}
	
	// Accumulate invasion urge slowly
	TimeSinceLastInvasionCheck += DeltaTime;
	InvasionUrge += BaseInvasionRate * DeltaTime;
}

void AKaijuActor::UpdateAlerted(float DeltaTime)
{
	// Look for vehicles in extended detection range
	float ExtendedRange = BaseDetectionRadius * 1.5f;
	AVehicleActor* NearbyVehicle = FindNearestVehicle(ExtendedRange);
	
	if (NearbyVehicle)
	{
		float Distance = FVector::Dist(GetActorLocation(), NearbyVehicle->GetActorLocation());
		
		// If vehicle is close, start attacking
		if (Distance <= AttackRange * 1.2f)
		{
			TransitionToAttacking(NearbyVehicle);
			return;
		}
		
		// Otherwise, move towards vehicle
		TargetLocation = NearbyVehicle->GetActorLocation();
		bHasTarget = true;
	}
	else
	{
		// No vehicle detected - return to idle if we're in guard area
		if (FVector::Dist(GetActorLocation(), GuardCenter) <= GuardRadius)
		{
			TransitionToIdle();
		}
		else
		{
			// Return to guard area
			TransitionToReturning();
		}
	}
}

void AKaijuActor::UpdateAttacking(float DeltaTime)
{
	if (!CurrentTarget || !IsValid(CurrentTarget))
	{
		// Target lost - check if we should pursue or return
		if (IsWithinLeash())
		{
			TransitionToAlerted(nullptr);
		}
		else
		{
			TransitionToReturning();
		}
		return;
	}
	
	float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
	
	// If target is out of range, pursue or return based on leash
	if (DistanceToTarget > AttackRange * 1.5f)
	{
		if (IsWithinLeash())
		{
			TransitionToPursuing(CurrentTarget);
		}
		else
		{
			TransitionToReturning();
		}
		return;
	}
	
	// Move towards target if not in optimal range
	if (DistanceToTarget > AttackRange * 0.8f)
	{
		TargetLocation = CurrentTarget->GetActorLocation();
		bHasTarget = true;
	}
	else
	{
		bHasTarget = false; // Stop moving, just attack
	}
}

void AKaijuActor::UpdatePursuing(float DeltaTime)
{
	// Check if we should give up pursuit
	if (ShouldReturn())
	{
		TransitionToReturning();
		return;
	}
	
	if (!CurrentTarget || !IsValid(CurrentTarget))
	{
		// Lost target
		TransitionToReturning();
		return;
	}
	
	float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
	float DistanceToGuard = FVector::Dist(GetActorLocation(), GuardCenter);
	
	// Check for nearby city - if close enough, might switch to invasion
	ACityActor* NearbyCity = FindNearestCity(InvasionRadius);
	if (NearbyCity)
	{
		float InvasionProbability = CalculateInvasionUrge();
		if (InvasionProbability >= InvasionThreshold)
		{
			TransitionToInvading(NearbyCity);
			return;
		}
	}
	
	// Continue pursuing
	if (DistanceToTarget <= AttackRange * 1.2f)
	{
		TransitionToAttacking(CurrentTarget);
	}
	else
	{
		TargetLocation = CurrentTarget->GetActorLocation();
		bHasTarget = true;
	}
}

void AKaijuActor::UpdateInvading(float DeltaTime)
{
	// Clean up invalid targets from the list
	InvasionTargets.RemoveAll([](ABuildingActor* Building) {
		return !Building || !IsValid(Building);
	});
	
	// If all targets destroyed, return to guard area
	if (InvasionTargets.Num() == 0)
	{
		TransitionToReturning();
		return;
	}
	
	// Get the first valid target
	ABuildingActor* CurrentBuildingTarget = InvasionTargets[0];
	
	if (!CurrentBuildingTarget || !IsValid(CurrentBuildingTarget))
	{
		InvasionTargets.RemoveAt(0);
		return;
	}
	
	float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentBuildingTarget->GetActorLocation());
	
	// Attack the building if in range
	if (DistanceToTarget <= AttackRange)
	{
		bHasTarget = false; // Stop moving, attack the building
		CurrentTarget = CurrentBuildingTarget; // Set as current combat target
	}
	else
	{
		// Move towards the building
		TargetLocation = CurrentBuildingTarget->GetActorLocation();
		bHasTarget = true;
		CurrentTarget = CurrentBuildingTarget;
	}
}

void AKaijuActor::UpdateReturning(float DeltaTime)
{
	float DistanceToGuard = FVector::Dist(GetActorLocation(), GuardCenter);
	
	// If back in guard area, return to idle
	if (DistanceToGuard <= GuardRadius * 0.5f)
	{
		TransitionToIdle();
		return;
	}
	
	// Continue returning to guard center
	TargetLocation = GuardCenter;
	bHasTarget = true;
	
	// Clear provocation over time
	if (ProvocationCount > 0)
	{
		// Reduce provocation slowly
		float ProvocationDecay = DeltaTime * 0.1f; // Decay rate
		if (FMath::FRand() < ProvocationDecay)
		{
			ProvocationCount--;
		}
	}
}

// ========== HELPER FUNCTIONS ==========

float AKaijuActor::GetDetectionRadius(AVehicleActor* Vehicle)
{
	if (!Vehicle) return BaseDetectionRadius;
	
	float Radius = BaseDetectionRadius;
	
	// Check if vehicle is capturing resource in our guard area
	if (Vehicle->TargetResource && IsResourceInGuardArea(Vehicle->TargetResource))
	{
		Radius *= DetectionMultiplierCapturing;
	}
	
	// Check if vehicle is in combat
	if (Vehicle->CurrentTarget)
	{
		Radius *= DetectionMultiplierCombat;
	}
	
	// Count nearby vehicles
	int32 NearbyVehicles = 0;
	TArray<AActor*> AllVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
	
	for (AActor* Actor : AllVehicles)
	{
		if (FVector::Dist(Vehicle->GetActorLocation(), Actor->GetActorLocation()) < 500.0f)
		{
			NearbyVehicles++;
		}
	}
	
	Radius += NearbyVehicles * DetectionPerVehicle;
	
	return Radius;
}

bool AKaijuActor::IsWithinLeash() const
{
	float Distance = FVector::Dist(GetActorLocation(), GuardCenter);
	return Distance < LeashRadius;
}

bool AKaijuActor::ShouldReturn() const
{
	return !IsWithinLeash() && CurrentState != EKaijuState::Invading;
}

float AKaijuActor::CalculateInvasionUrge()
{
	float Urge = InvasionUrge;
	
	// Add provocation memory
	Urge += ProvocationCount * ProvocationUrgeBonus;
	
	// Check if city visible
	ACityActor* NearbyCity = FindNearestCity(InvasionRadius);
	if (NearbyCity)
	{
		Urge += 30.0f;
	}
	
	// Size modifier (small more likely, large less likely)
	float SizeMultiplier = 1.0f;
	switch (KaijuSize)
	{
		case EKaijuSize::Small:
			SizeMultiplier = 1.5f;
			break;
		case EKaijuSize::Medium:
			SizeMultiplier = 1.0f;
			break;
		case EKaijuSize::Large:
			SizeMultiplier = 0.3f;
			break;
	}
	
	Urge *= SizeMultiplier;
	
	return Urge;
}

AVehicleActor* AKaijuActor::FindNearestVehicle(float SearchRadius)
{
	if (!GetWorld()) return nullptr;
	
	TArray<AActor*> AllVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
	
	AVehicleActor* NearestVehicle = nullptr;
	float NearestDistance = SearchRadius;
	
	for (AActor* Actor : AllVehicles)
	{
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (!Vehicle) continue;
		
		float Distance = FVector::Dist(GetActorLocation(), Vehicle->GetActorLocation());
		float VehicleDetectionRadius = GetDetectionRadius(Vehicle);
		
		if (Distance < VehicleDetectionRadius && Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestVehicle = Vehicle;
		}
	}
	
	return NearestVehicle;
}

ACityActor* AKaijuActor::FindNearestCity(float SearchRadius)
{
	if (!GetWorld()) return nullptr;
	
	TArray<AActor*> AllCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), AllCities);
	
	ACityActor* NearestCity = nullptr;
	float NearestDistance = SearchRadius;
	
	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (!City) continue;
		
		float Distance = FVector::Dist(GetActorLocation(), City->GetActorLocation());
		
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestCity = City;
		}
	}
	
	return NearestCity;
}

bool AKaijuActor::IsResourceInGuardArea(AResourceActor* Resource)
{
	if (!Resource) return false;
	
	float Distance = FVector::Dist(Resource->GetActorLocation(), GuardCenter);
	return Distance <= GuardRadius;
}

bool AKaijuActor::CanInvadeCity(ACityActor* City)
{
	if (!City) return false;
	
	// Large kaiju never attack cities
	if (KaijuSize == EKaijuSize::Large) return false;
	
	// Count turrets in the city
	int32 TurretCount = 0;
	for (ABuildingActor* Building : City->Buildings)
	{
		if (Building && Building->BuildingType == EBuildingType::Turret)
		{
			TurretCount++;
		}
	}
	
	// Small kaiju need 3+ turrets
	if (KaijuSize == EKaijuSize::Small)
	{
		return TurretCount >= 3;
	}
	
	// Medium kaiju need 6+ turrets
	if (KaijuSize == EKaijuSize::Medium)
	{
		return TurretCount >= 6;
	}
	
	return false;
}

void AKaijuActor::SetupInvasionTargets(ACityActor* City)
{
	if (!City) return;
	
	InvasionTargets.Empty();
	
	// Add all turrets as targets
	for (ABuildingActor* Building : City->Buildings)
	{
		if (Building && Building->BuildingType == EBuildingType::Turret)
		{
			InvasionTargets.Add(Building);
		}
	}
	
	// Find nearby mines and add 2 random ones
	UWorld* World = GetWorld();
	if (World)
	{
		TArray<AActor*> AllResources;
		UGameplayStatics::GetAllActorsOfClass(World, AResourceActor::StaticClass(), AllResources);
		
		TArray<AMineActor*> NearbyMines;
		float MineSearchRadius = 5000.0f; // Search within 5000 units of city (territory radius)
		
		for (AActor* Actor : AllResources)
		{
			if (AResourceActor* Resource = Cast<AResourceActor>(Actor))
			{
				if (Resource->Mine)
				{
					float Distance = FVector::Dist(Resource->Mine->GetActorLocation(), City->GetActorLocation());
					if (Distance <= MineSearchRadius)
					{
						NearbyMines.Add(Resource->Mine);
					}
				}
			}
		}
		
		// Sort mines by distance to kaiju (closest first)
		NearbyMines.Sort([this](const AMineActor& A, const AMineActor& B) {
			float DistA = FVector::Dist(A.GetActorLocation(), GetActorLocation());
			float DistB = FVector::Dist(B.GetActorLocation(), GetActorLocation());
			return DistA < DistB;
		});
		
		// Select the first 2 closest mines
		int32 MinesToAdd = FMath::Min(2, NearbyMines.Num());
		for (int32 i = 0; i < MinesToAdd; i++)
		{
			InvasionTargets.Add(NearbyMines[i]);
		}
	}
}

// ========== MOVEMENT ==========

void AKaijuActor::MoveTowardsTarget(float DeltaTime)
{
	if (!bHasTarget) return;
	
	FVector CurrentLocation = GetActorLocation();
	FVector ToTarget = (TargetLocation - CurrentLocation).GetSafeNormal();
	
	// Calculate direction on planet surface
	FVector CurrentDirection = (CurrentLocation - PlanetCenter).GetSafeNormal();
	FVector TargetDirection = (TargetLocation - CurrentLocation);
	TargetDirection = (TargetDirection - CurrentDirection * FVector::DotProduct(TargetDirection, CurrentDirection)).GetSafeNormal();
	
	// Use steering to avoid obstacles
	FVector SteeringDirection = CalculateSteeringDirection(CurrentDirection, TargetDirection, DeltaTime);
	
	// Move along planet surface
	float DistanceToMove = MovementSpeed * DeltaTime;
	float AngularDistance = DistanceToMove / PlanetRadius;
	
	FVector RotationAxis = FVector::CrossProduct(CurrentDirection, SteeringDirection).GetSafeNormal();
	FQuat Rotation(RotationAxis, AngularDistance);
	FVector NewDirection = Rotation.RotateVector(CurrentDirection);
	FVector NewLocation = PlanetCenter + NewDirection * PlanetRadius;
	
	SetActorLocation(NewLocation);
	
	// Update forward direction
	FRotator NewRotation = UKismetMathLibrary::MakeRotFromXZ(SteeringDirection, CurrentDirection);
	SetActorRotation(NewRotation);
	
	// Check if reached target
	float DistanceToTarget = FVector::Dist(NewLocation, TargetLocation);
	if (DistanceToTarget < 100.0f)
	{
		bHasTarget = false;
	}
}

bool AKaijuActor::DetectObstacleAhead(FVector CurrentDirection, float& OutDistance, FVector& OutHitLocation, AActor*& OutHitActor)
{
	if (!GetWorld()) return false;
	
	float CheckDistance = MovementSpeed * LookAheadTime;
	FVector StartLocation = GetActorLocation();
	
	FVector ForwardOnSurface = GetActorForwardVector();
	ForwardOnSurface = (ForwardOnSurface - CurrentDirection * FVector::DotProduct(ForwardOnSurface, CurrentDirection)).GetSafeNormal();
	
	float AngularDistance = CheckDistance / PlanetRadius;
	FVector RotationAxis = FVector::CrossProduct(CurrentDirection, ForwardOnSurface).GetSafeNormal();
	FQuat Rotation(RotationAxis, AngularDistance);
	FVector EndDirection = Rotation.RotateVector(CurrentDirection);
	FVector EndLocation = PlanetCenter + EndDirection * PlanetRadius;
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	float SweepRadius = CollisionCapsule ? CollisionCapsule->GetScaledCapsuleRadius() * 2.5f : 250.0f;
	
	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SweepRadius),
		QueryParams
	);
	
	// Filter out objects we should ignore (vehicles, resources, mines)
	if (bHit && HitResult.GetActor())
	{
		// Ignore all vehicles - kaiju pass through them
		if (HitResult.GetActor()->IsA(AVehicleActor::StaticClass()))
		{
			return false;
		}
		
		// Ignore all resources
		if (HitResult.GetActor()->IsA(AResourceActor::StaticClass()))
		{
			return false;
		}
		
		// Ignore all mines (if we add MineActor class later)
		// if (HitResult.GetActor()->IsA(AMineActor::StaticClass()))
		// {
		//     return false;
		// }
	}
	
	if (bHit)
	{
		OutDistance = HitResult.Distance;
		OutHitLocation = HitResult.Location;
		OutHitActor = HitResult.GetActor();
	}
	
	return bHit;
}

bool AKaijuActor::CheckLateralObstacle(FVector CurrentDirection, float AngleDegrees, float& OutDistance)
{
	if (!GetWorld()) return false;
	
	FVector StartLocation = GetActorLocation();
	FVector ForwardOnSurface = GetActorForwardVector();
	ForwardOnSurface = (ForwardOnSurface - CurrentDirection * FVector::DotProduct(ForwardOnSurface, CurrentDirection)).GetSafeNormal();
	
	float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	FQuat Rotation(CurrentDirection, AngleRadians);
	FVector LateralDirection = Rotation.RotateVector(ForwardOnSurface);
	
	float AngularDistance = LateralCheckDistance / PlanetRadius;
	FVector TestRotAxis = FVector::CrossProduct(CurrentDirection, LateralDirection).GetSafeNormal();
	FQuat TestRotation(TestRotAxis, AngularDistance);
	FVector EndDirection = TestRotation.RotateVector(CurrentDirection);
	FVector EndLocation = PlanetCenter + EndDirection * PlanetRadius;
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	float SweepRadius = CollisionCapsule ? CollisionCapsule->GetScaledCapsuleRadius() * 2.5f : 250.0f;
	
	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SweepRadius),
		QueryParams
	);
	
	// Filter out objects we should ignore
	if (bHit && HitResult.GetActor())
	{
		// Ignore vehicles, resources, mines (same as DetectObstacleAhead)
		if (HitResult.GetActor()->IsA(AVehicleActor::StaticClass()) ||
		    HitResult.GetActor()->IsA(AResourceActor::StaticClass()))
		{
			return false;
		}
	}
	
	if (bHit)
	{
		OutDistance = HitResult.Distance;
	}
	
	return bHit;
}

FVector AKaijuActor::CalculateSteeringDirection(FVector CurrentDirection, FVector TargetDirection, float DeltaTime)
{
	// Simplified version of vehicle steering - just avoid cities, not other kaiju/vehicles
	if (!GetWorld()) return TargetDirection;
	
	float Distance;
	FVector HitLocation;
	AActor* HitActor = nullptr;
	
	bool bObstacleDetected = DetectObstacleAhead(CurrentDirection, Distance, HitLocation, HitActor);
	
	if (!bObstacleDetected)
	{
		// Clear path - go straight to target
		return TargetDirection;
	}
	
	// Obstacle ahead - try to steer around it
	float BestScore = -FLT_MAX;
	FVector BestDirection = TargetDirection;
	
	FVector ForwardOnSurface = GetActorForwardVector();
	ForwardOnSurface = (ForwardOnSurface - CurrentDirection * FVector::DotProduct(ForwardOnSurface, CurrentDirection)).GetSafeNormal();
	
	// Test multiple steering angles
	for (int32 i = 0; i < SteeringAngles; i++)
	{
		float AngleDegrees = -180.0f + (360.0f / SteeringAngles) * i;
		float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		
		FQuat TestRotation(CurrentDirection, AngleRadians);
		FVector TestDirection = TestRotation.RotateVector(ForwardOnSurface);
		
		// Score = alignment with target - penalty for obstacles
		float Alignment = FVector::DotProduct(TestDirection, TargetDirection);
		
		// Check if this direction has obstacles
		float TestDistance;
		bool bTestObstacle = CheckLateralObstacle(CurrentDirection, AngleDegrees, TestDistance);
		
		float ObstaclePenalty = 0.0f;
		if (bTestObstacle && TestDistance < AvoidanceDistance)
		{
			ObstaclePenalty = 2.0f * (1.0f - TestDistance / AvoidanceDistance);
		}
		
		float Score = Alignment - ObstaclePenalty;
		
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDirection = TestDirection;
		}
	}
	
	// Smooth steering
	if (!PreviousSteeringDirection.IsNearlyZero())
	{
		BestDirection = FMath::Lerp(PreviousSteeringDirection, BestDirection, 0.3f).GetSafeNormal();
	}
	
	PreviousSteeringDirection = BestDirection;
	
	return BestDirection;
}

// ========== COMBAT ==========

void AKaijuActor::UpdateCombat(float DeltaTime)
{
	if (!CurrentTarget || !IsValid(CurrentTarget))
	{
		CurrentTarget = nullptr;
		return;
	}
	
	TimeSinceLastAttack += DeltaTime;
	
	float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
	
	// Fire if in range and cooldown ready
	if (DistanceToTarget <= AttackRange && TimeSinceLastAttack >= AttackCooldown)
	{
		FireAtTarget(CurrentTarget);
	}
}

void AKaijuActor::FireAtTarget(AActor* Target)
{
	if (!Target || !GetWorld()) return;
	
	// Spawn projectile
	FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
	FRotator SpawnRotation = (Target->GetActorLocation() - SpawnLocation).Rotation();
	
	AProjectileActor* Projectile = GetWorld()->SpawnActor<AProjectileActor>(
		AProjectileActor::StaticClass(),
		SpawnLocation,
		SpawnRotation
	);
	
	if (Projectile)
	{
		Projectile->SetOwner(this); // Set kaiju as owner so it's passed as attacking actor
		Projectile->TargetActor = Target;
		Projectile->PlanetCenter = PlanetCenter;
		Projectile->PlanetRadius = PlanetRadius;
		Projectile->Damage = AttackDamage;
		Projectile->OwnerTeam = EOwnerTeam::Neutral; // Kaiju are hostile to all
		
		// Color projectile (red/orange)
		if (Projectile->ProjectileMesh)
		{
			FLinearColor ProjectileColor = FLinearColor(1.0f, 0.3f, 0.1f);
			UMaterialInstanceDynamic* DynMaterial = Projectile->ProjectileMesh->CreateDynamicMaterialInstance(0);
			if (DynMaterial)
			{
				FLinearColor EmissiveColor = ProjectileColor * 3.0f;
				DynMaterial->SetVectorParameterValue(FName("BaseColor"), ProjectileColor);
				DynMaterial->SetVectorParameterValue(FName("Color"), ProjectileColor);
				DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), EmissiveColor);
			}
		}
		
		bIsFiring = true;
		TimeSinceLastAttack = 0.0f;
	}
}

void AKaijuActor::ApplyDamage(float DamageAmount, AActor* AttackingActor)
{
	CurrentHealth -= DamageAmount;
	
	// Update relationship when damaged by a vehicle
	if (AttackingActor)
	{
		AVehicleActor* AttackingVehicle = Cast<AVehicleActor>(AttackingActor);
		if (AttackingVehicle)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				float CurrentTime = World->GetTimeSeconds();
				int32 TeamID = static_cast<int32>(AttackingVehicle->OwnerTeam);
				UpdateRelationshipForDamage(TeamID, CurrentTime);
			}
		}
	}
	
	if (CurrentHealth <= 0.0f)
	{
		CurrentHealth = 0.0f;
		
		// Unmark all resources near GuardCenter as no longer Kaiju-guarded
		TArray<AActor*> AllResources;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), AllResources);
		
		for (AActor* Actor : AllResources)
		{
			AResourceActor* Resource = Cast<AResourceActor>(Actor);
			if (Resource && Resource->bKaijuGuarded)
			{
				// Check if this resource was guarded by THIS Kaiju (within GuardRadius of GuardCenter)
				float DistanceToGuardCenter = FVector::Dist(Resource->GetActorLocation(), GuardCenter);
				if (DistanceToGuardCenter <= GuardRadius * 1.5f) // Use 1.5x for safety margin
				{
					Resource->bKaijuGuarded = false;
				}
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Kaiju defeated! Unmarked guarded resources near GuardCenter."));
		
		// TODO: Death/destruction
		Destroy();
		return;
	}
	
	UpdateHealthBar();
	
	// If attacked by a vehicle, immediately become alerted/hostile to attacker
	// This overrides resting state and awareness zones - being shot at is instant alert
	// However, don't switch targets if already engaged - stick with current target like vehicles do
	if (AttackingActor)
	{
		AVehicleActor* AttackingVehicle = Cast<AVehicleActor>(AttackingActor);
		if (AttackingVehicle)
		{
			// Only respond to attacker if we don't have a current target
			if (!CurrentTarget || !IsValid(CurrentTarget))
			{
				// Interrupt current state and attack the aggressor
				if (CurrentState == EKaijuState::Idle || CurrentState == EKaijuState::Returning)
				{
					TransitionToAlerted(AttackingVehicle);
				}
				else if (CurrentState == EKaijuState::Alerted)
				{
					// If already alerted, switch to attacking this new threat
					TransitionToAttacking(AttackingVehicle);
				}
			}
			// If we already have a target, keep attacking it - don't switch targets mid-combat
		}
	}
}

void AKaijuActor::UpdateHealthBar()
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

	// Show health bar if recently changed
	bool bShouldShowHealthBar = bRecentHealthChange;

	HealthBarWidget->SetVisibility(bShouldShowHealthBar);

	// Update the widget's health percentage
	if (UHealthBarWidget* HealthWidget = Cast<UHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
	{
		float HealthPercent = CurrentHealth / MaxHealth;
		HealthWidget->SetHealthPercent(HealthPercent);
	}
}

void AKaijuActor::UpdateInfoDisplay()
{
	if (!InfoWidget) return;
	
	UInfoUIWidget* InfoUIWidget = Cast<UInfoUIWidget>(InfoWidget->GetUserWidgetObject());
	if (!InfoUIWidget) return;
	
	// Line 1: Size variant
	FText Line1;
	switch (KaijuSize)
	{
		case EKaijuSize::Small:
			Line1 = FText::FromString(TEXT("Small Kaiju"));
			break;
		case EKaijuSize::Medium:
			Line1 = FText::FromString(TEXT("Medium Kaiju"));
			break;
		case EKaijuSize::Large:
			Line1 = FText::FromString(TEXT("Large Kaiju"));
			break;
		default:
			Line1 = FText::FromString(TEXT("Kaiju"));
			break;
	}
	
	// Line 2: Team (only if owned/domesticated - for now all kaiju are wild so empty)
	FText Line2 = FText::GetEmpty();
	// TODO: When kaiju domestication is implemented, show team name here
	
	// Line 3: Empty
	FText Line3 = FText::GetEmpty();
	
	InfoUIWidget->SetInfoDisplay(Line1, Line2, Line3);
}

// ========== SIZE CONFIGURATION ==========

void AKaijuActor::ConfigureForSize(EKaijuSize Size)
{
	KaijuSize = Size;
	
	switch (Size)
	{
		case EKaijuSize::Small:
			// Small kaiju - fast, weak, guards small clusters
			MaxHealth = 300.0f;
			CurrentHealth = 300.0f;
			AttackDamage = 40.0f;
			AttackCooldown = 1.0f; // Double fire rate (was 2.0f)
			MovementSpeed = 100.0f; // 25% of original speed
			GuardRadius = 2000.0f;
			LeashRadius = 3500.0f;
			InvasionRadius = 4500.0f;
			BaseDetectionRadius = 1500.0f;
			
			// Mesh scale: 3.0 x 1.0 x 1.0
			if (KaijuMesh)
			{
				KaijuMesh->SetRelativeScale3D(FVector(3.0f, 1.0f, 1.0f));
			}
			if (SelectionBox)
			{
				// Selection box slightly larger than mesh
				SelectionBox->SetRelativeScale3D(FVector(3.3f, 1.1f, 1.1f));
			}
			if (CollisionCapsule)
			{
				CollisionCapsule->SetCapsuleSize(150.0f, 75.0f);
			}
			break;
			
		case EKaijuSize::Medium:
			// Medium kaiju - balanced
			MaxHealth = 500.0f;
			CurrentHealth = 500.0f;
			AttackDamage = 80.0f;
			AttackCooldown = 1.25f; // Double fire rate (was 2.5f)
			MovementSpeed = 75.0f; // 25% of original speed
			GuardRadius = 2500.0f;
			LeashRadius = 3500.0f;
			InvasionRadius = 4500.0f;
			BaseDetectionRadius = 1800.0f;
			
			// Mesh scale: 4.5 x 1.5 x 1.5
			if (KaijuMesh)
			{
				KaijuMesh->SetRelativeScale3D(FVector(4.5f, 1.5f, 1.5f));
			}
			if (SelectionBox)
			{
				// Selection box slightly larger than mesh
				SelectionBox->SetRelativeScale3D(FVector(4.8f, 1.6f, 1.6f));
			}
			if (CollisionCapsule)
			{
				CollisionCapsule->SetCapsuleSize(225.0f, 112.5f);
			}
			break;
			
		case EKaijuSize::Large:
			// Large kaiju - slow, powerful, guards best resources
			MaxHealth = 1000.0f;
			CurrentHealth = 1000.0f;
			AttackDamage = 120.0f;
			AttackCooldown = 1.5f; // Double fire rate (was 3.0f)
			MovementSpeed = 50.0f; // 25% of original speed
			GuardRadius = 3000.0f;
			LeashRadius = 3500.0f;
			InvasionRadius = 5000.0f;
			BaseDetectionRadius = 2200.0f;
			
			// Mesh scale: 6.0 x 2.0 x 2.0
			if (KaijuMesh)
			{
				KaijuMesh->SetRelativeScale3D(FVector(6.0f, 2.0f, 2.0f));
			}
			if (SelectionBox)
			{
				// Selection box slightly larger than mesh
				SelectionBox->SetRelativeScale3D(FVector(6.3f, 2.1f, 2.1f));
			}
			if (CollisionCapsule)
			{
				CollisionCapsule->SetCapsuleSize(300.0f, 150.0f);
			}
			break;
	}
}

void AKaijuActor::SetHovered(bool bHovered)
{
	// Show/hide hover highlight using selection box
	if (SelectionBox)
	{
		SelectionBox->SetVisibility(bHovered);
	}
	
	// Show/hide info widget
	if (InfoWidget)
	{
		InfoWidget->SetVisibility(bHovered);
		if (bHovered)
		{
			UpdateInfoDisplay();
		}
	}
}

// ========== RELATIONSHIP SYSTEM ==========

void AKaijuActor::UpdateRelationshipForDamage(int32 TeamID, float CurrentTime)
{
	// Set relationship to hostile (-100.0) and record damage time
	FKaijuTeamRelationship* Relationship = TeamRelationships.Find(TeamID);
	if (Relationship)
	{
		Relationship->RelationshipValue = -100.0f;
		Relationship->LastDamageTime = CurrentTime;
	}
	else
	{
		FKaijuTeamRelationship NewRelationship;
		NewRelationship.RelationshipValue = -100.0f;
		NewRelationship.LastDamageTime = CurrentTime;
		TeamRelationships.Add(TeamID, NewRelationship);
	}
}

bool AKaijuActor::IsHostileToTeam(int32 TeamID) const
{
	const FKaijuTeamRelationship* Relationship = TeamRelationships.Find(TeamID);
	if (Relationship)
	{
		return Relationship->RelationshipValue < 0.0f;
	}
	return false; // Neutral if no relationship exists
}

void AKaijuActor::UpdateRelationships(float CurrentTime)
{
	// Reset relationships to neutral after 30 seconds of no combat
	const float RelationshipResetTime = 30.0f;
	
	TArray<int32> TeamsToReset;
	for (auto& Pair : TeamRelationships)
	{
		int32 TeamID = Pair.Key;
		FKaijuTeamRelationship& Relationship = Pair.Value;
		
		// If relationship is hostile and enough time has passed, reset to neutral
		if (Relationship.RelationshipValue < 0.0f)
		{
			float TimeSinceLastDamage = CurrentTime - Relationship.LastDamageTime;
			if (TimeSinceLastDamage >= RelationshipResetTime)
			{
				TeamsToReset.Add(TeamID);
			}
		}
	}
	
	// Reset relationships to neutral
	for (int32 TeamID : TeamsToReset)
	{
		FKaijuTeamRelationship* Relationship = TeamRelationships.Find(TeamID);
		if (Relationship)
		{
			Relationship->RelationshipValue = 0.0f;
		}
	}
}

