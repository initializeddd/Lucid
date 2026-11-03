"""Model Lucid's rainy street: storefront, pavement, street light and rain."""
import bpy
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector
random.seed(618)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=24
scene.render.resolution_x=960; scene.render.resolution_y=600; scene.render.film_transparent=True

def mat(name,rgb,glow=0):
    m=bpy.data.materials.new(name); m['lucid_emission']=glow; m.diffuse_color=(*rgb,1); m.use_nodes=True
    s=m.node_tree.nodes.get('Principled BSDF'); s.inputs['Base Color'].default_value=(*rgb,1); s.inputs['Roughness'].default_value=.7
    if glow: s.inputs['Emission Color'].default_value=(*rgb,1); s.inputs['Emission Strength'].default_value=glow
    return m
wall=mat('Street masonry',(.15,.17,.23)); trim=mat('Storefront trim',(.29,.32,.41)); glass=mat('Window glass',(.20,.25,.35)); light=mat('Lamp light',(.69,.72,.9),.6)
interior=mat('Store interior',(.37,.43,.57),.35)
paving=mat('Wet pavement',(.16,.18,.24)); shine=mat('Rain reflections',(.31,.35,.46)); leaf=mat('Street foliage',(.14,.18,.23)); rain=mat('Rain streaks',(.47,.52,.65))
joint=mat('Pavement joints',(.29,.32,.41))
ripple_mat=mat('Puddle ripples',(.48,.53,.67),.15)
beam=mat('Street light beam',(.40,.44,.60),.25)
def block(name,loc,size,material,parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); obj=bpy.context.object; obj.name=name; obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); obj.data.materials.append(material)
    if parent: obj.parent=parent
    return obj

def cylinder(name,loc,radius,depth,material):
    bpy.ops.mesh.primitive_cylinder_add(vertices=10,radius=radius,depth=depth,location=loc)
    obj=bpy.context.object; obj.name=name; obj.data.materials.append(material); return obj

def tube_between(name,a,b,radius,material):
    a,b=Vector(a),Vector(b); delta=b-a
    obj=cylinder(name,(a+b)*.5,radius,delta.length,material)
    obj.rotation_euler=delta.to_track_quat('Z','Y').to_euler(); return obj

# Dim storefront with recessed frames, an awning and a hanging Lucid sign.
block('Store building',(-7,2,3.0),(7,3,6),wall)
block('Store cornice',(-7,.42,5.5),(7.2,.35,.3),trim)
block('Store window recess',(-7,.46,2.15),(4.3,.09,3.0),trim)
block('Store window glass',(-7,.39,2.15),(4.05,.05,2.75),glass)
for x in (-8.9,-7,-5.1): block('Window mullion',(x,.32,2.15),(.11,.09,2.75),trim)
for z in (1,2.1,3.45): block('Window crossbar',(-7,.31,z),(4.05,.09,.10),trim)
for x in (-8.0,-6.0):
    for z in (1.55,2.72):
        block('Lit window pane',(x,.35,z),(1.65,.025,.92),interior)
# Small display shelves and objects add depth to the shopfront.
for z in (1.35,2.45):
    block('Display shelf',(-7,.27,z),(3.7,.12,.07),trim)
    for x in (-8.3,-7.6,-6.8,-6.1,-5.5):
        block('Display object',(x,.22,z+.16),(.16,.08,.25),glass)
# Overlapping sloped fabric strips give the shop awning its recognizable
# silhouette and alternating bands without relying on an image texture.
for stripe in range(10):
    awning=block('Awning fabric',(-9.295+stripe*.51,.02,3.9),(.51,1.05,.12),trim if stripe%2==0 else glass)
    awning.rotation_euler.x=math.radians(-12)
block('Awning front edge',(-7,-.49,3.78),(5.15,.10,.20),trim)
block('Door frame',(-4.0,.40,1.5),(1.25,.11,2.9),trim)
block('Door glass',(-4.0,.32,1.5),(1.05,.06,2.6),glass)
block('Door handle',(-3.65,.26,1.45),(.06,.07,.25),light)
block('Hanging sign',(-3.55,.05,4.4),(.92,.15,.92),trim)
block('Sign inset',(-3.55,-.04,4.4),(.72,.04,.72),glass)
block('Sign L upright',(-3.7,-.08,4.44),(.07,.04,.46),light)
block('Sign L foot',(-3.57,-.08,4.23),(.32,.04,.07),light)
# Farther buildings add the layered silhouette from the reference.
for x,w,h in [(-1.5,2.2,3.1),(1,2.6,4.0),(4,2.4,3.3),(7,2.7,4.2),(10,2.4,3.1)]:
    block('Distant building',(x,5,h/2),(w,2,h),wall)
    for wx in (-.55,.1,.7):
        for z in (1.0,1.8,2.6):
            if abs(wx)<w*.42 and z<h-.15: block('Distant window',(x+wx,3.98,z),(.27,.04,.36),light if random.random()<.35 else glass)
# Reflective paving is made from real tiles and small muted glints.
block('Pavement',(0,12,-.14),(120,36,.22),paving)
for x in range(-60,61): block('Pavement joint',(x,12,-.02),(.025,36,.012),joint)
for y in range(-5,31): block('Pavement joint',(0,y,-.015),(120,.025,.012),joint)
for i in range(90):
    x=random.uniform(-14,14); y=random.uniform(-5,5)
    block('Wet reflection',(x,y,.003),(random.uniform(.1,.7),.025,.012),shine)
