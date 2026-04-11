// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PlanetActor.h"
#include "../Core/PlanetConquestSaveGame.h"
#include "../Core/PlanetConquestGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "../Entities/Cities/CityActor.h"
#include "../Entities/Vehicles/VehicleActor.h"
#include "../Entities/Resources/ResourceActor.h"
#include "../Entities/Buildings/MineActor.h"
#include "../Core/PlanetConquestPlayerController.h"
#include "../Core/AITeamController.h"
#include "../Core/PlanetConquestGameMode.h"

// =============================================================================
// DEBUG CONFIG — toggle these to enable/disable debug visualizations and logs
// =============================================================================
static constexpr bool bDebugDrawNavWaypoints = false; // Yellow spheres at each nav waypoint in FindPath
// =============================================================================

APlanetActor::APlanetActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create procedural mesh component
	PlanetMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PlanetMesh"));
	RootComponent = PlanetMesh;

	// Enable collision for click detection
	PlanetMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlanetMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PlanetMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Create directional light (sun)
	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(RootComponent);
	SunLight->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));
	SunLight->Intensity = 0.3f;
	SunLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.9f)); // Slightly warm white
	SunLight->LightSourceAngle = 10.0f; // 5 degree source angle for softer shadows
	SunLight->CastShadows = true; // Enable shadows

	// Create fill light (opposite angle for better visibility)
	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(RootComponent);
	FillLight->SetRelativeRotation(FRotator(45.0f, 180.0f, 0.0f)); // Opposite side
	FillLight->Intensity = 0.1f; // Very low intensity
	FillLight->SetLightColor(FLinearColor(0.3f, 0.4f, 1.0f)); // More bluish tint
	FillLight->CastShadows = false; // No shadows from fill light
	FillLight->bAtmosphereSunLight = false; // Disable atmosphere
	FillLight->bCastCloudShadows = false; // No cloud shadows

	// Create water sphere mesh
	WaterSphereMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterSphereMesh"));
	WaterSphereMesh->SetupAttachment(RootComponent);
	WaterSphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Water is visual only

	// Create skybox sphere (large sphere around planet for starfield)
	SkyboxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyboxMesh"));
	SkyboxMesh->SetupAttachment(RootComponent);
	SkyboxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyboxMesh->SetCastShadow(false);
	
	// Load sphere mesh for skybox
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		SkyboxMesh->SetStaticMesh(SphereMesh.Object);
		
		// Scale skybox to start at planet surface and extend 100k units beyond
		// Planet radius: 80k, Extra space: 100k = Total: 180k radius
		// Base sphere is 50 units radius, so scale = 180000 / 50 = 3600x
		SkyboxMesh->SetRelativeScale3D(FVector(3600.0f, 3600.0f, 3600.0f));
		
		// Set to render behind everything else
		SkyboxMesh->SetTranslucentSortPriority(-100);
		
		UE_LOG(LogTemp, Log, TEXT("Skybox sphere created: 180,000 unit radius (planet surface + 100k). Assign material from /Engine/MapTemplates/Sky"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load sphere mesh for skybox"));
	}

	// Load default water material from StarterContent (optional - can be set in editor)
	// NOTE: M_Ocean_Translucent is translucent and may not render correctly on sphere
	// Consider creating a simple opaque blue material instead
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMat(TEXT("/Game/Materials/M_Ocean_Translucent"));
	if (WaterMat.Succeeded())
	{
		WaterMaterial = WaterMat.Object;
		UE_LOG(LogTemp, Log, TEXT("Water material loaded successfully from StarterContent"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load water material - using vertex colors. Assign WaterMaterial in editor for better results."));
	}

	// Load planet blended material (base + basalt volcano blend)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlanetMat(TEXT("/Game/Materials/M_Planet_Blended"));
	if (PlanetMat.Succeeded())
	{
		PlanetMaterial = PlanetMat.Object;
		UE_LOG(LogTemp, Log, TEXT("Planet blended material loaded successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load planet material from /Game/Materials/M_Planet_Blended"));
	}

	// Load basalt rock material for volcano areas
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasaltMat(TEXT("/Game/StarterContent/Materials/M_Rock_Basalt"));
	if (BasaltMat.Succeeded())
	{
		BasaltMaterial = BasaltMat.Object;
		UE_LOG(LogTemp, Log, TEXT("Basalt material loaded successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load basalt material from /Game/StarterContent/Materials/M_Rock_Basalt"));
	}

	// Load volcano heightmap texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> VolcanoTex(TEXT("/Game/BlenderAssets/Volcanoes/volcano_heightmap"));
	if (VolcanoTex.Succeeded())
	{
		VolcanoHeightmap = VolcanoTex.Object;
		UE_LOG(LogTemp, Log, TEXT("Volcano heightmap texture loaded successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load volcano heightmap texture from /Game/BlenderAssets/Volcanoes/volcano_heightmap"));
	}
}

void APlanetActor::BeginPlay()
{
	Super::BeginPlay();

	// FORCE reload planet material to override any saved level instance values
	// This prevents the material from reverting to old saved values (like M_Rock_Sandstone)
	if (!PlanetMaterial || PlanetMaterial->GetPathName().Contains(TEXT("Sandstone")))
	{
		UMaterialInterface* ForcedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Planet_Blended"));
		if (ForcedMaterial)
		{
			PlanetMaterial = ForcedMaterial;
			UE_LOG(LogTemp, Warning, TEXT("FORCED planet material reload from /Game/Materials/M_Planet_Blended"));
		}
	}

	// Generate the cube sphere mesh
	GenerateCubeSphere();

	// Generate water sphere
	GenerateWaterSphere();

	// Apply water material if assigned
	if (WaterMaterial)
	{
		WaterSphereMesh->SetMaterial(0, WaterMaterial);
	}

	if (SkyboxMaterial && SkyboxMesh)
	{
		SkyboxMesh->SetMaterial(0, SkyboxMaterial);
		UE_LOG(LogTemp, Log, TEXT("Skybox material applied"));
	}

	// Set translucent sort priority so water renders on top of planet
	PlanetMesh->SetTranslucentSortPriority(0);
	WaterSphereMesh->SetTranslucentSortPriority(1);

	// Always spawn cities on startup
	SpawnCities();

	// Generate navigation graph for vehicle pathfinding (must be after cities are spawned)
	GenerateNavGraph();

	// -----------------------------------------------------------------
	// Seed initialisation: load from save on subsequent runs, or
	// generate + save on first run.  Must happen before any RNG use.
	// -----------------------------------------------------------------

	// Ask the GameInstance which slot is active.
	FString SeedSlot = TEXT("PlanetConquest_Save_0"); // safe fallback
	bool bIsLoadingGame  = false;
	bool bCameFromMenu   = false;
	if (UPlanetConquestGameInstance* GI = Cast<UPlanetConquestGameInstance>(GetGameInstance()))
	{
		SeedSlot       = GI->GetActiveSlotName();
		bIsLoadingGame = GI->bLoadingExistingGame;
		bCameFromMenu  = GI->bSlotWasSetByMenu;
	}

	// Decide whether to load an existing seed or generate a fresh one:
	//   - Came from menu "Load Game"           → load
	//   - Direct PIE launch (no menu) + save exists → load (preserves seed between Play sessions)
	//   - Came from menu "New Game"            → generate new seed (intentional overwrite)
	//   - No save exists yet                   → generate new seed
	const bool bSaveExists = UGameplayStatics::DoesSaveGameExist(SeedSlot, 0);
	const bool bShouldLoad = bIsLoadingGame || (!bCameFromMenu && bSaveExists);

	if (bShouldLoad)
	{
		if (UPlanetConquestSaveGame* LoadedSave = Cast<UPlanetConquestSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SeedSlot, 0)))
		{
			if (LoadedSave->bSeedInitialised)
			{
				NoiseSeed = LoadedSave->PlanetSeed;
				UE_LOG(LogTemp, Log, TEXT("PlanetActor: loaded NoiseSeed %d from slot '%s'"),
					NoiseSeed, *SeedSlot);
			}
		}
	}
	else
	{
		// New game — generate a fresh seed and write it to the chosen slot immediately.
		if (NoiseSeed == 0)
		{
			NoiseSeed = FMath::Rand();
		}
		UPlanetConquestSaveGame* NewSave = Cast<UPlanetConquestSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UPlanetConquestSaveGame::StaticClass()));
		if (NewSave)
		{
			NewSave->PlanetSeed       = NoiseSeed;
			NewSave->bSeedInitialised = true;
			NewSave->LastSaved        = FDateTime::Now();
			UGameplayStatics::SaveGameToSlot(NewSave, SeedSlot, 0);
			UE_LOG(LogTemp, Log, TEXT("PlanetActor: new game — saved NoiseSeed %d to slot '%s'"),
				NoiseSeed, *SeedSlot);
		}
	}

	// Resource RNG — seeded independently from NoiseSeed so placement is
	// deterministic but distinct from the continent-placement sequence.
	FRandomStream ResourceRNG(NoiseSeed ^ 0x5F3759DF);

	// Spawn territory resources around each city
	SpawnResourcesAroundPlanet(ResourceRNG);

	// Spawn AI controllers for AI teams
	TArray<EOwnerTeam> AITeams;
	AITeams.Add(EOwnerTeam::AI1);
	AITeams.Add(EOwnerTeam::AI2);
	AITeams.Add(EOwnerTeam::AI3);
	AITeams.Add(EOwnerTeam::AI4);
	AITeams.Add(EOwnerTeam::AI5);
	AITeams.Add(EOwnerTeam::AI6);
	AITeams.Add(EOwnerTeam::AI7);
	AITeams.Add(EOwnerTeam::AI8);
	AITeams.Add(EOwnerTeam::AI9);
	AITeams.Add(EOwnerTeam::AI10);
	AITeams.Add(EOwnerTeam::AI11);
	AITeams.Add(EOwnerTeam::AI12);
	AITeams.Add(EOwnerTeam::AI13);
	AITeams.Add(EOwnerTeam::AI14);
	AITeams.Add(EOwnerTeam::AI15);
	AITeams.Add(EOwnerTeam::AI16);
	AITeams.Add(EOwnerTeam::AI17);
	AITeams.Add(EOwnerTeam::AI18);
	AITeams.Add(EOwnerTeam::AI19);
	
	// Find which AI teams actually have cities
	TSet<EOwnerTeam> TeamsWithCities;
	for (ACityActor* City : SpawnedCities)
	{
		if (City && City->OwnerTeam != EOwnerTeam::Player && City->OwnerTeam != EOwnerTeam::Neutral)
		{
			TeamsWithCities.Add(City->OwnerTeam);
		}
	}
	
	// Count how many AI teams actually get controllers so we can evenly distribute
	// decision offsets across the full interval (no two teams fire at the same time).
	int32 NumAITeams = 0;
	for (EOwnerTeam Team : AITeams)
	{
		if (TeamsWithCities.Contains(Team)) NumAITeams++;
	}

	// Spawn one AI controller for each team that has cities
	int32 TeamSpawnIndex = 0;
	for (EOwnerTeam Team : AITeams)
	{
		if (TeamsWithCities.Contains(Team))
		{
			AAITeamController* AIController = GetWorld()->SpawnActorDeferred<AAITeamController>(
				AAITeamController::StaticClass(),
				FTransform::Identity,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn
			);
			
			if (AIController)
			{
				// Set properties BEFORE BeginPlay runs
				AIController->ControlledTeam = Team;
				// Pre-set offset so BeginPlay logs the correct value before it can overwrite it
				const float Offset = (NumAITeams > 0)
					? (TeamSpawnIndex * (AIController->DecisionInterval / NumAITeams))
					: 0.0f;
				AIController->TimeSinceLastDecision = Offset;
				AIController->FinishSpawning(FTransform::Identity);
				// Restore offset in case BeginPlay reset it (defensive)
				AIController->TimeSinceLastDecision = Offset;
				
				AIControllers.Add(AIController);
				TeamSpawnIndex++;
				
				UE_LOG(LogTemp, Log, TEXT("Spawned AITeamController for AI Team %d (decision offset %.2f s)"),
					(int32)Team, Offset);
			}
		}
	}

	// Generate minimap texture for HUD
	GenerateMinimapTexture(512, 256);
	UE_LOG(LogTemp, Log, TEXT("Minimap texture auto-generated at startup"));

	// -----------------------------------------------------------------
	// World state restore (must run after ALL entities are spawned)
	// -----------------------------------------------------------------
	if (bShouldLoad)
	{
		if (UPlanetConquestSaveGame* WorldSave = Cast<UPlanetConquestSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SeedSlot, 0)))
		{
			if (APlanetConquestGameMode* GM = Cast<APlanetConquestGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GM->ApplyWorldState(WorldSave);
			}
		}
	}
}

void APlanetActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Update day/night cycle by rotating the directional lights
	if (SunLight && FillLight && DayNightCycleSpeed > 0.0f)
	{
		// Increment rotation angle
		CurrentDayNightRotation += DayNightCycleSpeed * DeltaTime;
		
		// Keep angle in 0-360 range
		if (CurrentDayNightRotation >= 360.0f)
		{
			CurrentDayNightRotation -= 360.0f;
		}
		
		// Rotate both lights - they stay 180 degrees apart
		SunLight->SetRelativeRotation(FRotator(-45.0f, CurrentDayNightRotation, 0.0f));
		FillLight->SetRelativeRotation(FRotator(45.0f, CurrentDayNightRotation + 180.0f, 0.0f));
	}
	
	// DEBUG: Draw all navigation nodes every frame
	DebugDrawAllNavNodes();
}

#if WITH_EDITOR
void APlanetActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-regenerate when any planet parameter changes
	if (PropertyChangedEvent.Property != nullptr)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();
		
		// List of properties that should trigger regeneration
		if (PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, Resolution) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, NoiseScale) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, NoiseOctaves) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, NoisePersistence) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, NoiseSeed) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, DetailNoiseStrength) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, PlanetRadius) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, BaseHeight) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, SeedHeight) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, SeaLevel) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, NumVoronoiCells) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, NumLandCells) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, LandSpread) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, ContinentShapeStrength) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, CoastlineDetailStrength) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, HemisphereBias) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, ContinentRadius) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, MinContinentSeparation) ||		PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, CoastlineBlendDistance) ||			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, bShowWaterSphere) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APlanetActor, WaterSphereResolution))
		{
			RegeneratePlanet();
		}
	}
}
#endif

void APlanetActor::RegeneratePlanet()
{
	GenerateCubeSphere();
	GenerateWaterSphere();
	
	if (PlanetMaterial)
	{
		PlanetMesh->SetMaterial(0, PlanetMaterial);
	}

	if (WaterMaterial)
	{
		WaterSphereMesh->SetMaterial(0, WaterMaterial);
	}

	if (SkyboxMaterial && SkyboxMesh)
	{
		SkyboxMesh->SetMaterial(0, SkyboxMaterial);
	}

	// Set translucent sort priority so water renders on top of planet
	PlanetMesh->SetTranslucentSortPriority(0);
	WaterSphereMesh->SetTranslucentSortPriority(1);
	
	UE_LOG(LogTemp, Warning, TEXT("Planet regenerated: Resolution=%d, BaseHeight=%.3f, SeedHeight=%.3f, SeaLevel=%.3f"),
		Resolution, BaseHeight, SeedHeight, SeaLevel);
}

// ===== CUBE SPHERE GENERATION =====

