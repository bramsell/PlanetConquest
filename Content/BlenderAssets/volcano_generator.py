"""
Volcano Generator for Unreal Engine
Creates a procedural volcano with crater and erosion ridges/valleys
Low poly for texturing in Unreal
"""

import bpy
import bmesh
import math
import random
from mathutils import Vector


def create_volcano(
    base_radius=10.0,
    height=8.0,
    crater_radius=3.0,
    crater_depth=1.5,
    num_ridges=12,
    ridge_intensity=0.3,
    slope_curve=0.5,
    curve_position=0.5,
    base_distortion=0.3,
    segments=32,
    height_rings=12
):
    """
    Create a volcano mesh with ridges and valleys
    
    Args:
        base_radius: Radius at the base of the volcano
        height: Total height of the volcano
        crater_radius: Radius of the crater at the top
        crater_depth: How deep the crater goes
        num_ridges: Number of ridges/valleys around the volcano
        ridge_intensity: How pronounced the ridges are (0-1)
        slope_curve: How curved the slope is (0=linear, 1=maximum curve)
        curve_position: Where the curve effect is strongest (0=bottom, 1=top)
        base_distortion: How much the base is distorted/irregular (0=circle, 1=very irregular)
        segments: Number of segments around (resolution)
        height_rings: Number of vertex rings from base to top
    """
    mesh = bpy.data.meshes.new("Volcano")
    obj = bpy.data.objects.new("Volcano", mesh)
    
    bm = bmesh.new()
    
    # Store all rings of vertices
    rings = []
    
    # Pre-calculate base distortion pattern using multiple frequencies
    # This creates organic, non-regular shapes instead of perfect lobes
    freq1 = random.choice([3, 5, 7])  # Primary frequency (odd numbers avoid square look)
    freq2 = random.choice([2, 3])      # Secondary frequency
    freq3 = random.randint(8, 12)      # High frequency for detail
    
    phase1 = random.uniform(0, math.pi * 2)
    phase2 = random.uniform(0, math.pi * 2)
    phase3 = random.uniform(0, math.pi * 2)
    
    # Create vertex rings from bottom to top
    for ring_idx in range(height_rings):
        ring_verts = []
        
        # Height ratio from 0 (bottom) to 1 (top)
        t = ring_idx / (height_rings - 1)
        
        # Current height
        z = height * t
        
        # Radius tapers with curved profile
        # Use a smooth curve that transitions from linear to power curve
        if slope_curve > 0.01:  # Apply curve
            # Power value: higher = steeper curve
            # Map slope_curve (0-1) to power range (1 to 4)
            power = 1.0 + slope_curve * 3.0
            
            # Apply curve based on position preference
            # For volcano: we want gentle slope at base (radius stays large), steep at top (radius shrinks fast)
            # This means we need: 1 - (1-t)^power for steepening at top
            if curve_position < 0.5:
                # Steepening happens more at the bottom
                # Use standard power curve (radius shrinks faster at bottom)
                weight = (0.5 - curve_position) * 2.0  # 0 to 1
                curve_t = (1.0 - weight) * (1.0 - (1.0 - t) ** power) + weight * (t ** power)
            else:
                # Steepening happens more at the top (standard volcano)
                # Use inverse power curve (radius stays large, then shrinks fast at top)
                weight = (curve_position - 0.5) * 2.0  # 0 to 1
                curve_t = (1.0 - weight) * (1.0 - (1.0 - t) ** power) + weight * (1.0 - (1.0 - t) ** (power * 1.5))
        else:
            # Linear
            curve_t = t
        
        current_radius = base_radius * (1 - curve_t) + crater_radius * curve_t
        
        for i in range(segments):
            angle = (math.pi * 2 * i) / segments
            
            # Base distortion - creates oval/irregular base shape
            # Fades out as we go up the volcano
            distortion_fade = 1.0 - (t ** 0.5)  # Strong at base, fades toward top
            
            # Multi-frequency distortion for organic shapes (not square/regular)
            # Combine 3 different frequencies to break up regularity
            distort1 = math.sin(angle * freq1 + phase1) * 0.5
            distort2 = math.sin(angle * freq2 + phase2) * 0.3
            distort3 = math.sin(angle * freq3 + phase3) * 0.15
            
            base_distort = (distort1 + distort2 + distort3) * base_distortion * base_radius * 0.25
            base_distort *= distortion_fade
            
            # Add per-vertex random noise for extra irregularity
            noise_offset = random.uniform(-0.1, 0.1) * base_radius * 0.1 * base_distortion
            noise_offset *= distortion_fade
            
            # Calculate ridge displacement using sine wave
            # Multiple ridges around the volcano
            ridge_angle = angle * num_ridges
            ridge_offset = math.sin(ridge_angle) * ridge_intensity * base_radius * 0.15
            
            # Ridge effect diminishes at base and top
            ridge_fade = math.sin(t * math.pi)  # 0 at bottom and top, 1 in middle
            ridge_offset *= ridge_fade
            
            # Calculate position with all displacements
            radius_with_effects = current_radius + base_distort + noise_offset + ridge_offset
            x = math.cos(angle) * radius_with_effects
            y = math.sin(angle) * radius_with_effects
            
            ring_verts.append(bm.verts.new((x, y, z)))
        
        rings.append(ring_verts)
    
    bm.verts.ensure_lookup_table()
    
    # Create bottom face
    bm.faces.new(rings[0])
    
    # Create side faces between rings
    for ring_idx in range(len(rings) - 1):
        for i in range(segments):
            next_i = (i + 1) % segments
            bm.faces.new([
                rings[ring_idx][i],
                rings[ring_idx][next_i],
                rings[ring_idx + 1][next_i],
                rings[ring_idx + 1][i]
            ])
    
    # Create crater (inner depression)
    crater_ring_verts = []
    crater_bottom_verts = []
    
    # Crater rim (top of volcano)
    top_ring = rings[-1]
    
    # Inner crater ring at the rim level (very close to outer rim for thin edge)
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        
        # Make this very close to crater_radius for a thin rim
        noise = random.uniform(0.95, 1.05)
        inner_crater_radius = crater_radius * 0.9  # Much closer to outer rim
        x = math.cos(angle) * inner_crater_radius * noise
        y = math.sin(angle) * inner_crater_radius * noise
        z = height
        
        crater_ring_verts.append(bm.verts.new((x, y, z)))
    
    # Bottom of crater (lava pool area)
    crater_bottom_radius = crater_radius * 0.5
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        noise = random.uniform(0.95, 1.05)
        x = math.cos(angle) * crater_bottom_radius * noise
        y = math.sin(angle) * crater_bottom_radius * noise
        z = height - crater_depth
        
        crater_bottom_verts.append(bm.verts.new((x, y, z)))
    
    bm.verts.ensure_lookup_table()
    
    # Connect outer rim to inner crater ring
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([
            top_ring[i],
            top_ring[next_i],
            crater_ring_verts[next_i],
            crater_ring_verts[i]
        ])
    
    # Connect inner crater ring to crater bottom
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([
            crater_ring_verts[i],
            crater_ring_verts[next_i],
            crater_bottom_verts[next_i],
            crater_bottom_verts[i]
        ])
    
    # Crater bottom face
    bm.faces.new(crater_bottom_verts)
    
    bm.to_mesh(mesh)
    bm.free()
    
    # Flat shading for low-poly look
    for face in mesh.polygons:
        face.use_smooth = False
    
    return obj


