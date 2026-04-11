// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PauseMenuWidget.h"
#include "../Core/PlanetConquestSaveGame.h"
#include "../Core/PlanetConquestGameInstance.h"
#include "../Core/PlanetConquestGameMode.h"
#include "../Core/PlanetConquestPlayerController.h"
#include "GameHUDWidget.h"
#include "Kismet/GameplayStatics.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SaveConfirmText  = FText::GetEmpty();
	bSaveInProgress  = false;

	// Pause as soon as the widget is constructed
	UGameplayStatics::SetGamePaused(GetWorld(), true);

	// Switch to UI input so the mouse is usable while paused
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
}

void UPauseMenuWidget::SaveGame()
{
	if (bSaveInProgress)
	{
		return;
	}

	UPlanetConquestGameInstance* GI = Cast<UPlanetConquestGameInstance>(GetGameInstance());
	if (!GI)
	{
		SaveConfirmText = FText::FromString(TEXT("Error: no game instance"));
		return;
	}

	const FString SlotName = GI->GetActiveSlotName();

	// Load the existing save so we preserve the planet seed
	UPlanetConquestSaveGame* Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Save = Cast<UPlanetConquestSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}

	if (!Save)
	{
		Save = Cast<UPlanetConquestSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UPlanetConquestSaveGame::StaticClass()));
	}

	if (!Save)
	{
		SaveConfirmText = FText::FromString(TEXT("Error: could not create save"));
		return;
	}

	// Update metadata — world state serialisation will be added here later
	Save->LastSaved    = FDateTime::Now();
	Save->SlotDisplayName = FString::Printf(TEXT("Save %d"), GI->ActiveSlotIndex + 1);

	// Collect world state (units, resources, diplomacy, economy)
	if (APlanetConquestGameMode* GM = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->CollectWorldState(Save);
	}

	// Persist to disk
	bSaveInProgress = true;
	const bool bSuccess = UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
	bSaveInProgress = false;

	SaveConfirmText = bSuccess
		? FText::FromString(TEXT("Game Saved!"))
		: FText::FromString(TEXT("Save failed — check disk space"));
}

void UPauseMenuWidget::ResumeGame()
{
	// Restore game input before unpausing
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}

	UGameplayStatics::SetGamePaused(GetWorld(), false);
	RemoveFromParent();
}

void UPauseMenuWidget::QuitToMainMenu()
{
	// Save first, then leave — ignore save errors so quit always works
	SaveGame();

	// Explicitly remove the HUD widget from the viewport before OpenLevel.
	// AddToViewport() creates a strong reference in UGameViewportClient that
	// survives non-seamless level transitions; the stale widget would keep
	// ticking with freed Slate resources and crash on ClearChildren().
	if (APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer()))
	{
		if (PC->GameHUDWidgetInstance)
		{
			PC->GameHUDWidgetInstance->RemoveFromParent();
			PC->GameHUDWidgetInstance = nullptr;
		}
	}

	UGameplayStatics::SetGamePaused(GetWorld(), false);
	UGameplayStatics::OpenLevel(this, FName("MainMenuLevel"));
}
