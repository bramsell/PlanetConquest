// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITeamController.h"
#include "../Entities/Cities/CityActor.h"
#include "../Entities/Vehicles/VehicleActor.h"
#include "../Entities/Resources/ResourceActor.h"
#include "../Entities/Buildings/BuildingActor.h"
#include "../Entities/Buildings/FactoryBuildingActor.h"
#include "../Entities/Buildings/TurretBuildingActor.h"
#include "../Entities/Buildings/MineActor.h"
#include "../Entities/Buildings/CapitalBuildingActor.h"
#include "../Entities/Kaiju/KaijuActor.h"
#include "PlanetConquestGameMode.h"
#include "PlanetConquestPlayerController.h"
#include "../UI/TalkDialogueWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AAITeamController::AAITeamController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAITeamController::ApplyArchetypePreset()
{
	// Skip if archetype is set to Custom (manual configuration)
	if (Archetype == EAIArchetype::Custom)
	{
		return;
	}
	
	// Apply preset values based on archetype
	switch (Archetype)
	{
		case EAIArchetype::Warmonger:
			// Aggressive early fighter - short range, fearless against Kaiju, attacks almost anyone, standard income
			P2_1_RangeMultiplier = 3.0f;        // 15k range (stays local, prefers stealing)
			KaijuSafetyDistance = 3000.0f;      // Fearless - will fight Kaiju up close
			P2_3_RelationshipThreshold = 10.0f; // Attacks anyone with relationship ≤ +10
			P2_3_IncomeMultiplier = 1.0f;       // 100/cycle - balanced income needs
			break;
			
		case EAIArchetype::Opportunistic:
			// Balanced aggressor - moderate range, willing to risk Kaiju, attacks neutral/hostile, higher income
			P2_1_RangeMultiplier = 4.0f;        // 20k range (moderate expansion)
			KaijuSafetyDistance = 4000.0f;      // Willing to risk Kaiju for resources
			P2_3_RelationshipThreshold = 0.0f;  // Attacks neutral and hostile teams
			P2_3_IncomeMultiplier = 1.2f;       // 120/cycle - slightly higher needs
			break;
			
		case EAIArchetype::Cautious:
			// Defensive pacifist - long range, cautious around Kaiju, only attacks max-hostile, standard income
			P2_1_RangeMultiplier = 5.0f;         // 25k range (standard expansion)
			KaijuSafetyDistance = 6000.0f;       // Moderately cautious around Kaiju
			P2_3_RelationshipThreshold = -20.0f; // Only attacks max-hostile enemies (essentially pacifist)
			P2_3_IncomeMultiplier = 1.0f;        // 100/cycle - standard needs
			break;
			
		case EAIArchetype::Expansionist:
			// Economic powerhouse - very long range, willing to risk Kaiju, only attacks max-hostile, very high income needs
			P2_1_RangeMultiplier = 6.0f;         // 30k range (long-distance expansion)
			KaijuSafetyDistance = 4000.0f;       // Willing to risk Kaiju for economic gain
			P2_3_RelationshipThreshold = -20.0f; // Only attacks max-hostile enemies
			P2_3_IncomeMultiplier = 1.5f;        // 150/cycle - very income-hungry
			break;
			
		default:
			// Fallback to Opportunistic if something goes wrong
			P2_1_RangeMultiplier = 4.0f;
			KaijuSafetyDistance = 4000.0f;
			P2_3_RelationshipThreshold = 0.0f;
			P2_3_IncomeMultiplier = 1.2f;
			break;
	}
}

void AAITeamController::BeginPlay()
{
	Super::BeginPlay();
	
	// Auto-randomize archetype (unless manually set to Custom)
	if (Archetype != EAIArchetype::Custom)
	{
		int32 RandomArchetype = FMath::RandRange(0, 3);
		switch (RandomArchetype)
		{
			case 0: Archetype = EAIArchetype::Warmonger; break;
			case 1: Archetype = EAIArchetype::Opportunistic; break;
			case 2: Archetype = EAIArchetype::Cautious; break;
			case 3: Archetype = EAIArchetype::Expansionist; break;
			default: Archetype = EAIArchetype::Opportunistic; break;
		}
	}
	
	// Apply archetype preset (unless set to Custom)
	ApplyArchetypePreset();
	
	// Determine personality archetype name for logging
	FString ArchetypeName;
	switch (Archetype)
	{
		case EAIArchetype::Warmonger:     ArchetypeName = TEXT("Warmonger"); break;
		case EAIArchetype::Opportunistic: ArchetypeName = TEXT("Opportunistic"); break;
		case EAIArchetype::Cautious:      ArchetypeName = TEXT("Cautious"); break;
		case EAIArchetype::Expansionist:  ArchetypeName = TEXT("Expansionist"); break;
		case EAIArchetype::Custom:        ArchetypeName = TEXT("Custom"); break;
		default:                          ArchetypeName = TEXT("Unknown"); break;
	}
	
	// UE_LOG(LogTemp, Warning, TEXT("AI Team Controller started for team: %d | Archetype: %s (P2.1Range:%.1f Kaiju:%.0f RelThresh:%.0f IncomeMult:%.2f)"), 
	// 	(int32)ControlledTeam, *ArchetypeName, P2_1_RangeMultiplier, KaijuSafetyDistance, P2_3_RelationshipThreshold, P2_3_IncomeMultiplier);
	
	// Stagger decision cycles to prevent all AI teams from thinking simultaneously
	// This prevents frame spikes every 3 seconds by spreading AI decisions across time
	TimeSinceLastDecision = FMath::FRandRange(0.0f, DecisionInterval);
	UE_LOG(LogTemp, Log, TEXT("AI Team %d: Decision cycle offset by %.2f seconds"), 
		(int32)ControlledTeam, TimeSinceLastDecision);
	
	// Set up income collection timer (every 5 seconds, like player)
	GetWorld()->GetTimerManager().SetTimer(IncomeTimerHandle, this, &AAITeamController::CollectIncome, 5.0f, true);
	
	// Set up black substrate consumption timer (every 1 second)
	GetWorld()->GetTimerManager().SetTimer(ConsumptionTimerHandle, this, &AAITeamController::ConsumeBlackSubstrate, 1.0f, true);
}

void AAITeamController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Check if this team has been eliminated (no cities)
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		int32 CityCount = GameMode->CountCitiesForTeam(ControlledTeam);
		if (CityCount == 0)
		{
			// Team eliminated - stop making decisions
			return;
		}
	}
	
	TimeSinceLastDecision += DeltaTime;
	
	// Make decisions at regular intervals
	if (TimeSinceLastDecision >= DecisionInterval)
	{
		TimeSinceLastDecision = 0.0f;
		MakeDecision();
	}
}

void AAITeamController::MakeDecision()
{
	// Increment decision cycle counter for trade cooldown tracking
	CurrentDecisionCycle++;
	
	// Priority Ladder Architecture (Maslow's Hierarchy for AI)
	// All layers always run except Layer 5 (gated on foundation stable)
	
	// Log decision header for all teams to track resource targeting
	// UE_LOG(LogTemp, Warning, TEXT(""));
	// UE_LOG(LogTemp, Warning, TEXT("=== AI TEAM %d DECISION ==="), (int32)ControlledTeam);
	
	// Populate world state input structs for all layers
	AssessWorldState();
	
	// Layer 1: Survival - Defend cities under active attack (ALWAYS RUNS)
	ExecuteSurvivalLayer();
	
	// Layer 2: Income Generation - Hierarchical resource acquisition (ALWAYS RUNS)
	ExecuteIncomeLayer();
	
	// Layer 3: Defense - Build defenses based on threat proximity (ALWAYS RUNS)
	ExecuteDefenseLayer();
	
	// Layer 3.5: Alliance Formation - Seek allies against max-hostile enemies (ALWAYS RUNS)
	ExecuteAllianceFormation();
	
	// Layer 4: War - Amass vehicles to attack enemy cities (ALWAYS RUNS)
	ExecuteWarLayer();
	
	// Layer 5: Aid - Send resources and military support to allies under attack (ALWAYS RUNS)
	ExecuteAidLayer();
	
	// Detailed AI state logging (only for Team 2 to reduce log spam)
	if (ControlledTeam == EOwnerTeam::AI1)
	{
		// Get all vehicles
		TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
		int32 TotalVehicles = AllVehicles.Num();
		
		// Log stockpile and income
		// UE_LOG(LogTemp, Warning, TEXT("Stockpile: OS:%d  BS:%d"), OrangeSubstrate, BlackSubstrate);
		// UE_LOG(LogTemp, Warning, TEXT("Income/cycle: OS:%d  BS:%d"), OrangeIncomePerCycle, BlackIncomePerCycle);
		// UE_LOG(LogTemp, Warning, TEXT("Total Vehicles: %d"), TotalVehicles);
		
		// --- SUBSTRATE RESERVATION ANALYSIS ---
		// UE_LOG(LogTemp, Warning, TEXT(""));
		// UE_LOG(LogTemp, Warning, TEXT("Substrate Reservations:"));
		if (ReservedOrangeSubstrate > 0)
		{
			// UE_LOG(LogTemp, Warning, TEXT("  Orange: %d reserved (Available: %d)"), 
			// 	ReservedOrangeSubstrate, OrangeSubstrate - ReservedOrangeSubstrate);
		}
		else
		{
			// UE_LOG(LogTemp, Warning, TEXT("  Orange: Not being saved for any priority below"));
		}
		
		if (ReservedBlackSubstrate > 0)
		{
			// UE_LOG(LogTemp, Warning, TEXT("  Black: %d reserved (Available: %d)"), 
			// 	ReservedBlackSubstrate, BlackSubstrate - ReservedBlackSubstrate);
		}
		else
		{
			// UE_LOG(LogTemp, Warning, TEXT("  Black: Not being saved for any priority below"));
		}
		
		// Count vehicles per priority
		struct FPriorityInfo
		{
			FString Name;
			int32 VehicleCount;
			TMap<FString, int32> SubPriorities;
		};
		
		TArray<FPriorityInfo> Priorities;
		Priorities.SetNum(5);
		Priorities[0].Name = TEXT("P1: Survival");
		Priorities[0].VehicleCount = 0;
		Priorities[1].Name = TEXT("P2: Income");
		Priorities[1].VehicleCount = 0;
		Priorities[2].Name = TEXT("P3: Defense");
		Priorities[2].VehicleCount = 0;
		Priorities[3].Name = TEXT("P4: War");
		Priorities[3].VehicleCount = 0;
		Priorities[4].Name = TEXT("P5: Aid");
		Priorities[4].VehicleCount = 0;
		
		// Count vehicles per priority
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			if (!Vehicle) continue;
			
			EVehicleTaskType TaskType = Vehicle->CurrentTask.Type;
			int32 TaskPriority = Vehicle->CurrentTask.Priority;
			
			// P1: Survival
			if (Vehicle->bInP1 || TaskType == EVehicleTaskType::DefendTerritory)
			{
				Priorities[0].VehicleCount++;
				FString SubPriority = FString::Printf(TEXT("DefendTerritory"));
				Priorities[0].SubPriorities.FindOrAdd(SubPriority)++;
			}
			// P2: Income
			else if (TaskType == EVehicleTaskType::SecureIncome || Vehicle->bInP2)
			{
				Priorities[1].VehicleCount++;
				
				FString SubPriority;
				if (Vehicle->CurrentTask.PrimaryTarget.IsValid())
				{
					SubPriority = FString::Printf(TEXT("Intrusion R:%.0f"), Vehicle->CurrentTask.SearchRadius);
				}
				else
				{
					SubPriority = FString::Printf(TEXT("R:%.0f"), Vehicle->CurrentTask.SearchRadius);
				}
				Priorities[1].SubPriorities.FindOrAdd(SubPriority)++;
			}
			// P4: War
			else if (TaskType == EVehicleTaskType::AttackTarget && TaskPriority == 4)
			{
				Priorities[3].VehicleCount++;
				FString SubPriority = TEXT("WarParty");
				Priorities[3].SubPriorities.FindOrAdd(SubPriority)++;
			}
			// P5: Aid
			else if (TaskType == EVehicleTaskType::AidAlly || TaskPriority == 5)
			{
				Priorities[4].VehicleCount++;
				FString SubPriority = TEXT("AidAlly");
				Priorities[4].SubPriorities.FindOrAdd(SubPriority)++;
			}
		}
		
		// Count idle vehicles
		int32 IdleVehicleCount = 0;
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			if (Vehicle && Vehicle->IsIdle())
			{
				IdleVehicleCount++;
			}
		}
		
		// Log priority allocations
		// UE_LOG(LogTemp, Warning, TEXT("Vehicle Allocation:"));
		for (const FPriorityInfo& PriorityInfo : Priorities)
		{
			if (PriorityInfo.VehicleCount > 0 || PriorityInfo.SubPriorities.Num() > 0)
			{
				// UE_LOG(LogTemp, Warning, TEXT("  %s: %d vehicles"), *PriorityInfo.Name, PriorityInfo.VehicleCount);
				
				// Log subpriorities
				for (const TPair<FString, int32>& SubPair : PriorityInfo.SubPriorities)
				{
					// UE_LOG(LogTemp, Warning, TEXT("    - %s: %d"), *SubPair.Key, SubPair.Value);
				}
			}
		}
		
		// Log idle vehicles
		if (IdleVehicleCount > 0)
		{
			// UE_LOG(LogTemp, Warning, TEXT("  Idle: %d vehicles"), IdleVehicleCount);
		}
		
		// Check for unaccounted vehicles
		int32 TotalAllocated = Priorities[0].VehicleCount + Priorities[1].VehicleCount + 
							   Priorities[2].VehicleCount + Priorities[3].VehicleCount + 
							   Priorities[4].VehicleCount + IdleVehicleCount;
		int32 Unaccounted = TotalVehicles - TotalAllocated;
		if (Unaccounted != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("  UNACCOUNTED: %d vehicles"), Unaccounted);
		}
		
		// Log AI-AI relationships
		UWorld* World = GetWorld();
		APlanetConquestGameMode* GameMode = World ? Cast<APlanetConquestGameMode>(World->GetAuthGameMode()) : nullptr;
		if (GameMode)
		{
			// Get all 15 AI-to-AI relationships (6 teams: 2,3,4,5,6,7)
			float Rel_2_3 = GameMode->GetDisposition(EOwnerTeam::AI1, EOwnerTeam::AI2);
			float Rel_2_4 = GameMode->GetDisposition(EOwnerTeam::AI1, EOwnerTeam::AI3);
			float Rel_2_5 = GameMode->GetDisposition(EOwnerTeam::AI1, EOwnerTeam::AI4);
			float Rel_2_6 = GameMode->GetDisposition(EOwnerTeam::AI1, EOwnerTeam::AI5);
			float Rel_2_7 = GameMode->GetDisposition(EOwnerTeam::AI1, EOwnerTeam::AI6);
			float Rel_3_4 = GameMode->GetDisposition(EOwnerTeam::AI2, EOwnerTeam::AI3);
			float Rel_3_5 = GameMode->GetDisposition(EOwnerTeam::AI2, EOwnerTeam::AI4);
			float Rel_3_6 = GameMode->GetDisposition(EOwnerTeam::AI2, EOwnerTeam::AI5);
			float Rel_3_7 = GameMode->GetDisposition(EOwnerTeam::AI2, EOwnerTeam::AI6);
			float Rel_4_5 = GameMode->GetDisposition(EOwnerTeam::AI3, EOwnerTeam::AI4);
			float Rel_4_6 = GameMode->GetDisposition(EOwnerTeam::AI3, EOwnerTeam::AI5);
			float Rel_4_7 = GameMode->GetDisposition(EOwnerTeam::AI3, EOwnerTeam::AI6);
			float Rel_5_6 = GameMode->GetDisposition(EOwnerTeam::AI4, EOwnerTeam::AI5);
			float Rel_5_7 = GameMode->GetDisposition(EOwnerTeam::AI4, EOwnerTeam::AI6);
			float Rel_6_7 = GameMode->GetDisposition(EOwnerTeam::AI5, EOwnerTeam::AI6);
			
			//UE_LOG(LogTemp, Display, TEXT("[AI RELATIONSHIPS] T2<->T3: %.1f | T2<->T4: %.1f | T2<->T5: %.1f | T2<->T6: %.1f | T2<->T7: %.1f"),
				//Rel_2_3, Rel_2_4, Rel_2_5, Rel_2_6, Rel_2_7);
			//UE_LOG(LogTemp, Display, TEXT("[AI RELATIONSHIPS] T3<->T4: %.1f | T3<->T5: %.1f | T3<->T6: %.1f | T3<->T7: %.1f"),
				//Rel_3_4, Rel_3_5, Rel_3_6, Rel_3_7);
			//UE_LOG(LogTemp, Display, TEXT("[AI RELATIONSHIPS] T4<->T5: %.1f | T4<->T6: %.1f | T4<->T7: %.1f"),
				//Rel_4_5, Rel_4_6, Rel_4_7);
			//UE_LOG(LogTemp, Display, TEXT("[AI RELATIONSHIPS] T5<->T6: %.1f | T5<->T7: %.1f"),
				//Rel_5_6, Rel_5_7);
			//UE_LOG(LogTemp, Display, TEXT("[AI RELATIONSHIPS] T6<->T7: %.1f"),
				//Rel_6_7);
		}
	}
}

