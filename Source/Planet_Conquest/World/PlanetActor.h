// Copyright Benjamin Ramsell. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlanetActor.generated.h"

// Continent seed data for Voronoi-based continent generation
USTRUCT(BlueprintType)
struct FContinentSeed
{
	GENERATED_BODY()

	// Position on unit sphere (normalized vector)
	UPROPERTY()
	FVector Position;

	// Size multiplier for this continent (0.5 - 1.5)
	UPROPERTY()
	float Size;

	// Coastline roughness/irregularity (0.0 - 1.0)
	UPROPERTY()
	float Roughness;

	// Whether this seed is land (true) or ocean (false)
	UPROPERTY()
	bool bIsLand;

	FContinentSeed()
		: Position(FVector::ZeroVector)
		, Size(1.0f)
		, Roughness(0.5f)
		, bIsLand(true)
	{}

	FContinentSeed(FVector InPosition, float InSize, float InRoughness, bool bInIsLand = true)
		: Position(InPosition)
		, Size(InSize)
		, Roughness(InRoughness)
		, bIsLand(bInIsLand)
	{}
};

// Navigation graph edge connecting two nodes
USTRUCT()
struct FNavEdge
{
	GENERATED_BODY()

	UPROPERTY()
	int32 TargetIndex;

	UPROPERTY()
	float Distance;

	FNavEdge()
		: TargetIndex(-1)
		, Distance(0.0f)
	{}

	FNavEdge(int32 InTarget, float InDist)
		: TargetIndex(InTarget)
		, Distance(InDist)
	{}
};

// Navigation graph node on planet surface
USTRUCT()
struct FNavNode
{
	GENERATED_BODY()

	// Unit sphere direction (normalized)
	UPROPERTY()
	FVector Position;

	// Actual world position on planet surface
	UPROPERTY()
	FVector WorldPosition;

	// Node index in NavNodes array
	UPROPERTY()
	int32 Index;

	// Landmass ID for connectivity checking (nodes with same ID are connected)
	UPROPERTY()
	int32 LandmassID;

	// Owning actor (for dynamic waypoints from resources/cities, null for static waypoints)
	// Not serialized - rebuilt dynamically
	AActor* Owner;

	// Edges to neighboring nodes
	UPROPERTY()
	TArray<FNavEdge> Neighbors;

	FNavNode()
		: Position(FVector::ZeroVector)
		, WorldPosition(FVector::ZeroVector)
		, Index(-1)
		, LandmassID(-1)
		, Owner(nullptr)
	{}
};

UCLASS()
class PLANET_CONQUEST_API APlanetActor : public AActor
{
	GENERATED_BODY()
	
public:	
	APlanetActor();

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:	
	virtual void Tick(float DeltaTime) override;

	// Regenerate planet with current parameters (callable from editor)
	UFUNCTION(CallInEditor, Category = "Planet")
	void RegeneratePlanet();

	// Generate equirectangular minimap texture from planet height data
	UFUNCTION(BlueprintCallable, Category = "Planet|Minimap")
	UTexture2D* GenerateMinimapTexture(int32 TextureWidth = 512, int32 TextureHeight = 256);

	// Convert minimap UV coordinates (0-1) to world position on planet surface
	UFUNCTION(BlueprintCallable, Category = "Planet|Minimap")
	FVector MinimapUVToWorldPosition(FVector2D UV);

	// Convert world position to minimap UV coordinates (inverse of above)
	UFUNCTION(BlueprintCallable, Category = "Planet|Minimap")
	FVector2D WorldPositionToMinimapUV(FVector WorldPosition);

	// Calculate height at a sphere point using continent seeding (public for use by entities)
	float CalculateHeightAtPoint(const FVector& SpherePoint) const;

	// Check if a point on the unit sphere is above sea level (public for vehicle pathfinding)
	bool IsPointOnLand(const FVector& SpherePoint) const;

	// Calculate angular distance between two points on unit sphere (public for vehicle volcano avoidance)
	float AngularDistance(const FVector& A, const FVector& B) const;

	// Check if there's clear line-of-sight between two world positions (no water, cities, or resources blocking)
	bool HasClearLineOfSight(const FVector& StartPos, const FVector& EndPos) const;
	
