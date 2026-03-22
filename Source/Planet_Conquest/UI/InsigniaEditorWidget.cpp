// Copyright Epic Games, Inc. All Rights Reserved.

#include "InsigniaEditorWidget.h"

void UInsigniaEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UInsigniaEditorWidget::InitWithInsignia(const FInsigniaData& ExistingInsignia)
{
	WorkingData = ExistingInsignia;
	UndoStack.Empty();
	RedoStack.Empty();
}

// ---- Active slot selection ----

void UInsigniaEditorWidget::SetActiveSlots(EInsigniaShapeSlot NewSlots)
{
	WorkingData.ActiveSlots = NewSlots;
}

bool UInsigniaEditorWidget::IsSlotAActive() const
{
	return WorkingData.ActiveSlots == EInsigniaShapeSlot::ShapeA
		|| WorkingData.ActiveSlots == EInsigniaShapeSlot::Both;
}

bool UInsigniaEditorWidget::IsSlotBActive() const
{
	return WorkingData.ActiveSlots == EInsigniaShapeSlot::ShapeB
		|| WorkingData.ActiveSlots == EInsigniaShapeSlot::Both;
}

// ---- Shape A ----

void UInsigniaEditorWidget::SetShapeA(EInsigniaShape NewShape)
{
	WorkingData.ShapeA = NewShape;
}

void UInsigniaEditorWidget::SetShapeAFillColor(FLinearColor NewColor)
{
	WorkingData.ShapeAFillColor = NewColor;
}

void UInsigniaEditorWidget::SetShapeAOutlineColor(FLinearColor NewColor)
{
	WorkingData.ShapeAOutlineColor = NewColor;
}

void UInsigniaEditorWidget::SetShapeAScale(float NewScale)
{
	WorkingData.ShapeAScale = FMath::Clamp(NewScale, 0.2f, 1.5f);
}

void UInsigniaEditorWidget::SetShapeAFlipped(bool bFlipped)
{
	WorkingData.bShapeAFlipped = bFlipped;
}

// ---- Shape B ----

void UInsigniaEditorWidget::SetShapeB(EInsigniaShape NewShape)
{
	WorkingData.ShapeB = NewShape;
}

void UInsigniaEditorWidget::SetShapeBFillColor(FLinearColor NewColor)
{
	WorkingData.ShapeBFillColor = NewColor;
}

void UInsigniaEditorWidget::SetShapeBOutlineColor(FLinearColor NewColor)
{
	WorkingData.ShapeBOutlineColor = NewColor;
}

void UInsigniaEditorWidget::SetShapeBScale(float NewScale)
{
	WorkingData.ShapeBScale = FMath::Clamp(NewScale, 0.2f, 1.5f);
}

void UInsigniaEditorWidget::SetShapeBFlipped(bool bFlipped)
{
	WorkingData.bShapeBFlipped = bFlipped;
}

// ---- Background ----

void UInsigniaEditorWidget::SetBackgroundColor(FLinearColor NewColor)
{
	WorkingData.BackgroundColor = NewColor;
}

// ---- Confirm / Cancel ----

void UInsigniaEditorWidget::OnConfirmClicked()
{
	OnInsigniaConfirmed.Broadcast(WorkingData);
	RemoveFromParent();
}

void UInsigniaEditorWidget::OnCancelClicked()
{
	OnInsigniaCancelled.Broadcast();
	RemoveFromParent();
}

void UInsigniaEditorWidget::CloseEditor()
{
	RemoveFromParent();
}

// ---- Hue helpers ----------------------------------------------------------
// FLinearColor in HSV space: R=Hue(0-360), G=Saturation(0-1), B=Value(0-1)

void UInsigniaEditorWidget::SetShapeAHue(float Hue)
{
	PushUndoState();
	WorkingData.ShapeAFillColor = FLinearColor(FMath::Clamp(Hue, 0.0f, 1.0f) * 360.0f, 1.0f, 1.0f, 1.0f).HSVToLinearRGB();
}

float UInsigniaEditorWidget::GetShapeAHue() const
{
	return WorkingData.ShapeAFillColor.LinearRGBToHSV().R / 360.0f;
}

void UInsigniaEditorWidget::SetShapeBHue(float Hue)
{
	PushUndoState();
	WorkingData.ShapeBFillColor = FLinearColor(FMath::Clamp(Hue, 0.0f, 1.0f) * 360.0f, 1.0f, 1.0f, 1.0f).HSVToLinearRGB();
}

float UInsigniaEditorWidget::GetShapeBHue() const
{
	return WorkingData.ShapeBFillColor.LinearRGBToHSV().R / 360.0f;
}

// ---- Rotation helpers -----------------------------------------------------

void UInsigniaEditorWidget::SetShapeARotation(float Degrees)
{
	PushUndoState();
	WorkingData.ShapeARotation = FMath::Fmod(Degrees, 360.0f);
	if (WorkingData.ShapeARotation < 0.0f) WorkingData.ShapeARotation += 360.0f;
}

void UInsigniaEditorWidget::SetShapeBRotation(float Degrees)
{
	PushUndoState();
	WorkingData.ShapeBRotation = FMath::Fmod(Degrees, 360.0f);
	if (WorkingData.ShapeBRotation < 0.0f) WorkingData.ShapeBRotation += 360.0f;
}

// ---- Shape name helper ----