void AAITeamController::AssessWorldState()
{
	// Clear previous state
	MyGlobalState = FMyGlobalState();
	MyCityStates.Empty();
	OpponentStates.Empty();
	
	// Get territory radius from GameMode
	float TerritoryRadius = 5000.0f;
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		TerritoryRadius = GameMode->TERRITORY_RADIUS;
	}
	
	// ========== POPULATE MY GLOBAL STATE ==========
	MyGlobalState.OrangeSubstrate = OrangeSubstrate;
	MyGlobalState.BlackSubstrate = BlackSubstrate;
	
	// Get all cities and vehicles to avoid repeated queries
	TArray<AActor*> AllCityActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), AllCityActors);
	
	TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
	MyGlobalState.TotalVehicles = AllVehicles.Num();
	
	// Separate my cities and enemy cities
	TArray<ACityActor*> MyCities;
	TArray<ACityActor*> EnemyCities;
	for (AActor* Actor : AllCityActors)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (!City) continue;
		
		if (City->OwnerTeam == ControlledTeam)
			MyCities.Add(City);
		else if (City->OwnerTeam != EOwnerTeam::Neutral)
			EnemyCities.Add(City);
	}
	
	// Get all resources
	TArray<AActor*> AllResourceActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), AllResourceActors);
	
	// Calculate my income and find unclaimed resources
	for (AActor* Actor : AllResourceActors)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource) continue;
		
		// Calculate my income
		if (Resource->OwnerTeam == ControlledTeam)
		{
			if (Resource->ResourceType == EResourceType::OrangeSubstrate)
				MyGlobalState.OrangeIncomePerCycle += Resource->IncomePerInterval;
			else if (Resource->ResourceType == EResourceType::BlackSubstrate)
				MyGlobalState.BlackIncomePerCycle += Resource->IncomePerInterval;
		}
		
		// Find unclaimed resources
		if (Resource->OwnerTeam != ControlledTeam)
		{
			bool bInMyTerritory = false;
			for (ACityActor* City : MyCities)
			{
				if (FVector::Dist(City->GetActorLocation(), Resource->GetActorLocation()) <= TerritoryRadius)
				{
					bInMyTerritory = true;
					break;
				}
			}
			
			if (Resource->OwnerTeam == EOwnerTeam::Neutral)
			{
				if (bInMyTerritory)
					MyGlobalState.UnclaimedResourcesInTerritory.Add(Resource);
				else
					MyGlobalState.UnclaimedResourcesOutsideTerritory.Add(Resource);
			}
		}
	}
	
	// ========== POPULATE MY CITY STATES ==========
	for (ACityActor* City : MyCities)
	{
		FMyCityState CityState;
		CityState.City = City;
		
		FVector CityLocation = City->GetActorLocation();
		
		// Count vehicles and hostile vehicles in territory
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			float Distance = FVector::Dist(CityLocation, Vehicle->GetActorLocation());
			if (Distance <= TerritoryRadius)
			{
				CityState.VehiclesInTerritory++;
			}
		}
		
		// Count hostile vehicles (TODO: check if actively firing)
		TArray<AActor*> AllVehicleActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicleActors);
		for (AActor* Actor : AllVehicleActors)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
			if (!Vehicle || Vehicle->OwnerTeam == ControlledTeam || Vehicle->OwnerTeam == EOwnerTeam::Neutral) continue;
			
			float Distance = FVector::Dist(CityLocation, Vehicle->GetActorLocation());
			if (Distance <= TerritoryRadius)
			{
				// TODO: Check if actively firing (for now, count all enemy vehicles in territory)
				CityState.HostileVehiclesInTerritory++;
				MyGlobalState.bIsUnderActiveAttack = true;
				MyGlobalState.ActiveAttacker = Vehicle->OwnerTeam;
			}
		}
		
		// Count turrets in territory (TODO: when turrets are implemented)
		CityState.TurretsInTerritory = 0; // Placeholder
		
		// Calculate distance to nearest enemy city
		float NearestEnemyDistance = FLT_MAX;
		for (ACityActor* EnemyCity : EnemyCities)
		{
			float Distance = FVector::Dist(CityLocation, EnemyCity->GetActorLocation());
			if (Distance < NearestEnemyDistance)
				NearestEnemyDistance = Distance;
		}
		CityState.DistanceToNearestEnemyCity = NearestEnemyDistance;
		
		// Find enemy resources in my territory (intrusions)
		for (AActor* Actor : AllResourceActors)
		{
			AResourceActor* Resource = Cast<AResourceActor>(Actor);
			if (!Resource || Resource->OwnerTeam == ControlledTeam || Resource->OwnerTeam == EOwnerTeam::Neutral) continue;
			
			float Distance = FVector::Dist(CityLocation, Resource->GetActorLocation());
			if (Distance <= TerritoryRadius)
			{
				if (Resource->ResourceType == EResourceType::OrangeSubstrate)
				{
					CityState.EnemyOrangeInMyTerritory.Add(Resource);
					CityState.TotalOrangeIncomeLostInTerritory += Resource->IncomePerInterval;
				}
				else if (Resource->ResourceType == EResourceType::BlackSubstrate)
				{
					CityState.EnemyBlackInMyTerritory.Add(Resource);
					CityState.TotalBlackIncomeLostInTerritory += Resource->IncomePerInterval;
				}
			}
		}
		
		MyCityStates.Add(CityState);
	}
	
	// ========== POPULATE OPPONENT STATES ==========
	// Group enemy cities by team
	TMap<EOwnerTeam, TArray<ACityActor*>> EnemyCitiesByTeam;
	for (ACityActor* City : EnemyCities)
	{
		if (!EnemyCitiesByTeam.Contains(City->OwnerTeam))
			EnemyCitiesByTeam.Add(City->OwnerTeam, TArray<ACityActor*>());
		
		EnemyCitiesByTeam[City->OwnerTeam].Add(City);
	}
	
	// For each opponent team
	for (auto& Pair : EnemyCitiesByTeam)
	{
		EOwnerTeam OpponentTeam = Pair.Key;
		TArray<ACityActor*>& OpponentCities = Pair.Value;
		
		FOpponentGlobalState OpponentState;
		OpponentState.Team = OpponentTeam;
		
		// Get diplomacy from GameMode
		if (GameMode)
		{
			OpponentState.Disposition = GameMode->GetDisposition(ControlledTeam, OpponentTeam);
			OpponentState.Fear = GameMode->GetFear(ControlledTeam, OpponentTeam);
			OpponentState.Respect = GameMode->GetRespect(ControlledTeam, OpponentTeam);
		}
		
		// Count opponent vehicles
		TArray<AActor*> AllVehicleActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicleActors);
		for (AActor* Actor : AllVehicleActors)
		{
			AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
			if (Vehicle && Vehicle->OwnerTeam == OpponentTeam)
				OpponentState.TotalVehicles++;
		}
		
		// Calculate opponent income and find vulnerable resources
		for (AActor* Actor : AllResourceActors)
		{
			AResourceActor* Resource = Cast<AResourceActor>(Actor);
			if (!Resource || Resource->OwnerTeam != OpponentTeam) continue;
			
			// Add to their income
			if (Resource->ResourceType == EResourceType::OrangeSubstrate)
				OpponentState.OrangeIncomePerCycle += Resource->IncomePerInterval;
			else if (Resource->ResourceType == EResourceType::BlackSubstrate)
				OpponentState.BlackIncomePerCycle += Resource->IncomePerInterval;
			
			// Check if resource is outside their territory (vulnerable)
			bool bInTheirTerritory = false;
			for (ACityActor* City : OpponentCities)
			{
				if (FVector::Dist(City->GetActorLocation(), Resource->GetActorLocation()) <= TerritoryRadius)
				{
					bInTheirTerritory = true;
					break;
				}
			}
			
			if (!bInTheirTerritory)
			{
				if (Resource->ResourceType == EResourceType::OrangeSubstrate)
				{
					OpponentState.OrangeResourcesOutsideTheirTerritory.Add(Resource);
					OpponentState.OrangeIncomeOutsideTheirTerritory += Resource->IncomePerInterval;
				}
				else if (Resource->ResourceType == EResourceType::BlackSubstrate)
				{
					OpponentState.BlackResourcesOutsideTheirTerritory.Add(Resource);
					OpponentState.BlackIncomeOutsideTheirTerritory += Resource->IncomePerInterval;
				}
			}
		}
		
		// Populate opponent city states
		for (ACityActor* OpponentCity : OpponentCities)
		{
			FOpponentCityState OpponentCityState;
			OpponentCityState.City = OpponentCity;
			OpponentCityState.OwnerTeam = OpponentTeam;
			
			FVector OpponentCityLocation = OpponentCity->GetActorLocation();
			
			// Count their vehicles and turrets in their territory
			TArray<AActor*> AllVehicleActors2;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicleActors2);
			for (AActor* Actor : AllVehicleActors2)
			{
				AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
				if (Vehicle && Vehicle->OwnerTeam == OpponentTeam)
				{
					float Distance = FVector::Dist(OpponentCityLocation, Vehicle->GetActorLocation());
					if (Distance <= TerritoryRadius)
						OpponentCityState.VehiclesInTerritory++;
				}
			}
			
			// TODO: Count turrets when implemented
			OpponentCityState.TurretsInTerritory = 0;
			
			// Calculate distance to my closest city
			float ClosestMyCityDistance = FLT_MAX;
			for (ACityActor* MyCity : MyCities)
			{
				float Distance = FVector::Dist(OpponentCityLocation, MyCity->GetActorLocation());
				if (Distance < ClosestMyCityDistance)
					ClosestMyCityDistance = Distance;
			}
			OpponentCityState.DistanceToMyClosestCity = ClosestMyCityDistance;
			
			// Find resources in their territory
			for (AActor* Actor : AllResourceActors)
			{
				AResourceActor* Resource = Cast<AResourceActor>(Actor);
				if (!Resource) continue;
				
				float Distance = FVector::Dist(OpponentCityLocation, Resource->GetActorLocation());
				if (Distance <= TerritoryRadius)
				{
					// Unclaimed resources in their territory
					if (Resource->OwnerTeam == EOwnerTeam::Neutral)
					{
						OpponentCityState.UnclaimedResourcesInTheirTerritory.Add(Resource);
					}
					// Their owned resources in their territory
					else if (Resource->OwnerTeam == OpponentTeam)
					{
						if (Resource->ResourceType == EResourceType::OrangeSubstrate)
						{
							OpponentCityState.OrangeResourcesInTheirTerritory.Add(Resource);
							OpponentCityState.OrangeIncomeInTheirTerritory += Resource->IncomePerInterval;
						}
						else if (Resource->ResourceType == EResourceType::BlackSubstrate)
						{
							OpponentCityState.BlackResourcesInTheirTerritory.Add(Resource);
							OpponentCityState.BlackIncomeInTheirTerritory += Resource->IncomePerInterval;
						}
					}
				}
			}
			
			OpponentState.Cities.Add(OpponentCityState);
		}
		
		OpponentStates.Add(OpponentState);
	}
	
	// Sync income tracking to member variables (for trade system and other priority layers)
	OrangeIncomePerCycle = MyGlobalState.OrangeIncomePerCycle;
	BlackIncomePerCycle = MyGlobalState.BlackIncomePerCycle;
}

// Helper function: Get available vehicles for defense
TArray<AVehicleActor*> AAITeamController::GetDefenders(int32 Count)
{
	TArray<AVehicleActor*> Defenders;
	TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
	
	// Priority 1 can preempt lower priorities, so check CanPreempt
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (!Vehicle) continue;
		
		// Can use this vehicle if it can be preempted by P1 (priority 1)
		if (Vehicle->CanPreempt(1))
		{
			Defenders.Add(Vehicle);
			if (Defenders.Num() >= Count) break;
		}
	}
	
	return Defenders;
}

// Helper function: Build defensive vehicles
int32 AAITeamController::BuildDefenders(int32 Count)
{
	int32 Built = 0;
	const int32 VEHICLE_COST = 1000;
	
	TArray<ACityActor*> Cities = GetControlledCities();
	if (Cities.Num() == 0) return 0;
	
	ACityActor* BuildCity = Cities[0]; // Build at first city
	
	while (Built < Count && OrangeSubstrate >= VEHICLE_COST)
	{
		if (BuildVehicleAtCity(BuildCity))
		{
			Built++;
		}
		else
		{
			break; // Can't build more
		}
	}
	
	return Built;
}

