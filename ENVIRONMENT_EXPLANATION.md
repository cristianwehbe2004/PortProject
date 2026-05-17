# Port Environment Code Explanation

This document explains how the OpenGL/GLUT port scene in `main.cpp` is built. The project is a single-file C++ graphics program that renders an animated harbor environment with water, a lighthouse, boats, a cargo ship, birds, camera controls, lighting, and debug toggles.

## Overall Structure

The code follows the traditional GLUT program layout:

1. Include OpenGL/GLUT headers.
2. Declare global variables that store scene state.
3. Define helper functions for random values, texture loading, bird spawning, and display-list creation.
4. Define rendering functions such as `display()` and `drawRealisticBird()`.
5. Define animation/update logic in `idle()`.
6. Define window resizing in `reshape()`.
7. Define keyboard input in `keyboard()`.
8. Initialize everything in `main()` and enter the GLUT event loop.

GLUT calls the registered callback functions automatically. The program does not manually create a game loop. Instead, `glutMainLoop()` keeps the application alive and repeatedly calls `display()`, `idle()`, `reshape()`, and `keyboard()` when needed.

## Platform-Specific Includes

At the top of `main.cpp`, the code checks whether it is being compiled on macOS:

```cpp
#ifdef __APPLE__
  #include <GLUT/glut.h>
  #include <OpenGL/glu.h>
#else
  #include <GL/glut.h>
  #include <GL/glu.h>
#endif
```

This is necessary because macOS stores OpenGL and GLUT headers under framework-style paths, while Linux and many Windows/freeglut setups use the `GL/` include directory.

## Global Scene State

Most of the scene is controlled by global variables. This is common in small GLUT projects because GLUT callbacks are plain C-style functions and do not directly belong to an object.

Important globals:

- `gTime`: animation time used by waves, boat motion, and bird wing flapping.
- `gWaveAmp`, `gWaveFreq`, `gWaterBaseY`: control the height, frequency, and base elevation of the water surface.
- `gBoatFloatOffset`, `gBoatBobAmp`, `gBoatRollAmpDeg`: control how boats float, bob, and roll.
- `gLightAngle`: controls the rotating lighthouse spotlight.
- `gCamDistance`, `gCamPanX`, `gCamPanY`: control camera zoom and pan.
- `gShowGround`, `gShowWater`, `gShowLighthouse`, `gShowBoats`, `gShowBirds`, `gShowStats`: runtime toggles for parts of the scene.
- `gGroundList`, `gLighthouseList`, `gBoatMeshList`, `gCargoShipMeshList`: OpenGL display lists for reusable static geometry.
- `gCargoSpeedZ`, `gCargoMoving`: control cargo ship movement.

The scene uses a world coordinate system where:

- `X` moves left/right.
- `Y` moves up/down.
- `Z` moves forward/backward.

## Main Function

The `main()` function is the entry point of the program. It performs all startup work.

### Random Seed and Bird Initialization

```cpp
srand(static_cast<unsigned int>(time(nullptr)));
initBirds();
```

The random seed makes each run spawn the birds in slightly different positions. `initBirds()` calls `spawnBirdFlockFromLeft()`, which creates the first bird flock off the left side of the scene.

### GLUT Initialization

```cpp
glutInit(&argc, argv);
glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
glutInitWindowSize(800, 600);
glutInitWindowPosition(100, 100);
glutCreateWindow("Port Environment");
```

This creates the application window.

- `GLUT_DOUBLE` enables double buffering, so the scene is drawn to a back buffer and then swapped to the screen. This prevents flickering.
- `GLUT_RGB` uses RGB color mode.
- `GLUT_DEPTH` enables a depth buffer, allowing nearer objects to correctly hide farther objects.

### OpenGL State Setup

```cpp
glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
glEnable(GL_DEPTH_TEST);
```

The clear color is sky blue. Depth testing is enabled so 3D objects render with proper visibility.

### Lighting Setup

The scene uses two OpenGL fixed-function lights:

- `GL_LIGHT0`: a warm general light, like sunlight.
- `GL_LIGHT1`: a rotating lighthouse spotlight.

The code enables lighting:

```cpp
glEnable(GL_LIGHTING);
glEnable(GL_LIGHT0);
glEnable(GL_LIGHT1);
```

