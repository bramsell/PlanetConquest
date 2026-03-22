// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingActor.h"
#include "FactoryBuildingActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API AFactoryBuildingActor : public ABuildingActor
{
	GENERATED_BODY()
	
public:	
	AFactoryBuildingActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Income generation properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory")
	int32 IncomePerGeneration = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory")
	float GenerationInterval = 5.0f; // Seconds

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Factory")
    int32 BuildCost = 1000;

	virtual void UpdateInfoDisplay() override;

private:
	float TimeSinceLastGeneration = 0.0f;

	void GenerateIncome();
};
