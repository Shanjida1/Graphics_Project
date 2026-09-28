Bedroom 3D - FINAL STANDARD PROJECT
===================================

HOW TO RUN
----------
1. Extract the ZIP.
2. Open Bedroom3D.sln in Visual Studio 2022.
3. Make sure “Desktop development with C++” is installed.
4. Select x64 / Debug (recommended).
5. Press the green Run / Local Windows Debugger button.

No terminal command, CMake command, package manager, or C:\opengl folder is required.

FINAL PROJECT FEATURES
----------------------
- Realistic standard 3D bedroom layout
- Mouse-controlled viewing transformation
- WASD camera movement + zoom
- Bed, pillows, blanket, headboard and bedside table
- Wardrobe facing the bedside
- Realistic study desk / PC area
- Improved realistic window with:
  * recessed outside scenery behind the glass
  * proper outer frame and inner sash
  * separate glass panels
  * subtle glass reflections
  * window latch / handle
  * projecting sill
  * curtain rod, finials, rings and folded curtains
- Realistic door with frame, panels, hinges, lever handle and open/close animation
- Proper wall shelf with supported books and plant pot
- Point light and spotlight
- Rotating ceiling fan
- Rug transformation animation
- Multiple materials / colors
- Course-style helper and shader source files

CONTROLS
--------
Mouse move   : Look around
Mouse wheel  : Zoom
W A S D      : Move camera
Q / E        : Camera down / up
1            : Ceiling point light ON/OFF
2            : Picture spotlight ON/OFF
F            : Fan rotation ON/OFF
O            : Open / close room door
Z            : Move rug left
X            : Move rug right
C            : Center rug
M            : Capture / release mouse
R            : Reset camera and interactive objects
Esc          : Exit

COURSE-STYLE FILES INCLUDED
---------------------------
camera.h
basic_camera.h
shader.h
pointLight.h
sphere.h
vertexShader.vs
fragmentShader.fs
vertexShaderForGouraudShading.vs
fragmentShaderForGouraudShading.fs
vertexShaderForPhongShading.vs
fragmentShaderForPhongShading.fs

ABOUT GLAD.C
------------
The teacher's Lighting.vcxproj references glad.c from an EXTERNAL path:
    C:\opengl\glad.c
The supplied Lighting.zip does not contain that glad.c source file; it only contains
compiled glad.obj output.

This final bedroom project intentionally uses the Windows OpenGL compatibility API
(gl/GL.h + GLU) and only functions available without GLAD. Therefore glad.c is NOT
required for this project and is not added as a fake or unused dependency. This is
what keeps the project portable and lets it run directly from Visual Studio without
requiring the teacher's C:\opengl setup.

If the project were converted to the exact programmable OpenGL pipeline used by the
teacher (GLFW + GLAD + GLM + shader program objects), then glad.c and its matching
glad headers would be required together. Adding only glad.c to this compatibility
project would be technically unnecessary and could make the build less portable.

SUBMISSION NOTE
---------------
Generated build folders such as .vs, x64, obj, .pdb and .ipch are intentionally not
included. Visual Studio creates them automatically; they are not source-code files.

Final interaction update
------------------------
- K = open / close the sliding window with smooth animation
- P = PC / monitor ON or OFF
- O = open / close the bedroom door
- Z / X = move rug left / right
- C = center the rug

The right window sash now moves on its track instead of the complete window moving.
When the PC is ON, the monitor shows a lit desktop-style screen and the CPU power light glows.
When OFF, the monitor and CPU light become dark.


Realism update (v9)
-------------------
- K now opens/closes a hinged casement window with a real pivot animation.
- Window has visible hinges, moving handle, deeper frame, glass reflection, sill, curtains, and outdoor scenery behind the wall.
- P now uses a realistic PC power sequence instead of an instant screen toggle.
- PC power fades in/out, shows a short boot screen, then a desktop with taskbar/icons/application window.
- CPU power LED brightness follows the power state.


FINAL interaction upgrade v10
-----------------------------
- K: BOTH window leaves open/close together as a real double-casement window.
- Left leaf is hinged on the far-left frame; right leaf is hinged on the far-right frame.
- Both leaves swing inward in opposite directions with smooth animation.
- Handles stay attached to the moving meeting edges; hinges remain on the outside edges.
- P: PC and CPU tower are one power system. CPU starts first, then monitor boots.
- CPU case fan spins while powered; CPU power LED and activity LED respond to state.
- During shutdown, the monitor fades first; the CPU stays on briefly, then its fan/LED wind down.


