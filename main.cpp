// ============================================================
//  Port Environment – Step 1: Project Skeleton
//  Compile (macOS):
//    g++ main.cpp -o port -framework OpenGL -framework GLUT
//  Compile (Linux with freeglut):
//    g++ main.cpp -o port -lGL -lGLU -lglut
// ============================================================

#ifdef __APPLE__
  #include <GLUT/glut.h>
  #include <OpenGL/glu.h>
#else
  #include <GL/glut.h>
  #include <GL/glu.h>
#endif

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>

// ------------------------------------------------------------------
// Global animation time (updated by idle)
// ------------------------------------------------------------------
static float gTime = 0.0f;
static float gWaveAmp = 1.4f;
static float gWaveFreq = 0.1f;
static float gWaterBaseY = 1.6f;
static float gBoatFloatOffset = 0.35f;
static float gBoatBobAmp = 0.12f;
static float gBoatRollAmpDeg = 2.0f;
static float gLightAngle = 0.0f;
static float gCamDistance = 50.0f;
static float gCamPanX = 0.0f;
static float gCamPanY = 0.0f;

static bool gShowGround = true;
static bool gShowWater = true;
static bool gShowLighthouse = true;
static bool gShowBoats = true;
static bool gShowBirds = true;
static bool gShowStats = true;

static const int gWaterGridMin = -45;
static const int gWaterGridMax = 45;

static int gFrameCounter = 0;
static int gLastFpsTimeMs = 0;

static bool gUseSeaTexture = false;
static GLuint gSeaTex = 0;

static GLuint gGroundList = 0;
static GLuint gLighthouseList = 0;
static GLuint gBoatMeshList = 0;
static GLuint gCargoShipMeshList = 0;
static float gCargoSpeedZ = 0.035f;
static bool gCargoMoving = true;

struct Bird
{
    float x, y, z;
    float vx, vy, vz;
};

enum BirdStyle
{
    BIRD_STYLE_TIGHT = 1,
    BIRD_STYLE_LOOSE = 2,
    BIRD_STYLE_SEAGULL = 3
};

static const int gBirdCount = 12;
static Bird gBirds[gBirdCount];
static int gBirdStyle = BIRD_STYLE_SEAGULL;

static float frandRange(float minVal, float maxVal)
{
    return minVal + (maxVal - minVal) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}

static void spawnBirdFlockFromLeft()
{
    float centerY = frandRange(12.0f, 23.0f);
    float centerZ = frandRange(-20.0f, 20.0f);

    for (int i = 0; i < gBirdCount; ++i)
    {
        gBirds[i].x = frandRange(-55.0f, -45.0f);
        gBirds[i].y = centerY + frandRange(-4.0f, 4.0f);
        gBirds[i].z = centerZ + frandRange(-10.0f, 10.0f);
        gBirds[i].vx = frandRange(0.24f, 0.42f);
        gBirds[i].vy = frandRange(-0.06f, 0.06f);
        gBirds[i].vz = frandRange(-0.10f, 0.10f);
    }
}

static void initBirds()
{
    spawnBirdFlockFromLeft();
}

