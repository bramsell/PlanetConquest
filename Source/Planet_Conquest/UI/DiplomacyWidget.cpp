// Copyright Benjamin Ramsell. All Rights Reserved.

#include "DiplomacyWidget.h"
#include "TalkDialogueWidget.h"
#include "CityWidget.h"
#include "GameHUDWidget.h"
#include "../Entities/Cities/CityActor.h"
#include "../Entities/Vehicles/VehicleActor.h"
#include "../Entities/Buildings/CapitalBuildingActor.h"
#include "../Entities/Buildings/TurretBuildingActor.h"
#include "../Core/PlanetConquestPlayerController.h"
#include "../Core/PlanetConquestGameMode.h"
#include "../Core/AITeamController.h"
#include "../Camera/PlanetCameraPawn.h"
#include "Components/Button.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

void UDiplomacyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Cache the original button styles
	if (AttackButton && TalkButton)
	{
		AttackButtonOriginalStyle = AttackButton->GetStyle();
		TalkButtonOriginalStyle = TalkButton->GetStyle();
		bStylesCached = true;
	}
	
	// Cache City Editor button style if it exists
	if (CityEditorButton)
	{
		CityEditorButtonOriginalStyle = CityEditorButton->GetStyle();
	}
}

void UDiplomacyWidget::SetCity(ACityActor* InCity)
{
	CurrentCity = InCity;
}

float UDiplomacyWidget::GetRelationshipValue() const
{
	if (!CurrentCity) return 0.0f;

	UWorld* World = GetWorld();
	if (!World) return 0.0f;

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return 0.0f;

	return GameMode->GetRelationship(EOwnerTeam::Player, CurrentCity->OwnerTeam);
}

FText UDiplomacyWidget::GetRelationshipText() const
{
	float Relationship = GetRelationshipValue();

	// Check if officially allied
	if (!CurrentCity) return FText::FromString(TEXT("Unknown"));
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
						return FText::FromString(TEXT("Allied"));
					}
					break;
				}
			}
		}
	}

	// Not allied - use relationship thresholds
	if (Relationship >= 20.0f)
	{
		return FText::FromString(TEXT("Very Friendly"));
	}
	else if (Relationship >= 10.0f)
	{
		return FText::FromString(TEXT("Friendly"));
	}
	else if (Relationship >= -30.0f)
	{
		return FText::FromString(TEXT("Neutral"));
	}
	else if (Relationship >= -70.0f)
	{
		return FText::FromString(TEXT("Hostile"));
	}
	else
	{
		return FText::FromString(TEXT("Enemy"));
	}
}

FLinearColor UDiplomacyWidget::GetRelationshipColor() const
{
	float Relationship = GetRelationshipValue();

	if (Relationship >= 50.0f)
	{
		// Green for allies
		return FLinearColor::Green;
	}
	else if (Relationship >= 0.0f)
	{
		// Yellow-green for friendly
		return FLinearColor(0.5f, 1.0f, 0.0f);
	}
	else if (Relationship >= -50.0f)
	{
		// Yellow for neutral/slight hostility
		return FLinearColor::Yellow;
	}
	else
	{
		// Red for enemies
		return FLinearColor::Red;
	}
}

FText UDiplomacyWidget::GetCityName() const
{
	if (CurrentCity)
	{
		return FText::FromString(CurrentCity->CityName);
	}
	return FText::FromString(TEXT("Unknown City"));
}