void AAITeamController::ExecuteSurvivalLayer()
{
	// ========== PRIORITY 1: ACTIVE THREAT DETECTION & DEFENSE ==========
	// New system: Scans for hostile vehicles actively threatening our buildings
	// Threat levels (P1.1-P1.4): Capital (3:1) > Turrets (2:1) > Territory Mines (2:1) > Extended Mines (1:1)
	// Active threat = vehicle fired within 15s OR within firing range of building
	
	// Only log for Team 2 (AI1) to reduce log spam
	bool bShouldLog = false;
	
	const float TERRITORY_RADIUS = 5000.0f;
	const float EXTENDED_DEFENSE_RADIUS = 10000.0f;
	const float FIRING_RANGE = 1800.0f;
	const float THREAT_TIMEOUT = 15.0f; // Attacker considered active for 15s after last shot
	const int32 VEHICLE_COST = 1000;
	
	// === P1 STATUS SUMMARY (before processing) ===
	if (bShouldLog)
	{
		int32 P1Vehicles = 0;
		TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			if (Vehicle && (Vehicle->bInP1 || Vehicle->CurrentTask.Type == EVehicleTaskType::DefendTerritory))
			{
				P1Vehicles++;
			}
		}
		
		// UE_LOG(LogTemp, Warning, TEXT(""));
		// UE_LOG(LogTemp, Warning, TEXT("--- P1: SURVIVAL STATUS ---"));
		// UE_LOG(LogTemp, Warning, TEXT("Scanning for threats to %d cities..."), MyCityStates.Num());
		// UE_LOG(LogTemp, Warning, TEXT("Currently %d vehicles on defense duty"), P1Vehicles);
	}
	
	// Process each city independently for defense
	for (const FMyCityState& CityState : MyCityStates)
	{
		if (!CityState.City || !IsValid(CityState.City)) continue;
		
		FVector CityLocation = CityState.City->GetActorLocation();
		FString CityName = CityState.City->GetName();
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P1 - Evaluating threats for city '%s'"), (int32)ControlledTeam, *CityName);
		}
		
		// ===== STEP 1: SCAN FOR ACTIVE HOSTILE VEHICLES =====
		
		TArray<FDefenseThreat> CriticalThreats; // P1.1: Capital under attack
		TArray<FDefenseThreat> HighThreats;     // P1.2: Turrets under attack
		TArray<FDefenseThreat> MediumThreats;   // P1.3: Territory mines under attack
		TArray<FDefenseThreat> LowThreats;      // P1.4: Extended mines under attack
		
		// Get all our buildings in extended defense range
		TArray<AActor*> AllBuildingActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABuildingActor::StaticClass(), AllBuildingActors);
		
		TArray<ABuildingActor*> OurBuildings;
		for (AActor* Actor : AllBuildingActors)
		{
			ABuildingActor* Building = Cast<ABuildingActor>(Actor);
			if (!Building || !IsValid(Building) || Building->OwnerTeam != ControlledTeam) continue;
			
			float DistToCity = FVector::Dist(CityLocation, Building->GetActorLocation());
			if (DistToCity > EXTENDED_DEFENSE_RADIUS) continue;
			
			OurBuildings.Add(Building);
		}
		
		// Scan for hostile vehicles in extended range
		TArray<AActor*> AllVehicleActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicleActors);
		
		for (AActor* Actor : AllVehicleActors)
		{
			AVehicleActor* HostileVehicle = Cast<AVehicleActor>(Actor);
			if (!HostileVehicle || !IsValid(HostileVehicle)) continue;
			if (HostileVehicle->OwnerTeam == ControlledTeam) continue; // Not hostile
			
			// Check if vehicle is within extended defense radius
			float DistToCity = FVector::Dist(CityLocation, HostileVehicle->GetActorLocation());
			if (DistToCity > EXTENDED_DEFENSE_RADIUS) continue;
			
			// Check if vehicle is actively threatening (fired within 15s)
			bool bRecentlyAttacked = (HostileVehicle->TimeSinceLastAttack < THREAT_TIMEOUT);
			
			// Check what this hostile vehicle is threatening
			ABuildingActor* ThreatenedBuilding = nullptr;
			float ClosestDist = FIRING_RANGE;
			
			for (ABuildingActor* Building : OurBuildings)
			{
				float Dist = FVector::Dist(HostileVehicle->GetActorLocation(), Building->GetActorLocation());
				if (Dist < ClosestDist)
				{
					ClosestDist = Dist;
					ThreatenedBuilding = Building;
				}
			}
			
			// Only count as threat if recently attacked OR within firing range
			if (!bRecentlyAttacked && ClosestDist >= FIRING_RANGE) continue;
			if (!ThreatenedBuilding) continue;
			
			// Classify threat by building type
			EThreatLevel ThreatLevel = EThreatLevel::None;
			float BuildingDistToCity = FVector::Dist(CityLocation, ThreatenedBuilding->GetActorLocation());
			
			if (Cast<ACapitalBuildingActor>(ThreatenedBuilding))
			{
				ThreatLevel = EThreatLevel::Critical; // P1.1: Capital
			}
			else if (Cast<ATurretBuildingActor>(ThreatenedBuilding))
			{
				ThreatLevel = EThreatLevel::High; // P1.2: Turret
			}
			else if (Cast<AMineActor>(ThreatenedBuilding))
			{
				if (BuildingDistToCity <= TERRITORY_RADIUS)
				{
					ThreatLevel = EThreatLevel::Medium; // P1.3: Territory mine
				}
				else
				{
					ThreatLevel = EThreatLevel::Low; // P1.4: Extended mine
				}
			}
			
			if (ThreatLevel == EThreatLevel::None) continue;
			
			// Add to appropriate threat list
			TArray<FDefenseThreat>* ThreatList = nullptr;
			switch (ThreatLevel)
			{
				case EThreatLevel::Critical: ThreatList = &CriticalThreats; break;
				case EThreatLevel::High:     ThreatList = &HighThreats; break;
				case EThreatLevel::Medium:   ThreatList = &MediumThreats; break;
				case EThreatLevel::Low:      ThreatList = &LowThreats; break;
				default: break;
			}
			
			if (ThreatList)
			{
				// Find existing threat for this building or create new one
				FDefenseThreat* ExistingThreat = ThreatList->FindByPredicate([ThreatenedBuilding](const FDefenseThreat& T) {
					return T.Building == ThreatenedBuilding;
				});
				
				if (ExistingThreat)
				{
					// Add attacker to existing threat
					ExistingThreat->Attackers.AddUnique(HostileVehicle);
				}
				else
				{
					// Create new threat
					FDefenseThreat NewThreat;
					NewThreat.Level = ThreatLevel;
					NewThreat.Building = ThreatenedBuilding;
					NewThreat.DefendingCity = CityState.City;
					NewThreat.DistanceFromCity = BuildingDistToCity;
					NewThreat.AttackingTeam = HostileVehicle->OwnerTeam;
					NewThreat.Attackers.Add(HostileVehicle);
					ThreatList->Add(NewThreat);
				}
			}
		}
		
		// Scan for hostile kaiju (they attack all teams)
		TArray<AActor*> AllKaijuActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AKaijuActor::StaticClass(), AllKaijuActors);
		
		for (AActor* Actor : AllKaijuActors)
		{
			AKaijuActor* Kaiju = Cast<AKaijuActor>(Actor);
			if (!Kaiju || !IsValid(Kaiju)) continue;
			
			// Check if kaiju is within extended defense radius
			float DistToCity = FVector::Dist(CityLocation, Kaiju->GetActorLocation());
			if (DistToCity > EXTENDED_DEFENSE_RADIUS) continue;
			
			// Check if kaiju is firing or has a target
			bool bKaijuThreatening = Kaiju->bIsFiring || (Kaiju->CurrentTarget && IsValid(Kaiju->CurrentTarget));
			
			// Check what this kaiju is threatening
			ABuildingActor* ThreatenedBuilding = nullptr;
			float ClosestDist = Kaiju->AttackRange + 500.0f; // Slightly larger than attack range
			
			// Check if kaiju has a building as current target
			if (Kaiju->CurrentTarget && IsValid(Kaiju->CurrentTarget))
			{
				ABuildingActor* TargetBuilding = Cast<ABuildingActor>(Kaiju->CurrentTarget);
				if (TargetBuilding && TargetBuilding->OwnerTeam == ControlledTeam)
				{
					ThreatenedBuilding = TargetBuilding;
				}
			}
			
			// If no direct target, find closest building
			if (!ThreatenedBuilding)
			{
				for (ABuildingActor* Building : OurBuildings)
				{
					float Dist = FVector::Dist(Kaiju->GetActorLocation(), Building->GetActorLocation());
					if (Dist < ClosestDist)
					{
						ClosestDist = Dist;
						ThreatenedBuilding = Building;
					}
				}
			}
			
			// Only count as threat if kaiju is actively threatening or close to buildings
			if (!bKaijuThreatening && ClosestDist > Kaiju->AttackRange) continue;
			if (!ThreatenedBuilding) continue;
			
			// Classify threat by building type (kaiju are always high priority)
			EThreatLevel ThreatLevel = EThreatLevel::None;
			float BuildingDistToCity = FVector::Dist(CityLocation, ThreatenedBuilding->GetActorLocation());
			
			if (Cast<ACapitalBuildingActor>(ThreatenedBuilding))
			{
				ThreatLevel = EThreatLevel::Critical; // P1.1: Kaiju attacking capital
			}
			else if (Cast<ATurretBuildingActor>(ThreatenedBuilding))
			{
				ThreatLevel = EThreatLevel::High; // P1.2: Kaiju attacking turret
			}
			else if (Cast<AMineActor>(ThreatenedBuilding))
			{
				if (BuildingDistToCity <= TERRITORY_RADIUS)
				{
					ThreatLevel = EThreatLevel::Medium; // P1.3: Kaiju attacking territory mine
				}
				else
				{
					ThreatLevel = EThreatLevel::Low; // P1.4: Kaiju attacking extended mine
				}
			}
			
			if (ThreatLevel == EThreatLevel::None) continue;
			
			// Add to appropriate threat list
			TArray<FDefenseThreat>* ThreatList = nullptr;
			switch (ThreatLevel)
			{
				case EThreatLevel::Critical: ThreatList = &CriticalThreats; break;
				case EThreatLevel::High:     ThreatList = &HighThreats; break;
				case EThreatLevel::Medium:   ThreatList = &MediumThreats; break;
				case EThreatLevel::Low:      ThreatList = &LowThreats; break;
				default: break;
			}
			
			if (ThreatList)
			{
				// Find existing threat for this building or create new one
				FDefenseThreat* ExistingThreat = ThreatList->FindByPredicate([ThreatenedBuilding](const FDefenseThreat& T) {
					return T.Building == ThreatenedBuilding;
				});
				
				if (ExistingThreat)
				{
					// Add kaiju to existing threat
					ExistingThreat->Attackers.AddUnique(Kaiju);
				}
				else
				{
					// Create new threat
					FDefenseThreat NewThreat;
					NewThreat.Level = ThreatLevel;
					NewThreat.Building = ThreatenedBuilding;
					NewThreat.DefendingCity = CityState.City;
					NewThreat.DistanceFromCity = BuildingDistToCity;
					NewThreat.AttackingTeam = EOwnerTeam::Neutral; // Kaiju don't have teams
					NewThreat.Attackers.Add(Kaiju);
					ThreatList->Add(NewThreat);
				}
			}
		}
		
		// ===== STEP 2: COUNT EXISTING TURRET DEFENDERS =====
		// Turrets count as defenders for nearby threatened buildings
		
		auto CountNearbyTurrets = [&](ABuildingActor* Building) -> int32
		{
			if (!Building) return 0;
			
			int32 TurretCount = 0;
			float TurretRange = 2500.0f; // Turrets defend within 2500 units
			
			for (ABuildingActor* OurBuilding : OurBuildings)
			{
				if (Cast<ATurretBuildingActor>(OurBuilding))
				{
					float Dist = FVector::Dist(Building->GetActorLocation(), OurBuilding->GetActorLocation());
					if (Dist <= TurretRange)
					{
						TurretCount++;
					}
				}
			}
			return TurretCount;
		};
		
		// Calculate defenders needed (attackers × ratio - nearby turrets)
		for (FDefenseThreat& Threat : CriticalThreats)
		{
			Threat.CalculateDefendersNeeded();
			int32 NearbyTurrets = CountNearbyTurrets(Threat.Building);
			Threat.DefendersNeeded = FMath::Max(0, Threat.DefendersNeeded - NearbyTurrets);
		}
		for (FDefenseThreat& Threat : HighThreats)
		{
			Threat.CalculateDefendersNeeded();
			int32 NearbyTurrets = CountNearbyTurrets(Threat.Building);
			Threat.DefendersNeeded = FMath::Max(0, Threat.DefendersNeeded - NearbyTurrets);
		}
		for (FDefenseThreat& Threat : MediumThreats)
		{
			Threat.CalculateDefendersNeeded();
			int32 NearbyTurrets = CountNearbyTurrets(Threat.Building);
			Threat.DefendersNeeded = FMath::Max(0, Threat.DefendersNeeded - NearbyTurrets);
		}
		for (FDefenseThreat& Threat : LowThreats)
		{
			Threat.CalculateDefendersNeeded();
			int32 NearbyTurrets = CountNearbyTurrets(Threat.Building);
			Threat.DefendersNeeded = FMath::Max(0, Threat.DefendersNeeded - NearbyTurrets);
		}
		
		// ===== STEP 2.5: COUNT EXISTING DEFENDERS =====
		// Count vehicles already assigned to defend each building to prevent duplicate assignments
		TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
		
		auto CountExistingDefenders = [&](ABuildingActor* Building) -> int32
		{
			if (!Building) return 0;
			
			int32 ExistingDefenders = 0;
			FVector BuildingLocation = Building->GetActorLocation();
			const float PROXIMITY_THRESHOLD = 1000.0f; // Consider vehicle a defender if within 1000 units of destination
			
			for (AVehicleActor* Vehicle : AllVehicles)
			{
				if (!Vehicle || !IsValid(Vehicle)) continue;
				
				// Only count vehicles on DefendTerritory tasks
				if (Vehicle->CurrentTask.Type != EVehicleTaskType::DefendTerritory) continue;
				
				// Check if vehicle's task destination matches this building's location
				float DistToDest = FVector::Dist(Vehicle->CurrentTask.Destination, BuildingLocation);
				if (DistToDest <= PROXIMITY_THRESHOLD)
				{
					ExistingDefenders++;
				}
			}
			
			return ExistingDefenders;
		};
		
		// Add existing defenders to DefendersAssigned count
		for (FDefenseThreat& Threat : CriticalThreats)
		{
			Threat.DefendersAssigned = CountExistingDefenders(Threat.Building);
		}
		for (FDefenseThreat& Threat : HighThreats)
		{
			Threat.DefendersAssigned = CountExistingDefenders(Threat.Building);
		}
		for (FDefenseThreat& Threat : MediumThreats)
		{
			Threat.DefendersAssigned = CountExistingDefenders(Threat.Building);
		}
		for (FDefenseThreat& Threat : LowThreats)
		{
			Threat.DefendersAssigned = CountExistingDefenders(Threat.Building);
		}
		
		int32 TotalCritical = CriticalThreats.Num();
		int32 TotalHigh = HighThreats.Num();
		int32 TotalMedium = MediumThreats.Num();
		int32 TotalLow = LowThreats.Num();
		
		if (TotalCritical + TotalHigh + TotalMedium + TotalLow == 0)
		{
			if (bShouldLog)
			{
				UE_LOG(LogTemp, Log, TEXT("AI Team %d: P1 - City '%s' is SAFE (no threats)"), (int32)ControlledTeam, *CityName);
			}
			continue; // No threats for this city
		}
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - City '%s' UNDER ATTACK! Threats: %d P1.1 (capital), %d P1.2 (turrets), %d P1.3 (territory), %d P1.4 (extended)"),
				(int32)ControlledTeam, *CityName, TotalCritical, TotalHigh, TotalMedium, TotalLow);
		}
		
		// ===== STEP 3: ALLOCATE DEFENDERS HIERARCHICALLY (P1.1->P1.2->P1.3->P1.4) =====
		// Process each threat level (Critical â†’ High â†’ Medium â†’ Low)
		// Each level can pull from lower defense subpriorities
		
		TArray<AVehicleActor*> AvailableDefenders; // Pool of vehicles P1 can use
		
		// P1 can pull from: idle vehicles + any lower priority task (P2-P6)
		// Critical threats (capital under attack) can preempt ANY lower priority work
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			if (!Vehicle || !IsValid(Vehicle)) continue;
			
			// Idle vehicles always available
			if (Vehicle->IsIdle())
			{
				AvailableDefenders.Add(Vehicle);
				continue;
			}
			
			// P1 can preempt lower priorities (P2, P3, P4, P5, P6)
			int32 VehiclePriority = Vehicle->CurrentTask.Priority;
			if (VehiclePriority >= 2) // Any priority 2 or higher can be preempted
			{
				AvailableDefenders.Add(Vehicle);
			}
		}
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - '%s' has %d vehicles available for defense (pulling from idle + P2-P6)"),
				(int32)ControlledTeam, *CityName, AvailableDefenders.Num());
		}
		
		// Lambda: Try to get N defenders from available pool
		auto GetDefenders = [&](int32 Count) -> TArray<AVehicleActor*>
		{
			TArray<AVehicleActor*> Assigned;
			for (int32 i = 0; i < FMath::Min(Count, AvailableDefenders.Num()); i++)
			{
				Assigned.Add(AvailableDefenders[i]);
			}
			// Remove assigned from pool
			for (AVehicleActor* V : Assigned)
			{
				AvailableDefenders.Remove(V);
			}
			return Assigned;
		};
		
		// Lambda: Try to build defenders
		auto BuildDefenders = [&](int32 Count) -> int32
		{
			int32 Built = 0;
			while (Built < Count && OrangeSubstrate >= VEHICLE_COST)
			{
				BuildVehicleAtCity(CityState.City);
				Built++;
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - Built emergency defender for '%s'"),
					(int32)ControlledTeam, *CityName);
			}
			return Built;
		};
		
		// Lambda: Assign DefendTerritory task
		auto AssignDefenseTask = [&](AVehicleActor* Defender, const FDefenseThreat& Threat)
		{
			if (!Defender || !IsValid(Defender)) return;
			
			FVehicleTask Task;
			Task.Type = EVehicleTaskType::DefendTerritory;
			Task.Priority = 1; // P1
			Task.AssigningController = this;
			Task.MinesToDestroy.Add(Threat.AttackingTeam);
			Task.bAutonomousContinue = true; // Keep hunting
			
			// Set initial target
			if (Threat.Attackers.Num() > 0 && Threat.Attackers[0] && IsValid(Threat.Attackers[0]))
			{
				Task.PrimaryTarget = Threat.Attackers[0];
			}
			
			// Set search parameters based on threat level
			Task.Destination = Threat.Building->GetActorLocation();
			
			switch (Threat.Level)
			{
				case EThreatLevel::Critical:
				case EThreatLevel::High:
					// Territory-wide hunt for capital/turret defense
					Task.SearchRadius = TERRITORY_RADIUS;
					UE_LOG(LogTemp, Display, TEXT("AI Team %d: P1 - Assigning TERRITORY-WIDE defense (radius %.0f)"),
						(int32)ControlledTeam, TERRITORY_RADIUS);
					break;
					
				case EThreatLevel::Medium:
					// Perimeter defense for territory mines
					Task.SearchRadius = TERRITORY_RADIUS + FIRING_RANGE;
					UE_LOG(LogTemp, Display, TEXT("AI Team %d: P1 - Assigning PERIMETER defense (radius %.0f)"),
						(int32)ControlledTeam, TERRITORY_RADIUS + FIRING_RANGE);
					break;
					
				case EThreatLevel::Low:
					// Cluster defense for extended mines
					Task.ClusterID = Threat.ClusterID;
					Task.SearchRadius = Threat.ClusterRadius + FIRING_RANGE; // TODO: Calculate cluster radius
					UE_LOG(LogTemp, Display, TEXT("AI Team %d: P1 - Assigning CLUSTER defense (cluster %d)"),
						(int32)ControlledTeam, Threat.ClusterID);
					break;
					
				default:
					Task.SearchRadius = TERRITORY_RADIUS;
					break;
			}
			
			Defender->AssignTask(Task);
			Priority1Vehicles.Add(Defender);
		};
		
		// Process threats by level (Critical â†’ High â†’ Medium â†’ Low)
		TArray<TArray<FDefenseThreat>*> ThreatLevels = { &CriticalThreats, &HighThreats, &MediumThreats, &LowThreats };
		TArray<FString> LevelNames = { TEXT("CRITICAL"), TEXT("HIGH"), TEXT("MEDIUM"), TEXT("LOW") };
		
		for (int32 LevelIdx = 0; LevelIdx < ThreatLevels.Num(); LevelIdx++)
		{
			TArray<FDefenseThreat>& Threats = *ThreatLevels[LevelIdx];
			if (Threats.Num() == 0) continue;
			
			FString LevelName = LevelNames[LevelIdx];
			int32 TotalNeeded = 0;
			for (const FDefenseThreat& T : Threats) { TotalNeeded += T.DefendersNeeded; }
			
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - Processing %s threats (%d threats, %d defenders needed)"),
				(int32)ControlledTeam, *LevelName, Threats.Num(), TotalNeeded);
			
			// Assign threats in this level
			for (FDefenseThreat& Threat : Threats)
			{
				// Calculate how many more defenders we need (accounting for those already assigned)
				int32 StillNeeded = Threat.DefendersNeeded - Threat.DefendersAssigned;
				
				// Skip if already adequately defended
				if (StillNeeded <= 0) continue;
				
				// Try to get defenders from idle pool
				TArray<AVehicleActor*> Assigned = GetDefenders(StillNeeded);
				Threat.DefendersAssigned += Assigned.Num();
				
				// Assign defense tasks
				for (AVehicleActor* Defender : Assigned)
				{
					AssignDefenseTask(Defender, Threat);
				}
				
				StillNeeded = Threat.DefendersNeeded - Threat.DefendersAssigned;
				
				// If still need defenders, try to BUILD
				if (StillNeeded > 0)
				{
					int32 Built = BuildDefenders(StillNeeded);
					// Built vehicles will be idle and picked up next decision cycle
					StillNeeded -= Built;
				}
				
				// If STILL can't defend and this is a P1.1, P1.2, or P1.3 threat, escalate
				bool bCriticalThreat = (Threat.Level == EThreatLevel::Critical || 
				                       Threat.Level == EThreatLevel::High || 
				                       Threat.Level == EThreatLevel::Medium);
				
				if (StillNeeded > 0 && bCriticalThreat)
				{
					FString ThreatLabel = (Threat.Level == EThreatLevel::Critical) ? TEXT("P1.1 CAPITAL") :
					                     (Threat.Level == EThreatLevel::High) ? TEXT("P1.2 TURRET") : TEXT("P1.3 TERRITORY");
					
					// Try emergency trading if low on funds
					if (OrangeSubstrate < VEHICLE_COST * StillNeeded)
					{
						UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - %s THREAT! Need %d more defenders but only have %d orange"),
							(int32)ControlledTeam, *ThreatLabel, StillNeeded, OrangeSubstrate);
						
						bool bTradeSuccess = RequestTradeFromAI(true, 1000);
						if (bTradeSuccess)
						{
							UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - Emergency trade successful! Now have %d orange"),
								(int32)ControlledTeam, OrangeSubstrate);
							
							// Try building again
							int32 PostTradeBuilt = BuildDefenders(StillNeeded);
							StillNeeded -= PostTradeBuilt;
						}
					}
					
					// If STILL need defenders (after trade or if had funds), request aid
					if (StillNeeded > 0)
					{
						int32 SubstrateNeeded = StillNeeded * VEHICLE_COST;
						bool bAidRequested = RequestAidFromAllies(CityState.City, true, true, SubstrateNeeded);
						if (bAidRequested)
						{
							UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P1 - %s THREAT! Requested EMERGENCY AID from allies (need %d defenders, %d orange)"),
								(int32)ControlledTeam, *ThreatLabel, StillNeeded, SubstrateNeeded);
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("AI Team %d: P1 - %s THREAT! CRITICAL! Cannot defend - need %d more defenders, no allies available!"),
								(int32)ControlledTeam, *ThreatLabel, StillNeeded);
						}
					}
				}
			}
		}
	} // End per-city loop
	
	// ===== STEP 4: RELEASE DEFENDERS WHEN AREA CLEAR =====
	// TODO Phase 2: Check if defenders' search areas are clear of enemies
	// For now, keep existing release logic
	
	// Safely clean up invalid vehicles from Priority1Vehicles
	TArray<AVehicleActor*> ValidDefenders;
	for (AVehicleActor* V : Priority1Vehicles)
	{
		if (V && IsValid(V))
		{
			ValidDefenders.Add(V);
		}
	}
	Priority1Vehicles = ValidDefenders;
	
	// Check if any P1 vehicles have completed their missions (no enemies in search area)
	TArray<AVehicleActor*> VehiclesToRelease;
	for (AVehicleActor* Defender : Priority1Vehicles)
	{
		if (!Defender || !IsValid(Defender)) continue;
		
		// If vehicle's task is complete (Type == None), release it
		if (Defender->CurrentTask.Type == EVehicleTaskType::None)
		{
			VehiclesToRelease.Add(Defender);
		}
	}
	
	for (AVehicleActor* Vehicle : VehiclesToRelease)
	{
		Priority1Vehicles.Remove(Vehicle);
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: P1 - Released defender (mission complete)"), (int32)ControlledTeam);
	}
}

