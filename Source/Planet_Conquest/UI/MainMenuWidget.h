// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

/**
 * Root widget for the main menu screen.
 *
 * Blueprint usage:
 *   1. RefreshAllSlots() is called automatically on construct.
 *   2. Bind each slot button's OnClicked to OnSlotClicked(0), OnSlotClicked(1), etc.
 *   3. Bind Confirm button's OnClicked to ConfirmSelection().
 *   4. Bind Cancel button's OnClicked to CancelSelection().
 *   5. Bind Quit button's OnClicked to QuitGame().
 *   6. Bind confirm panel visibility to bConfirmPanelVisible.
 *   7. Bind confirm button text to ConfirmButtonText.
 *   8. Use SlotLabels[0..3] and LastSavedTexts[0..3] to populate slot text blocks.
 */
UCLASS()
class PLANET_CONQUEST_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Per-slot display data  (arrays of 4, index 0-3)
	// -----------------------------------------------------------------------

	/** Label for each slot button. "Empty Slot" or "Save 1", "Save 2", etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	TArray<FText> SlotLabels;

	/** Whether each slot has an existing save file. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	TArray<bool> SlotHasSave;

	// -----------------------------------------------------------------------
	// Confirm-panel state
	// -----------------------------------------------------------------------

	/** Index of the slot the player clicked, -1 when none selected. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	int32 SelectedSlotIndex = -1;

	/** Whether the confirm/cancel panel should be visible. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	bool bConfirmPanelVisible = false;

	/** Large title in the confirm panel. e.g. "Start New Game" or "Load Save 2" */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	FText ConfirmTitleText;

	/** Last-saved date string. Empty for new/empty slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	FText ConfirmDateText;

	/** Playtime string e.g. "14h 32m". Empty for new/empty slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	FText ConfirmPlaytimeText;

	/** Text shown on the confirm button ("Start" or "Load"). */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	FText ConfirmButtonText;

	// -----------------------------------------------------------------------
	// Functions bound in Blueprint
	// -----------------------------------------------------------------------

	/** Reads all 4 save slots and refreshes display arrays. Called automatically
	 *  on construct; call again if you ever need to force a refresh. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RefreshAllSlots();

	/** Bind each slot button's OnClicked to this, passing the correct index (0-3). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OnSlotClicked(int32 InSlotIndex);

	/** Bind the Confirm button's OnClicked to this. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ConfirmSelection();

	/** Bind the Cancel button's OnClicked to this. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void CancelSelection();

	/** Bind the Quit button's OnClicked to this. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitGame();

	/** Deletes the save file for the selected slot and returns to the slot list.
	 *  Only has an effect when SelectedSlotIndex points to an existing save. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void DeleteSave();

	/** Bind the confirm panel's Visibility property to this. */
	UFUNCTION(BlueprintPure, Category = "Menu")
	ESlateVisibility GetConfirmPanelVisibility() const;

	/** Bind the Delete button's Visibility property to this.
	 *  Visible only when the selected slot has an existing save file. */
	UFUNCTION(BlueprintPure, Category = "Menu")
	ESlateVisibility GetDeleteButtonVisibility() const;

protected:
	virtual void NativeConstruct() override;
};
