// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Entities/Resources/ResourceActor.h"
#include "AITeamController.generated.h"

// Forward declarations
class AVehicleActor;
class ACityActor;
class AResourceActor;
class ABuildingActor;

// ========== AI PERSONALITY ARCHETYPES ==========

/**
 * AI Personality Archetype - defines strategic behavior patterns
 */
UENUM(BlueprintType)
enum class EAIArchetype : uint8
{
	Warmonger     UMETA(DisplayName = "Warmonger - Aggressive early fighter"),
	Opportunistic UMETA(DisplayName = "Opportunistic - Balanced aggressor"),
	Cautious      UMETA(DisplayName = "Cautious - Defensive pacifist"),
	Expansionist  UMETA(DisplayName = "Expansionist - Economic powerhouse"),
	Custom        UMETA(DisplayName = "Custom - Manual configuration")
};

// ========== PRIORITY 1 DEFENSE STRUCTS ==========

/**
 * Threat level classification for defensive missions
 */
UENUM(BlueprintType)
enum class EThreatLevel : uint8
{
	None,
	Critical,  // Capital building under attack → 3 defenders per attacker
	High,      // Turret building under attack → 2 defenders per attacker  
	Medium,    // Mine in territory (≤5000 units) under attack → 2 defenders per attacker
	Low        // Mine outside territory (5000-10000 units) under attack → 1 defender per attacker
};

/**
 * Defense threat information - tracks what's under attack and who's attacking it
 */
USTRUCT()
struct FDefenseThreat
{
	GENERATED_BODY()

	EThreatLevel Level = EThreatLevel::None;
	ABuildingActor* Building = nullptr;           // What's being attacked
	TArray<AActor*> Attackers;                    // Who's attacking it (vehicles or kaiju)
	ACityActor* DefendingCity = nullptr;          // Our city defending this
	float DistanceFromCity = 0.0f;
	int32 DefendersNeeded = 0;                    // Based on threat level
	int32 DefendersAssigned = 0;
	EOwnerTeam AttackingTeam = EOwnerTeam::Neutral;
	int32 ClusterID = -1;                         // For cluster-based defense
	float ClusterRadius = 0.0f;                   // For cluster-based defense radius
	
	// Calculate defenders needed based on threat level
	void CalculateDefendersNeeded()
	{
		switch (Level)
		{
			case EThreatLevel::Critical:
				DefendersNeeded = Attackers.Num() * 3; // Capital: 3 per attacker
				break;
			case EThreatLevel::High:
				DefendersNeeded = Attackers.Num() * 2; // Turret: 2 per attacker
				break;
			case EThreatLevel::Medium:
				DefendersNeeded = Attackers.Num() * 2; // Territory mine: 2 per attacker
				break;
			case EThreatLevel::Low:
				DefendersNeeded = Attackers.Num() * 1; // Extended mine: 1 per attacker
				break;
			default:
				DefendersNeeded = 0;
				break;
		}
	}
};

// ========== WORLD STATE ASSESSMENT STRUCTS ==========

/**
 * Cluster information for resource evaluation
 */
USTRUCT()
struct FClusterInfo
{
	GENERATED_BODY()

	int32 ClusterID = -1;
	EResourceType ResourceType;
	TArray<AResourceActor*> Resources;
	FVector AverageLocation = FVector::ZeroVector;
	float DistanceToOwnCity = FLT_MAX;
	float DistanceToClosestEnemyCity = FLT_MAX;
	int32 Priority = 999;
	bool bIsContested = false;
	bool bIsUncontested = false;
};

/**
 * Global state of my team (independent of opponents)
 */
USTRUCT()
struct FMyGlobalState
{
	GENERATED_BODY()

	// Wealth
	int32 OrangeSubstrate = 0;          // Current orange substrate owned
	int32 BlackSubstrate = 0;           // Current black substrate owned
	int32 OrangeIncomePerCycle = 0;     // Orange income from all mines
	int32 BlackIncomePerCycle = 0;      // Black income from all mines
	
