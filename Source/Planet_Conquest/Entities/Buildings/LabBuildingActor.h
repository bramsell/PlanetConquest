// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingActor.h"
#include "LabBuildingActor.generated.h"

UCLASS()
class PLANET_CONQUEST_API ALabBuildingActor : public ABuildingActor
{
	GENERATED_BODY()
	
public:	
	ALabBuildingActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Second building mesh - the lab consists of two building structures
	// Main BuildingMesh (from parent) = horizontal building (3x1x1)
	// This is the detached cube building (1x1x1)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lab")
	class UStaticMeshComponent* CubeBuildingMesh;

	void UpdateInfoDisplay();

	virtual void SetSelected(bool bSelected) override;

	// Lab operation mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	bool bIsNationalized = true; // true = nationalized (slow, guaranteed), false = privatized (fast, company influence)

	// Assigned company (only if privatized)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	FString AssignedCompany = "";

	// Research properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	float ResearchCycleInterval = 600.0f; // 10 minutes in seconds

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	float BaseDiscoveryChance = 0.05f; // 5% base chance

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	float LabDiscoveryBonus = 0.08f; // +8% per lab

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	float PrivateMultiplier = 2.5f; // 2.5x for privatized labs

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lab")
	int32 BuildCost = 1500;

	// Toggle nationalized/privatized
	UFUNCTION(BlueprintCallable, Category = "Lab")
	void ToggleNationalization();

	UFUNCTION(BlueprintCallable, Category = "Lab")
	void Nationalize();

	UFUNCTION(BlueprintCallable, Category = "Lab")
	void Privatize(const FString& CompanyName);

private:
	float TimeSinceLastResearchCycle = 0.0f;

	void TickResearch(float DeltaTime);
};