static bool loadBMP24(const char* path, int& width, int& height, unsigned char*& outData)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    unsigned char fileHeader[14];
    unsigned char infoHeader[40];

    if (fread(fileHeader, 1, 14, f) != 14) { fclose(f); return false; }
    if (fread(infoHeader, 1, 40, f) != 40) { fclose(f); return false; }

    if (fileHeader[0] != 'B' || fileHeader[1] != 'M') { fclose(f); return false; }

    int bpp = infoHeader[14] | (infoHeader[15] << 8);
    int compression = infoHeader[16] | (infoHeader[17] << 8) | (infoHeader[18] << 16) | (infoHeader[19] << 24);
    if (bpp != 24 || compression != 0) { fclose(f); return false; }

    width = infoHeader[4] | (infoHeader[5] << 8) | (infoHeader[6] << 16) | (infoHeader[7] << 24);
    height = infoHeader[8] | (infoHeader[9] << 8) | (infoHeader[10] << 16) | (infoHeader[11] << 24);
    if (width <= 0 || height <= 0) { fclose(f); return false; }

    int rowPadded = (width * 3 + 3) & (~3);
    unsigned char* bmp = new unsigned char[rowPadded * height];
    if (fread(bmp, 1, rowPadded * height, f) != static_cast<size_t>(rowPadded * height))
    {
        delete[] bmp;
        fclose(f);
        return false;
    }
    fclose(f);

    outData = new unsigned char[width * height * 3];
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int src = y * rowPadded + x * 3;
            int dst = (height - 1 - y) * width * 3 + x * 3;
            // BMP stores BGR; OpenGL RGB expected.
            outData[dst + 0] = bmp[src + 2];
            outData[dst + 1] = bmp[src + 1];
            outData[dst + 2] = bmp[src + 0];
        }
    }

    delete[] bmp;
    return true;
}

static bool loadTextureFromBMP(const char* path, GLuint& texId)
{
    int w = 0, h = 0;
    unsigned char* data = nullptr;
    if (!loadBMP24(path, w, h, data))
        return false;

    texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

    delete[] data;
    return true;
}

static void tryLoadOptionalTextures()
{
    if (loadTextureFromBMP("sea.bmp", gSeaTex))
    {
        gUseSeaTexture = true;
        printf("Loaded optional texture: sea.bmp\n");
    }
    else
    {
        gUseSeaTexture = false;
    }
}

static void buildDisplayLists()
{
    gGroundList = glGenLists(1);
    glNewList(gGroundList, GL_COMPILE);
        glColor3f(0.45f, 0.62f, 0.36f);
        glBegin(GL_QUADS);
            glNormal3f(0.0f, 1.0f, 0.0f);
            glVertex3f(-40.0f, 0.0f, -40.0f);
            glVertex3f( 40.0f, 0.0f, -40.0f);
            glVertex3f( 40.0f, 0.0f,  40.0f);
            glVertex3f(-40.0f, 0.0f,  40.0f);
        glEnd();
    glEndList();

    gLighthouseList = glGenLists(1);
    glNewList(gLighthouseList, GL_COMPILE);
        GLUquadric* quad = gluNewQuadric();

        // Tower body (striped)
        glPushMatrix();
            glRotatef(-90, 1.0f, 0.0f, 0.0f);

            float baseRad = 3.0f;
            float topRad  = 2.0f;
            float height  = 15.0f;
            int   stripes = 5;
            float hStep   = height / stripes;
            float rStep   = (baseRad - topRad) / stripes;

            for (int i = 0; i < stripes; ++i)
            {
                if (i % 2 == 0) glColor3f(1.0f, 1.0f, 1.0f);
                else            glColor3f(0.1f, 0.1f, 0.1f);

                float r1 = baseRad - (i * rStep);
                float r2 = baseRad - ((i + 1) * rStep);
                gluCylinder(quad, r1, r2, hStep, 16, 1);
                glTranslatef(0.0f, 0.0f, hStep);
            }
        glPopMatrix();

        // Lantern geometry (static mesh only)
        glPushMatrix();
            glTranslatef(0.0f, 15.0f, 0.0f);
            glColor3f(1.0f, 1.0f, 0.8f);
            glutSolidSphere(2.0, 16, 16);
        glPopMatrix();

        gluDeleteQuadric(quad);
    glEndList();

    gBoatMeshList = glGenLists(1);
    glNewList(gBoatMeshList, GL_COMPILE);
        glColor3f(0.55f, 0.28f, 0.12f);
        glPushMatrix();
            glScalef(2.0f, 0.8f, 6.0f);
            glutSolidCube(1.0f);
        glPopMatrix();

        glColor3f(0.95f, 0.95f, 0.9f);
        glPushMatrix();
            glTranslatef(0.0f, 0.6f, -0.5f);
            glScalef(1.0f, 0.7f, 1.6f);
            glutSolidCube(1.0f);
        glPopMatrix();
    glEndList();

    gCargoShipMeshList = glGenLists(1);
    glNewList(gCargoShipMeshList, GL_COMPILE);
        // Hull
        glColor3f(0.14f, 0.16f, 0.20f);
        glPushMatrix();
            glScalef(5.0f, 1.0f, 14.0f);
            glutSolidCube(1.0f);
        glPopMatrix();

        // Deck strip
        glColor3f(0.35f, 0.35f, 0.35f);
        glPushMatrix();
            glTranslatef(0.0f, 0.55f, 0.0f);
            glScalef(4.4f, 0.12f, 12.0f);
            glutSolidCube(1.0f);
        glPopMatrix();

        // Containers in rows
        for (int row = -1; row <= 1; ++row)
        {
            for (int c = -2; c <= 2; ++c)
            {
                if ((row + c) % 3 == 0) glColor3f(0.75f, 0.22f, 0.18f);
                else if ((row + c) % 3 == 1) glColor3f(0.18f, 0.42f, 0.75f);
                else glColor3f(0.78f, 0.58f, 0.20f);

                glPushMatrix();
                    glTranslatef(row * 1.15f, 1.15f, c * 2.1f);
                    glScalef(0.95f, 0.8f, 1.8f);
                    glutSolidCube(1.0f);
                glPopMatrix();
            }
        }

        // Bridge block near stern
        glColor3f(0.90f, 0.90f, 0.88f);
        glPushMatrix();
            glTranslatef(0.0f, 1.55f, -5.1f);
            glScalef(1.4f, 1.2f, 1.8f);
            glutSolidCube(1.0f);
        glPopMatrix();
    glEndList();
}