void AAITeamController::ExecuteIncomeLayer()
{
	// Priority 2: Income Generation - Hierarchical resource acquisition
	// Subpriorities (highest to lowest):
	// 2.1 Easy Income: Capture unguarded unclaimed resources (filtered by Kaiju safety)
	// 2.2 Intrusion: Reclaim enemy-owned resources in our territory
	// 2.3 Mine Attack: Destroy enemy mines to capture resources (when income insufficient)
	// 2.4 Kaiju Clusters: Capture Kaiju-guarded unclaimed resources (when income insufficient)
	// Note: 2.3 and 2.4 compete for vehicles, with 2.3 having priority (safer option)
	
	// Economic actions: Trade for black substrate if low
	if (BlackSubstrate < 200)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: P2 Income - Low black substrate (%d), attempting to trade for 1000..."),
			(int32)ControlledTeam, BlackSubstrate);
		
		bool bTradeSuccess = RequestTradeFromAI(false, 1000);
		if (bTradeSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2 Income - Trade successful! Now have %d black substrate"),
				(int32)ControlledTeam, BlackSubstrate);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2 Income - Trade failed, no available partners"),
				(int32)ControlledTeam);
		}
	}
	
	// Get game mode for relationship checks
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	
	// Get territory and expansion radii
	float TERRITORY_RADIUS = 5000.0f;
	if (GameMode)
	{
		TERRITORY_RADIUS = GameMode->TERRITORY_RADIUS;
	}
	// P2.1 Easy Income radius: Territory radius * P2_1_RangeMultiplier (personality-based)
	const float EASY_INCOME_RADIUS = TERRITORY_RADIUS * P2_1_RangeMultiplier;
	
	// P2.3 Mine Attack radius: Same as easy income
	const float MINE_ATTACK_RADIUS = EASY_INCOME_RADIUS;
	
	// Get all controlled cities and vehicles
	TArray<ACityActor*> Cities = GetControlledCities();
	if (Cities.Num() == 0) return;
	
	TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
	if (AllVehicles.Num() == 0) return;
	
	// Get all resources and mines
	TArray<AActor*> AllResourceActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), AllResourceActors);
	
	TArray<AActor*> AllMineActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMineActor::StaticClass(), AllMineActors);
	
	// Helper lambda to find closest city distance
	auto GetClosestCityDistance = [&](FVector Location) -> float
	{
		float MinDist = FLT_MAX;
		for (ACityActor* City : Cities)
		{
			float Dist = FVector::Dist(City->GetActorLocation(), Location);
			if (Dist < MinDist) MinDist = Dist;
		}
		return MinDist;
	};
	
	// Helper lambda to find mine protecting a resource
	auto FindProtectingMine = [&](AResourceActor* Resource) -> AMineActor*
	{
		for (AActor* Actor : AllMineActors)
		{
			AMineActor* Mine = Cast<AMineActor>(Actor);
			if (Mine && Mine->TargetResource == Resource)
			{
				return Mine;
			}
		}
		return nullptr;
	};
	
	// Helper to check if a resource is already being targeted by another vehicle
	auto IsResourceTargeted = [&](AResourceActor* Resource) -> bool
	{
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			if (Vehicle && Vehicle->CurrentTask.Type == EVehicleTaskType::SecureIncome)
			{
				if (Vehicle->CurrentTask.Resource == Resource)
				{
					return true; // Already assigned
				}
			}
		}
		return false;
	};
	
	// Helper to check if a mine is already being targeted by another vehicle
	auto IsMineTargeted = [&](AMineActor* Mine) -> bool
	{
		if (!Mine) return false;
		
		for (AVehicleActor* Vehicle : AllVehicles)
		{
			if (Vehicle && Vehicle->CurrentTask.Type == EVehicleTaskType::SecureIncome)
			{
				// Check if this mine is the primary target
				AMineActor* TargetMine = Cast<AMineActor>(Vehicle->CurrentTask.PrimaryTarget);
				if (TargetMine == Mine)
				{
					return true; // Already assigned
				}
			}
		}
		return false;
	};
	
	// ========== RADIUS COMPLETION CHECKS: Release vehicles from completed radius tasks ==========
	// These checks run FIRST so vehicles can be freed before being collected as idle
	
	// Check EASY_INCOME_RADIUS (20000) completion
	TArray<AVehicleActor*> EasyIncomeVehicles;
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (Vehicle->CurrentTask.Type == EVehicleTaskType::SecureIncome && 
			Vehicle->CurrentTask.Priority == 2 &&
			FMath::IsNearlyEqual(Vehicle->CurrentTask.SearchRadius, EASY_INCOME_RADIUS, 100.0f))
		{
			EasyIncomeVehicles.Add(Vehicle);
		}
	}
	
	// If we have vehicles on easy income tasks, check if there are any resources left to capture
	if (EasyIncomeVehicles.Num() > 0)
	{
		bool bHasEasyIncomeTargets = false;
		
		for (AActor* Actor : AllResourceActors)
		{
			AResourceActor* Resource = Cast<AResourceActor>(Actor);
			if (!Resource || Resource->OwnerTeam != EOwnerTeam::Neutral) continue;
			
			// Skip if being captured by another team
			if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != ControlledTeam) continue;
			
			// Skip if already targeted
			if (IsResourceTargeted(Resource)) continue;
			
			// Check if within easy income radius
			float DistToCity = GetClosestCityDistance(Resource->GetActorLocation());
			if (DistToCity <= EASY_INCOME_RADIUS)
			{
				// Skip if protected by mine (handled by P2.3)
				AMineActor* ProtectingMine = FindProtectingMine(Resource);
				if (!ProtectingMine)
				{
					bHasEasyIncomeTargets = true;
					break;
				}
			}
		}
		
		// If no more easy income targets, release those vehicles to become idle
		if (!bHasEasyIncomeTargets)
		{
			for (AVehicleActor* Vehicle : EasyIncomeVehicles)
			{
				Vehicle->ClearTask();
				//UE_LOG(LogTemp, Log, TEXT("AI Team %d: Released vehicle from completed Easy Income radius (R:%.0f) - now idle"),
				//	(int32)ControlledTeam, EASY_INCOME_RADIUS);
			}
		}
	}
	
	// Find truly idle vehicles (not in P1, no current task)
	// NOTE: Vehicles already on P2 tasks have bAutonomousContinue=true and stay on their tier
	// NOTE: Vehicles released from completed radius tasks above are now idle and will be collected here
	TArray<AVehicleActor*> IdleVehicles;
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (Vehicle && Vehicle->IsIdle() && !Vehicle->bInP1)
		{
			IdleVehicles.Add(Vehicle);
		}
	}
	
	if (IdleVehicles.Num() == 0) return; // No idle vehicles for income tasks

	// ========== P2.1 EASY INCOME: Capture UNGUARDED neutral resources (ALWAYS RUNS, sorted by closest) ==========
	// Filters out Kaiju-guarded clusters - those are handled by P2.4 only when income is insufficient
	// Limit to max 3 vehicles for easy income to preserve vehicles for other priorities
	const int32 MAX_EASY_INCOME_VEHICLES = 3;
	
	// Count vehicles ALREADY working on easy income tasks
	int32 EasyIncomeVehiclesAssigned = 0;
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (Vehicle->CurrentTask.Type == EVehicleTaskType::SecureIncome && 
			Vehicle->CurrentTask.Priority == 2 &&
			FMath::IsNearlyEqual(Vehicle->CurrentTask.SearchRadius, EASY_INCOME_RADIUS, 100.0f))
		{
			EasyIncomeVehiclesAssigned++;
		}
	}
	
	// Collect and sort neutral resources by distance (closest first)
	TArray<TPair<float, AResourceActor*>> EasyIncomeResources;
	for (AActor* Actor : AllResourceActors)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource || Resource->OwnerTeam != EOwnerTeam::Neutral) continue;
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != ControlledTeam) continue;
		
		// Skip if already targeted by another vehicle
		if (IsResourceTargeted(Resource)) continue;
		
		// Must be within easy income radius
		float DistToCity = GetClosestCityDistance(Resource->GetActorLocation());
		if (DistToCity > EASY_INCOME_RADIUS) continue;
		
		// Check if protected by any mine (enemy or ally)
		AMineActor* ProtectingMine = FindProtectingMine(Resource);
		if (ProtectingMine) continue; // Skip if mined (handled by P2.3)
		
		// Skip if cluster is Kaiju-guarded (these are handled by P2.4)
		if (Resource->bKaijuGuarded) continue;
		
		EasyIncomeResources.Add(TPair<float, AResourceActor*>(DistToCity, Resource));
	}
	
	// Sort by distance (closest first)
	EasyIncomeResources.Sort([](const TPair<float, AResourceActor*>& A, const TPair<float, AResourceActor*>& B) {
		return A.Key < B.Key;
	});
	
	// Assign easy income tasks
	for (const TPair<float, AResourceActor*>& Pair : EasyIncomeResources)
	{
		if (EasyIncomeVehiclesAssigned >= MAX_EASY_INCOME_VEHICLES) break;
		if (IdleVehicles.Num() == 0) return;
		
		AResourceActor* Resource = Pair.Value;
		float DistToCity = Pair.Key;
		
		AVehicleActor* Vehicle = IdleVehicles[0];
		IdleVehicles.RemoveAt(0);
		
		FVehicleTask Task;
		Task.Type = EVehicleTaskType::SecureIncome;
		Task.Priority = 2;
		Task.Resource = Resource;
		Task.SearchRadius = EASY_INCOME_RADIUS;
		Task.bAutonomousContinue = true;
		Task.bCaptureNeutralOnly = true;
		Task.SubPriorityLabel = TEXT("P2.1");
		Task.AssigningController = this;
		
		Vehicle->AssignTask(Task);
		EasyIncomeVehiclesAssigned++;
		//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2.1 Easy Income - Capturing neutral %s (%.0f from city) [%d/%d vehicles]"),
		//	(int32)ControlledTeam, *Resource->GetName(), DistToCity, EasyIncomeVehiclesAssigned, MAX_EASY_INCOME_VEHICLES);
	}
	
	// ========== P2.2 INTRUSION: Reclaim enemy-owned resources in territory (sorted by closest) ==========
	// Independent of income threshold - always defend territory from hostile/neutral teams (relationship <= 0.0)
	TArray<TPair<float, AResourceActor*>> IntrusionResources;
	for (AActor* Actor : AllResourceActors)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource) continue;
		
		// Must be enemy-owned (not ours, not neutral)
		if (Resource->OwnerTeam == ControlledTeam || Resource->OwnerTeam == EOwnerTeam::Neutral) continue;
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != ControlledTeam) continue;
		
		// Skip if already targeted by another vehicle
		if (IsResourceTargeted(Resource)) continue;
		
		// Must be within territory
		float DistToCity = GetClosestCityDistance(Resource->GetActorLocation());
		if (DistToCity > TERRITORY_RADIUS) continue;
		
		// Only target teams with relationship <= 0.0 (hostile or neutral)
		float Relationship = GameMode->GetDisposition(ControlledTeam, Resource->OwnerTeam);
		if (Relationship > 0.0f) continue;
		
		// Skip if cluster is Kaiju-guarded (too risky for territory defense)
		if (Resource->bKaijuGuarded) continue;
		
		IntrusionResources.Add(TPair<float, AResourceActor*>(DistToCity, Resource));
	}
	
	// Sort by distance (closest first)
	IntrusionResources.Sort([](const TPair<float, AResourceActor*>& A, const TPair<float, AResourceActor*>& B) {
		return A.Key < B.Key;
	});
	
	// Assign intrusion tasks
	for (const TPair<float, AResourceActor*>& Pair : IntrusionResources)
	{
		if (IdleVehicles.Num() == 0) return;
		
		AResourceActor* Resource = Pair.Value;
		float DistToCity = Pair.Key;
		
		float Relationship = GameMode->GetDisposition(ControlledTeam, Resource->OwnerTeam);
		AMineActor* EnemyMine = FindProtectingMine(Resource);
		
		// Skip if this mine is already being targeted by another vehicle
		if (EnemyMine && IsMineTargeted(EnemyMine))
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P2.2 Intrusion - Skipping %s (mine already targeted by another vehicle)"),
				(int32)ControlledTeam, *Resource->GetName());
			continue;
		}
		
		AVehicleActor* Vehicle = IdleVehicles[0];
		IdleVehicles.RemoveAt(0);
		
		FVehicleTask Task;
		Task.Type = EVehicleTaskType::SecureIncome;
		Task.Priority = 2; // P2 Income
		Task.Resource = Resource;
		Task.PrimaryTarget = EnemyMine; // May be nullptr if no mine
		Task.SearchRadius = TERRITORY_RADIUS;
		Task.bAutonomousContinue = true; // Keep finding more resources
		Task.bCaptureNeutralOnly = false; // Can capture enemy resources
		Task.MinesToDestroy.Add(Resource->OwnerTeam); // Attack this team's mines
		Task.SubPriorityLabel = TEXT("P2.2");
		Task.AssigningController = this;
		
		Vehicle->AssignTask(Task);
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2.2 Intrusion - Reclaiming %s from Team %d (%.0f from city, relationship:%.1f, mine: %s)"),
			(int32)ControlledTeam, *Resource->GetName(), (int32)Resource->OwnerTeam, DistToCity, Relationship,
			EnemyMine ? TEXT("YES") : TEXT("NO"));
	}
	
	// ========== P2.3 MINE ATTACK: Destroy enemy mines within search radius (sorted by closest) ==========
	// Only attack mines if we need that resource type to reach income threshold (personality-based)
	int32 NumCities = MyCityStates.Num();
	if (NumCities == 0) return; // No cities, can't expand
	
	int32 RequiredOrangeIncome = FMath::RoundToInt(NumCities * 100.0f * P2_3_IncomeMultiplier);
	int32 RequiredBlackIncome = FMath::RoundToInt(NumCities * 100.0f * P2_3_IncomeMultiplier);
	
	bool bNeedOrangeIncome = (OrangeIncomePerCycle < RequiredOrangeIncome);
	bool bNeedBlackIncome = (BlackIncomePerCycle < RequiredBlackIncome);
	
	// Skip if both income requirements are met
	if (!bNeedOrangeIncome && !bNeedBlackIncome)
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2.3 Mine Attack - Skipping (income satisfied: Orange:%d/%d Black:%d/%d)"),
			(int32)ControlledTeam, OrangeIncomePerCycle, RequiredOrangeIncome, BlackIncomePerCycle, RequiredBlackIncome);
		return;
	}
	
	// Attack mines from hostile/neutral relationship teams (relationship <= 0.0)
	TArray<TPair<float, AMineActor*>> EnemyMines;
	
	for (AActor* Actor : AllMineActors)
	{
		AMineActor* Mine = Cast<AMineActor>(Actor);
		if (!Mine) continue;
		
		// Must be enemy-owned (not ours, not neutral)
		if (Mine->OwnerTeam == ControlledTeam || Mine->OwnerTeam == EOwnerTeam::Neutral) continue;
		
		// Must be within mine attack radius
		float DistToCity = GetClosestCityDistance(Mine->GetActorLocation());
		if (DistToCity > MINE_ATTACK_RADIUS) continue;
		
		// Only target teams below relationship threshold (personality-based hostility tolerance)
		float Relationship = GameMode->GetDisposition(ControlledTeam, Mine->OwnerTeam);
		if (Relationship > P2_3_RelationshipThreshold) continue;
		
		// Filter by resource type: only attack mines on resources we need
		if (Mine->TargetResource)
		{
			if (Mine->TargetResource->ResourceType == EResourceType::OrangeSubstrate && !bNeedOrangeIncome) continue;
			if (Mine->TargetResource->ResourceType == EResourceType::BlackSubstrate && !bNeedBlackIncome) continue;
			
			// Skip if target resource is being captured by another team
			if (Mine->TargetResource->CapturingTeam != EOwnerTeam::Neutral && Mine->TargetResource->CapturingTeam != ControlledTeam) continue;
			
			// Skip if resource cluster is Kaiju-guarded (too risky for mine attacks)
			if (Mine->TargetResource->bKaijuGuarded) continue;
		}
		else
		{
			// Skip mines without target resources (shouldn't happen, but safety check)
			continue;
		}
		
		EnemyMines.Add(TPair<float, AMineActor*>(DistToCity, Mine));
	}
	
	// Sort by distance (closest first)
	EnemyMines.Sort([](const TPair<float, AMineActor*>& A, const TPair<float, AMineActor*>& B) {
		return A.Key < B.Key;
	});
	
	// Assign mine attack tasks
	for (const TPair<float, AMineActor*>& Pair : EnemyMines)
	{
		if (IdleVehicles.Num() == 0) return;
		
		AMineActor* Mine = Pair.Value;
		float DistToCity = Pair.Key;
		
		// Skip if this mine is already being targeted by another vehicle
		if (IsMineTargeted(Mine))
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P2.3 Mine Attack - Skipping mine at %s (already targeted by another vehicle)"),
				(int32)ControlledTeam, *Mine->GetName());
			continue;
		}
		
		float Relationship = GameMode->GetDisposition(ControlledTeam, Mine->OwnerTeam);
		
		AVehicleActor* Vehicle = IdleVehicles[0];
		IdleVehicles.RemoveAt(0);
		
		FVehicleTask Task;
		Task.Type = EVehicleTaskType::SecureIncome; // Mine destruction + resource capture
		Task.Priority = 2; // P2 Income
		Task.PrimaryTarget = Mine; // Destroy this mine first
		Task.Resource = Mine->TargetResource; // Then capture this resource
		Task.SearchRadius = MINE_ATTACK_RADIUS;
		Task.bAutonomousContinue = true; // Keep finding more mines
		Task.MinesToDestroy.Add(Mine->OwnerTeam); // Attack this team's mines
		Task.SubPriorityLabel = TEXT("P2.3");
		Task.AssigningController = this;
		
		Vehicle->AssignTask(Task);
		
		FString ResourceTypeName = (Mine->TargetResource && Mine->TargetResource->ResourceType == EResourceType::OrangeSubstrate) 
			? TEXT("Orange") : TEXT("Black");
		
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2.3 Mine Attack - Destroying %s mine from Team %d at %s (%.0f from city, relationship:%.1f)"),
			(int32)ControlledTeam, *ResourceTypeName, (int32)Mine->OwnerTeam, *Mine->GetName(), DistToCity, Relationship);
	}
	
	// ========== P2.4 KAIJU CLUSTERS: Capture Kaiju-guarded unclaimed resources (sorted by closest) ==========
	// Only attempt if we need that resource type to reach income threshold
	// This runs AFTER P2.3 so AI prefers attacking enemy mines over risking Kaiju encounters
	
	// Use same income checks as P2.3
	if (!bNeedOrangeIncome && !bNeedBlackIncome)
	{
		return; // Income satisfied, no need for Kaiju clusters
	}
	
	TArray<TPair<float, AResourceActor*>> KaijuGuardedResources;
	
	for (AActor* Actor : AllResourceActors)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource) continue;
		
		// Must be unclaimed
		if (Resource->OwnerTeam != EOwnerTeam::Neutral) continue;
		
		// Must be within search radius
		float DistToCity = GetClosestCityDistance(Resource->GetActorLocation());
		if (DistToCity > EASY_INCOME_RADIUS) continue;
		
		// Filter by resource type: only target resources we need
		if (Resource->ResourceType == EResourceType::OrangeSubstrate && !bNeedOrangeIncome) continue;
		if (Resource->ResourceType == EResourceType::BlackSubstrate && !bNeedBlackIncome) continue;
		
		// Skip if being captured by another team
		if (Resource->CapturingTeam != EOwnerTeam::Neutral && Resource->CapturingTeam != ControlledTeam) continue;
		
		// ONLY include Kaiju-guarded clusters (opposite of P2.1 filter)
		if (!Resource->bKaijuGuarded) continue;
		
		// Skip if resource is protected by a mine
		AMineActor* ProtectingMine = FindProtectingMine(Resource);
		if (ProtectingMine) continue;
		
		KaijuGuardedResources.Add(TPair<float, AResourceActor*>(DistToCity, Resource));
	}
	
	// Sort by distance (closest first)
	KaijuGuardedResources.Sort([](const TPair<float, AResourceActor*>& A, const TPair<float, AResourceActor*>& B) {
		return A.Key < B.Key;
	});
	
	// Assign Kaiju cluster capture tasks
	for (const TPair<float, AResourceActor*>& Pair : KaijuGuardedResources)
	{
		if (IdleVehicles.Num() == 0) return;
		
		AResourceActor* Resource = Pair.Value;
		float DistToCity = Pair.Key;
		
		AVehicleActor* Vehicle = IdleVehicles[0];
		IdleVehicles.RemoveAt(0);
		
		FVehicleTask Task;
		Task.Type = EVehicleTaskType::SecureIncome;
		Task.Priority = 2; // P2 Income
		Task.Resource = Resource;
		Task.SearchRadius = EASY_INCOME_RADIUS;
		Task.bAutonomousContinue = true;
		Task.SubPriorityLabel = TEXT("P2.4");
		Task.AssigningController = this;
		
		Vehicle->AssignTask(Task);
		
		FString ResourceTypeName = (Resource->ResourceType == EResourceType::OrangeSubstrate) ? TEXT("Orange") : TEXT("Black");
		
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P2.4 Kaiju Cluster - Capturing Kaiju-guarded %s resource (%.0f from city, cluster:%d) [%d/%d vehicles]"),
			(int32)ControlledTeam, *ResourceTypeName, DistToCity, Resource->ClusterID, (AllVehicles.Num() - IdleVehicles.Num()), AllVehicles.Num());
	}
}

void AAITeamController::OnCityCountChanged()
{
	// Count current cities to determine new substrate requirements
	TArray<AActor*> AllCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), AllCities);
	
	int32 CityCount = 0;
	for (AActor* Actor : AllCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == ControlledTeam)
		{
			CityCount++;
		}
	}
	
	if (CityCount > 0)
	{
		int32 RequiredOrangeIncome = CityCount * 100;
		int32 RequiredBlackIncome = CityCount * 100;
		
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: City count changed to %d cities. New substrate requirements: %d orange/cycle, %d black/cycle (current: %d orange, %d black)"),
			(int32)ControlledTeam, CityCount, RequiredOrangeIncome, RequiredBlackIncome,
			MyGlobalState.OrangeIncomePerCycle, MyGlobalState.BlackIncomePerCycle);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AI Team %d: ELIMINATED - No cities remaining!"), (int32)ControlledTeam);
	}
}

