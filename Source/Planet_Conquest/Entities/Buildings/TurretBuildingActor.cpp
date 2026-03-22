// Copyright Epic Games, Inc. All Rights Reserved.

#include "TurretBuildingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "../Vehicles/VehicleActor.h"
#include "../Cities/CityActor.h"
#include "../Projectiles/ProjectileActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "../../Core/PlanetConquestGameMode.h"
#include "../../UI/InfoUIWidget.h"
#include "UObject/ConstructorHelpers.h"

ATurretBuildingActor::ATurretBuildingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	
	BuildingType = EBuildingType::Turret;
	
	// Small flat ring - same as resources
	// Load cylinder mesh
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh)
	{
		BuildingMesh->SetStaticMesh(CylinderMesh);
	}
	
	BuildingMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.3f)); // Flat ring like resources
	
	// Offset mesh upward so it sits on ground
	// Cube is 100 units, scaled to 30 in Z, so offset by 15 to sit on surface
	BuildingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 15.0f));
	
	// Small collision for turret
	CollisionSphere->SetSphereRadius(100.0f);
	
	// Low health for turret
	MaxHealth = 200.0f;
	CurrentHealth = 200.0f;
	
	// Fire rate twice as fast as vehicles (vehicles = 0.33 shots/sec, turrets = 0.67 shots/sec)
	FireRate = 0.67f;
	
	// Build cost
	BuildCost = 2000;
}

void ATurretBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	
	TimeSinceLastShot = 0.0f;
	
	// Initialize InfoWidget display
	UpdateInfoDisplay();
}

void ATurretBuildingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	TimeSinceLastShot += DeltaTime;
	
	// Check fire rate
	float ShotInterval = 1.0f / FireRate;
	if (TimeSinceLastShot >= ShotInterval)
	{
		FindAndFireAtEnemies();
		TimeSinceLastShot = 0.0f;
	}
}

void ATurretBuildingActor::FindAndFireAtEnemies()
{
	FVector TurretLocation = GetActorLocation();
	
	// Get GameMode for relationship checks
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	const float HostilityThreshold = -0.5f; // Fire on enemies (relationship <= -0.5)
	
	// First priority: Enemy vehicles
	TArray<AActor*> AllVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
	
	AActor* NearestEnemy = nullptr;
	float NearestDistance = FireRange;
	
	for (AActor* Actor : AllVehicles)
	{
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (Vehicle && Vehicle->OwnerTeam != OwnerTeam && Vehicle->OwnerTeam != EOwnerTeam::Neutral)
		{
			// Check relationship - only fire if hostile enough
			if (GameMode)
			{
				float Relationship = GameMode->GetRelationship(OwnerTeam, Vehicle->OwnerTeam);
				if (Relationship > HostilityThreshold)
				{
					continue; // Not hostile enough, skip this target
				}
			}
			
			float Distance = FVector::Dist(TurretLocation, Vehicle->GetActorLocation());
			if (Distance <= FireRange && Distance < NearestDistance)
			{
				NearestEnemy = Vehicle;
				NearestDistance = Distance;
			}
		}
	}
	
	// Second priority: Enemy cities (if no vehicles in range)
	if (!NearestEnemy)
	{
		TArray<AActor*> AllCities;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), AllCities);
		
		for (AActor* Actor : AllCities)
		{
			ACityActor* City = Cast<ACityActor>(Actor);
			if (City && City->OwnerTeam != OwnerTeam && City->OwnerTeam != EOwnerTeam::Neutral)
			{
				// Check relationship - only fire if hostile enough
				if (GameMode)
				{
					float Relationship = GameMode->GetRelationship(OwnerTeam, City->OwnerTeam);
					if (Relationship > HostilityThreshold)
					{
						continue; // Not hostile enough, skip this target
					}
				}
				
				float Distance = FVector::Dist(TurretLocation, City->GetActorLocation());
				if (Distance <= FireRange && Distance < NearestDistance)
				{
					NearestEnemy = City;
					NearestDistance = Distance;
				}
			}
		}
	}
	
	// Fire at nearest enemy
	if (NearestEnemy)
	{
		FireAtTarget(NearestEnemy);
	}
}

void ATurretBuildingActor::FireAtTarget(AActor* Target)
{
	if (!Target || !GetWorld()) return;
	
	FVector TurretLocation = GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	
	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = nullptr;
	
	AProjectileActor* Projectile = GetWorld()->SpawnActor<AProjectileActor>(AProjectileActor::StaticClass(), TurretLocation, FRotator::ZeroRotator, SpawnParams);
	
	if (Projectile)
	{
		// Configure projectile
		Projectile->TargetLocation = TargetLocation;
		Projectile->TargetActor = Target;
		Projectile->Damage = ProjectileDamage;
		Projectile->OwnerTeam = OwnerTeam;
		Projectile->PlanetCenter = PlanetCenter;
		Projectile->PlanetRadius = PlanetRadius;
	}
}

void ATurretBuildingActor::SetSelected(bool bSelected)
{
	// Call parent implementation to handle SelectionBox
	Super::SetSelected(bSelected);
	
	// Show/hide InfoWidget based on selection
	if (InfoWidget)
	{
		InfoWidget->SetVisibility(bSelected);
		
		if (bSelected)
		{
			UpdateInfoDisplay();
		}
	}
}

void ATurretBuildingActor::UpdateInfoDisplay()
{
	if (!InfoWidget) return;
	
	UInfoUIWidget* InfoUIWidget = Cast<UInfoUIWidget>(InfoWidget->GetUserWidgetObject());
	if (!InfoUIWidget) return;
	
	// Line 1: "Turret"
	FText Line1 = FText::FromString(TEXT("Turret"));
	
	// Line 2: Team name
	FText Line2;
	switch (OwnerTeam)
	{
		case EOwnerTeam::Player:
			Line2 = FText::FromString(TEXT("Player"));
			break;
		case EOwnerTeam::AI1:
			Line2 = FText::FromString(TEXT("Team 1"));
			break;
		case EOwnerTeam::AI2:
			Line2 = FText::FromString(TEXT("Team 2"));
			break;
		case EOwnerTeam::AI3:
			Line2 = FText::FromString(TEXT("Team 3"));
			break;
		case EOwnerTeam::AI4:
			Line2 = FText::FromString(TEXT("Team 4"));
			break;
		case EOwnerTeam::AI5:
			Line2 = FText::FromString(TEXT("Team 5"));
			break;
		case EOwnerTeam::AI6:
			Line2 = FText::FromString(TEXT("Team 6"));
			break;
		case EOwnerTeam::AI7:
			Line2 = FText::FromString(TEXT("Team 7"));
			break;
		case EOwnerTeam::Neutral:
			Line2 = FText::FromString(TEXT("Neutral"));
			break;
		default:
			Line2 = FText::FromString(TEXT("Unknown"));
			break;
	}
	
	// Line 3: Empty
	FText Line3 = FText::GetEmpty();
	
	// Update widget
	InfoUIWidget->SetInfoDisplay(Line1, Line2, Line3);
}