void APlanetActor::GenerateCubeSphere()
{
	// Place continent seeds first (regenerated each time)
	PlaceContinentSeeds();

	// Place volcanoes at continent centers
	StampVolcanoes();

	// Clear any existing mesh
	PlanetMesh->ClearAllMeshSections();

	// Arrays to hold all vertices, triangles, and UVs for the entire sphere
	TArray<FVector> AllVertices;
	TArray<int32> AllTriangles;
	TArray<FVector2D> AllUVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> VertexColors;
	TArray<FVector> BasePositions; // Store base sphere positions for consistent noise sampling

	// Define the 6 cube face directions
	TArray<FVector> Directions = {
		FVector(0, 0, 1),   // +Z (Top)
		FVector(0, 0, -1),  // -Z (Bottom)
		FVector(0, 1, 0),   // +Y (Right)
		FVector(0, -1, 0),  // -Y (Left)
		FVector(1, 0, 0),   // +X (Front)
		FVector(-1, 0, 0)   // -X (Back)
	};

	// Generate each of the 6 faces
	for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
	{
		TArray<FVector> FaceVertices;
		TArray<int32> FaceTriangles;
		TArray<FVector2D> FaceUVs;
		TArray<FVector> FaceBasePositions;

		GenerateFace(Directions[FaceIndex], FaceVertices, FaceTriangles, FaceUVs, FaceBasePositions);

		// Offset triangle indices by current vertex count
		int32 VertexOffset = AllVertices.Num();
		for (int32 TriIndex : FaceTriangles)
		{
			AllTriangles.Add(TriIndex + VertexOffset);
		}

		// Add face data to combined arrays
		AllVertices.Append(FaceVertices);
		AllUVs.Append(FaceUVs);
		BasePositions.Append(FaceBasePositions);
	}

	// Generate normals (pointing outward from sphere center)
	for (const FVector& Vertex : AllVertices)
	{
		Normals.Add(Vertex.GetSafeNormal());
	}

	// Generate vertex colors based on height (binary land/ocean visualization)
	// Alpha channel stores volcano influence (0 = no volcano, 255 = full volcano)
	int32 VolcanoVertexCount = 0; // Debug counter
	for (int32 i = 0; i < BasePositions.Num(); i++)
	{
		FVector UnitSpherePoint = BasePositions[i].GetSafeNormal();
		float HeightValue = CalculateHeightAtPoint(UnitSpherePoint);
		
		// Calculate volcano influence for this vertex (0.0 to 1.0)
		float VolcanoInfluence = 0.0f;
		if (bPlaceVolcanoes && VolcanoPositions.Num() > 0)
		{
			for (const FVector& VolcanoCenter : VolcanoPositions)
			{
				float AngularDist = AngularDistance(UnitSpherePoint, VolcanoCenter);
				
				// Use smaller radius for basalt material (not full volcano radius)
				float BasaltRadius = VolcanoRadius * BasaltRadiusMultiplier;
				float BasaltAngularRadius = BasaltRadius / PlanetRadius;
				
				if (AngularDist < BasaltAngularRadius)
				{
					// Calculate distance from center (normalized 0-1, where 0 = center, 1 = edge)
					float NormalizedDist = AngularDist / BasaltAngularRadius;
					
					// Add Perlin noise for irregular edges
					float NoiseValue = 0.0f;
					if (BasaltNoiseStrength > 0.0f)
					{
						FVector WorldPos = UnitSpherePoint * PlanetRadius;
						float BasaltNoiseScale = 0.0002f; // Scale for noise frequency
						NoiseValue = FMath::PerlinNoise3D(WorldPos * BasaltNoiseScale);
						NoiseValue = NoiseValue * BasaltNoiseStrength; // -0.3 to +0.3 range
					}
					
					// Blend factor: 1.0 at center, fades to 0 at edge
					// Noise shifts the edge boundary inward/outward
					float BlendFactor = 1.0f - NormalizedDist + NoiseValue;
					BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);
					
					// Smoothstep for natural falloff
					BlendFactor = BlendFactor * BlendFactor * (3.0f - 2.0f * BlendFactor);
					
					// Use maximum influence if within multiple volcanoes
					VolcanoInfluence = FMath::Max(VolcanoInfluence, BlendFactor);
					
					if (VolcanoInfluence > 0.0f)
					{
						VolcanoVertexCount++;
					}
				}
			}
		}
		
		// Binary color: ocean = blue, land = green/brown
		FColor VertexColor;
		if (HeightValue < SeaLevel)
		{
			// Below sea level - blue
			VertexColor = FColor(10, 60, 150, 0); // Alpha = 0 for ocean
		}
		else
		{
			// Above sea level - green with brown variation based on detail noise
			uint8 AlphaValue = FMath::Clamp(FMath::RoundToInt(VolcanoInfluence * 255.0f), 0, 255);
			
			if (DetailNoiseStrength > 0.0f)
			{
				// Vary color slightly with height for terrain detail
				uint8 GreenValue = FMath::Clamp(FMath::RoundToInt((0.3f + HeightValue * 0.5f) * 255.0f), 80, 180);
				uint8 BrownValue = FMath::Clamp(FMath::RoundToInt(HeightValue * 200.0f), 60, 140);
				VertexColor = FColor(BrownValue, GreenValue, 30, AlphaValue);
			}
			else
			{
				// Flat land - solid green
				VertexColor = FColor(90, 140, 40, AlphaValue);
			}
		}
		VertexColors.Add(VertexColor);
	}

	// Create the procedural mesh section
	PlanetMesh->CreateMeshSection(0, AllVertices, AllTriangles, Normals, AllUVs, VertexColors, Tangents, true);

	// Apply planet material immediately after mesh creation
	if (PlanetMaterial)
	{
		PlanetMesh->SetMaterial(0, PlanetMaterial);
		UE_LOG(LogTemp, Log, TEXT("Applied PlanetMaterial to mesh"));
	}

	UE_LOG(LogTemp, Warning, TEXT("GenerateCubeSphere: Created sphere with %d vertices, %d triangles, %d faces"), 
		AllVertices.Num(), AllTriangles.Num() / 3, 6);
	UE_LOG(LogTemp, Warning, TEXT("Volcano vertices: %d out of %d total (%.1f%%)"), 
		VolcanoVertexCount, AllVertices.Num(), (float)VolcanoVertexCount / AllVertices.Num() * 100.0f);
}

void APlanetActor::GenerateFace(const FVector& LocalUp, TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector2D>& UVs, TArray<FVector>& BasePositions)
{
	// Calculate two perpendicular axes to LocalUp
	FVector AxisA = FVector(LocalUp.Y, LocalUp.Z, LocalUp.X); // Swizzle to get perpendicular vector
	FVector AxisB = FVector::CrossProduct(LocalUp, AxisA);

	// Generate vertices in a grid
	for (int32 y = 0; y < Resolution; y++)
	{
		for (int32 x = 0; x < Resolution; x++)
		{
			// Calculate position on unit cube face (range -1 to 1)
			FVector2D Percent = FVector2D(x, y) / (Resolution - 1);
			FVector PointOnUnitCube = LocalUp + (Percent.X - 0.5f) * 2.0f * AxisA + (Percent.Y - 0.5f) * 2.0f * AxisB;

			// Project onto sphere by normalizing
			FVector PointOnUnitSphere = PointOnUnitCube.GetSafeNormal();
			
			// Calculate world position first (for noise sampling)
			FVector WorldPosition = PointOnUnitSphere * PlanetRadius;
			
			// Sample height using continent seeding instead of pure noise
			float HeightValue = CalculateHeightAtPoint(PointOnUnitSphere);
			
			// Convert normalized height to world space offset
			// HeightValue is in normalized units (e.g., 0.3 for land, -0.3 for ocean)
			float HeightOffset = HeightValue * PlanetRadius;
			
			// Apply height offset along the normal to create terrain
			FVector PointOnSphere = PointOnUnitSphere * (PlanetRadius + HeightOffset);

			Vertices.Add(PointOnSphere);
			BasePositions.Add(WorldPosition); // Store base position for color sampling

			// Generate UVs (0 to 1 range)
			UVs.Add(Percent);
		}
	}

	// Generate triangles (two triangles per quad)
	for (int32 y = 0; y < Resolution - 1; y++)
	{
		for (int32 x = 0; x < Resolution - 1; x++)
		{
			int32 i = y * Resolution + x;

			// First triangle (counter-clockwise winding)
			Triangles.Add(i);
			Triangles.Add(i + Resolution);
			Triangles.Add(i + Resolution + 1);

			// Second triangle
			Triangles.Add(i);
			Triangles.Add(i + Resolution + 1);
			Triangles.Add(i + 1);
		}
	}
}

// ===== NOISE SAMPLING (Phase 2) =====

float APlanetActor::SampleNoise(const FVector& Point) const
{
	// Simple multi-octave noise using Unreal's FMath::PerlinNoise3D
	float NoiseValue = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = NoiseScale;
	float MaxValue = 0.0f; // For normalization

	for (int32 Octave = 0; Octave < NoiseOctaves; Octave++)
	{
		// Sample 3D Perlin noise
		FVector SamplePoint = Point * Frequency + FVector(NoiseSeed * 100.0f, NoiseSeed * 200.0f, NoiseSeed * 300.0f);
		float Sample = FMath::PerlinNoise3D(SamplePoint);
		
		// PerlinNoise3D returns values roughly in [-1, 1], remap to [0, 1]
		Sample = (Sample + 1.0f) * 0.5f;
		
		NoiseValue += Sample * Amplitude;
		MaxValue += Amplitude;
		
		Amplitude *= NoisePersistence;
		Frequency *= 2.0f;
	}

	// Normalize to 0-1 range
	return NoiseValue / MaxValue;
}

// ===== CONTINENT SEEDING FUNCTIONS =====

void APlanetActor::PlaceContinentSeeds()
{
	ContinentSeeds.Empty();

	FRandomStream RNG(NoiseSeed);

	// Step 1: Place ALL Voronoi cell centers with even distribution
	TArray<FVector> AllCells = PlaceEvenlyDistributed(NumVoronoiCells, RNG);

	// Step 2: Choose which cells are land based on LandSpread
	TArray<int32> LandIndices = SelectLandCells(AllCells, NumLandCells, LandSpread, RNG);

	// Step 3: Build seed list
	for (int32 i = 0; i < AllCells.Num(); i++)
	{
		bool bIsLand = LandIndices.Contains(i);
		float Size = RNG.FRandRange(0.8f, 1.2f); // Random size variation per continent
		float Roughness = FMath::Max(ContinentShapeStrength, CoastlineDetailStrength); // Max of both noise types
		FContinentSeed Seed;
		Seed.Position = AllCells[i];
		Seed.Size = Size;
		Seed.bIsLand = bIsLand;
		Seed.Roughness = Roughness;
		ContinentSeeds.Add(Seed);
	}

	UE_LOG(LogTemp, Log, TEXT("PlaceContinentSeeds: Generated %d cells (%d land, %d ocean)"),
		ContinentSeeds.Num(), NumLandCells, ContinentSeeds.Num() - NumLandCells);
	
	// Log each seed for debugging
	for (int32 i = 0; i < ContinentSeeds.Num(); i++)
	{
		UE_LOG(LogTemp, Log, TEXT("  Seed %d: %s, Size=%.2f, Pos=%s"),
			i, ContinentSeeds[i].bIsLand ? TEXT("LAND") : TEXT("Ocean"), 
			ContinentSeeds[i].Size, *ContinentSeeds[i].Position.ToString());
	}
}

TArray<FVector> APlanetActor::PlaceEvenlyDistributed(int32 NumPoints, FRandomStream& RNG)
{
	TArray<FVector> Points;

	// Fibonacci sphere algorithm for even distribution
	const float GoldenRatio = (1.0f + FMath::Sqrt(5.0f)) / 2.0f;
	const float AngleIncrement = PI * 2.0f * GoldenRatio;

	for (int32 i = 0; i < NumPoints; i++)
	{
		// Apply hemisphere bias
		float t = ((float)i + 0.5f) / (float)NumPoints;
		
		// Bias toward desired hemisphere
		// HemisphereBias = 0.5 means no bias (even distribution)
		// HemisphereBias = 0.0 means all northern hemisphere
		// HemisphereBias = 1.0 means all southern hemisphere
		float BiasedT = t;
		if (HemisphereBias != 0.5f)
		{
			float Bias = (HemisphereBias - 0.5f) * 2.0f; // -1 to 1
			BiasedT = FMath::Pow(t, FMath::Exp(-Bias));
		}

		float Z = 1.0f - 2.0f * BiasedT;
		float Radius = FMath::Sqrt(1.0f - Z * Z);
		float Theta = AngleIncrement * i;

		FVector Point = FVector(
			Radius * FMath::Cos(Theta),
			Radius * FMath::Sin(Theta),
			Z
		).GetSafeNormal();

		Points.Add(Point);
	}

	return Points;
}

TArray<int32> APlanetActor::SelectLandCells(const TArray<FVector>& Cells, int32 NumLand, float Spread, FRandomStream& RNG)
{
	TArray<int32> Selected;
	TArray<int32> Remaining;
	
	for (int32 i = 0; i < Cells.Num(); i++)
	{
		Remaining.Add(i);
	}

	// Clamp NumLand to valid range
	NumLand = FMath::Clamp(NumLand, 1, Cells.Num());

	// Pick first land cell randomly
	int32 First = RNG.RandRange(0, Remaining.Num() - 1);
	Selected.Add(Remaining[First]);
	Remaining.RemoveAt(First);

	// Pick subsequent cells based on spread
	while (Selected.Num() < NumLand && Remaining.Num() > 0)
	{
		// Score each remaining cell
		int32 BestIndex = -1;
		float BestScore = -FLT_MAX;

		for (int32 i = 0; i < Remaining.Num(); i++)
		{
			float MinDistToSelected = FLT_MAX;
			for (int32 SelIdx : Selected)
			{
				float Dist = AngularDistance(Cells[Remaining[i]], Cells[SelIdx]);
				MinDistToSelected = FMath::Min(MinDistToSelected, Dist);
			}

			// Spread=1: prefer cells far from existing land (spread out)
			// Spread=0: prefer cells close to existing land (cluster)
			float RandomScore = RNG.FRand();
			float SpreadScore = MinDistToSelected;

			float Score = FMath::Lerp(
				-SpreadScore,    // Spread=0: low score for far cells
				SpreadScore,     // Spread=1: high score for far cells
				Spread
			) + RandomScore * 0.3f; // Small random factor prevents identical results

			if (Score > BestScore)
			{
				BestScore = Score;
				BestIndex = i;
			}
		}

		Selected.Add(Remaining[BestIndex]);
		Remaining.RemoveAt(BestIndex);
	}

	return Selected;
}

float APlanetActor::CalculateHeightAtPoint(const FVector& SpherePoint) const
{
	if (ContinentSeeds.Num() == 0)
	{
		return BaseHeight; // No continents, return base height
	}

	// === STEP 1: Apply large-scale continent shape deformation ===
	// Domain warp disabled - causes lakes by warping ocean cells into land regions
	FVector ShapeWarpedPoint = SpherePoint;

	// === STEP 2: Find nearest and second-nearest seeds (using shape-warped point) ===
	float NearestDist = FLT_MAX;
	float SecondDist = FLT_MAX;
	const FContinentSeed* NearestSeed = nullptr;

	for (const FContinentSeed& Seed : ContinentSeeds)
	{
		float Dist = AngularDistance(ShapeWarpedPoint, Seed.Position);
		if (Dist < NearestDist)
		{
			SecondDist = NearestDist;
			NearestDist = Dist;
			NearestSeed = &Seed;
		}
		else if (Dist < SecondDist)
		{
			SecondDist = Dist;
		}
	}

	if (!NearestSeed || !NearestSeed->bIsLand)
	{
		return BaseHeight; // Ocean cell or no seed
	}

	// === STEP 3: Calculate distance to Voronoi boundary ===
	float DistToBoundary = (SecondDist - NearestDist) / 2.0f;
	float MinSeparationRadians = MinContinentSeparation / PlanetRadius;

	// Note: We DON'T early return here - we want blending even at boundaries
	// The boundary constraint will be enforced in the blending calculation

	// === STEP 4: Apply fine-scale coastline detail ===
	// Domain warp disabled - passes point through unchanged
	FVector DetailWarpedPoint = ShapeWarpedPoint;

	// === STEP 5: Check if within continent radius and apply coastline blending ===
	float FinalDist = AngularDistance(DetailWarpedPoint, NearestSeed->Position);
	float ContRadius = ContinentRadius * NearestSeed->Size;

	// If we're close to a Voronoi boundary, pull the continent radius back to respect MinSeparation
	// The slope will eat into the land, not extend into the boundary zone
	if (DistToBoundary < MinSeparationRadians)
	{
		// Position effective coastline at the boundary edge minus the separation distance
		float MaxAllowedRadius = NearestDist + DistToBoundary - MinSeparationRadians;
		ContRadius = FMath::Min(ContRadius, FMath::Max(0.0f, MaxAllowedRadius));
	}

	// Calculate distance from coastline
	float DistFromCoastline = ContRadius - FinalDist;

	// Determine base heights
	float LandHeight = BaseHeight + SeedHeight;
	float OceanHeight = BaseHeight;

	// Apply blending near coastlines
	// Asymmetric blend: starts farther inland, extends farther into ocean for shallow slope
	float LandBlendStart = CoastlineBlendDistance * 1.5f;  // Start slope farther inland
	float OceanBlendEnd = CoastlineBlendDistance * 4.5f;   // Extend slope farther into ocean (3x shallower)
	float FinalHeight;
	
	if (DistFromCoastline > LandBlendStart)
	{
		// Deep inland - full land height
		FinalHeight = LandHeight;
	}
	else if (DistFromCoastline < -OceanBlendEnd)
	{
		// Deep ocean - full ocean height
		FinalHeight = OceanHeight;
	}
	else
	{
		// Transition zone - blend between land and ocean
		// Map distance to 0-1 range (0 = ocean, 1 = land)
		float TotalBlendWidth = LandBlendStart + OceanBlendEnd;
		float BlendFactor = (DistFromCoastline + OceanBlendEnd) / TotalBlendWidth;
		BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);
		
		// Apply smoothstep for natural falloff
		BlendFactor = BlendFactor * BlendFactor * (3.0f - 2.0f * BlendFactor);
		
		// Interpolate between ocean and land height
		FinalHeight = FMath::Lerp(OceanHeight, LandHeight, BlendFactor);
	}

	// Detail noise disabled - FBM noise can create sub-sea-level depressions that appear as lakes
	// Re-enable once the lake-prevention clamp is confirmed sufficient
	if (false && DetailNoiseStrength > 0.0f && DistFromCoastline > -OceanBlendEnd)
	{
		FVector WorldPoint = SpherePoint * 100000.0f;
		float DetailNoise = SampleNoise(WorldPoint);
		// Remap from 0-1 to -0.5 to 0.5 for variation
		DetailNoise = (DetailNoise - 0.5f);

		// Scale detail noise by distance from coastline (less noise near water)
		float DetailNoiseScale = FMath::Clamp((DistFromCoastline + OceanBlendEnd) / OceanBlendEnd, 0.0f, 1.0f);
		FinalHeight += DetailNoise * DetailNoiseStrength * DetailNoiseScale;
	}

	// ===== ADD VOLCANO HEIGHT (if applicable) =====
	if (bPlaceVolcanoes && VolcanoHeightmap && VolcanoPositions.Num() > 0)
	{
		// Check if point is within any volcano radius
		for (const FVector& VolcanoCenter : VolcanoPositions)
		{
			// Calculate angular distance from volcano center
			float AngularDist = AngularDistance(SpherePoint, VolcanoCenter);
			float VolcanoAngularRadius = VolcanoRadius / PlanetRadius;

			if (AngularDist < VolcanoAngularRadius)
			{
				// Point is within volcano radius - calculate local UV coordinates
				// Create local coordinate system at volcano center
				FVector VolcanoUp = VolcanoCenter;
				FVector Tangent = FVector::CrossProduct(VolcanoUp, FVector::UpVector).GetSafeNormal();
				if (Tangent.IsNearlyZero())
				{
					Tangent = FVector::CrossProduct(VolcanoUp, FVector::RightVector).GetSafeNormal();
				}
				FVector Bitangent = FVector::CrossProduct(VolcanoUp, Tangent).GetSafeNormal();

				// Project point onto local plane to get offset
				FVector ToPoint = SpherePoint - VolcanoCenter;
				float LocalX = FVector::DotProduct(ToPoint, Tangent);
				float LocalY = FVector::DotProduct(ToPoint, Bitangent);

				// Convert to UV coordinates (0-1 range)
				// Map from [-VolcanoRadius, +VolcanoRadius] to [0, 1]
				float WorldRadius = VolcanoRadius;
				float U = (LocalX * PlanetRadius / WorldRadius + 1.0f) * 0.5f;
				float V = (LocalY * PlanetRadius / WorldRadius + 1.0f) * 0.5f;

				// Sample the volcano heightmap
				float VolcanoSample = SampleVolcanoHeight(U, V);

				// Apply edge falloff
				float DistFromCenter = AngularDist * PlanetRadius;
				float DistFromEdge = VolcanoRadius - DistFromCenter;

				float BlendFactor = 1.0f;
				if (DistFromEdge < VolcanoEdgeFalloff)
				{
					// Fade out near edges
					BlendFactor = DistFromEdge / VolcanoEdgeFalloff;
					BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);
					// Smoothstep for smoother blending
					BlendFactor = BlendFactor * BlendFactor * (3.0f - 2.0f * BlendFactor);
				}

				// Add volcano height (target: 3500 units = 3500/80000 = 0.04375 in normalized space)
				float VolcanoHeightNormalized = (3500.0f / PlanetRadius) * VolcanoHeightMultiplier;
				float VolcanoContribution = VolcanoSample * VolcanoHeightNormalized * BlendFactor;
				FinalHeight += VolcanoContribution;

				// Only apply one volcano per point (use first match)
				break;
			}
		}
	}

	// Clamp: if Voronoi geometry says this is land (DistFromCoastline > 0),
	// never let FBM detail noise push the height below sea level.
	// This prevents interior lakes entirely without changing coastline shape.
	if (DistFromCoastline > 0.0f)
	{
		float MinLandHeight = BaseHeight + SeaLevel + 0.001f;
		FinalHeight = FMath::Max(FinalHeight, MinLandHeight);
	}

	return FinalHeight;
}

