// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingActor.h"
#include "MineActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API AMineActor : public ABuildingActor
{
	GENERATED_BODY()
	
public:	
	AMineActor();

	// Reference to the resource this mine is extracting from
	UPROPERTY(BlueprintReadOnly, Category = "Mine")
	class AResourceActor* TargetResource = nullptr;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void UpdateInfoDisplay() override;

 private:
	void DrawOwnershipCircle();
};