	// Helper: Check if two great-circle arcs intersect on the unit sphere
	bool DoArcsIntersect(const FVector& Arc1Start, const FVector& Arc1End, const FVector& Arc2Start, const FVector& Arc2End) const;
	
	// Helper: Calculate angular distance from a point to a great-circle arc (for quick rejection)
	float AngularDistanceToArc(const FVector& Point, const FVector& ArcStart, const FVector& ArcEnd) const;

	// ===== NAVIGATION GRAPH FUNCTIONS =====

	// Generate navigation graph for vehicle pathfinding (called at startup after terrain generation)
	void GenerateNavGraph();
	
	// Debug visualization: Draw all nav nodes
	void DebugDrawAllNavNodes();

	// Register a navigation waypoint dynamically (for resources, cities, etc)
	// Returns the index of the registered waypoint
	int32 RegisterNavigationWaypoint(FVector WorldPosition, AActor* OwnerActor = nullptr);

	// Unregister a navigation waypoint by index
	void UnregisterNavigationWaypoint(int32 WaypointIndex, AActor* OwnerActor = nullptr);

	// Regenerate navigation edges after waypoints are added/removed
	void RegenerateNavigationEdges();

	// Find path from start to end position using A* pathfinding
	// Returns true if path found, false if unreachable (different landmass, in water, etc)
	// OutFailureReason will contain specific reason for failure if provided
	bool FindPath(FVector StartWorldPos, FVector EndWorldPos, TArray<FVector>& OutPath, bool bDebugLog = false, FString* OutFailureReason = nullptr);

	// Calculate great-circle distance heuristic between two nav nodes (for A* pathfinding)
	float HeuristicDistance(int32 NodeA, int32 NodeB) const;

	// Find nearest navigation node to a world position
	int32 FindNearestNavNode(FVector WorldPos) const;

	// ===== PUBLIC MEMBERS =====

	// Continent seeds (public for camera positioning)
	TArray<FContinentSeed> ContinentSeeds;

	// Navigation graph nodes (for vehicle pathfinding)
	UPROPERTY()
	TArray<FNavNode> NavNodes;

	// Number of distinct landmasses (for connectivity checking)
	UPROPERTY()
	int32 LandmassCount;

	// ===== COMPONENTS =====
	
	// Planet mesh component (cube sphere)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
	class UProceduralMeshComponent* PlanetMesh;

	// Directional light (sun)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
	class UDirectionalLightComponent* SunLight;

	// Fill light (illuminates opposite side)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
	class UDirectionalLightComponent* FillLight;

	// Water sphere (visualizes sea level)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
	class UProceduralMeshComponent* WaterSphereMesh;

	// Skybox sphere (starfield background)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet")
	class UStaticMeshComponent* SkyboxMesh;

	// ===== DAY/NIGHT CYCLE =====
	
	// Speed of day/night cycle in degrees per second (360 = full cycle in 1 second)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night Cycle")
	float DayNightCycleSpeed = 0.5f; //720 seconds for full cycle at 0.5 deg/sec
	
	// Current rotation angle for day/night cycle
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day/Night Cycle")
	float CurrentDayNightRotation = 0.0f;

	// ===== PLANET PROPERTIES =====
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	float PlanetRadius = 80000.0f;

	// Resolution of each cube face (32-384 recommended, higher values may crash Live Coding)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet", meta = (ClampMin = "2", ClampMax = "512"))
	int32 Resolution = 256;

	// Material for the planet surface
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	class UMaterialInterface* PlanetMaterial;

	// Material for volcano areas (blended via vertex alpha)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	class UMaterialInterface* BasaltMaterial;

	// Material for the skybox (starfield)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	class UMaterialInterface* SkyboxMaterial;

	// Material for the water sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	class UMaterialInterface* WaterMaterial;

	// Generated minimap texture (equirectangular projection)
	UPROPERTY(BlueprintReadOnly, Category = "Planet|Minimap")
	class UTexture2D* MinimapTexture;

	// Center point for minimap (to center on player's continent)
	FVector MinimapCenterPoint = FVector::UpVector;

