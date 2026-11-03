"""A small modeled meteor, animated independently of the space scene."""
import bpy

def add_meteor(scene, material):
    parent=bpy.data.objects.new('Passing meteor',None); scene.collection.objects.link(parent)
    # Coordinates are in the same distant sky as the stars.
    vertices=[(-22,8,17),(-24.5,8,17.60),(-22.05,8,17.28)]
    mesh=bpy.data.meshes.new('Meteor streak'); mesh.from_pydata(vertices,[],[(0,1,2)]); mesh.update()
    obj=bpy.data.objects.new('Meteor streak',mesh); scene.collection.objects.link(obj)
    obj.parent=parent; obj.data.materials.append(material)
    return parent
