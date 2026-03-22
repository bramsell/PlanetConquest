import bpy
import bmesh
import math
import random
from mathutils import Vector, Matrix

def create_palm_trunk(
    height=8.0,
    base_radius=0.3,
    top_radius=0.25,
    curve_amount=0.5,
    curve_direction=0.0,
    segments=16,
    height_segments=20
):
    """
    Create a curved palm-like trunk
    
    Args:
        height: Total height of the trunk
        base_radius: Radius at the base
        top_radius: Radius at the top
        curve_amount: How much the trunk curves (0=straight, 1=heavy curve)
        curve_direction: Direction of curve in radians
        segments: Circular resolution
        height_segments: Vertical resolution
    """
    mesh = bpy.data.meshes.new("PalmTrunk")
    obj = bpy.data.objects.new("PalmTrunk", mesh)
    bpy.context.collection.objects.link(obj)
    
    bm = bmesh.new()
    
    # Create rings of vertices from bottom to top
    rings = []
    for ring_idx in range(height_segments + 1):
        t = ring_idx / height_segments
        
        # Height with curve
        z = t * height
        
        # Curve displacement (progressive lean, not sine wave)
        # This makes trunk lean in one direction with increasing offset toward top
        curve_offset = curve_amount * height * 0.3 * (t ** 1.5)
        x_offset = math.cos(curve_direction) * curve_offset
        y_offset = math.sin(curve_direction) * curve_offset
        
        # Radius interpolation
        radius = base_radius * (1 - t) + top_radius * t
        
        # Add slight bulge in middle for organic look
        bulge = 1.0 + math.sin(t * math.pi) * 0.1
        radius *= bulge
        
        ring_verts = []
        for i in range(segments):
            angle = (math.pi * 2 * i) / segments
            x = math.cos(angle) * radius + x_offset
            y = math.sin(angle) * radius + y_offset
            vert = bm.verts.new((x, y, z))
            ring_verts.append(vert)
        rings.append(ring_verts)
    
    bm.verts.ensure_lookup_table()
    
    # Create faces between rings
    for ring_idx in range(len(rings) - 1):
        for i in range(segments):
            next_i = (i + 1) % segments
            v1 = rings[ring_idx][i]
            v2 = rings[ring_idx][next_i]
            v3 = rings[ring_idx + 1][next_i]
            v4 = rings[ring_idx + 1][i]
            bm.faces.new([v1, v2, v3, v4])
    
    # Cap bottom
    bm.faces.new(rings[0])
    
    bm.to_mesh(mesh)
    bm.free()
    
    mesh.update()
    return obj