	// ===== NOISE SETTINGS (Phase 2) =====

	// Noise scale - controls continent size (smaller = larger continents)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Noise", meta = (ClampMin = "0.0001", ClampMax = "1.0"))
	float NoiseScale = 0.001f;

	// Number of noise octaves for detail
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	// Noise persistence (how much each octave contributes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NoisePersistence = 0.5f;

	// Random seed for noise generation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Noise")
	int32 NoiseSeed = 0;

	// Detail noise strength for terrain variation (0.0 - 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DetailNoiseStrength = 0.0f;

	// ===== HEIGHT SYSTEM =====

	// Base planet height as percentage of radius (0.0 = base radius, negative = below, positive = above)
	// Controls the elevation of the entire planet surface before continent offsets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Height", meta = (ClampMin = "-0.2", ClampMax = "0.2"))
	float BaseHeight = 0.0f;

	// Continent seed height offset as percentage of radius
	// Positive = continents rise above base (ocean planet), Negative = continents sink below base (acid pools)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Height", meta = (ClampMin = "-0.2", ClampMax = "0.2"))
	float SeedHeight = 0.015f;

	// Sea level as percentage of planet radius (where water sphere sits)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Height", meta = (ClampMin = "-0.1", ClampMax = "0.1"))
	float SeaLevel = 0.01f;

	// Show/hide water sphere for debugging
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Height")
	bool bShowWaterSphere = true;

	// Water sphere resolution (lower = better performance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Height", meta = (ClampMin = "8", ClampMax = "64"))
	int32 WaterSphereResolution = 16;

	// ===== CONTINENT SEEDING SETTINGS (Voronoi-based) =====

	// Number of Voronoi cells to divide sphere into (more cells = smaller pieces, more varied coastlines)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "4", ClampMax = "12"))
	int32 NumVoronoiCells = 6;

	// Number of cells that are raised land (rest are ocean)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "1", ClampMax = "6"))
	int32 NumLandCells = 4;

	// How spread out land cells are from each other
	// 0.0 = all land cells clustered together (supercontinent)
	// 1.0 = land cells as spread out as possible
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LandSpread = 0.0f;

	// Large-scale continent shape deformation (creates oblong, irregular continent shapes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ContinentShapeStrength = 0.7f;

	// Fine-scale coastline detail (textures coastlines with organic curves and inlets)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CoastlineDetailStrength = 0.4f;

	// Hemisphere bias for land distribution
	// 0.5 = evenly distributed, 0.0 = northern hemisphere, 1.0 = southern hemisphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HemisphereBias = 0.5f;

	// Base continent radius in radians (controls how large land masses are)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float ContinentRadius = 0.6f;

	// Minimum separation between continent borders in world units (guarantees ocean channels)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float MinContinentSeparation = 10000.0f;

	// Coastline blend distance in radians (creates smooth slopes at land/water transitions)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Continents", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CoastlineBlendDistance = 0.05f;

	// ===== VOLCANO SETTINGS =====

	// Heightmap texture for volcano stamping (baked from Volcano.fbx)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes")
	class UTexture2D* VolcanoHeightmap;

	// Radius of volcano stamp area in world units
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes", meta = (ClampMin = "1000.0", ClampMax = "50000.0"))
	float VolcanoRadius = 10000.0f;

	// Height multiplier for volcano (0.0 = no elevation, 1.0 = full height from texture)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolcanoHeightMultiplier = 1.0f;

	// Blend mode at volcano edges (distance in world units to fade from full height to zero)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float VolcanoEdgeFalloff = 2000.0f;

	// Whether to place a volcano at the center of each land continent
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes")
	bool bPlaceVolcanoes = true;

	// Fraction of volcano radius to apply basalt material (0.5 = basalt only on upper half of volcano)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float BasaltRadiusMultiplier = 0.7f;

	// Noise scale for irregular basalt edges (higher = more variation)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Volcanoes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BasaltNoiseStrength = 0.3f;

	// Positions of spawned volcanoes (stored for city spawn validation)
	UPROPERTY()
	TArray<FVector> VolcanoPositions;