It also enables color material:

```cpp
glEnable(GL_COLOR_MATERIAL);
glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
```

This means calls like `glColor3f(...)` also affect the material color of lit objects. Without this, object colors would not respond as simply under OpenGL lighting.

Smooth shading is enabled with:

```cpp
glShadeModel(GL_SMOOTH);
```

### Texture and Mesh Preparation

```cpp
tryLoadOptionalTextures();
buildDisplayLists();
```

`tryLoadOptionalTextures()` tries to load `sea.bmp`. If the file exists and is a valid uncompressed 24-bit BMP, the water uses it as a repeating texture. If not, the water is drawn with a plain blue color.

`buildDisplayLists()` creates reusable OpenGL display lists for static geometry:

- Ground plane
- Lighthouse body
- Regular boat mesh
- Cargo ship mesh

Display lists are an older OpenGL feature that store drawing commands for later reuse. This is useful here because the same boat mesh is drawn multiple times.

### Callback Registration

```cpp
glutDisplayFunc(display);
glutReshapeFunc(reshape);
glutIdleFunc(idle);
glutKeyboardFunc(keyboard);
```

These lines tell GLUT which functions to call:

- `display()` draws the scene.
- `reshape()` updates projection when the window size changes.
- `idle()` updates animation whenever GLUT is not handling another event.
- `keyboard()` handles key presses.

Finally:

```cpp
glutMainLoop();
```

This starts the program loop. GLUT takes control from this point onward.

## Init-Like Functions

There is no separate function named `init()`, but the initialization work is split between `main()`, `initBirds()`, `tryLoadOptionalTextures()`, and `buildDisplayLists()`.

### `initBirds()`

`initBirds()` calls `spawnBirdFlockFromLeft()`. This fills the `gBirds` array with randomized positions and velocities.

Each bird starts:

- Far left of the scene, around `x = -55` to `-45`.
- At a random flying height, around `y = 12` to `23`.
- With a positive `vx`, so it moves from left to right.

### `tryLoadOptionalTextures()`

This function tries to load `sea.bmp` using `loadTextureFromBMP()`. If loading succeeds:

- `gUseSeaTexture` becomes `true`.
- The OpenGL texture id is stored in `gSeaTex`.

If loading fails, the program simply continues with colored water.

### `buildDisplayLists()`

This function prebuilds static geometry:

- `gGroundList`: a green flat quad.
- `gLighthouseList`: a striped cylinder tower and a sphere lantern.
- `gBoatMeshList`: a small wooden boat made from scaled cubes.
- `gCargoShipMeshList`: a larger ship with hull, deck, containers, and bridge.

The dynamic parts of the scene are not fully stored in display lists. For example, boat placement, boat rotation, water waves, bird movement, and lighthouse spotlight rotation are computed every frame.

## Camera

The camera is configured inside `display()` using `gluLookAt()`:

```cpp
gluLookAt(
    gCamPanX, 20.0 + gCamPanY, gCamDistance,
    gCamPanX, gCamPanY, 0.0,
    0.0, 1.0, 0.0
);
```

This creates a fixed elevated view looking toward the center of the port.

The camera position is:

- `x = gCamPanX`
- `y = 20.0 + gCamPanY`
- `z = gCamDistance`

The camera target is:

- `x = gCamPanX`
- `y = gCamPanY`
- `z = 0.0`

This means panning moves both the camera and the target together, while zooming changes only the camera's distance along the Z axis.

Keyboard controls:

- `+` or `=`: zoom in by reducing `gCamDistance`.
- `-` or `_`: zoom out by increasing `gCamDistance`.
- `I/K`: pan up/down.
- `J/L`: pan left/right.

## Projection and Window Resizing

The `reshape()` function updates the viewport and projection matrix whenever the window is resized.

```cpp
glViewport(0, 0, w, h);
gluPerspective(60.0, static_cast<double>(w) / h, 0.1, 100.0);
```

The projection uses:

- `60.0` degree field of view.
- Aspect ratio based on the window width and height.
- Near clipping plane at `0.1`.
- Far clipping plane at `100.0`.

This gives the scene perspective depth instead of an orthographic flat look.

## Lighting

The scene uses old OpenGL fixed-function lighting.

### Sun Light: `GL_LIGHT0`

