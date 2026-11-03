"""Create the ocean lighthouse scene as authored 3D geometry for Lucid."""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scene_birds import add_birds

random.seed(711)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=24
scene.render.resolution_x=960; scene.render.resolution_y=600; scene.render.film_transparent=True
scene.frame_start=1; scene.frame_end=360

def material(name,rgb,glow=0):
    m=bpy.data.materials.new(name); m['lucid_emission']=glow; m.diffuse_color=(*rgb,1); m.use_nodes=True
    shader=m.node_tree.nodes.get('Principled BSDF'); shader.inputs['Base Color'].default_value=(*rgb,1); shader.inputs['Roughness'].default_value=.78
    if glow: shader.inputs['Emission Color'].default_value=(*rgb,1); shader.inputs['Emission Strength'].default_value=glow
    return m
water=material('Ocean water',(.12,.16,.25)); crest=material('Wave crests',(.39,.45,.59)); rock=material('Coastal rock',(.19,.21,.28))
wall=material('Lighthouse stone',(.53,.56,.64)); dark=material('Lighthouse trim',(.19,.21,.29)); lamp=material('Lantern glass',(.75,.78,.93),1.0); beam=material('Beacon beam',(.37,.40,.54))
foam=material('Sea foam',(.78,.82,.93),.35)

birds=add_birds(scene, foam, height=8.0)

def box(name,loc,size,mat,parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); obj=bpy.context.object; obj.name=name; obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); obj.data.materials.append(mat)
    if parent: obj.parent=parent
    return obj

def cylinder(name,loc,radius,depth,mat,vertices=24,radius_top=None):
    if radius_top is None: bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc)
    else: bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=radius,radius2=radius_top,depth=depth,location=loc)
    obj=bpy.context.object; obj.name=name; obj.data.materials.append(mat); return obj

# Use the same surface function for both water and foam so their geometry stays
# connected as the vertex shader animates the whole ocean together.
def wave_height(x, y):
    return (.23 * math.sin(y * .78 + .28 * math.sin(x * .43))
            + .07 * math.sin(x * 1.1 - y * 1.15))

# Wide surface, explicit waves and foamy ridges: all actual mesh geometry.
verts=[]; faces=[]; nx,ny=111,44
for j in range(ny):
    # Keep the distant edge behind the rocks: an infinitely deep plane puts
    # the horizon above the lantern and obscures the light against the sea.
    y=-30+j*34/(ny-1)
    for i in range(nx):
        x=-60+i*120/(nx-1)
        z=wave_height(x,y)
        verts.append((x,y,z))
for j in range(ny-1):
    for i in range(nx-1):
        k=j*nx+i; faces.append((k,k+1,k+nx+1,k+nx))
mesh=bpy.data.meshes.new('Wave surface'); mesh.from_pydata(verts,[],faces); mesh.update()
sea=bpy.data.objects.new('Animated ocean',mesh); scene.collection.objects.link(sea); sea.data.materials.append(water)

# Thin, broken ribbons follow the wave surface rather than floating straight
# lines. Their width and height vary along each crest, giving the foreground
# the layered, choppy silhouette of the reference. They are exported with the
# ocean so the runtime applies the same displacement to water and foam.
foam_vertices=[]; foam_faces=[]
for row in range(11):
    base_y=-11.0+row*1.42
    x=-58.0+random.uniform(-1.0,1.0)
    while x < 57:
        length=random.uniform(.8,3.8)
        segments=max(3,int(length/.18))
        phase=random.uniform(0,math.tau)
        start=len(foam_vertices)
        for step in range(segments+1):
            t=step/segments; px=x+length*t
            py=base_y+.30*math.sin(px*.75+row*1.37)+.09*math.sin(px*2.5+phase)
            width=(.07+.13*math.sin(math.pi*t))*random.uniform(.8,1.2)
            for side in (-1,1):
                yy=py+side*width
                zz=wave_height(px,yy)+.09
                foam_vertices.append((px,yy,zz))
            if step:
                k=start+(step-1)*2; foam_faces.append((k,k+1,k+3,k+2))
        x+=length+random.uniform(.15,1.3)
mesh=bpy.data.meshes.new('Broken wave crest ribbons'); mesh.from_pydata(foam_vertices,[],foam_faces); mesh.update()
foam_obj=bpy.data.objects.new('Animated sea foam',mesh); scene.collection.objects.link(foam_obj); foam_obj.data.materials.append(foam)

