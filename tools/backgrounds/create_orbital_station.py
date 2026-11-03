"""Build the orbital-station background as animated, texture-free 3D geometry."""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scene_meteor import add_meteor

random.seed(679)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=24
scene.render.resolution_x=960; scene.render.resolution_y=600; scene.render.film_transparent=True
scene.frame_start=1; scene.frame_end=360

def material(name,rgb,glow=0):
    m=bpy.data.materials.new(name); m['lucid_emission']=glow; m.diffuse_color=(*rgb,1); m.use_nodes=True
    shader=m.node_tree.nodes.get('Principled BSDF'); shader.inputs['Base Color'].default_value=(*rgb,1)
    shader.inputs['Roughness'].default_value=.72; shader.inputs['Metallic'].default_value=.2
    if glow: shader.inputs['Emission Color'].default_value=(*rgb,1); shader.inputs['Emission Strength'].default_value=glow
    return m
metal=material('Station metal',(.36,.40,.52)); dark=material('Station shadows',(.14,.18,.26))
solar=material('Solar cells',(.16,.21,.33)); bright=material('Windows and lights',(.66,.71,.92),.8)
beacon=material('Navigation beacons',(.88,.91,1.0),1.4)
ocean=material('Planet ocean',(.17,.22,.35)); land=material('Planet continents',(.35,.38,.49)); clouds=material('Planet clouds',(.48,.51,.61))

def block(name,loc,size,mat,parent=None,bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); obj=bpy.context.object; obj.name=name; obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=obj.modifiers.new('Beveled edges','BEVEL'); mod.width=min(bevel,min(size)*.2); mod.segments=2
        obj.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
    obj.data.materials.append(mat)
    if parent: obj.parent=parent
    return obj

def cylinder(name,loc,radius,depth,mat,parent=None,rotation=(0,0,0),vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc,rotation=rotation)
    obj=bpy.context.object; obj.name=name; obj.data.materials.append(mat)
    if parent: obj.parent=parent
    return obj

# The globe extends far below the viewport: only its curved limb is visible.
planet_center=Vector((-42,14,-56)); planet_radius=45
bpy.ops.mesh.primitive_uv_sphere_add(segments=64,ring_count=32,radius=planet_radius,location=planet_center)
planet=bpy.context.object; planet.name='Earth globe'; planet.data.materials.append(ocean)
# Raised, irregular patches are real geometry rather than a texture map.
for i in range(28):
    theta=random.uniform(0,2*math.pi); radius=random.uniform(.06,.20); phi=random.uniform(.08,.8)
    center=Vector((math.sin(phi)*math.cos(theta),math.sin(phi)*math.sin(theta),math.cos(phi)))
    tangent=Vector((-math.sin(theta),math.cos(theta),0)); bitangent=center.cross(tangent).normalized()
    points=[planet_center+center*(planet_radius+.025)]
    for j in range(9):
        a=j*2*math.pi/9; r=radius*random.uniform(.65,1.3)
        n=(center+math.cos(a)*r*tangent+math.sin(a)*r*bitangent).normalized()
        points.append(planet_center+n*(planet_radius+.035))
    faces=[(0,j+1,(j+1)%9+1) for j in range(9)]
    mesh=bpy.data.meshes.new('Continent geometry'); mesh.from_pydata(points,[],faces); mesh.update()
    obj=bpy.data.objects.new('Continent',mesh); scene.collection.objects.link(obj); obj.data.materials.append(clouds if i%5==0 else land)

station=bpy.data.objects.new('Orbiting station',None); scene.collection.objects.link(station)
# A toroidal rotating habitat and spokes create the reference's recognizable silhouette.
for radius,minor in [(2.0,.16),(1.62,.08)]:
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=10,location=(0,0,0),major_radius=radius,minor_radius=minor)
    obj=bpy.context.object; obj.name='Habitat ring'; obj.parent=station; obj.data.materials.append(metal)
for angle in range(0,360,45):
    a=math.radians(angle)
    obj=block('Habitat spoke',(math.cos(a)*.94,math.sin(a)*.94,0),(1.8,.11,.13),metal,station)
    obj.rotation_euler.z=a
    block('Ring window',(math.cos(a)*2.01,math.sin(a)*2.01,.12),(.19,.19,.08),bright,station)
cylinder('Central hub',(0,0,0),.5,.85,metal,station)
cylinder('Docking collar',(0,0,.62),.30,.40,dark,station)
# Pressurized modules above and below the ring make the silhouette read as a
# working station from a distance, rather than a flat wheel with solar panels.
for side in (-1,1):
    cylinder('Module connector',(0,0,side*.95),.18,1.15,dark,station,vertices=16)
    cylinder('Vertical habitat',(0,0,side*1.65),.32,.75,metal,station,vertices=20)
    for z in (side*1.36,side*1.92):
        cylinder('Module rim',(0,0,z),.35,.07,dark,station,vertices=20)
    for angle in range(0,360,90):
        a=math.radians(angle)
        window=block('Habitat window',(.32*math.cos(a),.32*math.sin(a),side*1.65),(.06,.20,.22),bright,station)
        window.rotation_euler.z=a
cylinder('Antenna mast',(0,0,2.36),.045,.70,metal,station,vertices=10)
for side in (-1,1):
    block('Array boom',(side*3.1,0,0),(2.3,.13,.14),metal,station)
    for panel in range(2):
        x=side*(3.0+panel*1.35)
        block('Solar panel',(x,0,0),(1.24,1.5,.07),solar,station)
        for stripe in range(5): block('Cell division',(x+(stripe-2)*.20,0,.042),(.025,1.45,.015),metal,station)
        for stripe in range(3): block('Cell division',(x,(stripe-1)*.42,.045),(1.2,.022,.015),metal,station)
    cylinder('Docking module',(side*2.4,0,.1),.28,.8,dark,station,rotation=(0,math.pi/2,0))
    block('Navigation beacon',(side*4.4,0,.12),(.14,.14,.14),beacon,station)
station.location=(4.5,0,5); station.keyframe_insert(data_path='rotation_euler',frame=1)
station.rotation_euler.z=2*math.pi; station.keyframe_insert(data_path='rotation_euler',frame=360)
for i in range(150):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=random.uniform(.025,.045),
        location=(random.uniform(-35,35),random.uniform(6,10),random.uniform(-8,28)))
    star=bpy.context.object; star.name='Distant star'; star.data.materials.append(bright)

meteor=add_meteor(scene, bright)

bpy.ops.object.camera_add(location=(0,-24,12)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,2.2))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=23; scene.camera=camera
bpy.ops.object.light_add(type='AREA',location=(-6,-8,16)); light=bpy.context.object; light.data.energy=1800; light.data.size=10
scene.world.color=(.04,.04,.08); scene.frame_set(40)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('/tmp/lucid-orbit.blend'); output.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1]!='-': scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
if len(args)>2:
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True)
    scene.frame_set(1); station.location=(0,0,0); station.rotation_euler=(0,0,0); bpy.context.view_layer.update(); groups={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group='station' if obj.parent==station else 'meteor' if obj.parent==meteor else 'planet'
        groups.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name),objects in sorted(groups.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'; bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
