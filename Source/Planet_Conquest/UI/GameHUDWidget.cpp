// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameHUDWidget.h"
#include "../Core/PlanetConquestPlayerController.h"
#include "../Core/PlanetConquestGameMode.h"
#include "../Core/AITeamController.h"
#include "../Entities/Resources/ResourceActor.h"
#include "../Entities/Cities/CityActor.h"
#include "../Entities/Vehicles/VehicleActor.h"
#include "../World/PlanetActor.h"
#include "Kismet/GameplayStatics.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Warning, TEXT("GameHUDWidget constructed"));
	
	// Set up minimap (Canvas Panel with terrain image)
	if (MinimapCanvas && MinimapImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("✓ MinimapCanvas and MinimapImage widgets bound"));
		
		// Reset color tint to white (no tint)
		MinimapImage->SetColorAndOpacity(FLinearColor::White);
		
		// Find the planet actor and get its minimap texture
		PlanetActorRef = Cast<APlanetActor>(UGameplayStatics::GetActorOfClass(GetWorld(), APlanetActor::StaticClass()));
		
		if (PlanetActorRef && PlanetActorRef->MinimapTexture)
		{
			UE_LOG(LogTemp, Warning, TEXT("Minimap texture found: %dx%d"), 
				PlanetActorRef->MinimapTexture->GetSizeX(), PlanetActorRef->MinimapTexture->GetSizeY());
			
			MinimapImage->SetBrushFromTexture(PlanetActorRef->MinimapTexture);
			MinimapImage->SetDesiredSizeOverride(FVector2D(512.0f, 256.0f));
			UE_LOG(LogTemp, Warning, TEXT("✓ Minimap texture assigned successfully!"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("✗ Minimap texture not available!"));
		}
		
		// Load city icon texture
		CityIconTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/InsigniaShapes/mix-square-diamond"));
		if (CityIconTexture)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ City icon texture loaded"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("✗ Failed to load city icon texture"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("✗ MinimapCanvas or MinimapImage is NULL - widgets not bound!"));
	}
	
	// Debug: Check initial vehicle efficiency mode
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC)
	{
		int32 RawModeValue = static_cast<int32>(PC->BlackSubstrateMode);
		FString ModeName = TEXT("UNKNOWN");
		switch (PC->BlackSubstrateMode)
		{
			case EBlackSubstrateMode::Auxiliary:
				ModeName = TEXT("Auxiliary");
				break;
			case EBlackSubstrateMode::Efficient:
				ModeName = TEXT("Efficient");
				break;
			case EBlackSubstrateMode::Overdrive:
				ModeName = TEXT("Overdrive");
				break;
			default:
				ModeName = FString::Printf(TEXT("INVALID (raw: %d)"), RawModeValue);
				break;
		}
		UE_LOG(LogTemp, Error, TEXT("=== GameHUDWidget NativeConstruct: PC->BlackSubstrateMode = %s (raw: %d), PlayerBlackSubstrate = %d ==="), 
			*ModeName, RawModeValue, PC->PlayerBlackSubstrate);
	}
}

void UGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// Auto-refresh display every frame
	RefreshFromPlayerController();
	UpdateAlliancesDisplay();
	
	// Update minimap city markers
	if (MinimapCanvas && PlanetActorRef)
	{
		FVector2D MinimapSize = MinimapCanvas->GetCachedGeometry().GetLocalSize();
		if (MinimapSize.X > 0 && MinimapSize.Y > 0)
		{
			UpdateCityMarkers(MinimapSize);
		}
	}
	
	// Throttle AI debug updates to avoid performance issues
	// Only update when game is not paused
	// Update every 3 seconds to match AI decision-making cycle
	if (!GetWorld()->IsPaused())
	{
		AIDebugUpdateTimer += InDeltaTime;
		if (AIDebugUpdateTimer >= 3.0f)
		{
			UpdateAIDebugDisplay();
			AIDebugUpdateTimer = 0.0f;
		}
	}
}

void UGameHUDWidget::UpdateDisplay(int32 OrangeSubstrate, int32 BlackSubstrate)
{
	DisplayedOrangeSubstrate = OrangeSubstrate;
	DisplayedBlackSubstrate = BlackSubstrate;
}

FText UGameHUDWidget::GetOrangeSubstrateText() const
{
	FString IncomeSign = DisplayedOrangeIncome >= 0 ? TEXT("+") : TEXT("");
	return FText::FromString(FString::Printf(TEXT("Orange Substrate: %d  (%s%d/cycle)"), 
		DisplayedOrangeSubstrate, *IncomeSign, DisplayedOrangeIncome));
}

FText UGameHUDWidget::GetBlackSubstrateText() const
{
	FString IncomeSign = DisplayedBlackIncome >= 0 ? TEXT("+") : TEXT("");
	return FText::FromString(FString::Printf(TEXT("Black Substrate: %d  (%s%d/cycle)"), 
		DisplayedBlackSubstrate, *IncomeSign, DisplayedBlackIncome));
}

