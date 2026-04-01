// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DiplomacyWidget.generated.h"

UCLASS()
class PLANET_CONQUEST_API UDiplomacyWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Set the city this widget is displaying
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetCity(class ACityActor* InCity);

	// Get the currently displayed city
	UFUNCTION(BlueprintCallable, Category = "UI")
	class ACityActor* GetCity() const { return CurrentCity; }

	// Get relationship value for display
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	float GetRelationshipValue() const;

	// Get relationship text for display (e.g., "Ally", "Neutral", "Enemy")
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetRelationshipText() const;

	// Get relationship color for display
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FLinearColor GetRelationshipColor() const;

	// Get city name for display
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetCityName() const;

	// Get owner team name for display (e.g., "Player", "AI Team 1")
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetOwnerTeamName() const;

	// Get city status text: "Owned" for player cities, relationship text for AI cities
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetCityStatusText() const;

	// Get population value for this city
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetPopulation() const;

	// Get population color (red/green/white based on sustainability)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FSlateColor GetPopulationColor() const;

	// Button click handler - attack with selected vehicles
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnAttackButtonClicked();

	// Button click handler - open talk dialog
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnTalkButtonClicked();

	// Button click handler - close UI
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnCloseButtonClicked();

	// Button click handler - open city editor (player cities only)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnCityEditorButtonClicked();

	// Check if this is a player-owned city
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool IsPlayerCity() const;

	// Get visibility for AI city elements (Talk/Attack buttons, relationship)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	ESlateVisibility GetAICityElementsVisibility() const;

	// Get visibility for player city elements (City Editor button, "Owned" text)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	ESlateVisibility GetPlayerCityElementsVisibility() const;

	// Check if player has vehicles selected
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	bool HasVehiclesSelected() const;

	// Get count of selected vehicles
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	int32 GetSelectedVehicleCount() const;

	// Get player's alliance name (returns "No Allegiance" if not in an alliance)
	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetPlayerAllianceName() const;

	// Get alliance name for the currently displayed city's team (returns "No Allegiance" if not in an alliance)
	UFUNCTION(BlueprintPure, Category = "UI")
	FText GetCityAllianceName() const;
	
	// Play button click animation
	UFUNCTION(BlueprintCallable, Category = "UI")
	void PlayAttackButtonAnimation();
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void PlayTalkButtonAnimation();
	
	// Update button hover states based on mouse position
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateButtonHoverStates(bool bIsHoveringAttack, bool bIsHoveringTalk);
	
	// Update City Editor button hover state (for player cities)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateCityEditorButtonHoverState(bool bIsHovering);
	
	// Play City Editor button click animation
	UFUNCTION(BlueprintCallable, Category = "UI")
	void PlayCityEditorButtonAnimation();
	
	// Button references (bind these in Blueprint)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UButton* AttackButton;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UButton* TalkButton;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "UI")
	class UButton* CityEditorButton;
	
	// Button animations (optional - create these in UMG Designer)
	UPROPERTY(BlueprintReadWrite, Transient, meta = (BindWidgetAnimOptional), Category = "UI")
	class UWidgetAnimation* AttackButtonClickAnim;
	
	UPROPERTY(BlueprintReadWrite, Transient, meta = (BindWidgetAnimOptional), Category = "UI")
	class UWidgetAnimation* TalkButtonClickAnim;
	
	UPROPERTY(BlueprintReadWrite, Transient, meta = (BindWidgetAnimOptional), Category = "UI")
	class UWidgetAnimation* CityEditorButtonClickAnim;

protected:
	virtual void NativeConstruct() override;

	// The city we're currently displaying
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class ACityActor* CurrentCity;
	
	// Track current button hover states
	bool bAttackButtonHovered = false;
	bool bTalkButtonHovered = false;
	bool bCityEditorButtonHovered = false;
	
	// Store original button styles
	FButtonStyle AttackButtonOriginalStyle;
	FButtonStyle TalkButtonOriginalStyle;
	FButtonStyle CityEditorButtonOriginalStyle;
	bool bStylesCached = false;
	
	// City Editor button colors (blue palette at 0.7 alpha)
	FLinearColor CityEditorNormalColor = FLinearColor(0.1f, 0.3f, 0.8f, 0.7f);
	FLinearColor CityEditorHoverColor = FLinearColor(0.2f, 0.5f, 1.0f, 0.7f);
	FLinearColor CityEditorPressedColor = FLinearColor(0.05f, 0.2f, 0.5f, 0.7f);
};