`GL_LIGHT0` is initialized in `main()` with warm ambient, diffuse, and specular values.

In every `display()` call, its position is reset:

```cpp
GLfloat light_pos[] = { 10.0f, 20.0f, 10.0f, 1.0f };
glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
```

Because this happens after the camera transform, the light is placed consistently in world space for the current frame.

### Lighthouse Spotlight: `GL_LIGHT1`

`GL_LIGHT1` is configured in `main()` as a warm bright spotlight. Its position and direction are updated inside the lighthouse drawing block in `display()`.

The code moves to the lighthouse position, then up to the lantern room:

```cpp
glTranslatef(40.0f, 0.0f, 0.0f);
glTranslatef(0.0f, 15.0f, 0.0f);
glRotatef(gLightAngle, 0.0f, 1.0f, 0.0f);
```

Then it places the spotlight at the lantern center and points it along local `+Z`:

```cpp
GLfloat light1_pos[] = { 0.0f, 0.0f, 0.0f, 1.0f };
GLfloat spot_dir[]   = { 0.0f, 0.0f, 1.0f };
glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, spot_dir);
glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 30.0f);
glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 2.0f);
```

The rotation comes from `gLightAngle`, which is increased in `idle()`. This makes the lighthouse beam sweep around the harbor.

## Ground

The ground is a single green quad stored in `gGroundList`.

It spans from `-40` to `40` on both X and Z, with `Y = 0`.

```cpp
glVertex3f(-40.0f, 0.0f, -40.0f);
glVertex3f( 40.0f, 0.0f, -40.0f);
glVertex3f( 40.0f, 0.0f,  40.0f);
glVertex3f(-40.0f, 0.0f,  40.0f);
```

It is drawn in `display()` only if `gShowGround` is true.

## Water and Waves

The water is generated every frame as a grid of quads from `gWaterGridMin` to `gWaterGridMax`, which is `-45` to `45`.

The whole water mesh is moved upward by `gWaterBaseY`:

```cpp
glTranslatef(0.0f, gWaterBaseY, 0.0f);
```

Each grid cell computes its vertex height using a sine wave:

```cpp
float y1 = gWaveAmp * sinf(x * gWaveFreq + gTime);
float y2 = gWaveAmp * sinf((x + 1) * gWaveFreq + gTime);
```

The water animation comes from `gTime`, which increases in `idle()`:

```cpp
gTime += 0.02f;
```

Because `gTime` changes every frame, the sine value changes, causing the surface to move.

The wave depends mainly on the X coordinate, so the water forms parallel rolling wave bands across the scene.

### Water Texture

If `sea.bmp` loads successfully, water texturing is enabled:

```cpp
glEnable(GL_TEXTURE_2D);
glBindTexture(GL_TEXTURE_2D, gSeaTex);
```

Each water quad receives texture coordinates:

```cpp
glTexCoord2f(x * 0.07f, z * 0.07f);
```

The texture uses `GL_REPEAT`, so it tiles across the whole water surface. If no texture is found, the water is drawn blue with `glColor3f(0.0f, 0.5f, 0.9f)`.

## BMP Texture Loading

The project includes a small custom loader for uncompressed 24-bit BMP files.

`loadBMP24()`:

- Opens the file.
- Reads the BMP file header and info header.
- Verifies that the file is a BMP.
- Verifies that it is 24 bits per pixel.
- Rejects compressed BMP files.
- Reads the pixel data.
- Converts BGR order from BMP into RGB order for OpenGL.
- Flips the image vertically because BMP stores rows bottom-to-top.

`loadTextureFromBMP()` then creates an OpenGL texture:

- Generates a texture id.
- Binds it.
- Sets filtering options.
- Sets wrapping to repeat.
- Builds mipmaps with `gluBuild2DMipmaps()`.

## Lighthouse Geometry

The lighthouse is stored in `gLighthouseList`.

It has two parts:

1. A striped tapered tower.
2. A spherical lantern room.

The tower is built with `gluCylinder()`. Since GLU cylinders are aligned along the Z axis by default, the code rotates the cylinder by `-90` degrees around X so it stands vertically along Y:

```cpp
glRotatef(-90, 1.0f, 0.0f, 0.0f);
```

The tower is divided into five stripes:

```cpp
int stripes = 5;
```

