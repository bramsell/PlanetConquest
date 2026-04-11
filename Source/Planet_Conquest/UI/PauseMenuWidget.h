// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

/**
 * In-game pause menu.
 *
 * Blueprint usage (WBP_PauseMenu):
 *   1. Parent class: PauseMenuWidget
 *   2. Bind the Save button's OnClicked  to SaveGame()
 *   3. Bind the Resume button's OnClicked to ResumeGame()
 *   4. Bind the Quit button's OnClicked   to QuitToMainMenu()
 *   5. Bind SaveConfirmText visibility / text for save feedback
 *
 * The GameHUD widget opens this menu by calling OpenPauseMenu() on itself,
 * which creates and displays this widget and pauses the game.
 */
UCLASS()
class PLANET_CONQUEST_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------

	/** Feedback text after saving ("Game Saved!" or an error message).
	 *  Empty until the player clicks Save. */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	FText SaveConfirmText;

	/** True while a save is in progress (can be used to disable the button). */
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	bool bSaveInProgress = false;

	// -----------------------------------------------------------------------
	// Functions bound in Blueprint
	// -----------------------------------------------------------------------

	/** Saves the current game to the active slot and shows SaveConfirmText. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void SaveGame();

	/** Unpauses the game and removes this widget from the viewport. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ResumeGame();

	/** Saves, then returns to the main menu level. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitToMainMenu();

protected:

	virtual void NativeConstruct() override;
};
