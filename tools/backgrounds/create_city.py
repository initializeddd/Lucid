"""Build Lucid's city-transit scene as real 3D geometry in Blender.

Run with: blender --background --python tools/backgrounds/create_city.py -- output.blend
No downloaded assets or image textures are used. The .blend is the editable
source of the modeled background; the runtime export is a separate step.
"""
import bpy
import math
import random
import sys
import json
from pathlib import Path
from mathutils import Vector

random.seed(37)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.render.resolution_x = 960
scene.render.resolution_y = 600
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.frame_start, scene.frame_end = 1, 360


def material(name, color, emission=0.0):
    m = bpy.data.materials.new(name)
    m['lucid_emission'] = emission
    m.diffuse_color = (*color, 1.0)
    m.use_nodes = True
    shader = m.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1.0)
    shader.inputs['Roughness'].default_value = 0.78
    if emission:
        shader.inputs['Emission Color'].default_value = (*color, 1.0)
        shader.inputs['Emission Strength'].default_value = emission
    return m

stone = material('Graphite masonry', (0.13, 0.15, 0.22))
ground = material('City ground', (0.075, 0.085, 0.12))
stone_light = material('Masonry trim', (0.25, 0.28, 0.38))
metal = material('Rail steel', (0.23, 0.26, 0.34))
glass = material('Dark glass', (0.07, 0.09, 0.15))
window = material('Warm windows', (0.55, 0.60, 0.82), 0.7)
train_body = material('Train body', (0.38, 0.42, 0.58))
train_window = material('Train interior', (0.65, 0.70, 0.96), 1.2)
aircraft_light = material('Aircraft lights', (0.75, 0.79, 0.98), 1.1)
sign_light = material('Sign glow', (0.66, 0.71, 0.90), 0.8)


def block(name, location, scale, mat, bevel=0.0, parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = obj.modifiers.new('Soft manufactured edges', 'BEVEL')
        mod.width = min(bevel, min(scale) * 0.24)
        mod.segments = 2
        mod.affect = 'EDGES'
        obj.modifiers.new('Weighted normals', 'WEIGHTED_NORMAL')
    obj.data.materials.append(mat)
    if parent:
        obj.parent = parent
    return obj


def building(x, y, w, d, h, index):
    # Align window centres to the shader's cell grid, so every face of a
    # window shares one lighting phase instead of flickering in fragments.
    columns = max(2, int((w-0.3)/0.62))
    first = x - (columns-1)*.305
    x += round(first/.61)*.61 - first
    block(f'Building {index}', (x, y, h/2), (w, d, h), stone, 0.04)
    # Cornice, recessed shopfront, window frames and rooftop details add depth.
    block(f'Cornice {index}', (x, y, h-0.12), (w+0.15, d+0.15, 0.22), stone_light, 0.025)
    block(f'Roof {index}', (x, y, h+0.04), (w+0.08, d+0.08, 0.12), metal, 0.02)
    floors = max(2, int((h-0.7)/0.75))
    columns = max(2, int((w-0.3)/0.62))
    for floor in range(floors):
        for column in range(columns):
            px = x + (column-(columns-1)/2)*0.61
            z = 0.74 + floor*0.74
            lit = random.random() < 0.42
            block('Window recess', (px, y-d/2-0.018, z), (0.42, 0.05, 0.52), stone_light)
            block('Window glass', (px, y-d/2-0.048, z), (0.32, 0.025, 0.42), window)
            block('Window sill', (px, y-d/2-0.08, z-0.26), (0.48, 0.14, 0.055), metal)
            block('Window mullion', (px, y-d/2-0.068, z), (0.028, 0.025, 0.42), metal)
    if index % 2 == 0:
        block('Roof machinery', (x+w*0.16, y, h+0.35), (w*0.35, d*0.35, 0.6), metal, 0.03)
        block('Antenna mast', (x-w*0.27, y, h+0.8), (0.045, 0.045, 1.55), metal)
        block('Antenna crossbar', (x-w*0.27, y, h+1.12), (0.55, 0.04, 0.04), metal)
    if index in (5, 12):
        # A rooftop water tank and its support frame add a recognizable
        # silhouette among the existing antennae and machinery.
        for dx in (-.30,.30):
            for dy in (-.25,.25):
                block('Water tank support',(x+dx,y+dy,h+.33),(.06,.06,.58),metal)
        bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.44,depth=.72,location=(x,y,h+.90))
        tank=bpy.context.object; tank.name='Rooftop water tank'; tank.data.materials.append(stone_light)
        block('Tank roof',(x,y,h+1.29),(.94,.94,.12),metal,.02)
    if index in (6, 7):
        # A small illuminated rooftop sign gives the near buildings identity.
        for dx in (-.46,.46):
            block('Sign support',(x+dx,y-d*.25,h+.30),(.06,.06,.58),metal)
        block('Sign frame',(x,y-d*.25,h+.65),(1.45,.12,.62),metal,.02)
        block('Sign glow',(x,y-d*.25-.075,h+.65),(1.24,.025,.42),sign_light)
        block('Sign mark upright',(x-.12,y-d*.25-.095,h+.65),(.045,.02,.28),metal)
        block('Sign mark baseline',(x+.01,y-d*.25-.095,h+.53),(.30,.02,.045),metal)

