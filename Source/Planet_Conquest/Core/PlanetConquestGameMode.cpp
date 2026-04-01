// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PlanetConquestGameMode.h"
#include "PlanetCameraPawn.h"
#include "PlanetConquestPlayerController.h"
#include "PlanetConquestHUD.h"
#include "AITeamController.h"
#include "EngineUtils.h"
#include "../Entities/Cities/CityActor.h"
#include "../UI/TalkDialogueWidget.h"
#include "Kismet/GameplayStatics.h"

APlanetConquestGameMode::APlanetConquestGameMode()
{
	// Set default pawn class to our camera pawn
	DefaultPawnClass = APlanetCameraPawn::StaticClass();
	
	// Set player controller class
	PlayerControllerClass = APlanetConquestPlayerController::StaticClass();
	
	// Set HUD class for box selection rendering
	HUDClass = APlanetConquestHUD::StaticClass();

	// Initialize alliance name pool
	AllianceNames.Add(TEXT("North Star Alliance"));
	AllianceNames.Add(TEXT("Eastern Rim Alliance"));
	AllianceNames.Add(TEXT("Crimson Pact"));
	AllianceNames.Add(TEXT("Void Coalition"));
	AllianceNames.Add(TEXT("Stellar Accord"));

	// Player starts with no alliance
	PlayerAllianceName = TEXT("");
}

void APlanetConquestGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize all team relationships to 0.0 (neutral)
	InitializeRelationships();
}

// ========== RELATIONSHIP SYSTEM IMPLEMENTATION ==========

uint32 APlanetConquestGameMode::MakeRelationshipKey(EOwnerTeam Team1, EOwnerTeam Team2)
{
	// Ensure smaller value is always first for consistent symmetric key
	uint8 A = static_cast<uint8>(Team1);
	uint8 B = static_cast<uint8>(Team2);
	if (A > B)
	{
		Swap(A, B);
	}
	// Pack two uint8s into uint32 (A in lower 16 bits, B in upper 16 bits)
	return (static_cast<uint32>(A) | (static_cast<uint32>(B) << 16));
}

uint32 APlanetConquestGameMode::MakeDirectionalKey(EOwnerTeam FromTeam, EOwnerTeam ToTeam)
{
	// Directional key: FromTeam -> ToTeam (order matters)
	uint8 From = static_cast<uint8>(FromTeam);
	uint8 To = static_cast<uint8>(ToTeam);
	// Pack: From in lower 16 bits, To in upper 16 bits
	return (static_cast<uint32>(From) | (static_cast<uint32>(To) << 16));
}

void APlanetConquestGameMode::InitializeRelationships()
{
	Relationships.Empty();
	FearValues.Empty();
	RespectValues.Empty();
	
	// All teams start with neutral relationships:
	// Disposition = 0 (neutral) - SYMMETRIC
	// Fear = 0 (no fear) - ASYMMETRIC
	// Respect = 0 (no respect) - ASYMMETRIC
	UE_LOG(LogTemp, Log, TEXT("[RELATIONSHIPS] Initialized - all teams start neutral (Disposition=0, Fear=0, Respect=0)"));
}

// ========== DISPOSITION ==========

float APlanetConquestGameMode::GetDisposition(EOwnerTeam Team1, EOwnerTeam Team2) const
{
	if (Team1 == Team2)
	{
		return 100.0f; // Team always has perfect disposition with itself
	}
	
	if (Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return 0.0f; // Neutral team has no relationships
	}
	
	uint32 Key = MakeRelationshipKey(Team1, Team2);
	const FTeamRelationship* Found = Relationships.Find(Key);
	
	// Calculate total disposition from all tracked bonuses
	float TotalDisposition = 0.0f;
	
	if (Found)
	{
		TotalDisposition = Found->BaseDisposition;
		
		// Trade bonus: +5 per trade (repeatable)
		TotalDisposition += Found->TradeCount * 5.0f;
		
		// Bribe bonus: +10 per successful bribe (repeatable)
		TotalDisposition += Found->BribeCount * 10.0f;
		
		// Alliance bonus: +40 per ally (one-time, removed when alliance ends)
		TotalDisposition += Found->AllianceBonuses.Num() * 40.0f;
		
		// Common enemy bonus: +20 per mutual enemy (one-time per enemy)
		TotalDisposition += Found->MutualEnemyBonusesApplied.Num() * 20.0f;
	}
	
	// Apply city count penalty for both teams (each additional city beyond first = -10)
	// This is ALWAYS applied, even if teams haven't interacted yet
	int32 Team1Cities = CountCitiesForTeam(Team1);
	int32 Team2Cities = CountCitiesForTeam(Team2);
	float Team1Penalty = (Team1Cities - 1) * -10.0f;
	float Team2Penalty = (Team2Cities - 1) * -10.0f;
	
	// Apply all modifiers
	return TotalDisposition + Team1Penalty + Team2Penalty;
}

void APlanetConquestGameMode::ModifyDisposition(EOwnerTeam Team1, EOwnerTeam Team2, float Delta)
{
	if (Team1 == Team2 || Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return; // Can't change relationship with self or neutral
	}
	
	uint32 Key = MakeRelationshipKey(Team1, Team2);
	FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
	
	float OldBaseValue = Rel.BaseDisposition;
	float OldTotalValue = GetDisposition(Team1, Team2);
	Rel.BaseDisposition = FMath::Clamp(Rel.BaseDisposition + Delta, -100.0f, 100.0f);
	float NewTotalValue = GetDisposition(Team1, Team2);
	
	UE_LOG(LogTemp, Log, TEXT("[DISPOSITION] %d <-> %d: Base %.1f -> %.1f (delta: %.1f), Total %.1f -> %.1f"),
		(int32)Team1, (int32)Team2, OldBaseValue, Rel.BaseDisposition, Delta, OldTotalValue, NewTotalValue);

	// Propagate hatred between alliances if not already doing so
	if (!bIsPropagatingHatred && Rel.BaseDisposition < -20.0f)
	{
		PropagateAllianceHatred();
	}
}

void APlanetConquestGameMode::AddAllianceBonus(EOwnerTeam Team1, EOwnerTeam Team2)
{
	if (Team1 == Team2 || Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return; // Can't add alliance bonus with self or neutral
	}
	
	uint32 Key = MakeRelationshipKey(Team1, Team2);
	FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
	
	// Add each team to the other's alliance bonus set (if not already there)
	bool bAlreadyHadBonus = Rel.AllianceBonuses.Contains(Team1) || Rel.AllianceBonuses.Contains(Team2);
	
	Rel.AllianceBonuses.Add(Team1);
	Rel.AllianceBonuses.Add(Team2);
	
	if (!bAlreadyHadBonus)
	{
		UE_LOG(LogTemp, Display, TEXT("[ALLIANCE BONUS] Team %d and Team %d: +40 relationship bonus added (Alliance formed)"),
			(int32)Team1, (int32)Team2);
	}
}

void APlanetConquestGameMode::RemoveAllianceBonus(EOwnerTeam Team1, EOwnerTeam Team2)
{
	if (Team1 == Team2 || Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return; // Can't remove alliance bonus with self or neutral
	}
	
	uint32 Key = MakeRelationshipKey(Team1, Team2);
	FTeamRelationship* Rel = Relationships.Find(Key);
	
	if (Rel)
	{
		// Remove each team from the other's alliance bonus set
		bool bHadTeam1 = Rel->AllianceBonuses.Remove(Team1) > 0;
		bool bHadTeam2 = Rel->AllianceBonuses.Remove(Team2) > 0;
		
		if (bHadTeam1 || bHadTeam2)
		{
			UE_LOG(LogTemp, Display, TEXT("[ALLIANCE BONUS] Team %d and Team %d: -40 relationship bonus removed (Alliance ended)"),
				(int32)Team1, (int32)Team2);
		}
	}
}

// ========== FEAR (ASYMMETRIC) ==========

float APlanetConquestGameMode::GetFear(EOwnerTeam Team1, EOwnerTeam Team2) const
{
	if (Team1 == Team2 || Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return 0.0f; // No fear of self or neutral
	}
	
	// Use directional key: Team1's fear of Team2
	uint32 Key = MakeDirectionalKey(Team1, Team2);
	const float* Found = FearValues.Find(Key);
	return Found ? *Found : 0.0f;
}

void APlanetConquestGameMode::SetFear(EOwnerTeam Team1, EOwnerTeam Team2, float Value)
{
	if (Team1 == Team2 || Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return; // Can't have fear of self or neutral
	}
	
	// Use directional key: Team1's fear of Team2 (asymmetric)
	uint32 Key = MakeDirectionalKey(Team1, Team2);
	float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	FearValues.Add(Key, ClampedValue);
	
	UE_LOG(LogTemp, Log, TEXT("[FEAR] Team %d's fear of Team %d set to %.1f"),
		(int32)Team1, (int32)Team2, ClampedValue);
}

// ========== RESPECT (ASYMMETRIC) ==========

float APlanetConquestGameMode::GetRespect(EOwnerTeam Team1, EOwnerTeam Team2) const
{
	if (Team1 == Team2)
	{
		return 100.0f; // Team always has full respect for itself
	}
	
	if (Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return 0.0f; // No respect for/from neutral
	}
	
	// Use directional key: Team1's respect for Team2
	uint32 Key = MakeDirectionalKey(Team1, Team2);
	const float* Found = RespectValues.Find(Key);
	return Found ? *Found : 0.0f;
}

void APlanetConquestGameMode::SetRespect(EOwnerTeam Team1, EOwnerTeam Team2, float Value)
{
	if (Team1 == Team2 || Team1 == EOwnerTeam::Neutral || Team2 == EOwnerTeam::Neutral)
	{
		return; // Can't set respect for self or neutral
	}
	
	// Use directional key: Team1's respect for Team2 (asymmetric)
	uint32 Key = MakeDirectionalKey(Team1, Team2);
	float ClampedValue = FMath::Clamp(Value, 0.0f, 100.0f);
	RespectValues.Add(Key, ClampedValue);
	
	UE_LOG(LogTemp, Log, TEXT("[RESPECT] Team %d's respect for Team %d set to %.1f"),
		(int32)Team1, (int32)Team2, ClampedValue);
}