struct Boat
{
    float x;
    float z;
    float phase;
    int type;
    float motionScale;
    float floatOffset;
};

enum BoatType
{
    BOAT_TYPE_REGULAR = 0,
    BOAT_TYPE_CARGO = 1
};

static Boat gBoats[] = {
    { -20.0f,   0.0f, 0.0f, BOAT_TYPE_REGULAR, 1.0f, gBoatFloatOffset },
    {  10.0f,  10.0f, 1.5f, BOAT_TYPE_REGULAR, 1.0f, gBoatFloatOffset },
    {   0.0f, -20.0f, 2.8f, BOAT_TYPE_REGULAR, 1.0f, gBoatFloatOffset },
    { -32.0f,  18.0f, 4.1f, BOAT_TYPE_REGULAR, 1.0f, gBoatFloatOffset },
    {  24.0f,  -8.0f, 0.9f, BOAT_TYPE_CARGO,   0.45f, 0.50f }
};
static const int gBoatCount = sizeof(gBoats) / sizeof(gBoats[0]);

// ------------------------------------------------------------------
// drawRealisticBird  –  seagull silhouette visible at distance
//
//  Bird faces +Z (forward). All geometry is in the XZ plane (Y=0)
//  so it reads as a top-down silhouette from the elevated camera.
//  Wing span is ~4 world units so it is clearly visible.
//
//  flap  : wing-tip lift in world units (positive = up)
//  scale : uniform size multiplier (pass 1.0 normally)
// ------------------------------------------------------------------
static void drawRealisticBird(float flapAngleDeg, float scale)
{
    // Volumetric bird made of compact primitives for a smooth 3D look.
    float flapRad = flapAngleDeg * 3.14159265f / 180.0f;

    // Body (ellipsoid)
    glColor3f(0.04f, 0.04f, 0.04f);
    glPushMatrix();
        glScalef(0.22f * scale, 0.16f * scale, 0.50f * scale);
        glutSolidSphere(1.0, 14, 14);
    glPopMatrix();

    // Head
    glColor3f(0.03f, 0.03f, 0.03f);
    glPushMatrix();
        glTranslatef(0.0f, 0.03f * scale, 0.48f * scale);
        glutSolidSphere(0.12f * scale, 12, 12);
    glPopMatrix();

    // Beak
    glColor3f(0.02f, 0.02f, 0.02f);
    glPushMatrix();
        glTranslatef(0.0f, 0.03f * scale, 0.60f * scale);
        glutSolidCone(0.035f * scale, 0.12f * scale, 10, 2);
    glPopMatrix();

    // Tail fin (small wedge-like cone)
    glColor3f(0.03f, 0.03f, 0.03f);
    glPushMatrix();
        glTranslatef(0.0f, 0.0f, -0.50f * scale);
        glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(-18.0f, 1.0f, 0.0f, 0.0f);
        glutSolidCone(0.08f * scale, 0.15f * scale, 8, 2);
    glPopMatrix();

    // Wings as two thick articulated segments on each side.
    for (int side = -1; side <= 1; side += 2)
    {
        float s = static_cast<float>(side);
        float wingLift = sinf(flapRad) * 16.0f;

        // Inner wing segment
        glColor3f(0.03f, 0.03f, 0.03f);
        glPushMatrix();
            glTranslatef(s * 0.16f * scale, 0.01f * scale, 0.02f * scale);
            glRotatef(s * wingLift, 0.0f, 0.0f, 1.0f);
            glTranslatef(s * 0.30f * scale, 0.0f, -0.03f * scale);
            glScalef(0.60f * scale, 0.05f * scale, 0.23f * scale);
            glutSolidCube(1.0f);
        glPopMatrix();

        // Outer wing segment
        glColor3f(0.01f, 0.01f, 0.01f);
        glPushMatrix();
            glTranslatef(s * 0.50f * scale, 0.02f * scale, -0.01f * scale);
            glRotatef(s * (wingLift * 1.25f), 0.0f, 0.0f, 1.0f);
            glTranslatef(s * 0.30f * scale, 0.0f, -0.01f * scale);
            glScalef(0.60f * scale, 0.04f * scale, 0.19f * scale);
            glutSolidCube(1.0f);
        glPopMatrix();
    }
}