float APlanetActor::AngularDistance(const FVector& A, const FVector& B) const
{
	// Both vectors should be on unit sphere (normalized)
	FVector NormA = A.GetSafeNormal();
	FVector NormB = B.GetSafeNormal();

	// Angular distance = arccos(dot product)
	float DotProduct = FVector::DotProduct(NormA, NormB);
	DotProduct = FMath::Clamp(DotProduct, -1.0f, 1.0f); // Clamp for numerical stability
	
	return FMath::Acos(DotProduct);
}

FVector APlanetActor::DomainWarp(const FVector& Point, float WarpStrength, float WarpScale, int32 Seed) const
{
	if (WarpStrength <= 0.0f)
	{
		return Point;
	}

	// Three orthogonal noise samples for X, Y, Z warp
	// WarpScale controls the frequency of the noise (smaller = larger features)
	FVector SamplePoint = Point * WarpScale;
	
	float WarpX = FMath::PerlinNoise3D(SamplePoint + FVector(1.7f, 3.2f, Seed));
	float WarpY = FMath::PerlinNoise3D(SamplePoint + FVector(8.1f, 0.4f, Seed + 1));
	float WarpZ = FMath::PerlinNoise3D(SamplePoint + FVector(4.3f, 6.7f, Seed + 2));

	// Perlin returns roughly -1 to 1, scale by warp strength
	FVector WarpOffset = FVector(WarpX, WarpY, WarpZ) * WarpStrength;

	// Apply warp and renormalize to sphere
	return (Point + WarpOffset).GetSafeNormal();
}

// ===== WATER SPHERE GENERATION =====

void APlanetActor::GenerateWaterSphere()
{
	if (!bShowWaterSphere)
	{
		WaterSphereMesh->ClearAllMeshSections();
		return;
	}

	WaterSphereMesh->ClearAllMeshSections();

	TArray<FVector> AllVertices;
	TArray<int32> AllTriangles;
	TArray<FVector2D> AllUVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> VertexColors;

	// Calculate water radius (sea level).
	// Render 50 units below logical sea level to avoid z-fighting with coastal terrain,
	// while keeping IsPointOnLand / ship / city logic at the unmodified SeaLevel value.
	float WaterRadius = PlanetRadius * (1.0f + SeaLevel) + 10.0f;

	// Define the 6 cube face directions (same as planet)
	TArray<FVector> Directions = {
		FVector(0, 0, 1),   // +Z (Top)
		FVector(0, 0, -1),  // -Z (Bottom)
		FVector(0, 1, 0),   // +Y (Right)
		FVector(0, -1, 0),  // -Y (Left)
		FVector(1, 0, 0),   // +X (Front)
		FVector(-1, 0, 0)   // -X (Back)
	};

	int32 WaterRes = WaterSphereResolution;

	// Generate each of the 6 faces
	for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
	{
		FVector LocalUp = Directions[FaceIndex];
		FVector AxisA = FVector(LocalUp.Y, LocalUp.Z, LocalUp.X);
		FVector AxisB = FVector::CrossProduct(LocalUp, AxisA);

		TArray<FVector> FaceVertices;
		TArray<int32> FaceTriangles;
		TArray<FVector2D> FaceUVs;

		// Generate vertices in a grid
		for (int32 y = 0; y < WaterRes; y++)
		{
			for (int32 x = 0; x < WaterRes; x++)
			{
				// Calculate position on unit cube face (range -1 to 1)
				FVector2D Percent = FVector2D(x, y) / (WaterRes - 1);
				FVector PointOnUnitCube = LocalUp + (Percent.X - 0.5f) * 2.0f * AxisA + (Percent.Y - 0.5f) * 2.0f * AxisB;

				// Project onto sphere by normalizing
				FVector PointOnUnitSphere = PointOnUnitCube.GetSafeNormal();

				// Scale to water radius (no height offset for water)
				FVector PointOnWaterSphere = PointOnUnitSphere * WaterRadius;

				FaceVertices.Add(PointOnWaterSphere);
				FaceUVs.Add(Percent);
			}
		}

		// Generate triangles (two triangles per quad)
		for (int32 y = 0; y < WaterRes - 1; y++)
		{
			for (int32 x = 0; x < WaterRes - 1; x++)
			{
				int32 i = y * WaterRes + x;

				// First triangle (counter-clockwise winding)
				FaceTriangles.Add(i);
				FaceTriangles.Add(i + WaterRes);
				FaceTriangles.Add(i + WaterRes + 1);

				// Second triangle
				FaceTriangles.Add(i);
				FaceTriangles.Add(i + WaterRes + 1);
				FaceTriangles.Add(i + 1);
			}
		}

		// Offset triangle indices by current vertex count
		int32 VertexOffset = AllVertices.Num();
		for (int32 TriIndex : FaceTriangles)
		{
			AllTriangles.Add(TriIndex + VertexOffset);
		}

		// Add face data to combined arrays
		AllVertices.Append(FaceVertices);
		AllUVs.Append(FaceUVs);
	}

	// Generate normals (pointing outward from sphere center)
	for (const FVector& Vertex : AllVertices)
	{
		Normals.Add(Vertex.GetSafeNormal());
	}

	// Generate vertex colors (opaque blue)
	for (int32 i = 0; i < AllVertices.Num(); i++)
	{
		VertexColors.Add(FColor(30, 100, 200, 255)); // Opaque blue
	}

	// Create mesh section
	WaterSphereMesh->CreateMeshSection(0, AllVertices, AllTriangles, Normals, AllUVs, VertexColors, Tangents, true);

	// Apply water material if available
	if (WaterMaterial)
	{
		WaterSphereMesh->SetMaterial(0, WaterMaterial);
	}

	// Ensure water mesh renders after planet mesh
	WaterSphereMesh->SetCastShadow(false); // Water shouldn't cast shadows
	WaterSphereMesh->bRenderCustomDepth = false;

	UE_LOG(LogTemp, Log, TEXT("GenerateWaterSphere: Created 6-face cube water sphere at radius %.1f with %d vertices"),
		WaterRadius, AllVertices.Num());
}

// ===== CITY PLACEMENT HELPERS =====

bool APlanetActor::IsPointOnLand(const FVector& SpherePoint) const
{
	// Calculate height at this point
	float Height = CalculateHeightAtPoint(SpherePoint);
	
	// Land is any point whose terrain height is above sea level.
	bool bIsLand = Height > BaseHeight + SeaLevel;
	
	return bIsLand;
}

bool APlanetActor::IsWaterPointOcean(const FVector& SpherePoint) const
{
	if (ContinentSeeds.Num() == 0)
	{
		return true; // Default to ocean if no seeds
	}

	// Find the nearest continent seed (land or ocean)
	int32 NearestSeedId = -1;
	float NearestDist = FLT_MAX;

	for (int32 i = 0; i < ContinentSeeds.Num(); i++)
	{
		float Dist = AngularDistance(SpherePoint, ContinentSeeds[i].Position);
		if (Dist < NearestDist)
		{
			NearestDist = Dist;
			NearestSeedId = i;
		}
	}

	// If nearest seed is an ocean cell, this is ocean
	// If nearest seed is a land cell, this is a lake within that continent
	return (NearestSeedId >= 0 && !ContinentSeeds[NearestSeedId].bIsLand);
}

int32 APlanetActor::GetContinentIdForPoint(const FVector& SpherePoint) const
{
	if (ContinentSeeds.Num() == 0)
	{
		return -1;
	}

	// Find nearest land seed
	int32 NearestLandSeed = -1;
	float NearestLandDist = FLT_MAX;

	for (int32 i = 0; i < ContinentSeeds.Num(); i++)
	{
		if (ContinentSeeds[i].bIsLand)
		{
			float Dist = AngularDistance(SpherePoint, ContinentSeeds[i].Position);
			if (Dist < NearestLandDist)
			{
				NearestLandDist = Dist;
				NearestLandSeed = i;
			}
		}
	}

	// Check if point is actually on land at this location
	if (NearestLandSeed >= 0 && IsPointOnLand(SpherePoint))
	{
		return NearestLandSeed;
	}

	return -1; // Not on land
}

void APlanetActor::GenerateCoastalCandidates(int32 TargetContinentId, TArray<FVector>& OutCandidates)
{
	OutCandidates.Empty();
	
	if (TargetContinentId < 0 || TargetContinentId >= ContinentSeeds.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateCoastalCandidates: Invalid continent ID %d"), TargetContinentId);
		return;
	}
	
	const FVector ContinentCenter = ContinentSeeds[TargetContinentId].Position;
	const float ContinentSize = ContinentSeeds[TargetContinentId].Size; // 0.8 - 1.2
	const float InsetDistance = CityRadius - 2000; // walk inland by city radius from the coastline

	// Estimate continent angular radius based on Voronoi cell distribution
	// With N cells evenly distributed, each cell has approximate angular radius: sqrt(4π/N)
	// However, continents don't fill entire cells (oceans between them), so use conservative multiplier
	float EstimatedAngularRadius = FMath::Sqrt(4.0f * PI / NumVoronoiCells);
	EstimatedAngularRadius *= ContinentSize * 0.6f; // Conservative: 60% of theoretical (targets ~45-50 deg)
	
	// Start sampling 20% beyond the estimated continent edge
	float SamplingAngularRadius = EstimatedAngularRadius * 1.2f;
	
	UE_LOG(LogTemp, Log, TEXT("GenerateCoastalCandidates: Continent size=%.2f, estimated angular radius=%.3f rad (%.1f deg)"),
		ContinentSize, EstimatedAngularRadius, FMath::RadiansToDegrees(EstimatedAngularRadius));
	UE_LOG(LogTemp, Log, TEXT("  Sampling at %.3f rad (%.1f deg) from center"), 
		SamplingAngularRadius, FMath::RadiansToDegrees(SamplingAngularRadius));
	
	// ===== STEP 1: Sample points in a ring around the continent (starting from outside) =====
	
	const int32 NumSampleRays = 1440; // Every 0.25 degrees for very dense coverage
	int32 OceanPointsFound = 0;
	int32 CoastlinesFound = 0;
	int32 ValidCandidates = 0;
	int32 FailedContinentCheck = 0;
	int32 FailedTerritoryCheck = 0;
	
	for (int32 RayIndex = 0; RayIndex < NumSampleRays; RayIndex++)
	{
		float Angle = RayIndex * (2.0f * PI / NumSampleRays);
		
		// Create tangent basis at continent center
		FVector Tangent = FVector::CrossProduct(ContinentCenter, FVector::UpVector).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::CrossProduct(ContinentCenter, FVector::RightVector).GetSafeNormal();
		}
		FVector Bitangent = FVector::CrossProduct(ContinentCenter, Tangent).GetSafeNormal();
		FVector RayDir = (Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle)).GetSafeNormal();
		
		// Rotate from continent center outward by SamplingAngularRadius
		FVector RotationAxis = FVector::CrossProduct(ContinentCenter, RayDir).GetSafeNormal();
		FQuat OutwardRotation = FQuat(RotationAxis, SamplingAngularRadius);
		FVector StartPoint = OutwardRotation.RotateVector(ContinentCenter);
		StartPoint.Normalize();
		
		// Skip only if starting point is on our target continent
		// Accept ocean, other continents, anything that's not our target
		bool bStartOnLand = IsPointOnLand(StartPoint);
		bool bOnTargetContinent = bStartOnLand && (GetContinentIdForPoint(StartPoint) == TargetContinentId);
		
		if (bOnTargetContinent)
		{
			// Starting point is on our continent - skip this ray
			continue;
		}
		
		OceanPointsFound++;
		
		// Determine start type for visualization
		bool bStartInOcean = !bStartOnLand;
		bool bStartOnDifferentContinent = bStartOnLand;
		
		// ===== STEP 2: Cast inward toward continent center until we hit land =====
		
		FVector CoastPoint = FVector::ZeroVector;
		bool bFoundCoast = false;
		const float InwardAngularStep = 0.005f; // ~400 units per step
		
		for (float InwardDist = InwardAngularStep; InwardDist < SamplingAngularRadius; InwardDist += InwardAngularStep)
		{
			float CurrentAngularDist = SamplingAngularRadius - InwardDist;
			FQuat InwardRotation = FQuat(RotationAxis, CurrentAngularDist);
			FVector TestPoint = InwardRotation.RotateVector(ContinentCenter);
			TestPoint.Normalize();
			
			if (IsPointOnLand(TestPoint))
			{
				if (GetContinentIdForPoint(TestPoint) == TargetContinentId)
				{
					CoastPoint = TestPoint;
					bFoundCoast = true;
				}
				break; // Either found coast or hit wrong continent — stop scanning
			}
		}
		
		if (!bFoundCoast)
		{
			continue;
		}
		
		CoastlinesFound++;
		
		// ===== STEP 3: Move 5000 units (InsetDistance) toward continent center =====
		
		FVector InwardDir = (ContinentCenter - CoastPoint).GetSafeNormal();
		float InsetAngularDist = InsetDistance / PlanetRadius;
		FVector InsetRotationAxis = FVector::CrossProduct(CoastPoint, InwardDir).GetSafeNormal();
		FQuat InsetRotation = FQuat(InsetRotationAxis, InsetAngularDist);
		FVector CandidatePoint = InsetRotation.RotateVector(CoastPoint);
		CandidatePoint.Normalize();
		
		// Verify candidate is still on target continent
		if (GetContinentIdForPoint(CandidatePoint) != TargetContinentId)
		{
			FailedContinentCheck++;
			continue;
		}
		
		// ===== STEP 4: Check no terrain within 3000 units is water =====
		
		bool bValidTerritory = true;
		const int32 NumChecks = 12;
		float TerritoryAngularRadius = 3000.0f / PlanetRadius;
		
		for (int32 Check = 0; Check < NumChecks; Check++)
		{
			float CheckAngle = Check * (2.0f * PI / NumChecks);
			
			FVector CheckTangent = FVector::CrossProduct(CandidatePoint, FVector::UpVector).GetSafeNormal();
			if (CheckTangent.IsNearlyZero())
			{
				CheckTangent = FVector::CrossProduct(CandidatePoint, FVector::RightVector).GetSafeNormal();
			}
			FVector CheckBitangent = FVector::CrossProduct(CandidatePoint, CheckTangent).GetSafeNormal();
			FVector CheckDir = (CheckTangent * FMath::Cos(CheckAngle) + CheckBitangent * FMath::Sin(CheckAngle)).GetSafeNormal();
			
			FVector CheckRotationAxis = FVector::CrossProduct(CandidatePoint, CheckDir).GetSafeNormal();
			FQuat CheckRotation = FQuat(CheckRotationAxis, TerritoryAngularRadius);
			FVector CheckPoint = CheckRotation.RotateVector(CandidatePoint);
			CheckPoint.Normalize();
			
			if (!IsPointOnLand(CheckPoint))
			{
				bValidTerritory = false;
				break;
			}
		}
		
		if (bValidTerritory)
		{
			OutCandidates.Add(CandidatePoint);
			ValidCandidates++;
		}
		else
		{
			FailedTerritoryCheck++;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("GenerateCoastalCandidates: Processed %d sample rays"), NumSampleRays);
	UE_LOG(LogTemp, Log, TEXT("  - Ocean/other continent points found: %d"), OceanPointsFound);
	UE_LOG(LogTemp, Log, TEXT("  - Coastlines found (inward cast hit target continent): %d"), CoastlinesFound);
	UE_LOG(LogTemp, Log, TEXT("  - Failed continent check (after inset): %d"), FailedContinentCheck);
	UE_LOG(LogTemp, Log, TEXT("  - Failed territory check: %d"), FailedTerritoryCheck);
	UE_LOG(LogTemp, Log, TEXT("  - Valid candidates: %d"), ValidCandidates);
}

