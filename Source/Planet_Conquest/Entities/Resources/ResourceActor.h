// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceActor.generated.h"

UENUM(BlueprintType)
enum class EOwnerTeam : uint8
{
	Neutral UMETA(DisplayName = "Neutral"),
	Player UMETA(DisplayName = "Player"),
	AI1 UMETA(DisplayName = "AI Team 1"),
	AI2 UMETA(DisplayName = "AI Team 2"),
	AI3 UMETA(DisplayName = "AI Team 3"),
	AI4 UMETA(DisplayName = "AI Team 4"),
	AI5 UMETA(DisplayName = "AI Team 5"),
	AI6 UMETA(DisplayName = "AI Team 6"),
	AI7 UMETA(DisplayName = "AI Team 7"),
	AI8 UMETA(DisplayName = "AI Team 8"),
	AI9 UMETA(DisplayName = "AI Team 9"),
	AI10 UMETA(DisplayName = "AI Team 10"),
	AI11 UMETA(DisplayName = "AI Team 11"),
	AI12 UMETA(DisplayName = "AI Team 12"),
	AI13 UMETA(DisplayName = "AI Team 13"),
	AI14 UMETA(DisplayName = "AI Team 14"),
	AI15 UMETA(DisplayName = "AI Team 15"),
	AI16 UMETA(DisplayName = "AI Team 16"),
	AI17 UMETA(DisplayName = "AI Team 17"),
	AI18 UMETA(DisplayName = "AI Team 18"),
	AI19 UMETA(DisplayName = "AI Team 19")
};

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	OrangeSubstrate UMETA(DisplayName = "Orange Substrate"),
	BlackSubstrate UMETA(DisplayName = "Black Substrate"),
	GreenSubstrate UMETA(DisplayName = "Green Substrate")
};

// Forward declarations
class AVehicleActor;

UCLASS()
class PLANET_CONQUEST_API AResourceActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AResourceActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Visual mesh component - Orange Substrate resource
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
	class UStaticMeshComponent* ResourceMesh;

	// Selection box
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
	class UStaticMeshComponent* SelectionBox;

	// Progress bar widget for capture progress
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource|Capture")
	class UWidgetComponent* ProgressBarWidget;

	// Resource info widget (shows income per cycle on hover)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource|Info")
	class UWidgetComponent* ResourceInfoWidget;

	// Planet alignment properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Planet")
	FVector PlanetCenter = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Planet")
	float PlanetRadius = 100000.0f;

	// Reference to owning planet actor (for navigation waypoint registration)
	UPROPERTY()
	class APlanetActor* OwningPlanet = nullptr;

	// Continent this resource sits on (index into PlanetActor::ContinentSeeds; -1 = ocean/unknown)
	UPROPERTY(BlueprintReadOnly, Category = "Resource|Planet")
	int32 ContinentID = -1;

	// Navigation waypoint indices registered with planet
	TArray<int32> RegisteredWaypointIndices;

	// Align this resource to the planet surface
	UFUNCTION(BlueprintCallable, Category = "Resource")
	void AlignToPlanet();

	// Generate navigation waypoints around this resource
	void GenerateNavigationWaypoints();

	// Remove navigation waypoints when resource is destroyed
	void RemoveNavigationWaypoints();

	// Selection
	UFUNCTION(BlueprintCallable, Category = "Resource")
	void SetSelected(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "Resource")
	void SetHovered(bool bHovered);

	UPROPERTY(BlueprintReadOnly, Category = "Resource")
	bool bIsSelected = false;

	// Ownership
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EOwnerTeam OwnerTeam = EOwnerTeam::Neutral;

	// Resource type (Orange or Green Substrate)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EResourceType ResourceType = EResourceType::OrangeSubstrate;

	// Cluster ID for wild cluster resources (-1 = not in cluster, 0+ = cluster ID)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 ClusterID = -1;

	// Whether this resource is guarded by a living Kaiju (set at spawn time)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	bool bKaijuGuarded = false;

	// Mine building that extracts from this resource
	UPROPERTY(BlueprintReadOnly, Category = "Resource")
	class AMineActor* Mine = nullptr;

	// Resource size and income (scaled by volume)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	float SizeScale = 1.0f; // Visual scale multiplier (0.7 to 1.3)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 IncomePerInterval = 25; // Income generated every 5 seconds (10, 25, 40, or 50)

	// Resource health (0.0 to 1.0 representing 0% to 100%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Health")
	float ResourceHealth = 1.0f;

	// Track last depletion and recovery times (for periodic health changes)
	float LastDepletionTime = 0.0f;
	float LastRecoveryTime = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Resource")
	void CaptureForTeam(EOwnerTeam NewOwner);

	UFUNCTION(BlueprintCallable, Category = "Resource")
	void UpdateColor();

	// Set up the appropriate mesh based on resource type (called once after RandomizeSize)
	UFUNCTION(BlueprintCallable, Category = "Resource")
	void SetupMesh();

	// Randomize size and income (called on spawn)
	UFUNCTION(BlueprintCallable, Category = "Resource")
	void RandomizeSize();

	// Get effective income based on resource health
	UFUNCTION(BlueprintCallable, Category = "Resource")
	int32 GetEffectiveIncome() const;

	// Capture system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Capture")
	float MaxInfluence = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Resource|Capture")
	float CurrentInfluence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Capture")
	float InfluenceGainRate = 5.0f; // Per interval

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Capture")
	float InfluenceDecayRate = 5.0f; // Per interval

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Capture")
	float InfluenceInterval = 0.5f; // Seconds between influence changes

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Capture")
	float CaptureRange = 500.0f; // Distance within which vehicles can capture

	float TimeSinceLastInfluenceChange = 0.0f;

	// Cached vehicle list - populated once per InfluenceInterval (was 3 calls/interval → 1)
	UPROPERTY()
	TArray<AActor*> CachedAllVehiclesForResource;

	// Track which team is currently influencing this resource
	EOwnerTeam CapturingTeam = EOwnerTeam::Neutral;

	// Track which vehicle currently has the capture lock (prevents deadlock)
	UPROPERTY()
	AVehicleActor* CapturingVehicle = nullptr;

	// Update progress bar visual
	void UpdateProgressBar();

	// Show/hide resource info widget
	UFUNCTION(BlueprintCallable, Category = "Resource|Info")
	void ShowResourceInfo(bool bShow);

	// Update resource info widget display
	UFUNCTION(BlueprintCallable, Category = "Resource|Info")
	void UpdateResourceInfo();

	// Territory circle (shows ownership)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Territory")
	bool bShowTerritoryCircle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Territory")
	float TerritoryCircleRadius = 300.0f;

	// Draw ownership territory circle
	void DrawTerritoryCircle();

	// Get team color for territory circle
	FLinearColor GetTeamColor() const;
};
