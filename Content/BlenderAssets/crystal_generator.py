"""
Black Quartz Crystal Generator for Unreal Engine
Creates procedural crystal clumps with semi-transparent exterior and solid black core
"""

import bpy
import bmesh
import math
import random
from mathutils import Vector, Euler


def create_crystal_material():
    """Create a material with transparent exterior blending to opaque black core"""
    mat = bpy.data.materials.new(name="BlackSubstrate_Material")
    mat.use_nodes = True
    mat.blend_method = 'BLEND'
    
    # Set shadow method if available (Blender 2.81+)
    if hasattr(mat, 'shadow_method'):
        mat.shadow_method = 'HASHED'
    
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    
    # Clear default nodes
    nodes.clear()
    
    # Create nodes
    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (600, 0)
    
    mix_shader = nodes.new('ShaderNodeMixShader')
    mix_shader.location = (400, 0)
    
    # Transparent shader for outer layer
    transparent = nodes.new('ShaderNodeBsdfTransparent')
    transparent.location = (200, 100)
    
    # Glass shader for the crystal look
    glass = nodes.new('ShaderNodeBsdfGlass')
    glass.location = (200, -100)
    glass.inputs['Color'].default_value = (0.02, 0.02, 0.02, 1.0)  # Very dark gray/black
    glass.inputs['Roughness'].default_value = 0.1
    glass.inputs['IOR'].default_value = 1.544  # Quartz IOR
    
    # Volume absorption for depth
    volume_absorption = nodes.new('ShaderNodeVolumeAbsorption')
    volume_absorption.location = (400, -200)
    volume_absorption.inputs['Color'].default_value = (0.0, 0.0, 0.0, 1.0)
    volume_absorption.inputs['Density'].default_value = 2.0  # Adjust for darkness depth
    
    # Layer weight for fresnel effect
    layer_weight = nodes.new('ShaderNodeLayerWeight')
    layer_weight.location = (0, 0)
    layer_weight.inputs['Blend'].default_value = 0.3
    
    # Color ramp to control transparency gradient
    color_ramp = nodes.new('ShaderNodeValToRGB')
    color_ramp.location = (200, 200)
    color_ramp.color_ramp.elements[0].position = 0.3
    color_ramp.color_ramp.elements[1].position = 0.7
    
    # Connect nodes
    links.new(layer_weight.outputs['Facing'], color_ramp.inputs['Fac'])
    links.new(color_ramp.outputs['Color'], mix_shader.inputs['Factor'])
    links.new(transparent.outputs['BSDF'], mix_shader.inputs[1])
    links.new(glass.outputs['BSDF'], mix_shader.inputs[2])
    links.new(mix_shader.outputs['Shader'], output.inputs['Surface'])
    links.new(volume_absorption.outputs['Volume'], output.inputs['Volume'])
    
    return mat


