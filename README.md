# Planet Conquest

A real-time strategy game built in Unreal Engine 5 featuring procedurally generated spherical planets, AI-controlled factions, and dynamic territory control.

## Overview

Planet Conquest is a 3D RTS where players compete for dominance on a procedurally generated spherical planet. The game features realistic planetary terrain with continents, oceans, volcanoes, and resource-rich zones. Eight AI-controlled factions build cities, gather resources, construct buildings, and wage war for territorial control.

## Key Features

- **Procedural Planet Generation**: Spherical worlds with Voronoi-based continents, realistic coastlines, and volcanic regions
- **Dynamic Terrain**: Height-based displacement, perlin noise detail, and shader-based lava pools at volcano peaks
- **Intelligent AI**: Decision-tree based AI for 8 teams with strategic planning, diplomacy, and combat
- **Vehicle Pathfinding**: Spherical surface navigation with obstacle avoidance, coastline-following for water, and resource targeting
- **Territory Management**: Cities with 5000-unit territories, building construction (factories, labs, mines, turrets), and resource gathering
- **Day/Night Cycle**: Rotating directional lights simulating planetary day/night cycles
- **UMG Interface**: HUD with minimap, resource tracking, diplomacy system, and city management

## Folder Structure

```
Planet_Conquest/
├── Source/                      # C++ source code
│   └── Planet_Conquest/
│       ├── Camera/              # Camera pawn and controls
│       ├── Core/                # Game mode, player controller, HUD, AI controller
│       ├── Entities/            # Game entities
│       │   ├── Buildings/       # Building actors (capital, factory, lab, mine, turret)
│       │   ├── Cities/          # City management
│       │   ├── Kaiju/           # Large creature entities
│       │   ├── Projectiles/     # Projectile physics
│       │   ├── Resources/       # Resource nodes (orange/black substrate)
│       │   └── Vehicles/        # Vehicle movement and AI
│       ├── UI/                  # UMG widget classes
│       └── World/               # Planet generation and management
│
├── Config/                      # Unreal Engine configuration files
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   └── DefaultInput.ini
│
├── Content/                     # Unreal Engine assets
│   ├── Materials/               # Custom materials (planet terrain, ocean, lava)
│   ├── BlenderAssets/           # 3D models and Blender scripts
│   ├── UI/                      # UMG widgets and UI assets
│   ├── ArrowTrail/              # VFX assets
│   └── StarterContent/Materials/# Unreal starter materials (basalt, rock)
│
└── Planet_Conquest.uproject     # Unreal Engine project file
```

## Technical Details

### Technologies
- **Engine**: Unreal Engine 5.3+
- **Language**: C++ (Unreal C++)
- **Graphics**: ProceduralMeshComponent for runtime planet generation
- **UI**: UMG (Unreal Motion Graphics)
- **Assets**: Blender for 3D modeling, Python for procedural generation scripts

### Core Systems

**Planet Generation** (`PlanetActor.cpp`):
- Cube sphere subdivision for even vertex distribution
- Voronoi cell-based continent placement with Fibonacci sphere distribution
- Domain warping for irregular coastlines
- Volcano stamping using heightmap textures
- Dual-material blending (terrain + volcanic basalt)

> **Attribution**: The spherical planet generation technique (cube-sphere subdivision, noise-based heightmap layering) is based on the method described by **Sebastian Lague** in his [Coding Adventures: Procedural Moons and Planets](https://www.youtube.com/watch?v=lctXaT9pxA0) series. His open approach to sharing these techniques made this project possible.

**Vehicle AI** (`VehicleActor.cpp`):
- Angular math for spherical pathfinding
- Resource targeting with priority queuing
- Coastline-following behavior for water avoidance
- Stuck detection and backup maneuvers
- Team coordination for resource gathering

**AI Decision System** (`AITeamController.cpp`):
- Income layer: Resource gathering, mine construction
- Military layer: Turret placement, army management
- Diplomatic layer: Trade, alliances, warfare
- Strategic layer: Expansion planning, long-term goals

**City Management** (`CityActor.cpp`):
- Territory control (5000 unit radius)
- Building queue system
- Resource storage and processing
- Population growth mechanics

## Building and Running

### Prerequisites
- Unreal Engine 5.3 or later
- Visual Studio 2022 with C++ development tools
- Windows 10/11 (primary development platform)

### Setup
1. Clone the repository
2. Right-click `Planet_Conquest.uproject` → Generate Visual Studio project files
3. Open `Planet_Conquest.sln` in Visual Studio
4. Build solution (Development Editor configuration)
5. Launch from Unreal Editor or Visual Studio

### First Run
- The planet generates automatically on level start
- Camera controls: WASD (move), Right-click drag (pan), Scroll (zoom), Middle mouse (rotate)
- Left-click to select cities and units
- Press Tab for diplomacy interface

## Development Notes

- Planet generation may take 10-30 seconds on first load depending on resolution settings
- Adjust `Resolution` in PlanetActor blueprint to balance quality vs. performance
- AI decision-making runs on a tick-based system with configurable decision intervals
- Resource clusters spawn at game start (not persistent between sessions)

## Future Enhancements

- Multiplayer support
- Save/load system for persistent worlds
- Additional building types and unit varieties
- More sophisticated diplomatic AI
- Shader-based ocean waves and foam
- Planetary weather systems

## Changelog

### 2026-03-23 — Ship Update
- Added `ShipActor` as a naval subclass of `VehicleActor`
- Ships continue moving while firing (`bCanMoveWhileFiring = true`) rather than stopping in place
- Ship projectiles use a high arc (`ProjectileArcHeight = 800`) so shells clear the planet mesh without clipping
- Ships do not capture land resource nodes after destroying mines (`bCanCaptureResources = false`) — they stay at sea
- Long coastal bombardment range (8000 units), 3× ground vehicle movement speed

### 2026-03-03 — Performance Optimisation
- Eliminated per-frame `GetAllActorsOfClass` calls across all major actor types
- World Tick reduced from ~33 ms → ~0.18 ms (≈180× improvement)
- HUD alliance display throttled to a 5-second refresh interval

### 2026-02-20 — Continent System
- Each city, resource node, and Kaiju is assigned a `ContinentID` at spawn
- AI resource targeting and trade requests are filtered to the same continent
- Prevents AI factions from attempting unreachable cross-ocean maneuvres

---

## License

This project is for portfolio demonstration purposes.

## Author

Benjamin Ramsell (bramsell)
- GitHub: [github.com/bramsell](https://github.com/bramsell)
