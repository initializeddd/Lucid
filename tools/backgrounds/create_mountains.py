"""Create Lucid's animated mountain-parallax 3D scene using Blender geometry."""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from scene_birds import add_birds

random.seed(1029)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.render.engine='CYCLES'; scene.cycles.samples=24
scene.render.resolution_x=960; scene.render.resolution_y=600
scene.render.film_transparent=True; scene.frame_start=1; scene.frame_end=360

def material(name,rgb):
    m=bpy.data.materials.new(name); m.diffuse_color=(*rgb,1); m.use_nodes=True
    node=m.node_tree.nodes.get('Principled BSDF'); node.inputs['Base Color'].default_value=(*rgb,1); node.inputs['Roughness'].default_value=.9
    return m
rock=material('Mountain slate',(.22,.25,.34))
snow=material('Snow caps',(.50,.54,.67))
tree=material('Pine needles',(.10,.13,.20))
wood=material('Pine bark',(.14,.15,.20))
cloud=material('Cloud silver',(.55,.58,.68))
foothill=material('Distant foothills',(.15,.19,.27))

birds=add_birds(scene, cloud, height=10.0)

# A real triangulated ridgeline, with several peaks, facets and snow.
def height(x,y):
    value=0.0
    for center,radius,peak in [(-29,5,4.4),(-22,5,5.7),(-15,4.5,4.3),(-8,4,4.5),(-3.0,4.3,6.5),(2.8,5,8.0),(8,3.6,5.0),(15,5,5.8),(22,4.5,4.8),(29,5,5.6)]:
        dx=(x-center)/radius; dy=(y-3.5)/(radius*.8)
        # Offset each ridge along its depth and carve smaller gullies into
        # its flanks. The maximum of overlapping slopes makes a continuous
        # mountain range with distinct saddles rather than isolated cones.
        ridge_shift=.13*math.sin(y*.7+center)+.06*math.sin(y*1.9-center)
        d=max(abs(dx+ridge_shift),abs(dy)*.82)+.24*abs(dx*dy)
        d+=.055*math.sin(x*1.35+y*.9+center)+.035*math.sin(x*3.1-y*2.4)
        value=max(value,peak*max(0,1-d))
    detail=(.15*math.sin(x*2.5+y*1.1)+.11*math.sin(x*4.3-y*.7))
    return max(0,value + detail*min(1.0,value))
nx,ny=201,45; vertices=[]; faces=[]
for j in range(ny):
    y=-1.0+j*11/(ny-1)
    for i in range(nx):
        x=-32+i*64/(nx-1)
        vertices.append((x,y,max(0,height(x,y)+random.uniform(-.05,.05))))
for j in range(ny-1):
    for i in range(nx-1):
        k=j*nx+i
        faces.extend([(k,k+1,k+nx+1),(k,k+nx+1,k+nx)])
mesh=bpy.data.meshes.new('Faceted mountain ridges'); mesh.from_pydata(vertices,[],faces); mesh.update()
mount=bpy.data.objects.new('Mountain range',mesh); scene.collection.objects.link(mount); mount.data.materials.append(rock); mount.data.materials.append(snow)
for face in mesh.polygons:
    average=sum(mesh.vertices[v].co.z for v in face.vertices)/len(face.vertices)
    center_x=sum(mesh.vertices[v].co.x for v in face.vertices)/len(face.vertices)
    center_y=sum(mesh.vertices[v].co.y for v in face.vertices)/len(face.vertices)
    snow_line=4.35+.24*math.sin(center_x*1.15+center_y*.55)
    face.material_index=1 if average>snow_line else 0

# Separate material geometry is exported per mesh; split snow from the rock.
bpy.context.view_layer.objects.active=mount; mount.select_set(True)
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='DESELECT'); bpy.ops.object.mode_set(mode='OBJECT')
for p in mount.data.polygons: p.select=(p.material_index==1)
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.separate(type='SELECTED'); bpy.ops.object.mode_set(mode='OBJECT')
for obj in list(bpy.context.selected_objects):
    if obj.type=='MESH' and obj!=mount:
        obj.name='Snow cap geometry'; obj.data.materials.clear(); obj.data.materials.append(snow)

