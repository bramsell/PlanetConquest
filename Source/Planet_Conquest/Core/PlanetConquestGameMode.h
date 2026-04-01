// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Entities/Resources/ResourceActor.h"
#include "PlanetConquestGameMode.generated.h"

/**
 * Relationship tracking between teams
 * Stored globally in GameMode for easy access by both Player and AI controllers
 * 
 * Three independent inputs that combine to determine behavior:
 * - Disposition: Emotional stance from -100 (hate) to +100 (goodwill) - SYMMETRIC
 * - Fear: Threat assessment from 0 (no threat) to 100 (terrified) - ASYMMETRIC (stored separately)
 * - Respect: Competence/reliability estimation from 0 (contempt) to 100 (admiration) - ASYMMETRIC (stored separately)
 * 
 * Disposition Calculation:
 * Total Disposition = BaseDisposition + (TradeCount * 5) + (BribeCount * 10) 
 *                     + (AllianceBonuses.Num() * 40) + (MutualEnemyBonusesApplied.Num() * 20)
 * 
 * Bonus Tracking:
 * - Trade: +5 per trade (repeatable) - tracked via TradeCount
 * - Bribe: +10 per successful bribe (repeatable) - tracked via BribeCount
 * - Alliance: +40 once per ally - tracked via AllianceBonuses set
 * - Common Enemy: +20 once per mutual enemy - tracked via MutualEnemyBonusesApplied set
 * - Negative actions (betrayal, attacks): modify BaseDisposition directly
 */
USTRUCT(BlueprintType)
struct FTeamRelationship
{
	GENERATED_BODY()

	// Base Disposition: Permanent relationship changes from attacks, betrayals, gifts, etc.
	// Range: -100 (bitter enemy) to +100 (close ally)
	// SYMMETRIC: Team A's base disposition toward Team B = Team B's base disposition toward Team A
	UPROPERTY(BlueprintReadWrite, Category = "Relationship")
	float BaseDisposition = 0.0f;

	// Track how many neutral resources this team has taken in our territory (affects disposition)
	UPROPERTY(BlueprintReadWrite, Category = "Relationship")
	int32 NeutralResourcesTakenInTerritory = 0;

	// Track successful trades with this team (+5 relationship per trade, repeatable)
	UPROPERTY(BlueprintReadWrite, Category = "Relationship")
	int32 TradeCount = 0;

	// Track successful bribes given to this team (+10 relationship per bribe, repeatable)
	UPROPERTY(BlueprintReadWrite, Category = "Relationship")
	int32 BribeCount = 0;

	// Track allies that contribute +40 bonus (one-time per ally, removed when alliance ends)
	// Contains teams that are allied with us through shared alliance
	UPROPERTY(BlueprintReadWrite, Category = "Relationship")
	TSet<EOwnerTeam> AllianceBonuses;

	// Track "enemy of my enemy" bonuses given (+20 per mutual enemy, one-time per enemy)
	UPROPERTY(BlueprintReadWrite, Category = "Relationship")
	TSet<EOwnerTeam> MutualEnemyBonusesApplied;

	FTeamRelationship() : BaseDisposition(0.0f), NeutralResourcesTakenInTerritory(0), TradeCount(0), BribeCount(0) {}
	
	// NOTE: Fear and Respect are stored separately in GameMode's maps (asymmetric)
	// Team A's fear of Team B != Team B's fear of Team A
	// Team A's respect for Team B != Team B's respect for Team A
	// Fear is calculated based on military strength differential
	// Respect is earned by demonstrating competence (winning battles, keeping territory)
};

// Trade request from AI to player
USTRUCT(BlueprintType)
struct FAITradeRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	EOwnerTeam RequestingTeam = EOwnerTeam::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	bool bRequestingOrange = false; // false = requesting black substrate

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	int32 AmountRequested = 0; // Amount of substrate AI wants from player

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	int32 AmountOffered = 0; // Amount of opposite substrate AI offers in exchange (if requesting Orange, offers Black; if requesting Black, offers Orange)

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	bool bIsCounterOffer = false; // True if this is a fallback offer after initial decline

	FAITradeRequest() {}

	FAITradeRequest(EOwnerTeam InTeam, bool bInOrange, int32 InRequested, int32 InOffered, bool bInCounterOffer = false)
		: RequestingTeam(InTeam), bRequestingOrange(bInOrange), AmountRequested(InRequested), AmountOffered(InOffered), bIsCounterOffer(bInCounterOffer)
	{}
};