Player movement + proximity interaction (v11)
----------------------------------------------
This version follows the same first-person interaction idea as the supplied Classroom project:
- W / A / S / D = walk as the person/player
- Hold Left Shift = run
- Move mouse = turn the person's view/look direction
- Player stays at normal eye height and cannot walk through the main furniture
- Walk close to the door, window, or PC before interacting
- E = interact with the nearby object (recommended classroom-style control)
- O = door shortcut, but ONLY works when near the door
- K = window shortcut, but ONLY works when near the window
- P = PC/CPU shortcut, but ONLY works when near the PC
- Window title shows the available nearby action
- PC interaction controls monitor + CPU together, including the existing boot/shutdown animation

Important: E is now the interaction key, so vertical flying movement was removed. This behaves like a person walking in the room rather than a free-fly camera.


Physical wall switches (v12)
----------------------------
A realistic 3-gang wall switch board is mounted beside the bedroom door.
The person must walk close to it and look directly at the individual rocker switch.
Then press E to operate it.

Switches:
- Fan switch     = ceiling fan ON/OFF
- Main light     = ceiling point light ON/OFF
- Spotlight      = picture spotlight ON/OFF

The rocker angle and small status LED change with each device state.
The old 1 / 2 / F keys no longer work from anywhere in the room; they only work
while the player is close to and aiming at the corresponding physical switch.
This keeps the interaction behavior consistent with the door, window and PC.


Reality-based interaction update (v13)
--------------------------------------
- Each wall switch is now a completely separate target. Aim at exactly one switch and press E.
- FAN / MAIN LIGHT / SPOTLIGHT no longer share direct shortcut controls; use the physical switches.
- A small center reticle turns green only when an interactable object is actually reachable.
- Door, window and PC use balanced interaction distances: close enough to reach naturally, but not face-to-object close.
- Door/window/PC also require aiming at the object; being nearby alone is no longer enough.
- Walking now has acceleration and deceleration for smoother human movement.
- Hold Shift while moving for a natural run speed.
- Collision boxes were tightened so the player does not snag on furniture edges as easily.
- The ceiling fan accelerates and coasts to a stop instead of starting/stopping instantly.

Recommended interaction:
1. Walk toward an object with WASD.
2. Turn with the mouse and place the center reticle on it.
3. When the reticle turns green / title shows the object, press E once.


Realistic balanced interaction update (v14)
-------------------------------------------
- Interaction range is now balanced: you do NOT need to stand almost touching the object.
- Door, double window and PC/CPU work from a natural nearby standing distance.
- Fan/light/spotlight wall switches also have a comfortable nearby reach while remaining independent.
- Rug now follows the same interaction rule as everything else: walk near it, aim at it, and press E.
- Rug no longer moves globally with Z/X/C from anywhere in the room.
- Each E press near the rug smoothly moves it to the opposite side.
- Crosshair turns green only when the aimed object is actually usable.


v15 - Comfortable vision and interaction
----------------------------------------
- Default first-person FOV increased to 62 degrees so nearby objects do not look excessively enlarged.
- Minimum zoom/FOV limited to 50 degrees to prevent accidental extreme zoom-in.
- Door, window, PC, switches, and rug can now be used from a more comfortable standing distance.
- You still need to aim with the center crosshair and press E.
- Switches remain fully independent.

v16 - Natural First Person Walkthrough Design
---------------------------------------------
Design goals:
- First person camera only (no visible character model)
- Natural human eye-height movement
- Comfortable distance for interaction
- No shooting/crosshair style interaction
- Object interaction should happen from a realistic standing position
- Door, window, PC and switches should not fill the whole screen during use

Recommended interaction style:
- Walk naturally near an object
- Look toward the object
- Press E to interact
- Keep a balanced distance instead of touching the object

This version is prepared as the base package for the natural walkthrough update.


v17 Realistic Walkthrough Update:
- Removed FPS shooting-style crosshair.
- Reduced interaction distance to natural human reach.
- Reduced switch/object interaction range so objects do not fill the camera view.
- Interaction remains through proximity + E only.


v18 Realistic Distance Fix
--------------------------
- Removed FPS-style interaction marker.
- Reduced interaction ray ranges.
- Increased natural first-person viewing angle.
- Reduced mouse sensitivity.
- Player stays at a comfortable distance so objects do not fill the screen.


v19 distance and scale polish
- Reduced wall switch board to realistic bedroom size.
- Increased comfortable interaction distance so animations are visible.
- Removed FPS-style aiming feeling.


v20 - Bright Bedroom Color Update
---------------------------------
- Room wall colors adjusted to lighter warm tones.
- Ambient lighting increased for a brighter and cleaner bedroom appearance.
- Existing interaction, movement, switches, PC, door, and window systems kept unchanged.
