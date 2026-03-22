"""
Orange Substrate Plateau Generator for Unreal Engine
Creates a slanted flat-topped boulder with rocky base
Low poly for texturing in Unreal
"""

import bpy
import bmesh
import math
import random
from mathutils import Vector, Matrix


def create_rocky_base(base_radius=2.0, base_height=1.0, segments=8):
    """
    Create an irregular rocky base (truncated cone with noise)
    
    Args:
        base_radius: Radius at the bottom
        base_height: Height of the base
        segments: Number of sides (lower = more angular/rocky)
    """
    mesh = bpy.data.meshes.new("RockyBase")
    bm = bmesh.new()
    
    # Bottom vertices (larger circle)
    bottom_verts = []
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        # Add noise to radius for irregular shape
        noise = random.uniform(0.85, 1.15)
        x = math.cos(angle) * base_radius * noise
        y = math.sin(angle) * base_radius * noise
        bottom_verts.append(bm.verts.new((x, y, 0)))
    
    # Top vertices (smaller circle, tapers inward)
    top_verts = []
    top_radius = base_radius * random.uniform(0.6, 0.8)
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        # Different noise for top
        noise = random.uniform(0.9, 1.1)
        x = math.cos(angle) * top_radius * noise
        y = math.sin(angle) * top_radius * noise
        top_verts.append(bm.verts.new((x, y, base_height)))
    
    bm.verts.ensure_lookup_table()
    
    # Create bottom face
    bm.faces.new(bottom_verts)
    
    # Create side faces
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([
            bottom_verts[i], 
            bottom_verts[next_i],
            top_verts[next_i], 
            top_verts[i]
        ])
    
    # Create top face
    bm.faces.new(top_verts)
    
    # Add subtle height variation to vertices for rocky look
    for vert in bm.verts:
        if vert.co.z > 0:  # Only top vertices
            vert.co.z += random.uniform(-0.1, 0.1) * base_height
    
    return bm, top_verts


def create_slanted_plateau_top(bm, base_verts, tilt_x=0.3, tilt_y=0.2, top_height=0.5, top_scale=1.2):
    """
    Create a slanted flat top on the base
    
    Args:
        bm: bmesh object to add to
        base_verts: Top vertices of the base to connect to
        tilt_x: Tilt angle in X direction (radians)
        tilt_y: Tilt angle in Y direction (radians)
        top_height: How high above base the plateau extends
        top_scale: Scale of the top relative to base (can overhang)
    """
    segments = len(base_verts)
    
    # Calculate center of base
    center = Vector((0, 0, 0))
    for v in base_verts:
        center += v.co
    center /= len(base_verts)
    
    # Create top vertices (scaled and tilted)
    top_verts = []
    
    for i in range(segments):
        # Get base vertex position in XY
        base_v = base_verts[i]
        angle = (math.pi * 2 * i) / segments
        
        # Scale outward for overhang
        radius_scale = top_scale
        noise = random.uniform(0.95, 1.05)
        x = math.cos(angle) * center.x + (base_v.co.x - center.x) * radius_scale * noise
        y = math.sin(angle) * center.y + (base_v.co.y - center.y) * radius_scale * noise
        
        # Base height for this vertex
        z = center.z + top_height
        
        # Apply tilt based on XY position
        z += x * math.tan(tilt_x)
        z += y * math.tan(tilt_y)
        
        # Add small random variation
        z += random.uniform(-0.05, 0.05) * top_height
        
        top_verts.append(bm.verts.new((x, y, z)))
    
    bm.verts.ensure_lookup_table()
    
    # Create side faces connecting base to top
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([
            base_verts[i],
            base_verts[next_i],
            top_verts[next_i],
            top_verts[i]
        ])
    
    # Create top face (the flat slanted surface)
    bm.faces.new(top_verts)
    
    return top_verts


