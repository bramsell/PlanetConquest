// Copyright Epic Games, Inc. All Rights Reserved.

#include "TalkDialogueWidget.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "../Entities/Cities/CityActor.h"
#include "../Entities/Vehicles/VehicleActor.h"
#include "../Core/PlanetConquestPlayerController.h"
#include "../Core/PlanetConquestGameMode.h"
#include "../Core/PlanetConquestHUD.h"
#include "../Core/AITeamController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

void UTalkDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click events
	if (TradeButton)
	{
		TradeButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnTradeButtonClicked);
	}

	if (RequestButton)
	{
		RequestButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRequestButtonClicked);
	}

	if (InfluenceButton)
	{
		InfluenceButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnInfluenceButtonClicked);
	}

	if (RelationsButton)
	{
		RelationsButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRelationsButtonClicked);
	}

	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBackButtonClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::CloseDialogue);
	}

	// Start in main menu state (only if not already initialized via InitializeDialogueInState)
	if (CurrentState == EDialogueState::MainMenu && CurrentCity == nullptr)
	{
		SetDialogueState(EDialogueState::MainMenu);
	}
}

void UTalkDialogueWidget::InitializeDialogue(ACityActor* InCity)
{
	CurrentCity = InCity;
	CurrentState = EDialogueState::MainMenu;
	CurrentDialogueTextString = TEXT("What would you like to discuss?");

	UpdateUIVisibility();

	// Update display
	if (TeamNameText && CurrentCity)
	{
		TeamNameText->SetText(GetCityTeamName());
	}

	if (RelationshipText)
	{
		RelationshipText->SetText(GetRelationshipText());
		RelationshipText->SetColorAndOpacity(FSlateColor(GetRelationshipColor()));
	}

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}

	// Cancel any active box selection and disable HUD selection box
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (PC)
	{
		// Cancel box selection
		PC->bIsBoxSelecting = false;
		
		// Disable HUD selection box drawing
		if (APlanetConquestHUD* HUD = Cast<APlanetConquestHUD>(PC->GetHUD()))
		{
			HUD->bDrawSelectionBox = false;
		}
		
		// Lock player controller input to UI only (prevent camera/selection while in dialogue)
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;

		// PAUSE GAMEPLAY while dialogue is open
		UGameplayStatics::SetGamePaused(GetWorld(), true);
	}
}

void UTalkDialogueWidget::InitializeDialogueInState(ACityActor* InCity, EDialogueState InitialState)
{
	CurrentCity = InCity;

	// Use SetDialogueState to properly set state and dialogue text
	// (this will check for active AI requests and display correct details)
	SetDialogueState(InitialState);

	// Update display
	if (TeamNameText && CurrentCity)
	{
		TeamNameText->SetText(GetCityTeamName());
	}

	if (RelationshipText)
	{
		RelationshipText->SetText(GetRelationshipText());
		RelationshipText->SetColorAndOpacity(FSlateColor(GetRelationshipColor()));
	}

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}

	// Cancel any active box selection and disable HUD selection box
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (PC)
	{
		// Cancel box selection
		PC->bIsBoxSelecting = false;
		
		// Disable HUD selection box drawing
		if (APlanetConquestHUD* HUD = Cast<APlanetConquestHUD>(PC->GetHUD()))
		{
			HUD->bDrawSelectionBox = false;
		}
		
		// Lock player controller input to UI only (prevent camera/selection while in dialogue)
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;

		// PAUSE GAMEPLAY while dialogue is open
		UGameplayStatics::SetGamePaused(GetWorld(), true);
	}
}

void UTalkDialogueWidget::CloseDialogue()
{
	UE_LOG(LogTemp, Warning, TEXT("========== CloseDialogue CALLED =========="));
	
	// If there's an active AI trade or alliance request, decline it (closing UI = refusal)
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		if (GameMode->bHasActiveTradeRequest)
		{
			UE_LOG(LogTemp, Warning, TEXT("TalkDialogue: Player closed UI with active trade request - declining"));
			GameMode->DeclineCurrentTradeRequest();
		}
		if (GameMode->bHasActiveAllianceRequest)
		{
			UE_LOG(LogTemp, Warning, TEXT("TalkDialogue: Player closed UI with active alliance request - declining"));
			GameMode->DeclineCurrentAllianceRequest();
		}
		if (GameMode->bHasActiveBribeRequest)
		{
			UE_LOG(LogTemp, Warning, TEXT("TalkDialogue: Player closed UI with active bribe request - declining"));
			GameMode->DeclineCurrentBribeRequest();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("CloseDialogue: Unpausing gameplay"));
	
	// UNPAUSE GAMEPLAY before closing dialogue
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	UE_LOG(LogTemp, Warning, TEXT("CloseDialogue: Removing widget from parent"));
	
	// Remove widget FIRST before changing input mode
	RemoveFromParent();
	
	UE_LOG(LogTemp, Warning, TEXT("CloseDialogue: Widget removed, restoring player controller state"));
	
	// Then restore player controller state
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (PC)
	{
		// Ensure box selection is cancelled
		PC->bIsBoxSelecting = false;
		
		// Ensure HUD selection box is disabled
		if (APlanetConquestHUD* HUD = Cast<APlanetConquestHUD>(PC->GetHUD()))
		{
			HUD->bDrawSelectionBox = false;
		}
		
		// Restore game and UI input mode
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("========== CloseDialogue COMPLETE =========="));
}

FText UTalkDialogueWidget::GetCityTeamName() const
{
	if (!CurrentCity) return FText::FromString(TEXT("Unknown"));

	switch (CurrentCity->OwnerTeam)
	{
		case EOwnerTeam::Player: return FText::FromString(TEXT("Player"));
		case EOwnerTeam::AI1: return FText::FromString(TEXT("AI Team 2"));
		case EOwnerTeam::AI2: return FText::FromString(TEXT("AI Team 3"));
		case EOwnerTeam::AI3: return FText::FromString(TEXT("AI Team 4"));
		case EOwnerTeam::AI4: return FText::FromString(TEXT("AI Team 5"));
		case EOwnerTeam::AI5: return FText::FromString(TEXT("AI Team 6"));
		case EOwnerTeam::AI6: return FText::FromString(TEXT("AI Team 7"));
		case EOwnerTeam::AI7: return FText::FromString(TEXT("AI Team 8"));
		case EOwnerTeam::AI8: return FText::FromString(TEXT("AI Team 9"));
		default: return FText::FromString(TEXT("Neutral"));
	}
}

FText UTalkDialogueWidget::GetCityArchetypeName() const
{
	if (!CurrentCity) return FText::FromString(TEXT(""));
	
	// Player has no archetype
	if (CurrentCity->OwnerTeam == EOwnerTeam::Player || CurrentCity->OwnerTeam == EOwnerTeam::Neutral)
	{
		return FText::FromString(TEXT(""));
	}
	
	// Find the AI team controller for this city's team
	UWorld* World = GetWorld();
	if (!World) return FText::FromString(TEXT(""));
	
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		AAITeamController* AIController = *It;
		if (AIController && AIController->ControlledTeam == CurrentCity->OwnerTeam)
		{
			return FText::FromString(AIController->GetArchetypeName());
		}
	}
	
	return FText::FromString(TEXT(""));
}

float UTalkDialogueWidget::GetRelationshipValue() const
{
	if (!CurrentCity) return 0.0f;

	UWorld* World = GetWorld();
	if (!World) return 0.0f;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return 0.0f;

	return GameMode->GetRelationship(EOwnerTeam::Player, CurrentCity->OwnerTeam);
}

FText UTalkDialogueWidget::GetRelationshipText() const
{
	float Relationship = GetRelationshipValue();
	
	// Check if officially allied
	if (CurrentCity)
	{
		APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
		if (GameMode)
		{
			// Check if player and this team share an alliance
			FString PlayerAlliance = GameMode->GetPlayerAllianceName();
			if (!PlayerAlliance.IsEmpty())
			{
				// Find AI controller for this team
				for (TActorIterator<AAITeamController> It(GetWorld()); It; ++It)
				{
					if (It->ControlledTeam == CurrentCity->OwnerTeam)
					{
						if (It->AllianceState.AllianceName == PlayerAlliance)
						{
							FString FormattedText = FString::Printf(TEXT("Status: Allied (%.2f)"), Relationship);
							return FText::FromString(FormattedText);
						}
						break;
					}
				}
			}
		}
	}
	
	// Not allied - use relationship thresholds
	FString StatusText;
	if (Relationship >= 20.0f) StatusText = TEXT("Very Friendly");
	else if (Relationship >= 10.0f) StatusText = TEXT("Friendly");
	else if (Relationship >= -50.0f) StatusText = TEXT("Neutral");
	else StatusText = TEXT("Hostile");

	// Format as "Status: Very Friendly (0.25)"
	FString FormattedText = FString::Printf(TEXT("Status: %s (%.2f)"), *StatusText, Relationship);
	return FText::FromString(FormattedText);
}

FLinearColor UTalkDialogueWidget::GetRelationshipColor() const
{
	float Relationship = GetRelationshipValue();

	if (Relationship >= 70.0f) return FLinearColor::Green;
	if (Relationship > 50.0f) return FLinearColor(0.5f, 1.0f, 0.5f); // Light green
	if (Relationship >= -50.0f) return FLinearColor::Yellow;
	return FLinearColor::Red;
}

FText UTalkDialogueWidget::GetDialogueText() const
{
	return FText::FromString(CurrentDialogueTextString);
}