void UGameHUDWidget::RefreshFromPlayerController()
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return;
	}
	
	// Get substrate counts from player controller
	DisplayedOrangeSubstrate = PC->PlayerOrangeSubstrate;
	DisplayedBlackSubstrate = PC->PlayerBlackSubstrate;
	
	// Get income per cycle
	DisplayedOrangeIncome = PC->PlayerOrangeIncomePerCycle;
	DisplayedBlackIncome = PC->PlayerBlackIncomePerCycle;
	
	// Get autopilot mode
	bAutopilotMode = PC->bVehicleAutopilotMode;
	
	// Get mining mode
	if (PC->MiningMode == EMiningMode::Aggressive)
	{
		MiningModeText = FText::FromString("Aggressive");
	}
	else
	{
		MiningModeText = FText::FromString("Sustainable");
	}
	
	// Get vehicle efficiency mode
	if (PC->PlayerBlackSubstrate <= 0)
	{
		// Out of Black Substrate - forced to Auxiliary
		VehicleEfficiencyText = FText::FromString("Auxiliary");
		bCanToggleVehicleEfficiency = false;
	}
	else
	{
		// Has Black Substrate - can toggle
		bCanToggleVehicleEfficiency = true;
		if (PC->BlackSubstrateMode == EBlackSubstrateMode::Efficient)
		{
			VehicleEfficiencyText = FText::FromString("Efficient");
		}
		else if (PC->BlackSubstrateMode == EBlackSubstrateMode::Overdrive)
		{
			VehicleEfficiencyText = FText::FromString("Overdrive");
		}
		else
		{
			VehicleEfficiencyText = FText::FromString("Auxiliary");
		}
	}
	
	// Debug: Log first 5 frames to track initial mode
	static int32 DebugCallCount = 0;
	if (DebugCallCount < 5)
	{
		int32 RawModeValue = static_cast<int32>(PC->BlackSubstrateMode);
		FString ModeName = TEXT("UNKNOWN");
		switch (PC->BlackSubstrateMode)
		{
			case EBlackSubstrateMode::Auxiliary:
				ModeName = TEXT("Auxiliary");
				break;
			case EBlackSubstrateMode::Efficient:
				ModeName = TEXT("Efficient");
				break;
			case EBlackSubstrateMode::Overdrive:
				ModeName = TEXT("Overdrive");
				break;
			default:
				ModeName = FString::Printf(TEXT("INVALID (raw: %d)"), RawModeValue);
				break;
		}
		UE_LOG(LogTemp, Error, TEXT("=== RefreshFromPlayerController #%d: Mode=%s (raw: %d), BS=%d, DisplayText=%s ==="), 
			DebugCallCount, *ModeName, RawModeValue, PC->PlayerBlackSubstrate, *VehicleEfficiencyText.ToString());
		DebugCallCount++;
	}
}

void UGameHUDWidget::ToggleMiningMode()
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ToggleMiningMode: PlayerController is NULL!"));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("ToggleMiningMode button clicked!"));
	
	// Toggle between modes
	if (PC->MiningMode == EMiningMode::Aggressive)
	{
		PC->SetMiningMode(EMiningMode::Sustainable);
	}
	else
	{
		PC->SetMiningMode(EMiningMode::Aggressive);
	}
}

void UGameHUDWidget::ToggleVehicleEfficiency()
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ToggleVehicleEfficiency: PlayerController is NULL!"));
		return;
	}
	
	// Don't allow toggle if out of Black Substrate
	if (PC->PlayerBlackSubstrate <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot toggle vehicle efficiency - out of Black Substrate!"));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("ToggleVehicleEfficiency button clicked!"));
	PC->ToggleVehicleEfficiency();
}