void AAITeamController::ExecuteDefenseLayer()
{
	// Priority 3: Defense - Build turrets based on proximity threat
	// SIMPLIFIED: Only builds turrets, no vehicle assignment
	// Count enemy vehicles from teams with relationship <= -20 within 25000 units
	// Build 1 turret per 2 enemy vehicles, max 8 turrets total
	
	// Only log for Team 2 (AI1) to reduce log spam
	bool bShouldLog = false;
	
	// Get game mode for relationship checks
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode)
	{
		return;
	}
	
	// === P3 STATUS ANALYSIS ===
	int32 TotalHostileVehicles = 0;
	int32 TotalTurretsNeeded = 0;
	int32 TotalCurrentTurrets = 0;
	int32 TotalTurretDeficit = 0;
	
	// Scan all cities for threats first
	for (FMyCityState& CityState : MyCityStates)
	{
		if (!CityState.City || !IsValid(CityState.City)) continue;
		
		FVector CityLocation = CityState.City->GetActorLocation();
		int32 HostileVehiclesNearby = 0;
		
		// Count enemy vehicles from teams with relationship <= -20 within 25000 units
		TArray<AActor*> AllVehicleActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicleActors);
		
		for (AActor* Actor : AllVehicleActors)
		{
			AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
			if (!EnemyVehicle || EnemyVehicle->OwnerTeam == ControlledTeam) continue;
			
			// Check relationship
			float Relationship = GameMode->GetDisposition(ControlledTeam, EnemyVehicle->OwnerTeam);
			if (Relationship > -20.0f) continue; // Not hostile enough
			
			// Check distance
			float Distance = FVector::Dist(CityLocation, EnemyVehicle->GetActorLocation());
			if (Distance < 25000.0f)
			{
				HostileVehiclesNearby++;
			}
		}
		
		// Calculate turrets needed: minimum 2, plus 1 per 2 hostile vehicles, max 8
		int32 ThreatBasedTurrets = HostileVehiclesNearby / 2;
		int32 CityTurretsNeeded = FMath::Clamp(FMath::Max(2, ThreatBasedTurrets), 2, 8);
		int32 CurrentTurrets = CityState.City->GetTurretCount();
		int32 CityDeficit = FMath::Max(0, CityTurretsNeeded - CurrentTurrets);
		
		TotalHostileVehicles += HostileVehiclesNearby;
		TotalTurretsNeeded += CityTurretsNeeded;
		TotalCurrentTurrets += CurrentTurrets;
		TotalTurretDeficit += CityDeficit;
	}
	
	if (bShouldLog)
	{
		UE_LOG(LogTemp, Warning, TEXT(""));
		UE_LOG(LogTemp, Warning, TEXT("--- P3: DEFENSE STATUS ---"));
		UE_LOG(LogTemp, Warning, TEXT("Hostile vehicles nearby: %d"), TotalHostileVehicles);
		UE_LOG(LogTemp, Warning, TEXT("Turrets: %d current, %d needed, %d deficit"), 
			TotalCurrentTurrets, TotalTurretsNeeded, TotalTurretDeficit);
		
		if (TotalTurretDeficit > 0)
		{
			int32 TurretCost = 2000;
			int32 TotalCost = TotalTurretDeficit * TurretCost;
			UE_LOG(LogTemp, Warning, TEXT("Need to build %d turrets (Cost: %d OS, Have: %d OS)"), 
				TotalTurretDeficit, TotalCost, OrangeSubstrate);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Defense is adequate - no turrets needed"));
		}
	}
	
	// For each of our cities, count hostile enemy vehicles within extended territory (25000 units)
	for (FMyCityState& CityState : MyCityStates)
	{
		if (!CityState.City || !IsValid(CityState.City)) continue;
		
		FVector CityLocation = CityState.City->GetActorLocation();
		int32 HostileVehiclesNearby = 0;
		
		// Count enemy vehicles from teams with relationship <= -20 within 25000 units
		TArray<AActor*> AllVehicleActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicleActors);
		
		for (AActor* Actor : AllVehicleActors)
		{
			AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
			if (!EnemyVehicle || EnemyVehicle->OwnerTeam == ControlledTeam) continue;
			
			// Check relationship
			float Relationship = GameMode->GetDisposition(ControlledTeam, EnemyVehicle->OwnerTeam);
			if (Relationship > -20.0f) continue; // Not hostile enough
			
			// Check distance
			float Distance = FVector::Dist(CityLocation, EnemyVehicle->GetActorLocation());
			if (Distance < 25000.0f)
			{
				HostileVehiclesNearby++;
			}
		}
		
		// Calculate turrets needed: minimum 2, plus 1 per 2 hostile vehicles, max 8
		int32 ThreatBasedTurrets = HostileVehiclesNearby / 2;
		int32 TurretsNeeded = FMath::Clamp(FMath::Max(2, ThreatBasedTurrets), 2, 8);
		int32 CurrentTurrets = CityState.City->GetTurretCount();
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P3 Defense - City '%s' has %d hostile vehicles nearby, needs %d turrets (has %d)"),
				(int32)ControlledTeam, *CityState.City->GetName(), HostileVehiclesNearby, TurretsNeeded, CurrentTurrets);
		}
		
		if (CurrentTurrets >= TurretsNeeded)
		{
			continue; // Already have enough turrets
		}
		
		// Build turrets up to the needed amount
		int32 TurretDeficit = TurretsNeeded - CurrentTurrets;
		int32 TurretCost = 2000;
		
		// Reserve orange substrate for turrets we plan to build
		ReservedOrangeSubstrate += TurretDeficit * TurretCost;
		
		while (TurretDeficit > 0 && OrangeSubstrate >= TurretCost)
		{
			BuildTurretAtCity(CityState.City);
			//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defense - Built defensive TURRET at '%s' (%d/%d, %d hostile nearby)"),
				//(int32)ControlledTeam, *CityState.City->GetName(), CurrentTurrets + 1, TurretsNeeded, HostileVehiclesNearby);
			TurretDeficit--;
			CurrentTurrets++;
		}
		
		// EMERGENCY TRADING: If we still need turrets but can't afford them, try to trade for orange
		if (TurretDeficit > 0 && OrangeSubstrate < TurretCost)
		{
			//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defense - City '%s' needs %d more turrets but only has %d orange, attempting to trade for 1000..."),
				//(int32)ControlledTeam, *CityState.City->GetName(), TurretDeficit, OrangeSubstrate);
			
			bool bTradeSuccess = RequestTradeFromAI(true, 1000); // Request 1000 orange
			if (bTradeSuccess)
			{
				//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defense - Emergency trade successful! Now have %d orange"),
					//(int32)ControlledTeam, OrangeSubstrate);
				
				// Try building turrets again after trade
				while (TurretDeficit > 0 && OrangeSubstrate >= TurretCost)
				{
					BuildTurretAtCity(CityState.City);
					//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defense - Built defensive TURRET (post-trade) at '%s'"),
						//(int32)ControlledTeam, *CityState.City->GetName());
					TurretDeficit--;
				}
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defense - Emergency trade failed, no available partners"),
					//(int32)ControlledTeam);
				continue;
			}
		}
	}
	
	// === P3 DEFENSIVE DIPLOMACY: BRIBE POTENTIAL ALLIES ===
	// If we have max-hostile enemies (-100 or worse), try to bribe neutral/friendly teams to improve relations
	// Strategy: Find strongest neutral team (income + vehicles) weighted by proximity, bribe with excess substrate
	// Once relationship reaches +20, alliance formation will happen in ExecuteAllianceFormation()
	
	// STEP 1: Check if we have any max-hostile enemies
	TArray<EOwnerTeam> MaxHostileEnemies;
	for (const FOpponentGlobalState& OpponentState : OpponentStates)
	{
		float Relationship = GameMode->GetDisposition(ControlledTeam, OpponentState.Team);
		if (Relationship <= -100.0f)
		{
			MaxHostileEnemies.Add(OpponentState.Team);
		}
	}
	
	if (MaxHostileEnemies.Num() == 0)
	{
		// No max-hostile enemies, no need for defensive diplomacy
		return;
	}
	
	if (bShouldLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defensive Diplomacy - %d max-hostile enemies detected, seeking allies..."),
			(int32)ControlledTeam, MaxHostileEnemies.Num());
	}
	
	// STEP 2: Find best third-party team to bribe (neutral or better, not already allied, not max-hostile)
	struct FBribeTarget
	{
		EOwnerTeam Team;
		float Score;
		float Proximity;
		int32 Income;
		int32 Vehicles;
		float Relationship;
	};
	
	TArray<FBribeTarget> PotentialTargets;
	
	for (const FOpponentGlobalState& OpponentState : OpponentStates)
	{
		// Skip if max-hostile or already allied
		if (MaxHostileEnemies.Contains(OpponentState.Team) || IsAlliedWith(OpponentState.Team))
		{
			continue;
		}
		
		float Relationship = GameMode->GetDisposition(ControlledTeam, OpponentState.Team);
		
		// Only bribe teams with neutral or better relationship (>= 0.2)
		if (Relationship < 0.2f)
		{
			continue;
		}
		
		// Check bribe cooldown (10 decision cycles = 30 seconds)
		if (LastBribeAttemptCycle.Contains(OpponentState.Team))
		{
			int32 LastBribe = LastBribeAttemptCycle[OpponentState.Team];
			if (CurrentDecisionCycle - LastBribe < 10)
			{
				continue; // Still on cooldown
			}
		}
		
		// Calculate strength: income (orange weighted 1.5x higher) + vehicles
		int32 WeightedIncome = OpponentState.OrangeIncomePerCycle * 3 / 2 + OpponentState.BlackIncomePerCycle;
		int32 Strength = WeightedIncome + OpponentState.TotalVehicles;
		
		// Calculate proximity: find closest city to any of their cities
		float ClosestCityDistance = FLT_MAX;
		for (const FMyCityState& MyCity : MyCityStates)
		{
			if (!MyCity.City || !IsValid(MyCity.City)) continue;
			
			for (const FOpponentCityState& TheirCity : OpponentState.Cities)
			{
				if (!TheirCity.City || !IsValid(TheirCity.City)) continue;
				
				float Distance = FVector::Dist(MyCity.City->GetActorLocation(), TheirCity.City->GetActorLocation());
				ClosestCityDistance = FMath::Min(ClosestCityDistance, Distance);
			}
		}
		
		// Proximity weight: closer teams score higher (inverse distance, normalized to 0-1 range)
		// Max distance on planet ~= 60000 units, so divide by this for normalization
		float ProximityWeight = 1.0f - FMath::Clamp(ClosestCityDistance / 60000.0f, 0.0f, 1.0f);
		
		// Final score: Strength * ProximityWeight
		float Score = Strength * (0.5f + ProximityWeight * 0.5f); // 50% base + 50% proximity bonus
		
		FBribeTarget Target;
		Target.Team = OpponentState.Team;
		Target.Score = Score;
		Target.Proximity = ClosestCityDistance;
		Target.Income = WeightedIncome;
		Target.Vehicles = OpponentState.TotalVehicles;
		Target.Relationship = Relationship;
		
		PotentialTargets.Add(Target);
	}
	
	if (PotentialTargets.Num() == 0)
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defensive Diplomacy - No valid bribe targets (all on cooldown or hostile)"),
				(int32)ControlledTeam);
		}
		return;
	}
	
	// Sort by score (highest first)
	PotentialTargets.Sort([](const FBribeTarget& A, const FBribeTarget& B) {
		return A.Score > B.Score;
	});
	
	// STEP 3: Attempt to bribe the best target
	FBribeTarget BestTarget = PotentialTargets[0];
	
	// Decide which substrate to use (whichever we have more of, but need 2000)
	bool bUseOrange = (OrangeSubstrate >= 2000 && OrangeSubstrate >= BlackSubstrate);
	bool bUseBlack = (BlackSubstrate >= 2000 && BlackSubstrate > OrangeSubstrate);
	
	if (!bUseOrange && !bUseBlack)
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defensive Diplomacy - Insufficient substrate for bribe (need 2000, have OS:%d BS:%d)"),
				(int32)ControlledTeam, OrangeSubstrate, BlackSubstrate);
		}
		return;
	}
	
	// Update cooldown regardless of attempt result
	LastBribeAttemptCycle.Add(BestTarget.Team, CurrentDecisionCycle);
	
	bool bSuccess = false;
	
	// Handle bribing Player vs AI teams differently
	if (BestTarget.Team == EOwnerTeam::Player)
	{
		// Queue bribe request for player (they respond via dialogue - no RNG)
		GameMode->QueueAIBribeRequest(ControlledTeam, bUseOrange);
		
		// Deduct substrate immediately (player will receive it when they accept)
		if (bUseOrange)
		{
			OrangeSubstrate -= 2000;
		}
		else
		{
			BlackSubstrate -= 2000;
		}
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defensive Diplomacy - Offered Player bribe (2000 %s substrate) - awaiting response"),
				(int32)ControlledTeam, bUseOrange ? TEXT("Orange") : TEXT("Black"));
		}
		
		// Don't attempt alliance with player here - RequestAlliance will handle that separately
		return;
	}
	else
	{
		// Attempt AI-to-AI bribe with 65% RNG success
		bSuccess = GameMode->AIAttemptBribe(ControlledTeam, BestTarget.Team, bUseOrange, OrangeSubstrate, BlackSubstrate);
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defensive Diplomacy - %s bribe to AI Team %d (Score:%.0f Income:%d Vehicles:%d Dist:%.0f Rel:%.1f->%.1f) with %s"),
				(int32)ControlledTeam, 
				bSuccess ? TEXT("SUCCESSFUL") : TEXT("FAILED"),
				(int32)BestTarget.Team,
				BestTarget.Score,
				BestTarget.Income,
				BestTarget.Vehicles,
				BestTarget.Proximity,
				BestTarget.Relationship,
				GameMode->GetDisposition(ControlledTeam, BestTarget.Team),
				bUseOrange ? TEXT("Orange") : TEXT("Black"));
		}
	}
	
	// STEP 4: If relationship now >= +20, try to form alliance (AI-to-AI only)
	if (bSuccess)
	{
		float NewRelationship = GameMode->GetDisposition(ControlledTeam, BestTarget.Team);
		if (NewRelationship >= 20.0f && !IsAlliedWith(BestTarget.Team))
		{
			// Find a common enemy to propose alliance against
			EOwnerTeam CommonEnemy = EOwnerTeam::Neutral;
			for (EOwnerTeam HostileTeam : MaxHostileEnemies)
			{
				// Check if the bribed team also dislikes this hostile team
				float TheirRelationship = GameMode->GetDisposition(BestTarget.Team, HostileTeam);
				if (TheirRelationship <= -20.0f) // They also dislike them
				{
					CommonEnemy = HostileTeam;
					break;
				}
			}
			
			if (CommonEnemy != EOwnerTeam::Neutral)
			{
				bool bAllianceFormed = RequestAlliance(BestTarget.Team, CommonEnemy);
				if (bAllianceFormed)
				{
					UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P3 Defensive Diplomacy - Alliance formed with Team %d against common enemy Team %d!"),
						(int32)ControlledTeam, (int32)BestTarget.Team, (int32)CommonEnemy);
				}
			}
		}
	}
}

// ========== ALLIANCE & JOINT ATTACK IMPLEMENTATION ==========

bool AAITeamController::IsAlliedWith(EOwnerTeam OtherTeam) const
{
	return AllianceState.AlliedTeams.Contains(OtherTeam);
}

bool AAITeamController::RequestAlliance(EOwnerTeam TargetTeam, EOwnerTeam CommonEnemy)
{
	// Check cooldown (don't spam alliance requests)
	if (AllianceState.LastAllianceRequestCycle.Contains(TargetTeam))
	{
		int32 LastRequest = AllianceState.LastAllianceRequestCycle[TargetTeam];
		if (CurrentDecisionCycle - LastRequest < 10) // Wait 10 cycles (30 seconds) between requests
		{
			return false;
		}
	}
	
	// Special case: Requesting alliance from Player
	if (TargetTeam == EOwnerTeam::Player)
	{
		APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
		if (!GameMode)
		{
			UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to request alliance from Player - GameMode not found!"), (int32)ControlledTeam);
			return false;
		}
		
		// Check if relationship is high enough (+20 required)
		float CurrentRelationship = GameMode->GetDisposition(ControlledTeam, EOwnerTeam::Player);
		if (CurrentRelationship < 20.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: Cannot request alliance from Player - relationship too low (%.1f, need +20)"),
				(int32)ControlledTeam, CurrentRelationship);
			return false;
		}
		
		// Queue alliance request for player to respond via dialogue UI
		GameMode->QueueAIAllianceRequest(ControlledTeam, CommonEnemy);
		
		// Update cooldown
		AllianceState.LastAllianceRequestCycle.Add(TargetTeam, CurrentDecisionCycle);
		
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: ALLIANCE REQUESTED from PLAYER (common enemy: Team %d, relationship: %.1f)"),
			(int32)ControlledTeam, (int32)CommonEnemy, CurrentRelationship);
		
		return true; // Request queued, player will respond later
	}
	
	// AI-to-AI alliance request
	// Find the other team's AI controller
	TArray<AActor*> FoundControllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
	
	for (AActor* Actor : FoundControllers)
	{
		AAITeamController* OtherController = Cast<AAITeamController>(Actor);
		if (OtherController && OtherController->ControlledTeam == TargetTeam)
		{
			// Ask them to evaluate our alliance request
			bool bAccepted = OtherController->EvaluateAllianceRequest(ControlledTeam, CommonEnemy);
			
			if (bAccepted)
			{
				APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
				if (!GameMode)
				{
					UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to form alliance - GameMode not found!"), (int32)ControlledTeam);
					return false;
				}

				FString AllianceName;
				
				// Determine how to form/join alliance using centralized helper functions
				if (!AllianceState.AllianceName.IsEmpty())
				{
					// Requesting team has existing alliance - other team joins it
					AllianceName = AllianceState.AllianceName;
					if (!GameMode->AddTeamToAlliance(TargetTeam, AllianceName))
					{
						UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to add Team %d to alliance '%s'!"), 
							(int32)ControlledTeam, (int32)TargetTeam, *AllianceName);
						return false;
					}
				}
				else if (!OtherController->AllianceState.AllianceName.IsEmpty())
				{
					// Accepting team has existing alliance - requesting team joins it
					AllianceName = OtherController->AllianceState.AllianceName;
					if (!GameMode->AddTeamToAlliance(ControlledTeam, AllianceName))
					{
						UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to add Team %d to alliance '%s'!"), 
							(int32)TargetTeam, (int32)ControlledTeam, *AllianceName);
						return false;
					}
				}
				else
				{
					// Neither has alliance - create new one
					AllianceName = GameMode->FormNewAlliance(ControlledTeam, TargetTeam);
					if (AllianceName.IsEmpty())
					{
						UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to form new alliance with Team %d!"), 
							(int32)ControlledTeam, (int32)TargetTeam);
						return false;
					}
				}
				
				// Validate alliance integrity
				GameMode->ValidateAllianceIntegrity();
				
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: ALLIANCE FORMED '%s' with Team %d against Team %d!"),
					(int32)ControlledTeam, *AllianceName, (int32)TargetTeam, (int32)CommonEnemy);
				
				// Check if both teams hate the player - if so, log threat message
				float MyDispositionToPlayer = GameMode->GetDisposition(ControlledTeam, EOwnerTeam::Player);
				float TheirDispositionToPlayer = GameMode->GetDisposition(TargetTeam, EOwnerTeam::Player);
				
				if (MyDispositionToPlayer < 0.0f && TheirDispositionToPlayer < 0.0f)
				{
					// Both teams hate the player - log the threat
					FString TargetTeamName;
					if (TargetTeam == EOwnerTeam::AI1) TargetTeamName = TEXT("Team 2");
					else if (TargetTeam == EOwnerTeam::AI2) TargetTeamName = TEXT("Team 3");
					else if (TargetTeam == EOwnerTeam::AI3) TargetTeamName = TEXT("Team 4");
					else if (TargetTeam == EOwnerTeam::AI4) TargetTeamName = TEXT("Team 5");
					else if (TargetTeam == EOwnerTeam::AI5) TargetTeamName = TEXT("Team 6");
					else if (TargetTeam == EOwnerTeam::AI6) TargetTeamName = TEXT("Team 7");
					
					FString MyTeamName;
					if (ControlledTeam == EOwnerTeam::AI1) MyTeamName = TEXT("Team 2");
					else if (ControlledTeam == EOwnerTeam::AI2) MyTeamName = TEXT("Team 3");
					else if (ControlledTeam == EOwnerTeam::AI3) MyTeamName = TEXT("Team 4");
					else if (ControlledTeam == EOwnerTeam::AI4) MyTeamName = TEXT("Team 5");
					else if (ControlledTeam == EOwnerTeam::AI5) MyTeamName = TEXT("Team 6");
					else if (ControlledTeam == EOwnerTeam::AI6) MyTeamName = TEXT("Team 7");
					
					UE_LOG(LogTemp, Warning, TEXT("[THREAT] %s to Player: We have formed an alliance with %s. Your days are numbered. Prepare to be exterminated."),
						*MyTeamName, *TargetTeamName);
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AI Team %d: Alliance request to Team %d REJECTED"),
					(int32)ControlledTeam, (int32)TargetTeam);
			}
			
			// Update cooldown
			AllianceState.LastAllianceRequestCycle.Add(TargetTeam, CurrentDecisionCycle);
			return bAccepted;
		}
	}
	
	return false;
}

bool AAITeamController::EvaluateAllianceRequest(EOwnerTeam RequestingTeam, EOwnerTeam CommonEnemy)
{
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode) return false;
	
	// Check relationship with requesting team (must be +20 or higher)
	float RelationshipWithRequester = GameMode->GetDisposition(ControlledTeam, RequestingTeam);
	if (RelationshipWithRequester < 20.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: Rejected alliance from Team %d - relationship too low (%.1f < 20.0)"),
			(int32)ControlledTeam, (int32)RequestingTeam, RelationshipWithRequester);
		return false;
	}
	
	// Check relationship with common enemy (must be -40 or worse)
	float RelationshipWithEnemy = GameMode->GetDisposition(ControlledTeam, CommonEnemy);
	if (RelationshipWithEnemy > -40.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: Rejected alliance from Team %d - don't hate enemy Team %d enough (%.1f > -40.0)"),
			(int32)ControlledTeam, (int32)RequestingTeam, (int32)CommonEnemy, RelationshipWithEnemy);
		return false;
	}
	
	// Check if we're enemies with any existing alliance member
	// Need to find requesting team's controller to check their alliance members
	TArray<AActor*> FoundControllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
	
	for (AActor* Actor : FoundControllers)
	{
		AAITeamController* RequesterController = Cast<AAITeamController>(Actor);
		if (RequesterController && RequesterController->ControlledTeam == RequestingTeam)
		{
			// Check each existing alliance member
			for (EOwnerTeam AllianceMember : RequesterController->AllianceState.AlliedTeams)
			{
				float RelationshipWithMember = GameMode->GetDisposition(ControlledTeam, AllianceMember);
				if (RelationshipWithMember < 0.0f)
				{
					UE_LOG(LogTemp, Log, TEXT("AI Team %d: Rejected alliance from Team %d - enemies with alliance member Team %d (%.1f < 0.0)"),
						(int32)ControlledTeam, (int32)RequestingTeam, (int32)AllianceMember, RelationshipWithMember);
					return false;
				}
			}
			break;
		}
	}
	
	// All conditions met - accept alliance
	UE_LOG(LogTemp, Warning, TEXT("AI Team %d: ACCEPTED alliance from Team %d against Team %d"),
		(int32)ControlledTeam, (int32)RequestingTeam, (int32)CommonEnemy);
	return true;
}

TArray<EOwnerTeam> AAITeamController::FindPotentialAllies(EOwnerTeam CommonEnemy)
{
	TArray<EOwnerTeam> PotentialAllies;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode) return PotentialAllies;
	
	// Check Player as potential ally
	if (CommonEnemy != EOwnerTeam::Player)
	{
		// Check if we have +20 or greater relationship with the player
		float OurRelationshipWithPlayer = GameMode->GetDisposition(ControlledTeam, EOwnerTeam::Player);
		if (OurRelationshipWithPlayer >= 20.0f)
		{
			// Check if player hates the common enemy (-40 or worse)
			float PlayerRelationshipWithEnemy = GameMode->GetDisposition(EOwnerTeam::Player, CommonEnemy);
			if (PlayerRelationshipWithEnemy <= -40.0f)
			{
				// Player is a potential ally
				PotentialAllies.Add(EOwnerTeam::Player);
			}
		}
	}
	
	// Check all opponent teams
	for (const FOpponentGlobalState& Opponent : OpponentStates)
	{
		if (Opponent.Team == CommonEnemy) continue; // Can't ally with target enemy
		
		// Check if we have +20 or greater relationship with this team
		float OurRelationship = GameMode->GetDisposition(ControlledTeam, Opponent.Team);
		if (OurRelationship < 20.0f) continue;
		
		// Check if they hate the common enemy (-40 or worse)
		float TheirRelationshipWithEnemy = GameMode->GetDisposition(Opponent.Team, CommonEnemy);
		if (TheirRelationshipWithEnemy > -40.0f) continue;
		
		// This team is a potential ally
		PotentialAllies.Add(Opponent.Team);
	}
	
	return PotentialAllies;
}

bool AAITeamController::ProposeJointAttack(ACityActor* TargetCity, EOwnerTeam AllyTeam)
{
	if (!TargetCity) return false;
	
	// Find ally's controller
	TArray<AActor*> FoundControllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
	
	for (AActor* Actor : FoundControllers)
	{
		AAITeamController* AllyController = Cast<AAITeamController>(Actor);
		if (AllyController && AllyController->ControlledTeam == AllyTeam)
		{
			// Create proposal
			FJointAttackProposal Proposal;
			Proposal.TargetCity = TargetCity;
			Proposal.ProposingTeam = ControlledTeam;
			Proposal.PartnerTeam = AllyTeam;
			Proposal.ProposalCycle = CurrentDecisionCycle;
			Proposal.bIsReady = false; // We're not ready yet
			Proposal.bPartnerReady = false;
			
			// Ask ally to evaluate
			bool bAccepted = AllyController->EvaluateJointAttackProposal(Proposal);
			
			if (bAccepted)
			{
				// Add to both teams' proposal lists
				JointAttackProposals.Add(Proposal);
				AllyController->JointAttackProposals.Add(Proposal);
				
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: JOINT ATTACK proposed with Team %d against city '%s'"),
					(int32)ControlledTeam, (int32)AllyTeam, *TargetCity->GetName());
				return true;
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AI Team %d: Joint attack proposal REJECTED by Team %d"),
					(int32)ControlledTeam, (int32)AllyTeam);
				return false;
			}
		}
	}
	
	return false;
}