void UTalkDialogueWidget::SetDialogueState(EDialogueState NewState)
{
	CurrentState = NewState;
	
	// Update dialogue text based on state
	switch (CurrentState)
	{
		case EDialogueState::MainMenu:
			CurrentDialogueTextString = TEXT("What would you like to discuss?");
			break;
		case EDialogueState::TradeMenu:
			// Check if AI has an active trade request
			{
				APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
				if (GameMode && GameMode->bHasActiveTradeRequest)
				{
					FAITradeRequest TradeRequest = GameMode->GetCurrentTradeRequest();
					FString RequestedSubstrate = TradeRequest.bRequestingOrange ? TEXT("Orange") : TEXT("Black");
					FString OfferedSubstrate = TradeRequest.bRequestingOrange ? TEXT("Black") : TEXT("Orange");
					FString CounterText = TradeRequest.bIsCounterOffer ? TEXT(" Well, what about this instead: ") : TEXT("");
					CurrentDialogueTextString = FString::Printf(TEXT("%sWe need %d %s Substrate. In return, we offer %d %s Substrate."),
						*CounterText, TradeRequest.AmountRequested, *RequestedSubstrate, TradeRequest.AmountOffered, *OfferedSubstrate);
				}
				else
				{
					CurrentDialogueTextString = TEXT("We're open to trade. What interests you?");
				}
			}
			break;
		case EDialogueState::RequestMenu:
			// Check if AI has an active alliance request
			{
				APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
				if (GameMode && GameMode->bHasActiveAllianceRequest)
				{
					FAIAllianceRequest AllianceRequest = GameMode->GetCurrentAllianceRequest();
					CurrentDialogueTextString = TEXT("We face a common threat. Let us form an alliance.");
				}
				else
				{
					CurrentDialogueTextString = TEXT("You wish to make a request? Speak.");
				}
			}
			break;
		case EDialogueState::InfluenceMenu:
			CurrentDialogueTextString = TEXT("Diplomacy takes many forms...");
			break;
		case EDialogueState::RelationsMenu:
			CurrentDialogueTextString = TEXT("Let's discuss our relationship.");
			break;
		case EDialogueState::BribeIncoming:
		{
			APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
			if (GameMode && GameMode->bHasActiveBribeRequest)
			{
				FAIBribeRequest BribeRequest = GameMode->GetCurrentBribeRequest();
				FString SubstrateType = BribeRequest.bUsingOrangeSubstrate ? TEXT("Orange") : TEXT("Black");
				CurrentDialogueTextString = FString::Printf(TEXT("We come in good faith. Please accept %d %s Substrate as a token of our friendship."),
					BribeRequest.Amount, *SubstrateType);
			}
			else
			{
				CurrentDialogueTextString = TEXT("We wish to improve our relations.");
			}
		}
			break;
	}

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}

	UpdateUIVisibility();

	if (CurrentState != EDialogueState::MainMenu)
	{
		PopulateSubmenu();
	}
}

void UTalkDialogueWidget::UpdateUIVisibility()
{
	UE_LOG(LogTemp, Warning, TEXT("=== UpdateUIVisibility called, CurrentState=%d ==="), (int32)CurrentState);
	
	// Show main menu or submenu based on state
	if (MainMenuPanel)
	{
		ESlateVisibility MainVis = (CurrentState == EDialogueState::MainMenu) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		MainMenuPanel->SetVisibility(MainVis);
		UE_LOG(LogTemp, Warning, TEXT("  MainMenuPanel visibility: %d (0=Visible, 4=Collapsed)"), (int32)MainVis);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  MainMenuPanel is NULL!"));
	}

	if (SubmenuScrollBox)
	{
		ESlateVisibility SubmenuVis = (CurrentState != EDialogueState::MainMenu) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		SubmenuScrollBox->SetVisibility(SubmenuVis);
		UE_LOG(LogTemp, Warning, TEXT("  SubmenuScrollBox visibility: %d, Children: %d"), (int32)SubmenuVis, SubmenuScrollBox->GetChildrenCount());
		UE_LOG(LogTemp, Warning, TEXT("  SubmenuScrollBox IsVisible: %d"), SubmenuScrollBox->IsVisible());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  SubmenuScrollBox is NULL!"));
	}

	if (BackButton)
	{
		ESlateVisibility BackVis = (CurrentState != EDialogueState::MainMenu) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		BackButton->SetVisibility(BackVis);
		UE_LOG(LogTemp, Warning, TEXT("  BackButton visibility: %d"), (int32)BackVis);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  BackButton is NULL!"));
	}
}

void UTalkDialogueWidget::PopulateSubmenu()
{
	if (!SubmenuScrollBox)
	{
		UE_LOG(LogTemp, Error, TEXT("SubmenuScrollBox is null!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PopulateSubmenu called for state: %d"), (int32)CurrentState);

	// Clear existing buttons
	SubmenuScrollBox->ClearChildren();

	// Add buttons based on current state
	switch (CurrentState)
	{
		case EDialogueState::TradeMenu:
		{
			// If AI has an active trade request, show acceptance buttons
			APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
			if (GameMode && GameMode->bHasActiveTradeRequest)
			{
				CreateSubmenuButton(FText::FromString(TEXT("Alright")), FName("Alright"));
				CreateSubmenuButton(FText::FromString(TEXT("No deal")), FName("NoDeal"));
				UE_LOG(LogTemp, Log, TEXT("Added 2 trade confirmation buttons (AI request)"));
			}
			else
			{
				CreateSubmenuButton(FText::FromString(TEXT("I want to buy 1,000 Black Substrate")), FName("BuyBlackSubstrate"));
				CreateSubmenuButton(FText::FromString(TEXT("I want to buy 1,000 Orange Substrate")), FName("BuyOrangeSubstrate"));
				CreateSubmenuButton(FText::FromString(TEXT("I want to buy 500 Black Substrate")), FName("Buy500BlackSubstrate"));
				CreateSubmenuButton(FText::FromString(TEXT("I want to buy 500 Orange Substrate")), FName("Buy500OrangeSubstrate"));
				CreateSubmenuButton(FText::FromString(TEXT("Actually, nevermind.")), FName("Back"));
				UE_LOG(LogTemp, Log, TEXT("Added 4 substrate trade menu buttons"));
			}
		}
		break;

		case EDialogueState::TradeConfirmation:
			CreateSubmenuButton(FText::FromString(TEXT("Alright")), FName("Alright"));
			CreateSubmenuButton(FText::FromString(TEXT("No deal")), FName("NoDeal"));
			UE_LOG(LogTemp, Log, TEXT("Added 2 trade confirmation buttons"));
			break;

		case EDialogueState::RequestMenu:
		{
			// If AI has an active alliance request, show acceptance buttons
			APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
			if (GameMode && GameMode->bHasActiveAllianceRequest)
			{
				CreateSubmenuButton(FText::FromString(TEXT("Accept Alliance")), FName("AcceptAlliance"));
				CreateSubmenuButton(FText::FromString(TEXT("Decline Alliance")), FName("DeclineAlliance"));
				UE_LOG(LogTemp, Log, TEXT("Added 2 alliance confirmation buttons"));
			}
			else
			{
				// Check if player is already allied with this team
				if (GameMode && CurrentCity && GameMode->IsPlayerAlliedWith(CurrentCity->OwnerTeam))
				{
					// Allied - show End Alliance and Aid Request options
					CreateSubmenuButton(FText::FromString(TEXT("End Alliance")), FName("EndAlliance"));
					CreateSubmenuButton(FText::FromString(TEXT("Request Military Aid (Vehicles)")), FName("RequestMilitaryAid"));
					CreateSubmenuButton(FText::FromString(TEXT("Request Substrate Aid (Up to 5,000 Orange)")), FName("RequestSubstrateAid"));
				}
				else
				{
					// Not allied - show Request Alliance option
					CreateSubmenuButton(FText::FromString(TEXT("Request Alliance")), FName("RequestAlliance"));
				}
				
				CreateSubmenuButton(FText::FromString(TEXT("Actually, nevermind.")), FName("Back"));
				UE_LOG(LogTemp, Log, TEXT("Added request menu buttons"));
			}
		}
		break;

		case EDialogueState::InfluenceMenu:
		{
			// Show bribe options (cooldown check happens when button is clicked)
			CreateSubmenuButton(FText::FromString(TEXT("Gift 2,000 Orange Substrate (65% chance for +10 relationship)")), FName("BribeOrangeSubstrate"));
			CreateSubmenuButton(FText::FromString(TEXT("Gift 2,000 Black Substrate (65% chance for +10 relationship)")), FName("BribeBlackSubstrate"));
			
			CreateSubmenuButton(FText::FromString(TEXT("Actually, nevermind.")), FName("Back"));
			UE_LOG(LogTemp, Log, TEXT("Influence menu populated with bribe options"));
		}
			break;

		case EDialogueState::TradeCompleted:
			UE_LOG(LogTemp, Warning, TEXT("PopulateSubmenu: Creating 'Of course' button for TradeCompleted state"));
			CreateSubmenuButton(FText::FromString(TEXT("Of course")), FName("OfCourse"));
			UE_LOG(LogTemp, Log, TEXT("Added trade completion button"));
			break;

		case EDialogueState::BribeIncoming:
		{
			// AI is offering a bribe - show acceptance buttons
			CreateSubmenuButton(FText::FromString(TEXT("Accept thankfully (+10 relationship)")), FName("AcceptBribeThankfully"));
			CreateSubmenuButton(FText::FromString(TEXT("Accept suspiciously (-10 relationship)")), FName("AcceptBribeSuspiciously"));
			UE_LOG(LogTemp, Log, TEXT("Added 2 bribe response buttons"));
		}
			break;

		case EDialogueState::RelationsMenu:
		{
			APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
			
			// Check if player is already allied with this team
			if (GameMode && CurrentCity && GameMode->IsPlayerAlliedWith(CurrentCity->OwnerTeam))
			{
				CreateSubmenuButton(FText::FromString(TEXT("End Alliance")), FName("RelationsEndAlliance"));
			}
			else
			{
				CreateSubmenuButton(FText::FromString(TEXT("Request Alliance")), FName("RelationsRequestAlliance"));
			}
			
			// Add a single button to ask about all relationships
			CreateSubmenuButton(FText::FromString(TEXT("What are your relationships with the other teams?")), FName("RelationsAskAboutOtherTeam"));
			
			CreateSubmenuButton(FText::FromString(TEXT("Actually, nevermind.")), FName("Back"));
			UE_LOG(LogTemp, Log, TEXT("Added relations menu buttons"));
		}
		break;

		default:
			break;
	}

	int32 ChildCount = SubmenuScrollBox->GetChildrenCount();
	UE_LOG(LogTemp, Log, TEXT("SubmenuScrollBox now has %d children"), ChildCount);
}

UButton* UTalkDialogueWidget::CreateSubmenuButton(const FText& ButtonText, const FName& FunctionName)
{
	// Create size box to control button dimensions
	USizeBox* SizeBox = NewObject<USizeBox>(this);
	if (!SizeBox) return nullptr;
	
	SizeBox->SetWidthOverride(1440.0f);
	SizeBox->SetHeightOverride(80.0f);

	// Create button widget
	UButton* NewButton = NewObject<UButton>(this);
	if (!NewButton) return nullptr;
	
	// Copy button style from one of the main menu buttons (for colors)
	if (TradeButton)
	{
		FButtonStyle MainButtonStyle = TradeButton->GetStyle();
		NewButton->SetStyle(MainButtonStyle);
	}
	else
	{
		// Fallback if no main button to copy from
		FButtonStyle ButtonStyle;
		ButtonStyle.Normal.TintColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
		ButtonStyle.Hovered.TintColor = FLinearColor(0.2f, 0.2f, 0.3f, 1.0f);
		ButtonStyle.Pressed.TintColor = FLinearColor(0.05f, 0.05f, 0.15f, 1.0f);
		NewButton->SetStyle(ButtonStyle);
	}
	
	// Create text block for button label
	UTextBlock* ButtonLabel = NewObject<UTextBlock>(this);
	if (ButtonLabel)
	{
		ButtonLabel->SetText(ButtonText);
		ButtonLabel->SetJustification(ETextJustify::Center);
		ButtonLabel->SetColorAndOpacity(FLinearColor::White);
		FSlateFontInfo FontInfo = ButtonLabel->GetFont();
		FontInfo.Size = 28;
		ButtonLabel->SetFont(FontInfo);
		
		// Add text to button with standard padding of 10.0
		UButtonSlot* ButtonSlot = Cast<UButtonSlot>(NewButton->AddChild(ButtonLabel));
		if (ButtonSlot)
		{
			UE_LOG(LogTemp, Warning, TEXT("ButtonSlot created successfully for: %s"), *ButtonText.ToString());
			ButtonSlot->SetPadding(FMargin(10.0f));
			FMargin VerifyPadding = ButtonSlot->GetPadding();
			UE_LOG(LogTemp, Warning, TEXT("After SetPadding(10): L=%.1f T=%.1f R=%.1f B=%.1f"), 
				VerifyPadding.Left, VerifyPadding.Top, VerifyPadding.Right, VerifyPadding.Bottom);
			
			// Also try setting horizontal and vertical alignment
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create ButtonSlot for: %s"), *ButtonText.ToString());
		}
	}

	// Add button to size box
	SizeBox->AddChild(NewButton);

	// Add size box to scroll box and set alignment to prevent stretching
	if (SubmenuScrollBox)
	{
		UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(SubmenuScrollBox->AddChild(SizeBox));
		if (ScrollSlot)
		{
			ScrollSlot->SetHorizontalAlignment(HAlign_Center);
			ScrollSlot->SetVerticalAlignment(VAlign_Top);
			ScrollSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 5.0f)); // Vertical spacing between buttons
			UE_LOG(LogTemp, Log, TEXT("Added button with centered alignment: %s"), *ButtonText.ToString());
		}
	}

	// Bind click event based on function name
	if (FunctionName == "Back")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBackButtonClicked);
	}
	else if (FunctionName == "SellBlackSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnSellBlackSubstrateClicked);
	}
	else if (FunctionName == "BuyBlackSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBuyBlackSubstrateClicked);
	}
	else if (FunctionName == "SellOrangeSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnSellOrangeSubstrateClicked);
	}
	else if (FunctionName == "BuyOrangeSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBuyOrangeSubstrateClicked);
	}
	else if (FunctionName == "Buy500BlackSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBuy500BlackSubstrateClicked);
	}
	else if (FunctionName == "Buy500OrangeSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBuy500OrangeSubstrateClicked);
	}
	else if (FunctionName == "Alright")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnAlrightClicked);
	}
	else if (FunctionName == "OfCourse")
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSubmenuButton: Binding 'Of course' button to OnOfCourseClicked"));
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnOfCourseClicked);
	}
	else if (FunctionName == "NoDeal")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnNoDealClicked);
	}
	else if (FunctionName == "AcceptAlliance")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnAcceptAllianceClicked);
	}
	else if (FunctionName == "DeclineAlliance")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnDeclineAllianceClicked);
	}
	else if (FunctionName == "AcceptBribeThankfully")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnAcceptBribeThankfullyClicked);
	}
	else if (FunctionName == "AcceptBribeSuspiciously")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnAcceptBribeSuspiciouslyClicked);
	}
	else if (FunctionName == "RelationsAskAboutOtherTeam")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRelationsAskAboutOtherTeamClicked);
	}
	else if (FunctionName == "RelationsRequestAlliance" || FunctionName == "RequestAlliance")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRequestAllianceClicked);
	}
	else if (FunctionName == "RelationsEndAlliance" || FunctionName == "EndAlliance")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnEndAllianceClicked);
	}
	else if (FunctionName == "RequestMilitaryAid")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRequestMilitaryAidClicked);
	}
	else if (FunctionName == "RequestSubstrateAid")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRequestSubstrateAidClicked);
	}
	else if (FunctionName == "BribeOrangeSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBribeOrangeSubstrateClicked);
	}
	else if (FunctionName == "BribeBlackSubstrate")
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBribeBlackSubstrateClicked);
	}
	else if (FunctionName == "RelationsAskAboutOtherTeam" || FunctionName.ToString().StartsWith("AskAbout_"))
	{
		// Bind to the generic handler that shows all relationships
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnRelationsAskAboutOtherTeamClicked);
	}
	else
	{
		NewButton->OnClicked.AddDynamic(this, &UTalkDialogueWidget::OnBackButtonClicked); // Placeholder for now
	}

	return NewButton;
}

