"""Small modeled silhouettes for the mountain and coastal skies."""
import bpy

def add_birds(scene, material, height=8.0):
    parent=bpy.data.objects.new('Flying birds',None)
    scene.collection.objects.link(parent)
    vertices=[]; faces=[]
    for x,z,scale in [(-5,0,.38),(-3.8,.45,.29),(-2.8,-.20,.23),(-1.9,.65,.27),(-.8,.25,.20)]:
        x += 2.0
        z += height
        i=len(vertices)
        # Two swept wings with a slender body, visible from the scene camera.
        vertices.extend([(x,2,z), (x-scale,2,z+scale*.35), (x-scale*.38,2,z-.04),
                         (x+scale,2,z+scale*.35), (x+scale*.38,2,z-.04)])
        faces.extend([(i,i+1,i+2),(i,i+3,i+4)])
    mesh=bpy.data.meshes.new('Bird silhouettes'); mesh.from_pydata(vertices,[],faces); mesh.update()
    obj=bpy.data.objects.new('Bird silhouettes',mesh); scene.collection.objects.link(obj)
    obj.parent=parent; obj.data.materials.append(material)
    return parent
