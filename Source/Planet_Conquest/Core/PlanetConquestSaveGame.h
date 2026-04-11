// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlanetConquestSaveGame.generated.h"

// ---------------------------------------------------------------------------
// World-state save structs
// All enums are stored as uint8 to avoid header dependencies in this file.
// ---------------------------------------------------------------------------

/** Player economy and mode settings. */
USTRUCT()
struct FPlayerStateSave
{
	GENERATED_BODY()
	UPROPERTY() int32 OrangeSubstrate = 2000;
	UPROPERTY() int32 BlackSubstrate  = 2000;
	UPROPERTY() int32 GreenSubstrate  = 100;
	UPROPERTY() uint8 BlackSubstrateMode = 1; // EBlackSubstrateMode::Efficient
	UPROPERTY() uint8 MiningMode         = 0; // EMiningMode::Aggressive
	UPROPERTY() bool  bVehicleAutopilotMode = true;
	UPROPERTY() TArray<uint8> DiscoveredResourceTypes; // EResourceType as uint8
};

/** Economy + diplomacy state for one AI team. */
USTRUCT()
struct FAITeamStateSave
{
	GENERATED_BODY()
	UPROPERTY() uint8 Team = 0;
	UPROPERTY() int32 OrangeSubstrate = 2000;
	UPROPERTY() int32 BlackSubstrate  = 2000;
	UPROPERTY() int32 CurrentDecisionCycle = 0;
	// Trade / bribe cooldowns (parallel arrays — TMap is not safely serialisable)
	UPROPERTY() TArray<uint8> TradeCooldownTeams;
	UPROPERTY() TArray<int32> TradeCooldownCycles;
	UPROPERTY() TArray<uint8> BribeCooldownTeams;
	UPROPERTY() TArray<int32> BribeCooldownCycles;
	// Alliance state
	UPROPERTY() TArray<uint8> AlliedTeams;
	UPROPERTY() FString AllianceName;
	UPROPERTY() TArray<uint8> AllianceReqCooldownTeams;
	UPROPERTY() TArray<int32> AllianceReqCooldownCycles;
};

/** City state — identified by world location. */
USTRUCT()
struct FCityStateSave
{
	GENERATED_BODY()
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() uint8   OwnerTeam = 0;
	UPROPERTY() float   CurrentHealth = 100.f;
	UPROPERTY() int32   Population = 0;
	UPROPERTY() int32   GreenSubstrate = 100;
};

/** Resource node state — identified by world location. */
USTRUCT()
struct FResourceStateSave
{
	GENERATED_BODY()
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() uint8   OwnerTeam = 0;
	UPROPERTY() float   ResourceHealth = 100.f;
	UPROPERTY() float   CurrentInfluence = 0.f;
};

/** Building state — identified by world location. */
USTRUCT()
struct FBuildingStateSave
{
	GENERATED_BODY()
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() uint8   OwnerTeam = 0;
	UPROPERTY() float   CurrentHealth = 100.f;
	UPROPERTY() uint8   BuildingType = 0; // EBuildingType as uint8
};

/** Vehicle / ship state — re-spawned at load time. */
USTRUCT()
struct FVehicleStateSave
{
	GENERATED_BODY()
	UPROPERTY() FVector  Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() uint8    OwnerTeam = 0;
	UPROPERTY() float    CurrentHealth = 100.f;
	UPROPERTY() bool     bIsShip = false;

	// --- Player vehicle targeting state ---
	// Actor references are stored as world locations for nearest-match restoration.
	// FVector::ZeroVector means "not set".
	UPROPERTY() bool    bPlayerAutonomousMode = false;
	UPROPERTY() bool    bClusterOnlyMode      = false;
	UPROPERTY() int32   ActiveClusterID       = -1;

	// Location of the TargetResource (resource being captured directly)
	UPROPERTY() FVector TargetResourceLocation = FVector::ZeroVector;
	// Location of the PostMineResource (resource to capture after mine destroyed)
	UPROPERTY() FVector PostMineResourceLocation = FVector::ZeroVector;
	// Location of CurrentTarget if it's a mine (for mine-attack tasks)
	UPROPERTY() FVector CurrentTargetMineLocation = FVector::ZeroVector;

