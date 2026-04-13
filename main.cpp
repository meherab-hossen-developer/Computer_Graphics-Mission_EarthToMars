#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <vector>

// ===================== Shuttle Simulation Data ======================
enum State
{
    SPLASH_SCREEN,
    READY,
    LAUNCHING,
    CRUISING_TO_MARS,
    ARRIVED_MARS,
    MISSION_FAILED,
    LANDED
};
State currentState = SPLASH_SCREEN;

float shuttleY = 0.0f;
float shuttleVelocity = 0.0f;
bool boosters_separated = false;
bool tank_separated = false;
int liftoffFrames = 0;
int boosterSeparatedFrame = -1;
bool rocketExploded = false;
float explosionRadius = 0.0f;
bool landingInProgress = false;
int landingTimer = 0;
float landingY = 67.0f;
bool landingDustActive = false;
float landingDustRadius = 0.0f;
float landingDustAlpha = 0.0f;
bool flagPlanting = false;
bool flagPlanted = false;
int flagPlantTimer = 0;
float flagPlantProgress = 0.0f;

float transferT = 0.0f;        // 0.0 at Earth -> 1.0 at Mars
float transferSpeed = 0.0012f; // cruise speed
float earthRot = 0.0f;
float marsRot = 0.0f;
GLuint marsTex = 0;
bool marsTextureOK = false;
const char *marsTexturePath = "mars_texture.bmp";

float cameraY = 80.0f;
float cameraZ = 200.0f;

float exhaustFlame = 0.0f;
float smokeTime = 0.0f;
int missionTimer = 0;

float astronautX = -55.0f;
float astronautY = -8.0f;
float astronautZ = -6.0f;
bool astronautBoarded = false;
bool astronautInRocket = false;
bool astronautOnMars = false;
float astronautWalkPhase = 0.0f;

const int WALK_DURATION_FRAMES = 312; // ~5 seconds at 16ms timer
const int BOARD_DURATION_FRAMES = 75;
const int MARS_CLIMB_DOWN_FRAMES = 40;
const int MARS_WALK_FRAMES = 60;
const float BOOSTER_SEP_MIN_ALTITUDE = 5000.0f;
const float BOOSTER_SEP_MAX_ALTITUDE = 11000.0f;
const float TANK_SEP_MIN_ALTITUDE = 15000.0f;
const float TANK_SEP_MAX_ALTITUDE = 21000.0f;
const float MAX_LAUNCH_ALTITUDE_METERS = 25000.0f;
const float MAX_EXPLOSION_RADIUS = 120.0f;
const int LANDING_DURATION_FRAMES = 180;
const float LANDING_START_Y = 140.0f;
const float LANDING_TARGET_Y = 67.0f;
const int FLAG_PLANT_DURATION_FRAMES = 60;
const float FLAG_X = 455.0f;
const float FLAG_Z = 40.0f;
const float FLAG_PLANT_Y = 43.0f;

struct SmokeParticle
{
    float x, y, z;
    float velY, velX, velZ;
    float size;
    float alpha;
    bool active;
};
SmokeParticle smoke[200];

struct Button
{
    float x, y, width, height;
    const char *text;
    float r, g, b;
    bool isHovered;
};
Button launchButton = {300, 50, 250, 70, "LAUNCH SHUTTLE", 0.0f, 0.7f, 0.0f, false};
Button returnButton = {300, 50, 250, 70, "RESET MISSION", 0.8f, 0.3f, 0.0f, false};
Button boosterButton = {40, 50, 300, 70, "SEPARATE BOOSTERS", 0.2f, 0.4f, 0.9f, false};
Button tankButton = {460, 50, 300, 70, "SEPARATE TANK", 0.9f, 0.5f, 0.1f, false};

// ============================= Utility and Drawing ============================
void initSmoke()
{
    for (int i = 0; i < 200; i++)
        smoke[i].active = false;
}

void spawnSmoke(float x, float y, float z)
{
    for (int i = 0; i < 200; i++)
        if (!smoke[i].active)
        {
            smoke[i].x = x + (rand() % 20 - 10) * 0.5f;
            smoke[i].y = y;
            smoke[i].z = z + (rand() % 20 - 10) * 0.5f;
            smoke[i].velY = -0.2f + (rand() % 10) * 0.05f;
            smoke[i].velX = (rand() % 20 - 10) * 0.1f;
            smoke[i].velZ = (rand() % 20 - 10) * 0.1f;
            smoke[i].size = 8.0f + (rand() % 10) * 0.5f;
            smoke[i].alpha = 0.8f;
            smoke[i].active = true;
            break;
        }
}

void updateSmoke()
{
    for (int i = 0; i < 200; i++)
        if (smoke[i].active)
        {
            smoke[i].x += smoke[i].velX;
            smoke[i].y += smoke[i].velY;
            smoke[i].z += smoke[i].velZ;
            smoke[i].size += 0.15f;
            smoke[i].alpha -= 0.008f;
            smoke[i].velY += 0.01f;
            if (smoke[i].alpha <= 0.0f || smoke[i].size > 35.0f)
                smoke[i].active = false;
        }
}

void drawSmoke()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    for (int i = 0; i < 200; i++)
        if (smoke[i].active)
        {
            glPushMatrix();
            glTranslatef(smoke[i].x, smoke[i].y, smoke[i].z);
            glColor4f(0.7f, 0.7f, 0.7f, smoke[i].alpha);
            glutSolidSphere(smoke[i].size, 12, 12);
            glPopMatrix();
        }
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
}

