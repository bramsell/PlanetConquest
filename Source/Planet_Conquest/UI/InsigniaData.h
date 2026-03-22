// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InsigniaData.generated.h"

/** Which shape slot(s) are active */
UENUM(BlueprintType)
enum class EInsigniaShapeSlot : uint8
{
	None       UMETA(DisplayName = "None"),
	ShapeA     UMETA(DisplayName = "Shape A Only"),
	ShapeB     UMETA(DisplayName = "Shape B Only"),
	Both       UMETA(DisplayName = "Both Overlapping"),
};

/** The available shape types */
UENUM(BlueprintType)
enum class EInsigniaShape : uint8
{
	Cross8Pizza      UMETA(DisplayName = "Cross8Pizza"),
	CircleCutoutPlus UMETA(DisplayName = "CircleCutoutPlus"),
	CircleCutoutX    UMETA(DisplayName = "CircleCutoutX"),
	StarRectangle12  UMETA(DisplayName = "StarRectangle12"),
};

/** Complete description of a player's insignia design */
USTRUCT(BlueprintType)
struct FInsigniaData
{
	GENERATED_BODY()

	// Which slots are visible
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia")
	EInsigniaShapeSlot ActiveSlots = EInsigniaShapeSlot::None;

	// Shape A settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeA")
	EInsigniaShape ShapeA = EInsigniaShape::Cross8Pizza;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeA")
	FLinearColor ShapeAFillColor = FLinearColor(0.1f, 0.4f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeA")
	FLinearColor ShapeAOutlineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeA")
	float ShapeAScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeA")
	float ShapeARotation = 0.0f; // degrees, 0-360

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeA")
	bool bShapeAFlipped = false;

	// Shape B settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeB")
	EInsigniaShape ShapeB = EInsigniaShape::CircleCutoutPlus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeB")
	FLinearColor ShapeBFillColor = FLinearColor(0.9f, 0.8f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeB")
	FLinearColor ShapeBOutlineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeB")
	float ShapeBScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeB")
	float ShapeBRotation = 0.0f; // degrees, 0-360

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia|ShapeB")
	bool bShapeBFlipped = false;

	// Background color of the insignia badge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insignia")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
};