def create_single_crystal(size=1.0, height_ratio=3.0, taper_ratio=0.3, broken_top=False):
    """
    Create a single crystal shape (hexagonal prism with tapered top or broken top)
    
    Args:
        size: Base size of the crystal
        height_ratio: Height relative to base size
        taper_ratio: How much the top tapers (0-1)
        broken_top: If True, creates an angled cleaved top instead of a point
    """
    mesh = bpy.data.meshes.new("Crystal")
    obj = bpy.data.objects.new("Crystal", mesh)
    
    bm = bmesh.new()
    
    # Create hexagonal base
    num_sides = 6
    base_verts = []
    mid_verts = []
    top_verts = []
    
    height = size * height_ratio
    
    # Bottom vertices (hexagon)
    for i in range(num_sides):
        angle = (math.pi * 2 * i) / num_sides
        x = math.cos(angle) * size
        y = math.sin(angle) * size
        base_verts.append(bm.verts.new((x, y, 0)))
    
    if broken_top:
        # Create a slanted broken top first to determine heights
        # Random angle for the break plane
        break_angle = random.uniform(0, math.pi * 2)
        break_tilt = random.uniform(0.1, 0.3)  # How much the plane tilts
        
        top_heights = []
        for i in range(num_sides):
            angle = (math.pi * 2 * i) / num_sides
            x = math.cos(angle) * size
            y = math.sin(angle) * size
            
            # Calculate height based on position relative to break angle
            angle_diff = angle - break_angle
            # Height varies based on angle, creating slanted top
            z_offset = math.cos(angle_diff) * break_tilt * height
            top_height = height * random.uniform(0.85, 0.95) + z_offset
            top_heights.append(top_height)
            
            top_verts.append(bm.verts.new((x, y, top_height)))
        
        # Create middle vertices, ensuring they don't exceed the top
        for i in range(num_sides):
            angle = (math.pi * 2 * i) / num_sides
            x = math.cos(angle) * size
            y = math.sin(angle) * size
            
            # Mid height should be 80% of the way up, but not higher than the top
            mid_height = min(height * 0.8, top_heights[i] * 0.85)
            mid_verts.append(bm.verts.new((x, y, mid_height)))
        
        bm.verts.ensure_lookup_table()
        
        # Create faces
        # Bottom face
        bm.faces.new(base_verts)
        
        # Side faces (base to middle)
        for i in range(num_sides):
            next_i = (i + 1) % num_sides
            bm.faces.new([base_verts[i], base_verts[next_i], 
                          mid_verts[next_i], mid_verts[i]])
        
        # Side faces (middle to top)
        for i in range(num_sides):
            next_i = (i + 1) % num_sides
            bm.faces.new([mid_verts[i], mid_verts[next_i], 
                          top_verts[next_i], top_verts[i]])
        
        # Top face (broken surface)
        bm.faces.new(top_verts)
        
    else:
        # For pointed crystals, create middle vertices normally
        mid_height = height * 0.8
        for i in range(num_sides):
            angle = (math.pi * 2 * i) / num_sides
            x = math.cos(angle) * size
            y = math.sin(angle) * size
            mid_verts.append(bm.verts.new((x, y, mid_height)))
        
        # Create pointed top (original style)
        taper_start_verts = []
        
        # Taper start vertices at 95% height (where narrowing begins)
        taper_height = height * 0.95
        taper_size = size * taper_ratio
        for i in range(num_sides):
            angle = (math.pi * 2 * i) / num_sides
            x = math.cos(angle) * taper_size
            y = math.sin(angle) * taper_size
            taper_start_verts.append(bm.verts.new((x, y, taper_height)))
        
        # Tip vertex
        tip = bm.verts.new((0, 0, height))
        
        bm.verts.ensure_lookup_table()
        
        # Create faces
        # Bottom face
        bm.faces.new(base_verts)
        
        # Side faces (base to middle - straight shaft)
        for i in range(num_sides):
            next_i = (i + 1) % num_sides
            bm.faces.new([base_verts[i], base_verts[next_i], 
                          mid_verts[next_i], mid_verts[i]])
        
        # Side faces (middle to taper start - straight shaft)
        for i in range(num_sides):
            next_i = (i + 1) % num_sides
            bm.faces.new([mid_verts[i], mid_verts[next_i], 
                          taper_start_verts[next_i], taper_start_verts[i]])
        
        # Top faces (taper to tip)
        for i in range(num_sides):
            next_i = (i + 1) % num_sides
            bm.faces.new([taper_start_verts[i], taper_start_verts[next_i], tip])
    
    # Add some irregularity to make it more natural
    for vert in bm.verts:
        if vert not in base_verts:
            noise = Vector((
                random.uniform(-0.1, 0.1),
                random.uniform(-0.1, 0.1),
                random.uniform(-0.05, 0.05)
            ))
            vert.co += noise * size * 0.06
    
    bm.to_mesh(mesh)
    bm.free()
    
    # Use flat shading for sharp, jagged crystals
    for face in mesh.polygons:
        face.use_smooth = False
    
    return obj