FText UDiplomacyWidget::GetOwnerTeamName() const
{
	if (!CurrentCity)
	{
		return FText::FromString(TEXT("Unknown"));
	}

	switch (CurrentCity->OwnerTeam)
	{
	case EOwnerTeam::Player:
		return FText::FromString(TEXT("Player"));
	case EOwnerTeam::AI1:
		return FText::FromString(TEXT("AI Team 2"));
	case EOwnerTeam::AI2:
		return FText::FromString(TEXT("AI Team 3"));
	case EOwnerTeam::AI3:
		return FText::FromString(TEXT("AI Team 4"));
	case EOwnerTeam::AI4:
		return FText::FromString(TEXT("AI Team 5"));
	case EOwnerTeam::AI5:
		return FText::FromString(TEXT("AI Team 6"));
	case EOwnerTeam::AI6:
		return FText::FromString(TEXT("AI Team 7"));
	case EOwnerTeam::AI7:
		return FText::FromString(TEXT("AI Team 8"));
	case EOwnerTeam::AI8:
		return FText::FromString(TEXT("AI Team 9"));
	case EOwnerTeam::AI9:
		return FText::FromString(TEXT("AI Team 10"));
	case EOwnerTeam::AI10:
		return FText::FromString(TEXT("AI Team 11"));
	case EOwnerTeam::AI11:
		return FText::FromString(TEXT("AI Team 12"));
	case EOwnerTeam::AI12:
		return FText::FromString(TEXT("AI Team 13"));
	case EOwnerTeam::AI13:
		return FText::FromString(TEXT("AI Team 14"));
	case EOwnerTeam::AI14:
		return FText::FromString(TEXT("AI Team 15"));
	case EOwnerTeam::AI15:
		return FText::FromString(TEXT("AI Team 16"));
	case EOwnerTeam::AI16:
		return FText::FromString(TEXT("AI Team 17"));
	case EOwnerTeam::AI17:
		return FText::FromString(TEXT("AI Team 18"));
	case EOwnerTeam::AI18:
		return FText::FromString(TEXT("AI Team 19"));
	case EOwnerTeam::AI19:
		return FText::FromString(TEXT("AI Team 20"));
	case EOwnerTeam::Neutral:
		return FText::FromString(TEXT("Neutral"));
	default:
		return FText::FromString(TEXT("Unknown Team"));
	}
}