def create_feather_leaf(
    length=3.0,
    spine_thickness=0.05,
    num_branches=12,
    branch_length=1.0,
    branch_thickness=0.02,
    branch_taper=0.5,
    forward_curve=0.3,
    leaf_droop=0.3,
    branch_angle=45.0
):
    """
    Create a single feather/coral-fan leaf with central spine and branches
    
    Args:
        length: Length of the central spine
        spine_thickness: Thickness of the spine
        num_branches: Number of branches per side
        branch_length: Maximum length of branches (at widest point)
        branch_thickness: Thickness of branches
        branch_taper: How much branches taper toward spine tip (0-1)
        forward_curve: How much branches curve forward
        leaf_droop: How much the whole leaf droops
        branch_angle: Angle branches stick out from spine (degrees)
    """
    mesh = bpy.data.meshes.new("FeatherLeaf")
    obj = bpy.data.objects.new("FeatherLeaf", mesh)
    bpy.context.collection.objects.link(obj)
    
    bm = bmesh.new()
    
    # Create central spine
    spine_segments = num_branches + 4
    spine_verts = []
    
    for i in range(spine_segments + 1):
        t = i / spine_segments
        
        # Spine position along length with droop curve
        x = 0
        y = 0
        z = t * length
        
        # Droop curve (parabolic, heaviest at tip)
        droop_offset = -leaf_droop * length * (t ** 2)
        z += droop_offset
        
        # Spine radius (tapers toward tip)
        spine_rad = spine_thickness * (1.0 - t * 0.7)
        
        spine_verts.append({'pos': Vector((x, y, z)), 'radius': spine_rad, 't': t})
    
    # Create branches coming off the spine
    branches = []
    branch_angle_rad = math.radians(branch_angle)
    
    for branch_idx in range(num_branches):
        # Position along spine (skip base, concentrate in middle/upper area)
        t = 0.2 + (branch_idx / (num_branches - 1)) * 0.8 if num_branches > 1 else 0.5
        
        # Find position on spine
        spine_idx = int(t * spine_segments)
        spine_pos = spine_verts[spine_idx]['pos']
        
        # Branch gets smaller toward tip of leaf
        size_factor = 1.0 - (t - 0.2) / 0.8  # Full size at base, smaller at tip
        size_factor = size_factor ** (1.0 - branch_taper)  # Control taper rate
        
        current_branch_length = branch_length * size_factor
        
        # Create branch on both sides
        for side in [-1, 1]:
            branch_verts = []
            branch_segments = 4
            
            for seg in range(branch_segments + 1):
                seg_t = seg / branch_segments
                
                # Base angle plus forward curve (negative = curves forward/upward in Z)
                angle = branch_angle_rad - forward_curve * seg_t * 0.5
                
                # Position along branch
                branch_dist = seg_t * current_branch_length
                
                # Branch extends perpendicular to spine (in Y direction, not X)
                # This way when leaf is rotated, branches will be horizontal
                local_x = 0
                local_y = math.sin(angle) * branch_dist * side  # Side to side
                local_z = math.cos(angle) * branch_dist  # Forward curve
                
                world_x = spine_pos.x + local_x
                world_y = spine_pos.y + local_y
                world_z = spine_pos.z + local_z
                
                # Branch thickness (tapers to tip)
                thick = branch_thickness * (1.0 - seg_t * 0.8)
                
                branch_verts.append({
                    'pos': Vector((world_x, world_y, world_z)),
                    'thickness': thick
                })
            
            branches.append(branch_verts)
    
    # Now create the actual mesh geometry
    # Spine as thin cylinder
    for i in range(len(spine_verts) - 1):
        pos1 = spine_verts[i]['pos']
        pos2 = spine_verts[i + 1]['pos']
        rad = spine_verts[i]['radius']
        
        # Simple box for spine segment
        v1 = bm.verts.new(pos1 + Vector((-rad, -rad, 0)))
        v2 = bm.verts.new(pos1 + Vector((rad, -rad, 0)))
        v3 = bm.verts.new(pos2 + Vector((rad, -rad, 0)))
        v4 = bm.verts.new(pos2 + Vector((-rad, -rad, 0)))
        bm.faces.new([v1, v2, v3, v4])
    
    # Branches as thin strips
    for branch in branches:
        for i in range(len(branch) - 1):
            pos1 = branch[i]['pos']
            pos2 = branch[i + 1]['pos']
            thick = branch[i]['thickness']
            
            # Create thin rectangular strip with thickness in X direction
            # (since branches now extend in Y)
            v1 = bm.verts.new(pos1 + Vector((-thick, 0, 0)))
            v2 = bm.verts.new(pos1 + Vector((thick, 0, 0)))
            v3 = bm.verts.new(pos2 + Vector((thick, 0, 0)))
            v4 = bm.verts.new(pos2 + Vector((-thick, 0, 0)))
            bm.faces.new([v1, v2, v3, v4])
    
    bm.to_mesh(mesh)
    bm.free()
    
    mesh.update()
    return obj

