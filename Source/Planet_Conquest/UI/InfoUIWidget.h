// InfoUIWidget.h
// Universal info UI widget that displays up to 3 lines of text for any hoverable object
// Supports: Resources, Vehicles, Turrets, Kaiju, Labs, Research Stations

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InfoUIWidget.generated.h"

/**
 * Universal info widget for displaying object information on hover
 * Uses a flexible 3-line text system:
 * - Line 1: Object type/name (e.g., "Orange Substrate", "Scout Vehicle", "Research Lab")
 * - Line 2: Ownership/control (e.g., "Team X", "Unclaimed", "TechCorp | Team X")
 * - Line 3: Stats/status (e.g., "Income: 85/cycle | Max: 100/cycle")
 */
UCLASS()
class PLANET_CONQUEST_API UInfoUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Line 1: Object type or name
	UPROPERTY(BlueprintReadWrite, Category = "Info Display")
	FText Line1Text;

	// Line 2: Ownership or control information
	UPROPERTY(BlueprintReadWrite, Category = "Info Display")
	FText Line2Text;

	// Line 3: Stats or status information
	UPROPERTY(BlueprintReadWrite, Category = "Info Display")
	FText Line3Text;

	/**
	 * Updates the info display with up to 3 lines of text
	 * @param Line1 - Object type/name (e.g., "Orange Substrate", "Research Lab")
	 * @param Line2 - Ownership info (e.g., "Team 1", "Unclaimed", "TechCorp | Team 2")
	 * @param Line3 - Stats/status (e.g., "Income: 50/cycle | Max: 100/cycle", or empty)
	 */
	UFUNCTION(BlueprintCallable, Category = "Info Display")
	void SetInfoDisplay(FText Line1, FText Line2, FText Line3);

	/**
	 * Getter functions for Blueprint binding
	 */
	UFUNCTION(BlueprintPure, Category = "Info Display")
	FText GetLine1Text() const { return Line1Text; }

	UFUNCTION(BlueprintPure, Category = "Info Display")
	FText GetLine2Text() const { return Line2Text; }

	UFUNCTION(BlueprintPure, Category = "Info Display")
	FText GetLine3Text() const { return Line3Text; }

	/**
	 * Checks if Line 3 has content (for conditional visibility in Blueprint)
	 * @return true if Line3Text is not empty
	 */
	UFUNCTION(BlueprintPure, Category = "Info Display")
	bool ShouldShowLine3() const;

	/**
	 * Gets visibility state for Line 3 (for Blueprint binding)
	 * @return Visible if Line3 has text, Collapsed otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "Info Display")
	ESlateVisibility GetLine3Visibility() const;

protected:
	virtual void NativeConstruct() override;
};