	// Military
	int32 TotalVehicles = 0;            // Total vehicles owned
	
	// Threat
	bool bIsUnderActiveAttack = false;  // Any city currently has hostile vehicles in territory
	EOwnerTeam ActiveAttacker = EOwnerTeam::Neutral; // Which team is attacking (for retaliation scoring)
	
	// Opportunity - Unclaimed resources
	TArray<AResourceActor*> UnclaimedResourcesInTerritory;     // Neutral resources within my territory
	TArray<AResourceActor*> UnclaimedResourcesOutsideTerritory; // Neutral resources beyond my territory
};

/**
 * Per-city state of my cities (defense and proximity)
 */
USTRUCT()
struct FMyCityState
{
	GENERATED_BODY()

	ACityActor* City = nullptr;
	
	// Defense (within territory radius)
	int32 VehiclesInTerritory = 0;        // Any friendly vehicle within 5000 units
	int32 TurretsInTerritory = 0;         // Turrets within 5000 units
	
	// Threat (within territory radius)
	int32 HostileVehiclesInTerritory = 0; // Enemy vehicles actively firing within 5000 units
	
	// Proximity
	float DistanceToNearestEnemyCity = 0.0f; // Distance to closest hostile city
	
	// Intrusions - Resources enemies have claimed in MY territory
	TArray<AResourceActor*> EnemyOrangeInMyTerritory; // Orange nodes stolen by any enemy
	TArray<AResourceActor*> EnemyBlackInMyTerritory;  // Black nodes stolen by any enemy
	int32 TotalOrangeIncomeLostInTerritory = 0;  // Cumulative orange income lost in this city's territory
	int32 TotalBlackIncomeLostInTerritory = 0;   // Cumulative black income lost in this city's territory
	
	// War Preparation (Priority 5)
	int32 WarVehiclesReserved = 0;  // Number of vehicles reserved for war (not used by other priorities)
	ACityActor* WarTarget = nullptr;  // Target enemy city for this war party
	bool bWarLaunched = false;  // True once attack launched, prevents recall if substrate drops
};

/**
 * Per-city state of opponent cities (defense assessment)
 */
USTRUCT()
struct FOpponentCityState
{
	GENERATED_BODY()

	ACityActor* City = nullptr;
	EOwnerTeam OwnerTeam = EOwnerTeam::Neutral;
	
	// Defense (within their territory radius)
	int32 VehiclesInTerritory = 0;      // Their vehicles within 5000 units of their city
	int32 TurretsInTerritory = 0;       // Their turrets within 5000 units of their city
	
	// Proximity
	float DistanceToMyClosestCity = 0.0f;  // How far is their city from my nearest city
	
	// Opportunity - Unclaimed resources near their city that we could contest
	TArray<AResourceActor*> UnclaimedResourcesInTheirTerritory; // Neutral resources in their territory
	
	// Resources they own INSIDE this city's territory (defended)
	TArray<AResourceActor*> OrangeResourcesInTheirTerritory;  // Orange nodes they own near this city
	TArray<AResourceActor*> BlackResourcesInTheirTerritory;   // Black nodes they own near this city
	int32 OrangeIncomeInTheirTerritory = 0;  // Cumulative orange income from this city's territory
	int32 BlackIncomeInTheirTerritory = 0;   // Cumulative black income from this city's territory
};

/**
 * Alliance state tracking
 */
USTRUCT()
struct FAllianceState
{
	GENERATED_BODY()

	// Teams in this alliance (not including this team)
	TSet<EOwnerTeam> AlliedTeams;
	
	// Alliance Request Cooldown - Prevents spamming alliance requests (10 cycles = 30 seconds)
	// NOTE: This is separate from trade cooldown system (LastTradeRequestCycle in main controller)
	TMap<EOwnerTeam, int32> LastAllianceRequestCycle;

	// Name of this alliance (e.g., "North Star Alliance", "Eastern Rim Alliance")
	UPROPERTY()
	FString AllianceName = TEXT("");
};

/**
 * Joint attack proposal state
 */
USTRUCT()
struct FJointAttackProposal
{
	GENERATED_BODY()

