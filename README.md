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
├── Planning/                    # Design documents and architecture notes
│   ├── AI_Decision_Tree.md
│   ├── CITY_CAMERA_ARCHITECTURE.md
│   └── GAME_DESIGN_DOCUMENT.txt
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

## License

This project is for portfolio demonstration purposes.

## Author

Benjamin Ramsell (bramsell)
- GitHub: [github.com/bramsell](https://github.com/bramsell)