def create_crystal_clump(num_crystals=8, base_radius=2.0, collection_name="BlackSubstrate_Crystals"):
    """
    Create a clump of crystals at various angles and sizes
    
    Args:
        num_crystals: Number of crystals in the clump
        base_radius: Radius of the clump base
        collection_name: Name for the collection
    """
    # Create collection
    collection = bpy.data.collections.new(collection_name)
    bpy.context.scene.collection.children.link(collection)
    
    # Create material once
    material = create_crystal_material()
    
    crystals = []
    
    for i in range(num_crystals):
        # Randomize crystal properties
        # Use global length parameter to determine height ratio
        # 0.0 = short (3-4), 0.5 = medium (3-6), 1.0 = long (5-8)
        min_height = 3.0 + GLOBAL_CRYSTAL_LENGTH * 2.0
        max_height = 4.0 + GLOBAL_CRYSTAL_LENGTH * 4.0
        height_ratio = random.uniform(min_height, max_height)
        
        # Longer crystals are wider
        size = random.uniform(0.15, 0.35) * (height_ratio / 4.5)
        taper_ratio = random.uniform(0.15, 0.3)
        
        # Chance of broken top (use global variable if set by operator)
        broken_top = random.random() < GLOBAL_BROKEN_CHANCE
        
        # Create crystal
        crystal = create_single_crystal(size, height_ratio, taper_ratio, broken_top)
        
        # Position within clump
        angle = random.uniform(0, math.pi * 2)
        distance = random.uniform(0, base_radius)
        x = math.cos(angle) * distance
        y = math.sin(angle) * distance
        
        crystal.location = (x, y, 0)
        
        # Calculate rotation based on outward angle parameter
        # Center crystals stick straight up, outer ones tilt outward
        distance_from_center = math.sqrt(x*x + y*y)
        
        # Normalize distance by base_radius (0 at center, 1 at edge)
        normalized_distance = min(distance_from_center / base_radius, 1.0)
        
        if distance_from_center > 0.01:
            # Direction vector from center to crystal
            dir_x = x / distance_from_center
            dir_y = y / distance_from_center
            
            # Tilt increases with distance from center
            # Center crystals have minimal tilt, edge crystals tilt based on outward_angle
            distance_factor = normalized_distance ** 1.5  # Power curve: center stays upright
            base_tilt = GLOBAL_OUTWARD_ANGLE * 0.8 * distance_factor
            
            # Add some randomness
            tilt_amount = base_tilt + random.uniform(-0.1, 0.1)
            
            # Tilt in the direction away from center
            rot_x = dir_y * tilt_amount  # Pitch based on Y position
            rot_y = -dir_x * tilt_amount  # Roll based on X position
        else:
            # Crystal at center stays mostly upright
            rot_x = random.uniform(-0.05, 0.05)
            rot_y = random.uniform(-0.05, 0.05)
        
        rot_z = random.uniform(0, math.pi * 2)  # Random spin
        crystal.rotation_euler = Euler((rot_x, rot_y, rot_z), 'XYZ')
        
        # Add material
        if crystal.data.materials:
            crystal.data.materials[0] = material
        else:
            crystal.data.materials.append(material)
        
        # Add to collection
        collection.objects.link(crystal)
        
        # Remove from scene collection to keep it organized
        if crystal.name in bpy.context.scene.collection.objects:
            bpy.context.scene.collection.objects.unlink(crystal)
        
        crystals.append(crystal)
    
    return crystals, collection


def setup_geometry_nodes_variation(collection):
    """
    Optional: Set up geometry nodes for additional procedural variation
    This can be added later for more advanced control
    """
    # This is a placeholder for future geometry nodes setup
    # Could add noise, subdivision, displacement, etc.
    pass


