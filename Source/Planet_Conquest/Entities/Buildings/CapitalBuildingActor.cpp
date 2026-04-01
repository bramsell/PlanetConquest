// Copyright Benjamin Ramsell. All Rights Reserved.

#include "CapitalBuildingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "../../UI/InfoUIWidget.h"

ACapitalBuildingActor::ACapitalBuildingActor()
{
	BuildingType = EBuildingType::Capital;
	
	// Taller cube - skyscraper style (thinner X and Y)
	BuildingMesh->SetRelativeScale3D(FVector(2.5f, 2.5f, 8.0f)); // Tall thin rectangular block
	
	// Offset mesh upward so it sits on ground (not centered at ground)
	// Cube is 100 units, scaled to 800 in Z, so offset by 400 to sit on surface
	BuildingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 400.0f));
	
	// Larger collision for capital building
	CollisionSphere->SetSphereRadius(200.0f);
	
	// More health for capital
	MaxHealth = 2000.0f;
	CurrentHealth = 2000.0f;
}

void ACapitalBuildingActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACapitalBuildingActor::UpdateInfoDisplay()
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
		FText::FromString("Capital"),
		FText::FromString(TeamName),
		FText::FromString("")
	);
}