void APlanetActor::SpawnCities()
{
	UE_LOG(LogTemp, Log, TEXT("========== SpawnCities START =========="));
	UE_LOG(LogTemp, Log, TEXT("Configuration: NumCities=%d, SameContinent=%s, MinDistance=%.1f, MaxAttempts=%d"),
		NumCitiesToSpawn, bPlaceOnSameContinent ? TEXT("true") : TEXT("false"), MinCityDistance, MaxCitySpawnAttempts);
	UE_LOG(LogTemp, Log, TEXT("Planet: Radius=%.1f, BaseHeight=%.3f, SeedHeight=%.3f, SeaLevel=%.3f"),
		PlanetRadius, BaseHeight, SeedHeight, SeaLevel);
	UE_LOG(LogTemp, Log, TEXT("Continents: %d total seeds, %d land cells"), ContinentSeeds.Num(), NumLandCells);

	if (NumCitiesToSpawn <= 0 || NumLandCells <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnCities: No cities to spawn or no land available"));
		return;
	}

	// Clear any existing cities
	for (ACityActor* City : SpawnedCities)
	{
		if (City)
		{
			City->Destroy();
		}
	}
	SpawnedCities.Empty();

	// Find the two largest continents:
	//   Continent 1 (largest)  -> cities 0-9  (player + AI1-AI9)
	//   Continent 2 (2nd largest) -> cities 10-19 (AI10-AI19)
	int32 Continent1Id = -1;
	int32 Continent2Id = -1;
	if (bPlaceOnSameContinent)
	{
		float LargestSize = 0.0f;
		float SecondSize  = 0.0f;
		for (int32 i = 0; i < ContinentSeeds.Num(); i++)
		{
			if (ContinentSeeds[i].bIsLand)
			{
				if (ContinentSeeds[i].Size > LargestSize)
				{
					SecondSize  = LargestSize;
					Continent2Id = Continent1Id;
					LargestSize = ContinentSeeds[i].Size;
					Continent1Id = i;
				}
				else if (ContinentSeeds[i].Size > SecondSize)
				{
					SecondSize  = ContinentSeeds[i].Size;
					Continent2Id = i;
				}
			}
		}

		if (Continent1Id < 0)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnCities: No land continents found!"));
			return;
		}
		if (Continent2Id < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnCities: Only one continent found, using it for both halves"));
			Continent2Id = Continent1Id;
		}

		UE_LOG(LogTemp, Log, TEXT("SpawnCities: Continent1=%d (size %.2f), Continent2=%d (size %.2f)"),
			Continent1Id, ContinentSeeds[Continent1Id].Size,
			Continent2Id, ContinentSeeds[Continent2Id].Size);
	}

	// Pre-generate coastal spawn candidates for both continents
	// Continent 1 serves cities 0-4 (player + AI1-AI4); Continent 2 serves cities 10-14 (AI10-AI14)
	TArray<FVector> CoastalCandidates1;
	TArray<FVector> CoastalCandidates2;
	if (NumCitiesToSpawn > 0 && Continent1Id >= 0)
	{
		GenerateCoastalCandidates(Continent1Id, CoastalCandidates1);
	}
	if (NumCitiesToSpawn > 0 && Continent2Id >= 0)
	{
		GenerateCoastalCandidates(Continent2Id, CoastalCandidates2);
	}

	// Track spawned locations for distance checking
	TArray<FVector> SpawnedLocations;
	FRandomStream RNG2(NoiseSeed + 888); // Separate RNG for city placement

	// Spawn cities one by one
	for (int32 CityIndex = 0; CityIndex < NumCitiesToSpawn; CityIndex++)
	{
		bool bValidLocationFound = false;
		FVector CitySpawnLocation;
		FVector SpawnDirection = FVector::UpVector; // Initialize to avoid compiler warning
		float ActualTerrainRadius = PlanetRadius; // Will store the actual terrain height at spawn point

		// Debug tracking
		int32 WrongContinentCount = 0;
		int32 NotLandCount = 0;
		int32 TooCloseCount = 0;

		// Try to find valid spawn location
		for (int32 Attempt = 0; Attempt < MaxCitySpawnAttempts; Attempt++)
		{
			// Determine if this should be a coastal or landlocked city
			// Continent 1 (0-9):  coastal=0-4, landlocked=5-9
			// Continent 2 (10-19): coastal=10-14, landlocked=15-19
			bool bRequireCoastal    = (CityIndex <= 4) || (CityIndex >= 10 && CityIndex <= 14);
			bool bRequireLandlocked = (CityIndex >= 5 && CityIndex <= 9) || CityIndex >= 15;
			int32 CurrentContinentId = (CityIndex < 10) ? Continent1Id : Continent2Id;
			TArray<FVector>& CurrentCoastalCandidates = (CityIndex < 10) ? CoastalCandidates1 : CoastalCandidates2;
			int32 UsedCandidateIndex = -1; // Track which coastal candidate was used

			// For coastal cities, use pre-generated candidates
			if (bRequireCoastal)
			{
				// Regenerate if running low on candidates (likely all too close to existing cities)
				if (CurrentCoastalCandidates.Num() < 3 && Attempt > 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("SpawnCities: Regenerating coastal candidates (had %d left, attempt %d)"), 
						CurrentCoastalCandidates.Num(), Attempt);
					CurrentCoastalCandidates.Empty();
					GenerateCoastalCandidates(CurrentContinentId, CurrentCoastalCandidates);
				}
				
				if (CurrentCoastalCandidates.Num() == 0)
				{
					UE_LOG(LogTemp, Error, TEXT("SpawnCities: No coastal candidates available for city %d"), CityIndex);
					break; // Can't spawn this city
				}
				
				// Pick random coastal candidate
				UsedCandidateIndex = RNG2.RandRange(0, CurrentCoastalCandidates.Num() - 1);
				SpawnDirection = CurrentCoastalCandidates[UsedCandidateIndex];
			}
			else
			{
				// For landlocked cities, sample near the target continent center
				if (bPlaceOnSameContinent)
				{
					// Sample within angular radius of continent center
					const float MaxAngularOffset = 0.7f; // ~40 degrees from center
					float RandomAngle = RNG2.FRandRange(0.0f, 2.0f * PI);
					float RandomDist = RNG2.FRandRange(0.0f, MaxAngularOffset);
					
					// Create perpendicular basis vectors at continent center
					FVector ContinentDir = ContinentSeeds[CurrentContinentId].Position;
					FVector Tangent = FVector::CrossProduct(ContinentDir, FVector::UpVector).GetSafeNormal();
					if (Tangent.IsNearlyZero())
					{
						Tangent = FVector::CrossProduct(ContinentDir, FVector::RightVector).GetSafeNormal();
					}
					FVector Bitangent = FVector::CrossProduct(ContinentDir, Tangent).GetSafeNormal();
					
					// Create random direction within the cone
					FVector RandomDir = (Tangent * FMath::Cos(RandomAngle) + Bitangent * FMath::Sin(RandomAngle)).GetSafeNormal();
					FVector RotationAxis = FVector::CrossProduct(ContinentDir, RandomDir).GetSafeNormal();
					FQuat Rotation = FQuat(RotationAxis, RandomDist);
					SpawnDirection = Rotation.RotateVector(ContinentDir);
					SpawnDirection.Normalize();
					
					// Verify it's on target continent
					int32 ContinentId = GetContinentIdForPoint(SpawnDirection);
					if (ContinentId != CurrentContinentId)
					{
						WrongContinentCount++;
						continue;
					}
				}
				else
				{
					// Random point on entire sphere (original logic for multi-continent)
					float Lon = RNG2.FRandRange(-PI, PI);
					float LatSin = RNG2.FRandRange(-1.0f, 1.0f);
					float LatCos = FMath::Sqrt(FMath::Max(0.0f, 1.0f - LatSin * LatSin));
					SpawnDirection = FVector(LatCos * FMath::Cos(Lon), LatCos * FMath::Sin(Lon), LatSin);
					SpawnDirection.Normalize();
					
					if (!IsPointOnLand(SpawnDirection))
					{
						NotLandCount++;
						continue;
					}
				}
			}

			// For coastal cities, validation is already done - skip to distance check
			// For landlocked cities, need to validate territory and check ocean distance
			bool bAllPointsOnLand = true;
			
			if (!bRequireCoastal)
			{
				// Check that all points within city radius are on land
				const int32 NumSamplePoints = 12;
				for (int32 SampleIndex = 0; SampleIndex < NumSamplePoints; SampleIndex++)
				{
					float AngleDegrees = SampleIndex * (360.0f / NumSamplePoints);
					float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
					
					FVector Tangent = FVector::CrossProduct(SpawnDirection, FVector::UpVector).GetSafeNormal();
					if (Tangent.IsNearlyZero())
					{
						Tangent = FVector::CrossProduct(SpawnDirection, FVector::RightVector).GetSafeNormal();
					}
					FVector Bitangent = FVector::CrossProduct(SpawnDirection, Tangent).GetSafeNormal();
					
					FVector Offset = (Tangent * FMath::Cos(AngleRadians) + Bitangent * FMath::Sin(AngleRadians)) * CityRadius;
					FVector SampleDirection = (SpawnDirection * PlanetRadius + Offset).GetSafeNormal();
					
					bool bSampleOnLand = bPlaceOnSameContinent ? 
					(GetContinentIdForPoint(SampleDirection) == CurrentContinentId) :
					IsPointOnLand(SampleDirection);
					
					if (!bSampleOnLand)
					{
						bAllPointsOnLand = false;
						break;
					}
				}
				
				if (!bAllPointsOnLand)
				{
					NotLandCount++;
					continue;
				}
			}

			// For landlocked cities, ensure no ocean within larger radius
			if (bRequireLandlocked)
			{
				bool bOceanTooClose = false;
				const int32 NumExtendedSamples = 16;
				float LandlockedRadius = CityRadius * 2.0f; // Check 2x the city radius
				
				for (int32 SampleIndex = 0; SampleIndex < NumExtendedSamples; SampleIndex++)
				{
					float AngleDegrees = SampleIndex * (360.0f / NumExtendedSamples);
					float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
					
					FVector Tangent = FVector::CrossProduct(SpawnDirection, FVector::UpVector).GetSafeNormal();
					if (Tangent.IsNearlyZero())
					{
						Tangent = FVector::CrossProduct(SpawnDirection, FVector::RightVector).GetSafeNormal();
					}
					FVector Bitangent = FVector::CrossProduct(SpawnDirection, Tangent).GetSafeNormal();
					
					FVector Offset = (Tangent * FMath::Cos(AngleRadians) + Bitangent * FMath::Sin(AngleRadians)) * LandlockedRadius;
					FVector SampleDirection = (SpawnDirection * PlanetRadius + Offset).GetSafeNormal();
					
					// Check if this point is water AND ocean (lakes are okay)
					if (!IsPointOnLand(SampleDirection) && IsWaterPointOcean(SampleDirection))
					{
						bOceanTooClose = true;
						break;
					}
				}
				
				if (bOceanTooClose)
				{
					NotLandCount++; // Use same counter
					continue; // Ocean too close for landlocked city
				}
			}

			// Calculate world position at actual terrain height
			float TerrainHeight = CalculateHeightAtPoint(SpawnDirection);
			ActualTerrainRadius = PlanetRadius * (1.0f + TerrainHeight);
			CitySpawnLocation = SpawnDirection * ActualTerrainRadius;

			// Check distance to all existing cities
			bool bTooClose = false;
			for (const FVector& ExistingLocation : SpawnedLocations)
			{
				float Distance = FVector::Dist(CitySpawnLocation, ExistingLocation);
				if (Distance < MinCityDistance)
				{
					bTooClose = true;
					break;
				}
			}

			// Check distance from volcanoes (must be at least VolcanoRadius + MinDistanceFromVolcano away)
			if (!bTooClose && bPlaceVolcanoes && VolcanoPositions.Num() > 0)
			{
				float MinVolcanoDistance = VolcanoRadius + MinDistanceFromVolcano;
				
				for (const FVector& VolcanoCenter : VolcanoPositions)
				{
					// Calculate 3D distance from city to volcano center on sphere
					float AngularDist = AngularDistance(SpawnDirection, VolcanoCenter);
					float WorldDist = AngularDist * PlanetRadius;
					
					if (WorldDist < MinVolcanoDistance)
					{
						bTooClose = true;
						break;
					}
				}
			}

			if (bTooClose)
			{
				TooCloseCount++;
				
				// Remove this coastal candidate since it's too close to existing cities
				if (bRequireCoastal && UsedCandidateIndex >= 0)
				{
					CurrentCoastalCandidates.RemoveAt(UsedCandidateIndex);
				}
				
				continue;
			}

			// Success!
			bValidLocationFound = true;
			const TCHAR* CityType = bRequireCoastal ? TEXT("COASTAL") : TEXT("LANDLOCKED");
			UE_LOG(LogTemp, Log, TEXT("SpawnCities: Found valid %s location for city %d on attempt %d (terrain radius=%.1f)"), 
				CityType, CityIndex, Attempt + 1, ActualTerrainRadius);
			
			// Remove used coastal candidate to avoid reuse
			if (bRequireCoastal && UsedCandidateIndex >= 0)
			{
				CurrentCoastalCandidates.RemoveAt(UsedCandidateIndex);
			}
			
			break;
		}

		if (bValidLocationFound)
		{
			// Determine team for this city
			EOwnerTeam CityTeam;
			if (CityIndex == 0)
			{
				CityTeam = EOwnerTeam::Player;
			}
			else
			{
				// Assign to AI teams (AI1-AI19)
				// CityIndex 1=AI1, 2=AI2, ..., 19=AI19
				int32 AIIndex = CityIndex; // 1-19
				switch (AIIndex)
				{
					case 1:  CityTeam = EOwnerTeam::AI1;  break;
					case 2:  CityTeam = EOwnerTeam::AI2;  break;
					case 3:  CityTeam = EOwnerTeam::AI3;  break;
					case 4:  CityTeam = EOwnerTeam::AI4;  break;
					case 5:  CityTeam = EOwnerTeam::AI5;  break;
					case 6:  CityTeam = EOwnerTeam::AI6;  break;
					case 7:  CityTeam = EOwnerTeam::AI7;  break;
					case 8:  CityTeam = EOwnerTeam::AI8;  break;
					case 9:  CityTeam = EOwnerTeam::AI9;  break;
					case 10: CityTeam = EOwnerTeam::AI10; break;
					case 11: CityTeam = EOwnerTeam::AI11; break;
					case 12: CityTeam = EOwnerTeam::AI12; break;
					case 13: CityTeam = EOwnerTeam::AI13; break;
					case 14: CityTeam = EOwnerTeam::AI14; break;
					case 15: CityTeam = EOwnerTeam::AI15; break;
					case 16: CityTeam = EOwnerTeam::AI16; break;
					case 17: CityTeam = EOwnerTeam::AI17; break;
					case 18: CityTeam = EOwnerTeam::AI18; break;
					case 19: CityTeam = EOwnerTeam::AI19; break;
					default: CityTeam = EOwnerTeam::AI1;  break;
				}
			}

			// Spawn city actor
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			ACityActor* NewCity = GetWorld()->SpawnActor<ACityActor>(ACityActor::StaticClass(), CitySpawnLocation, FRotator::ZeroRotator, SpawnParams);

			if (NewCity)
			{
				FVector PlanetCenter = GetActorLocation();
				NewCity->PlanetCenter = PlanetCenter;
				NewCity->PlanetRadius = ActualTerrainRadius; // Use actual terrain height, not base radius
				NewCity->OwningPlanet = this;
				NewCity->OwnerTeam = CityTeam;
				NewCity->bIsCoastal = (CityIndex <= 4) || (CityIndex >= 10 && CityIndex <= 14); // Coastal: player+AI1-AI4 and AI10-AI14
				NewCity->ContinentID = (CityIndex < 10) ? Continent1Id : Continent2Id;
				NewCity->AlignToPlanet(); // This will now position at the correct terrain height
				NewCity->UpdateColor();
				
				// Spawn capital building now that all properties are set
				NewCity->SpawnCapitalBuilding();
				
				SpawnedCities.Add(NewCity);
				SpawnedLocations.Add(CitySpawnLocation);

				// Store center point for minimap (center on player's starting continent)
				if (CityTeam == EOwnerTeam::Player)
				{
					MinimapCenterPoint = SpawnDirection; // Store player city's continent position
					UE_LOG(LogTemp, Log, TEXT("Minimap center set to player continent at: %s"), *MinimapCenterPoint.ToString());
				}

				// Add player city to player controller
				if (CityTeam == EOwnerTeam::Player)
				{
					APlanetConquestPlayerController* PlayerController = Cast<APlanetConquestPlayerController>(GetWorld()->GetFirstPlayerController());
					if (PlayerController)
					{
						PlayerController->ControlledCities.Add(NewCity);
						UE_LOG(LogTemp, Log, TEXT("SpawnCities: ✓ PLAYER city spawned and added to ControlledCities at %s"),
							*CitySpawnLocation.ToString());
					}
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("SpawnCities: ✓ AI Team %d city spawned at %s"),
						(int32)CityTeam - 1, *CitySpawnLocation.ToString());
				}

				// Spawn 3 starting vehicles for this city (spread around city)
				for (int32 VehicleIndex = 0; VehicleIndex < 3; VehicleIndex++)
				{
					// Create vehicles in a ring around the city
					float Angle = (VehicleIndex * 120.0f); // 120 degrees apart (3 vehicles)
					float AngleRad = FMath::DegreesToRadians(Angle);
					
					FVector CityDirection = SpawnDirection; // Direction from planet center to city
					FVector Tangent = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
					if (Tangent.IsNearlyZero())
					{
						Tangent = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
					}
					FVector Bitangent = FVector::CrossProduct(CityDirection, Tangent).GetSafeNormal();
					
					float SpawnDistance = 500.0f; // Distance from city
					FVector Offset = (Tangent * FMath::Cos(AngleRad) + Bitangent * FMath::Sin(AngleRad)) * SpawnDistance;
					FVector VehicleDirection = (CityDirection * ActualTerrainRadius + Offset).GetSafeNormal();
					
					// Get actual terrain height at vehicle spawn location (spawn 50 units above surface)
					float VehicleTerrainHeight = CalculateHeightAtPoint(VehicleDirection);
					float VehicleTerrainRadius = PlanetRadius * (1.0f + VehicleTerrainHeight);
					FVector VehicleSpawnLocation = VehicleDirection * (VehicleTerrainRadius + 50.0f);
					
					FActorSpawnParameters VehicleSpawnParams;
					VehicleSpawnParams.Owner = this;
					AVehicleActor* NewVehicle = GetWorld()->SpawnActor<AVehicleActor>(AVehicleActor::StaticClass(), VehicleSpawnLocation, FRotator::ZeroRotator, VehicleSpawnParams);
					
					if (NewVehicle)
					{
						NewVehicle->PlanetCenter = PlanetCenter;
						NewVehicle->PlanetRadius = VehicleTerrainRadius; // Use terrain radius (AlignToPlanet adds +50 for hovering)
						NewVehicle->OwnerTeam = CityTeam; // Inherit team from city
						NewVehicle->OwningPlanet = this; // Pass planet reference for terrain queries
						NewVehicle->AlignToPlanet();
						NewVehicle->UpdateColor();
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("SpawnCities: ✗ Failed to spawn ACityActor at valid location!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnCities: ✗ City %d failed after %d attempts - WrongContinent: %d, NotLand: %d, TooClose: %d"),
				CityIndex, MaxCitySpawnAttempts, WrongContinentCount, NotLandCount, TooCloseCount);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("========== SpawnCities COMPLETE: %d/%d cities spawned =========="),
		SpawnedCities.Num(), NumCitiesToSpawn);
	
	if (SpawnedCities.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("WARNING: No cities were spawned! Check that bSpawnCities is enabled in editor."));
	}
}

