# City Camera Architecture

## Overview
The game uses **one pawn with two cameras** - this is the correct approach!

## How It Works

### Camera Setup
- **PlanetCamera**: Orbits around the entire planet
- **CityCamera**: Focuses on individual cities
- Both cameras are attached to the same pawn but with separate spring arms
- Smooth transitions using Unreal's `SetViewTargetWithBlend()`

### City Camera Positioning
When entering city mode:
1. Calculate direction from planet center to city (this points "up" from city's perspective)
2. **Invert it** to point toward the planet
3. Set spring arm rotation to point toward planet
4. Spring arm extends in **-X direction** (opposite rotation), so camera ends up **away from planet**
5. Camera looks back (180° yaw) at the city with a downward tilt

Example: If city is East of planet at (100000, 0, 0):
- Arm points West (toward planet)
- Arm extends East (away from planet)
- Camera ends up at (110000, 0, 0) - above the city
- Camera looks West and down at the city

### Multiple Cities
**Current approach (recommended):**
- Keep the single pawn with two cameras
- When switching cities, just call `EnterCityEditorMode(NewCityLocation)`
- No need for multiple camera pawns!
- The transition system handles smooth movement between cities

**Why this works:**
- Lightweight (no spawning/despawning pawns)
- Smooth transitions handled by Unreal
- Easy to manage - just update the position
- Player owns one camera that can view any city

### Controls
**Planet Mode:**
- WASD: Camera-relative "drag ground" movement across planet sphere
- Right-click: Camera-relative panning (can cross poles)
- Middle mouse: Horizon rotation + camera tilt
- Scroll: Zoom in/out

**City Mode:**
- WASD: Orbit around the city (yaw rotation) and tilt (pitch adjustment)
- Right-click: Camera-relative panning around city (same as planet mode)
- Middle mouse: Same as planet mode
- Scroll: Zoom in/out (scaled for city size)

### Transitions
- Uses `PlayerController->SetViewTargetWithBlend(this, 1.0f, VTBlend_Cubic)`
- Smoothly interpolates between camera positions and rotations
- Blend time controlled by `CityEditorBlendTime` (default 1 second)
- No teleporting - Unreal handles everything!

## Debug Logging  When entering city mode, check the log forthe logs will show:
- City and planet locations
- Direction calculations
- Final arm rotation and position
- Where the camera actually ends up
- Distance from planet center (should be planet radius + arm length)
