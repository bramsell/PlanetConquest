// Copyright Epic Games, Inc. All Rights Reserved.

#include "FactoryBuildingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "../Cities/CityActor.h"
#include "../../Core/PlanetConquestPlayerController.h"
#include "../../Core/AITeamController.h"
#include "../../UI/InfoUIWidget.h"
#include "Kismet/GameplayStatics.h"

AFactoryBuildingActor::AFactoryBuildingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	
	BuildingType = EBuildingType::Factory;
	
	// Smaller, thinner factory - more refined proportions
	BuildingMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 5.0f)); // Smaller and thinner
	
	// Offset mesh upward so it sits on ground
	// Cube is 100 units, scaled to 500 in Z, so offset by 250 to sit on surface
	BuildingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f));
	
	// Smaller collision for factory
	CollisionSphere->SetSphereRadius(100.0f);
	
	// Medium health for factory
	MaxHealth = 300.0f;
	CurrentHealth = 300.0f;
}

void AFactoryBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Factory income is now generated globally every 5 seconds, not per-factory
}

void AFactoryBuildingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Income generation removed - now handled by global 5-second timer in PlanetActor
}

void AFactoryBuildingActor::GenerateIncome()
{
	// Give income to the owning team
	if (OwnerTeam == EOwnerTeam::Player)
	{
		// Player income - already handled by CollectIncome() in PlayerController
		// No bonus needed here anymore
	}
		else if (OwnerTeam != EOwnerTeam::Neutral)
	{
		// AI income - already handled by CollectEnemyIncome() in PlanetActor
		// No bonus needed here anymore
	}
}

void AFactoryBuildingActor::UpdateInfoDisplay()
{
	if (!InfoWidget) return;
	
	UInfoUIWidget* InfoUIWidget = Cast<UInfoUIWidget>(InfoWidget->GetUserWidgetObject());
	if (!InfoUIWidget) return;
	
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
		FText::FromString("Factory"),
		FText::FromString(TeamName),
		FText::FromString("")
	);
}
