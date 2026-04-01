// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleActor.h"
#include "ShipActor.generated.h"

/**
 * Ship - a water unit subclass of VehicleActor.
 * Floats at sea level, can only traverse ocean tiles.
 * Higher health and damage than ground vehicles, but slower.
 */
UCLASS()
class PLANET_CONQUEST_API AShipActor : public AVehicleActor
{
	GENERATED_BODY()

public:
	AShipActor();

	// Water unit flag - used by pathfinding and targeting to treat this as a sea unit
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
	bool bIsWaterUnit = true;

	// Override AlignToPlanet to float at sea level instead of terrain height
	virtual void AlignToPlanet() override;

	// Override CreatePathTo to skip land-based pathfinding — ships move directly
	virtual bool CreatePathTo(FVector Destination) override;

	// Override Tick to snap radial distance to sea level after base movement runs
	virtual void Tick(float DeltaTime) override;

private:
	// Throttles per-tick retries when a land target proved out of ship range last attempt
	FVector LastFailedLandTarget = FVector::ZeroVector;
};