void APlanetConquestGameMode::OnNeutralResourceCapturedInTerritory(EOwnerTeam CapturingTeam, EOwnerTeam TerritoryOwner)
{
	if (CapturingTeam == TerritoryOwner || CapturingTeam == EOwnerTeam::Neutral || TerritoryOwner == EOwnerTeam::Neutral)
	{
		return;
	}
	
	uint32 Key = MakeRelationshipKey(CapturingTeam, TerritoryOwner);
	FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
	
	// Only apply penalty if we haven't reached the cap (5 resources = -50 disposition total)
	if (Rel.NeutralResourcesTakenInTerritory < 5)
	{
		Rel.NeutralResourcesTakenInTerritory++;
		ModifyDisposition(CapturingTeam, TerritoryOwner, -10.0f);
		
		UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d captured neutral resource in Team %d's territory (%d/5) - relationship worsened"),
			(int32)CapturingTeam, (int32)TerritoryOwner, Rel.NeutralResourcesTakenInTerritory);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d captured neutral resource in Team %d's territory (cap reached 5/5)"),
			(int32)CapturingTeam, (int32)TerritoryOwner);
	}
}

void APlanetConquestGameMode::OnEnemyResourceCaptured(EOwnerTeam CapturingTeam, EOwnerTeam OriginalOwner)
{
	if (CapturingTeam == OriginalOwner || OriginalOwner == EOwnerTeam::Neutral)
	{
		return;
	}
	
	// -20 disposition per enemy resource captured, can go to -100
	ModifyDisposition(CapturingTeam, OriginalOwner, -20.0f);
	
	//UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d captured Team %d's resource - relationship worsened (-20)"),
	//	(int32)CapturingTeam, (int32)OriginalOwner);
}

void APlanetConquestGameMode::OnBuildingDamaged(EOwnerTeam AttackerTeam, EOwnerTeam VictimTeam, float DamagePercent, bool bIsCapitalBuilding, const FString& BuildingType)
{
	if (AttackerTeam == VictimTeam || AttackerTeam == EOwnerTeam::Neutral || VictimTeam == EOwnerTeam::Neutral)
	{
		return;
	}
	
	// CHECK FOR ALLIANCE BETRAYAL (PLAYER)
	if (AttackerTeam == EOwnerTeam::Player && !PlayerAllianceName.IsEmpty())
	{
		TSet<EOwnerTeam> AllianceMembers = GetAllTeamsInAlliance(PlayerAllianceName);
		if (AllianceMembers.Contains(VictimTeam))
		{
			// Player attacked their own ally - BETRAYAL!
			UE_LOG(LogTemp, Error, TEXT("[BETRAYAL] Player attacked ally Team %d! Kicking from alliance '%s'"),
				(int32)VictimTeam, *PlayerAllianceName);
			
			// Remove alliance bonuses and apply betrayal penalties to all alliance members
			for (EOwnerTeam Member : AllianceMembers)
			{
				if (Member != EOwnerTeam::Player)
				{
					// Remove +40 alliance bonus
					RemoveAllianceBonus(EOwnerTeam::Player, Member);
					
					// Apply -20 betrayal penalty to BaseDisposition
					ModifyDisposition(EOwnerTeam::Player, Member, -20.0f);
					
					UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d condemns Player for betraying the alliance (lost +40 bonus, -20 penalty = -60 total)"),
						(int32)Member);
				}
			}
			
			// Kick player from alliance
			FString OldAllianceName = PlayerAllianceName;
			PlayerAllianceName = TEXT("");
			
			// Remove player from all AI controllers' allied lists
			UWorld* World = GetWorld();
			if (World)
			{
				for (TActorIterator<AAITeamController> It(World); It; ++It)
				{
					if (It->AllianceState.AllianceName == OldAllianceName)
					{
						It->AllianceState.AlliedTeams.Remove(EOwnerTeam::Player);
					}
				}
			}
			
			UE_LOG(LogTemp, Warning, TEXT("[FEED] Player has been EXPELLED from '%s' for attacking an ally!"),
				*OldAllianceName);
			return; // Don't process normal damage relationship change
		}
	}
	
	// CHECK FOR ALLIANCE BETRAYAL (AI TEAMS)
	UWorld* World = GetWorld();
	if (World && AttackerTeam != EOwnerTeam::Player)
	{
		// Find the attacker's AI controller
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == AttackerTeam && !It->AllianceState.AllianceName.IsEmpty())
			{
				// Attacker is in an alliance - check if victim is also in it
				TSet<EOwnerTeam> AllianceMembers = GetAllTeamsInAlliance(It->AllianceState.AllianceName);
				if (AllianceMembers.Contains(VictimTeam))
				{
					// AI team attacked their own ally - BETRAYAL!
					UE_LOG(LogTemp, Error, TEXT("[BETRAYAL] Team %d attacked ally Team %d! Kicking from alliance '%s'"),
						(int32)AttackerTeam, (int32)VictimTeam, *It->AllianceState.AllianceName);
					
					// Remove alliance bonuses and apply betrayal penalties to all alliance members
					for (EOwnerTeam Member : AllianceMembers)
					{
						if (Member != AttackerTeam)
						{
							// Remove +40 alliance bonus
							RemoveAllianceBonus(AttackerTeam, Member);
							
							// Apply -20 betrayal penalty to BaseDisposition
							ModifyDisposition(AttackerTeam, Member, -20.0f);
							
							UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d condemns Team %d for betraying the alliance (lost +40 bonus, -20 penalty = -60 total)"),
								(int32)Member, (int32)AttackerTeam);
						}
					}
					
					// Kick attacker from alliance
					FString OldAllianceName = It->AllianceState.AllianceName;
					It->AllianceState.AllianceName = TEXT("");
					It->AllianceState.AlliedTeams.Empty();
					
					// Remove attacker from all other AI controllers' allied lists
					for (TActorIterator<AAITeamController> OtherIt(World); OtherIt; ++OtherIt)
					{
						if (OtherIt->AllianceState.AllianceName == OldAllianceName)
						{
							OtherIt->AllianceState.AlliedTeams.Remove(AttackerTeam);
						}
					}
					
					// Remove from player's alliance if player is in same alliance
					if (PlayerAllianceName == OldAllianceName)
					{
						// Don't kick player, just update the alliance roster
					}
					
					UE_LOG(LogTemp, Warning, TEXT("[FEED] Team %d has been EXPELLED from '%s' for attacking an ally!"),
						(int32)AttackerTeam, *OldAllianceName);
					return; // Don't process normal damage relationship change
				}
				break;
			}
		}
	}
	
	float Delta = 0.0f;
	
	if (bIsCapitalBuilding)
	{
		// Capital building: -10 disposition per 1% damage
		Delta = -10.0f * DamagePercent;
	}
	else
	{
		// Other buildings: -1 disposition per 1% damage
		Delta = -1.0f * DamagePercent;
	}
	
	ModifyDisposition(AttackerTeam, VictimTeam, Delta);
	
	if (bIsCapitalBuilding || FMath::Abs(Delta) >= 10.0f)
	{
		UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d damaged Team %d's %s (%.0f%% damage) - relationship worsened"),
			(int32)AttackerTeam, (int32)VictimTeam, *BuildingType, DamagePercent);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[RELATIONSHIPS] Team %d damaged Team %d's %s (%.1f%% damage = %.3f relationship change)"),
			(int32)AttackerTeam, (int32)VictimTeam, *BuildingType, DamagePercent, Delta);
	}
}

void APlanetConquestGameMode::OnVehicleDestroyed(EOwnerTeam AttackerTeam, EOwnerTeam VictimTeam)
{
	if (AttackerTeam == VictimTeam || AttackerTeam == EOwnerTeam::Neutral || VictimTeam == EOwnerTeam::Neutral)
	{
		return;
	}
	
	// CHECK FOR ALLIANCE BETRAYAL (PLAYER)
	if (AttackerTeam == EOwnerTeam::Player && !PlayerAllianceName.IsEmpty())
	{
		TSet<EOwnerTeam> AllianceMembers = GetAllTeamsInAlliance(PlayerAllianceName);
		if (AllianceMembers.Contains(VictimTeam))
		{
			// Player attacked their own ally - BETRAYAL!
			UE_LOG(LogTemp, Error, TEXT("[BETRAYAL] Player destroyed ally Team %d's vehicle! Kicking from alliance '%s'"),
				(int32)VictimTeam, *PlayerAllianceName);
			
			// Remove alliance bonuses and apply betrayal penalties to all alliance members
			for (EOwnerTeam Member : AllianceMembers)
			{
				if (Member != EOwnerTeam::Player)
				{
					// Remove +40 alliance bonus
					RemoveAllianceBonus(EOwnerTeam::Player, Member);
					
					// Apply -20 betrayal penalty to BaseDisposition
					ModifyDisposition(EOwnerTeam::Player, Member, -20.0f);
					
					UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d condemns Player for betraying the alliance (lost +40 bonus, -20 penalty = -60 total)"),
						(int32)Member);
				}
			}
			
			// Kick player from alliance
			FString OldAllianceName = PlayerAllianceName;
			PlayerAllianceName = TEXT("");
			
			// Remove player from all AI controllers' allied lists
			UWorld* World = GetWorld();
			if (World)
			{
				for (TActorIterator<AAITeamController> It(World); It; ++It)
				{
					if (It->AllianceState.AllianceName == OldAllianceName)
					{
						It->AllianceState.AlliedTeams.Remove(EOwnerTeam::Player);
					}
				}
			}
			
			UE_LOG(LogTemp, Warning, TEXT("[FEED] Player has been EXPELLED from '%s' for attacking an ally!"),
				*OldAllianceName);
			return; // Don't process normal damage relationship change
		}
	}
	
	// CHECK FOR ALLIANCE BETRAYAL (AI TEAMS)
	UWorld* World = GetWorld();
	if (World && AttackerTeam != EOwnerTeam::Player)
	{
		// Find the attacker's AI controller
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == AttackerTeam && !It->AllianceState.AllianceName.IsEmpty())
			{
				// Attacker is in an alliance - check if victim is also in it
				TSet<EOwnerTeam> AllianceMembers = GetAllTeamsInAlliance(It->AllianceState.AllianceName);
				if (AllianceMembers.Contains(VictimTeam))
				{
					// AI team attacked their own ally - BETRAYAL!
					UE_LOG(LogTemp, Error, TEXT("[BETRAYAL] Team %d destroyed ally Team %d's vehicle! Kicking from alliance '%s'"),
						(int32)AttackerTeam, (int32)VictimTeam, *It->AllianceState.AllianceName);
					
					// Remove alliance bonuses and apply betrayal penalties to all alliance members
					for (EOwnerTeam Member : AllianceMembers)
					{
						if (Member != AttackerTeam)
						{
							// Remove +40 alliance bonus
							RemoveAllianceBonus(AttackerTeam, Member);
							
							// Apply -20 betrayal penalty to BaseDisposition
							ModifyDisposition(AttackerTeam, Member, -20.0f);
							
							UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d condemns Team %d for betraying the alliance (lost +40 bonus, -20 penalty = -60 total)"),
								(int32)Member, (int32)AttackerTeam);
						}
					}
					
					// Kick attacker from alliance
					FString OldAllianceName = It->AllianceState.AllianceName;
					It->AllianceState.AllianceName = TEXT("");
					It->AllianceState.AlliedTeams.Empty();
					
					// Remove attacker from all other AI controllers' allied lists
					for (TActorIterator<AAITeamController> OtherIt(World); OtherIt; ++OtherIt)
					{
						if (OtherIt->AllianceState.AllianceName == OldAllianceName)
						{
							OtherIt->AllianceState.AlliedTeams.Remove(AttackerTeam);
						}
					}
					
					// Remove from player's alliance if player is in same alliance
					if (PlayerAllianceName == OldAllianceName)
					{
						// Don't kick player, just update the alliance roster
					}
					
					UE_LOG(LogTemp, Warning, TEXT("[FEED] Team %d has been EXPELLED from '%s' for attacking an ally!"),
						(int32)AttackerTeam, *OldAllianceName);
					return; // Don't process normal damage relationship change
				}
				break;
			}
		}
	}
	
	// -20 disposition per vehicle destroyed
	ModifyDisposition(AttackerTeam, VictimTeam, -20.0f);
	
	UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d destroyed Team %d's vehicle - relationship worsened (-20)"),
		(int32)AttackerTeam, (int32)VictimTeam);
}