def create_plateau_boulder(
    base_radius=2.0,
    base_height=1.0,
    top_height=0.8,
    tilt_x=0.0,
    tilt_y=0.0,
    segments=8,
    top_scale=1.1,
    elongation=1.6,
    overhang_shift=0.7,
    top_width=0.8
):
    """
    Create complete plateau boulder with 5 vertex rings
    
    Args:
        base_radius: Radius of the base
        base_height: Height of rocky base
        top_height: Height of plateau section
        tilt_x: Tilt in X direction
        tilt_y: Tilt in Y direction
        segments: Number of sides (6-12 for low poly rocky look)
        top_scale: How much the top overhangs (1.0 = no overhang)
        elongation: How much to elongate the top (1.0 = circular)
        overhang_shift: How much the top shifts toward the tilted side (0-1)
        top_width: Scale of the top boulder width (0.5-1.5)
    """
    mesh = bpy.data.meshes.new("PlateauBoulder")
    obj = bpy.data.objects.new("PlateauBoulder", mesh)
    
    bm = bmesh.new()
    
    # Total height
    total_height = base_height + top_height
    
    # Random elongation angle
    elongation_angle = random.uniform(0, math.pi * 2)
    
    # Ring 1: Bottom (base) - circular
    ring1_verts = []
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        noise = random.uniform(0.85, 1.15)
        x = math.cos(angle) * base_radius * noise
        y = math.sin(angle) * base_radius * noise
        ring1_verts.append(bm.verts.new((x, y, 0)))
    
    # Ring 2: Lower transition (getting wider) - circular
    ring2_verts = []
    ring2_radius = base_radius * 0.85
    ring2_height = base_height * 0.4
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        noise = random.uniform(0.9, 1.1)
        x = math.cos(angle) * ring2_radius * noise
        y = math.sin(angle) * ring2_radius * noise
        ring2_verts.append(bm.verts.new((x, y, ring2_height)))
    
    # Ring 3: Narrow neck (base of top rock) - starts elongation, with tilt and shift
    ring3_verts = []
    ring3_radius = base_radius * 0.55
    ring3_height = base_height * 0.85
    
    # Calculate shift direction (toward the higher tilted side)
    tilt_magnitude = math.sqrt(tilt_x**2 + tilt_y**2)
    if tilt_magnitude > 0.001:
        shift_x = (tilt_x / tilt_magnitude) * base_radius * overhang_shift
        shift_y = (tilt_y / tilt_magnitude) * base_radius * overhang_shift
    else:
        shift_x = 0
        shift_y = 0
    
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        noise = random.uniform(0.9, 1.1)
        
        # Apply elongation along the elongation_angle direction
        angle_diff = angle - elongation_angle
        scale_factor = 1.0 + (elongation - 1.0) * 0.3 * abs(math.cos(angle_diff))
        
        x = math.cos(angle) * ring3_radius * noise * scale_factor + shift_x * 0.5
        y = math.sin(angle) * ring3_radius * noise * scale_factor + shift_y * 0.5
        
        # Apply tilt
        z = ring3_height
        z += x * math.tan(tilt_x)
        z += y * math.tan(tilt_y)
        
        ring3_verts.append(bm.verts.new((x, y, z)))
    
    # Ring 4: Widest point (bulge of top boulder) - more elongated, with tilt and shift
    ring4_verts = []
    ring4_base_radius = base_radius * top_scale * 0.85
    ring4_height = base_height + top_height * 0.3
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        noise = random.uniform(0.95, 1.05)
        
        # Apply elongation in the elongation_angle direction
        angle_diff = angle - elongation_angle
        elongation_scale = 1.0 + (elongation - 1.0) * 0.6 * abs(math.cos(angle_diff))
        
        # Apply width in the perpendicular direction
        width_scale = 1.0 + (top_width - 1.0) * abs(math.sin(angle_diff))
        
        # Combine both scales
        combined_scale = elongation_scale * width_scale
        
        x = math.cos(angle) * ring4_base_radius * noise * combined_scale + shift_x * 0.7
        y = math.sin(angle) * ring4_base_radius * noise * combined_scale + shift_y * 0.7
        
        # Apply tilt
        z = ring4_height
        z += x * math.tan(tilt_x)
        z += y * math.tan(tilt_y)
        
        ring4_verts.append(bm.verts.new((x, y, z)))
    
    # Ring 5: Top plateau vertices (narrower, most elongated, with tilt and full shift)
    ring5_verts = []
    ring5_base_radius = base_radius * top_scale * 0.55
    center_height = base_height + top_height * 0.9
    
    for i in range(segments):
        angle = (math.pi * 2 * i) / segments
        noise = random.uniform(0.95, 1.05)
        
        # Apply elongation in the elongation_angle direction
        angle_diff = angle - elongation_angle
        elongation_scale = 1.0 + (elongation - 1.0) * abs(math.cos(angle_diff))
        
        # Apply width in the perpendicular direction
        width_scale = 1.0 + (top_width - 1.0) * abs(math.sin(angle_diff))
        
        # Combine both scales
        combined_scale = elongation_scale * width_scale
        
        x = math.cos(angle) * ring5_base_radius * noise * combined_scale + shift_x
        y = math.sin(angle) * ring5_base_radius * noise * combined_scale + shift_y
        
        # Apply tilt
        z = center_height
        z += x * math.tan(tilt_x)
        z += y * math.tan(tilt_y)
        z += random.uniform(-0.05, 0.05) * top_height
        
        ring5_verts.append(bm.verts.new((x, y, z)))
    
    bm.verts.ensure_lookup_table()
    
    # Create bottom face
    bm.faces.new(ring1_verts)
    
    # Create side faces between rings
    # Ring 1 to Ring 2
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([ring1_verts[i], ring1_verts[next_i], 
                      ring2_verts[next_i], ring2_verts[i]])
    
    # Ring 2 to Ring 3
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([ring2_verts[i], ring2_verts[next_i], 
                      ring3_verts[next_i], ring3_verts[i]])
    
    # Ring 3 to Ring 4
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([ring3_verts[i], ring3_verts[next_i], 
                      ring4_verts[next_i], ring4_verts[i]])
    
    # Ring 4 to Ring 5
    for i in range(segments):
        next_i = (i + 1) % segments
        bm.faces.new([ring4_verts[i], ring4_verts[next_i], 
                      ring5_verts[next_i], ring5_verts[i]])
    
    # Create top face (slanted plateau surface)
    bm.faces.new(ring5_verts)
    
    bm.to_mesh(mesh)
    bm.free()
    
    # Flat shading for low-poly rocky look
    for face in mesh.polygons:
        face.use_smooth = False
    
    return obj


