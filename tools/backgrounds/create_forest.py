"""Create the animated forest-fireflies background from 3D geometry."""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
random.seed(947)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=24
scene.render.resolution_x=960; scene.render.resolution_y=600; scene.render.film_transparent=True

def mat(name,rgb,emission=0):
    m=bpy.data.materials.new(name); m['lucid_emission']=emission; m.diffuse_color=(*rgb,1); m.use_nodes=True
    shader=m.node_tree.nodes.get('Principled BSDF'); shader.inputs['Base Color'].default_value=(*rgb,1); shader.inputs['Roughness'].default_value=.9
    if emission: shader.inputs['Emission Color'].default_value=(*rgb,1); shader.inputs['Emission Strength'].default_value=emission
    return m
bark=mat('Tree bark',(.13,.15,.21)); needles=mat('Pine foliage',(.12,.16,.21)); canopy=mat('Canopy foliage',(.19,.22,.29)); stone=mat('Forest stones',(.25,.27,.33)); ground=mat('Forest floor',(.065,.073,.10)); fern=mat('Ferns',(.22,.26,.31)); glow=mat('Firefly light',(.70,.76,.98),1.5)
leaf=mat('Individual leaves',(.27,.31,.40))
flame=mat('Campfire flames',(.78,.65,.54),1.2)
flame_core=mat('Campfire core',(.98,.95,.82),1.7)
embers=mat('Campfire embers',(.9,.8,.7),1.2)
tent_fabric=mat('Tent fabric',(.34,.37,.46))
tent_shadow=mat('Tent entrance',(.09,.11,.16))
camp_wood=mat('Camp wood',(.24,.26,.32))
lantern_light=mat('Lantern glass',(.70,.74,.86),.8)

def cylinder_between(name,a,b,radius,material,vertices=8):
    a,b=Vector(a),Vector(b); direction=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=direction.length,location=(a+b)*.5)
    obj=bpy.context.object; obj.name=name; obj.rotation_euler=direction.to_track_quat('Z','Y').to_euler(); obj.data.materials.append(material); return obj

def cube(name,loc,size,material,parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.name=name; o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); o.data.materials.append(material)
    if parent: o.parent=parent
    return o

# Build many little tapered leaves as one mesh, retaining real 3D geometry
# without issuing thousands of separate draw calls in the running editor.
leaf_vertices=[]; leaf_faces=[]
def pointed_leaf(start,end,width):
    a,b=Vector(start),Vector(end); direction=b-a
    lateral=direction.cross(Vector((0,0,1)))
    if lateral.length < .001: lateral=direction.cross(Vector((0,1,0)))
    lateral.normalize(); mid=a+direction*.52
    ridge=Vector((0,0,width*.18)); index=len(leaf_vertices)
    leaf_vertices.extend((a,mid+lateral*width, b, mid-lateral*width,mid+ridge))
    leaf_faces.extend(((index,index+1,index+4),(index+1,index+2,index+4),
                       (index+2,index+3,index+4),(index+3,index,index+4)))

# A dark, gently uneven floor connects the trees to the foreground. Its back
# edge sits behind the tree line, so it does not cut across the open sky.
floor_vertices=[]; floor_faces=[]; floor_columns=65; floor_rows=20
for row in range(floor_rows):
    y=-13+row*23/(floor_rows-1)
    for column in range(floor_columns):
        x=-38+column*76/(floor_columns-1)
        z=-.12+.035*math.sin(x*.7+y*.4)+.025*math.sin(x*1.4-y*.7)
        floor_vertices.append((x,y,z))
for row in range(floor_rows-1):
    for column in range(floor_columns-1):
        a=row*floor_columns+column
        floor_faces.append((a,a+1,a+1+floor_columns,a+floor_columns))
floor_mesh=bpy.data.meshes.new('Uneven forest floor'); floor_mesh.from_pydata(floor_vertices,[],floor_faces); floor_mesh.update()
floor_obj=bpy.data.objects.new('Uneven forest floor',floor_mesh); scene.collection.objects.link(floor_obj); floor_obj.data.materials.append(ground)

