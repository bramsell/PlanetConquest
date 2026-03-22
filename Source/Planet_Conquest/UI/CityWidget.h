// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InsigniaData.h"
#include "CityWidget.generated.h"

UCLASS()
class PLANET_CONQUEST_API UCityWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Set the city this widget is displaying
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetCity(class ACityActor* InCity);

	// Get the currently displayed city
	UFUNCTION(BlueprintCallable, Category = "UI")
	class ACityActor* GetCity() const { return CurrentCity; }

	// Button click handler - close UI (public so CityActor can call it)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnCloseButtonClicked();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// The city we're currently displaying
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class ACityActor* CurrentCity;

	// Button click handler - spawn vehicle
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSpawnVehicleClicked();

	// Button click handler - buy factory
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuyFactoryClicked();

	// Button click handler - buy turret
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuyTurretClicked();

	// Button click handler - buy lab
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuyLabClicked();

	// Check if player can afford to spawn a vehicle
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool CanAffordVehicle() const;

	// Check if player can afford to buy a factory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool CanAffordFactory() const;

	// Check if player can afford to buy a turret
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool CanAffordTurret() const;

	// Check if player can afford to buy a lab
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool CanAffordLab() const;

	// Get building counts for display
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetFactoryCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetTurretCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetLabCount() const;

	// Get city name for display
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetCityName() const;

	// Get population for display (formatted with commas)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetPopulationText() const;

	// Get population color (red/green/white based on sustainability)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FSlateColor GetPopulationColor() const;

	// Get green substrate value for this city
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetGreenSubstrate() const;

	// Get player's orange substrate
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetOrangeSubstrate() const;

	// Get player's black substrate
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetBlackSubstrate() const;

	// ---- Insignia editor ----

	// Button click handler - open the insignia designer
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnOpenInsigniaEditorClicked();

	// The player's saved insignia for this session
	UPROPERTY(BlueprintReadWrite, Category = "Insignia")
	FInsigniaData PlayerInsignia;

	// Blueprint subclass to spawn (assign WBP_InsigniaEditor in the Blueprint defaults)
	UPROPERTY(EditDefaultsOnly, Category = "Insignia")
	TSubclassOf<class UInsigniaEditorWidget> InsigniaEditorWidgetClass;

	// Live instance while the editor is open (nullptr when closed)
	UPROPERTY()
	class UInsigniaEditorWidget* InsigniaEditorInstance = nullptr;

	// Callback bound to the editor's OnInsigniaConfirmed delegate
	UFUNCTION()
	void HandleInsigniaConfirmed(FInsigniaData NewInsignia);

	// Getter so Blueprint can read the saved insignia data
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Insignia")
	FInsigniaData GetPlayerInsignia() const { return PlayerInsignia; }

	// Called after insignia is saved — implement in WBP_CityUI to refresh the preview images on the button
	UFUNCTION(BlueprintImplementableEvent, Category = "Insignia")
	void OnInsigniaUpdated();
};