void APlanetConquestGameMode::OnMoneyTransferred(EOwnerTeam GivingTeam, EOwnerTeam ReceivingTeam, int32 Amount)
{
	if (GivingTeam == ReceivingTeam || GivingTeam == EOwnerTeam::Neutral || ReceivingTeam == EOwnerTeam::Neutral)
	{
		return;
	}
	
	// Every $1000 given = +20 disposition (can reach +100)
	float Delta = (Amount / 1000.0f) * 20.0f;
	ModifyDisposition(GivingTeam, ReceivingTeam, Delta);
	
	UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d gave $%d to Team %d - relationship improved (+%.0f)"),
		(int32)GivingTeam, Amount, (int32)ReceivingTeam, Delta);
}

void APlanetConquestGameMode::OnTeamAttackedTeam(EOwnerTeam AttackerTeam, EOwnerTeam VictimTeam)
{
	if (AttackerTeam == VictimTeam || AttackerTeam == EOwnerTeam::Neutral || VictimTeam == EOwnerTeam::Neutral)
	{
		return;
	}
	
	// Check all other teams for "enemy of my enemy is my friend" bonus
	TArray<EOwnerTeam> AllTeams = {EOwnerTeam::Player, EOwnerTeam::AI1, EOwnerTeam::AI2, EOwnerTeam::AI3, EOwnerTeam::AI4, EOwnerTeam::AI5, EOwnerTeam::AI6, EOwnerTeam::AI7, EOwnerTeam::AI8, EOwnerTeam::AI9, EOwnerTeam::AI10, EOwnerTeam::AI11, EOwnerTeam::AI12, EOwnerTeam::AI13, EOwnerTeam::AI14, EOwnerTeam::AI15, EOwnerTeam::AI16, EOwnerTeam::AI17, EOwnerTeam::AI18, EOwnerTeam::AI19};
	
	for (EOwnerTeam ObserverTeam : AllTeams)
	{
		if (ObserverTeam == AttackerTeam || ObserverTeam == VictimTeam)
		{
			continue; // Skip attacker and victim
		}
		
		// Check if ObserverTeam dislikes VictimTeam (-50 disposition or worse)
		float ObserverVictimRelationship = GetDisposition(ObserverTeam, VictimTeam);
		if (ObserverVictimRelationship <= -50.0f)
		{
			// ObserverTeam dislikes VictimTeam, so they like the Attacker more
			uint32 Key = MakeRelationshipKey(AttackerTeam, ObserverTeam);
			FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
			
			// Check if we've already given this bonus for this mutual enemy
			if (!Rel.MutualEnemyBonusesApplied.Contains(VictimTeam))
			{
				Rel.MutualEnemyBonusesApplied.Add(VictimTeam);
				ModifyDisposition(AttackerTeam, ObserverTeam, 20.0f);
				
				UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d likes Team %d more (+20) - both dislike Team %d"),
					(int32)ObserverTeam, (int32)AttackerTeam, (int32)VictimTeam);
			}
		}
	}
}

float APlanetConquestGameMode::GetHostilityThreshold(EOwnerTeam Team) const
{
	// For now, return a default threshold
	// TODO: Once AITeamController has personality, query it here
	// Aggressive AI: 0.0 (fire on neutrals)
	// Normal AI: -50.0 (fire on enemies)
	// Defensive AI: -80.0 (only fire on bitter enemies)
	
	return -50.0f; // Default: fire when disposition is -50 or worse
}

int32 APlanetConquestGameMode::CountCitiesForTeam(EOwnerTeam Team) const
{
	if (Team == EOwnerTeam::Neutral)
	{
		return 0; // Neutral doesn't own cities
	}
	
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), FoundCities);
	
	int32 Count = 0;
	for (AActor* Actor : FoundCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == Team)
		{
			Count++;
		}
	}
	
	return Count;
}

// ========== AI-PLAYER TRADE REQUEST SYSTEM ==========

bool APlanetConquestGameMode::CanPlayerTradeWithTeam(EOwnerTeam Team) const
{
	return GetTradeCooldownRemaining(Team) <= 0.0f;
}

float APlanetConquestGameMode::GetTradeCooldownRemaining(EOwnerTeam Team) const
{
	const float* LastTradeTime = LastPlayerTradeTime.Find(Team);
	if (!LastTradeTime)
	{
		return 0.0f; // Never traded, can trade now
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	float CurrentTime = World->GetTimeSeconds();
	float TimeSinceLastTrade = CurrentTime - (*LastTradeTime);
	float RemainingCooldown = TRADE_COOLDOWN_SECONDS - TimeSinceLastTrade;

	return FMath::Max(0.0f, RemainingCooldown);
}

void APlanetConquestGameMode::MarkTradeInitiated(EOwnerTeam Team)
{
	UWorld* World = GetWorld();
	if (!World) return;

	float CurrentTime = World->GetTimeSeconds();
	LastPlayerTradeTime.Add(Team, CurrentTime);

	UE_LOG(LogTemp, Log, TEXT("GameMode: Trade cooldown started with Team %d - next trade available in %.1f seconds"),
		(int32)Team, TRADE_COOLDOWN_SECONDS);
}

// ========== BRIBE SYSTEM IMPLEMENTATION ==========

bool APlanetConquestGameMode::CanPlayerBribeTeam(EOwnerTeam Team) const
{
	return GetBribeCooldownRemaining(Team) <= 0.0f;
}

float APlanetConquestGameMode::GetBribeCooldownRemaining(EOwnerTeam Team) const
{
	const float* LastBribeTime = LastPlayerBribeTime.Find(Team);
	if (!LastBribeTime)
	{
		return 0.0f; // Never bribed, can bribe now
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	float CurrentTime = World->GetTimeSeconds();
	float TimeSinceLastBribe = CurrentTime - (*LastBribeTime);
	float RemainingCooldown = BRIBE_COOLDOWN_SECONDS - TimeSinceLastBribe;

	return FMath::Max(0.0f, RemainingCooldown);
}

bool APlanetConquestGameMode::AttemptBribe(EOwnerTeam TargetTeam, bool bUseOrangeSubstrate)
{
	// Check if bribe cooldown is active
	if (!CanPlayerBribeTeam(TargetTeam))
	{
		float RemainingCooldown = GetBribeCooldownRemaining(TargetTeam);
		UE_LOG(LogTemp, Warning, TEXT("Cannot bribe Team %d - cooldown active (%.1f seconds remaining)"),
			(int32)TargetTeam, RemainingCooldown);
		return false;
	}

	// Find player controller to check resources
	UWorld* World = GetWorld();
	if (!World) return false;

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("AttemptBribe: Could not find player controller"));
		return false;
	}

	// Check if player has enough substrate
	const int32 BribeCost = 2000;
	if (bUseOrangeSubstrate && PlayerController->PlayerOrangeSubstrate < BribeCost)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot bribe Team %d - insufficient orange substrate (%d / %d)"),
			(int32)TargetTeam, PlayerController->PlayerOrangeSubstrate, BribeCost);
		return false;
	}
	else if (!bUseOrangeSubstrate && PlayerController->PlayerBlackSubstrate < BribeCost)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot bribe Team %d - insufficient black substrate (%d / %d)"),
			(int32)TargetTeam, PlayerController->PlayerBlackSubstrate, BribeCost);
		return false;
	}

	// Deduct substrate cost
	if (bUseOrangeSubstrate)
	{
		PlayerController->PlayerOrangeSubstrate -= BribeCost;
	}
	else
	{
		PlayerController->PlayerBlackSubstrate -= BribeCost;
	}

	// Start bribe cooldown
	float CurrentTime = World->GetTimeSeconds();
	LastPlayerBribeTime.Add(TargetTeam, CurrentTime);

	// 65% chance of success
	float RandomValue = FMath::FRand();
	bool bSuccess = (RandomValue <= 0.65f);

	if (bSuccess)
	{
		// Increment bribe count for +10 relationship bonus (repeatable)
		uint32 Key = MakeRelationshipKey(EOwnerTeam::Player, TargetTeam);
		FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
		Rel.BribeCount++;
		
		UE_LOG(LogTemp, Log, TEXT("Bribe SUCCEEDED! Team %d bribe count increased (now %d bribes = +%d relationship, paid %d %s substrate)"),
			(int32)TargetTeam, Rel.BribeCount, Rel.BribeCount * 10, BribeCost, bUseOrangeSubstrate ? TEXT("orange") : TEXT("black"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Bribe FAILED! Team %d kept the %d %s substrate but relationship unchanged"),
			(int32)TargetTeam, BribeCost, bUseOrangeSubstrate ? TEXT("orange") : TEXT("black"));
	}

	return bSuccess;
}