Each stripe alternates between white and black:

```cpp
if (i % 2 == 0) glColor3f(1.0f, 1.0f, 1.0f);
else            glColor3f(0.1f, 0.1f, 0.1f);
```

The radius becomes smaller toward the top:

- Base radius: `3.0`
- Top radius: `2.0`
- Height: `15.0`

The lantern is drawn as a pale yellow sphere at `Y = 15`.

In `display()`, the full lighthouse is translated to `X = 40`, placing it on the right side of the scene.

## Regular Boats

Regular boats are described by the `Boat` struct:

```cpp
struct Boat
{
    float x;
    float z;
    float phase;
    int type;
    float motionScale;
    float floatOffset;
};
```

The regular boat mesh is stored in `gBoatMeshList`. It is made from two cubes:

- A brown scaled cube for the hull.
- A smaller light-colored cube for the cabin.

The hull is scaled to:

```cpp
glScalef(2.0f, 0.8f, 6.0f);
```

This makes it long along the Z axis and narrow along the X axis.

### Floating on the Waves

In `display()`, each boat is placed on the water by using the same sine wave formula as the water mesh:

```cpp
float surfaceY = gWaveAmp * sinf(gBoats[i].x * gWaveFreq + gTime);
```

Then the boat is translated to:

```cpp
gWaterBaseY + surfaceY + gBoats[i].floatOffset + bobY
```

This makes boats follow the animated water height instead of floating at a fixed Y position.

### Boat Motion

Regular boats also get animated rotations:

- `yawDrift`: turns left/right around Y.
- `pitchDrift`: tips forward/back around X.
- `rollDrift`: rocks side-to-side around Z.

These are all created with sine functions using `gTime` and each boat's `phase`, so boats move differently from each other.

## Cargo Ship

The cargo ship is a special boat with `type = BOAT_TYPE_CARGO`.

It is larger and heavier-looking than the regular boats. The mesh is stored in `gCargoShipMeshList` and contains:

- Dark hull
- Gray deck strip
- Colored container blocks
- White bridge block near the stern

The hull is scaled to:

```cpp
glScalef(5.0f, 1.0f, 14.0f);
```

This makes it much wider and longer than the regular boats.

### Containers

Containers are generated with nested loops:

```cpp
for (int row = -1; row <= 1; ++row)
{
    for (int c = -2; c <= 2; ++c)
    {
        ...
    }
}
```

This creates three rows and five columns of containers. Their colors alternate between red, blue, and yellow/gold using `(row + c) % 3`.

### Heavy Movement

The cargo ship uses smaller motion values than the regular boats:

- Smaller bobbing amplitude.
- Slower yaw.
- Smaller pitch and roll.

This makes the cargo ship feel heavier and more stable.

### Cargo Ship Forward Drift

Only the cargo ship changes its `z` position over time:

```cpp
gBoats[i].z += gCargoSpeedZ;
```

When it passes `z = 42`, it wraps back to `z = -42`:

```cpp
if (gBoats[i].z > 42.0f)
    gBoats[i].z = -42.0f;
```

Pressing `M` toggles `gCargoMoving`, which starts or stops this forward movement.

## Birds

Birds are represented by the `Bird` struct:

```cpp
struct Bird
{
    float x, y, z;
    float vx, vy, vz;
};
```

Each bird stores position and velocity. There are 12 birds:

```cpp
static const int gBirdCount = 12;
```

## Bird Spawning

`spawnBirdFlockFromLeft()` creates a flock entering from the left side.

It randomizes:

- Flock height.
- Flock depth.
- Individual bird position.
- Individual velocity.

All birds start with positive X velocity, so they fly left to right.

When every bird has passed `x = 56`, the flock respawns on the left.

## Bird Flocking

The flocking behavior is implemented in `idle()`.

It uses three classic flocking rules:

### Separation

If birds get too close, they push away from each other. This prevents them from collapsing into one point.

The code uses `separationRadius` and calculates a repulsion vector.

### Alignment

Nearby birds try to match each other's velocity. This makes them fly in a shared direction.

The code averages neighbor velocities and nudges each bird toward that average.

### Cohesion

Nearby birds move toward the average position of their neighbors. This keeps the flock together.

The code averages neighbor positions and nudges the bird toward that center.

### Style Presets