bool AAITeamController::EvaluateJointAttackProposal(const FJointAttackProposal& Proposal)
{
	// Check if we're already preparing for a different attack
	for (const FJointAttackProposal& ExistingProposal : JointAttackProposals)
	{
		if (ExistingProposal.TargetCity != Proposal.TargetCity)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: Rejected joint attack - already preparing attack on different city '%s'"),
				(int32)ControlledTeam, *ExistingProposal.TargetCity->GetName());
			return false;
		}
	}
	
	// Check if we're already preparing a solo attack on a different city
	for (const FMyCityState& CityState : MyCityStates)
	{
		if (CityState.WarTarget && CityState.WarTarget != Proposal.TargetCity && CityState.WarVehiclesReserved > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: Rejected joint attack - already preparing solo attack on '%s'"),
				(int32)ControlledTeam, *CityState.WarTarget->GetName());
			return false;
		}
	}
	
	// Accept the proposal
	UE_LOG(LogTemp, Warning, TEXT("AI Team %d: ACCEPTED joint attack proposal against city '%s'"),
		(int32)ControlledTeam, *Proposal.TargetCity->GetName());
	return true;
}

EOwnerTeam AAITeamController::FindJointAttackPartner(ACityActor* TargetCity)
{
	if (!TargetCity) return EOwnerTeam::Neutral;
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode) return EOwnerTeam::Neutral;
	
	EOwnerTeam TargetTeam = TargetCity->OwnerTeam;
	
	// Look through allied teams
	for (EOwnerTeam AllyTeam : AllianceState.AlliedTeams)
	{
		// Check if ally also hates the target team
		float AllyRelationship = GameMode->GetDisposition(AllyTeam, TargetTeam);
		if (AllyRelationship <= -100.0f) // Ally must also be max-hostile
		{
			return AllyTeam;
		}
	}
	
	return EOwnerTeam::Neutral;
}

void AAITeamController::ExecuteAllianceFormation()
{
	// Alliance Formation - Seek allies against max-hostile enemies (relationship <= -100)
	// Strategy:
	// 1. Find all teams with max hostility (-100 relationship)
	// 2. For each max-hostile enemy, try to form alliances with other teams
	// 3. Skip if already allied
	// 4. Use RequestAlliance() which handles player/AI requests differently
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("AI Team %d: Alliance Formation - Failed to get GameMode!"), (int32)ControlledTeam);
		return;
	}
	
	// STEP 1: Find all max-hostile enemies (-100 relationship)
	TArray<EOwnerTeam> MaxHostileEnemies;
	for (const FOpponentGlobalState& OpponentState : OpponentStates)
	{
		float Relationship = GameMode->GetDisposition(ControlledTeam, OpponentState.Team);
		if (Relationship <= -100.0f)
		{
			MaxHostileEnemies.Add(OpponentState.Team);
		}
	}
	
	// STEP 2: For each max-hostile enemy, try to form alliances
	for (EOwnerTeam HostileTeam : MaxHostileEnemies)
	{
		TArray<EOwnerTeam> PotentialAllies = FindPotentialAllies(HostileTeam);
		
		for (EOwnerTeam PotentialAlly : PotentialAllies)
		{
			// Skip if already allied
			if (IsAlliedWith(PotentialAlly)) continue;
			
			// Try to request alliance
			bool bAllianceFormed = RequestAlliance(PotentialAlly, HostileTeam);
			if (bAllianceFormed)
			{
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Alliance Formation - Alliance formed with Team %d against Team %d"),
					(int32)ControlledTeam, (int32)PotentialAlly, (int32)HostileTeam);
			}
		}
	}
}

void AAITeamController::ExecuteWarLayer()
{
	// Priority 4: War - Build war party and attack enemy cities
	// Strategy:
	// 1. Check if there's a valid target (relationship <= -100) - SKIP if none
	// 2. Build 10 vehicles and save 2000 black substrate
	// 3. Select best target city (weight: distance, hate level, turret count)
	// 4. Assemble war party at rally point (edge of territory toward target)
	// 5. Launch coordinated attack - vehicles destroy turrets then capital
	// 6. Each war party is a distinct group - no piecemeal replacements
	// 7. Only P1 (Survival) can recall war vehicles, not P2 (Income)
	
	// Only log for Team 2 (AI1) to reduce log spam
	bool bShouldLog = false;
	
	if (MyCityStates.Num() == 0) return; // No cities
	
	APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode) return;
	
	// Constants
	const int32 WAR_PARTY_SIZE = 10;
	const int32 WAR_STOCKPILE_BS = 2000;
	const int32 VEHICLE_COST = 1000;
	const float TERRITORY_RADIUS = GameMode->TERRITORY_RADIUS;
	
	// === P4 STATUS CHECK ===
	// Count valid targets and current war party status
	int32 ValidTargets = 0;
	for (const FOpponentGlobalState& OpponentState : OpponentStates)
	{
		float Relationship = GameMode->GetDisposition(ControlledTeam, OpponentState.Team);
		if (Relationship <= -100.0f && OpponentState.Cities.Num() > 0)
		{
			ValidTargets++;
		}
	}
	
	// Count active war party vehicles
	TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
	int32 WarPartyVehicles = 0;
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (Vehicle && Vehicle->CurrentTask.Type == EVehicleTaskType::AttackTarget && Vehicle->CurrentTask.Priority == 4)
		{
			WarPartyVehicles++;
		}
	}
	
	if (bShouldLog)
	{
		UE_LOG(LogTemp, Warning, TEXT(""));
		UE_LOG(LogTemp, Warning, TEXT("--- P4: WAR STATUS ---"));
		UE_LOG(LogTemp, Warning, TEXT("Valid enemy targets: %d (relationship <= -100)"), ValidTargets);
		UE_LOG(LogTemp, Warning, TEXT("Active war party: %d/%d vehicles"), WarPartyVehicles, WAR_PARTY_SIZE);
		
		if (WarPartyVehicles > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("War party in field - monitoring"));
		}
		else if (ValidTargets > 0)
		{
			int32 VehiclesNeeded = WAR_PARTY_SIZE - WarPartyVehicles;
			int32 VehicleCostTotal = VehiclesNeeded * VEHICLE_COST;
			UE_LOG(LogTemp, Warning, TEXT("Need to build %d vehicles (Cost: %d OS) + save %d BS"), 
				VehiclesNeeded, VehicleCostTotal, WAR_STOCKPILE_BS);
			
			// Reserve substrate for war party
			ReservedOrangeSubstrate += VehicleCostTotal;
			ReservedBlackSubstrate = FMath::Max(ReservedBlackSubstrate, WAR_STOCKPILE_BS);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No enemies to attack - war idle"));
		}
	}
	
	// STEP 0: Check if there are any valid targets BEFORE building war party
	// Only attack enemies we hate (relationship <= -100)
	bool bHasValidTarget = false;
	for (const FOpponentGlobalState& OpponentState : OpponentStates)
	{
		float Relationship = GameMode->GetDisposition(ControlledTeam, OpponentState.Team);
		if (Relationship <= -100.0f && OpponentState.Cities.Num() > 0)
		{
			bHasValidTarget = true;
			break;
		}
	}
	
	if (!bHasValidTarget)
	{
		return; // Don't build war party if no targets
	}
	
	// Get all our vehicles (recalculated for task assignment)
	AllVehicles = GetControlledVehicles();
	
	// Find active war party (vehicles on P4 attack missions)
	TArray<AVehicleActor*> ActiveWarParty;
	TArray<AVehicleActor*> IdleVehicles;
	
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (!Vehicle) continue;
		
		if (Vehicle->CurrentTask.Type == EVehicleTaskType::AttackTarget && Vehicle->CurrentTask.Priority == 4)
		{
			ActiveWarParty.Add(Vehicle);
		}
		else if (Vehicle->IsIdle() && !Vehicle->bInP1) // Not in P1 defense
		{
			IdleVehicles.Add(Vehicle);
		}
	}
	
	// If active war party exists, monitor but don't interfere
	if (ActiveWarParty.Num() > 0)
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - Active war party of %d vehicles in field"),
				(int32)ControlledTeam, ActiveWarParty.Num());
		}
		
		// Check if war party has been wiped out or mission completed
		if (ActiveWarParty.Num() < 3) // Less than 3 vehicles left = party failed
		{
			if (bShouldLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - War party failed/completed (%d survivors). Will rebuild new party."),
				(int32)ControlledTeam, ActiveWarParty.Num());
			}
			
			// Clear remaining vehicles' tasks so they can be reassigned
			for (AVehicleActor* Vehicle : ActiveWarParty)
			{
				if (Vehicle) Vehicle->ClearTask();
			}
		}
		else
		{
			// War party still active - don't launch another
			return;
		}
	}
	
	// No active war party - prepare new one
	
	// Reserve black substrate for war stockpile
	ReservedBlackSubstrate = FMath::Max(ReservedBlackSubstrate, WAR_STOCKPILE_BS);
	
	// STEP 1: Build/recruit war party vehicles
	// Count vehicles available for war (idle + can build)
	int32 AvailableForWar = IdleVehicles.Num();
	
	if (AvailableForWar < WAR_PARTY_SIZE)
	{
		int32 VehiclesNeeded = WAR_PARTY_SIZE - AvailableForWar;
		
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - Building war party: %d/%d ready"),
				(int32)ControlledTeam, AvailableForWar, WAR_PARTY_SIZE);
		}
		
		// Build more vehicles if we have resources
		while (VehiclesNeeded > 0 && OrangeSubstrate >= VEHICLE_COST)
		{
			ACityActor* BuildCity = MyCityStates[0].City;
			if (BuildCity && BuildVehicleAtCity(BuildCity))
			{
				VehiclesNeeded--;
				if (bShouldLog)
				{
					UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - Built war vehicle (%d/%d)"),
						(int32)ControlledTeam, WAR_PARTY_SIZE - VehiclesNeeded, WAR_PARTY_SIZE);
				}
			}
			else
			{
				break; // Can't build more
			}
		}
		
		// EMERGENCY TRADING: If we still need vehicles but can't afford them, try to trade for orange
		if (VehiclesNeeded > 0 && OrangeSubstrate < VEHICLE_COST)
		{
			int32 OrangeDeficit = (VehiclesNeeded * VEHICLE_COST) - OrangeSubstrate;
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Need %d more war vehicles but only have %d orange. Need %d more orange. Attempting to trade..."),
				(int32)ControlledTeam, VehiclesNeeded, OrangeSubstrate, OrangeDeficit);
			
			bool bTradeSuccess = RequestTradeFromAI(true, OrangeDeficit);
			if (bTradeSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Emergency trade successful! Now have %d orange"),
					(int32)ControlledTeam, OrangeSubstrate);
				
				// Try building war vehicles again after trade
				while (VehiclesNeeded > 0 && OrangeSubstrate >= VEHICLE_COST)
				{
					ACityActor* BuildCity = MyCityStates[0].City;
					if (BuildCity && BuildVehicleAtCity(BuildCity))
					{
						VehiclesNeeded--;
						if (bShouldLog)
						{
							UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - Built war vehicle (post-trade) (%d/%d)"),
								(int32)ControlledTeam, WAR_PARTY_SIZE - VehiclesNeeded, WAR_PARTY_SIZE);
						}
					}
					else
					{
						break; // Can't build more
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Emergency trade failed, no available partners. Waiting for income..."),
					(int32)ControlledTeam);
			}
		}
		
		return; // Not enough vehicles yet
	}
	
	// STEP 2: Check black substrate stockpile
	if (BlackSubstrate < WAR_STOCKPILE_BS)
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - Stockpiling black substrate: %d/%d"),
				(int32)ControlledTeam, BlackSubstrate, WAR_STOCKPILE_BS);
		}
		
		// Try trading for more
		int32 Deficit = WAR_STOCKPILE_BS - BlackSubstrate;
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - Attempting to trade for %d black substrate..."),
			(int32)ControlledTeam, Deficit);
		
		bool bTradeSuccess = RequestTradeFromAI(false, Deficit);
		if (bTradeSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Trade successful! Now have %d black substrate"),
				(int32)ControlledTeam, BlackSubstrate);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Trade failed, no available partners"),
				(int32)ControlledTeam);
		}
		
		return; // Not enough stockpile yet
	}
	
	// STEP 3: Select best target city
	// Scoring: Lower is better
	// - Distance factor (closer = better)
	// - Hate factor (more hate = better target)
	// - Turret factor (fewer turrets = better)
	
	struct FWarTarget
	{
		ACityActor* City;
		EOwnerTeam Team;
		float Distance;
		float Relationship;
		int32 TurretCount;
		float Score;
	};
	
	TArray<FWarTarget> PotentialTargets;
	FVector LaunchCityLocation = MyCityStates[0].City->GetActorLocation();
	
	for (const FOpponentGlobalState& OpponentState : OpponentStates)
	{
		float Relationship = GameMode->GetDisposition(ControlledTeam, OpponentState.Team);
		
		// Only attack enemies (relationship <= -50)
		if (Relationship > -50.0f) continue;
		
		for (const FOpponentCityState& OpponentCity : OpponentState.Cities)
		{
			if (!OpponentCity.City || !IsValid(OpponentCity.City)) continue;
			
			FWarTarget Target;
			Target.City = OpponentCity.City;
			Target.Team = OpponentState.Team;
			Target.Distance = FVector::Dist(LaunchCityLocation, OpponentCity.City->GetActorLocation());
			Target.Relationship = Relationship;
			
			// Count turrets in this city
			Target.TurretCount = 0;
			for (ABuildingActor* Building : OpponentCity.City->Buildings)
			{
				if (Cast<ATurretBuildingActor>(Building) && Building->CurrentHealth > 0)
				{
					Target.TurretCount++;
				}
			}
			
			// Calculate score (lower = better target)
			// Normalize factors to 0-1 range then combine
			float DistanceFactor = Target.Distance / 100000.0f; // Normalize to planet scale
			float HateFactor = (100.0f + Target.Relationship) / 100.0f; // -100 = 0 (best), 0 = 1 (worst)
			float TurretFactor = FMath::Min(Target.TurretCount / 8.0f, 1.0f); // 0-8 turrets normalized
			
			// Weight the factors (distance=2x, hate=1.5x, turrets=1x)
			Target.Score = (DistanceFactor * 2.0f) + (HateFactor * 1.5f) + TurretFactor;
			
			PotentialTargets.Add(Target);
		}
	}
	
	if (PotentialTargets.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: P4 War - No valid enemies to attack (need -50 relationship)"),
			(int32)ControlledTeam);
		return;
	}
	
	// Sort by score (lowest = best)
	PotentialTargets.Sort([](const FWarTarget& A, const FWarTarget& B) {
		return A.Score < B.Score;
	});
	
	FWarTarget BestTarget = PotentialTargets[0];
	
	UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Target selected: '%s' (Team %d) - Dist:%.0f Hate:%.0f Turrets:%d Score:%.2f"),
		(int32)ControlledTeam, *BestTarget.City->GetName(), (int32)BestTarget.Team,
		BestTarget.Distance, BestTarget.Relationship, BestTarget.TurretCount, BestTarget.Score);
	
	// STEP 4: Calculate rally point (edge of territory toward target)
	FVector OurCityLocation = MyCityStates[0].City->GetActorLocation();
	FVector TargetDirection = (BestTarget.City->GetActorLocation() - OurCityLocation).GetSafeNormal();
	
	// Project onto planet surface
	FVector PlanetCenter = FVector(0, 0, 0); // Assuming planet at origin
	float PlanetRadius = 100000.0f;
	FVector OurPlanetNormal = (OurCityLocation - PlanetCenter).GetSafeNormal();
	
	// Rally point is at territory edge in direction of target
	FVector RallyPoint = OurCityLocation + TargetDirection * TERRITORY_RADIUS;
	FVector RallyPlanetNormal = (RallyPoint - PlanetCenter).GetSafeNormal();
	RallyPoint = PlanetCenter + RallyPlanetNormal * PlanetRadius;
	
	// STEP 5: Recruit war party and assign attack tasks
	TArray<AVehicleActor*> WarParty;
	for (int32 i = 0; i < WAR_PARTY_SIZE && i < IdleVehicles.Num(); i++)
	{
		WarParty.Add(IdleVehicles[i]);
	}
	
	if (WarParty.Num() < WAR_PARTY_SIZE)
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 War - Not enough idle vehicles (%d/%d)"),
			(int32)ControlledTeam, WarParty.Num(), WAR_PARTY_SIZE);
		return;
	}
	
	// LAUNCH WAR PARTY!
	UE_LOG(LogTemp, Warning, TEXT("AI Team %d: P4 WAR LAUNCHED! %d vehicles attacking '%s' (Turrets:%d)"),
		(int32)ControlledTeam, WarParty.Num(), *BestTarget.City->GetName(), BestTarget.TurretCount);
	
	for (AVehicleActor* Vehicle : WarParty)
	{
		if (!Vehicle) continue;
		
		FVehicleTask WarTask;
		WarTask.Type = EVehicleTaskType::AttackTarget;
		WarTask.Priority = 4; // P4 War (only P1 can preempt)
		WarTask.PrimaryTarget = BestTarget.City->CapitalBuilding ? 
			(AActor*)BestTarget.City->CapitalBuilding : (AActor*)BestTarget.City;
		WarTask.Destination = RallyPoint; // Rally point for assembly
		WarTask.bAutonomousContinue = false; // Specific mission
		WarTask.AssigningController = this;
		WarTask.MinesToDestroy.Add(BestTarget.Team); // Can attack enemy mines/turrets
		
		Vehicle->AssignTask(WarTask);
	}
}

void AAITeamController::ExecuteAidLayer()
{
	// Priority 5: Aid Allies - Send resources and military support to allies under attack
	
	if (MyCityStates.Num() == 0 || AllianceState.AlliedTeams.Num() == 0)
	{
		return; // No cities or no allies, can't help
	}
	
	// STEP 0: Monitor existing aid missions and clean up completed ones
	Priority6Vehicles.RemoveAll([](AVehicleActor* V) { return V == nullptr || !IsValid(V); });
	
	TArray<AVehicleActor*> VehiclesToRelease;
	for (AVehicleActor* Vehicle : Priority6Vehicles)
	{
		if (!Vehicle || !IsValid(Vehicle))
		{
			VehiclesToRelease.Add(Vehicle);
			continue;
		}
		
		// Check if mission is complete (ally city captured, destroyed, or no longer under attack)
		ACityActor* AllyCity = Cast<ACityActor>(Vehicle->PrimaryTarget);
		if (!AllyCity || !IsValid(AllyCity))
		{
			// Ally city gone - send vehicle home
			SendVehicleHome(Vehicle);
			Vehicle->bInP6 = false;
			VehiclesToRelease.Add(Vehicle);
			continue;
		}
		
		// Check if ally city was captured by enemy
		if (!IsAlliedWith(AllyCity->OwnerTeam))
		{
			// City captured - flee and return home
			SendVehicleHome(Vehicle);
			Vehicle->bInP6 = false;
			VehiclesToRelease.Add(Vehicle);
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Aid mission failed - ally city captured, returning home"),
				(int32)ControlledTeam);
			continue;
		}
		
		// Check if CurrentTarget (attacker) was destroyed
		if (!Vehicle->CurrentTarget || !IsValid(Vehicle->CurrentTarget))
		{
			// Check for remaining attackers at ally city
			TArray<AVehicleActor*> RemainingAttackers;
			TArray<AActor*> AllVehicles;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllVehicles);
			
			float TerritoryRadius = 5000.0f;
			for (AActor* Actor : AllVehicles)
			{
				AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
				if (!EnemyVehicle || !IsValid(EnemyVehicle)) continue;
				if (EnemyVehicle->OwnerTeam == AllyCity->OwnerTeam || EnemyVehicle->OwnerTeam == ControlledTeam) continue;
				
				float DistanceToCity = FVector::Dist(EnemyVehicle->GetActorLocation(), AllyCity->GetActorLocation());
				if (DistanceToCity <= TerritoryRadius)
				{
					RemainingAttackers.Add(EnemyVehicle);
				}
			}
			
			if (RemainingAttackers.Num() > 0)
			{
				// Re-target another attacker
				Vehicle->SetTargetLocation(RemainingAttackers[0]->GetActorLocation());
				Vehicle->CurrentTarget = RemainingAttackers[0]; // Set AFTER SetTargetLocation
				Vehicle->bHasTarget = true;
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Aid vehicle killed attacker - targeting next attacker"),
					(int32)ControlledTeam);
			}
			else
			{
				// No more attackers - mission complete, return home
				SendVehicleHome(Vehicle);
				Vehicle->bInP6 = false;
				VehiclesToRelease.Add(Vehicle);
				UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Aid mission complete - all attackers defeated, returning home"),
					(int32)ControlledTeam);
			}
		}
	}
	
	// Clean up released vehicles
	for (AVehicleActor* Vehicle : VehiclesToRelease)
	{
		Priority6Vehicles.Remove(Vehicle);
	}
	
	// STEP 1: Process incoming aid requests from allies
	for (int32 i = AidRequests.Num() - 1; i >= 0; i--)
	{
		FAidRequest& Request = AidRequests[i];
		
		// Skip our own requests
		if (Request.RequestingTeam == ControlledTeam)
		{
			continue;
		}
		
		// Skip if already processed
		if (Request.SubstrateProvided > 0 || Request.bMilitaryAidSent)
		{
			// Remove old requests (older than 10 cycles)
			if (CurrentDecisionCycle - Request.RequestCycle > 10)
			{
				AidRequests.RemoveAt(i);
			}
			continue;
		}
		
		// Evaluate and send aid
		bool bAidSent = EvaluateAidRequest(Request);
		if (bAidSent)
		{
			// Mark as provided
			if (Request.bRequestedSubstrate)
			{
				Request.SubstrateProvided = Request.SubstrateAmount;
			}
			if (Request.bRequestedMilitary)
			{
				Request.bMilitaryAidSent = true;
			}
		}
	}
}