# Rows of conifers produce a dense receding silhouette, with tapered trunks and boughs.
for row in range(3):
    y=2.2+row*2.1
    for column in range(37):
        x=-24+column*1.32+random.uniform(-.4,.4); height=random.uniform(2.3,5.0)*(1+.08*row)
        cylinder_between('Pine trunk',(x,y,0),(x,y,height*.85),.11,bark)
        # A continuous asymmetric silhouette with irregular, descending boughs
        # avoids the repetitive stacks of perfect cones in the first draft.
        verts=[]; faces=[]; rings=13; sides=10
        phase=random.uniform(0,math.tau)
        for layer in range(rings):
            fraction=layer/(rings-1); z=height*(.15+.85*fraction)
            outer=height*.23*(1-fraction)**.8
            outer=max(.015,outer*(1.15 if layer%2==0 else .66))
            for side in range(sides):
                angle=phase+side*math.tau/sides
                radius=outer*random.uniform(.75,1.18)
                verts.append((x+radius*math.cos(angle),y+radius*math.sin(angle),z+random.uniform(-.025,.025)))
        for layer in range(rings-1):
            for side in range(sides):
                a=layer*sides+side; b=layer*sides+(side+1)%sides
                faces.append((a,b,b+sides,a+sides))
        mesh=bpy.data.meshes.new('Pine bough geometry'); mesh.from_pydata(verts,[],faces); mesh.update()
        obj=bpy.data.objects.new('Pine boughs',mesh); scene.collection.objects.link(obj); obj.data.materials.append(needles if row!=1 else canopy)
# Foreground rocks and fern fronds make the lower edge feel inhabited.
for i in range(75):
    x=random.uniform(-19,19); y=random.uniform(-2.4,.5); r=random.uniform(.18,.55)
    if -5.5 < x < 3.5 and -2.0 < y < 1.5: continue
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=r,location=(x,y,.04))
    obj=bpy.context.object; obj.name='Forest rock'; obj.scale=(1.4,1,.55); obj.data.materials.append(stone)
for i in range(50):
    x=random.uniform(-19,19); y=random.uniform(-2.2,.5)
    if -5.5 < x < 3.5 and -2.0 < y < 1.5: continue
    for branch in range(5):
        angle=branch*math.tau/5+random.uniform(-.2,.2); length=random.uniform(.28,.55)
        base=Vector((x,y,.05)); tip=Vector((x+math.cos(angle)*length,y+math.sin(angle)*length,.35))
        cylinder_between('Fern frond',base,tip,.015,fern,vertices=5)
        for leaflet in range(1,5):
            stem=base.lerp(tip,leaflet/5)
            for side in (-1,1):
                lateral=Vector((-math.sin(angle)*side,math.cos(angle)*side,.18))
                pointed_leaf(stem,stem+lateral*length*(.38-leaflet*.04),.035)
mesh=bpy.data.meshes.new('Leaves and fern leaflets'); mesh.from_pydata(leaf_vertices,[],leaf_faces); mesh.update()
obj=bpy.data.objects.new('Leaves and fern leaflets',mesh); scene.collection.objects.link(obj); obj.data.materials.append(leaf)
# A tiny campfire belongs to the clearing; the flame mesh is animated at runtime.
fire=bpy.data.objects.new('Campfire',None); scene.collection.objects.link(fire)
for angle in range(0,360,45):
    a=math.radians(angle)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.14,location=(-2+.6*math.cos(a),.2+.45*math.sin(a),.08))
    bpy.context.object.data.materials.append(stone)
for a in (-.7,.7):
    cylinder_between('Campfire log',(-2-.5*math.cos(a),.2-.5*math.sin(a),.12),(-2+.5*math.cos(a),.2+.5*math.sin(a),.12),.10,bark)
# Irregular faceted tongues give the fire a layered silhouette; both layers
# sway and pulse in the shader, with a brighter inner core.
def flame_tongue(dx,dy,r,h,material,lean):
    verts=[]; faces=[]; sides=7
    for level,(height,width) in enumerate(((0.0,.65),(.28,1.0),(.62,.55),(1.0,.025))):
        for i in range(sides):
            a=i*math.tau/sides
            verts.append((-2+dx+lean*height+math.cos(a)*r*width,
                          .2+dy+math.sin(a)*r*width,
                          .18+h*height))
    for level in range(3):
        for i in range(sides):
            a=level*sides+i; b=level*sides+(i+1)%sides
            faces.append((a,b,b+sides,a+sides))
    mesh=bpy.data.meshes.new('Flame tongue'); mesh.from_pydata(verts,[],faces); mesh.update()
    obj=bpy.data.objects.new('Flame tongue',mesh); scene.collection.objects.link(obj)
    obj.parent=fire; obj.data.materials.append(material)
for dx,dy,r,h,lean in [(-.22,0,.20,.95,-.13),(.18,.07,.21,.80,.15),(0,-.10,.23,1.18,.10)]:
    flame_tongue(dx,dy,r,h,flame,lean)
