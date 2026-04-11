// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PlanetConquestGameInstance.generated.h"

/**
 * Singleton that persists across level loads (main menu ↔ game world).
 *
 * Responsibilities:
 *   - Know which save slot is active so PlanetActor and future systems
 *     can read/write the correct file.
 *   - Know whether we are starting a new game or loading an existing one.
 *
 * Usage:
 *   // In the main-menu widget, before opening World1:
 *   GameInstance->SetActiveSlot(2, false);            // slot 2, new game
 *   UGameplayStatics::OpenLevel(this, "World1");
 *
 *   // In PlanetActor::BeginPlay:
 *   FString SlotName = GameInstance->GetActiveSlotName();
 *   bool bIsLoad     = GameInstance->bLoadingExistingGame;
 */
UCLASS()
class PLANET_CONQUEST_API UPlanetConquestGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Active slot state  (set by the main-menu widget before opening World1)
	// -----------------------------------------------------------------------

	/** 0-based index of the slot the player chose (0–3 for four slots). */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 ActiveSlotIndex = 0;

	/** True  → load an existing save.
	 *  False → start fresh (seed will be generated and written on first launch). */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bLoadingExistingGame = false;

	/** Set to true when SetActiveSlot() is called from the main-menu widget.
	 *  Stays false during a direct PIE launch (no menu involved).
	 *  PlanetActor uses this to distinguish "player chose New Game" from
	 *  "editor Play button pressed" so existing seeds aren't regenerated
	 *  on every PIE session. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bSlotWasSetByMenu = false;

	// -----------------------------------------------------------------------
	// Helpers (callable from Blueprint and C++)
	// -----------------------------------------------------------------------

	/**
	 * Call this from the main-menu widget when the player confirms a slot.
	 * @param SlotIndex  0-based slot index (0–3).
	 * @param bIsLoad    true = load existing game, false = new game.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetActiveSlot(int32 SlotIndex, bool bIsLoad);

	/**
	 * Returns the save-slot string for the active slot index,
	 * e.g. "PlanetConquest_Save_0".
	 * PlanetActor uses this to know which .sav file to read/write.
	 */
	UFUNCTION(BlueprintPure, Category = "Save")
	FString GetActiveSlotName() const;

	/**
	 * Returns the save-slot string for an arbitrary slot index.
	 * Used by the main-menu widget to populate slot previews.
	 */
	UFUNCTION(BlueprintPure, Category = "Save")
	FString GetSlotNameForIndex(int32 SlotIndex) const;
};