// ========== HELPER FUNCTIONS ==========

bool AAITeamController::BuildVehicleAtCity(ACityActor* City, int32 Priority)
{
	if (!City || !GetWorld()) return false;
	
	int32 VehicleCost = City->VehicleCost;
	
	// Check if we have enough orange substrate
	if (OrangeSubstrate < VehicleCost)
	{
		return false;
	}
	
	// Deduct cost
	OrangeSubstrate -= VehicleCost;
	
	// Get spawn parameters (similar to CityActor::SpawnVehicle)
	FVector CityLocation = City->GetActorLocation();
	FVector PlanetCenter = City->PlanetCenter;
	float PlanetRadius = City->PlanetRadius;
	FVector CityDirection = (CityLocation - PlanetCenter).GetSafeNormal();
	
	// Generate a random offset on the planet surface around the city
	FVector RandomTangent1 = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
	if (RandomTangent1.IsNearlyZero())
	{
		RandomTangent1 = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
	}
	FVector RandomTangent2 = FVector::CrossProduct(CityDirection, RandomTangent1).GetSafeNormal();
	
	// Random offset in tangent space
	float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	float TurretRadius = City->CalculateTurretRadius();
	float MinSpawnDistance = TurretRadius + 1000.0f;
	float MaxSpawnDistance = TurretRadius + 1500.0f;
	float RandomDistance = FMath::FRandRange(MinSpawnDistance, MaxSpawnDistance);
	
	FVector RandomOffset = (RandomTangent1 * FMath::Cos(RandomAngle) + RandomTangent2 * FMath::Sin(RandomAngle)) * RandomDistance;
	
	// Calculate spawn position on planet surface
	FVector SpawnDirection = (CityDirection * PlanetRadius + RandomOffset).GetSafeNormal();
	FVector SpawnLocation = PlanetCenter + SpawnDirection * PlanetRadius;
	
	// Spawn the vehicle
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = City;
	
	AVehicleActor* NewVehicle = GetWorld()->SpawnActor<AVehicleActor>(AVehicleActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	
	if (NewVehicle)
	{
		NewVehicle->PlanetCenter = PlanetCenter;
		NewVehicle->PlanetRadius = PlanetRadius;
		NewVehicle->OwnerTeam = ControlledTeam;
		NewVehicle->OwningPlanet = City->OwningPlanet; // Pass planet reference for terrain queries
		NewVehicle->AlignToPlanet();
		NewVehicle->UpdateColor();
		
		// Set appropriate priority flag based on which layer built this vehicle
		switch (Priority)
		{
		case 1:
			NewVehicle->bInP1 = true;
			break;
		case 2:
			NewVehicle->bInP2 = true;
			break;
		case 3:
			NewVehicle->bInP3 = true;
			break;
		case 4:
			NewVehicle->bInP4 = true;
			break;
		case 5:
			NewVehicle->bInP5 = true;
			break;
		default:
			// Priority 0 or unknown - no flag
			break;
		}
		
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: Vehicle built at city %s (location: %s)"), 
			(int32)ControlledTeam, *City->GetName(), *SpawnLocation.ToString());
		
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to spawn vehicle!"), (int32)ControlledTeam);
		// Refund cost if spawn failed
		OrangeSubstrate += VehicleCost;
		return false;
	}
}

TArray<ACityActor*> AAITeamController::GetControlledCities() const
{
	TArray<ACityActor*> ControlledCities;
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), FoundCities);
	
	for (AActor* Actor : FoundCities)
	{
		ACityActor* City = Cast<ACityActor>(Actor);
		if (City && City->OwnerTeam == ControlledTeam)
		{
			ControlledCities.Add(City);
		}
	}
	
	return ControlledCities;
}

TArray<AVehicleActor*> AAITeamController::GetControlledVehicles()
{
	TArray<AVehicleActor*> ControlledVehicles;
	TArray<AActor*> FoundVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), FoundVehicles);
	
	for (AActor* Actor : FoundVehicles)
	{
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (Vehicle && Vehicle->OwnerTeam == ControlledTeam)
		{
			ControlledVehicles.Add(Vehicle);
		}
	}
	
	return ControlledVehicles;
}

bool AAITeamController::IsClusterSafeFromKaiju(AResourceActor* Resource)
{
	if (!Resource) return true; // Null resource is "safe" (skip it)
	
	// If KaijuSafetyDistance is 0, team is fearless - ignore all Kaiju
	if (FMath::IsNearlyZero(KaijuSafetyDistance)) return true;
	
	// If resource isn't in a cluster, it's safe (no Kaiju guard clusters of ID -1)
	if (Resource->ClusterID < 0) return true;
	
	UWorld* World = GetWorld();
	if (!World) return true;
	
	// Find all resources in this cluster to calculate cluster center
	TArray<AActor*> AllResourceActors;
	UGameplayStatics::GetAllActorsOfClass(World, AResourceActor::StaticClass(), AllResourceActors);
	
	FVector ClusterCenter = FVector::ZeroVector;
	int32 ClusterResourceCount = 0;
	
	for (AActor* Actor : AllResourceActors)
	{
		AResourceActor* ClusterResource = Cast<AResourceActor>(Actor);
		if (ClusterResource && ClusterResource->ClusterID == Resource->ClusterID)
		{
			ClusterCenter += ClusterResource->GetActorLocation();
			ClusterResourceCount++;
		}
	}
	
	if (ClusterResourceCount == 0) return true; // Shouldn't happen, but safe default
	
	ClusterCenter /= ClusterResourceCount; // Average location = cluster center
	
	// Check if any living Kaiju is within safety distance of this cluster center
	TArray<AActor*> AllKaijus;
	UGameplayStatics::GetAllActorsOfClass(World, AKaijuActor::StaticClass(), AllKaijus);
	
	for (AActor* Actor : AllKaijus)
	{
		AKaijuActor* Kaiju = Cast<AKaijuActor>(Actor);
		if (!Kaiju || Kaiju->CurrentHealth <= 0) continue; // Dead Kaiju don't threaten
		
		// Check distance from Kaiju's GUARD CENTER to cluster center
		// Kaiju patrols within GuardRadius of GuardCenter, so we need to account for patrol area
		// If GuardCenter is close to cluster, the Kaiju will return there even if currently elsewhere
		float DistanceToGuardCenter = FVector::Dist(Kaiju->GuardCenter, ClusterCenter);
		
		// Cluster is unsafe if Kaiju's patrol area (GuardCenter +/- GuardRadius) overlaps with safety zone
		if (DistanceToGuardCenter < (KaijuSafetyDistance + Kaiju->GuardRadius))
		{
			// Kaiju's patrol area threatens this cluster
			return false;
		}
	}
	
	// No threatening Kaiju found - cluster is safe
	return true;
}

FString AAITeamController::GetArchetypeName() const
{
	switch (Archetype)
	{
		case EAIArchetype::Warmonger:
			return TEXT("Warmonger");
		case EAIArchetype::Opportunistic:
			return TEXT("Opportunistic");
		case EAIArchetype::Cautious:
			return TEXT("Cautious");
		case EAIArchetype::Expansionist:
			return TEXT("Expansionist");
		case EAIArchetype::Custom:
			return TEXT("Custom");
		default:
			return TEXT("Unknown");
	}
}

int32 AAITeamController::GetRequiredOrangeIncome() const
{
	TArray<ACityActor*> AICities = GetControlledCities();
	int32 NumCities = AICities.Num();
	return FMath::RoundToInt(NumCities * 100.0f * P2_3_IncomeMultiplier);
}

int32 AAITeamController::GetRequiredBlackIncome() const
{
	TArray<ACityActor*> AICities = GetControlledCities();
	int32 NumCities = AICities.Num();
	return FMath::RoundToInt(NumCities * 100.0f * P2_3_IncomeMultiplier);
}

bool AAITeamController::RequestTradeFromAI(bool bRequestingOrange, int32 AmountNeeded)
{
	// AI-AI Emergency Trading System (Priority 1, 2, 3, 4, & 5):
	// 1. Find all AI teams with good income and neutral/positive relationship (>= -20.0)
	// 2. Sort by income (highest first)
	// 3. Try to trade with each until one accepts
	// 4. If requesting >1000 but all partners have insufficient supply, retry with 1000, then 500
	// 5. If successful, execute INSTANT trade (no buffer, NO relationship bonus)
	
	// Enforce minimum trade amount of 500
	if (AmountNeeded < 500)
	{
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: Requested %d %s but minimum is 500, adjusting to 500"),
			(int32)ControlledTeam, AmountNeeded, bRequestingOrange ? TEXT("orange") : TEXT("black"));
		AmountNeeded = 500;
	}
	
	// Track original amount for retry logic
	int32 OriginalAmount = AmountNeeded;
	bool bShouldRetry = (AmountNeeded > 500); // Retry with smaller amounts if needed
	
	// Internal recursive call helper - try the trade with the specified amount
	auto TryTradeWithAmount = [&](int32 RequestAmount) -> bool
	{
		UWorld* World = GetWorld();
		if (!World) return false;
		
		APlanetConquestGameMode* GameMode = Cast<APlanetConquestGameMode>(World->GetAuthGameMode());
		if (!GameMode) return false;
		
		// Get all AI team controllers (exclude self, player, and neutral)
		TArray<AAITeamController*> AllAIControllers;
		for (TActorIterator<AAITeamController> It(World); It; ++It)
		{
			if (It->ControlledTeam != ControlledTeam && 
			    It->ControlledTeam != EOwnerTeam::Neutral &&
			    It->ControlledTeam != EOwnerTeam::Player)
			{
				AllAIControllers.Add(*It);
			}
		}
		
		if (AllAIControllers.Num() == 0) return false;
		
		// Filter and sort potential trading partners
		struct FTradingPartner
		{
			AAITeamController* Controller;
			int32 Income;
			float Relationship;
			float Proximity;
			float Score;
		};
		
		TArray<FTradingPartner> PotentialPartners;
		int32 RequiredIncome = 100; // Partner must have 100/cycle income to be a good trading partner
		
		//UE_LOG(LogTemp, Log, TEXT("AI Team %d: Seeking trade for %d %s substrate (require partner income >= %d)"),
		//	(int32)ControlledTeam, RequestAmount, bRequestingOrange ? TEXT("orange") : TEXT("black"), RequiredIncome);
		
		int32 PartnersWithInsufficientSupply = 0;
		
		for (AAITeamController* Partner : AllAIControllers)
		{
			// Check cooldown - can't request from same partner within 10 cycles
			if (LastTradeRequestCycle.Contains(Partner->ControlledTeam))
			{
				int32 LastRequest = LastTradeRequestCycle[Partner->ControlledTeam];
				if (CurrentDecisionCycle - LastRequest < 10)
				{
					//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: On cooldown (%d cycles since last trade)"),
					//	(int32)Partner->ControlledTeam, CurrentDecisionCycle - LastRequest);
					continue; // Still on cooldown
				}
			}
			
			// Check relationship (must be >= -20.0)
			float Disposition = GameMode->GetDisposition(ControlledTeam, Partner->ControlledTeam);
			if (Disposition < -20.0f)
			{
				//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: Hostile relationship (%.1f)"),
				//	(int32)Partner->ControlledTeam, Disposition);
				continue;
			}
			
			// Check if partner has good income of requested substrate (use member variables)
			int32 PartnerIncome = bRequestingOrange ? Partner->OrangeIncomePerCycle : Partner->BlackIncomePerCycle;
			if (PartnerIncome < RequiredIncome)
			{
				//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: Insufficient income (%d < %d)"),
					//(int32)Partner->ControlledTeam, PartnerIncome, RequiredIncome);
				continue;
			}
			
			// Calculate proximity: find closest city between our cities and their cities
			float ClosestCityDistance = FLT_MAX;
			for (const FMyCityState& MyCity : MyCityStates)
			{
				if (!MyCity.City || !IsValid(MyCity.City)) continue;
				
				// Find partner's cities
				TArray<AActor*> FoundCities;
				UGameplayStatics::GetAllActorsOfClass(World, ACityActor::StaticClass(), FoundCities);
				for (AActor* Actor : FoundCities)
				{
					ACityActor* City = Cast<ACityActor>(Actor);
					if (City && City->OwnerTeam == Partner->ControlledTeam)
					{
						float Distance = FVector::Dist(MyCity.City->GetActorLocation(), City->GetActorLocation());
						ClosestCityDistance = FMath::Min(ClosestCityDistance, Distance);
					}
				}
			}
			
			// Proximity weight: closer teams score higher (inverse distance, normalized to 0-1 range)
			// Max distance on planet ~= 60000 units
			float ProximityWeight = 1.0f - FMath::Clamp(ClosestCityDistance / 60000.0f, 0.0f, 1.0f);
			
			// Final score: Income * (50% base + 50% proximity bonus)
			float Score = PartnerIncome * (0.5f + ProximityWeight * 0.5f);
			
			// Valid partner - add to list
			//UE_LOG(LogTemp, Log, TEXT("  + AI Team %d: Valid partner (income=%d, distance=%.0f, score=%.1f, relationship=%.1f)"),
			//	(int32)Partner->ControlledTeam, PartnerIncome, ClosestCityDistance, Score, Disposition);
			PotentialPartners.Add({Partner, PartnerIncome, Disposition, ClosestCityDistance, Score});
		}
		
		if (PotentialPartners.Num() == 0)
		{
			//UE_LOG(LogTemp, Log, TEXT("AI Team %d: No suitable trading partners found for %s substrate"),
			//	(int32)ControlledTeam, bRequestingOrange ? TEXT("orange") : TEXT("black"));
			return false;
		}
		
		// Sort by score (income × proximity, highest first)
		PotentialPartners.Sort([](const FTradingPartner& A, const FTradingPartner& B) {
			return A.Score > B.Score;
		});
		
		// Try to trade with each partner in order
		//UE_LOG(LogTemp, Log, TEXT("AI Team %d: Evaluating %d potential trading partners..."), 
		//	(int32)ControlledTeam, PotentialPartners.Num());
		
		for (const FTradingPartner& PartnerInfo : PotentialPartners)
		{
			AAITeamController* Partner = PartnerInfo.Controller;
			
			// Check if partner has enough of requested substrate
			int32 PartnerSupply = bRequestingOrange ? Partner->OrangeSubstrate : Partner->BlackSubstrate;
			if (PartnerSupply < RequestAmount)
			{
				//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: Insufficient supply (%d < %d)"),
				//	(int32)Partner->ControlledTeam, PartnerSupply, RequestAmount);
				PartnersWithInsufficientSupply++;
				continue;
			}
			
			// Calculate price based on partner's scarcity
			int32 PartnerOrange = Partner->OrangeSubstrate;
			int32 PartnerBlack = Partner->BlackSubstrate;
			
			int32 PriceInOtherSubstrate = 0;
			bool bPartnerAccepts = false;
			
			if (bRequestingOrange)
			{
				// We want orange from partner, offering black in return
				// Partner sells orange, so check if orange is scarce for them
				bool bOrangeIsScarce = PartnerOrange < PartnerBlack;
				
				if (bOrangeIsScarce)
				{
					// Orange is scarce - partner demands 120%-140% of request
					PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(1.2f, 1.4f);
				}
				else
				{
					// Orange is surplus - partner asks fair price 80%-100%
					PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(0.8f, 1.0f);
				}
				
				// Check if we can afford it AND partner can afford to sell without going below reserves
				if (BlackSubstrate >= PriceInOtherSubstrate)
				{
					// Check if partner would fall below their reserved orange substrate
					if (Partner->OrangeSubstrate - RequestAmount >= Partner->ReservedOrangeSubstrate)
					{
						bPartnerAccepts = true;
					}
					else
					{
						//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: Cannot sell - would drop below reserved orange (%d - %d < %d)"),
						//	(int32)Partner->ControlledTeam, Partner->OrangeSubstrate, RequestAmount, Partner->ReservedOrangeSubstrate);
					}
				}
			}
			else
			{
				// We want black from partner, offering orange in return
				// Partner sells black, so check if black is scarce for them
				bool bBlackIsScarce = PartnerBlack < PartnerOrange;
				
				if (bBlackIsScarce)
				{
					// Black is scarce - partner demands 120%-140% of request
					PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(1.2f, 1.4f);
				}
				else
				{
					// Black is surplus - partner asks fair price 80%-100%
					PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(0.8f, 1.0f);
				}
				
				// Check if we can afford it AND partner can afford to sell without going below reserves
				if (OrangeSubstrate >= PriceInOtherSubstrate)
				{
					// Check if partner would fall below their reserved black substrate
					if (Partner->BlackSubstrate - RequestAmount >= Partner->ReservedBlackSubstrate)
					{
						bPartnerAccepts = true;
					}
					else
					{
						//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: Cannot sell - would drop below reserved black (%d - %d < %d)"),
						//	(int32)Partner->ControlledTeam, Partner->BlackSubstrate, RequestAmount, Partner->ReservedBlackSubstrate);
					}
				}
			}
			
			if (bPartnerAccepts)
			{
				//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: TRADE ACCEPTED with AI Team %d - Paying %d %s for %d %s"),
				//	(int32)ControlledTeam, (int32)Partner->ControlledTeam,
				//	PriceInOtherSubstrate, bRequestingOrange ? TEXT("black") : TEXT("orange"),
				//	RequestAmount, bRequestingOrange ? TEXT("orange") : TEXT("black"));
				
				// Execute INSTANT trade (no buffer, direct substrate transfer)
				if (bRequestingOrange)
				{
					// We get orange, partner gets black
					OrangeSubstrate += RequestAmount;
					BlackSubstrate -= PriceInOtherSubstrate;
					
					Partner->BlackSubstrate += PriceInOtherSubstrate;
					Partner->OrangeSubstrate -= RequestAmount;
				}
				else
				{
					// We get black, partner gets orange
					BlackSubstrate += RequestAmount;
					OrangeSubstrate -= PriceInOtherSubstrate;
					
					Partner->OrangeSubstrate += PriceInOtherSubstrate;
					Partner->BlackSubstrate -= RequestAmount;
				}
				
				// NO relationship bonus from emergency trading (prevents circular economy exploit)
				
				// Update cooldown (10 cycles between trades with same partner)
				LastTradeRequestCycle.FindOrAdd(Partner->ControlledTeam) = CurrentDecisionCycle;
				
				return true; // Trade successful!
			}
			else
			{
				//UE_LOG(LogTemp, Log, TEXT("  - AI Team %d: Cannot afford price of %d %s"),
				//	(int32)Partner->ControlledTeam, PriceInOtherSubstrate,
				//	bRequestingOrange ? TEXT("black") : TEXT("orange"));
			}
		}
		
		// Check if all partners failed due to insufficient supply
		if (PartnersWithInsufficientSupply == PotentialPartners.Num() && PartnersWithInsufficientSupply > 0)
		{
			//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: All %d AI partners have insufficient %s supply for %d"),
			//	(int32)ControlledTeam, PartnersWithInsufficientSupply, 
			//	bRequestingOrange ? TEXT("orange") : TEXT("black"), RequestAmount);
			return false; // Signal to caller to retry with smaller amount
		}
		
		// No AI partner accepted our offer - try the PLAYER as last resort
		UE_LOG(LogTemp, Log, TEXT("AI Team %d: No AI partners available, attempting trade with PLAYER..."),
			(int32)ControlledTeam);
		
		// Check cooldown with player (40 cycles)
		if (LastTradeRequestCycle.Contains(EOwnerTeam::Player))
		{
			int32 LastRequest = LastTradeRequestCycle[EOwnerTeam::Player];
			if (CurrentDecisionCycle - LastRequest < 40)
			{
				UE_LOG(LogTemp, Log, TEXT("  - Player: On cooldown (%d cycles since last trade)"),
					CurrentDecisionCycle - LastRequest);
				return false; // Player on cooldown
			}
		}
		
		// Check relationship with player (must be >= 0.0)
		float PlayerDisposition = GameMode->GetDisposition(ControlledTeam, EOwnerTeam::Player);
		if (PlayerDisposition < 0.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Player: Hostile relationship (%.1f)"), PlayerDisposition);
			return false; // Hostile to player
		}
		
		// Check if we have good income of what we're offering (100+ per cycle required)
		int32 RequiredPlayerIncome = 100;
		int32 MyOfferingIncome = bRequestingOrange ? BlackIncomePerCycle : OrangeIncomePerCycle;
		if (MyOfferingIncome < RequiredPlayerIncome)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Player: Insufficient income of offering substrate (%d < %d)"),
				MyOfferingIncome, RequiredPlayerIncome);
			return false; // Don't spam player if we can't sustain good income
		}
		
		// Get player controller
		APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(World->GetFirstPlayerController());
		if (!PlayerController)
		{
			UE_LOG(LogTemp, Error, TEXT("  - Player: Could not find player controller!"));
			return false;
		}
		
		// Check if player has good income of requested substrate (100+ per cycle required)
		int32 PlayerIncome = bRequestingOrange ? PlayerController->PlayerOrangeIncomePerCycle : PlayerController->PlayerBlackIncomePerCycle;
		if (PlayerIncome < RequiredPlayerIncome)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Player: Insufficient income of requested substrate (%d < %d)"),
				PlayerIncome, RequiredPlayerIncome);
			return false; // Player can't sustain this income level
		}
		
		// Check if player has enough supply
		int32 PlayerSupply = bRequestingOrange ? PlayerController->PlayerOrangeSubstrate : PlayerController->PlayerBlackSubstrate;
		if (PlayerSupply < RequestAmount)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Player: Insufficient supply (%d < %d)"), PlayerSupply, RequestAmount);
			return false; // Player doesn't have enough
		}
		
		// Calculate price (same scarcity-based logic as AI-AI trades)
		int32 PlayerOrange = PlayerController->PlayerOrangeSubstrate;
		int32 PlayerBlack = PlayerController->PlayerBlackSubstrate;
		
		int32 PriceInOtherSubstrate = 0;
		
		if (bRequestingOrange)
		{
			// We want orange from player, offering black in return
			bool bOrangeIsScarce = PlayerOrange < PlayerBlack;
			
			if (bOrangeIsScarce)
			{
				// Orange is scarce - player demands 120%-140%
				PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(1.2f, 1.4f);
			}
			else
			{
				// Orange is surplus - player asks fair price 80%-100%
				PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(0.8f, 1.0f);
			}
			
			// Check if we can afford it (don't go below reserves)
			int32 AvailableBlack = BlackSubstrate - ReservedBlackSubstrate;
			if (AvailableBlack < PriceInOtherSubstrate)
			{
				UE_LOG(LogTemp, Log, TEXT("  - Player: Cannot afford price of %d black"), PriceInOtherSubstrate);
				return false;
			}
		}
		else
		{
			// We want black from player, offering orange in return
			bool bBlackIsScarce = PlayerBlack < PlayerOrange;
			
			if (bBlackIsScarce)
			{
				// Black is scarce - player demands 120%-140%
				PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(1.2f, 1.4f);
			}
			else
			{
				// Black is surplus - player asks fair price 80%-100%
				PriceInOtherSubstrate = RequestAmount * FMath::FRandRange(0.8f, 1.0f);
			}
			
			// Check if we can afford it (don't go below reserves)
			int32 AvailableOrange = OrangeSubstrate - ReservedOrangeSubstrate;
			if (AvailableOrange < PriceInOtherSubstrate)
			{
				UE_LOG(LogTemp, Log, TEXT("  - Player: Cannot afford price of %d orange"), PriceInOtherSubstrate);
				return false;
			}
		}
		
		// Player trade is viable - queue the request with GameMode
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Queueing trade request with PLAYER - Want %d %s, offering %d %s"),
			(int32)ControlledTeam, RequestAmount, bRequestingOrange ? TEXT("orange") : TEXT("black"),
			PriceInOtherSubstrate, bRequestingOrange ? TEXT("black") : TEXT("orange"));
		
		GameMode->QueueAITradeRequest(ControlledTeam, bRequestingOrange, RequestAmount, PriceInOtherSubstrate);
		
		// Update cooldown (prevent spamming player with requests)
		LastTradeRequestCycle.FindOrAdd(EOwnerTeam::Player) = CurrentDecisionCycle;
		
		// Return true because we queued the request (player will accept/decline later)
		return true;
	};
	
	// Try with original amount first
	bool bSuccess = TryTradeWithAmount(AmountNeeded);
	
	// If failed and amount was larger than minimum, retry with smaller amounts
	if (!bSuccess && bShouldRetry)
	{
		// If we requested more than 1000, try 1000 first
		if (AmountNeeded > 1000)
		{
			//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Retrying trade with 1000 %s instead of %d..."),
				//(int32)ControlledTeam, bRequestingOrange ? TEXT("orange") : TEXT("black"), AmountNeeded);
			bSuccess = TryTradeWithAmount(1000);
		}
		
		// If still failed and haven't tried 500 yet, try minimum amount
		if (!bSuccess && AmountNeeded > 500)
		{
			//UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Retrying trade with 500 %s (minimum amount)..."),
				//(int32)ControlledTeam, bRequestingOrange ? TEXT("orange") : TEXT("black"));
			bSuccess = TryTradeWithAmount(500);
		}
	}
	
	return bSuccess;
}

