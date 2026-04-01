// Copyright Benjamin Ramsell. All Rights Reserved.

#include "ShipActor.h"
#include "../../World/PlanetActor.h"

AShipActor::AShipActor()
{
	// --- Stats: tankier, harder-hitting, longer range than ground vehicles ---
	MaxHealth = 200.0f;
	CurrentHealth = 200.0f;
	AttackDamage = 30.0f;
	AttackRange = 8000.0f;  // Long coastal bombardment range
	MovementSpeed = 1500.0f; // Fast at sea

	// Ships fire while sailing — don't stop when a target enters attack range
	bCanMoveWhileFiring = true;

	// Ships stay in water — they can destroy mines but can't move onto land to capture resources
	bCanCaptureResources = false;

	// Shell arc: lift projectiles 800 units above sea level at mid-flight so they
	// clear terrain instead of clipping through the planet mesh
	ProjectileArcHeight = 800.0f;

	// Elongated hull shape, double size in every dimension vs ground vehicle (ground = 0.5, so ship = 1.0 base * hull ratios)
	VehicleMesh->SetRelativeScale3D(FVector(7.0f, 2.0f, 1.2f));
	SelectionBox->SetRelativeScale3D(FVector(8.0f, 2.8f, 2.0f));
}

bool AShipActor::CreatePathTo(FVector Destination)
{
	if (!OwningPlanet) return false;

	float PlanetR = OwningPlanet->PlanetRadius;
	float SeaLevelRadius = PlanetR * (1.0f + OwningPlanet->SeaLevel);

	FVector Dir = (Destination - PlanetCenter).GetSafeNormal();
	if (Dir.IsNearlyZero()) return false;

	// If destination is on land, find the nearest coastal approach point by walking
	// from the target direction toward the ship until we reach water, then add a buffer.
	if (OwningPlanet->IsPointOnLand(Dir))
	{
		// Skip if we already tried and failed this land target this frame to
		// avoid burning CPU on an unreachable target every single Tick.
		if (Destination.Equals(LastFailedLandTarget, 200.0f))
		{
			return false;
		}

		// Find the continent center for this land point and scan away from it.
		// Rotating Dir around Cross(ContDir, Dir) sweeps it further away from the
		// continent center — i.e., toward the ocean.
		int32 ContId = OwningPlanet->GetContinentIdForPoint(Dir);
		if (!OwningPlanet->ContinentSeeds.IsValidIndex(ContId))
		{
			LastFailedLandTarget = Destination;
			LastPathfindingError = TEXT("Could not determine continent for target");
			CurrentPath.Empty();
			bHasTarget = false;
			return false;
		}
		FVector ContDir = OwningPlanet->ContinentSeeds[ContId].Position;

		// Rotation axis: sweeps Dir away from the continent center along the great circle
		FVector RotAxis = FVector::CrossProduct(ContDir, Dir).GetSafeNormal();
		if (RotAxis.IsNearlyZero())
		{
			// Target is exactly at or opposite the continent center — degenerate case
			LastFailedLandTarget = Destination;
			LastPathfindingError = TEXT("Target coincides with continent center");
			CurrentPath.Empty();
			bHasTarget = false;
			return false;
		}

		// Walk in 300-unit angular steps (fine enough to catch thin peninsulas)
		const float StepDist  = 300.0f;
		const float StepAngle = StepDist / PlanetR;
		const int32 MaxSteps  = 200; // 60 000 units max scan

		bool  bFoundWater = false;
		float CoastAngle  = 0.0f;

		for (int32 i = 1; i <= MaxSteps; i++)
		{
			FQuat   Rot    (RotAxis, StepAngle * static_cast<float>(i));
			FVector TestDir = Rot.RotateVector(Dir).GetSafeNormal();

			// IsPointOnLand now checks height > SeaLevel exactly (no extra buffer),
			// so this stops precisely at the visual coastline.
			if (!OwningPlanet->IsPointOnLand(TestDir))
			{
				CoastAngle  = StepAngle * static_cast<float>(i);
				bFoundWater = true;
				break;
			}
		}

		if (!bFoundWater)
		{
			LastFailedLandTarget = Destination;
			LastPathfindingError = TEXT("No coastline found — target unreachable by ship");
			CurrentPath.Empty();
			bHasTarget = false;
			return false;
		}

		// Add 2000-unit buffer past the coastline so the ship stays safely in water
		const float BufferAngle = CoastAngle + (2000.0f / PlanetR);
		FQuat  BufferRot(RotAxis, BufferAngle);
		FVector CoastDir = BufferRot.RotateVector(Dir).GetSafeNormal();

		// Verify the coastal position is within firing range of the inland target
		FVector CoastalPos   = PlanetCenter + CoastDir * (SeaLevelRadius + 50.0f);
		float   DistToTarget = FVector::Dist(CoastalPos, Destination);

		if (DistToTarget > AttackRange)
		{
			LastFailedLandTarget = Destination;
			LastPathfindingError = FString::Printf(
				TEXT("Target too far inland for ship bombardment (%.0f > %.0f)"),
				DistToTarget, AttackRange);
			CurrentPath.Empty();
			bHasTarget = false;
			return false;
		}

		Dir = CoastDir;
	}

	// Clear the last-failed cache on any successful path
	LastFailedLandTarget = FVector::ZeroVector;

	FVector SeaLevelDest = PlanetCenter + Dir * (SeaLevelRadius + 50.0f);

	CurrentPath.Empty();
	CurrentPath.Add(SeaLevelDest);
	CurrentWaypointIndex = 0;
	bHasTarget = true;
	TargetLocation = SeaLevelDest;
	LastPathfindingError.Empty();

	if (TargetMarker && IsValid(TargetMarker))
	{
		TargetMarker->SetWorldLocation(SeaLevelDest);
		TargetMarker->SetVisibility(true);
		FRotator MarkerRotation = FRotationMatrix::MakeFromZ(Dir).Rotator();
		TargetMarker->SetWorldRotation(MarkerRotation);
	}

	return true;
}

void AShipActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// After base movement positions the ship (which uses terrain height), snap back to sea level.
	// This keeps the ship at the correct radial distance while preserving the angular position
	// and rotation computed by the base class great-circle movement.
	if (OwningPlanet)
	{
		FVector Dir = (GetActorLocation() - PlanetCenter).GetSafeNormal();
		if (!Dir.IsNearlyZero())
		{
			float SeaLevelRadius = OwningPlanet->PlanetRadius * (1.0f + OwningPlanet->SeaLevel) + 50.0f;
			SetActorLocation(PlanetCenter + Dir * SeaLevelRadius);
		}
	}

	// Ships fire while moving — if the base combat logic stopped the path because we entered
	// attack range, but we haven't actually reached the destination waypoint yet, re-issue the
	// path so the ship keeps sailing while auto-fire handles combat independently.
	if (!bHasTarget && !TargetLocation.IsZero() && CurrentPath.Num() == 0)
	{
		float DistToTarget = FVector::Dist(GetActorLocation(), TargetLocation);
		if (DistToTarget > 500.0f)
		{
			CreatePathTo(TargetLocation);
		}
	}
}

void AShipActor::AlignToPlanet()
{
	FVector VehicleLocation = GetActorLocation();
	FVector Direction = (VehicleLocation - PlanetCenter).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		Direction = FVector(0.0f, 0.0f, 1.0f);
	}

	// Float at sea level surface (PlanetRadius * (1 + SeaLevel)) + hover offset
	float SeaLevelRadius = PlanetRadius;
	if (OwningPlanet)
	{
		SeaLevelRadius = OwningPlanet->PlanetRadius * (1.0f + OwningPlanet->SeaLevel);
	}

	FVector NewLocation = PlanetCenter + Direction * (SeaLevelRadius + 50.0f);
	SetActorLocation(NewLocation);

	// Orient so Z-axis (up) points away from planet center
	FRotator LookRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
	SetActorRotation(LookRotation);
}