void UTalkDialogueWidget::OnSubmenuButtonClicked(const FName& ActionName)
{
	// Handle specific submenu actions
	UE_LOG(LogTemp, Log, TEXT("Submenu action clicked: %s"), *ActionName.ToString());
}

// Main menu button handlers
void UTalkDialogueWidget::OnTradeButtonClicked()
{
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnTradeButtonClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;
	UE_LOG(LogTemp, Warning, TEXT("OnTradeButtonClicked: Processing click"));
	SetDialogueState(EDialogueState::TradeMenu);
}

void UTalkDialogueWidget::OnRequestButtonClicked()
{
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRequestButtonClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;
	UE_LOG(LogTemp, Warning, TEXT("OnRequestButtonClicked: Processing click"));
	SetDialogueState(EDialogueState::RequestMenu);
}

void UTalkDialogueWidget::OnInfluenceButtonClicked()
{
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnInfluenceButtonClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;
	UE_LOG(LogTemp, Warning, TEXT("OnInfluenceButtonClicked: Processing click"));
	SetDialogueState(EDialogueState::InfluenceMenu);
}

void UTalkDialogueWidget::OnRelationsButtonClicked()
{
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRelationsButtonClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;
	UE_LOG(LogTemp, Warning, TEXT("OnRelationsButtonClicked: Processing click"));
	SetDialogueState(EDialogueState::RelationsMenu);
}

void UTalkDialogueWidget::OnBackButtonClicked()
{
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnBackButtonClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;
	UE_LOG(LogTemp, Warning, TEXT("OnBackButtonClicked: Processing click"));
	SetDialogueState(EDialogueState::MainMenu);
}

