// Copyright Benjamin Ramsell. All Rights Reserved.

#include "ProjectileActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "../Vehicles/VehicleActor.h"
#include "../Cities/CityActor.h"
#include "../Buildings/BuildingActor.h"
#include "../Kaiju/KaijuActor.h"

AProjectileActor::AProjectileActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create collision sphere (root component) - NO COLLISION, projectiles fly through everything
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->InitSphereRadius(10.0f); // Small projectile
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); // No collision - use manual distance check
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Create projectile mesh (using sphere)
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);

	// Load sphere mesh from engine content
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereMesh.Object);
	}

	// Scale for small projectile size
	ProjectileMesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.2f));
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Load team color material (same as vehicles/cities)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColorMat(TEXT("/Game/M_TeamColor"));
	if (TeamColorMat.Succeeded())
	{
		ProjectileMesh->SetMaterial(0, TeamColorMat.Object);
	}

	// Create particle trail (will be configured in BeginPlay based on team)
	ParticleTrail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ParticleTrail"));
	ParticleTrail->SetupAttachment(RootComponent);
	ParticleTrail->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f)); // Scale down for smaller trail
	ParticleTrail->bAutoActivate = false; // Activate in BeginPlay after setting the system
}

void AProjectileActor::BeginPlay()
{
	Super::BeginPlay();

	// Set bright glowing orange color
	if (ProjectileMesh)
	{
		UMaterialInstanceDynamic* DynMaterial = ProjectileMesh->CreateDynamicMaterialInstance(0);
		if (DynMaterial)
		{
			FLinearColor BrightOrange = FLinearColor(3.0f, 1.5f, 0.0f); // Emissive bright orange
			DynMaterial->SetVectorParameterValue(FName("BaseColor"), BrightOrange);
			DynMaterial->SetVectorParameterValue(FName("Color"), BrightOrange);
			DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), BrightOrange);
		}
	}

	// Assign particle effect based on team
	// AI teams randomly select from ArrowTrail Niagara effects (designed for projectiles)
	if (ParticleTrail && OwnerTeam != EOwnerTeam::Player && OwnerTeam != EOwnerTeam::Neutral)
	{
		UNiagaraSystem* ParticleSystem = nullptr;
		
		// AI teams randomly pick from 5 arrow trail particle options
		int32 ParticleChoice = FMath::RandRange(0, 4);
		
		switch (ParticleChoice)
		{
			case 0:
				ParticleSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Fire"));
				break;
			case 1:
				ParticleSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Magic"));
				break;
			case 2:
				ParticleSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Ice"));
				break;
			case 3:
				ParticleSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Holy"));
				break;
			case 4:
				ParticleSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Nature"));
				break;
		}
		
		if (ParticleSystem)
		{
			ParticleTrail->SetAsset(ParticleSystem);
			ParticleTrail->Activate();
			UE_LOG(LogTemp, Warning, TEXT("Projectile for team %d loaded arrow trail effect"), (int32)OwnerTeam);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load arrow trail effect for team %d"), (int32)OwnerTeam);
		}
	}
}

void AProjectileActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Move towards target
	MoveTowardsTarget(DeltaTime);

	// Track lifetime and destroy if too old
	CurrentLifetime += DeltaTime;
	if (CurrentLifetime > MaxLifetime)
	{
		Destroy();
	}
}