void UGameHUDWidget::UpdateAlliancesDisplay()
{
	if (!AlliancesContainer)
	{
		return;
	}
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode)
	{
		return;
	}
	
	// Helper lambda to get team color
	auto GetTeamColor = [](EOwnerTeam Team) -> FLinearColor
	{
		switch (Team)
		{
			case EOwnerTeam::Player:    return FLinearColor(0.0f, 1.0f, 1.0f); // Cyan
			case EOwnerTeam::AI1:       return FLinearColor::Red;
			case EOwnerTeam::AI2:       return FLinearColor::Green;
			case EOwnerTeam::AI3:       return FLinearColor::Yellow;
			case EOwnerTeam::AI4:       return FLinearColor(0.5f, 0.0f, 0.5f); // Purple
			case EOwnerTeam::AI5:       return FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
			case EOwnerTeam::AI6:       return FLinearColor(0.1f, 0.1f, 0.1f); // Dark gray (black is too dark)
			case EOwnerTeam::AI7:       return FLinearColor(0.0f, 0.4f, 1.0f); // Royal blue
			case EOwnerTeam::AI8:       return FLinearColor::White;
			default:                    return FLinearColor::Gray;
		}
	};
	
	// Clear existing content
	AlliancesContainer->ClearChildren();
	
	// Gather alliance information
	TMap<FString, TArray<EOwnerTeam>> Alliances; // Alliance name -> teams
	TArray<EOwnerTeam> UnalignedTeams;
	
	// Add player
	FString PlayerAlliance = GameMode->GetPlayerAllianceName();
	if (PlayerAlliance.IsEmpty() || PlayerAlliance == TEXT("No Alliance") || PlayerAlliance == TEXT("No Allegiance"))
	{
		UnalignedTeams.Add(EOwnerTeam::Player);
	}
	else
	{
		Alliances.FindOrAdd(PlayerAlliance).Add(EOwnerTeam::Player);
	}
	
	// Add AI teams
	for (TActorIterator<AAITeamController> It(GetWorld()); It; ++It)
	{
		AAITeamController* AI = *It;
		if (AI && AI->ControlledTeam != EOwnerTeam::Neutral)
		{
			if (AI->AllianceState.AllianceName.IsEmpty())
			{
				UnalignedTeams.Add(AI->ControlledTeam);
			}
			else
			{
				Alliances.FindOrAdd(AI->AllianceState.AllianceName).Add(AI->ControlledTeam);
			}
		}
	}
	
	// Display each alliance
	for (const auto& AlliancePair : Alliances)
	{
		const FString& AllianceName = AlliancePair.Key;
		const TArray<EOwnerTeam>& Teams = AlliancePair.Value;
		
		// Skip "No Alliance" / "No Allegiance" - those teams should be in unaligned list
		if (AllianceName == TEXT("No Alliance") || AllianceName == TEXT("No Allegiance"))
		{
			continue;
		}
		
		// Calculate average relationship for this alliance (excluding player if in alliance)
		float TotalRelationship = 0.0f;
		int32 RelationshipCount = 0;
		for (EOwnerTeam Team : Teams)
		{
			if (Team != EOwnerTeam::Player)
			{
				float Relationship = GameMode->GetDisposition(EOwnerTeam::Player, Team);
				TotalRelationship += Relationship;
				RelationshipCount++;
			}
		}
		float AverageRelationship = (RelationshipCount > 0) ? (TotalRelationship / RelationshipCount) : 0.0f;
		
		// Alliance header with average relationship
		UTextBlock* Header = NewObject<UTextBlock>(this);
		if (RelationshipCount > 0)
		{
			Header->SetText(FText::FromString(FString::Printf(TEXT("Alliance: %s (Avg Rel: %.0f)"), *AllianceName, AverageRelationship)));
		}
		else
		{
			Header->SetText(FText::FromString(FString::Printf(TEXT("Alliance: %s"), *AllianceName)));
		}
		Header->SetColorAndOpacity(FLinearColor::White);
		FSlateFontInfo HeaderFont = Header->GetFont();
		HeaderFont.Size = 28;
		Header->SetFont(HeaderFont);
		UVerticalBoxSlot* HeaderSlot = AlliancesContainer->AddChildToVerticalBox(Header);
		HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		HeaderSlot->SetPadding(FMargin(0, 10, 0, 25));
		HeaderSlot->SetHorizontalAlignment(HAlign_Left);
		
		// Team entries
		for (EOwnerTeam Team : Teams)
		{
			// Count cities for this team
			int32 CityCount = 0;
			for (TActorIterator<ACityActor> CityIt(GetWorld()); CityIt; ++CityIt)
			{
				if (CityIt->OwnerTeam == Team)
				{
					CityCount++;
				}
			}
			
			// Skip teams with no cities
			if (CityCount == 0) continue;
			
			// Get relationship score with this team
			float Relationship = (Team == EOwnerTeam::Player) ? 0.0f : GameMode->GetDisposition(EOwnerTeam::Player, Team);
			
			// Create team entry with relationship score
			FString TeamName = (Team == EOwnerTeam::Player) ? TEXT("Player") : FString::Printf(TEXT("Team %d (AI)"), (int32)Team);
			UTextBlock* TeamEntry = NewObject<UTextBlock>(this);
			if (Team == EOwnerTeam::Player)
			{
				TeamEntry->SetText(FText::FromString(FString::Printf(TEXT("  %s - %d cities"), *TeamName, CityCount)));
			}
			else
			{
				TeamEntry->SetText(FText::FromString(FString::Printf(TEXT("  %s - %d cities (Rel: %.0f)"), *TeamName, CityCount, Relationship)));
			}
			TeamEntry->SetColorAndOpacity(GetTeamColor(Team));
			FSlateFontInfo TeamFont = TeamEntry->GetFont();
			TeamFont.Size = 22;
			TeamEntry->SetFont(TeamFont);
			UVerticalBoxSlot* TeamSlot = AlliancesContainer->AddChildToVerticalBox(TeamEntry);
			TeamSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			TeamSlot->SetPadding(FMargin(10, 0, 0, 25));
			TeamSlot->SetHorizontalAlignment(HAlign_Left);
		}
	}
	
	// Display unaligned teams
	if (UnalignedTeams.Num() > 0)
	{
		UTextBlock* UnalignedHeader = NewObject<UTextBlock>(this);
		UnalignedHeader->SetText(FText::FromString(TEXT("Unaligned:")));
		UnalignedHeader->SetColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f));
		FSlateFontInfo UnalignedHeaderFont = UnalignedHeader->GetFont();
		UnalignedHeaderFont.Size = 22;
		UnalignedHeader->SetFont(UnalignedHeaderFont);
		UVerticalBoxSlot* UnalignedHeaderSlot = AlliancesContainer->AddChildToVerticalBox(UnalignedHeader);
		UnalignedHeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		UnalignedHeaderSlot->SetPadding(FMargin(0, 10, 0, 25));
		UnalignedHeaderSlot->SetHorizontalAlignment(HAlign_Left);
		
		for (EOwnerTeam Team : UnalignedTeams)
		{
			// Count cities
			int32 CityCount = 0;
			for (TActorIterator<ACityActor> CityIt(GetWorld()); CityIt; ++CityIt)
			{
				if (CityIt->OwnerTeam == Team)
				{
					CityCount++;
				}
			}
			
			// Skip teams with no cities
			if (CityCount == 0) continue;
			
			// Get relationship score with this team
			float Relationship = (Team == EOwnerTeam::Player) ? 0.0f : GameMode->GetDisposition(EOwnerTeam::Player, Team);
			
			// Create team entry with relationship score
			FString TeamName = (Team == EOwnerTeam::Player) ? TEXT("Player") : FString::Printf(TEXT("Team %d (AI)"), (int32)Team);
			UTextBlock* TeamEntry = NewObject<UTextBlock>(this);
			if (Team == EOwnerTeam::Player)
			{
				TeamEntry->SetText(FText::FromString(FString::Printf(TEXT("  %s - %d cities"), *TeamName, CityCount)));
			}
			else
			{
				TeamEntry->SetText(FText::FromString(FString::Printf(TEXT("  %s - %d cities (%.0f)"), *TeamName, CityCount, Relationship)));
			}
			TeamEntry->SetColorAndOpacity(GetTeamColor(Team));
			FSlateFontInfo TeamFont = TeamEntry->GetFont();
			TeamFont.Size = 18;
			TeamEntry->SetFont(TeamFont);
			UVerticalBoxSlot* TeamSlot = AlliancesContainer->AddChildToVerticalBox(TeamEntry);
			TeamSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			TeamSlot->SetPadding(FMargin(10, 0, 0, 25));
			TeamSlot->SetHorizontalAlignment(HAlign_Left);
		}
	}
}

