// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KaijuActor.generated.h"

// Forward declarations
class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class AVehicleActor;
class ACityActor;
class AResourceActor;

/**
 * Relationship tracking between kaiju and teams
 */
USTRUCT(BlueprintType)
struct FKaijuTeamRelationship
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float RelationshipValue = 0.0f; // 0.0 = neutral, < 0.0 = hostile

	UPROPERTY(BlueprintReadWrite)
	float LastDamageTime = 0.0f; // Game time when last damage occurred

	FKaijuTeamRelationship() : RelationshipValue(0.0f), LastDamageTime(0.0f) {}
};

/**
 * Kaiju state machine
 */
UENUM(BlueprintType)
enum class EKaijuState : uint8
{
	Idle UMETA(DisplayName = "Idle"),              // Wandering patrol area, calm
	Alerted UMETA(DisplayName = "Alerted"),        // Detected something, moving to investigate
	Attacking UMETA(DisplayName = "Attacking"),    // Actively fighting vehicles
	Pursuing UMETA(DisplayName = "Pursuing"),      // Chasing vehicles beyond normal radius
	Invading UMETA(DisplayName = "Invading"),      // Targeting a city directly
	Returning UMETA(DisplayName = "Returning")     // Heading back to guard area after chase
};

/**
 * Kaiju size variants
 */
UENUM(BlueprintType)
enum class EKaijuSize : uint8
{
	Small UMETA(DisplayName = "Small"),
	Medium UMETA(DisplayName = "Medium"),
	Large UMETA(DisplayName = "Large")
};

/**
 * Kaiju actor - guards resource clusters and mountains
 * Can attack vehicles and invade cities
 */