void APlanetActor::SpawnResourcesAroundPlanet(FRandomStream& RNG)
{
	if (NumberOfResourcesAtStart <= 0)
	{
		return;
	}

	FVector PlanetCenter = GetActorLocation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;

	// Use spawned cities from the new system
	TArray<ACityActor*> AllCities = SpawnedCities;

	// If no cities exist, fall back to random spawning
	bool bSpawnNearCities = AllCities.Num() > 0;

	int32 SuccessfulSpawns = 0;
	float MinDistanceBetweenResources = 600.0f; // Minimum distance between territory resources and between cluster resources (ensures 400u waypoint rings don't overlap)
	float MinDistanceFromCities = 1500.0f; // Minimum distance from city center
	float MaxDistanceFromCities = 5000.0f; // Maximum distance from city (matches territory radius)
	int32 MaxAttemptsPerResource = 30; // Max tries to find a valid spot

	// PHASE 1: Guarantee each city gets imbalanced resource distribution
	// Each territory gets: 1-2 green, either (1-2 black + 5-6 orange) or (1-2 orange + 5-6 black)
	int32 ResourcesSpentOnGuarantee = 0;
	
	if (bSpawnNearCities)
	{
		// Track dominance types to ensure variety
		bool bHasBlackDominant = false;
		bool bHasOrangeDominant = false;
		int32 CityIndex = 0;
		
		for (ACityActor* City : AllCities)
		{
			int32 ResourcesForThisCity = 0;
			
			// Determine which substrate is scarce for this territory
			// Ensure at least one city is black-dominant and one is orange-dominant
			bool bBlackIsScarce;
			if (CityIndex == 0)
			{
				// First city: random choice
				bBlackIsScarce = RNG.RandRange(0, 1) == 1;
				if (bBlackIsScarce)
					bHasBlackDominant = true;
				else
					bHasOrangeDominant = true;
			}
			else if (CityIndex == 1)
			{
				// Second city: opposite of first to ensure variety
				bBlackIsScarce = !bHasBlackDominant;
				if (bBlackIsScarce)
					bHasBlackDominant = true;
				else
					bHasOrangeDominant = true;
			}
			else
			{
				// Rest can be random
				bBlackIsScarce = RNG.RandRange(0, 1) == 1;
			}
			
			// Determine exact counts
			int32 GreenCount = RNG.RandRange(1, 2);
			int32 ScarceCount = RNG.RandRange(1, 2);
			int32 AbundantCount = RNG.RandRange(5, 6);
			int32 TotalResourcesForCity = GreenCount + ScarceCount + AbundantCount;
			
			UE_LOG(LogTemp, Log, TEXT("City %s territory will spawn: %d green, %d %s (scarce), %d %s (abundant)"), 
				*City->CityName,
				GreenCount, 
				ScarceCount, bBlackIsScarce ? TEXT("black") : TEXT("orange"),
				AbundantCount, bBlackIsScarce ? TEXT("orange") : TEXT("black"));
			
			// Build array of resource types to spawn in order
			TArray<EResourceType> ResourcesToSpawn;
			
			// Add green resources
			for (int32 i = 0; i < GreenCount; i++)
			{
				ResourcesToSpawn.Add(EResourceType::GreenSubstrate);
			}
			
			// Add scarce substrate
			EResourceType ScarceType = bBlackIsScarce ? EResourceType::BlackSubstrate : EResourceType::OrangeSubstrate;
			for (int32 i = 0; i < ScarceCount; i++)
			{
				ResourcesToSpawn.Add(ScarceType);
			}
			
			// Add abundant substrate
			EResourceType AbundantType = bBlackIsScarce ? EResourceType::OrangeSubstrate : EResourceType::BlackSubstrate;
			for (int32 i = 0; i < AbundantCount; i++)
			{
				ResourcesToSpawn.Add(AbundantType);
			}
			
			// Now spawn each resource
			for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToSpawn.Num(); ResourceIndex++)
			{
				EResourceType ResourceType = ResourcesToSpawn[ResourceIndex];
				FVector ResourceSpawnLocation;
				bool bValidLocationFound = false;
				
				// Try to find a valid spawn location near THIS city
				for (int32 Attempt = 0; Attempt < MaxAttemptsPerResource; ++Attempt)
				{
					FVector CityLocation = City->GetActorLocation();
					FVector CityDirection = (CityLocation - PlanetCenter).GetSafeNormal();

					// Generate random distance from city (guaranteed resources spawn: 3000-5000)
					float DistanceFromCity = RNG.FRandRange(3000.0f, 5000.0f);

					// Create tangent vectors for random direction around city
					FVector Tangent1 = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
					if (Tangent1.IsNearlyZero())
					{
						Tangent1 = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
					}
					FVector Tangent2 = FVector::CrossProduct(CityDirection, Tangent1).GetSafeNormal();

					// Random angle around the city
					float RandomAngle = RNG.FRandRange(0.0f, 2.0f * PI);

					// Calculate offset in tangent space
					FVector Offset = (Tangent1 * FMath::Cos(RandomAngle) + Tangent2 * FMath::Sin(RandomAngle)) * DistanceFromCity;

					// Project onto planet surface with terrain height
					FVector SpawnDirection = (CityDirection * PlanetRadius + Offset).GetSafeNormal();
					
					// Get terrain height at this point (spawn 150 units above surface)
					float TerrainHeight = CalculateHeightAtPoint(SpawnDirection);
					float TerrainRadius = PlanetRadius * (1.0f + TerrainHeight);
					ResourceSpawnLocation = PlanetCenter + SpawnDirection * (TerrainRadius + 75.0f);
					
					// Check if spawn point is on land
					if (!IsPointOnLand(SpawnDirection))
					{
						continue; // Resources only spawn on land
					}
					
					// Check distance to all cities (make sure we're not TOO close)
					bValidLocationFound = true;
					for (ACityActor* OtherCity : AllCities)
					{
						if (OtherCity)
						{
							float DistanceToCity = FVector::Dist(ResourceSpawnLocation, OtherCity->GetActorLocation());
							if (DistanceToCity < MinDistanceFromCities)
							{
								bValidLocationFound = false;
								break;
							}
						}
					}
					
					if (!bValidLocationFound)
					{
						continue;
					}
					
					// Check distance to all existing resources
					for (AResourceActor* ExistingResource : SpawnedResources)
					{
						if (ExistingResource)
						{
							float Distance = FVector::Dist(ResourceSpawnLocation, ExistingResource->GetActorLocation());
							if (Distance < MinDistanceBetweenResources)
							{
								bValidLocationFound = false;
								break;
							}
						}
					}
					
					if (bValidLocationFound)
					{
						break; // Found a good spot!
					}
				}
				
				if (bValidLocationFound)
				{
					// Spawn the resource at the valid location
					AResourceActor* NewResource = GetWorld()->SpawnActor<AResourceActor>(AResourceActor::StaticClass(), ResourceSpawnLocation, FRotator::ZeroRotator, SpawnParams);

					if (NewResource)
					{
						NewResource->PlanetCenter = PlanetCenter;
						NewResource->PlanetRadius = PlanetRadius * (1.0f + CalculateHeightAtPoint((ResourceSpawnLocation - PlanetCenter).GetSafeNormal()));
						NewResource->ResourceType = ResourceType;
						
						// Determine income based on resource type and scarcity
						int32 IncomeValue = 0;
						float SizeScale = 1.0f;
						
						if (ResourceType == EResourceType::GreenSubstrate)
						{
							// Green substrate: random between 10, 25, 40
							int32 Choices[] = {10, 25, 40};
							IncomeValue = Choices[RNG.RandRange(0, 2)];
						}
						else if (ResourceType == ScarceType)
						{
							// Scarce substrate
							if (ScarceCount == 1)
							{
								// If only 1 scarce resource, must be at least 25, max 40
								int32 Choices[] = {25, 40};
								IncomeValue = Choices[RNG.RandRange(0, 1)];
							}
							else
							{
								// If 2 scarce resources, can be anything up to 40
								int32 Choices[] = {10, 25, 40};
								IncomeValue = Choices[RNG.RandRange(0, 2)];
							}
						}
						else
						{
							// Abundant substrate: can be anything up to 40
							int32 Choices[] = {10, 25, 40};
							IncomeValue = Choices[RNG.RandRange(0, 2)];
						}
						
						// Set income and size based on income value
						NewResource->IncomePerInterval = IncomeValue;
						
						// Size scaling based on income
						if (IncomeValue == 10)
						{
							SizeScale = 0.7f;
						}
						else if (IncomeValue == 25)
						{
							SizeScale = 0.9f;
						}
						else // 40
						{
							SizeScale = 1.1f;
						}
						
						// Store size scale in the resource
						NewResource->SizeScale = SizeScale;
						
						NewResource->AlignToPlanet();
						NewResource->SetupMesh(); // Load crystal mesh for black substrate
						NewResource->UpdateColor(); // Update color after setting type
						SpawnedResources.Add(NewResource);
						SuccessfulSpawns++;
						ResourcesForThisCity++;
						ResourcesSpentOnGuarantee++;
						
						UE_LOG(LogTemp, Log, TEXT("Spawned %s resource: %d/cycle"), 
							ResourceType == EResourceType::GreenSubstrate ? TEXT("green") : 
							(ResourceType == ScarceType ? TEXT("scarce") : TEXT("abundant")),
							IncomeValue);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not guarantee resource %d for city %s after %d attempts"), 
						ResourceIndex, *City->CityName, MaxAttemptsPerResource);
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("City %s guaranteed %d resources"), *City->CityName, ResourcesForThisCity);
			CityIndex++;
		}
	}

	// PHASE 2: Spawn resource clusters in the wild (groups of 2-5 resources, away from cities)
	// Clusters spawn on the same continent as cities
	
	// Find the two largest land continents for cluster spawning
	int32 ClusterContinent1 = -1;
	int32 ClusterContinent2 = -1;
	{
		float LargestSize = 0.0f;
		float SecondSize  = 0.0f;
		for (int32 i = 0; i < ContinentSeeds.Num(); i++)
		{
			if (ContinentSeeds[i].bIsLand)
			{
				if (ContinentSeeds[i].Size > LargestSize)
				{
					SecondSize    = LargestSize;
					ClusterContinent2 = ClusterContinent1;
					LargestSize   = ContinentSeeds[i].Size;
					ClusterContinent1 = i;
				}
				else if (ContinentSeeds[i].Size > SecondSize)
				{
					SecondSize    = ContinentSeeds[i].Size;
					ClusterContinent2 = i;
				}
			}
		}
	}

	if (ClusterContinent1 < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No land continent found for cluster spawning"));
	}
	else
	{
		TArray<int32> ClusterContinents = { ClusterContinent1 };
		if (ClusterContinent2 >= 0 && ClusterContinent2 != ClusterContinent1)
			ClusterContinents.Add(ClusterContinent2);

	for (int32 TargetContinentId : ClusterContinents)
	{
		FVector ContinentCenter = ContinentSeeds[TargetContinentId].Position;
		UE_LOG(LogTemp, Log, TEXT("Spawning clusters on continent %d (center: %s)"), TargetContinentId, *ContinentCenter.ToString());
		
		int32 MaxClusters = 12; // Resource clusters for AI teams to compete over
		float ClusterMinDistanceFromCities = 10000.0f; // 5000u territory radius + 5000u beyond border
		float MinDistanceBetweenClusters = 3000.0f; // Minimum spacing between clusters
		float ResourceSpacingInCluster = 1250.0f; // Resources within a cluster spread out
		int32 ClustersSpawned = 0;
		
		TArray<FVector> ClusterLocations; // Track cluster centers to avoid overlap
		
		for (int32 ClusterIndex = 0; ClusterIndex < MaxClusters; ClusterIndex++)
		{
			// Determine cluster size first (before finding location)
			int32 ResourcesInCluster;
			float Roll = RNG.GetFraction();
			if (Roll < 0.60f) // 60% chance
			{
				ResourcesInCluster = RNG.RandRange(2, 3); // 2 or 3 resources (most common)
			}
			else if (Roll < 0.90f) // 30% chance
			{
				ResourcesInCluster = 4; // 4 resources (uncommon)
			}
			else // 10% chance
			{
				ResourcesInCluster = 5; // 5 resources (rare)
			}
			
			FVector ClusterCenter;
			bool bValidClusterLocationFound = false;
			
			// Try to find a valid location for this cluster within 40° of continent center
			for (int32 Attempt = 0; Attempt < MaxAttemptsPerResource * 2; ++Attempt)
			{
				// Generate random point within cone around continent center (40 degrees)
				float RandomAngleDegrees = RNG.FRandRange(0.0f, 40.0f);
				float RandomAngleRadians = FMath::DegreesToRadians(RandomAngleDegrees);
				
				// Create tangent vectors
				FVector Tangent1 = FVector::CrossProduct(ContinentCenter, FVector::UpVector).GetSafeNormal();
				if (Tangent1.IsNearlyZero())
				{
					Tangent1 = FVector::CrossProduct(ContinentCenter, FVector::RightVector).GetSafeNormal();
				}
				FVector Tangent2 = FVector::CrossProduct(ContinentCenter, Tangent1).GetSafeNormal();
				
				// Random rotation around continent center
				float RandomRotation = RNG.FRandRange(0.0f, 2.0f * PI);
				FVector TangentDirection = Tangent1 * FMath::Cos(RandomRotation) + Tangent2 * FMath::Sin(RandomRotation);
				
				// Slerp between continent center and tangent direction
				FVector SpawnDirection = FMath::Lerp(ContinentCenter, TangentDirection, FMath::Sin(RandomAngleRadians)).GetSafeNormal();

				ClusterCenter = PlanetCenter + SpawnDirection * PlanetRadius;
				
				// Check if on land
				if (!IsPointOnLand(SpawnDirection))
				{
					continue;
				}
				
				// Check if on same continent
				int32 ClusterContinent = GetContinentIdForPoint(SpawnDirection);
				if (ClusterContinent != TargetContinentId)
				{
					continue;
				}
				
				// Check distance to all cities (10k = territory radius + buffer beyond border)
				bValidClusterLocationFound = true;
				for (ACityActor* City : AllCities)
				{
					if (City)
					{
						float DistanceToCity = FVector::Dist(ClusterCenter, City->GetActorLocation());
						if (DistanceToCity < ClusterMinDistanceFromCities)
						{
							bValidClusterLocationFound = false;
							break;
						}
					}
				}
				
				if (!bValidClusterLocationFound)
				{
					continue;
				}
				
				// Check distance to all volcanoes (continent centers) - minimum 8000 units
				for (const FContinentSeed& Seed : ContinentSeeds)
				{
					FVector VolcanoPosition = PlanetCenter + Seed.Position * PlanetRadius;
					float DistanceToVolcano = FVector::Dist(ClusterCenter, VolcanoPosition);
					if (DistanceToVolcano < 8000.0f)
					{
						bValidClusterLocationFound = false;
						break;
					}
				}
				
				if (!bValidClusterLocationFound)
				{
					continue;
				}
				
				// Check distance to other clusters
				for (const FVector& ExistingCluster : ClusterLocations)
				{
					float DistanceToCluster = FVector::Dist(ClusterCenter, ExistingCluster);
					if (DistanceToCluster < MinDistanceBetweenClusters)
					{
						bValidClusterLocationFound = false;
						break;
					}
				}
				
				if (bValidClusterLocationFound)
				{
					break; // Found a good cluster location!
				}
			}
			
			if (!bValidClusterLocationFound)
			{
				UE_LOG(LogTemp, Warning, TEXT("Could not find valid location for cluster %d"), ClusterIndex);
				continue;
			}
			
			// Track this cluster location
			ClusterLocations.Add(ClusterCenter);
			
			// Cluster type: 75% orange, 25% black substrate
		EResourceType ClusterType = (RNG.GetFraction() < 0.75f) ? EResourceType::OrangeSubstrate : EResourceType::BlackSubstrate;
			int32 SuccessfulClusterSpawns = 0;
		
		// Spawn all resources in this cluster
		for (int32 ResourceIndex = 0; ResourceIndex < ResourcesInCluster; ResourceIndex++)
		{
			FVector ResourceLocation;
			bool bValidResourceLocationFound = false;
			
			// Try to find a spot near the cluster center
			for (int32 Attempt = 0; Attempt < 20; ++Attempt)
			{
				// Get cluster center direction from planet
				FVector ClusterDirection = (ClusterCenter - PlanetCenter).GetSafeNormal();
				
				// Create tangent vectors for offsetting within cluster
				FVector Tangent1 = FVector::CrossProduct(ClusterDirection, FVector::UpVector).GetSafeNormal();
				if (Tangent1.IsNearlyZero())
				{
					Tangent1 = FVector::CrossProduct(ClusterDirection, FVector::RightVector).GetSafeNormal();
				}
				FVector Tangent2 = FVector::CrossProduct(ClusterDirection, Tangent1).GetSafeNormal();
				
				// Random offset within cluster (small radius for tight grouping)
				float OffsetDistance = RNG.FRandRange(0.0f, ResourceSpacingInCluster);
				float RandomAngle = RNG.FRandRange(0.0f, 2.0f * PI);
				FVector Offset = (Tangent1 * FMath::Cos(RandomAngle) + Tangent2 * FMath::Sin(RandomAngle)) * OffsetDistance;
				
				// Project onto planet surface
				FVector SpawnDirection = (ClusterDirection * PlanetRadius + Offset).GetSafeNormal();
				
				// Get terrain height (spawn 150 units above surface)
				float TerrainHeight = CalculateHeightAtPoint(SpawnDirection);
				float TerrainRadius = PlanetRadius * (1.0f + TerrainHeight);
				ResourceLocation = PlanetCenter + SpawnDirection * (TerrainRadius + 150.0f);
				
				// Check if on land
				if (!IsPointOnLand(SpawnDirection))
				{
					continue;
				}
				
				// Check distance to all existing resources
				bValidResourceLocationFound = true;
				for (AResourceActor* ExistingResource : SpawnedResources)
				{
					if (ExistingResource)
					{
						float Distance = FVector::Dist(ResourceLocation, ExistingResource->GetActorLocation());
						
						// Territory resources (ClusterID == -1) need 800 unit minimum from cluster resources
						// Cluster-to-cluster uses standard 600 unit minimum
						float RequiredDistance = (ExistingResource->ClusterID == -1) ? 800.0f : MinDistanceBetweenResources;
						
						if (Distance < RequiredDistance)
						{
							bValidResourceLocationFound = false;
							break;
						}
					}
				}
				
				if (bValidResourceLocationFound)
				{
					break;
				}
			}
			
			if (bValidResourceLocationFound)
			{
				// Spawn the clustered resource
				AResourceActor* NewResource = GetWorld()->SpawnActor<AResourceActor>(AResourceActor::StaticClass(), ResourceLocation, FRotator::ZeroRotator, SpawnParams);

				if (NewResource)
				{
					NewResource->PlanetCenter = PlanetCenter;
					NewResource->PlanetRadius = PlanetRadius * (1.0f + CalculateHeightAtPoint((ResourceLocation - PlanetCenter).GetSafeNormal()));
					NewResource->ResourceType = ClusterType; // All resources in cluster have same type
					NewResource->ClusterID = ClustersSpawned; // Tag this resource with its cluster ID
					
					// Cluster resources have random income (10, 25, or 40)
					int32 Choices[] = {10, 25, 40};
					int32 IncomeValue = Choices[RNG.RandRange(0, 2)];
					NewResource->IncomePerInterval = IncomeValue;
					
					// Size scaling based on income
					float SizeScale = 1.0f;
					if (IncomeValue == 10)
						SizeScale = 0.7f;
					else if (IncomeValue == 25)
						SizeScale = 0.9f;
					else
						SizeScale = 1.1f;
					
					NewResource->SizeScale = SizeScale;
					NewResource->AlignToPlanet();
					NewResource->SetupMesh();
					NewResource->UpdateColor();
					SpawnedResources.Add(NewResource);
					SuccessfulClusterSpawns++;
					SuccessfulSpawns++;
				}
			}
		}
		
		if (SuccessfulClusterSpawns > 0)
		{
			ClustersSpawned++;
			UE_LOG(LogTemp, Log, TEXT("Spawned cluster %d with %d %s resources"), 
				ClustersSpawned, SuccessfulClusterSpawns, 
				ClusterType == EResourceType::BlackSubstrate ? TEXT("BLACK") : TEXT("ORANGE"));
		}
	}
	
		UE_LOG(LogTemp, Log, TEXT("Cluster spawning complete: %d clusters with resources spawned on continent %d"), 
			ClustersSpawned, TargetContinentId);
	} // end per-continent cluster loop
	} // end ClusterContinent1 valid check

	UE_LOG(LogTemp, Log, TEXT("========== SpawnResourcesAroundPlanet COMPLETE: %d resources spawned =========="), 
		SuccessfulSpawns);
}