for center in (-7,4.27):
    for i in range(55):
        x=center+random.uniform(-1.0,1.0); y=random.uniform(-5,-.5)
        block('Light reflected in puddles',(x,y,.008),(random.uniform(.12,.65),random.uniform(.025,.08),.012),shine)
# A curved lamp arm and illuminated lantern.
cylinder('Lamp post',(5,-1,2.15),.08,4.3,trim)
arm=[(5,-1,4.30),(4.97,-1,4.57),(4.84,-1,4.78),(4.60,-1,4.88),(4.34,-1,4.83),(4.27,-1,4.56)]
for a,b in zip(arm,arm[1:]): tube_between('Curved lamp arm',a,b,.075,trim)
block('Lamp shade',(4.27,-1,4.40),(.62,.48,.16),trim)
block('Lamp lens',(4.27,-1,4.30),(.48,.36,.08),light)
# A translucent mesh cone evokes light passing through the rain. It is drawn
# after opaque objects and fades toward the pavement in the fragment shader.
beam_vertices=[(4.27,-1,4.29)]
for i in range(16):
    angle=i*math.tau/16
    beam_vertices.append((4.27+1.25*math.cos(angle),-1+.8*math.sin(angle),.08))
beam_faces=[(0,1+i,1+(i+1)%16) for i in range(16)]
beam_mesh=bpy.data.meshes.new('Streetlight cone'); beam_mesh.from_pydata(beam_vertices,[],beam_faces); beam_mesh.update()
beam_obj=bpy.data.objects.new('Streetlight cone',beam_mesh); scene.collection.objects.link(beam_obj); beam_obj.data.materials.append(beam)
# A low railing leads toward the background without adding a permanent panel.
for x in [1.4+i*.65 for i in range(15)]:
    block('Street railing upright',(x,-.85,.38),(.045,.06,.76),trim)
for z in (.18,.73): block('Street railing rail',(5.95,-.85,z),(9.2,.07,.06),trim)
# Planters and foliage anchor the store to the wet pavement.
for x in (-9.5,-4.65):
    cylinder('Planter',(x,-.25,.22),.22,.44,trim)
    for offset in (-.18,0,.18):
        cylinder('Plant stem',(x+offset,-.25,.65),.025,.55,leaf)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.20,location=(x+offset,-.25,.94))
        plant=bpy.context.object; plant.name='Planter leaves'; plant.scale=(.85,.65,1.2); plant.data.materials.append(leaf)
# Trees overhang the shop and frame the scene.
for x,y,h in [(-11,-.8,4.7),(11,2,4.1)]:
    cylinder('Street tree trunk',(x,y,h*.5),.15,h,trim)
    for i in range(12):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=random.uniform(.35,.7),location=(x+random.uniform(-1.4,1.4),y+random.uniform(-.6,.6),h+random.uniform(-.4,.8)))
        crown=bpy.context.object; crown.name='Tree foliage'; crown.scale=(1.4,1,.6); crown.data.materials.append(leaf)
# Each line is a mesh, displaced vertically in the vertex shader at runtime.
rain_parent=bpy.data.objects.new('Falling rain',None); scene.collection.objects.link(rain_parent)
for i in range(680):
    # Integer-height anchors let all vertices of each streak wrap together.
    x=random.uniform(-25,25); y=random.uniform(-4,5); z=random.randrange(1,40)
    block('Raindrop',(x,y,z),(.025,.025,random.uniform(.25,.65)),rain,rain_parent)

# Broken rings briefly catch the street lights when raindrops hit puddles.
ripples=bpy.data.objects.new('Puddle ripples',None); scene.collection.objects.link(ripples)
verts=[]; faces=[]
for i in range(40):
    x=random.uniform(-17,17); y=random.uniform(-4,2); radius=random.uniform(.12,.30)
    start=len(verts)
    for n in range(12):
        a=n*math.tau/12
        for r in (radius, radius+.025): verts.append((x+r*math.cos(a),y+r*math.sin(a),.035))
    for n in range(12):
        a=start+n*2; b=start+((n+1)%12)*2
        faces.append((a,b,b+1,a+1))
mesh=bpy.data.meshes.new('Rain impact rings'); mesh.from_pydata(verts,[],faces); mesh.update()
obj=bpy.data.objects.new('Rain impact rings',mesh); scene.collection.objects.link(obj); obj.parent=ripples; obj.data.materials.append(ripple_mat)

bpy.ops.object.camera_add(location=(0,-24,11)); camera=bpy.context.object
camera.rotation_euler=(Vector((0,1,2.8))-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=25; scene.camera=camera
bpy.ops.object.light_add(type='AREA',location=(-5,-5,12)); lamp=bpy.context.object; lamp.data.energy=1350; lamp.data.size=9
scene.world.color=(.05,.05,.08)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
output=Path(args[0]) if args else Path('/tmp/lucid-rain.blend'); output.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
if len(args)>1 and args[1]!='-': scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
if len(args)>2:
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True); groups={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group='rain' if obj.parent==rain_parent else 'ripples' if obj.parent==ripples else ('streetlight' if obj==beam_obj else 'street')
        groups.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name),objects in sorted(groups.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'; bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
