// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourceUIWidget.generated.h"

/**
 * Resource hover UI widget showing income per cycle
 * Aggressive mode: 1 cycle = 5 seconds
 * Sustainable mode: 1 cycle = 15 seconds
 */
UCLASS()
class PLANET_CONQUEST_API UResourceUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Resource type name ("Orange Substrate" or "Black Substrate")
	UPROPERTY(BlueprintReadWrite, Category = "Resource")
	FText ResourceTypeName;

	// Current income per cycle (affected by resource health)
	UPROPERTY(BlueprintReadWrite, Category = "Resource")
	int32 CurrentIncomePerCycle = 0;

	// Maximum income per cycle (full health)
	UPROPERTY(BlueprintReadWrite, Category = "Resource")
	int32 MaxIncomePerCycle = 0;

	// Is this a green substrate resource? (different display logic)
	UPROPERTY(BlueprintReadWrite, Category = "Resource")
	bool bIsGreenSubstrate = false;

	// Update the resource income display
	UFUNCTION(BlueprintCallable, Category = "Resource")
	void UpdateDisplay(FText TypeName, int32 CurrentIncome, int32 MaxIncome, bool bIsGreen = false);

	// Check if max income should be shown (hidden for green substrate - doesn't deplete)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Resource")
	bool ShouldShowMaxIncome() const;

	// Get visibility for max income display (for binding)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Resource")
	ESlateVisibility GetMaxIncomeVisibility() const;

protected:
	virtual void NativeConstruct() override;
};
