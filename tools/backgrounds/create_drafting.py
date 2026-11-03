"""Model an animated drafting grid using 3D lines, circles and markers."""
import math
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import bpy
from scene_common import setup, material, block, line, camera_light, export
scene=setup()
grid=material('Grid lines',(.20,.23,.31)); ink=material('Draft lines',(.42,.46,.57)); bright=material('Draft markers',(.68,.72,.91),.4)
# Fine grid and major divisions sit on a horizontal construction plane.
# Extend the construction plane well past the visible area on wide monitors.
for x in range(-40,41): line('Vertical grid',(x,-14,0),(x,14,0),.022 if x%4 else .032,grid)
for y in range(-14,15): line('Horizontal grid',(-40,y,0),(40,y,0),.022 if y%4 else .032,grid)
# Construction geometry is intentionally offset toward the right edge.
cx,cy=3.0,0.0
for radius in (2.0,3.5,5.0):
    bpy.ops.mesh.primitive_torus_add(major_segments=80,minor_segments=6,location=(cx,cy,.05),major_radius=radius,minor_radius=.025)
    bpy.context.object.name='Construction circle'; bpy.context.object.data.materials.append(ink)
for a,b in [((-6,-4,.08),(9,5,.08)),((-6,3,.08),(11,3,.08)),((7,-6,.08),(7,7,.08)),((-3,-5,.08),(-3,5,.08))]:
    line('Construction line',a,b,.025,ink)
for x,y in [(-3,-3),(3,0),(7,3),(-6,3)]:
    line('Crosshair',(x-.24,y,.10),(x+.24,y,.10),.03,bright)
    line('Crosshair',(x,y-.24,.10),(x,y+.24,.10),.03,bright)
tracer=bpy.data.objects.new('Moving tracer',None); scene.collection.objects.link(tracer)
block('Tracer light',(-5,-3.4,.15),(.30,.30,.12),bright,tracer)
# A fine compass arm and its illuminated tip sweep around the construction
# circle, while the existing tracer travels independently along the diagonal.
compass=bpy.data.objects.new('Rotating compass',None); scene.collection.objects.link(compass)
line('Compass arm',(0,0,.13),(5,0,.13),.022,ink,compass)
block('Compass tip',(5,0,.15),(.18,.18,.12),bright,compass)
camera_light(scene,(0,-8,25),(0,0,0),27)
export(scene,'/tmp/lucid-drafting.blend',{'Moving tracer':'tracer','Rotating compass':'compass'})
