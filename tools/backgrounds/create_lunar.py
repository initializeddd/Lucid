"""Model the lunar-horizon background without external textures or assets.

blender --background --python tools/backgrounds/create_lunar.py -- output.blend preview.png output-directory
The optional preview can be '-' to skip rendering. Geometry is exported by material
and animation group as OBJ, with a small runtime manifest.
"""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scene_meteor import add_meteor

random.seed(214)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.render.resolution_x, scene.render.resolution_y = 960, 600
scene.render.film_transparent = True
scene.frame_start, scene.frame_end = 1, 360


def material(name, rgb, glow=0):
    mat = bpy.data.materials.new(name)
    mat['lucid_emission'] = glow
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*rgb, 1)
    shader.inputs['Roughness'].default_value = 0.82
    if glow:
        shader.inputs['Emission Color'].default_value = (*rgb, 1)
        shader.inputs['Emission Strength'].default_value = glow
    return mat

regolith = material('Lunar regolith', (.21, .23, .32))
rim = material('Crater rock', (.30, .33, .44))
rock = material('Basalt', (.14, .16, .23))
earth = material('Earth ocean', (.14, .19, .35))
continent = material('Earth land', (.35, .39, .50))
solar = material('Solar cells', (.12, .17, .31))
metal = material('Satellite metal', (.43, .48, .62))
shine = material('Solar frame', (.58, .63, .77), .15)
starlight = material('Starlight', (.67, .71, .90), .8)


def box(name, loc, scale, mat, parent=None, bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj=bpy.context.object; obj.name=name; obj.dimensions=scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod=obj.modifiers.new('Edge bevel','BEVEL'); mod.width=min(bevel,min(scale)*.2); mod.segments=2
        obj.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
    obj.data.materials.append(mat)
    if parent: obj.parent=parent
    return obj


def terrain_height(x,y):
    value = .12*math.sin(x*.8+y*.3)+.08*math.sin(y*1.4-x*.6)
    for cx,cy,r,depth in [(-32,-1,1.4,.40),(-26,2,1.1,.33),(-20,-3,1.6,.44),(-14,1,1.2,.36),
                          (-5,-2,1.5,.48),(-.5,-.7,1.1,.34),(4.1,1.0,1.6,.48),(-8,3,.85,.26),(7,-3,.9,.3),
                          (13,2,1.5,.40),(19,-2,1.2,.36),(25,1,1.5,.42),(32,-3,1.3,.38)]:
        distance=math.hypot(x-cx,y-cy)/r
        value -= depth*max(0,1-distance*distance)**2
        value += .14*math.exp(-((distance-1.05)/.16)**2)
    return value

# A continuous subdivided surface with actual concave craters, not a flat decal.
verts=[]; faces=[]; nx=201; ny=51
for j in range(ny):
    y=-5.5+j*11/(ny-1)
    for i in range(nx):
        # Extend beyond the camera on both sides so the surface forms a
        # horizon rather than looking like a floating rectangular platform.
        x=-40+i*80/(nx-1)
        z=terrain_height(x,y)+random.uniform(-.035,.035)
        verts.append((x,y,z))
for j in range(ny-1):
    for i in range(nx-1):
        k=j*nx+i; faces.append((k,k+1,k+nx+1,k+nx))
mesh=bpy.data.meshes.new('Cratered surface'); mesh.from_pydata(verts,[],faces); mesh.update()
ground=bpy.data.objects.new('Cratered lunar ground',mesh); scene.collection.objects.link(ground); ground.data.materials.append(regolith)
for i in range(135):
    x=random.uniform(-36,36); y=random.uniform(-5,5); r=random.uniform(.08,.29)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=r, location=(x,y,terrain_height(x,y)+r*.25))
    obj=bpy.context.object; obj.name='Lunar boulder'; obj.scale=(1.3,.9,.65); obj.rotation_euler[2]=random.random()*math.pi; obj.data.materials.append(rock if i%3 else rim)

# Planet and sparse raised land shapes, all geometry so it survives offline export.
bpy.ops.mesh.primitive_uv_sphere_add(segments=36, ring_count=18, radius=1.55, location=(6.6,5.0,4.3))
planet=bpy.context.object; planet.name='Distant Earth'; planet.data.materials.append(earth)
for i in range(22):
    theta=random.uniform(0,2*math.pi); phi=random.uniform(.35,2.75)
    normal=Vector((math.sin(phi)*math.cos(theta),math.sin(phi)*math.sin(theta),math.cos(phi)))
    point=planet.location+normal*1.54
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=random.uniform(.15,.33), location=point)
    island=bpy.context.object; island.name='Continental patch'; island.scale=(1.1,.8,.34); island.rotation_euler=normal.to_track_quat('Z','Y').to_euler(); island.data.materials.append(continent)

# Sparse, real 3D stars extend past the visible area on wide windows. Their
# brightness varies independently in the runtime shader.
for i in range(150):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=random.uniform(.025,.045),
        location=(random.uniform(-34,34),random.uniform(7,11),random.uniform(-8,28)))
    star=bpy.context.object; star.name='Lunar sky star'; star.data.materials.append(starlight)

meteor=add_meteor(scene, starlight)

sat=bpy.data.objects.new('Animated satellite', None); scene.collection.objects.link(sat)
sat.rotation_euler = (math.radians(8), math.radians(-12), math.radians(14))
box('Satellite core',(0,0,0),(.65,.52,.5),metal,sat,.05)
for side in (-1,1):
    cx=side*1.25
    box('Solar panel',(cx,0,0),(1.65,.08,.64),solar,sat)
    for n in range(5): box('Solar cell divider',(cx+(n-2)*.27,-.052,0),(.025,.02,.64),shine,sat)
    for z in (-.2,0,.2): box('Solar cell crossbar',(cx,-.056,z),(1.65,.02,.018),shine,sat)
    box('Panel support',(side*.56,0,0),(.45,.1,.1),metal,sat)
bpy.ops.mesh.primitive_cone_add(vertices=24, radius1=.36, radius2=.07, depth=.22, location=(0,-.39,0), rotation=(math.pi/2,0,0))
dish=bpy.context.object; dish.name='Satellite dish'; dish.parent=sat; dish.data.materials.append(metal)
sat.location=(-5,0,4.8); sat.keyframe_insert(data_path='location',frame=1)
sat.location=(4,0,5.3); sat.keyframe_insert(data_path='location',frame=360)

bpy.ops.object.camera_add(location=(0,-21,12))
camera=bpy.context.object; camera.rotation_euler=(Vector((0,0,2.2))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=22; scene.camera=camera
bpy.ops.object.light_add(type='AREA', location=(-6,-7,13)); light=bpy.context.object; light.data.energy=1700; light.data.size=10
scene.world.color=(.04,.04,.08); scene.frame_set(120)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('/tmp/lucid-lunar.blend'); output.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1]!='-':
    scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
if len(args)>2:
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True)
    scene.frame_set(1); sat.location=(0,0,0); bpy.context.view_layer.update()
    groups={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group='satellite' if obj.parent==sat else 'meteor' if obj.parent==meteor else 'landscape'
        groups.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name), objects in sorted(groups.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'
        bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
