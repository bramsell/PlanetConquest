// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuGameMode.generated.h"

/**
 * GameMode for the main menu level.
 *
 * Set this (or a Blueprint child of it) as the GameMode Override on
 * MainMenuLevel.  In the Blueprint child, assign MainMenuWidgetClass
 * to WBP_MainMenu so the asset reference stays in Blueprint.
 *
 * BeginPlay automatically:
 *   - Creates and displays the main menu widget
 *   - Switches input to UI-only mode
 *   - Shows the mouse cursor
 */
UCLASS()
class PLANET_CONQUEST_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AMainMenuGameMode();

	/**
	 * The widget class to spawn as the main menu.
	 * Set this to WBP_MainMenu in a Blueprint child of this class,
	 * or in the GameMode Override details panel on the level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

protected:

	virtual void BeginPlay() override;

private:

	UPROPERTY()
	UUserWidget* MainMenuWidget = nullptr;
};
