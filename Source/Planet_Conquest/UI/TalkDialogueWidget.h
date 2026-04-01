// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TalkDialogueWidget.generated.h"

// Dialogue menu state
UENUM(BlueprintType)
enum class EDialogueState : uint8
{
	MainMenu,         // Showing 4 main category buttons
	TradeMenu,        // Showing trade options
	TradeConfirmation,// Showing AI's trade offer with Alright/No deal buttons
	RequestMenu,      // Showing request options
	InfluenceMenu,    // Showing influence options
	RelationsMenu,    // Showing relations options
	TradeCompleted,   // Showing trade completion acknowledgment
	AidRequestIncoming,// AI is requesting aid from player
	BribeIncoming     // AI is offering bribe to player
};

UCLASS()
class PLANET_CONQUEST_API UTalkDialogueWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Initialize the dialogue with a city
	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializeDialogue(class ACityActor* InCity);

	// Initialize dialogue in a specific state (for AI-initiated trades)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializeDialogueInState(class ACityActor* InCity, EDialogueState InitialState);

	// Close the dialogue
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseDialogue();

	// Current dialogue state
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	EDialogueState CurrentState = EDialogueState::MainMenu;

	// Reference to the city we're negotiating with
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class ACityActor* CurrentCity = nullptr;

	// Get display information
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetCityTeamName() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetCityArchetypeName() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	float GetRelationshipValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetRelationshipText() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FLinearColor GetRelationshipColor() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI")
	FText GetDialogueText() const;

	// Main menu button handlers
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnTradeButtonClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRequestButtonClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnInfluenceButtonClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRelationsButtonClicked();

	// Submenu action handlers
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnTradeResourcesClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnTradeTechnologyClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnTradeMapDataClicked();

	// New substrate trade handlers
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSellBlackSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuyBlackSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSellOrangeSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuyOrangeSubstrateClicked();

	// 500 substrate trade handlers
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuy500BlackSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBuy500OrangeSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRequestCeasefireClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRequestAllianceClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnEndAllianceClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRequestMilitaryAidClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRequestSubstrateAidClicked();

	// Aid response handlers (when AI requests aid from player)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSend1000OrangeAidClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSend2000OrangeAidClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSend5000OrangeAidClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnSendMilitaryAidClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnDeclineAidRequestClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnInfluenceThreatClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnInfluenceBribeClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBribeOrangeSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBribeBlackSubstrateClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnInfluencePropagandaClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRelationsDeclareWarClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRelationsMakePeaceClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRelationsViewHistoryClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnRelationsAskAboutOtherTeamClicked();

	// Back button to return to main menu
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnBackButtonClicked();

	// Trade confirmation buttons
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnAlrightClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnNoDealClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnOfCourseClicked();

	// Alliance confirmation buttons (for AI-initiated alliance requests)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnAcceptAllianceClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnDeclineAllianceClicked();

	// Bribe response buttons (for AI-initiated bribe offers)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnAcceptBribeThankfullyClicked();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnAcceptBribeSuspiciouslyClicked();

	// State management
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetDialogueState(EDialogueState NewState);

	// Widget references (bind in UMG Blueprint)
	
	// Top info bar
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UTextBlock* TeamNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UTextBlock* RelationshipText;

	// Middle dialogue area
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UTextBlock* DialogueText;

	// Main menu buttons (2x2 grid)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UButton* TradeButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UButton* RequestButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UButton* InfluenceButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UButton* RelationsButton;

	// Main menu container
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UCanvasPanel* MainMenuPanel;

	// Submenu scroll box for dynamic options
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "UI")
	class UScrollBox* SubmenuScrollBox;

	// Back button (optional - can use dynamic back in scroll box instead)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "UI")
	class UButton* BackButton;

	// Close button
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "UI")
	class UButton* CloseButton;

	// Current dialogue text based on state and context
	FString CurrentDialogueTextString;

protected:
	virtual void NativeConstruct() override;

private:
	// Update UI visibility based on state
	void UpdateUIVisibility();

	// Populate submenu with options
	void PopulateSubmenu();

	// Create a submenu button
	class UButton* CreateSubmenuButton(const FText& ButtonText, const FName& FunctionName);

	// Handle submenu button click
	void OnSubmenuButtonClicked(const FName& ActionName);

	// Check if player can trade with current team (cooldown check)
	// Returns true if can trade, false if on cooldown (and displays message)
	bool CheckTradeCooldown();

	// Track last button click time to prevent double-click issues
	// Use real-world time (not game time) since game is paused during dialogue
	double LastButtonClickTime = -999.0;
	double ButtonClickCooldown = 0.03; // 30ms to catch hardware double-clicks

	// Pending trade details (for confirmation state)
	enum class EPendingTradeType : uint8
	{
		None,
		SellBlack,
		BuyBlack,
		SellOrange,
		BuyOrange
	};

	EPendingTradeType PendingTradeType = EPendingTradeType::None;
	int32 PendingPlayerOffer = 0;     // Amount player is offering
	int32 PendingAIResponse = 0;       // Amount AI is offering/demanding
	int32 PendingTradeAmount = 0;      // The base trade amount (1000)

	// Pending aid request tracking
	UPROPERTY()
	class AAITeamController* PendingAidRequester = nullptr;
	
	UPROPERTY()
	class ACityActor* CityUnderAttack = nullptr;
	
	bool bAidRequestNeedsSubstrate = false;
	bool bAidRequestNeedsMilitary = false;
	int32 AidRequestSubstrateAmount = 0;
};
