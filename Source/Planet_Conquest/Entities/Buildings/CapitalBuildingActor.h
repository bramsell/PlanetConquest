// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingActor.h"
#include "CapitalBuildingActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API ACapitalBuildingActor : public ABuildingActor
{
	GENERATED_BODY()
	
public:	
	ACapitalBuildingActor();

protected:
	virtual void BeginPlay() override;

	virtual void UpdateInfoDisplay() override;
};
