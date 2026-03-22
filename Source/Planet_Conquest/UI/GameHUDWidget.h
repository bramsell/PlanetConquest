// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameHUDWidget.generated.h"

UCLASS()
class PLANET_CONQUEST_API UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Update the displayed values
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateDisplay(int32 OrangeSubstrate, int32 BlackSubstrate);
	
	// Get formatted text with income
	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetOrangeSubstrateText() const;
	
	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetBlackSubstrateText() const;

	// Exposed values for blueprint binding
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	int32 DisplayedOrangeSubstrate = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	int32 DisplayedBlackSubstrate = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	int32 DisplayedOrangeIncome = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	int32 DisplayedBlackIncome = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	bool bAutopilotMode = true;

	// Mining mode display ("Aggressive" or "Sustainable")
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	FText MiningModeText;

	// Toggle between Aggressive and Sustainable mining modes
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleMiningMode();
	
	// Vehicle efficiency mode display ("Efficient", "Overdrive", or "Auxiliary")
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	FText VehicleEfficiencyText;
	
	// Can the player toggle vehicle efficiency? (false when out of Black Substrate)
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	bool bCanToggleVehicleEfficiency = true;
	
	// Toggle between Efficient and Overdrive vehicle modes
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleVehicleEfficiency();

	// Container for alliance information (bind this in Blueprint)
	UPROPERTY(meta = (BindWidget))
	class UVerticalBox* AlliancesContainer;
	
	// Container for AI debug information (bind this in Blueprint)
	UPROPERTY(meta = (BindWidget))
	class UVerticalBox* AIDebugContainer;
	
	// Minimap canvas panel (contains terrain image + dynamic markers)
	UPROPERTY(meta = (BindWidget))
	class UCanvasPanel* MinimapCanvas;
	
	// Minimap terrain image widget (child of MinimapCanvas)
	UPROPERTY(meta = (BindWidget))
	class UImage* MinimapImage;
	
	// Which AI team to display (default AI1 = Team 2)
	UPROPERTY(BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	int32 DebugAITeamIndex = 2;

	// Reference to the main planet actor for minimap access
	UPROPERTY(BlueprintReadWrite, Category = "Minimap")
	class APlanetActor* PlanetActorRef;

	// City icon texture for markers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	class UTexture2D* CityIconTexture;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Dynamic marker tracking
	TMap<class ACityActor*, class UImage*> CityMarkers;
	
	// Update minimap city markers
	void UpdateCityMarkers(FVector2D MinimapSize);

private:
	// Timer for throttling AI debug updates
	float AIDebugUpdateTimer = 0.0f;
	
	// Cached AI debug state to avoid rebuilding UI when values haven't changed
	int32 CachedDebugTeamIndex = -1;
	int32 CachedOrangeSubstrate = -1;
	int32 CachedBlackSubstrate = -1;
	int32 CachedOrangeIncome = -1;
	int32 CachedBlackIncome = -1;
	int32 CachedRequiredOrangeIncome = -1;
	int32 CachedRequiredBlackIncome = -1;
	int32 CachedTotalVehicles = -1;
	TMap<FString, int32> CachedPriorityCounts;
	
	// Auto-update from player controller
	void RefreshFromPlayerController();
	
	// Update alliances display
	void UpdateAlliancesDisplay();
	
	// Update AI debug display
	void UpdateAIDebugDisplay();
};