for i, (x,y,w,d,h) in enumerate([
    (-9.1, 2.5, 2.6, 2.2, 5.6), (-6.2, 3.0, 2.7, 2.0, 4.7),
    (-3.3, 3.3, 2.8, 2.1, 3.8), (3.3, 3.2, 2.6, 2.2, 4.5),
    (6.2, 2.7, 2.7, 2.3, 5.5), (9.1, 2.2, 2.7, 2.1, 4.1),
    (-7.6, -2.9, 2.0, 1.5, 3.4), (7.5, -3.0, 2.0, 1.5, 3.7),
    (-12.0, 2.5, 2.7, 2.2, 4.7), (-15.0, 3.0, 2.8, 2.0, 5.5),
    (-18.0, 2.5, 2.8, 2.2, 4.3), (-21.0, 3.0, 2.8, 2.1, 5.2),
    (12.0, 2.7, 2.8, 2.1, 4.8), (15.0, 3.0, 2.8, 2.1, 5.4),
    (18.0, 2.5, 2.8, 2.2, 4.2), (21.0, 3.0, 2.8, 2.1, 5.0),
]):
    building(x,y,w,d,h,i)

# A broad city floor anchors buildings and viaduct supports to the same world.
block('City ground', (0,1,-0.16), (120,40,0.28), ground)
# Elevated track continues well beyond the visible frame on both sides.
# Elevated track: ballast, rails, ties, upright piers and supporting brackets.
block('Viaduct deck', (0,-0.55,1.72), (100,1.8,0.32), stone_light, 0.04)
for y in (-1.18, 0.08):
    block('Steel rail', (0,y,1.95), (100,0.07,0.09), metal)
for x in [i*0.34 for i in range(-147,148)]:
    block('Rail sleeper', (x,-0.55,1.89), (0.12,1.6,0.09), metal)
for x in [i * 3.5 for i in range(-14,15)]:
    block('Viaduct pier', (x,-0.55,0.78), (0.38,1.1,1.56), stone, 0.03)
    block('Pier capital', (x,-0.55,1.51), (0.8,1.5,0.18), stone_light, 0.02)

train = bpy.data.objects.new('Animated train', None)
scene.collection.objects.link(train)
for carriage in range(3):
    cx = carriage * 2.5
    block('Carriage body', (cx,0,2.35), (2.35,1.0,0.72), train_body, 0.08, train)
    block('Carriage roof', (cx,0,2.75), (2.28,0.94,0.13), metal, 0.04, train)
    for j in range(5):
        block('Train window', (cx-0.86+j*0.43,-0.515,2.42), (0.30,0.025,0.30), train_window, 0.02, train)
    for wx in (-0.75,0.75):
        for wy in (-0.4,0.4):
            bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.14, depth=0.08, location=(cx+wx,wy,1.99), rotation=(math.pi/2,0,0))
            wheel=bpy.context.object; wheel.name='Train wheel'; wheel.parent=train; wheel.data.materials.append(metal)
# A small distant aircraft periodically crosses the city above the roofs.
plane = bpy.data.objects.new('Passing aircraft', None)
scene.collection.objects.link(plane)
block('Aircraft fuselage', (0, 3.5, 8.5), (1.4, .16, .16), stone_light, parent=plane)
block('Aircraft wing', (0, 3.5, 8.5), (.42, 1.05, .05), metal, parent=plane)
block('Aircraft tail', (-.55, 3.5, 8.65), (.20, .08, .30), metal, parent=plane)
block('Aircraft navigation light', (.69, 3.5, 8.5), (.07, .18, .12), aircraft_light, parent=plane)
train.location = (-13,-0.55,0)
train.keyframe_insert(data_path='location', frame=1)
train.location = (13,-0.55,0)
train.keyframe_insert(data_path='location', frame=360)
bpy.ops.object.camera_add(location=(16,-24,15))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,2.0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO'; camera.data.ortho_scale=23
scene.camera=camera
bpy.ops.object.light_add(type='AREA', location=(-5,-8,15))
light=bpy.context.object; light.data.energy=1500; light.data.size=12
scene.world.color=(0.07,0.07,0.10)
scene.frame_set(120)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('assets/backgrounds/city-transit.blend')
output.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1] != '-':
    scene.render.filepath=str(Path(args[1]).resolve())
    bpy.ops.render.render(write_still=True)
if len(args)>2:
    # Export each material and animation group independently so the runtime can
    # shade and move the train without needing an image or a Blender installation.
    export_dir = Path(args[2]).resolve()
    export_dir.mkdir(parents=True, exist_ok=True)
    scene.frame_set(1)
    train.location.x = 0
    train.location.y = 0
    bpy.context.view_layer.update()
    groups = {}
    for obj in scene.objects:
        if obj.type != 'MESH' or not obj.data.materials:
            continue
        animated = obj.parent == train
        key = ('train' if animated else 'aircraft' if obj.parent == plane else 'city', obj.data.materials[0].name)
        groups.setdefault(key, []).append(obj)
    manifest = []
    for (group, material_name), objects in sorted(groups.items()):
        filename = group + '-' + material_name.lower().replace(' ', '-') + '.obj'
        bpy.ops.object.select_all(action='DESELECT')
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        bpy.ops.wm.obj_export(filepath=str(export_dir / filename), export_selected_objects=True,
                              export_materials=False, export_uv=False, forward_axis='NEGATIVE_Z', up_axis='Y')
        material = objects[0].data.materials[0]
        manifest.append({'mesh': filename, 'group': group, 'color': list(material.diffuse_color), 'emission': float(material.get('lucid_emission', 0.0))})
    (export_dir / 'scene.json').write_text(json.dumps({'parts': manifest}, indent=2) + '\n')