// ===== MINIMAP GENERATION =====

UTexture2D* APlanetActor::GenerateMinimapTexture(int32 TextureWidth, int32 TextureHeight)
{
	UE_LOG(LogTemp, Log, TEXT("Generating minimap texture: %dx%d"), TextureWidth, TextureHeight);

	// Create the texture with proper UI-compatible settings
	MinimapTexture = UTexture2D::CreateTransient(TextureWidth, TextureHeight, PF_B8G8R8A8);
	if (!MinimapTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create minimap texture"));
		return nullptr;
	}

	// Configure texture settings for UI rendering
	MinimapTexture->SRGB = false; // Disable SRGB for more direct color control
	MinimapTexture->Filter = TF_Bilinear;
	MinimapTexture->CompressionSettings = TC_VectorDisplacementmap; // No compression
	MinimapTexture->MipGenSettings = TMGS_NoMipmaps;
	MinimapTexture->LODGroup = TEXTUREGROUP_UI;
	MinimapTexture->NeverStream = true;
	MinimapTexture->AddToRoot(); // Prevent garbage collection
	
	UE_LOG(LogTemp, Warning, TEXT("Texture settings: SRGB=%d, Filter=%d, Compression=%d"), 
		MinimapTexture->SRGB, (int32)MinimapTexture->Filter, (int32)MinimapTexture->CompressionSettings);

	// Lock the mip data and write directly (synchronous - static terrain only)
	FTexture2DMipMap& Mip = MinimapTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* PixelPtr = static_cast<uint8*>(TextureData);

	// Track color distribution for debugging
	int32 OceanPixels = 0;
	int32 LandPixels = 0;

	// Calculate rotation to center map on MinimapCenterPoint (player's continent)
	// Center of equirectangular map (U=0.5, V=0.5) = longitude 0, latitude 0 = ForwardVector (1,0,0)
	FQuat MapRotation = FQuat::FindBetweenVectors(FVector::ForwardVector, MinimapCenterPoint);

	// Generate equirectangular projection
	for (int32 Y = 0; Y < TextureHeight; Y++)
	{
		for (int32 X = 0; X < TextureWidth; X++)
		{
			// Convert pixel position to UV (0-1 range)
			float U = (float)X / (float)TextureWidth;
			float V = (float)Y / (float)TextureHeight;

			// Convert UV to longitude/latitude (equirectangular projection)
			float Longitude = U * 2.0f * PI - PI;        // -PI to PI
			float Latitude = (1.0f - V) * PI - PI * 0.5f; // -PI/2 to PI/2 (flipped so north is up)

			// Convert lat/lon to unit sphere point
			float CosLat = FMath::Cos(Latitude);
			FVector SpherePoint = FVector(
				CosLat * FMath::Cos(Longitude),
				CosLat * FMath::Sin(Longitude),
				FMath::Sin(Latitude)
			);
			SpherePoint.Normalize();

			// Rotate sphere to center map on player's continent
			FVector RotatedSphere = MapRotation.RotateVector(SpherePoint);

			// Sample height at this point
			float Height = CalculateHeightAtPoint(RotatedSphere);

			// Check if this point is in a volcano area
			float VolcanoInfluence = 0.0f;
			if (bPlaceVolcanoes && VolcanoPositions.Num() > 0)
			{
				for (const FVector& VolcanoCenter : VolcanoPositions)
				{
					// Rotate volcano center to match rotated sphere
					FVector RotatedVolcanoCenter = MapRotation.RotateVector(VolcanoCenter);
					
					float AngularDist = AngularDistance(RotatedSphere, RotatedVolcanoCenter);
					float BasaltRadius = VolcanoRadius * BasaltRadiusMultiplier;
					float BasaltAngularRadius = BasaltRadius / PlanetRadius;
					
					if (AngularDist < BasaltAngularRadius)
					{
						float NormalizedDist = AngularDist / BasaltAngularRadius;
						float BlendFactor = 1.0f - NormalizedDist;
						BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);
						BlendFactor = BlendFactor * BlendFactor * (3.0f - 2.0f * BlendFactor); // Smoothstep
						VolcanoInfluence = FMath::Max(VolcanoInfluence, BlendFactor);
					}
				}
			}

			// Determine color based on land/ocean
			FColor PixelColor;
			if (Height < SeaLevel)
			{
				// Ocean - deeper darker blue shades based on depth
				float OceanDepth = (SeaLevel - Height) / SeaLevel;
				uint8 BlueValue = FMath::Clamp(FMath::RoundToInt(80 + OceanDepth * 60), 60, 140);
				PixelColor = FColor(5, 30, BlueValue); // Darker, deeper blue
				OceanPixels++;
			}
			else if (VolcanoInfluence > 0.1f)
			{
				// Volcano area - dark grey
				uint8 GreyValue = FMath::Clamp(FMath::RoundToInt(60 + VolcanoInfluence * 40), 60, 100);
				PixelColor = FColor(GreyValue, GreyValue, GreyValue);
				LandPixels++;
			}
			else
			{
				// Land - dark tan shades based on elevation
				float LandHeight = (Height - SeaLevel) / (SeedHeight - SeaLevel);
				uint8 RedValue = FMath::Clamp(FMath::RoundToInt(140 + LandHeight * 40), 120, 180);
				uint8 GreenValue = FMath::Clamp(FMath::RoundToInt(120 + LandHeight * 30), 100, 150);
				uint8 BlueValue = FMath::Clamp(FMath::RoundToInt(80 + LandHeight * 30), 70, 110);
				PixelColor = FColor(RedValue, GreenValue, BlueValue);
				LandPixels++;
			}

			// Set pixel in texture (BGRA format)
			int32 PixelIndex = (Y * TextureWidth + X) * 4;
			PixelPtr[PixelIndex + 0] = PixelColor.B; // Blue
			PixelPtr[PixelIndex + 1] = PixelColor.G; // Green
			PixelPtr[PixelIndex + 2] = PixelColor.R; // Red
			PixelPtr[PixelIndex + 3] = 255;          // Alpha
		}
	}

	// Unlock and create GPU resource with data in place (terrain only - markers added via UMG overlay)
	Mip.BulkData.Unlock();
	MinimapTexture->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("Minimap texture generated: %d ocean pixels, %d land pixels, Total: %d"), 
		OceanPixels, LandPixels, OceanPixels + LandPixels);
	UE_LOG(LogTemp, Log, TEXT("Minimap texture pointer: %p, Size: %dx%d"), 
		MinimapTexture, TextureWidth, TextureHeight);
	
	return MinimapTexture;
}

FVector APlanetActor::MinimapUVToWorldPosition(FVector2D UV)
{
	// Convert UV (0-1) to longitude/latitude
	float Longitude = UV.X * 2.0f * PI - PI;            // -PI to PI
	float Latitude = (1.0f - UV.Y) * PI - PI * 0.5f;    // -PI/2 to PI/2

	// Convert lat/lon to unit sphere point
	float CosLat = FMath::Cos(Latitude);
	FVector SpherePoint = FVector(
		CosLat * FMath::Cos(Longitude),
		CosLat * FMath::Sin(Longitude),
		FMath::Sin(Latitude)
	);
	SpherePoint.Normalize();

	// Get terrain height at this point
	float Height = CalculateHeightAtPoint(SpherePoint);
	float TerrainRadius = PlanetRadius * (1.0f + Height);

	// Convert to world position
	FVector PlanetCenter = GetActorLocation();
	FVector WorldPosition = PlanetCenter + SpherePoint * TerrainRadius;

	return WorldPosition;
}

FVector2D APlanetActor::WorldPositionToMinimapUV(FVector WorldPosition)
{
	// Get direction from planet center to world position
	FVector PlanetCenter = GetActorLocation();
	FVector Direction = (WorldPosition - PlanetCenter);
	Direction.Normalize();

	// Apply the inverse rotation to match the rotated minimap
	// The map is rotated from ForwardVector to MinimapCenterPoint, so we need the inverse
	FQuat MapRotation = FQuat::FindBetweenVectors(FVector::ForwardVector, MinimapCenterPoint);
	FVector RotatedDirection = MapRotation.Inverse().RotateVector(Direction);

	// Convert unit sphere direction to latitude/longitude
	float Latitude = FMath::Asin(RotatedDirection.Z);  // -PI/2 to PI/2
	float Longitude = FMath::Atan2(RotatedDirection.Y, RotatedDirection.X);  // -PI to PI

	// Convert lat/lon to UV (0-1)
	float U = (Longitude + PI) / (2.0f * PI);  // Map -PI..PI to 0..1
	float V = 1.0f - ((Latitude + PI * 0.5f) / PI);  // Map -PI/2..PI/2 to 1..0 (flip so north is up)

	return FVector2D(U, V);
}

// ===== VOLCANO STAMPING =====