// Alliance request from AI to player
USTRUCT(BlueprintType)
struct FAIAllianceRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Alliance")
	EOwnerTeam RequestingTeam = EOwnerTeam::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Alliance")
	EOwnerTeam CommonEnemy = EOwnerTeam::Neutral; // The mutual enemy prompting this alliance

	FAIAllianceRequest() {}

	FAIAllianceRequest(EOwnerTeam InTeam, EOwnerTeam InEnemy)
		: RequestingTeam(InTeam), CommonEnemy(InEnemy)
	{}
};

// Bribe request from AI to player
USTRUCT(BlueprintType)
struct FAIBribeRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Bribe")
	EOwnerTeam BribingTeam = EOwnerTeam::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Bribe")
	bool bUsingOrangeSubstrate = false; // false = using black substrate

	UPROPERTY(BlueprintReadOnly, Category = "Bribe")
	int32 Amount = 2000; // Amount of substrate being offered

	FAIBribeRequest() {}

	FAIBribeRequest(EOwnerTeam InTeam, bool bInOrange)
		: BribingTeam(InTeam), bUsingOrangeSubstrate(bInOrange)
	{}
};

UCLASS()
class PLANET_CONQUEST_API APlanetConquestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APlanetConquestGameMode();

	virtual void BeginPlay() override;

	// ========== RELATIONSHIP SYSTEM ==========

	// Territory radius around each city (5k units)
	static constexpr float TERRITORY_RADIUS = 5000.0f;

	// Get disposition between two teams (-100 to +100)
	UFUNCTION(BlueprintPure, Category = "Relationships")
	float GetDisposition(EOwnerTeam Team1, EOwnerTeam Team2) const;

	// Modify disposition between two teams (will clamp to -100 to +100)
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void ModifyDisposition(EOwnerTeam Team1, EOwnerTeam Team2, float Delta);

	// Add alliance bonus between two teams (+40 relationship, tracked separately)
	// This is symmetric - adds Team2 to Team1's bonuses AND Team1 to Team2's bonuses
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void AddAllianceBonus(EOwnerTeam Team1, EOwnerTeam Team2);

	// Remove alliance bonus between two teams (removes tracked +40 bonus)
	// This is symmetric - removes Team2 from Team1's bonuses AND Team1 from Team2's bonuses
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void RemoveAllianceBonus(EOwnerTeam Team1, EOwnerTeam Team2);

	// Get fear that Team1 has of Team2 (0 to 100)
	// ASYMMETRIC: Team A's fear of Team B != Team B's fear of Team A
	// Calculated based on military strength - strong teams have low fear, weak teams have high fear
	UFUNCTION(BlueprintPure, Category = "Relationships")
	float GetFear(EOwnerTeam Team1, EOwnerTeam Team2) const;

	// Set fear that Team1 has of Team2 (will clamp to 0 to 100)
	// ASYMMETRIC: Only sets Team1's fear of Team2, does NOT affect Team2's fear of Team1
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void SetFear(EOwnerTeam Team1, EOwnerTeam Team2, float Value);

	// Get respect that Team1 has for Team2 (0 to 100)
	// ASYMMETRIC: Team A's respect for Team B != Team B's respect for Team A
	// Earned by demonstrating strength, keeping promises, defending territory
	UFUNCTION(BlueprintPure, Category = "Relationships")
	float GetRespect(EOwnerTeam Team1, EOwnerTeam Team2) const;

	// Set respect that Team1 has for Team2 (will clamp to 0 to 100)
	// ASYMMETRIC: Only sets Team1's respect for Team2, does NOT affect Team2's respect for Team1
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void SetRespect(EOwnerTeam Team1, EOwnerTeam Team2, float Value);

	// LEGACY: Get relationship (now returns Disposition for backwards compatibility)
	UFUNCTION(BlueprintPure, Category = "Relationships")
	float GetRelationship(EOwnerTeam Team1, EOwnerTeam Team2) const { return GetDisposition(Team1, Team2); }

	// LEGACY: Modify relationship (now modifies Disposition for backwards compatibility)
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void ModifyRelationship(EOwnerTeam Team1, EOwnerTeam Team2, float Delta) { ModifyDisposition(Team1, Team2, Delta); }
	
	// Clean up alliances when a team is eliminated
	void CleanupAlliancesForEliminatedTeam(EOwnerTeam EliminatedTeam);

	// Called when Team1 captures a neutral resource in Team2's territory
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void OnNeutralResourceCapturedInTerritory(EOwnerTeam CapturingTeam, EOwnerTeam TerritoryOwner);

	// Called when Team1 captures Team2's resource
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void OnEnemyResourceCaptured(EOwnerTeam CapturingTeam, EOwnerTeam OriginalOwner);

	// Called when Team1 damages Team2's building
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void OnBuildingDamaged(EOwnerTeam AttackerTeam, EOwnerTeam VictimTeam, float DamagePercent, bool bIsCapitalBuilding, const FString& BuildingType = TEXT("Building"));

	// Called when Team1 destroys Team2's vehicle
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void OnVehicleDestroyed(EOwnerTeam AttackerTeam, EOwnerTeam VictimTeam);

	// Called when Team1 gives money to Team2
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void OnMoneyTransferred(EOwnerTeam GivingTeam, EOwnerTeam ReceivingTeam, int32 Amount);

	// Called when Team1 attacks Team2 - checks for "enemy of my enemy" bonuses with all other teams
	UFUNCTION(BlueprintCallable, Category = "Relationships")
	void OnTeamAttackedTeam(EOwnerTeam AttackerTeam, EOwnerTeam VictimTeam);

	// Get hostility threshold for a team based on their personality (aggressive teams fire earlier)
	// Returns relationship value at which auto-firing begins
	UFUNCTION(BlueprintPure, Category = "Relationships")
	float GetHostilityThreshold(EOwnerTeam Team) const;

	// Count how many cities a team owns
	UFUNCTION(BlueprintPure, Category = "Relationships")
	int32 CountCitiesForTeam(EOwnerTeam Team) const;

