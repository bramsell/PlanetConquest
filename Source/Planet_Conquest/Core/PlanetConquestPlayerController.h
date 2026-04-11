// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlanetConquestPlayerController.generated.h"

// Forward declarations
class UCityWidget;
class UGameHUDWidget;
enum class EOwnerTeam : uint8;

// Black Substrate consumption modes
UENUM(BlueprintType)
enum class EBlackSubstrateMode : uint8
{
	Auxiliary UMETA(DisplayName = "Auxiliary (0 BS/sec, 35% speed/fire)"),
	Efficient UMETA(DisplayName = "Efficient (10 BS/sec, 100% speed/fire)"),
	Overdrive UMETA(DisplayName = "Overdrive (20 BS/sec, 150% speed/fire)")
};

// Mining modes for resource depletion/recovery
UENUM(BlueprintType)
enum class EMiningMode : uint8
{
	Aggressive UMETA(DisplayName = "Aggressive (Full income, depletes health)"),
	Sustainable UMETA(DisplayName = "Sustainable (33% income, recovers health)")
};

UCLASS()
class PLANET_CONQUEST_API APlanetConquestPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	APlanetConquestPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

	// Currently selected actors (supports multiple selection)
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TArray<AActor*> SelectedActors;

	// City UI Widget
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCityWidget> CityWidgetClass;

	UPROPERTY()
	UCityWidget* CityWidgetInstance;

	// Game HUD Widget (money, resources, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UGameHUDWidget> GameHUDWidgetClass;

	UPROPERTY()
	UGameHUDWidget* GameHUDWidgetInstance;

	// Box selection
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bIsBoxSelecting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FVector2D BoxSelectionStart;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FVector2D BoxSelectionEnd;

	// Grace period for box selection (prevents accidental release)
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bBoxSelectionGracePeriod = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	float BoxSelectionReleaseTime = -1.0f;

	// Right mouse button panning state
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	bool bIsRightMouseHeld = false;

	// Handle left click for selection
	void HandleLeftClick();
	void HandleLeftClickRelease();
	
	// Handle right click for panning
	void HandleRightClick();
	void HandleRightClickRelease();

	// Try to select an actor at the mouse position
	void SelectActorUnderMouse();

	// Box selection
	void PerformBoxSelection();

	// Deselect current actor
	void DeselectAllActors();

	// Helper to count selected vehicles
	int32 GetSelectedVehicleCount() const;

	// Lock/unlock camera input
	void SetCameraLocked(bool bLocked);

	// Income system - Orange & Black Substrate
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 PlayerOrangeSubstrate = 2000;
	
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 PlayerBlackSubstrate = 2000;
	
	// Income per cycle (calculated in CollectIncome)
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 PlayerOrangeIncomePerCycle = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 PlayerBlackIncomePerCycle = 0;
	
	// Green Substrate (food production) - rate-based, not accumulated
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 PlayerGreenSubstrate = 100; // Starting with 100 (from capital)

	// Resource discovery system - track which resource types player has captured
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	TSet<EResourceType> DiscoveredResourceTypes;

	// Mark a resource type as discovered (called when first captured)
	UFUNCTION(BlueprintCallable, Category = "Economy")
	void DiscoverResourceType(EResourceType ResourceType);

	// Check if a resource type has been discovered
	UFUNCTION(BlueprintCallable, Category = "Economy")
	bool IsResourceTypeDiscovered(EResourceType ResourceType) const;

	FTimerHandle IncomeTimerHandle;
	// FTimerHandle MaintenanceTimerHandle;  // DISABLED - No longer using OS maintenance

	void CollectIncome();
	// void PayVehicleMaintenance();  // DISABLED - No longer using OS maintenance

	// Black Substrate consumption system
	UPROPERTY(BlueprintReadWrite, Category = "Economy")
	EBlackSubstrateMode BlackSubstrateMode = EBlackSubstrateMode::Efficient;

	FTimerHandle BlackSubstrateConsumptionHandle;
	void ConsumeBlackSubstrate();

	// Mining mode system
	UPROPERTY(BlueprintReadWrite, Category = "Economy")
	EMiningMode MiningMode = EMiningMode::Aggressive;

	// Income collection counter for sustainable mode (collect every 3rd call = 15 seconds)
	int32 IncomeCollectionCounter = 0;

	UFUNCTION(BlueprintCallable, Category = "Economy")
	void SetMiningMode(EMiningMode NewMode);
	
	void UpdateResourceHealth(float DeltaTime);

	// Get multipliers based on current mode
	float GetSpeedMultiplier() const;
	float GetFireRateMultiplier() const;

	// Vehicle autopilot mode (ON = capture all proximal resources, OFF = capture single resource only)
	UPROPERTY(BlueprintReadWrite, Category = "Vehicles")
	bool bVehicleAutopilotMode = true;

	UFUNCTION(BlueprintCallable, Category = "Vehicles")
	void ToggleVehicleAutopilotMode();

	UFUNCTION(BlueprintPure, Category = "Vehicles")
	bool GetVehicleAutopilotMode() const { return bVehicleAutopilotMode; }

	// Black Substrate mode control
	UFUNCTION(BlueprintCallable, Category = "Economy")
	void SetBlackSubstrateMode(EBlackSubstrateMode NewMode);
	
	UFUNCTION(BlueprintCallable, Category = "Economy")
	void ToggleVehicleEfficiency();

	// Hover detection for cursor changes
	UPROPERTY()
	AActor* HoveredActor = nullptr;

	// Track last hovered resource to hide its info widget
	UPROPERTY()
	class AResourceActor* LastHoveredResource = nullptr;

	// Track last hovered vehicle to hide its highlight
	UPROPERTY()
	class AVehicleActor* LastHoveredVehicle = nullptr;

	// Track last hovered kaiju to hide its highlight
	UPROPERTY()
	class AKaijuActor* LastHoveredKaiju = nullptr;

	// Track last hovered building to hide its hover UI (city editor only)
	UPROPERTY()
	class ABuildingActor* LastHoveredBuilding = nullptr;

	// Unified autopilot helper functions for territory/cluster detection
	
	// Find which city (if any) has this location in its territory
	class ACityActor* FindCityForTerritory(FVector Location) const;
	
	// Get all neutral resources in a territory (around a city)
	TArray<class AResourceActor*> GetNeutralResourcesInTerritory(class ACityActor* City) const;
	
	// Get all neutral resources in a cluster
	TArray<class AResourceActor*> GetNeutralResourcesInCluster(int32 ClusterID) const;
	
	// Get all enemy mines (owned by specific team) in a territory
	TArray<class AMineActor*> GetMinesInTerritory(class ACityActor* City, EOwnerTeam TargetTeam) const;
	
	// Get all enemy mines (owned by specific team) in a cluster
	TArray<class AMineActor*> GetMinesInCluster(int32 ClusterID, EOwnerTeam TargetTeam) const;

	// Allied aid system - send military units to help allied cities under attack
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	int32 SendMilitaryAidToAlly(EOwnerTeam AllyTeam, class ACityActor* AllyCityUnderAttack);

	// Controlled cities list
	UPROPERTY(BlueprintReadOnly, Category = "Cities")
	TArray<class ACityActor*> ControlledCities;

	// ESC key handler — toggles the pause menu open/closed
	void HandleEscapeKey();
};