def create_alien_tree(
    trunk_height=8.0,
    trunk_radius=0.3,
    trunk_curve=0.5,
    num_leaves=8,
    leaf_length=3.0,
    leaf_spread=60.0,
    leaf_rotation=0.0,
    num_branches=12,
    branch_length=1.0,
    branch_angle=45.0,
    forward_curve=0.3,
    leaf_droop=0.3,
    random_seed=42
):
    """
    Create complete alien palm tree with feather leaves
    
    Args:
        trunk_height: Height of the trunk
        trunk_radius: Base radius of trunk
        trunk_curve: How curved the trunk is
        num_leaves: Number of leaves at the top
        leaf_length: Length of each leaf spine
        leaf_spread: Angle spread of leaves (degrees)
        leaf_rotation: Rotation of leaf on its spine axis (degrees)
        num_branches: Branches per leaf
        branch_length: Length of branches on leaves
        branch_angle: Base angle branches stick out from spine (degrees)
        forward_curve: How much branches curve forward
        leaf_droop: How much leaves droop
        random_seed: Seed for randomization
    """
    random.seed(random_seed)
    
    # Random trunk curve direction
    curve_direction = random.uniform(0, math.pi * 2)
    
    # Create trunk
    trunk = create_palm_trunk(
        height=trunk_height,
        base_radius=trunk_radius,
        top_radius=trunk_radius * 0.85,
        curve_amount=trunk_curve,
        curve_direction=curve_direction
    )
    
    # Create leaves arranged around top of trunk
    leaves = []
    leaf_spread_rad = math.radians(leaf_spread)
    leaf_rotation_rad = math.radians(leaf_rotation)
    
    # Calculate actual top position of curved trunk (t=1.0)
    top_curve_offset = trunk_curve * trunk_height * 0.3 * (1.0 ** 1.5)
    top_x = math.cos(curve_direction) * top_curve_offset
    top_y = math.sin(curve_direction) * top_curve_offset
    
    # Trunk lean angle (for rotating leaf crown)
    trunk_lean_angle = trunk_curve * 0.4  # Lean the whole crown
    
    for i in range(num_leaves):
        # Create leaf
        leaf = create_feather_leaf(
            length=leaf_length,
            num_branches=num_branches,
            branch_length=branch_length,
            forward_curve=forward_curve,
            leaf_droop=leaf_droop,
            branch_angle=branch_angle
        )
        
        # Angle around the trunk
        angle = (math.pi * 2 * i) / num_leaves
        
        # Position at top of curved trunk
        leaf.location = (top_x, top_y, trunk_height)
        
        # Jellyfish-like radial spread - mostly horizontal with slight variation
        # Base spread angle (mostly horizontal, slightly down)
        base_spread = leaf_spread_rad
        
        # Small random tilt up or down for variation between leaves
        vertical_variation = random.uniform(-0.3, 0.3)
        
        # Set rotations - order matters! (applied as XYZ)
        # First: rotate 90 degrees so branches will be horizontal
        # Second: tilt outward (spread)
        # Third: rotate around trunk to position radially
        leaf.rotation_euler = (
            leaf_rotation_rad + random.uniform(-0.1, 0.1),  # X: rotation for horizontal branches
            -base_spread + vertical_variation,              # Y: outward tilt
            angle                                            # Z: position around trunk
        )
        
        leaves.append(leaf)
    
    # Select all created objects
    bpy.ops.object.select_all(action='DESELECT')
    trunk.select_set(True)
    for leaf in leaves:
        leaf.select_set(True)
    bpy.context.view_layer.objects.active = trunk
    
    # Join into single object
    bpy.ops.object.join()
    final_obj = bpy.context.active_object
    final_obj.name = "AlienTree"
    
    # Center at origin
    final_obj.location = (0, 0, 0)
    
    return final_obj