	// ===== CITY SPAWNING SETTINGS =====

	// Enable spawning cities on the planet
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities")
	bool bSpawnCities = true;

	// Total number of cities to spawn (player + AI)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities", meta = (ClampMin = "1", ClampMax = "20", EditCondition = "bSpawnCities"))
	int32 NumCitiesToSpawn = 20;

	// Place all cities on the same continent (recommended for gameplay balance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities", meta = (EditCondition = "bSpawnCities"))
	bool bPlaceOnSameContinent = true;

	// Minimum distance between cities in world units
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities", meta = (ClampMin = "1000.0", ClampMax = "50000.0", EditCondition = "bSpawnCities"))
	float MinCityDistance = 20000.0f;

	// City radius - all points within this radius must be above sea level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities", meta = (ClampMin = "1000.0", ClampMax = "10000.0", EditCondition = "bSpawnCities"))
	float CityRadius = 5000.0f;

	// Maximum attempts to find valid city spawn location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities", meta = (ClampMin = "10", ClampMax = "1000", EditCondition = "bSpawnCities"))
	int32 MaxCitySpawnAttempts = 100;

	// Minimum distance from volcano centers when spawning cities (volcano radius + this value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Cities", meta = (ClampMin = "0.0", ClampMax = "20000.0", EditCondition = "bSpawnCities"))
	float MinDistanceFromVolcano = 5000.0f;

	// ===== RESOURCE SPAWNING SETTINGS =====

	// Number of resources spawned near cities at start (7-10 per city territory)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Resources", meta = (ClampMin = "0", ClampMax = "100"))
	int32 NumberOfResourcesAtStart = 45;

	// Get the continent ID (index into ContinentSeeds) that a point belongs to
	// Returns -1 if point is in ocean
	int32 GetContinentIdForPoint(const FVector& SpherePoint) const;

private:
	// Spawned cities
	UPROPERTY()
	TArray<class ACityActor*> SpawnedCities;

	// Spawned resources
	UPROPERTY()
	TArray<class AResourceActor*> SpawnedResources;

	// AI Controllers (one per AI team)
	UPROPERTY()
	TArray<class AAITeamController*> AIControllers;

	// Generate the cube sphere mesh
	void GenerateCubeSphere();
	
	// Generate vertices for a single cube face
	void GenerateFace(const FVector& LocalUp, TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector2D>& UVs, TArray<FVector>& BasePositions);

	// Sample noise at a given point (returns 0-1)
	float SampleNoise(const FVector& Point) const;

	// ===== CONTINENT SEEDING FUNCTIONS =====

	// Place continent seeds on sphere with even distribution
	void PlaceContinentSeeds();

	// Place N points evenly distributed on sphere (Fibonacci sphere)
	TArray<FVector> PlaceEvenlyDistributed(int32 NumPoints, FRandomStream& RNG);

	// Select which cells are land based on spread parameter
	TArray<int32> SelectLandCells(const TArray<FVector>& Cells, int32 NumLand, float Spread, FRandomStream& RNG);

	// Check if a water point is ocean (true) or lake (false)
	// Only valid for points where IsPointOnLand() returns false
	bool IsWaterPointOcean(const FVector& SpherePoint) const;

	// Apply domain warp to a position for organic shapes
	FVector DomainWarp(const FVector& Point, float WarpStrength, float WarpScale, int32 Seed) const;

	// ===== VOLCANO STAMPING =====

	// Stamp volcano heightmap onto the planet at specified positions
	void StampVolcanoes();

	// Sample height from volcano texture at a given UV coordinate (0-1 range)
	float SampleVolcanoHeight(float U, float V) const;

	// Generate water sphere mesh
	void GenerateWaterSphere();

	// Generate coastal spawn candidates by ray-casting from continent center
	void GenerateCoastalCandidates(int32 TargetContinentId, TArray<FVector>& OutCandidates);

	// Spawn cities on the planet surface
	void SpawnCities();

	// Spawn territory resources around cities
	// RNG must be seeded from NoiseSeed so resource placement is deterministic on load.
	void SpawnResourcesAroundPlanet(FRandomStream& RNG);
};