// ------------------------------------------------------------------
// reshape – called whenever the window is resized
// ------------------------------------------------------------------
void reshape(int w, int h)
{
    if (h == 0) h = 1;                      // avoid division by zero

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
        60.0,                               // field-of-view (degrees)
        static_cast<double>(w) / h,         // aspect ratio
        0.1,                                // near clip
        100.0                               // far clip
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ------------------------------------------------------------------
// display – main render callback
// ------------------------------------------------------------------
void display()
{
    // Clear colour + depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Step 7 camera: fixed angle with keyboard zoom/pan
    gluLookAt(
        gCamPanX, 20.0 + gCamPanY, gCamDistance,
        gCamPanX, gCamPanY, 0.0,
        0.0, 1.0,  0.0    // up vector
    );

    // ---- Re-set light position each frame (in world space) --------
    GLfloat light_pos[] = { 10.0f, 20.0f, 10.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    // ---- Basic ground (static display list) ------------------------
    if (gShowGround && gGroundList)
        glCallList(gGroundList);

    // ---- Animated water grid (Step 3) ------------------------------
    if (gShowWater)
    {
        glPushMatrix();
        glTranslatef(0.0f, gWaterBaseY, 0.0f);

        if (gUseSeaTexture)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, gSeaTex);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.0f, 0.5f, 0.9f);
        }

        for (int x = gWaterGridMin; x < gWaterGridMax; ++x)
        {
            for (int z = gWaterGridMin; z < gWaterGridMax; ++z)
            {
                float y1 = gWaveAmp * sinf(x * gWaveFreq + gTime);
                float y2 = gWaveAmp * sinf((x + 1) * gWaveFreq + gTime);
                float y3 = gWaveAmp * sinf((x + 1) * gWaveFreq + gTime);
                float y4 = gWaveAmp * sinf(x * gWaveFreq + gTime);

                glBegin(GL_QUADS);
                    glNormal3f(0.0f, 1.0f, 0.0f);
                    glTexCoord2f(x * 0.07f, z * 0.07f);
                    glVertex3f(static_cast<float>(x),     y1, static_cast<float>(z));
                    glTexCoord2f((x + 1) * 0.07f, z * 0.07f);
                    glVertex3f(static_cast<float>(x + 1), y2, static_cast<float>(z));
                    glTexCoord2f((x + 1) * 0.07f, (z + 1) * 0.07f);
                    glVertex3f(static_cast<float>(x + 1), y3, static_cast<float>(z + 1));
                    glTexCoord2f(x * 0.07f, (z + 1) * 0.07f);
                    glVertex3f(static_cast<float>(x),     y4, static_cast<float>(z + 1));
                glEnd();
            }
        }

        if (gUseSeaTexture)
            glDisable(GL_TEXTURE_2D);

        glPopMatrix();
    }

    // ---- Lighthouse (Step 4) ---------------------------------------
    if (gShowLighthouse)
    {
        glPushMatrix();
            glTranslatef(40.0f, 0.0f, 0.0f);

            if (gLighthouseList)
                glCallList(gLighthouseList);

            // Dynamic spotlight sweep at the lantern room.
            glPushMatrix();
                glTranslatef(0.0f, 15.0f, 0.0f);
                glRotatef(gLightAngle, 0.0f, 1.0f, 0.0f);

                GLfloat light1_pos[] = { 0.0f, 0.0f, 0.0f, 1.0f };
                GLfloat spot_dir[]   = { 0.0f, 0.0f, 1.0f };

                glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
                glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, spot_dir);
                glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 30.0f);
                glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 2.0f);
            glPopMatrix();
        glPopMatrix();
    }

    // ---- Boats floating on waves (Step 5) --------------------------
    if (gShowBoats)
    {
        for (int i = 0; i < gBoatCount; ++i)
        {
            // Match boat height to the same wave function used by the water mesh.
            float surfaceY = gWaveAmp * sinf(gBoats[i].x * gWaveFreq + gTime);
            bool isCargo = (gBoats[i].type == BOAT_TYPE_CARGO);

            float bobY;
            float yawDrift;
            float pitchDrift;
            float rollDrift;

            if (isCargo)
            {
                // Cargo ship feels heavy: slower and lower-amplitude motion.
                bobY = (gBoatBobAmp * 0.45f) * (sinf(gTime * 1.6f + gBoats[i].phase) * 0.5f + 0.5f);
                yawDrift = sinf(gTime * 0.35f + gBoats[i].phase) * 2.0f;
                pitchDrift = sinf(gTime * 0.90f + gBoats[i].phase) * 1.8f;
                rollDrift = sinf(gTime * 1.10f + gBoats[i].phase + 0.4f) * 0.8f;
            }
            else
            {
                bobY = gBoatBobAmp * (sinf(gTime * 3.4f + gBoats[i].phase) * 0.5f + 0.5f);
                yawDrift = sinf(gTime * 0.9f + gBoats[i].phase) * 8.0f;
                pitchDrift = sinf(gTime * 2.2f + gBoats[i].phase) * 6.0f;
                rollDrift = sinf(gTime * 2.8f + gBoats[i].phase + 0.6f) * gBoatRollAmpDeg;
            }

            glPushMatrix();
                glTranslatef(gBoats[i].x, gWaterBaseY + surfaceY + gBoats[i].floatOffset + bobY, gBoats[i].z);
                glRotatef(yawDrift, 0.0f, 1.0f, 0.0f);
                glRotatef(pitchDrift, 1.0f, 0.0f, 0.0f);
                glRotatef(rollDrift, 0.0f, 0.0f, 1.0f);

                if (isCargo && gCargoShipMeshList)
                    glCallList(gCargoShipMeshList);
                else if (gBoatMeshList)
                    glCallList(gBoatMeshList);
            glPopMatrix();
        }
    }

    // ---- Birds (Step 6) --------------------------------------------
    if (gShowBirds)
    {
        // Keep birds unlit for silhouette clarity.
        glDisable(GL_LIGHTING);

        for (int i = 0; i < gBirdCount; ++i)
        {
            float speedXZ = sqrtf(gBirds[i].vx * gBirds[i].vx + gBirds[i].vz * gBirds[i].vz);
            float yaw   = atan2f(gBirds[i].vx, gBirds[i].vz) * 57.29578f;
            float pitch = -atan2f(gBirds[i].vy, speedXZ + 0.0001f) * 57.29578f;

            float flapFreq  = 3.5f + static_cast<float>(i % 4) * 0.5f;
            float flapAmp   = 30.0f;
            float flap      = flapAmp * sinf(gTime * flapFreq + static_cast<float>(i) * 0.6f);

            float birdScale = 2.0f + 0.64f * (static_cast<float>(i % 5) / 4.0f);

            glPushMatrix();
                glTranslatef(gBirds[i].x, gBirds[i].y, gBirds[i].z);
                glRotatef(yaw,   0.0f, 1.0f, 0.0f);
                glRotatef(pitch, 1.0f, 0.0f, 0.0f);
                drawRealisticBird(flap, birdScale);
            glPopMatrix();
        }

        glEnable(GL_LIGHTING);
    }

    // Debug stats once per second.
    gFrameCounter++;
    int nowMs = glutGet(GLUT_ELAPSED_TIME);
    if (gShowStats && nowMs - gLastFpsTimeMs >= 1000)
    {
        float fps = gFrameCounter * 1000.0f / (nowMs - gLastFpsTimeMs);
        printf("time=%.2f fps=%.1f birds=%d waterGrid=%dx%d\n",
               gTime,
               fps,
               gBirdCount,
               (gWaterGridMax - gWaterGridMin),
               (gWaterGridMax - gWaterGridMin));
        gFrameCounter = 0;
        gLastFpsTimeMs = nowMs;
    }

    // swap front/back buffers (double buffering)
    glutSwapBuffers();
}