bool APlanetConquestGameMode::AIAttemptBribe(EOwnerTeam BribingTeam, EOwnerTeam TargetTeam, bool bUseOrangeSubstrate, int32& OutOrangeSubstrate, int32& OutBlackSubstrate)
{
	// Check AI bribe cooldown first
	if (!CanAIBribeTeam(BribingTeam, TargetTeam))
	{
		float RemainingCooldown = GetAIBribeCooldownRemaining(BribingTeam, TargetTeam);
		UE_LOG(LogTemp, Log, TEXT("AI Team %d cannot bribe Team %d - cooldown active (%.1f seconds remaining)"),
			(int32)BribingTeam, (int32)TargetTeam, RemainingCooldown);
		return false;
	}
	
	// Check if AI has enough substrate
	const int32 BribeCost = 2000;
	if (bUseOrangeSubstrate && OutOrangeSubstrate < BribeCost)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d cannot bribe Team %d - insufficient orange substrate (%d / %d)"),
			(int32)BribingTeam, (int32)TargetTeam, OutOrangeSubstrate, BribeCost);
		return false;
	}
	else if (!bUseOrangeSubstrate && OutBlackSubstrate < BribeCost)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d cannot bribe Team %d - insufficient black substrate (%d / %d)"),
			(int32)BribingTeam, (int32)TargetTeam, OutBlackSubstrate, BribeCost);
		return false;
	}

	// Deduct substrate cost
	if (bUseOrangeSubstrate)
	{
		OutOrangeSubstrate -= BribeCost;
	}
	else
	{
		OutBlackSubstrate -= BribeCost;
	}
	
	// Start cooldown timer
	UWorld* World = GetWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		uint32 Key = MakeRelationshipKey(BribingTeam, TargetTeam);
		LastAIBribeTime.Add(Key, CurrentTime);
	}

	// 65% chance of success
	float RandomValue = FMath::FRand();
	bool bSuccess = (RandomValue <= 0.65f);

	if (bSuccess)
	{
		// Increment bribe count for +10 relationship bonus (repeatable)
		uint32 Key = MakeRelationshipKey(BribingTeam, TargetTeam);
		FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
		Rel.BribeCount++;
		
		UE_LOG(LogTemp, Log, TEXT("AI Bribe SUCCEEDED! Team %d -> Team %d (bribe count now %d = +%d relationship, paid %d %s substrate)"),
			(int32)BribingTeam, (int32)TargetTeam, Rel.BribeCount, Rel.BribeCount * 10, BribeCost, bUseOrangeSubstrate ? TEXT("orange") : TEXT("black"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AI Bribe FAILED! Team %d -> Team %d (kept the %d %s substrate but relationship unchanged)"),
			(int32)BribingTeam, (int32)TargetTeam, BribeCost, bUseOrangeSubstrate ? TEXT("orange") : TEXT("black"));
	}

	return bSuccess;
}

bool APlanetConquestGameMode::CanAIBribeTeam(EOwnerTeam BribingTeam, EOwnerTeam TargetTeam) const
{
	return GetAIBribeCooldownRemaining(BribingTeam, TargetTeam) <= 0.0f;
}

float APlanetConquestGameMode::GetAIBribeCooldownRemaining(EOwnerTeam BribingTeam, EOwnerTeam TargetTeam) const
{
	uint32 Key = MakeRelationshipKey(BribingTeam, TargetTeam);
	const float* LastBribeTime = LastAIBribeTime.Find(Key);
	if (!LastBribeTime)
	{
		return 0.0f; // Never bribed, can bribe now
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	float CurrentTime = World->GetTimeSeconds();
	float TimeSinceLastBribe = CurrentTime - (*LastBribeTime);
	float RemainingCooldown = BRIBE_COOLDOWN_SECONDS - TimeSinceLastBribe;

	return FMath::Max(0.0f, RemainingCooldown);
}

void APlanetConquestGameMode::QueueAIBribeRequest(EOwnerTeam BribingTeam, bool bUseOrangeSubstrate)
{
	// Check AI bribe cooldown
	if (!CanAIBribeTeam(BribingTeam, EOwnerTeam::Player))
	{
		float RemainingCooldown = GetAIBribeCooldownRemaining(BribingTeam, EOwnerTeam::Player);
		UE_LOG(LogTemp, Log, TEXT("GameMode: AI Team %d cannot bribe Player yet - cooldown: %.1f seconds remaining"),
			(int32)BribingTeam, RemainingCooldown);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("GameMode: AI Team %d queued bribe request for Player - Offering %d %s substrate"),
		(int32)BribingTeam, 2000, bUseOrangeSubstrate ? TEXT("orange") : TEXT("black"));

	FAIBribeRequest NewRequest(BribingTeam, bUseOrangeSubstrate);
	CurrentBribeRequest = NewRequest;
	bHasActiveBribeRequest = true;

	// Get player controller
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: World is null!"));
		bHasActiveBribeRequest = false;
		return;
	}

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find player controller!"));
		bHasActiveBribeRequest = false;
		return;
	}

	// Find a city owned by the bribing AI team
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), FoundCities);

	ACityActor* BribingCity = nullptr;
	for (AActor* Actor : FoundCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == BribingTeam)
		{
			BribingCity = City;
			break; // Use the first city we find
		}
	}

	if (!BribingCity)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find city for AI Team %d!"),
			(int32)BribingTeam);
		bHasActiveBribeRequest = false;
		return;
	}

	// Load the TalkDialogue widget class
	TSubclassOf<UTalkDialogueWidget> TalkDialogueClass = LoadClass<UTalkDialogueWidget>(nullptr, TEXT("/Game/UI/WBP_TalkDialogue.WBP_TalkDialogue_C"));
	if (!TalkDialogueClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to load WBP_TalkDialogue Blueprint class!"));
		bHasActiveBribeRequest = false;
		return;
	}

	// Create and show the TalkDialogue widget in BribeIncoming state
	UTalkDialogueWidget* TalkWidget = CreateWidget<UTalkDialogueWidget>(PlayerController, TalkDialogueClass);
	if (TalkWidget)
	{
		TalkWidget->InitializeDialogueInState(BribingCity, EDialogueState::BribeIncoming);
		TalkWidget->AddToViewport(100); // High Z-order to appear on top
		
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Opened AI-initiated bribe dialogue for Team %d"),
			(int32)BribingTeam);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to create TalkDialogueWidget!"));
		bHasActiveBribeRequest = false;
	}
}

bool APlanetConquestGameMode::AcceptCurrentBribeRequestThankfully()
{
	if (!bHasActiveBribeRequest)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: No active bribe request to accept!"));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World) return false;

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find player controller for bribe!"));
		return false;
	}

	// Give substrate to player
	if (CurrentBribeRequest.bUsingOrangeSubstrate)
	{
		PlayerController->PlayerOrangeSubstrate += CurrentBribeRequest.Amount;
	}
	else
	{
		PlayerController->PlayerBlackSubstrate += CurrentBribeRequest.Amount;
	}

	// Thankful response: +10 relationship
	uint32 Key = MakeRelationshipKey(CurrentBribeRequest.BribingTeam, EOwnerTeam::Player);
	FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
	Rel.BribeCount++; // +10 per bribe count
	
	// Start cooldown timer
	float CurrentTime = World->GetTimeSeconds();
	uint32 CooldownKey = MakeRelationshipKey(CurrentBribeRequest.BribingTeam, EOwnerTeam::Player);
	LastAIBribeTime.Add(CooldownKey, CurrentTime);

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Player THANKFULLY accepted bribe from AI Team %d (+%d %s substrate, +10 relationship)"),
		(int32)CurrentBribeRequest.BribingTeam, CurrentBribeRequest.Amount,
		CurrentBribeRequest.bUsingOrangeSubstrate ? TEXT("orange") : TEXT("black"));

	// Clear the request
	CurrentBribeRequest = FAIBribeRequest();
	bHasActiveBribeRequest = false;
	return true;
}

bool APlanetConquestGameMode::AcceptCurrentBribeRequestSuspiciously()
{
	if (!bHasActiveBribeRequest)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: No active bribe request to accept!"));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World) return false;

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find player controller for bribe!"));
		return false;
	}

	// Give substrate to player
	if (CurrentBribeRequest.bUsingOrangeSubstrate)
	{
		PlayerController->PlayerOrangeSubstrate += CurrentBribeRequest.Amount;
	}
	else
	{
		PlayerController->PlayerBlackSubstrate += CurrentBribeRequest.Amount;
	}

	// Suspicious response: -10 relationship
	uint32 Key = MakeRelationshipKey(CurrentBribeRequest.BribingTeam, EOwnerTeam::Player);
	FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
	Rel.BaseDisposition -= 10; // Direct penalty
	
	// Start cooldown timer
	float CurrentTime = World->GetTimeSeconds();
	uint32 CooldownKey = MakeRelationshipKey(CurrentBribeRequest.BribingTeam, EOwnerTeam::Player);
	LastAIBribeTime.Add(CooldownKey, CurrentTime);

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Player SUSPICIOUSLY accepted bribe from AI Team %d (+%d %s substrate, -10 relationship)"),
		(int32)CurrentBribeRequest.BribingTeam, CurrentBribeRequest.Amount,
		CurrentBribeRequest.bUsingOrangeSubstrate ? TEXT("orange") : TEXT("black"));

	// Clear the request
	CurrentBribeRequest = FAIBribeRequest();
	bHasActiveBribeRequest = false;
	return true;
}

