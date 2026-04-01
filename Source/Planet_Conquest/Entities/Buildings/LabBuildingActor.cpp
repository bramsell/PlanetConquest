// Copyright Benjamin Ramsell. All Rights Reserved.

#include "LabBuildingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "../Cities/CityActor.h"
#include "../../Core/PlanetConquestPlayerController.h"
#include "../../Core/AITeamController.h"
#include "../../UI/InfoUIWidget.h"
#include "Kismet/GameplayStatics.h"

ALabBuildingActor::ALabBuildingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	
	BuildingType = EBuildingType::Lab;
	
	// Main horizontal building (3x1x1 ratio)
	// Base cube is 100 units, so scaling gives us 300x100x100
	BuildingMesh->SetRelativeScale3D(FVector(3.0f, 1.0f, 1.0f));
	
	// Offset upward so it sits on ground (50 units up for half the height)
	BuildingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	
	// Create the detached cube building (1x1x1 ratio)
	CubeBuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeBuildingMesh"));
	CubeBuildingMesh->SetupAttachment(RootComponent);
	
	// Use the same cube mesh as the main building
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		CubeBuildingMesh->SetStaticMesh(CubeMeshAsset.Object);
	}
	
	// Position the cube to form an L shape - at left end, perpendicular to horizontal building
	// Horizontal building spans -150 to +150 in X (300 units wide), -50 to +50 in Y
	// Cube at X=-100 aligns its left edge (-150) with horizontal building's left edge
	// Cube at Y=175 creates nice gap (75 units) between the two buildings
	CubeBuildingMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	CubeBuildingMesh->SetRelativeLocation(FVector(-100.0f, 175.0f, 50.0f)); // L-shape: aligned left, with gap
	
	// Larger collision for lab complex
	CollisionSphere->SetSphereRadius(200.0f);
	
	// Higher health for lab
	MaxHealth = 400.0f;
	CurrentHealth = 400.0f;
}

void ALabBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Set cube building color to match team
	if (CubeBuildingMesh)
	{
		UMaterialInstanceDynamic* CubeDynamicMat = CubeBuildingMesh->CreateDynamicMaterialInstance(0);
		if (CubeDynamicMat)
		{
			FLinearColor TeamColor = FLinearColor::White;
			
			switch (OwnerTeam)
			{
			case EOwnerTeam::Player:
				TeamColor = FLinearColor(0.0f, 0.5f, 1.0f); // Blue
				break;
			case EOwnerTeam::AI1:
				TeamColor = FLinearColor(1.0f, 0.0f, 0.0f); // Red
				break;
			case EOwnerTeam::AI2:
				TeamColor = FLinearColor(0.0f, 1.0f, 0.0f); // Green
				break;
			case EOwnerTeam::AI3:
				TeamColor = FLinearColor(1.0f, 1.0f, 0.0f); // Yellow
				break;
			case EOwnerTeam::AI4:
				TeamColor = FLinearColor(1.0f, 0.0f, 1.0f); // Magenta
				break;
			case EOwnerTeam::AI5:
				TeamColor = FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
				break;
			case EOwnerTeam::AI6:
				TeamColor = FLinearColor(1.0f, 0.5f, 0.0f); // Orange
				break;
			case EOwnerTeam::AI7:
				TeamColor = FLinearColor(0.5f, 0.0f, 1.0f); // Purple
				break;
			default:
				TeamColor = FLinearColor(0.5f, 0.5f, 0.5f); // Gray for neutral
				break;
			}
			
			CubeDynamicMat->SetVectorParameterValue(FName("BaseColor"), TeamColor);
		}
	}
	
	// Initialize info widget
	UpdateInfoDisplay();
}

void ALabBuildingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Research tick (placeholder for now - will implement discovery system later)
	TickResearch(DeltaTime);
}

void ALabBuildingActor::TickResearch(float DeltaTime)
{
	// Accumulate time since last research cycle
	TimeSinceLastResearchCycle += DeltaTime;
	
	// Check if a research cycle has completed (10 minutes)
	if (TimeSinceLastResearchCycle >= ResearchCycleInterval)
	{
		TimeSinceLastResearchCycle = 0.0f;
		
		// TODO: Implement probabilistic discovery system
		// For now, just log that a research cycle completed
		UE_LOG(LogTemp, Log, TEXT("Lab: Research cycle completed. Nationalized: %s"), 
			bIsNationalized ? TEXT("Yes") : TEXT("No"));
	}
}

void ALabBuildingActor::ToggleNationalization()
{
	bIsNationalized = !bIsNationalized;
	
	if (bIsNationalized)
	{
		AssignedCompany = "";
		UE_LOG(LogTemp, Log, TEXT("Lab: Nationalized"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Lab: Privatized - awaiting company assignment"));
	}
}

void ALabBuildingActor::Nationalize()
{
	bIsNationalized = true;
	AssignedCompany = "";
	UE_LOG(LogTemp, Log, TEXT("Lab: Nationalized"));
}

void ALabBuildingActor::Privatize(const FString& CompanyName)
{
	bIsNationalized = false;
	AssignedCompany = CompanyName;
	UE_LOG(LogTemp, Log, TEXT("Lab: Privatized to company: %s"), *CompanyName);
}

void ALabBuildingActor::SetSelected(bool bSelected)
{
	// Call parent implementation to handle selection box
	Super::SetSelected(bSelected);
	
	// Show/hide info widget
	if (InfoWidget)
	{
		InfoWidget->SetVisibility(bSelected);
		if (bSelected)
		{
			UpdateInfoDisplay();
		}
	}
}

void ALabBuildingActor::UpdateInfoDisplay()
{
	if (!InfoWidget) return;
	
	UInfoUIWidget* InfoUIWidget = Cast<UInfoUIWidget>(InfoWidget->GetUserWidgetObject());
	if (!InfoUIWidget) return;
	
	// Line 1: Lab type
	FText Line1 = FText::FromString(TEXT("Research Lab"));
	
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
	
	// Line 3: Empty for now (company names later)
	FText Line3 = FText::GetEmpty();
	
	InfoUIWidget->SetInfoDisplay(Line1, Line2, Line3);
}