void UDiplomacyWidget::OnAttackButtonClicked()
{
	if (!CurrentCity) return;

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return;

	// Collect selected vehicles
	TArray<AVehicleActor*> SelectedVehicles;
	for (AActor* Actor : PC->SelectedActors)
	{
		if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
		{
			if (Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				SelectedVehicles.Add(Vehicle);
			}
		}
	}

	if (SelectedVehicles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No vehicles selected to attack with!"));
		return;
	}

	// Send vehicles to attack city using optimized positioning
	FVector CityLocation = CurrentCity->GetActorLocation();
	FVector PlanetCenter = CurrentCity->PlanetCenter;
	
	// Setup coordinate system for formation around city
	FVector SurfaceNormal = (CityLocation - PlanetCenter).GetSafeNormal();
	FVector Tangent = FVector::CrossProduct(SurfaceNormal, FVector::UpVector).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::CrossProduct(SurfaceNormal, FVector::ForwardVector).GetSafeNormal();
	}
	FVector Bitangent = FVector::CrossProduct(SurfaceNormal, Tangent).GetSafeNormal();
	float CityAltitude = (CityLocation - PlanetCenter).Size();
	
	// Helper lambda to calculate angle of a position around the city
	auto CalculateAngle = [&](const FVector& Position) -> float
	{
		FVector RelativePos = Position - CityLocation;
		float X = FVector::DotProduct(RelativePos, Tangent);
		float Y = FVector::DotProduct(RelativePos, Bitangent);
		return FMath::Atan2(Y, X);
	};
	
	// Helper lambda to generate formation position at a given angle and radius
	auto GeneratePosition = [&](float Angle, float Radius) -> FVector
	{
		FVector Offset = (Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle)) * Radius;
		FVector FormationTarget = CityLocation + Offset;
		FVector DirectionFromCenter = (FormationTarget - PlanetCenter).GetSafeNormal();
		return PlanetCenter + DirectionFromCenter * CityAltitude;
	};
	
	// Generate all formation positions
	struct FFormationSlot
	{
		FVector Position;
		float Angle;
		float Radius;
		bool bAssigned = false;
	};
	
	TArray<FFormationSlot> FormationSlots;
	int32 Ring = 0;
	int32 TotalSlotsNeeded = SelectedVehicles.Num();
	
	while (FormationSlots.Num() < TotalSlotsNeeded)
	{
		int32 VehiclesInThisRing = FMath::Max(4 + (Ring * 4), 1); // 4, 8, 12, 16...
		float RingRadius = 800.0f + (Ring * 200.0f); // 800, 1000, 1200...
		
		for (int32 i = 0; i < VehiclesInThisRing && FormationSlots.Num() < TotalSlotsNeeded; i++)
		{
			float Angle = (2.0f * PI * i) / VehiclesInThisRing;
			FFormationSlot FormationSlot;
			FormationSlot.Position = GeneratePosition(Angle, RingRadius);
			FormationSlot.Angle = Angle;
			FormationSlot.Radius = RingRadius;
			FormationSlots.Add(FormationSlot);
		}
		Ring++;
	}
	
	// Assign vehicles to closest available formation slots
	TArray<bool> VehicleAssigned;
	VehicleAssigned.SetNum(SelectedVehicles.Num());
	for (int32 i = 0; i < VehicleAssigned.Num(); i++)
	{
		VehicleAssigned[i] = false;
	}
	
	// Greedy assignment: for each slot, find the closest unassigned vehicle
	for (int32 SlotIdx = 0; SlotIdx < FormationSlots.Num(); SlotIdx++)
	{
		float ClosestDistance = MAX_FLT;
		int32 ClosestVehicleIdx = -1;
		
		for (int32 VehicleIdx = 0; VehicleIdx < SelectedVehicles.Num(); VehicleIdx++)
		{
			if (VehicleAssigned[VehicleIdx]) continue;
			
			FVector VehiclePos = SelectedVehicles[VehicleIdx]->GetActorLocation();
			float Distance = FVector::Dist(VehiclePos, FormationSlots[SlotIdx].Position);
			
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestVehicleIdx = VehicleIdx;
			}
		}
		
		if (ClosestVehicleIdx != -1)
		{
			SelectedVehicles[ClosestVehicleIdx]->SetTargetLocation(FormationSlots[SlotIdx].Position);
			SelectedVehicles[ClosestVehicleIdx]->ForcedHostileTeams.Add(CurrentCity->OwnerTeam);
			
			VehicleAssigned[ClosestVehicleIdx] = true;
		}
	}
	
	// Find all turrets in the city and assign them as targets
	TArray<class ATurretBuildingActor*> CityTurrets;
	for (ABuildingActor* Building : CurrentCity->Buildings)
	{
		if (ATurretBuildingActor* Turret = Cast<ATurretBuildingActor>(Building))
		{
			if (Turret->CurrentHealth > 0)
			{
				CityTurrets.Add(Turret);
			}
		}
	}
	
	// Assign vehicles to target turrets first, then capital
	if (CityTurrets.Num() > 0)
	{
		// Distribute turret targets among vehicles
		for (int32 i = 0; i < SelectedVehicles.Num(); i++)
		{
			// Assign turret target (cycle through turrets if more vehicles than turrets)
			int32 TurretIndex = i % CityTurrets.Num();
			SelectedVehicles[i]->CurrentTarget = CityTurrets[TurretIndex];
			SelectedVehicles[i]->PrimaryTarget = CurrentCity->CapitalBuilding; // Remember capital as final objective
			SelectedVehicles[i]->bHasTarget = true;
		}
		UE_LOG(LogTemp, Warning, TEXT("Vehicles will target %d turret(s) first, then capital"), CityTurrets.Num());
	}
	else
	{
		// No turrets - target capital directly
		for (int32 i = 0; i < SelectedVehicles.Num(); i++)
		{
			if (CurrentCity->CapitalBuilding)
			{
				SelectedVehicles[i]->CurrentTarget = CurrentCity->CapitalBuilding;
				SelectedVehicles[i]->PrimaryTarget = CurrentCity->CapitalBuilding;
				SelectedVehicles[i]->bHasTarget = true;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("No turrets found - targeting capital directly"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Attacking %s with %d vehicle(s) in optimized formation"), 
		*CurrentCity->CityName, SelectedVehicles.Num());

	// Keep UI open - don't close it
}

void UDiplomacyWidget::OnTalkButtonClicked()
{
	if (!CurrentCity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot open dialogue - no city set"));
		return;
	}

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot open dialogue - no player controller"));
		return;
	}

	// Load the Blueprint widget class
	TSubclassOf<UTalkDialogueWidget> TalkDialogueClass = LoadClass<UTalkDialogueWidget>(nullptr, TEXT("/Game/UI/WBP_TalkDialogue.WBP_TalkDialogue_C"));
	if (!TalkDialogueClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load WBP_TalkDialogue Blueprint class"));
		return;
	}

	// Create the talk dialogue widget
	UTalkDialogueWidget* TalkWidget = CreateWidget<UTalkDialogueWidget>(PC, TalkDialogueClass);
	if (TalkWidget)
	{
		TalkWidget->InitializeDialogue(CurrentCity);
		TalkWidget->AddToViewport(100); // High Z-order to appear on top
		UE_LOG(LogTemp, Log, TEXT("Opened talk dialogue for %s"), *CurrentCity->CityName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create TalkDialogueWidget"));
	}
}

void UDiplomacyWidget::OnCloseButtonClicked()
{
	// Hide the widget via the city's method
	if (CurrentCity)
	{
		CurrentCity->HideDiplomacyWidget();
		CurrentCity->SetSelected(false);
	}
}

bool UDiplomacyWidget::HasVehiclesSelected() const
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return false;

	for (AActor* Actor : PC->SelectedActors)
	{
		if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
		{
			if (Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				return true;
			}
		}
	}

	return false;
}

int32 UDiplomacyWidget::GetSelectedVehicleCount() const
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC) return 0;

	int32 Count = 0;
	for (AActor* Actor : PC->SelectedActors)
	{
		if (AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor))
		{
			if (Vehicle->OwnerTeam == EOwnerTeam::Player)
			{
				Count++;
			}
		}
	}

	return Count;
}

