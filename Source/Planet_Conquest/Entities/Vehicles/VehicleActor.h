// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceActor.h"
#include "VehicleActor.generated.h"

// Forward declare AI action enum
enum class EAIAction : uint8;

// Forward declarations
class AAITeamController;
class AResourceActor;

/**
 * Vehicle task types - represents what mission the vehicle is currently executing
 */
UENUM(BlueprintType)
enum class EVehicleTaskType : uint8
{
	None UMETA(DisplayName = "Idle"),                    // No active task - vehicle is idle
	SecureIncome UMETA(DisplayName = "Secure Income"),   // Capture resources (neutral or enemy-owned, may need to destroy mines)
	AttackTarget UMETA(DisplayName = "Attack Target"),   // Attack a specific enemy (vehicle, building, capital)
	DefendTerritory UMETA(DisplayName = "Defend"),       // Defend a specific location or city
	AidAlly UMETA(DisplayName = "Aid Ally"),            // Move to ally city and defend it
	Retaliate UMETA(DisplayName = "Retaliate")          // Counterattack enemies who attacked us
};

/**
 * Vehicle task structure - bundles all mission parameters into one atomic assignment
 * This replaces the scattered state fields (CurrentTarget, TargetResource, PostMineResource, etc.)
 */
USTRUCT(BlueprintType)
struct FVehicleTask
{
	GENERATED_BODY()

	// Task type and priority
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	EVehicleTaskType Type = EVehicleTaskType::None;
	
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	int32 Priority = 0; // 1-6, matches priority layer system (lower = higher priority)
	
	// Spatial constraints for autonomous behavior
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	int32 ClusterID = -1; // -1 = no cluster constraint
	
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	float SearchRadius = 5000.0f; // Operating area for autonomous target finding
	
	// Rules of engagement - AI strategic decisions bundled into the task
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	TSet<EOwnerTeam> MinesToDestroy; // Which teams' mines can we attack?
	
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	bool bCaptureNeutralOnly = true; // Only capture neutral resources, or also fight for enemy-owned?
	
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	bool bAvoidAlliedMines = true; // Skip resources protected by allied mines?
	
	// Specific targets (if task is not autonomous search)
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	TWeakObjectPtr<AActor> PrimaryTarget; // Main target (mine, building, vehicle, etc.)
	
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	TWeakObjectPtr<AResourceActor> Resource; // Resource to capture (or capture after mine destroyed)
	
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	FVector Destination = FVector::ZeroVector; // Specific location to move to
	
	// Autonomous continuation - should vehicle find next similar task when done?
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	bool bAutonomousContinue = false;
	
	// Task assignment tracking
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	TWeakObjectPtr<AAITeamController> AssigningController; // Which AI assigned this task?
	
	// Sub-priority label for debugging/display (e.g., "P2.1", "P2.3", etc.)
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	FString SubPriorityLabel;
	
	// Loyalty for player tasks
	UPROPERTY(BlueprintReadWrite, Category = "Task")
	TWeakObjectPtr<class ACityActor> LoyalCity; // City this vehicle is working near
	
	// Helper to check if task is valid (has targets if needed)
	bool IsValid() const
	{
		if (Type == EVehicleTaskType::None) return true;
		
		// Most tasks need either a target or autonomous continue enabled
		if (!PrimaryTarget.IsValid() && !Resource.IsValid() && !bAutonomousContinue)
			return false;
			
		return true;
	}
	
	// Default constructor
	FVehicleTask()
		: Type(EVehicleTaskType::None)
		, Priority(0)
		, ClusterID(-1)
		, SearchRadius(5000.0f)
		, bCaptureNeutralOnly(true)
		, bAvoidAlliedMines(true)
		, Destination(FVector::ZeroVector)
		, bAutonomousContinue(false)
	{
	}
};