flame_tongue(0,-.16,.18,.60,flame_core,-.06)
# A handful of modeled embers drift upward from the flames.
sparks=bpy.data.objects.new('Campfire sparks',None); scene.collection.objects.link(sparks)
for i in range(12):
    # Keep each entire spark inside one shader noise cell, so all its
    # vertices share a phase and it remains intact while rising.
    x=-2+random.uniform(-.34,.34); y=.2+random.uniform(-.18,.18)
    x=(math.floor(x*5)+.5)/5; y=(math.floor(y*5)+.5)/5
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=random.uniform(.025,.045),
        location=(x,y,.35+random.uniform(0,.25)))
    obj=bpy.context.object; obj.name='Campfire spark'; obj.parent=sparks; obj.data.materials.append(embers)
# A pitched canvas tent, facing the fire and the viewer. The entrance is a
# separate dark triangle so it reads clearly even at pixel-art resolution.
tent_x, tent_y = 1.0, 0.7
v=[(tent_x-1.1,tent_y-.8,.04),(tent_x+1.1,tent_y-.8,.04),(tent_x,tent_y-.8,1.65),
   (tent_x-1.1,tent_y+.8,.04),(tent_x+1.1,tent_y+.8,.04),(tent_x,tent_y+.8,1.65)]
mesh=bpy.data.meshes.new('Canvas tent'); mesh.from_pydata(v,[],[(0,2,5,3),(2,1,4,5),(3,5,4),(0,1,2)]); mesh.update()
obj=bpy.data.objects.new('Canvas tent',mesh); scene.collection.objects.link(obj); obj.data.materials.append(tent_fabric)
mesh=bpy.data.meshes.new('Tent doorway'); mesh.from_pydata([(tent_x-.52,tent_y-.815,.05),(tent_x+.52,tent_y-.815,.05),(tent_x,tent_y-.815,1.10)],[],[(0,1,2)]); mesh.update()
obj=bpy.data.objects.new('Tent doorway',mesh); scene.collection.objects.link(obj); obj.data.materials.append(tent_shadow)
for side in (-1,1):
    cylinder_between('Tent seam',(tent_x,tent_y-.84,1.66),(tent_x+side*1.1,tent_y-.84,.05),.025,camp_wood,vertices=5)
    cylinder_between('Tent guyline',(tent_x+side*1.1,tent_y-.8,.10),(tent_x+side*1.45,tent_y-1.10,.03),.015,camp_wood,vertices=5)
# A low log seat and a small stack of spare firewood complete the campsite.
cylinder_between('Log bench',(-4.8,.3,.40),(-3.5,.3,.40),.20,camp_wood)
for x in (-4.55,-3.75): cylinder_between('Bench support',(x,.3,.05),(x,.3,.30),.10,bark)
for i in range(3):
    cylinder_between('Spare firewood',(-3.8+i*.22,1.1,.12),(-3.8+i*.22,1.7,.12),.10,camp_wood)
# A small lantern sits beside the tent entrance.
cube('Lantern base',(2.55,-.25,.09),(.28,.28,.12),camp_wood)
cube('Lantern glass',(2.55,-.25,.26),(.18,.18,.25),lantern_light)
cube('Lantern cap',(2.55,-.25,.40),(.27,.27,.07),camp_wood)
flies=bpy.data.objects.new('Fireflies',None); scene.collection.objects.link(flies)
for i in range(22):
    x=random.uniform(-14,14); y=random.uniform(-1.5,4); z=random.uniform(.6,3.8)
    # Keep each firefly inside one shader cell so it blinks uniformly.
    x=(math.floor(x*2)+.5)/2; y=(math.floor(y*2)+.5)/2
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=random.uniform(.09,.15),location=(x,y,z))
    obj=bpy.context.object; obj.name='Firefly'; obj.parent=flies; obj.data.materials.append(glow)

bpy.ops.object.camera_add(location=(0,-24,10)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,1,2.6))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=23; scene.camera=camera
bpy.ops.object.light_add(type='AREA',location=(-5,-8,14)); light=bpy.context.object; light.data.energy=1200; light.data.size=12
scene.world.color=(.04,.045,.07)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('/tmp/lucid-forest.blend'); output.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1]!='-': scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
if len(args)>2:
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True); groups={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group='fireflies' if obj.parent==flies else 'flame' if obj.parent==fire else 'sparks' if obj.parent==sparks else 'forest'
        groups.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name),objects in sorted(groups.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'; bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