// ------------------------------------------------------------------
// idle – called when no other events are pending
// ------------------------------------------------------------------
void idle()
{
    gTime += 0.02f;           // wave animation phase step
    gLightAngle += 1.0f;      // rotate lighthouse 1 deg/frame
    if (gLightAngle >= 360.0f) gLightAngle -= 360.0f;

    for (int i = 0; i < gBoatCount; ++i)
    {
        if (gBoats[i].type == BOAT_TYPE_CARGO)
        {
            // Only cargo ship is slower.
            gBoats[i].phase += 0.010f * gBoats[i].motionScale;

            // Slow forward drift for cargo ship (regular boats stay static in x/z).
            if (gCargoMoving)
            {
                gBoats[i].z += gCargoSpeedZ;
                if (gBoats[i].z > 42.0f)
                    gBoats[i].z = -42.0f;
            }
        }
        else
        {
            // Regular boats keep normal speed profile.
            gBoats[i].phase += 0.010f + 0.0015f * static_cast<float>(i % 3);
        }
    }

    // Step 6: flocking birds (separation + alignment + cohesion)
    float neighborRadius = 13.0f;
    float separationRadius = 4.0f;
    float sepWeight = 0.020f;
    float alignWeight = 0.008f;
    float cohWeight = 0.005f;
    float maxSpeed = 0.45f;

    if (gBirdStyle == BIRD_STYLE_TIGHT)
    {
        neighborRadius = 16.0f;
        separationRadius = 3.5f;
        sepWeight = 0.020f;
        alignWeight = 0.022f;
        cohWeight = 0.016f;
        maxSpeed = 0.50f;
    }
    else if (gBirdStyle == BIRD_STYLE_LOOSE)
    {
        neighborRadius = 10.0f;
        separationRadius = 4.5f;
        sepWeight = 0.026f;
        alignWeight = 0.006f;
        cohWeight = 0.003f;
        maxSpeed = 0.42f;
    }
    else
    {
        neighborRadius = 14.0f;
        separationRadius = 4.2f;
        sepWeight = 0.024f;
        alignWeight = 0.012f;
        cohWeight = 0.006f;
        maxSpeed = 0.62f;
    }

    const float neighborRadius2 = neighborRadius * neighborRadius;
    const float separationRadius2 = separationRadius * separationRadius;
    const float dt = 1.0f;
    const float maxSpeed2 = maxSpeed * maxSpeed;

    float newVx[gBirdCount];
    float newVy[gBirdCount];
    float newVz[gBirdCount];

    for (int i = 0; i < gBirdCount; ++i)
    {
        float sepX = 0.0f, sepY = 0.0f, sepZ = 0.0f;
        float alignX = 0.0f, alignY = 0.0f, alignZ = 0.0f;
        float cohX = 0.0f, cohY = 0.0f, cohZ = 0.0f;
        int neighbors = 0;

        for (int j = 0; j < gBirdCount; ++j)
        {
            if (i == j) continue;

            float dx = gBirds[j].x - gBirds[i].x;
            float dy = gBirds[j].y - gBirds[i].y;
            float dz = gBirds[j].z - gBirds[i].z;
            float dist2 = dx * dx + dy * dy + dz * dz;

            if (dist2 < neighborRadius2)
            {
                alignX += gBirds[j].vx;
                alignY += gBirds[j].vy;
                alignZ += gBirds[j].vz;
                cohX += gBirds[j].x;
                cohY += gBirds[j].y;
                cohZ += gBirds[j].z;
                neighbors++;
            }

            if (dist2 < separationRadius2 && dist2 > 0.0001f)
            {
                float repel = 1.0f / (dist2 + 0.02f);
                sepX -= dx * repel;
                sepY -= dy * repel;
                sepZ -= dz * repel;
            }
        }

        float ax = sepX * sepWeight;
        float ay = sepY * sepWeight;
        float az = sepZ * sepWeight;

        if (neighbors > 0)
        {
            float inv = 1.0f / neighbors;
            alignX = (alignX * inv - gBirds[i].vx);
            alignY = (alignY * inv - gBirds[i].vy);
            alignZ = (alignZ * inv - gBirds[i].vz);

            cohX = (cohX * inv - gBirds[i].x);
            cohY = (cohY * inv - gBirds[i].y);
            cohZ = (cohZ * inv - gBirds[i].z);

            ax += alignX * alignWeight + cohX * cohWeight;
            ay += alignY * alignWeight + cohY * cohWeight;
            az += alignZ * alignWeight + cohZ * cohWeight;
        }

        // Keep flock in a flying band and bias motion left->right.
        ay += (17.0f - gBirds[i].y) * 0.002f;
        ax += 0.006f;

        newVx[i] = gBirds[i].vx + ax * dt;
        newVy[i] = gBirds[i].vy + ay * dt;
        newVz[i] = gBirds[i].vz + az * dt;

        float speed2 = newVx[i] * newVx[i] + newVy[i] * newVy[i] + newVz[i] * newVz[i];
        if (speed2 > maxSpeed2)
        {
            float scale = maxSpeed / sqrtf(speed2);
            newVx[i] *= scale;
            newVy[i] *= scale;
            newVz[i] *= scale;
        }
    }

    for (int i = 0; i < gBirdCount; ++i)
    {
        gBirds[i].vx = newVx[i];
        gBirds[i].vy = newVy[i];
        gBirds[i].vz = newVz[i];

        gBirds[i].x += gBirds[i].vx * dt;
        gBirds[i].y += gBirds[i].vy * dt;
        gBirds[i].z += gBirds[i].vz * dt;

        // Soft world limits except x, which respawns as a passing flock.
        if (gBirds[i].z > 45.0f) gBirds[i].z = 45.0f;
        if (gBirds[i].z < -45.0f) gBirds[i].z = -45.0f;
        if (gBirds[i].y > 30.0f) gBirds[i].y = 30.0f;
        if (gBirds[i].y < 8.0f) gBirds[i].y = 8.0f;
    }

    // When flock exits right side, start a new flock from the left.
    int passedCount = 0;
    for (int i = 0; i < gBirdCount; ++i)
    {
        if (gBirds[i].x > 56.0f)
            passedCount++;
    }
    if (passedCount == gBirdCount)
        spawnBirdFlockFromLeft();

    glutPostRedisplay();      // trigger a new display() call
}