class CrystalGeneratorProperties(bpy.types.PropertyGroup):
    """Properties for crystal generator"""
    num_crystals: bpy.props.IntProperty(
        name="Number of Crystals",
        description="How many crystals in the clump",
        default=12,
        min=1,
        max=50
    )
    
    base_radius: bpy.props.FloatProperty(
        name="Spread Radius",
        description="How spread out the clump is",
        default=1.5,
        min=0.1,
        max=10.0
    )
    
    broken_chance: bpy.props.FloatProperty(
        name="Broken Top Chance",
        description="Probability of crystals having sheared/broken tops (0=all pointed, 1=all broken)",
        default=0.4,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    outward_angle: bpy.props.FloatProperty(
        name="Outward Angle",
        description="How much crystals point outward vs upward (0=straight up, 1=more outward)",
        default=0.2,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    crystal_length: bpy.props.FloatProperty(
        name="Crystal Length",
        description="General length of crystals (0=short/thick, 1=long/thin)",
        default=0.5,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    random_seed: bpy.props.IntProperty(
        name="Random Seed",
        description="Seed for random generation (same seed = same result)",
        default=42,
        min=0,
        max=10000
    )
    
    clear_existing: bpy.props.BoolProperty(
        name="Clear Existing",
        description="Remove previous crystal collections before generating",
        default=False
    )


class OBJECT_OT_generate_crystals(bpy.types.Operator):
    """Generate Black Substrate Crystal Clump"""
    bl_idname = "object.generate_crystals"
    bl_label = "Generate Black Crystals"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        props = context.scene.crystal_props
        
        # Clear existing crystals if requested
        if props.clear_existing and "BlackSubstrate_Crystals" in bpy.data.collections:
            collection = bpy.data.collections["BlackSubstrate_Crystals"]
            for obj in collection.objects:
                bpy.data.objects.remove(obj, do_unlink=True)
            bpy.data.collections.remove(collection)
        
        # Set random seed
        random.seed(props.random_seed)
        
        # Store parameters for create_crystal_clump to use
        global GLOBAL_BROKEN_CHANCE, GLOBAL_OUTWARD_ANGLE, GLOBAL_CRYSTAL_LENGTH
        GLOBAL_BROKEN_CHANCE = props.broken_chance
        GLOBAL_OUTWARD_ANGLE = props.outward_angle
        GLOBAL_CRYSTAL_LENGTH = props.crystal_length
        
        # Generate crystal clump
        crystals, collection = create_crystal_clump(
            num_crystals=props.num_crystals,
            base_radius=props.base_radius,
            collection_name="BlackSubstrate_Crystals"
        )
        
        self.report({'INFO'}, f"Created {len(crystals)} crystals")
        return {'FINISHED'}


class VIEW3D_PT_crystal_generator(bpy.types.Panel):
    """Creates a Panel in the 3D Viewport N-panel"""
    bl_label = "Black Crystal Generator"
    bl_idname = "VIEW3D_PT_crystal_generator"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Crystal Gen'
    
    def draw(self, context):
        layout = self.layout
        props = context.scene.crystal_props
        
        # Draw all the property sliders
        layout.prop(props, "num_crystals")
        layout.prop(props, "base_radius")
        layout.prop(props, "crystal_length", slider=True)
        layout.prop(props, "broken_chance", slider=True)
        layout.prop(props, "outward_angle", slider=True)
        layout.prop(props, "random_seed")
        layout.prop(props, "clear_existing")
        
        layout.separator()
        
        # Generate button
        layout.operator("object.generate_crystals")


# Global variables to pass parameters
GLOBAL_BROKEN_CHANCE = 0.4
GLOBAL_OUTWARD_ANGLE = 0.2
GLOBAL_CRYSTAL_LENGTH = 0.5


def register():
    bpy.utils.register_class(CrystalGeneratorProperties)
    bpy.utils.register_class(OBJECT_OT_generate_crystals)
    bpy.utils.register_class(VIEW3D_PT_crystal_generator)
    bpy.types.Scene.crystal_props = bpy.props.PointerProperty(type=CrystalGeneratorProperties)


def unregister():
    bpy.utils.unregister_class(VIEW3D_PT_crystal_generator)
    bpy.utils.unregister_class(OBJECT_OT_generate_crystals)
    bpy.utils.unregister_class(CrystalGeneratorProperties)
    del bpy.types.Scene.crystal_props


def main():
    """Main function for running as script (not operator)"""
    # Generate new crystal clump
    random.seed(42)
    crystals, collection = create_crystal_clump(
        num_crystals=12,
        base_radius=1.5,
        collection_name="BlackSubstrate_Crystals"
    )
    
    print(f"Created {len(crystals)} crystals in collection '{collection.name}'")
    print("Crystal generator complete!")


# Run the script
if __name__ == "__main__":
    # Register the operator and panel
    register()
    print("Crystal Generator registered! Find it in the N-panel under 'Crystal Gen' tab")
