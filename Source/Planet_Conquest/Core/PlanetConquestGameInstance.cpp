// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PlanetConquestGameInstance.h"

void UPlanetConquestGameInstance::SetActiveSlot(int32 SlotIndex, bool bIsLoad)
{
	ActiveSlotIndex      = SlotIndex;
	bLoadingExistingGame = bIsLoad;
	bSlotWasSetByMenu    = true;  // came from the main-menu widget
}

FString UPlanetConquestGameInstance::GetActiveSlotName() const
{
	return GetSlotNameForIndex(ActiveSlotIndex);
}

FString UPlanetConquestGameInstance::GetSlotNameForIndex(int32 SlotIndex) const
{
	return FString::Printf(TEXT("PlanetConquest_Save_%d"), SlotIndex);
}
