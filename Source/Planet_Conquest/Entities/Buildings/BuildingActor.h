// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Resources/ResourceActor.h"
#include "BuildingActor.generated.h"

UENUM(BlueprintType)
enum class EBuildingType : uint8
{
	Capital,
	Factory,
	Turret,
	Lab
};

UCLASS()
class PLANET_CONQUEST_API ABuildingActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ABuildingActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Visual mesh component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	class UStaticMeshComponent* BuildingMesh;

	// Selection box (wireframe outline)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	class UStaticMeshComponent* SelectionBox;

	// Collision component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	class USphereComponent* CollisionSphere;

	// Health bar widget
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Health")
	class UWidgetComponent* HealthBarWidget;

	// Info widget (shows on hover in city editor)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	class UWidgetComponent* InfoWidget;

	// Building properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	EBuildingType BuildingType = EBuildingType::Capital;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	FVector PlanetCenter = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float PlanetRadius = 100000.0f;

	// Ownership
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	EOwnerTeam OwnerTeam = EOwnerTeam::Neutral;

	// Parent city that owns this building
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	class ACityActor* ParentCity = nullptr;

	// Health system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Health")
	float MaxHealth = 500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Building|Health")
	float CurrentHealth = 500.0f;

	UFUNCTION(BlueprintCallable, Category = "Building|Health")
	void ApplyDamage(float DamageAmount, EOwnerTeam AttackerTeam = EOwnerTeam::Neutral, AActor* AttackingActor = nullptr);

	// Track last team/actor that damaged this building for defense AI
	EOwnerTeam LastDamagingTeam = EOwnerTeam::Neutral;
	TWeakObjectPtr<AActor> LastDamagingActor = nullptr;
	float TimeSinceLastDamaged = 999.0f; // Start high so building can heal immediately at start

	// Track if this building has been attacked before (for first-attack detection)
	bool bHasBeenAttacked = false;

	void UpdateHealthBar();

	// Healing system (city-based: no building heals if any building was damaged in last 30 seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Healing")
	float HealingStartDelay = 30.0f; // Seconds after last damage to any building in city before healing starts

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Healing")
	float HealingInterval = 0.5f; // Heal every 0.5 seconds

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Healing")
	float HealingAmount = 1.0f; // HP restored per heal tick

	float TimeSinceLastHeal = 0.0f;

	// Align building to point outward from planet center
	UFUNCTION(CallInEditor, Category = "Building")
	void AlignToPlanet();

	// Update color based on team
	UFUNCTION(BlueprintCallable, Category = "Building")
	void UpdateColor();

	// Get team color
	FLinearColor GetTeamColor() const;

	// Selection
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	bool bIsSelected = false;

	UFUNCTION(BlueprintCallable, Category = "Building")
	virtual void SetSelected(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "Building")
	virtual void SetHovered(bool bHovered);

	// Update info widget display (virtual for different building types)
	virtual void UpdateInfoDisplay();
};