void UDiplomacyWidget::PlayAttackButtonAnimation()
{
	if (AttackButtonClickAnim)
	{
		PlayAnimation(AttackButtonClickAnim);
	}
	else
	{
		// Fallback: manually trigger pressed state using button's style from UMG
		if (AttackButton && bStylesCached)
		{
			// Create a temporary style with pressed state
			FButtonStyle PressedStyle = AttackButtonOriginalStyle;
			PressedStyle.Normal = AttackButtonOriginalStyle.Pressed;
			
			// Apply pressed style
			AttackButton->SetStyle(PressedStyle);
			
			// Reset based on hover state after 0.15 seconds
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				if (AttackButton && bStylesCached)
				{
					// Restore to hovered or normal state based on current hover
					if (bAttackButtonHovered)
					{
						FButtonStyle HoverStyle = AttackButtonOriginalStyle;
						HoverStyle.Normal = AttackButtonOriginalStyle.Hovered;
						AttackButton->SetStyle(HoverStyle);
					}
					else
					{
						AttackButton->SetStyle(AttackButtonOriginalStyle);
					}
				}
			}, 0.15f, false);
		}
	}
}

void UDiplomacyWidget::PlayTalkButtonAnimation()
{
	if (TalkButtonClickAnim)
	{
		PlayAnimation(TalkButtonClickAnim);
	}
	else
	{
		// Fallback: manually trigger pressed state using button's style from UMG
		if (TalkButton && bStylesCached)
		{
			// Create a temporary style with pressed state
			FButtonStyle PressedStyle = TalkButtonOriginalStyle;
			PressedStyle.Normal = TalkButtonOriginalStyle.Pressed;
			
			// Apply pressed style
			TalkButton->SetStyle(PressedStyle);
			
			// Reset based on hover state after 0.15 seconds
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				if (TalkButton && bStylesCached)
				{
					// Restore to hovered or normal state based on current hover
					if (bTalkButtonHovered)
					{
						FButtonStyle HoverStyle = TalkButtonOriginalStyle;
						HoverStyle.Normal = TalkButtonOriginalStyle.Hovered;
						TalkButton->SetStyle(HoverStyle);
					}
					else
					{
						TalkButton->SetStyle(TalkButtonOriginalStyle);
					}
				}
			}, 0.15f, false);
		}
	}
}

