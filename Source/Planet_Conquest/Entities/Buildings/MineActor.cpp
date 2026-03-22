// Copyright Epic Games, Inc. All Rights Reserved.

#include "MineActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "../Resources/ResourceActor.h"
#include "../../Core/PlanetConquestGameMode.h"
#include "../../UI/InfoUIWidget.h"
#include "DrawDebugHelpers.h"

AMineActor::AMineActor()
{
	// Enable ticking
	PrimaryActorTick.bCanEverTick = true;
	
	// Set health to match vehicles
	MaxHealth = 100.0f;
	CurrentHealth = 100.0f;
	
	// Override collision to QueryOnly (like resources) - mines should not physically block vehicles
	// This allows vehicles to move through mines to attack them
	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	
	// Scale the cube mesh to make it rectangular and rotate so long side is perpendicular
	if (BuildingMesh)
	{
		// Make it rectangular: long on X-axis, narrow on Y and Z
		BuildingMesh->SetRelativeScale3D(FVector(2.0f, 0.5f, 0.5f));
		
		// Rotate 90 degrees around Y-axis to swap the long axis orientation
		BuildingMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	}
	
	// Scale selection box to match rectangular mine shape
	if (SelectionBox)
	{
		SelectionBox->SetRelativeScale3D(FVector(2.2f, 0.6f, 0.6f)); // Slightly larger than mine
		SelectionBox->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f)); // Match mine rotation
	}
}

void AMineActor::BeginPlay()
{
	Super::BeginPlay();
}

void AMineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Draw ownership circle if owned by a team
	if (OwnerTeam != EOwnerTeam::Neutral)
	{
		DrawOwnershipCircle();
	}
}

void AMineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// When mine is destroyed, revert resource to neutral
	if (TargetResource && TargetResource->Mine == this)
	{
		// Log mine destruction for feed
		if (EndPlayReason == EEndPlayReason::Destroyed)
		{
			UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d's mine destroyed - resource %s reverted to neutral"), 
				(int32)OwnerTeam, *TargetResource->GetName());
		}
		
		// Clear the mine reference
		TargetResource->Mine = nullptr;
		
		// Revert resource to neutral (stops income, allows recapture)
		TargetResource->CaptureForTeam(EOwnerTeam::Neutral);
	}
	
	Super::EndPlay(EndPlayReason);
}

void AMineActor::DrawOwnershipCircle()
{
	if (!GetWorld() || !TargetResource)
	{
		return;
	}
	
	// Draw circle around the resource, not the mine building
	FVector ResourceLocation = TargetResource->GetActorLocation();
	FVector ResourceDirection = (ResourceLocation - PlanetCenter).GetSafeNormal();
	
	// Get team color
	FLinearColor TeamColor = GetTeamColor();
	
	// Draw circle on planet surface (smaller than city territory circles)
	const int32 NumSegments = 48; // Number of line segments to approximate circle
	const float MineRadius = 330.0f; // Radius around resource deposit
	
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
		FVector Offset1 = (Tangent1 * FMath::Cos(Angle1) + Tangent2 * FMath::Sin(Angle1)) * MineRadius;
		FVector Offset2 = (Tangent1 * FMath::Cos(Angle2) + Tangent2 * FMath::Sin(Angle2)) * MineRadius;
		
		// Project points back onto planet surface
		FVector Direction1 = (ResourceDirection * PlanetRadius + Offset1).GetSafeNormal();
		FVector Direction2 = (ResourceDirection * PlanetRadius + Offset2).GetSafeNormal();
		
		FVector Point1 = PlanetCenter + Direction1 * PlanetRadius;
		FVector Point2 = PlanetCenter + Direction2 * PlanetRadius;
		
		// Draw line segment (persistent for one frame, thicker and more visible)
		DrawDebugLine(GetWorld(), Point1, Point2, TeamColor.ToFColor(true), false, -1.0f, 0, 18.0f);
	}
}
void AMineActor::UpdateInfoDisplay()
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
	
	// Show income if there's a target resource
	if (TargetResource)
	{
		FString IncomeLine = FString::Printf(TEXT("%d/cycle"), TargetResource->IncomePerInterval);
		InfoUIWidget->SetInfoDisplay(
			FText::FromString("Mine"),
			FText::FromString(TeamName),
			FText::FromString(IncomeLine)
		);
	}
	else
	{
		InfoUIWidget->SetInfoDisplay(
			FText::FromString("Mine"),
			FText::FromString(TeamName),
			FText::FromString("")
		);
	}
}