void AProjectileActor::MoveTowardsTarget(float DeltaTime)
{
	FVector CurrentLocation = GetActorLocation();

	// Update target location if tracking an actor
	if (TargetActor && IsValid(TargetActor))
	{
		TargetLocation = TargetActor->GetActorLocation();
	}
	else if (TargetActor && !IsValid(TargetActor))
	{
		// Target was destroyed, clear reference and destroy projectile
		TargetActor = nullptr;
		Destroy();
		return;
	}

	// Calculate direction to target
	FVector DirectionToTarget = (TargetLocation - CurrentLocation).GetSafeNormal();

	// Move straight towards target
	FVector NewLocation = CurrentLocation + DirectionToTarget * ProjectileSpeed * DeltaTime;

	// Project onto planet surface to follow curvature, then apply arc height
	FVector DirectionFromPlanet = (NewLocation - PlanetCenter).GetSafeNormal();
	float ArcOffset = 0.0f;
	if (ArcHeight > 0.0f && ArcMaxFlightTime > KINDA_SMALL_NUMBER)
	{
		// Progress 0→1 over the estimated flight time; sin curve peaks at midpoint
		float Progress = FMath::Clamp(CurrentLifetime / ArcMaxFlightTime, 0.0f, 1.0f);
		ArcOffset = ArcHeight * FMath::Sin(Progress * PI);
	}
	NewLocation = PlanetCenter + DirectionFromPlanet * (PlanetRadius + ArcOffset);

	// MANUAL DISTANCE CHECK - Fallback for when sweep fails
	if (TargetActor && IsValid(TargetActor))
	{
		float DistanceToTarget = FVector::Dist(NewLocation, TargetLocation);
		
		// Check if we're within hit range (city radius is 400, add projectile size and margin)
		if (DistanceToTarget < 450.0f)
		{
			// Check for city hit
			if (ACityActor* TargetCity = Cast<ACityActor>(TargetActor))
			{
				if (TargetCity->OwnerTeam != OwnerTeam)
				{
				TargetCity->ApplyDamage(Damage, OwnerTeam, GetOwner()); // Pass the vehicle that fired this projectile
					UParticleSystem* ExplosionParticle = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/StarterContent/Particles/P_Explosion"));
					if (ExplosionParticle)
					{
						// Offset explosion outward from planet center
						FVector ExplosionLocation = TargetLocation;
						if (!TargetCity->PlanetCenter.IsNearlyZero())
						{
							FVector SurfaceNormal = (TargetLocation - TargetCity->PlanetCenter).GetSafeNormal();
							ExplosionLocation = TargetLocation + SurfaceNormal * 300.0f; // Offset 300 units outward
						}
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionParticle, ExplosionLocation, FRotator::ZeroRotator, FVector(1.5f, 1.5f, 1.5f));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to load explosion particle!"));
					}
					
					Destroy();
					return;
				}
			}
			// Check for vehicle hit
			else if (AVehicleActor* TargetVehicle = Cast<AVehicleActor>(TargetActor))
			{
				if (TargetVehicle->OwnerTeam != OwnerTeam)
				{
				TargetVehicle->ApplyDamage(Damage, OwnerTeam, GetOwner()); // Pass the vehicle that fired this projectile
					UParticleSystem* ExplosionParticle = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/StarterContent/Particles/P_Explosion"));
					if (ExplosionParticle)
					{
						UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionParticle, TargetLocation, FRotator::ZeroRotator, FVector(1.5f, 1.5f, 1.5f));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to load explosion particle!"));
					}
					
					Destroy();
					return;
				}
			}
			// Check for building hit (turrets, factories)
			else if (ABuildingActor* TargetBuilding = Cast<ABuildingActor>(TargetActor))
			{
				if (TargetBuilding->OwnerTeam != OwnerTeam)
				{
				TargetBuilding->ApplyDamage(Damage, OwnerTeam, GetOwner()); // Pass the vehicle that fired this projectile
					UParticleSystem* ExplosionParticle = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/StarterContent/Particles/P_Explosion"));
					if (ExplosionParticle)
					{
						// Offset explosion outward from planet center
						FVector ExplosionLocation = TargetLocation;
						if (!TargetBuilding->PlanetCenter.IsNearlyZero())
						{
							FVector SurfaceNormal = (TargetLocation - TargetBuilding->PlanetCenter).GetSafeNormal();
							ExplosionLocation = TargetLocation + SurfaceNormal * 300.0f; // Offset 300 units outward
						}
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionParticle, ExplosionLocation, FRotator::ZeroRotator, FVector(1.5f, 1.5f, 1.5f));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to load explosion particle!"));
					}
					
					Destroy();
					return;
				}
			}
			// Check for kaiju hit
			else if (AKaijuActor* TargetKaiju = Cast<AKaijuActor>(TargetActor))
			{
				// Kaiju don't have teams - always hit them
				TargetKaiju->ApplyDamage(Damage, GetOwner()); // Pass the vehicle that fired this projectile
				UParticleSystem* ExplosionParticle = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/StarterContent/Particles/P_Explosion"));
				if (ExplosionParticle)
				{
					// Offset explosion outward from planet center
					FVector ExplosionLocation = TargetLocation;
					if (!TargetKaiju->PlanetCenter.IsNearlyZero())
					{
						FVector SurfaceNormal = (TargetLocation - TargetKaiju->PlanetCenter).GetSafeNormal();
						ExplosionLocation = TargetLocation + SurfaceNormal * 300.0f; // Offset 300 units outward
					}
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionParticle, ExplosionLocation, FRotator::ZeroRotator, FVector(1.5f, 1.5f, 1.5f));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to load explosion particle!"));
				}
				
				Destroy();
				return;
			}
		}
	}

	// Move projectile - no collision, just set position
	SetActorLocation(NewLocation, false); // No sweep needed, projectiles fly through everything

	// Orient projectile to face movement direction
	FVector SurfaceNormal = (NewLocation - PlanetCenter).GetSafeNormal();
	FRotator NewRotation = FRotationMatrix::MakeFromZX(SurfaceNormal, DirectionToTarget).Rotator();
	SetActorRotation(NewRotation);

	// Check if we've reached the target
	float DistanceToTarget = FVector::Dist(NewLocation, TargetLocation);
	if (DistanceToTarget < 50.0f) // Close enough
	{
		// Missed or target destroyed
		Destroy();
	}
}