class VolcanoGeneratorProperties(bpy.types.PropertyGroup):
    """Properties for volcano generator"""
    base_radius: bpy.props.FloatProperty(
        name="Base Radius",
        description="Radius at the base of the volcano",
        default=10.0,
        min=2.0,
        max=50.0
    )
    
    height: bpy.props.FloatProperty(
        name="Height",
        description="Total height of the volcano",
        default=8.0,
        min=1.0,
        max=30.0
    )
    
    crater_radius: bpy.props.FloatProperty(
        name="Crater Radius",
        description="Radius of the crater at the top",
        default=3.0,
        min=0.5,
        max=20.0
    )
    
    crater_depth: bpy.props.FloatProperty(
        name="Crater Depth",
        description="How deep the crater depression is",
        default=1.5,
        min=0.2,
        max=5.0
    )
    
    slope_curve: bpy.props.FloatProperty(
        name="Slope Curve",
        description="How curved the slope is (0=linear/cone, 1=maximum curve/steep at top)",
        default=0.5,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    curve_position: bpy.props.FloatProperty(
        name="Curve Position",
        description="Where the curve effect concentrates (0.5=middle, 1=top is steepest)",
        default=0.7,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    base_distortion: bpy.props.FloatProperty(
        name="Base Distortion",
        description="How irregular/oval the base is (0=perfect circle, 1=very irregular)",
        default=0.3,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    num_ridges: bpy.props.IntProperty(
        name="Number of Ridges",
        description="Number of ridges/valleys running down the volcano",
        default=12,
        min=4,
        max=32
    )
    
    ridge_intensity: bpy.props.FloatProperty(
        name="Ridge Intensity",
        description="How pronounced the ridges and valleys are",
        default=0.3,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    segments: bpy.props.IntProperty(
        name="Segments",
        description="Number of segments around the volcano (resolution)",
        default=32,
        min=8,
        max=128
    )
    
    height_rings: bpy.props.IntProperty(
        name="Height Rings",
        description="Number of vertex rings from base to top (vertical resolution)",
        default=12,
        min=4,
        max=32
    )
    
    random_seed: bpy.props.IntProperty(
        name="Random Seed",
        description="Seed for random generation",
        default=42,
        min=0,
        max=10000
    )
    
    clear_existing: bpy.props.BoolProperty(
        name="Clear Existing",
        description="Remove previous volcano before generating",
        default=False
    )


class OBJECT_OT_generate_volcano(bpy.types.Operator):
    """Generate Procedural Volcano"""
    bl_idname = "object.generate_volcano"
    bl_label = "Generate Volcano"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        props = context.scene.volcano_props
        
        # Clear existing if requested
        if props.clear_existing:
            for obj in bpy.data.objects:
                if obj.name.startswith("Volcano"):
                    bpy.data.objects.remove(obj, do_unlink=True)
        
        # Set random seed
        random.seed(props.random_seed)
        
        # Generate volcano
        obj = create_volcano(
            base_radius=props.base_radius,
            height=props.height,
            crater_radius=props.crater_radius,
            crater_depth=props.crater_depth,
            num_ridges=props.num_ridges,
            ridge_intensity=props.ridge_intensity,
            slope_curve=props.slope_curve,
            curve_position=props.curve_position,
            base_distortion=props.base_distortion,
            segments=props.segments,
            height_rings=props.height_rings
        )
        
        # Add to scene
        context.collection.objects.link(obj)
        
        self.report({'INFO'}, "Created volcano")
        return {'FINISHED'}


class VIEW3D_PT_volcano_generator(bpy.types.Panel):
    """Creates a Panel in the 3D Viewport N-panel"""
    bl_label = "Volcano Generator"
    bl_idname = "VIEW3D_PT_volcano_generator"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Volcano Gen'
    
    def draw(self, context):
        layout = self.layout
        props = context.scene.volcano_props
        
        # Draw all the property sliders
        layout.label(text="Size:")
        layout.prop(props, "base_radius")
        layout.prop(props, "height")
        layout.prop(props, "slope_curve", slider=True)
        layout.prop(props, "curve_position", slider=True)
        layout.prop(props, "base_distortion", slider=True)
        
        layout.separator()
        layout.label(text="Crater:")
        layout.prop(props, "crater_radius")
        layout.prop(props, "crater_depth")
        
        layout.separator()
        layout.label(text="Erosion Detail:")
        layout.prop(props, "num_ridges")
        layout.prop(props, "ridge_intensity", slider=True)
        
        layout.separator()
        layout.label(text="Resolution:")
        layout.prop(props, "segments")
        layout.prop(props, "height_rings")
        
        layout.separator()
        layout.prop(props, "random_seed")
        layout.prop(props, "clear_existing")
        
        layout.separator()
        
        # Generate button
        layout.operator("object.generate_volcano")


def register():
    bpy.utils.register_class(VolcanoGeneratorProperties)
    bpy.utils.register_class(OBJECT_OT_generate_volcano)
    bpy.utils.register_class(VIEW3D_PT_volcano_generator)
    bpy.types.Scene.volcano_props = bpy.props.PointerProperty(type=VolcanoGeneratorProperties)


def unregister():
    bpy.utils.unregister_class(VIEW3D_PT_volcano_generator)
    bpy.utils.unregister_class(OBJECT_OT_generate_volcano)
    bpy.utils.unregister_class(VolcanoGeneratorProperties)
    del bpy.types.Scene.volcano_props


def main():
    """Main function for running as script"""
    random.seed(42)
    
    obj = create_volcano(
        base_radius=10.0,
        height=8.0,
        crater_radius=3.0,
        crater_depth=1.5,
        num_ridges=12,
        ridge_intensity=0.3,
        segments=32,
        height_rings=12
    )
    
    bpy.context.collection.objects.link(obj)
    print("Volcano created!")


# Run the script
if __name__ == "__main__":
    register()
    print("Volcano Generator registered! Find it in the N-panel under 'Volcano Gen' tab")