void APlanetConquestGameMode::DeclineCurrentBribeRequest()
{
	if (!bHasActiveBribeRequest)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: No active bribe request to decline!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("GameMode: Player declined bribe request from AI Team %d (no relationship change)"),
		(int32)CurrentBribeRequest.BribingTeam);

	// Clear the request (AI keeps their substrate)
	CurrentBribeRequest = FAIBribeRequest();
	bHasActiveBribeRequest = false;
}

FAIBribeRequest APlanetConquestGameMode::GetCurrentBribeRequest() const
{
	return CurrentBribeRequest;
}

// ========== END BRIBE SYSTEM ==========


void APlanetConquestGameMode::QueueAITradeRequest(EOwnerTeam RequestingTeam, bool bRequestingOrange, int32 AmountRequested, int32 AmountOffered)
{
	// Check if trade cooldown is active
	if (!CanPlayerTradeWithTeam(RequestingTeam))
	{
		float RemainingCooldown = GetTradeCooldownRemaining(RequestingTeam);
		UE_LOG(LogTemp, Log, TEXT("GameMode: AI Team %d cannot request trade yet - cooldown: %.1f seconds remaining"),
			(int32)RequestingTeam, RemainingCooldown);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("GameMode: AI Team %d queued trade request - Wants %d %s, offers %d %s"),
		(int32)RequestingTeam, AmountRequested, bRequestingOrange ? TEXT("orange") : TEXT("black"),
		AmountOffered, bRequestingOrange ? TEXT("black") : TEXT("orange"));

	// Add to queue
	FAITradeRequest NewRequest(RequestingTeam, bRequestingOrange, AmountRequested, AmountOffered);
	PendingAITradeRequests.Add(NewRequest);

	// If no active request, process immediately
	if (!bHasActiveTradeRequest)
	{
		ProcessNextTradeRequest();
	}
}

void APlanetConquestGameMode::ProcessNextTradeRequest()
{
	// Check if there are any pending requests
	if (PendingAITradeRequests.Num() == 0)
	{
		bHasActiveTradeRequest = false;
		return;
	}

	// Pop the first request from the queue
	CurrentTradeRequest = PendingAITradeRequests[0];
	PendingAITradeRequests.RemoveAt(0);
	bHasActiveTradeRequest = true;

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Processing trade request from AI Team %d"),
		(int32)CurrentTradeRequest.RequestingTeam);

	// Get player controller
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: World is null!"));
		bHasActiveTradeRequest = false;
		return;
	}

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find player controller!"));
		bHasActiveTradeRequest = false;
		return;
	}

	// Find a city owned by the requesting AI team
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), FoundCities);

	ACityActor* RequestingCity = nullptr;
	for (AActor* Actor : FoundCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == CurrentTradeRequest.RequestingTeam)
		{
			RequestingCity = City;
			break; // Use the first city we find
		}
	}

	if (!RequestingCity)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find city for AI Team %d!"),
			(int32)CurrentTradeRequest.RequestingTeam);
		bHasActiveTradeRequest = false;
		return;
	}

	// Load the TalkDialogue widget class
	TSubclassOf<UTalkDialogueWidget> TalkDialogueClass = LoadClass<UTalkDialogueWidget>(nullptr, TEXT("/Game/UI/WBP_TalkDialogue.WBP_TalkDialogue_C"));
	if (!TalkDialogueClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to load WBP_TalkDialogue Blueprint class!"));
		bHasActiveTradeRequest = false;
		return;
	}

	// Create and show the TalkDialogue widget in TradeMenu state
	UTalkDialogueWidget* TalkWidget = CreateWidget<UTalkDialogueWidget>(PlayerController, TalkDialogueClass);
	if (TalkWidget)
	{
		TalkWidget->InitializeDialogueInState(RequestingCity, EDialogueState::TradeMenu);
		TalkWidget->AddToViewport(100); // High Z-order to appear on top
		
		// Mark trade as initiated (starts cooldown regardless of acceptance)
		MarkTradeInitiated(CurrentTradeRequest.RequestingTeam);
		
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Opened AI-initiated trade dialogue for Team %d"),
			(int32)CurrentTradeRequest.RequestingTeam);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to create TalkDialogueWidget!"));
		bHasActiveTradeRequest = false;
	}
}

bool APlanetConquestGameMode::AcceptCurrentTradeRequest()
{
	if (!bHasActiveTradeRequest)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: No active trade request to accept!"));
		return false;
	}

	// Get player controller
	UWorld* World = GetWorld();
	if (!World) return false;

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find player controller!"));
		return false;
	}

	// Check if player has enough substrate to give
	int32 PlayerSupply = CurrentTradeRequest.bRequestingOrange ? PlayerController->PlayerOrangeSubstrate : PlayerController->PlayerBlackSubstrate;
	if (PlayerSupply < CurrentTradeRequest.AmountRequested)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Player has insufficient %s substrate (%d < %d)"),
			CurrentTradeRequest.bRequestingOrange ? TEXT("orange") : TEXT("black"),
			PlayerSupply, CurrentTradeRequest.AmountRequested);
		return false;
	}

	// Find the requesting AI controller
	AAITeamController* AIC = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentTradeRequest.RequestingTeam)
		{
			AIC = *It;
			break;
		}
	}

	if (!AIC)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find AI controller for team %d!"),
			(int32)CurrentTradeRequest.RequestingTeam);
		return false;
	}

	// Check if AI still has enough substrate to give
	int32 AISupply = CurrentTradeRequest.bRequestingOrange ? AIC->BlackSubstrate : AIC->OrangeSubstrate;
	if (AISupply < CurrentTradeRequest.AmountOffered)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: AI Team %d has insufficient %s substrate (%d < %d)"),
			(int32)CurrentTradeRequest.RequestingTeam,
			CurrentTradeRequest.bRequestingOrange ? TEXT("black") : TEXT("orange"),
			AISupply, CurrentTradeRequest.AmountOffered);
		return false;
	}

	// Execute the trade
	if (CurrentTradeRequest.bRequestingOrange)
	{
		// AI wants orange, player gets black
		PlayerController->PlayerOrangeSubstrate -= CurrentTradeRequest.AmountRequested;
		PlayerController->PlayerBlackSubstrate += CurrentTradeRequest.AmountOffered;
		AIC->OrangeSubstrate += CurrentTradeRequest.AmountRequested;
		AIC->BlackSubstrate -= CurrentTradeRequest.AmountOffered;
	}
	else
	{
		// AI wants black, player gets orange
		PlayerController->PlayerBlackSubstrate -= CurrentTradeRequest.AmountRequested;
		PlayerController->PlayerOrangeSubstrate += CurrentTradeRequest.AmountOffered;
		AIC->BlackSubstrate += CurrentTradeRequest.AmountRequested;
		AIC->OrangeSubstrate -= CurrentTradeRequest.AmountOffered;
	}

	UE_LOG(LogTemp, Warning, TEXT("GameMode: TRADE ACCEPTED - Player gave %d %s, received %d %s"),
		CurrentTradeRequest.AmountRequested, CurrentTradeRequest.bRequestingOrange ? TEXT("orange") : TEXT("black"),
		CurrentTradeRequest.AmountOffered, CurrentTradeRequest.bRequestingOrange ? TEXT("black") : TEXT("orange"));

	// Increment trade count for +5 relationship bonus (repeatable)
	uint32 Key = MakeRelationshipKey(EOwnerTeam::Player, CurrentTradeRequest.RequestingTeam);
	FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
	Rel.TradeCount++;
	
	UE_LOG(LogTemp, Log, TEXT("GameMode: Trade count increased between Player and Team %d (now %d trades = +%d relationship)"),
		(int32)CurrentTradeRequest.RequestingTeam, Rel.TradeCount, Rel.TradeCount * 5);

	// Clear active request
	bHasActiveTradeRequest = false;

	// Process next request in queue (if any)
	ProcessNextTradeRequest();

	return true;
}

void APlanetConquestGameMode::DeclineCurrentTradeRequest()
{
	if (!bHasActiveTradeRequest)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: No active trade request to decline!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Player declined trade request from AI Team %d (Amount: %d)"),
		(int32)CurrentTradeRequest.RequestingTeam, CurrentTradeRequest.AmountRequested);

	// Player declined - no counter-offer, just close the trade
	UE_LOG(LogTemp, Warning, TEXT("GameMode: Trade declined by player, moving to next request"));

	// Clear active request
	bHasActiveTradeRequest = false;

	// Process next request in queue (if any)
	ProcessNextTradeRequest();
}

FAITradeRequest APlanetConquestGameMode::GetCurrentTradeRequest() const
{
	return CurrentTradeRequest;
}
// ========== AI-PLAYER ALLIANCE REQUEST SYSTEM ==========

void APlanetConquestGameMode::QueueAIAllianceRequest(EOwnerTeam RequestingTeam, EOwnerTeam CommonEnemy)
{
	FAIAllianceRequest NewRequest(RequestingTeam, CommonEnemy);
	PendingAIAllianceRequests.Add(NewRequest);

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Queued alliance request from AI Team %d (common enemy: Team %d)"),
		(int32)RequestingTeam, (int32)CommonEnemy);

	// If no active request, process this one immediately
	if (!bHasActiveAllianceRequest && !bHasActiveTradeRequest)
	{
		ProcessNextAllianceRequest();
	}
}