# A second, lower range in front of the main peaks creates the layered depth
# visible in the reference, using faceted geometry rather than a flat strip.
ridge_vertices=[]; ridge_faces=[]; ridge_steps=129
for i in range(ridge_steps):
    x=-32+i*64/(ridge_steps-1)
    h=1.15+.52*math.sin(x*.42+1.1)+.27*math.sin(x*.97)+.12*math.sin(x*2.1)
    ridge_vertices.extend(((x,-1.8,0),(x,.4,max(.35,h)),(x,2.0,.15)))
for i in range(ridge_steps-1):
    a=i*3; b=(i+1)*3
    ridge_faces.extend(((a,b,b+1),(a,b+1,a+1),(a+1,b+1,b+2),(a+1,b+2,a+2)))
ridge_mesh=bpy.data.meshes.new('Faceted foreground foothills'); ridge_mesh.from_pydata(ridge_vertices,[],ridge_faces); ridge_mesh.update()
ridge_obj=bpy.data.objects.new('Faceted foreground foothills',ridge_mesh); scene.collection.objects.link(ridge_obj); ridge_obj.data.materials.append(foothill)

# Individually varied boughs make the forest edge read as layered pines,
# rather than a row of oversized cones in front of the mountains.
for i in range(90):
    x=-32+i*.72+random.uniform(-.17,.17); y=random.uniform(-4.5,-1.4); h=random.uniform(1.8,3.4)
    bpy.ops.mesh.primitive_cylinder_add(vertices=7,radius=.08,depth=h*.82,location=(x,y,h*.41))
    trunk=bpy.context.object; trunk.name='Pine trunk'; trunk.data.materials.append(wood)
    verts=[]; faces=[]; rings=13; sides=9; phase=random.uniform(0,math.tau)
    for layer in range(rings):
        fraction=layer/(rings-1)
        z=h*(.15+.85*fraction)
        width=max(.01,h*.23*(1-fraction)**.8*(1.12 if layer%2==0 else .67))
        for side in range(sides):
            angle=phase+side*math.tau/sides
            radius=width*random.uniform(.78,1.16)
            verts.append((x+radius*math.cos(angle),y+radius*math.sin(angle),z+random.uniform(-.02,.02)))
    for layer in range(rings-1):
        for side in range(sides):
            a=layer*sides+side; b=layer*sides+(side+1)%sides
            faces.append((a,b,b+sides,a+sides))
    mesh=bpy.data.meshes.new('Pine bough geometry'); mesh.from_pydata(verts,[],faces); mesh.update()
    crown=bpy.data.objects.new('Pine boughs',mesh); scene.collection.objects.link(crown); crown.data.materials.append(tree)

clouds=bpy.data.objects.new('Moving clouds',None); scene.collection.objects.link(clouds)
for x,y,z,w in [(-24,3,8.6,2.3),(-17,5,9.0,2.0),(-8,1,7.8,2.0),(-2,5,9.2,2.8),(6,3,8.2,2.4),(11,6,9.5,1.8),(19,4,8.3,2.5),(26,5,9.3,2.0)]:
    for segment in range(5):
        rise=(1-abs(segment-2)/2)*.25+random.uniform(-.05,.12)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=(x+(segment-2)*w*.23,y+random.uniform(-.08,.08),z+rise))
        puff=bpy.context.object; puff.name='Cloud puff'; puff.scale=(w*.23,.30,.30+random.uniform(0,.15)); puff.parent=clouds; puff.data.materials.append(cloud)
clouds.location=(-3,0,0); clouds.keyframe_insert(data_path='location',frame=1)
clouds.location=(3,0,0); clouds.keyframe_insert(data_path='location',frame=360)

bpy.ops.object.camera_add(location=(0,-24,13)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,2,3.0))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=26; scene.camera=camera
bpy.ops.object.light_add(type='AREA',location=(-8,-9,17)); light=bpy.context.object; light.data.energy=2000; light.data.size=11
scene.world.color=(.06,.06,.09); scene.frame_set(100)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('/tmp/lucid-mountains.blend'); output.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1]!='-': scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
if len(args)>2:
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True)
    scene.frame_set(1); clouds.location=(0,0,0); bpy.context.view_layer.update(); groups={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group='clouds' if obj.parent==clouds else 'birds' if obj.parent==birds else 'landscape'
        groups.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name),objects in sorted(groups.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'; bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
