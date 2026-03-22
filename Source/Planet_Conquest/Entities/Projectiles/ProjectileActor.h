// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Resources/ResourceActor.h"
#include "ProjectileActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API AProjectileActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AProjectileActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Visual mesh component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	class UStaticMeshComponent* ProjectileMesh;

	// Collision sphere
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	class USphereComponent* CollisionSphere;

	// Particle trail effect (Niagara)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	class UNiagaraComponent* ParticleTrail;

	// Movement properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float ProjectileSpeed = 600.0f; // Units per second (20% faster to reach turret range)

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	AActor* TargetActor = nullptr;

	// Damage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage = 10.0f;

	// Owner team (for friendly fire prevention)
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	EOwnerTeam OwnerTeam = EOwnerTeam::Neutral;

	// Planet properties for surface following
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector PlanetCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float PlanetRadius = 100000.0f;

	// Lifetime before auto-destroy
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float MaxLifetime = 7.0f; // Increased to ensure projectiles reach turret's 3000 unit range

	float CurrentLifetime = 0.0f;

private:
	void MoveTowardsTarget(float DeltaTime);
};
