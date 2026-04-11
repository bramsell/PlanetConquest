// Copyright Benjamin Ramsell. All Rights Reserved.

#include "MainMenuWidget.h"
#include "../Core/PlanetConquestSaveGame.h"
#include "../Core/PlanetConquestGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

static constexpr int32 NumSaveSlots = 4;

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Pre-size arrays so Blueprint Get(index) never goes out of bounds
	SlotLabels.SetNum(NumSaveSlots);
	SlotHasSave.SetNum(NumSaveSlots);

	bConfirmPanelVisible  = false;
	SelectedSlotIndex     = -1;
	ConfirmTitleText      = FText::GetEmpty();
	ConfirmDateText       = FText::GetEmpty();
	ConfirmPlaytimeText   = FText::GetEmpty();
	ConfirmButtonText     = FText::GetEmpty();

	RefreshAllSlots();
}

void UMainMenuWidget::RefreshAllSlots()
{
	UPlanetConquestGameInstance* GI = Cast<UPlanetConquestGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	for (int32 i = 0; i < NumSaveSlots; i++)
	{
		const FString SlotName = GI->GetSlotNameForIndex(i);

		if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			if (UPlanetConquestSaveGame* Save = Cast<UPlanetConquestSaveGame>(
				UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
			{
				SlotHasSave[i] = true;
				SlotLabels[i]  = FText::FromString(FString::Printf(TEXT("Save %d"), i + 1));
				continue;
			}
		}

		// No save file for this slot
		SlotHasSave[i] = false;
		SlotLabels[i]  = FText::FromString(TEXT("Empty Slot"));
	}
}

void UMainMenuWidget::OnSlotClicked(int32 InSlotIndex)
{
	if (InSlotIndex < 0 || InSlotIndex >= NumSaveSlots)
	{
		return;
	}

	SelectedSlotIndex    = InSlotIndex;
	bConfirmPanelVisible = true;

	if (SlotHasSave[InSlotIndex])
	{
		// Load existing save — populate confirm panel with its metadata
		UPlanetConquestGameInstance* GI = Cast<UPlanetConquestGameInstance>(GetGameInstance());
		const FString SlotName = GI ? GI->GetSlotNameForIndex(InSlotIndex) : FString();

		ConfirmTitleText  = FText::FromString(FString::Printf(TEXT("Load Save %d"), InSlotIndex + 1));
		ConfirmButtonText = FText::FromString(TEXT("Load"));

		if (!SlotName.IsEmpty())
		{
			if (UPlanetConquestSaveGame* Save = Cast<UPlanetConquestSaveGame>(
				UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
			{
				ConfirmDateText = FText::FromString(
					FString::Printf(TEXT("Last saved  %s"),
						*Save->LastSaved.ToString(TEXT("%B %d, %Y  %H:%M"))));

				const int64 Hours   = Save->PlaytimeSeconds / 3600;
				const int64 Minutes = (Save->PlaytimeSeconds % 3600) / 60;
				ConfirmPlaytimeText = FText::FromString(
					FString::Printf(TEXT("%lldh %02lldm played"), Hours, Minutes));
			}
		}
	}
	else
	{
		// Empty slot — new game
		ConfirmTitleText    = FText::FromString(TEXT("Start New Game"));
		ConfirmButtonText   = FText::FromString(TEXT("Start"));
		ConfirmDateText     = FText::GetEmpty();
		ConfirmPlaytimeText = FText::GetEmpty();
	}
}

void UMainMenuWidget::ConfirmSelection()
{
	if (SelectedSlotIndex < 0)
	{
		return;
	}

	UPlanetConquestGameInstance* GI = Cast<UPlanetConquestGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	GI->SetActiveSlot(SelectedSlotIndex, SlotHasSave[SelectedSlotIndex]);
	UGameplayStatics::OpenLevel(this, FName("World1"));
}

void UMainMenuWidget::CancelSelection()
{
	bConfirmPanelVisible = false;
	SelectedSlotIndex    = -1;
}

void UMainMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(
		GetWorld(),
		GetOwningPlayer(),
		EQuitPreference::Quit,
		/*bIgnorePlatformRestrictions=*/false);
}

void UMainMenuWidget::DeleteSave()
{
	if (SelectedSlotIndex < 0 || SelectedSlotIndex >= NumSaveSlots)
	{
		return;
	}
	if (!SlotHasSave[SelectedSlotIndex])
	{
		return;
	}

	UPlanetConquestGameInstance* GI = Cast<UPlanetConquestGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	const FString SlotName = GI->GetSlotNameForIndex(SelectedSlotIndex);
	UGameplayStatics::DeleteGameInSlot(SlotName, 0);

	// Refresh slot display then dismiss the confirm panel
	RefreshAllSlots();
	CancelSelection();
}

ESlateVisibility UMainMenuWidget::GetConfirmPanelVisibility() const
{
	return bConfirmPanelVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
}

ESlateVisibility UMainMenuWidget::GetDeleteButtonVisibility() const
{
	const bool bShow = SelectedSlotIndex >= 0
		&& SelectedSlotIndex < SlotHasSave.Num()
		&& SlotHasSave[SelectedSlotIndex];
	return bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
}