void APlanetActor::StampVolcanoes()
{
	VolcanoPositions.Empty();

	if (!bPlaceVolcanoes || !VolcanoHeightmap)
	{
		UE_LOG(LogTemp, Warning, TEXT("StampVolcanoes: Skipping (bPlaceVolcanoes=%s, VolcanoHeightmap=%s)"),
			bPlaceVolcanoes ? TEXT("true") : TEXT("false"),
			VolcanoHeightmap ? TEXT("valid") : TEXT("null"));
		return;
	}

	// Place one volcano at the center of each land continent
	for (const FContinentSeed& Seed : ContinentSeeds)
	{
		if (Seed.bIsLand)
		{
			VolcanoPositions.Add(Seed.Position);
			UE_LOG(LogTemp, Log, TEXT("Volcano placed at continent center: X=%.3f Y=%.3f Z=%.3f"),
				Seed.Position.X, Seed.Position.Y, Seed.Position.Z);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("StampVolcanoes: Placed %d volcanoes (one per land continent)"), VolcanoPositions.Num());
}

float APlanetActor::SampleVolcanoHeight(float U, float V) const
{
	if (!VolcanoHeightmap)
	{
		return 0.0f;
	}

	// Clamp UV to valid range
	U = FMath::Clamp(U, 0.0f, 1.0f);
	V = FMath::Clamp(V, 0.0f, 1.0f);

	// Get texture dimensions
	int32 Width = VolcanoHeightmap->GetSizeX();
	int32 Height = VolcanoHeightmap->GetSizeY();

	// Convert UV to pixel coordinates
	int32 X = FMath::Clamp(FMath::FloorToInt(U * Width), 0, Width - 1);
	int32 Y = FMath::Clamp(FMath::FloorToInt(V * Height), 0, Height - 1);

	// Read pixel data from mip level 0
	// Note: Texture must be readable (not compressed) - set to UserInterface2D or similar
	FTexture2DMipMap& Mip = VolcanoHeightmap->GetPlatformData()->Mips[0];
	FByteBulkData& BulkData = Mip.BulkData;

	// Lock for reading
	const uint8* TextureData = static_cast<const uint8*>(BulkData.LockReadOnly());
	if (!TextureData)
	{
		return 0.0f;
	}

	// Calculate pixel index (assuming PF_B8G8R8A8 format - 4 bytes per pixel: BGRA)
	int32 PixelIndex = (Y * Width + X) * 4;

	// Read grayscale value (all RGB channels should be the same for heightmap)
	// Format is BGRA, so R is at index +2
	uint8 HeightValue = TextureData[PixelIndex + 2];  // R channel

	BulkData.Unlock();

	// Convert 0-255 to 0-1 range
	return HeightValue / 255.0f;
}

// ===== LINE-OF-SIGHT CHECK =====

bool APlanetActor::HasClearLineOfSight(const FVector& StartPos, const FVector& EndPos) const
{
	// Convert to unit sphere directions
	FVector PlanetLoc = GetActorLocation();
	FVector StartDir = (StartPos - PlanetLoc).GetSafeNormal();
	FVector EndDir = (EndPos - PlanetLoc).GetSafeNormal();
	
	// Check if path crosses water (sample 5 points along the arc)
	const int32 NumSamples = 5;
	for (int32 s = 1; s < NumSamples; s++)
	{
		float T = (float)s / NumSamples;
		FVector MidDir = FMath::Lerp(StartDir, EndDir, T).GetSafeNormal();
		if (!IsPointOnLand(MidDir))
		{
			return false; // Water blocks line-of-sight
		}
	}
	
	// Check if path passes through any city (800u avoidance radius)
	const float CityAvoidanceRadius = 800.0f;
	for (ACityActor* City : SpawnedCities)
	{
		if (!City) continue;
		
		// Calculate closest point on line segment to city center
		FVector CityLoc = City->GetActorLocation();
		FVector LineDir = EndPos - StartPos;
		float LineLength = LineDir.Size();
		if (LineLength < 1.0f) continue;
		
		LineDir /= LineLength; // Normalize
		
		// Project city onto line
		float T = FVector::DotProduct(CityLoc - StartPos, LineDir);
		T = FMath::Clamp(T, 0.0f, LineLength); // Clamp to segment
		
		FVector ClosestPoint = StartPos + LineDir * T;
		float DistToCity = FVector::Dist(ClosestPoint, CityLoc);
		
		if (DistToCity < CityAvoidanceRadius)
		{
			return false; // City blocks line-of-sight
		}
	}
	
	// Check if path passes through any resource (400u avoidance radius)
	const float ResourceAvoidanceRadius = 400.0f;
	for (AResourceActor* Resource : SpawnedResources)
	{
		if (!Resource) continue;
		
		// Calculate closest point on line segment to resource center
		FVector ResourceLoc = Resource->GetActorLocation();
		FVector LineDir = EndPos - StartPos;
		float LineLength = LineDir.Size();
		if (LineLength < 1.0f) continue;
		
		LineDir /= LineLength; // Normalize
		
		// Project resource onto line
		float T = FVector::DotProduct(ResourceLoc - StartPos, LineDir);
		T = FMath::Clamp(T, 0.0f, LineLength); // Clamp to segment
		
		FVector ClosestPoint = StartPos + LineDir * T;
		float DistToResource = FVector::Dist(ClosestPoint, ResourceLoc);
		
		if (DistToResource < ResourceAvoidanceRadius)
		{
			return false; // Resource blocks line-of-sight
		}
	}
	
	return true; // Clear path
}

// ===== ARC INTERSECTION HELPERS =====

bool APlanetActor::DoArcsIntersect(const FVector& Arc1Start, const FVector& Arc1End, const FVector& Arc2Start, const FVector& Arc2End) const
{
	// Convert world positions to unit sphere directions
	FVector PlanetLoc = GetActorLocation();
	FVector A1 = (Arc1Start - PlanetLoc).GetSafeNormal();
	FVector A2 = (Arc1End - PlanetLoc).GetSafeNormal();
	FVector B1 = (Arc2Start - PlanetLoc).GetSafeNormal();
	FVector B2 = (Arc2End - PlanetLoc).GetSafeNormal();
	
	// Two great-circle arcs intersect if they cross each other
	// Use the spherical geometry property: arcs cross if the endpoints of one arc
	// are on opposite sides of the great circle containing the other arc
	
	// Great circle normal for arc A (cross product of endpoints)
	FVector NormalA = FVector::CrossProduct(A1, A2).GetSafeNormal();
	
	// Check if B's endpoints are on opposite sides of A's great circle
	float DotB1 = FVector::DotProduct(B1, NormalA);
	float DotB2 = FVector::DotProduct(B2, NormalA);
	
	// If same sign (or one is zero), B doesn't cross A's great circle
	if (DotB1 * DotB2 > 0.0f)
	{
		return false;
	}
	
	// Great circle normal for arc B
	FVector NormalB = FVector::CrossProduct(B1, B2).GetSafeNormal();
	
	// Check if A's endpoints are on opposite sides of B's great circle
	float DotA1 = FVector::DotProduct(A1, NormalB);
	float DotA2 = FVector::DotProduct(A2, NormalB);
	
	// If same sign (or one is zero), A doesn't cross B's great circle
	if (DotA1 * DotA2 > 0.0f)
	{
		return false;
	}
	
	// Both arcs cross each other's great circles
	return true;
}

float APlanetActor::AngularDistanceToArc(const FVector& Point, const FVector& ArcStart, const FVector& ArcEnd) const
{
	// Convert to unit sphere directions
	FVector PlanetLoc = GetActorLocation();
	FVector P = (Point - PlanetLoc).GetSafeNormal();
	FVector A = (ArcStart - PlanetLoc).GetSafeNormal();
	FVector B = (ArcEnd - PlanetLoc).GetSafeNormal();
	
	// Great circle normal for the arc
	FVector ArcNormal = FVector::CrossProduct(A, B).GetSafeNormal();
	
	// Angular distance from point to great circle plane
	// This is the minimum angular distance to the arc (or its extension)
	float DotP = FVector::DotProduct(P, ArcNormal);
	float AngularDist = FMath::Abs(FMath::Asin(FMath::Clamp(DotP, -1.0f, 1.0f)));
	
	return AngularDist;
}

// ===== NAVIGATION GRAPH GENERATION =====

void APlanetActor::GenerateNavGraph()
{
	NavNodes.Empty();
	LandmassCount = 0;

	UE_LOG(LogTemp, Log, TEXT("========== GENERATING NAVIGATION GRAPH =========="));

	const float MinWaypointSpacing = 200.0f; // Minimum distance between waypoints
	const float CoastlineHeightTarget = 0.005f; // Exact coastline height (0.5% = 400 units on 80k planet)
	const float CoastlineHeightTolerance = 0.0001f; // Tiny tolerance for floating point comparison
	
	// ===== PHASE 1: Generate coastline waypoints =====
	
	// Sample planet surface to find coastline points
	// Use finer resolution than old system to catch coastline details
	const int32 NumLatitudes = 120;   // 1.5-degree spacing
	const int32 NumLongitudes = 240;  // 1.5-degree spacing at equator
	
	TArray<FVector> CandidateCoastlinePoints;
	
	for (int32 Lat = 0; Lat <= NumLatitudes; Lat++)
	{
		float Phi = PI * Lat / NumLatitudes; // 0 to PI (north pole to south pole)
		float Z = FMath::Cos(Phi);
		float RingRadius = FMath::Sin(Phi);
		
		// Fewer samples near poles where ring radius is small
		int32 RingNodes = FMath::Max(4, FMath::RoundToInt(NumLongitudes * RingRadius));
		
		for (int32 Lon = 0; Lon < RingNodes; Lon++)
		{
			float Theta = 2.0f * PI * Lon / RingNodes;
			
			FVector Direction(
				RingRadius * FMath::Cos(Theta),
				RingRadius * FMath::Sin(Theta),
				Z
			);
			Direction.Normalize();
			
			// Check if this point is at exact coastline height
			float TerrainHeight = CalculateHeightAtPoint(Direction);
			if (FMath::Abs(TerrainHeight - CoastlineHeightTarget) <= CoastlineHeightTolerance)
			{
				// This is a coastline point
				FVector WorldPos = GetActorLocation() + Direction * PlanetRadius * (1.0f + TerrainHeight);
				CandidateCoastlinePoints.Add(WorldPos);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Phase 1a: Found %d candidate coastline points"), CandidateCoastlinePoints.Num());
	
	// Filter candidates to enforce minimum spacing
	TArray<FVector> CoastlineWaypoints;
	for (const FVector& Candidate : CandidateCoastlinePoints)
	{
		bool bTooClose = false;
		for (const FVector& Existing : CoastlineWaypoints)
		{
			if (FVector::Dist(Candidate, Existing) < MinWaypointSpacing)
			{
				bTooClose = true;
				break;
			}
		}
		
		if (!bTooClose)
		{
			CoastlineWaypoints.Add(Candidate);
		}
	}
	
	// Add coastline waypoints to NavNodes
	for (const FVector& WaypointPos : CoastlineWaypoints)
	{
		FNavNode Node;
		Node.WorldPosition = WaypointPos;
		Node.Position = (WaypointPos - GetActorLocation()).GetSafeNormal();
		Node.Index = NavNodes.Num();
		Node.LandmassID = GetContinentIdForPoint(Node.Position); // Use Voronoi continent seed ID
		Node.Owner = nullptr; // Static coastline waypoint
		NavNodes.Add(Node);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Phase 1b: Created %d coastline waypoints (%.0f unit spacing)"), 
		CoastlineWaypoints.Num(), MinWaypointSpacing);
	
	// ===== PHASE 2: Generate volcano perimeter waypoints =====
	
	const float VolcanoClearance = 3500.0f; // Distance from volcano center
	const int32 WaypointsPerVolcano = 12; // 12 waypoints = 30-degree spacing
	
	for (const FVector& VolcanoDir : VolcanoPositions)
	{
		// VolcanoDir is a unit sphere direction
		// Create a ring of waypoints around the volcano at VolcanoClearance distance
		
		// Create two tangent vectors perpendicular to volcano direction
		FVector Tangent1 = FVector::CrossProduct(VolcanoDir, FVector::UpVector).GetSafeNormal();
		if (Tangent1.IsNearlyZero())
		{
			Tangent1 = FVector::CrossProduct(VolcanoDir, FVector::RightVector).GetSafeNormal();
		}
		FVector Tangent2 = FVector::CrossProduct(VolcanoDir, Tangent1).GetSafeNormal();
		
		// Place waypoints in a circle around volcano
		for (int32 i = 0; i < WaypointsPerVolcano; i++)
		{
			float Angle = (2.0f * PI * i) / WaypointsPerVolcano;
			
			// Offset direction on sphere surface
			FVector OffsetDir = Tangent1 * FMath::Cos(Angle) + Tangent2 * FMath::Sin(Angle);
			
			// Calculate the direction that's VolcanoClearance units away on sphere surface
			// Angular distance: Angle = Distance / Radius
			float AngularDistance = VolcanoClearance / PlanetRadius;
			
			// Rotate VolcanoDir toward OffsetDir by AngularDistance
			FVector WaypointDir = FMath::Lerp(VolcanoDir, OffsetDir, FMath::Sin(AngularDistance)).GetSafeNormal();
			
			// Get terrain height at waypoint location
			float TerrainHeight = CalculateHeightAtPoint(WaypointDir);
			float TerrainRadius = PlanetRadius * (1.0f + TerrainHeight);
			FVector WaypointPos = GetActorLocation() + WaypointDir * TerrainRadius;
			
			// Only add if on land
			if (IsPointOnLand(WaypointDir))
			{
				FNavNode Node;
				Node.WorldPosition = WaypointPos;
				Node.Position = WaypointDir;
				Node.Index = NavNodes.Num();
				Node.LandmassID = GetContinentIdForPoint(WaypointDir); // Use Voronoi continent seed ID
				Node.Owner = nullptr; // Static volcano waypoint
				NavNodes.Add(Node);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Phase 2: Created volcano waypoints around %d volcanoes"), VolcanoPositions.Num());
	
	// ===== PHASE 3: Generate edges and flood fill landmasses =====
	
	RegenerateNavigationEdges();
	
	UE_LOG(LogTemp, Log, TEXT("========== NAV GRAPH GENERATION COMPLETE: %d nodes =========="), NavNodes.Num());
}

// ===== DYNAMIC WAYPOINT REGISTRATION =====

int32 APlanetActor::RegisterNavigationWaypoint(FVector WorldPosition, AActor* OwnerActor)
{
	FNavNode NewNode;
	NewNode.WorldPosition = WorldPosition;
	NewNode.Position = (WorldPosition - GetActorLocation()).GetSafeNormal();
	NewNode.Index = NavNodes.Num();
	NewNode.LandmassID = GetContinentIdForPoint(NewNode.Position); // Use Voronoi continent seed ID
	NewNode.Owner = OwnerActor;
	
	NavNodes.Add(NewNode);
	
	// Don't regenerate edges immediately - let caller batch multiple waypoints then call RegenerateNavigationEdges()
	
	return NewNode.Index;
}

void APlanetActor::UnregisterNavigationWaypoint(int32 WaypointIndex, AActor* OwnerActor)
{
	if (WaypointIndex < 0 || WaypointIndex >= NavNodes.Num())
	{
		//UE_LOG(LogTemp, Warning, TEXT("UnregisterNavigationWaypoint: Invalid index %d"), WaypointIndex);
		return;
	}
	
	// Verify owner matches (security check)
	if (OwnerActor && NavNodes[WaypointIndex].Owner != OwnerActor)
	{
		//UE_LOG(LogTemp, Warning, TEXT("UnregisterNavigationWaypoint: Owner mismatch for waypoint %d"), WaypointIndex);
		return;
	}
	
	// Remove the waypoint
	NavNodes.RemoveAt(WaypointIndex);
	
	// Update all node indices
	for (int32 i = 0; i < NavNodes.Num(); i++)
	{
		NavNodes[i].Index = i;
	}
	
	// Regenerate edges to reflect new connectivity
	RegenerateNavigationEdges();
}

void APlanetActor::RegenerateNavigationEdges()
{
	// Clear all existing edges
	for (FNavNode& Node : NavNodes)
	{
		Node.Neighbors.Empty();
	}
	
	// Rebuild edges (same logic as GenerateNavGraph Phase 2)
	const float MaxEdgeAngle = FMath::DegreesToRadians(5.0f); // Connect nodes within 5 degrees
	const float CityAvoidanceRadius = 800.0f; // Avoid edges passing through cities
	const float ResourceAvoidanceRadius = 400.0f; // Avoid edges passing through resources
	
	int32 TotalEdges = 0;
	for (int32 i = 0; i < NavNodes.Num(); i++)
	{
		for (int32 j = i + 1; j < NavNodes.Num(); j++)
		{
			// Check angular distance
			float Angle = FMath::Acos(FMath::Clamp(
				FVector::DotProduct(NavNodes[i].Position, NavNodes[j].Position), -1.0f, 1.0f));
			
			if (Angle > MaxEdgeAngle) continue;
			
			// Verify the path between nodes doesn't cross water
			bool bPathClear = true;
			const int32 NumSamples = 3;
			for (int32 s = 1; s < NumSamples; s++)
			{
				float T = (float)s / NumSamples;
				FVector MidPoint = FMath::Lerp(NavNodes[i].Position, NavNodes[j].Position, T).GetSafeNormal();
				if (!IsPointOnLand(MidPoint))
				{
					bPathClear = false;
					break;
				}
			}
			
			if (!bPathClear) continue;
			
			// Check if edge passes through any city
			for (ACityActor* City : SpawnedCities)
			{
				if (!City) continue;
				
				FVector EdgeMidPoint = FMath::Lerp(NavNodes[i].WorldPosition, NavNodes[j].WorldPosition, 0.5f);
				float DistToCity = FVector::Dist(EdgeMidPoint, City->GetActorLocation());
				if (DistToCity < CityAvoidanceRadius)
				{
					bPathClear = false;
					break;
				}
			}
			
			if (!bPathClear) continue;
			
			// Check if edge passes through any resource
			for (AResourceActor* Resource : SpawnedResources)
			{
				if (!Resource) continue;
				
				FVector EdgeMidPoint = FMath::Lerp(NavNodes[i].WorldPosition, NavNodes[j].WorldPosition, 0.5f);
				float DistToResource = FVector::Dist(EdgeMidPoint, Resource->GetActorLocation());
				if (DistToResource < ResourceAvoidanceRadius)
				{
					bPathClear = false;
					break;
				}
			}
			
			if (bPathClear)
			{
				float Distance = Angle * PlanetRadius; // Arc length
				NavNodes[i].Neighbors.Add(FNavEdge(j, Distance));
				NavNodes[j].Neighbors.Add(FNavEdge(i, Distance));
				TotalEdges++;
			}
		}
	}
	
	// Landmass IDs are now assigned using Voronoi continent seeds during node creation
	// Count unique landmass IDs for logging
	TSet<int32> UniqueLandmasses;
	for (const FNavNode& Node : NavNodes)
	{
		if (Node.LandmassID >= 0)
		{
			UniqueLandmasses.Add(Node.LandmassID);
		}
	}
	LandmassCount = UniqueLandmasses.Num();
	
	UE_LOG(LogTemp, Log, TEXT("RegenerateNavigationEdges: %d nodes, %d edges, %d continents"), 
		NavNodes.Num(), TotalEdges, LandmassCount);
}

// ===== DEBUG VISUALIZATION =====

void APlanetActor::DebugDrawAllNavNodes()
{
	if (!GetWorld()) return;
	
	// Draw all nav nodes as RED boxes
	for (const FNavNode& Node : NavNodes)
	{
		DrawDebugBox(GetWorld(), Node.WorldPosition, FVector(50.0f), FColor::Red, false, 0.1f, 0, 5.0f);
	}
}

// ===== PATHFINDING (A* ALGORITHM) =====

bool APlanetActor::FindPath(FVector StartWorldPos, FVector EndWorldPos, TArray<FVector>& OutPath, bool bDebugLog, FString* OutFailureReason)
{
	OutPath.Empty();

	// ===== TARGET VALIDATION =====
	// Check if target is in water
	FVector TargetDir = (EndWorldPos - GetActorLocation()).GetSafeNormal();
	if (!IsPointOnLand(TargetDir))
	{
		if (bDebugLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("FindPath FAILED: Target is in water"));
		}
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("Target is in water");
		}
		return false;
	}

	// Check if target is too close to any city
	const float CityExclusionRadius = 800.0f;
	for (ACityActor* City : SpawnedCities)
	{
		if (!City) continue;
		float DistToCity = FVector::Dist(EndWorldPos, City->GetActorLocation());
		if (DistToCity < CityExclusionRadius)
		{
			//UE_LOG(LogTemp, Warning, TEXT("FindPath FAILED: Target too close to city (%s)"), *City->GetName());
			if (OutFailureReason)
			{
				*OutFailureReason = FString::Printf(TEXT("Too close to city %s (%.0f < 800 units)"), *City->GetName(), DistToCity);
			}
			return false;
		}
	}
	
	// ===== STEP 1: CHECK SAME CONTINENT =====
	FVector StartDir = (StartWorldPos - GetActorLocation()).GetSafeNormal();
	FVector EndDir = (EndWorldPos - GetActorLocation()).GetSafeNormal();

	// Use GetContinentIdForPoint directly on directions - this is authoritative and avoids
	// false -1 results from nav nodes placed on land/water boundaries
	int32 StartContinent = GetContinentIdForPoint(StartDir);
	int32 EndContinent = GetContinentIdForPoint(EndDir);
	
	// Find nearest nav nodes for pathfinding
	int32 StartNode = FindNearestNavNode(StartWorldPos);
	int32 EndNode = FindNearestNavNode(EndWorldPos);

	if (StartNode < 0 || EndNode < 0)
	{
		if (bDebugLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("FindPath FAILED: Could not find nav nodes"));
		}
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("Could not find navigation nodes");
		}
		return false;
	}

	// Check same continent using authoritative continent IDs (not nav node LandmassIDs which can be -1 on borders)
	// Only block if BOTH have valid continent IDs and they differ
	if (StartContinent >= 0 && EndContinent >= 0 && StartContinent != EndContinent)
	{
		if (bDebugLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("FindPath FAILED: Different continents (%d vs %d)"),
				StartContinent, EndContinent);
		}
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("Different landmasses (%d vs %d)"), 
				StartContinent, EndContinent);
		}
		return false;
	}
	
	// ===== STEP 2: IDENTIFY DESTINATION OWNER =====
	// Find which actor (if any) owns the destination point
	// This allows pathfinding to not avoid the destination actor's waypoints
	AActor* DestinationOwner = nullptr;
	float ClosestDistToDestination = 1000.0f; // Only consider actors within 1000 units
	
	for (ACityActor* City : SpawnedCities)
	{
		if (City && FVector::Dist(City->GetActorLocation(), EndWorldPos) < ClosestDistToDestination)
		{
			ClosestDistToDestination = FVector::Dist(City->GetActorLocation(), EndWorldPos);
			DestinationOwner = City;
		}
	}
	for (AResourceActor* Resource : SpawnedResources)
	{
		if (Resource && FVector::Dist(Resource->GetActorLocation(), EndWorldPos) < ClosestDistToDestination)
		{
			ClosestDistToDestination = FVector::Dist(Resource->GetActorLocation(), EndWorldPos);
			DestinationOwner = Resource;
		}
	}
	
	// ===== STEP 3: BUILD OBSTACLE PERIMETER MAP =====
	// Group waypoints by owner and store them in order (for perimeter traversal)
	TMap<AActor*, TArray<int32>> ObstacleWaypoints;
	
	for (int32 i = 0; i < NavNodes.Num(); i++)
	{
		if (NavNodes[i].Owner != nullptr)
		{
			ObstacleWaypoints.FindOrAdd(NavNodes[i].Owner).Add(i);
		}
	}
	
	// ===== STEP 4: LINE-OF-SIGHT PATHFINDING WITH OBSTACLE ROUTING =====
	
	FVector CurrentPos = StartWorldPos;
	FVector TargetPos = EndWorldPos;
	const int32 MaxIterations = 50; // Prevent infinite loops
	int32 Iteration = 0;
	
	OutPath.Add(CurrentPos);
	
	while (FVector::Dist(CurrentPos, TargetPos) > 50.0f && Iteration < MaxIterations)
	{
		Iteration++;
		
		// Check if there's clear line-of-sight to target
		bool bPathBlocked = false;
		AActor* BlockingObstacle = nullptr;
		int32 BlockingEdgeStart = -1;
		int32 BlockingEdgeEnd = -1;
		
		// Check each obstacle's perimeter edges for intersection with current→target arc
		for (const auto& Pair : ObstacleWaypoints)
		{
			AActor* ObstacleOwner = Pair.Key;
			const TArray<int32>& Waypoints = Pair.Value;
			
			// Skip the destination owner - we're TRYING to reach it, not avoid it
			if (ObstacleOwner == DestinationOwner)
			{
				continue;
			}
			
			// Need at least 2 waypoints to form edges
			if (Waypoints.Num() < 2) continue;
			
			// Quick rejection: check if obstacle is far from the path
			FVector ObstacleCenter = ObstacleOwner->GetActorLocation();
			float ObstacleRadius = 0.0f;
			if (Cast<AResourceActor>(ObstacleOwner))
			{
				ObstacleRadius = 400.0f / PlanetRadius; // Angular radius
			}
			else if (Cast<ACityActor>(ObstacleOwner))
			{
				ObstacleRadius = 1600.0f / PlanetRadius; // Angular radius
			}
			
			float DistToArc = AngularDistanceToArc(ObstacleCenter, CurrentPos, TargetPos);
			if (DistToArc > ObstacleRadius * 1.5f)
			{
				continue; // Obstacle too far from path
			}
			
			// Check each consecutive edge on the perimeter
			// First, check if CurrentPos is at one of this obstacle's waypoints
			int32 CurrentPosWaypointIndex = -1;
			for (int32 i = 0; i < Waypoints.Num(); i++)
			{
				if (FVector::Dist(CurrentPos, NavNodes[Waypoints[i]].WorldPosition) < 50.0f)
				{
					CurrentPosWaypointIndex = i;
					break;
				}
			}
			
			for (int32 i = 0; i < Waypoints.Num(); i++)
			{
				int32 NextIndex = (i + 1) % Waypoints.Num(); // Wrap around for closed perimeter
				
				// Skip edges adjacent to our starting position to avoid false positives
				if (CurrentPosWaypointIndex >= 0)
				{
					if (i == CurrentPosWaypointIndex || NextIndex == CurrentPosWaypointIndex)
					{
						continue; // Skip edges connected to our current position
					}
				}
				
				FVector EdgeStart = NavNodes[Waypoints[i]].WorldPosition;
				FVector EdgeEnd = NavNodes[Waypoints[NextIndex]].WorldPosition;
				
				// Check if current→target arc intersects this edge
				if (DoArcsIntersect(CurrentPos, TargetPos, EdgeStart, EdgeEnd))
				{
					bPathBlocked = true;
					BlockingObstacle = ObstacleOwner;
					BlockingEdgeStart = Waypoints[i];
					BlockingEdgeEnd = Waypoints[NextIndex];
					break;
				}
			}
			
			if (bPathBlocked) break; // Found blocking obstacle
		}
		
		if (!bPathBlocked)
		{
			// Clear path to target - we're done!
			OutPath.Add(TargetPos);
			break;
		}
		
		// ===== STEP 4: ROUTE AROUND BLOCKING OBSTACLE =====
		
		if (bDebugLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PATHFINDING] Path blocked by %s (edge %d→%d)"),
				*BlockingObstacle->GetName(), BlockingEdgeStart, BlockingEdgeEnd);
		}
		
		// Find the waypoint on the obstacle that is closest to the VEHICLE'S CURRENT POSITION
		const TArray<int32>& ObstaclePerimeter = ObstacleWaypoints[BlockingObstacle];
		int32 EntryWaypoint = ObstaclePerimeter[0];
		float MinDist = FVector::Dist(NavNodes[EntryWaypoint].WorldPosition, CurrentPos);
		
		for (int32 WaypointIndex : ObstaclePerimeter)
		{
			float Dist = FVector::Dist(NavNodes[WaypointIndex].WorldPosition, CurrentPos);
			if (Dist < MinDist)
			{
				MinDist = Dist;
				EntryWaypoint = WaypointIndex;
			}
		}
		
		CurrentPos = NavNodes[EntryWaypoint].WorldPosition;
		OutPath.Add(CurrentPos);
		
		if (bDebugLog)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PATHFINDING] Entering obstacle at waypoint %d"), EntryWaypoint);
		}
		
		// ===== STEP 5: WALK PERIMETER TOWARD DESTINATION =====
		
		// ObstaclePerimeter already retrieved in STEP 4
		int32 CurrentWaypointIndex = ObstaclePerimeter.IndexOfByKey(EntryWaypoint);
		
		// Determine which direction to walk (pick ONCE and stick with it)
		int32 NextIndex = (CurrentWaypointIndex + 1) % ObstaclePerimeter.Num();
		int32 PrevIndex = (CurrentWaypointIndex - 1 + ObstaclePerimeter.Num()) % ObstaclePerimeter.Num();
		
		FVector NextPos = NavNodes[ObstaclePerimeter[NextIndex]].WorldPosition;
		FVector PrevPos = NavNodes[ObstaclePerimeter[PrevIndex]].WorldPosition;
		
		float DistNext = FVector::Dist(NextPos, TargetPos);
		float DistPrev = FVector::Dist(PrevPos, TargetPos);
		
		int32 WalkDirection = (DistNext < DistPrev) ? 1 : -1; // +1 = forward, -1 = backward
		
		const int32 MaxPerimeterSteps = ObstaclePerimeter.Num(); // One full loop maximum
		for (int32 Step = 0; Step < MaxPerimeterSteps; Step++)
		{
			// Move to next waypoint in chosen direction
			CurrentWaypointIndex = (CurrentWaypointIndex + WalkDirection + ObstaclePerimeter.Num()) % ObstaclePerimeter.Num();
			CurrentPos = NavNodes[ObstaclePerimeter[CurrentWaypointIndex]].WorldPosition;
			
			OutPath.Add(CurrentPos);
			
			// Check if we can now see the target (check ALL obstacles, not just current one)
			bool bStillBlocked = false;
			
			for (const auto& CheckPair : ObstacleWaypoints)
			{
				AActor* CheckObstacle = CheckPair.Key;
				const TArray<int32>& CheckWaypoints = CheckPair.Value;
				
				// Skip the destination owner - we're TRYING to reach it
				if (CheckObstacle == DestinationOwner)
				{
					continue;
				}
				
				// Check if this obstacle blocks the path from current position to target
				for (int32 i = 0; i < CheckWaypoints.Num(); i++)
				{
					int32 Next = (i + 1) % CheckWaypoints.Num();
					FVector EdgeStart = NavNodes[CheckWaypoints[i]].WorldPosition;
					FVector EdgeEnd = NavNodes[CheckWaypoints[Next]].WorldPosition;
					
					if (DoArcsIntersect(CurrentPos, TargetPos, EdgeStart, EdgeEnd))
					{
						bStillBlocked = true;
						break;
					}
				}
				
				if (bStillBlocked) break; // Found blocking obstacle, no need to check others
			}
			
			if (!bStillBlocked)
			{
				if (bDebugLog)
				{
					UE_LOG(LogTemp, Warning, TEXT("[PATHFINDING] Cleared all obstacles at waypoint %d (step %d)"),
						ObstaclePerimeter[CurrentWaypointIndex], Step + 1);
				}
				break; // Exited the obstacle, continue outer loop
			}
		}
		
		// SAFETY CHECK: If we walked the entire perimeter without finding clear line-of-sight,
		// we're stuck (target is surrounded or unreachable from this angle)
		// Prevent infinite loop by checking if we made progress
		float DistanceAfterWalk = FVector::Dist(CurrentPos, TargetPos);
		float DistanceBeforeWalk = FVector::Dist(NavNodes[EntryWaypoint].WorldPosition, TargetPos);
		
		if (DistanceAfterWalk >= DistanceBeforeWalk * 0.95f) // Less than 5% progress
		{
			if (bDebugLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PATHFINDING] Stuck after walking perimeter - no progress made"));
			}
			// Give up on this path - return what we have so far or fail
			break;
		}
	}
	
	// Draw debug visualization
	if (bDebugDrawNavWaypoints)
	{
		for (const FVector& Waypoint : OutPath)
		{
			DrawDebugSphere(GetWorld(), Waypoint, 80.0f, 8, FColor::Yellow, false, 10.0f);
		}
	}
	
	if (bDebugLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PATHFINDING] Path complete: %d waypoints"), OutPath.Num());
	}
	
	return OutPath.Num() > 0;
}