// ------------------------------------------------------------------
// keyboard – press ESC or 'q' to quit
// ------------------------------------------------------------------
void keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    if (key == 27 || key == 'q' || key == 'Q')   // ESC or Q
        exit(0);

    if (key == '1') gBirdStyle = BIRD_STYLE_TIGHT;
    if (key == '2') gBirdStyle = BIRD_STYLE_LOOSE;
    if (key == '3') gBirdStyle = BIRD_STYLE_SEAGULL;

    // Camera zoom
    if (key == '+' || key == '=')
    {
        gCamDistance -= 2.0f;
        if (gCamDistance < 20.0f) gCamDistance = 20.0f;
    }
    if (key == '-' || key == '_')
    {
        gCamDistance += 2.0f;
        if (gCamDistance > 120.0f) gCamDistance = 120.0f;
    }

    // Camera pan (IJKL)
    if (key == 'j' || key == 'J') gCamPanX -= 1.5f;
    if (key == 'l' || key == 'L') gCamPanX += 1.5f;
    if (key == 'i' || key == 'I') gCamPanY += 1.0f;
    if (key == 'k' || key == 'K') gCamPanY -= 1.0f;

    // Debug section toggles
    if (key == 'g' || key == 'G') gShowGround = !gShowGround;
    if (key == 'w' || key == 'W') gShowWater = !gShowWater;
    if (key == 'h' || key == 'H') gShowLighthouse = !gShowLighthouse;
    if (key == 'o' || key == 'O') gShowBoats = !gShowBoats;
    if (key == 'b' || key == 'B') gShowBirds = !gShowBirds;
    if (key == 'p' || key == 'P') gShowStats = !gShowStats;

    // Cargo control
    if (key == 'm' || key == 'M') gCargoMoving = !gCargoMoving;

    glutPostRedisplay();
}