void UGameHUDWidget::UpdateAIDebugDisplay()
{
	if (!AIDebugContainer)
	{
		return;
	}
	
	// Find the AI team controller we want to display
	AAITeamController* TargetAI = nullptr;
	EOwnerTeam TargetTeam = static_cast<EOwnerTeam>(DebugAITeamIndex);
	
	for (TActorIterator<AAITeamController> It(GetWorld()); It; ++It)
	{
		AAITeamController* AI = *It;
		if (AI && AI->ControlledTeam == TargetTeam)
		{
			TargetAI = AI;
			break;
		}
	}
	
	if (!TargetAI)
	{
		// Clear cache and display "no AI" message only if not already showing it
		if (CachedDebugTeamIndex != -2)
		{
			AIDebugContainer->ClearChildren();
			UTextBlock* NoDataText = NewObject<UTextBlock>(this);
			NoDataText->SetText(FText::FromString(TEXT("No AI found for this team")));
			NoDataText->SetColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f));
			FSlateFontInfo NoDataFont = NoDataText->GetFont();
			NoDataFont.Size = 18;
			NoDataText->SetFont(NoDataFont);
			UVerticalBoxSlot* NoDataSlot = AIDebugContainer->AddChildToVerticalBox(NoDataText);
			NoDataSlot->SetPadding(FMargin(0, 5, 0, 5));
			NoDataSlot->SetHorizontalAlignment(HAlign_Left);
			NoDataSlot->SetVerticalAlignment(VAlign_Top);
			CachedDebugTeamIndex = -2; // Special value indicating "no AI" state
		}
		return;
	}
	
	// Get current values
	int32 CurrentOrangeSubstrate = TargetAI->OrangeSubstrate;
	int32 CurrentBlackSubstrate = TargetAI->BlackSubstrate;
	TArray<AVehicleActor*> AllVehicles = TargetAI->GetControlledVehicles();
	int32 CurrentTotalVehicles = AllVehicles.Num();
	
	// Count vehicles per priority to build current state
	struct FPriorityInfo
	{
		FString Name;
		int32 VehicleCount;
		TMap<FString, int32> SubPriorities;
	};
	
	TArray<FPriorityInfo> Priorities;
	Priorities.SetNum(5);
	Priorities[0].Name = TEXT("P1: Survival");
	Priorities[0].VehicleCount = 0;
	Priorities[1].Name = TEXT("P2: Income");
	Priorities[1].VehicleCount = 0;
	Priorities[2].Name = TEXT("P3: Defense");
	Priorities[2].VehicleCount = 0;
	Priorities[3].Name = TEXT("P4: War");
	Priorities[3].VehicleCount = 0;
	Priorities[4].Name = TEXT("P5: Aid");
	Priorities[4].VehicleCount = 0;
	
	// Count vehicles per priority
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (!Vehicle) continue;
		
		EVehicleTaskType TaskType = Vehicle->CurrentTask.Type;
		int32 TaskPriority = Vehicle->CurrentTask.Priority;
		
		// P1: Survival - Show actual task type
		if (Vehicle->bInP1 || TaskType == EVehicleTaskType::DefendTerritory)
		{
			Priorities[0].VehicleCount++;
			FString SubPriority = FString::Printf(TEXT("Task:%d bInP1:%d"), (int32)TaskType, Vehicle->bInP1 ? 1 : 0);
			Priorities[0].SubPriorities.FindOrAdd(SubPriority)++;
		}
		// P2: Income - Show sub-priority label and search radius
		else if (TaskType == EVehicleTaskType::SecureIncome || Vehicle->bInP2)
		{
			Priorities[1].VehicleCount++;
			
			FString SubPriority;
			if (!Vehicle->CurrentTask.SubPriorityLabel.IsEmpty())
			{
				// Use sub-priority label if available (e.g., "P2.1", "P2.3")
				SubPriority = FString::Printf(TEXT("%s (%.0f)"), *Vehicle->CurrentTask.SubPriorityLabel, Vehicle->CurrentTask.SearchRadius);
			}
			else if (Vehicle->CurrentTask.PrimaryTarget.IsValid())
			{
				// Fallback: Intrusion (has PrimaryTarget)
				SubPriority = FString::Printf(TEXT("Intrusion (%.0f)"), Vehicle->CurrentTask.SearchRadius);
			}
			else
			{
				// Fallback: Generic radius display
				SubPriority = FString::Printf(TEXT("R:%.0f"), Vehicle->CurrentTask.SearchRadius);
			}
			Priorities[1].SubPriorities.FindOrAdd(SubPriority)++;
		}
		// P3: Defense (turret building - no vehicles directly assigned)
		// P4: War - Show actual task data
		else if (TaskType == EVehicleTaskType::AttackTarget && TaskPriority == 4)
		{
			Priorities[3].VehicleCount++;
			FString TargetName = Vehicle->CurrentTask.PrimaryTarget.IsValid() ? 
				Vehicle->CurrentTask.PrimaryTarget.Get()->GetName() : TEXT("None");
			FString SubPriority = FString::Printf(TEXT("Attack (Target:%s)"), *TargetName);
			Priorities[3].SubPriorities.FindOrAdd(SubPriority)++;
		}
		// P5: Aid - Show actual task data
		else if (TaskType == EVehicleTaskType::AidAlly || TaskPriority == 5)
		{
			Priorities[4].VehicleCount++;
			FString SubPriority = FString::Printf(TEXT("Task:%d Pri:%d"), (int32)TaskType, TaskPriority);
			Priorities[4].SubPriorities.FindOrAdd(SubPriority)++;
		}
	}
	
	// Count idle vehicles
	int32 IdleVehicleCount = 0;
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (Vehicle && Vehicle->IsIdle())
		{
			IdleVehicleCount++;
		}
	}
	
	// Build current priority counts map for comparison
	TMap<FString, int32> CurrentPriorityCounts;
	for (const FPriorityInfo& PriorityInfo : Priorities)
	{
		CurrentPriorityCounts.Add(PriorityInfo.Name, PriorityInfo.VehicleCount);
	}
	CurrentPriorityCounts.Add(TEXT("Idle"), IdleVehicleCount);
	
	// Check if anything changed
	bool bNeedsUpdate = (CachedDebugTeamIndex != DebugAITeamIndex) ||
						(CachedOrangeSubstrate != CurrentOrangeSubstrate) ||
						(CachedBlackSubstrate != CurrentBlackSubstrate) ||
						(CachedOrangeIncome != TargetAI->OrangeIncomePerCycle) ||
						(CachedBlackIncome != TargetAI->BlackIncomePerCycle) ||
						(CachedRequiredOrangeIncome != TargetAI->GetRequiredOrangeIncome()) ||
						(CachedRequiredBlackIncome != TargetAI->GetRequiredBlackIncome()) ||
						(CachedTotalVehicles != CurrentTotalVehicles) ||
						!CachedPriorityCounts.OrderIndependentCompareEqual(CurrentPriorityCounts);
	
	// Only rebuild UI if something changed
	if (!bNeedsUpdate)
	{
		return;
	}
	
	// Update cache
	CachedDebugTeamIndex = DebugAITeamIndex;
	CachedOrangeSubstrate = CurrentOrangeSubstrate;
	CachedBlackSubstrate = CurrentBlackSubstrate;
	CachedOrangeIncome = TargetAI->OrangeIncomePerCycle;
	CachedBlackIncome = TargetAI->BlackIncomePerCycle;
	CachedRequiredOrangeIncome = TargetAI->GetRequiredOrangeIncome();
	CachedRequiredBlackIncome = TargetAI->GetRequiredBlackIncome();
	CachedTotalVehicles = CurrentTotalVehicles;
	CachedPriorityCounts = CurrentPriorityCounts;
	
	// Clear and rebuild UI
	AIDebugContainer->ClearChildren();
	
	// Helper lambda to get team color
	auto GetTeamColor = [](EOwnerTeam Team) -> FLinearColor
	{
		switch (Team)
		{
			case EOwnerTeam::AI1:       return FLinearColor::Red;
			case EOwnerTeam::AI2:       return FLinearColor::Green;
			case EOwnerTeam::AI3:       return FLinearColor::Yellow;
			case EOwnerTeam::AI4:       return FLinearColor(0.5f, 0.0f, 0.5f); // Purple
			case EOwnerTeam::AI5:       return FLinearColor(1.0f, 0.3f, 0.0f); // Reddish-orange
			case EOwnerTeam::AI6:       return FLinearColor(0.8f, 0.8f, 0.8f); // Light gray
			default:                    return FLinearColor::Gray;
		}
	};
	
	// Header: Team name
	UTextBlock* TeamHeader = NewObject<UTextBlock>(this);
	TeamHeader->SetText(FText::FromString(FString::Printf(TEXT("AI Team %d"), (int32)TargetTeam)));
	TeamHeader->SetColorAndOpacity(GetTeamColor(TargetTeam));
	FSlateFontInfo TeamHeaderFont = TeamHeader->GetFont();
	TeamHeaderFont.Size = 32;
	TeamHeaderFont.TypefaceFontName = FName(TEXT("Bold"));
	TeamHeader->SetFont(TeamHeaderFont);
	UVerticalBoxSlot* TeamHeaderSlot = AIDebugContainer->AddChildToVerticalBox(TeamHeader);
	TeamHeaderSlot->SetPadding(FMargin(0, 5, 0, 10));
	TeamHeaderSlot->SetHorizontalAlignment(HAlign_Left);
	TeamHeaderSlot->SetVerticalAlignment(VAlign_Top);
	
	// Resource stockpiles
	UTextBlock* StockpileText = NewObject<UTextBlock>(this);
	StockpileText->SetText(FText::FromString(FString::Printf(TEXT("Stockpile: OS:%d  BS:%d"), 
		CurrentOrangeSubstrate, CurrentBlackSubstrate)));
	StockpileText->SetColorAndOpacity(FLinearColor::White);
	FSlateFontInfo StockpileFont = StockpileText->GetFont();
	StockpileFont.Size = 20;
	StockpileText->SetFont(StockpileFont);
	UVerticalBoxSlot* StockpileSlot = AIDebugContainer->AddChildToVerticalBox(StockpileText);
	StockpileSlot->SetPadding(FMargin(0, 2, 0, 2));
	StockpileSlot->SetHorizontalAlignment(HAlign_Left);
	StockpileSlot->SetVerticalAlignment(VAlign_Top);
	
	// Income per cycle
	UTextBlock* IncomeText = NewObject<UTextBlock>(this);
	IncomeText->SetText(FText::FromString(FString::Printf(TEXT("Income/cycle: OS:%d/%d  BS:%d/%d"), 
		TargetAI->OrangeIncomePerCycle, TargetAI->GetRequiredOrangeIncome(),
		TargetAI->BlackIncomePerCycle, TargetAI->GetRequiredBlackIncome())));
	IncomeText->SetColorAndOpacity(FLinearColor::White);
	FSlateFontInfo IncomeFont = IncomeText->GetFont();
	IncomeFont.Size = 20;
	IncomeText->SetFont(IncomeFont);
	UVerticalBoxSlot* IncomeSlot = AIDebugContainer->AddChildToVerticalBox(IncomeText);
	IncomeSlot->SetPadding(FMargin(0, 2, 0, 2));
	IncomeSlot->SetHorizontalAlignment(HAlign_Left);
	IncomeSlot->SetVerticalAlignment(VAlign_Top);
	
	// Resource statistics (territory zones)
	TArray<ACityActor*> AICities = TargetAI->GetControlledCities();
	if (AICities.Num() > 0)
	{
		const float EASY_INCOME_RADIUS = 20000.0f;
		
		TArray<AActor*> AllResourceActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), AllResourceActors);
		
		int32 TotalInZone = 0;
		int32 UnclaimedInZone = 0;
		
		for (AActor* Actor : AllResourceActors)
		{
			AResourceActor* Resource = Cast<AResourceActor>(Actor);
			if (!Resource) continue;
			
			// Find closest city distance
			float ClosestDist = FLT_MAX;
			for (ACityActor* City : AICities)
			{
				float Dist = FVector::Dist(City->GetActorLocation(), Resource->GetActorLocation());
				if (Dist < ClosestDist) ClosestDist = Dist;
			}
			
			// Count resources in income zone
			if (ClosestDist <= EASY_INCOME_RADIUS)
			{
				TotalInZone++;
				if (Resource->OwnerTeam == EOwnerTeam::Neutral) UnclaimedInZone++;
			}
		}
		
		// Display zone statistics
		UTextBlock* ZoneStatsText = NewObject<UTextBlock>(this);
		ZoneStatsText->SetText(FText::FromString(FString::Printf(
			TEXT("Zone 0-20k: %d/%d unclaimed/total"),
			UnclaimedInZone, TotalInZone)));
		ZoneStatsText->SetColorAndOpacity(FLinearColor(0.7f, 0.9f, 1.0f));  // Light blue
		FSlateFontInfo ZoneStatsFont = ZoneStatsText->GetFont();
		ZoneStatsFont.Size = 18;
		ZoneStatsText->SetFont(ZoneStatsFont);
		UVerticalBoxSlot* ZoneStatsSlot = AIDebugContainer->AddChildToVerticalBox(ZoneStatsText);
		ZoneStatsSlot->SetPadding(FMargin(0, 2, 0, 2));
		ZoneStatsSlot->SetHorizontalAlignment(HAlign_Left);
		ZoneStatsSlot->SetVerticalAlignment(VAlign_Top);
	}
	
	// Total vehicle count
	UTextBlock* VehicleCountText = NewObject<UTextBlock>(this);
	VehicleCountText->SetText(FText::FromString(FString::Printf(TEXT("Total Vehicles: %d"), CurrentTotalVehicles)));
	VehicleCountText->SetColorAndOpacity(FLinearColor::White);
	FSlateFontInfo VehicleCountFont = VehicleCountText->GetFont();
	VehicleCountFont.Size = 20;
	VehicleCountText->SetFont(VehicleCountFont);
	UVerticalBoxSlot* VehicleCountSlot = AIDebugContainer->AddChildToVerticalBox(VehicleCountText);
	VehicleCountSlot->SetPadding(FMargin(0, 2, 0, 10));
	VehicleCountSlot->SetHorizontalAlignment(HAlign_Left);
	VehicleCountSlot->SetVerticalAlignment(VAlign_Top);
	
	// Display each priority
	for (int32 i = 0; i < Priorities.Num(); i++)
	{
		const FPriorityInfo& PriorityInfo = Priorities[i];
		
		// Determine if priority is active (has vehicles OR is P3 with turrets needed)
		bool bIsActive = (PriorityInfo.VehicleCount > 0);
		
		// For P3 (Defense), check if turrets are being built/needed
		if (i == 2 && TargetAI)
		{
			// Priority 3 is active if reserved substrate > 0 (turrets being built)
			bIsActive = (TargetAI->ReservedOrangeSubstrate > 0);
		}
		
		// Priority header (show even if 0 vehicles)
		UTextBlock* PriorityHeader = NewObject<UTextBlock>(this);
		PriorityHeader->SetText(FText::FromString(FString::Printf(TEXT("%s (%d veh)"),
			*PriorityInfo.Name, PriorityInfo.VehicleCount)));
		
		// Active priorities shown in orange, inactive in yellow-ish
		FLinearColor HeaderColor = bIsActive ? FLinearColor(1.0f, 0.5f, 0.0f) : FLinearColor(1.0f, 0.8f, 0.3f);
		PriorityHeader->SetColorAndOpacity(HeaderColor);
		
		FSlateFontInfo PriorityHeaderFont = PriorityHeader->GetFont();
		PriorityHeaderFont.Size = 22;
		PriorityHeaderFont.TypefaceFontName = FName(TEXT("Bold"));
		PriorityHeader->SetFont(PriorityHeaderFont);
		UVerticalBoxSlot* PriorityHeaderSlot = AIDebugContainer->AddChildToVerticalBox(PriorityHeader);
		PriorityHeaderSlot->SetPadding(FMargin(0, 5, 0, 2));
		PriorityHeaderSlot->SetHorizontalAlignment(HAlign_Left);
		PriorityHeaderSlot->SetVerticalAlignment(VAlign_Top);
		
		// Sub-priorities
		for (const auto& SubPair : PriorityInfo.SubPriorities)
		{
			UTextBlock* SubPriorityText = NewObject<UTextBlock>(this);
			SubPriorityText->SetText(FText::FromString(FString::Printf(TEXT("  %s: %d"),
				*SubPair.Key, SubPair.Value)));
			SubPriorityText->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f));
			FSlateFontInfo SubPriorityFont = SubPriorityText->GetFont();
			SubPriorityFont.Size = 18;
			SubPriorityText->SetFont(SubPriorityFont);
			UVerticalBoxSlot* SubPrioritySlot = AIDebugContainer->AddChildToVerticalBox(SubPriorityText);
			SubPrioritySlot->SetPadding(FMargin(10, 1, 0, 1));
			SubPrioritySlot->SetHorizontalAlignment(HAlign_Left);
			SubPrioritySlot->SetVerticalAlignment(VAlign_Top);
		}
	}
	
	// Display idle vehicles if any
	if (IdleVehicleCount > 0)
	{
		UTextBlock* IdleHeader = NewObject<UTextBlock>(this);
		IdleHeader->SetText(FText::FromString(FString::Printf(TEXT("Idle (%d veh)"), IdleVehicleCount)));
		IdleHeader->SetColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f)); // Gray
		FSlateFontInfo IdleHeaderFont = IdleHeader->GetFont();
		IdleHeaderFont.Size = 22;
		IdleHeaderFont.TypefaceFontName = FName(TEXT("Bold"));
		IdleHeader->SetFont(IdleHeaderFont);
		UVerticalBoxSlot* IdleHeaderSlot = AIDebugContainer->AddChildToVerticalBox(IdleHeader);
		IdleHeaderSlot->SetPadding(FMargin(0, 5, 0, 2));
		IdleHeaderSlot->SetHorizontalAlignment(HAlign_Left);
		IdleHeaderSlot->SetVerticalAlignment(VAlign_Top);
	}
	
	// Calculate unaccounted vehicles (should always be 0 if logic is correct)
	int32 AccountedVehicles = IdleVehicleCount;
	for (const FPriorityInfo& PriorityInfo : Priorities)
	{
		AccountedVehicles += PriorityInfo.VehicleCount;
	}
	int32 UnaccountedVehicles = CurrentTotalVehicles - AccountedVehicles;
	
	if (UnaccountedVehicles != 0)
	{
		UTextBlock* UnaccountedText = NewObject<UTextBlock>(this);
		UnaccountedText->SetText(FText::FromString(FString::Printf(TEXT("UNACCOUNTED: %d vehicles!"), UnaccountedVehicles)));
		UnaccountedText->SetColorAndOpacity(FLinearColor::Red);
		FSlateFontInfo UnaccountedFont = UnaccountedText->GetFont();
		UnaccountedFont.Size = 20;
		UnaccountedFont.TypefaceFontName = FName(TEXT("Bold"));
		UnaccountedText->SetFont(UnaccountedFont);
		UVerticalBoxSlot* UnaccountedSlot = AIDebugContainer->AddChildToVerticalBox(UnaccountedText);
		UnaccountedSlot->SetPadding(FMargin(0, 5, 0, 2));
		UnaccountedSlot->SetHorizontalAlignment(HAlign_Left);
		UnaccountedSlot->SetVerticalAlignment(VAlign_Top);
	}
}