// ===================== Bresenham Line Algorithm ===================
void drawBresenhamLine(int x1, int y1, int x2, int y2)
{
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    glBegin(GL_POINTS);
    for (;;)
    {
        glVertex2i(x1, y1);
        if (x1 == x2 && y1 == y2)
            break;
        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
    glEnd();
}

// ===================== Midpoint Circle Algorithm ===================
void plotCirclePoints(int xc, int yc, int x, int y)
{
    glVertex2i(xc + x, yc + y);
    glVertex2i(xc - x, yc + y);
    glVertex2i(xc + x, yc - y);
    glVertex2i(xc - x, yc - y);
    glVertex2i(xc + y, yc + x);
    glVertex2i(xc - y, yc + x);
    glVertex2i(xc + y, yc - x);
    glVertex2i(xc - y, yc - x);
}

void drawMidpointCircle(int xc, int yc, int radius)
{
    int x = 0;
    int y = radius;
    int p = 1 - radius;

    glBegin(GL_POINTS);
    plotCirclePoints(xc, yc, x, y);

    while (x < y)
    {
        x++;
        if (p < 0)
            p += 2 * x + 1;
        else
        {
            y--;
            p += 2 * x - 2 * y + 1;
        }
        plotCirclePoints(xc, yc, x, y);
    }
    glEnd();
}

void drawMidpointCircleFilled(int xc, int yc, int radius)
{
    for (int r = 0; r <= radius; r++)
    {
        int x = 0;
        int y = r;
        int p = 1 - r;

        if (r == 0)
        {
            glBegin(GL_POINTS);
            glVertex2i(xc, yc);
            glEnd();
            continue;
        }

        glBegin(GL_POINTS);
        plotCirclePoints(xc, yc, x, y);

        while (x < y)
        {
            x++;
            if (p < 0)
                p += 2 * x + 1;
            else
            {
                y--;
                p += 2 * x - 2 * y + 1;
            }
            plotCirclePoints(xc, yc, x, y);
        }
        glEnd();
    }
}

void drawSunMidpoint(int centerX, int centerY, float visibility)
{
    if (visibility <= 0.0f)
        return;

    glDisable(GL_LIGHTING);

    glColor3f(1.0f * visibility, 0.8f * visibility, 0.2f * visibility);
    glPointSize(2.0f);
    for (int i = 0; i < 12; i++)
    {
        float angle = i * 30.0f * 3.14159f / 180.0f;
        int rayEndX = centerX + (int)(cos(angle) * 70);
        int rayEndY = centerY + (int)(sin(angle) * 70);
        int rayStartX = centerX + (int)(cos(angle) * 45);
        int rayStartY = centerY + (int)(sin(angle) * 45);
        drawBresenhamLine(rayStartX, rayStartY, rayEndX, rayEndY);
    }

    glColor3f(1.0f * visibility, 0.6f * visibility, 0.1f * visibility);
    glPointSize(3.0f);
    drawMidpointCircle(centerX, centerY, 42);
    drawMidpointCircle(centerX, centerY, 40);
    drawMidpointCircle(centerX, centerY, 38);

    glColor3f(1.0f * visibility, 0.75f * visibility, 0.2f * visibility);
    glPointSize(2.0f);
    drawMidpointCircleFilled(centerX, centerY, 35);

    glColor3f(1.0f * visibility, 0.95f * visibility, 0.6f * visibility);
    glPointSize(2.0f);
    drawMidpointCircleFilled(centerX, centerY, 25);

    glColor3f(1.0f * visibility, 1.0f * visibility, 0.9f * visibility);
    glPointSize(3.0f);
    drawMidpointCircleFilled(centerX, centerY, 15);

    glEnable(GL_LIGHTING);
}

void drawRocketTrail()
{
    if (currentState != LAUNCHING)
        return;
    if (shuttleY < 10.0f)
        return;
    if (rocketExploded)
        return;

    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    int rocketX = 400;
    int groundY = 50;
    int trailTopY = 180;

    if (shuttleY > 1200.0f)
    {
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_LIGHTING);
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPointSize(2.0f);
    glColor4f(0.75f, 0.75f, 0.78f, 0.25f);
    drawBresenhamLine(rocketX - 15, groundY, rocketX - 10, trailTopY);
    drawBresenhamLine(rocketX + 15, groundY, rocketX + 10, trailTopY);

    glPointSize(3.0f);
    glColor4f(0.82f, 0.82f, 0.85f, 0.4f);
    drawBresenhamLine(rocketX - 9, groundY, rocketX - 6, trailTopY);
    drawBresenhamLine(rocketX + 9, groundY, rocketX + 6, trailTopY);

    glPointSize(4.0f);
    glColor4f(0.9f, 0.9f, 0.93f, 0.55f);
    drawBresenhamLine(rocketX - 5, groundY, rocketX - 3, trailTopY);
    drawBresenhamLine(rocketX + 5, groundY, rocketX + 3, trailTopY);

    glPointSize(5.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
    drawBresenhamLine(rocketX - 1, groundY, rocketX - 1, trailTopY);
    drawBresenhamLine(rocketX + 1, groundY, rocketX + 1, trailTopY);
    drawBresenhamLine(rocketX, groundY, rocketX, trailTopY);

    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
}

void drawEarth()
{
    if (currentState != CRUISING_TO_MARS)
        return;

    glPushMatrix();
    glRotatef(25.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(earthRot, 0.0f, 1.0f, 0.0f);

    const int stacks = 80;
    const int slices = 128;
    const float radius = 80.0f;
    const float pi = 3.14159265f;

    for (int i = 0; i < stacks; ++i)
    {
        float lat0 = pi * (-0.5f + (float)i / stacks);
        float z0 = sinf(lat0);
        float zr0 = cosf(lat0);

        float lat1 = pi * (-0.5f + (float)(i + 1) / stacks);
        float z1 = sinf(lat1);
        float zr1 = cosf(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j)
        {
            float lng = 2.0f * pi * (float)j / slices;
            float x = cosf(lng);
            float y = sinf(lng);

            float nx0 = x * zr0;
            float ny0 = y * zr0;
            float nz0 = z0;
            float h0 = 0.5f + 0.5f * sinf(6.0f * nx0) * cosf(5.0f * ny0) + 0.25f * sinf(10.0f * nz0);
            if (h0 > 0.53f)
                glColor3f(0.22f, 0.52f, 0.22f);
            else
                glColor3f(0.05f, 0.22f, 0.60f);
            glNormal3f(nx0, ny0, nz0);
            glVertex3f(radius * nx0, radius * ny0, radius * nz0);

            float nx1 = x * zr1;
            float ny1 = y * zr1;
            float nz1 = z1;
            float h1 = 0.5f + 0.5f * sinf(6.0f * nx1) * cosf(5.0f * ny1) + 0.25f * sinf(10.0f * nz1);
            if (h1 > 0.53f)
                glColor3f(0.22f, 0.52f, 0.22f);
            else
                glColor3f(0.05f, 0.22f, 0.60f);
            glNormal3f(nx1, ny1, nz1);
            glVertex3f(radius * nx1, radius * ny1, radius * nz1);
        }
        glEnd();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.9f, 0.95f, 1.0f, 0.13f);
    glutSolidSphere(81.5f, 64, 64);
    glDisable(GL_BLEND);

    glPopMatrix();
}

GLuint loadMarsBMP(const char *filename, bool &ok)
{
    ok = false;
    FILE *f = fopen(filename, "rb");
    if (!f)
        return 0;

    unsigned char header[54];
    if (fread(header, 1, 54, f) != 54)
    {
        fclose(f);
        return 0;
    }

    if (header[0] != 'B' || header[1] != 'M')
    {
        fclose(f);
        return 0;
    }

    int dataPos = *(int *)&header[0x0A];
    int width = *(int *)&header[0x12];
    int height = *(int *)&header[0x16];
    short bpp = *(short *)&header[0x1C];
    int comp = *(int *)&header[0x1E];
    int imageSize = *(int *)&header[0x22];

    if (bpp != 24 || comp != 0)
    {
        fclose(f);
        return 0;
    }

    if (imageSize == 0)
        imageSize = width * height * 3;
    if (dataPos == 0)
        dataPos = 54;

    unsigned char *data = new unsigned char[imageSize];
    fseek(f, dataPos, SEEK_SET);
    fread(data, 1, imageSize, f);
    fclose(f);

    for (int i = 0; i < imageSize; i += 3)
    {
        unsigned char t = data[i];
        data[i] = data[i + 2];
        data[i + 2] = t;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    delete[] data;
    ok = true;
    return tex;
}

void setProceduralMarsColor(float nx, float ny, float nz)
{
    float lon = atan2f(nz, nx);
    float lat = asinf(ny);

    // Broad surface variation and dark bands.
    float band = 0.10f * sinf(2.8f * lon + 0.9f) + 0.08f * sinf(7.0f * lon - 1.6f * lat);
    float mottled = 0.5f + 0.5f * sinf(11.0f * nx + 8.0f * nz) * cosf(9.0f * ny);

    // Crater-like pitting.
    float crater = fabsf(sinf(26.0f * nx) * sinf(24.0f * nz) * cosf(18.0f * ny)) * 0.14f;

    // A long dark canyon-like stripe inspired by Valles Marineris.
    float canyon = expf(-powf((lat + 0.17f) * 7.0f, 2.0f)) * expf(-powf((lon + 1.08f) * 1.9f, 2.0f));

    float t = 0.58f + band + (mottled - 0.5f) * 0.22f - crater - canyon * 0.32f;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    float r = 0.58f + 0.28f * t;
    float g = 0.23f + 0.16f * t;
    float b = 0.12f + 0.10f * t;

    // Subtle polar brightening.
    float cap = (fabsf(ny) - 0.90f) * 8.0f;
    if (cap < 0.0f)
        cap = 0.0f;
    if (cap > 0.20f)
        cap = 0.20f;
    r += cap;
    g += cap * 0.8f;
    b += cap * 0.7f;

    glColor3f(r, g, b);
}

void drawMarsSphere(float r, int slices, int stacks)
{
    if (marsTextureOK)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, marsTex);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.78f, 0.35f, 0.20f);
    }

    for (int i = 0; i < stacks; i++)
    {
        float v0 = (float)i / stacks;
        float v1 = (float)(i + 1) / stacks;
        float lat0 = (float)M_PI * (v0 - 0.5f);
        float lat1 = (float)M_PI * (v1 - 0.5f);
        float y0 = sinf(lat0), rr0 = cosf(lat0);
        float y1 = sinf(lat1), rr1 = cosf(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++)
        {
            float u = (float)j / slices;
            float lon = 2.0f * (float)M_PI * (u - 0.5f);
            float cx = cosf(lon), sz = sinf(lon);

            if (!marsTextureOK)
                setProceduralMarsColor(cx * rr0, y0, sz * rr0);
            glNormal3f(cx * rr0, y0, sz * rr0);
            glTexCoord2f(u, 1.0f - v0);
            glVertex3f(r * cx * rr0, r * y0, r * sz * rr0);

            if (!marsTextureOK)
                setProceduralMarsColor(cx * rr1, y1, sz * rr1);
            glNormal3f(cx * rr1, y1, sz * rr1);
            glTexCoord2f(u, 1.0f - v1);
            glVertex3f(r * cx * rr1, r * y1, r * sz * rr1);
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
}

void drawMars()
{
    if (currentState != CRUISING_TO_MARS)
        return;

    glPushMatrix();
    glTranslatef(420.0f, 0.0f, 0.0f);

    glRotatef(25.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(marsRot, 0.0f, 1.0f, 0.0f);
    drawMarsSphere(45.0f, 180, 120);

    glPopMatrix();
}

void drawStars()
{
    if (currentState != CRUISING_TO_MARS)
        return;

    glDisable(GL_LIGHTING);
    srand(12345);

    glPointSize(1.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 500; i++)
    {
        float brightness = 0.3f + (rand() % 100) / 200.0f;
        glColor3f(brightness, brightness, brightness * 1.1f);
        float x = (float)(rand() % 3000 - 1500);
        float y = (float)(rand() % 3000 - 1500);
        float z = (float)(rand() % 3000 - 1500);
        glVertex3f(x, y, z);
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void drawSkyBackground()
{
    if (currentState == CRUISING_TO_MARS)
        return;

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float altitude = (shuttleY * 10.0f) / MAX_LAUNCH_ALTITUDE_METERS;
    if (altitude > 1.0f)
        altitude = 1.0f;
    if (altitude < 0.0f)
        altitude = 0.0f;

    float topR, topG, topB;
    float botR, botG, botB;

    if (currentState == ARRIVED_MARS)
    {
        topR = 0.62f;
        topG = 0.44f;
        topB = 0.38f;
        botR = 0.80f;
        botG = 0.63f;
        botB = 0.52f;
    }
    else if (currentState == READY || currentState == LANDED)
    {
        topR = 0.3f;
        topG = 0.5f;
        topB = 0.9f;
        botR = 0.6f;
        botG = 0.8f;
        botB = 1.0f;
    }
    else
    {
        topR = 0.02f + (1.0f - altitude) * 0.28f;
        topG = 0.02f + (1.0f - altitude) * 0.48f;
        topB = 0.08f + (1.0f - altitude) * 0.82f;
        botR = 0.4f * (1.0f - altitude * 0.7f);
        botG = 0.6f * (1.0f - altitude * 0.5f);
        botB = 0.9f * (1.0f - altitude * 0.3f);
    }

    glBegin(GL_QUADS);
    glColor3f(botR, botG, botB);
    glVertex2f(0, 0);
    glVertex2f(800, 0);
    glColor3f(topR, topG, topB);
    glVertex2f(800, 600);
    glVertex2f(0, 600);
    glEnd();

    if ((currentState == READY || currentState == LANDED || currentState == LAUNCHING) && altitude < 0.8f)
    {
        float sunVisibility = 1.0f - altitude;
        drawSunMidpoint(680, 500, sunVisibility);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glClear(GL_DEPTH_BUFFER_BIT);
}

struct EarthRock
{
    float x, z, y;
    float sx, sy, sz;
    float rot;
    float tone;
};

struct EarthTwig
{
    float x, z, y;
    float len, ang;
};

std::vector<EarthRock> earthRocksNear, earthRocksMid, earthRocksFar;
std::vector<EarthTwig> earthTwigs;

float earthHash2(float x, float y)
{
    float n = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return n - floorf(n);
}

float earthValueNoise(float x, float y)
{
    float xi = floorf(x), yi = floorf(y);
    float xf = x - xi, yf = y - yi;

    float a = earthHash2(xi, yi);
    float b = earthHash2(xi + 1.0f, yi);
    float c = earthHash2(xi, yi + 1.0f);
    float d = earthHash2(xi + 1.0f, yi + 1.0f);

    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = yf * yf * (3.0f - 2.0f * yf);

    float ab = a * (1.0f - u) + b * u;
    float cd = c * (1.0f - u) + d * u;
    return ab * (1.0f - v) + cd * v;
}

float earthFbm(float x, float y)
{
    float v = 0.0f, a = 0.5f, f = 1.0f;
    for (int i = 0; i < 6; i++)
    {
        v += a * earthValueNoise(x * f, y * f);
        f *= 2.0f;
        a *= 0.5f;
    }
    return v;
}

float earthGroundHeight(float x, float z)
{
    float h = 0.35f * sinf(x * 0.06f) * cosf(z * 0.05f);
    h += 0.18f * sinf((x + z) * 0.11f);
    h += 0.10f * cosf((x - z) * 0.09f);
    return h;
}

float earthCrackMask(float x, float z)
{
    float cx = x * 0.40f, cz = z * 0.40f;
    float gx = fabsf((cx - floorf(cx)) - 0.5f);
    float gz = fabsf((cz - floorf(cz)) - 0.5f);
    float c1 = fminf(gx, gz);

    float cx2 = x * 0.22f + 5.0f, cz2 = z * 0.23f - 3.0f;
    float gx2 = fabsf((cx2 - floorf(cx2)) - 0.5f);
    float gz2 = fabsf((cz2 - floorf(cz2)) - 0.5f);
    float c2 = fminf(gx2, gz2);

    float jitter = earthFbm(x * 0.15f, z * 0.15f) * 0.09f;
    float c = 1.0f - (0.7f * c1 + 0.6f * c2 + jitter) * 6.5f;
    if (c < 0.0f)
        c = 0.0f;
    if (c > 1.0f)
        c = 1.0f;
    return c;
}

void drawEarthGroundSkyGradient()
{
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glColor3f(0.22f, 0.46f, 0.78f);
    glVertex3f(-400.0f, 140.0f, -320.0f);
    glColor3f(0.22f, 0.46f, 0.78f);
    glVertex3f(400.0f, 140.0f, -320.0f);
    glColor3f(0.80f, 0.88f, 0.97f);
    glVertex3f(400.0f, 14.0f, -320.0f);
    glColor3f(0.80f, 0.88f, 0.97f);
    glVertex3f(-400.0f, 14.0f, -320.0f);
    glEnd();
    glEnable(GL_LIGHTING);
}

void drawEarthGroundCloudBands()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < 4; i++)
    {
        float y = 55.0f + i * 8.0f;
        float a = 0.10f - i * 0.015f;
        glColor4f(1.0f, 1.0f, 1.0f, a);
        glBegin(GL_QUADS);
        glVertex3f(-260.0f, y, -260.0f - i * 12.0f);
        glVertex3f(260.0f, y, -260.0f - i * 12.0f);
        glVertex3f(260.0f, y + 6.0f, -280.0f - i * 12.0f);
        glVertex3f(-260.0f, y + 6.0f, -280.0f - i * 12.0f);
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void drawEarthGroundMountainsDetailed()
{
    glDisable(GL_LIGHTING);

    glColor3f(0.55f, 0.62f, 0.70f);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 140; i++)
    {
        float x = -190.0f + i * 2.7f;
        float h = 14.0f + 7.0f * sinf(i * 0.19f);
        glVertex3f(x, 0.0f, -205.0f);
        glVertex3f(x, h, -215.0f);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 100; i++)
    {
        float z = -210.0f + i * 4.2f;
        float h = 22.0f + 12.0f * sinf(i * 0.24f) + 9.0f * sinf(i * 0.51f);
        glColor3f(0.18f, 0.22f, 0.26f);
        glVertex3f(-150.0f, 0.0f, z);
        glColor3f(0.34f, 0.38f, 0.42f);
        glVertex3f(-78.0f, h, z);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 100; i++)
    {
        float z = -210.0f + i * 4.2f;
        float h = 24.0f + 13.0f * sinf(i * 0.21f) + 8.0f * sinf(i * 0.47f);
        glColor3f(0.18f, 0.22f, 0.26f);
        glVertex3f(150.0f, 0.0f, z);
        glColor3f(0.36f, 0.40f, 0.44f);
        glVertex3f(78.0f, h, z);
    }
    glEnd();

    glBegin(GL_TRIANGLES);
    glColor3f(0.34f, 0.37f, 0.41f);
    glVertex3f(-48.0f, 0.0f, -175.0f);
    glColor3f(0.82f, 0.85f, 0.88f);
    glVertex3f(-28.0f, 44.0f, -192.0f);
    glColor3f(0.34f, 0.37f, 0.41f);
    glVertex3f(-8.0f, 0.0f, -175.0f);

    glColor3f(0.34f, 0.37f, 0.41f);
    glVertex3f(-14.0f, 0.0f, -182.0f);
    glColor3f(0.88f, 0.90f, 0.92f);
    glVertex3f(12.0f, 51.0f, -198.0f);
    glColor3f(0.34f, 0.37f, 0.41f);
    glVertex3f(35.0f, 0.0f, -182.0f);

    glColor3f(0.34f, 0.37f, 0.41f);
    glVertex3f(24.0f, 0.0f, -176.0f);
    glColor3f(0.80f, 0.84f, 0.88f);
    glVertex3f(48.0f, 40.0f, -191.0f);
    glColor3f(0.34f, 0.37f, 0.41f);
    glVertex3f(72.0f, 0.0f, -176.0f);
    glEnd();

    glEnable(GL_LIGHTING);
}

void drawEarthGroundDetailed(int size, int step)
{
    for (int z = -size; z < size; z += step)
    {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = -size; x <= size; x += step)
        {
            float h1 = earthGroundHeight((float)x, (float)z);
            float h2 = earthGroundHeight((float)x, (float)(z + step));

            float c1 = earthCrackMask((float)x, (float)z);
            float c2 = earthCrackMask((float)x, (float)(z + step));

            float n1 = earthFbm(x * 0.06f, z * 0.06f);
            float n2 = earthFbm(x * 0.06f, (z + step) * 0.06f);

            float r1 = 0.73f, g1 = 0.56f, b1 = 0.37f;
            float r2 = 0.73f, g2 = 0.56f, b2 = 0.37f;

            r1 += (n1 - 0.5f) * 0.18f;
            g1 += (n1 - 0.5f) * 0.12f;
            b1 += (n1 - 0.5f) * 0.08f;
            r2 += (n2 - 0.5f) * 0.18f;
            g2 += (n2 - 0.5f) * 0.12f;
            b2 += (n2 - 0.5f) * 0.08f;

            r1 -= c1 * 0.32f;
            g1 -= c1 * 0.28f;
            b1 -= c1 * 0.24f;
            r2 -= c2 * 0.32f;
            g2 -= c2 * 0.28f;
            b2 -= c2 * 0.24f;

            if (r1 < 0.0f)
                r1 = 0.0f;
            if (g1 < 0.0f)
                g1 = 0.0f;
            if (b1 < 0.0f)
                b1 = 0.0f;
            if (r2 < 0.0f)
                r2 = 0.0f;
            if (g2 < 0.0f)
                g2 = 0.0f;
            if (b2 < 0.0f)
                b2 = 0.0f;

            glColor3f(r1, g1, b1);
            glVertex3f((float)x, h1, (float)z);

            glColor3f(r2, g2, b2);
            glVertex3f((float)x, h2, (float)(z + step));
        }
        glEnd();
    }
}

void drawEarthGroundRocks(const std::vector<EarthRock> &group)
{
    for (size_t i = 0; i < group.size(); i++)
    {
        const EarthRock &r = group[i];
        glPushMatrix();
        glTranslatef(r.x, r.y, r.z);
        glRotatef(r.rot, 0.0f, 1.0f, 0.0f);
        glScalef(r.sx, r.sy, r.sz);

        float t = r.tone;
        glColor3f(0.56f + 0.18f * t, 0.53f + 0.16f * t, 0.48f + 0.14f * t);
        glutSolidSphere(1.0f, 12, 10);
        glPopMatrix();
    }
}

void drawEarthGroundTwigs()
{
    glDisable(GL_LIGHTING);
    glColor3f(0.28f, 0.20f, 0.14f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (size_t i = 0; i < earthTwigs.size(); i++)
    {
        float x = earthTwigs[i].x;
        float z = earthTwigs[i].z;
        float y = earthTwigs[i].y;
        float a = earthTwigs[i].ang;
        float l = earthTwigs[i].len;
        float x2 = x + cosf(a) * l;
        float z2 = z + sinf(a) * l;
        float y2 = earthGroundHeight(x2, z2) + 0.02f;
        glVertex3f(x, y, z);
        glVertex3f(x2, y2, z2);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

void initEarthGroundScatter()
{
    earthRocksNear.clear();
    earthRocksMid.clear();
    earthRocksFar.clear();
    earthTwigs.clear();

    for (int i = 0; i < 220; i++)
    {
        EarthRock r;
        r.x = -95.0f + (rand() % 190);
        r.z = -25.0f + (rand() % 70);
        r.y = earthGroundHeight(r.x, r.z) + 0.10f;
        r.sx = 0.20f + (rand() % 100) / 90.0f;
        r.sy = 0.10f + (rand() % 100) / 200.0f;
        r.sz = 0.18f + (rand() % 100) / 110.0f;
        r.rot = (float)(rand() % 360);
        r.tone = (rand() % 100) / 100.0f;
        earthRocksNear.push_back(r);
    }

    for (int i = 0; i < 320; i++)
    {
        EarthRock r;
        r.x = -110.0f + (rand() % 220);
        r.z = -95.0f + (rand() % 80);
        r.y = earthGroundHeight(r.x, r.z) + 0.08f;
        r.sx = 0.14f + (rand() % 100) / 140.0f;
        r.sy = 0.08f + (rand() % 100) / 260.0f;
        r.sz = 0.12f + (rand() % 100) / 150.0f;
        r.rot = (float)(rand() % 360);
        r.tone = (rand() % 100) / 100.0f;
        earthRocksMid.push_back(r);
    }

    for (int i = 0; i < 360; i++)
    {
        EarthRock r;
        r.x = -120.0f + (rand() % 240);
        r.z = -170.0f + (rand() % 90);
        r.y = earthGroundHeight(r.x, r.z) + 0.05f;
        r.sx = 0.08f + (rand() % 100) / 260.0f;
        r.sy = 0.05f + (rand() % 100) / 400.0f;
        r.sz = 0.07f + (rand() % 100) / 280.0f;
        r.rot = (float)(rand() % 360);
        r.tone = (rand() % 100) / 100.0f;
        earthRocksFar.push_back(r);
    }

    for (int i = 0; i < 180; i++)
    {
        EarthTwig t;
        t.x = -90.0f + (rand() % 180);
        t.z = -40.0f + (rand() % 85);
        t.y = earthGroundHeight(t.x, t.z) + 0.03f;
        t.len = 0.5f + (rand() % 100) / 60.0f;
        t.ang = (float)(rand() % 628) / 100.0f;
        earthTwigs.push_back(t);
    }
}

struct MarsRock
{
    float x, z, y;
    float sx, sy, sz;
    float rot;
    float tone;
};

std::vector<MarsRock> marsRocks;

float marsHash2(float x, float y)
{
    float n = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return n - floorf(n);
}

float marsNoise2(float x, float y)
{
    float xi = floorf(x), yi = floorf(y);
    float xf = x - xi, yf = y - yi;
    float a = marsHash2(xi, yi);
    float b = marsHash2(xi + 1.0f, yi);
    float c = marsHash2(xi, yi + 1.0f);
    float d = marsHash2(xi + 1.0f, yi + 1.0f);
    float u = xf * xf * (3.0f - 2.0f * xf);
    float v = yf * yf * (3.0f - 2.0f * yf);
    float ab = a * (1.0f - u) + b * u;
    float cd = c * (1.0f - u) + d * u;
    return ab * (1.0f - v) + cd * v;
}

float marsFbm(float x, float y)
{
    float v = 0.0f, a = 0.5f, f = 1.0f;
    for (int i = 0; i < 6; i++)
    {
        v += a * marsNoise2(x * f, y * f);
        f *= 2.0f;
        a *= 0.5f;
    }
    return v;
}

float marsTerrainHeight(float x, float z)
{
    float h = 0.35f * sinf(x * 0.06f) * cosf(z * 0.05f);
    h += 0.18f * sinf((x + z) * 0.08f);
    h += 0.12f * cosf((x - z) * 0.07f);
    h += (marsFbm(x * 0.03f, z * 0.03f) - 0.5f) * 0.9f;
    return h;
}

void drawMarsGroundSky()
{
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glColor3f(0.62f, 0.44f, 0.38f);
    glVertex3f(-420.0f, 150.0f, -320.0f);
    glColor3f(0.62f, 0.44f, 0.38f);
    glVertex3f(420.0f, 150.0f, -320.0f);
    glColor3f(0.80f, 0.63f, 0.52f);
    glVertex3f(420.0f, 12.0f, -320.0f);
    glColor3f(0.80f, 0.63f, 0.52f);
    glVertex3f(-420.0f, 12.0f, -320.0f);
    glEnd();
    glEnable(GL_LIGHTING);
}

void drawMarsGroundMountains()
{
    glDisable(GL_LIGHTING);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 180; i++)
    {
        float x = -220.0f + i * 2.5f;
        float h = 12.0f + 7.0f * sinf(i * 0.14f) + 6.0f * sinf(i * 0.39f);
        glColor3f(0.29f, 0.20f, 0.18f);
        glVertex3f(x, 0.0f, -220.0f);
        glColor3f(0.38f, 0.27f, 0.24f);
        glVertex3f(x, h, -235.0f);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 90; i++)
    {
        float z = -220.0f + i * 5.0f;
        float h = 16.0f + 10.0f * sinf(i * 0.21f) + 5.0f * sinf(i * 0.53f);
        glColor3f(0.20f, 0.14f, 0.13f);
        glVertex3f(-170.0f, 0.0f, z);
        glColor3f(0.33f, 0.22f, 0.20f);
        glVertex3f(-95.0f, h, z);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 90; i++)
    {
        float z = -220.0f + i * 5.0f;
        float h = 18.0f + 12.0f * sinf(i * 0.19f) + 7.0f * sinf(i * 0.47f);
        glColor3f(0.20f, 0.14f, 0.13f);
        glVertex3f(170.0f, 0.0f, z);
        glColor3f(0.30f, 0.20f, 0.18f);
        glVertex3f(95.0f, h, z);
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void drawMarsGroundDetailed(int size, int step)
{
    for (int z = -size; z < size; z += step)
    {
        glBegin(GL_TRIANGLE_STRIP);
        for (int x = -size; x <= size; x += step)
        {
            float h1 = marsTerrainHeight((float)x, (float)z);
            float h2 = marsTerrainHeight((float)x, (float)(z + step));

            float n1 = marsFbm(x * 0.05f, z * 0.05f);
            float n2 = marsFbm(x * 0.05f, (z + step) * 0.05f);

            float r1 = 0.58f + (n1 - 0.5f) * 0.22f;
            float g1 = 0.29f + (n1 - 0.5f) * 0.12f;
            float b1 = 0.18f + (n1 - 0.5f) * 0.08f;

            float r2 = 0.58f + (n2 - 0.5f) * 0.22f;
            float g2 = 0.29f + (n2 - 0.5f) * 0.12f;
            float b2 = 0.18f + (n2 - 0.5f) * 0.08f;

            float e1 = fabsf(sinf(x * 0.12f + z * 0.09f)) * 0.08f;
            float e2 = fabsf(sinf(x * 0.12f + (z + step) * 0.09f)) * 0.08f;
            r1 -= e1;
            g1 -= e1 * 0.7f;
            b1 -= e1 * 0.5f;
            r2 -= e2;
            g2 -= e2 * 0.7f;
            b2 -= e2 * 0.5f;

            if (r1 < 0.0f)
                r1 = 0.0f;
            if (g1 < 0.0f)
                g1 = 0.0f;
            if (b1 < 0.0f)
                b1 = 0.0f;
            if (r2 < 0.0f)
                r2 = 0.0f;
            if (g2 < 0.0f)
                g2 = 0.0f;
            if (b2 < 0.0f)
                b2 = 0.0f;

            glColor3f(r1, g1, b1);
            glVertex3f((float)x, h1, (float)z);

            glColor3f(r2, g2, b2);
            glVertex3f((float)x, h2, (float)(z + step));
        }
        glEnd();
    }
}

void drawMarsGroundRocks()
{
    for (size_t i = 0; i < marsRocks.size(); i++)
    {
        const MarsRock &r = marsRocks[i];
        glPushMatrix();
        glTranslatef(r.x, r.y, r.z);
        glRotatef(r.rot, 0.0f, 1.0f, 0.0f);
        glScalef(r.sx, r.sy, r.sz);

        float t = r.tone;
        glColor3f(0.44f + t * 0.18f, 0.25f + t * 0.10f, 0.18f + t * 0.08f);
        glutSolidSphere(1.0f, 12, 10);
        glPopMatrix();
    }
}

void initMarsGroundRocks()
{
    marsRocks.clear();
    for (int i = 0; i < 550; i++)
    {
        MarsRock r;
        r.x = -120.0f + (rand() % 240);
        r.z = -180.0f + (rand() % 220);
        r.y = marsTerrainHeight(r.x, r.z) + 0.07f;

        float zone = (r.z + 180.0f) / 220.0f;
        float sBase = 0.10f + (1.0f - zone) * 0.55f;

        r.sx = sBase * (0.6f + (rand() % 100) / 120.0f);
        r.sy = sBase * (0.3f + (rand() % 100) / 260.0f);
        r.sz = sBase * (0.5f + (rand() % 100) / 140.0f);
        r.rot = (float)(rand() % 360);
        r.tone = (rand() % 100) / 100.0f;
        marsRocks.push_back(r);
    }
}

void setupMarsGroundFog()
{
    GLfloat fogColor[4] = {0.72f, 0.50f, 0.42f, 1.0f};
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 45.0f);
    glFogf(GL_FOG_END, 250.0f);
}

void drawGround()
{
    if (currentState == CRUISING_TO_MARS)
        return;
    if (currentState != ARRIVED_MARS && shuttleY > 800.0f)
        return;

    float visibility = (currentState == ARRIVED_MARS) ? 1.0f : (1.0f - (shuttleY / 800.0f));
    if (visibility < 0.0f)
        visibility = 0.0f;

    glPushMatrix();

    if (currentState == ARRIVED_MARS)
    {
        setupMarsGroundFog();
        glPushMatrix();
        glTranslatef(450.0f, -8.0f, 40.0f);
        drawMarsGroundSky();
        drawMarsGroundMountains();
        drawMarsGroundDetailed(140, 2);
        drawMarsGroundRocks();
        glPopMatrix();
    }
    else
    {
        glDisable(GL_FOG);
        glPushMatrix();
        glTranslatef(0.0f, -8.0f, 0.0f);
        drawEarthGroundSkyGradient();
        drawEarthGroundCloudBands();
        drawEarthGroundMountainsDetailed();
        drawEarthGroundDetailed(125, 2);
        drawEarthGroundRocks(earthRocksFar);
        drawEarthGroundRocks(earthRocksMid);
        drawEarthGroundRocks(earthRocksNear);
        drawEarthGroundTwigs();
        glPopMatrix();
    }

    glPopMatrix();
}

void drawLaunchPad()
{
    if (currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS)
        return;

    auto setColor = [](float r, float g, float b)
    {
        glColor3f(r, g, b);
    };

    auto drawCylinder = [](float r, float h, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluCylinder(q, r, r, h, slices, 1);
        gluDeleteQuadric(q);
    };

    auto drawDisk = [](float innerR, float outerR, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluDisk(q, innerR, outerR, slices, 1);
        gluDeleteQuadric(q);
    };

    auto drawPadPetal = [&]()
    {
        setColor(0.78f, 0.76f, 0.70f);
        glPushMatrix();
        glScalef(5.0f, 0.4f, 2.5f);
        glutSolidCube(1.0f);
        glPopMatrix();

        setColor(0.15f, 0.17f, 0.45f);
        glPushMatrix();
        glTranslatef(0.0f, -0.23f, 0.0f);
        glScalef(5.15f, 0.15f, 2.65f);
        glutSolidCube(1.0f);
        glPopMatrix();
    };

    auto drawCenterPlatform = [&]()
    {
        setColor(0.78f, 0.76f, 0.70f);
        glPushMatrix();
        glScalef(6.0f, 0.45f, 6.0f);
        glutSolidCube(1.0f);
        glPopMatrix();

        setColor(0.15f, 0.17f, 0.45f);
        glPushMatrix();
        glTranslatef(0.0f, -0.28f, 0.0f);
        glScalef(6.3f, 0.18f, 6.3f);
        glutSolidCube(1.0f);
        glPopMatrix();

        setColor(0.08f, 0.08f, 0.08f);
        for (int i = 0; i < 20; i++)
        {
            float a = (float)i * 2.0f * (float)M_PI / 20.0f;
            float x = 2.7f * cosf(a);
            float z = 2.7f * sinf(a);
            glPushMatrix();
            glTranslatef(x, 0.25f, z);
            glutSolidSphere(0.07f, 8, 8);
            glPopMatrix();
        }
    };

    auto drawLaunchMount = [&]()
    {
        setColor(0.40f, 0.40f, 0.40f);
        glPushMatrix();
        glTranslatef(0.0f, 0.23f, 0.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        drawCylinder(1.1f, 0.35f, 40);
        glPopMatrix();

        setColor(0.25f, 0.25f, 0.25f);
        glPushMatrix();
        glTranslatef(0.0f, 0.42f, 0.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        drawDisk(0.0f, 1.1f, 40);
        glPopMatrix();
    };

    glPushMatrix();
    glTranslatef(0.0f, -2.0f, -8.0f);
    glScalef(5.5f, 5.5f, 5.5f);

    for (int i = 0; i < 4; i++)
    {
        glPushMatrix();
        glRotatef((float)i * 90.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(4.9f, 0.2f, 0.0f);
        drawPadPetal();
        glPopMatrix();
    }

    drawCenterPlatform();
    drawLaunchMount();
    glPopMatrix();
}

void drawLaunchTower()
{
    if (shuttleY > 10.0f)
        return;
    if (currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS)
        return;

    glPushMatrix();
    glTranslatef(16.0f, -2.0f, -8.0f);
    glScalef(7.2f, 7.2f, 7.2f);

    glColor3f(0.55f, 0.33f, 0.20f);
    glPushMatrix();
    glTranslatef(2.1f, 4.0f, 0.0f);
    glScalef(0.8f, 8.0f, 0.8f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.43f, 0.24f, 0.14f);
    glPushMatrix();
    glTranslatef(2.1f, 4.0f, 0.41f);
    glScalef(0.82f, 8.02f, 0.05f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(2.1f, 4.0f, -0.41f);
    glScalef(0.82f, 8.02f, 0.05f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.60f, 0.28f, 0.22f);
    glPushMatrix();
    glTranslatef(1.25f, 4.6f, 0.0f);
    glScalef(1.7f, 0.18f, 0.25f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.12f, 0.12f, 0.22f);
    glPushMatrix();
    glTranslatef(0.55f, 4.58f, 0.0f);
    glScalef(0.5f, 0.15f, 0.7f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.05f, 0.05f, 0.05f);
    glBegin(GL_LINES);
    glVertex3f(1.25f, 4.55f, 0.0f);
    glVertex3f(1.25f, 0.5f, 0.0f);
    glEnd();

    glPopMatrix();
}

void drawDispatchInstruments()
{
    if (currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS)
        return;

    glColor3f(0.28f, 0.3f, 0.33f);
    glPushMatrix();
    glTranslatef(-40.0f, -4.0f, 22.0f);
    glScalef(12.0f, 8.0f, 8.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.1f, 0.85f, 0.2f);
    glPushMatrix();
    glTranslatef(-35.5f, -1.5f, 26.5f);
    glScalef(2.2f, 1.2f, 0.2f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.65f, 0.68f, 0.72f);
    glPushMatrix();
    glTranslatef(-46.0f, 2.0f, 18.0f);
    GLUquadric *antenna = gluNewQuadric();
    gluCylinder(antenna, 0.35f, 0.35f, 7.5f, 10, 1);
    glTranslatef(0.0f, 0.0f, 7.8f);
    glutSolidSphere(1.0f, 12, 12);
    gluDeleteQuadric(antenna);
    glPopMatrix();

    glColor3f(0.5f, 0.55f, 0.58f);
    glPushMatrix();
    glTranslatef(22.0f, 5.0f, -24.0f);
    glScalef(8.0f, 5.0f, 6.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.9f, 0.15f, 0.12f);
    glPushMatrix();
    glTranslatef(22.0f, 8.4f, -24.0f);
    glutSolidSphere(0.9f, 10, 10);
    glPopMatrix();
}

void drawShuttleOrbiter()
{
    if (rocketExploded)
        return;
    glPushMatrix();

    if (currentState == CRUISING_TO_MARS)
    {
        float x = transferT * 420.0f;
        float y = 20.0f + sin(transferT * 3.14159f) * 35.0f;
        float z = 0.0f;

        glTranslatef(x, y, z);
        glRotatef(-90.0f, 0.0f, 0.0f, 1.0f);
    }
    else if (currentState == ARRIVED_MARS)
    {
        float baseY = -8.0f + marsTerrainHeight(420.0f - 450.0f, 35.0f - 40.0f);
        float y = baseY + 6.0f;
        if (landingInProgress)
        {
            float t = (landingY - LANDING_TARGET_Y) / (LANDING_START_Y - LANDING_TARGET_Y);
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;
            y = baseY + 6.0f + t * 60.0f;
        }
        glTranslatef(420.0f, y, 35.0f);
    }
    else
    {
        glTranslatef(0.0f, shuttleY, -8.0f);
    }

    auto drawCylinder = [](float r, float h, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluCylinder(q, r, r, h, slices, 1);
        gluDeleteQuadric(q);
    };

    auto drawCone = [](float r, float h, int slices)
    {
        glutSolidCone(r, h, slices, 20);
    };

    auto drawDisk = [](float innerR, float outerR, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluDisk(q, innerR, outerR, slices, 1);
        gluDeleteQuadric(q);
    };

    auto drawNoseWindowsAndDetails = [&]()
    {
        float wy[2] = {3.95f, 3.25f};
        for (int i = 0; i < 2; i++)
        {
            float y = wy[i];

            glColor3f(0.10f, 0.10f, 0.10f);
            glPushMatrix();
            glTranslatef(0.0f, y, 0.97f);
            glutSolidTorus(0.03f, 0.12f, 20, 30);
            glPopMatrix();

            glColor3f(0.15f, 0.18f, 0.22f);
            glPushMatrix();
            glTranslatef(0.0f, y, 0.97f);
            glutSolidSphere(0.085f, 20, 20);
            glPopMatrix();
        }

        glColor3f(0.88f, 0.15f, 0.10f);
        glPushMatrix();
        glTranslatef(0.0f, 2.35f, 0.56f);
        glScalef(0.03f, 1.5f, 0.03f);
        glutSolidCube(1.0f);
        glPopMatrix();
    };

    glScalef(4.4f, 4.4f, 4.4f);

    glColor3f(0.96f, 0.96f, 0.97f);
    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.55f, 3.4f, 50);
    glPopMatrix();

    glColor3f(0.70f, 0.70f, 0.72f);
    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawDisk(0.0f, 0.55f, 50);
    glPopMatrix();

    glColor3f(0.15f, 0.15f, 0.15f);
    glPushMatrix();
    glTranslatef(0.0f, 4.45f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.57f, 0.08f, 50);
    glPopMatrix();

    glColor3f(0.90f, 0.20f, 0.12f);
    glPushMatrix();
    glTranslatef(0.0f, 4.48f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCone(0.55f, 1.15f, 50);
    glPopMatrix();

    glColor3f(0.98f, 0.85f, 0.85f);
    glPushMatrix();
    glTranslatef(0.10f, 4.95f, 0.43f);
    glScalef(0.08f, 0.55f, 0.08f);
    glutSolidCube(1.0f);
    glPopMatrix();

    drawNoseWindowsAndDetails();

    glColor3f(0.95f, 0.95f, 0.95f);
    glPushMatrix();
    glTranslatef(-0.62f, 1.0f, 0.0f);
    glScalef(0.06f, 1.6f, 1.1f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.62f, 1.0f, 0.0f);
    glScalef(0.06f, 1.6f, 1.1f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.0f, -0.62f);
    glScalef(1.1f, 1.6f, 0.06f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.88f, 0.15f, 0.10f);
    glPushMatrix();
    glTranslatef(-0.67f, 1.0f, 0.0f);
    glScalef(0.015f, 1.62f, 1.12f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.67f, 1.0f, 0.0f);
    glScalef(0.015f, 1.62f, 1.12f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.0f, -0.67f);
    glScalef(1.12f, 1.62f, 0.015f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.45f, 0.26f, 0.18f);
    glPushMatrix();
    glTranslatef(0.0f, 1.05f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.28f, 0.22f, 36);
    glPopMatrix();

    float ex[3] = {0.0f, -0.22f, 0.22f};
    float ez[3] = {0.0f, 0.18f, 0.18f};
    float es[3] = {1.1f, 0.9f, 0.9f};

    for (int i = 0; i < 3; i++)
    {
        glPushMatrix();
        glTranslatef(ex[i], 0.25f, ez[i]);
        glScalef(es[i], es[i], es[i]);

        glColor3f(0.75f, 0.76f, 0.78f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        GLUquadric *q = gluNewQuadric();
        gluCylinder(q, 0.14f, 0.09f, 0.22f, 32, 1);
        glTranslatef(0.0f, 0.0f, 0.22f);
        gluDisk(q, 0.0f, 0.09f, 32, 1);
        gluDeleteQuadric(q);

        glPopMatrix();
    }

    glPopMatrix();
}

void drawSolidRocketBoosters()
{
    if (rocketExploded)
        return;
    if (boosters_separated)
        return;
    if (currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS)
        return;

    auto drawCylinder = [](float r, float h, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluCylinder(q, r, r, h, slices, 1);
        gluDeleteQuadric(q);
    };

    auto drawCone = [](float r, float h, int slices)
    {
        glutSolidCone(r, h, slices, 20);
    };

    auto drawBoosterAtX = [&](float x)
    {
        glPushMatrix();
        glTranslatef(x, 2.55f, -0.15f);

        glColor3f(0.95f, 0.95f, 0.96f);
        glPushMatrix();
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        drawCylinder(0.23f, 3.4f, 36);
        glPopMatrix();

        glColor3f(0.90f, 0.20f, 0.12f);
        glPushMatrix();
        glTranslatef(0.0f, 3.4f, 0.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        drawCone(0.23f, 0.55f, 32);
        glPopMatrix();

        glColor3f(0.65f, 0.65f, 0.66f);
        glPushMatrix();
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        drawCylinder(0.24f, 0.09f, 32);
        glPopMatrix();

        glColor3f(0.72f, 0.73f, 0.75f);
        glPushMatrix();
        glTranslatef(0.0f, -0.02f, 0.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        GLUquadric *q = gluNewQuadric();
        gluCylinder(q, 0.12f, 0.08f, 0.20f, 30, 1);
        glTranslatef(0.0f, 0.0f, 0.20f);
        gluDisk(q, 0.0f, 0.08f, 30, 1);
        gluDeleteQuadric(q);
        glPopMatrix();

        glPopMatrix();
    };

    glPushMatrix();
    glTranslatef(0.0f, shuttleY, -8.0f);
    glScalef(4.4f, 4.4f, 4.4f);
    drawBoosterAtX(-2.45f);
    drawBoosterAtX(2.45f);
    glPopMatrix();
}

void drawExternalTank()
{
    if (rocketExploded)
        return;
    if (tank_separated)
        return;
    if (currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS)
        return;

    auto drawCylinder = [](float r, float h, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluCylinder(q, r, r, h, slices, 1);
        gluDeleteQuadric(q);
    };

    auto drawCone = [](float r, float h, int slices)
    {
        glutSolidCone(r, h, slices, 20);
    };

    auto drawDisk = [](float innerR, float outerR, int slices)
    {
        GLUquadric *q = gluNewQuadric();
        if (!q)
            return;
        gluDisk(q, innerR, outerR, slices, 1);
        gluDeleteQuadric(q);
    };

    glPushMatrix();
    glTranslatef(0.0f, shuttleY, -8.0f);
    glScalef(4.4f, 4.4f, 4.4f);
    glTranslatef(0.0f, 2.8f, -1.05f);

    glColor3f(0.86f, 0.45f, 0.20f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(0.43f, 3.9f, 44);
    glPopMatrix();

    glColor3f(0.90f, 0.52f, 0.25f);
    glPushMatrix();
    glTranslatef(0.0f, 3.9f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCone(0.43f, 0.75f, 40);
    glPopMatrix();

    glColor3f(0.78f, 0.38f, 0.17f);
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawDisk(0.0f, 0.43f, 40);
    glPopMatrix();

    glPopMatrix();
}

void drawMainEngineFlames()
{
    if (currentState != LAUNCHING)
        return;
    if (rocketExploded)
        return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_LIGHTING);

    for (int e = 0; e < 3; e++)
    {
        float xOffset = (e - 1) * 2.5f;
        glPushMatrix();
        glTranslatef(xOffset, shuttleY - 30.0f, -8.0f);
        glBegin(GL_TRIANGLES);
        glColor4f(1.0f, 1.0f, 0.9f, 0.9f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glColor4f(1.0f, 0.7f, 0.2f, 0.6f);
        glVertex3f(-1.5f, -15.0f - exhaustFlame, 0.0f);
        glVertex3f(1.5f, -15.0f - exhaustFlame, 0.0f);
        glEnd();
        glPopMatrix();
    }

    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
}

void drawSRBFlames()
{
    if (currentState != LAUNCHING || boosters_separated)
        return;
    if (rocketExploded)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_LIGHTING);

    for (int side = 0; side < 2; side++)
    {
        float xOffset = (side == 0) ? -12.0f : 12.0f;
        glPushMatrix();
        glTranslatef(xOffset, shuttleY - 32.0f, 0.0f);
        glBegin(GL_TRIANGLES);
        glColor4f(1.0f, 1.0f, 0.95f, 1.0f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glColor4f(1.0f, 0.9f, 0.5f, 0.8f);
        glVertex3f(-4.0f, -25.0f - exhaustFlame * 1.5f, 0.0f);
        glVertex3f(4.0f, -25.0f - exhaustFlame * 1.5f, 0.0f);
        glEnd();
        glPopMatrix();
    }

    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
}

void drawLadder(float x, float bottomY, float topY, float z)
{
    glColor3f(0.75f, 0.75f, 0.78f);

    glPushMatrix();
    glTranslatef(x - 1.0f, (bottomY + topY) * 0.5f, z);
    glScalef(0.25f, topY - bottomY, 0.25f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x + 1.0f, (bottomY + topY) * 0.5f, z);
    glScalef(0.25f, topY - bottomY, 0.25f);
    glutSolidCube(1.0f);
    glPopMatrix();

    for (int i = 0; i < 8; i++)
    {
        float rungY = bottomY + 2.0f + i * ((topY - bottomY - 4.0f) / 7.0f);
        glPushMatrix();
        glTranslatef(x, rungY, z);
        glScalef(2.2f, 0.18f, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}

void drawRocketLadders()
{
    if (currentState == READY || (currentState == LAUNCHING && !astronautInRocket))
        drawLadder(-4.0f, -8.0f, 22.0f, -6.0f);
}

void drawAstronaut()
{
    if (currentState == CRUISING_TO_MARS)
        return;
    if (landingInProgress && currentState == ARRIVED_MARS)
        return;
    if (astronautInRocket && (currentState == LAUNCHING || currentState == ARRIVED_MARS))
        return;
    if (rocketExploded)
        return;

    float legSwing = 0.0f;
    if ((currentState == LAUNCHING && !astronautInRocket) ||
        (currentState == ARRIVED_MARS && !astronautOnMars))
        legSwing = sin(astronautWalkPhase) * 0.8f;

    float drawX = astronautX;
    float drawY = astronautY;
    float drawZ = astronautZ;

    if (currentState == ARRIVED_MARS && !astronautInRocket)
    {
        // Mars terrain is rendered around world offset (450, -8, 40).
        float localX = drawX - 450.0f;
        float localZ = drawZ - 40.0f;
        drawY = -8.0f + marsTerrainHeight(localX, localZ) + 0.15f;
    }

    glPushMatrix();
    glTranslatef(drawX, drawY, drawZ);

    glColor3f(0.95f, 0.95f, 0.95f);
    glPushMatrix();
    glTranslatef(0.0f, 7.5f, 0.0f);
    glutSolidSphere(1.8f, 14, 14);
    glPopMatrix();

    glColor3f(0.2f, 0.45f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 3.5f, 0.0f);
    glScalef(2.4f, 4.0f, 1.4f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.15f, 0.2f, 0.35f);
    glPushMatrix();
    glTranslatef(-0.8f, 0.8f + legSwing, 0.0f);
    glScalef(0.7f, 2.4f, 0.9f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.8f, 0.8f - legSwing, 0.0f);
    glScalef(0.7f, 2.4f, 0.9f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPopMatrix();
}

// ===================== SPLASH SCREEN =====================
void drawBitmapText(const char *text, float x, float y, void *font)
{
    glRasterPos2f(x, y);
    for (const char *c = text; *c != '\0'; c++)
        glutBitmapCharacter(font, *c);
}

int getTextWidth(const char *text, void *font)
{
    int width = 0;
    for (const char *c = text; *c != '\0'; c++)
        width += glutBitmapWidth(font, *c);
    return width;
}

void drawCenteredText(const char *text, float y, void *font)
{
    int textWidth = getTextWidth(text, font);
    float x = (800 - textWidth) / 2.0f;
    drawBitmapText(text, x, y, font);
}

void drawSplashScreen()
{
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);
    glColor3f(0.05f, 0.05f, 0.15f);
    glVertex2f(0, 600);
    glVertex2f(800, 600);
    glColor3f(0.1f, 0.1f, 0.25f);
    glVertex2f(800, 0);
    glVertex2f(0, 0);
    glEnd();

    glColor3f(1.0f, 0.85f, 0.0f);
    drawCenteredText("SPACE SHUTTLE MISSION: EARTH TO MARS", 360, GLUT_BITMAP_TIMES_ROMAN_24);

    static int blinkCounter = 0;
    blinkCounter++;
    if ((blinkCounter / 30) % 2 == 0)
    {
        glColor3f(0.0f, 1.0f, 0.0f);
        drawCenteredText(">>> Press any key to continue <<<", 100, GLUT_BITMAP_HELVETICA_18);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void drawButton(Button *btn)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    if (btn->isHovered)
        glColor3f(btn->r + 0.2f, btn->g + 0.2f, btn->b + 0.2f);
    else
        glColor3f(btn->r, btn->g, btn->b);

    glBegin(GL_QUADS);
    glVertex2f(btn->x, btn->y);
    glVertex2f(btn->x + btn->width, btn->y);
    glVertex2f(btn->x + btn->width, btn->y + btn->height);
    glVertex2f(btn->x, btn->y + btn->height);
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(3.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(btn->x, btn->y);
    glVertex2f(btn->x + btn->width, btn->y);
    glVertex2f(btn->x + btn->width, btn->y + btn->height);
    glVertex2f(btn->x, btn->y + btn->height);
    glEnd();

    float textX = btn->x + (btn->width - strlen(btn->text) * 10) / 2;
    float textY = btn->y + (btn->height - 10) / 2;
    glRasterPos2f(textX, textY);
    for (const char *c = btn->text; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void drawStatusText()
{
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    char statusText[256];
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(220, 550);
    const char *title = "SPACE SHUTTLE MISSION: EARTH TO MARS";
    for (const char *c = title; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    if (currentState == READY)
    {
        glColor3f(0.0f, 1.0f, 0.0f);
        sprintf(statusText, "READY FOR LAUNCH - Destination: Mars");
    }
    else if (currentState == LAUNCHING)
    {
        glColor3f(1.0f, 0.7f, 0.0f);
        if (!astronautInRocket)
        {
            int remaining = WALK_DURATION_FRAMES - missionTimer;
            if (remaining < 0)
                remaining = 0;
            sprintf(statusText, "Crew boarding in progress... T-%0.1fs", remaining * 0.016f);
        }
        else
        {
            float altitudeMeters = shuttleY * 10.0f;
            if (!boosters_separated)
                sprintf(statusText, "LIFTOFF! Altitude: %.0f m | Boosters: %.0f-%.0f m",
                        altitudeMeters, BOOSTER_SEP_MIN_ALTITUDE, BOOSTER_SEP_MAX_ALTITUDE);
            else if (!tank_separated)
                sprintf(statusText, "LIFTOFF! Altitude: %.0f m | Tank: %.0f-%.0f m",
                        altitudeMeters, TANK_SEP_MIN_ALTITUDE, TANK_SEP_MAX_ALTITUDE);
            else
                sprintf(statusText, "LIFTOFF! Altitude: %.0f m", altitudeMeters);
        }
    }
    else if (currentState == CRUISING_TO_MARS)
    {
        glColor3f(0.0f, 1.0f, 1.0f);
        sprintf(statusText, "CRUISING TO MARS | Progress: %.0f%%", transferT * 100.0f);
    }
    else if (currentState == ARRIVED_MARS)
    {
        glColor3f(0.0f, 1.0f, 0.0f);
        if (astronautOnMars)
            sprintf(statusText, "MISSION SUCCESS! Astronaut has stepped on Mars.");
        else
            sprintf(statusText, "Landed on Mars. Astronaut descending ladder...");
    }
    else if (currentState == MISSION_FAILED)
    {
        glColor3f(1.0f, 0.2f, 0.2f);
        sprintf(statusText, "MISSION FAILED! Rocket destroyed.");
    }
    else
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        sprintf(statusText, "Awaiting input...");
    }

    glRasterPos2f(170, 520);
    for (const char *c = statusText; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
}

void drawExplosion(float x, float y, float z)
{
    if (!rocketExploded)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_LIGHTING);

    glPushMatrix();
    glTranslatef(x, y, z);
    glColor4f(1.0f, 0.5f, 0.1f, 0.8f);
    glutSolidSphere(explosionRadius, 28, 28);
    glColor4f(1.0f, 0.9f, 0.3f, 0.6f);
    glutSolidSphere(explosionRadius * 0.65f, 24, 24);
    glColor4f(0.9f, 0.2f, 0.1f, 0.5f);
    glutSolidSphere(explosionRadius * 0.35f, 20, 20);
    glPopMatrix();

    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
}

void drawMarsDust()
{
    if (currentState != ARRIVED_MARS)
        return;
    if (!landingDustActive)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);

    glPushMatrix();
    glTranslatef(420.0f, -7.2f, 35.0f);
    glColor4f(0.85f, 0.55f, 0.3f, landingDustAlpha);
    glPushMatrix();
    glScalef(1.0f, 0.25f, 1.0f);
    glutSolidSphere(landingDustRadius, 20, 20);
    glPopMatrix();

    glTranslatef(18.0f, 0.0f, -12.0f);
    glColor4f(0.8f, 0.5f, 0.28f, landingDustAlpha * 0.8f);
    glPushMatrix();
    glScalef(1.0f, 0.2f, 1.0f);
    glutSolidSphere(landingDustRadius * 0.7f, 16, 16);
    glPopMatrix();

    glTranslatef(-32.0f, 0.0f, 18.0f);
    glColor4f(0.9f, 0.6f, 0.32f, landingDustAlpha * 0.7f);
    glPushMatrix();
    glScalef(1.0f, 0.2f, 1.0f);
    glutSolidSphere(landingDustRadius * 0.6f, 16, 16);
    glPopMatrix();
    glPopMatrix();

    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
}

void drawBangladeshFlag()
{
    if (currentState != ARRIVED_MARS || !astronautOnMars || !flagPlanted)
        return;

    glDisable(GL_LIGHTING);

    float flagBaseY = -8.0f + marsTerrainHeight(FLAG_X - 450.0f, FLAG_Z - 40.0f);

    glPushMatrix();
    glTranslatef(FLAG_X, flagBaseY, FLAG_Z);

    glColor3f(0.8f, 0.8f, 0.82f);
    glPushMatrix();
    glTranslatef(0.0f, 5.0f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    GLUquadric *pole = gluNewQuadric();
    gluCylinder(pole, 0.12f, 0.12f, 10.0f, 12, 1);
    gluDeleteQuadric(pole);
    glPopMatrix();

    glColor3f(0.0f, 0.45f, 0.22f);
    glPushMatrix();
    glTranslatef(2.5f, 10.0f, 0.0f);
    glScalef(5.0f, 3.0f, 0.15f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glColor3f(0.8f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(2.35f, 10.0f, 0.2f);
    glScalef(1.55f, 1.55f, 0.15f);
    glutSolidSphere(1.0f, 16, 16);
    glPopMatrix();

    glPopMatrix();

    glEnable(GL_LIGHTING);
}

// ============================= Display/Callback Logic ==========================
void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (currentState != ARRIVED_MARS)
        glDisable(GL_FOG);

    if (currentState == SPLASH_SCREEN)
    {
        drawSplashScreen();
        glutSwapBuffers();
        return;
    }

    drawSkyBackground();

    // Use a wider projection for Mars so the scene is less zoomed in.
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    if (h <= 0)
        h = 1;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (currentState == ARRIVED_MARS)
        gluPerspective(62.0, (float)w / (float)h, 1.0, 3000.0);
    else
        gluPerspective(45.0, (float)w / (float)h, 1.0, 3000.0);
    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    if (currentState == LAUNCHING)
    {
        cameraY = shuttleY + 80.0f;
        if (shuttleY > 100.0f)
            cameraZ = 250.0f;
        gluLookAt(0.0f, cameraY, cameraZ, 0.0f, shuttleY + 20.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    else if (currentState == CRUISING_TO_MARS)
    {
        gluLookAt(220.0f, 180.0f, 420.0f,
                  210.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f);
    }
    else if (currentState == ARRIVED_MARS)
    {
        // Wider, pulled-back Mars framing to keep rocket/astronaut/flag in view.
        gluLookAt(520.0f, 20.0f, 185.0f,
                  425.0f, -2.0f, 35.0f,
                  0.0f, 1.0f, 0.0f);

        GLfloat marsLightPos[] = {485.0f, 32.0f, 60.0f, 1.0f};
        GLfloat marsAmbient[] = {0.30f, 0.24f, 0.22f, 1.0f};
        GLfloat marsDiffuse[] = {0.95f, 0.80f, 0.72f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, marsLightPos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, marsAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, marsDiffuse);
    }
    else
    {
        gluLookAt(0.0f, cameraY, cameraZ, 0.0f, shuttleY + 20.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }

    drawStars();
    drawEarth();
    drawMars();
    drawGround();
    drawLaunchPad();
    drawLaunchTower();
    drawDispatchInstruments();
    drawSmoke();
    drawSRBFlames();
    drawMainEngineFlames();
    drawExternalTank();
    drawSolidRocketBoosters();
    drawRocketLadders();
    drawAstronaut();
    drawShuttleOrbiter();
    drawMarsDust();
    drawBangladeshFlag();
    drawExplosion(0.0f, shuttleY + 15.0f, -8.0f);
    drawRocketTrail();

    drawStatusText();

    float altitudeMeters = shuttleY * 10.0f;

    if (currentState == READY || currentState == LANDED)
        drawButton(&launchButton);
    else if (currentState == LAUNCHING && astronautInRocket && !boosters_separated &&
             altitudeMeters >= BOOSTER_SEP_MIN_ALTITUDE && altitudeMeters <= BOOSTER_SEP_MAX_ALTITUDE)
        drawButton(&boosterButton);
    else if (currentState == LAUNCHING && astronautInRocket && boosters_separated && !tank_separated &&
             altitudeMeters >= TANK_SEP_MIN_ALTITUDE && altitudeMeters <= TANK_SEP_MAX_ALTITUDE)
        drawButton(&tankButton);
    else if (currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS)
        drawButton(&returnButton);
    else if (currentState == MISSION_FAILED)
        drawButton(&returnButton);

    glutSwapBuffers();
}

// ========================== Event Handlers ===============================
bool isMouseOverButton(Button *btn, int mouseX, int mouseY)
{
    mouseY = 600 - mouseY;
    return (mouseX >= btn->x && mouseX <= btn->x + btn->width &&
            mouseY >= btn->y && mouseY <= btn->y + btn->height);
}

void mouse(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
        float altitudeMeters = shuttleY * 10.0f;

        if ((currentState == READY || currentState == LANDED) &&
            isMouseOverButton(&launchButton, x, y))
        {
            printf("LAUNCH! Earth to Mars mission started.\n");
            currentState = LAUNCHING;
            shuttleVelocity = 0.0f;
            shuttleY = 0.0f;
            boosters_separated = false;
            tank_separated = false;
            missionTimer = 0;
            liftoffFrames = 0;
            boosterSeparatedFrame = -1;
            rocketExploded = false;
            explosionRadius = 0.0f;
            landingInProgress = false;
            landingTimer = 0;
            landingY = LANDING_TARGET_Y;
            landingDustActive = false;
            landingDustRadius = 0.0f;
            landingDustAlpha = 0.0f;
            flagPlanting = false;
            flagPlanted = false;
            flagPlantTimer = 0;
            flagPlantProgress = 0.0f;
            transferT = 0.0f;
            astronautX = -55.0f;
            astronautY = -8.0f;
            astronautZ = -6.0f;
            astronautBoarded = false;
            astronautInRocket = false;
            astronautOnMars = false;
            astronautWalkPhase = 0.0f;
            initSmoke();
        }
        else if (currentState == LAUNCHING && astronautInRocket &&
                 !boosters_separated &&
                 altitudeMeters >= BOOSTER_SEP_MIN_ALTITUDE && altitudeMeters <= BOOSTER_SEP_MAX_ALTITUDE &&
                 isMouseOverButton(&boosterButton, x, y))
        {
            boosters_separated = true;
            boosterSeparatedFrame = liftoffFrames;
            printf("SRB SEPARATION (MANUAL)!\n");
        }
        else if (currentState == LAUNCHING && astronautInRocket &&
                 boosters_separated && !tank_separated &&
                 altitudeMeters >= TANK_SEP_MIN_ALTITUDE && altitudeMeters <= TANK_SEP_MAX_ALTITUDE &&
                 isMouseOverButton(&tankButton, x, y))
        {
            tank_separated = true;
            printf("EXTERNAL TANK SEPARATION (MANUAL)!\n");
        }
        else if ((currentState == CRUISING_TO_MARS || currentState == ARRIVED_MARS) &&
                 isMouseOverButton(&returnButton, x, y))
        {
            printf("Mission reset.\n");
            currentState = READY;
            shuttleY = 0.0f;
            shuttleVelocity = 0.0f;
            boosters_separated = false;
            tank_separated = false;
            transferT = 0.0f;
            missionTimer = 0;
            liftoffFrames = 0;
            boosterSeparatedFrame = -1;
            rocketExploded = false;
            explosionRadius = 0.0f;
            landingInProgress = false;
            landingTimer = 0;
            landingY = LANDING_TARGET_Y;
            landingDustActive = false;
            landingDustRadius = 0.0f;
            landingDustAlpha = 0.0f;
            flagPlanting = false;
            flagPlanted = false;
            flagPlantTimer = 0;
            flagPlantProgress = 0.0f;
            cameraY = 80.0f;
            cameraZ = 200.0f;
            astronautX = -55.0f;
            astronautY = -8.0f;
            astronautZ = -6.0f;
            astronautBoarded = false;
            astronautInRocket = false;
            astronautOnMars = false;
            astronautWalkPhase = 0.0f;
            initSmoke();
        }
        else if (currentState == MISSION_FAILED && isMouseOverButton(&returnButton, x, y))
        {
            printf("Mission reset.\n");
            currentState = READY;
            shuttleY = 0.0f;
            shuttleVelocity = 0.0f;
            boosters_separated = false;
            tank_separated = false;
            transferT = 0.0f;
            missionTimer = 0;
            liftoffFrames = 0;
            boosterSeparatedFrame = -1;
            rocketExploded = false;
            explosionRadius = 0.0f;
            landingInProgress = false;
            landingTimer = 0;
            landingY = LANDING_TARGET_Y;
            landingDustActive = false;
            landingDustRadius = 0.0f;
            landingDustAlpha = 0.0f;
            flagPlanting = false;
            flagPlanted = false;
            flagPlantTimer = 0;
            flagPlantProgress = 0.0f;
            cameraY = 80.0f;
            cameraZ = 200.0f;
            astronautX = -55.0f;
            astronautY = -8.0f;
            astronautZ = -6.0f;
            astronautBoarded = false;
            astronautInRocket = false;
            astronautOnMars = false;
            astronautWalkPhase = 0.0f;
            initSmoke();
        }
    }
}

void mouseMotion(int x, int y)
{
    launchButton.isHovered = isMouseOverButton(&launchButton, x, y);
    returnButton.isHovered = isMouseOverButton(&returnButton, x, y);
    boosterButton.isHovered = isMouseOverButton(&boosterButton, x, y);
    tankButton.isHovered = isMouseOverButton(&tankButton, x, y);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
    if (key == 27)
        exit(0);

    if (currentState == SPLASH_SCREEN)
    {
        currentState = READY;
        printf("\n>>> Starting Earth to Mars mission! <<<\n\n");
        glutPostRedisplay();
    }
}

// ========================== Animation/State Update ===================
void timer(int value)
{
    if (currentState == SPLASH_SCREEN)
    {
        glutPostRedisplay();
        glutTimerFunc(16, timer, value + 1);
        return;
    }

    missionTimer++;
    earthRot += 0.10f;
    if (earthRot > 360.0f)
        earthRot -= 360.0f;
    marsRot += 0.10f;
    if (marsRot > 360.0f)
        marsRot -= 360.0f;
    exhaustFlame = sin(value * 0.25f) * 5.0f;
    smokeTime += 0.1f;

    switch (currentState)
    {
    case LAUNCHING:
    {
        if (!astronautInRocket)
        {
            if (missionTimer <= WALK_DURATION_FRAMES)
            {
                float t = (float)missionTimer / (float)WALK_DURATION_FRAMES;
                astronautX = -55.0f + t * 51.0f;
                astronautY = -8.0f;
                astronautZ = -6.0f;
                astronautWalkPhase += 0.42f;
            }
            else if (missionTimer <= WALK_DURATION_FRAMES + BOARD_DURATION_FRAMES)
            {
                float t = (float)(missionTimer - WALK_DURATION_FRAMES) / (float)BOARD_DURATION_FRAMES;
                astronautX = -4.0f;
                astronautY = -8.0f + t * 30.0f;
                astronautZ = -6.0f;
                astronautWalkPhase += 0.25f;
            }
            else
            {
                astronautBoarded = true;
                astronautInRocket = true;
                liftoffFrames = 0;
                boosterSeparatedFrame = -1;
                printf("Crew boarded the rocket. Liftoff now!\n");
            }
            break;
        }

        liftoffFrames++;

        if (missionTimer % 2 == 0 && shuttleY < 100.0f)
        {
            spawnSmoke(0.0f, shuttleY - 30.0f, 0.0f);
            spawnSmoke(-12.0f, shuttleY - 32.0f, 0.0f);
            spawnSmoke(12.0f, shuttleY - 32.0f, 0.0f);
        }

        shuttleVelocity += 0.04f;
        shuttleY += shuttleVelocity;

        if (!boosters_separated && shuttleY * 10.0f >= BOOSTER_SEP_MAX_ALTITUDE)
        {
            currentState = MISSION_FAILED;
            rocketExploded = true;
            explosionRadius = 10.0f;
            printf("MISSION FAILED: Boosters not separated by %.0f m.\n", BOOSTER_SEP_MAX_ALTITUDE);
            break;
        }

        if (boosters_separated && !tank_separated && shuttleY * 10.0f >= TANK_SEP_MAX_ALTITUDE)
        {
            currentState = MISSION_FAILED;
            rocketExploded = true;
            explosionRadius = 10.0f;
            printf("MISSION FAILED: Tank not separated by %.0f m.\n", TANK_SEP_MAX_ALTITUDE);
            break;
        }

        if (shuttleY * 10.0f >= MAX_LAUNCH_ALTITUDE_METERS)
        {
            if (!boosters_separated || !tank_separated)
            {
                currentState = MISSION_FAILED;
                rocketExploded = true;
                explosionRadius = 10.0f;
                printf("MISSION FAILED: Separation not completed before orbit.\n");
            }
            else
            {
                currentState = CRUISING_TO_MARS;
                transferT = 0.0f;
                printf("Left Earth orbit. Cruising to Mars...\n");
            }
        }
        break;
    }

    case CRUISING_TO_MARS:
    {
        transferT += transferSpeed;
        if (transferT >= 1.0f)
        {
            transferT = 1.0f;
            currentState = ARRIVED_MARS;
            missionTimer = 0;
            landingInProgress = true;
            landingTimer = 0;
            landingY = LANDING_START_Y;
            landingDustActive = false;
            landingDustRadius = 0.0f;
            landingDustAlpha = 0.0f;
            flagPlanting = false;
            flagPlanted = false;
            flagPlantTimer = 0;
            flagPlantProgress = 0.0f;
            astronautInRocket = true;
            astronautOnMars = false;
            astronautBoarded = false;
            astronautX = 422.0f;
            astronautY = 72.0f;
            astronautZ = 35.0f;
            printf("Arrived at Mars!\n");
        }
        break;
    }

    case ARRIVED_MARS:
    {
        if (landingInProgress)
        {
            float t = (float)landingTimer / (float)LANDING_DURATION_FRAMES;
            if (t > 1.0f)
                t = 1.0f;
            landingY = LANDING_START_Y - (LANDING_START_Y - LANDING_TARGET_Y) * t;
            landingTimer++;
            if (landingTimer >= LANDING_DURATION_FRAMES)
            {
                landingInProgress = false;
                landingY = LANDING_TARGET_Y;
                landingDustActive = true;
                landingDustRadius = 6.0f;
                landingDustAlpha = 0.6f;
                missionTimer = 0;
                astronautX = 422.0f;
                astronautY = 72.0f;
                astronautZ = 35.0f;
            }
            break;
        }

        if (!astronautOnMars)
        {
            if (missionTimer <= MARS_CLIMB_DOWN_FRAMES)
            {
                float t = (float)missionTimer / (float)MARS_CLIMB_DOWN_FRAMES;
                astronautX = 422.0f;
                astronautY = 72.0f - t * 27.0f;
                astronautZ = 35.0f;
                astronautWalkPhase += 0.2f;
            }
            else if (missionTimer <= MARS_CLIMB_DOWN_FRAMES + MARS_WALK_FRAMES)
            {
                float t = (float)(missionTimer - MARS_CLIMB_DOWN_FRAMES) / (float)MARS_WALK_FRAMES;
                astronautInRocket = false;
                astronautX = 422.0f + t * 24.0f;
                astronautY = 45.0f;
                astronautZ = 35.0f;
                astronautWalkPhase += 0.42f;
            }
            else
            {
                astronautInRocket = false;
                astronautOnMars = true;
                flagPlanting = true;
                flagPlanted = false;
                flagPlantTimer = 0;
                flagPlantProgress = 0.0f;
                printf("Astronaut stepped onto Mars surface!\n");
            }
        }
        else if (flagPlanting)
        {
            flagPlantTimer++;
            flagPlantProgress = (float)flagPlantTimer / (float)FLAG_PLANT_DURATION_FRAMES;
            if (flagPlantProgress > 1.0f)
                flagPlantProgress = 1.0f;

            astronautX = FLAG_X - 6.0f;
            astronautY = FLAG_PLANT_Y - 2.0f + sin(flagPlantProgress * 3.14159f) * 1.5f;
            astronautZ = FLAG_Z - 3.0f;
            astronautWalkPhase = 0.0f;

            if (flagPlantTimer >= FLAG_PLANT_DURATION_FRAMES)
            {
                flagPlanting = false;
                flagPlanted = true;
                printf("Flag planted on Mars!\n");
            }
        }
        break;
    }

    default:
        break;
    }

    if (rocketExploded && explosionRadius < MAX_EXPLOSION_RADIUS)
        explosionRadius += 3.5f;

    if (landingDustActive)
    {
        landingDustRadius += 0.6f;
        landingDustAlpha -= 0.01f;
        if (landingDustAlpha <= 0.0f)
            landingDustActive = false;
    }

    updateSmoke();
    glutPostRedisplay();
    glutTimerFunc(16, timer, value + 1);
}

// ======================= Window Setup & Main =========================
void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / (float)h, 1.0, 3000.0);
    glMatrixMode(GL_MODELVIEW);
}

void init()
{
    srand((unsigned int)time(NULL));
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat lightPos[] = {100.0f, 200.0f, 100.0f, 1.0f};
    GLfloat lightAmbient[] = {0.5f, 0.5f, 0.5f, 1.0f};
    GLfloat lightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    marsTex = loadMarsBMP(marsTexturePath, marsTextureOK);

    initSmoke();
    initEarthGroundScatter();
    initMarsGroundRocks();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Space Shuttle Runner");

    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(mouseMotion);
    glutTimerFunc(0, timer, 0);

    printf("\n====================================================\n");
    printf("      SPACE SHUTTLE MISSION: EARTH TO MARS         \n");
    printf("====================================================\n");
    printf(" CONTROLS:                                          \n");
    printf(" - Press any key to continue from splash screen     \n");
    printf(" - Click LAUNCH SHUTTLE to start mission            \n");
    printf(" - Click RESET MISSION during/after cruise          \n");
    printf(" - ESC to exit                                      \n");
    printf("====================================================\n\n");

    glutMainLoop();
    return 0;
}