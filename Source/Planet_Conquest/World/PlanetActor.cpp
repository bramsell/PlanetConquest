// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlanetActor.h"
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
#include "../Core/PlanetConquestPlayerController.h"
#include "../Core/AITeamController.h"

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

	// Spawn territory resources around each city
	SpawnResourcesAroundPlanet();

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
	
	// Find which AI teams actually have cities
	TSet<EOwnerTeam> TeamsWithCities;
	for (ACityActor* City : SpawnedCities)
	{
		if (City && City->OwnerTeam != EOwnerTeam::Player && City->OwnerTeam != EOwnerTeam::Neutral)
		{
			TeamsWithCities.Add(City->OwnerTeam);
		}
	}
	
	// Spawn one AI controller for each team that has cities
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
				AIController->FinishSpawning(FTransform::Identity);
				
				AIControllers.Add(AIController);
				
				UE_LOG(LogTemp, Log, TEXT("Spawned AITeamController for AI Team %d"), (int32)Team);
			}
		}
	}

	// Generate minimap texture for HUD
	GenerateMinimapTexture(512, 256);
	UE_LOG(LogTemp, Log, TEXT("Minimap texture auto-generated at startup"));
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
	// This creates oblong, irregular continent shapes
	FVector ShapeWarpedPoint = DomainWarp(SpherePoint, ContinentShapeStrength, 1.5f, NoiseSeed);

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
	// Near boundaries, make detail warp only subtractive (can only shrink continent)
	// Far from boundaries, allow full bidirectional warping
	FVector DetailWarpedPoint;
	
	if (DistToBoundary < MinSeparationRadians * 3.0f)
	{
		// Near boundary: apply constrained warp
		// Calculate how much warp would push us toward/away from seed
		FVector PotentialWarp = DomainWarp(ShapeWarpedPoint, CoastlineDetailStrength, 8.0f, NoiseSeed + 100);
		float WarpedDist = AngularDistance(PotentialWarp, NearestSeed->Position);
		
		// Only allow warp if it increases distance (makes continent smaller)
		if (WarpedDist > NearestDist)
		{
			DetailWarpedPoint = PotentialWarp;
		}
		else
		{
			// Reject warp, use shape-warped point as-is
			DetailWarpedPoint = ShapeWarpedPoint;
		}
	}
	else
	{
		// Far from boundary: full bidirectional detail warp
		DetailWarpedPoint = DomainWarp(ShapeWarpedPoint, CoastlineDetailStrength, 8.0f, NoiseSeed + 100);
	}

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

	// Add detail noise if enabled (only on land or near-land areas)
	if (DetailNoiseStrength > 0.0f && DistFromCoastline > -OceanBlendEnd)
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

	// Calculate water radius (sea level)
	// Make slightly smaller than planet base + sea level to avoid z-fighting
	float WaterRadius = PlanetRadius * (1.0f + SeaLevel) * 0.999f;

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
	
	// Compare to sea level (with small threshold for safety)
	// Height is relative to BaseHeight, so land is BaseHeight + SeedHeight
	// Sea level is at BaseHeight + SeaLevel
	float LandThreshold = BaseHeight + SeaLevel + 0.001f; // Small buffer above sea level
	
	bool bIsLand = Height > LandThreshold;
	
	// Log first few checks for debugging
	static int32 CheckCount = 0;
	if (CheckCount < 5)
	{
		UE_LOG(LogTemp, Log, TEXT("IsPointOnLand: Height=%.4f, Threshold=%.4f, Result=%s"),
			Height, LandThreshold, bIsLand ? TEXT("LAND") : TEXT("OCEAN"));
		CheckCount++;
	}
	
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
	const float InsetDistance = CityRadius; // 5000 units inland from coast
	
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
		const float InwardAngularStep = 0.005f; // ~0.3 degrees per step
		
		for (float InwardDist = InwardAngularStep; InwardDist < SamplingAngularRadius; InwardDist += InwardAngularStep)
		{
			float CurrentAngularDist = SamplingAngularRadius - InwardDist;
			FQuat InwardRotation = FQuat(RotationAxis, CurrentAngularDist);
			FVector TestPoint = InwardRotation.RotateVector(ContinentCenter);
			TestPoint.Normalize();
			
			bool bOnLand = IsPointOnLand(TestPoint);
			
			if (bOnLand)
			{
				// Check if it's our target continent
				int32 TestContinentId = GetContinentIdForPoint(TestPoint);
				if (TestContinentId == TargetContinentId)
				{
					// Found the coastline!
					CoastPoint = TestPoint;
					bFoundCoast = true;
					break;
				}
				else
				{
					// Hit a different continent - not a valid coast for us
					break;
				}
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
		
		// ===== STEP 4: Check that all points within 3000 units are land =====
		
		bool bValidTerritory = true;
		const int32 NumChecks = 8;
		float TerritoryAngularRadius = 3000.0f / PlanetRadius; // Reduced from 4500 to generate more candidates
		
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
			
			if (GetContinentIdForPoint(CheckPoint) != TargetContinentId)
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

	// Find target continent (largest land mass or random if not placing on same continent)
	int32 TargetContinentId = -1;
	if (bPlaceOnSameContinent)
	{
		// Find largest land continent by size
		float LargestSize = 0.0f;
		for (int32 i = 0; i < ContinentSeeds.Num(); i++)
		{
			if (ContinentSeeds[i].bIsLand && ContinentSeeds[i].Size > LargestSize)
			{
				LargestSize = ContinentSeeds[i].Size;
				TargetContinentId = i;
			}
		}

		if (TargetContinentId < 0)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnCities: No land continents found!"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("SpawnCities: Target continent %d (size %.2f) at position %s"),
			TargetContinentId, ContinentSeeds[TargetContinentId].Size, *ContinentSeeds[TargetContinentId].Position.ToString());
	}

	// Pre-generate coastal spawn candidates for coastal cities (first 6)
	TArray<FVector> CoastalCandidates;
	if (NumCitiesToSpawn > 0 && TargetContinentId >= 0)
	{
		GenerateCoastalCandidates(TargetContinentId, CoastalCandidates);
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
			// Cities 1-6 are coastal (6 cities), Cities 0, 7, 8 are landlocked (3 cities)
			bool bRequireCoastal = (CityIndex >= 1 && CityIndex <= 6);
			bool bRequireLandlocked = (CityIndex == 0 || CityIndex >= 7);
			int32 UsedCandidateIndex = -1; // Track which coastal candidate was used

			// For coastal cities, use pre-generated candidates
			if (bRequireCoastal)
			{
				// Regenerate if running low on candidates (likely all too close to existing cities)
				if (CoastalCandidates.Num() < 3 && Attempt > 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("SpawnCities: Regenerating coastal candidates (had %d left, attempt %d)"), 
						CoastalCandidates.Num(), Attempt);
					CoastalCandidates.Empty();
					GenerateCoastalCandidates(TargetContinentId, CoastalCandidates);
				}
				
				if (CoastalCandidates.Num() == 0)
				{
					UE_LOG(LogTemp, Error, TEXT("SpawnCities: No coastal candidates available for city %d"), CityIndex);
					break; // Can't spawn this city
				}
				
				// Pick random coastal candidate
				UsedCandidateIndex = RNG2.RandRange(0, CoastalCandidates.Num() - 1);
				SpawnDirection = CoastalCandidates[UsedCandidateIndex];
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
					FVector ContinentDir = ContinentSeeds[TargetContinentId].Position;
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
					if (ContinentId != TargetContinentId)
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
						(GetContinentIdForPoint(SampleDirection) == TargetContinentId) :
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
					CoastalCandidates.RemoveAt(UsedCandidateIndex);
				}
				
				continue;
			}

			// Success!
			bValidLocationFound = true;
			const TCHAR* CityType = (CityIndex >= 1 && CityIndex <= 6) ? TEXT("COASTAL") : TEXT("LANDLOCKED");
			UE_LOG(LogTemp, Log, TEXT("SpawnCities: Found valid %s location for city %d on attempt %d (terrain radius=%.1f)"), 
				CityType, CityIndex, Attempt + 1, ActualTerrainRadius);
			
			// Remove used coastal candidate to avoid reuse
			if (CityIndex < 6 && UsedCandidateIndex >= 0)
			{
				CoastalCandidates.RemoveAt(UsedCandidateIndex);
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
				// Assign to AI teams (AI1-AI8)
				// CityIndex 1 = AI1, 2 = AI2, etc.
				int32 AIIndex = CityIndex; // 1-8
				switch (AIIndex)
				{
					case 1: CityTeam = EOwnerTeam::AI1; break;
					case 2: CityTeam = EOwnerTeam::AI2; break;
					case 3: CityTeam = EOwnerTeam::AI3; break;
					case 4: CityTeam = EOwnerTeam::AI4; break;
					case 5: CityTeam = EOwnerTeam::AI5; break;
					case 6: CityTeam = EOwnerTeam::AI6; break;
					case 7: CityTeam = EOwnerTeam::AI7; break;
					case 8: CityTeam = EOwnerTeam::AI8; break;
					default: CityTeam = EOwnerTeam::AI1; break; // Fallback for more than 9 cities
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

void APlanetActor::SpawnResourcesAroundPlanet()
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
	float MinDistanceBetweenResources = 500.0f; // Minimum distance between resources
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
				bBlackIsScarce = FMath::RandBool();
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
				bBlackIsScarce = FMath::RandBool();
			}
			
			// Determine exact counts
			int32 GreenCount = FMath::RandRange(1, 2);
			int32 ScarceCount = FMath::RandRange(1, 2);
			int32 AbundantCount = FMath::RandRange(5, 6);
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
					float DistanceFromCity = FMath::FRandRange(3000.0f, 5000.0f);

					// Create tangent vectors for random direction around city
					FVector Tangent1 = FVector::CrossProduct(CityDirection, FVector::UpVector).GetSafeNormal();
					if (Tangent1.IsNearlyZero())
					{
						Tangent1 = FVector::CrossProduct(CityDirection, FVector::RightVector).GetSafeNormal();
					}
					FVector Tangent2 = FVector::CrossProduct(CityDirection, Tangent1).GetSafeNormal();

					// Random angle around the city
					float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);

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
							IncomeValue = Choices[FMath::RandRange(0, 2)];
						}
						else if (ResourceType == ScarceType)
						{
							// Scarce substrate
							if (ScarceCount == 1)
							{
								// If only 1 scarce resource, must be at least 25, max 40
								int32 Choices[] = {25, 40};
								IncomeValue = Choices[FMath::RandRange(0, 1)];
							}
							else
							{
								// If 2 scarce resources, can be anything up to 40
								int32 Choices[] = {10, 25, 40};
								IncomeValue = Choices[FMath::RandRange(0, 2)];
							}
						}
						else
						{
							// Abundant substrate: can be anything up to 40
							int32 Choices[] = {10, 25, 40};
							IncomeValue = Choices[FMath::RandRange(0, 2)];
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
	
	// Find the target continent (largest land continent)
	int32 TargetContinentId = -1;
	float LargestSize = 0.0f;
	for (int32 i = 0; i < ContinentSeeds.Num(); i++)
	{
		if (ContinentSeeds[i].bIsLand && ContinentSeeds[i].Size > LargestSize)
		{
			LargestSize = ContinentSeeds[i].Size;
			TargetContinentId = i;
		}
	}
	
	if (TargetContinentId < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No land continent found for cluster spawning"));
	}
	else
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
			float Roll = FMath::FRand();
			if (Roll < 0.60f) // 60% chance
			{
				ResourcesInCluster = FMath::RandRange(2, 3); // 2 or 3 resources (most common)
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
				float RandomAngleDegrees = FMath::FRandRange(0.0f, 40.0f);
				float RandomAngleRadians = FMath::DegreesToRadians(RandomAngleDegrees);
				
				// Create tangent vectors
				FVector Tangent1 = FVector::CrossProduct(ContinentCenter, FVector::UpVector).GetSafeNormal();
				if (Tangent1.IsNearlyZero())
				{
					Tangent1 = FVector::CrossProduct(ContinentCenter, FVector::RightVector).GetSafeNormal();
				}
				FVector Tangent2 = FVector::CrossProduct(ContinentCenter, Tangent1).GetSafeNormal();
				
				// Random rotation around continent center
				float RandomRotation = FMath::FRandRange(0.0f, 2.0f * PI);
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
			EResourceType ClusterType = (FMath::FRand() < 0.75f) ? EResourceType::OrangeSubstrate : EResourceType::BlackSubstrate;
			
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
				float OffsetDistance = FMath::FRandRange(0.0f, ResourceSpacingInCluster);
				float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
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
						if (Distance < MinDistanceBetweenResources)
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
					int32 IncomeValue = Choices[FMath::RandRange(0, 2)];
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
	}

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
