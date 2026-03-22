// InfoUIWidget.cpp

#include "InfoUIWidget.h"

void UInfoUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UInfoUIWidget::SetInfoDisplay(FText Line1, FText Line2, FText Line3)
{
	Line1Text = Line1;
	Line2Text = Line2;
	Line3Text = Line3;
}

bool UInfoUIWidget::ShouldShowLine3() const
{
	return !Line3Text.IsEmpty();
}

ESlateVisibility UInfoUIWidget::GetLine3Visibility() const
{
	return ShouldShowLine3() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}