// Submenu action handlers (placeholders for now)
void UTalkDialogueWidget::OnTradeResourcesClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Trade Resources clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("A fair trade. We accept your offer.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnTradeTechnologyClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Trade Technology clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("Technology exchange requires mutual benefit.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnTradeMapDataClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Trade Map Data clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("Our cartographers will share their findings.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}



bool UTalkDialogueWidget::CheckTradeCooldown()
{
	if (!CurrentCity) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return false;

	// Check if trade is on cooldown
	if (!GameMode->CanPlayerTradeWithTeam(CurrentCity->OwnerTeam))
	{
		float RemainingCooldown = GameMode->GetTradeCooldownRemaining(CurrentCity->OwnerTeam);
		int32 RemainingCycles = FMath::CeilToInt(RemainingCooldown / 5.0f); // Convert seconds to cycles (5 sec per cycle)
		
		CurrentDialogueTextString = FString::Printf(
			TEXT("We just traded recently. It's too soon to trade again. Let's wait at least %d more cycles."),
			RemainingCycles
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		
		UE_LOG(LogTemp, Log, TEXT("TalkDialogue: Trade blocked - cooldown: %.1f seconds remaining (%d cycles)"),
			RemainingCooldown, RemainingCycles);
		
		return false;
	}

	return true;
}

// Substrate Trade Handlers
void UTalkDialogueWidget::OnSellBlackSubstrateClicked()
{
	// Check trade cooldown first
	if (!CheckTradeCooldown())
	{
		return;
	}

	// Player is offering 1k black substrate to AI
	const int32 OfferAmount = 1000;
	
	// Get AI team controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;
	
	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}
	
	if (!AIController) return;
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;
	
	// Check if player has enough black substrate
	if (PC->PlayerBlackSubstrate < OfferAmount)
	{
		CurrentDialogueTextString = FString::Printf(
			TEXT("You don't have enough black substrate. You only have %d."),
			PC->PlayerBlackSubstrate
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Determine AI's scarcity (which substrate is more scarce for them)
	bool bOrangeIsScarce = AIController->OrangeSubstrate < AIController->BlackSubstrate;
	
	if (bOrangeIsScarce)
	{
		// Orange is scarce for AI - they should offer a poor trade or decline
		int32 MinOrangeRequired = OfferAmount * 0.6f; // 60% of offer
		if (AIController->OrangeSubstrate < MinOrangeRequired)
		{
			// Not interested - don't have enough orange
			CurrentDialogueTextString = TEXT("We're not interested in black substrate at this time. We have other priorities.");
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: AI doesn't have enough orange (has %d)"), AIController->OrangeSubstrate);
		}
		else
		{
			// Poor trade: 60%-80% of offer
			int32 MaxOffer = FMath::Min(static_cast<int32>(OfferAmount * 0.8f), AIController->OrangeSubstrate);
			int32 OfferOrange = FMath::RandRange(MinOrangeRequired, MaxOffer);
			
			CurrentDialogueTextString = FString::Printf(
				TEXT("We'll give you %d orange substrate for your %d black substrate."),
				OfferOrange, OfferAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::SellBlack;
			PendingTradeAmount = OfferAmount;
			PendingAIResponse = OfferOrange;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d black, receives %d orange (poor trade)"), OfferAmount, OfferOrange);
		}
	}
	else
	{
		// Orange is surplus for AI - they should offer a good trade or express interest
		int32 MinOrangeRequired = OfferAmount * 0.8f; // 80% of offer
		if (AIController->OrangeSubstrate < MinOrangeRequired)
		{
			// Interested but can't trade right now
			CurrentDialogueTextString = TEXT("We're very interested in this trade, but we don't have enough orange substrate right now. Let's reconvene in the near future.");
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade interest: AI wants to trade but only has %d orange"), AIController->OrangeSubstrate);
		}
		else
		{
			// Good trade: 80%-110% of offer
			int32 MaxOffer = FMath::Min(static_cast<int32>(OfferAmount * 1.1f), AIController->OrangeSubstrate);
			int32 OfferOrange = FMath::RandRange(MinOrangeRequired, MaxOffer);
			
			CurrentDialogueTextString = FString::Printf(
				TEXT("A fair trade! We'll give you %d orange substrate for your %d black substrate."),
				OfferOrange, OfferAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::SellBlack;
			PendingTradeAmount = OfferAmount;
			PendingAIResponse = OfferOrange;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d black, receives %d orange (good trade)"), OfferAmount, OfferOrange);
		}
	}
}

void UTalkDialogueWidget::OnBuyBlackSubstrateClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnBuyBlackSubstrateClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	// Check trade cooldown first
	if (!CheckTradeCooldown())
	{
		return;
	}

	// Player wants to buy 1k black substrate from AI (offering orange)
	const int32 RequestAmount = 1000;
	
	// Get AI team controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	// Check relationship - refuse trade if hostile
	float Relationship = GameMode->GetDisposition(EOwnerTeam::Player, CurrentCity->OwnerTeam);
	if (Relationship < -20.0f)
	{
		CurrentDialogueTextString = TEXT("We will not trade with you. Our relationship is too hostile.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}
	
	if (!AIController) return;
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;
	
	// Check if AI has enough black substrate
	if (AIController->BlackSubstrate < RequestAmount)
	{
		CurrentDialogueTextString = FString::Printf(
			TEXT("We don't have that much black substrate to trade. We only have %d."),
			AIController->BlackSubstrate
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Determine AI's scarcity (which substrate is more scarce for them)
	bool bBlackIsScarce = AIController->BlackSubstrate < AIController->OrangeSubstrate;
	
	if (bBlackIsScarce)
	{
		// Black is scarce for AI - they should demand more orange or decline
		int32 DemandOrange = RequestAmount * FMath::FRandRange(1.2f, 1.4f); // Poor deal for player (120%-140% of request)
		
		if (PC->PlayerOrangeSubstrate < DemandOrange)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("Black substrate is valuable to us. We'd need at least %d orange substrate for %d black. You don't have enough."),
				DemandOrange, RequestAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d orange but only has %d"), DemandOrange, PC->PlayerOrangeSubstrate);
		}
		else
		{
			// Expensive trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("Black substrate is scarce for us. We'll part with %d black for %d orange substrate."),
				RequestAmount, DemandOrange
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyBlack;
			PendingPlayerOffer = DemandOrange;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d orange, receives %d black (expensive)"), DemandOrange, RequestAmount);
		}
	}
	else
	{
		// Black is surplus for AI - they should offer a fair/good trade
		int32 DemandOrange = RequestAmount * FMath::FRandRange(0.8f, 1.0f); // Fair deal (80%-100% of request)
		
		if (PC->PlayerOrangeSubstrate < DemandOrange)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("We can trade %d black substrate for %d orange. You only have %d orange."),
				RequestAmount, DemandOrange, PC->PlayerOrangeSubstrate
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d orange but only has %d"), DemandOrange, PC->PlayerOrangeSubstrate);
		}
		else
		{
			// Good trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("A fair exchange. %d black substrate for %d orange substrate."),
				RequestAmount, DemandOrange
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyBlack;
			PendingPlayerOffer = DemandOrange;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d orange, receives %d black (fair)"), DemandOrange, RequestAmount);
		}
	}
}

void UTalkDialogueWidget::OnSellOrangeSubstrateClicked()
{
	// Check trade cooldown first
	if (!CheckTradeCooldown())
	{
		return;
	}

	// Player is offering 1k orange substrate to AI
	const int32 OfferAmount = 1000;
	
	// Get AI team controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;
	
	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}
	
	if (!AIController) return;
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;
	
	// Check if player has enough orange substrate
	if (PC->PlayerOrangeSubstrate < OfferAmount)
	{
		CurrentDialogueTextString = FString::Printf(
			TEXT("You don't have enough orange substrate. You only have %d."),
			PC->PlayerOrangeSubstrate
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Determine AI's scarcity (which substrate is more scarce for them)
	bool bBlackIsScarce = AIController->BlackSubstrate < AIController->OrangeSubstrate;
	
	if (bBlackIsScarce)
	{
		// Black is scarce for AI - they should offer a poor trade or decline
		int32 MinBlackRequired = OfferAmount * 0.6f; // 60% of offer
		if (AIController->BlackSubstrate < MinBlackRequired)
		{
			// Not interested - don't have enough black
			CurrentDialogueTextString = TEXT("We're not interested in orange substrate at this time. We have other priorities.");
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: AI doesn't have enough black (has %d)"), AIController->BlackSubstrate);
		}
		else
		{
			// Poor trade: 60%-80% of offer
			int32 MaxOffer = FMath::Min(static_cast<int32>(OfferAmount * 0.8f), AIController->BlackSubstrate);
			int32 OfferBlack = FMath::RandRange(MinBlackRequired, MaxOffer);
			
			CurrentDialogueTextString = FString::Printf(
				TEXT("We'll give you %d black substrate for your %d orange substrate."),
				OfferBlack, OfferAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::SellOrange;
			PendingTradeAmount = OfferAmount;
			PendingAIResponse = OfferBlack;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d orange, receives %d black (poor trade)"), OfferAmount, OfferBlack);
		}
	}
	else
	{
		// Black is surplus for AI - they should offer a good trade or express interest
		int32 MinBlackRequired = OfferAmount * 0.8f; // 80% of offer
		if (AIController->BlackSubstrate < MinBlackRequired)
		{
			// Interested but can't trade right now
			CurrentDialogueTextString = TEXT("We're very interested in this trade, but we don't have enough black substrate right now. Let's reconvene in the near future.");
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade interest: AI wants to trade but only has %d black"), AIController->BlackSubstrate);
		}
		else
		{
			// Good trade: 80%-110% of offer
			int32 MaxOffer = FMath::Min(static_cast<int32>(OfferAmount * 1.1f), AIController->BlackSubstrate);
			int32 OfferBlack = FMath::RandRange(MinBlackRequired, MaxOffer);
			
			CurrentDialogueTextString = FString::Printf(
				TEXT("A fair trade! We'll give you %d black substrate for your %d orange substrate."),
				OfferBlack, OfferAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::SellOrange;
			PendingTradeAmount = OfferAmount;
			PendingAIResponse = OfferBlack;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d orange, receives %d black (good trade)"), OfferAmount, OfferBlack);
		}
	}
}

void UTalkDialogueWidget::OnBuyOrangeSubstrateClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnBuyOrangeSubstrateClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	// Check trade cooldown first
	if (!CheckTradeCooldown())
	{
		return;
	}

	// Player wants to buy 1k orange substrate from AI (offering black)
	const int32 RequestAmount = 1000;
	
	// Get AI team controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	// Check relationship - refuse trade if hostile
	float Relationship = GameMode->GetDisposition(EOwnerTeam::Player, CurrentCity->OwnerTeam);
	if (Relationship < -20.0f)
	{
		CurrentDialogueTextString = TEXT("We will not trade with you. Our relationship is too hostile.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}
	
	if (!AIController) return;
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;
	
	// Check if AI has enough orange substrate
	if (AIController->OrangeSubstrate < RequestAmount)
	{
		CurrentDialogueTextString = FString::Printf(
			TEXT("We don't have that much orange substrate to trade. We only have %d."),
			AIController->OrangeSubstrate
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Determine AI's scarcity (which substrate is more scarce for them)
	bool bOrangeIsScarce = AIController->OrangeSubstrate < AIController->BlackSubstrate;
	
	if (bOrangeIsScarce)
	{
		// Orange is scarce for AI - they should demand more black or decline
		int32 DemandBlack = RequestAmount * FMath::FRandRange(1.2f, 1.4f); // Poor deal for player (120%-140% of request)
		
		if (PC->PlayerBlackSubstrate < DemandBlack)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("Orange substrate is valuable to us. We'd need at least %d black substrate for %d orange. You don't have enough."),
				DemandBlack, RequestAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d black but only has %d"), DemandBlack, PC->PlayerBlackSubstrate);
		}
		else
		{
			// Expensive trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("Orange substrate is scarce for us. We'll part with %d orange for %d black substrate."),
				RequestAmount, DemandBlack
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyOrange;
			PendingPlayerOffer = DemandBlack;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d black, receives %d orange (expensive)"), DemandBlack, RequestAmount);
		}
	}
	else
	{
		// Orange is surplus for AI - they should offer a fair/good trade
		int32 DemandBlack = RequestAmount * FMath::FRandRange(0.8f, 1.0f); // Fair deal (80%-100% of request)
		
		if (PC->PlayerBlackSubstrate < DemandBlack)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("We can trade %d orange substrate for %d black. You only have %d black."),
				RequestAmount, DemandBlack, PC->PlayerBlackSubstrate
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d black but only has %d"), DemandBlack, PC->PlayerBlackSubstrate);
		}
		else
		{
			// Good trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("A fair exchange. %d orange substrate for %d black substrate."),
				RequestAmount, DemandBlack
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyOrange;
			PendingPlayerOffer = DemandBlack;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d black, receives %d orange (fair)"), DemandBlack, RequestAmount);
		}
	}
}

void UTalkDialogueWidget::OnBuy500BlackSubstrateClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnBuy500BlackSubstrateClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	// Check trade cooldown first
	if (!CheckTradeCooldown())
	{
		return;
	}

	// Player wants to buy 500 black substrate from AI (offering orange)
	const int32 RequestAmount = 500;
	
	// Get AI team controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	// Check relationship - refuse trade if hostile
	float Relationship = GameMode->GetDisposition(EOwnerTeam::Player, CurrentCity->OwnerTeam);
	if (Relationship < -20.0f)
	{
		CurrentDialogueTextString = TEXT("We will not trade with you. Our relationship is too hostile.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}
	
	if (!AIController) return;
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;
	
	// Check if AI has enough black substrate
	if (AIController->BlackSubstrate < RequestAmount)
	{
		CurrentDialogueTextString = FString::Printf(
			TEXT("We don't have that much black substrate to trade. We only have %d."),
			AIController->BlackSubstrate
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Determine AI's scarcity (which substrate is more scarce for them)
	bool bBlackIsScarce = AIController->BlackSubstrate < AIController->OrangeSubstrate;
	
	if (bBlackIsScarce)
	{
		// Black is scarce for AI - they should demand more orange or decline
		int32 DemandOrange = RequestAmount * FMath::FRandRange(1.2f, 1.4f); // Poor deal for player (120%-140% of request)
		
		if (PC->PlayerOrangeSubstrate < DemandOrange)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("Black substrate is valuable to us. We'd need at least %d orange substrate for %d black. You don't have enough."),
				DemandOrange, RequestAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d orange but only has %d"), DemandOrange, PC->PlayerOrangeSubstrate);
		}
		else
		{
			// Expensive trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("Black substrate is scarce for us. We'll part with %d black for %d orange substrate."),
				RequestAmount, DemandOrange
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyBlack;
			PendingPlayerOffer = DemandOrange;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d orange, receives %d black (expensive)"), DemandOrange, RequestAmount);
		}
	}
	else
	{
		// Black is surplus for AI - they should offer a fair/good trade
		int32 DemandOrange = RequestAmount * FMath::FRandRange(0.8f, 1.0f); // Fair deal (80%-100% of request)
		
		if (PC->PlayerOrangeSubstrate < DemandOrange)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("We can trade %d black substrate for %d orange. You only have %d orange."),
				RequestAmount, DemandOrange, PC->PlayerOrangeSubstrate
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d orange but only has %d"), DemandOrange, PC->PlayerOrangeSubstrate);
		}
		else
		{
			// Good trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("A fair exchange. %d black substrate for %d orange substrate."),
				RequestAmount, DemandOrange
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyBlack;
			PendingPlayerOffer = DemandOrange;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d orange, receives %d black (fair)"), DemandOrange, RequestAmount);
		}
	}
}

void UTalkDialogueWidget::OnBuy500OrangeSubstrateClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnBuy500OrangeSubstrateClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	// Check trade cooldown first
	if (!CheckTradeCooldown())
	{
		return;
	}

	// Player wants to buy 500 orange substrate from AI (offering black)
	const int32 RequestAmount = 500;
	
	// Get AI team controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	// Check relationship - refuse trade if hostile
	float Relationship = GameMode->GetDisposition(EOwnerTeam::Player, CurrentCity->OwnerTeam);
	if (Relationship < -20.0f)
	{
		CurrentDialogueTextString = TEXT("We will not trade with you. Our relationship is too hostile.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}
	
	if (!AIController) return;
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;
	
	// Check if AI has enough orange substrate
	if (AIController->OrangeSubstrate < RequestAmount)
	{
		CurrentDialogueTextString = FString::Printf(
			TEXT("We don't have that much orange substrate to trade. We only have %d."),
			AIController->OrangeSubstrate
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Determine AI's scarcity (which substrate is more scarce for them)
	bool bOrangeIsScarce = AIController->OrangeSubstrate < AIController->BlackSubstrate;
	
	if (bOrangeIsScarce)
	{
		// Orange is scarce for AI - they should demand more black or decline
		int32 DemandBlack = RequestAmount * FMath::FRandRange(1.2f, 1.4f); // Poor deal for player (120%-140% of request)
		
		if (PC->PlayerBlackSubstrate < DemandBlack)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("Orange substrate is valuable to us. We'd need at least %d black substrate for %d orange. You don't have enough."),
				DemandBlack, RequestAmount
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d black but only has %d"), DemandBlack, PC->PlayerBlackSubstrate);
		}
		else
		{
			// Expensive trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("Orange substrate is scarce for us. We'll part with %d orange for %d black substrate."),
				RequestAmount, DemandBlack
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyOrange;
			PendingPlayerOffer = DemandBlack;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d black, receives %d orange (expensive)"), DemandBlack, RequestAmount);
		}
	}
	else
	{
		// Orange is surplus for AI - they should offer a fair/good trade
		int32 DemandBlack = RequestAmount * FMath::FRandRange(0.8f, 1.0f); // Fair deal (80%-100% of request)
		
		if (PC->PlayerBlackSubstrate < DemandBlack)
		{
			// Player can't afford it
			CurrentDialogueTextString = FString::Printf(
				TEXT("We can trade %d orange substrate for %d black. You only have %d black."),
				RequestAmount, DemandBlack, PC->PlayerBlackSubstrate
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			UE_LOG(LogTemp, Log, TEXT("Trade declined: Player needs %d black but only has %d"), DemandBlack, PC->PlayerBlackSubstrate);
		}
		else
		{
			// Good trade for player
			CurrentDialogueTextString = FString::Printf(
				TEXT("A fair exchange. %d orange substrate for %d black substrate."),
				RequestAmount, DemandBlack
			);
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			
			// Store pending trade details
			PendingTradeType = EPendingTradeType::BuyOrange;
			PendingPlayerOffer = DemandBlack;
			PendingAIResponse = RequestAmount;
			
			// Mark trade as initiated (starts cooldown)
			GameMode->MarkTradeInitiated(CurrentCity->OwnerTeam);
			
			// Transition to confirmation state
			SetDialogueState(EDialogueState::TradeConfirmation);
			
			UE_LOG(LogTemp, Log, TEXT("Trade offer: Player gives %d black, receives %d orange (fair)"), DemandBlack, RequestAmount);
		}
	}
}

void UTalkDialogueWidget::OnRequestCeasefireClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Request Ceasefire clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("Very well. We agree to a temporary ceasefire.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnRequestAllianceClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRequestAllianceClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && CurrentCity)
	{
		// Find a common enemy (team that both player and AI hate)
		EOwnerTeam CommonEnemy = EOwnerTeam::Neutral;
		float BestEnemyScore = 0.0f;
		
		TArray<EOwnerTeam> PotentialEnemies = { EOwnerTeam::AI1, EOwnerTeam::AI2, EOwnerTeam::AI3, 
												EOwnerTeam::AI4, EOwnerTeam::AI5, EOwnerTeam::AI6,
												EOwnerTeam::AI7, EOwnerTeam::AI8 };
		
		for (EOwnerTeam PotentialEnemy : PotentialEnemies)
		{
			// Skip the AI we're talking to
			if (PotentialEnemy == CurrentCity->OwnerTeam) continue;
			
			// Skip eliminated teams (no cities)
			int32 PotentialEnemyCities = GameMode->CountCitiesForTeam(PotentialEnemy);
			if (PotentialEnemyCities == 0) continue;
			
			float PlayerRelationship = GameMode->GetDisposition(EOwnerTeam::Player, PotentialEnemy);
			float AIRelationship = GameMode->GetDisposition(CurrentCity->OwnerTeam, PotentialEnemy);
			
			// Both must hate this team
			if (PlayerRelationship < 0.0f && AIRelationship < 0.0f)
			{
				// Prefer the team that both hate the most
				float CombinedHatred = -(PlayerRelationship + AIRelationship);
				if (CombinedHatred > BestEnemyScore)
				{
					BestEnemyScore = CombinedHatred;
					CommonEnemy = PotentialEnemy;
				}
			}
		}
		
		// Get enemy team name for the message
		FString EnemyName = TEXT("our mutual enemy");
		if (CommonEnemy == EOwnerTeam::AI1) EnemyName = TEXT("AI Team 2");
		else if (CommonEnemy == EOwnerTeam::AI2) EnemyName = TEXT("AI Team 3");
		else if (CommonEnemy == EOwnerTeam::AI3) EnemyName = TEXT("AI Team 4");
		else if (CommonEnemy == EOwnerTeam::AI4) EnemyName = TEXT("AI Team 5");
		else if (CommonEnemy == EOwnerTeam::AI5) EnemyName = TEXT("AI Team 6");
		else if (CommonEnemy == EOwnerTeam::AI6) EnemyName = TEXT("AI Team 7");
		
		bool bSuccess = GameMode->PlayerRequestAlliance(CurrentCity->OwnerTeam, CommonEnemy);
		if (bSuccess)
		{
			if (CommonEnemy != EOwnerTeam::Neutral)
			{
				CurrentDialogueTextString = FString::Printf(
					TEXT("An alliance is formed. Together, we will stand against %s."),
					*EnemyName
				);
			}
			else
			{
				CurrentDialogueTextString = TEXT("An alliance is formed. Together, we are stronger.");
			}
		}
		else
		{
			CurrentDialogueTextString = TEXT("We must decline. Our relationship is not strong enough, or you are allied with our enemies.");
		}
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
}

void UTalkDialogueWidget::OnEndAllianceClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnEndAllianceClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && CurrentCity)
	{
		bool bSuccess = GameMode->PlayerEndAlliance(CurrentCity->OwnerTeam);
		if (bSuccess)
		{
			CurrentDialogueTextString = TEXT("So be it. Our alliance is ended. Do not expect our aid in the future.");
		}
		else
		{
			CurrentDialogueTextString = TEXT("We were not allied. There is nothing to end.");
		}
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
}

void UTalkDialogueWidget::OnRequestMilitaryAidClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRequestMilitaryAidClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	if (!CurrentCity) return;
	
	// Get player controller
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController is null when requesting aid!"));
		return;
	}
	
	// Check if player has cities under attack
	ACityActor* AttackedCity = nullptr;
	for (ACityActor* City : PlayerController->ControlledCities)
	{
		if (City && IsValid(City))
		{
			// Check for enemy vehicles nearby
			TArray<AActor*> AllVehicles;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
			
			float TerritoryRadius = 5000.0f;
			for (AActor* Actor : AllVehicles)
			{
				AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
				if (!EnemyVehicle || !IsValid(EnemyVehicle)) continue;
				if (EnemyVehicle->OwnerTeam == EOwnerTeam::Player) continue;
				
				float Distance = FVector::Dist(EnemyVehicle->GetActorLocation(), City->GetActorLocation());
				if (Distance <= TerritoryRadius)
				{
					AttackedCity = City;
					break;
				}
			}
			if (AttackedCity) break;
		}
	}
	
	if (!AttackedCity)
	{
		CurrentDialogueTextString = TEXT("We are not currently under attack. We do not need military assistance.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Find AI controller for current city
	TArray<AActor*> FoundControllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
	
	for (AActor* Actor : FoundControllers)
	{
		AAITeamController* AIController = Cast<AAITeamController>(Actor);
		if (AIController && AIController->ControlledTeam == CurrentCity->OwnerTeam)
		{
			// Request military aid from this AI
			int32 VehiclesSent = AIController->SendMilitaryAid(EOwnerTeam::Player, AttackedCity);
			
			if (VehiclesSent > 0)
			{
				CurrentDialogueTextString = FString::Printf(TEXT("We are sending %d vehicles to assist in your defense. Hold strong!"), VehiclesSent);
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d sent %d vehicles to player"), (int32)CurrentCity->OwnerTeam, VehiclesSent);
			}
			else
			{
				CurrentDialogueTextString = TEXT("We have no forces to spare at this time. You are on your own.");
			}
			
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			return;
		}
	}
	
	CurrentDialogueTextString = TEXT("We cannot provide assistance at this time.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnRequestSubstrateAidClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRequestSubstrateAidClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	if (!CurrentCity) return;
	
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController is null when requesting substrate aid!"));
		return;
	}
	
	// Check if player has cities under attack
	ACityActor* AttackedCity = nullptr;
	for (ACityActor* City : PlayerController->ControlledCities)
	{
		if (City && IsValid(City))
		{
			TArray<AActor*> AllVehicles;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
			
			float TerritoryRadius = 5000.0f;
			for (AActor* Actor : AllVehicles)
			{
				AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
				if (!EnemyVehicle || !IsValid(EnemyVehicle)) continue;
				if (EnemyVehicle->OwnerTeam == EOwnerTeam::Player) continue;
				
				float Distance = FVector::Dist(EnemyVehicle->GetActorLocation(), City->GetActorLocation());
				if (Distance <= TerritoryRadius)
				{
					AttackedCity = City;
					break;
				}
			}
			if (AttackedCity) break;
		}
	}
	
	if (!AttackedCity)
	{
		CurrentDialogueTextString = TEXT("We are not under attack. We do not need emergency substrate.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		return;
	}
	
	// Find AI controller for current city
	TArray<AActor*> FoundControllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
	
	for (AActor* Actor : FoundControllers)
	{
		AAITeamController* AIController = Cast<AAITeamController>(Actor);
		if (AIController && AIController->ControlledTeam == CurrentCity->OwnerTeam)
		{
			// Request 2000 orange substrate by default
			int32 AmountSent = AIController->SendSubstrateAid(EOwnerTeam::Player, 2000);
			
			if (AmountSent > 0)
			{
				CurrentDialogueTextString = FString::Printf(TEXT("We are sending you %d orange substrate. Use it wisely!"), AmountSent);
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d sent %d orange to player"), (int32)CurrentCity->OwnerTeam, AmountSent);
			}
			else
			{
				CurrentDialogueTextString = TEXT("We cannot spare any substrate at this time. Our resources are strained.");
			}
			
			if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
			return;
		}
	}
	
	CurrentDialogueTextString = TEXT("We cannot provide substrate assistance at this time.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnSend1000OrangeAidClicked()
{
	if (!PendingAidRequester) return;
	
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PlayerController) return;
	
	int32 AmountToSend = FMath::Min(1000, PlayerController->PlayerOrangeSubstrate);
	if (AmountToSend > 0)
	{
		PlayerController->PlayerOrangeSubstrate -= AmountToSend;
		PendingAidRequester->OrangeSubstrate += AmountToSend;
		
		CurrentDialogueTextString = FString::Printf(TEXT("Thank you for the %d orange substrate. We will not forget this!"), AmountToSend);
		UE_LOG(LogTemp, Warning, TEXT("Player sent %d orange aid to AI Team %d"), AmountToSend, (int32)PendingAidRequester->ControlledTeam);
	}
	else
	{
		CurrentDialogueTextString = TEXT("You... you have nothing to give! This will be remembered.");
	}
	
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	
	// Mark request as fulfilled
	for (FAidRequest& Request : PendingAidRequester->AidRequests)
	{
		if (Request.RequestingTeam == PendingAidRequester->ControlledTeam && !Request.bPlayerHasResponded)
		{
			Request.bPlayerHasResponded = true;
			Request.SubstrateProvided = AmountToSend;
			break;
		}
	}
	
	PendingAidRequester = nullptr;
	CityUnderAttack = nullptr;
	CloseDialogue();
}

void UTalkDialogueWidget::OnSend2000OrangeAidClicked()
{
	if (!PendingAidRequester) return;
	
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PlayerController) return;
	
	int32 AmountToSend = FMath::Min(2000, PlayerController->PlayerOrangeSubstrate);
	if (AmountToSend > 0)
	{
		PlayerController->PlayerOrangeSubstrate -= AmountToSend;
		PendingAidRequester->OrangeSubstrate += AmountToSend;
		
		CurrentDialogueTextString = FString::Printf(TEXT("Your generosity will not be forgotten! %d orange substrate received!"), AmountToSend);
		UE_LOG(LogTemp, Warning, TEXT("Player sent %d orange aid to AI Team %d"), AmountToSend, (int32)PendingAidRequester->ControlledTeam);
	}
	else
	{
		CurrentDialogueTextString = TEXT("You... you have nothing to give! This will be remembered.");
	}
	
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	
	for (FAidRequest& Request : PendingAidRequester->AidRequests)
	{
		if (Request.RequestingTeam == PendingAidRequester->ControlledTeam && !Request.bPlayerHasResponded)
		{
			Request.bPlayerHasResponded = true;
			Request.SubstrateProvided = AmountToSend;
			break;
		}
	}
	
	PendingAidRequester = nullptr;
	CityUnderAttack = nullptr;
	CloseDialogue();
}

void UTalkDialogueWidget::OnSend5000OrangeAidClicked()
{
	if (!PendingAidRequester) return;
	
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PlayerController) return;
	
	int32 AmountToSend = FMath::Min(5000, PlayerController->PlayerOrangeSubstrate);
	if (AmountToSend > 0)
	{
		PlayerController->PlayerOrangeSubstrate -= AmountToSend;
		PendingAidRequester->OrangeSubstrate += AmountToSend;
		
		CurrentDialogueTextString = FString::Printf(TEXT("By the stars! %d orange substrate! You are a true ally!"), AmountToSend);
		UE_LOG(LogTemp, Warning, TEXT("Player sent %d orange aid to AI Team %d"), AmountToSend, (int32)PendingAidRequester->ControlledTeam);
	}
	else
	{
		CurrentDialogueTextString = TEXT("You... you have nothing to give! This will be remembered.");
	}
	
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	
	for (FAidRequest& Request : PendingAidRequester->AidRequests)
	{
		if (Request.RequestingTeam == PendingAidRequester->ControlledTeam && !Request.bPlayerHasResponded)
		{
			Request.bPlayerHasResponded = true;
			Request.SubstrateProvided = AmountToSend;
			break;
		}
	}
	
	PendingAidRequester = nullptr;
	CityUnderAttack = nullptr;
	CloseDialogue();
}

void UTalkDialogueWidget::OnSendMilitaryAidClicked()
{
	if (!PendingAidRequester || !CityUnderAttack) return;
	
	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PlayerController) return;
	
	// Send military aid from player
	int32 VehiclesSent = PlayerController->SendMilitaryAidToAlly(PendingAidRequester->ControlledTeam, CityUnderAttack);
	
	if (VehiclesSent > 0)
	{
		CurrentDialogueTextString = FString::Printf(TEXT("Your %d vehicles arrive! We are saved!"), VehiclesSent);
		UE_LOG(LogTemp, Warning, TEXT("Player sent %d vehicles to aid AI Team %d"), VehiclesSent, (int32)PendingAidRequester->ControlledTeam);
	}
	else
	{
		CurrentDialogueTextString = TEXT("You have no forces to spare? We will remember this betrayal.");
	}
	
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	
	for (FAidRequest& Request : PendingAidRequester->AidRequests)
	{
		if (Request.RequestingTeam == PendingAidRequester->ControlledTeam && !Request.bPlayerHasResponded)
		{
			Request.bPlayerHasResponded = true;
			Request.bMilitaryAidSent = (VehiclesSent > 0);
			break;
		}
	}
	
	PendingAidRequester = nullptr;
	CityUnderAttack = nullptr;
	CloseDialogue();
}

void UTalkDialogueWidget::OnDeclineAidRequestClicked()
{
	if (!PendingAidRequester) return;
	
	CurrentDialogueTextString = TEXT("So... you abandon us in our time of need. We will not forget this.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	
	for (FAidRequest& Request : PendingAidRequester->AidRequests)
	{
		if (Request.RequestingTeam == PendingAidRequester->ControlledTeam && !Request.bPlayerHasResponded)
		{
			Request.bPlayerHasResponded = true;
			Request.SubstrateProvided = 0;
			Request.bMilitaryAidSent = false;
			break;
		}
	}
	
	PendingAidRequester = nullptr;
	CityUnderAttack = nullptr;
	CloseDialogue();
}

void UTalkDialogueWidget::OnInfluenceThreatClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Influence Threat clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("Your threats do not intimidate us... but we will comply.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnInfluenceBribeClicked()
{
	// This is now handled by OnBribeOrangeSubstrateClicked and OnBribeBlackSubstrateClicked
	UE_LOG(LogTemp, Warning, TEXT("Influence Bribe clicked - redirecting to submenu"));
	SetDialogueState(EDialogueState::InfluenceMenu);
}

void UTalkDialogueWidget::OnBribeOrangeSubstrateClicked()
{
	if (!CurrentCity)
	{
		UE_LOG(LogTemp, Error, TEXT("OnBribeOrangeSubstrateClicked: No current city"));
		return;
	}

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("OnBribeOrangeSubstrateClicked: No game mode"));
		return;
	}

	// Check if bribe is on cooldown
	if (!GameMode->CanPlayerBribeTeam(CurrentCity->OwnerTeam))
	{
		float RemainingCooldown = GameMode->GetBribeCooldownRemaining(CurrentCity->OwnerTeam);
		int32 RemainingCycles = FMath::CeilToInt(RemainingCooldown / 5.0f); // Convert seconds to cycles (5 sec per cycle)
		
		CurrentDialogueTextString = FString::Printf(
			TEXT("You just gave us a generous gift recently. We cannot accept another so soon. Perhaps in %d more cycles."),
			RemainingCycles
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		
		UE_LOG(LogTemp, Log, TEXT("Bribe blocked - cooldown: %.1f seconds remaining (%d cycles)"),
			RemainingCooldown, RemainingCycles);
		
		// Show acknowledgment button and return to main menu
		SetDialogueState(EDialogueState::TradeCompleted);
		return;
	}

	// Attempt the bribe
	bool bSuccess = GameMode->AttemptBribe(CurrentCity->OwnerTeam, true); // true = use orange substrate

	if (bSuccess)
	{
		// Bribe succeeded - relationships improved
		CurrentDialogueTextString = TEXT("Your generosity is most appreciated. We will remember this kindness.");
		UE_LOG(LogTemp, Log, TEXT("Bribe with orange substrate succeeded!"));
	}
	else
	{
		// Check if it failed due to insufficient resources
		APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
		if (PC && PC->PlayerOrangeSubstrate < 2000)
		{
			CurrentDialogueTextString = FString::Printf(
				TEXT("You don't have enough orange substrate. You need 2,000 but only have %d."),
				PC->PlayerOrangeSubstrate
			);
		}
		else
		{
			// Bribe was attempted but failed (35% chance) - still took the substrate
			CurrentDialogueTextString = TEXT("We appreciate the gesture, but remain cautious of gifts with unclear intentions.");
		}
		UE_LOG(LogTemp, Warning, TEXT("Bribe with orange substrate failed"));
	}

	// Update dialogue text
	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}

	// Update relationship display
	if (RelationshipText)
	{
		RelationshipText->SetText(GetRelationshipText());
		RelationshipText->SetColorAndOpacity(FSlateColor(GetRelationshipColor()));
	}

	// Transition to TradeCompleted state to show "Of course" button
	SetDialogueState(EDialogueState::TradeCompleted);
}

void UTalkDialogueWidget::OnBribeBlackSubstrateClicked()
{
	if (!CurrentCity)
	{
		UE_LOG(LogTemp, Error, TEXT("OnBribeBlackSubstrateClicked: No current city"));
		return;
	}

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("OnBribeBlackSubstrateClicked: No game mode"));
		return;
	}

	// Check if bribe is on cooldown
	if (!GameMode->CanPlayerBribeTeam(CurrentCity->OwnerTeam))
	{
		float RemainingCooldown = GameMode->GetBribeCooldownRemaining(CurrentCity->OwnerTeam);
		int32 RemainingCycles = FMath::CeilToInt(RemainingCooldown / 5.0f); // Convert seconds to cycles (5 sec per cycle)
		
		CurrentDialogueTextString = FString::Printf(
			TEXT("You just gave us a generous gift recently. We cannot accept another so soon. Perhaps in %d more cycles."),
			RemainingCycles
		);
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		
		UE_LOG(LogTemp, Log, TEXT("Bribe blocked - cooldown: %.1f seconds remaining (%d cycles)"),
			RemainingCooldown, RemainingCycles);
		
		// Show acknowledgment button and return to main menu
		SetDialogueState(EDialogueState::TradeCompleted);
		return;
	}

	// Attempt the bribe
	bool bSuccess = GameMode->AttemptBribe(CurrentCity->OwnerTeam, false); // false = use black substrate

	if (bSuccess)
	{
		// Bribe succeeded - relationships improved
		CurrentDialogueTextString = TEXT("Your generosity is most appreciated. We will remember this kindness.");
		UE_LOG(LogTemp, Log, TEXT("Bribe with black substrate succeeded!"));
	}
	else
	{
		// Check if it failed due to insufficient resources
		APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
		if (PC && PC->PlayerBlackSubstrate < 2000)
		{
			CurrentDialogueTextString = FString::Printf(
				TEXT("You don't have enough black substrate. You need 2,000 but only have %d."),
				PC->PlayerBlackSubstrate
			);
		}
		else
		{
			// Bribe was attempted but failed (35% chance) - still took the substrate
			CurrentDialogueTextString = TEXT("We appreciate the gesture, but remain cautious of gifts with unclear intentions.");
		}
		UE_LOG(LogTemp, Warning, TEXT("Bribe with black substrate failed"));
	}

	// Update dialogue text
	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}

	// Update relationship display
	if (RelationshipText)
	{
		RelationshipText->SetText(GetRelationshipText());
		RelationshipText->SetColorAndOpacity(FSlateColor(GetRelationshipColor()));
	}

	// Transition to TradeCompleted state to show "Of course" button
	SetDialogueState(EDialogueState::TradeCompleted);
}

void UTalkDialogueWidget::OnInfluencePropagandaClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Influence Propaganda clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("Public opinion is a powerful tool indeed.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnRelationsDeclareWarClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Declare War clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("So be it. This means war!");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnRelationsMakePeaceClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Make Peace clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("We welcome peace between our peoples.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnRelationsViewHistoryClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("View History clicked - Not yet implemented"));
	CurrentDialogueTextString = TEXT("Our relationship has been... complicated.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
}

void UTalkDialogueWidget::OnRelationsAskAboutOtherTeamClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRelationsAskAboutOtherTeamClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	if (!CurrentCity)
	{
		UE_LOG(LogTemp, Error, TEXT("CurrentCity is null!"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode is null!"));
		return;
	}

	// Get all AI teams except the current one
	TArray<EOwnerTeam> AllTeams = {EOwnerTeam::AI1, EOwnerTeam::AI2, EOwnerTeam::AI3, EOwnerTeam::AI4, EOwnerTeam::AI5, EOwnerTeam::AI6, EOwnerTeam::AI7, EOwnerTeam::AI8};
	EOwnerTeam CurrentAITeam = CurrentCity->OwnerTeam;
	
	// Build a comprehensive relationship summary
	TArray<FString> RelationshipLines;
	
	for (EOwnerTeam OtherTeam : AllTeams)
	{
		if (OtherTeam == CurrentAITeam) continue;
		
		// Skip eliminated teams (no cities)
		int32 OtherTeamCities = GameMode->CountCitiesForTeam(OtherTeam);
		if (OtherTeamCities == 0) continue;
		
		// Get the relationship with this team
		float RelationshipValue = GameMode->GetDisposition(CurrentAITeam, OtherTeam);
		
		// Convert to descriptive text
		FString RelationshipDescription;
		if (RelationshipValue >= 50.0f)
		{
			RelationshipDescription = TEXT("close allies");
		}
		else if (RelationshipValue >= 20.0f)
		{
			RelationshipDescription = TEXT("friendly");
		}
		else if (RelationshipValue >= 0.0f)
		{
			RelationshipDescription = TEXT("neutral");
		}
		else if (RelationshipValue >= -30.0f)
		{
			RelationshipDescription = TEXT("unfriendly");
		}
		else if (RelationshipValue >= -60.0f)
		{
			RelationshipDescription = TEXT("hostile");
		}
		else
		{
			RelationshipDescription = TEXT("bitter enemies");
		}
		
		// Get the other team's name
		FString OtherTeamName;
		if (OtherTeam == EOwnerTeam::AI1) OtherTeamName = TEXT("Team 2");
		else if (OtherTeam == EOwnerTeam::AI2) OtherTeamName = TEXT("Team 3");
		else if (OtherTeam == EOwnerTeam::AI3) OtherTeamName = TEXT("Team 4");
		else if (OtherTeam == EOwnerTeam::AI4) OtherTeamName = TEXT("Team 5");
		else if (OtherTeam == EOwnerTeam::AI5) OtherTeamName = TEXT("Team 6");
		else if (OtherTeam == EOwnerTeam::AI6) OtherTeamName = TEXT("Team 7");
		else if (OtherTeam == EOwnerTeam::AI7) OtherTeamName = TEXT("Team 8");
		else if (OtherTeam == EOwnerTeam::AI8) OtherTeamName = TEXT("Team 9");
		
		// Add this relationship to the list
		FString RelationLine = FString::Printf(TEXT("%s: %s (%.0f)"), *OtherTeamName, *RelationshipDescription, RelationshipValue);
		RelationshipLines.Add(RelationLine);
		
		UE_LOG(LogTemp, Log, TEXT("AI Team relationship query: %d with %s = %.2f (%s)"), 
			(int32)CurrentAITeam,
			*OtherTeamName,
			RelationshipValue,
			*RelationshipDescription);
	}
	
	// Format the complete response with all relationships in two columns
	CurrentDialogueTextString = TEXT("Our relationships with the other teams?\n\n");
	
	// Split into two columns (4 teams each)
	int32 MidPoint = (RelationshipLines.Num() + 1) / 2; // Round up for left column
	int32 MaxLines = FMath::Max(MidPoint, RelationshipLines.Num() - MidPoint);
	
	for (int32 i = 0; i < MaxLines; i++)
	{
		FString LeftColumn = (i < MidPoint && i < RelationshipLines.Num()) ? RelationshipLines[i] : TEXT("");
		FString RightColumn = ((i + MidPoint) < RelationshipLines.Num()) ? RelationshipLines[i + MidPoint] : TEXT("");
		
		// Pad left column to 35 characters for alignment
		LeftColumn = LeftColumn.RightPad(35);
		
		CurrentDialogueTextString += LeftColumn + TEXT("  ") + RightColumn + TEXT("\n");
	}

	if (DialogueText) 
	{
		DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
}

// Trade confirmation handlers
void UTalkDialogueWidget::OnAlrightClicked()
{
	// Ignore double-clicks
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnAlrightClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;

	// Get AI controller
	UWorld* World = GetWorld();
	if (!World || !CurrentCity) return;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return;

	// Check if this is an AI-initiated trade request
	if (GameMode->bHasActiveTradeRequest)
	{
		GameMode->AcceptCurrentTradeRequest();
		
		// Show a random trade completion message from AI
		TArray<FString> TradeCompletionMessages = {
			TEXT("This exchange has been fruitful."),
			TEXT("Thank you for trading with us."),
			TEXT("A mutually beneficial arrangement."),
			TEXT("Our partnership grows stronger."),
			TEXT("An excellent trade. We are pleased.")
		};
		
		int32 RandomIndex = FMath::RandRange(0, TradeCompletionMessages.Num() - 1);
		CurrentDialogueTextString = TradeCompletionMessages[RandomIndex];
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
		
		// Update relationship display
		if (RelationshipText)
		{
			RelationshipText->SetText(GetRelationshipText());
			RelationshipText->SetColorAndOpacity(FSlateColor(GetRelationshipColor()));
		}
		
		// Transition to trade completion acknowledgment state
		SetDialogueState(EDialogueState::TradeCompleted);
		return;
	}

	AAITeamController* AIController = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentCity->OwnerTeam)
		{
			AIController = *It;
			break;
		}
	}

	if (!AIController) return;

	// Execute the trade based on pending trade type (player-initiated trades)
	switch (PendingTradeType)
	{
		case EPendingTradeType::SellBlack:
			// Player selling black for orange
			PC->PlayerBlackSubstrate -= PendingTradeAmount;
			PC->PlayerOrangeSubstrate += PendingAIResponse;
			AIController->BlackSubstrate += PendingTradeAmount;
			AIController->OrangeSubstrate -= PendingAIResponse;
			CurrentDialogueTextString = FString::Printf(TEXT("Trade completed. You gave %d black substrate and received %d orange substrate."), 
				PendingTradeAmount, PendingAIResponse);
			UE_LOG(LogTemp, Log, TEXT("Trade executed: Player gave %d black, received %d orange"), PendingTradeAmount, PendingAIResponse);
			break;

		case EPendingTradeType::BuyBlack:
			// Player buying black with orange
			PC->PlayerOrangeSubstrate -= PendingPlayerOffer;
			PC->PlayerBlackSubstrate += PendingAIResponse;
			AIController->OrangeSubstrate += PendingPlayerOffer;
			AIController->BlackSubstrate -= PendingAIResponse;
			GameMode->ModifyDisposition(CurrentCity->OwnerTeam, EOwnerTeam::Player, 5.0f);
			CurrentDialogueTextString = FString::Printf(TEXT("Trade completed. You gave %d orange substrate and received %d black substrate."), 
				PendingPlayerOffer, PendingAIResponse);
			UE_LOG(LogTemp, Log, TEXT("Trade executed: Player gave %d orange, received %d black"), PendingPlayerOffer, PendingAIResponse);
			break;

		case EPendingTradeType::SellOrange:
			// Player selling orange for black
			PC->PlayerOrangeSubstrate -= PendingTradeAmount;
			PC->PlayerBlackSubstrate += PendingAIResponse;
			AIController->OrangeSubstrate += PendingTradeAmount;
			AIController->BlackSubstrate -= PendingAIResponse;
			GameMode->ModifyDisposition(CurrentCity->OwnerTeam, EOwnerTeam::Player, 5.0f);
			CurrentDialogueTextString = FString::Printf(TEXT("Trade completed. You gave %d orange substrate and received %d black substrate."), 
				PendingTradeAmount, PendingAIResponse);
			UE_LOG(LogTemp, Log, TEXT("Trade executed: Player gave %d orange, received %d black"), PendingTradeAmount, PendingAIResponse);
			break;

		case EPendingTradeType::BuyOrange:
			// Player buying orange with black
			PC->PlayerBlackSubstrate -= PendingPlayerOffer;
			PC->PlayerOrangeSubstrate += PendingAIResponse;
			AIController->BlackSubstrate += PendingPlayerOffer;
			AIController->OrangeSubstrate -= PendingAIResponse;
			GameMode->ModifyDisposition(CurrentCity->OwnerTeam, EOwnerTeam::Player, 5.0f);
			CurrentDialogueTextString = FString::Printf(TEXT("Trade completed. You gave %d black substrate and received %d orange substrate."), 
				PendingPlayerOffer, PendingAIResponse);
			UE_LOG(LogTemp, Log, TEXT("Trade executed: Player gave %d black, received %d orange"), PendingPlayerOffer, PendingAIResponse);
			break;

		default:
			CurrentDialogueTextString = TEXT("Error: No pending trade.");
			break;
	}

	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));

	// Update relationship display
	if (RelationshipText)
	{
		RelationshipText->SetText(GetRelationshipText());
		RelationshipText->SetColorAndOpacity(FSlateColor(GetRelationshipColor()));
	}

	// Clear pending trade
	PendingTradeType = EPendingTradeType::None;
	PendingPlayerOffer = 0;
	PendingAIResponse = 0;
	PendingTradeAmount = 0;

	// Return to trade menu
	SetDialogueState(EDialogueState::TradeMenu);
}

void UTalkDialogueWidget::OnNoDealClicked()
{
	// Ignore double-clicks
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnNoDealClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	// Check if this is an AI trade request (which might create a counter-offer)
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && GameMode->bHasActiveTradeRequest)
	{
		if (GameMode)
		{
			// Decline the current trade request (might create counter-offer)
			GameMode->DeclineCurrentTradeRequest();
			
			// Check if it became a counter-offer
			FAITradeRequest TradeRequest = GameMode->GetCurrentTradeRequest();
			if (TradeRequest.bIsCounterOffer && GameMode->bHasActiveTradeRequest) // Still has request = counter-offer created
			{
				// Refresh the UI to show the counter-offer
				SetDialogueState(EDialogueState::TradeMenu);
				return;
			}
			else
			{
				// No counter-offer, close the dialogue
				CurrentDialogueTextString = TEXT("Perhaps we can find a better arrangement another time.");
				if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
				CloseDialogue();
				return;
			}
		}
	}
	
	// Clear pending trade
	PendingTradeType = EPendingTradeType::None;
	PendingPlayerOffer = 0;
	PendingAIResponse = 0;
	PendingTradeAmount = 0;

	// Return to trade menu (player-initiated trade declined)
	CurrentDialogueTextString = TEXT("Perhaps we can find a better arrangement another time.");
	if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	
	SetDialogueState(EDialogueState::TradeMenu);
}

void UTalkDialogueWidget::OnOfCourseClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnOfCourseClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	UE_LOG(LogTemp, Warning, TEXT("========== OnOfCourseClicked CALLED =========="));
	
	// Player acknowledges trade completion - simply close the dialogue
	CloseDialogue();
	
	UE_LOG(LogTemp, Warning, TEXT("OnOfCourseClicked: CloseDialogue returned"));
}

void UTalkDialogueWidget::OnAcceptAllianceClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnAcceptAllianceClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && GameMode->bHasActiveAllianceRequest)
	{
		GameMode->AcceptCurrentAllianceRequest();
		CurrentDialogueTextString = TEXT("An alliance is formed. Together, we are stronger.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
	
	// Close the dialogue
	CloseDialogue();
}

void UTalkDialogueWidget::OnDeclineAllianceClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnDeclineAllianceClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && GameMode->bHasActiveAllianceRequest)
	{
		GameMode->DeclineCurrentAllianceRequest();
		CurrentDialogueTextString = TEXT("We must decline your offer at this time.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
	
	// Return to request menu
	SetDialogueState(EDialogueState::RequestMenu);
}

void UTalkDialogueWidget::OnAcceptBribeThankfullyClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnAcceptBribeThankfullyClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && GameMode->bHasActiveBribeRequest)
	{
		GameMode->AcceptCurrentBribeRequestThankfully();
		CurrentDialogueTextString = TEXT("Thank you for your generosity. This strengthens our bond.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
	
	// Close the dialogue
	CloseDialogue();
}

void UTalkDialogueWidget::OnAcceptBribeSuspiciouslyClicked()
{
	// Prevent double-click issues
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastButtonClickTime < ButtonClickCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnAcceptBribeSuspiciouslyClicked: Ignoring double-click"));
		return;
	}
	LastButtonClickTime = CurrentTime;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode && GameMode->bHasActiveBribeRequest)
	{
		GameMode->AcceptCurrentBribeRequestSuspiciously();
		CurrentDialogueTextString = TEXT("We'll take your gift... but we know there are strings attached.");
		if (DialogueText) DialogueText->SetText(FText::FromString(CurrentDialogueTextString));
	}
	
	// Close the dialogue
	CloseDialogue();
}