void APlanetConquestGameMode::ProcessNextAllianceRequest()
{
	// Check if there are any pending requests
	if (PendingAIAllianceRequests.Num() == 0)
	{
		bHasActiveAllianceRequest = false;
		return;
	}

	// Don't interrupt active trade requests
	if (bHasActiveTradeRequest)
	{
		UE_LOG(LogTemp, Log, TEXT("GameMode: Delaying alliance request - trade in progress"));
		return;
	}

	// Pop the first request from the queue
	CurrentAllianceRequest = PendingAIAllianceRequests[0];
	PendingAIAllianceRequests.RemoveAt(0);
	bHasActiveAllianceRequest = true;

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Processing alliance request from AI Team %d"),
		(int32)CurrentAllianceRequest.RequestingTeam);

	// Get player controller
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: World is null!"));
		bHasActiveAllianceRequest = false;
		return;
	}

	APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find player controller!"));
		bHasActiveAllianceRequest = false;
		return;
	}

	// Find a city owned by the requesting AI team
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), FoundCities);

	ACityActor* RequestingCity = nullptr;
	for (AActor* Actor : FoundCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == CurrentAllianceRequest.RequestingTeam)
		{
			RequestingCity = City;
			break;
		}
	}

	if (!RequestingCity)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find city for AI Team %d!"),
			(int32)CurrentAllianceRequest.RequestingTeam);
		bHasActiveAllianceRequest = false;
		return;
	}

	// Load the TalkDialogue widget class
	TSubclassOf<UTalkDialogueWidget> TalkDialogueClass = LoadClass<UTalkDialogueWidget>(nullptr, TEXT("/Game/UI/WBP_TalkDialogue.WBP_TalkDialogue_C"));
	if (!TalkDialogueClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to load WBP_TalkDialogue Blueprint class!"));
		bHasActiveAllianceRequest = false;
		return;
	}

	// Create and show the TalkDialogue widget in RequestMenu state (alliance is a request)
	UTalkDialogueWidget* TalkWidget = CreateWidget<UTalkDialogueWidget>(PlayerController, TalkDialogueClass);
	if (TalkWidget)
	{
		TalkWidget->InitializeDialogueInState(RequestingCity, EDialogueState::RequestMenu);
		TalkWidget->AddToViewport(100);
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Opened AI-initiated alliance dialogue for Team %d"),
			(int32)CurrentAllianceRequest.RequestingTeam);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to create TalkDialogueWidget!"));
		bHasActiveAllianceRequest = false;
	}
}

bool APlanetConquestGameMode::AcceptCurrentAllianceRequest()
{
	if (!bHasActiveAllianceRequest)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: No active alliance request to accept!"));
		return false;
	}
	
	UWorld* World = GetWorld();
	if (!World) return false;

	// Find the requesting AI controller
	AAITeamController* RequestingAI = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == CurrentAllianceRequest.RequestingTeam)
		{
			RequestingAI = *It;
			break;
		}
	}

	if (!RequestingAI)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find AI controller for team %d!"),
			(int32)CurrentAllianceRequest.RequestingTeam);
		return false;
	}
	
	// Validate that we can form this alliance
	if (AreTeamsEnemies(EOwnerTeam::Player, CurrentAllianceRequest.RequestingTeam))
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Cannot accept alliance - Player and Team %d are enemies!"),
			(int32)CurrentAllianceRequest.RequestingTeam);
		bHasActiveAllianceRequest = false;
		ProcessNextAllianceRequest();
		return false;
	}

	// Form or join alliance
	FString AllianceName;
	
	if (!PlayerAllianceName.IsEmpty())
	{
		// Player has existing alliance - requesting team joins it
		AllianceName = PlayerAllianceName;
		if (!AddTeamToAlliance(CurrentAllianceRequest.RequestingTeam, AllianceName))
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to add Team %d to player's alliance!"), 
				(int32)CurrentAllianceRequest.RequestingTeam);
			bHasActiveAllianceRequest = false;
			ProcessNextAllianceRequest();
			return false;
		}
	}
	else if (!RequestingAI->AllianceState.AllianceName.IsEmpty())
	{
		// Requesting team has existing alliance - player joins it
		AllianceName = RequestingAI->AllianceState.AllianceName;
		if (!AddTeamToAlliance(EOwnerTeam::Player, AllianceName))
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to add Player to Team %d's alliance!"), 
				(int32)CurrentAllianceRequest.RequestingTeam);
			bHasActiveAllianceRequest = false;
			ProcessNextAllianceRequest();
			return false;
		}
	}
	else
	{
		// Neither has alliance - create new one
		AllianceName = FormNewAlliance(EOwnerTeam::Player, CurrentAllianceRequest.RequestingTeam);
		if (AllianceName.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to form new alliance between Player and Team %d!"), 
				(int32)CurrentAllianceRequest.RequestingTeam);
			bHasActiveAllianceRequest = false;
			ProcessNextAllianceRequest();
			return false;
		}
	}

	// Validate alliance integrity
	ValidateAllianceIntegrity();

	UE_LOG(LogTemp, Display, TEXT("[FEED] Alliance '%s' formed between Player and Team %d"),
		*AllianceName, (int32)CurrentAllianceRequest.RequestingTeam);

	// Clear active request
	bHasActiveAllianceRequest = false;

	// Process next request in queue (if any)
	ProcessNextAllianceRequest();

	return true;
}

void APlanetConquestGameMode::DeclineCurrentAllianceRequest()
{
	if (!bHasActiveAllianceRequest)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: No active alliance request to decline!"));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("[FEED] Player declined alliance request from Team %d"),
		(int32)CurrentAllianceRequest.RequestingTeam);

	// Clear active request
	bHasActiveAllianceRequest = false;

	// Process next request in queue (if any)
	ProcessNextAllianceRequest();
}

bool APlanetConquestGameMode::PlayerRequestAlliance(EOwnerTeam TargetTeam, EOwnerTeam CommonEnemy)
{
	// Check if already allied with this team
	if (IsPlayerAlliedWith(TargetTeam))
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Player is already allied with Team %d!"),
			(int32)TargetTeam);
		return false;
	}

	// Check relationship with target team (must be +20 or higher)
	float Relationship = GetDisposition(EOwnerTeam::Player, TargetTeam);
	if (Relationship < 20.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Player cannot request alliance with Team %d - relationship too low (%.1f < 20.0)"),
			(int32)TargetTeam, Relationship);
		return false;
	}
	
	// Validate that player and target team aren't enemies
	if (AreTeamsEnemies(EOwnerTeam::Player, TargetTeam))
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Cannot form alliance - Player and Team %d are enemies!"),
			(int32)TargetTeam);
		return false;
	}

	// Find the target AI controller
	UWorld* World = GetWorld();
	if (!World) return false;

	AAITeamController* TargetAI = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == TargetTeam)
		{
			TargetAI = *It;
			break;
		}
	}

	if (!TargetAI)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find AI controller for team %d!"),
			(int32)TargetTeam);
		return false;
	}

	// Check if AI already allied with a team that hates the player
	for (EOwnerTeam AllyTeam : TargetAI->AllianceState.AlliedTeams)
	{
		if (AreTeamsEnemies(AllyTeam, EOwnerTeam::Player))
		{
			UE_LOG(LogTemp, Warning, TEXT("GameMode: Team %d rejected player's alliance request - allied with Team %d who is hostile to player"),
				(int32)TargetTeam, (int32)AllyTeam);
			return false;
		}
	}

	// Check if AI hates any of the player's existing allies
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		// Check if this AI is allied with the player
		if (It->AllianceState.AlliedTeams.Contains(EOwnerTeam::Player))
		{
			EOwnerTeam PlayerAllyTeam = It->ControlledTeam;
			if (AreTeamsEnemies(TargetTeam, PlayerAllyTeam))
			{
				UE_LOG(LogTemp, Warning, TEXT("GameMode: Team %d rejected player's alliance request - hostile to player's ally Team %d"),
					(int32)TargetTeam, (int32)PlayerAllyTeam);
				return false;
			}
		}
	}

	// All conditions met - form or join alliance
	FString AllianceName;
	
	if (!PlayerAllianceName.IsEmpty())
	{
		// Player has existing alliance - target team joins it
		AllianceName = PlayerAllianceName;
		if (!AddTeamToAlliance(TargetTeam, AllianceName))
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to add Team %d to player's alliance!"), (int32)TargetTeam);
			return false;
		}
	}
	else if (!TargetAI->AllianceState.AllianceName.IsEmpty())
	{
		// Target has existing alliance - player joins it
		AllianceName = TargetAI->AllianceState.AllianceName;
		if (!AddTeamToAlliance(EOwnerTeam::Player, AllianceName))
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to add Player to Team %d's alliance!"), (int32)TargetTeam);
			return false;
		}
	}
	else
	{
		// Neither has alliance - create new one
		AllianceName = FormNewAlliance(EOwnerTeam::Player, TargetTeam);
		if (AllianceName.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Failed to form new alliance between Player and Team %d!"), (int32)TargetTeam);
			return false;
		}
	}

	// Validate alliance integrity
	ValidateAllianceIntegrity();

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Player successfully formed alliance '%s' with Team %d"),
		*AllianceName, (int32)TargetTeam);

	return true;
}

FAIAllianceRequest APlanetConquestGameMode::GetCurrentAllianceRequest() const
{
	return CurrentAllianceRequest;
}

bool APlanetConquestGameMode::IsPlayerAlliedWith(EOwnerTeam TargetTeam) const
{
	if (TargetTeam == EOwnerTeam::Player || TargetTeam == EOwnerTeam::Neutral)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World) return false;

	// Find the target AI controller and check if Player is in their AlliedTeams
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == TargetTeam)
		{
			return It->AllianceState.AlliedTeams.Contains(EOwnerTeam::Player);
		}
	}

	return false;
}

