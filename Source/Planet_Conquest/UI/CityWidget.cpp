// Copyright Benjamin Ramsell. All Rights Reserved.

#include "CityWidget.h"
#include "CityActor.h"
#include "PlanetConquestPlayerController.h"
#include "GameHUDWidget.h"
#include "InsigniaEditorWidget.h"
#include "../Entities/Buildings/FactoryBuildingActor.h"
#include "../Entities/Buildings/TurretBuildingActor.h"
#include "../Entities/Buildings/LabBuildingActor.h"
#include "../Camera/PlanetCameraPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UCityWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCityWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// Close editor if city is no longer owned by player
	if (CurrentCity && CurrentCity->OwnerTeam != EOwnerTeam::Player)
	{
		OnCloseButtonClicked();
		UE_LOG(LogTemp, Warning, TEXT("City editor closed: city is no longer player-owned"));
	}
}

void UCityWidget::SetCity(ACityActor* InCity)
{
	CurrentCity = InCity;
	// UE_LOG(LogTemp, Warning, TEXT("CityWidget set to city: %s"), InCity ? *InCity->GetName() : TEXT("None"));
}

void UCityWidget::OnSpawnVehicleClicked()
{
	// Safety check - don't spawn if can't afford
	if (!CanAffordVehicle())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot spawn vehicle - insufficient funds"));
		return;
	}

	if (CurrentCity)
	{
		CurrentCity->SpawnVehicle();
		// UE_LOG(LogTemp, Warning, TEXT("Spawn vehicle button clicked for city: %s"), *CurrentCity->GetName());
	}
	else
	{
		// UE_LOG(LogTemp, Error, TEXT("Cannot spawn vehicle: No city set!"));
	}
}

void UCityWidget::OnSpawnShipClicked()
{
	if (!CanAffordShip())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot spawn ship - insufficient funds"));
		return;
	}

	if (CurrentCity)
	{
		CurrentCity->SpawnShip();
	}
}

void UCityWidget::OnCloseButtonClicked()
{
	// UE_LOG(LogTemp, Warning, TEXT("Close button clicked - hiding UI"));
	
	// Hide the widget
	SetVisibility(ESlateVisibility::Hidden);
	
	// Get the player controller and unlock camera
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC)
	{
		PC->SetCameraLocked(false);
		PC->DeselectAllActors();
		
		// Exit city editor camera mode
		if (APlanetCameraPawn* CameraPawn = Cast<APlanetCameraPawn>(PC->GetPawn()))
		{
			CameraPawn->ExitCityEditorMode();
		}
		
		// Show game HUD when city UI closes
		if (PC->GameHUDWidgetInstance)
		{
			PC->GameHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		}
		
		UE_LOG(LogTemp, Warning, TEXT("UI closed via button - camera unlocked"));
	}
}

bool UCityWidget::CanAffordVehicle() const
{
	if (!CurrentCity)
	{
		return false;
	}

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return false;
	}

	return PC->PlayerOrangeSubstrate >= CurrentCity->VehicleCost;
}

bool UCityWidget::CanAffordShip() const
{
	if (!CurrentCity || !CurrentCity->bIsCoastal)
	{
		return false;
	}

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return false;
	}

	return PC->PlayerOrangeSubstrate >= CurrentCity->ShipCost;
}

bool UCityWidget::IsCityCoastal() const
{
	return CurrentCity && CurrentCity->bIsCoastal;
}

ESlateVisibility UCityWidget::GetShipButtonVisibility() const
{
	return IsCityCoastal() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

void UCityWidget::OnBuyFactoryClicked()
{
	if (!CanAffordFactory())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot buy factory - insufficient Orange Substrate"));
		return;
	}

	if (CurrentCity)
	{
		AFactoryBuildingActor* NewFactory = CurrentCity->AddFactory();
		if (NewFactory)
		{
			// Deduct cost from player Orange Substrate
			APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
			if (PC)
			{
				PC->PlayerOrangeSubstrate -= NewFactory->BuildCost;
				// UE_LOG(LogTemp, Warning, TEXT("Factory purchased for %d OS. Remaining: %d"), NewFactory->BuildCost, PC->PlayerOrangeSubstrate);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to add factory - maximum chunks may be reached"));
		}
	}
}

void UCityWidget::OnBuyTurretClicked()
{
	if (!CanAffordTurret())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot buy turret - insufficient Orange Substrate"));
		return;
	}

	if (CurrentCity)
	{
		ATurretBuildingActor* NewTurret = CurrentCity->AddTurret();
		if (NewTurret)
		{
			// Deduct cost from player Orange Substrate
			APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
			if (PC)
			{
				PC->PlayerOrangeSubstrate -= NewTurret->BuildCost;
				// UE_LOG(LogTemp, Warning, TEXT("Turret purchased for %d OS. Remaining: %d"), NewTurret->BuildCost, PC->PlayerOrangeSubstrate);
			}
		}
	}
}

