"""
Blender Python Script: Bake Volcano FBX to Heightmap Texture

How to run:
1. Open Blender
2. Go to Scripting workspace
3. Open this script
4. Update the paths below for your system
5. Run script (Alt+P or click Run button)

Output: volcano_heightmap.png (1024x1024 grayscale heightmap)
"""

import bpy
import os
import math
from pathlib import Path

# ===== CONFIGURATION =====
FBX_PATH = r"C:\Users\benja\Documents\Unreal Projects\Planet_Conquest\Content\BlenderAssets\Volcano.fbx"
OUTPUT_PATH = r"C:\Users\benja\Documents\Unreal Projects\Planet_Conquest\Content\BlenderAssets\Volcanoes\volcano_heightmap.png"
TEXTURE_SIZE = 1024  # Resolution of output heightmap (1024x1024)

def clear_scene():
    """Remove all existing objects from the scene"""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    
    # Clear orphaned data
    for block in bpy.data.meshes:
        if block.users == 0:
            bpy.data.meshes.remove(block)
    for block in bpy.data.materials:
        if block.users == 0:
            bpy.data.materials.remove(block)

def import_fbx(filepath):
    """Import the FBX file"""
    print(f"Importing FBX from: {filepath}")
    bpy.ops.import_scene.fbx(filepath=filepath)
    
    # Get the imported object
    imported_obj = bpy.context.selected_objects[0]
    print(f"Imported: {imported_obj.name}")
    return imported_obj

def get_mesh_bounds(obj):
    """Get the bounding box of the mesh in world space"""
    # Get all vertex positions in world space
    vertices = [obj.matrix_world @ v.co for v in obj.data.vertices]
    
    min_x = min(v.x for v in vertices)
    max_x = max(v.x for v in vertices)
    min_y = min(v.y for v in vertices)
    max_y = max(v.y for v in vertices)
    min_z = min(v.z for v in vertices)
    max_z = max(v.z for v in vertices)
    
    return (min_x, max_x, min_y, max_y, min_z, max_z)

def create_heightmap_texture(obj, texture_size):
    """
    Create a heightmap by casting rays from above and sampling mesh height
    """
    min_x, max_x, min_y, max_y, min_z, max_z = get_mesh_bounds(obj)
    
    print(f"Mesh bounds:")
    print(f"  X: {min_x:.2f} to {max_x:.2f}")
    print(f"  Y: {min_y:.2f} to {max_y:.2f}")
    print(f"  Z: {min_z:.2f} to {max_z:.2f}")
    
    # Calculate size and center
    size_x = max_x - min_x
    size_y = max_y - min_y
    size_z = max_z - min_z
    max_size = max(size_x, size_y)  # Use square bounds
    
    center_x = (min_x + max_x) / 2
    center_y = (min_y + max_y) / 2
    
    print(f"Creating {texture_size}x{texture_size} heightmap...")
    print(f"Height range: {min_z:.2f} to {max_z:.2f} (delta: {size_z:.2f})")
    
    # Create a new image for the heightmap
    image_name = "VolcanoHeightmap"
    if image_name in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[image_name])
    
    image = bpy.data.images.new(image_name, texture_size, texture_size, alpha=False)
    
    # Initialize pixels array (RGBA format, but we'll only use R for grayscale)
    pixels = [0.0] * (texture_size * texture_size * 4)
    
    # Sample the mesh from top-down view
    for py in range(texture_size):
        if py % 100 == 0:
            print(f"  Processing row {py}/{texture_size}...")
        
        for px in range(texture_size):
            # Convert pixel coordinates to world space (top-down view)
            # Center the sample area
            u = px / (texture_size - 1)
            v = py / (texture_size - 1)
            
            world_x = center_x + (u - 0.5) * max_size
            world_y = center_y + (v - 0.5) * max_size
            
            # Ray cast from high above downward
            ray_start = (world_x, world_y, max_z + 1000.0)
            ray_end = (world_x, world_y, min_z - 1000.0)
            
            # Cast ray
            result, location, normal, index = obj.ray_cast(
                obj.matrix_world.inverted() @ mathutils.Vector(ray_start),
                obj.matrix_world.inverted() @ mathutils.Vector(ray_end)
            )
            
            # Calculate height value
            if result:
                # Hit the mesh - convert height to 0-1 range
                world_location = obj.matrix_world @ location
                height = (world_location.z - min_z) / size_z if size_z > 0 else 0
                height = max(0.0, min(1.0, height))  # Clamp to [0, 1]
            else:
                # No hit - use black (0.0)
                height = 0.0
            
            # Set pixel (RGBA format)
            pixel_index = (py * texture_size + px) * 4
            pixels[pixel_index] = height      # R
            pixels[pixel_index + 1] = height  # G
            pixels[pixel_index + 2] = height  # B
            pixels[pixel_index + 3] = 1.0     # A
    
    # Apply pixels to image
    image.pixels[:] = pixels
    image.update()
    
    return image

def save_image(image, output_path):
    """Save the image to disk"""
    # Ensure output directory exists
    output_dir = os.path.dirname(output_path)
    os.makedirs(output_dir, exist_ok=True)
    
    # Set file format
    image.file_format = 'PNG'
    
    # Save the image
    image.filepath_raw = output_path
    image.save()
    
    print(f"Heightmap saved to: {output_path}")

def main():
    print("=" * 60)
    print("VOLCANO HEIGHTMAP BAKER")
    print("=" * 60)
    
    # Check if FBX exists
    if not os.path.exists(FBX_PATH):
        print(f"ERROR: FBX file not found at: {FBX_PATH}")
        return
    
    # Import mathutils here (Blender built-in)
    import mathutils
    globals()['mathutils'] = mathutils
    
    # Step 1: Clear scene
    print("\n1. Clearing scene...")
    clear_scene()
    
    # Step 2: Import FBX
    print("\n2. Importing FBX...")
    volcano_obj = import_fbx(FBX_PATH)
    
    # Step 3: Create heightmap
    print("\n3. Creating heightmap texture...")
    heightmap_image = create_heightmap_texture(volcano_obj, TEXTURE_SIZE)
    
    # Step 4: Save image
    print("\n4. Saving heightmap...")
    save_image(heightmap_image, OUTPUT_PATH)
    
    print("\n" + "=" * 60)
    print("COMPLETE!")
    print("=" * 60)
    print(f"Heightmap texture created: {OUTPUT_PATH}")
    print(f"Resolution: {TEXTURE_SIZE}x{TEXTURE_SIZE}")
    print("\nYou can now use this texture in Unreal Engine!")

# Run the script
if __name__ == "__main__":
    main()