bool APlanetConquestGameMode::PlayerEndAlliance(EOwnerTeam TargetTeam)
{
	// Check if actually allied with this team
	if (!IsPlayerAlliedWith(TargetTeam))
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Player is not allied with Team %d!"),
			(int32)TargetTeam);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World) return false;

	// Find the target AI controller
	AAITeamController* TargetAI = nullptr;
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->ControlledTeam == TargetTeam)
		{
			TargetAI = *It;
			break;
		}
	}

	if (!TargetAI)
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find AI controller for team %d!"),
			(int32)TargetTeam);
		return false;
	}

	// Get the alliance name before removing
	FString FormerAllianceName = TargetAI->AllianceState.AllianceName;

	// Remove Player from target AI's alliance
	TargetAI->AllianceState.AlliedTeams.Remove(EOwnerTeam::Player);

	// If target AI has no more allies, clear their alliance name
	if (TargetAI->AllianceState.AlliedTeams.Num() == 0)
	{
		TargetAI->AllianceState.AllianceName = TEXT("");
	}

	// Find all of player's other allies and remove player from their alliance
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->AllianceState.AlliedTeams.Contains(EOwnerTeam::Player))
		{
			// Remove player from this ally's alliance
			It->AllianceState.AlliedTeams.Remove(EOwnerTeam::Player);
			
			// If this ally has no more allies, clear their alliance name
			if (It->AllianceState.AlliedTeams.Num() == 0)
			{
				It->AllianceState.AllianceName = TEXT("");
			}

			// Apply -60 relationship penalty with all former alliance members
			ModifyDisposition(EOwnerTeam::Player, It->ControlledTeam, -60.0f);
			UE_LOG(LogTemp, Warning, TEXT("GameMode: Applied -60 relationship penalty between Player and Team %d (alliance ended)"),
				(int32)It->ControlledTeam);
		}
	}

	// Apply -60 relationship penalty to the target team
	ModifyDisposition(EOwnerTeam::Player, TargetTeam, -60.0f);

	// Clear player's alliance name
	PlayerAllianceName = TEXT("");

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Player ended alliance '%s' with Team %d and all allies. Applied -60 to all former allies."),
		*FormerAllianceName, (int32)TargetTeam);

	return true;
}

void APlanetConquestGameMode::CleanupAlliancesForEliminatedTeam(EOwnerTeam EliminatedTeam)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UE_LOG(LogTemp, Warning, TEXT("GameMode: Cleaning up alliances for eliminated Team %d"), (int32)EliminatedTeam);

	// Iterate all AI controllers and remove the eliminated team from their alliances
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		AAITeamController* AI = *It;
		if (!AI) continue;

		// Remove eliminated team from this AI's alliance
		if (AI->AllianceState.AlliedTeams.Contains(EliminatedTeam))
		{
			AI->AllianceState.AlliedTeams.Remove(EliminatedTeam);

			// If this AI has no more allies, clear their alliance name (becomes unaffiliated)
			if (AI->AllianceState.AlliedTeams.Num() == 0)
			{
				FString FormerAllianceName = AI->AllianceState.AllianceName;
				AI->AllianceState.AllianceName = TEXT("");
				UE_LOG(LogTemp, Warning, TEXT("GameMode: Team %d left alliance '%s' after Team %d elimination (now unaffiliated)"),
					(int32)AI->ControlledTeam, *FormerAllianceName, (int32)EliminatedTeam);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GameMode: Team %d removed Team %d from alliance '%s' (still has %d allies)"),
					(int32)AI->ControlledTeam, (int32)EliminatedTeam, *AI->AllianceState.AllianceName, AI->AllianceState.AlliedTeams.Num());
			}
		}
	}
}

FString APlanetConquestGameMode::GetRandomAllianceName()
{
	if (AllianceNames.Num() == 0)
	{
		return TEXT("The Alliance");
	}

	int32 RandomIndex = FMath::RandRange(0, AllianceNames.Num() - 1);
	return AllianceNames[RandomIndex];
}

// ========== ALLIANCE MANAGEMENT HELPERS ==========

TSet<EOwnerTeam> APlanetConquestGameMode::GetAllTeamsInAlliance(const FString& AllianceName)
{
	TSet<EOwnerTeam> TeamsInAlliance;
	
	if (AllianceName.IsEmpty())
	{
		return TeamsInAlliance;
	}
	
	UWorld* World = GetWorld();
	if (!World) return TeamsInAlliance;
	
	// Check if player is in this alliance
	if (PlayerAllianceName == AllianceName)
	{
		TeamsInAlliance.Add(EOwnerTeam::Player);
	}
	
	// Check all AI teams
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (It->AllianceState.AllianceName == AllianceName)
		{
			TeamsInAlliance.Add(It->ControlledTeam);
		}
	}
	
	return TeamsInAlliance;
}

bool APlanetConquestGameMode::AddTeamToAlliance(EOwnerTeam NewTeam, const FString& AllianceName)
{
	if (AllianceName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Cannot add team %d to alliance - alliance name is empty!"),
			(int32)NewTeam);
		return false;
	}
	
	UWorld* World = GetWorld();
	if (!World) return false;
	
	// Get all current members of this alliance
	TSet<EOwnerTeam> CurrentMembers = GetAllTeamsInAlliance(AllianceName);
	
	// Validate that new team isn't enemies with any current member
	for (EOwnerTeam ExistingMember : CurrentMembers)
	{
		if (AreTeamsEnemies(NewTeam, ExistingMember))
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Cannot add Team %d to alliance '%s' - they are enemies with Team %d (disposition < 0)!"),
				(int32)NewTeam, *AllianceName, (int32)ExistingMember);
			return false;
		}
	}
	
	// Add new team to the alliance
	if (NewTeam == EOwnerTeam::Player)
	{
		PlayerAllianceName = AllianceName;
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Player joined alliance '%s'"), *AllianceName);
	}
	else
	{
		// Find the new team's AI controller
		AAITeamController* NewTeamAI = nullptr;
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == NewTeam)
			{
				NewTeamAI = *It;
				break;
			}
		}
		
		if (!NewTeamAI)
		{
			UE_LOG(LogTemp, Error, TEXT("GameMode: Could not find AI controller for Team %d!"), (int32)NewTeam);
			return false;
		}
		
		NewTeamAI->AllianceState.AllianceName = AllianceName;
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Team %d joined alliance '%s'"), (int32)NewTeam, *AllianceName);
	}
	
	// Cross-link: Add new team to all existing members' AlliedTeams lists
	for (EOwnerTeam ExistingMember : CurrentMembers)
	{
		if (ExistingMember == EOwnerTeam::Player)
		{
			// Player doesn't have AlliedTeams list, skip
			continue;
		}
		
		// Find existing member's AI controller
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == ExistingMember)
			{
				It->AllianceState.AlliedTeams.Add(NewTeam);
				UE_LOG(LogTemp, Log, TEXT("GameMode: Added Team %d to Team %d's allied list"),
					(int32)NewTeam, (int32)ExistingMember);
				break;
			}
		}
	}
	
	// Cross-link: Add all existing members to new team's AlliedTeams list
	if (NewTeam != EOwnerTeam::Player)
	{
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == NewTeam)
			{
				for (EOwnerTeam ExistingMember : CurrentMembers)
				{
					It->AllianceState.AlliedTeams.Add(ExistingMember);
					UE_LOG(LogTemp, Log, TEXT("GameMode: Added Team %d to Team %d's allied list"),
						(int32)ExistingMember, (int32)NewTeam);
				}
				break;
			}
		}
	}
	
	// Boost relationships between new member and all existing members (+40)
	for (EOwnerTeam ExistingMember : CurrentMembers)
	{
		AddAllianceBonus(NewTeam, ExistingMember);
		UE_LOG(LogTemp, Display, TEXT("[FEED] Team %d and Team %d became allies (+40 relationship)"),
			(int32)NewTeam, (int32)ExistingMember);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("GameMode: Successfully added Team %d to alliance '%s' (now has %d members)"),
		(int32)NewTeam, *AllianceName, CurrentMembers.Num() + 1);
	
	return true;
}

FString APlanetConquestGameMode::FormNewAlliance(EOwnerTeam Team1, EOwnerTeam Team2)
{
	// Validate teams aren't enemies
	if (AreTeamsEnemies(Team1, Team2))
	{
		UE_LOG(LogTemp, Error, TEXT("GameMode: Cannot form alliance between Team %d and Team %d - they are enemies!"),
			(int32)Team1, (int32)Team2);
		return TEXT("");
	}
	
	// Generate alliance name
	FString NewAllianceName = GetRandomAllianceName();
	
	UWorld* World = GetWorld();
	if (!World) return TEXT("");
	
	// Set alliance for both teams
	if (Team1 == EOwnerTeam::Player)
	{
		PlayerAllianceName = NewAllianceName;
	}
	else
	{
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == Team1)
			{
				It->AllianceState.AllianceName = NewAllianceName;
				It->AllianceState.AlliedTeams.Add(Team2);
				break;
			}
		}
	}
	
	if (Team2 == EOwnerTeam::Player)
	{
		PlayerAllianceName = NewAllianceName;
	}
	else
	{
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam == Team2)
			{
				It->AllianceState.AllianceName = NewAllianceName;
				It->AllianceState.AlliedTeams.Add(Team1);
				break;
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("GameMode: Formed new alliance '%s' between Team %d and Team %d"),
		*NewAllianceName, (int32)Team1, (int32)Team2);
	
	// Apply alliance bonus between the two founding teams (+40)
	AddAllianceBonus(Team1, Team2);
	UE_LOG(LogTemp, Display, TEXT("[FEED] Alliance '%s' formed between Team %d and Team %d"),
		*NewAllianceName, (int32)Team1, (int32)Team2);
	
	return NewAllianceName;
}

bool APlanetConquestGameMode::AreTeamsEnemies(EOwnerTeam Team1, EOwnerTeam Team2) const
{
	float Disposition = GetDisposition(Team1, Team2);
	return Disposition < 0.0f;
}

