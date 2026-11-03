"""Build the retro computer landscape from 3D terrain, grid, and a striped sun."""
import math
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import bpy
from scene_common import setup, material, block, line, camera_light, export
scene=setup()
terrain=material('Wireframe mountains',(.13,.15,.21)); grid=material('Perspective grid',(.25,.29,.42)); sun=material('Striped sun',(.57,.61,.88),.2)
# Layered faceted ridges are modeled meshes; ridge lines carry the retro glow.
def height(x,y):
    # Uneven, overlapping peaks form a continuous range rather than repeating
    # identical pyramids. The shallow folds remain real terrain geometry.
    peaks = [(-28,2.8,3.8), (-19,3.1,3.0), (-11,2.5,3.4),
             (-4,3.2,4.0), (3,4.3,3.0), (10,2.8,3.8),
             (18,3.2,3.3), (27,2.7,4.1)]
    ridge = 0.20 + sum(h * math.exp(-((x-center)/width)**2)
                       for center,h,width in peaks)
    roughness = (0.16 * math.sin(x * 1.07 + y * .39)
                 + 0.08 * math.sin(x * 2.17 - y * .27)
                 + 0.035 * math.sin(x * 4.1 + y * .68))
    depth = max(0.0, 1.0 - abs(y - 4.8) / 7.0)
    return max(0.0, (ridge * depth + roughness * depth)
               * min(1.0, y / 2.2))
vertices=[]; faces=[]; nx,ny=161,24
for j in range(ny):
    y=j*.42
    for i in range(nx):
        x=-32+i*64/(nx-1); z=height(x,y)
        vertices.append((x,y,max(0,z)))
for j in range(ny-1):
    for i in range(nx-1):
        k=j*nx+i; faces.extend([(k,k+1,k+nx+1),(k,k+nx+1,k+nx)])
mesh=bpy.data.meshes.new('Mountain terrain'); mesh.from_pydata(vertices,[],faces); mesh.update()
mount=bpy.data.objects.new('Mountain terrain',mesh); scene.collection.objects.link(mount); mount.data.materials.append(terrain)
# Contour ridges remain real thin 3D meshes, rather than painted lines.
for layer in (3.0, 4.4, 5.8):
    points=[]
    for i in range(161):
        x=-32+i*.4
        y=layer+.26*math.sin(x*.31+layer)+.10*math.sin(x*.83)
        points.append((x,y,height(x,y)+.07))
    for a,b in zip(points,points[1:]): line('Ridge contour',a,b,.025,sun)
retrogrid=bpy.data.objects.new('Moving grid',None); scene.collection.objects.link(retrogrid)
for x in range(-36,37): line('Grid longitudinal',(x,-18,.05),(x,0,.05),.035,grid,retrogrid)
for y in range(-18,1): line('Grid transverse',(-36,y,.05),(36,y,.05),.035,grid,retrogrid)
# Model a circle sliced into horizontal luminous bands, as in the reference.
sun_parent=bpy.data.objects.new('Retro sun',None); scene.collection.objects.link(sun_parent)
for band in range(-7,8):
    z=5.8+band*.29; r2=2.2**2-(band*.29)**2
    if r2<=0: continue
    width=2*math.sqrt(r2)
    block('Sun stripe',(15.0,8,z),(width,.10,.18),sun,sun_parent)
camera_light(scene,(0,-24,10),(0,0,2.8),25)
export(scene,'/tmp/lucid-retro.blend',{'Moving grid':'retro-grid','Retro sun':'retro-sun'})