float APlanetActor::HeuristicDistance(int32 NodeA, int32 NodeB) const
{
	if (NodeA < 0 || NodeA >= NavNodes.Num() || NodeB < 0 || NodeB >= NavNodes.Num())
		return FLT_MAX;

	float Dot = FVector::DotProduct(NavNodes[NodeA].Position, NavNodes[NodeB].Position);
	return FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)) * PlanetRadius;
}

int32 APlanetActor::FindNearestNavNode(FVector WorldPos) const
{
	FVector Direction = (WorldPos - GetActorLocation()).GetSafeNormal();

	// Build list of candidate nodes sorted by angular distance
	TArray<TPair<int32, float>> Candidates;
	for (int32 i = 0; i < NavNodes.Num(); i++)
	{
		float Dot = FVector::DotProduct(Direction, NavNodes[i].Position);
		Candidates.Add(TPair<int32, float>(i, Dot));
	}
	
	// Sort by dot product (higher = closer)
	Candidates.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) {
		return A.Value > B.Value;
	});
	
	// Try to find the nearest node with clear line-of-sight
	int32 MaxCandidates = FMath::Min(10, Candidates.Num());
	for (int32 i = 0; i < MaxCandidates; i++)
	{
		int32 NodeIndex = Candidates[i].Key;
		if (HasClearLineOfSight(WorldPos, NavNodes[NodeIndex].WorldPosition))
		{
			return NodeIndex;
		}
	}
	
	// No LOS - return closest node (pathfinding will route around obstacles)
	return Candidates.Num() > 0 ? Candidates[0].Key : -1;
}