# Cliff uses faceted boulders and a layered concrete base.
for i in range(22):
    x=random.uniform(3.5,9.5); y=random.uniform(-2.6,2.7); radius=random.uniform(.5,1.35)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=radius,location=(x,y,.15))
    obj=bpy.context.object; obj.name='Coastal boulder'; obj.scale=(1.25,1,.6); obj.rotation_euler[2]=random.random()*math.pi; obj.data.materials.append(rock)

x,y=6.4,0.0
cylinder('Foundation',(x,y,.55),1.15,.35,dark)
cylinder('Tapered lighthouse',(x,y,2.25),.82,3.15,wall,radius_top=.57)
# Separate tapered bands follow the tower profile, rather than sitting as
# horizontal trim. The slight overlap prevents gaps at oblique viewpoints.
for bottom,top in [(1.08,1.52),(2.02,2.46),(2.96,3.40)]:
    radius_bottom=.82-(bottom-.675)*(.25/3.15)+.008
    radius_top=.82-(top-.675)*(.25/3.15)+.008
    cylinder('Tower dark band',(x,y,(bottom+top)/2),radius_bottom,top-bottom,dark,radius_top=radius_top)
for z,r in [(1.15,.78),(2.25,.70),(3.30,.61)]:
    cylinder('Masonry trim',(x,y,z),r+.04,.10,dark)
for z in (1.7,2.55,3.2): box('Tower window',(x,y-.73,z),(.22,.07,.30),lamp)
cylinder('Gallery platform',(x,y,3.93),.93,.16,dark)
for angle in range(0,360,30):
    a=math.radians(angle)
    cylinder('Gallery railing post',(x+.8*math.cos(a),y+.8*math.sin(a),4.15),.035,.45,dark,vertices=8)
cylinder('Lantern chamber',(x,y,4.42),.50,.82,lamp)
for angle in range(0,360,45):
    a=math.radians(angle)
    cylinder('Lantern mullion',(x+.48*math.cos(a),y+.48*math.sin(a),4.42),.035,.90,dark,vertices=8)
cylinder('Lantern roof',(x,y,4.95),.69,.28,dark,radius_top=.10)
cylinder('Lightning rod',(x,y,5.22),.035,.35,dark,vertices=8)
box('Keeper cottage',(8.3,1.2,.75),(1.7,1.3,1.5),wall)
box('Cottage roof',(8.3,1.2,1.55),(1.9,1.5,.22),dark)
box('Cottage window',(8.3,.53,.9),(.5,.04,.5),lamp)

# Beacon mesh is centered on its parent for the runtime rotation.
beacon=bpy.data.objects.new('Rotating beacon',None); scene.collection.objects.link(beacon)
vertices=[(0,0,0),(-9,-1.8,-.18),(-9,1.8,-.18),(-9,1.8,.18),(-9,-1.8,.18)]
faces=[(0,1,2),(0,2,3),(0,3,4),(0,4,1),(1,4,3,2)]
mesh=bpy.data.meshes.new('Beacon volume'); mesh.from_pydata(vertices,[],faces); mesh.update()
beam_obj=bpy.data.objects.new('Beacon sweep',mesh); scene.collection.objects.link(beam_obj); beam_obj.parent=beacon; beam_obj.data.materials.append(beam)
beacon.location=(x,y,4.42); beacon.rotation_euler.z=-.22; beacon.keyframe_insert(data_path='rotation_euler',frame=1)
beacon.rotation_euler.z=.22; beacon.keyframe_insert(data_path='rotation_euler',frame=180)
beacon.rotation_euler.z=-.22; beacon.keyframe_insert(data_path='rotation_euler',frame=360)

bpy.ops.object.camera_add(location=(1,-23,12)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,1.5))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=25; scene.camera=camera
bpy.ops.object.light_add(type='AREA',location=(-4,-7,14)); light=bpy.context.object; light.data.energy=1800; light.data.size=10
scene.world.color=(.05,.05,.08); scene.frame_set(100)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('/tmp/lucid-lighthouse.blend'); output.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1]!='-': scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
if len(args)>2:
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True); scene.frame_set(1)
    beacon.location=(0,0,0); beacon.rotation_euler=(0,0,0); bpy.context.view_layer.update(); groups={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group='beacon' if obj.parent==beacon else 'birds' if obj.parent==birds else ('ocean' if obj in (sea,foam_obj) else 'coast')
        groups.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name),objects in sorted(groups.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'; bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