The bird style changes the flocking constants:

- `1`: tight flock
- `2`: loose flock
- `3`: fast seagull-style flock

These modes adjust values such as:

- Neighbor radius
- Separation radius
- Separation weight
- Alignment weight
- Cohesion weight
- Maximum speed

## Bird Drawing

Birds are drawn in `drawRealisticBird()`.

Each bird is built from simple 3D primitives:

- Ellipsoid body using a scaled sphere.
- Small sphere head.
- Cone beak.
- Cone tail.
- Cube wing segments.

The wings flap by rotating them according to a sine wave:

```cpp
float flap = flapAmp * sinf(gTime * flapFreq + static_cast<float>(i) * 0.6f);
```

Each bird has a slightly different flap frequency and phase, so they do not flap perfectly in sync.

During bird rendering, lighting is temporarily disabled:

```cpp
glDisable(GL_LIGHTING);
```

This keeps the birds visible as dark silhouettes against the sky. Lighting is re-enabled afterward.

## Display Function

`display()` is the main render callback. It draws one complete frame.

The order is:

1. Clear color and depth buffers.
2. Reset the model-view matrix.
3. Set the camera using `gluLookAt()`.
4. Place the sun light.
5. Draw ground if enabled.
6. Draw animated water if enabled.
7. Draw lighthouse and update spotlight if enabled.
8. Draw boats and cargo ship if enabled.
9. Draw birds if enabled.
10. Print FPS/debug stats once per second.
11. Swap the back buffer to the screen.

The use of `glPushMatrix()` and `glPopMatrix()` is important. Each object applies its own translation, rotation, and scale, then restores the previous matrix so it does not accidentally affect the next object.

## Idle Function

`idle()` updates the animated state of the scene.

It performs:

- Water time update through `gTime`.
- Lighthouse rotation through `gLightAngle`.
- Boat phase updates.
- Cargo ship forward movement.
- Bird flocking calculations.
- Bird position updates.
- Bird respawn when the flock exits the scene.
- A call to `glutPostRedisplay()`.

`glutPostRedisplay()` tells GLUT that the scene needs to be redrawn. This causes another `display()` call.

## Keyboard Function

`keyboard()` handles runtime controls.

Quit:

- `ESC`
- `Q`

Bird styles:

- `1`: tight
- `2`: loose
- `3`: fast seagull

Camera:

- `+` or `=`: zoom in
- `-` or `_`: zoom out
- `I`: pan up
- `K`: pan down
- `J`: pan left
- `L`: pan right

Scene toggles:

- `G`: ground
- `W`: water
- `H`: lighthouse
- `O`: boats
- `B`: birds
- `P`: FPS/debug printing

Cargo:

- `M`: toggle cargo ship movement

At the end of `keyboard()`, `glutPostRedisplay()` asks GLUT to redraw the scene after the input change.

## Rendering Techniques Used

This project uses classic OpenGL immediate-mode and fixed-function features:

- `glBegin()` / `glEnd()` for drawing the water and ground quads.
- `glTranslatef()`, `glRotatef()`, and `glScalef()` for object transforms.
- `glPushMatrix()` and `glPopMatrix()` for hierarchical transforms.
- GLU shapes such as cylinders and mipmap generation.
- GLUT shapes such as cubes, spheres, and cones.
- Display lists for reusable static meshes.
- Fixed-function lighting with `GL_LIGHT0` and `GL_LIGHT1`.
- Texture loading and binding for optional water texture.

These techniques are older than modern shader-based OpenGL, but they are useful for learning because the scene logic is visible and direct.

## Environment Summary

The final environment works as a complete animated harbor scene:

- The sky is created using the clear color.
- The camera looks down from an elevated position.
- A green ground plane forms the island/land base.
- A large animated water grid creates moving waves.
- The lighthouse stands on the right side and casts a rotating spotlight.
- Regular boats float by sampling the same sine wave as the water surface.
- A larger cargo ship moves forward more slowly and wraps around the scene.
- Birds fly across the sky using flocking behavior and animated wings.
- Keyboard controls allow the user to inspect and debug individual parts of the scene.

The main idea is that every animated object is connected to simple mathematical state: sine waves for water and rocking, angle accumulation for the lighthouse, velocity vectors for birds, and keyboard-controlled variables for camera and toggles.