protected:
	// Relationship storage for symmetric values (Disposition only)
	// Key is pair of teams (order doesn't matter), Value is relationship data
	// Team1-Team2 relationship is same as Team2-Team1 relationship (symmetric)
	TMap<uint32, FTeamRelationship> Relationships;

	// Fear storage (asymmetric - Team1's fear of Team2 != Team2's fear of Team1)
	// Key is directional: (Team1 << 16) | Team2
	TMap<uint32, float> FearValues;

	// Respect storage (asymmetric - Team1's respect for Team2 != Team2's respect for Team1)
	// Key is directional: (Team1 << 16) | Team2
	TMap<uint32, float> RespectValues;

	// Helper to create consistent key for team pairs (always smaller enum value first) - for symmetric values (Disposition)
	static uint32 MakeRelationshipKey(EOwnerTeam Team1, EOwnerTeam Team2);

	// Helper to create directional key for asymmetric values like Fear and Respect (Team1 -> Team2)
	static uint32 MakeDirectionalKey(EOwnerTeam FromTeam, EOwnerTeam ToTeam);

	// Initialize all team relationships to 0.0 at game start
	void InitializeRelationships();

public:
	// ========== AI-PLAYER TRADE REQUEST SYSTEM ==========

	// Trade cooldown in seconds (10 income cycles = 50 seconds)
	static constexpr float TRADE_COOLDOWN_SECONDS = 50.0f;

	// Track last trade time with each AI team (uses GetWorld()->GetTimeSeconds())
	UPROPERTY()
	TMap<EOwnerTeam, float> LastPlayerTradeTime;

	// Check if player can trade with a specific team (cooldown expired)
	UFUNCTION(BlueprintPure, Category = "Trade")
	bool CanPlayerTradeWithTeam(EOwnerTeam Team) const;

	// Get remaining cooldown time in seconds (returns 0.0 if can trade)
	UFUNCTION(BlueprintPure, Category = "Trade")
	float GetTradeCooldownRemaining(EOwnerTeam Team) const;

	// Mark that a trade was initiated with a team (starts cooldown)
	UFUNCTION(BlueprintCallable, Category = "Trade")
	void MarkTradeInitiated(EOwnerTeam Team);

	// Queue of pending AI trade requests (AI teams wanting to trade with player)
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	TArray<FAITradeRequest> PendingAITradeRequests;

	// Add a trade request from an AI team to the queue
	UFUNCTION(BlueprintCallable, Category = "Trade")
	void QueueAITradeRequest(EOwnerTeam RequestingTeam, bool bRequestingOrange, int32 AmountRequested, int32 AmountOffered);

	// Process the next trade request in the queue (opens TalkDialogue with player)
	UFUNCTION(BlueprintCallable, Category = "Trade")
	void ProcessNextTradeRequest();

	// Accept the current trade request (called from UI)
	UFUNCTION(BlueprintCallable, Category = "Trade")
	bool AcceptCurrentTradeRequest();

	// Decline the current trade request (called from UI)
	UFUNCTION(BlueprintCallable, Category = "Trade")
	void DeclineCurrentTradeRequest();

	// Get the current active trade request (if any)
	UFUNCTION(BlueprintPure, Category = "Trade")
	FAITradeRequest GetCurrentTradeRequest() const;

	// Check if there's an active trade request being displayed
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	bool bHasActiveTradeRequest = false;

	// Check if there's an active bribe request being displayed
	UPROPERTY(BlueprintReadOnly, Category = "Diplomacy")
	bool bHasActiveBribeRequest = false;

	// ========== AI-PLAYER ALLIANCE REQUEST SYSTEM ==========

	// Queue of pending AI alliance requests (AI teams wanting to ally with player)
	UPROPERTY(BlueprintReadOnly, Category = "Alliance")
	TArray<FAIAllianceRequest> PendingAIAllianceRequests;

	// Add an alliance request from an AI team to the queue
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	void QueueAIAllianceRequest(EOwnerTeam RequestingTeam, EOwnerTeam CommonEnemy);

	// Process the next alliance request in the queue (opens TalkDialogue with player)
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	void ProcessNextAllianceRequest();

	// Accept the current alliance request (called from UI)
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	bool AcceptCurrentAllianceRequest();

	// Decline the current alliance request (called from UI)
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	void DeclineCurrentAllianceRequest();

	// Player requests an alliance with an AI team
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	bool PlayerRequestAlliance(EOwnerTeam TargetTeam, EOwnerTeam CommonEnemy);

	// Player ends an alliance with an AI team
	UFUNCTION(BlueprintCallable, Category = "Alliance")
	bool PlayerEndAlliance(EOwnerTeam TargetTeam);

	// Check if the player is allied with a specific team
	UFUNCTION(BlueprintPure, Category = "Alliance")
	bool IsPlayerAlliedWith(EOwnerTeam TargetTeam) const;

	// Get the current active alliance request (if any)
	UFUNCTION(BlueprintPure, Category = "Alliance")
	FAIAllianceRequest GetCurrentAllianceRequest() const;

	// Check if there's an active alliance request being displayed
	UPROPERTY(BlueprintReadOnly, Category = "Alliance")
	bool bHasActiveAllianceRequest = false;

	// Get the player's current alliance name (returns "No Allegiance" if not in an alliance)
	UFUNCTION(BlueprintPure, Category = "Alliance")
	FString GetPlayerAllianceName() const;

	// Get the alliance name for a specific team (returns "No Allegiance" if not in an alliance)
	UFUNCTION(BlueprintPure, Category = "Alliance")
	FString GetTeamAllianceName(EOwnerTeam Team) const;

	// Alliance names pool (randomly assigned when alliances form)
	UPROPERTY()
	TArray<FString> AllianceNames;

	// Player's current alliance name (empty if no alliance)
	UPROPERTY(BlueprintReadOnly, Category = "Alliance")
	FString PlayerAllianceName;

	// Get a random alliance name from the pool
	FString GetRandomAllianceName();

	// ========== ALLIANCE MANAGEMENT HELPERS ==========
	
	// Get all teams currently in a specific alliance (including player if applicable)
	TSet<EOwnerTeam> GetAllTeamsInAlliance(const FString& AllianceName);
	
	// Add a team to an existing alliance (handles all cross-linking and validation)
	// Returns true if successful, false if validation fails (e.g., enemies in same alliance)
	bool AddTeamToAlliance(EOwnerTeam NewTeam, const FString& AllianceName);
	
	// Form a new alliance between two teams (creates new alliance name)
	// Returns the alliance name if successful, empty string if failed
	FString FormNewAlliance(EOwnerTeam Team1, EOwnerTeam Team2);
	
	// Check if two teams are enemies (disposition < 0.0)
	bool AreTeamsEnemies(EOwnerTeam Team1, EOwnerTeam Team2) const;
	
	// Validate and fix alliance integrity (removes enemies from same alliance)
	void ValidateAllianceIntegrity();

	// Propagate hatred between alliances (if any member hates another alliance member, all hate)
	void PropagateAllianceHatred();

	// ========== BRIBE SYSTEM ==========
	
	// Bribe cooldown in seconds (10 income cycles = 50 seconds)
	static constexpr float BRIBE_COOLDOWN_SECONDS = 50.0f;
	
	// Track last bribe time with each AI team (uses GetWorld()->GetTimeSeconds())
	// For player->AI bribes only
	UPROPERTY()
	TMap<EOwnerTeam, float> LastPlayerBribeTime;
	
	// Track last bribe time for AI teams (uses GetWorld()->GetTimeSeconds())
	// Key is MakeRelationshipKey(BribingTeam, TargetTeam)
	UPROPERTY()
	TMap<uint32, float> LastAIBribeTime;
	
	// Check if player can bribe a specific team (cooldown expired)
	UFUNCTION(BlueprintPure, Category = "Diplomacy")
	bool CanPlayerBribeTeam(EOwnerTeam Team) const;
	
	// Get remaining bribe cooldown time in seconds (returns 0.0 if can bribe)
	UFUNCTION(BlueprintPure, Category = "Diplomacy")
	float GetBribeCooldownRemaining(EOwnerTeam Team) const;
	
	// Attempt to bribe a team with substrate (2000 orange or black)
	// Returns true if successful (65% chance), false otherwise
	// Also returns false if on cooldown or insufficient resources
	UFUNCTION(BlueprintCallable, Category = "Diplomacy")
	bool AttemptBribe(EOwnerTeam TargetTeam, bool bUseOrangeSubstrate);
	
	// AI-initiated bribe attempt (uses AI's substrate stockpile)
	// BribingTeam is the AI team sending the bribe
	// Returns true if successful (65% chance), false otherwise
	bool AIAttemptBribe(EOwnerTeam BribingTeam, EOwnerTeam TargetTeam, bool bUseOrangeSubstrate, int32& OutOrangeSubstrate, int32& OutBlackSubstrate);
	
	// Check if an AI team can bribe another team (cooldown expired)
	bool CanAIBribeTeam(EOwnerTeam BribingTeam, EOwnerTeam TargetTeam) const;
	
	// Get remaining AI bribe cooldown time in seconds (returns 0.0 if can bribe)
	float GetAIBribeCooldownRemaining(EOwnerTeam BribingTeam, EOwnerTeam TargetTeam) const;
	
	// Queue AI bribe request for player to respond
	void QueueAIBribeRequest(EOwnerTeam BribingTeam, bool bUseOrangeSubstrate);
	
	// Accept current AI bribe request with thankful response (+10 relationship)
	UFUNCTION(BlueprintCallable, Category = "Diplomacy")
	bool AcceptCurrentBribeRequestThankfully();
	
	// Accept current AI bribe request with suspicious response (-10 relationship)
	UFUNCTION(BlueprintCallable, Category = "Diplomacy")
	bool AcceptCurrentBribeRequestSuspiciously();
	
	// Decline current AI bribe request (AI keeps substrate, no relationship change)
	UFUNCTION(BlueprintCallable, Category = "Diplomacy")
	void DeclineCurrentBribeRequest();
	
	// Get current bribe request for UI display
	UFUNCTION(BlueprintPure, Category = "Diplomacy")
	FAIBribeRequest GetCurrentBribeRequest() const;

private:
	// Currently active trade request being shown to player
	FAITradeRequest CurrentTradeRequest;

	// Currently active alliance request being shown to player
	FAIAllianceRequest CurrentAllianceRequest;
	
	// Currently active bribe request being shown to player
	FAIBribeRequest CurrentBribeRequest;

	// Flag to prevent recursion during alliance hatred propagation
	bool bIsPropagatingHatred = false;
};
