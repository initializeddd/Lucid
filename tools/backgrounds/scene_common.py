"""Shared, image-free Blender export helpers for Lucid 3D backgrounds."""
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector


def material(name, color, emission=0):
    m=bpy.data.materials.new(name); m['lucid_emission']=emission; m.diffuse_color=(*color,1); m.use_nodes=True
    node=m.node_tree.nodes.get('Principled BSDF'); node.inputs['Base Color'].default_value=(*color,1); node.inputs['Roughness'].default_value=.8
    if emission: node.inputs['Emission Color'].default_value=(*color,1); node.inputs['Emission Strength'].default_value=emission
    return m


def block(name, position, dimensions, mat, parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=position); obj=bpy.context.object; obj.name=name; obj.dimensions=dimensions
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); obj.data.materials.append(mat)
    if parent: obj.parent=parent
    return obj


def line(name, start, end, radius, mat, parent=None):
    a,b=Vector(start),Vector(end); d=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=radius,depth=d.length,location=(a+b)*.5)
    obj=bpy.context.object; obj.name=name; obj.rotation_euler=d.to_track_quat('Z','Y').to_euler(); obj.data.materials.append(mat)
    if parent: obj.parent=parent
    return obj


def setup():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    scene=bpy.context.scene; scene.render.engine='CYCLES'; scene.cycles.samples=24
    scene.render.resolution_x=960; scene.render.resolution_y=600; scene.render.film_transparent=True
    scene.world.color=(.04,.04,.07); return scene


def camera_light(scene, position, target, scale):
    bpy.ops.object.camera_add(location=position); camera=bpy.context.object
    camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=scale; scene.camera=camera
    bpy.ops.object.light_add(type='AREA',location=(-6,-7,15)); light=bpy.context.object; light.data.energy=1600; light.data.size=10


def export(scene, fallback, groups):
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    output=Path(args[0]) if args else Path(fallback); output.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(output.resolve()))
    if len(args)>1 and args[1]!='-': scene.render.filepath=str(Path(args[1]).resolve()); bpy.ops.render.render(write_still=True)
    if len(args)<3: return
    out=Path(args[2]).resolve(); out.mkdir(parents=True,exist_ok=True); collections={}
    for obj in scene.objects:
        if obj.type!='MESH' or not obj.data.materials: continue
        group=groups.get(obj.parent.name,'static') if obj.parent else 'static'
        collections.setdefault((group,obj.data.materials[0].name),[]).append(obj)
    parts=[]
    for (group,name),objects in sorted(collections.items()):
        filename=group+'-'+name.lower().replace(' ','-')+'.obj'; bpy.ops.object.select_all(action='DESELECT')
        for obj in objects: obj.select_set(True)
        bpy.context.view_layer.objects.active=objects[0]
        bpy.ops.wm.obj_export(filepath=str(out/filename),export_selected_objects=True,export_materials=False,export_uv=False,forward_axis='NEGATIVE_Z',up_axis='Y')
        parts.append({'mesh':filename,'group':group,'color':list(objects[0].data.materials[0].diffuse_color),'emission':float(objects[0].data.materials[0].get('lucid_emission',0.0))})
    (out/'scene.json').write_text(json.dumps({'parts':parts},indent=2)+'\n')