class PlateauGeneratorProperties(bpy.types.PropertyGroup):
    """Properties for plateau generator"""
    base_radius: bpy.props.FloatProperty(
        name="Base Radius",
        description="Radius of the rocky base",
        default=2.0,
        min=0.5,
        max=10.0
    )
    
    base_height: bpy.props.FloatProperty(
        name="Base Height",
        description="Height of the rocky base/support",
        default=1.0,
        min=0.2,
        max=5.0
    )
    
    top_height: bpy.props.FloatProperty(
        name="Plateau Height",
        description="Height of the flat plateau section",
        default=0.8,
        min=0.1,
        max=3.0
    )
    
    tilt_amount: bpy.props.FloatProperty(
        name="Tilt Amount",
        description="How much the plateau top tilts",
        default=0.3,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    segments: bpy.props.IntProperty(
        name="Segments",
        description="Number of sides (lower = more angular/rocky)",
        default=8,
        min=5,
        max=16
    )
    
    top_overhang: bpy.props.FloatProperty(
        name="Top Overhang",
        description="How much the top overhangs the base (1.0 = no overhang)",
        default=1.4,
        min=0.8,
        max=2.0
    )
    
    elongation: bpy.props.FloatProperty(
        name="Elongation",
        description="How much longer one axis is than the other (1.0 = circular, 2.0 = twice as long)",
        default=1.6,
        min=1.0,
        max=3.0
    )
    
    overhang_shift: bpy.props.FloatProperty(
        name="Overhang Shift",
        description="How much the top shifts toward the higher tilted side (0=centered, 1=dramatic overhang)",
        default=0.7,
        min=0.0,
        max=1.5,
        subtype='FACTOR'
    )
    
    top_width: bpy.props.FloatProperty(
        name="Top Rock Width",
        description="Width of the top boulder section (0.5=narrow, 1.0=wide)",
        default=0.8,
        min=0.3,
        max=1.5
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
        description="Remove previous plateau before generating",
        default=False
    )


class OBJECT_OT_generate_plateau(bpy.types.Operator):
    """Generate Orange Substrate Plateau Boulder"""
    bl_idname = "object.generate_plateau"
    bl_label = "Generate Plateau"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        props = context.scene.plateau_props
        
        # Clear existing if requested
        if props.clear_existing:
            for obj in bpy.data.objects:
                if obj.name.startswith("PlateauBoulder"):
                    bpy.data.objects.remove(obj, do_unlink=True)
        
        # Set random seed
        random.seed(props.random_seed)
        
        # Random tilt direction
        tilt_angle = random.uniform(0, math.pi * 2)
        tilt_x = math.cos(tilt_angle) * props.tilt_amount * 0.4
        tilt_y = math.sin(tilt_angle) * props.tilt_amount * 0.4
        
        # Generate plateau
        obj = create_plateau_boulder(
            base_radius=props.base_radius,
            base_height=props.base_height,
            top_height=props.top_height,
            tilt_x=tilt_x,
            tilt_y=tilt_y,
            segments=props.segments,
            top_scale=props.top_overhang,
            elongation=props.elongation,
            overhang_shift=props.overhang_shift,
            top_width=props.top_width
        )
        
        # Add to scene
        context.collection.objects.link(obj)
        
        self.report({'INFO'}, "Created plateau boulder")
        return {'FINISHED'}


class VIEW3D_PT_plateau_generator(bpy.types.Panel):
    """Creates a Panel in the 3D Viewport N-panel"""
    bl_label = "Plateau Boulder Generator"
    bl_idname = "VIEW3D_PT_plateau_generator"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Plateau Gen'
    
    def draw(self, context):
        layout = self.layout
        props = context.scene.plateau_props
        
        # Draw all the property sliders
        layout.prop(props, "base_radius")
        layout.prop(props, "base_height")
        layout.prop(props, "top_height")
        layout.prop(props, "tilt_amount", slider=True)
        layout.prop(props, "segments")
        layout.prop(props, "top_overhang", slider=True)
        layout.prop(props, "top_width", slider=True)
        layout.prop(props, "elongation", slider=True)
        layout.prop(props, "overhang_shift", slider=True)
        layout.prop(props, "random_seed")
        layout.prop(props, "clear_existing")
        
        layout.separator()
        
        # Generate button
        layout.operator("object.generate_plateau")


def register():
    bpy.utils.register_class(PlateauGeneratorProperties)
    bpy.utils.register_class(OBJECT_OT_generate_plateau)
    bpy.utils.register_class(VIEW3D_PT_plateau_generator)
    bpy.types.Scene.plateau_props = bpy.props.PointerProperty(type=PlateauGeneratorProperties)


def unregister():
    bpy.utils.unregister_class(VIEW3D_PT_plateau_generator)
    bpy.utils.unregister_class(OBJECT_OT_generate_plateau)
    bpy.utils.unregister_class(PlateauGeneratorProperties)
    del bpy.types.Scene.plateau_props


def main():
    """Main function for running as script"""
    random.seed(42)
    
    obj = create_plateau_boulder(
        base_radius=2.0,
        base_height=1.0,
        top_height=0.8,
        tilt_x=0.2,
        tilt_y=0.15,
        segments=8,
        top_scale=1.1
    )
    
    bpy.context.collection.objects.link(obj)
    print("Plateau boulder created!")


# Run the script
if __name__ == "__main__":
    register()
    print("Plateau Generator registered! Find it in the N-panel under 'Plateau Gen' tab")