UCLASS()
class PLANET_CONQUEST_API AVehicleActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AVehicleActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Visual mesh component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	class UStaticMeshComponent* VehicleMesh;

	// Collision sphere for vehicle-to-vehicle blocking
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	class USphereComponent* CollisionSphere;

	// Selection box
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	class UStaticMeshComponent* SelectionBox;

	// Target location marker
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	class UStaticMeshComponent* TargetMarker;

	// Planet alignment properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Planet")
	FVector PlanetCenter = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Planet")
	float PlanetRadius = 100000.0f;

	// Reference to the planet actor (for terrain queries like IsPointOnLand and volcano positions)
	UPROPERTY()
	class APlanetActor* OwningPlanet = nullptr;

	// Align this vehicle to the planet surface
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	virtual void AlignToPlanet();

	// Selection
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetSelected(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetHovered(bool bHovered);

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
	bool bIsSelected = false;

	// Movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Movement")
	float MovementSpeed = 500.0f; // Units per second

	// If true, combat "within attack range" logic won't clear the movement path.
	// Ships set this true so they keep sailing while auto-fire handles combat.
	bool bCanMoveWhileFiring = false;

	// If false, vehicle never moves onto land to capture a resource (e.g. ships).
	// PostMineResource is discarded instead of triggering a capture approach.
	bool bCanCaptureResources = true;

	// World-unit arc height added to projectile mid-flight (0 = flat surface-hugging).
	// Set per vehicle subclass (e.g. ships use 800 so shells clear terrain).
	float ProjectileArcHeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Movement")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Movement")
	bool bHasTarget = false;

	// Activity tracking for Green Substrate consumption
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Movement")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Combat")
	bool bIsFiring = false;

	// ===== WAYPOINT PATHFINDING (NEW SYSTEM) =====
	
	// Current path as array of waypoints (world positions)
	UPROPERTY()
	TArray<FVector> CurrentPath;

	// Index of current waypoint we're moving toward
	UPROPERTY()
	int32 CurrentWaypointIndex = 0;

	// Timer for repathing during combat (repath every 2-3 seconds for moving targets)
	float CombatRepathTimer = 0.0f;
	
	// Last pathfinding error (for debugging)
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Debug")
	FString LastPathfindingError;

	// Recovery maneuvers (kept for resource clipping recovery)
	bool bIsBackingUp = false;
	FVector BackupTargetLocation = FVector::ZeroVector;
	float BackupProgress = 0.0f;
	bool bIsMovingPerpendicular = false;
	FVector PerpendicularTargetLocation = FVector::ZeroVector;
	float PerpendicularProgress = 0.0f;

	// Debug throttle timer for waypoint following
	float DebugLogTimer = 0.0f;

	// Set a target location for the vehicle to move to
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetTargetLocation(FVector NewTarget);

	// Set a path to target location using navigation graph pathfinding
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetPathToTarget(FVector Destination);
	
	// Internal path creation - preserves combat state (used by task execution and combat positioning)
	virtual bool CreatePathTo(FVector Destination);

	// Clear the target
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ClearTarget();

	// Ownership
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	EOwnerTeam OwnerTeam = EOwnerTeam::Player;

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void UpdateColor();

	// Combat system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Combat")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Combat")
	float AttackRange = 1800.0f; // Range to auto-attack enemies

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Combat")
	float AttackDamage = 20.0f; // Damage per shot

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Combat")
	float AttackCooldown = 3.0f; // Seconds between attacks

	float TimeSinceLastAttack = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Combat")
	AActor* CurrentTarget = nullptr; // Can be a vehicle or city

	// Primary assigned target (persists through combat distractions)
	// When vehicle is sent to attack a city but gets distracted by defenders,
	// it will return to attacking this primary target once distractions are eliminated
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Combat")
	AActor* PrimaryTarget = nullptr;

	// Teams that this vehicle should attack regardless of relationship (set when given explicit attack orders)
	UPROPERTY(BlueprintReadWrite, Category = "Vehicle|Combat")
	TSet<EOwnerTeam> ForcedHostileTeams;

	// City healing system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Healing")
	float HealingStartDelay = 10.0f; // Seconds after last damage before healing starts

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Healing")
	float HealingInterval = 0.5f; // Heal every 0.5 seconds

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Healing")
	float HealingAmount = 1.0f; // HP restored per heal tick

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Healing")
	float CityHealingRange = 1500.0f; // Distance from city to allow healing

	float TimeSinceLastDamaged = 999.0f; // Start high so vehicles can heal immediately at start
	float TimeSinceLastHeal = 0.0f;

	// Health bar timing
	float LastHealthChangeTime = -999.0f; // Time since last health change (for health bar display)
	float PreviousHealth = 100.0f; // Previous health value to detect changes

	// Health bar widget
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|UI")
	class UWidgetComponent* HealthBarWidget;

	// Info widget (shows on hover)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|UI")
	class UWidgetComponent* InfoWidget;

	void UpdateInfoDisplay();

	// Damage smoke effect
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|VFX")
	class UParticleSystemComponent* DamageSmoke;

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Combat")
	void ApplyDamage(float DamageAmount, EOwnerTeam AttackerTeam = EOwnerTeam::Neutral, AActor* AttackingActor = nullptr);

	// Track last team/actor that damaged this vehicle for relationship updates and defense AI
	EOwnerTeam LastDamagingTeam = EOwnerTeam::Neutral;
	TWeakObjectPtr<AActor> LastDamagingActor = nullptr;

	// Actor caches - refreshed every 2s to eliminate per-frame GetAllActorsOfClass queries
	UPROPERTY()
	TArray<AActor*> CachedAllCities;
	UPROPERTY()
	TArray<AActor*> CachedAllVehicles;
	UPROPERTY()
	TArray<AActor*> CachedAllResources;
	UPROPERTY()
	TArray<AActor*> CachedAllBuildings;
	UPROPERTY()
	TArray<AActor*> CachedAllMines;
	UPROPERTY()
	AAITeamController* CachedTeamController = nullptr;
	float ActorCacheTimer = 999.0f; // Start high so first tick populates immediately

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Combat")
	void UpdateHealthBar();

	// Capture mechanics
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
	class AResourceActor* TargetResource = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetTargetResource(AResourceActor* Resource);

	// AI Mission assignment - what this vehicle is currently doing
	EAIAction AssignedAction;
	bool bHasAssignment = false;

	// Reference to the AI controller managing this vehicle (if AI-controlled)
	UPROPERTY()
	class AAITeamController* AIController = nullptr;

	// Specific city to defend (for DefendCities action)
	UPROPERTY()
	class ACityActor* DefendedCity = nullptr;

	// Player autonomous resource capture mode
	bool bPlayerAutonomousMode = false; // Enabled when player commands vehicle to autonomously capture resources
	bool bTargetNeutralOnly = true; // If true, only capture neutral resources; if false, capture neutral + enemy
	EOwnerTeam TargetEnemyTeam = EOwnerTeam::Neutral; // Which enemy team to target (only used when bTargetNeutralOnly is false)
	
	// AI autonomous resource capture mode
	bool bAIAutonomousMode = false; // Enabled when AI sends vehicle to capture resources autonomously
	float AIAutonomousSearchRadius = 5000.0f; // Territory radius to search for resources in autonomous mode (set by AI controller)
	
	UPROPERTY()
	class ACityActor* LoyalCity = nullptr; // City this vehicle is loyal to (for proximal resource capture)
	
	// AI Priority System - tracks which priority layer built/controls this vehicle
	bool bInP1 = false; // Priority 1: Survival (retaliatory vehicles)
	bool bInP2 = false; // Priority 2: Defense (defensive vehicles)
	bool bInP3 = false; // Priority 3: Intrusion (reclaim territory vehicles)
	bool bInP4 = false; // Priority 4: Expansion (claim unclaimed vehicles)
	bool bInP5 = false; // Priority 5: War (war party vehicles)
	bool bInP6 = false; // Priority 6: Aid (helping allies)
	
	// Cluster tracking for automatic cluster capture (only outside territories)
	int32 ActiveClusterID = -1; // ID of the cluster being captured (-1 = not capturing a cluster)
	bool bClusterOnlyMode = false; // True if vehicle was sent to capture cluster resources (outside territory) - won't switch to proximal mode

	// Mine attack tracking - resource to capture after destroying enemy mine
	UPROPERTY()
	class AResourceActor* PostMineResource = nullptr; // Resource to capture after destroying its mine

	// ========== NEW TASK SYSTEM ==========
	// This will eventually replace all the scattered state fields above
	
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Task")
	FVehicleTask CurrentTask;
	
	// Task management methods
	
	/** Assign a new task to this vehicle - atomically sets all mission parameters */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Task")
	void AssignTask(const FVehicleTask& NewTask);
	
	/** Clear the current task and go idle */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Task")
	void ClearTask();
	
	/** Check if vehicle is idle (no active task) */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Task")
	bool IsIdle() const { return CurrentTask.Type == EVehicleTaskType::None; }
	
	/** Check if a new task can preempt the current task based on priority */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Task")
	bool CanPreempt(int32 NewPriority) const;
	
	// Task execution methods (called by Tick based on CurrentTask.Type)
	void ExecuteSecureIncomeTask(float DeltaTime);
	void ExecuteAttackTargetTask(float DeltaTime);
	void ExecuteDefendTerritoryTask(float DeltaTime);
	void ExecuteAidAllyTask(float DeltaTime);
	void ExecuteRetaliateTask(float DeltaTime);
	
	// ========== END NEW TASK SYSTEM ==========

	// Autonomously find and pursue the next target based on assigned action
	void FindNextTarget();
	
	// AI task-based autonomous targeting - finds next target based on CurrentTask parameters
	void FindNextAITaskTarget();

	// Player autonomous resource helpers
	ACityActor* DeterminePlayerLoyalCity(); // Find the closest player-owned city
	AResourceActor* FindBestPlayerResourceToCapture(ACityActor* NearCity); // Find best resource near the loyal city
	
	// AI autonomous resource helpers
	ACityActor* DetermineAILoyalCity(); // Find the closest AI-owned city
	AResourceActor* FindBestAIResourceToCapture(ACityActor* NearCity); // Find best resource near the loyal city
	
	// Cluster resource helpers
	AResourceActor* FindClusterResource(int32 ClusterID); // Find uncaptured resource in the specified cluster
	
	// Formation positioning for coordinated city attacks
	FVector CalculateFormationPosition(class ACityActor* TargetCity, FVector ApproachDirection);

private:
	void MoveTowardsTarget(float DeltaTime);
	// Push a candidate combat position outside any city's pathfinding exclusion zone.
	// Prevents defenders from getting stuck when the optimal stop point falls inside a city.
	FVector AdjustPositionForCityExclusion(FVector Position) const;
};