void UCityWidget::OnBuyLabClicked()
{
	if (!CanAffordLab())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot buy lab - insufficient Orange Substrate"));
		return;
	}

	if (CurrentCity)
	{
		ALabBuildingActor* NewLab = CurrentCity->AddLab();
		if (NewLab)
		{
			// Deduct cost from player Orange Substrate
			APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
			if (PC)
			{
				PC->PlayerOrangeSubstrate -= NewLab->BuildCost;
				// UE_LOG(LogTemp, Warning, TEXT("Lab purchased for %d OS. Remaining: %d"), NewLab->BuildCost, PC->PlayerOrangeSubstrate);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to add lab - maximum chunks may be reached"));
		}
	}
}

bool UCityWidget::CanAffordFactory() const
{
	if (!CurrentCity)
	{
		return false;
	}

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return false;
	}

	// Factory costs 3,000 Orange Substrate
	return PC->PlayerOrangeSubstrate >= 3000;
}

bool UCityWidget::CanAffordTurret() const
{
	if (!CurrentCity)
	{
		return false;
	}

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return false;
	}

	// Turret costs 2,000 Orange Substrate
	return PC->PlayerOrangeSubstrate >= 2000;
}

bool UCityWidget::CanAffordLab() const
{
	if (!CurrentCity)
	{
		return false;
	}

	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return false;
	}

	// Lab costs 1,500 Orange Substrate
	return PC->PlayerOrangeSubstrate >= 1500;
}

int32 UCityWidget::GetFactoryCount() const
{
	if (!CurrentCity)
	{
		return 0;
	}

	return CurrentCity->GetFactoryCount();
}

int32 UCityWidget::GetTurretCount() const
{
	if (!CurrentCity)
	{
		return 0;
	}

	return CurrentCity->GetTurretCount();
}

int32 UCityWidget::GetLabCount() const
{
	if (!CurrentCity)
	{
		return 0;
	}

	return CurrentCity->GetLabCount();
}

FText UCityWidget::GetCityName() const
{
	if (!CurrentCity)
	{
		return FText::FromString(TEXT("Unknown City"));
	}

	return FText::FromString(CurrentCity->CityName);
}

FText UCityWidget::GetPopulationText() const
{
	if (!CurrentCity)
	{
		return FText::FromString(TEXT("0"));
	}

	// Format population with commas (e.g., 100,000)
	FNumberFormattingOptions NumberFormat;
	NumberFormat.UseGrouping = true;
	NumberFormat.MinimumIntegralDigits = 1;
	
	return FText::AsNumber(CurrentCity->Population, &NumberFormat);
}

FSlateColor UCityWidget::GetPopulationColor() const
{
	if (!CurrentCity)
	{
		return FSlateColor(FLinearColor::White);
	}

	return FSlateColor(CurrentCity->GetGreenSubstrateColor());
}

int32 UCityWidget::GetGreenSubstrate() const
{
	if (!CurrentCity)
	{
		return 0;
	}

	return CurrentCity->GreenSubstrate;
}

int32 UCityWidget::GetOrangeSubstrate() const
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return 0;
	}

	return PC->PlayerOrangeSubstrate;
}

int32 UCityWidget::GetBlackSubstrate() const
{
	APlanetConquestPlayerController* PC = Cast<APlanetConquestPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		return 0;
	}

	return PC->PlayerBlackSubstrate;
}

void UCityWidget::OnOpenInsigniaEditorClicked()
{
	UE_LOG(LogTemp, Error, TEXT("CityWidget: OnOpenInsigniaEditorClicked entered. Class=%s Instance=%s InViewport=%d"),
		InsigniaEditorWidgetClass ? *InsigniaEditorWidgetClass->GetName() : TEXT("NULL"),
		InsigniaEditorInstance ? TEXT("valid") : TEXT("null"),
		InsigniaEditorInstance ? (int32)InsigniaEditorInstance->IsInViewport() : 0);

	// Don't open a second instance if one is already showing
	if (InsigniaEditorInstance && InsigniaEditorInstance->IsInViewport())
	{
		UE_LOG(LogTemp, Error, TEXT("CityWidget: Already showing, aborting."));
		return;
	}

	if (!InsigniaEditorWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("CityWidget: InsigniaEditorWidgetClass is not set. Assign WBP_InsigniaEditor in the Blueprint defaults."));
		return;
	}

	InsigniaEditorInstance = CreateWidget<UInsigniaEditorWidget>(GetWorld(), InsigniaEditorWidgetClass);
	if (!InsigniaEditorInstance)
	{
		return;
	}

	// Seed the editor with whatever the player already has
	InsigniaEditorInstance->InitWithInsignia(PlayerInsignia);

	// Subscribe to result delegates before showing
	InsigniaEditorInstance->OnInsigniaConfirmed.AddDynamic(this, &UCityWidget::HandleInsigniaConfirmed);
	// Cancel just closes itself internally; nothing extra needed on this side

	InsigniaEditorInstance->AddToViewport(10); // higher Z-order than the city widget
}

void UCityWidget::HandleInsigniaConfirmed(FInsigniaData NewInsignia)
{
	PlayerInsignia = NewInsignia;
	OnInsigniaUpdated();
}