	ACityActor* TargetCity = nullptr;           // City to attack
	EOwnerTeam ProposingTeam = EOwnerTeam::Neutral;  // Who proposed the attack
	EOwnerTeam PartnerTeam = EOwnerTeam::Neutral;    // Who they want to attack with
	int32 ProposalCycle = 0;                    // When the proposal was made
	bool bIsReady = false;                      // Is this team ready to attack
	bool bPartnerReady = false;                 // Is partner ready to attack
};

/**
 * Aid request state
 */
USTRUCT()
struct FAidRequest
{
	GENERATED_BODY()

	EOwnerTeam RequestingTeam = EOwnerTeam::Neutral;  // Who is requesting aid
	ACityActor* CityUnderAttack = nullptr;            // Which city is under attack
	int32 RequestCycle = 0;                           // When the request was made
	bool bRequestedSubstrate = false;                 // Requesting orange substrate
	bool bRequestedMilitary = false;                  // Requesting military aid
	int32 SubstrateAmount = 0;                        // Amount of substrate requested (1000-5000)
	bool bWaitingForPlayerResponse = false;           // True if player needs to respond
	bool bPlayerHasResponded = false;                 // True after player responds
	int32 SubstrateProvided = 0;                      // Amount actually sent
	bool bMilitaryAidSent = false;                    // True if military aid dispatched
};

/**
 * Global state of opponent team
 */
USTRUCT()
struct FOpponentGlobalState
{
	GENERATED_BODY()

	EOwnerTeam Team = EOwnerTeam::Neutral;
	
	// Wealth (observable - income only, can't see their bank)
	int32 OrangeIncomePerCycle = 0;     // Their total orange income from all mines
	int32 BlackIncomePerCycle = 0;      // Their total black income from all mines
	
	// Diplomacy (from GameMode relationship system)
	float Disposition = 0.0f;           // -100 to +100
	float Fear = 0.0f;                  // 0 to 100 (my fear of them)
	float Respect = 0.0f;               // 0 to 100 (my respect for them)
	
	// Strength
	int32 TotalVehicles = 0;            // Their total vehicle count
	
	// Opportunity - Resources they own OUTSIDE any of their city territories (vulnerable)
	TArray<AResourceActor*> OrangeResourcesOutsideTheirTerritory;  // Orange nodes outside their 5000 radius
	TArray<AResourceActor*> BlackResourcesOutsideTheirTerritory;   // Black nodes outside their 5000 radius
	int32 OrangeIncomeOutsideTheirTerritory = 0;  // Cumulative orange income from exposed resources
	int32 BlackIncomeOutsideTheirTerritory = 0;   // Cumulative black income from exposed resources
	
	// Per-city breakdown
	TArray<FOpponentCityState> Cities;
};

/**
 * Simple state-based AI controller for team behavior
 * Manages AI team cities, vehicles, and strategic decisions
 */