void AAITeamController::BuildTurretAtCity(ACityActor* City)
{
	if (!City || !GetWorld()) return;
	
	int32 TurretCost = 2000;
	
	// Check if we have enough orange substrate
	if (OrangeSubstrate < TurretCost)
	{
		return;
	}
	
	// Check if city has hit turret limit
	if (City->GetTurretCount() >= 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: City %s at max turret capacity (8)"), 
			(int32)ControlledTeam, *City->GetName());
		return;
	}
	
	// Deduct cost
	OrangeSubstrate -= TurretCost;
	
	// Add turret to city (CityActor handles positioning)
	ATurretBuildingActor* NewTurret = City->AddTurret();
	
	if (NewTurret)
	{
		//UE_LOG(LogTemp, Log, TEXT("AI Team %d: Turret built at city %s (OS: %d)"), 
		//	(int32)ControlledTeam, *City->GetName(), OrangeSubstrate);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AI Team %d: Failed to spawn turret!"), (int32)ControlledTeam);
		// Refund cost if spawn failed
		OrangeSubstrate += TurretCost;
	}
}

void AAITeamController::CollectIncome()
{
	if (!GetWorld()) return;
	
	// Find all cities in the world
	TArray<AActor*> FoundCities;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACityActor::StaticClass(), FoundCities);
	
	// Initialize each controlled city's green substrate to base value (100 from capital)
	for (AActor* Actor : FoundCities)
	{
		if (ACityActor* City = Cast<ACityActor>(Actor))
		{
			if (City->OwnerTeam == ControlledTeam)
			{
				City->GreenSubstrate = 100; // Base from capital building
			}
		}
	}
	
	// Find all resources in the world
	TArray<AActor*> FoundResources;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AResourceActor::StaticClass(), FoundResources);
	
	int32 OrangeIncome = 0;
	int32 BlackIncome = 0;
	
	// Collect income from owned resources and assign green resources to cities
	for (AActor* Actor : FoundResources)
	{
		AResourceActor* Resource = Cast<AResourceActor>(Actor);
		if (!Resource || Resource->OwnerTeam != ControlledTeam) continue;
		
		if (Resource->ResourceType == EResourceType::GreenSubstrate)
		{
			// Find closest city owned by this team
			ACityActor* ClosestCity = nullptr;
			float ClosestDistance = FLT_MAX;
			
			for (AActor* CityActor : FoundCities)
			{
				if (ACityActor* City = Cast<ACityActor>(CityActor))
				{
					if (City->OwnerTeam == ControlledTeam)
					{
						float Distance = FVector::Dist(Resource->GetActorLocation(), City->GetActorLocation());
						if (Distance < ClosestDistance)
						{
							ClosestDistance = Distance;
							ClosestCity = City;
						}
					}
				}
			}
			
			// Add resource income to closest city
			if (ClosestCity)
			{
				ClosestCity->GreenSubstrate += Resource->GetEffectiveIncome();
			}
		}
		else if (Resource->ResourceType == EResourceType::OrangeSubstrate)
		{
			OrangeIncome += Resource->GetEffectiveIncome();
		}
		else if (Resource->ResourceType == EResourceType::BlackSubstrate)
		{
			BlackIncome += Resource->GetEffectiveIncome();
		}
	}
	
	// Find all buildings (factories)
	TArray<AActor*> FoundBuildings;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABuildingActor::StaticClass(), FoundBuildings);
	
	int32 FactoryCount = 0;
	for (AActor* Actor : FoundBuildings)
	{
		if (AFactoryBuildingActor* Factory = Cast<AFactoryBuildingActor>(Actor))
		{
			if (Factory->OwnerTeam == ControlledTeam)
			{
				FactoryCount++;
			}
		}
	}
	
	// Add factory bonus (200 per factory)
	OrangeIncome += FactoryCount * 200;
	
	// Store income per cycle (for display/tracking)
	OrangeIncomePerCycle = OrangeIncome;
	BlackIncomePerCycle = BlackIncome;
	
	// Apply income
	OrangeSubstrate += OrangeIncome;
	BlackSubstrate += BlackIncome;
	
	if (OrangeIncome > 0 || BlackIncome > 0)
	{
		//UE_LOG(LogTemp, Log, TEXT("AI Team %d: Income collected - Orange: +%d (total: %d), Black: +%d (total: %d)"),
			//(int32)ControlledTeam, OrangeIncome, OrangeSubstrate, BlackIncome, BlackSubstrate);
	}
}

// ========== UTILITY FUNCTIONS ==========

ACityActor* AAITeamController::GetClosestOwnCity(FVector Location)
{
	TArray<ACityActor*> OurCities = GetControlledCities();
	ACityActor* ClosestCity = nullptr;
	float ClosestDistance = FLT_MAX;
	
	for (ACityActor* City : OurCities)
	{
		float Distance = FVector::Dist(Location, City->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestCity = City;
		}
	}
	
	return ClosestCity;
}

void AAITeamController::SendVehicleHome(AVehicleActor* Vehicle)
{
	if (!Vehicle) return;

	ACityActor* NearestCity = GetClosestOwnCity(Vehicle->GetActorLocation());
	if (NearestCity)
	{
		FVector CityLocation = NearestCity->GetActorLocation();
		FVector PlanetCenter = NearestCity->PlanetCenter;
		float PlanetRadius = NearestCity->PlanetRadius;
		FVector CityDirection = (CityLocation - PlanetCenter).GetSafeNormal();

		// Position near city
		float Radius = 1200.0f;
		FVector Tangent = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
		}

		FVector Offset = Tangent * Radius;
		FVector HomeDirection = (CityDirection * PlanetRadius + Offset).GetSafeNormal();
		FVector HomePosition = PlanetCenter + HomeDirection * PlanetRadius;

		Vehicle->SetTargetLocation(HomePosition);
	}
}

void AAITeamController::ConsumeBlackSubstrate()
{
	if (!GetWorld()) return;
	
	// Consume black substrate for AI teams based on active vehicles
	// AI uses fixed 10 BS/sec per active vehicle (Economic mode equivalent)
	// If moving AND firing, doubles to 20 BS/sec
	const int32 BSPerVehiclePerSecond = 10;
	
	// Count active vehicles and calculate consumption
	TArray<AVehicleActor*> Vehicles = GetControlledVehicles();
	int32 TotalConsumption = 0;
	
	for (AVehicleActor* Vehicle : Vehicles)
	{
		if (!Vehicle) continue;
		
		bool bMoving = Vehicle->bIsMoving;
		bool bFiring = Vehicle->bIsFiring;
		
		// If doing both moving AND firing, double the consumption
		if (bMoving && bFiring)
		{
			TotalConsumption += BSPerVehiclePerSecond * 2;  // 20 BS/sec
		}
		else if (bMoving || bFiring)
		{
			TotalConsumption += BSPerVehiclePerSecond;  // 10 BS/sec
		}
		// If neither moving nor firing, no consumption
	}
	
	// Only consume if we have active vehicles
	if (TotalConsumption > 0)
	{
		BlackSubstrate = FMath::Max(0, BlackSubstrate - TotalConsumption);
	}
}

bool AAITeamController::RequestAidFromAllies(ACityActor* CityUnderAttack, bool bNeedSubstrate, bool bNeedMilitary, int32 SubstrateNeeded)
{
	if (!CityUnderAttack || AllianceState.AlliedTeams.Num() == 0)
	{
		return false; // No allies to request from
	}
	
	// Check if we already have a pending aid request for this city
	for (const FAidRequest& ExistingRequest : AidRequests)
	{
		if (ExistingRequest.RequestingTeam == ControlledTeam && 
		    ExistingRequest.CityUnderAttack == CityUnderAttack &&
		    !ExistingRequest.bPlayerHasResponded)
		{
			return false; // Already requested, waiting for response
		}
	}
	
	// Create aid request
	FAidRequest NewRequest;
	NewRequest.RequestingTeam = ControlledTeam;
	NewRequest.CityUnderAttack = CityUnderAttack;
	NewRequest.RequestCycle = CurrentDecisionCycle;
	NewRequest.bRequestedSubstrate = bNeedSubstrate;
	NewRequest.bRequestedMilitary = bNeedMilitary;
	NewRequest.SubstrateAmount = SubstrateNeeded;
	NewRequest.bWaitingForPlayerResponse = false;
	NewRequest.bPlayerHasResponded = false;
	
	// Send request to all allies
	bool bRequestSent = false;
	for (EOwnerTeam AllyTeam : AllianceState.AlliedTeams)
	{
		if (AllyTeam == EOwnerTeam::Player)
		{
			// Player needs to respond via dialogue UI
			NewRequest.bWaitingForPlayerResponse = true;
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Requesting aid from PLAYER (substrate:%d military:%d)"),
				(int32)ControlledTeam, bNeedSubstrate ? SubstrateNeeded : 0, bNeedMilitary ? 1 : 0);
		}
		else
		{
			// AI ally - send to their controller
			TArray<AActor*> FoundControllers;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
			
			for (AActor* Actor : FoundControllers)
			{
				AAITeamController* AllyController = Cast<AAITeamController>(Actor);
				if (AllyController && AllyController->ControlledTeam == AllyTeam)
				{
					// Add request to ally's queue - they'll evaluate it in their Priority 6 layer
					AllyController->AidRequests.Add(NewRequest);
					UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Requesting aid from AI Team %d"),
						(int32)ControlledTeam, (int32)AllyTeam);
					bRequestSent = true;
					break;
				}
			}
		}
	}
	
	// Add to our own tracking array
	AidRequests.Add(NewRequest);
	
	return bRequestSent || NewRequest.bWaitingForPlayerResponse;
}

bool AAITeamController::EvaluateAidRequest(const FAidRequest& Request)
{
	// Check if we're allied with requesting team
	if (!IsAlliedWith(Request.RequestingTeam))
	{
		return false;
	}
	
	bool bAidSent = false;
	
	// Send substrate aid if requested and we can afford it
	if (Request.bRequestedSubstrate && Request.SubstrateAmount > 0)
	{
		int32 AmountSent = SendSubstrateAid(Request.RequestingTeam, Request.SubstrateAmount);
		if (AmountSent > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Sent %d orange substrate to ally Team %d"),
				(int32)ControlledTeam, AmountSent, (int32)Request.RequestingTeam);
			bAidSent = true;
		}
	}
	
	// Send military aid if requested
	if (Request.bRequestedMilitary && Request.CityUnderAttack)
	{
		int32 VehiclesSent = SendMilitaryAid(Request.RequestingTeam, Request.CityUnderAttack);
		if (VehiclesSent > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Sent %d vehicles to aid ally Team %d"),
				(int32)ControlledTeam, VehiclesSent, (int32)Request.RequestingTeam);
			bAidSent = true;
		}
	}
	
	return bAidSent;
}

int32 AAITeamController::SendSubstrateAid(EOwnerTeam AllyTeam, int32 RequestedAmount)
{
	// Calculate how much we can afford to send (max 5000, only surplus from priorities)
	int32 SurplusOrange = OrangeSubstrate - ReservedOrangeSubstrate;
	int32 MaxCanSend = FMath::Min(5000, SurplusOrange);
	int32 AmountToSend = FMath::Min(RequestedAmount, MaxCanSend);
	
	if (AmountToSend <= 0)
	{
		return 0; // Can't afford to send anything
	}
	
	// Deduct from our substrate
	OrangeSubstrate -= AmountToSend;
	
	// Add to ally's substrate
	if (AllyTeam == EOwnerTeam::Player)
	{
		// Give to player
		APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
		if (PlayerController)
		{
			PlayerController->PlayerOrangeSubstrate += AmountToSend;
			UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Sent %d orange to PLAYER"),
				(int32)ControlledTeam, AmountToSend);
		}
	}
	else
	{
		// Give to AI ally
		TArray<AActor*> FoundControllers;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAITeamController::StaticClass(), FoundControllers);
		
		for (AActor* Actor : FoundControllers)
		{
			AAITeamController* AllyController = Cast<AAITeamController>(Actor);
			if (AllyController && AllyController->ControlledTeam == AllyTeam)
			{
				AllyController->OrangeSubstrate += AmountToSend;
				break;
			}
		}
	}
	
	return AmountToSend;
}

int32 AAITeamController::SendMilitaryAid(EOwnerTeam AllyTeam, ACityActor* AllyCityUnderAttack)
{
	if (!AllyCityUnderAttack)
	{
		return 0;
	}
	
	// Find up to 10 idle vehicles (not assigned to any priority)
	TArray<AVehicleActor*> AllVehicles = GetControlledVehicles();
	TArray<AVehicleActor*> IdleVehicles;
	
	for (AVehicleActor* Vehicle : AllVehicles)
	{
		if (!Vehicle || !IsValid(Vehicle)) continue;
		
		// Check if vehicle is truly idle (not in any priority array)
		bool bInPriorityArray = Priority1Vehicles.Contains(Vehicle) ||
		                        Priority2Vehicles.Contains(Vehicle) ||
		                        Priority3Vehicles.Contains(Vehicle) ||
		                        Priority4Vehicles.Contains(Vehicle) ||
		                        Priority5Vehicles.Contains(Vehicle) ||
		                        Priority6Vehicles.Contains(Vehicle);
		
		if (!bInPriorityArray && !Vehicle->bHasTarget && !Vehicle->TargetResource)
		{
			IdleVehicles.Add(Vehicle);
			if (IdleVehicles.Num() >= 10) break; // Max 10 vehicles
		}
	}
	
	if (IdleVehicles.Num() == 0)
	{
		return 0; // No idle vehicles available
	}
	
	// Find attackers at ally's city
	TArray<AVehicleActor*> Attackers;
	TArray<AActor*> AllEnemyVehicles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVehicleActor::StaticClass(), AllEnemyVehicles);
	
	float TerritoryRadius = 5000.0f;
	for (AActor* Actor : AllEnemyVehicles)
	{
		AVehicleActor* EnemyVehicle = Cast<AVehicleActor>(Actor);
		if (!EnemyVehicle || !IsValid(EnemyVehicle)) continue;
		if (EnemyVehicle->OwnerTeam == AllyTeam || EnemyVehicle->OwnerTeam == ControlledTeam) continue;
		
		float DistanceToCity = FVector::Dist(EnemyVehicle->GetActorLocation(), AllyCityUnderAttack->GetActorLocation());
		if (DistanceToCity <= TerritoryRadius)
		{
			Attackers.Add(EnemyVehicle);
		}
	}
	
	if (Attackers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: No attackers found at ally city - aid not needed"),
			(int32)ControlledTeam);
		return 0; // No attackers, aid not needed
	}
	
	// Send idle vehicles to attack the attackers
	int32 VehiclesSent = 0;
	for (int32 i = 0; i < FMath::Min(IdleVehicles.Num(), Attackers.Num()); i++)
	{
		AVehicleActor* AidVehicle = IdleVehicles[i];
		AVehicleActor* TargetAttacker = Attackers[i % Attackers.Num()]; // Cycle through attackers
		
		// Send vehicle to attack
		AidVehicle->SetTargetLocation(TargetAttacker->GetActorLocation());
		AidVehicle->CurrentTarget = TargetAttacker;
		AidVehicle->bHasTarget = true;
		AidVehicle->bAIAutonomousMode = false;
		AidVehicle->bInP6 = true; // Mark as Priority 6 (aid mission)
		
		// Store ally city as PrimaryTarget so vehicle knows to return home after mission
		// We'll use PrimaryTarget to track the ally city we're defending
		AidVehicle->PrimaryTarget = AllyCityUnderAttack;
		
		// Add to Priority 6 tracking
		Priority6Vehicles.Add(AidVehicle);
		
		VehiclesSent++;
		UE_LOG(LogTemp, Warning, TEXT("AI Team %d: Sent aid vehicle to attack enemy at ally Team %d city"),
			(int32)ControlledTeam, (int32)AllyTeam);
	}
	
	return VehiclesSent;
}