// ------------------------------------------------------------------
// main
// ------------------------------------------------------------------
int main(int argc, char** argv)
{
    srand(static_cast<unsigned int>(time(nullptr)));
    initBirds();

    // --- GLUT initialisation ---------------------------------------
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Port Environment");

    // --- OpenGL state ----------------------------------------------
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);    // sky-blue background
    glEnable(GL_DEPTH_TEST);

    // Lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // LIGHT0 – warm directional sun light
    GLfloat ambient[]  = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat diffuse[]  = { 1.0f, 0.95f, 0.8f, 1.0f };
    GLfloat specular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

    // LIGHT1 – Rotating Spotlight
    GLfloat ambient1[]  = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat diffuse1[]  = { 1.0f, 1.0f, 0.8f, 1.0f }; // Warm white
    GLfloat specular1[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // Very bright specular
    glLightfv(GL_LIGHT1, GL_AMBIENT,  ambient1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  diffuse1);
    glLightfv(GL_LIGHT1, GL_SPECULAR, specular1);
    glEnable(GL_LIGHT1);

    // Enable colour-tracking for materials (convenient for quick colouring)
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Smooth shading
    glShadeModel(GL_SMOOTH);

    tryLoadOptionalTextures();
    buildDisplayLists();
    gLastFpsTimeMs = glutGet(GLUT_ELAPSED_TIME);

    // --- Register callbacks ----------------------------------------
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutKeyboardFunc(keyboard);

    printf("Press ESC or Q to quit.\n");
    printf("Bird flock styles: 1=tight 2=loose 3=fast-seagull\n");
    printf("Camera: +/- zoom, I/J/K/L pan\n");
    printf("Toggles: G=ground W=water H=lighthouse O=boats B=birds P=fps\n");
    printf("Cargo: M=toggle cargo ship movement\n");
    printf("Optional texture: put sea.bmp in project folder\n");
    // --- Enter GLUT event loop (never returns) ---------------------
    glutMainLoop();
    return 0;
}