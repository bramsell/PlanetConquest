// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceActor.h"
#include "CityActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API ACityActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ACityActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Buildings that make up this city
	UPROPERTY(BlueprintReadOnly, Category = "City|Buildings")
	TArray<class ABuildingActor*> Buildings;

	// Capital building (always exists, only one per city)
	UPROPERTY(BlueprintReadOnly, Category = "City|Buildings")
	class ACapitalBuildingActor* CapitalBuilding = nullptr;

	// Collision sphere for city boundaries
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "City")
	class USphereComponent* CollisionSphere;

	// Selection box
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "City")
	class UStaticMeshComponent* SelectionBox;

	// Health bar widget
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "City|Health")
	class UWidgetComponent* HealthBarWidget;

	// Diplomacy widget (shown when hovering over AI cities)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "City|Diplomacy")
	class UWidgetComponent* DiplomacyWidgetComponent;

	// City properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	FString CityName = TEXT("Unnamed City");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	FVector PlanetCenter = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	float PlanetRadius = 100000.0f;

	// Reference to owning planet (set by PlanetActor when spawned)
	UPROPERTY(BlueprintReadOnly, Category = "City")
	class APlanetActor* OwningPlanet;

	// Continent this city sits on (index into PlanetActor::ContinentSeeds; -1 = unknown)
	// Set by PlanetActor::SpawnCities at spawn time
	UPROPERTY(BlueprintReadOnly, Category = "City")
	int32 ContinentID = -1;

	// Navigation waypoint indices registered with planet
	TArray<int32> RegisteredWaypointIndices;

	// Align city to point outward from planet center
	UFUNCTION(CallInEditor, Category = "City")
	void AlignToPlanet();

	// Generate navigation waypoints around this city
	void GenerateNavigationWaypoints();

	// Remove navigation waypoints when city is destroyed
	void RemoveNavigationWaypoints();

	// Spawn capital building (called by PlanetActor after setting properties)
	void SpawnCapitalBuilding();

	// Selection
	UFUNCTION(BlueprintCallable, Category = "City")
	void SetSelected(bool bSelected);

	UPROPERTY(BlueprintReadOnly, Category = "City")
	bool bIsSelected = false;

	// Diplomacy UI
	UFUNCTION(BlueprintCallable, Category = "City|Diplomacy")
	void ShowDiplomacyWidget();

	UFUNCTION(BlueprintCallable, Category = "City|Diplomacy")
	void HideDiplomacyWidget();

	// Handle manual button clicks on screen-space widget
	void HandleDiplomacyWidgetClick();

	// Track if mouse is currently hovering over this city
	bool bIsMouseHovering = false;

	// Prevent diplomacy widget from being reshown immediately after closing
	float LastDiplomacyWidgetHideTime = -999.0f;

	// Ownership
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	EOwnerTeam OwnerTeam = EOwnerTeam::Player;

	UFUNCTION(BlueprintCallable, Category = "City")
	void UpdateColor();

	// Population system
	UPROPERTY(BlueprintReadOnly, Category = "City|Population")
	int32 Population = 100000; // Starting population

	void UpdatePopulation(float DeltaTime);

	// Green Substrate (food production) - per-city rate-based resource
	UPROPERTY(BlueprintReadOnly, Category = "City|Population")
	int32 GreenSubstrate = 100; // Starting with 100 from capital building

	// Get green substrate color based on population needs (red/green/white)
	FLinearColor GetGreenSubstrateColor() const;

	// Spawn a vehicle near this city
	UFUNCTION(BlueprintCallable, Category = "City")
	void SpawnVehicle();

	// Spawn a ship in the water near this city (requires bIsCoastal)
	UFUNCTION(BlueprintCallable, Category = "City")
	void SpawnShip();

	// Add buildings to the city
	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	class AFactoryBuildingActor* AddFactory();

	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	class ATurretBuildingActor* AddTurret();

	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	class ALabBuildingActor* AddLab();

	// Get count of specific building types
	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	int32 GetFactoryCount() const;

	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	int32 GetTurretCount() const;

	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	int32 GetLabCount() const;

	// Reposition all turrets evenly around the ring
	void RepositionTurrets();

	// Calculate turret ring radius based on factory chunks
	UFUNCTION(BlueprintCallable, Category = "City|Buildings")
	float CalculateTurretRadius() const;

	// Cost to spawn a vehicle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	int32 VehicleCost = 1000;

	// Cost to spawn a ship (requires bIsCoastal)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	int32 ShipCost = 1000;

	// Whether this city was spawned on a coastal location (adjacent to water)
	// Coastal cities can build ships
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "City")
	bool bIsCoastal = false;

	// Distance from city to spawn vehicles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	float VehicleSpawnRadius = 1000.0f;

	// Territory visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	bool bShowTerritoryCircle = true;

	// Helper function to draw territory circle
	void DrawTerritoryCircle();

	// Get team color for this city
	FLinearColor GetTeamColor() const;

	// Health system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Health")
	float MaxHealth = 1000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "City|Health")
	float CurrentHealth = 1000.0f;

	// Track which team last damaged this city (for ownership flip)
	EOwnerTeam LastDamagingTeam = EOwnerTeam::Neutral;

	// Track if this city has been attacked before (for first-attack detection)
	bool bHasBeenAttacked = false;

	// Apply damage to this city
	UFUNCTION(BlueprintCallable, Category = "City|Health")
	void ApplyDamage(float DamageAmount, EOwnerTeam AttackingTeam, AActor* AttackingActor = nullptr);

	// Flip ownership to the attacking team
	void FlipOwnership(EOwnerTeam NewTeam);

	// Update health bar visual
	void UpdateHealthBar();

	// Building healing system - track when any building was last damaged
	UPROPERTY(BlueprintReadOnly, Category = "City|Buildings")
	float TimeSinceAnyBuildingDamaged = 999.0f; // Start high so buildings can heal at start

	// Called by buildings when they take damage
	void NotifyBuildingDamaged();

private:
	// Timer handle for population updates
	FTimerHandle PopulationUpdateHandle;

	// Factory chunk tracking: 6 inner chunks + 10 outer chunks = 16 total
	// Chunks 0-5: Inner ring (60° apart), Chunks 6-15: Outer ring (36° apart)
	TArray<int32> AvailableChunkIndices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	TArray<int32> UsedChunkIndices;
	int32 CurrentChunkIndex = -1; // Which chunk is currently being filled
	int32 FactoriesInCurrentChunk = 0; // How many factories in the current chunk (max 4)
	
	// Calculate position for a factory in a chunk (uses 2x2 grid within chunk)
	FVector CalculateFactoryPosition(int32 ChunkIndex, int32 PositionInChunk);
	
	// Calculate position for a lab at chunk center (labs take entire chunk)
	FVector CalculateLabPosition(int32 ChunkIndex);
};