UCLASS()
class PLANET_CONQUEST_API AAITeamController : public AActor
{
	GENERATED_BODY()
	
public:	
	AAITeamController();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Which AI team this controller manages (AITeam1 or AITeam2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	EOwnerTeam ControlledTeam;

	// ========== AI PERSONALITY ==========
	
	// AI Personality Archetype - automatically sets parameters below when BeginPlay runs
	// Set to "Custom" to manually configure individual parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Personality")
	EAIArchetype Archetype = EAIArchetype::Opportunistic;

	// Kaiju Safety Distance: How far away a Kaiju must be from a cluster center for it to be considered safe
	// Archetype defaults: Warmonger=3000, Opportunistic=4000, Cautious=6000, Expansionist=4000
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Personality")
	float KaijuSafetyDistance = 4000.0f;

	// P2.1 Range: Maximum distance from city to claim unclaimed resources (multiplier of territory radius 5000)
	// Archetype defaults: Warmonger=3.0, Opportunistic=4.0, Cautious=5.0, Expansionist=6.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Personality")
	float P2_1_RangeMultiplier = 4.0f;

	// P2.3 Relationship Threshold: Will only attack mines from teams with relationship BELOW this value
	// Archetype defaults: Warmonger=+10, Opportunistic=0, Cautious=-20, Expansionist=-20
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Personality")
	float P2_3_RelationshipThreshold = 0.0f;

	// P2.3 Income Threshold Multiplier: How much income per city before stopping mine attacks
	// Income required = NumCities * 100 * ThisMultiplier
	// Archetype defaults: Warmonger=1.0, Opportunistic=1.2, Cautious=1.0, Expansionist=1.5
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Personality")
	float P2_3_IncomeMultiplier = 1.2f;

	// Resources
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	int32 OrangeSubstrate = 2000; // Starting orange substrate for AI
	
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	int32 BlackSubstrate = 2000; // Starting black substrate for AI
	
	// Income per cycle (calculated in CollectIncome)
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	int32 OrangeIncomePerCycle = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	int32 BlackIncomePerCycle = 0;
	
	// Reserved resources (protected from trading)
	// Updated each decision cycle based on active priorities
	int32 ReservedOrangeSubstrate = 0;
	int32 ReservedBlackSubstrate = 0;

	// Trade Request Cooldown - Prevents spamming trade offers (10 cycles AI-AI, 40 cycles player)
	// Prevents player from rapidly boosting relationships through repeated trades
	// NOTE: This is separate from alliance cooldown system (LastAllianceRequestCycle in AllianceState)
	// Trading only happens in Priority 1 (under attack) and Priority 2 (low black substrate)
	TMap<EOwnerTeam, int32> LastTradeRequestCycle;
	
	// Bribe Request Cooldown - Prevents spamming bribes (10 cycles = 30 seconds)
	// Bribery happens in Priority 3 (Defense) when threatened by max-hostile enemies
	TMap<EOwnerTeam, int32> LastBribeAttemptCycle;
	
	int32 CurrentDecisionCycle = 0; // Incremented each decision cycle
	
	// Timer for decision making (check every N seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float DecisionInterval = 3.0f;

	// Time since last decision
	float TimeSinceLastDecision = 0.0f;

	// ========== WORLD STATE ASSESSMENT ==========
	
	// Current world state (populated every decision cycle)
	FMyGlobalState MyGlobalState;
	TArray<FMyCityState> MyCityStates;
	TArray<FOpponentGlobalState> OpponentStates;
	
	// Alliance state
	FAllianceState AllianceState;
	
	// Joint attack proposals (both incoming and outgoing)
	TArray<FJointAttackProposal> JointAttackProposals;
	
	// Aid requests (both incoming and outgoing)
	TArray<FAidRequest> AidRequests;
	
	// Assess current world state for decision-making (called every DecisionInterval)
	void AssessWorldState();
	
	// Get territory radius from GameMode (5000 units by default)
	float GetTerritoryRadius() const;

	// ========== DECISION MAKING ==========
	
	// Make strategic decisions based on assessed world state
	void MakeDecision();

	// ========== PRIORITY LAYERS ==========
	// All layers always run (handle what they can), no early returns
	
	// Priority 1: Survival - Defend cities under active attack
	void ExecuteSurvivalLayer();
	
	// Priority 2: Income Generation - Hierarchical resource acquisition (merges old P2+P3)
	void ExecuteIncomeLayer();
	
	// Priority 3: DEPRECATED - Merged into ExecuteIncomeLayer
	// void ExecuteExpansionLayer();
	
	// Priority 3: Defense - Build defenses based on proximity-based threat calculation
	void ExecuteDefenseLayer();
	
	// Priority 3.5: Alliance Formation - Seek allies against max-hostile enemies
	void ExecuteAllianceFormation();
	
	// Priority 4: War - Amass vehicles and attack enemy cities directly
	void ExecuteWarLayer();
	
	// Priority 5: Aid Allies - Send resources and military support to allies under attack
	void ExecuteAidLayer();
	
	// Helper functions for priority layers
	TArray<AVehicleActor*> GetDefenders(int32 Count); // Get vehicles for P1 defense (can preempt lower priorities)
	int32 BuildDefenders(int32 Count); // Build defensive vehicles

	// ========== ALLIANCE & JOINT ATTACKS ==========
	
	// Check if this team is allied with another team
	bool IsAlliedWith(EOwnerTeam OtherTeam) const;
	
	// Request alliance with another team against a common enemy
	// Returns true if request sent, false if cooldown active or invalid request
	bool RequestAlliance(EOwnerTeam TargetTeam, EOwnerTeam CommonEnemy);
	
	// Evaluate incoming alliance request
	// Returns true if accepted, false if rejected
	bool EvaluateAllianceRequest(EOwnerTeam RequestingTeam, EOwnerTeam CommonEnemy);
	
	// Propose joint attack with an allied team
	// Returns true if proposal sent, false if no suitable ally
	bool ProposeJointAttack(ACityActor* TargetCity, EOwnerTeam AllyTeam);
	
	// Evaluate incoming joint attack proposal
	// Returns true if accepted, false if rejected (already preparing for different attack)
	bool EvaluateJointAttackProposal(const FJointAttackProposal& Proposal);
	
	// Find all teams we could request alliance with for attacking a specific enemy
	TArray<EOwnerTeam> FindPotentialAllies(EOwnerTeam CommonEnemy);
	
	// Find allied team that could join a joint attack on a target
	EOwnerTeam FindJointAttackPartner(ACityActor* TargetCity);
	
	// ========== AID REQUESTS ==========
	
	// Request aid from allies (substrate and/or military)
	// Returns true if request sent, false if no suitable ally or cooldown active
	bool RequestAidFromAllies(ACityActor* CityUnderAttack, bool bNeedSubstrate, bool bNeedMilitary, int32 SubstrateNeeded = 1000);
	
	// Evaluate incoming aid request from ally
	// Returns true if aid sent, false if can't afford or not allied
	bool EvaluateAidRequest(const FAidRequest& Request);
	
	// Send substrate aid to an ally
	// Returns amount actually sent (may be less than requested if can't afford full amount)
	int32 SendSubstrateAid(EOwnerTeam AllyTeam, int32 RequestedAmount);
	
	// Send military aid to an ally (up to 10 idle vehicles)
	// Returns number of vehicles sent
	int32 SendMilitaryAid(EOwnerTeam AllyTeam, ACityActor* AllyCityUnderAttack);

	// ========== ACTION EXECUTORS ==========
	// Standardized logic for accomplishing common goals (shared across layers)
	
	// Ensure a city has enough defenders (send vehicles, build, call allies, overdrive)
	// Priority parameter determines which vehicle array to use (1-5)
	// bBuildTurrets: If true, build turrets; if false, build vehicles
	// Returns true if action taken, false if dead end
	bool EnsureDefenders(FMyCityState& CityState, ACityActor* City, int32 DefendersNeeded, int32 Priority = 1, bool bBuildTurrets = false);
	
	// Ensure resources get captured (send idle vehicles, build vehicles, wait for income)
	// Priority parameter determines which vehicle array to use (1-5)
	// Returns true if action taken, false if dead end
	bool EnsureResourceCapture(const TArray<AResourceActor*>& Resources, ACityActor* OriginCity, int32 Priority = 2);
	
	// Ensure enemy mines get destroyed to free up resources (send idle vehicles, build vehicles)
	// Priority parameter determines which vehicle array to use (1-5)
	// Returns true if action taken, false if dead end
	bool EnsureMineDestruction(const TArray<class AMineActor*>& Mines, ACityActor* OriginCity, int32 Priority = 3);
	
	// AI-AI Trade: Attempt to buy substrate from best trading partner
	// SubstrateType: Which substrate to request (Orange or Black)
	// AmountNeeded: How much substrate to request (typically 1000)
	// Returns true if trade successful, false if no suitable partner found or can't afford
	bool RequestTradeFromAI(bool bRequestingOrange, int32 AmountNeeded = 1000);
	
	// Calculate how many defenders a city needs based on active threat
	// DIFFICULTY TUNING: Increase multiplier for harder AI, decrease for easier
	int32 CalculateDefendersNeeded(const FMyCityState& CityState) const;
	
	// Notify AI that city count has changed (city captured or lost)
	// This logs the new substrate requirements based on updated city count
	void OnCityCountChanged();

	// Apply archetype preset values to personality parameters
	// Called automatically during BeginPlay if Archetype is not Custom
	void ApplyArchetypePreset();

	// ========== CLUSTER EVALUATION ==========
	
	// Get all resource clusters in the world (neutral and enemy)
	TArray<FClusterInfo> GetAllClusters();
	
	// Check if a resource type is scarce (< 2 mines owned)
	bool IsScarceResource(EResourceType Type);
	
	// Find closest friendly city to a location
	ACityActor* GetClosestOwnCity(FVector Location);
	
	// Find closest enemy city to a location
	ACityActor* GetClosestEnemyCity(FVector Location);
	
	// Evaluate all clusters and assign vehicles to capture them
	void EvaluateAndAssignClusters();

	// Collect resource income
	void CollectIncome();
	
	// Consume black substrate for active vehicles
	void ConsumeBlackSubstrate();

	// ========== VEHICLE TRACKING ==========
	
	// Priority-based vehicle tracking (higher priorities can pull from lower ones)
	TArray<AVehicleActor*> Priority1Vehicles;  // Survival - Defending cities under active attack
	TArray<AVehicleActor*> Priority2Vehicles;  // Territory - Claiming nearby resources (personality radius)
	TArray<AVehicleActor*> Priority3Vehicles;  // Expansion - Claiming distant resources (income threshold)
	TArray<AVehicleActor*> Priority4Vehicles;  // Defense - Defensive positions (proximity threat)
	TArray<AVehicleActor*> Priority5Vehicles;  // War - Attacking enemy cities
	TArray<AVehicleActor*> Priority6Vehicles;  // Aid - Helping allies under attack

	// ========== HELPER METHODS ==========
	
	// Helper: Position defensive vehicles near city
	void PositionDefensiveVehicle(AVehicleActor* Vehicle, ACityActor* City, int32 DefenseSlot);

	// Helper: Send vehicle home (to nearest city)
	void SendVehicleHome(AVehicleActor* Vehicle);

	// Helper methods
	TArray<ACityActor*> GetControlledCities() const;
	TArray<AVehicleActor*> GetControlledVehicles();
	TArray<AResourceActor*> GetAvailableResources(EResourceType Type = EResourceType::BlackSubstrate);
	AResourceActor* FindClosestResource(FVector FromLocation, TArray<AResourceActor*> Resources);
	void SendVehicleToResource(AVehicleActor* Vehicle, AResourceActor* Resource);
	bool BuildVehicleAtCity(ACityActor* City, int32 Priority = 0);
	void BuildTurretAtCity(ACityActor* City);
	
	// Helper: Check if a resource's cluster is safe from Kaiju threats (based on KaijuSafetyDistance)
	// Returns true if no living Kaiju is within safety distance of the cluster center
	bool IsClusterSafeFromKaiju(AResourceActor* Resource);
	
	// Get archetype name as string for UI display
	UFUNCTION(BlueprintCallable, Category = "AI")
	FString GetArchetypeName() const;
	
	// Get required orange income (NumCities * 100 * P2_3_IncomeMultiplier)
	UFUNCTION(BlueprintCallable, Category = "AI")
	int32 GetRequiredOrangeIncome() const;
	
	// Get required black income (NumCities * 100 * P2_3_IncomeMultiplier)
	UFUNCTION(BlueprintCallable, Category = "AI")
	int32 GetRequiredBlackIncome() const;
	
	// Purchasing: Try to buy a vehicle if we have 1000 orange substrate
	bool TryPurchaseVehicle(const FString& Purpose);

private:
	// Timer handle for income collection
	FTimerHandle IncomeTimerHandle;
	
	// Timer handle for black substrate consumption
	FTimerHandle ConsumptionTimerHandle;
};