	// Teams whose mines we are ordered to destroy
	UPROPERTY() TArray<uint8> ForcedHostileTeams;
};

/** Symmetric relationship (Disposition) between two teams. */
USTRUCT()
struct FRelationshipSave
{
	GENERATED_BODY()
	UPROPERTY() uint8 Team1 = 0;
	UPROPERTY() uint8 Team2 = 0;
	UPROPERTY() float BaseDisposition = 0.f;
	UPROPERTY() int32 NeutralResourcesTaken = 0;
	UPROPERTY() int32 TradeCount = 0;
	UPROPERTY() int32 BribeCount = 0;
	UPROPERTY() TArray<uint8> AllianceBonuses;
	UPROPERTY() TArray<uint8> MutualEnemyBonuses;
};

/** Asymmetric Fear + Respect from one team toward another. */
USTRUCT()
struct FAsymmetricRelSave
{
	GENERATED_BODY()
	UPROPERTY() uint8 FromTeam = 0;
	UPROPERTY() uint8 ToTeam   = 0;
	UPROPERTY() float Fear    = 0.f;
	UPROPERTY() float Respect = 0.f;
};

// ---------------------------------------------------------------------------

/**
 * Persistent save data for Planet Conquest.
 *
 * Lifecycle:
 *   - Created once on first launch (PlanetActor::BeginPlay).
 *   - PlanetSeed is written immediately and never changes for this save slot.
 *   - On every subsequent load, the planet is regenerated deterministically
 *     from PlanetSeed, so terrain/coastlines never need to be serialised.
 *   - bWorldStateValid becomes true after the first in-game save (pause menu).
 *     Saves without world state (e.g. from the first launch) will not attempt
 *     to restore entity state.
 */
UCLASS()
class PLANET_CONQUEST_API UPlanetConquestSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Planet generation
	// -----------------------------------------------------------------------

	/** Seed used for all deterministic planet generation (Voronoi, continent
	 *  placement, resource clustering).  Set once on first launch; constant
	 *  for the lifetime of this save slot. */
	UPROPERTY(VisibleAnywhere, Category = "Planet")
	int32 PlanetSeed = 0;

	/** True once the seed has been written for the first time. */
	UPROPERTY()
	bool bSeedInitialised = false;

	// -----------------------------------------------------------------------
	// Save-slot metadata
	// -----------------------------------------------------------------------

	/** Human-readable slot name shown in the load-game UI (e.g. "Slot 1"). */
	UPROPERTY(VisibleAnywhere, Category = "Meta")
	FString SlotDisplayName = TEXT("New Game");

	/** Wall-clock time when this slot was last saved. */
	UPROPERTY(VisibleAnywhere, Category = "Meta")
	FDateTime LastSaved;

	/** Total seconds spent in this save slot. Incremented while the game is
	 *  running; written to disk on save. Displayed in the main menu as hours. */
	UPROPERTY(VisibleAnywhere, Category = "Meta")
	int64 PlaytimeSeconds = 0;

	// -----------------------------------------------------------------------
	// World state (populated by APlanetConquestGameMode::CollectWorldState)
	// -----------------------------------------------------------------------

	/** Guard flag — false for saves that pre-date world-state serialisation.
	 *  ApplyWorldState is skipped when this is false. */
	UPROPERTY()
	bool bWorldStateValid = false;

	UPROPERTY() FPlayerStateSave             PlayerState;
	UPROPERTY() TArray<FAITeamStateSave>     AITeamStates;
	UPROPERTY() TArray<FCityStateSave>       CityStates;
	UPROPERTY() TArray<FResourceStateSave>   ResourceStates;
	UPROPERTY() TArray<FBuildingStateSave>   BuildingStates;
	UPROPERTY() TArray<FVehicleStateSave>    VehicleStates;
	UPROPERTY() TArray<FRelationshipSave>    RelationshipStates;
	UPROPERTY() TArray<FAsymmetricRelSave>   AsymmetricRelStates;
};