UCLASS()
class PLANET_CONQUEST_API AKaijuActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AKaijuActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Planet alignment
	UPROPERTY(BlueprintReadWrite, Category = "Kaiju|Planet")
	FVector PlanetCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Kaiju|Planet")
	float PlanetRadius = 10000.0f;

	// Continent this kaiju guards (index into PlanetActor::ContinentSeeds; -1 = ocean/unknown)
	// Set in BeginPlay from spawn location
	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Planet")
	int32 ContinentID = -1;

	UFUNCTION(BlueprintCallable, Category = "Kaiju")
	void AlignToPlanet();

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kaiju|Components")
	UCapsuleComponent* CollisionCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kaiju|Components")
	UStaticMeshComponent* KaijuMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kaiju|Components")
	UWidgetComponent* HealthBarWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kaiju|Components")
	UWidgetComponent* InfoWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kaiju|Components")
	UStaticMeshComponent* SelectionBox;

	void UpdateInfoDisplay();

	// Selection/Hover
	UFUNCTION(BlueprintCallable, Category = "Kaiju")
	void SetHovered(bool bHovered);

	// State machine
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|State")
	EKaijuState CurrentState = EKaijuState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|State")
	EKaijuSize KaijuSize = EKaijuSize::Medium;

	// Guard area
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Guard")
	FVector GuardCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Guard")
	float GuardRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Guard")
	float LeashRadius = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Guard")
	float InvasionRadius = 3500.0f;

	// Detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Detection")
	float BaseDetectionRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Detection")
	float DetectionMultiplierCapturing = 2.5f; // When vehicle is capturing resources

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Detection")
	float DetectionMultiplierCombat = 1.3f; // When vehicle is in combat

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Detection")
	float DetectionPerVehicle = 50.0f; // Extra radius per nearby vehicle

	// Combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Combat")
	float MaxHealth = 500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Combat")
	float CurrentHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Combat")
	float AttackRange = 1800.0f; // Same range as vehicles

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Combat")
	float AttackDamage = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Combat")
	float AttackCooldown = 2.5f;

	float TimeSinceLastAttack = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Combat")
	AActor* CurrentTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Combat")
	bool bIsFiring = false;

	// Movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Movement")
	float MovementSpeed = 300.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Movement")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Movement")
	bool bHasTarget = false;

	// Obstacle avoidance (same system as vehicles)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Movement")
	float AvoidanceDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Movement")
	float LookAheadTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Movement")
	float LateralCheckDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Movement")
	int32 SteeringAngles = 8;

	FVector PreviousSteeringDirection = FVector::ZeroVector;

	// Invasion system
	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Invasion")
	float InvasionUrge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Invasion")
	int32 ProvocationCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Invasion")
	float BaseInvasionRate = 0.01f; // Per second accumulation

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Invasion")
	float ProvocationUrgeBonus = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kaiju|Invasion")
	float InvasionThreshold = 100.0f;

	float TimeSinceLastInvasionCheck = 0.0f;

	// Task-based invasion system
	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Invasion")
	TArray<class ABuildingActor*> InvasionTargets; // List of buildings to destroy during invasion

	// State transition functions
	void TransitionToIdle();
	void TransitionToAlerted(AVehicleActor* DetectedVehicle);
	void TransitionToAttacking(AActor* Target);
	void TransitionToPursuing(AActor* Target);
	void TransitionToInvading(ACityActor* TargetCity);
	void TransitionToReturning();

	// State behavior functions
	void UpdateIdle(float DeltaTime);
	void UpdateAlerted(float DeltaTime);
	void UpdateAttacking(float DeltaTime);
	void UpdatePursuing(float DeltaTime);
	void UpdateInvading(float DeltaTime);
	void UpdateReturning(float DeltaTime);

	// Helper functions
	float GetDetectionRadius(AVehicleActor* Vehicle, const TArray<AActor*>& AllVehicles);
	bool IsWithinLeash() const;
	bool ShouldReturn() const;
	float CalculateInvasionUrge();
	AVehicleActor* FindNearestVehicle(float SearchRadius);
	ACityActor* FindNearestCity(float SearchRadius);
	bool IsResourceInGuardArea(AResourceActor* Resource);

	// Invasion helpers
	bool CanInvadeCity(ACityActor* City); // Check if kaiju size can invade based on turret count
	void SetupInvasionTargets(ACityActor* City); // Gather turrets and mines to destroy

	// Movement helpers (similar to vehicle)
	void MoveTowardsTarget(float DeltaTime);
	bool DetectObstacleAhead(FVector CurrentDirection, float& OutDistance, FVector& OutHitLocation, AActor*& OutHitActor);
	bool CheckLateralObstacle(FVector CurrentDirection, float AngleDegrees, float& OutDistance);
	FVector CalculateSteeringDirection(FVector CurrentDirection, FVector TargetDirection, float DeltaTime);

	// Combat
	void UpdateCombat(float DeltaTime);
	void FireAtTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Kaiju|Combat")
	void ApplyDamage(float DamageAmount, AActor* AttackingActor);

	void UpdateHealthBar();

	// Relationship system
	UPROPERTY(BlueprintReadOnly, Category = "Kaiju|Relationships")
	TMap<int32, FKaijuTeamRelationship> TeamRelationships;

	UFUNCTION(BlueprintCallable, Category = "Kaiju|Relationships")
	void UpdateRelationshipForDamage(int32 TeamID, float CurrentTime);

	UFUNCTION(BlueprintCallable, Category = "Kaiju|Relationships")
	bool IsHostileToTeam(int32 TeamID) const;

	void UpdateRelationships(float CurrentTime);

	// Size configuration
	void ConfigureForSize(EKaijuSize Size);

private:
	// Idle patrol
	FVector IdleWanderTarget = FVector::ZeroVector;
	float TimeSinceLastWander = 0.0f;
	float WanderInterval = 5.0f;
	
	// Resting system (kaiju stays in place ~50% of the time)
	bool bIsResting = false;
	float TimeInCurrentRestState = 0.0f;
	float RestStateDuration = 0.0f;
	
	// Health bar tracking
	float PreviousHealth = 500.0f;
	float LastHealthChangeTime = -999.0f;
	
	// Mine destruction timer
	float TimeSinceLastMineCheck = 0.0f;
	
	// City invasion timer (every 30s, 2% chance)
	float TimeSinceLastCityInvasionCheck = 0.0f;

	// Vehicle cache to avoid O(N²) GetAllActorsOfClass in detection (refreshed every 2s)
	UPROPERTY()
	TArray<AActor*> CachedKaijuVehicles;
	float KaijuVehicleCacheTimer = 999.0f;
};