void APlanetConquestGameMode::ValidateAllianceIntegrity()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	UE_LOG(LogTemp, Warning, TEXT("GameMode: Validating alliance integrity..."));
	
	// Build a map of alliance name -> teams in that alliance
	TMap<FString, TSet<EOwnerTeam>> AllianceMap;
	
	if (!PlayerAllianceName.IsEmpty())
	{
		AllianceMap.FindOrAdd(PlayerAllianceName).Add(EOwnerTeam::Player);
	}
	
	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (!It->AllianceState.AllianceName.IsEmpty())
		{
			AllianceMap.FindOrAdd(It->AllianceState.AllianceName).Add(It->ControlledTeam);
		}
	}
	
	// Check each alliance for enemy teams
	for (auto& AlliancePair : AllianceMap)
	{
		FString AllianceName = AlliancePair.Key;
		TSet<EOwnerTeam>& TeamsInAlliance = AlliancePair.Value;
		
		TArray<EOwnerTeam> TeamsToRemove;
		
		// Check all pairs of teams in this alliance
		for (EOwnerTeam Team1 : TeamsInAlliance)
		{
			for (EOwnerTeam Team2 : TeamsInAlliance)
			{
				if (Team1 != Team2 && AreTeamsEnemies(Team1, Team2))
				{
					UE_LOG(LogTemp, Error, TEXT("GameMode: ALLIANCE INTEGRITY VIOLATION! Team %d and Team %d are enemies but in same alliance '%s'!"),
						(int32)Team1, (int32)Team2, *AllianceName);
					
					// Remove the team with the lower relationship to the alliance
					// Calculate average relationship of each team to all other alliance members
					float Team1AvgRelationship = 0.0f;
					float Team2AvgRelationship = 0.0f;
					int32 Count = 0;
					
					for (EOwnerTeam OtherTeam : TeamsInAlliance)
					{
						if (OtherTeam != Team1 && OtherTeam != Team2)
						{
							Team1AvgRelationship += GetDisposition(Team1, OtherTeam);
							Team2AvgRelationship += GetDisposition(Team2, OtherTeam);
							Count++;
						}
					}
					
					if (Count > 0)
					{
						Team1AvgRelationship /= Count;
						Team2AvgRelationship /= Count;
					}
					
					// Remove the team with lower average relationship
					EOwnerTeam TeamToRemove = (Team1AvgRelationship < Team2AvgRelationship) ? Team1 : Team2;
					
					if (!TeamsToRemove.Contains(TeamToRemove))
					{
						TeamsToRemove.Add(TeamToRemove);
						UE_LOG(LogTemp, Warning, TEXT("GameMode: Removing Team %d from alliance '%s' (avg relationship: %.1f)"),
							(int32)TeamToRemove, *AllianceName, 
							(TeamToRemove == Team1) ? Team1AvgRelationship : Team2AvgRelationship);
					}
				}
			}
		}
		
		// Remove teams that shouldn't be in this alliance
		for (EOwnerTeam TeamToRemove : TeamsToRemove)
		{
			if (TeamToRemove == EOwnerTeam::Player)
			{
				PlayerAllianceName = TEXT("");
				UE_LOG(LogTemp, Warning, TEXT("GameMode: Removed Player from alliance '%s'"), *AllianceName);
			}
			else
			{
				for (TActorIterator<AAITeamController> It(World); It; ++It)
				{
					if (It->ControlledTeam == TeamToRemove)
					{
						It->AllianceState.AllianceName = TEXT("");
						It->AllianceState.AlliedTeams.Empty();
						UE_LOG(LogTemp, Warning, TEXT("GameMode: Removed Team %d from alliance '%s'"),
							(int32)TeamToRemove, *AllianceName);
						break;
					}
				}
			}
			
			// Remove this team from all other alliance members
			for (EOwnerTeam OtherTeam : TeamsInAlliance)
			{
				if (OtherTeam != TeamToRemove && OtherTeam != EOwnerTeam::Player)
				{
					for (TActorIterator<AAITeamController> It(World); It; ++It)
					{
						if (It->ControlledTeam == OtherTeam)
						{
							It->AllianceState.AlliedTeams.Remove(TeamToRemove);
							break;
						}
					}
				}
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("GameMode: Alliance integrity validation complete."));
}

void APlanetConquestGameMode::PropagateAllianceHatred()
{
	if (bIsPropagatingHatred) return; // Prevent recursion
	bIsPropagatingHatred = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		bIsPropagatingHatred = false;
		return;
	}

	// Build a map of alliance name -> teams in that alliance
	TMap<FString, TSet<EOwnerTeam>> AllianceMap;

	if (!PlayerAllianceName.IsEmpty())
	{
		AllianceMap.FindOrAdd(PlayerAllianceName).Add(EOwnerTeam::Player);
	}

	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		if (!It->AllianceState.AllianceName.IsEmpty())
		{
			AllianceMap.FindOrAdd(It->AllianceState.AllianceName).Add(It->ControlledTeam);
		}
	}
	
	// Dissolve any alliances with only 1 member
	TArray<FString> AlliancesToDissolve;
	for (const auto& AlliancePair : AllianceMap)
	{
		if (AlliancePair.Value.Num() == 1)
		{
			AlliancesToDissolve.Add(AlliancePair.Key);
		}
	}
	
	for (const FString& AllianceName : AlliancesToDissolve)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameMode: Dissolving alliance '%s' - only 1 member remaining"), *AllianceName);
		
		// Get the single member
		TSet<EOwnerTeam>& Members = AllianceMap[AllianceName];
		EOwnerTeam SingleMember = *Members.CreateIterator();
		
		// Clear alliance for this member
		if (SingleMember == EOwnerTeam::Player)
		{
			PlayerAllianceName.Empty();
			UE_LOG(LogTemp, Log, TEXT("  - Player removed from dissolved alliance"));
		}
		else
		{
			for (TActorIterator<AAITeamController> It(World); It; ++It)
			{
				if (It->ControlledTeam == SingleMember)
				{
					It->AllianceState.AllianceName.Empty();
					UE_LOG(LogTemp, Log, TEXT("  - Team %d removed from dissolved alliance"), (int32)SingleMember);
					break;
				}
			}
		}
		
		// Remove from map
		AllianceMap.Remove(AllianceName);
	}

	// Check all pairs of different alliances
	TArray<FString> AllianceNamesList;
	AllianceMap.GetKeys(AllianceNamesList);

	for (int32 i = 0; i < AllianceNamesList.Num(); i++)
	{
		for (int32 j = i + 1; j < AllianceNamesList.Num(); j++)
		{
			FString AllianceA = AllianceNamesList[i];
			FString AllianceB = AllianceNamesList[j];
			TSet<EOwnerTeam>& TeamsA = AllianceMap[AllianceA];
			TSet<EOwnerTeam>& TeamsB = AllianceMap[AllianceB];

			// Check if any member of Alliance A hates any member of Alliance B
			bool bFoundHatred = false;
			for (EOwnerTeam TeamA : TeamsA)
			{
				for (EOwnerTeam TeamB : TeamsB)
				{
					float Disposition = GetDisposition(TeamA, TeamB);
					if (Disposition < -20.0f) // Hatred threshold
					{
						bFoundHatred = true;
						break;
					}
				}
				if (bFoundHatred) break;
			}

			// If hatred found, ensure all members of both alliances hate each other
			if (bFoundHatred)
			{
				UE_LOG(LogTemp, Warning, TEXT("GameMode: Hatred detected between alliances '%s' and '%s' - propagating to all members"),
					*AllianceA, *AllianceB);

				for (EOwnerTeam TeamA : TeamsA)
				{
					for (EOwnerTeam TeamB : TeamsB)
					{
						float CurrentDisposition = GetDisposition(TeamA, TeamB);
						if (CurrentDisposition > -20.0f)
						{
							// Set to -20.0 directly (avoid ModifyDisposition to prevent recursion)
							uint32 Key = MakeRelationshipKey(TeamA, TeamB);
							FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
							Rel.BaseDisposition = -20.0f;
							
							UE_LOG(LogTemp, Log, TEXT("  - Set Team %d <-> Team %d disposition to -20.0 (was %.2f)"),
								(int32)TeamA, (int32)TeamB, CurrentDisposition);
						}
					}
				}
			}
		}
	}
	
	// Also check for unaffiliated teams that hate alliance members
	// If any team outside an alliance hates a member of an alliance, all alliance members should hate that team
	for (const auto& AlliancePair : AllianceMap)
	{
		const FString& AllianceName = AlliancePair.Key;
		const TSet<EOwnerTeam>& AllianceMembers = AlliancePair.Value;
		
		// Check all teams (Player through AI8)
		for (int32 TeamIndex = 0; TeamIndex <= 8; TeamIndex++)
		{
			EOwnerTeam OutsideTeam = (EOwnerTeam)TeamIndex;
			
			// Skip if this team is in the alliance we're checking
			if (AllianceMembers.Contains(OutsideTeam)) continue;
			
			// Check if this outside team hates any alliance member
			bool bOutsideHatesAlliance = false;
			for (EOwnerTeam AllianceMember : AllianceMembers)
			{
				float Disposition = GetDisposition(OutsideTeam, AllianceMember);
				if (Disposition < -20.0f)
				{
					bOutsideHatesAlliance = true;
					break;
				}
			}
			
			// If outside team hates alliance, make all alliance members hate the outside team
			if (bOutsideHatesAlliance)
			{
				for (EOwnerTeam AllianceMember : AllianceMembers)
				{
					float CurrentDisposition = GetDisposition(AllianceMember, OutsideTeam);
					if (CurrentDisposition > -20.0f)
					{
						uint32 Key = MakeRelationshipKey(AllianceMember, OutsideTeam);
						FTeamRelationship& Rel = Relationships.FindOrAdd(Key);
						Rel.BaseDisposition = -20.0f;
						
						UE_LOG(LogTemp, Log, TEXT("GameMode: Team %d (in alliance '%s') now hates Team %d (unaffiliated enemy)"),
							(int32)AllianceMember, *AllianceName, (int32)OutsideTeam);
					}
				}
			}
		}
	}

	bIsPropagatingHatred = false;
}

FString APlanetConquestGameMode::GetPlayerAllianceName() const
{
	if (PlayerAllianceName.IsEmpty())
	{
		return TEXT("No Allegiance");
	}
	return PlayerAllianceName;
}

FString APlanetConquestGameMode::GetTeamAllianceName(EOwnerTeam Team) const
{
	// If querying player's alliance, use player alliance name
	if (Team == EOwnerTeam::Player)
	{
		return GetPlayerAllianceName();
	}

	// Find the AI controller for this team
	UWorld* World = GetWorld();
	if (!World)
	{
		return TEXT("No Allegiance");
	}

	for (TActorIterator<AAITeamController> It(World); It; ++It)
	{
		AAITeamController* Controller = *It;
		if (Controller && Controller->ControlledTeam == Team)
		{
			// Return alliance name if it exists, otherwise "No Allegiance"
			if (Controller->AllianceState.AllianceName.IsEmpty())
			{
				return TEXT("No Allegiance");
			}
			return Controller->AllianceState.AllianceName;
		}
	}

	return TEXT("No Allegiance");
}