# Blender UI Properties
class AlienTreeGeneratorProperties(bpy.types.PropertyGroup):
    trunk_height: bpy.props.FloatProperty(
        name="Trunk Height",
        description="Height of the trunk",
        default=8.0,
        min=2.0,
        max=20.0
    )
    
    trunk_radius: bpy.props.FloatProperty(
        name="Trunk Radius",
        description="Base radius of the trunk",
        default=0.3,
        min=0.1,
        max=1.0
    )
    
    trunk_curve: bpy.props.FloatProperty(
        name="Trunk Curve",
        description="How much the trunk curves",
        default=0.5,
        min=0.0,
        max=1.5,
        subtype='FACTOR'
    )
    
    num_leaves: bpy.props.IntProperty(
        name="Number of Leaves",
        description="Number of leaves at the top",
        default=8,
        min=4,
        max=16
    )
    
    leaf_length: bpy.props.FloatProperty(
        name="Leaf Length",
        description="Length of the leaf spine",
        default=3.0,
        min=1.0,
        max=8.0
    )
    
    leaf_spread: bpy.props.FloatProperty(
        name="Leaf Spread",
        description="How much leaves spread outward (degrees)",
        default=60.0,
        min=0.0,
        max=90.0
    )
    
    leaf_rotation: bpy.props.FloatProperty(
        name="Leaf Rotation",
        description="Rotation of leaf on its spine axis (degrees)",
        default=0.0,
        min=0.0,
        max=360.0
    )
    
    num_branches: bpy.props.IntProperty(
        name="Branches Per Leaf",
        description="Number of branches on each side of leaf",
        default=12,
        min=4,
        max=30
    )
    
    branch_length: bpy.props.FloatProperty(
        name="Branch Length",
        description="Maximum length of branches",
        default=1.0,
        min=0.3,
        max=3.0
    )
    
    branch_angle: bpy.props.FloatProperty(
        name="Branch Angle",
        description="Base angle branches stick out from spine (degrees)",
        default=45.0,
        min=0.0,
        max=90.0
    )
    
    forward_curve: bpy.props.FloatProperty(
        name="Forward Curve",
        description="How much branches curve forward",
        default=0.3,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    leaf_droop: bpy.props.FloatProperty(
        name="Leaf Droop",
        description="How much leaves droop downward",
        default=0.3,
        min=0.0,
        max=1.0,
        subtype='FACTOR'
    )
    
    random_seed: bpy.props.IntProperty(
        name="Random Seed",
        description="Seed for randomization",
        default=42,
        min=0,
        max=10000
    )

# Operator
class OBJECT_OT_generate_alien_tree(bpy.types.Operator):
    bl_idname = "object.generate_alien_tree"
    bl_label = "Generate Alien Tree"
    bl_description = "Generate a procedural alien palm tree with feather leaves"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        props = context.scene.alien_tree_props
        
        # Generate tree
        obj = create_alien_tree(
            trunk_height=props.trunk_height,
            trunk_radius=props.trunk_radius,
            trunk_curve=props.trunk_curve,
            num_leaves=props.num_leaves,
            leaf_length=props.leaf_length,
            leaf_spread=props.leaf_spread,
            leaf_rotation=props.leaf_rotation,
            num_branches=props.num_branches,
            branch_length=props.branch_length,
            branch_angle=props.branch_angle,
            forward_curve=props.forward_curve,
            leaf_droop=props.leaf_droop,
            random_seed=props.random_seed
        )
        
        self.report({'INFO'}, f"Generated alien tree: {obj.name}")
        return {'FINISHED'}

# UI Panel
class VIEW3D_PT_alien_tree_generator(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Alien Tree"
    bl_label = "Alien Tree Generator"
    
    def draw(self, context):
        layout = self.layout
        props = context.scene.alien_tree_props
        
        layout.label(text="Trunk:", icon='OUTLINER_OB_CURVE')
        layout.prop(props, "trunk_height")
        layout.prop(props, "trunk_radius")
        layout.prop(props, "trunk_curve", slider=True)
        
        layout.separator()
        
        layout.label(text="Leaves:", icon='OUTLINER_OB_FORCE_FIELD')
        layout.prop(props, "num_leaves")
        layout.prop(props, "leaf_length")
        layout.prop(props, "leaf_spread")
        layout.prop(props, "leaf_rotation")
        layout.prop(props, "leaf_droop", slider=True)
        
        layout.separator()
        
        layout.label(text="Leaf Details:", icon='MESH_DATA')
        layout.prop(props, "num_branches")
        layout.prop(props, "branch_length")
        layout.prop(props, "branch_angle")
        layout.prop(props, "forward_curve", slider=True)
        
        layout.separator()
        
        layout.label(text="Randomization:", icon='FORCE_TURBULENCE')
        layout.prop(props, "random_seed")
        
        layout.separator()
        
        layout.operator("object.generate_alien_tree", icon='MESH_UVSPHERE')

# Registration
def register():
    bpy.utils.register_class(AlienTreeGeneratorProperties)
    bpy.utils.register_class(OBJECT_OT_generate_alien_tree)
    bpy.utils.register_class(VIEW3D_PT_alien_tree_generator)
    bpy.types.Scene.alien_tree_props = bpy.props.PointerProperty(type=AlienTreeGeneratorProperties)

def unregister():
    bpy.utils.unregister_class(VIEW3D_PT_alien_tree_generator)
    bpy.utils.unregister_class(OBJECT_OT_generate_alien_tree)
    bpy.utils.unregister_class(AlienTreeGeneratorProperties)
    del bpy.types.Scene.alien_tree_props

if __name__ == "__main__":
    register()