void UDiplomacyWidget::UpdateButtonHoverStates(bool bIsHoveringAttack, bool bIsHoveringTalk)
{
	if (!bStylesCached || !AttackButton || !TalkButton)
	{
		return;
	}
	
	// Update Attack button hover state
	if (bIsHoveringAttack != bAttackButtonHovered)
	{
		bAttackButtonHovered = bIsHoveringAttack;
		
		if (bIsHoveringAttack)
		{
			// Apply hover style
			FButtonStyle HoverStyle = AttackButtonOriginalStyle;
			HoverStyle.Normal = AttackButtonOriginalStyle.Hovered;
			AttackButton->SetStyle(HoverStyle);
		}
		else
		{
			// Restore normal style
			AttackButton->SetStyle(AttackButtonOriginalStyle);
		}
	}
	
	// Update Talk button hover state
	if (bIsHoveringTalk != bTalkButtonHovered)
	{
		bTalkButtonHovered = bIsHoveringTalk;
		
		if (bIsHoveringTalk)
		{
			// Apply hover style
			FButtonStyle HoverStyle = TalkButtonOriginalStyle;
			HoverStyle.Normal = TalkButtonOriginalStyle.Hovered;
			TalkButton->SetStyle(HoverStyle);
		}
		else
		{
			// Restore normal style
			TalkButton->SetStyle(TalkButtonOriginalStyle);
		}
	}
}

// Update City Editor button hover state (for player cities)
void UDiplomacyWidget::UpdateCityEditorButtonHoverState(bool bIsHovering)
{
	if (!CityEditorButton)
	{
		return;
	}
	
	if (bIsHovering != bCityEditorButtonHovered)
	{
		bCityEditorButtonHovered = bIsHovering;
		
		if (bIsHovering)
		{
			// Apply hover color
			FButtonStyle HoverStyle = CityEditorButtonOriginalStyle;
			HoverStyle.Normal.TintColor = FSlateColor(CityEditorHoverColor);
			CityEditorButton->SetStyle(HoverStyle);
		}
		else
		{
			// Restore normal color
			FButtonStyle NormalStyle = CityEditorButtonOriginalStyle;
			NormalStyle.Normal.TintColor = FSlateColor(CityEditorNormalColor);
			CityEditorButton->SetStyle(NormalStyle);
		}
	}
}

// Play City Editor button click animation
void UDiplomacyWidget::PlayCityEditorButtonAnimation()
{
	if (!CityEditorButton)
	{
		return;
	}
	
	// Apply pressed color briefly
	FButtonStyle PressedStyle = CityEditorButtonOriginalStyle;
	PressedStyle.Normal.TintColor = FSlateColor(CityEditorPressedColor);
	CityEditorButton->SetStyle(PressedStyle);
	
	// Play animation if it exists
	if (CityEditorButtonClickAnim)
	{
		PlayAnimation(CityEditorButtonClickAnim);
	}
	
	// Reset to hover color after a short delay
	if (UWorld* World = GetWorld())
	{
		FTimerHandle ResetTimerHandle;
		World->GetTimerManager().SetTimer(ResetTimerHandle, [this]()
		{
			if (CityEditorButton)
			{
				FButtonStyle HoverStyle = CityEditorButtonOriginalStyle;
				HoverStyle.Normal.TintColor = FSlateColor(bCityEditorButtonHovered ? CityEditorHoverColor : CityEditorNormalColor);
				CityEditorButton->SetStyle(HoverStyle);
			}
		}, 0.15f, false);
	}
}

