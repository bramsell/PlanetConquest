// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResourceUIWidget.h"

void UResourceUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UResourceUIWidget::UpdateDisplay(FText TypeName, int32 CurrentIncome, int32 MaxIncome, bool bIsGreen)
{
	ResourceTypeName = TypeName;
	CurrentIncomePerCycle = CurrentIncome;
	MaxIncomePerCycle = MaxIncome;
	bIsGreenSubstrate = bIsGreen;
}

bool UResourceUIWidget::ShouldShowMaxIncome() const
{
	// Green substrate doesn't deplete, so don't show max income
	return !bIsGreenSubstrate;
}

ESlateVisibility UResourceUIWidget::GetMaxIncomeVisibility() const
{
	// Green substrate doesn't deplete, so hide max income display
	return bIsGreenSubstrate ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
}
