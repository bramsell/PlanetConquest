// Copyright Epic Games, Inc. All Rights Reserved.

#include "HealthBarWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHealthBarWidget::SetHealthPercent(float Percent)
{
	HealthPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
	HealthBarColor = GetHealthColor();
}

FLinearColor UHealthBarWidget::GetHealthColor() const
{
	// Gradient: Red (0%) -> Orange (33%) -> Yellow (66%) -> Green (100%)
	if (HealthPercent < 0.33f)
	{
		// Red to Orange
		float Alpha = HealthPercent / 0.33f;
		return FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor(1.0f, 0.5f, 0.0f), Alpha);
	}
	else if (HealthPercent < 0.66f)
	{
		// Orange to Yellow
		float Alpha = (HealthPercent - 0.33f) / 0.33f;
		return FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.5f, 0.0f), FLinearColor::Yellow, Alpha);
	}
	else
	{
		// Yellow to Green
		float Alpha = (HealthPercent - 0.66f) / 0.34f;
		return FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, Alpha);
	}
}
