// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsigniaData.h"
#include "InsigniaEditorWidget.generated.h"

/**
 * Insignia designer widget.
 *
 * The actual visual layout is built in the Blueprint subclass (WBP_InsigniaEditor).
 * This C++ class exposes all the state, getters, and button handlers needed by UMG bindings.
 *
 * Design flow:
 *   1. Player picks Active Slots (A only / B only / Both)
 *   2. Player picks shape for each active slot from the shape picker row
 *   3. Player adjusts fill color, outline color, scale, flip for each slot
 *   4. Preview canvas (a UCanvasPanel child) re-draws live via NativeTick bindings
 *   5. Confirm → saves FInsigniaData, calls OnInsigniaConfirmed delegate
 *   6. Cancel → calls OnInsigniaCancelled delegate
 */
UCLASS()
class PLANET_CONQUEST_API UInsigniaEditorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ---- Delegates (bound in CityWidget to open/close the overlay) ----

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsigniaConfirmed, FInsigniaData, NewInsignia);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInsigniaCancelled);

	UPROPERTY(BlueprintAssignable, Category = "Insignia")
	FOnInsigniaConfirmed OnInsigniaConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Insignia")
	FOnInsigniaCancelled OnInsigniaCancelled;

	// ---- Initialise with existing data (called before AddToViewport) ----

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void InitWithInsignia(const FInsigniaData& ExistingInsignia);

	// ---- Active slot selection ----

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetActiveSlots(EInsigniaShapeSlot NewSlots);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	EInsigniaShapeSlot GetActiveSlots() const { return WorkingData.ActiveSlots; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool IsSlotAActive() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool IsSlotBActive() const;

	// Visibility versions — bind these directly to Image/widget Visibility in UMG
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	ESlateVisibility GetSlotAVisibility() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	ESlateVisibility GetSlotBVisibility() const;

	// ---- Shape A ----

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeA(EInsigniaShape NewShape);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	EInsigniaShape GetShapeA() const { return WorkingData.ShapeA; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeAFillColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FLinearColor GetShapeAFillColor() const { return WorkingData.ShapeAFillColor; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeAOutlineColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FLinearColor GetShapeAOutlineColor() const { return WorkingData.ShapeAOutlineColor; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeAScale(float NewScale);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	float GetShapeAScale() const { return WorkingData.ShapeAScale; }

	// Hue slider (0.0-1.0); full saturation and brightness are kept constant
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeAHue(float Hue);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	float GetShapeAHue() const;

	// Rotation slider (0.0-360.0 degrees)
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeARotation(float Degrees);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	float GetShapeARotation() const { return WorkingData.ShapeARotation; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeAFlipped(bool bFlipped);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool GetShapeAFlipped() const { return WorkingData.bShapeAFlipped; }

	// ---- Shape B ----

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeB(EInsigniaShape NewShape);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	EInsigniaShape GetShapeB() const { return WorkingData.ShapeB; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeBFillColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FLinearColor GetShapeBFillColor() const { return WorkingData.ShapeBFillColor; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeBOutlineColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FLinearColor GetShapeBOutlineColor() const { return WorkingData.ShapeBOutlineColor; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeBScale(float NewScale);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	float GetShapeBScale() const { return WorkingData.ShapeBScale; }

	// Hue slider (0.0-1.0); full saturation and brightness are kept constant
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeBHue(float Hue);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	float GetShapeBHue() const;

	// Rotation slider (0.0-360.0 degrees)
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeBRotation(float Degrees);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	float GetShapeBRotation() const { return WorkingData.ShapeBRotation; }

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetShapeBFlipped(bool bFlipped);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool GetShapeBFlipped() const { return WorkingData.bShapeBFlipped; }

	// ---- Background ----

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetBackgroundColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FLinearColor GetBackgroundColor() const { return WorkingData.BackgroundColor; }

	// ---- Confirm / Cancel / Close buttons ----

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void OnConfirmClicked();

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void OnCancelClicked();

	// Closes (removes from parent) without broadcasting any delegate — use for a plain X button
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void CloseEditor();

	// ---- Read the current working data (for live preview bindings) ----

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FInsigniaData GetWorkingData() const { return WorkingData; }

	// ---- Shape name helper (for button labels in the shape picker) ----

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	static FText GetShapeName(EInsigniaShape Shape);

	// ---- Click-to-add flow ------------------------------------------------
	// Call AddShape when the user clicks a shape button in the left list.
	// First click fills Slot A (bottom layer), second click fills Slot B (top).
	// Returns the slot that was assigned, or None if both are already full.
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	EInsigniaShapeSlot AddShape(EInsigniaShape Shape);

	// Call RemoveShape when user clicks the X in the layer stack.
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void RemoveShape(EInsigniaShapeSlot SlotToRemove);

	// False when both slots are in use — use to grey out shape buttons.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool CanAddMoreShapes() const;

	// 0, 1, or 2 — handy for Blueprint display logic.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	int32 GetActiveShapeCount() const;

	// ---- Shared color (tints ALL active shapes together) ------------------
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SetSharedColor(FLinearColor NewColor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FLinearColor GetSharedColor() const { return WorkingData.ShapeAFillColor; }

	// ---- Layer ordering ---------------------------------------------------
	// Swaps all settings between Slot A and Slot B so the visual stack flips.
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void SwapLayers();

	// ---- Undo / Redo ------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void Undo();

	UFUNCTION(BlueprintCallable, Category = "Insignia")
	void Redo();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool CanUndo() const { return UndoStack.Num() > 0; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	bool CanRedo() const { return RedoStack.Num() > 0; }

protected:
	virtual void NativeConstruct() override;

	// The live-edited copy — only committed on Confirm
	UPROPERTY(BlueprintReadOnly, Category = "Insignia")
	FInsigniaData WorkingData;

private:
	// Call before any state-mutating operation to record a snapshot.
	void PushUndoState();

	TArray<FInsigniaData> UndoStack;
	TArray<FInsigniaData> RedoStack;

	static const int32 MaxUndoSteps = 20;
};