// Get city status text (shows "Owned" for player cities, relationship for AI cities)
FText UDiplomacyWidget::GetCityStatusText() const
{
	if (IsPlayerCity())
	{
		return FText::FromString(TEXT("Owned"));
	}
	else
	{
		// For AI cities, show team name and relationship
		FText TeamName = GetOwnerTeamName();
		FText Relationship = GetRelationshipText();
		return FText::Format(FText::FromString(TEXT("{0} - {1}")), TeamName, Relationship);
	}
}

// Get population value for this city
int32 UDiplomacyWidget::GetPopulation() const
{
	if (CurrentCity)
	{
		return CurrentCity->Population;
	}
	return 0;
}

// Get population color based on sustainability and growth
FSlateColor UDiplomacyWidget::GetPopulationColor() const
{
	if (CurrentCity)
	{
		return FSlateColor(CurrentCity->GetGreenSubstrateColor());
	}
	return FSlateColor(FLinearColor::White);
}

// Check if current city is player-owned
bool UDiplomacyWidget::IsPlayerCity() const
{
	return CurrentCity && CurrentCity->OwnerTeam == EOwnerTeam::Player;
}

// Get visibility for AI city elements (Talk/Attack buttons, relationship)
ESlateVisibility UDiplomacyWidget::GetAICityElementsVisibility() const
{
	return !IsPlayerCity() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

// Get visibility for player city elements (City Editor button, "Owned" text)
ESlateVisibility UDiplomacyWidget::GetPlayerCityElementsVisibility() const
{
	return IsPlayerCity() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

// Handle City Editor button click
void UDiplomacyWidget::OnCityEditorButtonClicked()
{
	if (!CurrentCity) return;
	
	UE_LOG(LogTemp, Log, TEXT("City Editor button clicked for city: %s"), *CurrentCity->CityName);
	
	// Get player controller
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(GetOwningPlayer());
	if (!PC || !PC->CityWidgetInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot open city editor - no player controller or CityWidgetInstance"));
		return;
	}
	
	// Hide the diplomacy widget
	CurrentCity->HideDiplomacyWidget();
	
	// Show the city editor
	PC->CityWidgetInstance->SetCity(CurrentCity);
	PC->CityWidgetInstance->SetVisibility(ESlateVisibility::Visible);
	PC->SetCameraLocked(true);
	
	// Enter city editor camera mode
	if (APlanetCameraPawn* CameraPawn = Cast<APlanetCameraPawn>(PC->GetPawn()))
	{
		CameraPawn->EnterCityEditorMode(CurrentCity->GetActorLocation());
	}
	
	// Hide game HUD when city UI shows
	if (PC->GameHUDWidgetInstance)
	{
		PC->GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
	}
	
	UE_LOG(LogTemp, Log, TEXT("City editor opened for: %s"), *CurrentCity->CityName);
}

FText UDiplomacyWidget::GetPlayerAllianceName() const
{
	UWorld* World = GetWorld();
	if (!World) return FText::FromString(TEXT("No Allegiance"));

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return FText::FromString(TEXT("No Allegiance"));

	return FText::FromString(GameMode->GetPlayerAllianceName());
}

FText UDiplomacyWidget::GetCityAllianceName() const
{
	if (!CurrentCity)
	{
		return FText::FromString(TEXT("No Allegiance"));
	}

	UWorld* World = GetWorld();
	if (!World) return FText::FromString(TEXT("No Allegiance"));

	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
	if (!GameMode) return FText::FromString(TEXT("No Allegiance"));

	// Get the alliance name for this city's owner team
	return FText::FromString(GameMode->GetTeamAllianceName(CurrentCity->OwnerTeam));
}

