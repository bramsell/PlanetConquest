// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingActor.h"
#include "TurretBuildingActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API ATurretBuildingActor : public ABuildingActor
{
	GENERATED_BODY()
	
public:	
	ATurretBuildingActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Combat properties (similar to vehicle but stationary)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float FireRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float FireRate = 1.0f; // Shots per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float ProjectileDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    int32 BuildCost = 2000;

	void UpdateInfoDisplay();

	virtual void SetSelected(bool bSelected) override;

private:
	float TimeSinceLastShot = 0.0f;

	// Find and fire at enemies
	void FindAndFireAtEnemies();
	void FireAtTarget(AActor* Target);
};
