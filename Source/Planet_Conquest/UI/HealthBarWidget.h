// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarWidget.generated.h"

/**
 * Simple health bar widget for vehicles
 */
UCLASS()
class PLANET_CONQUEST_API UHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Health percentage (0.0 to 1.0)
	UPROPERTY(BlueprintReadWrite, Category = "Health")
	float HealthPercent = 1.0f;

	// Health bar color (auto-calculated from health percent)
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FLinearColor HealthBarColor = FLinearColor::Green;

	// Update the health bar display
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetHealthPercent(float Percent);

	// Calculate color based on health percentage
	UFUNCTION(BlueprintCallable, Category = "Health")
	FLinearColor GetHealthColor() const;

protected:
	virtual void NativeConstruct() override;
};