FText UInsigniaEditorWidget::GetShapeName(EInsigniaShape Shape)
{
	switch (Shape)
	{
		case EInsigniaShape::Cross8Pizza:      return FText::FromString(TEXT("Cross8Pizza"));
		case EInsigniaShape::CircleCutoutPlus: return FText::FromString(TEXT("CircleCutoutPlus"));
		case EInsigniaShape::CircleCutoutX:    return FText::FromString(TEXT("CircleCutoutX"));
		case EInsigniaShape::StarRectangle12:  return FText::FromString(TEXT("StarRectangle12"));
		default:                               return FText::FromString(TEXT("Unknown"));
	}
}

// ---- Slot visibility helpers ---------------------------------------------

ESlateVisibility UInsigniaEditorWidget::GetSlotAVisibility() const
{
	return IsSlotAActive() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
}

ESlateVisibility UInsigniaEditorWidget::GetSlotBVisibility() const
{
	return IsSlotBActive() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
}

// ---- Undo / Redo internals ------------------------------------------------

void UInsigniaEditorWidget::PushUndoState()
{
	UndoStack.Add(WorkingData);
	if (UndoStack.Num() > MaxUndoSteps)
	{
		UndoStack.RemoveAt(0);
	}
	// Any new action clears the redo branch
	RedoStack.Empty();
}

void UInsigniaEditorWidget::Undo()
{
	if (UndoStack.Num() == 0) return;
	RedoStack.Add(WorkingData);
	WorkingData = UndoStack.Last();
	UndoStack.RemoveAt(UndoStack.Num() - 1);
}

void UInsigniaEditorWidget::Redo()
{
	if (RedoStack.Num() == 0) return;
	UndoStack.Add(WorkingData);
	WorkingData = RedoStack.Last();
	RedoStack.RemoveAt(RedoStack.Num() - 1);
}

// ---- Click-to-add flow ----------------------------------------------------

EInsigniaShapeSlot UInsigniaEditorWidget::AddShape(EInsigniaShape Shape)
{
	const bool bAActive = IsSlotAActive();
	const bool bBActive = IsSlotBActive();

	if (!bAActive)
	{
		PushUndoState();
		WorkingData.ShapeA = Shape;
		WorkingData.ShapeAFillColor = WorkingData.ShapeAFillColor; // keep current shared color
		WorkingData.ActiveSlots = bBActive ? EInsigniaShapeSlot::Both : EInsigniaShapeSlot::ShapeA;
		return EInsigniaShapeSlot::ShapeA;
	}
	else if (!bBActive)
	{
		PushUndoState();
		WorkingData.ShapeB = Shape;
		WorkingData.ShapeBFillColor = WorkingData.ShapeAFillColor; // match shared color
		WorkingData.ActiveSlots = EInsigniaShapeSlot::Both;
		return EInsigniaShapeSlot::ShapeB;
	}

	// Both slots full — caller should have checked CanAddMoreShapes() first
	return EInsigniaShapeSlot::None;
}

void UInsigniaEditorWidget::RemoveShape(EInsigniaShapeSlot SlotToRemove)
{
	if (SlotToRemove == EInsigniaShapeSlot::None) return;

	const bool bAWasActive = IsSlotAActive();
	const bool bBWasActive = IsSlotBActive();

	PushUndoState();

	if (SlotToRemove == EInsigniaShapeSlot::ShapeA && bAWasActive)
	{
		if (bBWasActive)
		{
			// Promote B into A so the remaining shape stays in slot A
			WorkingData.ShapeA         = WorkingData.ShapeB;
			WorkingData.ShapeAFillColor    = WorkingData.ShapeBFillColor;
			WorkingData.ShapeAOutlineColor = WorkingData.ShapeBOutlineColor;
			WorkingData.ShapeAScale    = WorkingData.ShapeBScale;
			WorkingData.ShapeARotation = WorkingData.ShapeBRotation;
			WorkingData.bShapeAFlipped = WorkingData.bShapeBFlipped;
			WorkingData.ActiveSlots    = EInsigniaShapeSlot::ShapeA;
		}
		else
		{
			WorkingData.ActiveSlots = EInsigniaShapeSlot::None;
		}
	}
	else if (SlotToRemove == EInsigniaShapeSlot::ShapeB && bBWasActive)
	{
		WorkingData.ActiveSlots = bAWasActive ? EInsigniaShapeSlot::ShapeA : EInsigniaShapeSlot::None;
	}
}

bool UInsigniaEditorWidget::CanAddMoreShapes() const
{
	return WorkingData.ActiveSlots != EInsigniaShapeSlot::Both;
}

int32 UInsigniaEditorWidget::GetActiveShapeCount() const
{
	switch (WorkingData.ActiveSlots)
	{
		case EInsigniaShapeSlot::Both:   return 2;
		case EInsigniaShapeSlot::None:   return 0;
		default:                         return 1;
	}
}

// ---- Shared color ---------------------------------------------------------

void UInsigniaEditorWidget::SetSharedColor(FLinearColor NewColor)
{
	PushUndoState();
	WorkingData.ShapeAFillColor = NewColor;
	WorkingData.ShapeBFillColor = NewColor;
}

// ---- Layer ordering -------------------------------------------------------

void UInsigniaEditorWidget::SwapLayers()
{
	if (WorkingData.ActiveSlots != EInsigniaShapeSlot::Both) return;

	PushUndoState();

	Swap(WorkingData.ShapeA,         WorkingData.ShapeB);
	Swap(WorkingData.ShapeAFillColor,    WorkingData.ShapeBFillColor);
	Swap(WorkingData.ShapeAOutlineColor, WorkingData.ShapeBOutlineColor);
	Swap(WorkingData.ShapeAScale,    WorkingData.ShapeBScale);
	Swap(WorkingData.bShapeAFlipped, WorkingData.bShapeBFlipped);
}