void UGameHUDWidget::UpdateCityMarkers(FVector2D MinimapSize)
{
	if (!GetWorld()) return;
	
	// Update markers for all cities on this planet
	for (TActorIterator<ACityActor> It(GetWorld()); It; ++It)
	{
		ACityActor* City = *It;
		if (!City || City->OwningPlanet != PlanetActorRef) continue;
		
		// Create marker if it doesn't exist
		if (!CityMarkers.Contains(City))
		{
			UImage* Marker = NewObject<UImage>(this);
			
			// Use city icon texture if available
			if (CityIconTexture)
			{
				Marker->SetBrushFromTexture(CityIconTexture);
			}
			else
			{
				// Fallback: solid color square
				FSlateBrush Brush;
				Brush.TintColor = FSlateColor(FLinearColor::White);
Brush.ImageSize = FVector2D(12.0f, 12.0f);
			Marker->SetBrush(Brush);
		}
		
		// Set marker size (icon will be tinted by team color) - Half size = 6x6
		Marker->SetDesiredSizeOverride(FVector2D(12.0f, 12.0f));
			
			// Add to canvas
			UCanvasPanelSlot* CanvasSlot = MinimapCanvas->AddChildToCanvas(Marker);
			if (CanvasSlot)
			{
				CanvasSlot->SetAutoSize(false); // Must be false to respect size override
				CanvasSlot->SetSize(FVector2D(12.0f, 12.0f)); // Explicitly set slot size
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f)); // Center on position
				CanvasSlot->SetZOrder(10); // Above terrain texture
			}
			
			CityMarkers.Add(City, Marker);
			
			UE_LOG(LogTemp, Log, TEXT("Created minimap marker for city: %s"), *City->CityName);
		}
		
		// Update position based on city's world location
		UImage* Marker = CityMarkers[City];
		if (Marker)
		{
			FVector2D UV = PlanetActorRef->WorldPositionToMinimapUV(City->GetActorLocation());
			FVector2D PixelPos = UV * MinimapSize;
			
			UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot);
			if (CanvasSlot)
			{
				CanvasSlot->SetPosition(PixelPos);
			}
			
			// Update color to match team
			FLinearColor TeamColor = City->GetTeamColor();
			Marker->SetColorAndOpacity(TeamColor);
		}
	}
	
	// Remove markers for destroyed cities
	TArray<ACityActor*> ToRemove;
	for (auto& Pair : CityMarkers)
	{
		if (!IsValid(Pair.Key))
		{
			if (Pair.Value)
			{
				Pair.Value->RemoveFromParent();
			}
			ToRemove.Add(Pair.Key);
		}
	}
	
	for (ACityActor* Key : ToRemove)
	{
		CityMarkers.Remove(Key);
		UE_LOG(LogTemp, Log, TEXT("Removed marker for destroyed city"));
	}
}
