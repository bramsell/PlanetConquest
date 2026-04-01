// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlanetConquestHUD.generated.h"

UCLASS()
class PLANET_CONQUEST_API APlanetConquestHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	virtual void DrawHUD() override;

	// Box selection drawing
	UPROPERTY(BlueprintReadWrite, Category = "Selection")
	bool bDrawSelectionBox = false;

	UPROPERTY(BlueprintReadWrite, Category = "Selection")
	FVector2D SelectionBoxStart;

	UPROPERTY(BlueprintReadWrite, Category = "Selection")
	FVector2D SelectionBoxEnd;
};
