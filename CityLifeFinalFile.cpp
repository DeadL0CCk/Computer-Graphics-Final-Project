// ============================================================================
//  CityLife.cpp  --  Group Project: four scenarios in one program
//    Scenario 1: Dynamic Coastal City    Scenario 2: Downtown Neon District
//    Scenario 3: Riverside Park          Scenario 4: Winter Night Market
//
//  CONTROLS:  N/B or Arrow keys = switch scene | F1-F4 = jump to scene
//             SPACE = pause | H = help panel | ESC = quit
// ============================================================================

#include <windows.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
using namespace std;

// App state: which scenario is currently active
enum AppScreen { SCENARIO_1, SCENARIO_2, SCENARIO_3, SCENARIO_4 };
AppScreen currentScreen = SCENARIO_1;
const int NUM_SCENARIOS = 4;
int currentScenarioIndex = 0;

const int WINDOW_WIDTH  = 1200;
const int WINDOW_HEIGHT = 800;
int viewportPixelWidth = WINDOW_WIDTH;
const float WORLD_LEFT   = -60.0f, WORLD_RIGHT = 60.0f;
const float WORLD_BOTTOM = -40.0f, WORLD_TOP   = 40.0f;

// --- Text rendering helpers ---
inline void DrawText(float x, float y, void* font, const char* text)
{
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(font, *c);
    }
}
inline int TextPixelWidth(void* font, const char* text)
{
    int w = 0;
    for (const char* c = text; *c != '\0'; ++c) {
        w += glutBitmapWidth(font, *c);
    }
    return w;
}
inline void DrawTextCentered(float cx, float y, void* font, const char* text)
{
    int pixelWidth = TextPixelWidth(font, text);
    float worldWidth = pixelWidth * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
    DrawText(cx - worldWidth * 0.5f, y, font, text);
}

// --- Depth/perspective scaling helpers ---
const float DEPTH_HORIZON_Y = -6.0f;
const float DEPTH_NEAR_Y    = -26.0f;
inline float DepthScale(float y)
{
    float t = (y - DEPTH_HORIZON_Y) / (DEPTH_NEAR_Y - DEPTH_HORIZON_Y);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 0.62f + t * (1.36f - 0.62f);
}
inline float DepthScaleRange(float y, float farY, float nearY,
                             float farScale, float nearScale)
{
    float t = (y - farY) / (nearY - farY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return farScale + t * (nearScale - farScale);
}

// Draws concentric ellipses with fading alpha (used for shadows and light pools)
inline void DrawSoftEllipse(float cx, float cy, float rx, float ry,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char a, int layers)
{
    if (layers < 1) layers = 1;
    for (int L = layers; L >= 1; --L) {
        float f = (float)L / (float)layers;
        glColor4ub(r, g, b, (unsigned char)(a / (float)L));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int i = 0; i <= 24; i++) {
                float ang = (float)i / 24.0f * 6.2831853f;
                glVertex2f(cx + rx * f * cosf(ang), cy + ry * f * sinf(ang));
            }
        glEnd();
    }
}

// Scales a sprite around its anchor point for depth perspective
inline void BeginDepthSprite(float anchorX, float anchorY, float scale)
{
    glPushMatrix();
    glTranslatef(anchorX, anchorY, 0.0f);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-anchorX, -anchorY, 0.0f);
}
inline void EndDepthSprite()
{
    glPopMatrix();
}

// --- Water reflection helpers ---
// BeginReflection: mirrors geometry below waterline
// WashReflection: fades reflection toward water colour with ripple bands
inline void BeginReflection(float surfaceY, float squash, float wobble)
{
    glPushMatrix();
    glTranslatef(wobble, surfaceY, 0.0f);
    glScalef(1.0f, -squash, 1.0f);
    glTranslatef(-wobble, -surfaceY, 0.0f);
}
inline void EndReflection()
{
    glPopMatrix();
}
inline void WashReflection(float cx, float halfW, float surfaceY, float depth,
                           unsigned char r, unsigned char g, unsigned char b,
                           unsigned char nearAlpha, float ripplePhase)
{
    glBegin(GL_QUADS);
        glColor4ub(r, g, b, nearAlpha);
        glVertex2f(cx - halfW, surfaceY);
        glVertex2f(cx + halfW, surfaceY);
        glColor4ub(r, g, b, 255);
        glVertex2f(cx + halfW, surfaceY - depth);
        glVertex2f(cx - halfW, surfaceY - depth);
    glEnd();
    for (int i = 0; i < 7; i++) {
        float t  = (i + 0.5f) / 7.0f;
        float y  = surfaceY - t * depth;
        float th = 0.10f + 0.16f * (0.5f + 0.5f * sinf(ripplePhase * 0.7f + i * 1.9f));
        float sx = sinf(ripplePhase * 0.5f + i * 2.3f) * halfW * 0.18f;
        glColor4ub(r, g, b, (unsigned char)(150 + 80 * t));
        glBegin(GL_QUADS);
            glVertex2f(cx - halfW + sx, y);
            glVertex2f(cx + halfW + sx, y);
            glVertex2f(cx + halfW + sx, y - th);
            glVertex2f(cx - halfW + sx, y - th);
        glEnd();
    }
}

// Contact shadow under a figure
inline void DrawGroundShadow(float cx, float cy, float rx, float lean,
                             unsigned char a)
{
    DrawSoftEllipse(cx + lean, cy, rx, rx * 0.26f, 18, 22, 30, a, 3);
}

// --- Shared HUD / help overlay ---
bool showHelp = true;
bool isPaused = false;
inline void DrawSceneHUD(const char* title, const char* const* lines)
{
    int count = 0;
    while (lines[count] != nullptr) count++;
    char navLine[64];
    sprintf(navLine, "SCENARIO %d / %d", currentScenarioIndex + 1, NUM_SCENARIOS);
    glColor4ub(255, 255, 255, 210);
    DrawText(WORLD_RIGHT - 15.0f, WORLD_TOP - 2.6f, GLUT_BITMAP_HELVETICA_12, navLine);
    glColor4ub(200, 212, 230, 175);
    DrawText(WORLD_RIGHT - 15.0f, WORLD_TOP - 4.4f, GLUT_BITMAP_HELVETICA_10,
             "N next    B back");
    if (!showHelp) {
        glColor4ub(255, 255, 255, 130);
        DrawText(WORLD_LEFT + 1.5f, WORLD_BOTTOM + 1.4f,
                 GLUT_BITMAP_HELVETICA_10, "H  help");
        if (isPaused) {
            glColor4ub(255, 220, 120, 220);
            DrawText(WORLD_LEFT + 1.5f, WORLD_BOTTOM + 3.2f,
                     GLUT_BITMAP_HELVETICA_12, "|| PAUSED");
        }
        return;
    }
    float lineH = 2.0f;
    float panelH = 4.2f + count * lineH;
    float panelW = 34.0f;
    float x0 = WORLD_LEFT + 1.0f;
    float y0 = WORLD_BOTTOM + 1.0f;
    glColor4ub(10, 12, 18, 165);
    glBegin(GL_QUADS);
        glVertex2f(x0, y0);
        glVertex2f(x0 + panelW, y0);
        glVertex2f(x0 + panelW, y0 + panelH);
        glVertex2f(x0, y0 + panelH);
    glEnd();
    glColor4ub(180, 190, 210, 110);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y0);
        glVertex2f(x0 + panelW, y0);
        glVertex2f(x0 + panelW, y0 + panelH);
        glVertex2f(x0, y0 + panelH);
    glEnd();
    float ty = y0 + panelH - 2.3f;
    glColor4ub(255, 255, 255, 235);
    DrawText(x0 + 1.2f, ty, GLUT_BITMAP_HELVETICA_12, title);
    ty -= 1.0f;
    if (isPaused) {
        glColor4ub(255, 220, 120, 235);
        DrawText(x0 + panelW - 9.5f, y0 + panelH - 2.3f,
                 GLUT_BITMAP_HELVETICA_12, "|| PAUSED");
    }
    glColor4ub(200, 210, 225, 210);
    for (int i = 0; i < count; i++) {
        ty -= lineH;
        DrawText(x0 + 1.2f, ty, GLUT_BITMAP_HELVETICA_10, lines[i]);
    }
    glLineWidth(1.0f);
}

// ============================================================================
//  SCENARIO 1 -- Dynamic Coastal City
//  Day/night cycle, storm weather, helicopter, lighthouse, harbour vessels
// ============================================================================
namespace Scenario1_CoastalCity {
constexpr float PI = 3.1416f;
bool isNight = false;
bool isAnimating = true;
// Day/night cycle and atmosphere
float timeOfDay = 0.2f;
float stormFactor = 0.0f;
float beaconPulse = 0.0f;
// Weather system: 0=Clear, 1=Rain, 2=Snow
int weatherMode = 0;
bool isSnowing = false;
float snowAccumulation = 0.0f;
float fogPos = 0.0f;
// Lightning effect state
bool lightningActive = false;
float lightningIntensity = 0.0f;
float lightningX[15];
float lightningY[15];
int lightningPoints = 0;
float moonX_glob = 0.0f;
float moonY_glob = -20.0f;
// Helicopter animation
float heliX = -45.0f;
float heliY = 14.0f;
float heliScale = 0.05f;
float heliPropAngle = 0.0f;
bool heliActive = true;
float pedWalkTimer = 0.0f;
// Traffic light: 0=Green, 1=Yellow, 2=Red
int trafficLightState = 0;
int trafficLightTimer = 0;
int activeAdSlide = 0;
int adSlideTimer = 0;
bool isCarRedBraking     = false;
bool isCarGreenBraking   = false;
bool isCarYellowBraking  = false;
bool isBusBraking        = false;
bool isCargoTruckBraking = false;
bool isSmallTruckBraking = false;

// Vehicle exhaust particle system
struct ExhaustParticle
{
    float x, y;
    float vx, vy;
    float size;
    float alpha;
    int   life;
    bool  active;
};
constexpr int MAX_EXHAUST = 80;
ExhaustParticle exhaustParticles[MAX_EXHAUST];

// Seagull flock system
struct Seagull
{
    float x, y;
    float baseSpeed;
    float wingAngle;
    float flapSpeed;
    float scale;
    float targetY;
    int   dir;
};
constexpr int NUM_SEAGULLS = 5;
Seagull seagulls[NUM_SEAGULLS] = {
    {-40.0f, 22.0f, 0.18f, 0.0f, 0.14f, 0.85f, 22.0f,  1},
    {-25.0f, 25.0f, 0.22f, 1.2f, 0.16f, 0.95f, 25.0f,  1},
    {-10.0f, 20.0f, 0.15f, 2.4f, 0.13f, 0.75f, 20.0f,  1},
    { 10.0f, 26.0f, 0.20f, 0.8f, 0.15f, 0.90f, 26.0f,  1},
    { 30.0f, 23.0f, 0.17f, 1.8f, 0.14f, 0.80f, 23.0f,  1}
};

// Glass elevator on Building2
float elevatorY = -7.5f;
int elevatorState = 0;
int elevatorPauseTimer = 0;

// Pedestrian system
struct Pedestrian
{
    float x;
    float y;
    float speed;
    float scale;
    float phase;
    int   dir;
    unsigned char shirtR, shirtG, shirtB;
    unsigned char pantsR, pantsG, pantsB;
    unsigned char skinR,  skinG,  skinB;
    bool crossing;
    float crossTimer;
    float savedX;
    bool  hasDog;
};
constexpr int NUM_PEDS = 7;
Pedestrian peds[NUM_PEDS] = {
    {-22.0f, -10.5f, 0.10f, 1.6f, 0.0f,  1, 220,70,70,   60,60,120,  220,175,130, false,0.f,0.f, false},
    {-10.0f, -10.5f, 0.07f, 1.8f, 1.2f, -1, 70,120,200,  50,40,40,   200,160,120, false,0.f,0.f, false},
    { 5.0f,  -10.5f, 0.12f, 1.5f, 2.4f,  1, 50,180,80,   80,50,80,   240,195,155, false,0.f,0.f, false},
    { 15.0f, -10.5f, 0.09f, 1.7f, 0.8f, -1, 200,150,50,  30,60,30,   210,170,130, false,0.f,0.f, false},
    {-35.0f, -10.5f, 0.11f, 1.6f, 3.6f,  1, 180,80,180,  60,60,60,   225,180,140, false,0.f,0.f, false},
    { 25.0f, -10.5f, 0.08f, 1.75f,1.8f, -1, 100,100,100, 30,30,80,   195,155,115, false,0.f,0.f, false},
    { 40.0f, -10.5f, 0.075f,1.65f,4.8f,  1, 90,70,60,    40,40,50,   215,175,130, false,0.f,0.f, true }
};
float getLightFactor();
constexpr int MAX_DROPS = 1000;
float dropX[MAX_DROPS];
float dropY[MAX_DROPS];
float dropTargetY[MAX_DROPS];
int totalDrops = 0;
bool isRaining = false;
constexpr int MAX_BUBBLES = 100;
float bubbleX[MAX_BUBBLES];
float bubbleY[MAX_BUBBLES];
float bubbleRadius[MAX_BUBBLES];
float bubbleAlpha[MAX_BUBBLES];
bool bubbleActive[MAX_BUBBLES];
int bubbleCount = 0;
float oceanSurfaceY = -30.0f;
float turbineAngle = 0.0f;
float tireAngle = 0.0f;
float cloudPos = 0.0f;
float sunY = 0.0f;
float waveMove = 0.0f;
float trainPos = 20.0f;
float busPos = -35.0f;
float carRedPos = -10.0f;
float carGreenPos = 5.0f;
float carYellowPos = 55.0f;
float truckCargoPos = -47.0f;
float truckSmallPos = 30.0f;
float cruisePos = -25.0f;
float yachtPos = -1.0f;
float boatSmall1Pos = 19.0f;
float fishingBoatX = 8.0f;
float cargoShipPos = 35.0f;
float boatSmall2Pos = -45.0f;
inline unsigned char clampToUByte(float val)
{
    if (val <= 0.0f) return 0;
    if (val >= 255.0f) return 255;
    return (unsigned char)(val + 0.5f);
}
void circle(float radius, float xc, float yc, float r, float g, float b, float a)

{
    unsigned char ur = clampToUByte(r);
    unsigned char ug = clampToUByte(g);
    unsigned char ub = clampToUByte(b);
    unsigned char ua = clampToUByte(a);
    glColor4ub(ur, ug, ub, ua);
    glBegin(GL_POLYGON);
    const int segments = 50;
    const float pi = 3.14159265f;
    for (int i = 0; i < segments; i++)
    {
        float A = (i * 2.0f * pi) / segments;
        float x = radius * cos(A);
        float y = radius * sin(A);
        glVertex2f(x + xc, y + yc);
    }
    glEnd();
}
void DrawLightCone(float xSource, float ySource, float dx, float dy, float length, float spreadWidth, unsigned char r, unsigned char g, unsigned char b, unsigned char maxAlpha)

{
    float px = -dy;
    float py = dx;
    float xBaseCenter = xSource + dx * length;
    float yBaseCenter = ySource + dy * length;
    glBegin(GL_TRIANGLE_FAN);
    glColor4ub(r, g, b, maxAlpha);
    glVertex2f(xSource, ySource);
    int segments = 12;
    for (int i = 0; i <= segments; i++) {
        float t = -1.0f + 2.0f * (float)i / (float)segments;
        float xVertex = xBaseCenter + px * (t * spreadWidth);
        float yVertex = yBaseCenter + py * (t * spreadWidth);
        float edgeFade = 1.0f - (t * t);
        float vertexAlpha = maxAlpha * 0.15f * edgeFade;
        glColor4ub(r, g, b, (unsigned char)vertexAlpha);
        glVertex2f(xVertex, yVertex);
    }
    glEnd();
}

// Sky gradient colour interpolation
struct SkyColor
{
    float r, g, b;
};
SkyColor lerpColor(SkyColor c1, SkyColor c2, float t)
{
    SkyColor result;
    result.r = c1.r + (c2.r - c1.r) * t;
    result.g = c1.g + (c2.g - c1.g) * t;
    result.b = c1.b + (c2.b - c1.b) * t;
    return result;
}
// Renders sky gradient, sun, moon, and lightning
void DrawSky()

{
    SkyColor NoonTop = {0.0f, 140.0f, 255.0f};
    SkyColor NoonBot = {180.0f, 225.0f, 255.0f};
    SkyColor SunsetTop = {30.0f, 20.0f, 60.0f};
    SkyColor SunsetBot = {253.0f, 94.0f, 83.0f};
    SkyColor NightTop = {0.0f, 0.0f, 15.0f};
    SkyColor NightBot = {15.0f, 15.0f, 40.0f};
    SkyColor DawnTop = {70.0f, 70.0f, 120.0f};
    SkyColor DawnBot = {255.0f, 170.0f, 150.0f};
    SkyColor StormTop = {30.0f, 30.0f, 40.0f};
    SkyColor StormBot = {120.0f, 170.0f, 200.0f};
    SkyColor currentTop, currentBot;
    float t = 0.0f;
    if (timeOfDay < 0.25f) {
        t = timeOfDay / 0.25f;
        currentTop = lerpColor(NoonTop, SunsetTop, t);
        currentBot = lerpColor(NoonBot, SunsetBot, t);
    } else if (timeOfDay < 0.5f) {
        t = (timeOfDay - 0.25f) / 0.25f;
        currentTop = lerpColor(SunsetTop, NightTop, t);
        currentBot = lerpColor(SunsetBot, NightBot, t);
    } else if (timeOfDay < 0.75f) {
        t = (timeOfDay - 0.5f) / 0.25f;
        currentTop = lerpColor(NightTop, DawnTop, t);
        currentBot = lerpColor(NightBot, DawnBot, t);
    } else {
        t = (timeOfDay - 0.75f) / 0.25f;
        currentTop = lerpColor(DawnTop, NoonTop, t);
        currentBot = lerpColor(DawnBot, NoonBot, t);
    }
    if (stormFactor > 0.0f) {
        currentTop = lerpColor(currentTop, StormTop, stormFactor);
        currentBot = lerpColor(currentBot, StormBot, stormFactor);
    }
    if (lightningActive) {
        SkyColor white = {255.0f, 255.0f, 255.0f};
        float flashBlend = lightningIntensity * 0.7f;
        currentTop = lerpColor(currentTop, white, flashBlend);
        currentBot = lerpColor(currentBot, white, flashBlend);
    }
    glBegin(GL_QUADS);
    glColor3ub((unsigned char)currentBot.r, (unsigned char)currentBot.g, (unsigned char)currentBot.b);
    glVertex2f(-60, -10);
    glVertex2f(60, -10);
    glColor3ub((unsigned char)currentTop.r, (unsigned char)currentTop.g, (unsigned char)currentTop.b);
    glVertex2f(60, 40);
    glVertex2f(-60, 40);
    glEnd();
    if (lightningActive && lightningPoints > 1) {
        glLineWidth(3.0f);
        glColor4ub(220, 240, 255, (unsigned char)(255 * lightningIntensity));
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < lightningPoints; i++) {
            glVertex2f(lightningX[i], lightningY[i]);
        }
        glEnd();
        glLineWidth(1.5f);
        glColor4ub(255, 255, 255, (unsigned char)(255 * lightningIntensity));
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < lightningPoints; i++) {
            glVertex2f(lightningX[i], lightningY[i]);
        }
        glEnd();
        glLineWidth(2.0f);
    }
    float sunAngle = (0.25f - timeOfDay) * 2.0f * PI;
    float sunX = 45.0f * cos(sunAngle);
    float sunY_pos = 28.0f * sin(sunAngle);
    float moonAngle = sunAngle + PI;
    float moonX = 45.0f * cos(moonAngle);
    float moonY_pos = 28.0f * sin(moonAngle);
    moonX_glob = moonX;
    moonY_glob = moonY_pos;
    if (sunY_pos > -12.0f && stormFactor < 1.0f) {
        float sunAlpha = (1.0f - stormFactor) * (sunY_pos > 0.0f ? 1.0f : (sunY_pos + 12.0f) / 12.0f);
        if (sunAlpha > 0.01f) {
            circle(5.5f, sunX, sunY_pos, 255, 255, 200, 50 * sunAlpha);
            circle(4.9f, sunX, sunY_pos, 255, 255, 140, 90 * sunAlpha);
            circle(4.0f, sunX, sunY_pos, 255, 250, 0, 255 * sunAlpha);
        }
    }
    if (moonY_pos > -12.0f && stormFactor < 1.0f) {
        float moonAlpha = (1.0f - stormFactor) * (moonY_pos > 0.0f ? 1.0f : (moonY_pos + 12.0f) / 12.0f);
        if (moonAlpha > 0.01f) {
            circle(4.5f, moonX, moonY_pos, 255, 255, 255, 255 * moonAlpha);
            circle(1.0f, moonX - 1.0f, moonY_pos + 1.0f, 200, 200, 200, 255 * moonAlpha);
            circle(0.7f, moonX + 2.0f, moonY_pos - 1.0f, 200, 200, 200, 255 * moonAlpha);
        }
    }
}
// Timer: advances day/night cycle and storm factor
void UpdateSun(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSun, 0); return; }
    if (isAnimating) {
        timeOfDay += 0.0003f;
        if (timeOfDay > 1.0f) timeOfDay -= 1.0f;
        if (isRaining) {
            if (stormFactor < 1.0f) stormFactor += 0.01f;
        } else {
            if (stormFactor > 0.0f) stormFactor -= 0.01f;
        }
        beaconPulse += 0.1f;
        if (beaconPulse > 2.0f * PI) beaconPulse -= 2.0f * PI;
        isNight = (timeOfDay > 0.32f && timeOfDay < 0.82f);
    }
    glutTimerFunc(16, UpdateSun, 0);
}
// Renders star points at night (hidden during rain)
void DrawStars()

{
    if (!isNight || isRaining) return;
    glColor3ub(255, 255, 255);
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    glVertex2f(-55, 35);
    glVertex2f(-45, 25);
    glVertex2f(-35, 38);
    glVertex2f(-25, 30);
    glVertex2f(-15, 36);
    glVertex2f(-5, 28);
    glVertex2f(5, 35);
    glVertex2f(15, 22);
    glVertex2f(25, 38);
    glVertex2f(35, 30);
    glVertex2f(45, 34);
    glVertex2f(55, 28);
    glVertex2f(-50, 18);
    glVertex2f(-30, 15);
    glVertex2f(-10, 20);
    glVertex2f(10, 15);
    glVertex2f(30, 12);
    glVertex2f(50, 18);
    glVertex2f(-58, 32);
    glVertex2f(-20, 39);
    glVertex2f(20, 32);
    glVertex2f(58, 38);
    glVertex2f(0, 39);
    glVertex2f(-40, 32);
    glVertex2f(40, 25);
    glVertex2f(-12, 33);
    glVertex2f(12, 36);
    glVertex2f(-43, 35);
    glVertex2f(-33, 25);
    glVertex2f(-45, 38);
    glVertex2f(-21, 30);
    glVertex2f(-19, 36);
    glVertex2f(-8, 28);
    glVertex2f(2, 35);
    glVertex2f(5, 11);
    glVertex2f(38, 38);
    glVertex2f(-25, 30);
    glVertex2f(-45, 34);
    glVertex2f(-55, 28);
    glEnd();
}

// Cloud drawing functions (4 different clouds)
void DrawCloud1()

{
    int r, g, b;
    if(isNight){
        r = 100;
        g = 100;
        b = 120;
    } else {
        r = 255;
        g = 255;
        b = 255;
    }
    circle(2.0, -30 + cloudPos, 25, r, g, b, 255);
    circle(3.0, -27 + cloudPos, 26, r, g, b, 255);
    circle(2.5, -24 + cloudPos, 25, r, g, b, 255);
    circle(2.0, -27 + cloudPos, 24, r, g, b, 255);
}
void DrawCloud2()

{
    int r, g, b;
    if(isNight){
        r = 100;
        g = 100;
        b = 120;
    } else {
        r = 255;
        g = 255;
        b = 255;
    }
    circle(2.4, 10 + cloudPos, 30, r, g, b, 255);
    circle(3.6, 13.6 + cloudPos, 31.2, r, g, b, 255);
    circle(3.0, 17.2 + cloudPos, 30, r, g, b, 255);
    circle(2.4, 13.6 + cloudPos, 28.8, r, g, b, 255);
}
void DrawCloud3()

{
    int r, g, b;
    if(isNight){
        r = 100;
        g = 100;
        b = 120;
    } else {
        r = 255;
        g = 255;
        b = 255;
    }
    circle(1.6, 40 + cloudPos, 20, r, g, b, 255);
    circle(2.4, 42.4 + cloudPos, 20.8, r, g, b, 255);
    circle(2.0, 44.8 + cloudPos, 20, r, g, b, 255);
    circle(1.6, 42.4 + cloudPos, 19.2, r, g, b, 255);
}
void DrawCloud4()

{
    int r, g, b;
    if(isNight){
        r = 100;
        g = 100;
        b = 120;
    } else {
        r = 255;
        g = 255;
        b = 255;
    }
    circle(2.0, -50 + cloudPos, 22, r, g, b, 255);
    circle(3.0, -47 + cloudPos, 23, r, g, b, 255);
    circle(2.5, -44 + cloudPos, 22, r, g, b, 255);
    circle(2.0, -47 + cloudPos, 21, r, g, b, 255);
}
void UpdateCloud(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCloud, 0); return; }
    if (isAnimating) {
        cloudPos += 0.1f;
        if (cloudPos > 120.0f) cloudPos = -120.0f;
    }
    glutTimerFunc(25, UpdateCloud, 0);
}

// Mountain drawing with snow caps
void DrawMountain1()

{
    glColor3ub(100, 80, 70);
    glBegin(GL_TRIANGLES);
    glVertex2f(-45.0f, 0.0f);
    glVertex2f(-25.0f, 20.0f);
    glVertex2f(-17.0f, 0.0f);
    glEnd();
    glColor3ub(60, 48, 42);
    glBegin(GL_TRIANGLES);
    glVertex2f(-17.0f, 0.0f);
    glVertex2f(-25.0f, 20.0f);
    glVertex2f(-5.0f, 0.0f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        float capHeight = 3.5f * snowAccumulation;
        glColor3ub(255, 255, 255);
        glBegin(GL_TRIANGLES);
        glVertex2f(-25.0f - capHeight, 20.0f - capHeight);
        glVertex2f(-25.0f + capHeight, 20.0f - capHeight);
        glVertex2f(-25.0f, 20.0f);
        glEnd();
    }
}
void DrawMountain2()

{
    glColor3ub(120, 90, 80);
    glBegin(GL_TRIANGLES);
    glVertex2f(-15.0f, 0.0f);
    glVertex2f(10.0f, 30.0f);
    glVertex2f(20.0f, 0.0f);
    glEnd();
    glColor3ub(72, 54, 48);
    glBegin(GL_TRIANGLES);
    glVertex2f(20.0f, 0.0f);
    glVertex2f(10.0f, 30.0f);
    glVertex2f(35.0f, 0.0f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        float capHeight = 4.5f * snowAccumulation;
        glColor3ub(255, 255, 255);
        glBegin(GL_TRIANGLES);
        glVertex2f(10.0f - capHeight, 30.0f - capHeight);
        glVertex2f(10.0f + capHeight, 30.0f - capHeight);
        glVertex2f(10.0f, 30.0f);
        glEnd();
    }
}
void DrawMountain3()

{
    glColor3ub(90, 70, 60);
    glBegin(GL_TRIANGLES);
    glVertex2f(25.0f, 0.0f);
    glVertex2f(40.0f, 15.0f);
    glVertex2f(46.0f, 0.0f);
    glEnd();
    glColor3ub(54, 42, 36);
    glBegin(GL_TRIANGLES);
    glVertex2f(46.0f, 0.0f);
    glVertex2f(40.0f, 15.0f);
    glVertex2f(55.0f, 0.0f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        float capHeight = 2.5f * snowAccumulation;
        glColor3ub(255, 255, 255);
        glBegin(GL_TRIANGLES);
        glVertex2f(40.0f - capHeight, 15.0f - capHeight);
        glVertex2f(40.0f + capHeight, 15.0f - capHeight);
        glVertex2f(40.0f, 15.0f);
        glEnd();
    }
}
// Semi-transparent overlay to darken the scene at night
void DrawNightOverlay()

{
    if (isNight) {
        glColor4ub(0, 0, 20, 120);
        glBegin(GL_QUADS);
        glVertex2f(-60, -40);
        glVertex2f(60, -40);
        glVertex2f(60, 40);
        glVertex2f(-60, 40);
        glEnd();
    }
}

// Road barriers with fence posts and guard rails
void RoadDivider1()

{
    glColor3ub(160, 160, 170);
    glBegin(GL_QUADS);
    glVertex2f(-60, -14);
    glVertex2f(60, -14);
    glVertex2f(60, -13);
    glVertex2f(-60, -13);
    glEnd();
    float postSpacing = 5.0f;
    for (float x = -60; x <= 60; x += postSpacing) {
        glColor3ub(40, 35, 40);
        glBegin(GL_QUADS);
        glVertex2f(x - 0.4, -14);
        glVertex2f(x + 0.4, -14);
        glVertex2f(x + 0.4, -10.5);
        glVertex2f(x - 0.4, -10.5);
        glEnd();
        if (isNight)
            circle(0.5, x, -10.2, 180, 140, 0, 255);
        else
            circle(0.5, x, -10.2, 255, 215, 0, 255);
        if (x < 60) {
            glColor3ub(60, 30, 20);
            glBegin(GL_QUADS);
            glVertex2f(x, -11);
            glVertex2f(x + postSpacing, -11);
            glVertex2f(x + postSpacing, -10.7);
            glVertex2f(x, -10.7);
            glEnd();
            glColor3ub(40, 40, 40);
            glBegin(GL_QUADS);
            glVertex2f(x, -13.5);
            glVertex2f(x + postSpacing, -13.5);
            glVertex2f(x + postSpacing, -13.2);
            glVertex2f(x, -13.2);
            glEnd();
        }
    }
}
void RoadDivider2()

{
    glColor3ub(160, 160, 170);
    glBegin(GL_QUADS);
    glVertex2f(-60, -24);
    glVertex2f(60, -24);
    glVertex2f(60, -23);
    glVertex2f(-60, -23);
    glEnd();
    float postSpacing = 5.0f;
    for (float x = -60; x <= 60; x += postSpacing) {
        glColor3ub(40, 35, 40);
        glBegin(GL_QUADS);
        glVertex2f(x - 0.4, -24);
        glVertex2f(x + 0.4, -24);
        glVertex2f(x + 0.4, -20.5);
        glVertex2f(x - 0.4, -20.5);
        glEnd();
        if (isNight)
            circle(0.5, x, -20.2, 180, 140, 0, 255);
        else
            circle(0.5, x, -20.2, 255, 215, 0, 255);
        if (x < 60) {
            glColor3ub(60, 30, 20);
            glBegin(GL_QUADS);
            glVertex2f(x, -21);
            glVertex2f(x + postSpacing, -21);
            glVertex2f(x + postSpacing, -20.7);
            glVertex2f(x, -20.7);
            glEnd();
            glColor3ub(40, 40, 40);
            glBegin(GL_QUADS);
            glVertex2f(x, -23.5);
            glVertex2f(x + postSpacing, -23.5);
            glVertex2f(x + postSpacing, -23.2);
            glVertex2f(x, -23.2);
            glEnd();
        }
    }
}

// Wind turbines (6 units with rotating blades)
void WindTurbine1()

{
    if (isNight) glColor3ub(100, 100, 100);
    else glColor3ub(220, 220, 220);
    glBegin(GL_QUADS);
    glVertex2f(-55.5f, -5.0f);
    glVertex2f(-54.5f, -5.0f);
    glVertex2f(-54.5f, 7.0f);
    glVertex2f(-55.5f, 7.0f);
    glEnd();
    glPushMatrix();
    glTranslatef(-55.0f, 7.0f, 0);
    glRotatef(turbineAngle, 0, 0, 1);
    if (isNight) circle(0.8, 0, 0, 50, 50, 50, 255);
    else circle(0.8, 0, 0, 100, 100, 100, 255);
    if (isNight) glColor3ub(150, 150, 150);
    else glColor3ub(240, 240, 240);
    float bladeAngle = 0.0f;
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glRotatef(bladeAngle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(-0.5, 6.0);
        glVertex2f(0.5, 6.0);
        glEnd();
        glPopMatrix();
        bladeAngle += 120.0f;
    }
    glPopMatrix();
}
void WindTurbine2()

{
    if (isNight) glColor3ub(100, 100, 100);
    else glColor3ub(220, 220, 220);
    glBegin(GL_QUADS);
    glVertex2f(54.5f, 0.0f);
    glVertex2f(55.5f, 0.0f);
    glVertex2f(55.5f, 12.0f);
    glVertex2f(54.5f, 12.0f);
    glEnd();
    glPushMatrix();
    glTranslatef(55.0f, 12.0f, 0);
    glRotatef(turbineAngle, 0, 0, 1);
    if (isNight) circle(0.8, 0, 0, 50, 50, 50, 255);
    else circle(0.8, 0, 0, 100, 100, 100, 255);
    if (isNight) glColor3ub(150, 150, 150);
    else glColor3ub(240, 240, 240);
    float bladeAngle = 0.0f;
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glRotatef(bladeAngle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(-0.5, 6.0);
        glVertex2f(0.5, 6.0);
        glEnd();
        glPopMatrix();
        bladeAngle += 120.0f;
    }
    glPopMatrix();
}
void WindTurbine3()

{
    if (isNight) glColor3ub(100, 100, 100);
    else glColor3ub(220, 220, 220);
    glBegin(GL_QUADS);
    glVertex2f(-52.5f, -2.0f);
    glVertex2f(-51.5f, -2.0f);
    glVertex2f(-51.5f, 10.0f);
    glVertex2f(-52.5f, 10.0f);
    glEnd();
    glPushMatrix();
    glTranslatef(-52.0f, 10.0f, 0);
    glRotatef(turbineAngle, 0, 0, 1);
    if (isNight) circle(0.8, 0, 0, 50, 50, 50, 255);
    else circle(0.8, 0, 0, 100, 100, 100, 255);
    if (isNight) glColor3ub(150, 150, 150);
    else glColor3ub(240, 240, 240);
    float bladeAngle = 0.0f;
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glRotatef(bladeAngle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(-0.5, 6.0);
        glVertex2f(0.5, 6.0);
        glEnd();
        glPopMatrix();
        bladeAngle += 120.0f;
    }
    glPopMatrix();
}
void WindTurbine4()

{
    if (isNight) glColor3ub(100, 100, 100);
    else glColor3ub(220, 220, 220);
    glBegin(GL_QUADS);
    glVertex2f(-48.5f, -4.0f);
    glVertex2f(-47.5f, -4.0f);
    glVertex2f(-47.5f, 8.0f);
    glVertex2f(-48.5f, 8.0f);
    glEnd();
    glPushMatrix();
    glTranslatef(-48.0f, 8.0f, 0);
    glRotatef(turbineAngle, 0, 0, 1);
    if (isNight) circle(0.8, 0, 0, 50, 50, 50, 255);
    else circle(0.8, 0, 0, 100, 100, 100, 255);
    if (isNight) glColor3ub(150, 150, 150);
    else glColor3ub(240, 240, 240);
    float bladeAngle = 0.0f;
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glRotatef(bladeAngle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(-0.5, 6.0);
        glVertex2f(0.5, 6.0);
        glEnd();
        glPopMatrix();
        bladeAngle += 120.0f;
    }
    glPopMatrix();
}
void WindTurbine5()

{
    if (isNight) glColor3ub(100, 100, 100);
    else glColor3ub(220, 220, 220);
    glBegin(GL_QUADS);
    glVertex2f(-44.5f, 0.0f);
    glVertex2f(-43.5f, 0.0f);
    glVertex2f(-43.5f, 12.0f);
    glVertex2f(-44.5f, 12.0f);
    glEnd();
    glPushMatrix();
    glTranslatef(-44.0f, 12.0f, 0);
    glRotatef(turbineAngle, 0, 0, 1);
    if (isNight) circle(0.8, 0, 0, 50, 50, 50, 255);
    else circle(0.8, 0, 0, 100, 100, 100, 255);
    if (isNight) glColor3ub(150, 150, 150);
    else glColor3ub(240, 240, 240);
    float bladeAngle = 0.0f;
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glRotatef(bladeAngle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(-0.5, 6.0);
        glVertex2f(0.5, 6.0);
        glEnd();
        glPopMatrix();
        bladeAngle += 120.0f;
    }
    glPopMatrix();
}
void WindTurbine6()

{
    if (isNight) glColor3ub(100, 100, 100);
    else glColor3ub(220, 220, 220);
    glBegin(GL_QUADS);
    glVertex2f(-40.5f, -3.0f);
    glVertex2f(-39.5f, -3.0f);
    glVertex2f(-39.5f, 9.0f);
    glVertex2f(-40.5f, 9.0f);
    glEnd();
    glPushMatrix();
    glTranslatef(-40.0f, 9.0f, 0);
    glRotatef(turbineAngle, 0, 0, 1);
    if (isNight) circle(0.8, 0, 0, 50, 50, 50, 255);
    else circle(0.8, 0, 0, 100, 100, 100, 255);
    if (isNight) glColor3ub(150, 150, 150);
    else glColor3ub(240, 240, 240);
    float bladeAngle = 0.0f;
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        glRotatef(bladeAngle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(-0.5, 6.0);
        glVertex2f(0.5, 6.0);
        glEnd();
        glPopMatrix();
        bladeAngle += 120.0f;
    }
    glPopMatrix();
}
void UpdateTurbine(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTurbine, 0); return; }
    if (isAnimating) {
        float speed = 2.0f;
        if (weatherMode == 1) speed = 5.5f;
        else if (weatherMode == 2) speed = 3.8f;
        turbineAngle -= speed;
        if (turbineAngle < -360.0f) turbineAngle += 360.0f;
    }
    glutTimerFunc(25, UpdateTurbine, 0);
}

// Draws an animated walking pedestrian with limb swing
void DrawPerson(const Pedestrian& p, float walk)
{
    float lf = getLightFactor();
    float dim = 1.0f - 0.55f * lf;
    glPushMatrix();
    glTranslatef(p.x, p.y, 0.0f);
    glScalef(p.scale, p.scale, 1.0f);
    if (p.dir < 0) glScalef(-1.0f, 1.0f, 1.0f);
    float t    = walk * 6.0f + p.phase;
    float swing = 0.35f * sin(t);
    float bob   = 0.05f * fabs(sin(t));
    glLineWidth(2);
    glColor3ub((unsigned char)(p.pantsR*dim),(unsigned char)(p.pantsG*dim),(unsigned char)(p.pantsB*dim));
    glPushMatrix();
    glTranslatef(-0.12f, bob, 0.0f);
    glRotatef(-swing * 50.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -0.55f);
    glEnd();
    glBegin(GL_LINES);
    glVertex2f(0.0f, -0.55f);
    glVertex2f(0.15f, -0.55f);
    glEnd();
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.12f, bob, 0.0f);
    glRotatef( swing * 50.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -0.55f);
    glEnd();
    glBegin(GL_LINES);
    glVertex2f(0.0f, -0.55f);
    glVertex2f(0.15f, -0.55f);
    glEnd();
    glPopMatrix();
    glColor3ub((unsigned char)(p.shirtR*dim),(unsigned char)(p.shirtG*dim),(unsigned char)(p.shirtB*dim));
    glBegin(GL_QUADS);
    glVertex2f(-0.2f, bob);
    glVertex2f( 0.2f, bob);
    glVertex2f( 0.2f, bob + 0.5f);
    glVertex2f(-0.2f, bob + 0.5f);
    glEnd();
    glLineWidth(2);
    glColor3ub((unsigned char)(p.skinR*dim),(unsigned char)(p.skinG*dim),(unsigned char)(p.skinB*dim));
    glPushMatrix();
    glTranslatef(-0.2f, bob + 0.4f, 0.0f);
    glRotatef( swing * 55.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-0.1f, -0.4f);
    glEnd();
    glPopMatrix();
    glPushMatrix();
    glTranslatef( 0.2f, bob + 0.4f, 0.0f);
    glRotatef(-swing * 55.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f( 0.1f, -0.4f);
    glEnd();
    glPopMatrix();
    circle(0.2f, 0.0f, bob + 0.7f,
           (unsigned char)(p.skinR*dim),
           (unsigned char)(p.skinG*dim),
           (unsigned char)(p.skinB*dim), 255);
    if (lf > 0.3f) {
        circle(0.28f, 0.0f, bob + 0.7f, 255, 200, 100, (unsigned char)(25 * lf));
    }
    if (p.hasDog) {
        float dogT = t * 1.6f;
        float legPh = sin(dogT);
        float dogGround = -0.55f;
        glColor3ub((unsigned char)(90*dim), (unsigned char)(90*dim), (unsigned char)(95*dim));
        glLineWidth(1.2f);
        glBegin(GL_LINES);
        glVertex2f(-0.2f, bob + 0.55f);
        glVertex2f(-0.62f, dogGround + 0.22f);
        glEnd();
        glColor3ub((unsigned char)(165*dim), (unsigned char)(120*dim), (unsigned char)(75*dim));
        glBegin(GL_QUADS);
        glVertex2f(-0.85f, dogGround + 0.03f);
        glVertex2f(-0.42f, dogGround + 0.03f);
        glVertex2f(-0.42f, dogGround + 0.22f);
        glVertex2f(-0.85f, dogGround + 0.22f);
        glEnd();
        circle(0.10f, -0.90f, dogGround + 0.18f,
               (unsigned char)(165*dim), (unsigned char)(120*dim), (unsigned char)(75*dim), 255);
        glLineWidth(1.3f);
        glBegin(GL_LINES);
        glVertex2f(-0.78f, dogGround + 0.03f); glVertex2f(-0.78f + 0.06f*legPh, dogGround - 0.05f);
        glVertex2f(-0.48f, dogGround + 0.03f); glVertex2f(-0.48f - 0.06f*legPh, dogGround - 0.05f);
        glEnd();
        glBegin(GL_LINES);
        glVertex2f(-0.85f, dogGround + 0.18f);
        glVertex2f(-0.97f, dogGround + 0.26f + 0.06f*sin(dogT*2.0f));
        glEnd();
    }
    glPopMatrix();
}

// Park bench with seated person
void DrawBench(float x)

{
    float lf  = getLightFactor();
    float dim = 1.0f - 0.4f * lf;
    float y = -9.3f;
    glColor3ub((unsigned char)(90*dim), (unsigned char)(60*dim), (unsigned char)(35*dim));
    glBegin(GL_QUADS);
    glVertex2f(x-1.4f, y);
    glVertex2f(x+1.4f, y);
    glVertex2f(x+1.4f, y+0.25f);
    glVertex2f(x-1.4f, y+0.25f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x-1.4f, y+0.25f);
    glVertex2f(x+1.4f, y+0.25f);
    glVertex2f(x+1.4f, y+0.9f);
    glVertex2f(x-1.4f, y+0.9f);
    glEnd();
    glColor3ub((unsigned char)(50*dim), (unsigned char)(50*dim), (unsigned char)(50*dim));
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(x-1.2f, y); glVertex2f(x-1.2f, y-0.4f);
    glVertex2f(x+1.2f, y); glVertex2f(x+1.2f, y-0.4f);
    glEnd();
}
void DrawSeatedPerson(float x)

{
    float lf  = getLightFactor();
    float dim = 1.0f - 0.55f * lf;
    float y = -9.3f;
    glColor3ub((unsigned char)(60*dim), (unsigned char)(60*dim), (unsigned char)(110*dim));
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex2f(x-0.15f, y+0.25f); glVertex2f(x-0.15f, y+0.05f);
    glVertex2f(x-0.15f, y+0.05f); glVertex2f(x+0.15f, y+0.02f);
    glVertex2f(x+0.15f, y+0.25f); glVertex2f(x+0.15f, y+0.05f);
    glVertex2f(x+0.15f, y+0.05f); glVertex2f(x+0.35f, y+0.02f);
    glEnd();
    glColor3ub((unsigned char)(200*dim), (unsigned char)(90*dim), (unsigned char)(70*dim));
    glBegin(GL_QUADS);
    glVertex2f(x-0.18f, y+0.25f);
    glVertex2f(x+0.18f, y+0.25f);
    glVertex2f(x+0.18f, y+0.65f);
    glVertex2f(x-0.18f, y+0.65f);
    glEnd();
    glColor3ub((unsigned char)(225*dim), (unsigned char)(180*dim), (unsigned char)(135*dim));
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(x-0.18f, y+0.5f); glVertex2f(x-0.05f, y+0.28f);
    glVertex2f(x+0.18f, y+0.5f); glVertex2f(x+0.20f, y+0.28f);
    glEnd();
    circle(0.16f, x, y+0.85f, (unsigned char)(225*dim), (unsigned char)(180*dim), (unsigned char)(135*dim), 255);
    if (lf > 0.3f) {
        circle(0.22f, x, y+0.85f, 255, 200, 100, (unsigned char)(20 * lf));
    }
}

// Renders a single seagull with flapping wings
void DrawSeagull(const Seagull& g)
{
    float lf = getLightFactor();
    float dim = 1.0f - 0.45f * lf;
    glPushMatrix();
    glTranslatef(g.x, g.y, 0.0f);
    glScalef(g.scale, g.scale, 1.0f);
    if (g.dir < 0) glScalef(-1.0f, 1.0f, 1.0f);
    float wingOffset = 0.45f * sin(g.wingAngle);
    circle(0.25f, 0.0f, 0.0f, (unsigned char)(240*dim), (unsigned char)(240*dim), (unsigned char)(245*dim), 255);
    circle(0.16f, 0.25f, 0.05f, (unsigned char)(250*dim), (unsigned char)(250*dim), (unsigned char)(255*dim), 255);
    glColor3ub((unsigned char)(255*dim), (unsigned char)(190*dim), 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.38f, 0.08f);
    glVertex2f(0.55f, 0.03f);
    glVertex2f(0.38f, 0.0f);
    glEnd();
    glLineWidth(2.5f);
    glColor3ub((unsigned char)(230*dim), (unsigned char)(230*dim), (unsigned char)(235*dim));
    glBegin(GL_LINE_STRIP);
    glVertex2f(0.0f, 0.05f);
    glVertex2f(-0.5f, 0.3f + wingOffset);
    glVertex2f(-1.1f, 0.1f + wingOffset * 1.2f);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2f(0.0f, 0.05f);
    glVertex2f(0.5f, 0.3f + wingOffset);
    glVertex2f(1.1f, 0.1f + wingOffset * 1.2f);
    glEnd();
    glColor3ub((unsigned char)(70*dim), (unsigned char)(70*dim), (unsigned char)(80*dim));
    glBegin(GL_LINES);
    glVertex2f(-0.8f, 0.18f + wingOffset * 1.1f);
    glVertex2f(-1.1f, 0.1f + wingOffset * 1.2f);
    glVertex2f(0.8f, 0.18f + wingOffset * 1.1f);
    glVertex2f(1.1f, 0.1f + wingOffset * 1.2f);
    glEnd();
    glColor3ub((unsigned char)(220*dim), (unsigned char)(220*dim), (unsigned char)(225*dim));
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.2f, 0.02f);
    glVertex2f(-0.55f, -0.05f);
    glVertex2f(-0.45f, 0.1f);
    glEnd();
    glPopMatrix();
}

// Exhaust smoke emission and rendering
void EmitExhaustSmoke(float x, float y, float intensity)
{
    for (int i = 0; i < MAX_EXHAUST; i++) {
        if (!exhaustParticles[i].active) {
            exhaustParticles[i].active = true;
            exhaustParticles[i].x = x;
            exhaustParticles[i].y = y;
            exhaustParticles[i].vx = -0.06f - (rand() % 10) * 0.004f;
            exhaustParticles[i].vy = 0.015f + (rand() % 10) * 0.003f;
            exhaustParticles[i].size = 0.25f + (rand() % 10) * 0.015f;
            exhaustParticles[i].alpha = (0.35f + 0.25f * intensity);
            exhaustParticles[i].life = 25 + (rand() % 15);
            break;
        }
    }
}
// Renders expanding, fading exhaust smoke particles behind moving vehicles
void DrawExhaustSmoke()
{
    float lf = getLightFactor();
    unsigned char r = (unsigned char)(160.0f * (1.0f - 0.5f * lf));
    unsigned char g = (unsigned char)(160.0f * (1.0f - 0.5f * lf));
    unsigned char b = (unsigned char)(170.0f * (1.0f - 0.5f * lf));
    for (int i = 0; i < MAX_EXHAUST; i++) {
        if (exhaustParticles[i].active) {
            float a = exhaustParticles[i].alpha * 255.0f;
            if (a > 255.0f) a = 255.0f;
            if (a < 0.0f) a = 0.0f;
            circle(exhaustParticles[i].size, exhaustParticles[i].x, exhaustParticles[i].y, r, g, b, (unsigned char)a);
        }
    }
}

// Glass elevator with translucent cabin and guide cables
void DrawGlassElevator()
{
    float lf = getLightFactor();
    glColor3ub(70, 70, 80);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-20.9f, -8.0f);
    glVertex2f(-20.9f, 12.0f);
    glVertex2f(-19.7f, -8.0f);
    glVertex2f(-19.7f, 12.0f);
    glEnd();
    circle(0.35f, -20.3f, 12.1f, 100, 100, 110, 255);
    circle(0.35f, -20.3f, -7.8f, 100, 100, 110, 255);
    float cabinX = -20.9f;
    float cabinY = elevatorY;
    float cabinW = 1.3f;
    float cabinH = 2.2f;
    circle(0.45f, cabinX + cabinW * 0.5f, cabinY + cabinH - 0.3f, 255, 235, 130, 255);
    if (lf > 0.01f) {
        glColor4ub(255, 220, 110, (unsigned char)(110 * lf));
        glBegin(GL_TRIANGLES);
        glVertex2f(cabinX + cabinW * 0.5f, cabinY + cabinH - 0.3f);
        glVertex2f(cabinX - 0.6f, cabinY - 1.5f);
        glVertex2f(cabinX + cabinW + 0.6f, cabinY - 1.5f);
        glEnd();
    }
    glColor4ub(180, 230, 255, 170);
    glBegin(GL_QUADS);
    glVertex2f(cabinX, cabinY);
    glVertex2f(cabinX + cabinW, cabinY);
    glVertex2f(cabinX + cabinW, cabinY + cabinH);
    glVertex2f(cabinX, cabinY + cabinH);
    glEnd();
    glColor3ub(40, 45, 55);
    glBegin(GL_QUADS);
    glVertex2f(cabinX - 0.1f, cabinY - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY + 0.2f);
    glVertex2f(cabinX - 0.1f, cabinY + 0.2f);
    glVertex2f(cabinX - 0.1f, cabinY + cabinH - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY + cabinH - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY + cabinH + 0.2f);
    glVertex2f(cabinX - 0.1f, cabinY + cabinH + 0.2f);
    glEnd();
    glColor4ub(255, 255, 255, (unsigned char)(160 * (1.0f - 0.5f * lf)));
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(cabinX + 0.2f, cabinY + 0.4f);
    glVertex2f(cabinX + cabinW - 0.3f, cabinY + cabinH - 0.4f);
    glEnd();
    if (elevatorState == 0)      circle(0.12f, cabinX + cabinW * 0.5f, cabinY + cabinH, 30, 255, 30, 255);
    else if (elevatorState == 2) circle(0.12f, cabinX + cabinW * 0.5f, cabinY + cabinH, 255, 30, 30, 255);
    else                         circle(0.12f, cabinX + cabinW * 0.5f, cabinY + cabinH, 255, 200, 0, 255);
}
// Renders illuminated 'HOTEL' neon signage with pulsating outline glow
void DrawHotelNeonSign(float x, float y)
{
    float pulse = 0.5f + 0.5f * sin(beaconPulse * 6.0f);
    float lf = getLightFactor();
    glColor3ub(20, 20, 25);
    glBegin(GL_QUADS);
    glVertex2f(x - 0.2f, y - 0.2f);
    glVertex2f(x + 4.4f, y - 0.2f);
    glVertex2f(x + 4.4f, y + 1.2f);
    glVertex2f(x - 0.2f, y + 1.2f);
    glEnd();
    glColor4ub(255, 0, 180, (unsigned char)((70 + 70 * pulse) * (0.3f + 0.7f * lf)));
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.3f, y - 0.3f);
    glVertex2f(x + 4.5f, y - 0.3f);
    glVertex2f(x + 4.5f, y + 1.3f);
    glVertex2f(x - 0.3f, y + 1.3f);
    glEnd();
    glColor3ub((unsigned char)(0.0f + 255.0f * pulse), 240, 255);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(x+0.2f, y+0.1f); glVertex2f(x+0.2f, y+0.9f);
    glVertex2f(x+0.7f, y+0.1f); glVertex2f(x+0.7f, y+0.9f);
    glVertex2f(x+0.2f, y+0.5f); glVertex2f(x+0.7f, y+0.5f);
    glVertex2f(x+1.0f, y+0.1f); glVertex2f(x+1.5f, y+0.1f);
    glVertex2f(x+1.5f, y+0.1f); glVertex2f(x+1.5f, y+0.9f);
    glVertex2f(x+1.5f, y+0.9f); glVertex2f(x+1.0f, y+0.9f);
    glVertex2f(x+1.0f, y+0.9f); glVertex2f(x+1.0f, y+0.1f);
    glVertex2f(x+1.8f, y+0.9f); glVertex2f(x+2.4f, y+0.9f);
    glVertex2f(x+2.1f, y+0.9f); glVertex2f(x+2.1f, y+0.1f);
    glVertex2f(x+2.7f, y+0.1f); glVertex2f(x+2.7f, y+0.9f);
    glVertex2f(x+2.7f, y+0.9f); glVertex2f(x+3.2f, y+0.9f);
    glVertex2f(x+2.7f, y+0.5f); glVertex2f(x+3.1f, y+0.5f);
    glVertex2f(x+2.7f, y+0.1f); glVertex2f(x+3.2f, y+0.1f);
    glVertex2f(x+3.5f, y+0.9f); glVertex2f(x+3.5f, y+0.1f);
    glVertex2f(x+3.5f, y+0.1f); glVertex2f(x+4.0f, y+0.1f);
    glEnd();
}
// Renders 'COFFEE' neon sign with animated curling steam curves above cup
void DrawCoffeeNeonSign(float x, float y)
{
    float pulse = 0.5f + 0.5f * sin(beaconPulse * 7.0f);
    float lf = getLightFactor();
    glColor3ub(25, 15, 10);
    glBegin(GL_QUADS);
    glVertex2f(x - 0.2f, y - 0.2f);
    glVertex2f(x + 4.8f, y - 0.2f);
    glVertex2f(x + 4.8f, y + 1.2f);
    glVertex2f(x - 0.2f, y + 1.2f);
    glEnd();
    glColor4ub(255, 160, 0, (unsigned char)((80 + 60 * pulse) * (0.3f + 0.7f * lf)));
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.3f, y - 0.3f);
    glVertex2f(x + 4.9f, y - 0.3f);
    glVertex2f(x + 4.9f, y + 1.3f);
    glVertex2f(x - 0.3f, y + 1.3f);
    glEnd();
    glColor3ub(255, (unsigned char)(140 + 80 * pulse), 30);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(x+0.7f, y+0.9f); glVertex2f(x+0.2f, y+0.9f);
    glVertex2f(x+0.2f, y+0.9f); glVertex2f(x+0.2f, y+0.1f);
    glVertex2f(x+0.2f, y+0.1f); glVertex2f(x+0.7f, y+0.1f);
    glVertex2f(x+1.0f, y+0.1f); glVertex2f(x+1.5f, y+0.1f);
    glVertex2f(x+1.5f, y+0.1f); glVertex2f(x+1.5f, y+0.9f);
    glVertex2f(x+1.5f, y+0.9f); glVertex2f(x+1.0f, y+0.9f);
    glVertex2f(x+1.0f, y+0.9f); glVertex2f(x+1.0f, y+0.1f);
    glVertex2f(x+1.8f, y+0.1f); glVertex2f(x+1.8f, y+0.9f);
    glVertex2f(x+1.8f, y+0.9f); glVertex2f(x+2.3f, y+0.9f);
    glVertex2f(x+1.8f, y+0.5f); glVertex2f(x+2.2f, y+0.5f);
    glVertex2f(x+2.5f, y+0.1f); glVertex2f(x+2.5f, y+0.9f);
    glVertex2f(x+2.5f, y+0.9f); glVertex2f(x+3.0f, y+0.9f);
    glVertex2f(x+2.5f, y+0.5f); glVertex2f(x+2.9f, y+0.5f);
    glVertex2f(x+3.2f, y+0.1f); glVertex2f(x+3.2f, y+0.9f);
    glVertex2f(x+3.2f, y+0.9f); glVertex2f(x+3.7f, y+0.9f);
    glVertex2f(x+3.2f, y+0.5f); glVertex2f(x+3.6f, y+0.5f);
    glVertex2f(x+3.2f, y+0.1f); glVertex2f(x+3.7f, y+0.1f);
    glVertex2f(x+3.9f, y+0.1f); glVertex2f(x+3.9f, y+0.9f);
    glVertex2f(x+3.9f, y+0.9f); glVertex2f(x+4.4f, y+0.9f);
    glVertex2f(x+3.9f, y+0.5f); glVertex2f(x+4.3f, y+0.5f);
    glVertex2f(x+3.9f, y+0.1f); glVertex2f(x+4.4f, y+0.1f);
    glEnd();
    glColor4ub(255, 230, 180, (unsigned char)(180 * pulse));
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    float steamS = sin(waveMove * 8.0f);
    glVertex2f(x - 0.6f, y + 0.1f);
    glVertex2f(x - 0.6f + 0.15f * steamS, y + 0.6f);
    glVertex2f(x - 0.6f - 0.15f * steamS, y + 1.1f);
    glEnd();
}
// Renders 'CINEMA' neon marquee with animated chasing bulb border
void DrawCinemaNeonSign(float x, float y)
{
    float lf = getLightFactor();
    float pulse = 0.5f + 0.5f * sin(beaconPulse * 8.0f);
    glColor3ub(15, 15, 25);
    glBegin(GL_QUADS);
    glVertex2f(x - 0.2f, y - 0.2f);
    glVertex2f(x + 5.2f, y - 0.2f);
    glVertex2f(x + 5.2f, y + 1.2f);
    glVertex2f(x - 0.2f, y + 1.2f);
    glEnd();
    glColor4ub(30, 255, 120, (unsigned char)((70 + 70 * pulse) * (0.3f + 0.7f * lf)));
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.3f, y - 0.3f);
    glVertex2f(x + 5.3f, y - 0.3f);
    glVertex2f(x + 5.3f, y + 1.3f);
    glVertex2f(x - 0.3f, y + 1.3f);
    glEnd();
    int chaseIndex = (int)(beaconPulse * 12.0f) % 8;
    for (int b = 0; b < 10; b++) {
        float bx = x - 0.1f + b * 0.54f;
        bool isOn = ((b + chaseIndex) % 2 == 0);
        if (isOn) circle(0.12f, bx, y + 1.15f, 255, 220, 50, 255);
        else      circle(0.12f, bx, y + 1.15f, 100, 80, 20, 255);
        if (isOn) circle(0.12f, bx, y - 0.15f, 255, 220, 50, 255);
        else      circle(0.12f, bx, y - 0.15f, 100, 80, 20, 255);
    }
    glColor3ub(30, 255, 120);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(x+0.7f, y+0.8f); glVertex2f(x+0.2f, y+0.8f);
    glVertex2f(x+0.2f, y+0.8f); glVertex2f(x+0.2f, y+0.2f);
    glVertex2f(x+0.2f, y+0.2f); glVertex2f(x+0.7f, y+0.2f);
    glVertex2f(x+1.1f, y+0.8f); glVertex2f(x+1.1f, y+0.2f);
    glVertex2f(x+1.5f, y+0.2f); glVertex2f(x+1.5f, y+0.8f);
    glVertex2f(x+1.5f, y+0.8f); glVertex2f(x+2.1f, y+0.2f);
    glVertex2f(x+2.1f, y+0.2f); glVertex2f(x+2.1f, y+0.8f);
    glVertex2f(x+2.5f, y+0.2f); glVertex2f(x+2.5f, y+0.8f);
    glVertex2f(x+2.5f, y+0.8f); glVertex2f(x+3.0f, y+0.8f);
    glVertex2f(x+2.5f, y+0.5f); glVertex2f(x+2.9f, y+0.5f);
    glVertex2f(x+2.5f, y+0.2f); glVertex2f(x+3.0f, y+0.2f);
    glVertex2f(x+3.3f, y+0.2f); glVertex2f(x+3.3f, y+0.8f);
    glVertex2f(x+3.3f, y+0.8f); glVertex2f(x+3.7f, y+0.4f);
    glVertex2f(x+3.7f, y+0.4f); glVertex2f(x+4.1f, y+0.8f);
    glVertex2f(x+4.1f, y+0.8f); glVertex2f(x+4.1f, y+0.2f);
    glVertex2f(x+4.4f, y+0.2f); glVertex2f(x+4.7f, y+0.8f);
    glVertex2f(x+4.7f, y+0.8f); glVertex2f(x+5.0f, y+0.2f);
    glVertex2f(x+4.5f, y+0.5f); glVertex2f(x+4.9f, y+0.5f);
    glEnd();
}
// Renders a single digit on a 7-segment digital display using GL_LINES
void Draw7SegmentDigit(float x, float y, float size, int digit)
{
    bool a = false, b = false, c = false, d = false, e = false, f = false, g = false;
    switch (digit) {
        case 0: a=1; b=1; c=1; d=1; e=1; f=1; g=0; break;
        case 1: a=0; b=1; c=1; d=0; e=0; f=0; g=0; break;
        case 2: a=1; b=1; c=0; d=1; e=1; f=0; g=1; break;
        case 3: a=1; b=1; c=1; d=1; e=0; f=0; g=1; break;
        case 4: a=0; b=1; c=1; d=0; e=0; f=1; g=1; break;
        case 5: a=1; b=0; c=1; d=1; e=0; f=1; g=1; break;
        case 6: a=1; b=0; c=1; d=1; e=1; f=1; g=1; break;
        case 7: a=1; b=1; c=1; d=0; e=0; f=0; g=0; break;
        case 8: a=1; b=1; c=1; d=1; e=1; f=1; g=1; break;
        case 9: a=1; b=1; c=1; d=1; e=0; f=1; g=1; break;
    }
    float w = size * 0.5f;
    float h = size;
    glBegin(GL_LINES);
    if (a) { glVertex2f(x, y + h); glVertex2f(x + w, y + h); }
    if (b) { glVertex2f(x + w, y + h/2.0f); glVertex2f(x + w, y + h); }
    if (c) { glVertex2f(x + w, y); glVertex2f(x + w, y + h/2.0f); }
    if (d) { glVertex2f(x, y); glVertex2f(x + w, y); }
    if (e) { glVertex2f(x, y); glVertex2f(x, y + h/2.0f); }
    if (f) { glVertex2f(x, y + h/2.0f); glVertex2f(x, y + h); }
    if (g) { glVertex2f(x, y + h/2.0f); glVertex2f(x + w, y + h/2.0f); }
    glEnd();
}
// Multi-slide roadside electronic billboard cycling between brand ad, digital clock, and weather
void Billboard()

{
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(35.0, -10.0);
    glVertex2f(35.5, -10.0);
    glVertex2f(35.5, -2.0);
    glVertex2f(35.0, -2.0);
    glEnd();
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(40.5, -10.0);
    glVertex2f(41.0, -10.0);
    glVertex2f(41.0, -2.0);
    glVertex2f(40.5, -2.0);
    glEnd();
    glColor3ub(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(34.5, -2.0);
    glVertex2f(41.5, -2.0);
    glVertex2f(41.5, 2.0);
    glVertex2f(34.5, 2.0);
    glEnd();
    if (activeAdSlide == 0) {
        glColor3ub(15, 15, 25);
    } else if (activeAdSlide == 1) {
        glColor3ub(5, 10, 5);
    } else {
        glColor3ub(35, 15, 0);
    }
    glBegin(GL_QUADS);
    glVertex2f(35.0, -1.8);
    glVertex2f(41.0, -1.8);
    glVertex2f(41.0, 1.8);
    glVertex2f(35.0, 1.8);
    glEnd();
    if (activeAdSlide == 0) {
        float neonPulseVal = 0.5f + 0.5f * sin(beaconPulse * 8.0f);
        glColor4ub(255, 30, 180, (unsigned char)(60 + 60 * neonPulseVal));
        glBegin(GL_TRIANGLES);
        glVertex2f(38.0f, 1.1f);
        glVertex2f(36.2f, -0.7f);
        glVertex2f(39.8f, -0.7f);
        glEnd();
        glColor4ub(255, 50, 200, (unsigned char)(180 + 75 * neonPulseVal));
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        glVertex2f(38.0f, 1.1f);
        glVertex2f(36.2f, -0.7f);
        glVertex2f(39.8f, -0.7f);
        glEnd();
        glColor4ub(30, 240, 255, 255);
        glBegin(GL_TRIANGLES);
        glVertex2f(38.0f, 0.7f);
        glVertex2f(36.7f, -0.5f);
        glVertex2f(39.3f, -0.5f);
        glEnd();
        glColor4ub(30, 220, 255, (unsigned char)(100 + 155 * neonPulseVal));
        glBegin(GL_LINE_LOOP);
        glVertex2f(36.0f, -0.9f);
        glVertex2f(40.0f, -0.9f);
        glVertex2f(39.5f, -1.3f);
        glVertex2f(36.5f, -1.3f);
        glEnd();
    }
    else if (activeAdSlide == 1) {
        float rawHours = timeOfDay * 24.0f + 12.0f;
        int hour = ((int)rawHours) % 24;
        int minute = (int)(fmod(rawHours * 60.0f, 60.0f));
        int h1 = hour / 10;
        int h2 = hour % 10;
        int m1 = minute / 10;
        int m2 = minute % 10;
        glColor3ub(50, 255, 50);
        glLineWidth(3);
        Draw7SegmentDigit(35.5f, -0.5f, 1.0f, h1);
        Draw7SegmentDigit(36.5f, -0.5f, 1.0f, h2);
        if (fmod(beaconPulse * 4.0f, 2.0f) < 1.0f) {
            circle(0.1f, 37.75f, 0.2f, 50, 255, 50, 255);
            circle(0.1f, 37.75f, -0.2f, 50, 255, 50, 255);
        }
        Draw7SegmentDigit(38.3f, -0.5f, 1.0f, m1);
        Draw7SegmentDigit(39.3f, -0.5f, 1.0f, m2);
    }
    else {
        if (weatherMode == 0) {
            circle(0.4f, 36.3f, 0.5f, 255, 210, 0, 255);
            glColor3ub(255, 210, 0);
            glLineWidth(2);
            glPushMatrix();
            glTranslatef(36.3f, 0.5f, 0.0f);
            glRotatef(waveMove * 20.0f, 0.0f, 0.0f, 1.0f);
            glBegin(GL_LINES);
            for (int r = 0; r < 8; r++) {
                float angle = r * 45.0f * 3.14159f / 180.0f;
                glVertex2f(0.5f * cos(angle), 0.5f * sin(angle));
                glVertex2f(0.8f * cos(angle), 0.8f * sin(angle));
            }
            glEnd();
            glPopMatrix();
        } else if (weatherMode == 1) {
            circle(0.35f, 36.0f, 0.6f, 140, 150, 160, 255);
            circle(0.35f, 36.6f, 0.6f, 140, 150, 160, 255);
            circle(0.4f, 36.3f, 0.8f, 140, 150, 160, 255);
            glColor3ub(100, 180, 255);
            glLineWidth(2);
            glBegin(GL_LINES);
            float dropOffset = fmod(waveMove * 10.0f, 0.6f);
            glVertex2f(36.0f, 0.3f - dropOffset);
            glVertex2f(36.0f, 0.15f - dropOffset);
            glVertex2f(36.3f, 0.2f - dropOffset);
            glVertex2f(36.3f, 0.05f - dropOffset);
            glVertex2f(36.6f, 0.3f - dropOffset);
            glVertex2f(36.6f, 0.15f - dropOffset);
            glEnd();
        } else {
            glColor3ub(255, 255, 255);
            glLineWidth(2);
            glPushMatrix();
            glTranslatef(36.3f, 0.5f, 0.0f);
            glRotatef(waveMove * 5.0f, 0.0f, 0.0f, 1.0f);
            glBegin(GL_LINES);
            glVertex2f(-0.6f, 0.0f); glVertex2f(0.6f, 0.0f);
            glVertex2f(0.0f, -0.6f); glVertex2f(0.0f, 0.6f);
            glVertex2f(-0.4f, -0.4f); glVertex2f(0.4f, 0.4f);
            glVertex2f(-0.4f, 0.4f); glVertex2f(0.4f, -0.4f);
            glEnd();
            glPopMatrix();
        }
        glColor3ub(10, 5, 0);
        glBegin(GL_QUADS);
        glVertex2f(35.1f, -1.4f);
        glVertex2f(40.9f, -1.4f);
        glVertex2f(40.9f, -0.5f);
        glVertex2f(35.1f, -0.5f);
        glEnd();
        float scrollX = 35.1f + fmod(waveMove * 8.0f, 6.0f);
        glColor3ub(255, 140, 0);
        glLineWidth(2);
        for (int i = 0; i < 4; i++) {
            float xOffset = scrollX - i * 1.5f;
            if (xOffset < 35.1f) xOffset += 5.8f;
            if (xOffset >= 35.3f && xOffset <= 40.7f) {
                glBegin(GL_LINE_STRIP);
                glVertex2f(xOffset - 0.2f, -0.7f);
                glVertex2f(xOffset, -0.95f);
                glVertex2f(xOffset - 0.2f, -1.2f);
                glEnd();
            }
        }
    }
}
// Renders railway track along the ground with ballast bed, ties (sleepers), and twin steel rails
void RailwayTrack()

{
    glColor3ub(90, 90, 95);
    glBegin(GL_QUADS);
    glVertex2f(-60, -8.7f);
    glVertex2f(60, -8.7f);
    glVertex2f(60, -7.5f);
    glVertex2f(-60, -7.5f);
    glEnd();
    glColor3ub(70, 40, 20);
    glLineWidth(2);
    glBegin(GL_LINES);
    for (int i = -60; i < 60; i += 3) {
        glVertex2f(i, -8.6f);
        glVertex2f(i, -7.6f);
    }
    glEnd();
    glColor3ub(210, 210, 215);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(-60, -7.8f);
    glVertex2f(60, -7.8f);
    glVertex2f(-60, -8.3f);
    glVertex2f(60, -8.3f);
    glEnd();
}
// Renders left coastguard watchtower on stilts with cabin, railing, windows, and chimney smoke
void WatchTower1()

{
    glColor3ub(60, 60, 60);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(-11, 5);
    glVertex2f(-9, 17);
    glVertex2f(-5, 5);
    glVertex2f(-7, 17);
    glEnd();
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-11, 5);
    glVertex2f(-6, 11);
    glVertex2f(-5, 5);
    glVertex2f(-10, 11);
    glVertex2f(-10, 11);
    glVertex2f(-7, 17);
    glVertex2f(-6, 11);
    glVertex2f(-9, 17);
    glEnd();
    glColor3ub(80, 50, 20);
    glBegin(GL_QUADS);
    glVertex2f(-12, 17);
    glVertex2f(-4, 17);
    glVertex2f(-4, 18);
    glVertex2f(-12, 18);
    glEnd();
    glColor3ub(139, 69, 19);
    glBegin(GL_QUADS);
    glVertex2f(-10.5, 18);
    glVertex2f(-5.5, 18);
    glVertex2f(-5.5, 23);
    glVertex2f(-10.5, 23);
    glEnd();
    glColor3ub(60, 30, 10);
    glBegin(GL_QUADS);
    glVertex2f(-8.8, 18);
    glVertex2f(-7.2, 18);
    glVertex2f(-7.2, 21.5);
    glVertex2f(-8.8, 21.5);
    glEnd();
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(135, 206, 235);
    glBegin(GL_QUADS);
    glVertex2f(-10.2, 20);
    glVertex2f(-9.0, 20);
    glVertex2f(-9.0, 21.5);
    glVertex2f(-10.2, 21.5);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-7.0, 20);
    glVertex2f(-5.8, 20);
    glVertex2f(-5.8, 21.5);
    glVertex2f(-7.0, 21.5);
    glEnd();
    glColor3ub(100, 0, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(-11.5, 23);
    glVertex2f(-4.5, 23);
    glVertex2f(-8, 26.5);
    glEnd();
    glColor3ub(40, 40, 40);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-11.8, 18);
    glVertex2f(-11.8, 19.5);
    glVertex2f(-4.2, 18);
    glVertex2f(-4.2, 19.5);
    glVertex2f(-12, 19.5);
    glVertex2f(-4, 19.5);
    glEnd();
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(-10.4f, 23.5f);
    glVertex2f(-9.7f, 23.5f);
    glVertex2f(-9.7f, 26.0f);
    glVertex2f(-10.4f, 26.0f);
    glEnd();
    if (isAnimating) {
        float smokeX = -10.05f;
        float smokeY = 26.0f;
        float windSpeed = (weatherMode == 1) ? -4.5f : ((weatherMode == 2) ? -3.0f : 1.2f);
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 10.0f + i * 2.0f;
            float life = fmod(t, 6.0f) / 6.0f;
            float xOffset = windSpeed * life + 0.25f * sin(t * 1.8f);
            float yOffset = 2.4f * life + 0.1f * cos(t * 1.8f);
            float scale = 0.2f + 0.6f * life;
            int grey = (weatherMode == 1) ? 140 : 210;
            unsigned char alpha = (unsigned char)(130.0f * (1.0f - life));
            circle(scale, smokeX + xOffset, smokeY + yOffset, grey, grey, grey + 5, alpha);
        }
    }
}
// Renders right coastguard watchtower with observation deck and weather-responsive chimney smoke
void WatchTower2()

{
    glColor3ub(60, 60, 60);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(30, 1);
    glVertex2f(32, 13);
    glVertex2f(36, 1);
    glVertex2f(34, 13);
    glEnd();
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(30, 1);
    glVertex2f(35, 7);
    glVertex2f(36, 1);
    glVertex2f(31, 7);
    glVertex2f(31, 7);
    glVertex2f(34, 13);
    glVertex2f(35, 7);
    glVertex2f(32, 13);
    glEnd();
    glColor3ub(80, 50, 20);
    glBegin(GL_QUADS);
    glVertex2f(29, 13);
    glVertex2f(37, 13);
    glVertex2f(37, 14);
    glVertex2f(29, 14);
    glEnd();
    glColor3ub(139, 69, 19);
    glBegin(GL_QUADS);
    glVertex2f(30.5, 14);
    glVertex2f(35.5, 14);
    glVertex2f(35.5, 19);
    glVertex2f(30.5, 19);
    glEnd();
    glColor3ub(60, 30, 10);
    glBegin(GL_QUADS);
    glVertex2f(32.2, 14);
    glVertex2f(33.8, 14);
    glVertex2f(33.8, 17.5);
    glVertex2f(32.2, 17.5);
    glEnd();
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(135, 206, 235);
    glBegin(GL_QUADS);
    glVertex2f(30.8, 16);
    glVertex2f(32.0, 16);
    glVertex2f(32.0, 17.5);
    glVertex2f(30.8, 17.5);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(34.0, 16);
    glVertex2f(35.2, 16);
    glVertex2f(35.2, 17.5);
    glVertex2f(34.0, 17.5);
    glEnd();
    glColor3ub(100, 0, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(29.5, 19);
    glVertex2f(36.5, 19);
    glVertex2f(33, 22.5);
    glEnd();
    glColor3ub(40, 40, 40);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(29.2, 14);
    glVertex2f(29.2, 15.5);
    glVertex2f(36.8, 14);
    glVertex2f(36.8, 15.5);
    glVertex2f(29, 15.5);
    glVertex2f(37, 15.5);
    glEnd();
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(30.1f, 19.5f);
    glVertex2f(30.8f, 19.5f);
    glVertex2f(30.8f, 22.0f);
    glVertex2f(30.1f, 22.0f);
    glEnd();
    if (isAnimating) {
        float smokeX = 30.45f;
        float smokeY = 22.0f;
        float windSpeed = (weatherMode == 1) ? -4.5f : ((weatherMode == 2) ? -3.0f : 1.2f);
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 10.0f + i * 2.0f;
            float life = fmod(t, 6.0f) / 6.0f;
            float xOffset = windSpeed * life + 0.25f * sin(t * 1.8f);
            float yOffset = 2.4f * life + 0.1f * cos(t * 1.8f);
            float scale = 0.2f + 0.6f * life;
            int grey = (weatherMode == 1) ? 140 : 210;
            unsigned char alpha = (unsigned char)(130.0f * (1.0f - life));
            circle(scale, smokeX + xOffset, smokeY + yOffset, grey, grey, grey + 5, alpha);
        }
    }
}
// Coastal lighthouse on rock foundation with twin sweeping rotating volumetric light beams
void Lighthouse()

{
    float lf = getLightFactor();
    glColor3ub(90, 85, 80);
    glBegin(GL_TRIANGLES);
    glVertex2f(52.0f, 0.0f);
    glVertex2f(62.0f, 0.0f);
    glVertex2f(57.0f, 3.0f);
    glEnd();
    glColor3ub(235, 235, 230);
    glBegin(GL_QUADS);
    glVertex2f(55.6f, 3.0f);
    glVertex2f(58.4f, 3.0f);
    glVertex2f(57.9f, 16.0f);
    glVertex2f(56.1f, 16.0f);
    glEnd();
    glColor3ub(180, 40, 40);
    glBegin(GL_QUADS);
    glVertex2f(55.9f, 6.5f);
    glVertex2f(58.1f, 6.5f);
    glVertex2f(58.0f, 8.5f);
    glVertex2f(56.0f, 8.5f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(56.05f, 11.5f);
    glVertex2f(57.95f, 11.5f);
    glVertex2f(57.85f, 13.5f);
    glVertex2f(56.15f, 13.5f);
    glEnd();
    glColor3ub(60, 60, 65);
    glBegin(GL_QUADS);
    glVertex2f(55.6f, 16.0f);
    glVertex2f(58.4f, 16.0f);
    glVertex2f(58.4f, 16.6f);
    glVertex2f(55.6f, 16.6f);
    glEnd();
    glColor3ub(40, 40, 45);
    glBegin(GL_QUADS);
    glVertex2f(56.1f, 16.6f);
    glVertex2f(57.9f, 16.6f);
    glVertex2f(57.9f, 18.6f);
    glVertex2f(56.1f, 18.6f);
    glEnd();
    float glow = 0.4f + 0.6f * lf;
    glColor3ub((unsigned char)(255*glow), (unsigned char)(240*glow), (unsigned char)(180*glow));
    glBegin(GL_QUADS);
    glVertex2f(56.3f, 16.8f);
    glVertex2f(57.7f, 16.8f);
    glVertex2f(57.7f, 18.4f);
    glVertex2f(56.3f, 18.4f);
    glEnd();
    glColor3ub(150, 30, 30);
    glBegin(GL_TRIANGLES);
    glVertex2f(55.9f, 18.6f);
    glVertex2f(58.1f, 18.6f);
    glVertex2f(57.0f, 20.2f);
    glEnd();
    if (lf > 0.1f) {
        float beamAngle = waveMove * 0.5f;
        float dx = cos(beamAngle);
        float dy = sin(beamAngle);
        unsigned char a = (unsigned char)(140 * lf);
        DrawLightCone(57.0f, 17.6f,  dx,  dy, 34.0f, 5.5f, 255, 250, 195, a);
        DrawLightCone(57.0f, 17.6f, -dx, -dy, 34.0f, 5.5f, 255, 250, 195, a);
    }
}
// Spawns a new precipitation particle (rain drop or snow flake) with randomized coordinates
void addDrop()
{
    if (totalDrops < MAX_DROPS) {
        dropX[totalDrops] = (rand() % 120) - 60.0f;
        dropY[totalDrops] = 40.0f;
        dropTargetY[totalDrops] = -24.0f - (rand() % 16);
        totalDrops++;
    }
}
// Timer callback: simulates rain/snow physics, water splashes, snow accumulation, and lightning generation
void updateRain(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, updateRain, 0); return; }
    if (weatherMode == 1 || weatherMode == 2) {
        float fallSpeed = (weatherMode == 2) ? 0.22f : 1.0f;
        for (int i = 0; i < totalDrops; i++) {
            dropY[i] -= fallSpeed;
            if (weatherMode == 2) {
                dropX[i] += 0.06f * sin(beaconPulse * 1.5f + i * 0.5f);
            }
            if (dropY[i] <= dropTargetY[i]) {
                if (weatherMode == 1 && bubbleCount < MAX_BUBBLES) {
                    bubbleX[bubbleCount] = dropX[i];
                    bubbleY[bubbleCount] = dropTargetY[i];
                    bubbleRadius[bubbleCount] = 0.2f;
                    bubbleAlpha[bubbleCount] = 255.0f;
                    bubbleActive[bubbleCount] = true;
                    bubbleCount++;
                }
                for (int j = i; j < totalDrops - 1; j++) {
                    dropX[j] = dropX[j + 1];
                    dropY[j] = dropY[j + 1];
                    dropTargetY[j] = dropTargetY[j + 1];
                }
                totalDrops--;
                i--;
            }
        }
        int dropsToCreate = (weatherMode == 2) ? 4 : 10;
        for (int i = 0; i < dropsToCreate; i++) {
            addDrop();
        }
    } else {
        totalDrops = 0;
    }
    int activeCount = 0;
    for (int i = 0; i < bubbleCount; i++) {
        if (bubbleActive[i]) {
            bubbleRadius[i] += 0.15f;
            bubbleAlpha[i] -= 10.0f;
            if (bubbleAlpha[i] > 0) {
                bubbleX[activeCount] = bubbleX[i];
                bubbleY[activeCount] = bubbleY[i];
                bubbleRadius[activeCount] = bubbleRadius[i];
                bubbleAlpha[activeCount] = bubbleAlpha[i];
                bubbleActive[activeCount] = true;
                activeCount++;
            }
        }
    }
    bubbleCount = activeCount;
    if (weatherMode == 2) {
        if (snowAccumulation < 1.2f) snowAccumulation += 0.0006f;
    } else if (weatherMode == 1) {
        if (snowAccumulation > 0.0f) snowAccumulation -= 0.004f;
        if (snowAccumulation < 0.0f) snowAccumulation = 0.0f;
    } else {
        if (snowAccumulation > 0.0f) snowAccumulation -= 0.0015f;
        if (snowAccumulation < 0.0f) snowAccumulation = 0.0f;
    }
    if (isAnimating) {
        fogPos += 0.05f;
        if (fogPos > 120.0f) fogPos = -120.0f;
    }
    if (weatherMode == 1 && isAnimating) {
        if (rand() % 160 == 0 && !lightningActive) {
            lightningActive = true;
            lightningIntensity = 1.0f;
            float startX = (rand() % 90) - 45.0f;
            float currentY = 40.0f;
            float currentX = startX;
            lightningPoints = 0;
            lightningX[lightningPoints] = currentX;
            lightningY[lightningPoints] = currentY;
            lightningPoints++;
            while (currentY > -10.0f && lightningPoints < 15) {
                currentY -= (3.0f + (rand() % 100) * 0.03f);
                currentX += ((rand() % 100) * 0.08f - 4.0f);
                lightningX[lightningPoints] = currentX;
                lightningY[lightningPoints] = currentY;
                lightningPoints++;
            }
        }
    }
    if (lightningActive) {
        lightningIntensity -= 0.08f;
        if (lightningIntensity <= 0.0f) {
            lightningIntensity = 0.0f;
            lightningActive = false;
            lightningPoints = 0;
        }
    }
    glutTimerFunc(25, updateRain, 0);
}
// Renders expanding circular ripple rings on water where raindrops splash
void DrawRipples()
{
    glLineWidth(2);
    for (int i = 0; i < bubbleCount; i++) {
        glBegin(GL_LINE_LOOP);
        glColor4ub(200, 200, 255, (unsigned char)bubbleAlpha[i]);
        for (int j = 0; j < 30; j++) {
            float theta = 2.0f * 3.1416f * float(j) / float(30);
            float rx = bubbleRadius[i] * cosf(theta);
            float ry = (bubbleRadius[i] * 0.2f) * sinf(theta);
            glVertex2f(bubbleX[i] + rx, bubbleY[i] + ry);
        }
        glEnd();
    }
}
// Renders falling precipitation streaks for rain or drifting quads for snow
void drawWeatherParticles()
{
    if (weatherMode == 1) {
        glLineWidth(1);
        glColor3ub(170, 200, 255);
        glBegin(GL_LINES);
        for (int i = 0; i < totalDrops; i++) {
            glVertex2f(dropX[i], dropY[i]);
            glVertex2f(dropX[i], dropY[i] - 1.5f);
        }
        glEnd();
    } else if (weatherMode == 2) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        for (int i = 0; i < totalDrops; i++) {
            float x = dropX[i];
            float y = dropY[i];
            float size = 0.2f;
            glVertex2f(x - size, y - size);
            glVertex2f(x + size, y - size);
            glVertex2f(x + size, y + size);
            glVertex2f(x - size, y + size);
        }
        glEnd();
    }
}
// Renders horizontal drifting atmospheric fog layers across the scene
void DrawFog()
{
    float intensity = 0.0f;
    if (weatherMode == 1) {
        intensity = stormFactor;
    } else if (weatherMode == 2) {
        intensity = 0.6f * (snowAccumulation / 1.2f) + 0.4f * stormFactor;
    } else {
        intensity = stormFactor;
    }
    if (intensity < 0.05f) return;
    glBegin(GL_QUADS);
    glColor4ub(200, 200, 205, (unsigned char)(60.0f * intensity));
    glVertex2f(-60.0f, -28.0f);
    glVertex2f(60.0f, -28.0f);
    glColor4ub(220, 220, 225, 0);
    glVertex2f(60.0f, -14.0f);
    glVertex2f(-60.0f, -14.0f);
    glEnd();
    float rFog = (weatherMode == 2) ? 240.0f : 200.0f;
    float gFog = (weatherMode == 2) ? 240.0f : 200.0f;
    float bFog = (weatherMode == 2) ? 245.0f : 205.0f;
    unsigned char alphaFog = (unsigned char)(28.0f * intensity);
    for (float xOffset = -100.0f; xOffset <= 100.0f; xOffset += 40.0f) {
        float x = xOffset + fogPos;
        if (x > 70.0f) x -= 140.0f;
        if (x < -70.0f) x += 140.0f;
        circle(8.0f, x, -20.0f + 2.0f * sin(xOffset), rFog, gFog, bFog, alphaFog);
        circle(6.0f, x + 5.0f, -18.0f + 1.0f * cos(xOffset), rFog, gFog, bFog, alphaFog);
    }
}
// Road intersection traffic signal pole with sun visors and cycling green/yellow/red lamps
void DrawTrafficLight()
{
    glColor3ub(60, 60, 60);
    glBegin(GL_QUADS);
    glVertex2f(4.7f, -12.0f);
    glVertex2f(5.3f, -12.0f);
    glVertex2f(5.3f, -4.0f);
    glVertex2f(4.7f, -4.0f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(0.0f, -4.3f);
    glVertex2f(5.0f, -4.3f);
    glVertex2f(5.0f, -3.7f);
    glVertex2f(0.0f, -3.7f);
    glEnd();
    glColor3ub(30, 30, 30);
    glBegin(GL_QUADS);
    glVertex2f(-0.6f, -6.5f);
    glVertex2f(0.6f, -6.5f);
    glVertex2f(0.6f, -3.8f);
    glVertex2f(-0.6f, -3.8f);
    glEnd();
    glColor3ub(15, 15, 15);
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    glVertex2f(-0.5f, -4.2f);
    glVertex2f(0.0f, -3.9f);
    glVertex2f(0.5f, -4.2f);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2f(-0.5f, -4.9f);
    glVertex2f(0.0f, -4.6f);
    glVertex2f(0.5f, -4.9f);
    glEnd();
    glBegin(GL_LINE_STRIP);
    glVertex2f(-0.5f, -5.6f);
    glVertex2f(0.0f, -5.3f);
    glVertex2f(0.5f, -5.6f);
    glEnd();
    if (trafficLightState == 2) {
        circle(0.3f, 0.0f, -4.5f, 255, 30, 30, 255);
        circle(0.45f, 0.0f, -4.5f, 255, 30, 30, 100);
    } else {
        circle(0.3f, 0.0f, -4.5f, 80, 10, 10, 255);
    }
    if (trafficLightState == 1) {
        circle(0.3f, 0.0f, -5.2f, 255, 200, 0, 255);
        circle(0.45f, 0.0f, -5.2f, 255, 200, 0, 100);
    } else {
        circle(0.3f, 0.0f, -5.2f, 80, 60, 0, 255);
    }
    if (trafficLightState == 0) {
        circle(0.3f, 0.0f, -5.9f, 30, 255, 30, 255);
        circle(0.45f, 0.0f, -5.9f, 30, 255, 30, 100);
    } else {
        circle(0.3f, 0.0f, -5.9f, 10, 80, 10, 255);
    }
}

// Animated helicopter with spinning propeller
void DrawHelicopter()
{
    if (!heliActive) return;
    float fadeAlpha = 1.0f;
    if (heliScale < 0.15f) {
        fadeAlpha = (heliScale - 0.05f) / 0.10f;
    }
    if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
    if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;
    glPushMatrix();
    glTranslatef(heliX, heliY, 0.0f);
    glScalef(heliScale, heliScale, 1.0f);
    circle(1.8f, 0.0f, 0.0f, (isNight ? 80.0f : 180.0f), (isNight ? 20.0f : 40.0f), (isNight ? 20.0f : 40.0f), 255.0f * fadeAlpha);
    if (isNight) glColor4ub(100, 200, 255, (unsigned char)(180 * fadeAlpha));
    else glColor4ub(180, 230, 255, (unsigned char)(200 * fadeAlpha));
    glBegin(GL_POLYGON);
    glVertex2f(0.3f, 0.8f);
    glVertex2f(1.4f, 0.3f);
    glVertex2f(1.2f, -0.6f);
    glVertex2f(0.0f, -0.6f);
    glEnd();
    if (isNight) glColor4ub(60, 60, 60, (unsigned char)(255 * fadeAlpha));
    else glColor4ub(120, 120, 120, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_QUADS);
    glVertex2f(-1.6f, 0.2f);
    glVertex2f(-4.0f, 0.8f);
    glVertex2f(-4.0f, 0.5f);
    glVertex2f(-1.6f, -0.2f);
    glEnd();
    if (isNight) glColor4ub(60, 60, 60, (unsigned char)(255 * fadeAlpha));
    else glColor4ub(120, 120, 120, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_TRIANGLES);
    glVertex2f(-4.0f, 0.5f);
    glVertex2f(-4.3f, 1.8f);
    glVertex2f(-3.7f, 0.5f);
    glEnd();
    glColor4ub(50, 50, 50, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_QUADS);
    glVertex2f(-0.2f, 1.6f);
    glVertex2f(0.2f, 1.6f);
    glVertex2f(0.2f, 2.2f);
    glVertex2f(-0.2f, 2.2f);
    glEnd();
    glColor4ub(50, 50, 50, (unsigned char)(255 * fadeAlpha));
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-0.8f, -1.6f);
    glVertex2f(-1.0f, -2.2f);
    glVertex2f(0.8f, -1.6f);
    glVertex2f(0.6f, -2.2f);
    glVertex2f(-1.6f, -2.2f);
    glVertex2f(1.6f, -2.2f);
    glEnd();
    glPushMatrix();
    glTranslatef(0.0f, 2.2f, 0.0f);
    glRotatef(heliPropAngle, 0.0f, 0.0f, 1.0f);
    glColor4ub(30, 30, 30, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(4.8f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-4.8f, 0.0f);
    glEnd();
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-4.0f, 1.3f, 0.0f);
    glRotatef(heliPropAngle * 1.5f, 0.0f, 0.0f, 1.0f);
    glColor4ub(30, 30, 30, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-1.0f, 0.0f);
    glEnd();
    glPopMatrix();
    float flash = (0.5f + 0.5f * sin(beaconPulse * 8.0f));
    circle(0.3f, -0.2f, 1.7f, 255, 0, 0, (unsigned char)(255 * flash * fadeAlpha));
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(0.8f, -0.4f, 0.8f, -0.6f, 12.0f, 3.5f, 255, 255, 220, (unsigned char)(95 * lf * fadeAlpha));
    }
    glPopMatrix();
}
// Computes interior window illumination color based on building location and day/night transition
void GetDynamicWindowColor(float x, float y, float rDay, float gDay, float bDay, float &rOut, float &gOut, float &bOut)
{
    int hashVal = (int)(fabsf(x) * 17.3f + fabsf(y) * 23.7f) % 100;
    float nightFactor = getLightFactor();
    float rCurrent = rDay, gCurrent = gDay, bCurrent = bDay;
    float rNight = 40.0f, gNight = 40.0f, bNight = 50.0f;
    bool isAlwaysOff = (hashVal % 3 == 0);
    if (!isAlwaysOff) {
        if (hashVal % 10 < 6) {
            rNight = 255.0f; gNight = 230.0f; bNight = 120.0f;
        } else if (hashVal % 10 < 9) {
            rNight = 255.0f; gNight = 170.0f; bNight = 80.0f;
        } else {
            rNight = 240.0f; gNight = 240.0f; bNight = 255.0f;
        }
        float speed = 2.0f + (hashVal % 5) * 0.5f;
        float flicker = 1.0f + 0.08f * sin(beaconPulse * speed);
        rNight *= flicker; gNight *= flicker; bNight *= flicker;
        if (rNight > 255.0f) rNight = 255.0f;
        if (gNight > 255.0f) gNight = 255.0f;
        if (bNight > 255.0f) bNight = 255.0f;
    }
    rOut = rCurrent + (rNight - rCurrent) * nightFactor;
    gOut = gCurrent + (gNight - gCurrent) * nightFactor;
    bOut = bCurrent + (bNight - bCurrent) * nightFactor;
}
// Multi-story commercial building with grid windows, ground storefronts, and roof antenna
void Building1()
{
    glColor3ub(230, 230, 220);
    glBegin(GL_QUADS);
    glVertex2f(-45.00f, -7.50f);
    glVertex2f(-38.00f, -7.50f);
    glVertex2f(-38.00f, 0.00f);
    glVertex2f(-45.00f, 0.00f);
    glEnd();
    glColor3ub(180, 180, 170);
    glBegin(GL_QUADS);
    glVertex2f(-38.00f, -7.50f);
    glVertex2f(-37.00f, -7.50f);
    glVertex2f(-37.00f, 0.00f);
    glVertex2f(-38.00f, 0.00f);
    glEnd();
    glColor3ub(120, 30, 30);
    glBegin(GL_QUADS);
    glVertex2f(-45.50f, 0.00f);
    glVertex2f(-36.50f, 0.00f);
    glVertex2f(-36.50f, 0.50f);
    glVertex2f(-45.50f, 0.50f);
    glEnd();
    glColor3ub(199, 41, 38);
    glBegin(GL_QUADS);
    glVertex2f(-45.20f, 0.50f);
    glVertex2f(-36.80f, 0.50f);
    glVertex2f(-37.50f, 2.00f);
    glVertex2f(-44.50f, 2.00f);
    glEnd();
    glColor3ub(230, 230, 220);
    glBegin(GL_QUADS);
    glVertex2f(-42.50f, 2.00f);
    glVertex2f(-39.50f, 2.00f);
    glVertex2f(-39.5f, 4.50f);
    glVertex2f(-42.50f, 4.50f);
    glEnd();
    glColor3ub(199, 41, 38);
    glBegin(GL_TRIANGLES);
    glVertex2f(-42.80f, 4.50f);
    glVertex2f(-39.20f, 4.50f);
    glVertex2f(-41.00f, 6.50f);
    glEnd();
    glColor3ub(255, 255, 255);
    glBegin(GL_POLYGON);
    glVertex2f(-41.80f, 3.25f);
    glVertex2f(-41.56f, 3.81f);
    glVertex2f(-41.00f, 4.05f);
    glVertex2f(-40.44f, 3.81f);
    glVertex2f(-40.20f, 3.25f);
    glVertex2f(-40.44f, 2.69f);
    glVertex2f(-41.00f, 2.45f);
    glVertex2f(-41.56f, 2.69f);
    glEnd();
    glLineWidth(2);
    glColor3ub(0, 0, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-41.80f, 3.25f);
    glVertex2f(-41.56f, 3.81f);
    glVertex2f(-41.00f, 4.05f);
    glVertex2f(-40.44f, 3.81f);
    glVertex2f(-40.20f, 3.25f);
    glVertex2f(-40.44f, 2.69f);
    glVertex2f(-41.00f, 2.45f);
    glVertex2f(-41.56f, 2.69f);
    glEnd();
    glBegin(GL_LINES);
    glVertex2f(-41.00f, 3.25f);
    glVertex2f(-41.00f, 3.85f);
    glVertex2f(-41.00f, 3.25f);
    glVertex2f(-40.60f, 3.25f);
    glEnd();
    for (float x = -44.5f; x < -38.0f; x += 1.8f) {
        for (float y = -6.5f; y < -0.5f; y += 2.2f) {
            glColor3ub(80, 80, 90);
            glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + 1.2f, y);
            glVertex2f(x + 1.2f, y + 1.6f);
            glVertex2f(x, y + 1.6f);
            glEnd();
            float rW, gW, bW;
            GetDynamicWindowColor(x, y, 100.0f, 200.0f, 255.0f, rW, gW, bW);
            glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
            glBegin(GL_QUADS);
            glVertex2f(x + 0.1f, y + 0.1f);
            glVertex2f(x + 1.1f, y + 0.1f);
            glVertex2f(x + 1.1f, y + 1.5f);
            glVertex2f(x + 0.1f, y + 1.5f);
            glEnd();
            glColor4ub(255, 255, 255, (unsigned char)(255 * (1.0f - getLightFactor())));
            glBegin(GL_TRIANGLES);
            glVertex2f(x + 0.2f, y + 0.3f);
            glVertex2f(x + 0.5f, y + 0.3f);
            glVertex2f(x + 0.2f, y + 0.8f);
            glEnd();
        }
    }
    glColor3ub(50, 50, 60);
    glBegin(GL_QUADS);
    glVertex2f(-42.20f, -7.50f);
    glVertex2f(-39.80f, -7.50f);
    glVertex2f(-39.80f, -4.80f);
    glVertex2f(-42.20f, -4.80f);
    glEnd();
    glColor3ub(69, 118, 247);
    glBegin(GL_QUADS);
    glVertex2f(-42.00f, -7.50f);
    glVertex2f(-40.00f, -7.50f);
    glVertex2f(-40.00f, -5.00f);
    glVertex2f(-42.00f, -5.00f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        glVertex2f(-45.50f, 0.50f);
        glVertex2f(-36.50f, 0.50f);
        glVertex2f(-36.50f, 0.50f + snowAccumulation);
        glVertex2f(-45.50f, 0.50f + snowAccumulation);
        glEnd();
        glBegin(GL_QUADS);
        glVertex2f(-42.50f, 4.50f);
        glVertex2f(-39.50f, 4.50f);
        glVertex2f(-39.50f, 4.50f + snowAccumulation);
        glVertex2f(-42.50f, 4.50f + snowAccumulation);
        glEnd();
    }
}
// Residential apartment block with balconies, dynamic window lighting, and decorative rooftop trims
void Building3()
{
    glColor3ub(240, 240, 235);
    glBegin(GL_QUADS);
    glVertex2f(-9.0f, -7.5f);
    glVertex2f(-2.5f, -7.5f);
    glVertex2f(-2.5f, 2.5f);
    glVertex2f(-9.0f, 2.5f);
    glEnd();
    glColor3ub(200, 200, 195);
    glBegin(GL_QUADS);
    glVertex2f(-2.5f, -7.5f);
    glVertex2f(-1.0f, -7.5f);
    glVertex2f(-1.0f, 2.5f);
    glVertex2f(-2.5f, 2.5f);
    glEnd();
    glColor3ub(218, 111, 69);
    glBegin(GL_QUADS);
    glVertex2f(-9.0f, -7.5f);
    glVertex2f(-7.5f, -7.5f);
    glVertex2f(-7.5f, 2.5f);
    glVertex2f(-9.0f, 2.5f);
    glEnd();
    glColor3ub(180, 90, 50);
    glBegin(GL_QUADS);
    glVertex2f(-7.6f, -7.5f);
    glVertex2f(-7.5f, -7.5f);
    glVertex2f(-7.5f, 2.5f);
    glVertex2f(-7.6f, 2.5f);
    glEnd();
    glColor3ub(30, 10, 40);
    glBegin(GL_QUADS);
    glVertex2f(-9.5f, 2.5f);
    glVertex2f(-0.5f, 2.5f);
    glVertex2f(-0.5f, 3.0f);
    glVertex2f(-9.5f, 3.0f);
    glEnd();
    glColor3ub(70, 30, 90);
    glBegin(GL_TRIANGLES);
    glVertex2f(-9.5f, 3.0f);
    glVertex2f(-0.5f, 3.0f);
    glVertex2f(-5.0f, 6.5f);
    glEnd();
    glColor3ub(60, 60, 70);
    glBegin(GL_QUADS);
    glVertex2f(-8.6f, -5.0f);
    glVertex2f(-7.9f, -5.0f);
    glVertex2f(-7.9f, 1.0f);
    glVertex2f(-8.6f, 1.0f);
    glEnd();
    float rW, gW, bW;
    GetDynamicWindowColor(-8.25f, -2.0f, 100.0f, 200.0f, 255.0f, rW, gW, bW);
    glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
    glBegin(GL_QUADS);
    glVertex2f(-8.5f, -4.9f);
    glVertex2f(-8.0f, -4.9f);
    glVertex2f(-8.0f, 0.9f);
    glVertex2f(-8.5f, 0.9f);
    glEnd();
    glColor3ub(60, 60, 70);
    glBegin(GL_QUADS);
    glVertex2f(-4.5f, -2.0f);
    glVertex2f(-2.5f, -2.0f);
    glVertex2f(-2.5f, 0.5f);
    glVertex2f(-4.5f, 0.5f);
    glEnd();
    GetDynamicWindowColor(-3.5f, -0.7f, 100.0f, 200.0f, 255.0f, rW, gW, bW);
    glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
    glBegin(GL_QUADS);
    glVertex2f(-4.4f, -1.9f);
    glVertex2f(-2.6f, -1.9f);
    glVertex2f(-2.6f, 0.4f);
    glVertex2f(-4.4f, 0.4f);
    glEnd();
    glColor4ub(255, 255, 255, (unsigned char)(255 * (1.0f - getLightFactor())));
    glBegin(GL_TRIANGLES);
    glVertex2f(-4.3f, -1.5f);
    glVertex2f(-3.8f, -1.5f);
    glVertex2f(-4.3f, -0.5f);
    glEnd();
    glColor3ub(60, 60, 70);
    glBegin(GL_QUADS);
    glVertex2f(-5.6f, 3.5f);
    glVertex2f(-4.4f, 3.5f);
    glVertex2f(-4.4f, 4.7f);
    glVertex2f(-5.6f, 4.7f);
    glEnd();
    GetDynamicWindowColor(-5.0f, 4.1f, 150.0f, 220.0f, 255.0f, rW, gW, bW);
    glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
    glBegin(GL_QUADS);
    glVertex2f(-5.5f, 3.6f);
    glVertex2f(-4.5f, 3.6f);
    glVertex2f(-4.5f, 4.6f);
    glVertex2f(-5.5f, 4.6f);
    glEnd();
    glColor3ub(50, 30, 20);
    glBegin(GL_QUADS);
    glVertex2f(-6.5f, -7.5f);
    glVertex2f(-5.0f, -7.5f);
    glVertex2f(-5.0f, -4.5f);
    glVertex2f(-6.5f, -4.5f);
    glEnd();
    glColor3ub(80, 40, 30);
    glBegin(GL_QUADS);
    glVertex2f(-6.3f, -7.5f);
    glVertex2f(-5.2f, -7.5f);
    glVertex2f(-5.2f, -4.7f);
    glVertex2f(-6.3f, -4.7f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        glVertex2f(-9.5f, 3.0f);
        glVertex2f(-0.5f, 3.0f);
        glVertex2f(-0.5f, 3.0f + snowAccumulation);
        glVertex2f(-9.5f, 3.0f + snowAccumulation);
        glEnd();
    }
}
// Corporate office tower with dark glass facade, vertical mullions, and roof communication spire
void Building4()
{
    glColor3ub(150, 150, 155);
    glBegin(GL_QUADS);
    glVertex2f(6.0f, -7.5f);
    glVertex2f(10.0f, -7.5f);
    glVertex2f(10.0f, 12.0f);
    glVertex2f(6.0f, 12.0f);
    glEnd();
    glColor3ub(120, 120, 125);
    glBegin(GL_QUADS);
    glVertex2f(10.0f, -7.5f);
    glVertex2f(11.0f, -7.5f);
    glVertex2f(11.0f, 12.0f);
    glVertex2f(10.0f, 12.0f);
    glEnd();
    float rW, gW, bW;
    GetDynamicWindowColor(8.25f, 2.0f, 40.0f, 80.0f, 150.0f, rW, gW, bW);
    glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
    glBegin(GL_QUADS);
    glVertex2f(7.5f, -8.0f);
    glVertex2f(9.0f, -8.0f);
    glVertex2f(9.0f, 11.5f);
    glVertex2f(7.5f, 11.5f);
    glEnd();
    glLineWidth(1);
    glColor3ub(100, 200, 255);
    glBegin(GL_LINES);
    glVertex2f(8.0f, -8.0f);
    glVertex2f(8.0f, 11.5f);
    glVertex2f(8.5f, -8.0f);
    glVertex2f(8.5f, 11.5f);
    for(float y = -7.0f; y < 11.0f; y += 2.0f) {
        glVertex2f(7.5f, y);
        glVertex2f(9.0f, y);
    }
    glEnd();
    glColor3ub(80, 80, 85);
    glBegin(GL_QUADS);
    glVertex2f(5.7f, 12.0f);
    glVertex2f(11.3f, 12.0f);
    glVertex2f(11.3f, 12.8f);
    glVertex2f(5.7f, 12.8f);
    glEnd();
    glColor3ub(100, 100, 105);
    glBegin(GL_QUADS);
    glVertex2f(6.5f, 12.8f);
    glVertex2f(8.0f, 12.8f);
    glVertex2f(8.0f, 14.0f);
    glVertex2f(6.5f, 14.0f);
    glEnd();
    glColor3ub(50, 50, 50);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(9.5f, 12.8f);
    glVertex2f(9.5f, 17.0f);
    glVertex2f(10.3f, 12.8f);
    glVertex2f(10.3f, 17.0f);
    glVertex2f(9.5f, 13.5f);
    glVertex2f(10.3f, 14.5f);
    glVertex2f(10.3f, 13.5f);
    glVertex2f(9.5f, 14.5f);
    glVertex2f(9.5f, 15.5f);
    glVertex2f(10.3f, 16.5f);
    glVertex2f(10.3f, 15.5f);
    glVertex2f(9.5f, 16.5f);
    glVertex2f(9.9f, 17.0f);
    glVertex2f(9.9f, 18.5f);
    glEnd();
    float pulseHaloAlpha = 40.0f + 180.0f * (0.5f + 0.5f * sin(beaconPulse * 4.0f));
    circle(0.2f, 9.9f, 18.6f, 255, 0, 0, 255);
    circle(0.4f + 0.2f * sin(beaconPulse * 4.0f), 9.9f, 18.6f, 255, 0, 0, (unsigned char)pulseHaloAlpha);
    if (snowAccumulation > 0.0f) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        glVertex2f(5.7f, 12.8f);
        glVertex2f(11.3f, 12.8f);
        glVertex2f(11.3f, 12.8f + snowAccumulation);
        glVertex2f(5.7f, 12.8f + snowAccumulation);
        glEnd();
    }
}
// Downtown cinema/theater building hosting the illuminated marquee sign
void Building6()
{
    glColor3ub(100, 100, 110);
    glBegin(GL_QUADS);
    glVertex2f(46.0f, -7.5f);
    glVertex2f(52.0f, -7.5f);
    glVertex2f(52.0f, -3.0f);
    glVertex2f(46.0f, -3.0f);
    glEnd();
    glColor3ub(160, 60, 50);
    glBegin(GL_QUADS);
    glVertex2f(46.0f, -3.0f);
    glVertex2f(52.0f, -3.0f);
    glVertex2f(52.0f, 9.0f);
    glVertex2f(46.0f, 9.0f);
    glEnd();
    glColor3ub(120, 40, 30);
    glBegin(GL_QUADS);
    glVertex2f(51.5f, -8.0f);
    glVertex2f(52.0f, -8.0f);
    glVertex2f(52.0f, 9.0f);
    glVertex2f(51.5f, 9.0f);
    glEnd();
    glColor3ub(230, 230, 230);
    glBegin(GL_QUADS);
    glVertex2f(45.8f, 9.0f);
    glVertex2f(52.2f, 9.0f);
    glVertex2f(52.2f, 10.0f);
    glVertex2f(45.8f, 10.0f);
    glEnd();
    for (float x = 47.0f; x < 51.0f; x += 2.5f) {
        for (float y = -2.0f; y < 8.0f; y += 2.5f) {
            glColor3ub(220, 220, 220);
            glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + 1.5f, y);
            glVertex2f(x + 1.5f, y + 1.8f);
            glVertex2f(x, y + 1.8f);
            glEnd();
            float rW, gW, bW;
            GetDynamicWindowColor(x, y, 100.0f, 200.0f, 255.0f, rW, gW, bW);
            glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
            glBegin(GL_QUADS);
            glVertex2f(x + 0.1f, y + 0.1f);
            glVertex2f(x + 1.4f, y + 0.1f);
            glVertex2f(x + 1.4f, y + 1.7f);
            glVertex2f(x + 0.1f, y + 1.7f);
            glEnd();
            glColor4ub(255, 255, 255, (unsigned char)(255 * (1.0f - getLightFactor())));
            glBegin(GL_TRIANGLES);
            glVertex2f(x + 0.2f, y + 0.3f);
            glVertex2f(x + 0.5f, y + 0.3f);
            glVertex2f(x + 0.2f, y + 0.8f);
            glEnd();
        }
    }
    glColor3ub(40, 40, 40);
    glBegin(GL_QUADS);
    glVertex2f(47.5f, -7.5f);
    glVertex2f(50.5f, -7.5f);
    glVertex2f(50.5f, -4.0f);
    glVertex2f(47.5f, -4.0f);
    glEnd();
    glColor3ub(80, 100, 120);
    glBegin(GL_QUADS);
    glVertex2f(47.7f, -7.5f);
    glVertex2f(48.9f, -7.5f);
    glVertex2f(48.9f, -4.2f);
    glVertex2f(47.7f, -4.2f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(49.1f, -7.5f);
    glVertex2f(50.3f, -7.5f);
    glVertex2f(50.3f, -4.2f);
    glVertex2f(49.1f, -4.2f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        glVertex2f(45.8f, 10.0f);
        glVertex2f(52.2f, 10.0f);
        glVertex2f(52.2f, 10.0f + snowAccumulation);
        glVertex2f(45.8f, 10.0f + snowAccumulation);
        glEnd();
    }
}
// Tall high-rise building with external vertical glass elevator shaft attached to right facade
void Building2()
{
    glColor3ub(100, 100, 200);
    glBegin(GL_QUADS);
    glVertex2f(-27.0f, -7.5f);
    glVertex2f(-21.0f, -7.5f);
    glVertex2f(-21.0f, 12.0f);
    glVertex2f(-27.0f, 12.0f);
    glEnd();
    glColor3ub(66, 66, 133);
    glBegin(GL_QUADS);
    glVertex2f(-22.5f, -7.5f);
    glVertex2f(-21.0f, -7.5f);
    glVertex2f(-21.0f, 12.0f);
    glVertex2f(-22.5f, 12.0f);
    glEnd();
    glColor3ub(40, 40, 40);
    glBegin(GL_QUADS);
    glVertex2f(-27.5f, 12.0f);
    glVertex2f(-20.5f, 12.0f);
    glVertex2f(-20.5f, 13.2f);
    glVertex2f(-27.5f, 13.2f);
    glEnd();
    glColor3ub(70, 70, 70);
    glBegin(GL_QUADS);
    glVertex2f(-25.5f, 13.2f);
    glVertex2f(-23.0f, 13.2f);
    glVertex2f(-23.0f, 14.7f);
    glVertex2f(-25.5f, 14.7f);
    glEnd();
    glLineWidth(2);
    glColor3ub(100, 100, 100);
    glBegin(GL_LINES);
    glVertex2f(-23.0f, 12.0f);
    glVertex2f(-23.0f, 17.0f);
    glEnd();
    float pulseHaloAlpha = 40.0f + 180.0f * (0.5f + 0.5f * sin(beaconPulse * 4.0f));
    circle(0.2f, -23.0f, 17.0f, 255, 0, 0, 255);
    circle(0.4f + 0.2f * sin(beaconPulse * 4.0f), -23.0f, 17.0f, 255, 0, 0, (unsigned char)pulseHaloAlpha);
    for (float i = -25.8f; i < -23.0f; i += 2.0f) {
        for (float j = -5.0f; j < 10.5f; j += 2.8f) {
            glColor3ub(50, 50, 50);
            glBegin(GL_QUADS);
            glVertex2f(i, j);
            glVertex2f(i + 1.5f, j);
            glVertex2f(i + 1.5f, j + 2.0f);
            glVertex2f(i, j + 2.0f);
            glEnd();
            float rW, gW, bW;
            GetDynamicWindowColor(i, j, 100.0f, 200.0f, 255.0f, rW, gW, bW);
            glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
            glBegin(GL_QUADS);
            glVertex2f(i + 0.1f, j + 0.1f);
            glVertex2f(i + 1.4f, j + 0.1f);
            glVertex2f(i + 1.4f, j + 1.9f);
            glVertex2f(i + 0.1f, j + 1.9f);
            glEnd();
            glColor4ub(255, 255, 255, (unsigned char)(255 * (1.0f - getLightFactor())));
            glBegin(GL_TRIANGLES);
            glVertex2f(i + 0.2f, j + 0.3f);
            glVertex2f(i + 0.5f, j + 0.3f);
            glVertex2f(i + 0.2f, j + 0.8f);
            glEnd();
        }
    }
    glColor3ub(20, 20, 30);
    glBegin(GL_QUADS);
    glVertex2f(-25.2f, -7.5f);
    glVertex2f(-22.8f, -7.5f);
    glVertex2f(-22.8f, -5.5f);
    glVertex2f(-25.2f, -5.5f);
    glEnd();
    glColor3ub(200, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(-25.7f, -5.5f);
    glVertex2f(-22.3f, -5.5f);
    glVertex2f(-22.3f, -4.7f);
    glVertex2f(-25.7f, -4.7f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        glVertex2f(-27.5f, 13.2f);
        glVertex2f(-20.5f, 13.2f);
        glVertex2f(-20.5f, 13.2f + snowAccumulation);
        glVertex2f(-27.5f, 13.2f + snowAccumulation);
        glEnd();
    }
}
// Mid-rise brick building with fire escape stairs and rooftop water tank
void Building5()
{
    glColor3ub(180, 100, 150);
    glBegin(GL_QUADS);
    glVertex2f(25.0f, -7.5f);
    glVertex2f(37.0f, -7.5f);
    glVertex2f(37.0f, 2.0f);
    glVertex2f(25.0f, 2.0f);
    glEnd();
    glColor3ub(120, 66, 100);
    glBegin(GL_QUADS);
    glVertex2f(35.5f, -7.5f);
    glVertex2f(37.0f, -7.5f);
    glVertex2f(37.0f, 2.0f);
    glVertex2f(35.5f, 2.0f);
    glEnd();
    glColor3ub(40, 40, 40);
    glBegin(GL_QUADS);
    glVertex2f(24.5f, 2.0f);
    glVertex2f(37.5f, 2.0f);
    glVertex2f(37.5f, 3.2f);
    glVertex2f(24.5f, 3.2f);
    glEnd();
    glColor3ub(70, 70, 70);
    glBegin(GL_QUADS);
    glVertex2f(26.5f, 3.2f);
    glVertex2f(29.0f, 3.2f);
    glVertex2f(29.0f, 4.7f);
    glVertex2f(26.5f, 4.7f);
    glEnd();
    glLineWidth(2);
    glColor3ub(100, 100, 100);
    glBegin(GL_LINES);
    glVertex2f(35.0f, 2.0f);
    glVertex2f(35.0f, 7.0f);
    glEnd();
    float pulseHaloAlpha = 40.0f + 180.0f * (0.5f + 0.5f * sin(beaconPulse * 4.0f));
    circle(0.2f, 35.0f, 7.0f, 255, 0, 0, 255);
    circle(0.4f + 0.2f * sin(beaconPulse * 4.0f), 35.0f, 7.0f, 255, 0, 0, (unsigned char)pulseHaloAlpha);
    for (float i = 26.2f; i < 35.0f; i += 2.0f) {
        for (float j = -5.0f; j < 0.5f; j += 2.8f) {
            glColor3ub(50, 50, 50);
            glBegin(GL_QUADS);
            glVertex2f(i, j);
            glVertex2f(i + 1.5f, j);
            glVertex2f(i + 1.5f, j + 2.0f);
            glVertex2f(i, j + 2.0f);
            glEnd();
            float rW, gW, bW;
            GetDynamicWindowColor(i, j, 100.0f, 200.0f, 255.0f, rW, gW, bW);
            glColor3ub((unsigned char)rW, (unsigned char)gW, (unsigned char)bW);
            glBegin(GL_QUADS);
            glVertex2f(i + 0.1f, j + 0.1f);
            glVertex2f(i + 1.4f, j + 0.1f);
            glVertex2f(i + 1.4f, j + 1.9f);
            glVertex2f(i + 0.1f, j + 1.9f);
            glEnd();
            glColor4ub(255, 255, 255, (unsigned char)(255 * (1.0f - getLightFactor())));
            glBegin(GL_TRIANGLES);
            glVertex2f(i + 0.2f, j + 0.3f);
            glVertex2f(i + 0.5f, j + 0.3f);
            glVertex2f(i + 0.2f, j + 0.8f);
            glEnd();
        }
    }
    glColor3ub(20, 20, 30);
    glBegin(GL_QUADS);
    glVertex2f(28.6f, -8.0f);
    glVertex2f(33.4f, -8.0f);
    glVertex2f(33.4f, -5.5f);
    glVertex2f(28.6f, -5.5f);
    glEnd();
    glColor3ub(200, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(28.1f, -5.5f);
    glVertex2f(33.9f, -5.5f);
    glVertex2f(33.9f, -4.7f);
    glVertex2f(28.1f, -4.7f);
    glEnd();
    if (snowAccumulation > 0.0f) {
        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
        glVertex2f(24.5f, 3.2f);
        glVertex2f(37.5f, 3.2f);
        glVertex2f(37.5f, 3.2f + snowAccumulation);
        glVertex2f(24.5f, 3.2f + snowAccumulation);
        glEnd();
    }
}
// Renders deciduous park tree with tall canopy and layered green foliage
void Tree1()

{
    glColor3ub(80, 50, 30);
    glBegin(GL_POLYGON);
    glVertex2f(-52.6f, -7.5f);
    glVertex2f(-50.4f, -7.5f);
    glVertex2f(-51.0f, -4.0f);
    glVertex2f(-52.0f, -4.0f);
    glEnd();
    circle(2.2, -53.5f, -4.0f, 20, 80, 20, 255);
    circle(2.2, -49.5f, -4.0f, 20, 80, 20, 255);
    circle(2.4, -53.0f, -2.5f, 34, 110, 34, 255);
    circle(2.4, -50.0f, -2.5f, 34, 110, 34, 255);
    circle(2.6, -51.5f, -0.5f, 50, 150, 30, 255);
}
// Renders rounded ornamental shade tree with dark bark and dense leaves
void Tree2()

{
    glColor3ub(60, 40, 20);
    glBegin(GL_QUADS);
    glVertex2f(-32.5f, -7.5f);
    glVertex2f(-31.5f, -7.5f);
    glVertex2f(-31.5f, -6.0f);
    glVertex2f(-32.5f, -6.0f);
    glEnd();
    glColor3ub(15, 70, 15);
    glBegin(GL_TRIANGLES);
    glVertex2f(-35.0f, -6.0f);
    glVertex2f(-29.0f, -6.0f);
    glVertex2f(-32.0f, -2.0f);
    glEnd();
    glColor3ub(20, 85, 20);
    glBegin(GL_TRIANGLES);
    glVertex2f(-34.5f, -3.0f);
    glVertex2f(-29.5f, -3.0f);
    glVertex2f(-32.0f, 1.0f);
    glEnd();
    glColor3ub(25, 100, 25);
    glBegin(GL_TRIANGLES);
    glVertex2f(-34.0f, 0.0f);
    glVertex2f(-30.0f, 0.0f);
    glVertex2f(-32.0f, 4.0f);
    glEnd();
}
// Renders slender roadside tree with branching twigs
void Tree3()

{
    glColor3ub(80, 50, 30);
    glBegin(GL_POLYGON);
    glVertex2f(-15.6f, -7.5f);
    glVertex2f(-13.4f, -7.5f);
    glVertex2f(-14.0f, -4.0f);
    glVertex2f(-15.0f, -4.0f);
    glEnd();
    circle(2.2, -16.5f, -4.0f, 20, 80, 20, 255);
    circle(2.2, -12.5f, -4.0f, 20, 80, 20, 255);
    circle(2.4, -16.0f, -2.5f, 34, 110, 34, 255);
    circle(2.4, -13.0f, -2.5f, 34, 110, 34, 255);
    circle(2.6, -14.5f, -0.5f, 50, 150, 30, 255);
}
// Renders compact flowering shrub-tree with bright green canopy
void Tree4()

{
    glColor3ub(60, 40, 20);
    glBegin(GL_QUADS);
    glVertex2f(19.5f, -7.5f);
    glVertex2f(20.5f, -7.5f);
    glVertex2f(20.5f, -6.0f);
    glVertex2f(19.5f, -6.0f);
    glEnd();
    glColor3ub(15, 70, 15);
    glBegin(GL_TRIANGLES);
    glVertex2f(17.0f, -6.0f);
    glVertex2f(23.0f, -6.0f);
    glVertex2f(20.0f, -2.0f);
    glEnd();
    glColor3ub(20, 85, 20);
    glBegin(GL_TRIANGLES);
    glVertex2f(17.5f, -3.0f);
    glVertex2f(22.5f, -3.0f);
    glVertex2f(20.0f, 1.0f);
    glEnd();
    glColor3ub(25, 100, 25);
    glBegin(GL_TRIANGLES);
    glVertex2f(18.0f, 0.0f);
    glVertex2f(22.0f, 0.0f);
    glVertex2f(20.0f, 4.0f);
    glEnd();
}
// Computes normalized darkness factor (0.0=full daylight noon, 1.0=deep night) from timeOfDay
float getLightFactor()
{
    if (timeOfDay > 0.25f && timeOfDay <= 0.35f) {
        return (timeOfDay - 0.25f) / 0.10f;
    } else if (timeOfDay > 0.35f && timeOfDay <= 0.75f) {
        return 1.0f;
    } else if (timeOfDay > 0.75f && timeOfDay <= 0.85f) {
        return 1.0f - (timeOfDay - 0.75f) / 0.10f;
    }
    return 0.0f;
}
// First highway lamppost casting downward cone of light on road at night
void StreetLight1()

{
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(-42.8f, -3.0f, 0.0f, -1.0f, 13.0f, 3.0f, 255, 255, 220, (unsigned char)(160 * lf));
    }
    glColor3ub(25, 25, 30);
    glBegin(GL_QUADS);
    glVertex2f(-40.4f, -12.0f);
    glVertex2f(-39.1f, -12.0f);
    glVertex2f(-39.3f, -10.5f);
    glVertex2f(-40.2f, -10.5f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-40.0f, -10.5f);
    glVertex2f(-39.5f, -10.5f);
    glVertex2f(-39.5f, -3.0f);
    glVertex2f(-40.0f, -3.0f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-40.0f, -3.5f);
    glVertex2f(-39.5f, -3.2f);
    glVertex2f(-43.0f, -2.5f);
    glVertex2f(-43.0f, -3.0f);
    glEnd();
    if (lf > 0.5f) circle(0.6f, -42.8f, -3.0f, 255, 255, 255, 255);
    else circle(0.6f, -42.8f, -3.0f, 50, 50, 50, 255);
    glColor3ub(10, 10, 10);
    glBegin(GL_QUADS);
    glVertex2f(-43.5f, -3.0f);
    glVertex2f(-42.0f, -2.8f);
    glVertex2f(-42.0f, -2.4f);
    glVertex2f(-43.5f, -2.4f);
    glEnd();
}
// Second highway lamppost illuminating the mid-section of the road
void StreetLight2()

{
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(-12.8f, -3.0f, 0.0f, -1.0f, 13.0f, 3.0f, 255, 255, 220, (unsigned char)(160 * lf));
    }
    glColor3ub(25, 25, 30);
    glBegin(GL_QUADS);
    glVertex2f(-10.4f, -12.0f);
    glVertex2f(-9.1f, -12.0f);
    glVertex2f(-9.3f, -10.5f);
    glVertex2f(-10.2f, -10.5f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-10.0f, -10.5f);
    glVertex2f(-9.5f, -10.5f);
    glVertex2f(-9.5f, -3.0f);
    glVertex2f(-10.0f, -3.0f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-10.0f, -3.5f);
    glVertex2f(-9.5f, -3.2f);
    glVertex2f(-13.0f, -2.5f);
    glVertex2f(-13.0f, -3.0f);
    glEnd();
    if (lf > 0.5f) circle(0.6f, -12.8f, -3.0f, 255, 255, 255, 255);
    else circle(0.6f, -12.8f, -3.0f, 50, 50, 50, 255);
    glColor3ub(10, 10, 10);
    glBegin(GL_QUADS);
    glVertex2f(-13.5f, -3.0f);
    glVertex2f(-12.0f, -2.8f);
    glVertex2f(-12.0f, -2.4f);
    glVertex2f(-13.5f, -2.4f);
    glEnd();
}
// Third highway lamppost positioned near the commercial zone
void StreetLight3()

{
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(17.2f, -3.0f, 0.0f, -1.0f, 13.0f, 3.0f, 255, 255, 220, (unsigned char)(160 * lf));
    }
    glColor3ub(25, 25, 30);
    glBegin(GL_QUADS);
    glVertex2f(19.6f, -12.0f);
    glVertex2f(20.9f, -12.0f);
    glVertex2f(20.7f, -10.5f);
    glVertex2f(19.8f, -10.5f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(20.0f, -10.5f);
    glVertex2f(20.5f, -10.5f);
    glVertex2f(20.5f, -3.0f);
    glVertex2f(20.0f, -3.0f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(20.0f, -3.5f);
    glVertex2f(20.5f, -3.2f);
    glVertex2f(17.0f, -2.5f);
    glVertex2f(17.0f, -3.0f);
    glEnd();
    if (lf > 0.5f) circle(0.6f, 17.2f, -3.0f, 255, 255, 255, 255);
    else circle(0.6f, 17.2f, -3.0f, 50, 50, 50, 255);
    glColor3ub(10, 10, 10);
    glBegin(GL_QUADS);
    glVertex2f(16.5f, -3.0f);
    glVertex2f(18.0f, -2.8f);
    glVertex2f(18.0f, -2.4f);
    glVertex2f(16.5f, -2.4f);
    glEnd();
}
// Fourth highway lamppost near the right intersection
void StreetLight4()

{
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(47.2f, -3.0f, 0.0f, -1.0f, 13.0f, 3.0f, 255, 255, 220, (unsigned char)(160 * lf));
    }
    glColor3ub(25, 25, 30);
    glBegin(GL_QUADS);
    glVertex2f(49.6f, -12.0f);
    glVertex2f(50.9f, -12.0f);
    glVertex2f(50.7f, -10.5f);
    glVertex2f(49.8f, -10.5f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(50.0f, -10.5f);
    glVertex2f(50.5f, -10.5f);
    glVertex2f(50.5f, -3.0f);
    glVertex2f(50.0f, -3.0f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(50.0f, -3.5f);
    glVertex2f(50.5f, -3.2f);
    glVertex2f(47.0f, -2.5f);
    glVertex2f(47.0f, -3.0f);
    glEnd();
    if (lf > 0.5f) circle(0.6f, 47.2f, -3.0f, 255, 255, 255, 255);
    else circle(0.6f, 47.2f, -3.0f, 50, 50, 50, 255);
    glColor3ub(10, 10, 10);
    glBegin(GL_QUADS);
    glVertex2f(46.5f, -3.0f);
    glVertex2f(48.0f, -2.8f);
    glVertex2f(48.0f, -2.4f);
    glVertex2f(46.5f, -2.4f);
    glEnd();
}
// Modern passenger commuter train with multiple cars, illuminated windows, and headlamp
void Train()

{
    glColor3ub(200, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(trainPos, -7.1f);
    glVertex2f(trainPos + 8.0f, -7.1f);
    glVertex2f(trainPos + 8.0f, -3.1f);
    glVertex2f(trainPos, -3.1f);
    glEnd();
    glColor3ub(180, 40, 40);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 5.0f, -7.1f);
    glVertex2f(trainPos + 8.0f, -7.1f);
    glVertex2f(trainPos + 8.0f, -1.6f);
    glVertex2f(trainPos + 5.0f, -1.6f);
    glEnd();
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 1.0f, -3.1f);
    glVertex2f(trainPos + 2.0f, -3.1f);
    glVertex2f(trainPos + 2.0f, -1.6f);
    glVertex2f(trainPos + 1.0f, -1.6f);
    glEnd();
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(200, 240, 255);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 5.5f, -4.3f);
    glVertex2f(trainPos + 7.5f, -4.3f);
    glVertex2f(trainPos + 7.5f, -2.8f);
    glVertex2f(trainPos + 5.5f, -2.8f);
    glEnd();
    glColor3ub(100, 100, 100);
    glBegin(GL_TRIANGLES);
    glVertex2f(trainPos, -7.1f);
    glVertex2f(trainPos, -5.3f);
    glVertex2f(trainPos - 2.0f, -7.1f);
    glEnd();
    circle(1, trainPos + 2.0f, -7.1f, 30, 30, 30, 255);
    circle(1, trainPos + 6.0f, -7.1f, 30, 30, 30, 255);
    glColor3ub(0, 100, 180);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 9.0f, -7.1f);
    glVertex2f(trainPos + 18.0f, -7.1f);
    glVertex2f(trainPos + 18.0f, -3.1f);
    glVertex2f(trainPos + 9.0f, -3.1f);
    glEnd();
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(200, 240, 255);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 9.5f, -5.3f);
    glVertex2f(trainPos + 17.5f, -5.3f);
    glVertex2f(trainPos + 17.5f, -3.8f);
    glVertex2f(trainPos + 9.5f, -3.8f);
    glEnd();
    glColor3ub(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 8.0f, -6.3f);
    glVertex2f(trainPos + 9.0f, -6.3f);
    glVertex2f(trainPos + 9.0f, -5.3f);
    glVertex2f(trainPos + 8.0f, -5.3f);
    glEnd();
    circle(1, trainPos + 11.0f, -7.1f, 30, 30, 30, 255);
    circle(1, trainPos + 16.0f, -7.1f, 30, 30, 30, 255);
    glColor3ub(0, 100, 180);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 19.0f, -7.1f);
    glVertex2f(trainPos + 28.0f, -7.1f);
    glVertex2f(trainPos + 28.0f, -3.1f);
    glVertex2f(trainPos + 19.0f, -3.1f);
    glEnd();
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(200, 240, 255);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 19.5f, -5.3f);
    glVertex2f(trainPos + 27.5f, -5.3f);
    glVertex2f(trainPos + 27.5f, -3.8f);
    glVertex2f(trainPos + 19.5f, -3.8f);
    glEnd();
    glColor3ub(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 18.0f, -6.3f);
    glVertex2f(trainPos + 19.0f, -6.3f);
    glVertex2f(trainPos + 19.0f, -5.3f);
    glVertex2f(trainPos + 18.0f, -5.3f);
    glEnd();
    circle(1, trainPos + 21.0f, -7.1f, 30, 30, 30, 255);
    circle(1, trainPos + 26.0f, -7.1f, 30, 30, 30, 255);
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(trainPos - 2.0f, -6.8f, -1.0f, 0.0f, 15.0f, 3.5f, 255, 255, 200, (unsigned char)(120 * lf));
    }
    if (isAnimating) {
        float stackX = trainPos + 1.5f;
        float stackY = -1.6f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 15.0f + i * 2.0f;
            float life = fmod(t, 6.0f) / 6.0f;
            float xOffset = 3.2f * life;
            float yOffset = 2.5f * life;
            float scale = 0.25f + 0.7f * life;
            unsigned char alpha = (unsigned char)(140.0f * (1.0f - life));
            circle(scale, stackX + xOffset, stackY + yOffset, 220, 220, 225, alpha);
        }
    }
}
// Timer callback: drives train along railway tracks from left to right with wrap-around
void UpdateTrain(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTrain, 0); return; }
    if (isAnimating) {
        trainPos -= 0.6f;
        if (trainPos < -90.0f) trainPos = 70.0f;
    }
    glutTimerFunc(25, UpdateTrain, 0);
}
// Red passenger sedan with headlights, taillights, windows, and rotating wheel rims
void CarRed()

{
    glPushMatrix();
    glTranslatef(carRedPos, 0.0f, 0.0f);
    glColor3ub(40, 40, 40);
    circle(1.15, -2.2, -21.1, 40, 40, 40, 255);
    circle(1.15,  2.2, -21.1, 40, 40, 40, 255);
    glColor3ub(255, 0, 0);
    glBegin(GL_POLYGON);
    glVertex2f(-4.8, -21.1);
    glVertex2f( 4.8, -21.1);
    glVertex2f( 4.8, -20.2);
    glVertex2f( 2.8, -19.8);
    glVertex2f(-2.8, -19.8);
    glVertex2f(-4.8, -20.1);
    glEnd();
    glBegin(GL_POLYGON);
    glVertex2f(-2.8, -19.8);
    glVertex2f( 2.8, -19.8);
    glVertex2f( 1.2, -18.1);
    glVertex2f(-1.6, -18.1);
    glEnd();
    glColor3ub(50, 60, 70);
    glBegin(GL_QUADS);
    glVertex2f(-2.6, -19.6);
    glVertex2f(-0.6, -19.6);
    glVertex2f(-0.6, -18.3);
    glVertex2f(-1.7, -18.3);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-0.2, -19.6);
    glVertex2f( 2.6, -19.6);
    glVertex2f( 1.3, -18.3);
    glVertex2f(-0.2, -18.3);
    glEnd();
    if (isNight) glColor3ub(255, 255, 150);
    else         glColor3ub(255, 255, 220);
    glBegin(GL_QUADS);
    glVertex2f(4.4, -20.8);
    glVertex2f(4.8, -20.8);
    glVertex2f(4.8, -20.3);
    glVertex2f(4.4, -20.3);
    glEnd();
    if (isNight || isCarRedBraking) glColor3ub(255, 30, 30);
    else                           glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-4.8, -20.8);
    glVertex2f(-4.4, -20.8);
    glVertex2f(-4.4, -20.3);
    glVertex2f(-4.8, -20.3);
    glEnd();
    if (isCarRedBraking) {
        circle(0.85f, -4.8f, -20.5f, 255, 20, 20, 210);
        circle(1.4f,  -4.8f, -20.5f, 255, 0,  0,  90);
    }
    glColor3ub(30, 30, 30);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-2.0, -20.5);
    glVertex2f( 2.0, -20.5);
    glEnd();
    circle(1.0, -2.2, -21.1, 20, 20, 20, 255);
    circle(0.6, -2.2, -21.1, 180, 180, 180, 255);
    circle(0.15,-2.2, -21.1, 50, 50, 50, 255);
    circle(1.0,  2.2, -21.1, 20, 20, 20, 255);
    circle(0.6,  2.2, -21.1, 180, 180, 180, 255);
    circle(0.15, 2.2, -21.1, 50, 50, 50, 255);
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(4.8f, -20.5f, 1.0f, 0.0f, 9.0f, 2.5f, 255, 255, 200, (unsigned char)(100 * lf));
    }
    glPopMatrix();
}
// Green compact car traveling along the lower road lane
void CarGreen()

{
    glPushMatrix();
    glTranslatef(carGreenPos, 0.0f, 0.0f);
    glColor3ub(40, 40, 40);
    circle(1.15, -2.2, -15.1, 40, 40, 40, 255);
    circle(1.15,  2.2, -15.1, 40, 40, 40, 255);
    glColor3ub(0, 180, 0);
    glBegin(GL_POLYGON);
    glVertex2f(-4.8, -15.1);
    glVertex2f( 4.8, -15.1);
    glVertex2f( 4.8, -14.2);
    glVertex2f( 2.8, -13.8);
    glVertex2f(-2.8, -13.8);
    glVertex2f(-4.8, -14.1);
    glEnd();
    glBegin(GL_POLYGON);
    glVertex2f(-2.8, -13.8);
    glVertex2f( 2.8, -13.8);
    glVertex2f( 1.2, -12.1);
    glVertex2f(-1.6, -12.1);
    glEnd();
    glColor3ub(50, 60, 70);
    glBegin(GL_QUADS);
    glVertex2f(-2.6, -13.6);
    glVertex2f(-0.6, -13.6);
    glVertex2f(-0.6, -12.3);
    glVertex2f(-1.7, -12.3);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-0.2, -13.6);
    glVertex2f( 2.6, -13.6);
    glVertex2f( 1.3, -12.3);
    glVertex2f(-0.2, -12.3);
    glEnd();
    circle(1.0, -2.2, -15.1, 20, 20, 20, 255);
    circle(1.0,  2.2, -15.1, 20, 20, 20, 255);
    float lf = getLightFactor();
    if (lf > 0.5f) glColor3ub(255, 255, 150);
    else         glColor3ub(255, 255, 220);
    glBegin(GL_QUADS);
    glVertex2f(4.4, -14.8);
    glVertex2f(4.8, -14.8);
    glVertex2f(4.8, -14.3);
    glVertex2f(4.4, -14.3);
    glEnd();
    if (lf > 0.5f || isCarGreenBraking) glColor3ub(255, 30, 30);
    else                                glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-4.8, -14.8);
    glVertex2f(-4.4, -14.8);
    glVertex2f(-4.4, -14.3);
    glVertex2f(-4.8, -14.3);
    glEnd();
    if (isCarGreenBraking) {
        circle(0.85f, -4.8f, -14.5f, 255, 20, 20, 210);
        circle(1.4f,  -4.8f, -14.5f, 255, 0,  0,  90);
    }
    if (lf > 0.01f) {
        DrawLightCone(4.8f, -14.5f, 1.0f, 0.0f, 9.0f, 2.5f, 255, 255, 200, (unsigned char)(100 * lf));
    }
    glPopMatrix();
}
// Yellow passenger taxi/coupe cruising down the highway
void CarYellow()

{
    glPushMatrix();
    glTranslatef(carYellowPos, 0.0f, 0.0f);
    glColor3ub(40, 40, 40);
    circle(1.15, -2.2, -15.1, 40, 40, 40, 255);
    circle(1.15,  2.2, -15.1, 40, 40, 40, 255);
    glColor3ub(150, 162, 170);
    glBegin(GL_POLYGON);
    glVertex2f(-4.8, -15.1);
    glVertex2f( 4.8, -15.1);
    glVertex2f( 4.8, -14.2);
    glVertex2f( 2.8, -13.8);
    glVertex2f(-2.8, -13.8);
    glVertex2f(-4.8, -14.1);
    glEnd();
    glBegin(GL_POLYGON);
    glVertex2f(-2.8, -13.8);
    glVertex2f( 2.8, -13.8);
    glVertex2f( 1.2, -12.1);
    glVertex2f(-1.6, -12.1);
    glEnd();
    glColor3ub(50, 60, 70);
    glBegin(GL_QUADS);
    glVertex2f(-2.6, -13.6);
    glVertex2f(-0.6, -13.6);
    glVertex2f(-0.6, -12.3);
    glVertex2f(-1.7, -12.3);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-0.2, -13.6);
    glVertex2f( 2.6, -13.6);
    glVertex2f( 1.3, -12.3);
    glVertex2f(-0.2, -12.3);
    glEnd();
    if (isNight) glColor3ub(255, 255, 150);
    else         glColor3ub(255, 255, 220);
    glBegin(GL_QUADS);
    glVertex2f(4.4, -14.8);
    glVertex2f(4.8, -14.8);
    glVertex2f(4.8, -14.3);
    glVertex2f(4.4, -14.3);
    glEnd();
    if (isNight || isCarYellowBraking) glColor3ub(255, 0, 0);
    else         glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-4.8, -14.8);
    glVertex2f(-4.4, -14.8);
    glVertex2f(-4.4, -14.3);
    glVertex2f(-4.8, -14.3);
    glEnd();
    if (isCarYellowBraking) {
        circle(0.85f, -4.8f, -14.5f, 255, 20, 20, 210);
        circle(1.4f,  -4.8f, -14.5f, 255, 0,  0,  90);
    }
    glColor3ub(30, 30, 30);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-2.0, -14.5);
    glVertex2f( 2.0, -14.5);
    glEnd();
    circle(1.0, -2.2, -15.1, 20, 20, 20, 255);
    circle(0.6, -2.2, -15.1, 180, 180, 180, 255);
    circle(0.15,-2.2, -15.1, 50, 50, 50, 255);
    circle(1.0,  2.2, -15.1, 20, 20, 20, 255);
    circle(0.6,  2.2, -15.1, 180, 180, 180, 255);
    circle(0.15, 2.2, -15.1, 50, 50, 50, 255);
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(4.8f, -14.5f, 1.0f, 0.0f, 9.0f, 2.5f, 255, 255, 200, (unsigned char)(100 * lf));
    }
    glPopMatrix();
}
// Timer callback: drives red car, handles braking at red traffic lights, and animates wheels
void UpdateCarRed(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCarRed, 0); return; }
    if (isAnimating) {
        float speed = 0.4f;
        isCarRedBraking = false;
        if ((trafficLightState == 2 || trafficLightState == 1) && carRedPos < -15.0f && carRedPos > -30.0f) {
            float distance = -15.0f - carRedPos;
            speed = 0.4f * (distance / 15.0f);
            if (speed < 0.02f) speed = 0.0f;
            isCarRedBraking = true;
        }
        if (speed > 0.05f && (rand() % 3 == 0)) {
            EmitExhaustSmoke(carRedPos - 4.8f, -21.0f, (speed < 0.3f ? 1.5f : 0.8f));
        }
        carRedPos += speed;
        if (carRedPos > 70.0f) carRedPos = -70.0f;
    }
    glutTimerFunc(25, UpdateCarRed, 0);
}
// Timer callback: drives green car with traffic light deceleration and stopping behavior
void UpdateCarGreen(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCarGreen, 0); return; }
    if (isAnimating) {
        float speed = 0.4f;
        isCarGreenBraking = false;
        if ((trafficLightState == 2 || trafficLightState == 1) && carGreenPos < -5.0f && carGreenPos > -20.0f) {
            float distance = -5.0f - carGreenPos;
            speed = 0.4f * (distance / 15.0f);
            if (speed < 0.02f) speed = 0.0f;
            isCarGreenBraking = true;
        }
        if (speed > 0.05f && (rand() % 3 == 0)) {
            EmitExhaustSmoke(carGreenPos - 4.8f, -15.0f, (speed < 0.3f ? 1.5f : 0.8f));
        }
        carGreenPos += speed;
        if (carGreenPos > 70.0f) carGreenPos = -70.0f;
    }
    glutTimerFunc(25, UpdateCarGreen, 0);
}
// Timer callback: advances yellow car position and checks traffic light signals
void UpdateCarYellow(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCarYellow, 0); return; }
    if (isAnimating) {
        float speed = 0.4f;
        isCarYellowBraking = false;
        if ((trafficLightState == 2 || trafficLightState == 1) && carYellowPos < -25.0f && carYellowPos > -40.0f) {
            float distance = -25.0f - carYellowPos;
            speed = 0.4f * (distance / 15.0f);
            if (speed < 0.02f) speed = 0.0f;
            isCarYellowBraking = true;
        }
        if (speed > 0.05f && (rand() % 3 == 0)) {
            EmitExhaustSmoke(carYellowPos - 4.8f, -15.0f, (speed < 0.3f ? 1.5f : 0.8f));
        }
        carYellowPos += speed;
        if (carYellowPos > 70.0f) carYellowPos = -70.0f;
    }
    glutTimerFunc(25, UpdateCarYellow, 0);
}
// Large municipal public transit bus with passenger windows, destination sign, and headlights
void Bus()

{
    glPushMatrix();
    glTranslatef(busPos, 0.0f, 0.0f);
    glColor3ub(40, 40, 40);
    circle(1.3, -38.5, -20.0, 40, 40, 40, 255);
    circle(1.3, -31.5, -20.0, 40, 40, 40, 255);
    glColor3ub(220, 20, 60);
    glBegin(GL_POLYGON);
    glVertex2f(-41.0, -20.0);
    glVertex2f(-29.0, -20.0);
    glVertex2f(-29.0, -18.5);
    glVertex2f(-29.4, -16.5);
    glVertex2f(-41.0, -16.5);
    glEnd();
    glColor3ub(230, 230, 230);
    glBegin(GL_QUADS);
    glVertex2f(-41.0, -16.5);
    glVertex2f(-29.4, -16.5);
    glVertex2f(-29.4, -16.1);
    glVertex2f(-41.0, -16.1);
    glEnd();
    glColor3ub(180, 180, 180);
    glBegin(GL_QUADS);
    glVertex2f(-39.0, -16.1);
    glVertex2f(-36.0, -16.1);
    glVertex2f(-36.0, -15.7);
    glVertex2f(-39.0, -15.7);
    glEnd();
    glColor3ub(20, 20, 20);
    glBegin(GL_QUADS);
    glVertex2f(-32.0, -17.2);
    glVertex2f(-29.5, -17.2);
    glVertex2f(-29.5, -16.6);
    glVertex2f(-32.0, -16.6);
    glEnd();
    glColor3ub(255, 140, 0);
    glBegin(GL_QUADS);
    glVertex2f(-31.8, -17.0);
    glVertex2f(-29.7, -17.0);
    glVertex2f(-29.7, -16.8);
    glVertex2f(-31.8, -16.8);
    glEnd();
    glColor3ub(160, 210, 255);
    glBegin(GL_QUADS);
    glVertex2f(-40.5, -18.8);
    glVertex2f(-39.1, -18.8);
    glVertex2f(-39.1, -17.0);
    glVertex2f(-40.5, -17.0);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-38.9, -18.8);
    glVertex2f(-37.5, -18.8);
    glVertex2f(-37.5, -17.0);
    glVertex2f(-38.9, -17.0);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-37.3, -18.8);
    glVertex2f(-35.9, -18.8);
    glVertex2f(-35.9, -17.0);
    glVertex2f(-37.3, -17.0);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-35.7, -18.8);
    glVertex2f(-34.3, -18.8);
    glVertex2f(-34.3, -17.0);
    glVertex2f(-35.7, -17.0);
    glEnd();
    glColor3ub(130, 180, 220);
    glBegin(GL_QUADS);
    glVertex2f(-30.5, -18.8);
    glVertex2f(-29.5, -18.8);
    glVertex2f(-29.5, -17.3);
    glVertex2f(-30.5, -17.3);
    glEnd();
    glColor3ub(180, 180, 180);
    glBegin(GL_QUADS);
    glVertex2f(-34.0, -20.0);
    glVertex2f(-32.2, -20.0);
    glVertex2f(-32.2, -16.8);
    glVertex2f(-34.0, -16.8);
    glEnd();
    glColor3ub(100, 150, 200);
    glBegin(GL_QUADS);
    glVertex2f(-33.9, -19.2);
    glVertex2f(-33.2, -19.2);
    glVertex2f(-33.2, -17.0);
    glVertex2f(-33.9, -17.0);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-33.0, -19.2);
    glVertex2f(-32.3, -19.2);
    glVertex2f(-32.3, -17.0);
    glVertex2f(-33.0, -17.0);
    glEnd();
    glColor3ub(60, 60, 60);
    glBegin(GL_QUADS);
    glVertex2f(-41.1, -20.2);
    glVertex2f(-28.9, -20.2);
    glVertex2f(-28.9, -19.8);
    glVertex2f(-41.1, -19.8);
    glEnd();
    glColor3ub(255, 255, 200);
    glBegin(GL_QUADS);
    glVertex2f(-29.2, -19.6);
    glVertex2f(-28.9, -19.6);
    glVertex2f(-28.9, -19.2);
    glVertex2f(-29.2, -19.2);
    glEnd();
    if (isNight || isBusBraking) glColor3ub(255, 30, 30);
    else                         glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-41.1, -19.6);
    glVertex2f(-40.8, -19.6);
    glVertex2f(-40.8, -19.2);
    glVertex2f(-41.1, -19.2);
    glEnd();
    if (isBusBraking) {
        circle(1.0f, -41.0f, -19.4f, 255, 20, 20, 210);
        circle(1.6f, -41.0f, -19.4f, 255, 0,  0,  90);
    }
    glLineWidth(2);
    glColor3ub(40, 40, 40);
    glBegin(GL_LINES);
    glVertex2f(-29.4, -18.0);
    glVertex2f(-28.8, -17.8);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-28.9, -18.1);
    glVertex2f(-28.7, -18.1);
    glVertex2f(-28.7, -17.5);
    glVertex2f(-28.9, -17.5);
    glEnd();
    circle(1.2, -38.5, -20.0, 20, 20, 20, 255);
    circle(0.7, -38.5, -20.0, 150, 150, 150, 255);
    circle(1.2, -31.5, -20.0, 20, 20, 20, 255);
    circle(0.7, -31.5, -20.0, 150, 150, 150, 255);
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(-28.9f, -19.4f, 1.0f, 0.0f, 11.0f, 3.0f, 255, 255, 200, (unsigned char)(110 * lf));
    }
    glPopMatrix();
}
// Timer callback: drives transit bus, stops at red lights, and triggers exhaust smoke
void UpdateBus(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateBus, 0); return; }
    if (isAnimating) {
        float speed = 0.4f;
        isBusBraking = false;
        if ((trafficLightState == 2 || trafficLightState == 1) && busPos < 27.0f && busPos > 12.0f) {
            float distance = 27.0f - busPos;
            speed = 0.4f * (distance / 15.0f);
            if (speed < 0.02f) speed = 0.0f;
            isBusBraking = true;
        }
        if (speed > 0.05f && (rand() % 3 == 0)) {
            EmitExhaustSmoke(busPos - 41.0f, -20.0f, (speed < 0.3f ? 1.6f : 0.9f));
        }
        busPos += speed;
        if (busPos > 70.0f) busPos = -70.0f;
    }
    glutTimerFunc(25, UpdateBus, 0);
}
// Heavy commercial 18-wheeler semi truck hauling a large freight cargo container
void CargoTruck()

{
    glPushMatrix();
    glTranslatef(truckCargoPos, 0.0f, 0.0f);
    glColor3ub(230, 230, 230);
    glBegin(GL_QUADS);
    glVertex2f(-8, -14.5);
    glVertex2f(1, -14.5);
    glVertex2f(1, -10.0);
    glVertex2f(-8, -10.0);
    glEnd();
    glColor3ub(255, 69, 0);
    glBegin(GL_QUADS);
    glVertex2f(-8, -12.5);
    glVertex2f(1, -12.5);
    glVertex2f(1, -11.7);
    glVertex2f(-8, -11.7);
    glEnd();
    glColor3ub(100, 100, 100);
    glBegin(GL_QUADS);
    glVertex2f(-8.2, -14.5);
    glVertex2f(-7.7, -14.5);
    glVertex2f(-7.7, -10.0);
    glVertex2f(-8.2, -10.0);
    glEnd();
    if (isNight || isCargoTruckBraking) glColor3ub(255, 30, 30);
    else                               glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-8.3, -14.3);
    glVertex2f(-8.0, -14.3);
    glVertex2f(-8.0, -13.5);
    glVertex2f(-8.3, -13.5);
    glEnd();
    if (isCargoTruckBraking) {
        circle(1.0f, -8.2f, -14.0f, 255, 20, 20, 210);
        circle(1.6f, -8.2f, -14.0f, 255, 0,  0,  90);
    }
    glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(1.5, -14.5);
    glVertex2f(5.0, -14.5);
    glVertex2f(5.0, -11.0);
    glVertex2f(1.5, -11.0);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex2f(1.5, -11.0);
    glVertex2f(5.0, -11.0);
    glVertex2f(1.5, -9.8);
    glEnd();
    glColor3ub(135, 206, 250);
    glBegin(GL_QUADS);
    glVertex2f(3.0, -13.0);
    glVertex2f(5.0, -13.0);
    glVertex2f(5.0, -11.5);
    glVertex2f(3.0, -11.5);
    glEnd();
    glColor3ub(50, 50, 50);
    glBegin(GL_LINES);
    for (float i = -14.2f; i < -13.2f; i += 0.3f) {
        glVertex2f(4.8, i);
        glVertex2f(5.2, i);
    }
    glEnd();
    glColor3ub(192, 192, 192);
    glBegin(GL_QUADS);
    glVertex2f(2.0, -15.5);
    glVertex2f(3.5, -15.5);
    glVertex2f(3.5, -14.5);
    glVertex2f(2.0, -14.5);
    glEnd();
    glColor3ub(80, 80, 80);
    glBegin(GL_QUADS);
    glVertex2f(1.2, -14.5);
    glVertex2f(1.5, -14.5);
    glVertex2f(1.5, -9.0);
    glVertex2f(1.2, -9.0);
    glEnd();
    glColor3ub(30, 30, 30);
    glBegin(GL_QUADS);
    glVertex2f(1.0, -14.5);
    glVertex2f(1.5, -14.5);
    glVertex2f(1.5, -14.0);
    glVertex2f(1.0, -14.0);
    glEnd();
    circle(1.0, -6.5, -15.5, 0, 0, 0, 255);
    circle(0.5, -6.5, -15.5, 150, 150, 150, 255);
    circle(1.0, -4.0, -15.5, 0, 0, 0, 255);
    circle(0.5, -4.0, -15.5, 150, 150, 150, 255);
    circle(1.0, 3.5, -15.5, 0, 0, 0, 255);
    circle(0.5, 3.5, -15.5, 150, 150, 150, 255);
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(5.0f, -13.5f, 1.0f, 0.0f, 11.0f, 3.0f, 255, 255, 200, (unsigned char)(110 * lf));
    }
    glPopMatrix();
}
// Timer callback: advances cargo truck along highway with traffic light braking
void UpdateTruckCargo(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTruckCargo, 0); return; }
    if (isAnimating) {
        float speed = 0.4f;
        isCargoTruckBraking = false;
        if ((trafficLightState == 2 || trafficLightState == 1) && truckCargoPos < -10.0f && truckCargoPos > -25.0f) {
            float distance = -10.0f - truckCargoPos;
            speed = 0.4f * (distance / 15.0f);
            if (speed < 0.02f) speed = 0.0f;
            isCargoTruckBraking = true;
        }
        if (speed > 0.05f && (rand() % 3 == 0)) {
            EmitExhaustSmoke(truckCargoPos - 8.2f, -14.5f, (speed < 0.3f ? 1.7f : 0.9f));
        }
        truckCargoPos += speed;
        if (truckCargoPos > 70.0f) truckCargoPos = -70.0f;
    }
    glutTimerFunc(25, UpdateTruckCargo, 0);
}
// Light commercial delivery flatbed truck carrying utility cargo
void SmallTruck()

{
    glPushMatrix();
    glTranslatef(truckSmallPos, 0.0f, 0.0f);
    glColor3ub(200, 200, 200);
    glBegin(GL_QUADS);
    glVertex2f(-4, -22);
    glVertex2f(2, -22);
    glVertex2f(2, -18);
    glVertex2f(-4, -18);
    glEnd();
    glColor3ub(0, 100, 200);
    glBegin(GL_QUADS);
    glVertex2f(2.1, -22);
    glVertex2f(4.6, -22);
    glVertex2f(4.6, -19);
    glVertex2f(2.1, -19);
    glEnd();
    glColor3ub(200, 250, 255);
    glBegin(GL_QUADS);
    glVertex2f(2.5, -20.5);
    glVertex2f(4.0, -20.5);
    glVertex2f(4.0, -19.5);
    glVertex2f(2.5, -19.5);
    glEnd();
    if (isNight || isSmallTruckBraking) glColor3ub(255, 30, 30);
    else                               glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-4.2, -21.0);
    glVertex2f(-3.9, -21.0);
    glVertex2f(-3.9, -20.2);
    glVertex2f(-4.2, -20.2);
    glEnd();
    if (isSmallTruckBraking) {
        circle(0.85f, -4.0f, -20.5f, 255, 20, 20, 210);
        circle(1.4f,  -4.0f, -20.5f, 255, 0,  0,  90);
    }
    circle(1.0, -2.0, -22.0, 0, 0, 0, 255);
    circle(1.0,  0.0, -22.0, 0, 0, 0, 255);
    circle(1.0,  3.5, -22.0, 0, 0, 0, 255);
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(4.6f, -21.0f, 1.0f, 0.0f, 10.0f, 2.8f, 255, 255, 200, (unsigned char)(100 * lf));
    }
    glPopMatrix();
}
// Timer callback: updates small truck highway speed and traffic stop compliance
void UpdateTruckSmall(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTruckSmall, 0); return; }
    if (isAnimating) {
        float speed = 0.4f;
        isSmallTruckBraking = false;
        if ((trafficLightState == 2 || trafficLightState == 1) && truckSmallPos < -10.0f && truckSmallPos > -25.0f) {
            float distance = -10.0f - truckSmallPos;
            speed = 0.4f * (distance / 15.0f);
            if (speed < 0.02f) speed = 0.0f;
            isSmallTruckBraking = true;
        }
        if (speed > 0.05f && (rand() % 3 == 0)) {
            EmitExhaustSmoke(truckSmallPos - 4.0f, -21.0f, (speed < 0.3f ? 1.5f : 0.8f));
        }
        truckSmallPos += speed;
        if (truckSmallPos > 70.0f) truckSmallPos = -70.0f;
    }
    glutTimerFunc(25, UpdateTruckSmall, 0);
}
// Renders a single undulating sinusoidal ocean wave strip with vertical color gradient
void DrawWaveLayer(float baseY, float amp, float freq, float speedMult, float phaseShift, SkyColor topCol, SkyColor botCol)
{
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 1.5f) {
        float yVal = baseY + amp * sin(freq * x + waveMove * speedMult + phaseShift);
        glColor3ub((unsigned char)botCol.r, (unsigned char)botCol.g, (unsigned char)botCol.b);
        glVertex2f(x, -40.0f);
        glColor3ub((unsigned char)topCol.r, (unsigned char)topCol.g, (unsigned char)topCol.b);
        glVertex2f(x, yVal);
    }
    glEnd();
}
// Draws shimmering, vertically smeared water reflection underneath a coastal light source
void DrawWavyReflection(float xLight, unsigned char r, unsigned char g, unsigned char b, float maxA)
{
    float lf = getLightFactor();
    if (lf < 0.01f) return;
    float actualAlpha = maxA * lf;
    glBegin(GL_QUAD_STRIP);
    for (float y = -24.0f; y >= -40.0f; y -= 1.0f) {
        float depthFactor = (y + 40.0f) / 16.0f;
        float wiggleAmp = 1.0f * (0.3f + 0.7f * depthFactor);
        float wiggle = wiggleAmp * sin(0.9f * y + waveMove * 6.0f);
        float xCenter = xLight + wiggle;
        float width = 1.6f * (0.4f + 0.6f * depthFactor);
        float alpha = actualAlpha * (0.2f + 0.8f * depthFactor);
        glColor4ub(r, g, b, 0);
        glVertex2f(xCenter - width, y);
        glColor4ub(r, g, b, (unsigned char)alpha);
        glVertex2f(xCenter, y);
    }
    glEnd();
    glBegin(GL_QUAD_STRIP);
    for (float y = -24.0f; y >= -40.0f; y -= 1.0f) {
        float depthFactor = (y + 40.0f) / 16.0f;
        float wiggleAmp = 1.0f * (0.3f + 0.7f * depthFactor);
        float wiggle = wiggleAmp * sin(0.9f * y + waveMove * 6.0f);
        float xCenter = xLight + wiggle;
        float width = 1.6f * (0.4f + 0.6f * depthFactor);
        float alpha = actualAlpha * (0.2f + 0.8f * depthFactor);
        glColor4ub(r, g, b, (unsigned char)alpha);
        glVertex2f(xCenter, y);
        glColor4ub(r, g, b, 0);
        glVertex2f(xCenter + width, y);
    }
    glEnd();
}
// Renders deep coastal ocean water with multiple layered animated wave currents and horizon blend
void Ocean()

{
    float lf = getLightFactor();
    SkyColor backNoonT = {0.0f, 70.0f, 130.0f};
    SkyColor backNoonB = {0.0f, 90.0f, 150.0f};
    SkyColor backNightT = {0.0f, 10.0f, 25.0f};
    SkyColor backNightB = {0.0f, 20.0f, 45.0f};
    SkyColor backStormT = {10.0f, 30.0f, 45.0f};
    SkyColor backStormB = {20.0f, 45.0f, 65.0f};
    SkyColor midNoonT = {0.0f, 90.0f, 160.0f};
    SkyColor midNoonB = {0.0f, 110.0f, 180.0f};
    SkyColor midNightT = {0.0f, 15.0f, 35.0f};
    SkyColor midNightB = {0.0f, 30.0f, 55.0f};
    SkyColor midStormT = {15.0f, 40.0f, 55.0f};
    SkyColor midStormB = {25.0f, 55.0f, 75.0f};
    SkyColor frontNoonT = {0.0f, 110.0f, 190.0f};
    SkyColor frontNoonB = {0.0f, 140.0f, 220.0f};
    SkyColor frontNightT = {0.0f, 20.0f, 45.0f};
    SkyColor frontNightB = {0.0f, 40.0f, 70.0f};
    SkyColor frontStormT = {20.0f, 50.0f, 70.0f};
    SkyColor frontStormB = {30.0f, 70.0f, 95.0f};
    SkyColor backT, backB, midT, midB, frontT, frontB;
    auto getLayerColors = [&](SkyColor noonT, SkyColor noonB, SkyColor nightT, SkyColor nightB, SkyColor stormT, SkyColor stormB, SkyColor &topOut, SkyColor &botOut) {
        SkyColor baseT = lerpColor(noonT, nightT, lf);
        SkyColor baseB = lerpColor(noonB, nightB, lf);
        topOut = lerpColor(baseT, stormT, stormFactor);
        botOut = lerpColor(baseB, stormB, stormFactor);
    };
    getLayerColors(backNoonT, backNoonB, backNightT, backNightB, backStormT, backStormB, backT, backB);
    getLayerColors(midNoonT, midNoonB, midNightT, midNightB, midStormT, midStormB, midT, midB);
    getLayerColors(frontNoonT, frontNoonB, frontNightT, frontNightB, frontStormT, frontStormB, frontT, frontB);
    DrawWaveLayer(-25.0f, 0.3f, 0.35f, 1.5f, 0.0f, backT, backB);
    DrawWaveLayer(-28.0f, 0.5f, 0.20f, -2.5f, 1.0f, midT, midB);
    DrawWaveLayer(-31.0f, 0.7f, 0.12f, 4.0f, 2.0f, frontT, frontB);
    if (lf > 0.01f) {
        if (moonY_glob > -12.0f) {
            float moonAlpha = (1.0f - stormFactor) * (moonY_glob > 0.0f ? 1.0f : (moonY_glob + 12.0f) / 12.0f);
            if (moonAlpha > 0.01f) {
                DrawWavyReflection(moonX_glob, 230, 240, 255, 65.0f * moonAlpha);
            }
        }
        DrawWavyReflection(-42.8f, 255, 230, 150, 50.0f);
        DrawWavyReflection(-12.8f, 255, 230, 150, 50.0f);
        DrawWavyReflection(17.2f, 255, 230, 150, 50.0f);
        DrawWavyReflection(47.2f, 255, 230, 150, 50.0f);
    }
    glLineWidth(2);
    glColor4ub(255, 255, 255, 80);
    glBegin(GL_LINE_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 1.5f) {
        float yVal = -31.0f + 0.7f * sin(0.12f * x + waveMove * 4.0f + 2.0f);
        glVertex2f(x, yVal);
    }
    glEnd();
}
// Timer callback: advances ocean wave propagation phase and water oscillation timers
void UpdateWaves(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateWaves, 0); return; }
    if (isAnimating) {
        waveMove += 0.04f;
    }
    glutTimerFunc(25, UpdateWaves, 0);
}
// Multi-deck luxury passenger cruise ship sailing on the horizon with lighted portholes
void CruiseShip()

{
    glPushMatrix();
    glTranslatef(cruisePos, 0.0f, 0.0f);
    glColor3ub(240, 240, 240);
    glBegin(GL_POLYGON);
    glVertex2f(-10, -27);
    glVertex2f(10, -27);
    glVertex2f(8, -30);
    glVertex2f(-9, -30);
    glEnd();
    glColor3ub(255, 255, 255);
    glBegin(GL_QUADS);
    glVertex2f(-8, -27);
    glVertex2f(7, -27);
    glVertex2f(7, -25);
    glVertex2f(-8, -25);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(-6, -25);
    glVertex2f(4, -25);
    glVertex2f(4, -23.5);
    glVertex2f(-6, -23.5);
    glEnd();
    glColor3ub(0, 0, 150);
    glPointSize(3);
    glBegin(GL_POINTS);
    for (float i = -7; i < 6; i += 0.8)
        glVertex2f(i, -26.5);
    for (float i = -5; i < 4; i += 0.8)
        glVertex2f(i, -24.5);
    glEnd();
    glColor3ub(200, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(-2, -23.5);
    glVertex2f(-0.5, -23.5);
    glVertex2f(-0.5, -21.5);
    glVertex2f(-2, -21.5);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(1, -23.5);
    glVertex2f(2.5, -23.5);
    glVertex2f(2.5, -21.5);
    glVertex2f(1, -21.5);
    glEnd();
    if (isNight) {
        glColor3ub(255, 255, 100);
        glBegin(GL_QUADS);
        glVertex2f(-8, -26.8);
        glVertex2f(7, -26.8);
        glVertex2f(7, -26.3);
        glVertex2f(-8, -26.3);
        glEnd();
    }
    if (isAnimating) {
        float bowX = 9.8f;
        float bowY = -29.0f;
        float direction = 1.0f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 8.0f + i * 1.5f;
            float scale = 0.25f + 0.15f * sin(t);
            float xOffset = -direction * (0.3f + i * 0.8f);
            float yOffset = -0.08f * i + 0.12f * cos(t);
            circle(scale, bowX + xOffset, bowY + yOffset, 245, 250, 255, (unsigned char)(150 - i * 35));
        }
    }
    glPopMatrix();
}
// Timer callback: glides cruise ship smoothly across the distant sea channel
void UpdateCruiseShip(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCruiseShip, 0); return; }
    if (isAnimating) {
        cruisePos += 0.2f;
        if (cruisePos > 80.0f) cruisePos = -80.0f;
    }
    glutTimerFunc(25, UpdateCruiseShip, 0);
}
// Industrial container cargo vessel loaded with multi-colored freight shipping containers
void CargoShip()

{
    glPushMatrix();
    glTranslatef(cargoShipPos, 0.0f, 0.0f);
    glColor3ub(100, 20, 20);
    glBegin(GL_QUADS);
    glVertex2f(-8, -31);
    glVertex2f(8, -31);
    glVertex2f(7, -33);
    glVertex2f(-7, -33);
    glEnd();
    glColor3ub(30, 30, 30);
    glBegin(GL_QUADS);
    glVertex2f(-8, -31);
    glVertex2f(8, -31);
    glVertex2f(8, -29.5);
    glVertex2f(-8, -29.5);
    glEnd();
    glColor3ub(255, 165, 0);
    glBegin(GL_QUADS);
    glVertex2f(-6, -29.5);
    glVertex2f(-3, -29.5);
    glVertex2f(-3, -27.5);
    glVertex2f(-6, -27.5);
    glEnd();
    glColor3ub(0, 0, 200);
    glBegin(GL_QUADS);
    glVertex2f(-2, -29.5);
    glVertex2f(1, -29.5);
    glVertex2f(1, -27.5);
    glVertex2f(-2, -27.5);
    glEnd();
    glColor3ub(0, 200, 0);
    glBegin(GL_QUADS);
    glVertex2f(2, -29.5);
    glVertex2f(5, -29.5);
    glVertex2f(5, -27.5);
    glVertex2f(2, -27.5);
    glEnd();
    glColor3ub(255, 255, 255);
    glBegin(GL_QUADS);
    glVertex2f(5.5, -29.5);
    glVertex2f(7.5, -29.5);
    glVertex2f(7.5, -26.5);
    glVertex2f(5.5, -26.5);
    glEnd();
    if (isAnimating) {
        float bowX = -8.0f;
        float bowY = -31.0f;
        float direction = -1.0f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 8.0f + i * 1.5f;
            float scale = 0.25f + 0.15f * sin(t);
            float xOffset = -direction * (0.3f + i * 0.8f);
            float yOffset = -0.08f * i + 0.12f * cos(t);
            circle(scale, bowX + xOffset, bowY + yOffset, 245, 250, 255, (unsigned char)(150 - i * 35));
        }
    }
    glPopMatrix();
}
// Timer callback: advances container cargo ship steadily across the sea lane
void UpdateCargoShip(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCargoShip, 0); return; }
    if (isAnimating) {
        cargoShipPos -= 0.1f;
        if (cargoShipPos < -80.0f) cargoShipPos = 80.0f;
    }
    glutTimerFunc(25, UpdateCargoShip, 0);
}
// Elegant private sailing yacht with twin white sails, hull stripe, and deck cabin
void LuxuryYacht()

{
    glPushMatrix();
    glTranslatef(yachtPos, 0.0f, 0.0f);
    glColor3ub(30, 35, 45);
    glBegin(GL_POLYGON);
    glVertex2f(-14, -34.5);
    glVertex2f(16, -33);
    glVertex2f(13, -37);
    glVertex2f(-12, -37);
    glEnd();
    glColor3ub(192, 192, 192);
    glBegin(GL_QUADS);
    glVertex2f(-14, -34.5);
    glVertex2f(15, -33.2);
    glVertex2f(14.5, -32.5);
    glVertex2f(-14, -32.5);
    glEnd();
    glColor3ub(255, 255, 255);
    glBegin(GL_POLYGON);
    glVertex2f(-13, -32.5);
    glVertex2f(8, -32.5);
    glVertex2f(6, -29.5);
    glVertex2f(-10, -29.5);
    glEnd();
    glColor3ub(245, 245, 245);
    glBegin(GL_POLYGON);
    glVertex2f(-9, -29.5);
    glVertex2f(3, -29.5);
    glVertex2f(1.5, -27);
    glVertex2f(-8, -27);
    glEnd();
    glColor3ub(0, 200, 200);
    glBegin(GL_POLYGON);
    glVertex2f(-8, -31.5);
    glVertex2f(5, -31.5);
    glVertex2f(4, -30.5);
    glVertex2f(-8, -30.5);
    glEnd();
    glBegin(GL_POLYGON);
    glVertex2f(-7, -28.8);
    glVertex2f(1, -28.8);
    glVertex2f(0.5, -28);
    glVertex2f(-7, -28);
    glEnd();
    glColor3ub(100, 100, 100);
    glBegin(GL_QUADS);
    glVertex2f(9, -32.4);
    glVertex2f(15, -32.4);
    glVertex2f(14, -32);
    glVertex2f(9.5, -32);
    glEnd();
    glColor3ub(255, 255, 255);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(11.5, -32.3);
    glVertex2f(11.5, -32.1);
    glVertex2f(12.5, -32.3);
    glVertex2f(12.5, -32.1);
    glVertex2f(11.5, -32.2);
    glVertex2f(12.5, -32.2);
    glEnd();
    glColor3ub(220, 220, 220);
    glBegin(GL_POLYGON);
    glVertex2f(-4, -27);
    glVertex2f(-1, -27);
    glVertex2f(-2, -24);
    glVertex2f(-5, -25);
    glEnd();
    circle(0.8, -2.5, -24, 255, 255, 255, 255);
    circle(0.6, -4, -24.8, 255, 255, 255, 255);
    glColor3ub(0, 150, 255);
    glBegin(GL_QUADS);
    glVertex2f(-13, -32.4);
    glVertex2f(-10, -32.4);
    glVertex2f(-10, -31.6);
    glVertex2f(-13, -31.6);
    glEnd();
    if (isAnimating) {
        float bowX = -14.0f;
        float bowY = -34.5f;
        float direction = -1.0f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 8.0f + i * 1.5f;
            float scale = 0.25f + 0.15f * sin(t);
            float xOffset = -direction * (0.3f + i * 0.8f);
            float yOffset = -0.08f * i + 0.12f * cos(t);
            circle(scale, bowX + xOffset, bowY + yOffset, 245, 250, 255, (unsigned char)(150 - i * 35));
        }
    }
    glPopMatrix();
}
// Timer callback: moves luxury yacht along coastal waterway with gentle wave bobbing
void UpdateYacht(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateYacht, 0); return; }
    if (isAnimating) {
        yachtPos -= 0.3f;
        if (yachtPos < -80.0f) yachtPos = 80.0f;
    }
    glutTimerFunc(25, UpdateYacht, 0);
}
// Motorized recreational speedboat cruising near the harbor with foaming wake
void SmallBoat1()

{
    glPushMatrix();
    glTranslatef(boatSmall1Pos, -29.0f, 0.0f);
    glColor3ub(101, 67, 33);
    glBegin(GL_POLYGON);
    glVertex2f(-3.5, 1.5);
    glVertex2f(3.5, 1.5);
    glVertex2f(2.5, 0);
    glVertex2f(-2.5, 0);
    glEnd();
    glColor3ub(218, 165, 32);
    glBegin(GL_QUADS);
    glVertex2f(-3.4, 1.0);
    glVertex2f(3.4, 1.0);
    glVertex2f(3.4, 1.3);
    glVertex2f(-3.4, 1.3);
    glEnd();
    glColor3ub(60, 40, 20);
    glBegin(GL_QUADS);
    glVertex2f(-0.2, 1.5);
    glVertex2f(0.2, 1.5);
    glVertex2f(0.2, 7.5);
    glVertex2f(-0.2, 7.5);
    glEnd();
    glColor3ub(250, 250, 250);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.2, 2.5);
    glVertex2f(0.2, 7.0);
    glVertex2f(3.0, 2.5);
    glEnd();
    glColor3ub(230, 230, 220);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.3, 6.8);
    glVertex2f(-0.3, 2.5);
    glVertex2f(-2.5, 2.5);
    glEnd();
    glColor3ub(220, 20, 60);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 7.5);
    glVertex2f(-1.5, 7.2);
    glVertex2f(0, 6.9);
    glEnd();
    if (isAnimating) {
        float bowX = 3.5f;
        float bowY = 0.5f;
        float direction = 1.0f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 8.0f + i * 1.5f;
            float scale = 0.18f + 0.08f * sin(t);
            float xOffset = -direction * (0.2f + i * 0.5f);
            float yOffset = -0.05f * i + 0.08f * cos(t);
            circle(scale, bowX + xOffset, bowY + yOffset, 245, 250, 255, (unsigned char)(140 - i * 35));
        }
    }
    glPopMatrix();
}
// Second harbor patrol launch boat with cabin and antenna
void SmallBoat2()

{
    glPushMatrix();
    glTranslatef(boatSmall2Pos, -39.0f, 0.0f);
    glColor3ub(101, 67, 33);
    glBegin(GL_POLYGON);
    glVertex2f(-3.5, 1.5);
    glVertex2f(3.5, 1.5);
    glVertex2f(2.5, 0);
    glVertex2f(-2.5, 0);
    glEnd();
    glColor3ub(218, 165, 32);
    glBegin(GL_QUADS);
    glVertex2f(-3.4, 1.0);
    glVertex2f(3.4, 1.0);
    glVertex2f(3.4, 1.3);
    glVertex2f(-3.4, 1.3);
    glEnd();
    glColor3ub(60, 40, 20);
    glBegin(GL_QUADS);
    glVertex2f(-0.2, 1.5);
    glVertex2f(0.2, 1.5);
    glVertex2f(0.2, 7.5);
    glVertex2f(-0.2, 7.5);
    glEnd();
    glColor3ub(250, 250, 250);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.2, 2.5);
    glVertex2f(0.2, 7.0);
    glVertex2f(3.0, 2.5);
    glEnd();
    glColor3ub(230, 230, 220);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.3, 6.8);
    glVertex2f(-0.3, 2.5);
    glVertex2f(-2.5, 2.5);
    glEnd();
    glColor3ub(220, 20, 60);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 7.5);
    glVertex2f(-1.5, 7.2);
    glVertex2f(0, 6.9);
    glEnd();
    if (isAnimating) {
        float bowX = 3.5f;
        float bowY = 0.5f;
        float direction = 1.0f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 8.0f + i * 1.5f;
            float scale = 0.18f + 0.08f * sin(t);
            float xOffset = -direction * (0.2f + i * 0.5f);
            float yOffset = -0.05f * i + 0.08f * cos(t);
            circle(scale, bowX + xOffset, bowY + yOffset, 245, 250, 255, (unsigned char)(140 - i * 35));
        }
    }
    glPopMatrix();
}
// Traditional wooden coastal fishing boat bobbing in place with fishing lines cast
void FishingBoat()

{
    float bob = 0.3f * sin(waveMove * 2.0f + 1.7f);
    float lf  = getLightFactor();
    float dim = 1.0f - 0.5f * lf;
    glPushMatrix();
    glTranslatef(fishingBoatX, -27.0f + bob, 0.0f);
    glColor3ub((unsigned char)(140*dim), (unsigned char)(95*dim), (unsigned char)(55*dim));
    glBegin(GL_POLYGON);
    glVertex2f(-3.0f, 1.3f);
    glVertex2f(3.0f, 1.3f);
    glVertex2f(2.2f, 0.0f);
    glVertex2f(-2.2f, 0.0f);
    glEnd();
    glColor3ub((unsigned char)(190*dim), (unsigned char)(150*dim), (unsigned char)(90*dim));
    glBegin(GL_QUADS);
    glVertex2f(-2.9f, 1.0f);
    glVertex2f(2.9f, 1.0f);
    glVertex2f(2.9f, 1.3f);
    glVertex2f(-2.9f, 1.3f);
    glEnd();
    glColor3ub((unsigned char)(100*dim), (unsigned char)(70*dim), (unsigned char)(45*dim));
    glBegin(GL_QUADS);
    glVertex2f(1.4f, 1.3f);
    glVertex2f(2.5f, 1.3f);
    glVertex2f(2.5f, 2.2f);
    glVertex2f(1.4f, 2.2f);
    glEnd();
    glColor3ub((unsigned char)(70*dim), (unsigned char)(110*dim), (unsigned char)(90*dim));
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, 1.3f);
    glVertex2f(-0.4f, 1.3f);
    glVertex2f(-0.4f, 2.2f);
    glVertex2f(-1.0f, 2.2f);
    glEnd();
    circle(0.22f, -0.7f, 2.5f, (unsigned char)(220*dim), (unsigned char)(175*dim), (unsigned char)(130*dim), 255);
    glColor3ub((unsigned char)(90*dim), (unsigned char)(60*dim), (unsigned char)(35*dim));
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glVertex2f(-0.7f, 2.0f);
    glVertex2f(-2.8f, 3.0f);
    glEnd();
    float lineWave = 0.15f * sin(waveMove * 3.0f);
    glColor4ub(220, 220, 230, 160);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(-2.8f, 3.0f);
    glVertex2f(-3.3f + lineWave, -1.0f);
    glEnd();
    circle(0.12f, -3.3f + lineWave, -1.05f, 255, 80, 60, 255);
    glPopMatrix();
}
// Timer callback: advances first speedboat along coastal patrol path
void UpdateSmallBoat1(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSmallBoat1, 0); return; }
    if (isAnimating) {
        boatSmall1Pos += 0.15f;
        if (boatSmall1Pos > 70.0f) boatSmall1Pos = -70.0f;
    }
    glutTimerFunc(25, UpdateSmallBoat1, 0);
}
// Timer callback: advances second motorboat across harbor waters
void UpdateSmallBoat2(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSmallBoat2, 0); return; }
    if (isAnimating) {
        boatSmall2Pos += 0.25f;
        if (boatSmall2Pos > 70.0f) boatSmall2Pos = -70.0f;
    }
    glutTimerFunc(25, UpdateSmallBoat2, 0);
}
// Timer callback: steers helicopter along sinusoidal flight path and spins rotor blades
void UpdateHelicopter(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateHelicopter, 0); return; }
    if (isAnimating) {
        heliX += 0.17f;
        heliY -= 0.03f;
        if (heliScale < 1.5f) {
            heliScale += 0.003f;
        }
        heliPropAngle -= 25.0f;
        if (heliPropAngle < -360.0f) heliPropAngle += 360.0f;
        if (heliX > 75.0f) {
            heliX = -45.0f - (rand() % 20);
            heliY = 14.0f + (rand() % 6);
            heliScale = 0.05f;
        }
    }
    glutTimerFunc(25, UpdateHelicopter, 0);
}
// Timer callback: cycles coastal intersection traffic signal (Green -> Yellow -> Red)
void UpdateTrafficLight(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTrafficLight, 0); return; }
    if (isAnimating) {
        trafficLightTimer++;
        if (trafficLightState == 0) {
            if (trafficLightTimer >= 320) {
                trafficLightState = 1;
                trafficLightTimer = 0;
            }
        } else if (trafficLightState == 1) {
            if (trafficLightTimer >= 100) {
                trafficLightState = 2;
                trafficLightTimer = 0;
            }
        } else if (trafficLightState == 2) {
            if (trafficLightTimer >= 320) {
                trafficLightState = 0;
                trafficLightTimer = 0;
            }
        }
    }
    glutTimerFunc(25, UpdateTrafficLight, 0);
}
// Timer callback: cycles roadside advertising billboard between its 3 displays
void UpdateBillboardAd(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateBillboardAd, 0); return; }
    if (isAnimating) {
        adSlideTimer++;
        if (adSlideTimer >= 200) {
            activeAdSlide = (activeAdSlide + 1) % 3;
            adSlideTimer = 0;
        }
    }
    glutTimerFunc(25, UpdateBillboardAd, 0);
}
// Timer callback: advances pedestrians along sidewalks and handles crosswalk crossing
void UpdatePedestrians(int)

{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdatePedestrians, 0); return; }
    if (isAnimating) {
        pedWalkTimer += 0.04f;
        for (int i = 0; i < NUM_PEDS; i++) {
            Pedestrian& p = peds[i];
            if (!p.crossing) p.y = -10.5f;
            if (!p.crossing) {
                float spd = p.speed;
                if (trafficLightState == 2) {
                    if (p.x >= -4.0f && p.x <= 4.0f) {
                        p.crossing   = true;
                        p.crossTimer = 0.0f;
                        p.savedX     = p.x;
                    }
                } else {
                    float stopX = (p.dir > 0) ? -2.0f : 2.5f;
                    bool approachingCrosswalk = (p.dir > 0) ? (p.x > stopX - 6.0f && p.x <= stopX) :
                                                              (p.x < stopX + 6.0f && p.x >= stopX);
                    if (approachingCrosswalk) {
                        float distToStop = (p.dir > 0) ? (stopX - p.x) : (p.x - stopX);
                        spd = p.speed * (distToStop / 6.0f);
                        if (spd < 0.01f) spd = 0.0f;
                    }
                }
                p.x += p.dir * spd;
                if (p.x > 62.0f)  p.x = -62.0f;
                if (p.x < -62.0f) p.x =  62.0f;
            } else {
                const float crossDistance = 11.0f;
                p.x += p.dir * p.speed;
                float progress = fabsf(p.x - p.savedX) / crossDistance;
                if (progress < 1.0f) {
                    p.y = -10.5f - 7.5f * sin(progress * 3.14159f);
                } else {
                    p.y = -10.5f;
                    p.crossing = false;
                    p.crossTimer = 0.0f;
                }
            }
        }
    }
    glutTimerFunc(25, UpdatePedestrians, 0);
}
// Timer callback: animates seagull flight paths, wing flap cycles, and storm evasion altitude
void UpdateSeagulls(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSeagulls, 0); return; }
    if (isAnimating) {
        for (int i = 0; i < NUM_SEAGULLS; i++) {
            Seagull& g = seagulls[i];
            float spd = g.baseSpeed;
            float flapSpd = g.flapSpeed;
            if (weatherMode == 1) {
                g.targetY = 12.0f + i * 2.5f;
                spd *= 0.7f;
                flapSpd *= 1.8f;
            } else if (weatherMode == 2) {
                g.targetY = 15.0f + i * 2.2f;
                spd *= 0.85f;
                flapSpd *= 1.3f;
            } else {
                g.targetY = 22.0f + i * 2.2f;
            }
            g.y += (g.targetY - g.y) * 0.03f;
            g.x += g.dir * spd;
            g.wingAngle += flapSpd;
            if (g.x > 70.0f) g.x = -70.0f;
            if (g.x < -70.0f) g.x = 70.0f;
        }
    }
    glutTimerFunc(25, UpdateSeagulls, 0);
}
// Timer callback: expands particle radius, updates velocity, and fades alpha of exhaust puffs
void UpdateExhaustSmoke(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateExhaustSmoke, 0); return; }
    if (isAnimating) {
        for (int i = 0; i < MAX_EXHAUST; i++) {
            if (exhaustParticles[i].active) {
                exhaustParticles[i].x += exhaustParticles[i].vx;
                exhaustParticles[i].y += exhaustParticles[i].vy;
                exhaustParticles[i].size += 0.012f;
                exhaustParticles[i].alpha -= 0.018f;
                exhaustParticles[i].life--;
                if (exhaustParticles[i].life <= 0 || exhaustParticles[i].alpha <= 0.0f) {
                    exhaustParticles[i].active = false;
                }
            }
        }
    }
    glutTimerFunc(25, UpdateExhaustSmoke, 0);
}
// Timer callback: manages 4-phase elevator state machine (ascending, top pause, descending, bottom pause)
void UpdateElevator(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateElevator, 0); return; }
    if (isAnimating) {
        float speed = 0.09f;
        if (elevatorState == 0) {
            elevatorY += speed;
            if (elevatorY >= 9.8f) {
                elevatorY = 9.8f;
                elevatorState = 1;
                elevatorPauseTimer = 0;
            }
        } else if (elevatorState == 1) {
            elevatorPauseTimer++;
            if (elevatorPauseTimer >= 60) {
                elevatorState = 2;
            }
        } else if (elevatorState == 2) {
            elevatorY -= speed;
            if (elevatorY <= -7.5f) {
                elevatorY = -7.5f;
                elevatorState = 3;
                elevatorPauseTimer = 0;
            }
        } else if (elevatorState == 3) {
            elevatorPauseTimer++;
            if (elevatorPauseTimer >= 60) {
                elevatorState = 0;
            }
        }
    }
    glutTimerFunc(25, UpdateElevator, 0);
}

// Scenario 1 master draw: composes all scene elements in order
void Scene()

{
    DrawSky();
    DrawStars();
    DrawCloud1();
    DrawCloud2();
    DrawCloud3();
    DrawCloud4();
    for (int si = 0; si < NUM_SEAGULLS; si++) {
        DrawSeagull(seagulls[si]);
    }
    if (heliScale < 0.35f) {
        DrawHelicopter();
    }
    WatchTower1();
    WatchTower2();
    DrawMountain1();
    DrawMountain2();
    DrawMountain3();
    WindTurbine1();
    WindTurbine2();
    WindTurbine3();
    WindTurbine4();
    WindTurbine5();
    WindTurbine6();
    Lighthouse();
    if (heliScale >= 0.35f && heliScale < 0.85f) {
        DrawHelicopter();
    }
    glColor3ub(34, 139, 34);
    glBegin(GL_QUADS);
    glVertex2f(-60, -7.5f);
    glVertex2f(60, -7.5f);
    glVertex2f(60, 3.0f);
    glVertex2f(-60, 3.0f);
    glEnd();
    Building1();
    Building2();
    DrawGlassElevator();
    DrawHotelNeonSign(-26.2f, -5.3f);
    Building3();
    DrawCoffeeNeonSign(-6.8f, -4.5f);
    Building4();
    Building5();
    Building6();
    DrawCinemaNeonSign(46.4f, -2.6f);
    Billboard();
    Tree1();
    Tree2();
    Tree3();
    Tree4();
    if (heliScale >= 0.85f) {
        DrawHelicopter();
    }
    RailwayTrack();
    Train();
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(-60, -24);
    glVertex2f(60, -24);
    glVertex2f(60, -12);
    glVertex2f(-60, -12);
    glEnd();
    for (int ci = 0; ci < 8; ci++) {
        float cx = -4.2f + ci * 1.1f;
        glColor3ub(245, 245, 245);
        glBegin(GL_QUADS);
        glVertex2f(cx,        -23.5f);
        glVertex2f(cx + 0.65f, -23.5f);
        glVertex2f(cx + 0.65f, -12.5f);
        glVertex2f(cx,        -12.5f);
        glEnd();
    }
    glColor3ub(250, 250, 250);
    glBegin(GL_QUADS);
    glVertex2f(-6.2f, -23.8f);
    glVertex2f(-5.6f, -23.8f);
    glVertex2f(-5.6f, -18.2f);
    glVertex2f(-6.2f, -18.2f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f( 5.6f, -17.8f);
    glVertex2f( 6.2f, -17.8f);
    glVertex2f( 6.2f, -12.2f);
    glVertex2f( 5.6f, -12.2f);
    glEnd();
    glColor3ub(255, 255, 255);
    glLineWidth(2);
    glBegin(GL_LINES);
    for (int i = -60; i < 60; i += 8) {
        if (i >= -8 && i <= 4) continue;
        glVertex2f(i, -18);
        glVertex2f(i + 4, -18);
    }
    glEnd();
    glColor3ub(185, 180, 170);
    glBegin(GL_QUADS);
    glVertex2f(-60.0f, -12.0f);
    glVertex2f( 60.0f, -12.0f);
    glVertex2f( 60.0f,  -9.0f);
    glVertex2f(-60.0f,  -9.0f);
    glEnd();
    glColor3ub(160, 155, 145);
    glLineWidth(1.0f);
    for (int tl = 0; tl < 3; tl++) {
        float ty = -12.0f + tl * 1.0f;
        glBegin(GL_LINES);
        glVertex2f(-60.0f, ty);
        glVertex2f( 60.0f, ty);
        glEnd();
    }
    for (int tv = -60; tv <= 60; tv += 6) {
        glBegin(GL_LINES);
        glVertex2f((float)tv, -12.0f);
        glVertex2f((float)tv,  -9.0f);
        glEnd();
    }
    glColor3ub(220, 215, 205);
    glBegin(GL_QUADS);
    glVertex2f(-60.0f, -9.0f);
    glVertex2f( 60.0f, -9.0f);
    glVertex2f( 60.0f, -8.7f);
    glVertex2f(-60.0f, -8.7f);
    glEnd();
    RoadDivider1();
    DrawTrafficLight();
    StreetLight1();
    StreetLight2();
    StreetLight3();
    StreetLight4();
    Bus();
    CarRed();
    CarGreen();
    CargoTruck();
    SmallTruck();
    CarYellow();
    DrawExhaustSmoke();
    DrawBench(-52.0f);
    DrawSeatedPerson(-52.0f);
    DrawBench(36.0f);
    for (int pi = 0; pi < NUM_PEDS; pi++) {
        DrawPerson(peds[pi], pedWalkTimer);
    }
    Ocean();
    DrawRipples();
    RoadDivider2();
    SmallBoat1();
    CruiseShip();
    CargoShip();
    LuxuryYacht();
    SmallBoat2();
    FishingBoat();
    DrawFog();
    drawWeatherParticles();
    DrawNightOverlay();
}
// Mouse callback for Scenario 1: toggles animation playback on click
void handleMouse(int button, int , int , int )

{
    if (button == GLUT_LEFT_BUTTON) {
        isAnimating = true;
    }
    if (button == GLUT_RIGHT_BUTTON) {
        isAnimating = false;
        isRaining = false;
        isSnowing = false;
        weatherMode = 0;
    }
    glutPostRedisplay();
}
// Keyboard callback for Scenario 1: controls time of day (1/2) and weather modes (3)
void handleKeypress(unsigned char key, int , int )

{
    switch (key) {
    case '1':
        timeOfDay = 0.0f;
        isNight = false;
        break;
    case '2':
        timeOfDay = 0.5f;
        isNight = true;
        break;
    case '3':
        weatherMode = (weatherMode + 1) % 3;
        isRaining = (weatherMode == 1);
        isSnowing = (weatherMode == 2);
        break;
    }
    glutPostRedisplay();
}
// Configures core OpenGL state (background clear color, ortho projection matrix, blend mode)
void init()

{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glPointSize(2.0f);
    glLineWidth(2.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-60, 60, -40, 40, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
const char* kTitle = "Dynamic Coastal City";
// Initializes scenario state, resets animation timers, and pre-allocates particle buffers
void Init()
{
    init();
    glutTimerFunc(0, UpdateSun, 0);
    glutTimerFunc(0, UpdateCloud, 0);
    glutTimerFunc(0, UpdateTurbine, 0);
    glutTimerFunc(0, UpdateTrain, 0);
    glutTimerFunc(0, UpdateBus, 0);
    glutTimerFunc(0, UpdateCarRed, 0);
    glutTimerFunc(0, UpdateCarGreen, 0);
    glutTimerFunc(0, UpdateCarYellow, 0);
    glutTimerFunc(0, UpdateTruckCargo, 0);
    glutTimerFunc(0, UpdateTruckSmall, 0);
    glutTimerFunc(0, UpdateCruiseShip, 0);
    glutTimerFunc(0, UpdateCargoShip, 0);
    glutTimerFunc(0, UpdateYacht, 0);
    glutTimerFunc(0, UpdateSmallBoat1, 0);
    glutTimerFunc(0, UpdateSmallBoat2, 0);
    glutTimerFunc(0, UpdateHelicopter, 0);
    glutTimerFunc(0, UpdateWaves, 0);
    glutTimerFunc(0, updateRain, 0);
    glutTimerFunc(0, UpdateTrafficLight, 0);
    glutTimerFunc(0, UpdateBillboardAd, 0);
    glutTimerFunc(0, UpdatePedestrians, 0);
    glutTimerFunc(0, UpdateSeagulls, 0);
    glutTimerFunc(0, UpdateExhaustSmoke, 0);
    glutTimerFunc(0, UpdateElevator, 0);
}
// Master rendering routine: clears buffers, sets projection, and draws complete scenario composition
void Draw()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLineWidth(1);
    Scene();
    static const char* const hud[] = {
        "1 day        2 night",
        "3  cycle weather (clear / rain / snow)",
        "F1-F4  jump straight to a scenario",
        "SPACE pause    H help    N/B change scene    ESC quit",
        nullptr
    };
    DrawSceneHUD(kTitle, hud);
}
// Scenario keyboard handler: dispatches scenario-specific hotkeys and feature toggles
void Keyboard(unsigned char key, int x, int y)
{
    handleKeypress(key, x, y);
}
// Scenario mouse handler: toggles scene animation play/pause state
void Mouse(int button, int state, int x, int y)
{
    handleMouse(button, state, x, y);
}
}

// ============================================================================
//  SCENARIO 2 -- Downtown Neon District
//  Rainy night city, neon signs, elevated train, wet sidewalks
// ============================================================================
namespace Scenario2 {
constexpr float PI2 = 3.1416f;
int  nightPhase2    = 1;
bool isRaining2     = true;
bool isAnimating2   = true;
int  activeAdSlide2 = 0;
float neonTime2      = 0.0f;
float pedWalkTimer2  = 0.0f;
// Neon sign definitions
struct NeonSign2
{
    float x, y, w, h;
    unsigned char r, g, b;
    float phase;
    const char* text;
};
constexpr int NUM_NEON2 = 6;
NeonSign2 neonSigns2[NUM_NEON2] = {
    { -40.0f,  6.0f, 11.0f, 4.2f, 255,  70, 110, 0.0f, "DINER"  },
    { -12.0f,  9.5f, 10.5f, 4.2f,  70, 210, 255, 1.4f, "ARCADE" },
    {  15.0f, 13.0f, 11.0f, 4.2f, 255, 205,  50, 2.7f, "HOTEL"  },
    {  41.0f,  7.0f, 10.0f, 4.2f, 255, 100, 230, 4.1f, "PIZZA"  },
    { -54.0f,  5.0f,  8.5f, 3.8f,  60, 230, 140, 5.3f, "CAFE"   },
    {  55.0f,  6.0f,  8.0f, 3.8f, 255, 140,  60, 6.2f, "BARBER" }
};

// Skyscraper buildings with window light grid
struct Skyscraper2
{
    float x, baseY, w, topY;
    int   cols, rows;
    bool  lit[6][12];
};
constexpr int NUM_BUILDINGS2 = 6;
Skyscraper2 towers2[NUM_BUILDINGS2] = {
    { -40.0f, -8.0f, 16.0f, 24.0f, 4, 8 , {} },
    { -12.0f, -8.0f, 14.0f, 30.0f, 4, 10, {} },
    {  15.0f, -8.0f, 15.0f, 34.0f, 5, 11, {} },
    {  41.0f, -8.0f, 13.0f, 22.0f, 4, 7 , {} },
    { -54.0f, -8.0f,  9.0f, 18.0f, 3, 5 , {} },
    {  55.0f, -8.0f, 10.0f, 19.0f, 3, 5 , {} }
};
constexpr int NUM_STARS2 = 30;
struct Star2
{
    float x, y;
};
Star2 stars2[NUM_STARS2];
float trainX2        = -80.0f;
bool  trainActive2   = false;
int   trainCooldown2 = 200;

// Street vehicles: taxi, bus, van, motorcycle, limo
enum VehicleType2 { VEH_TAXI = 0, VEH_BUS, VEH_VAN, VEH_MOTORCYCLE, VEH_LIMO };
struct Vehicle2
{
    int type;
    float x, laneY, speed;
    int dir;
    unsigned char r, g, b;
    bool braking;
};
constexpr int NUM_VEHICLES2 = 6;
Vehicle2 vehicles2[NUM_VEHICLES2] = {
    { VEH_TAXI,       -30.0f, -14.0f, 0.16f,  1, 240, 200,   0, false },
    { VEH_TAXI,        20.0f, -17.0f, 0.13f, -1, 240, 200,   0, false },
    { VEH_BUS,        -55.0f, -14.0f, 0.10f,  1,  60, 140, 210, false },
    { VEH_VAN,         45.0f, -17.0f, 0.12f, -1, 210,  90,  60, false },
    { VEH_MOTORCYCLE, -15.0f, -14.0f, 0.24f,  1,  30,  30,  35, false },
    { VEH_LIMO,        38.0f, -17.0f, 0.09f, -1,  20,  20,  25, false }
};
float policeX2         = -62.0f;
float policeLightPhase2 = 0.0f;
float bikeX2      = 68.0f;
float bikeSpeed2  = 0.14f;
float pedalAngle2 = 0.0f;

// Pedestrians with umbrellas
struct Ped2
{
    float x, y, speed, phase;
    int dir;
    bool umbrella;
    unsigned char shirtR, shirtG, shirtB;
    bool  wantsToCross;
    int   crossState;
    float crossT;
};
constexpr int NUM_PEDS2 = 8;
Ped2 peds2[NUM_PEDS2] = {
    { -46.0f, -6.3f, 0.07f, 0.0f,  1, true,  200,  60,  60, true,  0, 0.0f },
    { -20.0f, -6.3f, 0.09f, 1.1f, -1, false,  60, 120, 200, false, 0, 0.0f },
    {   5.0f, -6.3f, 0.06f, 2.2f,  1, true,   90, 180, 100, false, 0, 0.0f },
    {  25.0f, -6.3f, 0.08f, 3.1f, -1, false, 220, 190,  60, true,  0, 0.0f },
    {  48.0f, -6.3f, 0.07f, 4.0f,  1, false, 180,  90, 200, false, 0, 0.0f },
    {  -2.0f, -6.3f, 0.00f, 5.0f,  1, false,  80,  80,  85, false, 0, 0.0f },
    {  15.0f, -6.3f, 0.085f,5.5f, -1, false, 160, 200, 210, true,  0, 0.0f },
    { -33.0f, -6.3f, 0.065f,6.0f,  1, true,   70, 130, 160, false, 0, 0.0f }
};
float performerArmAngle2 = 0.0f;
constexpr int MAX_RAIN2 = 260;
float rainX2[MAX_RAIN2], rainY2[MAX_RAIN2], rainLen2[MAX_RAIN2];
struct SteamPuff2
{
    float x, y, vy, alpha, size; bool active;
};
constexpr int MAX_STEAM2 = 40;
SteamPuff2 steam2[MAX_STEAM2];
int trafficState2 = 0;
int trafficTimer2 = 0;
const char* adTexts2[3] = { "FRESH CITY EATS", "NOVA MOBILE 5G", "GRAND HOTEL - BOOK NOW" };
float heliX2          = -85.0f;
float heliY2          = 22.0f;
float heliScale2      = 0.4f;
float heliPropAngle2  = 0.0f;
float heliSearchAngle2 = 0.0f;
float grateFlicker2   = 0.0f;
bool  grateRumbling2  = false;
int   grateCooldown2  = 220;
struct Ripple2
{
    float x, y, radius, alpha; bool active;
};
constexpr int MAX_RIPPLES2 = 24;
Ripple2 ripples2[MAX_RIPPLES2];
bool  isThunderstorm2      = false;
float lightningFlash2      = 0.0f;
bool  lightningBoltActive2 = false;
float lightningBoltX2      = 0.0f;
int   lightningCooldown2   = 200;
constexpr int   BOLT_NODES2 = 9;
float boltX2[BOLT_NODES2], boltY2[BOLT_NODES2];
float boltBranchX2[3], boltBranchY2[3];
int   boltBranchFrom2[3];
bool tintOn2 = true;
struct Tint2
{
    float r, g, b;
};
Tint2 GetTint2() {
    if (nightPhase2 == 0) return { 1.14f, 0.92f, 0.80f };
    if (nightPhase2 == 1) return { 0.86f, 0.90f, 1.10f };
    return                       { 0.98f, 1.00f, 1.12f };
}
// Calculates scene brightness exposure based on current night phase (dusk, deep night, or dawn)
float Exposure2()
{
    if (nightPhase2 == 0) return 1.22f;
    if (nightPhase2 == 1) return 0.86f;
    return 1.05f;
}
inline unsigned char ClampByte2(float v)
{
    if (v < 0.0f)   return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}
inline void TintRGBA2(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (!tintOn2) { glColor4ub(r, g, b, a); return; }
    Tint2 t = GetTint2();
    float e = Exposure2();
    glColor4ub(ClampByte2(r * t.r * e), ClampByte2(g * t.g * e), ClampByte2(b * t.b * e), a);
}
inline void TintRGB2(unsigned char r, unsigned char g, unsigned char b)
{
    TintRGBA2(r, g, b, 255);
}
struct NoTint2
{
    bool saved;
    NoTint2() : saved(tintOn2)
  {
      tintOn2 = false;
  }
    ~NoTint2() { tintOn2 = saved; }
};
// Helper: renders a filled circle with smooth 24-point perimeter and RGBA color
void FilledCircle2(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    TintRGBA2(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 24;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI2;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}
struct SkyColor2
{
    float r, g, b;
};
SkyColor2 LerpC2(SkyColor2 a, SkyColor2 b, float t)
{
    SkyColor2 o;
    o.r = a.r + (b.r - a.r) * t;
    o.g = a.g + (b.g - a.g) * t;
    o.b = a.b + (b.b - a.b) * t;
    return o;
}
// Draws a smooth volumetric light cone with quadratic edge falloff (for headlights and streetlights)
void DrawLightCone2(float xSource, float ySource, float dx, float dy, float length, float spreadWidth, unsigned char r, unsigned char g, unsigned char b, unsigned char maxAlpha)
{
    NoTint2 emissive;
    float px = -dy, py = dx;
    float xBaseCenter = xSource + dx * length;
    float yBaseCenter = ySource + dy * length;
    glBegin(GL_TRIANGLE_FAN);
    TintRGBA2(r, g, b, maxAlpha);
    glVertex2f(xSource, ySource);
    int segments = 12;
    for (int i = 0; i <= segments; i++) {
        float t = -1.0f + 2.0f * (float)i / (float)segments;
        float xVertex = xBaseCenter + px * (t * spreadWidth);
        float yVertex = yBaseCenter + py * (t * spreadWidth);
        float edgeFade = 1.0f - (t * t);
        float vertexAlpha = maxAlpha * 0.15f * edgeFade;
        TintRGBA2(r, g, b, (unsigned char)vertexAlpha);
        glVertex2f(xVertex, yVertex);
    }
    glEnd();
}

// Renders neon-lit night sky with stars
void DrawSky2()
{
    NoTint2 emissive;
    SkyColor2 duskTop  = {35, 20, 55},  duskBot  = {200, 90, 90};
    SkyColor2 nightTop = {4, 4, 14},    nightBot = {18, 18, 34};
    SkyColor2 dawnTop  = {60, 55, 90},  dawnBot  = {230, 150, 130};
    SkyColor2 top, bot;
    if      (nightPhase2 == 0) { top = duskTop;  bot = duskBot;  }
    else if (nightPhase2 == 1) { top = nightTop; bot = nightBot; }
    else                        { top = dawnTop;  bot = dawnBot;  }
    glBegin(GL_QUADS);
        TintRGB2((unsigned char)top.r, (unsigned char)top.g, (unsigned char)top.b);
        glVertex2f(-60, 40); glVertex2f(60, 40);
        TintRGB2((unsigned char)bot.r, (unsigned char)bot.g, (unsigned char)bot.b);
        glVertex2f(60, -6);  glVertex2f(-60, -6);
    glEnd();
}
// Renders scattered star points in clear night sky areas
void DrawStars2()
{
    NoTint2 emissive;
    int visible = (nightPhase2 == 1) ? NUM_STARS2 : NUM_STARS2 / 4;
    unsigned char alpha = (nightPhase2 == 1) ? 255 : 110;
    TintRGBA2(255, 255, 255, alpha);
    glPointSize(1.6f);
    glBegin(GL_POINTS);
        for (int i = 0; i < visible; i++) glVertex2f(stars2[i].x, stars2[i].y);
    glEnd();
    glPointSize(2.0f);
}
// Renders silhouette backdrop of distant city skyscrapers with tiny illuminated office windows
void DrawSkyline2()
{
    float xs[7] = {-58, -45, -30, -10, 8, 28, 50};
    float hs[7] = {10, 14, 9, 16, 11, 13, 9};
    float ws[7] = {9, 7, 8, 9, 7, 9, 8};
    for (int i = 0; i < 7; i++) {
        TintRGB2(20, 18, 35);
        glBegin(GL_QUADS);
            glVertex2f(xs[i]-ws[i]*0.5f, -6.0f);
            glVertex2f(xs[i]+ws[i]*0.5f, -6.0f);
            glVertex2f(xs[i]+ws[i]*0.5f, -6.0f+hs[i]);
            glVertex2f(xs[i]-ws[i]*0.5f, -6.0f+hs[i]);
        glEnd();
        TintRGB2(255, 230, 140);
        for (int w = 0; w < 3; w++) {
            float wx = xs[i] - ws[i]*0.3f + w*ws[i]*0.3f;
            float wy = -6.0f + hs[i]*0.3f + (w % 2) * hs[i]*0.35f;
            glPointSize(2.0f);
            glBegin(GL_POINTS); glVertex2f(wx, wy); glEnd();
        }
    }
}
// Elevated subway/train rail infrastructure supported by heavy steel pillars and lattice girders
void DrawTrainTrack2()
{
    TintRGB2(60, 60, 68);
    glBegin(GL_QUADS);
        glVertex2f(-60, 8.6f); glVertex2f(60, 8.6f);
        glVertex2f(60, 9.4f);  glVertex2f(-60, 9.4f);
    glEnd();
    TintRGB2(45, 45, 52);
    for (int x = -50; x <= 50; x += 20) {
        glBegin(GL_QUADS);
            glVertex2f(x-0.6f, -6.0f); glVertex2f(x+0.6f, -6.0f);
            glVertex2f(x+0.6f, 8.6f);  glVertex2f(x-0.6f, 8.6f);
        glEnd();
    }
}
// Elevated train passing through the scene
void DrawTrain2()
{
    if (!trainActive2) return;
    float y = 9.0f;
    for (int c = 0; c < 3; c++) {
        float cx = trainX2 + c * 10.2f;
        if (c > 0) {
            TintRGB2(70, 70, 78);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
                glVertex2f(cx - 0.2f, y + 1.2f); glVertex2f(cx, y + 1.2f);
            glEnd();
        }
        TintRGB2(198, 202, 214);
        glBegin(GL_QUADS);
            glVertex2f(cx, y + 0.5f);        glVertex2f(cx + 9.6f, y + 0.5f);
            glVertex2f(cx + 9.6f, y + 3.0f); glVertex2f(cx, y + 3.0f);
        glEnd();
        TintRGB2(120, 124, 136);
        glBegin(GL_QUADS);
            glVertex2f(cx, y);               glVertex2f(cx + 9.6f, y);
            glVertex2f(cx + 9.6f, y + 0.5f); glVertex2f(cx, y + 0.5f);
        glEnd();
        for (int i = 0; i < 4; i++) {
            float wx = cx + 0.9f + i * 2.2f;
            int k = (c * 4 + i);
            unsigned char br = (k % 3 == 0) ? 150 : ((k % 3 == 1) ? 225 : 255);
            TintRGB2(br, (unsigned char)(br * 0.93f), (unsigned char)(br * 0.70f));
            glBegin(GL_QUADS);
                glVertex2f(wx, y + 1.0f);        glVertex2f(wx + 1.5f, y + 1.0f);
                glVertex2f(wx + 1.5f, y + 2.4f); glVertex2f(wx, y + 2.4f);
            glEnd();
        }
    }
    float leadX = trainX2 + 3 * 10.2f - 0.6f;
    FilledCircle2(leadX, y + 1.4f, 0.30f, 255, 250, 210, 255);
    DrawLightCone2(leadX, y + 1.4f, 1.0f, -0.05f, 11.0f, 1.7f, 255, 245, 200, 90);
    glLineWidth(1.0f);
}
// Timer callback: advances elevated train position across the skyline with randomized dispatch intervals
void UpdateTrain2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateTrain2, 0); return; }
    if (isAnimating2) {
        if (trainActive2) {
            trainX2 += 0.55f;
            if (trainX2 > 65.0f) { trainActive2 = false; trainCooldown2 = 250 + rand() % 150; }
        } else {
            trainCooldown2--;
            if (trainCooldown2 <= 0) { trainActive2 = true; trainX2 = -75.0f; }
        }
    }
    glutTimerFunc(20, UpdateTrain2, 0);
}
// Police patrol helicopter hovering in the night sky with spinning rotors and sweeping searchlight
void DrawHelicopter2()
{
    glPushMatrix();
    glTranslatef(heliX2, heliY2, 0.0f);
    glScalef(heliScale2, heliScale2, 1.0f);
    FilledCircle2(0.0f, 0.0f, 1.6f, 30, 35, 45, 255);
    TintRGBA2(120, 200, 230, 220);
    glBegin(GL_POLYGON);
        glVertex2f(0.3f, 0.7f);
        glVertex2f(1.3f, 0.25f);
        glVertex2f(1.1f, -0.5f);
        glVertex2f(0.0f, -0.5f);
    glEnd();
    TintRGB2(25, 30, 38);
    glBegin(GL_QUADS);
        glVertex2f(-1.4f, 0.18f);
        glVertex2f(-3.6f, 0.7f);
        glVertex2f(-3.6f, 0.45f);
        glVertex2f(-1.4f, -0.18f);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(-3.6f, 0.45f);
        glVertex2f(-3.9f, 1.6f);
        glVertex2f(-3.3f, 0.45f);
    glEnd();
    TintRGB2(20, 20, 24);
    glBegin(GL_QUADS);
        glVertex2f(-0.15f, 1.4f);
        glVertex2f(0.15f, 1.4f);
        glVertex2f(0.15f, 1.9f);
        glVertex2f(-0.15f, 1.9f);
    glEnd();
    glLineWidth(2.0f);
    TintRGB2(20, 20, 24);
    glBegin(GL_LINES);
        glVertex2f(-0.7f, -1.4f); glVertex2f(-0.9f, -1.9f);
        glVertex2f(0.7f, -1.4f);  glVertex2f(0.5f, -1.9f);
        glVertex2f(-1.4f, -1.9f); glVertex2f(1.4f, -1.9f);
    glEnd();
    glPushMatrix();
        glTranslatef(0.0f, 1.9f, 0.0f);
        glRotatef(heliPropAngle2, 0.0f, 0.0f, 1.0f);
        TintRGB2(15, 15, 18);
        glBegin(GL_LINES);
            glVertex2f(0.0f, 0.0f); glVertex2f(4.2f, 0.0f);
            glVertex2f(0.0f, 0.0f); glVertex2f(-4.2f, 0.0f);
        glEnd();
    glPopMatrix();
    glPushMatrix();
        glTranslatef(-3.6f, 1.15f, 0.0f);
        glRotatef(heliPropAngle2 * 1.6f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_LINES);
            glVertex2f(0.0f, 0.0f); glVertex2f(0.9f, 0.0f);
            glVertex2f(0.0f, 0.0f); glVertex2f(-0.9f, 0.0f);
        glEnd();
    glPopMatrix();
    bool redPhase = fmodf(neonTime2, 1.2f) < 0.6f;
    float flash = 0.5f + 0.5f * sinf(heliPropAngle2 * 0.05f);
    if (redPhase) FilledCircle2(-0.2f, 1.5f, 0.25f, 255, 40, 40, (unsigned char)(220 * flash));
    else          FilledCircle2(-0.2f, 1.5f, 0.25f, 255, 255, 255, (unsigned char)(220 * flash));
    float sweep = sinf(heliSearchAngle2) * 0.5f;
    DrawLightCone2(0.6f, -0.5f, 0.35f + sweep, -1.0f, 16.0f, 3.0f, 255, 255, 210, 150);
    glPopMatrix();
}
// Timer callback: animates helicopter gentle altitude bobbing and rotor rotation
void UpdateHelicopter2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateHelicopter2, 0); return; }
    if (isAnimating2) {
        heliX2 += 0.22f;
        heliPropAngle2 -= 30.0f;
        if (heliPropAngle2 < -360.0f) heliPropAngle2 += 360.0f;
        heliSearchAngle2 += 0.04f;
        if (heliScale2 < 1.2f) heliScale2 += 0.0012f;
        if (heliX2 > 85.0f) {
            heliX2 = -85.0f - (rand() % 20);
            heliY2 = 18.0f + (rand() % 8);
            heliScale2 = 0.28f + (rand() % 10) / 100.0f;
        }
    }
    glutTimerFunc(25, UpdateHelicopter2, 0);
}
// Renders base geometry and facade shading of a downtown skyscraper
void DrawBuildingBody2(const Skyscraper2& t)
{
    TintRGB2(30, 28, 42);
    glBegin(GL_QUADS);
        glVertex2f(t.x - t.w*0.5f, t.baseY);
        glVertex2f(t.x + t.w*0.5f, t.baseY);
        glVertex2f(t.x + t.w*0.5f, t.topY);
        glVertex2f(t.x - t.w*0.5f, t.topY);
    glEnd();
}
// Renders the illuminated window grid of a skyscraper with individual lit/unlit states
void DrawWindows2(const Skyscraper2& t)
{
    float cellW = t.w / (t.cols + 1);
    float cellH = (t.topY - t.baseY) / (t.rows + 1);
    for (int c = 0; c < t.cols; c++) {
        for (int r = 0; r < t.rows; r++) {
            float wx = t.x - t.w*0.5f + cellW*(c+1) - cellW*0.3f;
            float wy = t.baseY + cellH*(r+1) - cellH*0.3f;
            if (t.lit[c][r]) { NoTint2 emissive; TintRGB2(255, 225, 140); }
            else                                 TintRGB2(15, 15, 24);
            glBegin(GL_QUADS);
                glVertex2f(wx, wy);
                glVertex2f(wx+cellW*0.55f, wy);
                glVertex2f(wx+cellW*0.55f, wy+cellH*0.55f);
                glVertex2f(wx, wy+cellH*0.55f);
            glEnd();
        }
    }
}
// Renders a rooftop air conditioning compressor unit with cooling fan grille
void DrawACUnit2(float x, float y)
{
    TintRGB2(80, 80, 85);
    glBegin(GL_QUADS);
        glVertex2f(x-0.8f, y);      glVertex2f(x+0.8f, y);
        glVertex2f(x+0.8f, y+0.6f); glVertex2f(x-0.8f, y+0.6f);
    glEnd();
    TintRGB2(50, 50, 55);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x-0.5f, y+0.3f); glVertex2f(x+0.5f, y+0.3f);
        glVertex2f(x, y);           glVertex2f(x, y+0.6f);
    glEnd();
}
// Renders exterior zigzagging metal fire escape stairs, landings, and safety railings
void DrawFireEscape2(const Skyscraper2& t, float side)
{
    float x = t.x + side * (t.w * 0.5f + 0.3f);
    TintRGB2(45, 45, 50);
    glLineWidth(1.5f);
    for (int level = 0; level < 4; level++) {
        float y = t.baseY + 3.0f + level * 4.0f;
        if (y > t.topY - 2.0f) break;
        glBegin(GL_LINES);
            glVertex2f(x - side*1.2f, y); glVertex2f(x + side*0.2f, y);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(x - side*1.2f, y); glVertex2f(x - side*1.2f, y+0.5f);
            glVertex2f(x + side*0.2f, y); glVertex2f(x + side*0.2f, y+0.5f);
        glEnd();
        if (level > 0) {
            glBegin(GL_LINES);
                glVertex2f(x - side*1.2f, y); glVertex2f(x + side*0.2f, y - 4.0f);
            glEnd();
        }
    }
}
// Renders rooftop terrace scene with party silhouettes, string fairy lights, and planters
void DrawRooftopParty2(float topX, float topY)
{
    TintRGB2(50, 50, 58);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(topX-3.2f, topY);     glVertex2f(topX+3.2f, topY);
        glVertex2f(topX-3.2f, topY);     glVertex2f(topX-3.2f, topY+0.5f);
        glVertex2f(topX+3.2f, topY);     glVertex2f(topX+3.2f, topY+0.5f);
    glEnd();
    unsigned char cols[3][3] = { {230, 90, 160}, {90, 200, 230}, {230, 200, 90} };
    for (int i = 0; i < 7; i++) {
        float t = (float)i / 6.0f;
        float lx = topX - 2.8f + t * 5.6f;
        float ly = topY + 0.6f + sinf(t * PI2) * 0.3f;
        int c = i % 3;
        float tw = 0.6f + 0.4f * sinf(neonTime2*2.0f + i*0.8f);
        FilledCircle2(lx, ly, 0.15f, cols[c][0], cols[c][1], cols[c][2], (unsigned char)(160 + 90*tw));
    }
    TintRGB2(90, 60, 45);
    glBegin(GL_QUADS);
        glVertex2f(topX-1.2f, topY);      glVertex2f(topX+0.4f, topY);
        glVertex2f(topX+0.4f, topY+0.7f); glVertex2f(topX-1.2f, topY+0.7f);
    glEnd();
    unsigned char people[3][3] = { {40,40,50}, {60,50,70}, {45,55,60} };
    float px[3] = { topX-2.0f, topX+1.5f, topX+2.4f };
    for (int i = 0; i < 3; i++) {
        FilledCircle2(px[i], topY+1.1f, 0.24f, 225, 185, 145, 255);
        TintRGB2(people[i][0], people[i][1], people[i][2]);
        glBegin(GL_QUADS);
            glVertex2f(px[i]-0.2f, topY+0.3f); glVertex2f(px[i]+0.2f, topY+0.3f);
            glVertex2f(px[i]+0.18f, topY+0.85f); glVertex2f(px[i]-0.18f, topY+0.85f);
        glEnd();
    }
}
// Renders rooftop infrastructure: water towers, antennas, access doors, and HVAC units
void DrawRooftopProps2(int index, const Skyscraper2& t)
{
    float topX = t.x, topY = t.topY;
    if (index == 0) {
        TintRGB2(70, 55, 45);
        glBegin(GL_QUADS);
            glVertex2f(topX-1.6f, topY);       glVertex2f(topX+1.6f, topY);
            glVertex2f(topX+1.2f, topY+2.4f);  glVertex2f(topX-1.2f, topY+2.4f);
        glEnd();
        TintRGB2(50, 40, 35);
        glBegin(GL_LINES);
            glVertex2f(topX-1.4f, topY); glVertex2f(topX-0.8f, topY-1.0f);
            glVertex2f(topX+1.4f, topY); glVertex2f(topX+0.8f, topY-1.0f);
        glEnd();
    } else if (index == 1) {
        TintRGB2(180, 180, 185);
        glBegin(GL_LINES);
            glVertex2f(topX, topY); glVertex2f(topX, topY+5.0f);
        glEnd();
        float blink = 0.5f + 0.5f * sinf(neonTime2 * 3.0f);
        FilledCircle2(topX, topY+5.0f, 0.35f, 255, 40, 40, (unsigned char)(120 + 120*blink));
    } else if (index == 2) {
        TintRGB2(40, 40, 48);
        glBegin(GL_QUADS);
            glVertex2f(topX-3.0f, topY);       glVertex2f(topX+3.0f, topY);
            glVertex2f(topX+3.0f, topY+0.4f);  glVertex2f(topX-3.0f, topY+0.4f);
        glEnd();
        TintRGB2(255, 210, 60);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(topX-1.0f, topY+0.5f); glVertex2f(topX-1.0f, topY+1.6f);
            glVertex2f(topX+1.0f, topY+0.5f); glVertex2f(topX+1.0f, topY+1.6f);
            glVertex2f(topX-1.0f, topY+1.05f); glVertex2f(topX+1.0f, topY+1.05f);
        glEnd();
    } else if (index == 4) {
        DrawRooftopParty2(topX, topY);
    } else {
        DrawACUnit2(topX - 1.5f, topY);
        DrawACUnit2(topX + 1.2f, topY);
    }
}
// Street-level underground metro entrance with tiled stairs, handrails, and globe beacon lights
void DrawSubwayEntrance2()
{
    float x = towers2[3].x - towers2[3].w*0.5f - 2.2f;
    float baseY = -8.0f;
    TintRGB2(40, 38, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f, baseY);       glVertex2f(x+3.0f, baseY);
        glVertex2f(x+2.4f, baseY+2.2f);  glVertex2f(x-2.4f, baseY+2.2f);
    glEnd();
    TintRGB2(255, 210, 60);
    DrawTextCentered(x, baseY+1.0f, GLUT_BITMAP_HELVETICA_12, "SUBWAY");
    TintRGB2(5, 5, 10);
    glBegin(GL_QUADS);
        glVertex2f(x-1.8f, baseY-0.2f); glVertex2f(x+1.8f, baseY-0.2f);
        glVertex2f(x+1.4f, baseY+0.6f); glVertex2f(x-1.4f, baseY+0.6f);
    glEnd();
}
// Stylized graffiti murals and street tags painted on alleyway brick walls
void DrawGraffiti2()
{
    float x = -45.0f, y = -7.3f;
    TintRGBA2(230, 80, 150, 190);
    FilledCircle2(x, y, 1.0f, 230, 80, 150, 190);
    TintRGBA2(80, 180, 230, 170);
    FilledCircle2(x+1.2f, y+0.3f, 0.8f, 80, 180, 230, 170);
    TintRGB2(255, 220, 60);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x-1.2f, y-0.5f); glVertex2f(x-0.5f, y+0.8f);
        glVertex2f(x+0.3f, y-0.3f); glVertex2f(x+1.5f, y+0.9f);
    glEnd();
}
// Illuminated ground-floor American diner interior with counter seating, stools, and warm yellow glow
void DrawDinerInterior2()
{
    float x = -35.0f, y = -7.0f, w = 5.0f, h = 2.6f;
    TintRGB2(255, 200, 140);
    glBegin(GL_QUADS);
        glVertex2f(x-w*0.5f, y);      glVertex2f(x+w*0.5f, y);
        glVertex2f(x+w*0.5f, y+h);    glVertex2f(x-w*0.5f, y+h);
    glEnd();
    TintRGB2(60, 45, 35);
    glBegin(GL_QUADS);
        glVertex2f(x-w*0.5f+0.3f, y+0.3f); glVertex2f(x+w*0.5f-0.3f, y+0.3f);
        glVertex2f(x+w*0.5f-0.3f, y+1.0f); glVertex2f(x-w*0.5f+0.3f, y+1.0f);
    glEnd();
    for (int i = 0; i < 2; i++) {
        float px = x - 1.2f + i * 1.6f;
        FilledCircle2(px, y+1.7f, 0.32f, 45, 38, 32, 255);
        TintRGB2(45, 38, 32);
        glBegin(GL_QUADS);
            glVertex2f(px-0.24f, y+1.0f); glVertex2f(px+0.24f, y+1.0f);
            glVertex2f(px+0.2f, y+1.35f); glVertex2f(px-0.2f, y+1.35f);
        glEnd();
    }
}
// Draws an individual glowing storefront neon sign with letter strokes and bloom backplate
void DrawNeonSign2(const NeonSign2& s, float pulse)
{
    NoTint2 emissive;
    for (int i = 3; i >= 1; i--) {
        float pad = i * 0.9f;
        unsigned char alpha = (unsigned char)(40.0f * pulse / i);
        TintRGBA2(s.r, s.g, s.b, alpha);
        glBegin(GL_QUADS);
            glVertex2f(s.x - s.w*0.5f - pad, s.y - s.h*0.5f - pad*0.4f);
            glVertex2f(s.x + s.w*0.5f + pad, s.y - s.h*0.5f - pad*0.4f);
            glVertex2f(s.x + s.w*0.5f + pad, s.y + s.h*0.5f + pad*0.4f);
            glVertex2f(s.x - s.w*0.5f - pad, s.y + s.h*0.5f + pad*0.4f);
        glEnd();
    }
    TintRGBA2(10, 10, 14, 230);
    glBegin(GL_QUADS);
        glVertex2f(s.x - s.w*0.5f, s.y - s.h*0.5f);
        glVertex2f(s.x + s.w*0.5f, s.y - s.h*0.5f);
        glVertex2f(s.x + s.w*0.5f, s.y + s.h*0.5f);
        glVertex2f(s.x - s.w*0.5f, s.y + s.h*0.5f);
    glEnd();
    unsigned char tr = (unsigned char)(s.r * pulse);
    unsigned char tg = (unsigned char)(s.g * pulse);
    unsigned char tb = (unsigned char)(s.b * pulse);
    TintRGB2(tr, tg, tb);
    glLineWidth(3.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(s.x - s.w*0.5f, s.y - s.h*0.5f);
        glVertex2f(s.x + s.w*0.5f, s.y - s.h*0.5f);
        glVertex2f(s.x + s.w*0.5f, s.y + s.h*0.5f);
        glVertex2f(s.x - s.w*0.5f, s.y + s.h*0.5f);
    glEnd();
    TintRGB2(tr, tg, tb);
    DrawTextCentered(s.x, s.y - 0.5f, GLUT_BITMAP_HELVETICA_18, s.text);
}
// Iterates through all downtown neon signs (DINER, ARCADE, HOTEL, PIZZA, etc.) and renders them
void DrawAllNeon2()
{
    for (int i = 0; i < NUM_NEON2; i++) {
        float pulse = 0.55f + 0.45f * sinf(neonTime2 + neonSigns2[i].phase);
        DrawNeonSign2(neonSigns2[i], pulse);
    }
}
// Draws soft colored diffuse glow pools on wet pavement underneath neon storefronts
void DrawPuddleGlow2()
{
    NoTint2 emissive;
    if (!isRaining2) return;
    for (int i = 0; i < NUM_NEON2; i++) {
        float pulse = 0.4f + 0.3f * sinf(neonTime2 + neonSigns2[i].phase);
        TintRGBA2(neonSigns2[i].r, neonSigns2[i].g, neonSigns2[i].b, (unsigned char)(70*pulse));
        glBegin(GL_QUADS);
            glVertex2f(neonSigns2[i].x - neonSigns2[i].w*0.6f, -8.0f);
            glVertex2f(neonSigns2[i].x + neonSigns2[i].w*0.6f, -8.0f);
            glVertex2f(neonSigns2[i].x + neonSigns2[i].w*0.3f, -11.5f);
            glVertex2f(neonSigns2[i].x - neonSigns2[i].w*0.3f, -11.5f);
        glEnd();
    }
}
// Initializes puddle ripple pool array
void InitRipples2()
{
    for (int i = 0; i < MAX_RIPPLES2; i++) ripples2[i].active = false;
}
// Spawns a new raindrop impact ripple ring in a random street puddle
void SpawnRipple2()
{
    for (int i = 0; i < MAX_RIPPLES2; i++) {
        if (!ripples2[i].active) {
            ripples2[i].active = true;
            ripples2[i].x = -58.0f + (rand() % 1160) / 10.0f;
            ripples2[i].y = -19.5f + (rand() % 100) / 10.0f;
            ripples2[i].radius = 0.05f;
            ripples2[i].alpha = 160.0f;
            return;
        }
    }
}
// Renders expanding circular ripple rings on wet street puddles
void DrawRipples2()
{
    if (!isRaining2) return;
    TintRGBA2(200, 220, 240, 0);
    glLineWidth(1.2f);
    for (int i = 0; i < MAX_RIPPLES2; i++) {
        if (!ripples2[i].active) continue;
        TintRGBA2(200, 220, 240, (unsigned char)ripples2[i].alpha);
        glBegin(GL_LINE_LOOP);
            for (int s = 0; s < 16; s++) {
                float a = (float)s / 16 * 2.0f * PI2;
                glVertex2f(ripples2[i].x + ripples2[i].radius*cos(a), ripples2[i].y + ripples2[i].radius*0.35f*sin(a));
            }
        glEnd();
    }
}
// Timer callback: advances ripple ring radii and fades out their alpha
void UpdateRipples2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateRipples2, 0); return; }
    if (isAnimating2 && isRaining2) {
        if (rand() % 8 == 0) SpawnRipple2();
        for (int i = 0; i < MAX_RIPPLES2; i++) {
            if (!ripples2[i].active) continue;
            ripples2[i].radius += 0.045f;
            ripples2[i].alpha -= 4.5f;
            if (ripples2[i].alpha <= 0.0f) ripples2[i].active = false;
        }
    }
    glutTimerFunc(30, UpdateRipples2, 0);
}
// Generates random branching vertices for a lightning strike during thunderstorms
void SpawnLightningBolt2()
{
    float x = lightningBoltX2, y = 40.0f;
    for (int i = 0; i < BOLT_NODES2; i++) {
        boltX2[i] = x;
        boltY2[i] = y;
        x += (rand() % 100 - 50) / 22.0f;
        y -= 5.0f;
    }
    for (int b = 0; b < 3; b++) {
        int from = 2 + rand() % (BOLT_NODES2 - 4);
        boltBranchFrom2[b] = from;
        boltBranchX2[b] = boltX2[from] + ((rand() % 100) - 50) / 14.0f;
        boltBranchY2[b] = boltY2[from] - 2.0f - (rand() % 200) / 100.0f;
    }
}
// Draws jagged, intense multi-point lightning bolt during thunderstorm strikes
void DrawLightningBolt2()
{
    NoTint2 emissive;
    if (!lightningBoltActive2) return;
    TintRGBA2(150, 180, 255, (unsigned char)(90 * lightningFlash2));
    glLineWidth(6.0f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i < BOLT_NODES2; i++) glVertex2f(boltX2[i], boltY2[i]);
    glEnd();
    TintRGBA2(255, 255, 255, (unsigned char)(235 * lightningFlash2));
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i < BOLT_NODES2; i++) glVertex2f(boltX2[i], boltY2[i]);
    glEnd();
    glLineWidth(1.4f);
    glBegin(GL_LINES);
        for (int b = 0; b < 3; b++) {
            int f = boltBranchFrom2[b];
            glVertex2f(boltX2[f], boltY2[f]);
            glVertex2f(boltBranchX2[b], boltBranchY2[b]);
        }
    glEnd();
    glLineWidth(1.0f);
}
// Full-screen white/blue flash overlay during a lightning strike
void DrawLightningFlash2()
{
    NoTint2 emissive;
    if (lightningFlash2 <= 0.01f) return;
    TintRGBA2(220, 230, 255, (unsigned char)(lightningFlash2 * 170));
    glBegin(GL_QUADS);
        glVertex2f(-60, -40); glVertex2f(60, -40);
        glVertex2f(60, 40);   glVertex2f(-60, 40);
    glEnd();
}
// Timer callback: manages thunderstorm lightning frequency, strike duration, and cooldown
void UpdateLightning2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateLightning2, 0); return; }
    if (isAnimating2 && isThunderstorm2) {
        lightningCooldown2--;
        if (lightningCooldown2 <= 0) {
            lightningFlash2 = 1.0f;
            lightningBoltActive2 = true;
            lightningBoltX2 = -50.0f + (rand() % 1000) / 10.0f;
            SpawnLightningBolt2();
            lightningCooldown2 = 200 + rand() % 400;
        }
        lightningFlash2 *= 0.85f;
        if (lightningFlash2 < 0.05f) lightningBoltActive2 = false;
    } else {
        lightningFlash2 = 0.0f;
        lightningBoltActive2 = false;
    }
    glutTimerFunc(30, UpdateLightning2, 0);
}
// Large elevated neon billboard displaying animated high-contrast advertisements
void DrawBillboard2()
{
    NoTint2 emissive;
    float x = 15.0f, y = 24.0f, w = 16.0f, h = 5.0f;
    TintRGB2(8, 8, 10);
    glBegin(GL_QUADS);
        glVertex2f(x-w*0.5f, y-h*0.5f); glVertex2f(x+w*0.5f, y-h*0.5f);
        glVertex2f(x+w*0.5f, y+h*0.5f); glVertex2f(x-w*0.5f, y+h*0.5f);
    glEnd();
    TintRGB2(0, 220, 255);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x-w*0.5f, y-h*0.5f); glVertex2f(x+w*0.5f, y-h*0.5f);
        glVertex2f(x+w*0.5f, y+h*0.5f); glVertex2f(x-w*0.5f, y+h*0.5f);
    glEnd();
    TintRGB2(255, 255, 255);
    DrawTextCentered(x, y-0.5f, GLUT_BITMAP_HELVETICA_18, adTexts2[activeAdSlide2]);
}
// Timer callback: advances neon flickering phases and pulse oscillation timers
void UpdateNeon2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateNeon2, 0); return; }
    if (isAnimating2) neonTime2 += 0.05f;
    glutTimerFunc(20, UpdateNeon2, 0);
}
// Timer callback: cycles slides and animations on the neon billboard
void UpdateBillboard2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateBillboard2, 0); return; }
    static int t = 0;
    if (isAnimating2) {
        t++;
        if (t > 150) { t = 0; activeAdSlide2 = (activeAdSlide2 + 1) % 3; }
    }
    glutTimerFunc(30, UpdateBillboard2, 0);
}
// Dedicated asphalt bicycle lane marked with green surface paint and painted bike symbols
void DrawBikeLane2()
{
    TintRGB2(30, 110, 60);
    glBegin(GL_QUADS);
        glVertex2f(-60, -9.0f); glVertex2f(60, -9.0f);
        glVertex2f(60, -8.0f);  glVertex2f(-60, -8.0f);
    glEnd();
}

// Road surface with lane markings and wet reflections
void DrawRoad2()
{
    TintRGB2(35, 35, 38);
    glBegin(GL_QUADS);
        glVertex2f(-60, -20); glVertex2f(60, -20);
        glVertex2f(60, -9);   glVertex2f(-60, -9);
    glEnd();
    TintRGB2(230, 220, 80);
    glBegin(GL_LINES);
        for (int x = -60; x < 60; x += 6) {
            if (x >= -6 && x <= 4) continue;
            glVertex2f((float)x, -14.5f); glVertex2f((float)x+3, -14.5f);
        }
    glEnd();
    TintRGB2(230, 230, 230);
    for (int i = 0; i < 6; i++) {
        float cx = -4.5f + i * 1.6f;
        glBegin(GL_QUADS);
            glVertex2f(cx, -19.5f);      glVertex2f(cx+0.9f, -19.5f);
            glVertex2f(cx+0.9f, -9.5f);  glVertex2f(cx, -9.5f);
        glEnd();
    }
    TintRGB2(70, 68, 72);
    glBegin(GL_QUADS);
        glVertex2f(-60, -9); glVertex2f(60, -9);
        glVertex2f(60, -5);  glVertex2f(-60, -5);
    glEnd();
}
// Cast-iron sidewalk utility cover with venting slots
void DrawStreetVent2()
{
    TintRGB2(35, 35, 38);
    glBegin(GL_QUADS);
        glVertex2f(-2.8f, -8.3f); glVertex2f(-1.2f, -8.3f);
        glVertex2f(-1.2f, -8.0f); glVertex2f(-2.8f, -8.0f);
    glEnd();
}
// Downtown traffic light cantilever post with overhead signals and walk/don't-walk indicators
void DrawTrafficLight2()
{
    NoTint2 emissive;
    float x = 6.0f, y = -9.0f;
    TintRGB2(30, 30, 30);
    glBegin(GL_QUADS);
        glVertex2f(x-0.3f, y);       glVertex2f(x+0.3f, y);
        glVertex2f(x+0.3f, y+4.0f);  glVertex2f(x-0.3f, y+4.0f);
    glEnd();
    TintRGB2(20, 20, 20);
    glBegin(GL_QUADS);
        glVertex2f(x-0.8f, y+4.0f);  glVertex2f(x+0.8f, y+4.0f);
        glVertex2f(x+0.8f, y+6.4f);  glVertex2f(x-0.8f, y+6.4f);
    glEnd();
    FilledCircle2(x, y+6.0f, 0.35f, (unsigned char)(trafficState2==2?255:60), 40, 40, 255);
    FilledCircle2(x, y+5.2f, 0.35f, (unsigned char)(trafficState2==1?255:60), (unsigned char)(trafficState2==1?230:60), 40, 255);
    FilledCircle2(x, y+4.4f, 0.35f, 40, (unsigned char)(trafficState2==0?255:60), 40, 255);
}
// Timer callback: advances urban traffic signal state machine through green, yellow, and red intervals
void UpdateTrafficLight2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateTrafficLight2, 0); return; }
    if (isAnimating2) {
        trafficTimer2++;
        if      (trafficState2 == 0 && trafficTimer2 > 150) { trafficState2 = 1; trafficTimer2 = 0; }
        else if (trafficState2 == 1 && trafficTimer2 > 40)  { trafficState2 = 2; trafficTimer2 = 0; }
        else if (trafficState2 == 2 && trafficTimer2 > 150) { trafficState2 = 0; trafficTimer2 = 0; }
    }
    glutTimerFunc(30, UpdateTrafficLight2, 0);
}
// Pedestrian crossing signal housing showing lighted walking person or stop hand
void DrawCrosswalkSignal2()
{
    NoTint2 emissive;
    float x = -8.0f, y = -8.5f;
    TintRGB2(30, 30, 32);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, y);      glVertex2f(x+0.5f, y);
        glVertex2f(x+0.5f, y+1.2f); glVertex2f(x-0.5f, y+1.2f);
    glEnd();
    bool walkSignal = (trafficState2 == 2);
    if (walkSignal) {
        TintRGB2(60, 220, 90);
        glBegin(GL_QUADS);
            glVertex2f(x-0.15f, y+0.3f); glVertex2f(x+0.15f, y+0.3f);
            glVertex2f(x+0.15f, y+0.8f); glVertex2f(x-0.15f, y+0.8f);
        glEnd();
        FilledCircle2(x, y+0.95f, 0.12f, 60, 220, 90, 255);
    } else {
        TintRGB2(230, 60, 60);
        glBegin(GL_QUADS);
            glVertex2f(x-0.2f, y+0.35f); glVertex2f(x+0.2f, y+0.35f);
            glVertex2f(x+0.2f, y+0.85f); glVertex2f(x-0.2f, y+0.85f);
        glEnd();
    }
}
// Detailed parked passenger car along the sidewalk curb with windshield reflections
void DrawParkedCar2(float x, unsigned char r, unsigned char g, unsigned char b)
{
    float y = -11.2f;
    TintRGB2(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-2.3f, y);      glVertex2f(x+2.3f, y);
        glVertex2f(x+2.3f, y+1.1f); glVertex2f(x-2.3f, y+1.1f);
    glEnd();
    TintRGB2(30, 30, 35);
    glBegin(GL_QUADS);
        glVertex2f(x-1.3f, y+1.1f); glVertex2f(x+1.0f, y+1.1f);
        glVertex2f(x+0.8f, y+1.8f); glVertex2f(x-1.1f, y+1.8f);
    glEnd();
    FilledCircle2(x-1.4f, y-0.1f, 0.5f, 15, 15, 15, 255);
    FilledCircle2(x+1.4f, y-0.1f, 0.5f, 15, 15, 15, 255);
}
// Draws row of parked vehicles along the downtown street curb
void DrawParkedCars2()
{
    DrawParkedCar2(-28.0f, 150, 150, 155);
    DrawParkedCar2(-22.0f,  90,  40,  40);
}
// Public sidewalk metal mesh waste receptacle
void DrawTrashCan2(float x)
{
    float y = -9.4f;
    TintRGB2(60, 90, 60);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, y);       glVertex2f(x+0.5f, y);
        glVertex2f(x+0.45f, y+1.1f); glVertex2f(x-0.45f, y+1.1f);
    glEnd();
    TintRGB2(40, 60, 40);
    glBegin(GL_QUADS);
        glVertex2f(x-0.55f, y+1.1f);  glVertex2f(x+0.55f, y+1.1f);
        glVertex2f(x+0.55f, y+1.25f); glVertex2f(x-0.55f, y+1.25f);
    glEnd();
}
// Classic red roadside cast-iron fire hydrant with valve caps
void DrawFireHydrant2(float x)
{
    float y = -9.4f;
    TintRGB2(210, 50, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-0.25f, y);      glVertex2f(x+0.25f, y);
        glVertex2f(x+0.22f, y+0.7f); glVertex2f(x-0.22f, y+0.7f);
    glEnd();
    FilledCircle2(x, y+0.75f, 0.18f, 210, 50, 50, 255);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-0.35f, y+0.4f); glVertex2f(x+0.35f, y+0.4f);
    glEnd();
}
// Corner newsstand kiosk with colorful magazine racks, newspapers, and canopy awning
void DrawNewsstand2()
{
    float x = -15.0f, y = -9.0f;
    TintRGB2(90, 70, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-1.6f, y);      glVertex2f(x+1.6f, y);
        glVertex2f(x+1.6f, y+1.6f); glVertex2f(x-1.6f, y+1.6f);
    glEnd();
    TintRGB2(200, 60, 60);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-2.0f, y+1.6f); glVertex2f(x+2.0f, y+1.6f); glVertex2f(x, y+2.6f);
    glEnd();
    FilledCircle2(x-0.9f, y+2.0f, 0.28f, 220, 180, 140, 255);
    TintRGB2(70, 90, 130);
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(x-0.9f, y+1.75f); glVertex2f(x-0.9f, y+0.9f); glEnd();
}
// Striped canvas storefront sunshade awning over sidewalk entrance
void DrawAwning2(float x1, float x2, float y)
{
    TintRGB2(150, 40, 50);
    glBegin(GL_QUADS);
        glVertex2f(x1, y);         glVertex2f(x2, y);
        glVertex2f(x2-0.3f, y+0.5f); glVertex2f(x1+0.3f, y+0.5f);
    glEnd();
    TintRGB2(230, 230, 230);
    for (float sx = x1+0.3f; sx < x2-0.5f; sx += 1.0f) {
        glBegin(GL_QUADS);
            glVertex2f(sx, y);              glVertex2f(sx+0.5f, y);
            glVertex2f(sx+0.44f, y+0.5f);   glVertex2f(sx+0.06f, y+0.5f);
        glEnd();
    }
}
// Metal subway ventilation grate in sidewalk
void DrawSubwayGrate2()
{
    float x = -16.0f, y = -10.6f;
    TintRGB2(20, 20, 22);
    glBegin(GL_QUADS);
        glVertex2f(x-1.8f, y-0.5f); glVertex2f(x+1.8f, y-0.5f);
        glVertex2f(x+1.8f, y+0.5f); glVertex2f(x-1.8f, y+0.5f);
    glEnd();
    glLineWidth(1.5f);
    for (float sx = x-1.6f; sx <= x+1.6f; sx += 0.5f) {
        if (grateRumbling2) {
            unsigned char glowA = (unsigned char)(90 + 60 * sinf(grateFlicker2 * 20.0f + sx));
            TintRGBA2(255, 220, 150, glowA);
        } else {
            TintRGB2(45, 45, 48);
        }
        glBegin(GL_LINES);
            glVertex2f(sx, y-0.45f); glVertex2f(sx, y+0.45f);
        glEnd();
    }
}
// Timer callback: manages intermittent steam release timing from subway grates
void UpdateSubwayGrate2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateSubwayGrate2, 0); return; }
    if (isAnimating2) {
        grateCooldown2--;
        if (grateRumbling2) {
            grateFlicker2 += 0.05f;
            if (grateCooldown2 <= 0) { grateRumbling2 = false; grateCooldown2 = 220 + rand() % 200; }
        } else {
            if (grateCooldown2 <= 0) { grateRumbling2 = true; grateFlicker2 = 0.0f; grateCooldown2 = 60; }
        }
    }
    glutTimerFunc(30, UpdateSubwayGrate2, 0);
}
inline float VehicleHalfLen2(int type)
{
    switch (type) {
        case VEH_BUS:        return 4.6f;
        case VEH_LIMO:       return 4.3f;
        case VEH_VAN:        return 3.1f;
        case VEH_MOTORCYCLE: return 1.3f;
        default:             return 2.7f;
    }
}
// Distinctive yellow city taxi cab with roof checkerboard sign and headlights
void DrawTaxiShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b, int dir)
{
    TintRGB2(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-2.6f, y);      glVertex2f(x+2.6f, y);
        glVertex2f(x+2.6f, y+1.3f); glVertex2f(x-2.6f, y+1.3f);
    glEnd();
    TintRGB2(30, 30, 30);
    glBegin(GL_QUADS);
        glVertex2f(x-1.6f, y+1.3f); glVertex2f(x+1.6f, y+1.3f);
        glVertex2f(x+1.2f, y+2.2f); glVertex2f(x-1.2f, y+2.2f);
    glEnd();
    TintRGB2(255, 220, 80);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, y+2.2f); glVertex2f(x+0.5f, y+2.2f);
        glVertex2f(x+0.5f, y+2.5f); glVertex2f(x-0.5f, y+2.5f);
    glEnd();
    FilledCircle2(x-1.6f, y-0.1f, 0.55f, 15, 15, 15, 255);
    FilledCircle2(x+1.6f, y-0.1f, 0.55f, 15, 15, 15, 255);
    float lx = dir > 0 ? x+2.6f : x-2.6f;
    FilledCircle2(lx, y+0.6f, 0.2f, 255, 255, 200, 255);
}
// City transit bus with large passenger side windows, destination board, and taillights
void DrawBusShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b)
{
    TintRGB2(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-4.5f, y);      glVertex2f(x+4.5f, y);
        glVertex2f(x+4.5f, y+2.6f); glVertex2f(x-4.5f, y+2.6f);
    glEnd();
    TintRGB2(180, 220, 255);
    for (int i = 0; i < 4; i++) {
        float wx = x - 3.6f + i * 2.1f;
        glBegin(GL_QUADS);
            glVertex2f(wx, y+1.4f);      glVertex2f(wx+1.3f, y+1.4f);
            glVertex2f(wx+1.3f, y+2.2f); glVertex2f(wx, y+2.2f);
        glEnd();
    }
    FilledCircle2(x-3.0f, y-0.1f, 0.6f, 15, 15, 15, 255);
    FilledCircle2(x+3.0f, y-0.1f, 0.6f, 15, 15, 15, 255);
}
// Commercial delivery cargo van moving down the traffic lane
void DrawVanShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b)
{
    TintRGB2(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f, y);      glVertex2f(x+3.0f, y);
        glVertex2f(x+3.0f, y+2.0f); glVertex2f(x-3.0f, y+2.0f);
    glEnd();
    TintRGB2(200, 225, 255);
    glBegin(GL_QUADS);
        glVertex2f(x+1.4f, y+1.1f); glVertex2f(x+2.6f, y+1.1f);
        glVertex2f(x+2.6f, y+1.8f); glVertex2f(x+1.4f, y+1.8f);
    glEnd();
    FilledCircle2(x-1.8f, y-0.1f, 0.55f, 15, 15, 15, 255);
    FilledCircle2(x+1.8f, y-0.1f, 0.55f, 15, 15, 15, 255);
}
// Motorcyclist with helmet speeding along the downtown street with bright single headlamp
void DrawMotorcycleShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b, int dir)
{
    TintRGB2(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-1.2f, y+0.5f); glVertex2f(x+1.0f, y+0.5f);
        glVertex2f(x+1.0f, y+0.9f); glVertex2f(x-1.2f, y+0.9f);
    glEnd();
    TintRGB2(25, 25, 30);
    glBegin(GL_QUADS);
        glVertex2f(x-0.3f, y+0.9f); glVertex2f(x+0.3f, y+0.9f);
        glVertex2f(x+0.2f, y+1.6f); glVertex2f(x-0.2f, y+1.6f);
    glEnd();
    FilledCircle2(x, y+1.85f, 0.22f, 25, 25, 30, 255);
    FilledCircle2(x-1.0f, y+0.2f, 0.45f, 15, 15, 15, 255);
    FilledCircle2(x+0.9f, y+0.2f, 0.45f, 15, 15, 15, 255);
    float lx = dir > 0 ? x+1.0f : x-1.2f;
    FilledCircle2(lx, y+0.6f, 0.15f, 255, 255, 200, 255);
}
// Luxury extended limousine with tinted panoramic windows
void DrawLimoShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b)
{
    TintRGB2(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-4.2f, y);      glVertex2f(x+4.2f, y);
        glVertex2f(x+4.2f, y+1.1f); glVertex2f(x-4.2f, y+1.1f);
    glEnd();
    TintRGB2(20, 20, 25);
    glBegin(GL_QUADS);
        glVertex2f(x-2.6f, y+1.1f); glVertex2f(x+2.0f, y+1.1f);
        glVertex2f(x+1.6f, y+1.7f); glVertex2f(x-2.2f, y+1.7f);
    glEnd();
    TintRGB2(150, 190, 220);
    glBegin(GL_QUADS);
        glVertex2f(x-2.0f, y+1.2f); glVertex2f(x+1.4f, y+1.2f);
        glVertex2f(x+1.1f, y+1.6f); glVertex2f(x-1.7f, y+1.6f);
    glEnd();
    FilledCircle2(x-2.8f, y-0.1f, 0.55f, 15, 15, 15, 255);
    FilledCircle2(x+0.0f, y-0.1f, 0.55f, 15, 15, 15, 255);
    FilledCircle2(x+2.8f, y-0.1f, 0.55f, 15, 15, 15, 255);
}
// Volumetric twin headlight beam cones cast forward onto the wet asphalt from vehicles
void DrawHeadlightCone2(const Vehicle2& v)
{
    NoTint2 emissive;
    float nose = v.x + v.dir * VehicleHalfLen2(v.type);
    float ly   = v.laneY + 0.55f;
    float len  = 13.0f * v.dir;
    unsigned char a = isRaining2 ? 40 : 24;
    glBegin(GL_TRIANGLES);
        glColor4ub(255, 240, 200, (unsigned char)(a * 2));
        glVertex2f(nose, ly);
        glColor4ub(255, 240, 200, 0);
        glVertex2f(nose + len, ly + 2.6f);
        glVertex2f(nose + len, ly - 1.9f);
    glEnd();
    FilledCircle2(nose, ly, 0.85f, 255, 240, 200, 55);
    FilledCircle2(nose, ly, 0.20f, 255, 250, 230, 255);
    if (isRaining2) {
        glColor4ub(255, 236, 190, 46);
        glBegin(GL_QUADS);
            glVertex2f(nose,               v.laneY - 1.1f);
            glVertex2f(nose + len * 0.75f, v.laneY - 1.1f);
            glVertex2f(nose + len * 0.75f, v.laneY - 2.6f);
            glVertex2f(nose,               v.laneY - 2.3f);
        glEnd();
    }
}
// Street-level vehicles: taxi, bus, van, motorcycle, limo
void DrawVehicles2()
{
    for (int i = 0; i < NUM_VEHICLES2; i++) DrawHeadlightCone2(vehicles2[i]);
    for (int i = 0; i < NUM_VEHICLES2; i++) {
        Vehicle2& v = vehicles2[i];
        if      (v.type == VEH_TAXI) DrawTaxiShape2(v.x, v.laneY, v.r, v.g, v.b, v.dir);
        else if (v.type == VEH_BUS)  DrawBusShape2(v.x, v.laneY, v.r, v.g, v.b);
        else if (v.type == VEH_VAN)  DrawVanShape2(v.x, v.laneY, v.r, v.g, v.b);
        else if (v.type == VEH_MOTORCYCLE) DrawMotorcycleShape2(v.x, v.laneY, v.r, v.g, v.b, v.dir);
        else                          DrawLimoShape2(v.x, v.laneY, v.r, v.g, v.b);
        if (v.braking) {
            float rear = v.x - v.dir * (VehicleHalfLen2(v.type) - 0.2f);
            float ly   = v.laneY + 0.55f;
            FilledCircle2(rear, ly, 0.9f, 255, 40, 30, 55);
            FilledCircle2(rear, ly, 0.22f, 255, 70, 50, 245);
        }
    }
}
inline float StopLineFor2(int dir)
{
    return (dir > 0) ? -6.8f : 6.8f;
}
// Timer callback: updates vehicle street physics, speeds, braking, and wrap-around respawning
void UpdateVehicles2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateVehicles2, 0); return; }
    if (isAnimating2) {
        for (int i = 0; i < NUM_VEHICLES2; i++) {
            Vehicle2& v = vehicles2[i];
            float nextX = v.x + v.speed * v.dir;
            bool  braking = false;
            if (trafficState2 != 0) {
                float line = StopLineFor2(v.dir);
                bool before     = (v.dir > 0) ? (v.x    <= line) : (v.x    >= line);
                bool wouldCross = (v.dir > 0) ? (nextX  >  line) : (nextX  <  line);
                if (before && wouldCross) { nextX = line; braking = true; }
            }
            for (int j = 0; j < NUM_VEHICLES2; j++) {
                if (j == i) continue;
                const Vehicle2& o = vehicles2[j];
                if (o.dir != v.dir) continue;
                if (fabsf(o.laneY - v.laneY) > 0.5f) continue;
                float ahead = (o.x - v.x) * v.dir;
                if (ahead <= 0.0f) continue;
                float minGap = VehicleHalfLen2(v.type) + VehicleHalfLen2(o.type) + 0.6f;
                float nextAhead = (o.x - nextX) * v.dir;
                if (nextAhead < minGap) {
                    nextX = o.x - v.dir * minGap;
                    braking = true;
                }
            }
            v.x = nextX;
            v.braking = braking;
            if (v.dir > 0 && v.x >  70.0f) v.x = -70.0f;
            if (v.dir < 0 && v.x < -70.0f) v.x =  70.0f;
        }
    }
    glutTimerFunc(20, UpdateVehicles2, 0);
}
// Police squad car with flashing red/blue lightbar reflecting off wet asphalt
void DrawPoliceCar2()
{
    float x = policeX2, y = -14.0f;
    TintRGB2(20, 20, 30);
    glBegin(GL_QUADS);
        glVertex2f(x-2.8f, y);      glVertex2f(x+2.8f, y);
        glVertex2f(x+2.8f, y+1.4f); glVertex2f(x-2.8f, y+1.4f);
    glEnd();
    TintRGB2(235, 235, 240);
    glBegin(GL_QUADS);
        glVertex2f(x-1.5f, y+1.4f); glVertex2f(x+1.5f, y+1.4f);
        glVertex2f(x+1.1f, y+2.2f); glVertex2f(x-1.1f, y+2.2f);
    glEnd();
    bool redOn = fmodf(policeLightPhase2, 1.0f) < 0.5f;
    unsigned char lr = redOn ? 255 : 20, lb = redOn ? 20 : 255;
    TintRGB2(lr, 20, lb);
    glBegin(GL_QUADS);
        glVertex2f(x-0.9f, y+2.2f);  glVertex2f(x+0.9f, y+2.2f);
        glVertex2f(x+0.9f, y+2.55f); glVertex2f(x-0.9f, y+2.55f);
    glEnd();
    FilledCircle2(x, y+2.4f, 1.0f, lr, 60, lb, 90);
    FilledCircle2(x-1.6f, y-0.1f, 0.55f, 15, 15, 15, 255);
    FilledCircle2(x+1.6f, y-0.1f, 0.55f, 15, 15, 15, 255);
}
// Timer callback: advances police cruiser position and toggles alternating strobe siren lights
void UpdatePoliceCar2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdatePoliceCar2, 0); return; }
    if (isAnimating2) {
        policeX2 += 0.12f;
        if (policeX2 > 70.0f) policeX2 = -70.0f;
        policeLightPhase2 += 0.12f;
    }
    glutTimerFunc(20, UpdatePoliceCar2, 0);
}
// Commuter cyclist riding along the bike lane with spinning wheel spokes
void DrawBicycle2()
{
    float x = bikeX2, y = -8.6f;
    TintRGB2(20, 20, 20);
    glLineWidth(1.5f);
    for (int w = -1; w <= 1; w += 2) {
        float wx = x + w * 1.3f;
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 20; i++) {
                float ang = (float)i / 20 * 2 * PI2;
                glVertex2f(wx + 0.75f*cos(ang), y + 0.75f*sin(ang));
            }
        glEnd();
        glBegin(GL_LINES);
            for (int s = 0; s < 4; s++) {
                float ang = pedalAngle2 + s * (PI2 / 2.0f);
                glVertex2f(wx, y);
                glVertex2f(wx + 0.75f*cos(ang), y + 0.75f*sin(ang));
            }
        glEnd();
    }
    TintRGB2(220, 60, 60);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2f(x-1.3f, y);       glVertex2f(x+0.2f, y+1.1f);
        glVertex2f(x+0.2f, y+1.1f);  glVertex2f(x+1.3f, y);
        glVertex2f(x+0.2f, y+1.1f);  glVertex2f(x+0.9f, y+1.6f);
        glVertex2f(x-1.3f, y);       glVertex2f(x+0.9f, y+1.6f);
    glEnd();
    FilledCircle2(x+0.15f, y+2.15f, 0.42f, 230, 190, 150, 255);
    TintRGB2(50, 90, 180);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(x+0.15f, y+1.75f); glVertex2f(x+0.2f, y+1.1f);
        glVertex2f(x+0.2f, y+1.1f);   glVertex2f(x+0.9f, y+1.6f);
    glEnd();
    TintRGB2(40, 40, 60);
    glLineWidth(3.0f);
    float pedalR = 0.45f;
    float legAx = x + 0.2f, legAy = y + 1.0f;
    for (int leg = 0; leg < 2; leg++) {
        float ang = pedalAngle2 + leg * PI2;
        float px = x - 0.1f + pedalR * cos(ang);
        float py = y + 0.35f + pedalR * sin(ang) * 0.6f;
        glBegin(GL_LINES);
            glVertex2f(legAx, legAy);
            glVertex2f(px, py);
        glEnd();
    }
}
// Timer callback: drives cyclist along bike lane and rotates pedaling cadence
void UpdateBicycle2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateBicycle2, 0); return; }
    if (isAnimating2) {
        bikeX2 -= bikeSpeed2;
        if (bikeX2 < -70.0f) bikeX2 = 70.0f;
        pedalAngle2 += 0.25f;
        if (pedalAngle2 > 2*PI2) pedalAngle2 -= 2*PI2;
    }
    glutTimerFunc(20, UpdateBicycle2, 0);
}
// Initializes sidewalk steam puff particle pool
void InitSteam2()
{
    for (int i = 0; i < MAX_STEAM2; i++) steam2[i].active = false;
}
// Spawns rising steam cloud puff from sidewalk vents or manhole covers
void SpawnSteamPuff2()
{
    for (int i = 0; i < MAX_STEAM2; i++) {
        if (!steam2[i].active) {
            steam2[i].active = true;
            steam2[i].x = -2.0f + (rand() % 40 - 20) / 100.0f;
            steam2[i].y = -8.2f;
            steam2[i].vy = 0.05f + (rand() % 20) / 1000.0f;
            steam2[i].alpha = 140.0f;
            steam2[i].size = 0.4f + (rand() % 20) / 100.0f;
            return;
        }
    }
}
// Renders rising, semi-transparent steam puffs drifting upwards from the ground
void DrawSteam2()
{
    for (int i = 0; i < MAX_STEAM2; i++) {
        if (!steam2[i].active) continue;
        FilledCircle2(steam2[i].x, steam2[i].y, steam2[i].size, 200, 200, 210, (unsigned char)steam2[i].alpha);
    }
}
// Timer callback: moves steam particles upward, expands their size, and diminishes alpha
void UpdateSteam2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateSteam2, 0); return; }
    if (isAnimating2) {
        static int spawnAcc = 0;
        spawnAcc++;
        if (spawnAcc > 6) { SpawnSteamPuff2(); spawnAcc = 0; }
        for (int i = 0; i < MAX_STEAM2; i++) {
            if (!steam2[i].active) continue;
            steam2[i].y += steam2[i].vy;
            steam2[i].x += 0.01f;
            steam2[i].size += 0.01f;
            steam2[i].alpha -= 1.6f;
            if (steam2[i].alpha <= 0) steam2[i].active = false;
        }
    }
    glutTimerFunc(30, UpdateSteam2, 0);
}
// Parked gourmet food truck with open serving window, menu blackboard, and hanging lights
void DrawFoodTruck2()
{
    float x = 30.0f, y = -6.5f;
    TintRGB2(220, 90, 60);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f, y);      glVertex2f(x+3.0f, y);
        glVertex2f(x+3.0f, y+2.2f); glVertex2f(x-3.0f, y+2.2f);
    glEnd();
    TintRGB2(255, 230, 150);
    glBegin(GL_QUADS);
        glVertex2f(x-2.6f, y+2.2f); glVertex2f(x+0.5f, y+2.2f);
        glVertex2f(x+0.5f, y+2.6f); glVertex2f(x-2.6f, y+2.6f);
    glEnd();
    TintRGB2(255, 255, 255);
    DrawTextCentered(x-1.0f, y+1.0f, GLUT_BITMAP_HELVETICA_12, "EATS");
    for (int i = 0; i < 2; i++) {
        float qx = x - 4.5f - i * 1.3f;
        FilledCircle2(qx, y+1.4f, 0.3f, 210, 180, 150, 255);
        TintRGB2(70, 70, 90);
        glLineWidth(3.0f);
        glBegin(GL_LINES); glVertex2f(qx, y+1.1f); glVertex2f(qx, y+0.1f); glEnd();
    }
}
// Acoustic busker street musician playing guitar with an open instrument case for tips
void DrawStreetPerformer2()
{
    float x = -2.0f, y = -6.3f;
    FilledCircle2(x, y+1.5f, 0.34f, 225, 185, 145, 255);
    TintRGB2(90, 60, 40);
    glLineWidth(4.0f);
    glBegin(GL_LINES); glVertex2f(x, y+1.15f); glVertex2f(x, y+0.2f); glEnd();
    FilledCircle2(x+0.3f, y+0.5f, 0.5f, 150, 100, 50, 255);
    float armAng = 0.5f + 0.4f * sinf(performerArmAngle2);
    TintRGB2(225, 185, 145);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(x, y+0.9f);
        glVertex2f(x + 0.5f + 0.3f*cos(armAng), y + 0.5f + 0.3f*sin(armAng));
    glEnd();
    TintRGB2(30, 20, 15);
    glBegin(GL_QUADS);
        glVertex2f(x-1.1f, y-0.1f); glVertex2f(x-0.3f, y-0.1f);
        glVertex2f(x-0.3f, y+0.25f); glVertex2f(x-1.1f, y+0.25f);
    glEnd();
}
// Timer callback: animates guitarist strumming arm motion
void UpdateStreetPerformer2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateStreetPerformer2, 0); return; }
    if (isAnimating2) performerArmAngle2 += 0.15f;
    glutTimerFunc(30, UpdateStreetPerformer2, 0);
}
// Pedestrian walking in rain holding an umbrella, with animated leg and arm gait
void DrawPerson2(const Ped2& p, float walkTimer)
{
    float bob = sinf(walkTimer*3.0f + p.phase) * 0.15f;
    float hipX = p.x, hipY = p.y + 1.0f + bob;
    TintRGB2(40, 40, 50);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2f(hipX, hipY); glVertex2f(hipX + 0.25f*sinf(walkTimer*4.0f+p.phase), p.y);
        glVertex2f(hipX, hipY); glVertex2f(hipX - 0.25f*sinf(walkTimer*4.0f+p.phase), p.y);
    glEnd();
    TintRGB2(p.shirtR, p.shirtG, p.shirtB);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(hipX, hipY); glVertex2f(hipX, hipY+1.1f);
    glEnd();
    FilledCircle2(hipX, hipY+1.4f, 0.32f, 225, 185, 145, 255);
    if (p.umbrella && isRaining2) {
        unsigned char ur = (unsigned char)(90 + (int)(p.phase * 47) % 150);
        unsigned char ug = (unsigned char)(40 + (int)(p.phase * 83) % 120);
        unsigned char ub = (unsigned char)(120 + (int)(p.phase * 61) % 110);
        TintRGB2(ur, ug, ub);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(hipX, hipY+2.3f);
            for (int i = 0; i <= 10; i++) {
                float ang = PI2 + (float)i / 10 * PI2;
                glVertex2f(hipX + 1.25f*cos(ang), hipY + 2.05f + 0.42f*sin(ang));
            }
        glEnd();
        TintRGB2((unsigned char)(ur*0.7f), (unsigned char)(ug*0.7f), (unsigned char)(ub*0.7f));
        for (int i = 0; i < 5; i++)
            FilledCircle2(hipX - 1.0f + i * 0.5f, hipY + 2.02f, 0.15f,
                          (unsigned char)(ur*0.7f), (unsigned char)(ug*0.7f),
                          (unsigned char)(ub*0.7f), 255);
        TintRGB2(80, 80, 80);
        glLineWidth(1.4f);
        glBegin(GL_LINES);
            glVertex2f(hipX, hipY+2.3f); glVertex2f(hipX, hipY+1.25f);
        glEnd();
        NoTint2 emissive;
        for (int d = 0; d < 3; d++) {
            float dt = fmodf(neonTime2 * 1.6f + p.phase + d * 0.3f, 1.0f);
            glColor4ub(165, 195, 225, (unsigned char)(170 * (1.0f - dt)));
            glBegin(GL_LINES);
                float ddx = hipX - 1.1f + d * 1.1f;
                glVertex2f(ddx, hipY + 1.95f - dt * 1.4f);
                glVertex2f(ddx, hipY + 1.80f - dt * 1.4f);
            glEnd();
        }
    } else if (isRaining2) {
        TintRGB2((unsigned char)(p.shirtR*0.8f), (unsigned char)(p.shirtG*0.8f),
                 (unsigned char)(p.shirtB*0.8f));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(hipX, hipY+1.42f);
            for (int i = 0; i <= 8; i++) {
                float ang = PI2 * ((float)i / 8.0f);
                glVertex2f(hipX + 0.44f*cosf(ang), hipY + 1.42f + 0.44f*sinf(ang));
            }
        glEnd();
    }
    glLineWidth(1.0f);
}
// Pedestrians with umbrellas on wet sidewalk
void DrawPedestrians2()
{
    for (int i = 0; i < NUM_PEDS2; i++) DrawPerson2(peds2[i], pedWalkTimer2);
}
// Timer callback: advances pedestrians along sidewalks, handling boundaries and walking phase
void UpdatePedestrians2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdatePedestrians2, 0); return; }
    if (isAnimating2) {
        pedWalkTimer2 += 0.12f;
        bool walkSignal = (trafficState2 == 2);
        const float CROSS_X   = 0.2f;
        const float PAVEMENT_Y = -6.3f;
        const float FAR_Y      = -19.0f;
        for (int i = 0; i < NUM_PEDS2; i++) {
            Ped2& p = peds2[i];
            if (p.crossState == 2) {
                p.crossT += 0.010f;
                p.y = PAVEMENT_Y + (FAR_Y - PAVEMENT_Y) * sinf(p.crossT * PI2);
                if (p.crossT >= 1.0f) {
                    p.crossT = 0.0f;
                    p.crossState = 0;
                    p.y = PAVEMENT_Y;
                }
                continue;
            }
            if (p.crossState == 1) {
                if (walkSignal) p.crossState = 2;
                continue;
            }
            p.x += p.speed * p.dir;
            if (p.wantsToCross && p.speed > 0.0f) {
                float prev = p.x - p.speed * p.dir;
                bool reached = (p.dir > 0) ? (prev < CROSS_X && p.x >= CROSS_X)
                                           : (prev > CROSS_X && p.x <= CROSS_X);
                if (reached) {
                    p.x = CROSS_X;
                    p.crossState = walkSignal ? 2 : 1;
                }
            }
            if (p.dir > 0 && p.x > 65.0f)  p.x = -65.0f;
            if (p.dir < 0 && p.x < -65.0f) p.x = 65.0f;
        }
    }
    glutTimerFunc(30, UpdatePedestrians2, 0);
}
// Initializes angled raindrop particle buffer for rainy night ambiance
void InitRain2()
{
    for (int i = 0; i < MAX_RAIN2; i++) {
        rainX2[i] = -60.0f + (rand() % 1200) / 10.0f;
        rainY2[i] = (rand() % 800) / 10.0f;
        rainLen2[i] = 1.0f + (rand() % 10) / 10.0f;
    }
}
// Road surface with specular sheen, lane striping, crosswalk zebras, and curb gutters
void DrawWetRoad2()
{
    if (!isRaining2) return;
    glBegin(GL_QUADS);
        TintRGBA2(90, 120, 155, 70);
        glVertex2f(-60, -20); glVertex2f(60, -20);
        TintRGBA2(70, 95, 130, 18);
        glVertex2f(60, -9);   glVertex2f(-60, -9);
    glEnd();
    int count = isThunderstorm2 ? 44 : 26;
    for (int i = 0; i < count; i++) {
        float h1 = sinf(i * 12.9898f + floorf(neonTime2 * 6.0f)) * 43758.5453f;
        float h2 = sinf(i * 78.2330f + floorf(neonTime2 * 6.0f)) * 12345.6789f;
        float jx = h1 - floorf(h1);
        float jy = h2 - floorf(h2);
        float sx = -58.0f + jx * 116.0f;
        float sy = -19.6f + jy * 10.4f;
        float life = fmodf(neonTime2 * 6.0f + i * 0.37f, 1.0f);
        unsigned char a = (unsigned char)(150 * (1.0f - life));
        float rad = 0.18f + life * 0.55f;
        TintRGBA2(200, 225, 245, a);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
            for (int k = 0; k < 10; k++) {
                float ang = (float)k / 10 * 2.0f * PI2;
                glVertex2f(sx + rad * cosf(ang), sy + rad * 0.30f * sinf(ang));
            }
        glEnd();
    }
    glLineWidth(1.0f);
}
// Rain streaks and splash effects
void DrawRain2()
{
    if (!isRaining2) return;
    TintRGBA2(180, 200, 230, 150);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < MAX_RAIN2; i++) {
            glVertex2f(rainX2[i], rainY2[i]);
            glVertex2f(rainX2[i]-0.3f, rainY2[i]-rainLen2[i]);
        }
    glEnd();
}
// Timer callback: moves rain particles downward at a wind-driven slant and triggers puddle splashes
void UpdateRain2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateRain2, 0); return; }
    if (isAnimating2 && isRaining2) {
        for (int i = 0; i < MAX_RAIN2; i++) {
            rainY2[i] -= 1.6f;
            rainX2[i] -= 0.15f;
            if (rainY2[i] < -12.0f) {
                rainY2[i] = 38.0f + (rand() % 100) / 10.0f;
                rainX2[i] = -60.0f + (rand() % 1200) / 10.0f;
            }
        }
    }
    glutTimerFunc(20, UpdateRain2, 0);
}
// Timer callback: periodically toggles random apartment window lights to simulate occupancy
void UpdateWindowFlicker2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateWindowFlicker2, 0); return; }
    if (isAnimating2) {
        int bIdx = rand() % NUM_BUILDINGS2;
        int c = rand() % towers2[bIdx].cols;
        int r = rand() % towers2[bIdx].rows;
        towers2[bIdx].lit[c][r] = !towers2[bIdx].lit[c][r];
    }
    glutTimerFunc(400, UpdateWindowFlicker2, 0);
}
float apartmentT2 = 0.0f;
// Detailed lit apartment window showing interior silhouette and soft curtain draping
void DrawApartmentWindow2()
{
    NoTint2 emissive;
    const float wx = 13.6f, wy = 12.0f, w = 3.4f, h = 4.2f;
    glColor4ub(255, 196, 120, 34);
    glBegin(GL_QUADS);
        glVertex2f(wx-w*0.5f-1.6f, wy-1.6f); glVertex2f(wx+w*0.5f+1.6f, wy-1.6f);
        glVertex2f(wx+w*0.5f+1.6f, wy+h+1.6f); glVertex2f(wx-w*0.5f-1.6f, wy+h+1.6f);
    glEnd();
    glColor3ub(255, 214, 150);
    glBegin(GL_QUADS);
        glVertex2f(wx-w*0.5f, wy);     glVertex2f(wx+w*0.5f, wy);
        glVertex2f(wx+w*0.5f, wy+h);   glVertex2f(wx-w*0.5f, wy+h);
    glEnd();
    glColor3ub(120, 78, 44);
    glBegin(GL_QUADS);
        glVertex2f(wx+0.35f, wy+0.2f); glVertex2f(wx+1.5f, wy+0.2f);
        glVertex2f(wx+1.5f, wy+1.5f);  glVertex2f(wx+0.35f, wy+1.5f);
    glEnd();
    glColor3ub(58, 96, 54);
    FilledCircle2(wx-1.15f, wy+1.3f, 0.42f, 58, 96, 54, 255);
    glColor3ub(120, 78, 44);
    glBegin(GL_QUADS);
        glVertex2f(wx-1.35f, wy+0.2f); glVertex2f(wx-0.95f, wy+0.2f);
        glVertex2f(wx-0.95f, wy+0.9f); glVertex2f(wx-1.35f, wy+0.9f);
    glEnd();
    float t = fmodf(apartmentT2, 1.0f);
    float px, sit = 0.0f;
    if      (t < 0.25f) px = -1.1f + (t / 0.25f) * 2.1f;
    else if (t < 0.55f) { px = 1.0f; sit = 1.0f; }
    else if (t < 0.70f) { px = 1.0f; sit = 1.0f - (t - 0.55f) / 0.15f; }
    else if (t < 0.85f) px = 1.0f - ((t - 0.70f) / 0.15f) * 2.2f;
    else                px = -1.2f + ((t - 0.85f) / 0.15f) * 0.1f;
    float fx = wx + px;
    float fy = wy + 0.25f + (sit > 0.0f ? 0.55f * sit : 0.0f);
    float stride = (sit > 0.0f) ? 0.0f : 0.22f * sinf(apartmentT2 * 40.0f);
    float bodyH  = 1.55f - 0.45f * sit;
    glColor3ub(42, 34, 40);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(fx, fy + bodyH * 0.45f); glVertex2f(fx + stride, fy);
        glVertex2f(fx, fy + bodyH * 0.45f); glVertex2f(fx - stride, fy);
    glEnd();
    glLineWidth(3.4f);
    glBegin(GL_LINES);
        glVertex2f(fx, fy + bodyH * 0.45f); glVertex2f(fx, fy + bodyH);
    glEnd();
    FilledCircle2(fx, fy + bodyH + 0.26f, 0.24f, 42, 34, 40, 255);
    glLineWidth(1.0f);
    glColor3ub(24, 22, 30);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(wx-w*0.5f, wy);     glVertex2f(wx+w*0.5f, wy);
        glVertex2f(wx+w*0.5f, wy+h);   glVertex2f(wx-w*0.5f, wy+h);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(wx, wy); glVertex2f(wx, wy+h);
        glVertex2f(wx-w*0.5f, wy+h*0.5f); glVertex2f(wx+w*0.5f, wy+h*0.5f);
    glEnd();
    glLineWidth(1.0f);
}
// Timer callback: animates television blue-noise flicker glow inside apartment windows
void UpdateApartment2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateApartment2, 0); return; }
    if (isAnimating2) apartmentT2 += 0.0011f;
    glutTimerFunc(30, UpdateApartment2, 0);
}
constexpr float kerbY2      = -20.0f;
constexpr float nearWalkY2  = -40.0f;
// Renders vertical smeared reflections of all neon signs across the wet street surface
void DrawNeonReflections2()
{
    NoTint2 emissive;
    for (int i = 0; i < NUM_NEON2; i++) {
        const NeonSign2& sg = neonSigns2[i];
        float pulse = 0.62f + 0.38f * sinf(neonTime2 * 2.0f + sg.phase);
        float wet   = isRaining2 ? 1.0f : 0.45f;
        for (int k = 0; k < 7; k++) {
            float t   = (float)k / 7.0f;
            float y0  = kerbY2 - t * (kerbY2 - nearWalkY2);
            float y1  = kerbY2 - (t + 1.0f/7.0f) * (kerbY2 - nearWalkY2);
            float off = sinf(neonTime2 * 1.3f + i * 2.1f + k * 1.7f) * (1.0f + t * 2.6f);
            float w   = sg.w * (0.42f + t * 0.55f);
            unsigned char a = (unsigned char)(78 * pulse * wet * (1.0f - t * 0.75f));
            glColor4ub(sg.r, sg.g, sg.b, a);
            glBegin(GL_QUADS);
                glVertex2f(sg.x + off - w,        y0);
                glVertex2f(sg.x + off + w,        y0);
                glVertex2f(sg.x + off * 1.5f + w * 0.7f, y1);
                glVertex2f(sg.x + off * 1.5f - w * 0.7f, y1);
            glEnd();
        }
    }
}
// High-detail foreground sidewalk pavement with tactile paving stones and drain grates
void DrawNearSidewalk2()
{
    glBegin(GL_QUADS);
        TintRGB2(26, 26, 32);
        glVertex2f(-60.0f, nearWalkY2); glVertex2f(60.0f, nearWalkY2);
        TintRGB2(15, 15, 20);
        glVertex2f(60.0f, kerbY2);      glVertex2f(-60.0f, kerbY2);
    glEnd();
    TintRGB2(74, 74, 82);
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, kerbY2 - 1.1f); glVertex2f(60.0f, kerbY2 - 1.1f);
        glVertex2f(60.0f, kerbY2);         glVertex2f(-60.0f, kerbY2);
    glEnd();
    DrawNeonReflections2();
    TintRGBA2(120, 124, 138, 60);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = -8; i <= 8; i++) {
            glVertex2f(i * 7.0f,        kerbY2 - 1.1f);
            glVertex2f(i * 7.0f * 2.6f, nearWalkY2);
        }
        for (int c = 1; c <= 5; c++) {
            float t = (float)c / 6.0f;
            float y = kerbY2 - 1.1f - (kerbY2 - nearWalkY2) * (1.0f - (1.0f - t) * (1.0f - t));
            glVertex2f(-60.0f, y); glVertex2f(60.0f, y);
        }
    glEnd();
    if (isRaining2) {
        TintRGBA2(40, 48, 64, 150);
        DrawSoftEllipse(-24.0f, -28.0f, 15.0f, 3.0f, 40, 48, 64, 150, 2);
        DrawSoftEllipse( 22.0f, -33.0f, 18.0f, 3.4f, 40, 48, 64, 150, 2);
    }
    float dx = 34.0f;
    TintRGB2(38, 38, 44);
    glBegin(GL_QUADS);
        glVertex2f(dx - 3.0f, kerbY2 - 2.4f); glVertex2f(dx + 3.0f, kerbY2 - 2.4f);
        glVertex2f(dx + 3.0f, kerbY2 - 1.1f); glVertex2f(dx - 3.0f, kerbY2 - 1.1f);
    glEnd();
    TintRGB2(16, 16, 20);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 5; i++) {
            float gx = dx - 2.2f + i * 1.1f;
            glVertex2f(gx, kerbY2 - 2.2f); glVertex2f(gx, kerbY2 - 1.3f);
        }
    glEnd();
    if (isRaining2) {
        NoTint2 emissive;
        glColor4ub(150, 180, 210, 120);
        glBegin(GL_QUADS);
            glVertex2f(dx - 4.5f, kerbY2 - 1.6f); glVertex2f(dx - 2.6f, kerbY2 - 1.6f);
            glVertex2f(dx - 2.2f, kerbY2 - 1.2f); glVertex2f(dx - 5.0f, kerbY2 - 1.2f);
        glEnd();
    }
    for (int i = -3; i <= 3; i++) {
        float bx = i * 17.0f + 6.0f;
        TintRGB2(46, 48, 56);
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.75f, kerbY2 - 6.0f); glVertex2f(bx + 0.75f, kerbY2 - 6.0f);
            glVertex2f(bx + 0.60f, kerbY2 - 1.2f); glVertex2f(bx - 0.60f, kerbY2 - 1.2f);
        glEnd();
        TintRGB2(190, 170, 60);
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.68f, kerbY2 - 2.6f); glVertex2f(bx + 0.68f, kerbY2 - 2.6f);
            glVertex2f(bx + 0.66f, kerbY2 - 2.0f); glVertex2f(bx - 0.66f, kerbY2 - 2.0f);
        glEnd();
    }
    glLineWidth(1.0f);
}
float nearWalker2X[2]    = { -30.0f, 26.0f };
const float nearWalker2Y[2] = { -31.0f, -36.5f };
const int   nearWalker2Dir[2] = { 1, -1 };
// Close-up detailed foreground pedestrian with textured clothing, umbrella, and footwear
void DrawNearWalker2(int idx)
{
    float x  = nearWalker2X[idx];
    float y  = nearWalker2Y[idx];
    float sc = DepthScaleRange(y, kerbY2, nearWalkY2, 1.5f, 2.6f);
    int   dir = nearWalker2Dir[idx];
    float t  = pedWalkTimer2 + idx * 2.0f;
    unsigned char cr = (idx == 0) ? 46 : 70, cg = (idx == 0) ? 50 : 40, cb = (idx == 0) ? 68 : 52;
    BeginDepthSprite(x, y, sc);
    TintRGB2(24, 24, 30);
    glLineWidth(3.4f);
    glBegin(GL_LINES);
        glVertex2f(x, y + 1.9f); glVertex2f(x + 0.55f*sinf(t*3.0f), y);
        glVertex2f(x, y + 1.9f); glVertex2f(x - 0.55f*sinf(t*3.0f), y);
    glEnd();
    TintRGB2(cr, cg, cb);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.85f, y + 1.6f); glVertex2f(x + 0.85f, y + 1.6f);
        glVertex2f(x + 0.70f, y + 4.3f); glVertex2f(x - 0.70f, y + 4.3f);
    glEnd();
    FilledCircle2(x, y + 4.8f, 0.52f, 60, 58, 66, 255);
    if (isRaining2) {
        TintRGB2(180, 180, 190);
        glLineWidth(1.8f);
        glBegin(GL_LINES); glVertex2f(x + 0.5f*dir, y + 4.2f); glVertex2f(x + 0.5f*dir, y + 6.6f); glEnd();
        unsigned char ur = (idx == 0) ? 190 : 60, ug = (idx == 0) ? 60 : 90, ub = (idx == 0) ? 70 : 150;
        TintRGB2(ur, ug, ub);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x + 0.5f*dir, y + 6.9f);
            for (int i = 0; i <= 10; i++) {
                float a = 3.1416f * ((float)i / 10.0f);
                glVertex2f(x + 0.5f*dir - 2.6f*cosf(a), y + 6.5f + 0.55f*sinf(a));
            }
        glEnd();
        NoTint2 emissive;
        for (int d = 0; d < 4; d++) {
            float dt = fmodf(neonTime2 * 1.4f + d * 0.27f, 1.0f);
            glColor4ub(170, 195, 225, (unsigned char)(190 * (1.0f - dt)));
            glBegin(GL_LINES);
                float ddx = x + 0.5f*dir - 2.3f + d * 1.5f;
                glVertex2f(ddx, y + 6.4f - dt * 2.2f);
                glVertex2f(ddx, y + 6.1f - dt * 2.2f);
            glEnd();
        }
    }
    glLineWidth(1.0f);
    EndDepthSprite();
}
// Renders all close-up pedestrians on the near sidewalk
void DrawNearWalkers2()
{
    if (nearWalker2Y[0] < nearWalker2Y[1]) { DrawNearWalker2(1); DrawNearWalker2(0); }
    else                                   { DrawNearWalker2(0); DrawNearWalker2(1); }
}
// Timer callback: animates foreground pedestrian walking speeds and gait cycles
void UpdateNearWalkers2(int)
{
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateNearWalkers2, 0); return; }
    if (isAnimating2) {
        for (int i = 0; i < 2; i++) {
            nearWalker2X[i] += 0.055f * nearWalker2Dir[i];
            if (nearWalker2X[i] >  68.0f) nearWalker2X[i] = -68.0f;
            if (nearWalker2X[i] < -68.0f) nearWalker2X[i] =  68.0f;
        }
    }
    glutTimerFunc(30, UpdateNearWalkers2, 0);
}
const char* kTitle = "Downtown Neon District";
// Initializes scenario state, resets animation timers, and pre-allocates particle buffers
void Init()
{
    for (int i = 0; i < NUM_STARS2; i++) {
        stars2[i].x = -58.0f + (rand() % 1160) / 10.0f;
        stars2[i].y = 12.0f + (rand() % 260) / 10.0f;
    }
    for (int t = 0; t < NUM_BUILDINGS2; t++) {
        for (int c = 0; c < towers2[t].cols; c++)
            for (int r = 0; r < towers2[t].rows; r++)
                towers2[t].lit[c][r] = (rand() % 100) < 55;
    }
    InitRain2();
    InitSteam2();
    InitRipples2();
    glutTimerFunc(0, UpdateNeon2, 0);
    glutTimerFunc(0, UpdateBillboard2, 0);
    glutTimerFunc(0, UpdateTrain2, 0);
    glutTimerFunc(0, UpdateVehicles2, 0);
    glutTimerFunc(0, UpdatePoliceCar2, 0);
    glutTimerFunc(0, UpdateBicycle2, 0);
    glutTimerFunc(0, UpdatePedestrians2, 0);
    glutTimerFunc(0, UpdateStreetPerformer2, 0);
    glutTimerFunc(0, UpdateRain2, 0);
    glutTimerFunc(0, UpdateSteam2, 0);
    glutTimerFunc(0, UpdateTrafficLight2, 0);
    glutTimerFunc(0, UpdateWindowFlicker2, 0);
    glutTimerFunc(0, UpdateHelicopter2, 0);
    glutTimerFunc(0, UpdateSubwayGrate2, 0);
    glutTimerFunc(0, UpdateRipples2, 0);
    glutTimerFunc(0, UpdateLightning2, 0);
    glutTimerFunc(0, UpdateNearWalkers2, 0);
    glutTimerFunc(0, UpdateApartment2, 0);
}
// Master rendering routine: clears buffers, sets projection, and draws complete scenario composition
void Draw()
{
    glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    DrawSky2();
    DrawStars2();
    DrawSkyline2();
    DrawLightningBolt2();
    DrawTrainTrack2();
    DrawTrain2();
    if (heliScale2 < 0.6f) DrawHelicopter2();
    for (int i = 0; i < NUM_BUILDINGS2; i++) {
        DrawBuildingBody2(towers2[i]);
        DrawWindows2(towers2[i]);
        DrawRooftopProps2(i, towers2[i]);
    }
    DrawFireEscape2(towers2[1], -1.0f);
    DrawFireEscape2(towers2[2],  1.0f);
    DrawSubwayEntrance2();
    DrawApartmentWindow2();
    DrawAllNeon2();
    DrawBillboard2();
    if (heliScale2 >= 0.6f) DrawHelicopter2();
    DrawAwning2(-5.0f, 1.0f, -5.5f);
    DrawBikeLane2();
    DrawRoad2();
    DrawWetRoad2();
    DrawPuddleGlow2();
    DrawRipples2();
    DrawSubwayGrate2();
    DrawTrafficLight2();
    DrawCrosswalkSignal2();
    DrawStreetVent2();
    DrawParkedCars2();
    DrawTrashCan2(9.0f);
    DrawFireHydrant2(25.0f);
    DrawNewsstand2();
    DrawGraffiti2();
    DrawDinerInterior2();
    DrawVehicles2();
    DrawPoliceCar2();
    DrawBicycle2();
    DrawSteam2();
    DrawFoodTruck2();
    DrawStreetPerformer2();
    DrawPedestrians2();
    DrawNearSidewalk2();
    DrawNearWalkers2();
    DrawRain2();
    DrawLightningFlash2();
    static const char* const hud[] = {
        "1 dusk       2 night       3 dawn",
        "R  rain on/off      T  thunderstorm",
        "X  strike lightning now",
        "SPACE pause    H help    N/B change scene    ESC quit",
        nullptr
    };
    DrawSceneHUD(kTitle, hud);
}
// Scenario keyboard handler: dispatches scenario-specific hotkeys and feature toggles
void Keyboard(unsigned char key, int , int )
{
    switch (key) {
        case '1': nightPhase2 = 0; break;
        case '2': nightPhase2 = 1; break;
        case '3': nightPhase2 = 2; break;
        case 'r': case 'R':
            isRaining2 = !isRaining2;
            if (!isRaining2) isThunderstorm2 = false;
            break;
        case 't': case 'T':
            isThunderstorm2 = !isThunderstorm2;
            if (isThunderstorm2) isRaining2 = true;
            break;
        case 'x': case 'X':
            isThunderstorm2 = true;
            isRaining2 = true;
            lightningCooldown2 = 1;
            break;
    }
    glutPostRedisplay();
}
// Scenario mouse handler: toggles scene animation play/pause state
void Mouse(int button, int state, int , int )
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        isAnimating2 = true;
    }
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        isAnimating2 = false;
    }
    glutPostRedisplay();
}
}

// ============================================================================
//  SCENARIO 3 -- Riverside Park
//  Cherry blossoms, jetty, kayaks, Ferris wheel, balloon, four time phases
// ============================================================================
namespace Scenario3 {
const float SHARED_PI = 3.14159265f;
void DrawSoftEllipse(float cx, float cy, float rx, float ry,
                     unsigned char r, unsigned char g, unsigned char b,
                     unsigned char alpha, int layers)
{
    if (layers < 1) layers = 1;
    for (int L = layers; L >= 1; L--) {
        float f = (float)L / layers;
        unsigned char a = (unsigned char)(alpha * (1.0f - f) / layers * 2.2f + alpha * 0.10f);
        glColor4ub(r, g, b, a);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int i = 0; i <= 26; i++) {
                float ang = (float)i / 26.0f * 2.0f * SHARED_PI;
                glVertex2f(cx + rx * f * cosf(ang), cy + ry * f * sinf(ang));
            }
        glEnd();
    }
}
// Renders soft elliptical contact shadow on ground beneath park figures
void DrawGroundShadow(float x, float y, float rx, float lean, unsigned char alpha)
{
    float cx = x + lean * 0.5f;
    float rr = rx + fabsf(lean) * 0.5f;
    float ry = rx * 0.30f;
    for (int L = 3; L >= 1; L--) {
        float f = (float)L / 3.0f;
        glColor4ub(20, 26, 20, (unsigned char)(alpha * (1.0f - f * 0.55f) / 2.2f));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, y);
            for (int i = 0; i <= 22; i++) {
                float ang = (float)i / 22.0f * 2.0f * SHARED_PI;
                glVertex2f(cx + rr * f * cosf(ang), y + ry * f * sinf(ang));
            }
        glEnd();
    }
}
// Computes perspective scaling factor based on vertical ground depth position
float DepthScale(float y)
{
    const float horizonY = -6.0f, nearY = -20.0f;
    float t = (y - horizonY) / (nearY - horizonY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 0.68f + 0.42f * t;
}
// Computes depth scale interpolated within a customized vertical range
float DepthScaleRange(float y, float farY, float nearY, float minS, float maxS)
{
    float t = (y - farY) / (nearY - farY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return minS + (maxS - minS) * t;
}
// Sets up OpenGL modelview matrix to scale an object around its local anchor point
void BeginDepthSprite(float x, float y, float scale)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-x, -y, 0.0f);
}
// Restores OpenGL modelview matrix after depth-scaled object rendering
void EndDepthSprite()
{
    glPopMatrix();
}
// Prepares reflection matrix by mirroring geometry vertically across waterline
void BeginReflection(float waterlineY, float squash, float wobble)
{
    glPushMatrix();
    glTranslatef(wobble, waterlineY, 0.0f);
    glScalef(1.0f, -squash, 1.0f);
    glTranslatef(0.0f, -waterlineY, 0.0f);
}
// Restores modelview matrix after rendering water reflection geometry
void EndReflection()
{
    glPopMatrix();
}
void WashReflection(float cx, float halfW, float waterlineY, float depth,
                    unsigned char r, unsigned char g, unsigned char b,
                    unsigned char alpha, float scroll)
{
    float x0 = cx - halfW * 1.35f, x1 = cx + halfW * 1.35f;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 8; i++) {
        float t = (float)i / 8.0f;
        float y = waterlineY - depth * t;
        glColor4ub(r, g, b, (unsigned char)(alpha * (0.22f + 0.78f * t)));
        glVertex2f(x0, y);
        glVertex2f(x1, y);
    }
    glEnd();
    for (int i = 0; i < 7; i++) {
        float t = (i + 0.5f) / 7.0f;
        float y = waterlineY - depth * t + 0.18f * sinf(scroll * 0.16f + i * 1.7f);
        float w = halfW * (0.50f + 0.45f * sinf(scroll * 0.09f + i * 1.3f));
        glColor4ub(r, g, b, (unsigned char)(alpha * 0.85f));
        glBegin(GL_QUADS);
            glVertex2f(cx - w, y);         glVertex2f(cx + w, y);
            glVertex2f(cx + w, y + 0.17f); glVertex2f(cx - w, y + 0.17f);
        glEnd();
    }
}
// Renders bitmap font text string at given world raster coordinates
void DrawBitmapText(float x, float y, const char* s, void* font)
{
    glRasterPos2f(x, y);
    for (const char* c = s; *c; ++c) glutBitmapCharacter(font, *c);
}
// Measures the pixel width of a given text string rendered in specified font
int BitmapTextWidthPx(const char* s, void* font)
{
    int w = 0;
    for (const char* c = s; *c; ++c) w += glutBitmapWidth(font, *c);
    return w;
}
// Renders text centered horizontally around specified world X coordinate
void DrawBitmapTextCentered(float cx, float y, const char* s, void* font)
{
    float worldW = BitmapTextWidthPx(s, font)
                 * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
    DrawBitmapText(cx - worldW * 0.5f, y, s, font);
}
// Draws bottom-left semi-transparent on-screen help HUD panel with keybindings
void DrawSceneHUD(const char* title, const char* const* lines)
{
    int count = 0;
    size_t longest = strlen(title);
    for (const char* const* p = lines; *p; ++p) {
        count++;
        if (strlen(*p) > longest) longest = strlen(*p);
    }
    const float left = WORLD_LEFT + 2.0f;
    const float top  = WORLD_TOP  - 1.6f;
    float panelW = showHelp ? (longest * 0.66f + 2.4f) : (strlen(title) * 0.95f + 2.4f);
    float panelH = showHelp ? (3.6f + count * 2.0f)    : 3.4f;
    glColor4ub(12, 18, 28, 120);
    glBegin(GL_QUADS);
        glVertex2f(left - 1.0f, top + 1.2f);
        glVertex2f(left - 1.0f + panelW, top + 1.2f);
        glVertex2f(left - 1.0f + panelW, top + 1.2f - panelH);
        glVertex2f(left - 1.0f, top + 1.2f - panelH);
    glEnd();
    glColor4ub(255, 246, 224, 255);
    DrawBitmapText(left, top - 0.8f, title, GLUT_BITMAP_HELVETICA_18);
    if (showHelp) {
        glColor4ub(228, 234, 242, 255);
        float y = top - 3.4f;
        for (const char* const* p = lines; *p; ++p, y -= 2.0f)
            DrawBitmapText(left, y, *p, GLUT_BITMAP_HELVETICA_12);
    }
    if (isPaused) {
        glColor4ub(255, 210, 120, 255);
        DrawBitmapText(-4.0f, 0.0f, "PAUSED", GLUT_BITMAP_HELVETICA_18);
    }
}
constexpr float PI3 = 3.1416f;
int   dayPhase3      = 1;
bool  isAnimating3   = true;
float windPhase3      = 0.0f;
float windIntensity3  = 1.0f;
float pedWalkTimer3   = 0.0f;

// Clouds
struct Cloud3
{
    float x, y, scale, speed;
};
constexpr int NUM_CLOUDS3 = 4;
Cloud3 clouds3[NUM_CLOUDS3] = {
    { -50.0f, 28.0f, 2.10f, 0.03f  },
    { -10.0f, 33.0f, 2.70f, 0.02f  },
    {  20.0f, 26.0f, 1.70f, 0.035f },
    {  45.0f, 31.0f, 2.30f, 0.025f }
};

// Bird flock system
struct Bird3
{
    float cx, cy, radius, angle, speed;
};
constexpr int NUM_BIRDS3 = 4;
Bird3 birds3[NUM_BIRDS3] = {
    { -20.0f, 25.0f, 10.0f, 0.0f, 0.02f  },
    { -20.0f, 25.0f, 10.0f, 1.5f, 0.018f },
    {  25.0f, 22.0f,  8.0f, 0.7f, 0.022f },
    {  25.0f, 22.0f,  8.0f, 3.0f, 0.02f  }
};
float rippleScroll3 = 0.0f;
float kayakX3      = -30.0f;
float kayakSpeed3  = 0.055f;
float kayakStroke3 = 0.0f;
float waterClock3  = 0.0f;
struct Duck3
{
    float x, y, speed, phase; bool flapping; float flapTimer;
};
constexpr int NUM_DUCKS3 = 4;
Duck3 ducks3[NUM_DUCKS3] = {
    { -10.0f, -25.0f, 0.03f,  0.0f, false, 0.0f },
    {  -4.0f, -26.0f, 0.025f, 1.0f, false, 0.0f },
    {   3.0f, -25.5f, 0.028f, 2.0f, false, 0.0f },
    {  10.0f, -26.5f, 0.022f, 3.0f, false, 0.0f }
};
struct Tree3
{
    float x, y, scale, swayPhase;
};
constexpr int NUM_TREES3 = 6;
Tree3 trees3[NUM_TREES3] = {
    { -44.0f, -8.8f, 1.95f, 0.0f },
    {  28.0f, -6.5f, 1.02f, 1.0f },
    {  47.0f, -9.4f, 1.78f, 2.0f },
    {  57.0f, -6.9f, 0.92f, 3.0f },
    { -27.0f, -7.4f, 1.16f, 4.0f },
    {   8.0f, -6.3f, 0.88f, 5.0f }
};
constexpr float hillCrestY3    =  1.0f;
constexpr float PATH_TOP_Y3    = -10.8f;
constexpr float PATH_BOTTOM_Y3 = -13.2f;
constexpr float LAWN_BOTTOM_Y3 = -20.0f;
constexpr float GAZEBO_Y3      = -19.0f;
constexpr float SWING_BASE_Y3  = -18.5f;
constexpr float SWING_TOP_Y3   = -13.4f;
constexpr float SLIDE_BASE_Y3  = -18.5f;
constexpr float SLIDE_TOP_Y3   = -13.4f;
constexpr float SEESAW_Y3      = -17.5f;
constexpr float PICNIC_Y3      = -18.5f;
constexpr float FOUNTAIN_Y3    = -14.8f;
constexpr float BENCH_Y3       = -15.4f;
constexpr int NUM_BENCHES3 = 3;
float benchX3[NUM_BENCHES3] = { -38.0f, 25.0f, 42.0f };
float swingAngle3  = 0.0f;
float seesawAngle3 = 0.0f;
float kiteBaseX3     = 18.0f;
float kiteBaseY3     = 20.0f;
float kiteBobPhase3  = 0.0f;

// Park pedestrians
struct Ped3
{
    float x, y, speed, phase;
    int dir;
    int kind;
    unsigned char shirtR, shirtG, shirtB;
};
constexpr int NUM_PEDS3 = 9;
Ped3 peds3[NUM_PEDS3] = {
    { -40.0f, -11.3f, 0.05f, 0.0f,  1, 0, 200,  90,  90 },
    { -15.0f, -12.8f, 0.11f, 1.0f, -1, 1,  90, 140, 220 },
    {   8.0f, -11.7f, 0.06f, 2.0f,  1, 2, 210, 180,  60 },
    {  25.0f, -12.4f, 0.05f, 3.0f, -1, 0, 150,  90, 190 },
    {  42.0f, -11.2f, 0.10f, 4.0f,  1, 1,  90, 200, 130 },
    { -25.0f, -12.9f, 0.045f,5.0f, -1, 0, 100, 160, 170 },
    {  35.0f, -12.1f, 0.095f,6.0f,  1, 1, 210, 130,  80 },
    {   0.0f, -11.6f, 0.205f,2.5f, -1, 3,  70, 180, 190 },
    { -52.0f, -12.6f, 0.075f,4.5f,  1, 4, 240, 150,  70 }
};
float balloonX3        = -95.0f;
float balloonY3        = 30.0f;
float balloonScale3    = 0.5f;
float balloonBobPhase3 = 0.0f;
float balloonCruiseY3   = 30.0f;
const float balloonApproachY3 = -1.5f;
float fountainPhase3 = 0.0f;
float kayak2X3      = 45.0f;
float kayak2Stroke3 = 2.1f;
float squirrelPhase3 = 0.0f;
float playPhase3 = 0.0f;
struct Butterfly3
{
    float baseX, baseY, phase;
};
constexpr int NUM_BUTTERFLIES3 = 3;
Butterfly3 butterflies3[NUM_BUTTERFLIES3] = {
    { -45.0f, -13.0f, 0.0f },
    { -43.0f, -13.5f, 2.0f },
    {  35.0f, -13.0f, 4.0f }
};
float gooseX3    = -5.0f;
float gooseSpeed3 = 0.02f;
int   gooseDir3   = 1;
float duckFamilyX3     = -20.0f;
float duckFamilySpeed3 = 0.018f;
float frisbeeT3 = 0.0f;
constexpr float frisbeeBaseX3 = -8.0f;
struct FishJump3
{
    float x, phase; bool active;
};
constexpr int MAX_FISH3 = 2;
FishJump3 fishJumps3[MAX_FISH3];
int fishCooldown3 = 150;
bool autumnMode3 = false;
constexpr int MAX_PETALS3 = 170;
struct Petal3
{
    float x, y, fall, phase, swing, rot, spin, size, jit;
    int   tree;
    float landY;
    bool  afloat;
};
Petal3 petals3[MAX_PETALS3];
float slideT3 = 0.0f;
constexpr int NUM_FIREFLIES3 = 16;

// Firefly particles (dusk phase)
struct Firefly3
{
    float x, y, phase, speed;
};
Firefly3 fireflies3[NUM_FIREFLIES3];
float fireflyClock3 = 0.0f;
bool tintOn3 = true;
struct Tint3
{
    float r, g, b;
};
Tint3 GetTint3() {
    if (dayPhase3 == 0) return { 0.80f, 0.88f, 1.08f };
    if (dayPhase3 == 1) return { 1.00f, 1.00f, 1.00f };
    if (dayPhase3 == 2) return { 1.16f, 0.90f, 0.68f };
    return                     { 0.54f, 0.58f, 0.86f };
}
constexpr float SUN_ARC_RX3 = 44.0f;
constexpr float SUN_ARC_RY3 = 40.0f;
constexpr float SUN_ARC_CY3 = -4.0f;
inline float SunAngle3()
{
    if (dayPhase3 == 0) return 2.42f;
    if (dayPhase3 == 1) return 1.57f;
    if (dayPhase3 == 2) return 0.42f;
    return                     0.08f;
}
inline float SunX3()
{
    return SUN_ARC_RX3 * cosf(SunAngle3());
}
inline float SunY3()
{
    return SUN_ARC_CY3 + SUN_ARC_RY3 * sinf(SunAngle3());
}
inline float SunLowness3()
{
    float h = (SunY3() - (-6.0f)) / (SUN_ARC_CY3 + SUN_ARC_RY3 - (-6.0f));
    if (h < 0.0f) h = 0.0f;
    if (h > 1.0f) h = 1.0f;
    return 1.0f - h;
}
inline bool IsDusk3()
{
    return dayPhase3 == 3;
}
inline bool LampsOn3()
{
    return dayPhase3 == 3;
}
inline unsigned char ClampByte3(float v)
{
    if (v < 0.0f)   return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}
inline void TintCol4(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (!tintOn3) { glColor4ub(r, g, b, a); return; }
    Tint3 t = GetTint3();
    glColor4ub(ClampByte3(r * t.r), ClampByte3(g * t.g), ClampByte3(b * t.b), a);
}
inline void TintCol3(unsigned char r, unsigned char g, unsigned char b)
{
    TintCol4(r, g, b, 255);
}
// Helper: renders smooth filled circle with specified RGBA color
void FilledCircle3(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    TintCol4(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 22;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI3;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}
// Draws soft elliptical contact shadow beneath trees, park benches, and figures
void DrawFigureShadow3(float x, float y, float rx)
{
    float low  = SunLowness3();
    float lean = -(SunX3() / SUN_ARC_RX3) * (0.35f + 3.2f * low) * rx;
    unsigned char a = (unsigned char)(78 - 34 * low);
    if (IsDusk3()) a = 26;
    DrawGroundShadow(x, y, rx * (1.0f + 0.5f * low), lean, a);
}
struct SkyColor3
{
    float r, g, b;
};
SkyColor3 LerpC3(SkyColor3 a, SkyColor3 b, float t)
{
    SkyColor3 o;
    o.r = a.r + (b.r - a.r) * t;
    o.g = a.g + (b.g - a.g) * t;
    o.b = a.b + (b.b - a.b) * t;
    return o;
}

// Sky with sun/moon position based on time phase
void DrawSky3()
{
    SkyColor3 mornTop = {140, 190, 230}, mornBot = {255, 235, 210};
    SkyColor3 middTop = { 70, 160, 235}, middBot = {200, 230, 250};
    SkyColor3 goldTop = {255, 170,  90}, goldBot = {255, 220, 150};
    SkyColor3 duskTop = { 26,  32,  74}, duskBot = {226, 116,  86};
    SkyColor3 top, bot;
    if      (dayPhase3 == 0) { top = mornTop; bot = mornBot; }
    else if (dayPhase3 == 1) { top = middTop; bot = middBot; }
    else if (dayPhase3 == 2) { top = goldTop; bot = goldBot; }
    else                     { top = duskTop; bot = duskBot; }
    tintOn3 = false;
    glBegin(GL_QUADS);
        TintCol3((unsigned char)top.r, (unsigned char)top.g, (unsigned char)top.b);
        glVertex2f(-60, 40); glVertex2f(60, 40);
        TintCol3((unsigned char)bot.r, (unsigned char)bot.g, (unsigned char)bot.b);
        glVertex2f(60, -6);  glVertex2f(-60, -6);
    glEnd();
    tintOn3 = true;
}
// Renders bright golden sun with soft radial corona adapted to current time phase
void DrawSun3()
{
    float sx = SunX3(), sy = SunY3();
    float low = SunLowness3();
    tintOn3 = false;
    if (IsDusk3()) {
        glPointSize(1.7f);
        glBegin(GL_POINTS);
        for (int i = 0; i < 42; i++) {
            float h1 = sinf(i * 31.17f) * 43758.5453f;
            float h2 = sinf(i * 57.93f) * 12345.6789f;
            float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2);
            glColor4ub(255, 255, 255,
                       (unsigned char)(120 + 135 * (0.5f + 0.5f * sinf(windPhase3 * 2.0f + i))));
            glVertex2f(-58.0f + j1 * 116.0f, 4.0f + j2 * 34.0f);
        }
        glEnd();
        glPointSize(2.0f);
        float mx = -sx * 0.75f, my = 26.0f;
        for (int i = 4; i >= 1; i--)
            FilledCircle3(mx, my, 2.2f + i * 1.3f, 210, 220, 245, (unsigned char)(24 / i));
        FilledCircle3(mx, my, 2.2f, 236, 241, 252, 255);
        FilledCircle3(mx + 0.9f, my + 0.4f, 1.9f, 26, 32, 74, 255);
    }
    if (low > 0.35f) {
        unsigned char ra = (unsigned char)(34 * low);
        unsigned char rr = 255, rg = IsDusk3() ? 140 : 190, rb = IsDusk3() ? 90 : 110;
        float base = atan2f(SUN_ARC_CY3 - sy, 0.0f - sx);
        for (int i = 0; i < 11; i++) {
            float a = base - 1.1f + i * 0.22f;
            glColor4ub(rr, rg, rb, ra);
            glBegin(GL_TRIANGLES);
                glVertex2f(sx, sy);
                glVertex2f(sx + 80.0f * cosf(a - 0.05f), sy + 80.0f * sinf(a - 0.05f));
                glVertex2f(sx + 80.0f * cosf(a + 0.05f), sy + 80.0f * sinf(a + 0.05f));
            glEnd();
        }
    }
    unsigned char g = (unsigned char)(238 - 120 * low);
    unsigned char b = (unsigned char)(158 - 120 * low);
    for (int i = 5; i >= 1; i--) {
        float rad = 3.0f + i * (1.6f + 1.4f * low);
        FilledCircle3(sx, sy, rad, 255, g, b, (unsigned char)(32 / i));
    }
    FilledCircle3(sx, sy, 3.0f + low * 0.9f, 255,
                  (unsigned char)(244 - 70 * low),
                  (unsigned char)(190 - 110 * low), 255);
    tintOn3 = true;
}
// Procedural fluffy cumulus cloud cluster built from overlapping shaded circles
void DrawCloudShape3(float x, float y, float scale)
{
    FilledCircle3(x - 2.2f*scale, y - 0.30f*scale, 1.55f*scale, 214, 222, 236, 205);
    FilledCircle3(x + 0.2f*scale, y - 0.42f*scale, 1.95f*scale, 214, 222, 236, 205);
    FilledCircle3(x + 2.4f*scale, y - 0.26f*scale, 1.45f*scale, 214, 222, 236, 205);
    FilledCircle3(x - 2.6f*scale, y + 0.10f*scale, 1.50f*scale, 252, 253, 255, 225);
    FilledCircle3(x - 0.9f*scale, y + 0.15f*scale, 1.90f*scale, 252, 253, 255, 225);
    FilledCircle3(x + 0.9f*scale, y + 0.05f*scale, 1.80f*scale, 252, 253, 255, 225);
    FilledCircle3(x + 2.7f*scale, y + 0.14f*scale, 1.35f*scale, 252, 253, 255, 225);
    FilledCircle3(x - 0.4f*scale, y + 1.35f*scale, 1.55f*scale, 255, 255, 255, 232);
    FilledCircle3(x + 1.3f*scale, y + 1.15f*scale, 1.25f*scale, 255, 255, 255, 232);
    FilledCircle3(x + 0.4f*scale, y + 2.25f*scale, 0.95f*scale, 255, 255, 255, 236);
}
// Renders drifting cloud layer across the park sky
void DrawClouds3()
{
    for (int i = 0; i < NUM_CLOUDS3; i++) DrawCloudShape3(clouds3[i].x, clouds3[i].y, clouds3[i].scale);
}
// Timer callback: drifts clouds horizontally across the sky and wraps them around screen edge
void UpdateClouds3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateClouds3, 0); return; }
    if (isAnimating3) {
        for (int i = 0; i < NUM_CLOUDS3; i++) {
            clouds3[i].x += clouds3[i].speed * windIntensity3;
            if (clouds3[i].x > 70.0f) clouds3[i].x = -70.0f;
        }
    }
    glutTimerFunc(30, UpdateClouds3, 0);
}
constexpr int   JET_TRAIL_PTS3 = 46;
constexpr float JET_Y3         = 35.2f;
constexpr float JET_LEN3       = 52.0f;
float jetX3      = -90.0f;
float jetCooldown3 = 0.0f;
// Commercial airliner flying high above the park leaving an expanding vapor contrail
void DrawJet3()
{
    if (jetX3 < -84.0f || jetX3 > 84.0f) return;
    tintOn3 = false;
    const float x = jetX3, y = JET_Y3;
    for (int lane = 0; lane < 2; lane++) {
        float side = (lane == 0) ? 1.0f : -1.0f;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i < JET_TRAIL_PTS3; i++) {
            float age = (float)i / (JET_TRAIL_PTS3 - 1);
            float tx  = x - 0.9f - age * JET_LEN3;
            if (tx < -62.0f) break;
            float sep = (0.26f + age * 1.05f) * side;
            float puff = sinf(age * 26.0f + jetX3 * 0.12f + lane * 2.0f) * 0.16f * age;
            float half = (0.10f + age * 0.62f) + puff;
            float sag  = -age * age * 0.9f;
            unsigned char a = (unsigned char)(215.0f * (1.0f - age) * (1.0f - age));
            glColor4ub(252, 253, 255, a);
            glVertex2f(tx, y + sep + sag + half);
            glColor4ub(238, 244, 252, (unsigned char)(a * 0.55f));
            glVertex2f(tx, y + sep + sag - half);
        }
        glEnd();
    }
    glColor4ub(255, 255, 255, 235);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.8f, y + 0.16f); glVertex2f(x - 2.6f, y + 0.30f);
        glVertex2f(x - 2.6f, y - 0.34f); glVertex2f(x - 0.8f, y - 0.20f);
    glEnd();
    glColor4ub(246, 248, 252, 255);
    glBegin(GL_TRIANGLES);
        glVertex2f(x + 0.62f, y);
        glVertex2f(x - 0.40f, y + 0.13f);
        glVertex2f(x - 0.40f, y - 0.11f);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(x + 0.10f, y + 0.02f);
        glVertex2f(x - 0.34f, y + 0.46f);
        glVertex2f(x - 0.20f, y - 0.02f);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(x + 0.10f, y - 0.02f);
        glVertex2f(x - 0.30f, y - 0.30f);
        glVertex2f(x - 0.20f, y + 0.02f);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 0.34f, y + 0.02f);
        glVertex2f(x - 0.52f, y + 0.26f);
        glVertex2f(x - 0.44f, y + 0.01f);
    glEnd();
    float glint = 1.0f - SunLowness3();
    glColor4ub(255, 255, 245, (unsigned char)(220 * glint));
    FilledCircle3(x + 0.34f, y + 0.04f, 0.10f, 255, 255, 245,
                  (unsigned char)(220 * glint));
    tintOn3 = true;
}
// Timer callback: moves passenger jet along its flight path and updates jet contrail history
void UpdateJet3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateJet3, 0); return; }
    if (isAnimating3) {
        if (jetCooldown3 > 0.0f) {
            jetCooldown3 -= 1.0f;
            if (jetCooldown3 <= 0.0f) jetX3 = -90.0f;
        } else {
            jetX3 += 0.16f;
            if (jetX3 - JET_LEN3 > 70.0f) {
                jetX3 = 200.0f;
                jetCooldown3 = 900.0f + (rand() % 900);
            }
        }
    }
    glutTimerFunc(30, UpdateJet3, 0);
}
// Flock of songbirds flying across the sky above the treetops with flapping wings
void DrawBirds3()
{
    TintCol3(50, 50, 60);
    glLineWidth(2.0f);
    for (int i = 0; i < NUM_BIRDS3; i++) {
        float bx = birds3[i].cx + birds3[i].radius * cosf(birds3[i].angle);
        float by = birds3[i].cy + birds3[i].radius * sinf(birds3[i].angle) * 0.4f;
        glBegin(GL_LINE_STRIP);
            glVertex2f(bx-0.6f, by);
            glVertex2f(bx, by+0.3f);
            glVertex2f(bx+0.6f, by);
        glEnd();
    }
}
// Timer callback: advances bird flock positions and wing oscillation angles
void UpdateBirds3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateBirds3, 0); return; }
    if (isAnimating3) {
        for (int i = 0; i < NUM_BIRDS3; i++)
            birds3[i].angle += birds3[i].speed * (0.7f + 0.3f * windIntensity3);
    }
    glutTimerFunc(30, UpdateBirds3, 0);
}
// Hot air balloon floating across the sky
void DrawBalloon3()
{
    float bob = sinf(balloonBobPhase3) * 0.5f;
    glPushMatrix();
    glTranslatef(balloonX3, balloonY3 + bob, 0.0f);
    glScalef(balloonScale3, balloonScale3, 1.0f);
    unsigned char cols[4][3] = { {230, 80, 80}, {250, 210, 80}, {80, 160, 230}, {250, 250, 250} };
    const int segs = 20;
    const float cy = 1.2f, rx = 3.0f, ry = 3.4f;
    auto envX = [&](float a) -> float {
        float taper = 0.40f + 0.60f * (0.5f + 0.5f * cosf(a));
        return rx * sinf(a) * taper;
    };
    auto envY = [&](float a) -> float { return cy + ry * cosf(a); };
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i     / segs * PI3;
        float a1 = (float)(i+1) / segs * PI3;
        int c = i % 4;
        TintCol3(cols[c][0], cols[c][1], cols[c][2]);
        glBegin(GL_QUADS);
            glVertex2f(-envX(a0), envY(a0)); glVertex2f( envX(a0), envY(a0));
            glVertex2f( envX(a1), envY(a1)); glVertex2f(-envX(a1), envY(a1));
        glEnd();
    }
    TintCol4(70, 60, 55, 110);
    glLineWidth(1.0f);
    for (int s = -2; s <= 2; s++) {
        float frac = s / 2.5f;
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segs; i++) {
            float a = (float)i / segs * PI3;
            glVertex2f(envX(a) * frac, envY(a));
        }
        glEnd();
    }
    float flame = 0.55f + 0.45f * sinf(balloonBobPhase3 * 7.0f);
    TintCol4(255, 200, 120, (unsigned char)(70 * flame));
    FilledCircle3(0.0f, -2.05f, 1.05f * flame, 255, 200, 120, (unsigned char)(70 * flame));
    TintCol3(255, 170, 60);
    glBegin(GL_TRIANGLES);
        glVertex2f(-0.22f, -2.45f);
        glVertex2f( 0.22f, -2.45f);
        glVertex2f( 0.0f,  -2.45f + 0.85f * flame);
    glEnd();
    TintCol3(255, 235, 170);
    glBegin(GL_TRIANGLES);
        glVertex2f(-0.10f, -2.45f);
        glVertex2f( 0.10f, -2.45f);
        glVertex2f( 0.0f,  -2.45f + 0.45f * flame);
    glEnd();
    TintCol3(90, 70, 50);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(-1.05f, -2.35f); glVertex2f(-0.62f, -3.25f);
        glVertex2f(-0.45f, -2.50f); glVertex2f(-0.50f, -3.25f);
        glVertex2f( 0.45f, -2.50f); glVertex2f( 0.50f, -3.25f);
        glVertex2f( 1.05f, -2.35f); glVertex2f( 0.62f, -3.25f);
    glEnd();
    TintCol3(150, 110, 70);
    glBegin(GL_QUADS);
        glVertex2f(-0.8f, -4.0f); glVertex2f(0.8f, -4.0f);
        glVertex2f(0.65f, -3.25f); glVertex2f(-0.65f, -3.25f);
    glEnd();
    TintCol4(105, 75, 45, 200);
    glBegin(GL_LINES);
        for (int i = 1; i < 4; i++) {
            float t = i / 4.0f;
            float yy = -4.0f + t * 0.75f;
            glVertex2f(-0.8f + t*0.15f, yy); glVertex2f(0.8f - t*0.15f, yy);
        }
        for (int i = 1; i < 5; i++) {
            float t = i / 5.0f;
            glVertex2f(-0.8f + t*1.6f, -4.0f);
            glVertex2f(-0.65f + t*1.3f, -3.25f);
        }
    glEnd();
    TintCol3(185, 140, 90);
    glLineWidth(2.0f);
    glBegin(GL_LINES); glVertex2f(-0.68f, -3.25f); glVertex2f(0.68f, -3.25f); glEnd();
    FilledCircle3(-0.28f, -3.05f, 0.19f, 235, 195, 155, 255);
    FilledCircle3( 0.26f, -3.08f, 0.17f, 228, 186, 146, 255);
    glLineWidth(1.0f);
    glPopMatrix();
}
// Timer callback: simulates hot air balloon gentle vertical bobbing and slow wind drift
void UpdateBalloon3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateBalloon3, 0); return; }
    if (isAnimating3) {
        balloonX3 += 0.045f + 0.045f * windIntensity3;
        balloonBobPhase3 += 0.02f + 0.006f * windIntensity3;
        if (balloonScale3 < 1.30f) balloonScale3 += 0.0008f;
        float approach = (balloonScale3 - 0.42f) / (1.30f - 0.42f);
        if (approach < 0.0f) approach = 0.0f;
        if (approach > 1.0f) approach = 1.0f;
        float eased = approach * approach;
        balloonY3 = balloonCruiseY3 + (balloonApproachY3 - balloonCruiseY3) * eased;
        if (balloonX3 > 95.0f) {
            balloonX3 = -95.0f - (rand() % 20);
            balloonCruiseY3 = 27.0f + (rand() % 8);
            balloonScale3 = 0.42f + (rand() % 10) / 100.0f;
            balloonY3 = balloonCruiseY3;
        }
    }
    glutTimerFunc(30, UpdateBalloon3, 0);
}
// Soft rolling green/autumn hills across the river horizon with atmospheric haze
void DrawDistantHills3()
{
    TintCol3(120, 160, 145);
    glBegin(GL_POLYGON);
        glVertex2f(-60, -6);
        for (int i = 0; i <= 40; i++) {
            float x = -60.0f + i * 3.0f;
            float y = -6.0f + 6.5f + 3.2f*sinf(x*0.045f + 1.1f) + 1.4f*sinf(x*0.13f);
            glVertex2f(x, y);
        }
        glVertex2f(60, -6);
    glEnd();
}
// Mid-ground hillside terrain with layered tree stands and undulating crests
void DrawHills3()
{
    TintCol3(60, 140, 80);
    glBegin(GL_POLYGON);
        glVertex2f(-60, -6);
        for (int i = 0; i <= 40; i++) {
            float x = -60.0f + i * 3.0f;
            float y = -6.0f + 3.5f + 2.5f*sinf(x*0.07f) + 1.0f*sinf(x*0.19f + 2.0f);
            glVertex2f(x, y);
        }
        glVertex2f(60, -6);
    glEnd();
}
inline float RiverSurfaceY3(float x)
{
    float chop = 0.55f + 0.45f * windIntensity3;
    return LAWN_BOTTOM_Y3
         + 0.45f * chop * sinf(0.18f * x + rippleScroll3 * 0.12f)
         + 0.20f * chop * sinf(0.40f * x - rippleScroll3 * 0.09f);
}

// River with current and reflections
void DrawRiver3()
{
    SkyColor3 deep, surf, crestLo, crestHi;
    if (dayPhase3 == 0) {
        deep = {26, 62, 96};   surf = {74, 124, 168};
        crestLo = {96, 158, 200}; crestHi = {200, 228, 245};
    } else if (dayPhase3 == 1) {
        deep = {22, 62, 104};  surf = {58, 122, 178};
        crestLo = {90, 165, 215}; crestHi = {200, 235, 255};
    } else if (dayPhase3 == 2) {
        deep = {44, 54, 78};   surf = {132, 108, 104};
        crestLo = {190, 140, 100}; crestHi = {248, 214, 170};
    } else {
        deep = {10, 14, 30};   surf = {38, 44, 76};
        crestLo = {70, 74, 118};  crestHi = {186, 122, 104};
    }
    tintOn3 = false;
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 2.5f) {
        float ys = RiverSurfaceY3(x);
        glColor3ub((unsigned char)deep.r, (unsigned char)deep.g, (unsigned char)deep.b);
        glVertex2f(x, -40.0f);
        glColor3ub((unsigned char)surf.r, (unsigned char)surf.g, (unsigned char)surf.b);
        glVertex2f(x, ys);
    }
    glEnd();
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 2.5f) {
        float ys = RiverSurfaceY3(x);
        glColor4ub((unsigned char)crestLo.r, (unsigned char)crestLo.g, (unsigned char)crestLo.b, 190);
        glVertex2f(x, ys - 0.55f);
        glColor4ub((unsigned char)crestHi.r, (unsigned char)crestHi.g, (unsigned char)crestHi.b, 230);
        glVertex2f(x, ys);
    }
    glEnd();
    tintOn3 = true;
    float low     = SunLowness3();
    float sunX    = SunX3();
    float spread  = 4.5f + 5.0f * low;
    float depth   = 9.0f + 11.0f * low;
    int   streaks = 20 + (int)(18.0f * low);
    for (int i = 0; i < streaks; i++) {
        float t   = (float)i / streaks;
        float by  = -20.6f - t * depth;
        float h1  = sinf(i * 12.9898f) * 43758.5453f;
        float h2  = sinf(i * 78.233f)  * 12345.6789f;
        float j1  = h1 - floorf(h1);
        float j2  = h2 - floorf(h2);
        float halfW = spread * (0.18f + 0.55f * j1) * (1.0f - t * 0.35f);
        float cx  = sunX + (j2 - 0.5f) * spread * 1.5f
                         + sinf(rippleScroll3 * 0.22f + i * 1.7f) * 1.1f;
        unsigned char a = (unsigned char)((70.0f + 80.0f * low)
                                         * (1.0f - t) * (0.45f + 0.55f * j1));
        TintCol4(255, (unsigned char)(238 - 40 * low), (unsigned char)(195 - 70 * low), a);
        glBegin(GL_QUADS);
            glVertex2f(cx - halfW,        by);
            glVertex2f(cx + halfW,        by);
            glVertex2f(cx + halfW * 0.7f, by - 0.34f);
            glVertex2f(cx - halfW * 0.7f, by - 0.34f);
        glEnd();
    }
    for (int i = 0; i < 14; i++) {
        float bx = -66.0f + fmodf(i*9.0f + rippleScroll3, 132.0f);
        TintCol4(180, 220, 240, 90);
        glBegin(GL_QUADS);
            glVertex2f(bx, -30.5f);      glVertex2f(bx+5.0f, -30.5f);
            glVertex2f(bx+5.0f, -29.9f); glVertex2f(bx, -29.9f);
        glEnd();
    }
    for (int i = 0; i < 10; i++) {
        float bx = -66.0f + fmodf(i*13.0f + rippleScroll3 * 0.55f, 132.0f);
        TintCol4(160, 205, 232, 70);
        glBegin(GL_QUADS);
            glVertex2f(bx, -35.5f);      glVertex2f(bx+7.0f, -35.5f);
            glVertex2f(bx+7.0f, -35.0f); glVertex2f(bx, -35.0f);
        glEnd();
    }
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 2.0f) {
        float ys = RiverSurfaceY3(x);
        TintCol3(216, 196, 148);
        glVertex2f(x, ys + 1.05f);
        TintCol3(178, 154, 112);
        glVertex2f(x, ys + 0.10f);
    }
    glEnd();
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 2.0f) {
        float ys   = RiverSurfaceY3(x);
        float lace = 0.10f + 0.12f * sinf(x * 0.55f + rippleScroll3 * 0.35f);
        TintCol4(250, 252, 250, 0);
        glVertex2f(x, ys + 0.34f + lace);
        TintCol4(248, 252, 255, 205);
        glVertex2f(x, ys - 0.05f);
    }
    glEnd();
    for (int i = 0; i < 46; i++) {
        float h1 = sinf(i * 27.31f) * 43758.5453f;
        float h2 = sinf(i * 91.77f) * 12345.6789f;
        float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2);
        float px = -60.0f + j1 * 120.0f;
        float py = RiverSurfaceY3(px) + 0.25f + j2 * 0.65f;
        unsigned char g = (unsigned char)(130 + j2 * 60);
        FilledCircle3(px, py, 0.10f + j2 * 0.10f, g, (unsigned char)(g - 12), (unsigned char)(g - 34), 220);
    }
}
// Computes river water base tint according to time of day phase (morning, noon, golden hour, dusk)
void RiverSurfaceColour3(unsigned char& r, unsigned char& g, unsigned char& b)
{
    if      (dayPhase3 == 0) { r =  74; g = 124; b = 168; }
    else if (dayPhase3 == 1) { r =  58; g = 122; b = 178; }
    else if (dayPhase3 == 2) { r = 132; g = 108; b = 104; }
    else                     { r =  38; g =  44; b =  76; }
}
void DrawWithReflection3(void (*drawFn)(), float cx, float halfW,
                         float waterlineY, float depth)
{
    unsigned char wr, wg, wb;
    RiverSurfaceColour3(wr, wg, wb);
    float wobble = sinf(rippleScroll3 * 0.11f + cx * 0.05f) * 0.35f;
    BeginReflection(waterlineY, 0.58f, wobble);
    drawFn();
    EndReflection();
    WashReflection(cx + wobble, halfW, waterlineY, depth, wr, wg, wb, 120, rippleScroll3);
    drawFn();
}
// Renders V-shaped expanding foamy water wake behind moving kayaks and ducks
void DrawWake3(float x, float y, float dir, float width, float length, unsigned char alpha)
{
    TintCol4(225, 245, 255, alpha);
    glLineWidth(1.4f);
    glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(x - dir * length, y + width);
        glVertex2f(x, y);
        glVertex2f(x - dir * length, y - width);
    glEnd();
    glLineWidth(1.0f);
}
// Timer callback: updates river surface ripple movement, water currents, and wave phase
void UpdateRiver3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateRiver3, 0); return; }
    if (isAnimating3) rippleScroll3 += 0.18f + 0.12f * windIntensity3;
    glutTimerFunc(30, UpdateRiver3, 0);
}
// Animated fish leaping in an arc out of the river surface with water droplet splash rings
void DrawFishJumps3()
{
    for (int i = 0; i < MAX_FISH3; i++) {
        if (!fishJumps3[i].active) continue;
        float t = fishJumps3[i].phase;
        float surf = RiverSurfaceY3(fishJumps3[i].x);
        float y = surf + sinf(t * PI3) * 1.35f;
        float x = fishJumps3[i].x + (t - 0.5f) * 1.5f;
        glPushMatrix();
        glTranslatef(x, y, 0.0f);
        glRotatef(cosf(t*PI3) * 35.0f, 0.0f, 0.0f, 1.0f);
        TintCol3(120, 160, 190);
        FilledCircle3(0.0f, 0.0f, 0.35f, 120, 160, 190, 255);
        glBegin(GL_TRIANGLES);
            glVertex2f(-0.35f, 0.0f); glVertex2f(-0.6f, 0.22f); glVertex2f(-0.6f, -0.22f);
        glEnd();
        glPopMatrix();
        if (t < 0.08f || t > 0.92f) {
            TintCol4(210, 230, 245, 150);
            glBegin(GL_LINE_LOOP);
                for (int s = 0; s < 12; s++) {
                    float a = (float)s / 12 * 2.0f * PI3;
                    glVertex2f(fishJumps3[i].x + 0.5f*cos(a), surf + 0.18f*sin(a));
                }
            glEnd();
        }
    }
}
// Timer callback: simulates fish jumping physics trajectory and splash ring expansion
void UpdateFishJumps3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFishJumps3, 0); return; }
    if (isAnimating3) {
        fishCooldown3--;
        if (fishCooldown3 <= 0) {
            for (int i = 0; i < MAX_FISH3; i++) {
                if (!fishJumps3[i].active) {
                    fishJumps3[i].active = true;
                    float fx = -50.0f + (rand() % 1000) / 10.0f;
                    if (fx > 12.0f && fx < 28.0f) fx += (fx < 20.0f) ? -12.0f : 12.0f;
                    fishJumps3[i].x = fx;
                    fishJumps3[i].phase = 0.0f;
                    break;
                }
            }
            fishCooldown3 = 150 + rand() % 250;
        }
        for (int i = 0; i < MAX_FISH3; i++) {
            if (!fishJumps3[i].active) continue;
            fishJumps3[i].phase += 0.025f;
            if (fishJumps3[i].phase >= 1.0f) fishJumps3[i].active = false;
        }
    }
    glutTimerFunc(30, UpdateFishJumps3, 0);
}
// Two park visitors in open lawn throwing an animated flying spinning disc back and forth
void DrawFrisbeeScene3()
{
    float t = fmodf(frisbeeT3, 1.0f);
    float throwX = frisbeeBaseX3 - 6.0f + t * 12.0f;
    float throwY = -18.0f + sinf(t * PI3) * 3.0f;
    TintCol3(250, 210, 60);
    FilledCircle3(throwX, throwY, 0.3f, 250, 210, 60, 255);
    float dogX = frisbeeBaseX3 - 6.0f + t * 10.0f;
    float dogY = -18.5f;
    float legPh = sinf(t * 40.0f);
    TintCol3(190, 150, 90);
    glBegin(GL_QUADS);
        glVertex2f(dogX-0.5f, dogY+0.25f); glVertex2f(dogX+0.5f, dogY+0.25f);
        glVertex2f(dogX+0.5f, dogY+0.55f); glVertex2f(dogX-0.5f, dogY+0.55f);
    glEnd();
    FilledCircle3(dogX+0.55f, dogY+0.5f, 0.2f, 190, 150, 90, 255);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(dogX-0.3f, dogY+0.25f); glVertex2f(dogX-0.3f+0.15f*legPh, dogY);
        glVertex2f(dogX+0.3f, dogY+0.25f); glVertex2f(dogX+0.3f-0.15f*legPh, dogY);
    glEnd();
}
// Timer callback: computes parabolic flight trajectory and spin rotation of flying frisbee
void UpdateFrisbee3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFrisbee3, 0); return; }
    if (isAnimating3) frisbeeT3 += 0.006f;
    glutTimerFunc(30, UpdateFrisbee3, 0);
}
// Wooden riverside jetty / boat dock with weathered pilings, mooring cleats, and rope fenders
void DrawDock3()
{
    float x = 20.0f;
    TintCol3(120, 80, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-6.0f, -21.0f); glVertex2f(x+6.0f, -21.0f);
        glVertex2f(x+6.0f, -20.0f); glVertex2f(x-6.0f, -20.0f);
    glEnd();
    TintCol3(90, 60, 35);
    glLineWidth(2.0f);
    for (int i = -1; i <= 1; i++) {
        float px = x + i * 5.0f;
        glBegin(GL_LINES); glVertex2f(px, -21.0f); glVertex2f(px, -24.0f); glEnd();
        glBegin(GL_LINES); glVertex2f(px, -20.0f); glVertex2f(px, -19.0f); glEnd();
    }
}
void DrawKayakBody3(float x, float y, float dir, float stroke,
                    unsigned char hullR, unsigned char hullG, unsigned char hullB,
                    bool paddling)
{
    const float HALF_LEN = 3.6f;
    const int SEG = 26;
    TintCol3((unsigned char)(hullR * 0.52f), (unsigned char)(hullG * 0.58f),
             (unsigned char)(hullB * 0.72f));
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + 2.0f * (float)i / SEG;
        float hx = x + t * HALF_LEN;
        float lo = y - 0.62f + 1.52f * t * t;
        glVertex2f(hx, y);
        glVertex2f(hx, lo < y ? lo : y);
    }
    glEnd();
    TintCol3(hullR, hullG, hullB);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + 2.0f * (float)i / SEG;
        float hx = x + t * HALF_LEN;
        float up = y + 0.30f + 0.60f * t * t;
        float lo = y - 0.62f + 1.52f * t * t;
        glVertex2f(hx, up);
        glVertex2f(hx, lo);
    }
    glEnd();
    TintCol3(245, 248, 252);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + 2.0f * (float)i / SEG;
        float hx = x + t * HALF_LEN;
        float up = y + 0.30f + 0.60f * t * t;
        glVertex2f(hx, up - 0.10f);
        glVertex2f(hx, up - 0.26f);
    }
    glEnd();
    TintCol4(60, 50, 40, 190);
    glLineWidth(1.4f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t = -1.0f + 2.0f * (float)i / SEG;
        glVertex2f(x + t * HALF_LEN, y + 0.30f + 0.60f * t * t);
    }
    glEnd();
    const float cockpitX = x - dir * 0.35f;
    const float deckY    = y + 0.32f;
    TintCol4(40, 42, 48, 210);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 3; i++) {
            float a = cockpitX + dir * (1.25f + i * 0.62f);
            float b = cockpitX + dir * (1.62f + i * 0.62f);
            glVertex2f(a, deckY + 0.10f); glVertex2f(b, deckY + 0.26f);
            glVertex2f(a, deckY + 0.26f); glVertex2f(b, deckY + 0.10f);
        }
        glVertex2f(cockpitX - dir * 0.95f, deckY + 0.16f);
        glVertex2f(cockpitX - dir * 2.45f, deckY + 0.34f);
    glEnd();
    TintCol3(210, 70, 60);
    FilledCircle3(x + HALF_LEN * 0.96f, y + 0.80f, 0.12f, 210, 70, 60, 255);
    FilledCircle3(x - HALF_LEN * 0.96f, y + 0.80f, 0.12f, 210, 70, 60, 255);
    TintCol3(34, 36, 42);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cockpitX, deckY + 0.06f);
        for (int i = 0; i <= 16; i++) {
            float a = (float)i / 16.0f * 2.0f * PI3;
            glVertex2f(cockpitX + 0.86f * cosf(a), deckY + 0.06f + 0.24f * sinf(a));
        }
    glEnd();
    TintCol3(28, 30, 36);
    glLineWidth(2.2f);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 18; i++) {
            float a = (float)i / 18.0f * 2.0f * PI3;
            glVertex2f(cockpitX + 0.92f * cosf(a), deckY + 0.10f + 0.27f * sinf(a));
        }
    glEnd();
    glLineWidth(1.0f);
    if (!paddling) return;
    float swing   = sinf(stroke);
    float torsoLn = swing * 0.30f;
    float hipX    = cockpitX;
    float hipY    = deckY + 0.18f;
    float shX     = hipX + dir * 0.10f + torsoLn * 0.45f;
    float shY     = hipY + 1.02f;
    TintCol3(232, 96, 40);
    glLineWidth(6.5f);
    glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(shX, shY); glEnd();
    TintCol4(180, 60, 24, 220);
    glLineWidth(1.4f);
    glBegin(GL_LINES);
        glVertex2f(hipX + (shX - hipX) * 0.45f, hipY + 0.46f);
        glVertex2f(shX, shY - 0.12f);
    glEnd();
    float headX = shX + dir * 0.06f, headY = shY + 0.42f;
    FilledCircle3(headX, headY, 0.30f, 232, 194, 156, 255);
    TintCol3(60, 130, 190);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(headX, headY + 0.10f);
        for (int i = 0; i <= 10; i++) {
            float a = PI3 * ((float)i / 10.0f);
            glVertex2f(headX + 0.34f * cosf(a), headY + 0.10f + 0.30f * sinf(a));
        }
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(headX + dir * 0.10f, headY + 0.14f);
        glVertex2f(headX + dir * 0.10f, headY + 0.02f);
        glVertex2f(headX + dir * 0.62f, headY + 0.06f);
    glEnd();
    float pivX  = shX + dir * 0.30f;
    float pivY  = shY - 0.16f;
    float ang   = swing * 0.92f;
    float shaft = 2.05f;
    float ax = pivX + cosf(ang) * shaft * dir, ay = pivY + sinf(ang) * shaft;
    float bx = pivX - cosf(ang) * shaft * dir, by = pivY - sinf(ang) * shaft;
    TintCol3(60, 56, 52);
    glLineWidth(2.6f);
    glBegin(GL_LINES); glVertex2f(ax, ay); glVertex2f(bx, by); glEnd();
    TintCol3(232, 194, 156);
    FilledCircle3(pivX + cosf(ang) * 0.55f * dir, pivY + sinf(ang) * 0.55f, 0.13f, 232, 194, 156, 255);
    FilledCircle3(pivX - cosf(ang) * 0.55f * dir, pivY - sinf(ang) * 0.55f, 0.13f, 232, 194, 156, 255);
    bool aIsLow = (ay < by);
    float lowX = aIsLow ? ax : bx, lowY = aIsLow ? ay : by;
    float hiX  = aIsLow ? bx : ax, hiY  = aIsLow ? by : ay;
    TintCol3(250, 250, 252);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(lowX, lowY);
        for (int i = 0; i <= 12; i++) {
            float a = (float)i / 12.0f * 2.0f * PI3;
            glVertex2f(lowX + 0.34f * cosf(a), lowY + 0.72f * sinf(a));
        }
    glEnd();
    TintCol4(90, 96, 110, 200);
    glLineWidth(1.2f);
    glBegin(GL_LINES); glVertex2f(lowX, lowY - 0.70f); glVertex2f(lowX, lowY + 0.70f); glEnd();
    TintCol3(238, 240, 245);
    glBegin(GL_QUADS);
        glVertex2f(hiX - 0.09f, hiY - 0.66f); glVertex2f(hiX + 0.09f, hiY - 0.66f);
        glVertex2f(hiX + 0.09f, hiY + 0.66f); glVertex2f(hiX - 0.09f, hiY + 0.66f);
    glEnd();
    if (hiY > y + 1.2f) {
        for (int i = 0; i < 4; i++) {
            float dt = fmodf(stroke * 0.5f + i * 0.25f, 1.0f);
            float dx = hiX + (i - 1.5f) * 0.16f;
            float dy = hiY - 0.55f - dt * (hiY - y - 0.4f);
            TintCol4(205, 232, 248, (unsigned char)(215 * (1.0f - dt)));
            FilledCircle3(dx, dy, 0.075f, 205, 232, 248, (unsigned char)(215 * (1.0f - dt)));
        }
    }
    if (lowY < y + 0.55f) {
        float bite = 1.0f - (lowY - y) / 0.55f;
        if (bite > 1.0f) bite = 1.0f;
        TintCol4(248, 253, 255, (unsigned char)(190 * bite));
        FilledCircle3(lowX, y + 0.10f, 0.30f + 0.34f * bite, 248, 253, 255,
                      (unsigned char)(150 * bite));
        glLineWidth(1.3f);
        glBegin(GL_LINES);
            for (int i = 0; i < 5; i++) {
                float a = 0.5f + i * 0.45f;
                glVertex2f(lowX, y + 0.10f);
                glVertex2f(lowX + cosf(a) * 0.75f * bite, y + 0.10f + sinf(a) * 0.62f * bite);
            }
        glEnd();
    }
    glLineWidth(1.0f);
}
// First kayaker paddling downstream with animated rhythmic dual-bladed paddle strokes
void DrawKayak3()
{
    float bob  = sinf(waterClock3 * 0.5f) * 0.20f;
    float x    = kayakX3;
    float y    = -26.0f + bob;
    float dir  = 1.0f;
    DrawWake3(x - 3.4f, y + 0.15f, 1.0f, 1.4f, 8.0f, 150);
    TintCol4(235, 250, 255, 170);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + 4.4f, y + 0.55f);
        glVertex2f(x + 3.5f, y + 0.02f);
        glVertex2f(x + 4.3f, y - 0.42f);
    glEnd();
    glLineWidth(1.0f);
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(sinf(kayakStroke3) * 2.6f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-x, -y, 0.0f);
    DrawKayakBody3(x, y, dir, kayakStroke3, 250, 196, 52, true);
    glPopMatrix();
}
// Timer callback: advances kayak along river channel and synchronizes paddle dipping
void UpdateKayak3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateKayak3, 0); return; }
    if (isAnimating3) {
        kayakX3 += kayakSpeed3;
        if (kayakX3 > 70.0f) kayakX3 = -70.0f;
        kayakStroke3 += 0.105f + 0.02f * (windIntensity3 - 1.0f);
        waterClock3  += 0.08f;
    }
    glutTimerFunc(30, UpdateKayak3, 0);
}
// Second kayaker navigating river bends with colored hull and lifejacket
void DrawKayak2_3()
{
    float bob = 0.20f * sinf(waterClock3 * 0.6f + 2.0f);
    float x   = kayak2X3;
    float y   = -25.5f + bob;
    float dir = -1.0f;
    DrawWake3(x + 3.4f, y + 0.15f, -1.0f, 1.4f, 8.0f, 150);
    TintCol4(235, 250, 255, 170);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 4.4f, y + 0.55f);
        glVertex2f(x - 3.5f, y + 0.02f);
        glVertex2f(x - 4.3f, y - 0.42f);
    glEnd();
    glLineWidth(1.0f);
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(sinf(kayak2Stroke3) * -2.4f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-x, -y, 0.0f);
    DrawKayakBody3(x, y, dir, kayak2Stroke3, 62, 168, 210, true);
    glPopMatrix();
}
// Timer callback: controls movement and paddling cadence of the second kayak
void UpdateKayak2_3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateKayak2_3, 0); return; }
    if (isAnimating3) {
        kayak2X3 -= 0.042f + 0.008f * (windIntensity3 - 1.0f);
        if (kayak2X3 < -70.0f) kayak2X3 = 70.0f;
        kayak2Stroke3 += 0.118f + 0.024f * (windIntensity3 - 1.0f);
    }
    glutTimerFunc(30, UpdateKayak2_3, 0);
}
// Renders a swimming mallard duck with colored plumage, beak, and ripple wake
void DrawDuckShape3(float x, float y, bool flap)
{
    FilledCircle3(x, y, 0.5f, 230, 200, 60, 255);
    FilledCircle3(x+0.5f, y+0.35f, 0.28f, 230, 200, 60, 255);
    TintCol3(230, 140, 40);
    glBegin(GL_TRIANGLES);
        glVertex2f(x+0.75f, y+0.35f); glVertex2f(x+1.05f, y+0.4f); glVertex2f(x+0.75f, y+0.25f);
    glEnd();
    if (flap) {
        TintCol3(210, 180, 50);
        glBegin(GL_TRIANGLES);
            glVertex2f(x-0.1f, y+0.2f); glVertex2f(x-0.5f, y+0.9f); glVertex2f(x+0.2f, y+0.5f);
        glEnd();
    }
}
// Renders group of wild ducks paddling in the river currents
void DrawDucks3()
{
    for (int i = 0; i < NUM_DUCKS3; i++) {
        float bob = sinf(ducks3[i].phase) * 0.1f;
        DrawDuckShape3(ducks3[i].x, ducks3[i].y + bob, ducks3[i].flapping);
    }
}
// Timer callback: updates duck swimming paths, current drifting, and wing flap cycles
void UpdateDucks3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateDucks3, 0); return; }
    if (isAnimating3) {
        for (int i = 0; i < NUM_DUCKS3; i++) {
            ducks3[i].x += ducks3[i].speed;
            ducks3[i].phase += 0.1f;
            if (ducks3[i].x > 60.0f) ducks3[i].x = -60.0f;
            if (ducks3[i].flapping) {
                ducks3[i].flapTimer -= 1.0f;
                if (ducks3[i].flapTimer <= 0) ducks3[i].flapping = false;
            } else if (rand() % 400 == 0) {
                ducks3[i].flapping = true;
                ducks3[i].flapTimer = 12.0f;
            }
        }
    }
    glutTimerFunc(30, UpdateDucks3, 0);
}
// Mother goose followed by a neat line of goslings swimming near the river reeds
void DrawGooseFamily3()
{
    float bob = sinf(squirrelPhase3 * 2.2f) * 0.08f;
    FilledCircle3(gooseX3, -13.0f + bob, 0.42f, 235, 235, 230, 255);
    FilledCircle3(gooseX3 + gooseDir3*0.42f, -12.75f + bob, 0.20f, 235, 235, 230, 255);
    TintCol3(230, 140, 40);
    glBegin(GL_TRIANGLES);
        glVertex2f(gooseX3 + gooseDir3*0.60f, -12.75f+bob);
        glVertex2f(gooseX3 + gooseDir3*0.80f, -12.70f+bob);
        glVertex2f(gooseX3 + gooseDir3*0.60f, -12.85f+bob);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float gx = gooseX3 - gooseDir3 * (1.0f + i*0.6f);
        float gy = -12.7f + bob * 1.3f + 0.05f*sinf(squirrelPhase3*3.0f + i);
        FilledCircle3(gx, gy, 0.22f, 240, 225, 130, 255);
    }
}
// Timer callback: animates trailing formation of goose family swimming across the water
void UpdateGooseFamily3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateGooseFamily3, 0); return; }
    if (isAnimating3) {
        gooseX3 += gooseSpeed3 * gooseDir3;
        if (gooseX3 > 15.0f)  gooseDir3 = -1;
        if (gooseX3 < -15.0f) gooseDir3 = 1;
    }
    glutTimerFunc(35, UpdateGooseFamily3, 0);
}
// Duck family swimming peacefully along the shoreline
void DrawDuckFamily3()
{
    float y = -27.0f;
    float bob = sinf(squirrelPhase3 * 2.5f) * 0.08f;
    DrawDuckShape3(duckFamilyX3, y + bob, false);
    for (int i = 0; i < 3; i++) {
        float dx = duckFamilyX3 - (1.0f + i * 0.5f);
        float dy = y + bob*1.3f + 0.03f*sinf(squirrelPhase3*3.0f + i);
        FilledCircle3(dx, dy, 0.22f, 235, 210, 70, 255);
        FilledCircle3(dx+0.18f, dy+0.12f, 0.12f, 235, 210, 70, 255);
    }
}
// Timer callback: advances duck family position along river edge
void UpdateDuckFamily3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateDuckFamily3, 0); return; }
    if (isAnimating3) {
        duckFamilyX3 += duckFamilySpeed3;
        if (duckFamilyX3 > 60.0f) duckFamilyX3 = -60.0f;
    }
    glutTimerFunc(30, UpdateDuckFamily3, 0);
}
// Park riverbank lawn with subtle multi-toned grassy slope gradients
void DrawGrass3()
{
    unsigned char farR = 68,  farG = 138, farB = 84;
    unsigned char nrR  = 104, nrG  = 182, nrB  = 96;
    if (autumnMode3) { farR=134; farG=112; farB=56;  nrR=176; nrG=142; nrB=66; }
    glBegin(GL_QUADS);
        TintCol3(nrR, nrG, nrB);
        glVertex2f(-60, LAWN_BOTTOM_Y3 - 1.5f); glVertex2f(60, LAWN_BOTTOM_Y3 - 1.5f);
        TintCol3(farR, farG, farB);
        glVertex2f(60, -6);   glVertex2f(-60, -6);
    glEnd();
    TintCol4(255, 255, 255, 22);
    for (int i = 0; i < 9; i++) {
        float t0 = (float)i / 9.0f, t1 = (i + 0.5f) / 9.0f;
        float y0 = -6.0f - 14.0f * (t0 * t0);
        float y1 = -6.0f - 14.0f * (t1 * t1);
        glBegin(GL_QUADS);
            glVertex2f(-60, y1); glVertex2f(60, y1);
            glVertex2f(60, y0);  glVertex2f(-60, y0);
        glEnd();
    }
}
// Curving park promenade pathway bordered by cobblestone stone curbing
void DrawPath3()
{
    TintCol3(215, 195, 160);
    glBegin(GL_QUADS);
        glVertex2f(-60, PATH_BOTTOM_Y3); glVertex2f(60, PATH_BOTTOM_Y3);
        glVertex2f(60, PATH_TOP_Y3);     glVertex2f(-60, PATH_TOP_Y3);
    glEnd();
    TintCol4(196, 178, 144, 190);
    glBegin(GL_QUADS);
        glVertex2f(-60, PATH_TOP_Y3); glVertex2f(60, PATH_TOP_Y3);
        glVertex2f(60, PATH_TOP_Y3 + 0.35f); glVertex2f(-60, PATH_TOP_Y3 + 0.35f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(-60, PATH_BOTTOM_Y3 - 0.35f); glVertex2f(60, PATH_BOTTOM_Y3 - 0.35f);
        glVertex2f(60, PATH_BOTTOM_Y3); glVertex2f(-60, PATH_BOTTOM_Y3);
    glEnd();
    TintCol4(160, 145, 118, 130);
    glPointSize(1.8f);
    glBegin(GL_POINTS);
        for (int i = 0; i < 180; i++) {
            float h1 = sinf(i * 12.9898f) * 43758.5453f;
            float h2 = sinf(i * 78.233f)  * 12345.6789f;
            float gx = -60.0f + (h1 - floorf(h1)) * 120.0f;
            float gy = PATH_BOTTOM_Y3 + (h2 - floorf(h2)) * (PATH_TOP_Y3 - PATH_BOTTOM_Y3);
            glVertex2f(gx, gy);
        }
    glEnd();
    glPointSize(2.0f);
}
// Semicircular ornamental garden bed planted with multi-colored blossoming flowers
void DrawFlowerBed3(float x, float y, float sc)
{
    TintCol3(70, 120, 60);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f*sc, y-0.5f*sc); glVertex2f(x+3.0f*sc, y-0.5f*sc);
        glVertex2f(x+3.0f*sc, y+1.5f*sc); glVertex2f(x-3.0f*sc, y+1.5f*sc);
    glEnd();
    unsigned char cols[4][3] = { {230,80,100}, {240,200,60}, {230,120,200}, {255,255,255} };
    for (int i = 0; i < 8; i++) {
        float fx = x + (-2.5f + (i % 4) * 1.5f) * sc;
        float fy = y + (0.0f + (i / 4) * 1.2f) * sc;
        TintCol3(60, 110, 55);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(fx, fy); glVertex2f(fx, fy - 0.45f*sc);
        glEnd();
        FilledCircle3(fx, fy, 0.3f*sc, cols[i%4][0], cols[i%4][1], cols[i%4][2], 255);
    }
    glLineWidth(1.0f);
}
// Draws background flowerbeds along distant park walkways
void DrawFlowerBedsFar3()
{
    DrawFlowerBed3( 35.0f, -9.6f,  0.78f);
    DrawFlowerBed3(-18.0f, -8.9f,  0.70f);
}
// Draws foreground detailed flower beds brimming with blossoms
void DrawFlowerBedsNear3()
{
    DrawFlowerBed3(-45.0f, -15.2f, 1.15f);
}
// Park maintenance groundskeeper tending flowers with a watering can and kneeling pad
void DrawGardener3()
{
    float x = -49.0f, y = -15.0f;
    FilledCircle3(x, y+1.1f, 0.28f, 225, 185, 145, 255);
    TintCol3(90, 130, 80);
    glBegin(GL_QUADS);
        glVertex2f(x-0.22f, y+0.3f); glVertex2f(x+0.22f, y+0.3f);
        glVertex2f(x+0.2f, y+0.85f); glVertex2f(x-0.2f, y+0.85f);
    glEnd();
    TintCol3(80, 60, 50);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-0.15f, y+0.3f); glVertex2f(x-0.15f, y);
        glVertex2f(x+0.15f, y+0.3f); glVertex2f(x+0.05f, y);
    glEnd();
    TintCol3(140, 140, 150);
    glBegin(GL_QUADS);
        glVertex2f(x+0.3f, y+0.35f); glVertex2f(x+0.7f, y+0.35f);
        glVertex2f(x+0.7f, y+0.6f);  glVertex2f(x+0.3f, y+0.6f);
    glEnd();
    float t = fmodf(squirrelPhase3 * 2.0f, 1.0f);
    TintCol4(180, 210, 240, 200);
    FilledCircle3(x+0.85f, y+0.4f - t*0.3f, 0.06f, 180, 210, 240, 200);
}
// Classic wooden slat park bench with cast-iron frame and ground contact shadow
void DrawBench3(float x, float y, float sc)
{
    BeginDepthSprite(x, y, sc);
    TintCol3(110, 75, 45);
    glBegin(GL_QUADS);
        glVertex2f(x-1.6f, y);      glVertex2f(x+1.6f, y);
        glVertex2f(x+1.6f, y+0.3f); glVertex2f(x-1.6f, y+0.3f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(x-1.6f, y+0.5f); glVertex2f(x+1.6f, y+0.5f);
        glVertex2f(x+1.6f, y+1.1f); glVertex2f(x-1.6f, y+1.1f);
    glEnd();
    TintCol3(60, 60, 60);
    glBegin(GL_LINES);
        glVertex2f(x-1.4f, y); glVertex2f(x-1.4f, y-0.6f);
        glVertex2f(x+1.4f, y); glVertex2f(x+1.4f, y-0.6f);
    glEnd();
    TintCol4(80, 52, 30, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.6f, y+0.72f); glVertex2f(x+1.6f, y+0.72f);
        glVertex2f(x-1.6f, y+0.92f); glVertex2f(x+1.6f, y+0.92f);
    glEnd();
    EndDepthSprite();
}
// Renders foreground park benches with sitting visitors reading or chatting
void DrawBenchesNear3()
{
    DrawBench3(benchX3[0], BENCH_Y3,        1.08f);
    DrawBench3(benchX3[1], BENCH_Y3 + 1.4f, 1.00f);
}
// Renders distant benches along the riverbank path
void DrawBenchesFar3()
{
    DrawBench3(benchX3[2], -9.7f, 0.72f);
}
// Hexagonal wooden garden gazebo with ornate posts, railing, and shingled cupola roof
void DrawGazebo3()
{
    float x = -12.0f, y = GAZEBO_Y3;
    TintCol3(230, 225, 210);
    glLineWidth(2.0f);
    for (int i = -1; i <= 1; i += 2) {
        glBegin(GL_LINES); glVertex2f(x+i*3.0f, y); glVertex2f(x+i*3.0f, y+3.0f); glEnd();
    }
    glBegin(GL_LINES); glVertex2f(x, y); glVertex2f(x, y+3.0f); glEnd();
    TintCol3(160, 60, 60);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-4.0f, y+3.0f); glVertex2f(x+4.0f, y+3.0f); glVertex2f(x, y+5.5f);
    glEnd();
}
// Three-tiered ornamental stone park fountain with water cascading into circular pool basin
void DrawFountain3()
{
    float x = -4.5f, y = FOUNTAIN_Y3;
    TintCol3(150, 148, 152);
    glBegin(GL_POLYGON);
        for (int i = 0; i < 24; i++) { float a = (float)i/24*2*PI3; glVertex2f(x+2.5f*cos(a), y+0.95f*sin(a)); }
    glEnd();
    TintCol3(186, 184, 192);
    glBegin(GL_POLYGON);
        for (int i = 0; i < 24; i++) { float a = (float)i/24*2*PI3; glVertex2f(x+2.2f*cos(a), y+0.80f*sin(a)); }
    glEnd();
    TintCol3(120, 175, 210);
    glBegin(GL_POLYGON);
        for (int i = 0; i < 24; i++) { float a = (float)i/24*2*PI3; glVertex2f(x+1.85f*cos(a), y+0.62f*sin(a)); }
    glEnd();
    TintCol3(160, 160, 170);
    glBegin(GL_QUADS);
        glVertex2f(x-0.22f, y); glVertex2f(x+0.22f, y);
        glVertex2f(x+0.16f, y+1.1f); glVertex2f(x-0.16f, y+1.1f);
    glEnd();
    FilledCircle3(x, y+1.15f, 0.28f, 175, 175, 185, 255);
    const int   JETS = 7;
    const float g    = 26.0f;
    const float nozzleY = y + 1.25f;
    for (int j = 0; j < JETS; j++) {
        float spread = -1.0f + 2.0f * (float)j / (JETS - 1);
        float vx = spread * 3.4f + 0.55f * (windIntensity3 - 1.0f);
        float vy = 5.6f - fabsf(spread) * 1.1f;
        float wob = 1.0f + 0.05f * sinf(fountainPhase3 * 6.0f + j);
        vx *= wob; vy *= wob;
        TintCol4(215, 240, 255, 190);
        glLineWidth(1.6f);
        glBegin(GL_LINE_STRIP);
        for (int s = 0; s <= 12; s++) {
            float t  = s / 12.0f * 0.45f;
            float px = x + vx * t;
            float py = nozzleY + vy * t - 0.5f * g * t * t;
            if (py < y + 0.1f) break;
            glVertex2f(px, py);
        }
        glEnd();
        float dt = fmodf(fountainPhase3 * 0.55f + j * 0.13f, 0.45f);
        float dx = x + vx * dt;
        float dy = nozzleY + vy * dt - 0.5f * g * dt * dt;
        if (dy > y + 0.1f) FilledCircle3(dx, dy, 0.12f, 235, 248, 255, 220);
    }
    for (int r = 0; r < 3; r++) {
        float t = fmodf(fountainPhase3 * 0.7f + r * 0.33f, 1.0f);
        float rad = 0.4f + t * 1.5f;
        TintCol4(235, 250, 255, (unsigned char)(130 * (1.0f - t)));
        glLineWidth(1.2f);
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 16; i++) {
                float a = (float)i/16*2*PI3;
                glVertex2f(x + rad*cos(a), y + rad*0.33f*sin(a));
            }
        glEnd();
    }
    glLineWidth(1.0f);
}
// Timer callback: animates fountain central spray arcs and falling droplet streams
void UpdateFountain3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFountain3, 0); return; }
    if (isAnimating3) fountainPhase3 += 0.03f;
    glutTimerFunc(30, UpdateFountain3, 0);
}
// Family picnic setting on checkered blanket with picnic hamper, food, and relaxed figures
void DrawPicnic3(float x)
{
    float y = PICNIC_Y3;
    TintCol3(230, 60, 70);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f, y-1.5f); glVertex2f(x+3.0f, y-1.5f);
        glVertex2f(x+3.0f, y+1.5f); glVertex2f(x-3.0f, y+1.5f);
    glEnd();
    TintCol3(250, 250, 250);
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            if ((r+c) % 2 == 0) continue;
            float bx = x - 3.0f + c * 2.0f;
            float by = y - 1.5f + r * 1.0f;
            glBegin(GL_QUADS);
                glVertex2f(bx, by);       glVertex2f(bx+2.0f, by);
                glVertex2f(bx+2.0f, by+1.0f); glVertex2f(bx, by+1.0f);
            glEnd();
        }
    }
    TintCol3(150, 110, 70);
    glBegin(GL_QUADS);
        glVertex2f(x+1.5f, y-0.3f); glVertex2f(x+2.3f, y-0.3f);
        glVertex2f(x+2.3f, y+0.4f); glVertex2f(x+1.5f, y+0.4f);
    glEnd();
    for (int i = 0; i < 2; i++) {
        float px = x - 1.0f + i * 2.0f;
        FilledCircle3(px, y+0.9f, 0.3f, 225, 185, 145, 255);
        if (i == 0) TintCol3(70, 110, 200);
        else         TintCol3(200, 150, 60);
        glBegin(GL_QUADS);
            glVertex2f(px-0.25f, y+0.1f); glVertex2f(px+0.25f, y+0.1f);
            glVertex2f(px+0.22f, y+0.6f); glVertex2f(px-0.22f, y+0.6f);
        glEnd();
    }
}
void DrawChild3(float x, float y, float lean,
                unsigned char r, unsigned char g, unsigned char b,
                bool seated, float legKick)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(lean * 57.2958f, 0.0f, 0.0f, 1.0f);
    TintCol3(50, 55, 90);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    if (seated) {
        glVertex2f(-0.05f, 0.15f); glVertex2f(0.35f + 0.12f*legKick, 0.05f);
        glVertex2f( 0.08f, 0.15f); glVertex2f(0.45f - 0.12f*legKick, 0.12f);
    } else {
        glVertex2f(-0.05f, 0.45f); glVertex2f(-0.12f + 0.1f*legKick, 0.0f);
        glVertex2f( 0.08f, 0.45f); glVertex2f( 0.14f - 0.1f*legKick, 0.0f);
    }
    glEnd();
    TintCol3(r, g, b);
    glLineWidth(3.5f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, seated ? 0.15f : 0.45f);
        glVertex2f(0.0f, seated ? 0.72f : 1.02f);
    glEnd();
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, seated ? 0.60f : 0.90f);
        glVertex2f(0.30f, seated ? 0.78f : 1.00f);
        glVertex2f(0.0f, seated ? 0.60f : 0.90f);
        glVertex2f(-0.28f, seated ? 0.74f : 0.98f);
    glEnd();
    FilledCircle3(0.0f, seated ? 0.94f : 1.24f, 0.22f, 228, 190, 150, 255);
    glLineWidth(1.0f);
    glPopMatrix();
}
// Playground swing set with child swinging forward and back on flexible swing chains
void DrawSwing3()
{
    float x = 2.0f, topY = SWING_TOP_Y3;
    TintCol3(90, 90, 95);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-3.0f, SWING_BASE_Y3); glVertex2f(x-1.0f, topY);
        glVertex2f(x+3.0f, SWING_BASE_Y3); glVertex2f(x+1.0f, topY);
        glVertex2f(x-1.0f, topY);          glVertex2f(x+1.0f, topY);
    glEnd();
    float ropeLen = 3.6f;
    float amp   = 0.30f + 0.30f * (0.5f + 0.5f * sinf(swingAngle3 * 0.11f));
    float theta = sinf(swingAngle3) * amp;
    float seatX = x + ropeLen * sinf(theta);
    float seatY = topY - ropeLen * cosf(theta);
    TintCol3(60, 60, 65);
    glBegin(GL_LINES); glVertex2f(x, topY); glVertex2f(seatX, seatY); glEnd();
    TintCol3(200, 60, 60);
    glBegin(GL_QUADS);
        glVertex2f(seatX-0.6f, seatY);      glVertex2f(seatX+0.6f, seatY);
        glVertex2f(seatX+0.6f, seatY+0.3f); glVertex2f(seatX-0.6f, seatY+0.3f);
    glEnd();
    DrawChild3(seatX, seatY + 0.25f, theta, 90, 150, 210, true,
               sinf(swingAngle3 * 2.0f));
}
// Timer callback: simulates pendulum swing oscillation for playground swing
void UpdateSwing3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateSwing3, 0); return; }
    if (isAnimating3) swingAngle3 += 0.04f;
    glutTimerFunc(20, UpdateSwing3, 0);
}
// Playground seesaw with two animated children alternating up and down motion
void DrawSeesaw3()
{
    float x = 10.0f, y = SEESAW_Y3;
    FilledCircle3(x, y, 0.6f, 120, 90, 60, 255);
    float ang = sinf(seesawAngle3) * 0.35f;
    TintCol3(200, 150, 60);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 3.5f*cosf(ang), y + 0.6f - 3.5f*sinf(ang));
        glVertex2f(x + 3.5f*cosf(ang), y + 0.6f + 3.5f*sinf(ang));
    glEnd();
    float lx = x - 3.2f*cosf(ang), ly = y + 0.6f - 3.2f*sinf(ang);
    float rx = x + 3.2f*cosf(ang), ry = y + 0.6f + 3.2f*sinf(ang);
    DrawChild3(lx, ly + 0.15f, ang, 210, 90, 110, true,  sinf(seesawAngle3*2.0f));
    DrawChild3(rx, ry + 0.15f, ang, 110, 190, 130, true, -sinf(seesawAngle3*2.0f));
    glLineWidth(1.0f);
}
// Timer callback: oscillates seesaw angle and adjusts child seating heights
void UpdateSeesaw3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateSeesaw3, 0); return; }
    if (isAnimating3) seesawAngle3 += 0.03f;
    glutTimerFunc(20, UpdateSeesaw3, 0);
}
// Children's playground metal slide with ladder rungs and safety top platform
void DrawSlide3()
{
    float x = 16.0f, baseY = SLIDE_BASE_Y3, topY = SLIDE_TOP_Y3;
    TintCol3(120, 120, 125);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.0f, baseY); glVertex2f(x-1.0f, topY);
        glVertex2f(x-0.4f, baseY); glVertex2f(x-0.4f, topY);
    glEnd();
    for (float ry = baseY; ry < topY; ry += 1.0f) {
        glBegin(GL_LINES); glVertex2f(x-1.0f, ry); glVertex2f(x-0.4f, ry); glEnd();
    }
    TintCol3(200, 150, 60);
    glBegin(GL_QUADS);
        glVertex2f(x-1.2f, topY);       glVertex2f(x+0.5f, topY);
        glVertex2f(x+0.5f, topY+0.3f);  glVertex2f(x-1.2f, topY+0.3f);
    glEnd();
    TintCol3(230, 80, 80);
    glBegin(GL_QUADS);
        glVertex2f(x+0.5f, topY);  glVertex2f(x+0.9f, topY);
        glVertex2f(x+3.5f, baseY); glVertex2f(x+3.1f, baseY);
    glEnd();
    float loop = fmodf(slideT3, 1.0f);
    if (loop < 0.25f) {
        DrawChild3(x - 0.35f, topY + 0.3f, 0.0f, 240, 190, 70, false, 0.0f);
    } else {
        float s  = (loop - 0.25f) / 0.75f;
        float cx = (x + 0.7f) + s * 2.6f;
        float cy = topY - s * (topY - baseY);
        DrawChild3(cx, cy + 0.15f, -0.5f, 240, 190, 70, true, 0.0f);
    }
    glLineWidth(1.0f);
}
constexpr float CAFE_X3 = 40.5f;
constexpr float CAFE_Y3 = -19.0f;
// Outdoor cafe bistro table with umbrella parasol, chairs, and patron enjoying coffee
void DrawCafeTable3(float x, float y, int seed)
{
    TintCol3(122, 96, 74);
    glLineWidth(2.0f);
    glBegin(GL_LINES); glVertex2f(x, y); glVertex2f(x, y + 1.05f); glEnd();
    for (int lay = 0; lay < 2; lay++) {
        float rr = (lay == 0) ? 1.05f : 0.94f;
        float yy = y + 1.10f + lay * 0.07f;
        if (lay == 0) TintCol3(150, 122, 94); else TintCol3(196, 170, 138);
        glBegin(GL_POLYGON);
            for (int i = 0; i < 18; i++) {
                float a = (float)i / 18.0f * 2.0f * PI3;
                glVertex2f(x + rr * cosf(a), yy + rr * 0.30f * sinf(a));
            }
        glEnd();
    }
    TintCol3(250, 250, 250);
    glBegin(GL_QUADS);
        glVertex2f(x + 0.30f, y + 1.16f); glVertex2f(x + 0.62f, y + 1.16f);
        glVertex2f(x + 0.58f, y + 1.52f); glVertex2f(x + 0.34f, y + 1.52f);
    glEnd();
    for (int s = 0; s < 3; s++) {
        float t = fmodf(playPhase3 * 0.30f + s * 0.34f + seed * 0.11f, 1.0f);
        float sy = y + 1.58f + t * 1.30f;
        FilledCircle3(x + 0.46f + sinf(t * 5.5f + s) * 0.18f, sy,
                      0.07f + t * 0.08f, 244, 246, 250,
                      (unsigned char)(85 * (1.0f - t)));
    }
    for (int i = 0; i < 2; i++) {
        float cx = x + (i == 0 ? -1.55f : 1.55f);
        TintCol3(96, 78, 62);
        glLineWidth(1.8f);
        glBegin(GL_LINES);
            glVertex2f(cx, y); glVertex2f(cx, y + 0.72f);
            glVertex2f(cx - 0.34f, y + 0.72f); glVertex2f(cx + 0.34f, y + 0.72f);
        glEnd();
        unsigned char r = (i == 0) ? 210 : 90;
        unsigned char g = (i == 0) ? 120 : 150;
        unsigned char b = (i == 0) ? 140 : 190;
        BeginDepthSprite(cx, y + 0.72f, 1.32f);
        DrawChild3(cx, y + 0.72f, 0.0f, r, g, b, true,
                   0.15f * sinf(playPhase3 * 0.7f + seed));
        EndDepthSprite();
    }
    TintCol3(126, 116, 104);
    glLineWidth(2.2f);
    glBegin(GL_LINES); glVertex2f(x, y + 0.2f); glVertex2f(x, y + 3.5f); glEnd();
    const int WEDGES = 8;
    for (int i = 0; i < WEDGES; i++) {
        float a0 = PI3 * (float)i / WEDGES;
        float a1 = PI3 * (float)(i + 1) / WEDGES;
        if (i % 2 == 0) TintCol3(232, 96, 96);
        else            TintCol3(248, 246, 240);
        glBegin(GL_TRIANGLES);
            glVertex2f(x, y + 3.5f);
            glVertex2f(x + 2.05f * cosf(a0), y + 2.72f + 0.22f * sinf(a0));
            glVertex2f(x + 2.05f * cosf(a1), y + 2.72f + 0.22f * sinf(a1));
        glEnd();
    }
    TintCol4(150, 96, 96, 190);
    glLineWidth(1.4f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 16; i++) {
            float a = PI3 * (float)i / 16.0f;
            glVertex2f(x + 2.05f * cosf(a),
                       y + 2.72f + 0.22f * sinf(a) - 0.12f * fabsf(sinf(a * 8.0f)));
        }
    glEnd();
    glLineWidth(1.0f);
}
// Riverside promenade open-air cafe with counter, espresso machine, and dining terrace
void DrawCafe3()
{
    const float x = CAFE_X3, y = CAFE_Y3;
    TintCol3(146, 116, 86);
    glBegin(GL_QUADS);
        glVertex2f(x - 9.5f, y - 0.75f); glVertex2f(x + 9.8f, y - 0.75f);
        glVertex2f(x + 9.8f, y);         glVertex2f(x - 9.5f, y);
    glEnd();
    TintCol4(108, 84, 60, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i <= 18; i++) {
            float px = x - 9.5f + i * (19.3f / 18.0f);
            glVertex2f(px, y - 0.75f); glVertex2f(px, y);
        }
    glEnd();
    TintCol3(238, 230, 214);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.4f, y);         glVertex2f(x + 3.4f, y);
        glVertex2f(x + 3.4f, y + 3.30f); glVertex2f(x - 3.4f, y + 3.30f);
    glEnd();
    TintCol4(198, 188, 170, 130);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float py = y + i * 0.66f;
            glVertex2f(x - 3.4f, py); glVertex2f(x + 3.4f, py);
        }
    glEnd();
    TintCol3(62, 58, 62);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.35f, y + 1.30f); glVertex2f(x + 1.25f, y + 1.30f);
        glVertex2f(x + 1.25f, y + 2.72f); glVertex2f(x - 2.35f, y + 2.72f);
    glEnd();
    TintCol3(150, 112, 78);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.70f, y + 1.12f); glVertex2f(x + 1.60f, y + 1.12f);
        glVertex2f(x + 1.60f, y + 1.34f); glVertex2f(x - 2.70f, y + 1.34f);
    glEnd();
    {
        float lean = 0.10f * sinf(playPhase3 * 0.8f);
        TintCol3(96, 160, 170);
        glLineWidth(4.5f);
        glBegin(GL_LINES);
            glVertex2f(x - 0.75f + lean, y + 1.34f);
            glVertex2f(x - 0.70f + lean, y + 2.20f);
        glEnd();
        FilledCircle3(x - 0.68f + lean, y + 2.45f, 0.30f, 228, 190, 152, 255);
        glLineWidth(1.0f);
    }
    DrawChild3(x + 2.55f, y, 0.0f, 220, 160, 80, false,
               0.25f * sinf(playPhase3 * 0.5f));
    const int STRIPES = 9;
    for (int i = 0; i < STRIPES; i++) {
        float x0 = x - 4.1f + i * (8.2f / STRIPES);
        float x1 = x - 4.1f + (i + 1) * (8.2f / STRIPES);
        if (i % 2 == 0) TintCol3(226, 84,  84);
        else            TintCol3(248, 244, 236);
        glBegin(GL_QUADS);
            glVertex2f(x0, y + 3.30f); glVertex2f(x1, y + 3.30f);
            glVertex2f(x1, y + 2.62f); glVertex2f(x0, y + 2.62f);
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex2f(x0, y + 2.62f); glVertex2f(x1, y + 2.62f);
            glVertex2f((x0 + x1) * 0.5f, y + 2.34f);
        glEnd();
    }
    TintCol3(142, 66, 58);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 4.4f, y + 3.30f);
        glVertex2f(x + 4.4f, y + 3.30f);
        glVertex2f(x,        y + 5.40f);
    glEnd();
    TintCol4(96, 44, 40, 150);
    glLineWidth(1.2f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float t = (float)i / 5.0f;
            glVertex2f(x - 4.4f * (1.0f - t), y + 3.30f + 2.10f * t);
            glVertex2f(x + 4.4f * (1.0f - t), y + 3.30f + 2.10f * t);
        }
    glEnd();
    glLineWidth(1.0f);
    TintCol3(58, 46, 40);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f, y + 3.55f); glVertex2f(x + 1.55f, y + 3.55f);
        glVertex2f(x + 1.55f, y + 4.35f); glVertex2f(x - 1.55f, y + 4.35f);
    glEnd();
    glColor4ub(250, 228, 178, 255);
    DrawBitmapTextCentered(x, y + 3.80f, "CAFE", GLUT_BITMAP_HELVETICA_12);
    {
        float bx = x - 5.4f;
        TintCol3(104, 78, 56);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(bx - 0.5f, y); glVertex2f(bx + 0.15f, y + 1.7f);
            glVertex2f(bx + 0.6f, y); glVertex2f(bx + 0.15f, y + 1.7f);
        glEnd();
        TintCol3(48, 56, 50);
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.62f, y + 0.45f); glVertex2f(bx + 0.72f, y + 0.45f);
            glVertex2f(bx + 0.72f, y + 1.68f); glVertex2f(bx - 0.62f, y + 1.68f);
        glEnd();
        TintCol4(236, 240, 232, 200);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int i = 0; i < 4; i++) {
                float ly = y + 1.44f - i * 0.26f;
                glVertex2f(bx - 0.44f, ly);
                glVertex2f(bx + 0.30f + 0.18f * ((i % 2) ? 1.0f : -1.0f), ly);
            }
        glEnd();
        glLineWidth(1.0f);
    }
    DrawCafeTable3(x - 7.2f, y, 0);
    DrawCafeTable3(x + 6.9f, y, 1);
    {
        float lx = x - 9.0f, rx = x + 9.3f;
        float postTop = y + 4.1f, roofTop = y + 5.2f;
        TintCol3(96, 84, 72);
        glLineWidth(1.8f);
        glBegin(GL_LINES);
            glVertex2f(lx, y); glVertex2f(lx, postTop);
            glVertex2f(rx, y); glVertex2f(rx, postTop);
        glEnd();
        for (int side = 0; side < 2; side++) {
            float ax = (side == 0) ? lx : rx,  ay = postTop;
            float bx2 = x,             by2 = roofTop;
            TintCol4(70, 64, 58, 220);
            glLineWidth(1.0f);
            glBegin(GL_LINE_STRIP);
                for (int i = 0; i <= 12; i++) {
                    float t = (float)i / 12.0f;
                    float sx = ax + (bx2 - ax) * t;
                    float sy = ay + (by2 - ay) * t - 1.35f * t * (1.0f - t) * 4.0f;
                    glVertex2f(sx, sy);
                }
            glEnd();
            for (int i = 1; i < 12; i += 2) {
                float t = (float)i / 12.0f;
                float sx = ax + (bx2 - ax) * t;
                float sy = ay + (by2 - ay) * t - 1.35f * t * (1.0f - t) * 4.0f - 0.16f;
                if (LampsOn3()) {
                    float pulse = 0.80f + 0.20f * sinf(playPhase3 * 1.6f + i * 1.3f + side);
                    tintOn3 = false;
                    FilledCircle3(sx, sy, 0.85f * pulse, 255, 198, 122, 30);
                    FilledCircle3(sx, sy, 0.34f * pulse, 255, 216, 150, 90);
                    FilledCircle3(sx, sy, 0.15f, 255, 246, 212, 255);
                    tintOn3 = true;
                } else {
                    FilledCircle3(sx, sy, 0.14f, 176, 180, 184, 255);
                }
            }
        }
        glLineWidth(1.0f);
    }
}
// Traditional Victorian concert bandstand pavilion with raised platform and railings
void DrawBandstand3()
{
    const float gx = -12.0f;
    const float gy = GAZEBO_Y3 + 1.05f;
    float beat = sinf(playPhase3 * 3.2f);
    {
        float px = gx - 2.05f;
        DrawChild3(px, gy, 0.0f, 210, 90, 110, false, 0.06f * beat);
        TintCol3(126, 66, 34);
        glBegin(GL_QUADS);
            glVertex2f(px + 0.10f, gy + 1.00f); glVertex2f(px + 0.70f, gy + 1.14f);
            glVertex2f(px + 0.66f, gy + 1.36f); glVertex2f(px + 0.06f, gy + 1.22f);
        glEnd();
        TintCol3(226, 214, 190);
        glLineWidth(1.4f);
        glBegin(GL_LINES);
            glVertex2f(px + 0.18f, gy + 1.42f + 0.14f * beat);
            glVertex2f(px + 0.78f, gy + 0.96f + 0.14f * beat);
        glEnd();
        glLineWidth(1.0f);
    }
    {
        float px = gx + 0.10f;
        DrawChild3(px, gy, 0.0f, 110, 170, 210, false, -0.06f * beat);
        TintCol3(178, 116, 56);
        FilledCircle3(px + 0.40f, gy + 0.76f, 0.30f, 178, 116, 56, 255);
        FilledCircle3(px + 0.28f, gy + 0.96f, 0.22f, 178, 116, 56, 255);
        FilledCircle3(px + 0.40f, gy + 0.76f, 0.09f, 74, 48, 26, 255);
        TintCol3(96, 66, 38);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px + 0.18f, gy + 1.06f); glVertex2f(px - 0.42f, gy + 1.44f);
        glEnd();
        TintCol3(228, 190, 152);
        FilledCircle3(px + 0.46f, gy + 0.70f + 0.12f * beat, 0.10f, 228, 190, 152, 255);
        glLineWidth(1.0f);
    }
    {
        float px = gx + 2.30f;
        DrawChild3(px, gy, 0.0f, 120, 190, 130, false, 0.0f);
        TintCol3(214, 206, 196);
        glBegin(GL_QUADS);
            glVertex2f(px - 0.44f, gy + 0.54f); glVertex2f(px + 0.44f, gy + 0.54f);
            glVertex2f(px + 0.44f, gy + 0.94f); glVertex2f(px - 0.44f, gy + 0.94f);
        glEnd();
        TintCol3(186, 70, 62);
        glLineWidth(1.6f);
        glBegin(GL_LINES);
            glVertex2f(px - 0.44f, gy + 0.60f); glVertex2f(px + 0.44f, gy + 0.88f);
            glVertex2f(px - 0.44f, gy + 0.88f); glVertex2f(px + 0.44f, gy + 0.60f);
        glEnd();
        TintCol3(232, 214, 186);
        glLineWidth(1.6f);
        glBegin(GL_LINES);
            glVertex2f(px - 0.08f, gy + 1.02f + 0.26f * fabsf(beat));
            glVertex2f(px - 0.40f, gy + 0.92f);
            glVertex2f(px + 0.12f, gy + 1.02f + 0.26f * fabsf(sinf(playPhase3 * 3.2f + PI3)));
            glVertex2f(px + 0.44f, gy + 0.92f);
        glEnd();
        glLineWidth(1.0f);
    }
    for (int i = 0; i < 5; i++) {
        float t  = fmodf(playPhase3 * 0.16f + i * 0.2f, 1.0f);
        float nx = gx - 1.4f + i * 0.9f + sinf(t * 4.2f + i) * 0.9f;
        float ny = gy + 2.2f + t * 4.6f;
        unsigned char a = (unsigned char)(210 * (1.0f - t));
        TintCol4(54, 52, 66, a);
        FilledCircle3(nx, ny, 0.19f, 54, 52, 66, a);
        glLineWidth(1.6f);
        glBegin(GL_LINES);
            glVertex2f(nx + 0.17f, ny);
            glVertex2f(nx + 0.17f, ny + 0.62f);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(nx + 0.17f, ny + 0.62f);
            glVertex2f(nx + 0.52f, ny + 0.46f);
        glEnd();
        glLineWidth(1.0f);
    }
    for (int i = 0; i < 2; i++) {
        float lx = (i == 0) ? gx - 5.6f : gx + 5.4f;
        DrawFigureShadow3(lx, gy + 1.2f, 0.5f);
        DrawChild3(lx, gy + 1.2f, 0.0f, (i == 0) ? 230 : 150,
                   (i == 0) ? 170 : 130, (i == 0) ? 90 : 200, true,
                   0.10f * sinf(playPhase3 * 0.9f + i));
    }
}
// Child happily jumping over an animated rotating jump rope on the lawn
void DrawSkipping3()
{
    const float y = -16.9f, cx = -21.0f;
    float turn = playPhase3 * 2.4f;
    float arc  = cosf(turn);
    float handY = y + 1.30f;
    for (int i = 0; i < 2; i++) {
        float tx = cx + (i == 0 ? -2.5f : 2.5f);
        DrawFigureShadow3(tx, y, 0.44f);
        DrawChild3(tx, y, 0.0f, (i == 0) ? 240 : 120, (i == 0) ? 140 : 190,
                   (i == 0) ? 90 : 210, false, 0.0f);
    }
    TintCol3(238, 232, 220);
    glLineWidth(1.8f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 20; i++) {
            float u  = (float)i / 20.0f;
            float rx = cx - 2.5f + u * 5.0f;
            float ry = handY + arc * 1.65f * sinf(PI3 * u);
            glVertex2f(rx, ry);
        }
    glEnd();
    glLineWidth(1.0f);
    float lift = (arc < 0.0f) ? (-arc) * 0.42f : 0.0f;
    DrawFigureShadow3(cx, y, 0.40f + lift * 0.3f);
    DrawChild3(cx, y + lift, 0.0f, 230, 120, 170, false, 0.8f * lift);
}
// Kids playing soccer/football on grass with animated kicking and bouncing ball
void DrawBallGame3()
{
    const float y = -16.4f, lx = 22.8f, rx = 29.2f;
    float cycle = fmodf(playPhase3 * 0.34f, 2.0f);
    bool  toRight = (cycle < 1.0f);
    float t  = toRight ? cycle : (cycle - 1.0f);
    float bx = toRight ? (lx + (rx - lx) * t) : (rx - (rx - lx) * t);
    float by = y + 0.55f + sinf(PI3 * t) * 2.35f;
    float reachL = toRight ? 0.0f : (t * t);
    float reachR = toRight ? (t * t) : 0.0f;
    DrawFigureShadow3(lx, y, 0.46f);
    DrawChild3(lx, y, 0.0f, 90, 180, 140, false, 0.25f * reachL);
    DrawFigureShadow3(rx, y, 0.46f);
    DrawChild3(rx, y, 0.0f, 220, 120, 110, false, 0.25f * reachR);
    TintCol3(228, 190, 152);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(lx, y + 0.92f);
        glVertex2f(lx + 0.42f, y + 1.05f + 0.55f * reachL);
        glVertex2f(rx, y + 0.92f);
        glVertex2f(rx - 0.42f, y + 1.05f + 0.55f * reachR);
    glEnd();
    glLineWidth(1.0f);
    FilledCircle3(bx, by, 0.40f, 250, 246, 238, 255);
    float spin = bx * 1.3f;
    for (int i = 0; i < 3; i++) {
        float a = spin + i * (PI3 / 3.0f);
        unsigned char cr = (i == 0) ? 232 : ((i == 1) ? 90 : 244);
        unsigned char cg = (i == 0) ? 92  : ((i == 1) ? 160 : 200);
        unsigned char cb = (i == 0) ? 100 : ((i == 1) ? 220 : 80);
        TintCol3(cr, cg, cb);
        glBegin(GL_TRIANGLES);
            glVertex2f(bx, by);
            glVertex2f(bx + 0.40f * cosf(a),          by + 0.40f * sinf(a));
            glVertex2f(bx + 0.40f * cosf(a + 0.55f),  by + 0.40f * sinf(a + 0.55f));
        glEnd();
    }
    float h = (by - (y + 0.55f)) / 2.35f;
    DrawGroundShadow(bx, y, 0.42f * (1.0f - 0.35f * h), 0.0f,
                     (unsigned char)(70 * (1.0f - 0.6f * h)));
}
constexpr int MAX_BLOSSOMS3 = 800;
struct BlossomBlob3
{
    float x, y, r, jit;
};
BlossomBlob3 blossomBuf3[MAX_BLOSSOMS3];
int   blossomCount3 = 0;
float blossomLo3 = 0.0f, blossomHi3 = 1.0f;
float blossomLx3 = 0.0f, blossomRx3 = 1.0f;
inline float TreeRand3(int seed)
{
    float s = sinf((float)seed * 12.9898f) * 43758.5453f;
    return s - floorf(s);
}
inline int TreeSeed3(const Tree3& t)
{
    return (int)(t.x * 7.0f) + (int)(t.swayPhase * 131.0f) + 1009;
}
inline float TreeCanopyY3(const Tree3& t)
{
    return t.y + 6.4f * t.scale;
}
inline float TreeCanopyR3(const Tree3& t)
{
    return 5.6f * t.scale;
}
// Helper: records a blossoming leaf/flower cluster position into petal buffers
void PushBlossom3(float x, float y, float r, float jit)
{
    if (blossomCount3 >= MAX_BLOSSOMS3) return;
    blossomBuf3[blossomCount3].x   = x;
    blossomBuf3[blossomCount3].y   = y;
    blossomBuf3[blossomCount3].r   = r;
    blossomBuf3[blossomCount3].jit = jit;
    if (y < blossomLo3) blossomLo3 = y;
    if (y > blossomHi3) blossomHi3 = y;
    if (x < blossomLx3) blossomLx3 = x;
    if (x > blossomRx3) blossomRx3 = x;
    blossomCount3++;
}
// Generates organic clustered blossom clumps for tree canopies
void PushCluster3(float x, float y, float r, int seed)
{
    int n = 7 + (int)(TreeRand3(seed * 3 + 7) * 4.0f);
    for (int i = 0; i < n; i++) {
        float a  = TreeRand3(seed *  5 + i * 17 + 1) * 2.0f * PI3;
        float d  = TreeRand3(seed * 11 + i * 23 + 2) * r * 0.85f;
        float rr = r * (0.45f + 0.55f * TreeRand3(seed * 13 + i * 29 + 3));
        PushBlossom3(x + cosf(a) * d * 1.25f, y + sinf(a) * d * 0.80f, rr,
                     TreeRand3(seed * 17 + i * 31 + 4));
    }
}
// Returns petal color palette: cherry blossom pinks in spring or vibrant golds in autumn
void BlossomTone3(int tone, unsigned char& r, unsigned char& g, unsigned char& b)
{
    if (autumnMode3) {
        if      (tone == 0) { r = 150; g =  68; b =  34; }
        else if (tone == 1) { r = 206; g = 110; b =  44; }
        else if (tone == 2) { r = 238; g = 162; b =  62; }
        else                { r = 250; g = 214; b = 148; }
        return;
    }
    if      (tone == 0) { r = 196; g =  94; b = 140; }
    else if (tone == 1) { r = 234; g = 128; b = 176; }
    else if (tone == 2) { r = 250; g = 180; b = 212; }
    else                { r = 255; g = 240; b = 248; }
}
void DrawBranch3(float bx, float by, float ang, float len, float w,
                 int depth, int seed, float gust)
{
    const int STEPS = 4;
    float curl = (TreeRand3(seed * 3 + 1) - 0.5f) * 0.85f;
    float sway = gust * (0.04f + 0.075f * (float)(4 - depth));
    float ptx[STEPS + 1], pty[STEPS + 1];
    float cx = bx, cy = by, a = ang;
    ptx[0] = bx; pty[0] = by;
    for (int i = 1; i <= STEPS; i++) {
        a  += curl / STEPS + sway / STEPS;
        cx += cosf(a) * (len / STEPS);
        cy += sinf(a) * (len / STEPS);
        ptx[i] = cx; pty[i] = cy;
    }
    TintCol3(58, 42, 36);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= STEPS; i++) {
        float t  = (float)i / STEPS;
        float hw = w * (1.0f - 0.45f * t) * 0.5f;
        float dx, dy;
        if (i < STEPS) { dx = ptx[i+1] - ptx[i]; dy = pty[i+1] - pty[i]; }
        else           { dx = ptx[i] - ptx[i-1]; dy = pty[i] - pty[i-1]; }
        float l = sqrtf(dx * dx + dy * dy);
        if (l < 0.0001f) l = 0.0001f;
        float nx = -dy / l * hw, ny = dx / l * hw;
        glVertex2f(ptx[i] + nx, pty[i] + ny);
        glVertex2f(ptx[i] - nx, pty[i] - ny);
    }
    glEnd();
    if (depth >= 3) {
        TintCol4(96, 72, 60, 170);
        glLineWidth(1.4f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= STEPS; i++) {
            float t  = (float)i / STEPS;
            float hw = w * (1.0f - 0.45f * t) * 0.28f;
            glVertex2f(ptx[i] - hw, pty[i]);
        }
        glEnd();
        glLineWidth(1.0f);
    }
    float tipX = ptx[STEPS], tipY = pty[STEPS];
    if (depth <= 0) {
        PushCluster3(tipX, tipY, 1.95f * len + 0.95f, seed);
        return;
    }
    if (depth == 1)
        PushCluster3(ptx[STEPS / 2], pty[STEPS / 2], 1.55f * len, seed * 7 + 5);
    if (depth == 2)
        PushCluster3(tipX, tipY, 1.15f * len, seed * 11 + 9);
    int kids = (TreeRand3(seed * 19 + 6) > 0.45f) ? 3 : 2;
    for (int k = 0; k < kids; k++) {
        float r1 = TreeRand3(seed * 23 + k * 37 + depth * 5 + 8);
        float r2 = TreeRand3(seed * 29 + k * 41 + depth * 9 + 12);
        float side = (k == 0) ? -1.0f : (k == 1 ? 1.0f : (r1 - 0.5f) * 0.9f);
        float na = a + side * (0.42f + 0.46f * r1);
        if (na < 0.12f) na = 0.12f;
        if (na > 3.02f) na = 3.02f;
        DrawBranch3(tipX, tipY, na, len * (0.64f + 0.16f * r2), w * 0.56f,
                    depth - 1, seed * 31 + k * 13 + 17, gust);
    }
}
// Renders a deciduous tree with textured bark, branching limbs, and dense foliage canopy
void DrawTree3(const Tree3& t)
{
    float sc   = t.scale;
    float gust = sinf(windPhase3 + t.swayPhase) * 0.42f * windIntensity3;
    int   seed = TreeSeed3(t);
    DrawFigureShadow3(t.x, t.y, 4.0f * sc);
    TintCol3(78, 140, 76);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(t.x, t.y - 0.15f * sc);
        for (int i = 0; i <= 16; i++) {
            float a = PI3 * (float)i / 16.0f;
            glVertex2f(t.x + 2.8f * sc * cosf(a),
                       t.y - 0.15f * sc + 0.75f * sc * sinf(a));
        }
    glEnd();
    {
        unsigned char dr, dg, db;
        BlossomTone3(2, dr, dg, db);
        TintCol4(dr, dg, db, 85);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(t.x, t.y);
            for (int i = 0; i <= 20; i++) {
                float a = 2.0f * PI3 * (float)i / 20.0f;
                glVertex2f(t.x + 4.3f * sc * cosf(a), t.y + 0.95f * sc * sinf(a));
            }
        glEnd();
        for (int i = 0; i < 44; i++) {
            float h1 = TreeRand3(seed + i * 7 + 3);
            float h2 = TreeRand3(seed + i * 13 + 5);
            float a  = h1 * 2.0f * PI3;
            float d  = 0.25f + 0.75f * h2;
            float px = t.x + cosf(a) * 4.6f * sc * d;
            float py = t.y + sinf(a) * 1.05f * sc * d;
            float pr = 0.16f * sc;
            BlossomTone3(1 + (int)(h2 * 2.99f), dr, dg, db);
            TintCol4(dr, dg, db, 235);
            glBegin(GL_QUADS);
                glVertex2f(px - pr, py - pr * 0.45f);
                glVertex2f(px + pr, py - pr * 0.40f);
                glVertex2f(px + pr * 0.8f, py + pr * 0.45f);
                glVertex2f(px - pr * 0.9f, py + pr * 0.40f);
            glEnd();
        }
    }
    TintCol3(50, 36, 30);
    for (int i = -2; i <= 2; i++) {
        float rx = t.x + i * 0.62f * sc;
        glBegin(GL_TRIANGLES);
            glVertex2f(t.x, t.y + 1.7f * sc);
            glVertex2f(rx - 0.38f * sc, t.y);
            glVertex2f(rx + 0.38f * sc, t.y);
        glEnd();
    }
    blossomCount3 = 0;
    blossomLo3 =  1e9f;
    blossomHi3 = -1e9f;
    blossomLx3 =  1e9f;
    blossomRx3 = -1e9f;
    float lean = (TreeRand3(seed + 3) - 0.5f) * 0.30f;
    DrawBranch3(t.x, t.y, PI3 * 0.5f + lean, 3.4f * sc, 2.3f * sc, 4, seed, gust);
    float span = blossomHi3 - blossomLo3;
    if (span < 0.001f) span = 0.001f;
    float wide = blossomRx3 - blossomLx3;
    if (wide < 0.001f) wide = 0.001f;
    float low    = SunLowness3();
    float sunDir = (SunX3() >= 0.0f) ? 1.0f : -1.0f;
    for (int pass = 0; pass < 4; pass++) {
        unsigned char r, g, b;
        BlossomTone3(pass, r, g, b);
        for (int i = 0; i < blossomCount3; i++) {
            const BlossomBlob3& bl = blossomBuf3[i];
            float h = (bl.y - blossomLo3) / span;
            int tone = (int)(h * 2.6f + bl.jit * 1.7f);
            if (tone < 0) tone = 0;
            if (tone > 3) tone = 3;
            if (tone != pass) continue;
            float lit = 0.5f + 0.5f * sunDir
                      * ((bl.x - blossomLx3) / wide * 2.0f - 1.0f);
            float k = low * lit * 0.60f;
            TintCol4(ClampByte3(r + (255 - r) * k),
                     ClampByte3(g + (192 - g) * k),
                     ClampByte3(b + (132 - b) * k), 255);
            glBegin(GL_POLYGON);
            for (int s = 0; s < 9; s++) {
                float a = (float)s / 9.0f * 2.0f * PI3;
                glVertex2f(bl.x + bl.r * cosf(a), bl.y + bl.r * sinf(a) * 0.90f);
            }
            glEnd();
        }
    }
}

// Cherry blossom / autumn trees along the path
void DrawTrees3()
{
    int order[NUM_TREES3];
    for (int i = 0; i < NUM_TREES3; i++) order[i] = i;
    for (int i = 1; i < NUM_TREES3; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && trees3[order[j]].y < trees3[key].y) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_TREES3; i++) DrawTree3(trees3[order[i]]);
}
// Initializes a single fluttering blossom/leaf petal in the breeze
void SpawnPetal3(int i, bool initial)
{
    Petal3& p = petals3[i];
    if (p.tree >= 0) {
        const Tree3& t = trees3[p.tree];
        float cy = TreeCanopyY3(t), cr = TreeCanopyR3(t);
        float a  = (rand() % 628) / 100.0f;
        float d  = (rand() % 100) / 100.0f;
        p.x = t.x + cosf(a) * cr * (0.25f + 0.75f * d);
        p.y = cy  + sinf(a) * cr * 0.55f;
        if (initial) p.y = t.y + ((rand() % 100) / 100.0f) * (cy + cr * 0.5f - t.y);
        p.size = 0.16f + (rand() % 12) / 100.0f;
    } else {
        p.x    = -62.0f + (rand() % 1240) / 10.0f;
        p.y    = -4.0f + (rand() % 1100) / 100.0f;
        p.size = 0.30f + (rand() % 24) / 100.0f;
        p.landY = -24.0f - (rand() % 1000) / 100.0f;
    }
    p.afloat = false;
    p.fall  = 0.030f + (rand() % 35) / 1000.0f;
    p.phase = (rand() % 628) / 100.0f;
    p.swing = 0.030f + (rand() % 40) / 1000.0f;
    p.rot   = (float)(rand() % 360);
    p.spin  = -2.4f + (rand() % 480) / 100.0f;
    p.jit   = (rand() % 100) / 100.0f;
}
// Populates falling petal particle array across park airspace
void InitPetals3()
{
    float total = 0.0f;
    for (int i = 0; i < NUM_TREES3; i++) total += trees3[i].scale;
    for (int i = 0; i < MAX_PETALS3; i++) {
        if (i % 6 == 5) {
            petals3[i].tree = -1;
        } else {
            float r = (rand() % 1000) / 1000.0f * total, acc = 0.0f;
            petals3[i].tree = NUM_TREES3 - 1;
            for (int k = 0; k < NUM_TREES3; k++) {
                acc += trees3[k].scale;
                if (r <= acc) { petals3[i].tree = k; break; }
            }
        }
        SpawnPetal3(i, true);
    }
}
// Renders drifting cherry blossom / autumn leaf petals swirling on the wind
void DrawPetals3(int layer)
{
    for (int i = 0; i < MAX_PETALS3; i++) {
        const Petal3& p = petals3[i];
        int mine = p.afloat ? 2 : (p.tree < 0 ? 1 : 0);
        if (mine != layer) continue;
        unsigned char r, g, b;
        BlossomTone3(1 + (int)(p.jit * 2.99f), r, g, b);
        float sq = 0.25f + 0.75f * fabsf(sinf(p.phase * 1.7f));
        glPushMatrix();
        if (layer == 2) {
            float bob = 0.16f * sinf(rippleScroll3 * 0.11f + p.x * 0.22f);
            glTranslatef(p.x, p.y + bob, 0.0f);
            glRotatef(p.rot, 0.0f, 0.0f, 1.0f);
            glScalef(1.0f, 0.38f, 1.0f);
        } else {
            glTranslatef(p.x, p.y, 0.0f);
            glRotatef(p.rot, 0.0f, 0.0f, 1.0f);
            glScalef(sq, 1.0f, 1.0f);
        }
        TintCol4(r, g, b, layer == 0 ? 245 : (layer == 1 ? 215 : 200));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(0.0f, 0.0f);
            for (int s = 0; s <= 10; s++) {
                float a = (float)s / 10.0f * 2.0f * PI3;
                float rr = p.size * (1.0f - 0.30f * fabsf(cosf(a)));
                glVertex2f(rr * cosf(a), rr * sinf(a) * 1.30f);
            }
        glEnd();
        glPopMatrix();
    }
}
// Timer callback: simulates aerodynamics (gravity, wind gusts, fluttering wobble) for petals
void UpdatePetals3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdatePetals3, 0); return; }
    if (isAnimating3) {
        for (int i = 0; i < MAX_PETALS3; i++) {
            Petal3& p = petals3[i];
            if (p.afloat) {
                p.x   += 0.045f + 0.028f * windIntensity3;
                p.rot += p.spin * 0.05f;
                if (p.x > 64.0f) SpawnPetal3(i, false);
                continue;
            }
            p.phase += p.swing;
            p.x   += sinf(p.phase) * 0.055f + 0.010f * windIntensity3;
            p.y   -= p.fall * (0.55f + 0.45f * windIntensity3);
            p.rot += p.spin * (0.6f + 0.4f * windIntensity3);
            if (p.tree >= 0) {
                const Tree3& t = trees3[p.tree];
                if (p.y < t.y - 0.4f ||
                    fabsf(p.x - t.x) > TreeCanopyR3(t) * 2.1f) SpawnPetal3(i, false);
            } else if (p.y <= p.landY) {
                p.afloat = true;
                p.y      = p.landY;
            } else if (p.x > 64.0f) {
                SpawnPetal3(i, false);
            }
        }
    }
    glutTimerFunc(30, UpdatePetals3, 0);
}
constexpr float FERRIS_CX3     = -8.0f;
constexpr float FERRIS_CY3     =  3.5f;
constexpr float FERRIS_R3      =  7.8f;
constexpr float FERRIS_GROUND3 = -8.6f;
constexpr int   NUM_GONDOLAS3  = 10;
constexpr int   FERRIS_BULBS3  = 34;
float ferrisAngle3 = 0.0f;

// Ferris wheel with rotating gondolas
void DrawFerrisWheel3()
{
    TintCol3(96, 100, 110);
    glLineWidth(3.2f);
    glBegin(GL_LINES);
        glVertex2f(FERRIS_CX3 - 4.6f, FERRIS_GROUND3); glVertex2f(FERRIS_CX3, FERRIS_CY3);
        glVertex2f(FERRIS_CX3 + 4.6f, FERRIS_GROUND3); glVertex2f(FERRIS_CX3, FERRIS_CY3);
    glEnd();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(FERRIS_CX3 - 2.9f, FERRIS_GROUND3 + 4.2f);
        glVertex2f(FERRIS_CX3 + 2.9f, FERRIS_GROUND3 + 4.2f);
        glVertex2f(FERRIS_CX3 - 1.6f, FERRIS_GROUND3 + 7.6f);
        glVertex2f(FERRIS_CX3 + 1.6f, FERRIS_GROUND3 + 7.6f);
    glEnd();
    TintCol3(150, 148, 142);
    for (int i = -1; i <= 1; i += 2) {
        glBegin(GL_QUADS);
            glVertex2f(FERRIS_CX3 + i * 5.5f, FERRIS_GROUND3 - 0.30f);
            glVertex2f(FERRIS_CX3 + i * 3.7f, FERRIS_GROUND3 - 0.30f);
            glVertex2f(FERRIS_CX3 + i * 3.9f, FERRIS_GROUND3 + 0.55f);
            glVertex2f(FERRIS_CX3 + i * 5.3f, FERRIS_GROUND3 + 0.55f);
        glEnd();
    }
    TintCol4(120, 124, 134, 235);
    glLineWidth(1.3f);
    glBegin(GL_LINES);
    for (int i = 0; i < NUM_GONDOLAS3 * 2; i++) {
        float a = ferrisAngle3 + i * (PI3 / NUM_GONDOLAS3);
        glVertex2f(FERRIS_CX3, FERRIS_CY3);
        glVertex2f(FERRIS_CX3 + FERRIS_R3 * cosf(a), FERRIS_CY3 + FERRIS_R3 * sinf(a));
    }
    glEnd();
    TintCol3(186, 78, 92);
    glLineWidth(2.6f);
    for (int ring = 0; ring < 2; ring++) {
        float rr = FERRIS_R3 - ring * 0.55f;
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 48; i++) {
                float a = (float)i / 48.0f * 2.0f * PI3;
                glVertex2f(FERRIS_CX3 + rr * cosf(a), FERRIS_CY3 + rr * sinf(a));
            }
        glEnd();
    }
    TintCol4(186, 78, 92, 200);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 48; i++) {
            float a0 = (float)i / 48.0f * 2.0f * PI3;
            float a1 = (float)(i + 1) / 48.0f * 2.0f * PI3;
            glVertex2f(FERRIS_CX3 + FERRIS_R3 * cosf(a0),
                       FERRIS_CY3 + FERRIS_R3 * sinf(a0));
            glVertex2f(FERRIS_CX3 + (FERRIS_R3 - 0.55f) * cosf(a1),
                       FERRIS_CY3 + (FERRIS_R3 - 0.55f) * sinf(a1));
        }
    glEnd();
    for (int i = 0; i < FERRIS_BULBS3; i++) {
        float a  = (float)i / FERRIS_BULBS3 * 2.0f * PI3 + ferrisAngle3 * 0.3f;
        float bx = FERRIS_CX3 + (FERRIS_R3 - 0.28f) * cosf(a);
        float by = FERRIS_CY3 + (FERRIS_R3 - 0.28f) * sinf(a);
        if (LampsOn3()) {
            float chase = 0.35f + 0.65f * (0.5f + 0.5f * sinf(windPhase3 * 3.0f - i * 0.55f));
            tintOn3 = false;
            FilledCircle3(bx, by, 0.62f * chase, 255, 214, 140, (unsigned char)(70 * chase));
            FilledCircle3(bx, by, 0.20f, 255, (unsigned char)(226 * chase),
                          (unsigned char)(170 * chase), 255);
            tintOn3 = true;
        } else {
            FilledCircle3(bx, by, 0.17f, 226, 224, 216, 255);
        }
    }
    const unsigned char cabCols[5][3] = {
        {214, 76, 84}, {236, 176, 56}, {84, 156, 204}, {108, 176, 96}, {196, 116, 190}
    };
    for (int i = 0; i < NUM_GONDOLAS3; i++) {
        float a  = ferrisAngle3 + i * (2.0f * PI3 / NUM_GONDOLAS3);
        float cx = FERRIS_CX3 + FERRIS_R3 * cosf(a);
        float cy = FERRIS_CY3 + FERRIS_R3 * sinf(a);
        const unsigned char* c = cabCols[i % 5];
        TintCol3(110, 112, 120);
        glLineWidth(1.4f);
        glBegin(GL_LINES); glVertex2f(cx, cy); glVertex2f(cx, cy - 0.62f); glEnd();
        TintCol3(c[0], c[1], c[2]);
        glBegin(GL_QUADS);
            glVertex2f(cx - 0.70f, cy - 1.52f); glVertex2f(cx + 0.70f, cy - 1.52f);
            glVertex2f(cx + 0.80f, cy - 0.66f); glVertex2f(cx - 0.80f, cy - 0.66f);
        glEnd();
        TintCol4(40, 42, 48, 220);
        glLineWidth(1.6f);
        glBegin(GL_LINES);
            glVertex2f(cx - 0.78f, cy - 0.86f); glVertex2f(cx + 0.78f, cy - 0.86f);
        glEnd();
        TintCol3((unsigned char)(c[0] * 0.72f), (unsigned char)(c[1] * 0.72f),
                 (unsigned char)(c[2] * 0.72f));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy - 0.62f);
            for (int k = 0; k <= 10; k++) {
                float a2 = PI3 * ((float)k / 10.0f);
                glVertex2f(cx + 0.86f * cosf(a2), cy - 0.62f + 0.42f * sinf(a2));
            }
        glEnd();
        if (i % 3 != 2) {
            FilledCircle3(cx - 0.24f, cy - 1.00f, 0.17f, 236, 198, 160, 255);
            FilledCircle3(cx - 0.24f, cy - 1.18f, 0.20f, 90, 130, 200, 255);
            if (i % 2 == 0) {
                FilledCircle3(cx + 0.26f, cy - 1.04f, 0.15f, 232, 190, 152, 255);
                FilledCircle3(cx + 0.26f, cy - 1.20f, 0.18f, 206, 96, 110, 255);
            }
        }
    }
    TintCol3(96, 100, 110);
    FilledCircle3(FERRIS_CX3, FERRIS_CY3, 0.95f, 96, 100, 110, 255);
    TintCol3(150, 154, 164);
    FilledCircle3(FERRIS_CX3, FERRIS_CY3, 0.46f, 150, 154, 164, 255);
    if (LampsOn3()) {
        tintOn3 = false;
        FilledCircle3(FERRIS_CX3, FERRIS_CY3, 2.2f, 255, 226, 170, 44);
        FilledCircle3(FERRIS_CX3, FERRIS_CY3, 0.24f, 255, 246, 220, 255);
        tintOn3 = true;
    }
    glLineWidth(1.0f);
}
void DrawStripedAwning3(float cx, float topY, float halfW, float rise,
                        unsigned char r, unsigned char g, unsigned char b)
{
    const int STRIPES = 8;
    for (int i = 0; i < STRIPES; i++) {
        float t0 = (float)i / STRIPES, t1 = (float)(i + 1) / STRIPES;
        bool pale = (i % 2 == 0);
        if (pale) TintCol3(248, 246, 240);
        else      TintCol3(r, g, b);
        glBegin(GL_TRIANGLES);
            glVertex2f(cx - halfW + 2.0f * halfW * t0, topY);
            glVertex2f(cx - halfW + 2.0f * halfW * t1, topY);
            glVertex2f(cx, topY + rise);
        glEnd();
    }
    TintCol3(r, g, b);
    for (int i = 0; i <= 7; i++)
        FilledCircle3(cx - halfW + i * (2.0f * halfW / 7.0f), topY - 0.04f,
                      halfW * 0.13f, r, g, b, 255);
}
constexpr float BOOTH_X3 = -18.2f;
// Striped vintage carnival ticket kiosk with ticket roll window and signage
void DrawTicketBooth3()
{
    const float x = BOOTH_X3, y = FERRIS_GROUND3 - 0.2f;
    DrawFigureShadow3(x, y, 1.8f);
    TintCol3(228, 226, 218);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f, y);         glVertex2f(x + 1.55f, y);
        glVertex2f(x + 1.55f, y + 3.10f); glVertex2f(x - 1.55f, y + 3.10f);
    glEnd();
    TintCol4(150, 146, 136, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.52f, y); glVertex2f(x - 0.52f, y + 3.10f);
        glVertex2f(x + 0.52f, y); glVertex2f(x + 0.52f, y + 3.10f);
        glVertex2f(x - 1.55f, y + 0.55f); glVertex2f(x + 1.55f, y + 0.55f);
    glEnd();
    TintCol3(52, 62, 72);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.05f, y + 1.35f); glVertex2f(x + 1.05f, y + 1.35f);
        glVertex2f(x + 1.05f, y + 2.35f); glVertex2f(x - 1.05f, y + 2.35f);
    glEnd();
    TintCol3(176, 132, 84);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.30f, y + 1.20f); glVertex2f(x + 1.30f, y + 1.20f);
        glVertex2f(x + 1.30f, y + 1.38f); glVertex2f(x - 1.30f, y + 1.38f);
    glEnd();
    DrawStripedAwning3(x, y + 3.10f, 1.95f, 1.05f, 206, 68, 74);
    TintCol3(62, 48, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.45f, y + 2.45f); glVertex2f(x + 1.45f, y + 2.45f);
        glVertex2f(x + 1.45f, y + 3.05f); glVertex2f(x - 1.45f, y + 3.05f);
    glEnd();
    TintCol3(255, 228, 168);
    DrawBitmapTextCentered(x, y + 2.62f, "TICKETS", GLUT_BITMAP_HELVETICA_10);
    if (LampsOn3()) {
        tintOn3 = false;
        FilledCircle3(x, y + 2.60f, 1.55f, 255, 206, 132, 46);
        FilledCircle3(x, y + 2.60f, 0.24f, 255, 244, 206, 255);
        tintOn3 = true;
    }
    glLineWidth(1.0f);
}
// Renders catenary drooping festive bunting pennant flag garland between two anchor points
void DrawBuntingSpan3(float x1, float y1, float x2, float y2, int flags)
{
    TintCol4(70, 62, 54, 215);
    glLineWidth(1.2f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 24; i++) {
            float t = (float)i / 24.0f;
            float lx = x1 + (x2 - x1) * t;
            float ly = y1 + (y2 - y1) * t - 2.1f * t * (1.0f - t);
            glVertex2f(lx, ly);
        }
    glEnd();
    const unsigned char cols[4][3] = {
        {224, 78, 84}, {244, 190, 62}, {86, 162, 210}, {112, 184, 100}
    };
    for (int i = 0; i < flags; i++) {
        float t  = (i + 0.5f) / flags;
        float lx = x1 + (x2 - x1) * t;
        float ly = y1 + (y2 - y1) * t - 2.1f * t * (1.0f - t);
        float sway = sinf(windPhase3 * 1.4f + i * 0.8f) * 0.16f * windIntensity3;
        const unsigned char* c = cols[i % 4];
        TintCol3(c[0], c[1], c[2]);
        glBegin(GL_TRIANGLES);
            glVertex2f(lx - 0.26f, ly);
            glVertex2f(lx + 0.26f, ly);
            glVertex2f(lx + sway,  ly - 0.62f);
        glEnd();
    }
    glLineWidth(1.0f);
}
// Renders all decorative bunting garlands across the fairground area
void DrawBunting3()
{
    DrawBuntingSpan3(BOOTH_X3 + 1.6f, FERRIS_GROUND3 + 3.9f,
                     FERRIS_CX3 - 4.4f, FERRIS_GROUND3 + 0.6f, 7);
    DrawBuntingSpan3(FERRIS_CX3 + 4.4f, FERRIS_GROUND3 + 0.6f,
                     0.0f, FERRIS_GROUND3 + 3.4f, 7);
}
// Stanchions and velvet queue guide ropes leading up to fairground ride entrances
void DrawQueueLane3()
{
    const float y = FERRIS_GROUND3 - 0.35f;
    for (int i = 0; i < 4; i++) {
        float px = FERRIS_CX3 - 3.3f + i * 2.2f;
        TintCol3(92, 96, 104);
        glLineWidth(2.0f);
        glBegin(GL_LINES); glVertex2f(px, y); glVertex2f(px, y + 1.25f); glEnd();
        FilledCircle3(px, y + 1.32f, 0.16f, 188, 158, 82, 255);
        FilledCircle3(px, y, 0.26f, 92, 96, 104, 255);
        if (i < 3) {
            TintCol4(196, 76, 76, 235);
            glLineWidth(1.8f);
            glBegin(GL_LINE_STRIP);
                for (int k = 0; k <= 8; k++) {
                    float t = (float)k / 8.0f;
                    glVertex2f(px + t * 2.2f, y + 1.18f - 0.32f * t * (1.0f - t) * 4.0f * 0.25f);
                }
            glEnd();
        }
    }
    glLineWidth(1.0f);
}
constexpr float STALL_X3 = 15.0f;
// Park cotton candy / ice cream food kiosk with striped awning and menu board
void DrawRefreshmentStall3()
{
    const float x = STALL_X3, y = -9.3f;
    DrawFigureShadow3(x, y, 2.1f);
    TintCol3(158, 112, 70);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.05f, y);         glVertex2f(x + 2.05f, y);
        glVertex2f(x + 2.05f, y + 1.55f); glVertex2f(x - 2.05f, y + 1.55f);
    glEnd();
    TintCol4(104, 72, 44, 180);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float px = x - 2.05f + i * 0.82f;
            glVertex2f(px, y); glVertex2f(px, y + 1.55f);
        }
    glEnd();
    TintCol3(120, 88, 56);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 2.05f, y + 1.55f); glVertex2f(x - 2.05f, y + 3.05f);
        glVertex2f(x + 2.05f, y + 1.55f); glVertex2f(x + 2.05f, y + 3.05f);
    glEnd();
    for (int i = 0; i < 4; i++) {
        float gx = x - 1.35f + i * 0.90f;
        unsigned char jr = (i % 2) ? 236 : 210, jg = (i % 2) ? 150 : 90, jb = (i % 2) ? 70 : 120;
        TintCol3(jr, jg, jb);
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.20f, y + 1.55f); glVertex2f(gx + 0.20f, y + 1.55f);
            glVertex2f(gx + 0.20f, y + 2.05f); glVertex2f(gx - 0.20f, y + 2.05f);
        glEnd();
        TintCol3(240, 240, 244);
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.22f, y + 2.05f); glVertex2f(gx + 0.22f, y + 2.05f);
            glVertex2f(gx + 0.22f, y + 2.16f); glVertex2f(gx - 0.22f, y + 2.16f);
        glEnd();
    }
    DrawStripedAwning3(x, y + 3.05f, 2.45f, 1.10f, 84, 154, 108);
    TintCol3(62, 48, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.65f, y + 2.42f); glVertex2f(x + 1.65f, y + 2.42f);
        glVertex2f(x + 1.65f, y + 3.00f); glVertex2f(x - 1.65f, y + 3.00f);
    glEnd();
    TintCol3(255, 228, 168);
    DrawBitmapTextCentered(x, y + 2.58f, "LEMONADE", GLUT_BITMAP_HELVETICA_10);
    if (LampsOn3()) {
        tintOn3 = false;
        FilledCircle3(x, y + 2.55f, 1.70f, 255, 206, 132, 44);
        FilledCircle3(x, y + 2.55f, 0.22f, 255, 244, 206, 255);
        tintOn3 = true;
    }
    glLineWidth(1.0f);
}
// Composes the riverside fairground district: rides, games, booths, and carnival flags
void DrawFairground3()
{
    DrawTicketBooth3();
    DrawQueueLane3();
    DrawBunting3();
}
// Timer callback: rotates giant Ferris wheel structure and keeps passenger gondolas level
void UpdateFerrisWheel3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFerrisWheel3, 0); return; }
    if (isAnimating3) ferrisAngle3 += 0.0042f;
    glutTimerFunc(30, UpdateFerrisWheel3, 0);
}
// Fluffy rounded landscape shrub with leafy gradient shading
void DrawBush3(float x, float y, float scale)
{
    float sway = sinf(windPhase3 * 0.8f) * 0.15f * windIntensity3;
    FilledCircle3(x+sway, y, 0.9f*scale, 60, 130, 60, 255);
    FilledCircle3(x+sway-0.6f*scale, y-0.1f, 0.65f*scale, 65, 140, 65, 255);
    FilledCircle3(x+sway+0.6f*scale, y-0.1f, 0.65f*scale, 65, 140, 65, 255);
}
// Renders decorative shrubs lining walkways and garden boundaries
void DrawBushes3()
{
    DrawBush3(-55.0f,  -9.4f, 1.0f * DepthScale(-9.4f) / DepthScale(-13.8f));
    DrawBush3(-17.0f, -10.1f, 0.8f * DepthScale(-10.1f) / DepthScale(-13.8f));
    DrawBush3( 37.0f,  -9.4f, 0.9f * DepthScale(-9.4f) / DepthScale(-13.8f));
    DrawBush3( 52.0f, -16.8f, 1.25f);
}
// Manicured formal boxwood hedge border along pathway edges
void DrawHedgeRun3(float x0, float x1, float y, float h, float scale)
{
    int lobes = (int)((x1 - x0) / (0.85f * scale)) + 1;
    if (lobes < 2) lobes = 2;
    DrawGroundShadow((x0 + x1) * 0.5f, y, (x1 - x0) * 0.5f, 0.3f, 60);
    for (int pass = 0; pass < 2; pass++) {
        float lift = (pass == 0) ? 0.0f : 0.22f * scale;
        unsigned char r = (pass == 0) ? 44  : 78;
        unsigned char g = (pass == 0) ? 96  : 146;
        unsigned char b = (pass == 0) ? 52  : 70;
        for (int i = 0; i < lobes; i++) {
            float lx = x0 + (x1 - x0) * (float)i / (lobes - 1);
            float jitter = sinf(lx * 2.3f) * 0.12f * scale;
            FilledCircle3(lx, y + h * 0.62f + lift + jitter, 0.62f * scale, r, g, b, 255);
        }
    }
    TintCol3(92, 158, 78);
    glBegin(GL_QUADS);
        glVertex2f(x0 - 0.4f * scale, y + h * 0.62f + 0.30f * scale);
        glVertex2f(x1 + 0.4f * scale, y + h * 0.62f + 0.30f * scale);
        glVertex2f(x1 + 0.4f * scale, y + h * 0.62f + 0.46f * scale);
        glVertex2f(x0 - 0.4f * scale, y + h * 0.62f + 0.46f * scale);
    glEnd();
    TintCol3(38, 84, 46);
    glBegin(GL_QUADS);
        glVertex2f(x0 - 0.3f * scale, y);
        glVertex2f(x1 + 0.3f * scale, y);
        glVertex2f(x1 + 0.3f * scale, y + h * 0.30f);
        glVertex2f(x0 - 0.3f * scale, y + h * 0.30f);
    glEnd();
}
// Clump of ornamental feather reed grass swaying gently in the wind
void DrawOrnamentalGrass3(float x, float y, float scale, int seed)
{
    for (int i = 0; i < 11; i++) {
        float h1   = sinf((seed * 17 + i) * 12.9898f) * 43758.5453f;
        float j    = h1 - floorf(h1);
        float bx   = x + (j - 0.5f) * 1.9f * scale;
        float len  = (1.6f + j * 1.5f) * scale;
        float lean = ((i % 2) ? 1.0f : -1.0f) * (0.4f + j * 0.5f) * scale;
        float sway = sinf(windPhase3 * 1.3f + i * 0.9f + seed) * 0.32f * windIntensity3 * scale;
        TintCol3(126, 158, 84);
        glLineWidth(1.6f);
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 5; k++) {
                float t = (float)k / 5.0f;
                glVertex2f(bx + (lean + sway) * t * t, y + len * t);
            }
        glEnd();
        if (j > 0.62f) {
            TintCol3(206, 188, 138);
            FilledCircle3(bx + lean + sway, y + len, 0.13f * scale, 206, 188, 138, 255);
        }
    }
    glLineWidth(1.0f);
}
// Clustered planting of mixed flowering shrubs and garden plants
void DrawShrubCluster3(float x, float y, float scale)
{
    DrawGroundShadow(x, y, 1.7f * scale, 0.25f, 62);
    const float off[5][3] = {
        { -1.15f, 0.34f, 0.80f }, { 1.10f, 0.30f, 0.74f }, { 0.0f, 0.62f, 0.96f },
        { -0.55f, 0.95f, 0.62f }, { 0.62f, 0.92f, 0.58f }
    };
    for (int i = 0; i < 5; i++) {
        bool top = (i >= 3);
        FilledCircle3(x + off[i][0] * scale, y + off[i][1] * scale, off[i][2] * scale,
                      top ? 96 : 54, top ? 164 : 112, top ? 76 : 58, 255);
    }
    TintCol3(206, 72, 66);
    for (int i = 0; i < 4; i++)
        FilledCircle3(x + (-0.8f + i * 0.55f) * scale, y + (0.5f + (i % 2) * 0.4f) * scale,
                      0.11f * scale, 206, 72, 66, 255);
}
// Slotted wooden park waste bin mounted on concrete base
void DrawLitterBin3(float x, float y, float scale)
{
    DrawGroundShadow(x, y, 0.7f * scale, 0.25f, 70);
    TintCol3(62, 76, 64);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.44f * scale, y);
        glVertex2f(x + 0.44f * scale, y);
        glVertex2f(x + 0.38f * scale, y + 1.35f * scale);
        glVertex2f(x - 0.38f * scale, y + 1.35f * scale);
    glEnd();
    TintCol4(34, 44, 36, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 4; i++) {
            float px = x + (-0.44f + i * 0.22f) * scale;
            glVertex2f(px, y + 0.10f * scale); glVertex2f(px, y + 1.25f * scale);
        }
    glEnd();
    TintCol3(84, 100, 86);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y + 1.35f * scale);
        for (int i = 0; i <= 10; i++) {
            float a = PI3 * ((float)i / 10.0f);
            glVertex2f(x + 0.50f * scale * cosf(a), y + 1.35f * scale + 0.34f * scale * sinf(a));
        }
    glEnd();
    TintCol3(26, 32, 28);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.22f * scale, y + 1.42f * scale);
        glVertex2f(x + 0.22f * scale, y + 1.42f * scale);
        glVertex2f(x + 0.22f * scale, y + 1.56f * scale);
        glVertex2f(x - 0.22f * scale, y + 1.56f * scale);
    glEnd();
    glLineWidth(1.0f);
}
// Park information signboard with nature trail map and event announcements
void DrawNoticeboard3(float x, float y, float scale)
{
    DrawGroundShadow(x, y, 1.5f * scale, 0.3f, 66);
    TintCol3(104, 76, 48);
    glLineWidth(2.6f * scale);
    glBegin(GL_LINES);
        glVertex2f(x - 1.05f * scale, y); glVertex2f(x - 1.05f * scale, y + 2.0f * scale);
        glVertex2f(x + 1.05f * scale, y); glVertex2f(x + 1.05f * scale, y + 2.0f * scale);
    glEnd();
    TintCol3(126, 92, 58);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.35f * scale, y + 1.85f * scale);
        glVertex2f(x + 1.35f * scale, y + 1.85f * scale);
        glVertex2f(x + 1.35f * scale, y + 3.70f * scale);
        glVertex2f(x - 1.35f * scale, y + 3.70f * scale);
    glEnd();
    TintCol3(214, 226, 198);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.12f * scale, y + 2.02f * scale);
        glVertex2f(x + 1.12f * scale, y + 2.02f * scale);
        glVertex2f(x + 1.12f * scale, y + 3.50f * scale);
        glVertex2f(x - 1.12f * scale, y + 3.50f * scale);
    glEnd();
    TintCol3(112, 164, 206);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.12f * scale, y + 2.12f * scale);
        glVertex2f(x + 1.12f * scale, y + 2.12f * scale);
        glVertex2f(x + 1.12f * scale, y + 2.42f * scale);
        glVertex2f(x - 1.12f * scale, y + 2.42f * scale);
    glEnd();
    TintCol3(226, 208, 172);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.12f * scale, y + 2.72f * scale);
        glVertex2f(x + 1.12f * scale, y + 2.72f * scale);
        glVertex2f(x + 1.12f * scale, y + 2.92f * scale);
        glVertex2f(x - 1.12f * scale, y + 2.92f * scale);
    glEnd();
    TintCol3(214, 62, 58);
    FilledCircle3(x - 0.30f * scale, y + 2.82f * scale, 0.11f * scale, 214, 62, 58, 255);
    TintCol3(86, 62, 42);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.60f * scale, y + 3.70f * scale);
        glVertex2f(x + 1.60f * scale, y + 3.70f * scale);
        glVertex2f(x, y + 4.40f * scale);
    glEnd();
    glLineWidth(1.0f);
}
// Serene stone-rimmed lily pond with floating lily pads, flowering blooms, and sky reflection
void DrawOrnamentalPond3(float x, float y, float rx, float ry)
{
    TintCol3(168, 164, 156);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= 28; i++) {
            float a = (float)i / 28.0f * 2.0f * PI3;
            glVertex2f(x + (rx + 0.55f) * cosf(a), y + (ry + 0.30f) * sinf(a));
        }
    glEnd();
    TintCol4(120, 116, 108, 180);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 18; i++) {
            float a = (float)i / 18.0f * 2.0f * PI3;
            glVertex2f(x + rx * cosf(a), y + ry * sinf(a));
            glVertex2f(x + (rx + 0.55f) * cosf(a), y + (ry + 0.30f) * sinf(a));
        }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
        TintCol3(118, 172, 186);
        glVertex2f(x, y);
        TintCol3(52, 104, 128);
        for (int i = 0; i <= 28; i++) {
            float a = (float)i / 28.0f * 2.0f * PI3;
            glVertex2f(x + rx * cosf(a), y + ry * sinf(a));
        }
    glEnd();
    for (int i = 0; i < 3; i++) {
        float t  = playPhase3 * (0.16f + i * 0.05f) + i * 2.1f;
        float fx = x + cosf(t) * rx * 0.58f;
        float fy = y + sinf(t * 1.3f) * ry * 0.45f;
        float dirx = -sinf(t);
        TintCol4(236, 138, 68, 210);
        FilledCircle3(fx, fy, 0.24f, 236, 138, 68, 210);
        glBegin(GL_TRIANGLES);
            glVertex2f(fx - dirx * 0.22f, fy);
            glVertex2f(fx - dirx * 0.58f, fy + 0.17f);
            glVertex2f(fx - dirx * 0.58f, fy - 0.17f);
        glEnd();
    }
    const float pads[4][3] = {
        { -0.55f, 0.42f, 0.62f }, { 0.48f, -0.35f, 0.52f },
        { 0.12f, 0.55f, 0.44f }, { -0.72f, -0.40f, 0.40f }
    };
    for (int i = 0; i < 4; i++) {
        float px = x + pads[i][0] * rx;
        float py = y + pads[i][1] * ry;
        float pr = pads[i][2];
        TintCol3(62, 128, 66);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(px, py);
            for (int k = 0; k <= 14; k++) {
                float a = 0.45f + (float)k / 14.0f * (2.0f * PI3 - 0.9f);
                glVertex2f(px + pr * cosf(a), py + pr * 0.42f * sinf(a));
            }
        glEnd();
        if (i < 2) {
            TintCol3(246, 186, 208);
            FilledCircle3(px + pr * 0.2f, py + 0.12f, 0.17f, 246, 186, 208, 255);
            TintCol3(252, 228, 150);
            FilledCircle3(px + pr * 0.2f, py + 0.12f, 0.07f, 252, 228, 150, 255);
        }
    }
    float rt = fmodf(playPhase3 * 0.10f, 1.0f);
    TintCol4(226, 244, 250, (unsigned char)(140 * (1.0f - rt)));
    glLineWidth(1.2f);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 20; i++) {
            float a = (float)i / 20.0f * 2.0f * PI3;
            glVertex2f(x + rx * 0.75f * rt * cosf(a), y + ry * 0.75f * rt * sinf(a));
        }
    glEnd();
    DrawOrnamentalGrass3(x - rx * 0.75f, y + ry * 0.55f, 0.8f, 7);
    DrawOrnamentalGrass3(x + rx * 0.70f, y + ry * 0.50f, 0.7f, 11);
    glLineWidth(1.0f);
}
// Ambient background strollers and scenery details populating distant park areas
void DrawFarFillers3()
{
    DrawHedgeRun3(-60.0f, -56.0f, -8.8f, 1.5f, 0.92f);
    DrawOrnamentalGrass3(-57.0f, -8.4f, 0.85f, 3);
    DrawHedgeRun3(19.2f, 22.0f, -9.1f, 1.4f, 0.88f);
    DrawOrnamentalGrass3(12.6f, -9.0f, 0.90f, 5);
    DrawOrnamentalGrass3(20.6f, -8.7f, 0.80f, 9);
}
// Left foreground character details: pet owner with playful terrier dog
void DrawNearFillersLeft3()
{
    DrawOrnamentalPond3(-55.0f, -17.4f, 3.6f, 1.15f);
    DrawNoticeboard3(-59.4f, -14.4f, 1.0f);
    DrawShrubCluster3(-50.6f, -19.2f, 1.0f);
}
// Right foreground character details: photographer with tripod camera
void DrawNearFillersRight3()
{
    DrawBench3(47.0f, -17.6f, 1.06f);
    DrawLitterBin3(50.6f, -18.2f, 1.1f);
    DrawHedgeRun3(54.5f, 60.0f, -19.0f, 1.6f, 1.15f);
    DrawShrubCluster3(57.0f, -15.4f, 1.05f);
}
// Small squirrel with bushy tail foraging acorns near the roots of an oak tree
void DrawSquirrel3()
{
    float x = 25.0f, y = -14.5f;
    float scurry = sinf(squirrelPhase3) * 1.5f;
    x += scurry;
    FilledCircle3(x, y, 0.28f, 150, 100, 60, 255);
    FilledCircle3(x + (scurry>=0?0.28f:-0.28f), y+0.18f, 0.18f, 150, 100, 60, 255);
    float tailX = x - (scurry>=0?0.35f:-0.35f);
    FilledCircle3(tailX, y+0.35f, 0.24f, 160, 110, 65, 220);
}
// Pair of animated butterflies fluttering around sunny flower beds
void DrawButterflies3()
{
    for (int i = 0; i < NUM_BUTTERFLIES3; i++) {
        const Butterfly3& b = butterflies3[i];
        float t = squirrelPhase3 * 1.3f + b.phase;
        float x = b.baseX + sinf(t) * 1.8f;
        float y = b.baseY + sinf(t * 2.0f) * 0.6f + 0.4f;
        float wing = fabsf(sinf(t * 6.0f)) * 0.25f + 0.08f;
        TintCol4(255, 210, 90, 230);
        glBegin(GL_TRIANGLES);
            glVertex2f(x, y); glVertex2f(x-wing, y+0.18f); glVertex2f(x-0.03f, y-0.05f);
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex2f(x, y); glVertex2f(x+wing, y+0.18f); glVertex2f(x+0.03f, y-0.05f);
        glEnd();
    }
}
// Initializes firefly particle positions and randomized blinking phase offsets
void InitFireflies3()
{
    for (int i = 0; i < NUM_FIREFLIES3; i++) {
        fireflies3[i].x = -52.0f + (rand() % 1040) / 10.0f;
        fireflies3[i].y = -18.5f + (rand() % 100) / 10.0f;
        fireflies3[i].phase = (rand() % 628) / 100.0f;
        fireflies3[i].speed = 0.4f + (rand() % 60) / 100.0f;
    }
}
// Glowing firefly particles at dusk
void DrawFireflies3()
{
    if (dayPhase3 < 2) return;
    for (int i = 0; i < NUM_FIREFLIES3; i++) {
        const Firefly3& f = fireflies3[i];
        float dx = sinf(fireflyClock3 * f.speed + f.phase) * 1.6f;
        float dy = cosf(fireflyClock3 * f.speed * 0.7f + f.phase * 1.3f) * 0.9f;
        float blink = sinf(fireflyClock3 * 2.6f + f.phase * 2.0f);
        if (blink <= 0.0f) continue;
        unsigned char a = (unsigned char)((IsDusk3() ? 255 : 190) * blink);
        tintOn3 = false;
        FilledCircle3(f.x + dx, f.y + dy, 0.30f, 210, 255, 130, (unsigned char)(a / 5));
        FilledCircle3(f.x + dx, f.y + dy, 0.12f, 235, 255, 170, a);
        tintOn3 = true;
    }
}
// Timer callback: animates firefly gentle floating drift and luminous glowing pulses
void UpdateFireflies3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFireflies3, 0); return; }
    if (isAnimating3) {
        fireflyClock3 += 0.05f;
        slideT3       += 0.004f;
    }
    glutTimerFunc(30, UpdateFireflies3, 0);
}
// Tall stainless steel flagpole with municipal flag rippling in the river breeze
void DrawFlagpole3()
{
    float x = -52.0f, baseY = -14.0f, topY = -6.0f;
    TintCol3(150, 150, 150);
    glLineWidth(2.0f);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    float wave = sinf(windPhase3 * 1.5f) * 0.4f * windIntensity3;
    TintCol3(210, 60, 60);
    glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(x, topY-0.2f);
        glVertex2f(x+2.2f+wave, topY-0.5f);
        glVertex2f(x, topY-1.0f);
        glVertex2f(x+1.8f+wave*1.3f, topY-1.3f);
    glEnd();
}
// Timer callback: updates dynamic wind strength and directional turbulence oscillations
void UpdateWind3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateWind3, 0); return; }
    if (isAnimating3) {
        windPhase3 += 0.05f;
        squirrelPhase3 += 0.04f;
        playPhase3 += 0.045f;
    }
    glutTimerFunc(30, UpdateWind3, 0);
}
// Child standing on open lawn actively flying a high-altitude kite
void DrawKiteHolder3()
{
    float x = 18.0f, y = -11.0f;
    FilledCircle3(x, y+1.6f, 0.32f, 225, 185, 145, 255);
    TintCol3(90, 140, 200);
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(x, y+1.3f); glVertex2f(x, y+0.3f); glEnd();
    TintCol3(40, 40, 50);
    glBegin(GL_LINES);
        glVertex2f(x, y+0.3f); glVertex2f(x-0.25f, y);
        glVertex2f(x, y+0.3f); glVertex2f(x+0.25f, y);
    glEnd();
}
// Fine catenary curved string tensioned between kite runner and flying kite
void DrawKiteString3()
{
    float holderX = 18.0f, holderY = PATH_BOTTOM_Y3 + 1.9f;
    float kx = kiteBaseX3 + sinf(windPhase3*0.8f) * 1.5f * windIntensity3;
    float ky = kiteBaseY3 + sinf(kiteBobPhase3) * 0.6f;
    float slack = 1.0f - (windIntensity3 - 1.0f) / 3.2f;
    if (slack < 0.06f) slack = 0.06f;
    TintCol3(230, 230, 230);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
        const int segs = 14;
        for (int i = 0; i <= segs; i++) {
            float t = (float)i / segs;
            float sx = holderX + (kx - holderX) * t;
            float sy = holderY + (ky - holderY) * t;
            sy -= 4.2f * slack * t * (1.0f - t);
            sy += sinf(t*6.0f + windPhase3) * 0.35f * (1.0f - t) * windIntensity3;
            glVertex2f(sx, sy);
        }
    glEnd();
}
// Diamond-shaped colorful stunt kite with fluttering ribbon tail streamers
void DrawKite3()
{
    float kx = kiteBaseX3 + sinf(windPhase3*0.8f) * 1.5f * windIntensity3;
    float ky = kiteBaseY3 + sinf(kiteBobPhase3) * 0.6f;
    TintCol3(230, 70, 120);
    glBegin(GL_QUADS);
        glVertex2f(kx, ky+1.2f); glVertex2f(kx+1.0f, ky);
        glVertex2f(kx, ky-1.2f); glVertex2f(kx-1.0f, ky);
    glEnd();
    TintCol3(60, 60, 70);
    glBegin(GL_LINES);
        glVertex2f(kx, ky+1.2f);  glVertex2f(kx, ky-1.2f);
        glVertex2f(kx-1.0f, ky);  glVertex2f(kx+1.0f, ky);
    glEnd();
    TintCol3(255, 210, 60);
    for (int i = 1; i <= 3; i++) {
        float tx = kx - i * 0.8f;
        float ty = ky - 1.2f - i * 0.5f + sinf(windPhase3 + i) * 0.3f;
        FilledCircle3(tx, ty, 0.25f, 255, 210, 60, 255);
    }
}
// Timer callback: animates kite aerodynamic pitch, sway, and tail rippling
void UpdateKite3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateKite3, 0); return; }
    if (isAnimating3) {
        kiteBobPhase3 += 0.04f + 0.03f * windIntensity3;
        float target = 16.0f + 3.4f * windIntensity3;
        kiteBaseY3 += (target - kiteBaseY3) * 0.02f;
    }
    glutTimerFunc(30, UpdateKite3, 0);
}
constexpr float JETTY_LEFT_X3  = -52.0f;
constexpr float JETTY_RIGHT_X3 = -22.0f;
constexpr float JETTY_DECK_Y3  = -28.5f;
// Heavy timber framing, pilings, and wooden planking supporting the river jetty pier
void DrawJettyStructure3()
{
    const float L = JETTY_LEFT_X3, R = JETTY_RIGHT_X3, D = JETTY_DECK_Y3;
    for (int i = 0; i < 5; i++) {
        float px = L + 1.8f + i * ((R - L - 3.6f) / 4.0f);
        TintCol3(96, 72, 48);
        glBegin(GL_QUADS);
            glVertex2f(px - 0.42f, D - 5.6f); glVertex2f(px + 0.42f, D - 5.6f);
            glVertex2f(px + 0.34f, D);        glVertex2f(px - 0.34f, D);
        glEnd();
        TintCol4(52, 40, 30, 190);
        glBegin(GL_QUADS);
            glVertex2f(px - 0.42f, D - 3.1f); glVertex2f(px + 0.42f, D - 3.1f);
            glVertex2f(px + 0.40f, D - 1.9f); glVertex2f(px - 0.40f, D - 1.9f);
        glEnd();
        TintCol4(72, 104, 62, 150);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(px - 0.30f, D - 2.9f); glVertex2f(px - 0.62f, D - 2.1f);
            glVertex2f(px + 0.30f, D - 2.8f); glVertex2f(px + 0.60f, D - 2.0f);
        glEnd();
    }
    TintCol3(84, 62, 42);
    glLineWidth(2.4f);
    glBegin(GL_LINES);
        for (int i = 0; i < 4; i++) {
            float a = L + 1.8f + i * ((R - L - 3.6f) / 4.0f);
            float b = a + ((R - L - 3.6f) / 4.0f);
            glVertex2f(a, D - 0.9f); glVertex2f(b, D - 2.6f);
            glVertex2f(b, D - 0.9f); glVertex2f(a, D - 2.6f);
        }
    glEnd();
    const int BOARDS = 26;
    for (int i = 0; i < BOARDS; i++) {
        float t0 = (float)i / BOARDS, t1 = (float)(i + 1) / BOARDS;
        float bx0 = L + (R - L) * t0;
        float bx1 = L + (R - L) * t1 - 0.10f;
        float h = sinf(i * 21.7f) * 43758.5453f;
        float j = h - floorf(h);
        unsigned char base = (unsigned char)(148 + j * 34);
        TintCol3(base, (unsigned char)(base * 0.82f), (unsigned char)(base * 0.62f));
        glBegin(GL_QUADS);
            glVertex2f(bx0, D);         glVertex2f(bx1, D);
            glVertex2f(bx1, D + 0.80f); glVertex2f(bx0, D + 0.80f);
        glEnd();
        TintCol4(96, 74, 50, 110);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            glVertex2f(bx0 + 0.06f, D + 0.28f); glVertex2f(bx1 - 0.06f, D + 0.34f);
        glEnd();
    }
    TintCol3(186, 156, 112);
    glBegin(GL_QUADS);
        glVertex2f(L, D + 0.80f); glVertex2f(R, D + 0.80f);
        glVertex2f(R, D + 1.02f); glVertex2f(L, D + 1.02f);
    glEnd();
    for (int i = 0; i < 2; i++) {
        float bx = R - 1.2f - i * 3.4f;
        TintCol3(72, 54, 38);
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.34f, D + 1.02f); glVertex2f(bx + 0.34f, D + 1.02f);
            glVertex2f(bx + 0.28f, D + 2.30f); glVertex2f(bx - 0.28f, D + 2.30f);
        glEnd();
        FilledCircle3(bx, D + 2.38f, 0.40f, 92, 70, 50, 255);
    }
    TintCol4(196, 178, 132, 230);
    glLineWidth(1.6f);
    for (int i = 0; i < 3; i++) {
        glBegin(GL_LINE_LOOP);
            for (int k = 0; k < 14; k++) {
                float a = (float)k / 14.0f * 2.0f * PI3;
                glVertex2f(R - 4.6f + (0.34f + i * 0.16f) * cosf(a),
                           D + 1.20f + (0.13f + i * 0.06f) * sinf(a));
            }
        glEnd();
    }
    float lx = L + 2.6f;
    TintCol3(72, 54, 38);
    glLineWidth(2.6f);
    glBegin(GL_LINES); glVertex2f(lx, D + 1.02f); glVertex2f(lx, D + 3.20f); glEnd();
    TintCol3(226, 72, 48);
    glLineWidth(3.4f);
    glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 16; k++) {
            float a = (float)k / 16.0f * 2.0f * PI3;
            glVertex2f(lx + 0.62f * cosf(a), D + 3.55f + 0.62f * sinf(a));
        }
    glEnd();
    glLineWidth(1.0f);
}
// Empty kayak securely tied to jetty mooring cleats bobbing on gentle wakes
void DrawMooredKayak3()
{
    const float mx = JETTY_RIGHT_X3 + 4.2f;
    const float my = JETTY_DECK_Y3 - 1.55f + sinf(waterClock3 * 0.42f + 1.1f) * 0.16f;
    glPushMatrix();
    glTranslatef(mx, my, 0.0f);
    glRotatef(sinf(waterClock3 * 0.33f) * 1.8f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-mx, -my, 0.0f);
    DrawKayakBody3(mx, my, -1.0f, 0.0f, 74, 176, 206, false);
    glPopMatrix();
    float bollardX = JETTY_RIGHT_X3 - 1.2f;
    float bollardY = JETTY_DECK_Y3 + 2.30f;
    float bowX = mx - 3.4f, bowY = my + 0.80f;
    TintCol4(206, 188, 142, 235);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 8; i++) {
            float t = (float)i / 8.0f;
            float lx = bowX + (bollardX - bowX) * t;
            float ly = bowY + (bollardY - bowY) * t - 0.55f * t * (1.0f - t);
            glVertex2f(lx, ly);
        }
    glEnd();
    TintCol3(60, 56, 52);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(bollardX - 6.6f, JETTY_DECK_Y3 + 0.42f);
        glVertex2f(bollardX - 2.6f, JETTY_DECK_Y3 + 0.42f);
    glEnd();
    TintCol3(248, 250, 252);
    glBegin(GL_QUADS);
        glVertex2f(bollardX - 7.3f, JETTY_DECK_Y3 + 0.26f);
        glVertex2f(bollardX - 6.5f, JETTY_DECK_Y3 + 0.30f);
        glVertex2f(bollardX - 6.5f, JETTY_DECK_Y3 + 0.56f);
        glVertex2f(bollardX - 7.3f, JETTY_DECK_Y3 + 0.60f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(bollardX - 1.9f, JETTY_DECK_Y3 + 0.26f);
        glVertex2f(bollardX - 2.7f, JETTY_DECK_Y3 + 0.30f);
        glVertex2f(bollardX - 2.7f, JETTY_DECK_Y3 + 0.56f);
        glVertex2f(bollardX - 1.9f, JETTY_DECK_Y3 + 0.60f);
    glEnd();
    glLineWidth(1.0f);
}
// Wooden jetty extending into the river
void DrawJetty3()
{
    unsigned char wr, wg, wb;
    RiverSurfaceColour3(wr, wg, wb);
    float wobble = sinf(rippleScroll3 * 0.12f) * 0.40f;
    BeginReflection(JETTY_DECK_Y3 - 5.6f, 0.44f, wobble);
    DrawJettyStructure3();
    EndReflection();
    WashReflection((JETTY_LEFT_X3 + JETTY_RIGHT_X3) * 0.5f + wobble,
                   (JETTY_RIGHT_X3 - JETTY_LEFT_X3) * 0.5f + 2.0f,
                   JETTY_DECK_Y3 - 5.6f, 3.0f, wr, wg, wb, 140, rippleScroll3);
    DrawJettyStructure3();
    DrawMooredKayak3();
}
constexpr int NUM_REEDS3 = 58;
float reedX3[NUM_REEDS3], reedY3[NUM_REEDS3], reedH3[NUM_REEDS3], reedPhase3[NUM_REEDS3];
bool  reedCattail3[NUM_REEDS3];
// Sets up riverside cattail and reed marsh vegetation arrays
void InitReeds3()
{
    for (int i = 0; i < NUM_REEDS3; i++) {
        reedX3[i]       = -64.0f + (rand() % 12800) / 100.0f;
        reedY3[i]       = -40.0f + (rand() % 900) / 100.0f;
        reedH3[i]       = 5.0f + (rand() % 700) / 100.0f;
        reedPhase3[i]   = (rand() % 628) / 100.0f;
        reedCattail3[i] = (rand() % 100) < 34;
    }
}
// Riverbank marsh reeds and bulrushes swaying at the water's edge
void DrawReeds3()
{
    for (int i = 0; i < NUM_REEDS3; i++) {
        float bx = reedX3[i], by = reedY3[i], h = reedH3[i];
        float bend = sinf(windPhase3 * 1.4f + reedPhase3[i]) * 0.9f * windIntensity3;
        TintCol3(52, 96, 58);
        glLineWidth(2.6f);
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 6; k++) {
                float t = (float)k / 6.0f;
                glVertex2f(bx + bend * t * t, by + h * t);
            }
        glEnd();
        TintCol3(62, 112, 64);
        glLineWidth(1.8f);
        glBegin(GL_LINE_STRIP);
            glVertex2f(bx + bend * 0.16f, by + h * 0.40f);
            glVertex2f(bx + bend * 0.5f + 1.5f, by + h * 0.62f);
            glVertex2f(bx + bend * 0.7f + 2.4f, by + h * 0.55f);
        glEnd();
        if (reedCattail3[i]) {
            float tx = bx + bend, ty = by + h;
            TintCol3(104, 66, 38);
            glLineWidth(4.2f);
            glBegin(GL_LINES);
                glVertex2f(tx, ty - 1.5f); glVertex2f(tx, ty - 0.1f);
            glEnd();
        }
    }
    glLineWidth(1.0f);
}
constexpr int NUM_LAMPS3 = 5;
const float lampX3[NUM_LAMPS3] = { -41.0f, -22.0f, 0.0f, 34.0f, 50.0f };
// Victorian cast-iron ornamental park lamppost with glowing glass lantern
void DrawParkLamp3(float x)
{
    const float baseY = PATH_TOP_Y3 + 0.2f;
    const float topY  = baseY + 6.4f;
    if (LampsOn3()) {
        tintOn3 = false;
        DrawSoftEllipse(x, baseY - 0.2f, 6.2f, 1.5f, 255, 206, 130, 78, 4);
        tintOn3 = true;
    }
    TintCol3(52, 54, 58);
    glLineWidth(2.4f);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    glBegin(GL_QUADS);
        glVertex2f(x-0.42f, baseY);        glVertex2f(x+0.42f, baseY);
        glVertex2f(x+0.30f, baseY+0.75f);  glVertex2f(x-0.30f, baseY+0.75f);
    glEnd();
    glLineWidth(1.6f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x, topY - 0.9f);
        glVertex2f(x + 0.9f, topY - 0.55f);
        glVertex2f(x + 1.25f, topY - 0.05f);
    glEnd();
    float lx = x + 1.25f, ly = topY - 0.55f;
    if (LampsOn3()) {
        tintOn3 = false;
        FilledCircle3(lx, ly, 2.3f, 255, 200, 120, 26);
        FilledCircle3(lx, ly, 1.1f, 255, 214, 140, 60);
        FilledCircle3(lx, ly, 0.46f, 255, 244, 206, 255);
        tintOn3 = true;
    } else {
        FilledCircle3(lx, ly, 0.46f, 190, 196, 200, 255);
    }
    TintCol3(52, 54, 58);
    glBegin(GL_TRIANGLES);
        glVertex2f(lx-0.55f, ly+0.35f); glVertex2f(lx+0.55f, ly+0.35f); glVertex2f(lx, ly+0.95f);
    glEnd();
    glLineWidth(1.0f);
}
// Renders row of park lampposts illuminating paths as daylight fades
void DrawParkLamps3()
{
    for (int i = 0; i < NUM_LAMPS3; i++) DrawParkLamp3(lampX3[i]);
}
// Soft atmospheric distance haze tinted by current time-of-day sunlight angle
void DrawHaze3()
{
    unsigned char r, g, b;
    if      (dayPhase3 == 0) { r = 255; g = 235; b = 210; }
    else if (dayPhase3 == 1) { r = 200; g = 230; b = 250; }
    else if (dayPhase3 == 2) { r = 255; g = 220; b = 150; }
    else                     { r = 226; g = 116; b =  86; }
    const unsigned char peak = IsDusk3() ? 58 : 44;
    tintOn3 = false;
    glBegin(GL_QUADS);
        glColor4ub(r, g, b, 0);    glVertex2f(-60.0f, 16.0f);
        glColor4ub(r, g, b, 0);    glVertex2f( 60.0f, 16.0f);
        glColor4ub(r, g, b, peak); glVertex2f( 60.0f, -6.0f);
        glColor4ub(r, g, b, peak); glVertex2f(-60.0f, -6.0f);
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(r, g, b, peak); glVertex2f(-60.0f, -6.0f);
        glColor4ub(r, g, b, peak); glVertex2f( 60.0f, -6.0f);
        glColor4ub(r, g, b, 0);    glVertex2f( 60.0f, PATH_TOP_Y3);
        glColor4ub(r, g, b, 0);    glVertex2f(-60.0f, PATH_TOP_Y3);
    glEnd();
    tintOn3 = true;
}
// Mobile ice cream / pretzel bicycle pushcart with colorful sun canopy
void DrawVendorCart3()
{
    float x = -25.0f, y = -11.5f;
    TintCol3(230, 230, 235);
    glBegin(GL_QUADS);
        glVertex2f(x-1.8f, y);      glVertex2f(x+1.8f, y);
        glVertex2f(x+1.8f, y+1.6f); glVertex2f(x-1.8f, y+1.6f);
    glEnd();
    TintCol3(220, 60, 60);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-2.2f, y+1.6f); glVertex2f(x+2.2f, y+1.6f); glVertex2f(x, y+2.6f);
    glEnd();
    FilledCircle3(x-1.2f, y-0.15f, 0.35f, 40, 40, 40, 255);
    FilledCircle3(x+1.2f, y-0.15f, 0.35f, 40, 40, 40, 255);
    for (int i = 0; i < 2; i++) {
        float qx = x - 3.0f - i * 1.2f;
        FilledCircle3(qx, y+1.0f, 0.3f, 210, 180, 150, 255);
        TintCol3(70, 90, 120);
        glLineWidth(3.0f);
        glBegin(GL_LINES); glVertex2f(qx, y+0.7f); glVertex2f(qx, y-0.4f); glEnd();
    }
}
// Plein-air landscape artist with wooden easel and palette painting the river view
void DrawPainter3()
{
    float x = 24.0f, y = -13.5f;
    TintCol3(120, 90, 60);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-0.6f, y-0.8f); glVertex2f(x+0.1f, y+1.2f);
        glVertex2f(x+0.6f, y-0.8f); glVertex2f(x+0.1f, y+1.2f);
        glVertex2f(x-0.3f, y-0.8f); glVertex2f(x+0.4f, y-0.8f);
    glEnd();
    TintCol3(250, 248, 240);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, y+0.2f); glVertex2f(x+0.5f, y+0.2f);
        glVertex2f(x+0.5f, y+1.3f); glVertex2f(x-0.5f, y+1.3f);
    glEnd();
    FilledCircle3(x-1.3f, y+1.3f, 0.3f, 225, 185, 145, 255);
    TintCol3(90, 70, 130);
    glLineWidth(4.0f);
    glBegin(GL_LINES); glVertex2f(x-1.3f, y+1.0f); glVertex2f(x-1.3f, y+0.1f); glEnd();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.3f, y+0.1f); glVertex2f(x-1.5f, y-0.7f);
        glVertex2f(x-1.3f, y+0.1f); glVertex2f(x-1.1f, y-0.7f);
    glEnd();
    TintCol3(225, 185, 145);
    glLineWidth(2.0f);
    glBegin(GL_LINES); glVertex2f(x-1.3f, y+0.9f); glVertex2f(x-0.4f, y+0.7f); glEnd();
}
// Solitary fisherman seated on jetty stool holding a long fishing rod over water
void DrawFisherman3()
{
    float x = -2.0f, y = -19.6f;
    FilledCircle3(x, y+1.3f, 0.3f, 225, 185, 145, 255);
    TintCol3(80, 100, 70);
    glLineWidth(4.0f);
    glBegin(GL_LINES); glVertex2f(x, y+1.0f); glVertex2f(x, y+0.1f); glEnd();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x, y+0.1f); glVertex2f(x-0.2f, y-0.6f);
        glVertex2f(x, y+0.1f); glVertex2f(x+0.2f, y-0.6f);
    glEnd();
    TintCol3(90, 60, 35);
    glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(x+0.2f, y+0.9f); glVertex2f(x+2.2f, y+0.3f); glEnd();
    float lineWave = 0.1f * sinf(squirrelPhase3 * 2.0f);
    float bobY     = 0.10f * sinf(squirrelPhase3 * 3.2f);
    float biteCycle = fmodf(squirrelPhase3 * 0.16f, 1.0f);
    bool  biting    = (biteCycle > 0.86f);
    float dip       = biting ? -0.55f * sinf((biteCycle - 0.86f) / 0.14f * PI3) : 0.0f;
    float bx = x + 2.6f + lineWave;
    float by = y - 0.95f + bobY + dip;
    TintCol4(220, 220, 230, 180);
    glBegin(GL_LINES); glVertex2f(x+2.2f, y+0.3f); glVertex2f(bx, by + 0.05f); glEnd();
    FilledCircle3(bx, by, 0.12f, 255, 80, 60, 255);
    if (biting) {
        float t = (biteCycle - 0.86f) / 0.14f;
        TintCol4(235, 250, 255, (unsigned char)(160 * (1.0f - t)));
        glLineWidth(1.2f);
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 14; i++) {
                float a = (float)i/14*2*PI3;
                glVertex2f(bx + (0.3f + t*1.1f)*cos(a), (y - 0.95f) + (0.1f + t*0.35f)*sin(a));
            }
        glEnd();
    }
    glLineWidth(1.0f);
}
// Animated park pedestrian with seasonal clothing, swaying arms, and walking stride
void DrawPerson3(const Ped3& p, float t)
{
    float sc = DepthScaleRange(p.y, PATH_TOP_Y3, PATH_BOTTOM_Y3, 0.86f, 1.08f);
    DrawFigureShadow3(p.x, p.y, 0.55f * sc);
    BeginDepthSprite(p.x, p.y, sc);
    float bob = sinf(t*3.0f + p.phase) * 0.12f;
    float hipX = p.x, hipY = p.y + 1.0f + bob;
    float legSwing = (p.kind == 1) ? 0.45f : 0.25f;
    if (p.kind != 3) {
        TintCol3(40, 40, 50);
        glLineWidth(2.5f);
        glBegin(GL_LINES);
            glVertex2f(hipX, hipY); glVertex2f(hipX + legSwing*sinf(t*4.0f+p.phase), p.y);
            glVertex2f(hipX, hipY); glVertex2f(hipX - legSwing*sinf(t*4.0f+p.phase), p.y);
        glEnd();
        TintCol3(p.shirtR, p.shirtG, p.shirtB);
        glLineWidth(4.0f);
        glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(hipX, hipY+1.1f); glEnd();
        FilledCircle3(hipX, hipY+1.4f, 0.32f, 225, 185, 145, 255);
    }
    if (p.kind == 3) {
        float wheelR = 0.62f;
        float axleY  = p.y + wheelR;
        float crank  = p.x * 2.4f;
        TintCol3(30, 30, 36);
        glLineWidth(1.8f);
        for (int w = -1; w <= 1; w += 2) {
            float wx = hipX + w * 1.05f * p.dir;
            glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 14; i++) {
                    float a = (float)i / 14.0f * 2.0f * PI3;
                    glVertex2f(wx + wheelR*cosf(a), axleY + wheelR*sinf(a));
                }
            glEnd();
            glBegin(GL_LINES);
                for (int i = 0; i < 4; i++) {
                    float a = crank * 0.3f + i * (PI3 / 4.0f);
                    glVertex2f(wx - wheelR*cosf(a), axleY - wheelR*sinf(a));
                    glVertex2f(wx + wheelR*cosf(a), axleY + wheelR*sinf(a));
                }
            glEnd();
        }
        TintCol3(200, 80, 60);
        glLineWidth(2.2f);
        glBegin(GL_LINE_STRIP);
            glVertex2f(hipX - 1.05f*p.dir, axleY);
            glVertex2f(hipX - 0.05f*p.dir, axleY + 0.05f);
            glVertex2f(hipX + 0.25f*p.dir, axleY + 0.95f);
            glVertex2f(hipX + 1.05f*p.dir, axleY);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(hipX - 0.05f*p.dir, axleY + 0.05f);
            glVertex2f(hipX - 0.55f*p.dir, axleY + 0.95f);
            glVertex2f(hipX + 0.25f*p.dir, axleY + 0.95f);
            glVertex2f(hipX + 0.95f*p.dir, axleY + 1.15f);
        glEnd();
        TintCol3(p.shirtR, p.shirtG, p.shirtB);
        glLineWidth(4.0f);
        glBegin(GL_LINES);
            glVertex2f(hipX - 0.5f*p.dir, axleY + 1.05f);
            glVertex2f(hipX + 0.55f*p.dir, axleY + 2.00f);
        glEnd();
        TintCol3(40, 40, 50);
        glLineWidth(2.2f);
        glBegin(GL_LINES);
            glVertex2f(hipX - 0.5f*p.dir, axleY + 1.05f);
            glVertex2f(hipX - 0.05f*p.dir + 0.30f*sinf(crank), axleY + 0.05f + 0.30f*cosf(crank));
            glVertex2f(hipX - 0.5f*p.dir, axleY + 1.05f);
            glVertex2f(hipX - 0.05f*p.dir - 0.30f*sinf(crank), axleY + 0.05f - 0.30f*cosf(crank));
        glEnd();
        FilledCircle3(hipX + 0.75f*p.dir, axleY + 2.25f, 0.32f, 225, 185, 145, 255);
        TintCol3(230, 220, 60);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(hipX + 0.75f*p.dir, axleY + 2.30f);
            for (int i = 0; i <= 8; i++) {
                float a = PI3 * ((float)i / 8.0f);
                glVertex2f(hipX + 0.75f*p.dir + 0.40f*cosf(a), axleY + 2.30f + 0.36f*sinf(a));
            }
        glEnd();
        glLineWidth(1.0f);
        EndDepthSprite();
        return;
    }
    if (p.kind == 4) {
        float cx = p.x + p.dir * 2.3f;
        float cbob = sinf(t*4.4f + p.phase + 1.0f) * 0.10f;
        float chipY = p.y + 0.62f + cbob;
        DrawFigureShadow3(cx, p.y, 0.34f);
        TintCol3(40, 40, 50);
        glLineWidth(1.8f);
        glBegin(GL_LINES);
            glVertex2f(cx, chipY); glVertex2f(cx + 0.36f*sinf(t*5.5f), p.y);
            glVertex2f(cx, chipY); glVertex2f(cx - 0.36f*sinf(t*5.5f), p.y);
        glEnd();
        TintCol3(240, 120, 150);
        glLineWidth(3.0f);
        glBegin(GL_LINES); glVertex2f(cx, chipY); glVertex2f(cx, chipY+0.72f); glEnd();
        glLineWidth(1.8f);
        glBegin(GL_LINES);
            glVertex2f(cx, chipY+0.60f); glVertex2f(cx + 0.45f, chipY + 1.05f);
            glVertex2f(cx, chipY+0.60f); glVertex2f(cx - 0.45f, chipY + 1.00f);
        glEnd();
        FilledCircle3(cx, chipY+0.95f, 0.24f, 235, 198, 160, 255);
        glLineWidth(1.0f);
    }
    if (p.kind == 0) {
        float sx = p.x + p.dir * 1.15f;
        float sy = p.y;
        TintCol3(70, 70, 80);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(sx - p.dir*0.55f, sy + 1.15f);
            glVertex2f(hipX + p.dir*0.15f, hipY + 0.75f);
        glEnd();
        TintCol3(60, 90, 150);
        glBegin(GL_QUADS);
            glVertex2f(sx - 0.55f, sy + 0.42f);
            glVertex2f(sx + 0.55f, sy + 0.42f);
            glVertex2f(sx + 0.48f, sy + 1.02f);
            glVertex2f(sx - 0.48f, sy + 1.02f);
        glEnd();
        TintCol3(40, 65, 115);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(sx + p.dir*0.05f, sy + 1.00f);
            for (int i = 0; i <= 8; i++) {
                float a = PI3 * ((float)i / 8.0f);
                glVertex2f(sx + p.dir*0.05f + p.dir * 0.62f * cosf(a),
                           sy + 1.00f + 0.50f * sinf(a));
            }
        glEnd();
        FilledCircle3(sx + p.dir*0.30f, sy + 1.05f, 0.17f, 242, 205, 170, 255);
        FilledCircle3(sx - p.dir*0.42f, sy + 0.20f, 0.22f, 35, 35, 40, 255);
        FilledCircle3(sx + p.dir*0.42f, sy + 0.14f, 0.16f, 35, 35, 40, 255);
        glLineWidth(1.0f);
    }
    if (p.kind == 1) {
        TintCol3(p.shirtR, p.shirtG, p.shirtB);
        glLineWidth(3.0f);
        glBegin(GL_LINES);
            glVertex2f(hipX, hipY+1.0f);
            glVertex2f(hipX + 0.4f*p.dir, hipY + 0.6f + 0.2f*sinf(t*4.0f));
        glEnd();
    }
    if (p.kind == 2) {
        float dx = hipX - p.dir * 1.3f;
        float dy = p.y;
        float legPh = sinf(t*5.0f + p.phase);
        TintCol3(150, 110, 70);
        glBegin(GL_LINES); glVertex2f(dx-0.5f, dy+0.3f); glVertex2f(dx+0.5f, dy+0.3f); glEnd();
        FilledCircle3(dx - 0.5f*p.dir, dy+0.35f, 0.18f, 150, 110, 70, 255);
        glBegin(GL_LINES);
            glVertex2f(dx-0.2f, dy+0.3f); glVertex2f(dx-0.2f+0.15f*legPh, dy);
            glVertex2f(dx+0.2f, dy+0.3f); glVertex2f(dx+0.2f-0.15f*legPh, dy);
        glEnd();
        TintCol3(120, 120, 120);
        glBegin(GL_LINES); glVertex2f(hipX, hipY+0.7f); glVertex2f(dx, dy+0.35f); glEnd();
    }
    EndDepthSprite();
}
// Renders all park visitors walking, running, and socializing along paths
void DrawPedestrians3()
{
    int order[NUM_PEDS3];
    for (int i = 0; i < NUM_PEDS3; i++) order[i] = i;
    for (int i = 1; i < NUM_PEDS3; i++) {
        int key = order[i];
        int j = i - 1;
        while (j >= 0 && peds3[order[j]].y < peds3[key].y) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_PEDS3; i++) DrawPerson3(peds3[order[i]], pedWalkTimer3);
}
// Timer callback: advances park pedestrians along pathways with obstacle and edge turning
void UpdatePedestrians3(int)
{
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdatePedestrians3, 0); return; }
    if (isAnimating3) {
        pedWalkTimer3 += 0.12f;
        for (int i = 0; i < NUM_PEDS3; i++) {
            peds3[i].x += peds3[i].speed * peds3[i].dir;
            if (peds3[i].dir > 0 && peds3[i].x > 65.0f)  peds3[i].x = -65.0f;
            if (peds3[i].dir < 0 && peds3[i].x < -65.0f) peds3[i].x = 65.0f;
        }
    }
    glutTimerFunc(30, UpdatePedestrians3, 0);
}
const char* kTitle = "Lakeside Park View";
// Initializes scenario state, resets animation timers, and pre-allocates particle buffers
void Init()
{
    glutTimerFunc(0, UpdateWind3, 0);
    glutTimerFunc(0, UpdateClouds3, 0);
    glutTimerFunc(0, UpdateBirds3, 0);
    glutTimerFunc(0, UpdateJet3, 0);
    glutTimerFunc(0, UpdateRiver3, 0);
    glutTimerFunc(0, UpdateKayak3, 0);
    glutTimerFunc(0, UpdateFerrisWheel3, 0);
    glutTimerFunc(0, UpdateDucks3, 0);
    glutTimerFunc(0, UpdateSwing3, 0);
    glutTimerFunc(0, UpdateSeesaw3, 0);
    glutTimerFunc(0, UpdateKite3, 0);
    glutTimerFunc(0, UpdatePedestrians3, 0);
    glutTimerFunc(0, UpdateBalloon3, 0);
    glutTimerFunc(0, UpdateFountain3, 0);
    glutTimerFunc(0, UpdateKayak2_3, 0);
    glutTimerFunc(0, UpdateGooseFamily3, 0);
    glutTimerFunc(0, UpdateDuckFamily3, 0);
    glutTimerFunc(0, UpdateFishJumps3, 0);
    glutTimerFunc(0, UpdateFrisbee3, 0);
    glutTimerFunc(0, UpdatePetals3, 0);
    glutTimerFunc(0, UpdateFireflies3, 0);
    InitPetals3();
    InitFireflies3();
    InitReeds3();
}
// Master rendering routine: clears buffers, sets projection, and draws complete scenario composition
void Draw()
{
    glClearColor(0.5f, 0.75f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    DrawSky3();
    DrawSun3();
    DrawJet3();
    DrawClouds3();
    DrawBirds3();
    if (balloonY3 > hillCrestY3 + 2.0f) DrawBalloon3();
    DrawDistantHills3();
    DrawHills3();
    DrawGrass3();
    DrawFarFillers3();
    DrawFerrisWheel3();
    DrawFairground3();
    DrawRefreshmentStall3();
    DrawFlagpole3();
    DrawTrees3();
    DrawPetals3(0);
    DrawBushes3();
    DrawFlowerBedsFar3();
    DrawBenchesFar3();
    DrawParkLamps3();
    DrawHaze3();
    if (balloonY3 <= hillCrestY3 + 2.0f && balloonScale3 < 1.05f) DrawBalloon3();
    DrawPath3();
    DrawVendorCart3();
    DrawPedestrians3();
    DrawFlowerBedsNear3();
    DrawNearFillersLeft3();
    DrawGardener3();
    DrawButterflies3();
    DrawGooseFamily3();
    DrawSquirrel3();
    DrawBenchesNear3();
    DrawFountain3();
    DrawGazebo3();
    DrawBandstand3();
    DrawSkipping3();
    DrawSwing3();
    DrawSeesaw3();
    DrawSlide3();
    DrawPicnic3(-33.0f);
    DrawFrisbeeScene3();
    DrawBallGame3();
    DrawPainter3();
    DrawCafe3();
    DrawNearFillersRight3();
    DrawKiteString3();
    DrawKite3();
    DrawKiteHolder3();
    if (balloonScale3 >= 1.05f) DrawBalloon3();
    DrawRiver3();
    DrawPetals3(2);
    DrawFisherman3();
    DrawWithReflection3(DrawDock3, 20.0f, 7.0f, -21.0f, 3.4f);
    DrawDuckFamily3();
    DrawDucks3();
    DrawFishJumps3();
    DrawWithReflection3(DrawKayak2_3,  kayak2X3,   3.9f, -25.5f, 2.4f);
    DrawWithReflection3(DrawKayak3,    kayakX3,    3.9f, -26.0f, 2.4f);
    DrawJetty3();
    DrawReeds3();
    DrawFireflies3();
    DrawPetals3(1);
    static const char* const hud[] = {
        "1 morning   2 midday   3 golden hour   4 dusk",
        "W  wind calm/breezy/gusty    A  blossom/autumn colour",
        "X  bring the balloon in close, jump a fish",
        "SPACE pause    H help    ESC quit",
        nullptr
    };
    DrawSceneHUD(kTitle, hud);
}
// Scenario keyboard handler: dispatches scenario-specific hotkeys and feature toggles
void Keyboard(unsigned char key, int , int )
{
    switch (key) {
        case '1': dayPhase3 = 0; break;
        case '2': dayPhase3 = 1; break;
        case '3': dayPhase3 = 2; break;
        case '4': dayPhase3 = 3; break;
        case 'w': case 'W':
            if      (windIntensity3 < 1.5f) windIntensity3 = 2.5f;
            else if (windIntensity3 < 3.2f) windIntensity3 = 4.0f;
            else                            windIntensity3 = 1.0f;
            break;
        case 'a': case 'A': autumnMode3 = !autumnMode3; break;
        case 'x': case 'X':
            fishCooldown3 = 1;
            balloonX3 = -20.0f;
            balloonScale3 = 1.02f;
            break;
    }
    glutPostRedisplay();
}
// Scenario mouse handler: toggles scene animation play/pause state
void Mouse(int button, int state, int , int )
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)  isAnimating3 = true;
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) isAnimating3 = false;
    glutPostRedisplay();
}
}

// ============================================================================
//  SCENARIO 4 -- Winter Night Market
//  Snowy plaza, Ferris wheel, ice rink, river, street traffic, seasons
// ============================================================================
namespace Scenario4 {
constexpr float PI4 = 3.1416f;
bool  isAnimating4      = true;
int   snowIntensity4    = 1;
bool  multicolorLights4 = false;
float dayT4      = 0.0f;
float dayTarget4 = 0.0f;
inline float MixF4(float night, float day)
{
    return night + (day - night) * dayT4;
}
inline unsigned char MixB4(int night, int day)
{
    float v = night + (day - night) * dayT4;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (unsigned char)(v + 0.5f);
}
inline unsigned char DayGlow4(int night, int day, float strength = 1.0f)
{
    return (unsigned char)((night + (day - night) * dayT4) * strength + 0.5f);
}
inline void ColourDN4(int nr, int ng, int nb, int dr, int dg, int db)
{
    glColor3ub(MixB4(nr, dr), MixB4(ng, dg), MixB4(nb, db));
}
inline void ColourDN4A(int nr, int ng, int nb, int na,
                       int dr, int dg, int db, int da)
{
    glColor4ub(MixB4(nr, dr), MixB4(ng, dg), MixB4(nb, db), MixB4(na, da));
}
inline float NightT4()
{
    return 1.0f - dayT4;
}
inline unsigned char NightA4(int nightAlpha)
{
    return (unsigned char)(nightAlpha * NightT4());
}
inline unsigned char LampB4(int lit, int unlit)
{
    return (unsigned char)(unlit + (lit - unlit) * NightT4());
}
float seasonT4      = 0.0f;
float seasonTarget4 = 0.0f;
inline float MixSF4(float winter, float autumnV)
{
    return winter + (autumnV - winter) * seasonT4;
}
inline unsigned char MixSB4(int winter, int autumnV)
{
    float v = winter + (autumnV - winter) * seasonT4;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (unsigned char)(v + 0.5f);
}
inline void ColourSeason4(int wr, int wg, int wb, int ar, int ag, int ab)
{
    glColor3ub(MixSB4(wr, ar), MixSB4(wg, ag), MixSB4(wb, ab));
}
inline float SeasonWinter4()
{
    return 1.0f - seasonT4;
}
inline float SeasonAutumn4()
{
    return seasonT4;
}
inline unsigned char WinterA4(int alpha)
{
    return (unsigned char)(alpha * SeasonWinter4());
}
inline unsigned char AutumnA4(int alpha)
{
    return (unsigned char)(alpha * SeasonAutumn4());
}
float snowCover4 = 0.55f;
inline float SnowDepth4()
{
    return snowCover4 * SeasonWinter4();
}
float twinklePhase4     = 0.0f;
float pedTimer4         = 0.0f;
constexpr int NUM_STARS4 = 190;
struct Star4
{
    float x, y, twinkle, mag, rate; int bucket; unsigned char r, g, b;
};
Star4 stars4[NUM_STARS4];
constexpr int NUM_BRIGHT_STARS4 = 8;
struct BrightStar4
{
    float x, y, r, phase;
};
BrightStar4 brightStars4[NUM_BRIGHT_STARS4];
constexpr int NUM_BIRDS4 = 12;
struct Bird4
{
    float x, y, speed, scale;
    float flapPhase, flapRate;
    float bobPhase,  bobAmp;
    int   dir;
};
Bird4 birds4[NUM_BIRDS4];
struct Shop4
{
    float x, w, h; const char* sign; bool lit[5];
};
constexpr int NUM_SHOPS4 = 7;
Shop4 shops4[NUM_SHOPS4] = {
    { -22.7f, 11.4f, 17.0f, "TOYS",   {} },
    {  -7.2f, 12.6f, 21.0f, "BAKERY", {} },
    {   9.2f, 13.2f, 18.0f, "GIFTS",  {} },
    {  25.2f, 12.0f, 22.0f, "CAFE",   {} },
    {  40.2f, 10.8f, 19.0f, "SKATES", {} },
    {  56.0f, 12.0f, 18.0f, "CIDER",  {} },
    { -55.0f, 12.0f, 17.0f, nullptr,  {} }
};
constexpr int NUM_LIGHT_SPANS4 = 5;
constexpr int BULBS_PER_SPAN4  = 8;
struct LightSpan4
{
    float x1, y1, x2, y2;
};
LightSpan4 lightSpans4[NUM_LIGHT_SPANS4] = {
    { -28.4f, 10.0f, -13.5f, 14.0f },
    { -13.5f, 14.0f,   2.6f, 11.0f },
    {   2.6f, 11.0f,  19.2f, 15.0f },
    {  19.2f, 15.0f,  34.8f, 12.0f },
    {  34.8f, 12.0f,  50.0f, 11.0f }
};
bool bulbLit4[NUM_LIGHT_SPANS4][BULBS_PER_SPAN4];
constexpr float treeX4  =  0.0f;
constexpr float clockX4 = 50.0f;
constexpr int   NUM_STALLS4   = 4;
constexpr float stallScale4   = 1.55f;
constexpr float stallGroundY4 = -12.1f;
inline float StallHalfW4()
{
    return 2.60f * stallScale4;
}
inline float StallCounterTopY4()
{
    return stallGroundY4 + 2.0f * stallScale4;
}
inline float StallApexY4()
{
    return stallGroundY4 + 3.6f * stallScale4;
}

// Market stalls
struct Stall4
{
    float x; unsigned char r, g, b; const char* sign; int goods;
};
Stall4 stalls4[NUM_STALLS4] = {
    { -24.0f, 130,  80, 170, "COCOA",     0 },
    { -15.0f, 200,  60,  60, "WREATHS",   1 },
    {  28.0f,  60, 140,  90, "CHESTNUTS", 2 },
    {  38.0f, 210, 160,  60, "TRINKETS",  3 }
};
float ferrisAngle4 = 0.0f;
constexpr float ferrisCX4    = -52.0f;
constexpr float ferrisR4     =  10.5f;
constexpr float ferrisBaseY4 = -10.0f;
constexpr float ferrisCY4    = ferrisBaseY4 + ferrisR4 + 1.2f;
constexpr float ferrisScale4 = ferrisR4 / 9.0f;
constexpr int   NUM_CABINS4 = 8;
constexpr float rinkCX4 = 10.0f, rinkCY4 = -14.5f;
struct Skater4
{
    float angle, speed, radiusX, radiusY; unsigned char r, g, b;
};
constexpr int NUM_SKATERS4 = 5;
Skater4 skaters4[NUM_SKATERS4] = {
    { 0.0f,  0.013f, 5.1f, 1.95f, 210, 70,  90  },
    { 3.14f, 0.017f, 4.2f, 1.60f,  70, 110, 200 },
    { 1.6f,  0.030f, 2.4f, 0.95f, 240, 200,  60 },
    { 4.4f,  0.019f, 3.6f, 1.38f, 120, 200, 140 },
    { 4.68f, 0.019f, 3.6f, 1.38f, 200, 130, 220 }
};
float skaterFallT4     = 0.0f;
int   skaterFallCool4  = 420;
constexpr int FALLING_SKATER4 = 1;
float sledX4        = -80.0f;
bool  sledActive4    = false;
int   sledCooldown4  = 300;
constexpr float fireX4 = -5.0f, fireY4 = -10.5f;
float firePhase4 = 0.0f;
struct SmokePuff4
{
    float x, y, vy, alpha, size; bool active;
};
constexpr int MAX_SMOKE4 = 30;
SmokePuff4 chimneySmoke4[MAX_SMOKE4];
constexpr int MAX_STEAM4 = 30;
SmokePuff4 stallSteam4[MAX_STEAM4];
constexpr int MAX_SNOW4 = 300;
float snowX4[MAX_SNOW4], snowY4[MAX_SNOW4], snowDrift4[MAX_SNOW4], snowSize4[MAX_SNOW4];

// Market pedestrians
struct Ped4
{
    float x, y, speed, phase; int dir; unsigned char coatR, coatG, coatB;
};
constexpr int NUM_PEDS4 = 7;
Ped4 peds4[NUM_PEDS4] = {
    { -30.0f, -10.4f, 0.05f, 0.0f,  1, 150,  40,  50 },
    {  -5.0f, -13.6f, 0.07f, 1.0f, -1,  40,  60, 120 },
    {  20.0f, -10.8f, 0.04f, 2.0f,  1,  90,  90,  90 },
    {  35.0f, -12.4f, 0.06f, 3.0f, -1, 120,  70,  40 },
    { -45.0f, -10.9f, 0.05f, 4.0f,  1, 200, 170,  60 },
    {  10.0f, -13.9f, 0.055f,5.0f,  1, 180,  60, 130 },
    { -18.0f, -12.9f, 0.065f,6.0f, -1,  60, 130, 110 }
};
float sleighX4          = -100.0f;
float sleighY4          = 27.0f;
float sleighScale4      = 1.05f;
float sleighTrailPhase4 = 0.0f;
float metroCarX4      = -200.0f;
float metroTime4      = 0.0f;
float metroNextStart4 = 5.0f;
bool  metroActive4    = false;
float metroSpeed4     = 1.8f;
float pendulumAngle4 = 0.0f;
enum FireworkState4 { FW_INACTIVE, FW_RISING, FW_BURSTING };

// Firework effects
struct Firework4
{
    float x, y;
    float targetY;
    float phase;
    FireworkState4 state;
    unsigned char r, g, b;
    float sparkAng[18];
    float sparkSpd[18];
};
constexpr int MAX_FIREWORKS4 = 3;
Firework4 fireworks4[MAX_FIREWORKS4];
int fireworkCooldown4 = 200;
inline float SnowCoverTarget4()
{
    if (snowIntensity4 == 0) return 0.20f;
    if (snowIntensity4 == 1) return 0.55f;
    return 0.90f;
}
bool  fogMode4  = false;
float fogPhase4 = 0.0f;
float fogAmount4 = 0.0f;
bool  auroraActive4   = false;
float auroraPhase4    = 0.0f;
float auroraTimer4    = 0.0f;
int   auroraCooldown4 = 600;
// Helper: renders smooth filled circle with RGBA color
void FilledCircle4(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    glColor4ub(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 20;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI4;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}
inline void SkyAirColour4(float y, float& r, float& g, float& b)
{
    float t = (y + 6.0f) / 46.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    r = MixF4(44.0f - 30.0f * t, 216.0f - 150.0f * t);
    g = MixF4(50.0f - 34.0f * t, 230.0f - 100.0f * t);
    b = MixF4(80.0f - 52.0f * t, 244.0f -  30.0f * t);
}
inline void AirFade4(float depth, float atY,
                     int nr, int ng, int nb, int dr, int dg, int db,
                     unsigned char& outR, unsigned char& outG, unsigned char& outB)
{
    float hr, hg, hb;
    SkyAirColour4(atY, hr, hg, hb);
    float br = MixF4((float)nr, (float)dr);
    float bg = MixF4((float)ng, (float)dg);
    float bb = MixF4((float)nb, (float)db);
    float k = depth; if (k < 0.0f) k = 0.0f; if (k > 1.0f) k = 1.0f;
    outR = (unsigned char)(br + (hr - br) * k + 0.5f);
    outG = (unsigned char)(bg + (hg - bg) * k + 0.5f);
    outB = (unsigned char)(bb + (hb - bb) * k + 0.5f);
}
inline void DrawRadialGlow4(float cx, float cy, float rx, float ry,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char centreA)
{
    const int SEG = 48;
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(r, g, b, centreA);
        glVertex2f(cx, cy);
        glColor4ub(r, g, b, 0);
        for (int i = 0; i <= SEG; i++) {
            float a = (float)i / SEG * 6.2831853f;
            glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a));
        }
    glEnd();
}
inline void DrawHazeBank4(float cx, float cy, float rx, float ry,
                          unsigned char r, unsigned char g, unsigned char b,
                          float peakA)
{
    const int COLS = 26;
    for (int half = 0; half < 2; half++) {
        float edgeY = (half == 0) ? cy + ry : cy - ry;
        glBegin(GL_QUAD_STRIP);
        for (int k = 0; k <= COLS; k++) {
            float u    = (float)k / COLS;
            float x    = cx - rx + 2.0f * rx * u;
            float bell = 0.5f - 0.5f * cosf(u * 6.2831853f);
            bell *= bell;
            glColor4ub(r, g, b, (unsigned char)(peakA * bell));
            glVertex2f(x, cy);
            glColor4ub(r, g, b, 0);
            glVertex2f(x, edgeY);
        }
        glEnd();
    }
}

// Winter sky with aurora/clouds based on season and time
void DrawSky4()
{
    float dayBlend = dayT4;
    const int   NSTOP = 6;
    const float stopY [NSTOP]    = { 40.0f, 30.0f, 21.0f, 13.0f, 4.0f, -6.0f };
    const int wNightC[NSTOP][3] = { {4,6,18}, {7,10,27}, {12,17,38},
                                    {19,25,52}, {29,36,68}, {44,50,82} };
    const int aNightC[NSTOP][3] = { {12,8,18}, {20,13,26}, {32,20,34},
                                    {48,29,42}, {68,41,48}, {92,58,54} };
    const int wDayC  [NSTOP][3] = { {56,120,208}, {74,140,220}, {100,164,232},
                                    {136,190,240}, {180,213,246}, {216,231,245} };
    const int aDayC  [NSTOP][3] = { {84,118,168}, {112,140,178}, {148,166,186},
                                    {186,188,188}, {218,204,180}, {236,218,184} };
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i < NSTOP; i++) {
        glColor3ub(MixB4(MixSB4(wNightC[i][0], aNightC[i][0]),
                         MixSB4(wDayC  [i][0], aDayC  [i][0])),
                   MixB4(MixSB4(wNightC[i][1], aNightC[i][1]),
                         MixSB4(wDayC  [i][1], aDayC  [i][1])),
                   MixB4(MixSB4(wNightC[i][2], aNightC[i][2]),
                         MixSB4(wDayC  [i][2], aDayC  [i][2])));
        glVertex2f(-60.0f, stopY[i]);
        glVertex2f( 60.0f, stopY[i]);
    }
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(MixB4(70, 255), MixB4(86, 240), MixB4(126, 214), MixB4(30, 46));
        glVertex2f(60.0f, 40.0f); glVertex2f(60.0f, -6.0f);
        glColor4ub(MixB4(70, 255), MixB4(86, 240), MixB4(126, 214), 0);
        glVertex2f(-6.0f, -6.0f); glVertex2f(-6.0f, 40.0f);
    glEnd();
    for (int i = 0; i < 6; i++) {
        float span  = 260.0f;
        float drift = fmodf(twinklePhase4 * (0.22f + i * 0.09f) + i * 43.0f, span) - span * 0.5f;
        float cy    = 33.0f - i * 5.4f;
        float rx    = 24.0f + i * 7.0f;
        float ry    = 1.6f  + i * 0.7f;
        float a     = MixB4(20, 40) * (1.0f - i * 0.09f);
        unsigned char cr = MixB4(92, 244), cg = MixB4(108, 248), cb = MixB4(148, 252);
        DrawHazeBank4(drift,        cy, rx, ry, cr, cg, cb, a);
        DrawHazeBank4(drift - span, cy, rx, ry, cr, cg, cb, a);
    }
    glBegin(GL_QUADS);
        glColor4ub(MixB4(90, 150), MixB4(60, 190), MixB4(55, 240), (unsigned char)(MixB4(0, 80) * (1.0f - dayBlend * 0.25f)));
        glVertex2f(-60, 10); glVertex2f(60, 10);
        glColor4ub(MixB4(150, 160), MixB4(95, 215), MixB4(60, 220), (unsigned char)(MixB4(120, 90) * (1.0f - 0.65f * dayBlend)));
        glVertex2f(60, -6);  glVertex2f(-60, -6);
    glEnd();
    if (dayBlend > 0.02f) {
        glColor4ub(255, 220, 160, (unsigned char)(40.0f * dayBlend));
        glBegin(GL_QUADS);
            glVertex2f(-60, 4.0f); glVertex2f(60, 4.0f);
            glVertex2f(60, -6.0f); glVertex2f(-60, -6.0f);
        glEnd();
    }
}
constexpr float moonX4 = 34.0f, moonY4 = 27.0f, moonR4 = 3.1f;
// Glowing winter crescent/full moon in cold nocturnal sky with crater details
void DrawMoon4()
{
    float nightFade = NightT4();
    if (dayT4 > 0.05f) {
        float sunA = dayT4 * 255.0f;
        DrawRadialGlow4(moonX4, moonY4, moonR4 * 9.0f, moonR4 * 9.0f,
                        255, 226, 160, (unsigned char)(58.0f * dayT4));
        DrawRadialGlow4(moonX4, moonY4, moonR4 * 3.4f, moonR4 * 3.4f,
                        255, 232, 176, (unsigned char)(96.0f * dayT4));
        DrawRadialGlow4(moonX4, moonY4, moonR4 * 1.7f, moonR4 * 1.7f,
                        255, 238, 194, (unsigned char)(130.0f * dayT4));
        glColor4ub(255, 221, 150, (unsigned char)sunA);
        glBegin(GL_TRIANGLE_FAN);
            for (int j = 0; j <= 24; j++) {
                float ang = (float)j / 24.0f * 6.2831853f;
                glVertex2f(moonX4 + moonR4 * cosf(ang), moonY4 + moonR4 * sinf(ang));
            }
        glEnd();
        return;
    }
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 8.5f, moonR4 * 8.5f,
                    150, 178, 232, (unsigned char)(40 * nightFade));
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 3.6f, moonR4 * 3.6f,
                    196, 216, 248, (unsigned char)(70 * nightFade));
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 1.85f, moonR4 * 1.85f,
                    226, 238, 254, (unsigned char)(105 * nightFade));
    FilledCircle4(moonX4, moonY4, moonR4, 238, 243, 252, (unsigned char)(255 * nightFade));
    FilledCircle4(moonX4 + moonR4 * 0.16f, moonY4 + moonR4 * 0.14f, moonR4 * 0.82f,
                  248, 251, 255, (unsigned char)(190 * nightFade));
    FilledCircle4(moonX4 - 0.9f, moonY4 + 0.7f, 0.62f, 214, 222, 236, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 + 1.0f, moonY4 - 0.5f, 0.45f, 218, 226, 239, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 + 0.2f, moonY4 + 1.5f, 0.30f, 220, 228, 241, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 - 1.3f, moonY4 - 1.2f, 0.26f, 220, 228, 241, (unsigned char)(235 * nightFade));
}
// Clear frosty night star field twinkling over the snowy market square
void DrawStars4()
{
    float night = NightT4();
    if (night <= 0.02f) return;
    const float sizes[3] = { 1.1f, 1.8f, 2.7f };
    for (int pass = 0; pass < 3; pass++) {
        glPointSize(sizes[pass]);
        glBegin(GL_POINTS);
        for (int i = 0; i < NUM_STARS4; i++) {
            const Star4& st = stars4[i];
            if (st.bucket != pass) continue;
            float tw = 0.55f + 0.45f * sinf(twinklePhase4 * st.rate + st.twinkle);
            float hz = (st.y - 4.0f) / 16.0f;
            if (hz < 0.0f) hz = 0.0f;
            if (hz > 1.0f) hz = 1.0f;
            float a = st.mag * tw * night * (0.22f + 0.78f * hz);
            glColor4ub(st.r, st.g, st.b, (unsigned char)(255.0f * a));
            glVertex2f(st.x, st.y);
        }
        glEnd();
    }
    glPointSize(1.0f);
    for (int i = 0; i < NUM_BRIGHT_STARS4; i++) {
        const BrightStar4& b = brightStars4[i];
        float tw = 0.58f + 0.42f * sinf(twinklePhase4 * 1.6f + b.phase);
        float a  = night * tw;
        FilledCircle4(b.x, b.y, b.r * 3.0f, 150, 180, 235, (unsigned char)(20 * a));
        FilledCircle4(b.x, b.y, b.r * 1.4f, 206, 224, 250, (unsigned char)(52 * a));
        FilledCircle4(b.x, b.y, b.r * 0.55f, 255, 255, 255, (unsigned char)(240 * a));
        glColor4ub(240, 246, 255, (unsigned char)(80 * a));
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            glVertex2f(b.x - b.r * 2.6f, b.y); glVertex2f(b.x + b.r * 2.6f, b.y);
            glVertex2f(b.x, b.y - b.r * 2.6f); glVertex2f(b.x, b.y + b.r * 2.6f);
        glEnd();
    }
}
// Reinitializes winter bird flight path across the market sky
void RespawnBird4(Bird4& b, float atX)
{
    b.x         = atX;
    b.y         = 12.0f + (rand() % 210) / 10.0f;
    b.speed     = 0.10f + (rand() % 22) / 100.0f;
    b.scale     = 0.55f + (rand() % 70) / 100.0f;
    b.flapRate  = 0.16f + (rand() % 16) / 100.0f;
    b.flapPhase = (rand() % 628) / 100.0f;
    b.bobPhase  = (rand() % 628) / 100.0f;
    b.bobAmp    = 0.25f + (rand() % 60) / 100.0f;
}
// Winter crows / pigeons flying across the snow-draped rooftops
void DrawBirds4()
{
    unsigned char br = MixB4(112, 48), bg = MixB4(124, 52), bb = MixB4(152, 62);
    unsigned char alpha = MixB4(120, 215);
    glLineWidth(1.4f);
    for (int i = 0; i < NUM_BIRDS4; i++) {
        const Bird4& bd = birds4[i];
        float sp   = bd.scale;
        float y    = bd.y + sinf(bd.bobPhase) * bd.bobAmp;
        float flap = sinf(bd.flapPhase);
        float up   = 0.30f + 0.70f * (0.5f + 0.5f * flap);
        unsigned char a = (unsigned char)(alpha * (0.45f + 0.55f * sp));
        glColor4ub(br, bg, bb, a);
        glBegin(GL_LINE_STRIP);
            glVertex2f(bd.x - 1.20f * sp, y + up * 0.80f * sp);
            glVertex2f(bd.x - 0.50f * sp, y + (up * 0.16f - 0.06f) * sp);
            glVertex2f(bd.x,              y + 0.10f * sp);
            glVertex2f(bd.x + 0.50f * sp, y + (up * 0.16f - 0.06f) * sp);
            glVertex2f(bd.x + 1.20f * sp, y + up * 0.80f * sp);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(bd.x, y + 0.10f * sp);
            glVertex2f(bd.x + 0.34f * sp * bd.dir, y + 0.14f * sp);
        glEnd();
    }
    glLineWidth(1.0f);
}
// Timer callback: animates bird flock flight and wing flaps over winter market
void UpdateBirds4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBirds4, 0); return; }
    if (isAnimating4) {
        for (int i = 0; i < NUM_BIRDS4; i++) {
            Bird4& b = birds4[i];
            b.x         += b.speed * (float)b.dir;
            b.flapPhase += b.flapRate;
            b.bobPhase  += 0.035f;
            if (b.dir > 0 && b.x >  70.0f) RespawnBird4(b, -70.0f);
            if (b.dir < 0 && b.x < -70.0f) RespawnBird4(b,  70.0f);
        }
    }
    glutTimerFunc(45, UpdateBirds4, 0);
}
// Magical Santa sleigh pulled by reindeer soaring high above the night rooftops
void DrawSleigh4()
{
    glPushMatrix();
    glTranslatef(sleighX4, sleighY4, 0.0f);
    glScalef(sleighScale4, sleighScale4, 1.0f);
    glColor3ub(15, 15, 22);
    for (int i = 0; i < 3; i++) {
        float dx = -i * 2.2f;
        glBegin(GL_QUADS);
            glVertex2f(dx-0.9f, 0.3f); glVertex2f(dx+0.9f, 0.3f);
            glVertex2f(dx+0.9f, 0.9f); glVertex2f(dx-0.9f, 0.9f);
        glEnd();
        glLineWidth(1.5f);
        glBegin(GL_LINES);
            glVertex2f(dx-0.6f, 0.3f); glVertex2f(dx-0.7f, -0.3f);
            glVertex2f(dx+0.5f, 0.3f); glVertex2f(dx+0.6f, -0.3f);
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex2f(dx+0.9f, 0.9f); glVertex2f(dx+1.4f, 1.1f); glVertex2f(dx+0.9f, 0.6f);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(dx+1.2f, 1.05f); glVertex2f(dx+1.5f, 1.5f);
            glVertex2f(dx+1.2f, 1.05f); glVertex2f(dx+1.0f, 1.5f);
        glEnd();
    }
    glColor3ub(120, 85, 50);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(1.2f, 0.6f);
        glVertex2f(-1.0f, 0.6f);
        glVertex2f(-3.2f, 0.6f);
        glVertex2f(-5.4f, 0.6f);
        glVertex2f(-6.2f, 0.7f);
    glEnd();
    FilledCircle4(1.55f, 1.12f, 0.55f, 255, 70, 60, 60);
    FilledCircle4(1.55f, 1.12f, 0.28f, 255, 90, 70, 130);
    FilledCircle4(1.55f, 1.12f, 0.15f, 255, 160, 140, 255);
    glColor3ub(15, 15, 22);
    glBegin(GL_POLYGON);
        glVertex2f(-8.0f, 0.3f); glVertex2f(-6.0f, 0.3f);
        glVertex2f(-6.0f, 1.0f); glVertex2f(-7.0f, 1.0f);
        glVertex2f(-8.5f, 0.6f);
    glEnd();
    for (int i = 0; i < 6; i++) {
        float t = fmodf(sleighTrailPhase4 + i*0.3f, 1.0f);
        float tx = -8.0f - t*6.0f;
        float ty = 0.6f + sinf(t*10.0f)*0.3f;
        FilledCircle4(tx, ty, 0.15f*(1.0f-t), 255, 240, 200, (unsigned char)(220*(1.0f-t)));
    }
    glPopMatrix();
}
// Timer callback: propels flying sleigh across the starlit sky
void UpdateSleigh4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSleigh4, 0); return; }
    if (isAnimating4) {
        sleighX4 += 0.25f;
        sleighTrailPhase4 += 0.05f;
        if (sleighX4 > 100.0f) {
            sleighX4 = -100.0f - (rand() % 30);
            sleighY4 = 25.0f + (rand() % 9);
        }
    }
    glutTimerFunc(30, UpdateSleigh4, 0);
}
// Timer callback: advances distant commuter metro train across background viaduct
void UpdateMetroRail4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(60, UpdateMetroRail4, 0); return; }
    if (isAnimating4) metroTime4 += 0.06f;
    if (!metroActive4) {
        if (metroTime4 >= metroNextStart4) {
            metroActive4 = true;
            metroCarX4 = -80.0f;
        }
    } else {
        metroCarX4 += metroSpeed4;
        if (metroCarX4 > 80.0f) {
            metroActive4 = false;
            metroNextStart4 = metroTime4 + 20.0f;
        }
    }
    glutPostRedisplay();
    glutTimerFunc(60, UpdateMetroRail4, 0);
}
constexpr float SHOP_BASE_Y4    = -6.0f;
constexpr float SHOP_EAVE_OVER4 =  0.6f;
inline float ShopEaveY4(const Shop4& s)
{
    return SHOP_BASE_Y4 + s.h;
}
inline float ShopRoofRise4(const Shop4& s)
{
    return 2.1f + s.w * 0.10f;
}
inline float ShopRoofY4(const Shop4& s, float x)
{
    float half = s.w * 0.5f + SHOP_EAVE_OVER4;
    float t    = fabsf(x - s.x) / half;
    if (t > 1.0f) t = 1.0f;
    return ShopEaveY4(s) + ShopRoofRise4(s) * (1.0f - t);
}
// Renders glowing row of cozy paned shop windows with festive window displays
void DrawShopWindowRow4(const Shop4& s, float y, float wh, int shift)
{
    const int   n    = 5;
    const float slot = s.w / (float)(n + 1);
    const float ww   = slot * 0.52f;
    for (int i = 0; i < n; i++) {
        float wx = s.x - s.w*0.5f + slot*(i+1) - ww*0.5f;
        bool  on = s.lit[(i + shift) % 5];
        if (on) {
            glColor4ub(255, 196, 110, (unsigned char)(38.0f * (0.35f + 0.65f * NightT4())));
            glBegin(GL_QUADS);
                glVertex2f(wx-0.7f,    y-0.7f);     glVertex2f(wx+ww+0.7f, y-0.7f);
                glVertex2f(wx+ww+0.7f, y+wh+0.7f);  glVertex2f(wx-0.7f,    y+wh+0.7f);
            glEnd();
            glColor3ub(255, 210, 134);
        } else {
            glColor3ub(MixB4(17, 74), MixB4(15, 88), MixB4(25, 112));
        }
        glBegin(GL_QUADS);
            glVertex2f(wx,    y);      glVertex2f(wx+ww, y);
            glVertex2f(wx+ww, y+wh);   glVertex2f(wx,    y+wh);
        glEnd();
        glColor3ub(MixB4(82, 168), MixB4(70, 156), MixB4(72, 156));
        glLineWidth(1.3f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(wx,    y);      glVertex2f(wx+ww, y);
            glVertex2f(wx+ww, y+wh);   glVertex2f(wx,    y+wh);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(wx+ww*0.5f, y);         glVertex2f(wx+ww*0.5f, y+wh);
            glVertex2f(wx,         y+wh*0.55f); glVertex2f(wx+ww,     y+wh*0.55f);
        glEnd();
        glColor3ub(MixB4(96, 196), MixB4(88, 188), MixB4(96, 190));
        glBegin(GL_QUADS);
            glVertex2f(wx-0.25f, y-0.30f); glVertex2f(wx+ww+0.25f, y-0.30f);
            glVertex2f(wx+ww+0.25f, y);    glVertex2f(wx-0.25f,    y);
        glEnd();
        glLineWidth(1.0f);
    }
}
// Traditional European gabled town merchant house with snow-covered pitched roof
void DrawShop4(const Shop4& s)
{
    const float baseY = SHOP_BASE_Y4;
    const float halfW = s.w * 0.5f;
    const float eaveY = ShopEaveY4(s);
    const float rise  = ShopRoofRise4(s);
    const float over  = SHOP_EAVE_OVER4;
    {
        float dir  = (s.x < 0.0f) ? 1.0f : -1.0f;
        float fx   = s.x + dir * halfW;
        float sideW = fabsf(fx) * 0.030f;
        if (sideW > 2.0f)  sideW = 2.0f;
        if (sideW > 0.22f) {
            float k       = sideW / (fabsf(fx) + 0.001f);
            float backEave = eaveY + (baseY - eaveY) * k;
            bool lit = (dir > 0.0f);
            glBegin(GL_QUADS);
                if (lit) glColor3ub(MixB4(24, 84),  MixB4(21, 78),  MixB4(31, 90));
                else     glColor3ub(MixB4(13, 52),  MixB4(11, 48),  MixB4(18, 58));
                glVertex2f(fx,           baseY);
                glVertex2f(fx + dir*sideW, baseY);
                if (lit) glColor3ub(MixB4(41, 128), MixB4(36, 120), MixB4(52, 134));
                else     glColor3ub(MixB4(22, 78),  MixB4(19, 72),  MixB4(29, 84));
                glVertex2f(fx + dir*sideW, backEave);
                glVertex2f(fx,             eaveY);
            glEnd();
            glColor3ub(MixB4(196, 232), MixB4(206, 238), MixB4(224, 248));
            float cap = 0.30f + 0.55f * SnowDepth4();
            glBegin(GL_QUADS);
                glVertex2f(fx,             eaveY);
                glVertex2f(fx + dir*sideW, backEave);
                glVertex2f(fx + dir*sideW, backEave + cap * 0.7f);
                glVertex2f(fx,             eaveY + cap);
            glEnd();
        }
    }
    glBegin(GL_QUADS);
        glColor3ub(MixB4(29, 118), MixB4(25, 110), MixB4(37, 116));
        glVertex2f(s.x-halfW, baseY);  glVertex2f(s.x+halfW, baseY);
        glColor3ub(MixB4(48, 168), MixB4(42, 160), MixB4(60, 164));
        glVertex2f(s.x+halfW, eaveY);  glVertex2f(s.x-halfW, eaveY);
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(255, MixB4(226, 244), MixB4(190, 226), (unsigned char)(MixB4(30, 54)));
        glVertex2f(s.x+halfW, baseY); glVertex2f(s.x+halfW, eaveY);
        glColor4ub(255, MixB4(226, 244), MixB4(190, 226), 0);
        glVertex2f(s.x+halfW-s.w*0.34f, eaveY); glVertex2f(s.x+halfW-s.w*0.34f, baseY);
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(MixB4(4, 40), MixB4(6, 48), MixB4(14, 70), (unsigned char)(MixB4(120, 86)));
        glVertex2f(s.x-halfW, baseY); glVertex2f(s.x-halfW, eaveY);
        glColor4ub(MixB4(4, 40), MixB4(6, 48), MixB4(14, 70), 0);
        glVertex2f(s.x-halfW+s.w*0.40f, eaveY); glVertex2f(s.x-halfW+s.w*0.40f, baseY);
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(MixB4(4, 38), MixB4(6, 44), MixB4(14, 62), (unsigned char)(MixB4(135, 105)));
        glVertex2f(s.x-halfW, eaveY); glVertex2f(s.x+halfW, eaveY);
        glColor4ub(MixB4(4, 38), MixB4(6, 44), MixB4(14, 62), 0);
        glVertex2f(s.x+halfW, eaveY-2.6f); glVertex2f(s.x-halfW, eaveY-2.6f);
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(MixB4(4, 44), MixB4(6, 50), MixB4(14, 68), (unsigned char)(MixB4(105, 72)));
        glVertex2f(s.x-halfW, baseY); glVertex2f(s.x+halfW, baseY);
        glColor4ub(MixB4(4, 44), MixB4(6, 50), MixB4(14, 68), 0);
        glVertex2f(s.x+halfW, baseY+1.8f); glVertex2f(s.x-halfW, baseY+1.8f);
    glEnd();
    glColor3ub(MixB4(64, 186), MixB4(56, 178), MixB4(74, 182));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW,       baseY); glVertex2f(s.x-halfW+0.45f, baseY);
        glVertex2f(s.x-halfW+0.45f, eaveY); glVertex2f(s.x-halfW,       eaveY);
        glVertex2f(s.x+halfW-0.45f, baseY); glVertex2f(s.x+halfW,       baseY);
        glVertex2f(s.x+halfW,       eaveY); glVertex2f(s.x+halfW-0.45f, eaveY);
    glEnd();
    float winH = s.h * 0.125f;
    DrawShopWindowRow4(s, baseY + s.h * 0.50f, winH, 0);
    DrawShopWindowRow4(s, baseY + s.h * 0.72f, winH, 2);
    float bandY = baseY + s.h * 0.41f;
    glColor3ub(MixB4(66, 176), MixB4(58, 168), MixB4(76, 172));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW, bandY);       glVertex2f(s.x+halfW, bandY);
        glVertex2f(s.x+halfW, bandY+0.4f);  glVertex2f(s.x-halfW, bandY+0.4f);
    glEnd();
    float frontTop = baseY + s.h * 0.26f;
    float frontIn  = halfW - 0.9f;
    float doorW    = s.w * 0.16f;
    float glassL   = s.x - frontIn;
    float glassR   = s.x + frontIn - doorW - 0.7f;
    glColor4ub(255, 198, 118, (unsigned char)(66.0f * (0.30f + 0.70f * NightT4())));
    glBegin(GL_QUADS);
        glVertex2f(glassL-1.2f, baseY-1.4f); glVertex2f(s.x+frontIn+1.2f, baseY-1.4f);
        glVertex2f(s.x+frontIn+1.2f, frontTop+0.6f); glVertex2f(glassL-1.2f, frontTop+0.6f);
    glEnd();
    glColor3ub(255, 214, 148);
    glBegin(GL_QUADS);
        glVertex2f(glassL, baseY+0.35f); glVertex2f(glassR, baseY+0.35f);
        glVertex2f(glassR, frontTop);    glVertex2f(glassL, frontTop);
    glEnd();
    for (int i = 0; i < 2; i++) {
        float t  = glassL + (glassR - glassL) * (0.3f + 0.4f * i)
                 + sinf(twinklePhase4 * 0.5f + i * 2.1f) * s.w * 0.05f;
        float fh = (frontTop - baseY) * 0.55f;
        glColor4ub(60, 40, 34, 190);
        glBegin(GL_QUADS);
            glVertex2f(t-0.45f, baseY+0.35f); glVertex2f(t+0.45f, baseY+0.35f);
            glVertex2f(t+0.45f, baseY+0.35f+fh); glVertex2f(t-0.45f, baseY+0.35f+fh);
        glEnd();
        FilledCircle4(t, baseY+0.35f+fh+0.35f, 0.38f, 60, 40, 34, 190);
    }
    glColor3ub(58, 44, 38);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        for (int i = 1; i <= 3; i++) {
            float mx = glassL + (glassR - glassL) * i / 4.0f;
            glVertex2f(mx, baseY+0.35f); glVertex2f(mx, frontTop);
        }
        glVertex2f(glassL, frontTop-(frontTop-baseY)*0.22f);
        glVertex2f(glassR, frontTop-(frontTop-baseY)*0.22f);
    glEnd();
    glBegin(GL_LINE_LOOP);
        glVertex2f(glassL, baseY+0.35f); glVertex2f(glassR, baseY+0.35f);
        glVertex2f(glassR, frontTop);    glVertex2f(glassL, frontTop);
    glEnd();
    float doorL = glassR + 0.7f, doorR = doorL + doorW;
    glColor3ub(92, 58, 44);
    glBegin(GL_QUADS);
        glVertex2f(doorL, baseY+0.35f); glVertex2f(doorR, baseY+0.35f);
        glVertex2f(doorR, frontTop);    glVertex2f(doorL, frontTop);
    glEnd();
    glColor3ub(255, 226, 170);
    glBegin(GL_QUADS);
        glVertex2f(doorL+0.18f, frontTop-0.95f); glVertex2f(doorR-0.18f, frontTop-0.95f);
        glVertex2f(doorR-0.18f, frontTop-0.18f); glVertex2f(doorL+0.18f, frontTop-0.18f);
    glEnd();
    float wreathY = baseY + (frontTop-baseY) * 0.55f;
    FilledCircle4((doorL+doorR)*0.5f, wreathY, doorW*0.30f, 52, 112, 68, 255);
    FilledCircle4((doorL+doorR)*0.5f, wreathY, doorW*0.16f,  92, 58, 44, 255);
    float awnY    = frontTop;
    float awnH    = 1.20f;
    float awnHalf = halfW - 0.55f;
    int   stripes = 9;
    for (int i = 0; i < stripes; i++) {
        float x0 = s.x - awnHalf + (2.0f*awnHalf) * i     / stripes;
        float x1 = s.x - awnHalf + (2.0f*awnHalf) * (i+1) / stripes;
        if (i % 2 == 0) glColor3ub(186, 62, 62);
        else            glColor3ub(238, 238, 240);
        glBegin(GL_QUADS);
            glVertex2f(x0, awnY); glVertex2f(x1, awnY);
            glVertex2f(x1, awnY+awnH); glVertex2f(x0, awnY+awnH);
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex2f(x0, awnY); glVertex2f(x1, awnY);
            glVertex2f((x0+x1)*0.5f, awnY-0.45f);
        glEnd();
    }
    glColor3ub(46, 40, 52);
    glBegin(GL_QUADS);
        glVertex2f(s.x-awnHalf, awnY+awnH);       glVertex2f(s.x+awnHalf, awnY+awnH);
        glVertex2f(s.x+awnHalf, awnY+awnH+0.28f); glVertex2f(s.x-awnHalf, awnY+awnH+0.28f);
    glEnd();
    if (s.sign != nullptr) {
        const char* name = s.sign;
        float ty    = awnY + awnH + 0.85f;
        float signH = 2.2f;
        float txtW  = TextPixelWidth(GLUT_BITMAP_HELVETICA_12, name)
                    * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        float half  = txtW * 0.5f + 1.4f;
        glColor4ub(255, 198, 112, 52);
        glBegin(GL_QUADS);
            glVertex2f(s.x-half-1.4f, ty-1.1f);        glVertex2f(s.x+half+1.4f, ty-1.1f);
            glVertex2f(s.x+half+1.4f, ty+signH+1.1f);  glVertex2f(s.x-half-1.4f, ty+signH+1.1f);
        glEnd();
        glColor3ub(48, 36, 28);
        glBegin(GL_QUADS);
            glVertex2f(s.x-half, ty);         glVertex2f(s.x+half, ty);
            glVertex2f(s.x+half, ty+signH);   glVertex2f(s.x-half, ty+signH);
        glEnd();
        glColor3ub(206, 164, 100);
        glLineWidth(1.6f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(s.x-half, ty);         glVertex2f(s.x+half, ty);
            glVertex2f(s.x+half, ty+signH);   glVertex2f(s.x-half, ty+signH);
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(s.x-half, ty+signH); glVertex2f(s.x-half-0.9f, ty+signH+0.9f);
            glVertex2f(s.x+half, ty+signH); glVertex2f(s.x+half+0.9f, ty+signH+0.9f);
        glEnd();
        glLineWidth(1.0f);
        glColor3ub(255, 232, 178);
        DrawTextCentered(s.x, ty + signH*0.5f - 0.55f, GLUT_BITMAP_HELVETICA_12, name);
    }
    glColor3ub(MixB4(38, 82), MixB4(33, 76), MixB4(48, 88));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW-over, eaveY-0.6f); glVertex2f(s.x+halfW+over, eaveY-0.6f);
        glVertex2f(s.x+halfW+over, eaveY);      glVertex2f(s.x-halfW-over, eaveY);
    glEnd();
    glColor3ub(MixB4(40, 104), MixB4(35, 96), MixB4(50, 108));
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x-halfW-over, eaveY);
        glVertex2f(s.x,            eaveY);
        glVertex2f(s.x,            eaveY+rise);
    glEnd();
    glColor3ub(MixB4(66, 150), MixB4(58, 142), MixB4(80, 150));
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x,            eaveY);
        glVertex2f(s.x+halfW+over, eaveY);
        glVertex2f(s.x,            eaveY+rise);
    glEnd();
    glColor3ub(MixB4(200, 214), MixB4(212, 226), MixB4(234, 242));
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x-halfW-over+0.55f, eaveY+0.75f);
        glVertex2f(s.x,                  eaveY+0.75f);
        glVertex2f(s.x,                  eaveY+rise+0.40f);
    glEnd();
    glColor3ub(238, 243, 252);
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x,                  eaveY+0.75f);
        glVertex2f(s.x+halfW+over-0.55f, eaveY+0.75f);
        glVertex2f(s.x,                  eaveY+rise+0.40f);
    glEnd();
    if (s.lit[2]) FilledCircle4(s.x, eaveY + rise*0.42f, 0.45f, 255, 206, 128, 255);
    else          FilledCircle4(s.x, eaveY + rise*0.42f, 0.45f,  26,  23,  34, 255);
}
// Renders architectural street wall of medieval/renaissance style market buildings
void DrawShops4()
{
    for (int i = 0; i < NUM_SHOPS4; i++) DrawShop4(shops4[i]);
}
inline float ChimneyX4()
{
    return shops4[1].x + shops4[1].w * 0.26f;
}
// Brick rooftop chimneys puffing cozy woodfire smoke into the winter air
void DrawChimney4()
{
    float x    = ChimneyX4();
    float foot = ShopRoofY4(shops4[1], x) - 0.3f;
    float top  = foot + 2.8f;
    glColor3ub(58, 52, 62);
    glBegin(GL_QUADS);
        glVertex2f(x-0.7f, foot); glVertex2f(x+0.7f, foot);
        glVertex2f(x+0.7f, top);  glVertex2f(x-0.7f, top);
    glEnd();
    glColor3ub(74, 66, 78);
    glBegin(GL_QUADS);
        glVertex2f(x-0.95f, top);       glVertex2f(x+0.95f, top);
        glVertex2f(x+0.95f, top+0.35f); glVertex2f(x-0.95f, top+0.35f);
    glEnd();
    glColor3ub(238, 243, 252);
    glBegin(GL_QUADS);
        glVertex2f(x-0.95f, top+0.35f); glVertex2f(x+0.95f, top+0.35f);
        glVertex2f(x+0.75f, top+0.75f); glVertex2f(x-0.75f, top+0.75f);
    glEnd();
}
// Festoon fairy string lights suspended in catenary curves between market buildings
void DrawLightSpan4(const LightSpan4& span, int spanIdx)
{
    glColor3ub(60, 50, 40);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= BULBS_PER_SPAN4+1; i++) {
            float t = (float)i / (BULBS_PER_SPAN4+1);
            float x = span.x1 + (span.x2-span.x1)*t;
            float y = span.y1 + (span.y2-span.y1)*t - sinf(t*PI4)*1.5f;
            glVertex2f(x, y);
        }
    glEnd();
    for (int i = 0; i < BULBS_PER_SPAN4; i++) {
        float t = (float)(i+1) / (BULBS_PER_SPAN4+1);
        float x = span.x1 + (span.x2-span.x1)*t;
        float y = span.y1 + (span.y2-span.y1)*t - sinf(t*PI4)*1.5f;
        unsigned char r, g, b;
        if (multicolorLights4) {
            int c = i % 4;
            unsigned char cols[4][3] = { {230,60,60}, {60,200,90}, {60,140,230}, {240,210,60} };
            r = cols[c][0]; g = cols[c][1]; b = cols[c][2];
        } else { r = 255; g = 210; b = 140; }
        float on = bulbLit4[spanIdx][i] ? 1.0f : 0.35f;
        FilledCircle4(x, y, 0.28f, (unsigned char)(r*on), (unsigned char)(g*on), (unsigned char)(b*on), 255);
    }
}
// Renders all illuminated holiday light strings spanning over the market plaza
void DrawLightSpans4()
{
    if (dayT4 > 0.05f) {
        glColor4ub(255, 218, 170, (unsigned char)(90.0f * dayT4));
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int i = 0; i < NUM_LIGHT_SPANS4; i++) {
                const LightSpan4& sp = lightSpans4[i];
                glVertex2f(sp.x1, sp.y1); glVertex2f(sp.x2, sp.y2);
            }
        glEnd();
    }
    for (int i = 0; i < NUM_LIGHT_SPANS4; i++) DrawLightSpan4(lightSpans4[i], i);
}
inline bool FarWindowLit4(int seed, int col, int row)
{
    int h = seed * 7919 + col * 733 + row * 197;
    h ^= (h >> 7);
    return (h % 100) < 34;
}
void DrawFarBlock4(float cx, float w, float h, float depth, float baseY,
                   int seed, bool pitchedRoof)
{
    float half = w * 0.5f;
    float topY = baseY + h;
    unsigned char botR, botG, botB, topR, topG, topB;
    AirFade4(depth, baseY, 24, 26, 40,  78,  90, 116, botR, botG, botB);
    AirFade4(depth, topY,  42, 45, 64, 120, 134, 160, topR, topG, topB);
    glBegin(GL_QUADS);
        glColor3ub(botR, botG, botB);
        glVertex2f(cx - half, baseY); glVertex2f(cx + half, baseY);
        glColor3ub(topR, topG, topB);
        glVertex2f(cx + half, topY);  glVertex2f(cx - half, topY);
    glEnd();
    float dir = (cx < 0.0f) ? 1.0f : -1.0f;
    float fx  = cx + dir * half;
    float sw  = fabsf(fx) * 0.048f * (1.0f - depth * 0.55f);
    if (sw > 2.0f) sw = 2.0f;
    if (sw > 0.12f) {
        float k       = sw / (fabsf(fx) + 0.001f);
        float backTop = topY + (baseY - topY) * k;
        bool  lit = (dir > 0.0f);
        unsigned char sr, sg, sb;
        if (lit) AirFade4(depth, topY, 34, 37, 52, 120, 130, 152, sr, sg, sb);
        else     AirFade4(depth, topY, 13, 15, 25,  62,  70,  90, sr, sg, sb);
        glColor3ub(sr, sg, sb);
        glBegin(GL_QUADS);
            glVertex2f(fx,          baseY);
            glVertex2f(fx + dir*sw, baseY);
            glVertex2f(fx + dir*sw, backTop);
            glVertex2f(fx,          topY);
        glEnd();
    }
    if (depth < 0.82f && w > 3.0f && h > 3.0f) {
        int cols = (int)(w / 1.9f); if (cols > 7) cols = 7;
        int rows = (int)(h / 2.0f); if (rows > 6) rows = 6;
        if (cols > 0 && rows > 0) {
            float slot = w / (cols + 1.0f);
            float ww   = slot * 0.42f;
            float wh   = (h / (rows + 1.0f)) * 0.44f;
            float fade = 1.0f - depth;
            for (int c = 0; c < cols; c++) {
                for (int rw = 0; rw < rows; rw++) {
                    float wx = cx - half + slot * (c + 1) - ww * 0.5f;
                    float wy = baseY + (h / (rows + 1.0f)) * (rw + 1) - wh * 0.5f;
                    bool on = FarWindowLit4(seed, c, rw);
                    if (on) {
                        glColor4ub(MixB4(255, 150), MixB4(202, 168), MixB4(120, 192),
                                   (unsigned char)((MixB4(225, 120)) * (0.35f + 0.65f * fade)));
                    } else {
                        glColor4ub(MixB4(14, 74), MixB4(16, 84), MixB4(26, 104),
                                   (unsigned char)(190 * (0.30f + 0.70f * fade)));
                    }
                    glBegin(GL_QUADS);
                        glVertex2f(wx,      wy);      glVertex2f(wx + ww, wy);
                        glVertex2f(wx + ww, wy + wh); glVertex2f(wx,      wy + wh);
                    glEnd();
                }
            }
        }
    }
    if (pitchedRoof) {
        unsigned char rr, rg, rb;
        AirFade4(depth, topY, 46, 40, 58, 118, 122, 140, rr, rg, rb);
        glColor3ub(rr, rg, rb);
        glBegin(GL_TRIANGLES);
            glVertex2f(cx - half - 0.35f, topY);
            glVertex2f(cx + half + 0.35f, topY);
            glVertex2f(cx, topY + 1.1f + w * 0.055f);
        glEnd();
    }
    unsigned char sc_r, sc_g, sc_b;
    AirFade4(depth * 0.55f, topY, 214, 224, 240, 246, 250, 255, sc_r, sc_g, sc_b);
    float cap = (0.22f + 0.42f * SnowDepth4()) * (1.0f - depth * 0.35f);
    if (pitchedRoof) {
        glColor3ub(sc_r, sc_g, sc_b);
        glBegin(GL_TRIANGLES);
            glVertex2f(cx - half + 0.25f, topY + 0.35f);
            glVertex2f(cx + half - 0.25f, topY + 0.35f);
            glVertex2f(cx, topY + 1.1f + w * 0.055f + cap * 0.6f);
        glEnd();
    } else {
        glColor3ub(sc_r, sc_g, sc_b);
        glBegin(GL_QUADS);
            glVertex2f(cx - half - 0.2f, topY);
            glVertex2f(cx + half + 0.2f, topY);
            glVertex2f(cx + half + 0.1f, topY + cap);
            glVertex2f(cx - half - 0.1f, topY + cap);
        glEnd();
    }
    glColor4ub(MixB4(6, 60), MixB4(8, 68), MixB4(16, 86), (unsigned char)(70 * (1.0f - depth)));
    glBegin(GL_QUADS);
        glVertex2f(cx - half, baseY);
        glVertex2f(cx + half, baseY);
        glVertex2f(cx + half, baseY + 0.9f);
        glVertex2f(cx - half, baseY + 0.9f);
    glEnd();
}
struct FarBlock4
{
    float x, w, h; bool pitched;
};
// Far background snow-capped town silhouette and distant church spires
void DrawDistantBuildings4()
{
    static const FarBlock4 rowBack[] = {
        { -54.0f, 15.0f,  7.5f, false }, { -40.0f, 11.0f, 10.5f, false },
        { -29.0f, 10.0f,  6.2f, true  }, { -16.0f, 13.0f,  9.0f, false },
        {  -2.0f, 11.0f, 11.5f, false }, {  11.0f, 12.0f,  7.8f, true  },
        {  24.0f, 13.0f, 12.2f, false }, {  38.0f, 11.0f,  8.6f, false },
        {  51.0f, 15.0f, 10.0f, false }
    };
    static const FarBlock4 rowMid[] = {
        { -57.0f, 12.0f,  5.4f, true  }, { -45.0f,  9.0f,  8.2f, false },
        { -33.5f, 10.0f,  4.6f, true  }, { -21.0f, 11.0f,  7.4f, false },
        {  -9.0f,  9.5f,  9.6f, false }, {   3.0f, 10.0f,  5.2f, true  },
        {  16.0f, 11.0f,  8.8f, false }, {  30.0f,  9.5f,  6.0f, true  },
        {  44.0f, 12.0f,  9.2f, false }, {  57.0f, 10.0f,  6.6f, true  }
    };
    static const FarBlock4 rowNear[] = {
        { -51.0f, 10.0f,  4.0f, true  }, { -37.5f,  8.5f,  6.4f, false },
        { -24.0f,  9.0f,  3.4f, true  }, { -11.0f,  9.5f,  5.6f, false },
        {   2.5f,  8.0f,  3.8f, true  }, {  15.0f,  9.0f,  6.8f, false },
        {  28.0f,  8.5f,  4.2f, true  }, {  41.0f,  9.0f,  5.8f, false },
        {  54.5f, 10.0f,  3.6f, true  }
    };
    const int nBack = (int)(sizeof(rowBack) / sizeof(rowBack[0]));
    const int nMid  = (int)(sizeof(rowMid)  / sizeof(rowMid[0]));
    const int nNear = (int)(sizeof(rowNear) / sizeof(rowNear[0]));
    for (int i = 0; i < nBack; i++)
        DrawFarBlock4(rowBack[i].x, rowBack[i].w, rowBack[i].h, 0.84f, -3.2f, i + 3, rowBack[i].pitched);
    glBegin(GL_QUAD_STRIP);
        glColor4ub(MixB4(46, 208), MixB4(54, 224), MixB4(86, 242), 0);
        glVertex2f(-60.0f, 8.5f); glVertex2f(60.0f, 8.5f);
        glColor4ub(MixB4(46, 208), MixB4(54, 224), MixB4(86, 242), MixB4(56, 96));
        glVertex2f(-60.0f, 1.0f); glVertex2f(60.0f, 1.0f);
        glColor4ub(MixB4(46, 208), MixB4(54, 224), MixB4(86, 242), 0);
        glVertex2f(-60.0f, -5.6f); glVertex2f(60.0f, -5.6f);
    glEnd();
    for (int i = 0; i < nMid; i++)
        DrawFarBlock4(rowMid[i].x, rowMid[i].w, rowMid[i].h, 0.58f, -4.1f, i + 21, rowMid[i].pitched);
    glBegin(GL_QUAD_STRIP);
        glColor4ub(MixB4(44, 204), MixB4(52, 220), MixB4(84, 240), 0);
        glVertex2f(-60.0f, 5.5f); glVertex2f(60.0f, 5.5f);
        glColor4ub(MixB4(44, 204), MixB4(52, 220), MixB4(84, 240), MixB4(38, 66));
        glVertex2f(-60.0f, -0.5f); glVertex2f(60.0f, -0.5f);
        glColor4ub(MixB4(44, 204), MixB4(52, 220), MixB4(84, 240), 0);
        glVertex2f(-60.0f, -5.8f); glVertex2f(60.0f, -5.8f);
    glEnd();
    for (int i = 0; i < nNear; i++)
        DrawFarBlock4(rowNear[i].x, rowNear[i].w, rowNear[i].h, 0.34f, -5.0f, i + 47, rowNear[i].pitched);
}
constexpr float roadFarX4  = -35.4f;
constexpr float roadFarY4  =  -6.0f;
constexpr float roadFarHW4 =   1.70f;
void DrawRoadsideRun4(float nearX, float nearTop, float farX, float farTop,
                      int seed, int nBays)
{
    for (int i = 0; i < nBays; i++) {
        float t0 = (float)i       / nBays;
        float t1 = (float)(i + 1) / nBays;
        float x0 = nearX + (farX - nearX) * t0;
        float x1 = nearX + (farX - nearX) * t1;
        float jitter = sinf((float)(seed + i) * 2.3f) * 0.9f * (1.0f - t0);
        float y0 = nearTop + (farTop - nearTop) * t0 + jitter;
        float y1 = nearTop + (farTop - nearTop) * t1 + jitter;
        float depth = 0.30f + 0.55f * t0;
        unsigned char botR, botG, botB, topR, topG, topB;
        AirFade4(depth, roadFarY4, 27, 29, 42,  86,  94, 114, botR, botG, botB);
        AirFade4(depth, y0,        50, 53, 72, 126, 134, 152, topR, topG, topB);
        glBegin(GL_QUADS);
            glColor3ub(botR, botG, botB);
            glVertex2f(x0, roadFarY4); glVertex2f(x1, roadFarY4);
            glColor3ub(topR, topG, topB);
            glVertex2f(x1, y1);        glVertex2f(x0, y0);
        glEnd();
        unsigned char er, eg, eb;
        AirFade4(depth, y0, 72, 76, 96, 158, 166, 180, er, eg, eb);
        glColor3ub(er, eg, eb);
        glBegin(GL_QUADS);
            glVertex2f(x1 - (x1 - x0) * 0.10f, roadFarY4);
            glVertex2f(x1,                     roadFarY4);
            glVertex2f(x1,                     y1);
            glVertex2f(x1 - (x1 - x0) * 0.10f, y1 + (y0 - y1) * 0.10f);
        glEnd();
        float bayW = fabsf(x1 - x0);
        for (int rw = 0; rw < 2; rw++) {
            for (int c = 0; c < 2; c++) {
                float fx = x0 + (x1 - x0) * (0.24f + 0.44f * c);
                float fy = roadFarY4 + (y0 - roadFarY4) * (0.34f + 0.33f * rw);
                float ww = bayW * 0.16f * (1.0f - t0 * 0.45f);
                float wh = ww * 1.5f;
                if (ww < 0.08f) continue;
                if (FarWindowLit4(seed * 3 + 11, c, rw + i)) {
                    glColor4ub(MixB4(255, 168), MixB4(206, 182), MixB4(126, 202),
                               (unsigned char)(MixB4(250, 150) * (1.0f - t0 * 0.4f)));
                } else {
                    glColor4ub(MixB4(14, 66), MixB4(17, 76), MixB4(30, 98),
                               (unsigned char)(215 * (1.0f - t0 * 0.4f)));
                }
                glBegin(GL_QUADS);
                    glVertex2f(fx - ww, fy - wh); glVertex2f(fx + ww, fy - wh);
                    glVertex2f(fx + ww, fy + wh); glVertex2f(fx - ww, fy + wh);
                glEnd();
            }
        }
        unsigned char sr, sg, sb;
        AirFade4(depth * 0.5f, y0, 210, 220, 238, 244, 248, 255, sr, sg, sb);
        float cap = (0.22f + 0.40f * SnowDepth4()) * (1.0f - t0 * 0.5f);
        glColor3ub(sr, sg, sb);
        glBegin(GL_QUADS);
            glVertex2f(x0, y0); glVertex2f(x1, y1);
            glVertex2f(x1, y1 + cap); glVertex2f(x0, y0 + cap);
        glEnd();
    }
}
// Architectural building facades framing the perspective avenue
void DrawRoadsideBlocks4()
{
    DrawRoadsideRun4(-48.6f, 7.8f, roadFarX4 - roadFarHW4 - 0.2f, -3.0f, 5, 5);
    DrawRoadsideRun4(-28.8f, 6.4f, roadFarX4 + roadFarHW4 + 0.2f, -3.2f, 12, 3);
    float hr, hg, hb;
    SkyAirColour4(-3.5f, hr, hg, hb);
    unsigned char hzR = (unsigned char)hr, hzG = (unsigned char)hg, hzB = (unsigned char)hb;
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(hzR, hzG, hzB, MixB4(96, 140));
        glVertex2f(roadFarX4, roadFarY4 + 0.8f);
        glColor4ub(hzR, hzG, hzB, 0);
        for (int i = 0; i <= 20; i++) {
            float a = (float)i / 20.0f * 6.2831853f;
            glVertex2f(roadFarX4 + 5.2f * cosf(a), roadFarY4 + 0.8f + 3.0f * sinf(a));
        }
    glEnd();
    float night = NightT4();
    if (night > 0.02f) {
        DrawSoftEllipse(roadFarX4, roadFarY4 + 1.6f, 2.6f, 1.6f,
                        255, 198, 128, (unsigned char)(78 * night), 4);
    }
}
// Distant concrete arched elevated transit rail viaduct with passing lighted train
void DrawMetroRail4()
{
    float maxEave = -999.0f;
    for (int i = 0; i < NUM_SHOPS4; i++) {
        float ey = ShopEaveY4(shops4[i]);
        if (ey > maxEave) maxEave = ey;
    }
    float railY = maxEave + 1.6f;
    float night = NightT4();
    unsigned char beamA = (unsigned char)fminf(255.0f, 90.0f + night * 140.0f);
    glColor4ub(48, 48, 56, beamA);
    glBegin(GL_QUADS);
        glVertex2f(-62.0f, railY + 0.45f); glVertex2f(62.0f, railY + 0.45f);
        glVertex2f(62.0f, railY - 0.05f);  glVertex2f(-62.0f, railY - 0.05f);
    glEnd();
    unsigned char supportA = (unsigned char)fminf(255.0f, 70.0f + night * 150.0f);
    glColor4ub(36, 36, 44, supportA);
    for (float x = -52.0f; x <= 52.0f; x += 8.0f) {
        glBegin(GL_QUADS);
            glVertex2f(x - 0.6f, railY - 0.05f); glVertex2f(x + 0.6f, railY - 0.05f);
            glVertex2f(x + 1.2f, railY - 8.0f);   glVertex2f(x - 1.2f, railY - 8.0f);
        glEnd();
    }
    unsigned char carA = (unsigned char)fminf(255.0f, 120.0f + night * 120.0f);
    float carHalf = 18.0f;
    float carTop = railY + 0.9f, carBot = railY - 0.05f;
    float carCenter = metroCarX4;
    glColor4ub(80, 78, 90, carA);
    glBegin(GL_QUADS);
        glVertex2f(carCenter - carHalf, carTop); glVertex2f(carCenter + carHalf, carTop);
        glVertex2f(carCenter + carHalf, carBot); glVertex2f(carCenter - carHalf, carBot);
    glEnd();
    glColor4ub(70, 68, 78, (unsigned char)fminf(220.0f, 90.0f + night * 120.0f));
    glBegin(GL_QUADS);
        glVertex2f(carCenter - carHalf - 0.6f, carTop); glVertex2f(carCenter + carHalf + 0.6f, carTop);
        glVertex2f(carCenter + carHalf + 0.6f, carTop + 0.5f); glVertex2f(carCenter - carHalf - 0.6f, carTop + 0.5f);
    glEnd();
    unsigned char winA = (unsigned char)fminf(255.0f, 140.0f + dayT4 * 80.0f);
    glColor4ub(200, 210, 220, winA);
    float w = 3.2f;
    for (float x = carCenter - carHalf + 2.0f; x < carCenter + carHalf - 1.0f; x += 5.0f) {
        glBegin(GL_QUADS);
            glVertex2f(x, carTop - 0.15f); glVertex2f(x + w, carTop - 0.15f);
            glVertex2f(x + w, carBot + 0.08f); glVertex2f(x, carBot + 0.08f);
        glEnd();
    }
}
// Timer callback: modulates festive string light sparkling and bulb color shifting
void UpdateTwinkle4(int)
{
    if (dayT4 != dayTarget4) {
        const float step = 0.018f;
        if (dayT4 < dayTarget4) dayT4 = (dayT4 + step > dayTarget4) ? dayTarget4 : dayT4 + step;
        else                    dayT4 = (dayT4 - step < dayTarget4) ? dayTarget4 : dayT4 - step;
        glutPostRedisplay();
    }
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateTwinkle4, 0); return; }
    if (isAnimating4) {
        twinklePhase4 += 0.03f;
        if (rand() % 10 == 0) {
            int s = rand() % NUM_LIGHT_SPANS4;
            int b = rand() % BULBS_PER_SPAN4;
            bulbLit4[s][b] = !bulbLit4[s][b];
        }
    }
    glutTimerFunc(60, UpdateTwinkle4, 0);
}
void DrawGroundGlow4(float x, float y, float rx, float ry,
                     unsigned char r, unsigned char g, unsigned char b,
                     unsigned char a)
{
    DrawSoftEllipse(x, y, rx, ry, r, g, b, a, 4);
}
// Warm yellow and golden pools of ambient illumination cast onto snow from lanterns and stalls
void DrawLightPools4()
{
    float nightGlow = NightT4();
    DrawGroundGlow4(18.0f, -18.0f, 62.0f, 17.0f, 150, 178, 225, (unsigned char)(34 * nightGlow));
    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        DrawGroundGlow4(sh.x, -7.4f, sh.w * 0.62f, 2.4f, 255, 196, 120, (unsigned char)(62 * nightGlow));
    }
    for (int i = 0; i < NUM_LIGHT_SPANS4; i++) {
        const LightSpan4& sp = lightSpans4[i];
        float mx = (sp.x1 + sp.x2) * 0.5f;
        unsigned char r = 255, g = 205, b = 140;
        if (multicolorLights4) { r = 225; g = 175; b = 205; }
        DrawGroundGlow4(mx, -8.6f, fabsf(sp.x2 - sp.x1) * 0.42f, 2.8f, r, g, b, (unsigned char)(46 * nightGlow));
    }
    for (int i = 0; i < NUM_STALLS4; i++)
        DrawGroundGlow4(stalls4[i].x, stallGroundY4 - 0.2f,
                        StallHalfW4() + 1.2f, 1.7f, 255, 190, 110, (unsigned char)(88 * nightGlow));
    DrawGroundGlow4(ferrisCX4, ferrisBaseY4 + 1.5f, ferrisR4 * 1.25f, 3.8f,
                    multicolorLights4 ? 210 : 255,
                    multicolorLights4 ? 150 : 190,
                    multicolorLights4 ? 220 : 110, (unsigned char)(74 * nightGlow));
    DrawGroundGlow4(treeX4,    -9.2f,  8.0f, 2.3f, 150, 255, 180, (unsigned char)(62 * nightGlow));
    DrawGroundGlow4(clockX4,   -6.9f,  6.5f, 2.0f, 255, 210, 150, (unsigned char)(58 * nightGlow));
    DrawGroundGlow4(-56.5f,    -9.4f,  6.0f, 1.9f, 255, 170, 120, (unsigned char)(54 * nightGlow));
    if (dayT4 > 0.02f) {
        DrawGroundGlow4(8.0f, -16.0f, 48.0f, 12.0f, 232, 220, 180, (unsigned char)(65.0f * dayT4));
    }
    if (auroraActive4) {
        float env = sinf(auroraTimer4 * PI4);
        if (env > 0.0f)
            DrawGroundGlow4(-4.0f, -13.0f, 66.0f, 13.0f, 90, 230, 170,
                            (unsigned char)(30.0f * env * nightGlow));
    }
}
// Computes alternating jewel-toned carnival bulb colors for Ferris wheel rim
void FerrisBulbColour4(int index, unsigned char& r, unsigned char& g, unsigned char& b)
{
    if (multicolorLights4) {
        const unsigned char cols[4][3] = { {255,70,70}, {70,225,110}, {80,160,255}, {255,225,90} };
        int c = index % 4;
        r = cols[c][0]; g = cols[c][1]; b = cols[c][2];
    } else { r = 255; g = 208; b = 135; }
}
constexpr int FERRIS_RIM_BULBS4 = 30;

// Market Ferris wheel with seasonal decoration
void DrawFerrisWheel4()
{
    float nightGlow = NightT4();
    unsigned char hr, hg, hb;
    FerrisBulbColour4(0, hr, hg, hb);
    DrawSoftEllipse(ferrisCX4, ferrisCY4, ferrisR4 * 1.75f, ferrisR4 * 1.75f,
                    hr, hg, hb, (unsigned char)(34 * nightGlow), 4);
    const float frameH  = ferrisCY4 - ferrisBaseY4;
    const float legHalf = frameH * 0.46f;
    const float braceY  = ferrisBaseY4 + frameH * 0.38f;
    glColor3ub(MixB4(70, 110), MixB4(70, 118), MixB4(80, 120));
    glLineWidth(2.6f * ferrisScale4);
    glBegin(GL_LINES);
        glVertex2f(ferrisCX4-legHalf, ferrisBaseY4); glVertex2f(ferrisCX4, ferrisCY4);
        glVertex2f(ferrisCX4+legHalf, ferrisBaseY4); glVertex2f(ferrisCX4, ferrisCY4);
        glVertex2f(ferrisCX4-legHalf*0.62f, braceY);
        glVertex2f(ferrisCX4+legHalf*0.62f, braceY);
    glEnd();
    glLineWidth(3.4f * ferrisScale4);
    glBegin(GL_LINES);
        glVertex2f(ferrisCX4-legHalf-1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4-legHalf+1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4+legHalf-1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4+legHalf+1.0f, ferrisBaseY4);
    glEnd();
    glColor3ub(MixB4(150, 135), MixB4(52, 110), MixB4(74, 90));
    glLineWidth(2.4f * ferrisScale4);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 28; i++) {
            float a = (float)i / 28 * 2 * PI4;
            glVertex2f(ferrisCX4 + ferrisR4*cos(a), ferrisCY4 + ferrisR4*sin(a));
        }
    glEnd();
    for (int i = 0; i < FERRIS_RIM_BULBS4; i++) {
        float a = (float)i / FERRIS_RIM_BULBS4 * 2.0f * PI4 + ferrisAngle4 * 0.25f;
        float bx = ferrisCX4 + ferrisR4 * cosf(a);
        float by = ferrisCY4 + ferrisR4 * sinf(a);
        float chase = 0.35f + 0.65f * (0.5f + 0.5f * sinf(twinklePhase4 * 4.0f - i * 0.62f));
        unsigned char r, g, b;
        FerrisBulbColour4(i, r, g, b);
        FilledCircle4(bx, by, 0.62f * ferrisScale4 * chase, r, g, b, (unsigned char)(70 * chase * nightGlow));
        FilledCircle4(bx, by, 0.20f * ferrisScale4,
                      (unsigned char)(r * chase * nightGlow), (unsigned char)(g * chase * nightGlow),
                      (unsigned char)(b * chase * nightGlow), (unsigned char)(255 * nightGlow));
    }
    for (int sp = 0; sp < NUM_CABINS4; sp++) {
        float a = ferrisAngle4 + sp * (2*PI4/NUM_CABINS4);
        for (int k = 1; k <= 2; k++) {
            float rr = ferrisR4 * (0.38f + 0.30f * k);
            float bx = ferrisCX4 + rr * cosf(a);
            float by = ferrisCY4 + rr * sinf(a);
            float chase = 0.4f + 0.6f * (0.5f + 0.5f * sinf(twinklePhase4 * 3.0f - sp * 0.8f));
            unsigned char r, g, b;
            FerrisBulbColour4(sp + k, r, g, b);
            FilledCircle4(bx, by, 0.16f * ferrisScale4,
                          (unsigned char)(r * chase * nightGlow), (unsigned char)(g * chase * nightGlow),
                          (unsigned char)(b * chase * nightGlow), (unsigned char)(245 * nightGlow));
        }
    }
    for (int i = 0; i < NUM_CABINS4; i++) {
        float a = ferrisAngle4 + i * (2*PI4/NUM_CABINS4);
        float cx = ferrisCX4 + ferrisR4*cosf(a);
        float cy = ferrisCY4 + ferrisR4*sinf(a);
        glColor3ub(MixB4(90, 120), MixB4(90, 120), MixB4(100, 122));
        glLineWidth(1.0f * ferrisScale4);
        glBegin(GL_LINES); glVertex2f(ferrisCX4, ferrisCY4); glVertex2f(cx, cy); glEnd();
        const float g = ferrisScale4;
        glColor3ub(MixB4(70, 90), MixB4(70, 90), MixB4(80, 100));
        glLineWidth(1.2f * g);
        glBegin(GL_LINES);
            glVertex2f(cx, cy + 0.95f*g); glVertex2f(cx, cy + 0.45f*g);
        glEnd();
        unsigned char lit = (i % 2 == 0) ? 235 : 150;
        FilledCircle4(cx, cy, 1.15f*g, 255, 210, 120, (unsigned char)((lit / 5) * nightGlow));
        glColor3ub(MixB4(210, 195), MixB4(70, 80), MixB4(85, 100));
        glBegin(GL_QUADS);
            glVertex2f(cx-0.62f*g, cy-0.52f*g); glVertex2f(cx+0.62f*g, cy-0.52f*g);
            glVertex2f(cx+0.70f*g, cy+0.20f*g); glVertex2f(cx-0.70f*g, cy+0.20f*g);
        glEnd();
        glColor3ub((unsigned char)(255 * nightGlow), (unsigned char)(226 * nightGlow), (unsigned char)(lit * nightGlow));
        glBegin(GL_QUADS);
            glVertex2f(cx-0.52f*g, cy-0.18f*g); glVertex2f(cx+0.52f*g, cy-0.18f*g);
            glVertex2f(cx+0.56f*g, cy+0.18f*g); glVertex2f(cx-0.56f*g, cy+0.18f*g);
        glEnd();
        glColor3ub(240, 240, 245);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy + 0.22f*g);
            for (int k = 0; k <= 8; k++) {
                float a2 = PI4 * ((float)k / 8.0f);
                glVertex2f(cx + 0.72f*g * cosf(a2), cy + 0.22f*g + 0.40f*g * sinf(a2));
            }
        glEnd();
        if (i % 2 == 0) {
            FilledCircle4(cx - 0.20f*g, cy - 0.02f*g, 0.13f*g, 60, 50, 60, 235);
            FilledCircle4(cx + 0.20f*g, cy - 0.04f*g, 0.11f*g, 60, 50, 60, 235);
        }
    }
    FilledCircle4(ferrisCX4, ferrisCY4, 2.10f * ferrisScale4, 255, 225, 170, (unsigned char)(44 * nightGlow));
    FilledCircle4(ferrisCX4, ferrisCY4, 0.90f * ferrisScale4, 255, 236, 196, (unsigned char)(120 * nightGlow));
    FilledCircle4(ferrisCX4, ferrisCY4, 0.50f * ferrisScale4, 70, 66, 76, (unsigned char)(255 * nightGlow));
    FilledCircle4(ferrisCX4, ferrisCY4, 0.22f * ferrisScale4, 255, 246, 220, (unsigned char)(255 * nightGlow));
}
// Timer callback: rotates giant market Ferris wheel and maintains gondola level orientations
void UpdateFerrisWheel4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFerrisWheel4, 0); return; }
    if (isAnimating4) ferrisAngle4 += 0.01f;
    glutTimerFunc(30, UpdateFerrisWheel4, 0);
}
// Open-air public ice skating rink with frosted ice surface, perimeter timber barriers, and festoon posts
void DrawRink4()
{
    if (SeasonWinter4() < 0.02f) return;
    glBegin(GL_TRIANGLE_FAN);
        glColor3ub(MixB4(196, 160), MixB4(224, 220), MixB4(242, 245));
        glVertex2f(rinkCX4, rinkCY4);
        glColor3ub(MixB4(168, 140), MixB4(202, 202), MixB4(226, 206));
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a));
        }
    glEnd();
    glColor4ub(238, 246, 255, (unsigned char)(90 * NightT4()));
    glBegin(GL_QUADS);
        glVertex2f(rinkCX4 + 1.0f, rinkCY4 + 1.5f);
        glVertex2f(rinkCX4 + 5.2f, rinkCY4 + 0.2f);
        glVertex2f(rinkCX4 + 5.0f, rinkCY4 - 0.3f);
        glVertex2f(rinkCX4 + 0.9f, rinkCY4 + 1.0f);
    glEnd();
    if (dayT4 > 0.08f) {
        glColor4ub(255, 214, 160, (unsigned char)(45 * dayT4));
        glBegin(GL_QUADS);
            glVertex2f(rinkCX4 - 1.0f, rinkCY4 + 1.7f);
            glVertex2f(rinkCX4 + 4.4f, rinkCY4 + 0.5f);
            glVertex2f(rinkCX4 + 4.0f, rinkCY4 - 0.2f);
            glVertex2f(rinkCX4 - 0.8f, rinkCY4 + 1.1f);
        glEnd();
    }
    glColor4ub(232, 242, 252, 120);
    glLineWidth(1.0f);
    for (int i = 0; i < 7; i++) {
        float a0 = i * 0.9f;
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 10; k++) {
                float a = a0 + k * 0.16f;
                float rr = 2.2f + (i % 3) * 1.3f;
                glVertex2f(rinkCX4 + rr*cosf(a), rinkCY4 + rr*0.36f*sinf(a));
            }
        glEnd();
    }
    glColor3ub(226, 230, 238);
    glLineWidth(2.4f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 30; i++) {
            float a = -0.35f + (float)i / 30 * (2 * PI4 - 0.70f);
            glVertex2f(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a));
        }
    glEnd();
    glColor3ub(150, 60, 65);
    for (int i = 0; i < 6; i++) {
        float a = (float)i / 6 * 2 * PI4 + 0.5f;
        FilledCircle4(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a), 0.16f, 150, 60, 65, 255);
    }
    FilledCircle4(rinkCX4 + 6.0f*cosf(-0.35f), rinkCY4 + 2.2f*sinf(-0.35f), 0.20f, 210, 200, 120, 255);
    FilledCircle4(rinkCX4 + 6.0f*cosf(0.35f),  rinkCY4 + 2.2f*sinf(0.35f),  0.20f, 210, 200, 120, 255);
    glLineWidth(1.0f);
}
// Individual ice skater gliding across the rink with graceful skate strides and scarf trailing
void DrawSkater4(const Skater4& sk)
{
    float x = rinkCX4 + sk.radiusX * cosf(sk.angle);
    float y = rinkCY4 + sk.radiusY * sinf(sk.angle);
    bool  isFaller = (&sk == &skaters4[FALLING_SKATER4]);
    float fall = 0.0f;
    if (isFaller && skaterFallT4 > 0.0f) {
        if      (skaterFallT4 < 0.18f) fall = skaterFallT4 / 0.18f;
        else if (skaterFallT4 < 0.70f) fall = 1.0f;
        else                            fall = (1.0f - skaterFallT4) / 0.30f;
        if (fall < 0.0f) fall = 0.0f;
        if (fall > 1.0f) fall = 1.0f;
    }
    float leanDeg = -cosf(sk.angle) * (sk.speed * 900.0f) / (sk.radiusX + 1.0f);
    if (leanDeg >  22.0f) leanDeg =  22.0f;
    if (leanDeg < -22.0f) leanDeg = -22.0f;
    float dirSign = (-sinf(sk.angle) >= 0.0f) ? 1.0f : -1.0f;
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(leanDeg + fall * 72.0f * dirSign, 0.0f, 0.0f, 1.0f);
    glScalef(1.0f, 1.0f - fall * 0.35f, 1.0f);
    if (fall > 0.0f && skaterFallT4 < 0.30f) {
        for (int i = 0; i < 5; i++) {
            float t = fmodf(skaterFallT4 * 3.0f + i * 0.2f, 1.0f);
            FilledCircle4(-0.6f * dirSign - t * 1.4f, 0.15f + t * 0.8f,
                          0.16f * (1.0f - t), 244, 250, 255,
                          (unsigned char)(190 * (1.0f - t)));
        }
    }
    glColor3ub(238, 246, 255);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(-0.30f * dirSign, 0.02f); glVertex2f(0.34f * dirSign, 0.02f);
    glEnd();
    glColor3ub(40, 40, 52);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, 1.0f); glVertex2f(-0.14f * dirSign, 0.05f);
        glVertex2f(0.0f, 1.0f); glVertex2f( 0.20f * dirSign, 0.05f);
    glEnd();
    glColor3ub(sk.r, sk.g, sk.b);
    glLineWidth(3.5f);
    glBegin(GL_LINES); glVertex2f(0.0f, 1.0f); glVertex2f(0.0f, 1.9f); glEnd();
    float swing = sinf(sk.angle * 6.0f) * 0.22f;
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, 1.70f); glVertex2f( 0.62f * dirSign, 1.55f + swing);
        glVertex2f(0.0f, 1.70f); glVertex2f(-0.58f * dirSign, 1.60f - swing);
    glEnd();
    FilledCircle4(0.0f, 2.20f, 0.28f, 225, 185, 145, 255);
    FilledCircle4(0.0f, 2.44f, 0.22f, sk.r, sk.g, sk.b, 255);
    FilledCircle4(0.0f, 2.64f, 0.09f, 250, 250, 255, 255);
    glColor3ub(sk.r, sk.g, sk.b);
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(0.0f, 1.95f);
        glVertex2f(-0.45f * dirSign, 1.80f + swing * 0.5f);
        glVertex2f(-0.85f * dirSign, 1.62f - swing * 0.4f);
    glEnd();
    glLineWidth(1.0f);
    glPopMatrix();
}
// Renders all ice skaters circling and gliding around the ice rink
void DrawSkaters4()
{
    if (SeasonWinter4() < 0.02f) return;
    for (int i = 0; i < NUM_SKATERS4; i++) DrawSkater4(skaters4[i]);
    const Skater4& a = skaters4[3];
    const Skater4& b = skaters4[4];
    float ax = rinkCX4 + a.radiusX * cosf(a.angle), ay = rinkCY4 + a.radiusY * sinf(a.angle);
    float bx = rinkCX4 + b.radiusX * cosf(b.angle), by = rinkCY4 + b.radiusY * sinf(b.angle);
    glColor3ub(232, 196, 158);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(ax, ay + 1.68f);
        glVertex2f((ax + bx) * 0.5f, (ay + by) * 0.5f + 1.48f);
        glVertex2f(bx, by + 1.68f);
    glEnd();
    glLineWidth(1.0f);
}
// Timer callback: moves skaters along elliptical orbits around the ice rink with lean angles
void UpdateSkaters4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSkaters4, 0); return; }
    if (isAnimating4) {
        for (int i = 0; i < NUM_SKATERS4; i++) {
            float slow = (i == FALLING_SKATER4 && skaterFallT4 > 0.0f)
                       ? (1.0f - 0.92f * (skaterFallT4 < 0.70f ? 1.0f : 0.4f)) : 1.0f;
            skaters4[i].angle += skaters4[i].speed * slow;
        }
        if (skaterFallT4 > 0.0f) {
            skaterFallT4 += 0.006f;
            if (skaterFallT4 >= 1.0f) { skaterFallT4 = 0.0f; skaterFallCool4 = 420 + rand() % 500; }
        } else if (--skaterFallCool4 <= 0) {
            skaterFallT4 = 0.001f;
        }
    }
    glutTimerFunc(25, UpdateSkaters4, 0);
}
// Majestic draft horse with winter harness pulling a wooden passenger sled
void DrawHorse4(float x, float y, float gait)
{
    float legA = sinf(gait), legB = sinf(gait + PI4);
    glColor3ub(58, 40, 30);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.75f, y + 0.55f); glVertex2f(x - 0.75f + 0.34f*legA, y - 0.35f);
        glVertex2f(x - 0.55f, y + 0.55f); glVertex2f(x - 0.55f + 0.30f*legB, y - 0.35f);
        glVertex2f(x + 0.70f, y + 0.55f); glVertex2f(x + 0.70f + 0.34f*legB, y - 0.35f);
        glVertex2f(x + 0.50f, y + 0.55f); glVertex2f(x + 0.50f + 0.30f*legA, y - 0.35f);
    glEnd();
    glColor3ub(78, 54, 38);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.95f, y + 0.55f);
        glVertex2f(x + 0.95f, y + 0.55f);
        glVertex2f(x + 1.00f, y + 1.30f);
        glVertex2f(x - 0.90f, y + 1.35f);
    glEnd();
    glBegin(GL_POLYGON);
        glVertex2f(x + 0.80f, y + 1.10f);
        glVertex2f(x + 1.15f, y + 1.15f);
        glVertex2f(x + 1.70f, y + 2.05f);
        glVertex2f(x + 1.35f, y + 2.10f);
    glEnd();
    glBegin(GL_POLYGON);
        glVertex2f(x + 1.34f, y + 1.98f);
        glVertex2f(x + 1.95f, y + 2.12f);
        glVertex2f(x + 1.98f, y + 2.40f);
        glVertex2f(x + 1.36f, y + 2.34f);
    glEnd();
    glColor3ub(44, 30, 22);
    glBegin(GL_TRIANGLES);
        glVertex2f(x + 1.42f, y + 2.34f); glVertex2f(x + 1.56f, y + 2.34f); glVertex2f(x + 1.48f, y + 2.62f);
    glEnd();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.30f, y + 2.05f); glVertex2f(x + 0.95f, y + 1.35f);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(x - 0.92f, y + 1.25f); glVertex2f(x - 1.45f, y + 0.62f + 0.12f*legA);
    glEnd();
    glColor3ub(120, 80, 45);
    glLineWidth(1.6f);
    glBegin(GL_LINES);
        glVertex2f(x + 0.85f, y + 1.20f); glVertex2f(x + 0.85f, y + 0.55f);
        glVertex2f(x - 0.95f, y + 0.95f); glVertex2f(x - 2.10f, y + 0.80f);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.30f + i * 0.33f, 1.0f);
        FilledCircle4(x + 2.05f + t * 1.5f, y + 2.25f + t * 0.35f,
                      0.12f + t * 0.20f, 225, 232, 240,
                      (unsigned char)(120 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}
// Traditional horse-drawn passenger sleigh gliding over packed snow
void DrawSled4()
{
    if (!sledActive4) return;
    if (SeasonWinter4() < 0.02f) return;
    float x = sledX4, y = -11.0f;
    float gait = sledX4 * 1.6f;
    DrawHorse4(x + 3.4f, y, gait);
    glColor3ub(112, 72, 46);
    glBegin(GL_QUADS);
        glVertex2f(x-1.3f, y+0.15f);  glVertex2f(x+1.5f, y+0.15f);
        glVertex2f(x+1.5f, y+1.25f);  glVertex2f(x-1.3f, y+1.25f);
    glEnd();
    glColor3ub(140, 92, 58);
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x+1.5f, y+1.25f); glVertex2f(x+1.9f, y+1.15f); glVertex2f(x+2.0f, y+0.70f);
    glEnd();
    glColor3ub(196, 202, 214);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.5f, y+0.05f); glVertex2f(x+1.9f, y+0.05f);
        glVertex2f(x-1.2f, y+0.15f); glVertex2f(x-1.2f, y+0.05f);
        glVertex2f(x+1.3f, y+0.15f); glVertex2f(x+1.3f, y+0.05f);
    glEnd();
    glColor3ub(150, 40, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-1.1f, y+1.25f); glVertex2f(x+1.0f, y+1.25f);
        glVertex2f(x+1.0f, y+1.70f); glVertex2f(x-1.1f, y+1.70f);
    glEnd();
    FilledCircle4(x-0.45f, y+2.00f, 0.30f, 232, 194, 154, 255);
    FilledCircle4(x+0.45f, y+1.95f, 0.27f, 224, 186, 146, 255);
    glColor3ub(40, 40, 50);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.72f, y+2.22f); glVertex2f(x-0.18f, y+2.22f); glVertex2f(x-0.45f, y+2.62f);
    glEnd();
    glColor3ub(90, 62, 38);
    glLineWidth(1.2f);
    glBegin(GL_LINES);
        glVertex2f(x-0.20f, y+1.95f); glVertex2f(x+4.25f, y+1.20f);
    glEnd();
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.5f + i * 0.2f, 1.0f);
        FilledCircle4(x - 1.6f - t * 1.8f, y + 0.08f + t * 0.5f,
                      0.14f * (1.0f - t), 245, 248, 255,
                      (unsigned char)(170 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}
// Timer callback: advances horse and sleigh along the snowy riverside avenue
void UpdateSled4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSled4, 0); return; }
    if (isAnimating4) {
        if (sledActive4) {
            sledX4 += 0.4f;
            if (sledX4 > 70.0f) { sledActive4 = false; sledCooldown4 = 300 + rand() % 200; }
        } else {
            sledCooldown4--;
            if (sledCooldown4 <= 0) { sledActive4 = true; sledX4 = -75.0f; }
        }
    }
    glutTimerFunc(25, UpdateSled4, 0);
}
constexpr int MAX_EMBERS4 = 14;
struct Ember4
{
    float x, y, vy, drift, life; bool active;
};
Ember4 embers4[MAX_EMBERS4];

// Fire pit with animated flames and sparks
void DrawFirePit4()
{
    float flick = 0.72f + 0.28f * sinf(firePhase4 * 3.0f);
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(255, 145, 45, (unsigned char)(95 * flick));
        glVertex2f(fireX4, fireY4);
        glColor4ub(255, 120, 40, 0);
        for (int i = 0; i <= 26; i++) {
            float a = (float)i / 26.0f * 2.0f * PI4;
            glVertex2f(fireX4 + 6.2f * cosf(a), fireY4 + 2.4f * sinf(a));
        }
    glEnd();
    for (int i = -3; i <= 3; i++) {
        float sx = fireX4 + i * 0.45f;
        FilledCircle4(sx, fireY4 - 0.25f, 0.26f, 96, 96, 104, 255);
        FilledCircle4(sx, fireY4 - 0.20f, 0.18f, 122, 122, 130, 255);
    }
    glColor3ub(96, 64, 40);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(fireX4 - 1.0f, fireY4 - 0.05f); glVertex2f(fireX4 + 0.9f, fireY4 + 0.35f);
        glVertex2f(fireX4 + 1.0f, fireY4 - 0.05f); glVertex2f(fireX4 - 0.9f, fireY4 + 0.35f);
    glEnd();
    struct FlameLayer
    {
        float w, h, ph; unsigned char r, g, b, a;
    };
    FlameLayer layers[5] = {
        { 1.05f, 2.35f, 0.0f, 235,  70,  25, 205 },
        { 0.82f, 2.00f, 1.1f, 255, 120,  35, 220 },
        { 0.60f, 1.62f, 2.2f, 255, 170,  55, 232 },
        { 0.40f, 1.20f, 3.3f, 255, 215, 105, 240 },
        { 0.22f, 0.78f, 4.4f, 255, 246, 205, 250 }
    };
    for (int i = 0; i < 5; i++) {
        FlameLayer& L = layers[i];
        float wob = sinf(firePhase4 * 4.0f + L.ph) * 0.16f;
        float h   = L.h * (0.85f + 0.15f * sinf(firePhase4 * 5.0f + L.ph));
        glColor4ub(L.r, L.g, L.b, L.a);
        glBegin(GL_TRIANGLES);
            glVertex2f(fireX4 - L.w, fireY4 + 0.15f);
            glVertex2f(fireX4 + L.w, fireY4 + 0.15f);
            glVertex2f(fireX4 + wob, fireY4 + 0.15f + h);
        glEnd();
    }
    for (int i = 0; i < MAX_EMBERS4; i++) {
        if (!embers4[i].active) continue;
        unsigned char a = (unsigned char)(230 * embers4[i].life);
        FilledCircle4(embers4[i].x, embers4[i].y, 0.09f * embers4[i].life,
                      255, 180, 80, a);
    }
    for (int s = -1; s <= 1; s += 2) {
        float px = fireX4 + s * 2.6f;
        FilledCircle4(px, fireY4 + 1.55f, 0.28f, 228, 190, 150, 255);
        glColor3ub(s < 0 ? 150 : 60, s < 0 ? 50 : 90, s < 0 ? 60 : 140);
        glBegin(GL_QUADS);
            glVertex2f(px - 0.26f, fireY4 + 0.35f); glVertex2f(px + 0.26f, fireY4 + 0.35f);
            glVertex2f(px + 0.22f, fireY4 + 1.30f); glVertex2f(px - 0.22f, fireY4 + 1.30f);
        glEnd();
        glColor3ub(228, 190, 150);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px - s * 0.22f, fireY4 + 1.05f);
            glVertex2f(px - s * 1.05f, fireY4 + 0.80f);
        glEnd();
    }
    glLineWidth(1.0f);
}
// Timer callback: animates crackling fire pit flame tongues and floating upward embers
void UpdateFire4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFire4, 0); return; }
    if (isAnimating4) {
        firePhase4 += 0.15f;
        static int acc = 0;
        if (++acc > 5) {
            acc = 0;
            for (int i = 0; i < MAX_EMBERS4; i++) {
                if (!embers4[i].active) {
                    embers4[i].active = true;
                    embers4[i].x = fireX4 + (rand() % 100 - 50) / 90.0f;
                    embers4[i].y = fireY4 + 1.1f;
                    embers4[i].vy = 0.07f + (rand() % 40) / 1000.0f;
                    embers4[i].drift = (rand() % 100 - 50) / 2600.0f;
                    embers4[i].life = 1.0f;
                    break;
                }
            }
        }
        for (int i = 0; i < MAX_EMBERS4; i++) {
            if (!embers4[i].active) continue;
            embers4[i].y += embers4[i].vy;
            embers4[i].x += embers4[i].drift;
            embers4[i].life -= 0.012f;
            if (embers4[i].life <= 0.0f) embers4[i].active = false;
        }
    }
    glutTimerFunc(30, UpdateFire4, 0);
}
// Wooden alpine Christmas market chalet stall with striped canvas canopy, counter goods, and warm lamp
void DrawStall4(const Stall4& st)
{
    const float x  = st.x;
    const unsigned char ar = st.r, ag = st.g, ab = st.b;
    const char* sign = st.sign;
    const int   goods = st.goods;
    float y = -9.5f;
    BeginDepthSprite(x, y - 2.0f, stallScale4);
    glColor3ub(120, 85, 55);
    glBegin(GL_QUADS);
        glVertex2f(x-2.2f, y-2.0f); glVertex2f(x+2.2f, y-2.0f);
        glVertex2f(x+2.2f, y);      glVertex2f(x-2.2f, y);
    glEnd();
    glColor4ub(86, 60, 38, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float px = x - 2.2f + i * 0.88f;
            glVertex2f(px, y-2.0f); glVertex2f(px, y);
        }
    glEnd();
    float rock = sinf(pedTimer4 * 0.8f + x) * 0.10f;
    glColor3ub(70, 60, 88);
    glLineWidth(5.0f);
    glBegin(GL_LINES); glVertex2f(x - 0.6f + rock, y + 0.1f); glVertex2f(x - 0.6f + rock, y + 1.3f); glEnd();
    FilledCircle4(x - 0.6f + rock, y + 1.65f, 0.32f, 228, 190, 152, 255);
    FilledCircle4(x - 0.6f + rock, y + 1.92f, 0.30f, ar, ag, ab, 255);
    glLineWidth(1.0f);
    for (int i = 0; i < 4; i++) {
        float gx = x - 1.5f + i * 1.0f;
        if (goods == 0) {
            glColor3ub(240, 240, 245);
            glBegin(GL_QUADS);
                glVertex2f(gx-0.20f, y); glVertex2f(gx+0.20f, y);
                glVertex2f(gx+0.20f, y+0.42f); glVertex2f(gx-0.20f, y+0.42f);
            glEnd();
            float t = fmodf(firePhase4 * 0.35f + i * 0.25f, 1.0f);
            FilledCircle4(gx, y + 0.5f + t * 0.9f, 0.10f + t * 0.16f,
                          235, 240, 248, (unsigned char)(120 * (1.0f - t)));
        } else if (goods == 1) {
            glColor3ub(40, 110, 62);
            glLineWidth(3.0f);
            glBegin(GL_LINE_LOOP);
                for (int k = 0; k < 10; k++) {
                    float a = (float)k / 10.0f * 2.0f * PI4;
                    glVertex2f(gx + 0.26f*cosf(a), y + 0.30f + 0.26f*sinf(a));
                }
            glEnd();
            FilledCircle4(gx + 0.20f, y + 0.12f, 0.08f, 210, 60, 60, 255);
            glLineWidth(1.0f);
        } else if (goods == 2) {
            if (i == 0) {
                glColor3ub(48, 46, 52);
                glBegin(GL_QUADS);
                    glVertex2f(x-1.7f, y); glVertex2f(x+1.1f, y);
                    glVertex2f(x+1.1f, y+0.34f); glVertex2f(x-1.7f, y+0.34f);
                glEnd();
                for (int k = 0; k < 7; k++)
                    FilledCircle4(x - 1.5f + k * 0.40f, y + 0.40f, 0.15f, 128, 78, 44, 255);
                float t = fmodf(firePhase4 * 0.3f, 1.0f);
                FilledCircle4(x - 0.2f, y + 0.8f + t * 1.1f, 0.18f + t * 0.22f,
                              228, 214, 200, (unsigned char)(110 * (1.0f - t)));
            }
        } else {
            unsigned char cols[4][3] = { {210,70,80}, {70,140,210}, {230,190,70}, {140,90,190} };
            glColor3ub(cols[i][0], cols[i][1], cols[i][2]);
            glBegin(GL_QUADS);
                glVertex2f(gx-0.26f, y); glVertex2f(gx+0.26f, y);
                glVertex2f(gx+0.26f, y+0.5f); glVertex2f(gx-0.26f, y+0.5f);
            glEnd();
            glColor3ub(250, 250, 250);
            glBegin(GL_LINES);
                glVertex2f(gx, y); glVertex2f(gx, y+0.5f);
            glEnd();
        }
    }
    for (int i = 0; i < 6; i++) {
        float t0 = (float)i / 6.0f, t1 = (float)(i+1) / 6.0f;
        bool pale = (i % 2 == 0);
        glColor3ub(pale ? 246 : ar, pale ? 246 : ag, pale ? 250 : ab);
        glBegin(GL_TRIANGLES);
            glVertex2f(x - 2.6f + 5.2f*t0, y);
            glVertex2f(x - 2.6f + 5.2f*t1, y);
            glVertex2f(x, y + 1.6f);
        glEnd();
    }
    glColor3ub(ar, ag, ab);
    for (int i = 0; i < 7; i++)
        FilledCircle4(x - 2.5f + i * 0.85f, y - 0.05f, 0.22f, ar, ag, ab, 255);
    FilledCircle4(x, y + 1.05f, 0.9f, 255, 210, 140, 50);
    FilledCircle4(x, y + 1.05f, 0.22f, 255, 244, 205, 255);
    EndDepthSprite();
    if (sign != nullptr) {
        float apex  = StallApexY4();
        float signH = 1.70f;
        float ty    = apex - 0.35f;
        float txtW  = TextPixelWidth(GLUT_BITMAP_HELVETICA_12, sign)
                    * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        float half  = txtW * 0.5f + 0.8f;
        float eave   = StallCounterTopY4();
        float postX  = half * 0.5f;
        float slopeY = eave + (apex - eave) * (1.0f - postX / StallHalfW4());
        glColor3ub(78, 58, 42);
        glLineWidth(2.6f);
        glBegin(GL_LINES);
            glVertex2f(x-postX, slopeY); glVertex2f(x-postX, ty+signH*0.5f);
            glVertex2f(x+postX, slopeY); glVertex2f(x+postX, ty+signH*0.5f);
        glEnd();
        glLineWidth(1.0f);
        glColor4ub(255, 198, 112, 48);
        glBegin(GL_QUADS);
            glVertex2f(x-half-1.1f, ty-0.9f);       glVertex2f(x+half+1.1f, ty-0.9f);
            glVertex2f(x+half+1.1f, ty+signH+0.9f); glVertex2f(x-half-1.1f, ty+signH+0.9f);
        glEnd();
        glColor3ub(48, 36, 28);
        glBegin(GL_QUADS);
            glVertex2f(x-half, ty);        glVertex2f(x+half, ty);
            glVertex2f(x+half, ty+signH);  glVertex2f(x-half, ty+signH);
        glEnd();
        glColor3ub(198, 156, 96);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x-half, ty);        glVertex2f(x+half, ty);
            glVertex2f(x+half, ty+signH);  glVertex2f(x-half, ty+signH);
        glEnd();
        glLineWidth(1.0f);
        glColor3ub(255, 230, 176);
        DrawTextCentered(x, ty + signH*0.5f - 0.45f, GLUT_BITMAP_HELVETICA_12, sign);
    }
}
// Line of shivering, bundled-up patrons waiting to buy hot cocoa or roasted snacks
void DrawStallQueue4(float x, int n)
{
    for (int i = 0; i < n; i++) {
        float qx  = x - StallHalfW4() - 1.1f - i * 1.9f;
        float bob = sinf(pedTimer4 * 1.1f + i * 1.7f) * 0.09f;
        float fy  = stallGroundY4 + bob;
        float hipY = fy + 1.0f;
        unsigned char cols[3][3] = { {150, 60, 70}, {50, 80, 130}, {90, 110, 70} };
        unsigned char cr = cols[i%3][0], cg = cols[i%3][1], cb = cols[i%3][2];
        DrawGroundShadow(qx, stallGroundY4, 0.62f, 0.25f, 80);
        glColor3ub(30, 30, 35);
        glLineWidth(2.5f);
        glBegin(GL_LINES);
            glVertex2f(qx, hipY); glVertex2f(qx-0.16f, fy);
            glVertex2f(qx, hipY); glVertex2f(qx+0.16f, fy);
        glEnd();
        glColor3ub(cr, cg, cb);
        glLineWidth(5.0f);
        glBegin(GL_LINES); glVertex2f(qx, hipY); glVertex2f(qx, hipY+1.15f); glEnd();
        FilledCircle4(qx, hipY+1.45f, 0.30f, 226, 188, 150, 255);
        glColor3ub(cr, cg, cb);
        glBegin(GL_TRIANGLES);
            glVertex2f(qx-0.30f, hipY+1.65f); glVertex2f(qx+0.30f, hipY+1.65f);
            glVertex2f(qx,       hipY+2.20f);
        glEnd();
        glColor4ub(246, 249, 255, 230);
        glBegin(GL_TRIANGLES);
            glVertex2f(qx-0.16f*SnowDepth4(), hipY+1.95f);
            glVertex2f(qx+0.16f*SnowDepth4(), hipY+1.95f);
            glVertex2f(qx,                  hipY+2.20f);
        glEnd();
        for (int k = 0; k < 2; k++) {
            float t = fmodf(firePhase4 * 0.25f + k * 0.5f + i * 0.2f, 1.0f);
            FilledCircle4(qx + 0.35f + t * 0.9f, hipY + 1.40f + t * 0.35f,
                          0.10f + t * 0.18f, 226, 234, 244,
                          (unsigned char)(95 * (1.0f - t)));
        }
        glLineWidth(1.0f);
    }
}
// Draw all market stalls
void DrawStalls4()
{
    for (int i = 0; i < NUM_STALLS4; i++) DrawStall4(stalls4[i]);
    DrawStallQueue4(stalls4[1].x, 2);
    DrawStallQueue4(stalls4[3].x, 2);
}

// Decorated Christmas tree with lights and star
void DrawChristmasTree4()
{
    float x = treeX4, baseY = -9.0f;
    glColor3ub(80, 55, 35);
    glBegin(GL_QUADS);
        glVertex2f(x-0.4f, baseY-0.5f); glVertex2f(x+0.4f, baseY-0.5f);
        glVertex2f(x+0.4f, baseY);      glVertex2f(x-0.4f, baseY);
    glEnd();
    for (int tier = 0; tier < 4; tier++) {
        float ty = baseY + tier * 1.8f;
        float tw = 4.2f - tier * 0.85f;
        int lift = tier * 6;
        glColor3ub(MixSB4(30 + lift, 168 + lift), MixSB4(90 + lift, 96 + lift),
                   MixSB4(55 + lift, 44 + lift));
        glBegin(GL_TRIANGLES);
            glVertex2f(x-tw, ty); glVertex2f(x+tw, ty); glVertex2f(x, ty+2.3f);
        glEnd();
    }
    if (SeasonWinter4() > 0.02f) {
        float c = SnowDepth4();
        glColor4ub(246, 249, 255, WinterA4(225));
        for (int tier = 0; tier < 4; tier++) {
            float ty = baseY + tier * 1.8f;
            float tw = (4.2f - tier * 0.85f) * (0.55f + 0.35f * c);
            glBegin(GL_TRIANGLES);
                glVertex2f(x - tw, ty + 0.15f);
                glVertex2f(x + tw, ty + 0.15f);
                glVertex2f(x,      ty + 0.15f + 0.75f * (0.5f + 0.5f * c));
            glEnd();
        }
    }
    FilledCircle4(x, baseY+7.7f, 0.35f, MixSB4(255, 210), MixSB4(220, 170), MixSB4(80, 96), 255);
    float ornX[12] = { -2.5f, 1.8f, -1.0f, 2.6f, -3.2f, 0.5f, -1.6f, 2.0f, -0.6f, 1.2f, -2.0f, 0.2f };
    float ornY[12] = {  0.6f, 1.0f,  1.8f, 2.2f,  2.8f, 3.2f,  3.9f, 4.3f,  4.9f, 5.3f,  5.9f, 6.3f };
    unsigned char warm[3][3] = { {255, 196, 120}, {255, 214, 150}, {255, 176,  96} };
    unsigned char many[3][3] = { {230,  60,  60}, { 60, 140, 230}, {240, 210,  60} };
    for (int i = 0; i < 12; i++) {
        float tw = 0.5f + 0.5f * sinf(twinklePhase4*2.0f + i*0.7f);
        int c = i % 3;
        unsigned char r = multicolorLights4 ? many[c][0] : warm[c][0];
        unsigned char g = multicolorLights4 ? many[c][1] : warm[c][1];
        unsigned char b = multicolorLights4 ? many[c][2] : warm[c][2];
        FilledCircle4(x+ornX[i], baseY+ornY[i], 0.75f, r, g, b, (unsigned char)(60 * tw));
        FilledCircle4(x+ornX[i], baseY+ornY[i], 0.22f, r, g, b, (unsigned char)(150 + 105*tw));
        if (SeasonAutumn4() > 0.02f) {
            unsigned char a = AutumnA4(235);
            glColor4ub(226, 138, 62, a);
            glBegin(GL_QUADS);
                glVertex2f(x+ornX[i]-0.24f, baseY+ornY[i]-0.02f);
                glVertex2f(x+ornX[i]+0.24f, baseY+ornY[i]-0.02f);
                glVertex2f(x+ornX[i]+0.19f, baseY+ornY[i]-0.52f);
                glVertex2f(x+ornX[i]-0.19f, baseY+ornY[i]-0.52f);
            glEnd();
            glColor4ub(150, 82, 36, a);
            glLineWidth(1.0f);
            glBegin(GL_LINES);
                glVertex2f(x+ornX[i], baseY+ornY[i]+0.10f);
                glVertex2f(x+ornX[i], baseY+ornY[i]-0.02f);
            glEnd();
        }
    }
    glLineWidth(1.0f);
}
// Festive wrapped gift present box with decorative ribbon bow knot
void DrawGiftBox4(float x, float y, unsigned char r, unsigned char g, unsigned char b)
{
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-0.4f, y);      glVertex2f(x+0.4f, y);
        glVertex2f(x+0.4f, y+0.7f); glVertex2f(x-0.4f, y+0.7f);
    glEnd();
    glColor3ub(255, 255, 255);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x, y);           glVertex2f(x, y+0.7f);
        glVertex2f(x-0.4f, y+0.35f); glVertex2f(x+0.4f, y+0.35f);
    glEnd();
}
// Gift boxes under the Christmas tree
void DrawGiftBoxes4()
{
    DrawGiftBox4(-2.0f, -9.0f, 200, 60, 70);
    DrawGiftBox4(-1.0f, -9.0f, 60, 140, 200);
    DrawGiftBox4( 2.2f, -9.0f, 220, 180, 60);
}
// Snowman figure
void DrawSnowman4(float x, float scale)
{
    float y = -9.0f;
    FilledCircle4(x, y+0.5f*scale, 0.7f*scale,  250, 250, 255, 255);
    FilledCircle4(x, y+1.5f*scale, 0.5f*scale,  250, 250, 255, 255);
    FilledCircle4(x, y+2.2f*scale, 0.35f*scale, 250, 250, 255, 255);
    glColor3ub(230, 120, 40);
    glBegin(GL_TRIANGLES);
        glVertex2f(x, y+2.2f*scale); glVertex2f(x+0.25f*scale, y+2.18f*scale); glVertex2f(x, y+2.15f*scale);
    glEnd();
    glColor3ub(40, 40, 45);
    FilledCircle4(x-0.1f*scale, y+2.25f*scale, 0.04f*scale, 30, 30, 30, 255);
    FilledCircle4(x+0.1f*scale, y+2.25f*scale, 0.04f*scale, 30, 30, 30, 255);
    FilledCircle4(x, y+1.6f*scale, 0.05f*scale, 40, 40, 45, 255);
    FilledCircle4(x, y+1.4f*scale, 0.05f*scale, 40, 40, 45, 255);
    glColor3ub(200, 50, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-0.35f*scale, y+1.85f*scale); glVertex2f(x+0.35f*scale, y+1.85f*scale);
        glVertex2f(x+0.35f*scale, y+2.0f*scale);  glVertex2f(x-0.35f*scale, y+2.0f*scale);
    glEnd();
    glColor3ub(90, 60, 40);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(x-0.5f*scale, y+1.55f*scale); glVertex2f(x-1.1f*scale, y+1.9f*scale);
        glVertex2f(x+0.5f*scale, y+1.55f*scale); glVertex2f(x+1.1f*scale, y+1.9f*scale);
    glEnd();
}
// Family of whimsical snowmen built on the plaza with coal eyes, carrot noses, and scarves
void DrawSnowmanFamily4()
{
    if (SeasonWinter4() < 0.02f) return;
    DrawSnowman4(20.0f, 1.0f);
    DrawSnowman4(21.6f, 0.6f);
    DrawSnowman4(18.6f, 0.5f);
}
constexpr float SANTA_X4 = 0.85f;
constexpr float SANTA_Y4 = -10.2f;

// Santa Claus figure with sleigh
void DrawSanta4()
{
    if (SeasonWinter4() < 0.02f) return;
    const float x = SANTA_X4, y = SANTA_Y4;
    const float S = 1.05f;
    DrawGroundShadow(x, y, 1.05f * S, 0.30f, 86);
    glColor3ub(26, 26, 30);
    for (int i = -1; i <= 1; i += 2) {
        glBegin(GL_QUADS);
            glVertex2f(x + i*0.10f*S - 0.24f*S, y);
            glVertex2f(x + i*0.10f*S + 0.24f*S, y);
            glVertex2f(x + i*0.10f*S + 0.24f*S, y + 0.42f*S);
            glVertex2f(x + i*0.10f*S - 0.24f*S, y + 0.42f*S);
        glEnd();
    }
    glColor3ub(196, 44, 44);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.52f*S, y + 0.30f*S);
        glVertex2f(x + 0.52f*S, y + 0.30f*S);
        glVertex2f(x + 0.60f*S, y + 0.95f*S);
        glVertex2f(x + 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.60f*S, y + 0.95f*S);
    glEnd();
    glColor4ub(150, 30, 32, 170);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.52f*S, y + 0.30f*S);
        glVertex2f(x - 0.22f*S, y + 0.30f*S);
        glVertex2f(x - 0.20f*S, y + 1.62f*S);
        glVertex2f(x - 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.60f*S, y + 0.95f*S);
    glEnd();
    glColor3ub(248, 248, 250);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.56f*S, y + 0.28f*S); glVertex2f(x + 0.56f*S, y + 0.28f*S);
        glVertex2f(x + 0.56f*S, y + 0.50f*S); glVertex2f(x - 0.56f*S, y + 0.50f*S);
    glEnd();
    glColor3ub(30, 28, 32);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.60f*S, y + 0.86f*S); glVertex2f(x + 0.60f*S, y + 0.86f*S);
        glVertex2f(x + 0.60f*S, y + 1.10f*S); glVertex2f(x - 0.60f*S, y + 1.10f*S);
    glEnd();
    glColor3ub(226, 188, 84);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.17f*S, y + 0.88f*S); glVertex2f(x + 0.17f*S, y + 0.88f*S);
        glVertex2f(x + 0.17f*S, y + 1.08f*S); glVertex2f(x - 0.17f*S, y + 1.08f*S);
    glEnd();
    glColor3ub(196, 44, 44);
    glLineWidth(5.4f * S);
    float wave = sinf(twinklePhase4 * 1.6f) * 0.16f;
    glBegin(GL_LINES);
        glVertex2f(x - 0.48f*S, y + 1.46f*S); glVertex2f(x - 0.88f*S, y + 0.86f*S);
        glVertex2f(x + 0.48f*S, y + 1.46f*S);
        glVertex2f(x + 1.00f*S, y + (2.06f + wave)*S);
    glEnd();
    glColor3ub(248, 248, 250);
    FilledCircle4(x - 0.92f*S, y + 0.80f*S, 0.17f*S, 248, 248, 250, 255);
    FilledCircle4(x + 1.04f*S, y + (2.12f + wave)*S, 0.17f*S, 248, 248, 250, 255);
    FilledCircle4(x, y + 1.92f*S, 0.34f*S, 244, 202, 166, 255);
    glColor3ub(250, 250, 252);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.36f*S, y + 1.96f*S);
        glVertex2f(x + 0.36f*S, y + 1.96f*S);
        glVertex2f(x + 0.24f*S, y + 1.58f*S);
        glVertex2f(x,           y + 1.40f*S);
        glVertex2f(x - 0.24f*S, y + 1.58f*S);
    glEnd();
    FilledCircle4(x - 0.15f*S, y + 1.99f*S, 0.11f*S, 250, 250, 252, 255);
    FilledCircle4(x + 0.15f*S, y + 1.99f*S, 0.11f*S, 250, 250, 252, 255);
    FilledCircle4(x,           y + 2.02f*S, 0.09f*S, 232, 158, 132, 255);
    glColor3ub(40, 34, 34);
    FilledCircle4(x - 0.13f*S, y + 2.12f*S, 0.045f*S, 40, 34, 34, 255);
    FilledCircle4(x + 0.13f*S, y + 2.12f*S, 0.045f*S, 40, 34, 34, 255);
    glColor3ub(196, 44, 44);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.36f*S, y + 2.20f*S);
        glVertex2f(x + 0.36f*S, y + 2.20f*S);
        glVertex2f(x + 0.52f*S, y + 2.72f*S);
        glVertex2f(x + 0.46f*S, y + 2.94f*S);
    glEnd();
    glColor3ub(248, 248, 250);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.40f*S, y + 2.16f*S); glVertex2f(x + 0.40f*S, y + 2.16f*S);
        glVertex2f(x + 0.40f*S, y + 2.34f*S); glVertex2f(x - 0.40f*S, y + 2.34f*S);
    glEnd();
    FilledCircle4(x + 0.50f*S, y + 2.98f*S, 0.16f*S, 248, 248, 250, 255);
    glColor3ub(146, 104, 62);
    glBegin(GL_POLYGON);
        glVertex2f(x - 1.62f*S, y + 0.02f*S);
        glVertex2f(x - 0.72f*S, y + 0.02f*S);
        glVertex2f(x - 0.66f*S, y + 0.92f*S);
        glVertex2f(x - 1.10f*S, y + 1.24f*S);
        glVertex2f(x - 1.70f*S, y + 0.86f*S);
    glEnd();
    glColor3ub(110, 76, 44);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 1.44f*S, y + 1.00f*S); glVertex2f(x - 0.84f*S, y + 0.90f*S);
    glEnd();
    glColor3ub(214, 78, 88);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.36f*S, y + 1.06f*S); glVertex2f(x - 1.02f*S, y + 1.06f*S);
        glVertex2f(x - 1.02f*S, y + 1.36f*S); glVertex2f(x - 1.36f*S, y + 1.36f*S);
    glEnd();
    glColor3ub(226, 188, 84);
    glBegin(GL_LINES);
        glVertex2f(x - 1.19f*S, y + 1.06f*S); glVertex2f(x - 1.19f*S, y + 1.36f*S);
    glEnd();
    glLineWidth(1.0f);
}

// Ice sculpture display
void DrawIceSculpture4(float x, float scale)
{
    float y = -9.0f;
    glColor4ub(150, 210, 235, 220);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor3ub(90, 160, 200);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor4ub(210, 235, 250, 180);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.3f*scale, y+0.3f); glVertex2f(x+0.1f*scale, y+0.3f); glVertex2f(x-0.1f*scale, y+1.3f*scale);
    glEnd();
    FilledCircle4(x+0.15f*scale, y+1.3f*scale, 0.08f*scale, 255, 255, 255, 240);
}
// Renders illuminated carved ice crystal sculptures exhibited in the plaza
void DrawIceSculptures4()
{
    if (SeasonWinter4() < 0.02f) return;
    DrawIceSculpture4(13.0f, 1.0f);
    DrawIceSculpture4(17.5f, 0.8f);
}
// Translucent carved ice statue with internal color refraction and light glints
void DrawIceSculptureAt(float x, float baseY, float scale)
{
    float y = baseY;
    glColor4ub(150, 210, 235, 220);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor3ub(90, 160, 200);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x-0.6f*scale, y); glVertex2f(x+0.6f*scale, y); glVertex2f(x, y+2.0f*scale);
    glEnd();
    glColor4ub(210, 235, 250, 180);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.3f*scale, y+0.3f); glVertex2f(x+0.1f*scale, y+0.3f); glVertex2f(x-0.1f*scale, y+1.3f*scale);
    glEnd();
    FilledCircle4(x+0.15f*scale, y+1.3f*scale, 0.08f*scale, 255, 255, 255, 240);
}

// Performance stage
void DrawStage4()
{
    float x = -56.5f, y = -9.0f;
    glColor3ub(110, 80, 55);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f, y-0.5f); glVertex2f(x+3.0f, y-0.5f);
        glVertex2f(x+3.0f, y);      glVertex2f(x-3.0f, y);
    glEnd();
    unsigned char cols[3][3] = { {140, 50, 60}, {50, 90, 140}, {60, 120, 70} };
    for (int i = 0; i < 3; i++) {
        float px = x - 1.6f + i * 1.6f;
        FilledCircle4(px, y+1.1f, 0.26f, 225, 185, 145, 255);
        glColor3ub(cols[i][0], cols[i][1], cols[i][2]);
        glBegin(GL_QUADS);
            glVertex2f(px-0.22f, y+0.1f); glVertex2f(px+0.22f, y+0.1f);
            glVertex2f(px+0.2f, y+0.85f); glVertex2f(px-0.2f, y+0.85f);
        glEnd();
    }
    glColor3ub(120, 80, 40);
    FilledCircle4(x, y+0.5f, 0.25f, 120, 80, 40, 255);
    glColor3ub(40, 40, 45);
    glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(x-1.6f, y-0.1f); glVertex2f(x-1.6f, y+0.9f); glEnd();
    FilledCircle4(x-1.6f, y+0.95f, 0.1f, 60, 60, 65, 255);
}
// Decorative nutcracker statue
void DrawNutcracker4(float x)
{
    float y = -10.4f;
    glColor3ub(20, 20, 25);
    glBegin(GL_QUADS); glVertex2f(x-0.4f, y); glVertex2f(x+0.4f, y); glVertex2f(x+0.4f, y+0.3f); glVertex2f(x-0.4f, y+0.3f); glEnd();
    glColor3ub(240, 240, 235);
    glBegin(GL_QUADS); glVertex2f(x-0.35f, y+0.3f); glVertex2f(x+0.35f, y+0.3f); glVertex2f(x+0.35f, y+0.9f); glVertex2f(x-0.35f, y+0.9f); glEnd();
    glColor3ub(180, 30, 40);
    glBegin(GL_QUADS); glVertex2f(x-0.4f, y+0.9f); glVertex2f(x+0.4f, y+0.9f); glVertex2f(x+0.38f, y+1.9f); glVertex2f(x-0.38f, y+1.9f); glEnd();
    glColor3ub(230, 200, 80);
    for (int i = 0; i < 3; i++) FilledCircle4(x, y+1.1f+i*0.25f, 0.05f, 230, 200, 80, 255);
    glColor3ub(30, 40, 90);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(x-0.4f, y+1.7f); glVertex2f(x-0.65f, y+1.2f);
        glVertex2f(x+0.4f, y+1.7f); glVertex2f(x+0.65f, y+1.2f);
    glEnd();
    FilledCircle4(x, y+2.2f, 0.32f, 240, 200, 170, 255);
    glColor3ub(20, 20, 25);
    glBegin(GL_QUADS); glVertex2f(x-0.32f, y+2.4f); glVertex2f(x+0.32f, y+2.4f); glVertex2f(x+0.32f, y+2.9f); glVertex2f(x-0.32f, y+2.9f); glEnd();
    glColor3ub(240, 240, 235);
    glBegin(GL_QUADS); glVertex2f(x-0.35f, y+2.35f); glVertex2f(x+0.35f, y+2.35f); glVertex2f(x+0.35f, y+2.5f); glVertex2f(x-0.35f, y+2.5f); glEnd();
    glColor3ub(30, 30, 30);
    glBegin(GL_QUADS); glVertex2f(x-0.25f, y+2.1f); glVertex2f(x+0.25f, y+2.1f); glVertex2f(x+0.25f, y+2.18f); glVertex2f(x-0.25f, y+2.18f); glEnd();
}

// Clock tower with swinging pendulum
void DrawClockTower4()
{
    float x = clockX4, baseY = -6.0f, topY = 21.0f;
    glColor3ub(60, 55, 65);
    glBegin(GL_QUADS);
        glVertex2f(x-1.5f, baseY); glVertex2f(x+1.5f, baseY);
        glVertex2f(x+1.5f, topY);  glVertex2f(x-1.5f, topY);
    glEnd();
    FilledCircle4(x, topY-2.0f, 2.8f, 255, 226, 170, 34);
    FilledCircle4(x, topY-2.0f, 1.3f, 250, 242, 214, 255);
    glColor3ub(70, 70, 78);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int t = 0; t < 12; t++) {
            float a = PI4 * 0.5f - t * (2.0f * PI4 / 12.0f);
            float inner = (t % 3 == 0) ? 0.92f : 1.05f;
            glVertex2f(x + 1.22f*cosf(a), (topY-2.0f) + 1.22f*sinf(a));
            glVertex2f(x + inner*cosf(a), (topY-2.0f) + inner*sinf(a));
        }
    glEnd();
    time_t raw = time(nullptr);
    struct tm* lt = localtime(&raw);
    float secs  = lt ? (float)lt->tm_sec  : 0.0f;
    float mins  = lt ? (float)lt->tm_min + secs / 60.0f      : 0.0f;
    float hours = lt ? (float)(lt->tm_hour % 12) + mins / 60.0f : 0.0f;
    float secA  = PI4 * 0.5f - secs  * (2.0f * PI4 / 60.0f);
    float minA  = PI4 * 0.5f - mins  * (2.0f * PI4 / 60.0f);
    float hourA = PI4 * 0.5f - hours * (2.0f * PI4 / 12.0f);
    glColor3ub(40, 40, 45);
    glLineWidth(2.6f);
    glBegin(GL_LINES);
        glVertex2f(x, topY-2.0f); glVertex2f(x + 0.60f*cosf(hourA), (topY-2.0f) + 0.60f*sinf(hourA));
    glEnd();
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(x, topY-2.0f); glVertex2f(x + 0.95f*cosf(minA), (topY-2.0f) + 0.95f*sinf(minA));
    glEnd();
    glColor3ub(170, 50, 55);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.18f*cosf(secA), (topY-2.0f) - 0.18f*sinf(secA));
        glVertex2f(x + 1.05f*cosf(secA), (topY-2.0f) + 1.05f*sinf(secA));
    glEnd();
    FilledCircle4(x, topY-2.0f, 0.09f, 40, 40, 45, 255);
    glColor3ub(150, 40, 50);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-2.0f, topY); glVertex2f(x+2.0f, topY); glVertex2f(x, topY+2.5f);
    glEnd();
    glColor3ub(20, 18, 22);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, baseY+1.0f); glVertex2f(x+0.5f, baseY+1.0f);
        glVertex2f(x+0.5f, topY-3.5f);  glVertex2f(x-0.5f, topY-3.5f);
    glEnd();
    float swing = sinf(pendulumAngle4) * 0.35f;
    float pivotX = x, pivotY = topY - 3.5f;
    float bobX = pivotX + swing * 3.0f;
    float bobY = pivotY - 2.5f;
    glColor3ub(180, 170, 120);
    glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(pivotX, pivotY); glVertex2f(bobX, bobY); glEnd();
    FilledCircle4(bobX, bobY, 0.3f, 200, 180, 90, 255);
}
// Timer callback: swings town hall clock tower pendulum back and forth
void UpdatePendulum4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdatePendulum4, 0); return; }
    if (isAnimating4) pendulumAngle4 += 0.05f;
    glutTimerFunc(20, UpdatePendulum4, 0);
}
// Carolers singing group
void DrawCarolers4()
{
    float baseX = 45.5f, y = -9.5f;
    unsigned char coatCols[3][3] = { {150, 40, 60}, {40, 90, 140}, {90, 140, 60} };
    for (int i = 0; i < 3; i++) {
        float x = baseX + (i-1) * 0.9f;
        FilledCircle4(x, y+1.4f, 0.28f, 225, 185, 145, 255);
        glColor3ub(coatCols[i][0], coatCols[i][1], coatCols[i][2]);
        glBegin(GL_QUADS);
            glVertex2f(x-0.25f, y+0.3f); glVertex2f(x+0.25f, y+0.3f);
            glVertex2f(x+0.22f, y+1.15f); glVertex2f(x-0.22f, y+1.15f);
        glEnd();
    }
    const float noteCeil = -7.7f;
    for (int i = 0; i < 3; i++) {
        float t = fmodf(twinklePhase4*3.0f + i*0.7f, 2.0f);
        float nx = baseX - 0.5f + i*0.6f + sinf(t*3.0f)*0.3f;
        float ny = y + 1.9f + t*0.55f;
        if (ny > noteCeil) ny = noteCeil;
        unsigned char alpha = (unsigned char)(200 * (1.0f - t/2.0f));
        FilledCircle4(nx, ny, 0.12f, 255, 255, 255, alpha);
        glColor4ub(255, 255, 255, alpha);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(nx + 0.11f, ny); glVertex2f(nx + 0.11f, ny + 0.34f);
        glEnd();
        glLineWidth(1.0f);
    }
}
// Firework burst display
void DrawFireworks4()
{
    for (int i = 0; i < MAX_FIREWORKS4; i++) {
        const Firework4& f = fireworks4[i];
        if (f.state == FW_INACTIVE) continue;
        if (f.state == FW_RISING) {
            FilledCircle4(f.x, f.y, 0.18f, 255, 240, 200, 255);
            for (int t = 1; t <= 5; t++) {
                FilledCircle4(f.x, f.y - t * 0.5f, 0.13f - t * 0.02f,
                              255, 190, 110, (unsigned char)(150 - t * 28));
            }
            continue;
        }
        float t = f.phase;
        for (int s = 0; s < 18; s++) {
            float d  = f.sparkSpd[s] * t * 7.0f;
            float px = f.x + d * cosf(f.sparkAng[s]);
            float py = f.y + d * sinf(f.sparkAng[s]) - t * t * 5.0f;
            unsigned char a = (unsigned char)(255 * (1.0f - t));
            FilledCircle4(px, py, 0.16f * (1.0f - t * 0.5f), f.r, f.g, f.b, a);
        }
        if (t < 0.25f) {
            FilledCircle4(f.x, f.y, 2.2f * t / 0.25f, f.r, f.g, f.b,
                          (unsigned char)(90 * (1.0f - t / 0.25f)));
        }
    }
}
// Instantaneous radial flash brightening snow ground during overhead firework explosions
void DrawFireworkGroundFlash4()
{
    float glow = 0.0f;
    unsigned char gr = 255, gg = 255, gb = 255;
    for (int i = 0; i < MAX_FIREWORKS4; i++) {
        if (fireworks4[i].state != FW_BURSTING) continue;
        float g = (1.0f - fireworks4[i].phase) * 0.6f;
        if (g > glow) { glow = g; gr = fireworks4[i].r; gg = fireworks4[i].g; gb = fireworks4[i].b; }
    }
    if (glow <= 0.01f) return;
    glColor4ub(gr, gg, gb, (unsigned char)(38 * glow));
    glBegin(GL_QUADS);
        glVertex2f(-60, -40); glVertex2f(60, -40);
        glVertex2f(60, -6);   glVertex2f(-60, -6);
    glEnd();
}
// Timer callback: launches rocket shells, handles explosion timing, and sparkles falling spark trails
void UpdateFireworks4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFireworks4, 0); return; }
    if (isAnimating4) {
        fireworkCooldown4--;
        if (fireworkCooldown4 <= 0) {
            for (int i = 0; i < MAX_FIREWORKS4; i++) {
                if (fireworks4[i].state == FW_INACTIVE) {
                    Firework4& f = fireworks4[i];
                    f.state   = FW_RISING;
                    f.x       = -45.0f + (rand() % 900) / 10.0f;
                    f.y       = -5.0f;
                    f.targetY = 20.0f + (rand() % 120) / 10.0f;
                    f.phase   = 0.0f;
                    unsigned char cols[4][3] = { {230,60,60}, {60,200,230}, {230,210,60}, {210,110,235} };
                    int c = rand() % 4;
                    f.r = cols[c][0]; f.g = cols[c][1]; f.b = cols[c][2];
                    for (int s = 0; s < 18; s++) {
                        f.sparkAng[s] = (float)s / 18.0f * 2.0f * PI4
                                      + (rand() % 40 - 20) / 300.0f;
                        f.sparkSpd[s] = 0.75f + (rand() % 50) / 100.0f;
                    }
                    break;
                }
            }
            fireworkCooldown4 = 180 + rand() % 260;
        }
        for (int i = 0; i < MAX_FIREWORKS4; i++) {
            Firework4& f = fireworks4[i];
            if (f.state == FW_RISING) {
                f.y += 0.55f;
                if (f.y >= f.targetY) { f.state = FW_BURSTING; f.phase = 0.0f; }
            } else if (f.state == FW_BURSTING) {
                f.phase += 0.018f;
                if (f.phase >= 1.0f) f.state = FW_INACTIVE;
            }
        }
    }
    glutTimerFunc(30, UpdateFireworks4, 0);
}
// Aurora borealis shimmer effect
void DrawAurora4()
{
    if (!auroraActive4) return;
    float envelope = sinf(auroraTimer4 * PI4);
    if (envelope <= 0.0f) return;
    envelope *= 0.18f + 0.82f * NightT4();
    const unsigned char cols[4][3] = {
        { 56, 214, 142 }, { 74, 202, 188 }, { 82, 142, 230 }, { 168, 104, 220 }
    };
    for (int band = 0; band < 4; band++) {
        float baseY = 28.5f + band * 2.7f;
        float h     = 5.4f + band * 1.2f;
        for (int shell = 0; shell < 3; shell++) {
            float spread = 1.0f + shell * 1.05f;
            float aMul   = (shell == 0) ? 1.0f : 0.34f / (float)shell;
            glBegin(GL_QUAD_STRIP);
            for (int i = 0; i <= 30; i++) {
                float x    = -62.0f + i * 4.2f;
                float wave = sinf(auroraPhase4 + i * 0.30f + band * 1.45f) * 2.7f
                           + sinf(auroraPhase4 * 0.55f + i * 0.12f) * 1.5f;
                float yc   = baseY + wave;
                float ray  = 0.45f + 0.55f * (0.5f + 0.5f * sinf(auroraPhase4 * 1.6f + i * 0.85f + band));
                unsigned char a = (unsigned char)(66.0f * envelope * aMul * ray);
                glColor4ub(cols[band][0], cols[band][1], cols[band][2], a);
                glVertex2f(x, yc + h * spread * 0.55f);
                glColor4ub(cols[band][0], cols[band][1], cols[band][2], (unsigned char)(a * 0.22f));
                glVertex2f(x, yc - h * spread * 0.65f);
            }
            glEnd();
        }
    }
    glBegin(GL_QUADS);
        glColor4ub(70, 190, 150, (unsigned char)(26.0f * envelope));
        glVertex2f(-60.0f, 30.0f); glVertex2f(60.0f, 30.0f);
        glColor4ub(70, 190, 150, 0);
        glVertex2f( 60.0f,  8.0f); glVertex2f(-60.0f, 8.0f);
    glEnd();
}
// Timer callback: animates flowing, shimmering waves of northern lights curtains
void UpdateAurora4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateAurora4, 0); return; }
    if (isAnimating4) {
        auroraPhase4 += 0.02f;
        if (auroraActive4) {
            auroraTimer4 += 0.006f;
            if (auroraTimer4 >= 1.0f) { auroraActive4 = false; auroraCooldown4 = 500 + rand() % 600; }
        } else {
            auroraCooldown4--;
            if (auroraCooldown4 <= 0) { auroraActive4 = true; auroraTimer4 = 0.0f; }
        }
    }
    glutTimerFunc(30, UpdateAurora4, 0);
}
// Emits fresh steam clouds rising from hot cider and mulled wine kettles
void SpawnStallSteam4()
{
    for (int i = 0; i < MAX_STEAM4; i++) {
        if (!stallSteam4[i].active) {
            stallSteam4[i].active = true;
            stallSteam4[i].x = stalls4[0].x + (rand()%40-20)/100.0f;
            stallSteam4[i].y = StallCounterTopY4() + 0.4f;
            stallSteam4[i].vy = 0.04f + (rand()%20)/1000.0f;
            stallSteam4[i].alpha = 130.0f;
            stallSteam4[i].size = 0.35f + (rand()%15)/100.0f;
            return;
        }
    }
}
// Renders rising steam clouds from hot food and beverage market stalls
void DrawStallSteam4()
{
    for (int i = 0; i < MAX_STEAM4; i++) {
        if (!stallSteam4[i].active) continue;
        FilledCircle4(stallSteam4[i].x, stallSteam4[i].y, stallSteam4[i].size, 220, 220, 225, (unsigned char)stallSteam4[i].alpha);
    }
}
// Timer callback: updates upward buoyancy and dissipation of food stall steam
void UpdateStallSteam4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateStallSteam4, 0); return; }
    if (isAnimating4) {
        static int acc = 0;
        acc++;
        if (acc > 8) { SpawnStallSteam4(); acc = 0; }
        for (int i = 0; i < MAX_STEAM4; i++) {
            if (!stallSteam4[i].active) continue;
            stallSteam4[i].y += stallSteam4[i].vy;
            stallSteam4[i].size += 0.008f;
            stallSteam4[i].alpha -= 1.4f;
            if (stallSteam4[i].alpha <= 0) stallSteam4[i].active = false;
        }
    }
    glutTimerFunc(35, UpdateStallSteam4, 0);
}
// Spawns billows of warm woodsmoke from rooftop chimneys
void SpawnChimneySmoke4()
{
    for (int i = 0; i < MAX_SMOKE4; i++) {
        if (!chimneySmoke4[i].active) {
            chimneySmoke4[i].active = true;
            chimneySmoke4[i].x = ChimneyX4() + (rand()%30-15)/100.0f;
            chimneySmoke4[i].y = ShopRoofY4(shops4[1], ChimneyX4()) + 3.4f;
            chimneySmoke4[i].vy = 0.03f + (rand()%15)/1000.0f;
            chimneySmoke4[i].alpha = 110.0f;
            chimneySmoke4[i].size = 0.4f + (rand()%15)/100.0f;
            return;
        }
    }
}
// Renders curling smoke clouds drifting from chimneys into the winter night
void DrawChimneySmoke4()
{
    for (int i = 0; i < MAX_SMOKE4; i++) {
        if (!chimneySmoke4[i].active) continue;
        FilledCircle4(chimneySmoke4[i].x, chimneySmoke4[i].y, chimneySmoke4[i].size, 180, 180, 190, (unsigned char)chimneySmoke4[i].alpha);
    }
}
// Timer callback: moves chimney smoke upward with wind drift and fading alpha
void UpdateChimneySmoke4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateChimneySmoke4, 0); return; }
    if (isAnimating4) {
        static int acc = 0;
        acc++;
        if (acc > 10) { SpawnChimneySmoke4(); acc = 0; }
        for (int i = 0; i < MAX_SMOKE4; i++) {
            if (!chimneySmoke4[i].active) continue;
            chimneySmoke4[i].y += chimneySmoke4[i].vy;
            chimneySmoke4[i].x += 0.008f;
            chimneySmoke4[i].size += 0.006f;
            chimneySmoke4[i].alpha -= 1.0f;
            if (chimneySmoke4[i].alpha <= 0) chimneySmoke4[i].active = false;
        }
    }
    glutTimerFunc(35, UpdateChimneySmoke4, 0);
}
// Snow piling up on rooftops and surfaces
void DrawSnowAccumulation4()
{
    if (SeasonWinter4() < 0.02f) return;
    float c = SnowDepth4();
    glColor3ub(246, 249, 255);
    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        float topY = -6.0f + sh.h;
        float th   = 0.35f + 1.05f * c;
        glBegin(GL_QUADS);
            glVertex2f(sh.x - sh.w*0.5f - 0.5f, topY);
            glVertex2f(sh.x + sh.w*0.5f + 0.5f, topY);
            glVertex2f(sh.x + sh.w*0.5f + 0.2f, topY + th);
            glVertex2f(sh.x - sh.w*0.5f - 0.2f, topY + th);
        glEnd();
        if (c > 0.4f) {
            int n = 3 + (int)(c * 4);
            for (int k = 0; k < n; k++) {
                float ix = sh.x - sh.w*0.42f + k * (sh.w * 0.84f / (n - 1));
                float len = 0.30f + 0.55f * c * (0.6f + 0.4f * sinf(ix));
                glBegin(GL_TRIANGLES);
                    glVertex2f(ix - 0.10f, topY);
                    glVertex2f(ix + 0.10f, topY);
                    glVertex2f(ix,         topY - len);
                glEnd();
            }
        }
    }
    for (int i = 0; i < NUM_STALLS4; i++) {
        float sx   = stalls4[i].x;
        float half = StallHalfW4(), eave = StallCounterTopY4(), apex = StallApexY4();
        float th   = 0.30f + 0.6f * c;
        glBegin(GL_QUADS);
            glVertex2f(sx-half, eave);      glVertex2f(sx, apex);
            glVertex2f(sx,      apex+th);   glVertex2f(sx-half, eave+th);
            glVertex2f(sx+half, eave);      glVertex2f(sx, apex);
            glVertex2f(sx,      apex+th);   glVertex2f(sx+half, eave+th);
        glEnd();
    }
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, -6.0f);
        glVertex2f(-46.0f, -6.0f);
        glVertex2f(-48.0f, -6.0f + 1.2f * c);
        glVertex2f(-60.0f, -6.0f + 1.9f * c);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(60.0f, -6.0f);
        glVertex2f(46.0f, -6.0f);
        glVertex2f(48.0f, -6.0f + 1.2f * c);
        glVertex2f(60.0f, -6.0f + 1.9f * c);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(clockX4 - 1.5f * c, 16.4f);
        glVertex2f(clockX4 + 1.5f * c, 16.4f);
        glVertex2f(clockX4,            16.4f + 1.2f * c);
    glEnd();
}
// Autumn-season variant: flowing park fountain surrounded by fallen golden foliage
void DrawFountainAutumn4()
{
    if (SeasonAutumn4() < 0.02f) return;
    const unsigned char A = AutumnA4(255);
    const float cx = rinkCX4, cy = rinkCY4;
    glColor4ub(MixB4(74, 108), MixB4(80, 100), MixB4(72, 86), AutumnA4(120));
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 7.4f*cosf(a), cy + 2.7f*sinf(a));
        }
    glEnd();
    glColor4ub(MixB4(112, 156), MixB4(110, 150), MixB4(104, 138), A);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 6.0f*cosf(a), cy + 2.2f*sinf(a));
        }
    glEnd();
    glColor4ub(MixB4(30, 58), MixB4(62, 104), MixB4(74, 118), A);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 5.1f*cosf(a), cy + 1.75f*sinf(a));
        }
    glEnd();
    glColor4ub(MixB4(86, 124), MixB4(84, 118), MixB4(80, 108), A);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 20; i++) {
            float a = (float)i / 20 * 2 * PI4;
            glVertex2f(cx + 5.1f*cosf(a), cy + 1.75f*sinf(a));
            glVertex2f(cx + 6.0f*cosf(a), cy + 2.2f*sinf(a));
        }
    glEnd();
    glColor4ub(MixB4(104, 148), MixB4(102, 142), MixB4(96, 130), A);
    glBegin(GL_QUADS);
        glVertex2f(cx - 0.7f, cy);        glVertex2f(cx + 0.7f, cy);
        glVertex2f(cx + 0.5f, cy + 2.1f); glVertex2f(cx - 0.5f, cy + 2.1f);
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy + 2.3f);
        for (int i = 0; i <= 16; i++) {
            float a = PI4 + PI4 * ((float)i / 16.0f);
            glVertex2f(cx + 1.7f*cosf(a), cy + 2.3f + 0.6f*sinf(a));
        }
    glEnd();
    for (int j = 0; j < 7; j++) {
        float spread = -1.0f + 2.0f * (float)j / 6.0f;
        float vx = spread * 3.2f, vy = 3.3f - fabsf(spread) * 0.9f;
        glColor4ub(214, 236, 248, AutumnA4(180));
        glLineWidth(1.5f);
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 12; k++) {
                float t  = k / 12.0f * 0.55f;
                float px = cx + vx * t;
                float py = cy + 2.5f + vy * t - 0.5f * 22.0f * t * t;
                if (py < cy + 0.1f) break;
                glVertex2f(px, py);
            }
        glEnd();
        float dt = fmodf(firePhase4 * 0.5f + j * 0.14f, 0.55f);
        float dx = cx + vx * dt;
        float dy = cy + 2.5f + vy * dt - 0.5f * 22.0f * dt * dt;
        if (dy > cy + 0.1f) FilledCircle4(dx, dy, 0.13f, 236, 248, 255, AutumnA4(220));
    }
    for (int r = 0; r < 3; r++) {
        float t   = fmodf(firePhase4 * 0.30f + r * 0.33f, 1.0f);
        float rad = 1.0f + t * 3.6f;
        glColor4ub(228, 244, 252, AutumnA4((unsigned char)(130 * (1.0f - t))));
        glLineWidth(1.2f);
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 20; i++) {
                float a = (float)i / 20 * 2 * PI4;
                glVertex2f(cx + rad*cosf(a), cy + rad*0.34f*sinf(a));
            }
        glEnd();
    }
    for (int i = 0; i < 2; i++) {
        float sx = cx + (i == 0 ? -4.2f : 3.6f);
        float sy = cy + (i == 0 ? -1.2f : -1.5f);
        unsigned char cr = (i == 0) ? 132 : 62, cg = (i == 0) ? 58 : 78, cb = (i == 0) ? 60 : 116;
        glColor4ub(cr, cg, cb, A);
        glBegin(GL_QUADS);
            glVertex2f(sx - 0.42f, sy);        glVertex2f(sx + 0.42f, sy);
            glVertex2f(sx + 0.36f, sy + 1.24f); glVertex2f(sx - 0.36f, sy + 1.24f);
        glEnd();
        FilledCircle4(sx, sy + 1.56f, 0.30f, 226, 188, 152, A);
        FilledCircle4(sx, sy + 1.74f, 0.27f, cr, cg, cb, A);
    }
    glLineWidth(1.0f);
}
// Decorative stone urn planter filled with winter evergreen boughs or autumn mums
void DrawPlanter4(float x, float scale)
{
    const unsigned char A = AutumnA4(255);
    const float y = -9.0f;
    DrawGroundShadow(x, y, 1.5f * scale, 0.28f, AutumnA4(80));
    glColor4ub(MixB4(120, 176), MixB4(62, 96), MixB4(44, 68), A);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.05f*scale, y);
        glVertex2f(x + 1.05f*scale, y);
        glVertex2f(x + 1.28f*scale, y + 1.55f*scale);
        glVertex2f(x - 1.28f*scale, y + 1.55f*scale);
    glEnd();
    glColor4ub(MixB4(142, 198), MixB4(76, 112), MixB4(54, 80), A);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.34f*scale, y + 1.48f*scale);
        glVertex2f(x + 1.34f*scale, y + 1.48f*scale);
        glVertex2f(x + 1.34f*scale, y + 1.80f*scale);
        glVertex2f(x - 1.34f*scale, y + 1.80f*scale);
    glEnd();
    const unsigned char blooms[4][3] = {
        {198, 76, 52}, {226, 150, 46}, {160, 54, 62}, {212, 186, 70}
    };
    for (int i = 0; i < 9; i++) {
        float h = sinf((x * 7.0f + i) * 12.9898f) * 43758.5453f;
        float j = h - floorf(h);
        float bx = x + (-1.05f + 0.26f * i) * scale;
        float by = y + (1.85f + 0.55f * j) * scale;
        glColor4ub(54, 92, 48, A);
        glLineWidth(1.4f);
        glBegin(GL_LINES);
            glVertex2f(bx, y + 1.70f*scale); glVertex2f(bx, by);
        glEnd();
        const unsigned char* c = blooms[i % 4];
        FilledCircle4(bx, by, 0.30f * scale, c[0], c[1], c[2], A);
    }
    glLineWidth(1.0f);
}
// Renders row of ornamental planters across the plaza in autumn palette
void DrawPlantersAutumn4()
{
    if (SeasonAutumn4() < 0.02f) return;
    DrawPlanter4(20.0f, 1.0f);
    DrawPlanter4(21.9f, 0.7f);
    DrawPlanter4(18.4f, 0.6f);
    const float px = 23.8f, py = -9.0f;
    const unsigned char A = AutumnA4(255);
    DrawGroundShadow(px, py, 1.3f, 0.26f, AutumnA4(80));
    for (int i = 0; i < 3; i++) {
        float r = 0.86f - i * 0.22f;
        float cy = py + r + i * 1.20f;
        glColor4ub(MixB4(154, 226), MixB4(78, 128), MixB4(30, 46), A);
        FilledCircle4(px, cy, r, MixB4(154, 226), MixB4(78, 128), MixB4(30, 46), A);
        glColor4ub(MixB4(120, 186), MixB4(58, 100), MixB4(22, 34), A);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(px - r*0.45f, cy - r*0.55f); glVertex2f(px - r*0.45f, cy + r*0.55f);
            glVertex2f(px + r*0.45f, cy - r*0.55f); glVertex2f(px + r*0.45f, cy + r*0.55f);
        glEnd();
        glColor4ub(72, 96, 44, A);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px, cy + r*0.9f); glVertex2f(px + 0.12f, cy + r*1.35f);
        glEnd();
    }
    glLineWidth(1.0f);
}

// Chestnut roasting cart with steam
void DrawChestnutRoaster4()
{
    if (SeasonAutumn4() < 0.02f) return;
    const unsigned char A = AutumnA4(255);
    const float x = SANTA_X4, y = SANTA_Y4;
    DrawGroundShadow(x, y, 1.5f, 0.30f, AutumnA4(86));
    glColor4ub(44, 42, 46, A);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.72f, y); glVertex2f(x - 0.52f, y + 1.05f);
        glVertex2f(x + 0.72f, y); glVertex2f(x + 0.52f, y + 1.05f);
    glEnd();
    glColor4ub(58, 54, 58, A);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.92f, y + 1.02f); glVertex2f(x + 0.92f, y + 1.02f);
        glVertex2f(x + 0.78f, y + 1.72f); glVertex2f(x - 0.78f, y + 1.72f);
    glEnd();
    float flick = 0.72f + 0.28f * sinf(firePhase4 * 3.0f);
    glColor4ub(236, 118, 40, AutumnA4((unsigned char)(210 * flick)));
    glBegin(GL_QUADS);
        glVertex2f(x - 0.80f, y + 1.62f); glVertex2f(x + 0.80f, y + 1.62f);
        glVertex2f(x + 0.80f, y + 1.80f); glVertex2f(x - 0.80f, y + 1.80f);
    glEnd();
    DrawSoftEllipse(x, y + 1.9f, 2.6f * flick, 1.5f * flick,
                    255, 150, 70, AutumnA4((unsigned char)(70 * flick * NightT4())), 3);
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.34f + i * 0.2f, 1.0f);
        FilledCircle4(x - 0.5f + i * 0.25f + sinf(t * 5.0f + i) * 0.25f,
                      y + 1.9f + t * 2.4f, 0.09f * (1.0f - t),
                      255, 186, 96, AutumnA4((unsigned char)(200 * (1.0f - t))));
    }
    for (int i = 0; i < 6; i++)
        FilledCircle4(x - 0.62f + i * 0.25f, y + 1.84f, 0.13f,
                      MixB4(92, 128), MixB4(52, 74), MixB4(34, 44), A);
    float stir = sinf(firePhase4 * 1.4f) * 0.16f;
    glColor4ub(96, 62, 48, A);
    glLineWidth(5.0f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.55f, y); glVertex2f(x + 1.55f, y + 1.55f);
    glEnd();
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.50f, y + 1.42f); glVertex2f(x + 0.70f + stir, y + 1.86f);
    glEnd();
    FilledCircle4(x + 1.55f, y + 1.88f, 0.30f, 226, 188, 152, A);
    glColor4ub(132, 66, 54, A);
    FilledCircle4(x + 1.55f, y + 2.08f, 0.27f, 132, 66, 54, A);
    glColor4ub(224, 208, 178, A);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.20f, y + 1.78f);
        glVertex2f(x - 0.72f, y + 1.78f);
        glVertex2f(x - 0.96f, y + 1.06f);
    glEnd();
    glLineWidth(1.0f);
}
constexpr int FOG_BLOBS4 = 15;
// Atmospheric fog layer
void DrawFog4(float y0, float y1, unsigned char alpha)
{
    if (fogAmount4 < 0.01f) return;
    float density = 0.72f + 0.20f * snowIntensity4;
    float breathe = 0.86f + 0.14f * sinf(fogPhase4 * 0.21f);
    float band    = y1 - y0;
    for (int i = 0; i < FOG_BLOBS4; i++) {
        float h1 = sinf(i * 12.9898f) * 43758.5453f;
        float h2 = sinf(i * 78.2330f) * 12345.6789f;
        float h3 = sinf(i * 45.1640f) * 27182.8182f;
        float j1 = h1 - floorf(h1);
        float j2 = h2 - floorf(h2);
        float j3 = h3 - floorf(h3);
        float speed = 0.30f + 0.85f * j1;
        float span  = 150.0f;
        float cx    = fmodf(j2 * span + fogPhase4 * speed, span) - span * 0.5f;
        float cy = y0 + band * (0.15f + 0.75f * j3)
                      + sinf(fogPhase4 * 0.13f + i * 1.7f) * band * 0.10f;
        float rx = (9.0f + 16.0f * j3) * (0.85f + 0.30f * j1);
        float ry = band * (0.16f + 0.20f * j2) * breathe;
        unsigned char a = (unsigned char)(alpha * fogAmount4 * density
                                          * (0.34f + 0.40f * j1));
        float warm = 1.0f - (cy - y0) / (band + 0.001f);
        DrawSoftEllipse(cx, cy, rx, ry,
                        (unsigned char)(196 + 22 * warm),
                        (unsigned char)(208 + 12 * warm),
                        (unsigned char)(230 - 10 * warm),
                        a, 3);
    }
    unsigned char va = (unsigned char)(alpha * fogAmount4 * 0.22f);
    glBegin(GL_QUAD_STRIP);
    for (int k = 0; k <= 40; k++) {
        float px = -62.0f + 124.0f * (float)k / 40.0f;
        float top = y0 + band * 0.74f
                  + sinf(px * 0.09f + fogPhase4 * 0.30f) * band * 0.10f
                  + sinf(px * 0.23f - fogPhase4 * 0.17f) * band * 0.05f;
        glColor4ub(200, 212, 232, 0);   glVertex2f(px, top);
        glColor4ub(200, 212, 232, va);  glVertex2f(px, y0);
    }
    glEnd();
}
// Timer callback: smoothly transitions environmental parameters between Winter and Autumn modes
void UpdateSeason4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSeason4, 0); return; }
    if (isAnimating4) {
        seasonT4 += (seasonTarget4 - seasonT4) * 0.011f;
        if (fabsf(seasonTarget4 - seasonT4) < 0.002f) seasonT4 = seasonTarget4;
    }
    glutTimerFunc(30, UpdateSeason4, 0);
}
// Timer callback: drifts low-lying ground fog blankets across the plaza cobblestones
void UpdateFog4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFog4, 0); return; }
    if (isAnimating4) {
        fogPhase4 += 0.13f;
        float tgt = fogMode4 ? 1.0f : 0.0f;
        fogAmount4 += (tgt - fogAmount4) * 0.02f;
    }
    glutTimerFunc(30, UpdateFog4, 0);
}
constexpr float terraceY4 = -20.0f;
constexpr float canalTopY4 = -27.0f;
constexpr float canalBotY4 = -36.0f;
// Massive stone ashlar river embankment wall separating upper plaza from lower canal dock
void DrawTerraceWall4()
{
    glBegin(GL_QUADS);
        glColor3ub(96, 104, 120);
        glVertex2f(-60.0f, terraceY4 - 2.6f); glVertex2f(60.0f, terraceY4 - 2.6f);
        glColor3ub(126, 134, 150);
        glVertex2f(60.0f, terraceY4);         glVertex2f(-60.0f, terraceY4);
    glEnd();
    glColor4ub(72, 80, 96, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = -30; i <= 30; i++) {
            float x = i * 4.0f;
            glVertex2f(x, terraceY4 - 2.6f); glVertex2f(x, terraceY4);
        }
        glVertex2f(-60.0f, terraceY4 - 1.3f); glVertex2f(60.0f, terraceY4 - 1.3f);
    glEnd();
    float cap = 0.45f * SeasonWinter4() + 1.1f * SnowDepth4();
    glColor3ub(246, 249, 255);
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, terraceY4);
        glVertex2f( 60.0f, terraceY4);
        glVertex2f( 60.0f, terraceY4 + cap);
        glVertex2f(-60.0f, terraceY4 + cap);
    glEnd();
    glColor4ub(238, 243, 252, 220);
    for (int i = 0; i < 9; i++) {
        float cx = -54.0f + i * 13.5f + sinf(i * 2.7f) * 3.0f;
        DrawSoftEllipse(cx, terraceY4 - 2.6f, 9.0f, 1.6f * SnowDepth4() + 0.6f,
                        238, 243, 252, 200, 2);
    }
    glLineWidth(1.0f);
}

// Frozen canal surface in winter
void DrawFrozenCanal4()
{
    glBegin(GL_QUADS);
        glColor3ub(MixB4(38, 90), MixB4(52, 140), MixB4(76, 160));
        glVertex2f(-60.0f, canalBotY4); glVertex2f(60.0f, canalBotY4);
        glColor3ub(MixB4(66, 120), MixB4(86, 160), MixB4(116, 190));
        glVertex2f(60.0f, canalTopY4);  glVertex2f(-60.0f, canalTopY4);
    glEnd();
    struct Smear
    {
        float x; unsigned char r, g, b; float w;
    };
    Smear smears[9] = {
        { shops4[0].x, 255, 196, 120, 2.6f },
        { shops4[1].x, 255, 196, 120, 2.4f },
        { shops4[2].x, 255, 196, 120, 2.6f },
        { shops4[3].x, 255, 196, 120, 2.2f },
        { ferrisCX4,   255, 150, 120, 4.6f },
        { treeX4,      140, 255, 170, 3.2f },
        { clockX4,     255, 210, 150, 2.4f },
        { -56.5f,      255, 170, 120, 2.4f },
        { moonX4,      200, 216, 245, 3.4f }
    };
    for (int i = 0; i < 9; i++) {
        Smear& sm = smears[i];
        unsigned char r = sm.r, g = sm.g, b = sm.b;
        if (multicolorLights4 && i < 4) { r = 220; g = 150; b = 220; }
        r = (unsigned char)(r * (0.25f + 0.75f * NightT4()));
        g = (unsigned char)(g * (0.25f + 0.75f * NightT4()));
        b = (unsigned char)(b * (0.25f + 0.75f * NightT4()));
        for (int k = 0; k < 5; k++) {
            float t   = (float)k / 5.0f;
            float y0  = canalTopY4 - t * (canalTopY4 - canalBotY4);
            float y1  = canalTopY4 - (t + 0.2f) * (canalTopY4 - canalBotY4);
            float off = sinf(twinklePhase4 * 0.9f + i * 1.7f + k * 2.4f) * 0.9f;
            float w   = sm.w * (1.0f - 0.45f * t);
            glColor4ub(r, g, b, (unsigned char)((70 * (1.0f - t)) * NightT4()));
            glBegin(GL_QUADS);
                glVertex2f(sm.x + off - w, y0);
                glVertex2f(sm.x + off + w, y0);
                glVertex2f(sm.x + off * 1.4f + w * 0.7f, y1);
                glVertex2f(sm.x + off * 1.4f - w * 0.7f, y1);
            glEnd();
        }
    }
    glColor4ub(190, 214, 240, 90);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 14; i++) {
            float x0 = -58.0f + i * 8.3f;
            float y0 = canalTopY4 - 1.0f - (i % 4) * 2.1f;
            glVertex2f(x0, y0); glVertex2f(x0 + 6.0f + (i % 3) * 2.0f, y0 - 0.7f);
        }
    glEnd();
    glLineWidth(1.0f);
}
constexpr float riverTopY4 = canalTopY4 + 1.5f;
constexpr float riverBotY4 = canalBotY4 - 3.0f;
constexpr float dockY4     = riverTopY4 + 1.0f;
constexpr float wallBotY4  = terraceY4 - 2.6f;
constexpr float stairX4[2] = { -48.0f, 24.0f };

// River boats
struct Boat4
{
    float x, speed, dir;
    float scale, phase;
    unsigned char hullR, hullG, hullB;
    bool anchored;
    bool hasPeople;
    bool hasLantern;
};
constexpr int NUM_BOATS4 = 5;
Boat4 boats4[NUM_BOATS4] = {
    { -48.0f, 0.00f,  0, 1.05f, 0.0f, 150,  96,  58, true,  false, true  },
    { -33.0f, 0.09f,  1, 0.92f, 1.7f, 120,  74,  48, false, false, true  },
    {  -8.0f, 0.06f, -1, 0.98f, 3.1f,  96,  62,  42, false, true,  false },
    {  18.0f, 0.07f,  1, 0.90f, 4.4f, 132,  88,  54, false, true,  false },
    {  38.0f, 0.05f, -1, 1.00f, 5.6f, 108,  70,  46, false, false, true  }
};
// River-side pedestrians
struct RiverPed4
{
    float x, speed, dir, phase, laneY; unsigned char coatR, coatG, coatB;
};
constexpr int NUM_RIVER_PEDS4 = 4;
RiverPed4 riverPeds4[NUM_RIVER_PEDS4] = {
    { -20.0f, 0.11f,  1, 0.0f, dockY4 + 0.2f, 150,  40,  50 },
    {   5.0f, 0.09f, -1, 1.4f, dockY4 + 0.9f,  40,  70, 120 },
    {  32.0f, 0.13f,  1, 2.6f, dockY4 + 0.2f, 190, 150,  60 },
    { -55.0f, 0.10f, -1, 3.9f, dockY4 + 0.9f,  70,  90,  80 }
};
float riverPhase4 = 0.0f;
inline void BoatPose4(const Boat4& b, float t, float& x, float& y)
{
    x = b.x;
    y = riverTopY4 - 1.2f + sinf(t * 1.2f + b.phase) * 0.28f;
}
// Renders traditional wooden canal cargo boat hull with sheer line and cargo deck
void DrawBoatShape4(float x, float y, float scale,
                     unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    const int SEG = 22;
    const float STERN_T = 0.88f;
    glColor4ub(r, g, b, a);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        float hx = x + t * 3.2f * scale;
        float sheer = ( 0.62f + 0.34f * t * t) * scale;
        float keel  = (-0.60f + 1.22f * t * t) * scale;
        if (keel > sheer) keel = sheer;
        glVertex2f(hx, y + sheer);
        glVertex2f(hx, y + keel);
    }
    glEnd();
    glColor4ub((unsigned char)(r * 0.62f), (unsigned char)(g * 0.62f),
               (unsigned char)(b * 0.62f), a);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        float hx = x + t * 3.2f * scale;
        float sheer = ( 0.62f + 0.34f * t * t) * scale;
        float keel  = (-0.60f + 1.22f * t * t) * scale;
        if (keel > sheer) keel = sheer;
        float mid = keel + (sheer - keel) * 0.38f;
        glVertex2f(hx, y + mid);
        glVertex2f(hx, y + keel);
    }
    glEnd();
    float tx    = x + STERN_T * 3.2f * scale;
    float tTop  = y + ( 0.62f + 0.34f * STERN_T * STERN_T) * scale;
    float tBot  = y + (-0.60f + 1.22f * STERN_T * STERN_T) * scale;
    glColor4ub((unsigned char)(r * 0.80f), (unsigned char)(g * 0.80f),
               (unsigned char)(b * 0.80f), a);
    glBegin(GL_QUADS);
        glVertex2f(tx,                 tBot);
        glVertex2f(tx + 0.30f * scale, tBot + 0.10f * scale);
        glVertex2f(tx + 0.30f * scale, tTop);
        glVertex2f(tx,                 tTop);
    glEnd();
    float bx = x - 3.2f * scale;
    glColor4ub((unsigned char)(r * 0.74f), (unsigned char)(g * 0.74f),
               (unsigned char)(b * 0.74f), a);
    glBegin(GL_TRIANGLES);
        glVertex2f(bx,                 y + 0.96f * scale);
        glVertex2f(bx - 0.34f * scale, y + 1.34f * scale);
        glVertex2f(bx + 0.16f * scale, y + 0.70f * scale);
    glEnd();
    unsigned char rr = (unsigned char)fminf(255.0f, r * 1.30f + 20.0f);
    unsigned char rg = (unsigned char)fminf(255.0f, g * 1.30f + 20.0f);
    unsigned char rb = (unsigned char)fminf(255.0f, b * 1.30f + 20.0f);
    glColor4ub(rr, rg, rb, a);
    glLineWidth(1.6f * scale);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        glVertex2f(x + t * 3.2f * scale, y + (0.62f + 0.34f * t * t) * scale);
    }
    glEnd();
    glColor4ub((unsigned char)(r * 0.88f + 30), (unsigned char)(g * 0.88f + 26),
               (unsigned char)(b * 0.88f + 20), a);
    for (int k = 0; k < 2; k++) {
        float t  = -0.34f + k * 0.62f;
        float hx = x + t * 3.2f * scale;
        float sy = y + (0.62f + 0.34f * t * t) * scale;
        glBegin(GL_QUADS);
            glVertex2f(hx - 0.34f * scale, sy - 0.16f * scale);
            glVertex2f(hx + 0.34f * scale, sy - 0.16f * scale);
            glVertex2f(hx + 0.34f * scale, sy);
            glVertex2f(hx - 0.34f * scale, sy);
        glEnd();
    }
    float ox = x - 0.30f * scale;
    float oy = y + (0.62f + 0.34f * 0.0088f) * scale;
    glColor4ub(70, 62, 54, a);
    glLineWidth(1.4f * scale);
    glBegin(GL_LINES);
        glVertex2f(ox, oy); glVertex2f(ox, oy + 0.24f * scale);
    glEnd();
    glColor4ub(196, 168, 126, a);
    glLineWidth(1.8f * scale);
    glBegin(GL_LINES);
        glVertex2f(ox - 1.50f * scale, oy + 0.46f * scale);
        glVertex2f(ox + 1.70f * scale, oy + 0.10f * scale);
    glEnd();
    glColor4ub(214, 190, 150, a);
    glBegin(GL_TRIANGLES);
        glVertex2f(ox + 1.70f * scale, oy + 0.10f * scale);
        glVertex2f(ox + 2.34f * scale, oy + 0.26f * scale);
        glVertex2f(ox + 2.30f * scale, oy - 0.10f * scale);
    glEnd();
    glColor4ub(232, 244, 250, (unsigned char)(a * 0.55f));
    glLineWidth(1.3f * scale);
    glBegin(GL_LINES);
        glVertex2f(x - 3.0f * scale, y - 0.30f * scale);
        glVertex2f(tx + 0.24f * scale, y - 0.30f * scale);
    glEnd();
    glLineWidth(1.0f);
}
// Flight of stone steps leading from the upper market promenade down to the river landing
void DrawStairs4(float x)
{
    const int STEPS = 6;
    float topY = terraceY4;
    float botY = dockY4;
    float dropPer = (topY - botY) / STEPS;
    for (int i = 0; i < STEPS; i++) {
        float y0 = topY - i * dropPer;
        float y1 = y0 - dropPer;
        float w  = 3.2f + i * 0.12f;
        glColor3ub(MixB4(58, 132), MixB4(60, 118), MixB4(64, 104));
        glBegin(GL_QUADS);
            glVertex2f(x - w, y1);         glVertex2f(x + w, y1);
            glVertex2f(x + w, y1 + 0.55f); glVertex2f(x - w, y1 + 0.55f);
        glEnd();
        glColor3ub(MixB4(30, 84), MixB4(32, 74), MixB4(36, 66));
        glBegin(GL_QUADS);
            glVertex2f(x - w, y0 - 0.05f); glVertex2f(x + w, y0 - 0.05f);
            glVertex2f(x + w, y1 + 0.55f); glVertex2f(x - w, y1 + 0.55f);
        glEnd();
        float cap = 0.18f + 0.35f * SnowDepth4();
        glColor4ub(246, 249, 255, 235);
        glBegin(GL_QUADS);
            glVertex2f(x - w, y1 + 0.55f);       glVertex2f(x + w, y1 + 0.55f);
            glVertex2f(x + w, y1 + 0.55f + cap); glVertex2f(x - w, y1 + 0.55f + cap);
        glEnd();
        DrawGroundShadow(x, y1, w * 0.9f, 0.0f, 55);
    }
    glColor3ub(MixB4(40, 70), MixB4(42, 68), MixB4(48, 74));
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 3.3f, topY); glVertex2f(x - (3.2f + STEPS*0.12f), botY + 0.6f);
        glVertex2f(x + 3.3f, topY); glVertex2f(x + (3.2f + STEPS*0.12f), botY + 0.6f);
    glEnd();
    glLineWidth(1.0f);
}
// Renders all access stairways connecting terrace levels
void DrawStairsAll4()
{
    for (int i = 0; i < 2; i++) DrawStairs4(stairX4[i]);
    for (int i = 0; i < 2; i++) {
        float x = stairX4[i];
        glColor3ub(MixB4(70, 132), MixB4(52, 100), MixB4(38, 76));
        glBegin(GL_QUADS);
            glVertex2f(x - 5.0f, dockY4);        glVertex2f(x + 5.0f, dockY4);
            glVertex2f(x + 5.0f, dockY4 + 1.0f); glVertex2f(x - 5.0f, dockY4 + 1.0f);
        glEnd();
        glColor4ub(30, 22, 16, 120);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int p = -4; p <= 4; p++) { glVertex2f(x + p, dockY4); glVertex2f(x + p, dockY4 + 1.0f); }
        glEnd();
        float cap = 0.12f + 0.3f * SnowDepth4();
        glColor4ub(246, 249, 255, 210);
        glBegin(GL_QUADS);
            glVertex2f(x - 5.0f, dockY4 + 1.0f);       glVertex2f(x + 5.0f, dockY4 + 1.0f);
            glVertex2f(x + 5.0f, dockY4 + 1.0f + cap); glVertex2f(x - 5.0f, dockY4 + 1.0f + cap);
        glEnd();
        DrawGroundShadow(x, dockY4, 5.0f, 0.3f, 60);
    }
    glLineWidth(1.0f);
}

// River/canal with boats and reflections
void DrawRiver4()
{
    glBegin(GL_QUADS);
        glColor3ub(MixB4(MixSB4(14, 26), MixSB4(60, 74)),
                   MixB4(MixSB4(34, 46), MixSB4(96, 96)),
                   MixB4(MixSB4(52, 36), MixSB4(118, 74)));
        glVertex2f(-60.0f, riverBotY4); glVertex2f(60.0f, riverBotY4);
        glColor3ub(MixB4(MixSB4(26, 44), MixSB4(84, 104)),
                   MixB4(MixSB4(58, 62), MixSB4(128, 122)),
                   MixB4(MixSB4(86, 46), MixSB4(152, 88)));
        glVertex2f(60.0f, riverTopY4);  glVertex2f(-60.0f, riverTopY4);
    glEnd();
    const int RIPPLE_ROWS = 16;
    for (int i = 0; i < RIPPLE_ROWS; i++) {
        float u  = (float)i / (RIPPLE_ROWS - 1);
        float f  = 1.0f - (1.0f - u) * (1.0f - u);
        float y  = riverBotY4 + f * (riverTopY4 - riverBotY4);
        float amp = 0.30f * (1.0f - f) + 0.06f;
        unsigned char al = (unsigned char)((26 + 34 * (1.0f - f)));
        glColor4ub(MixB4(150, 205), MixB4(180, 226), MixB4(206, 240), al);
        glLineWidth(1.0f + 0.7f * (1.0f - f));
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 40; k++) {
                float px = -62.0f + 124.0f * (float)k / 40.0f;
                float py = y
                         + sinf(px * 0.22f + riverPhase4 * 0.9f + i * 0.8f) * amp
                         + sinf(px * 0.07f - riverPhase4 * 0.5f + i * 1.9f) * amp * 0.6f;
                glVertex2f(px, py);
            }
        glEnd();
    }
    for (int i = 0; i < 26; i++) {
        float h1 = sinf(i * 31.77f) * 43758.5453f;
        float h2 = sinf(i * 12.13f) * 12345.6789f;
        float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2);
        float f  = 0.15f + 0.80f * j2;
        float y  = riverBotY4 + f * (riverTopY4 - riverBotY4);
        float px = fmodf(j1 * 124.0f + riverPhase4 * (1.4f + 1.6f * (1.0f - f)), 124.0f) - 62.0f;
        float len = (1.4f + 2.6f * (1.0f - f));
        glColor4ub(MixB4(10, 44), MixB4(26, 78), MixB4(44, 100),
                   (unsigned char)(40 + 40 * (1.0f - f)));
        glLineWidth(1.0f + 1.1f * (1.0f - f));
        glBegin(GL_LINES);
            glVertex2f(px, y);
            glVertex2f(px + len, y + 0.10f);
        glEnd();
    }
    glLineWidth(1.4f);
    glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 60; k++) {
            float px = -60.0f + 120.0f * (float)k / 60.0f;
            float lace = sinf(px * 0.5f + riverPhase4 * 1.3f) * 0.10f
                       + sinf(px * 1.3f - riverPhase4 * 0.8f) * 0.05f;
            glColor4ub(MixB4(180, 232), MixB4(205, 244), MixB4(224, 250),
                       (unsigned char)(70 + 40 * sinf(px * 0.4f + riverPhase4)));
            glVertex2f(px, riverTopY4 - 0.18f + lace);
        }
    glEnd();
    glLineWidth(1.0f);
    for (int i = 0; i < 8; i++) {
        float t = fmodf(i / 8.0f + riverPhase4 * 0.05f, 1.0f);
        float y = riverBotY4 + t * (riverTopY4 - riverBotY4);
        float x = fmodf(i * 23.7f + riverPhase4 * 3.0f, 120.0f) - 60.0f;
        float len = 5.0f + 3.0f * sinf(i * 1.7f);
        glColor4ub(MixB4(150, 235), MixB4(185, 245), MixB4(210, 250),
                   (unsigned char)(35 + 20 * sinf(riverPhase4 + i)));
        glLineWidth(1.3f);
        glBegin(GL_LINES); glVertex2f(x, y); glVertex2f(x + len, y + 0.15f); glEnd();
    }
    glLineWidth(1.0f);
}
// Boats on the river/canal
void DrawBoats4()
{
    for (int i = 0; i < NUM_BOATS4; i++) {
        Boat4& b = boats4[i];
        float x, y;
        BoatPose4(b, riverPhase4, x, y);
        DrawGroundShadow(x, riverTopY4 + 0.2f, 2.4f * b.scale, 0.0f, 40);
        DrawBoatShape4(x, y, b.scale, b.hullR, b.hullG, b.hullB, 255);
        glColor4ub(60, 40, 26, 200);
        glLineWidth(1.6f * b.scale);
        glBegin(GL_LINES);
            glVertex2f(x - 0.6f*b.scale, y + 0.5f*b.scale);
            glVertex2f(x - 0.6f*b.scale, y + 0.9f*b.scale);
        glEnd();
        glColor3ub(90, 64, 40);
        glLineWidth(1.2f * b.scale);
        glBegin(GL_LINES);
            glVertex2f(x - 1.6f*b.scale, y + 0.9f*b.scale);
            glVertex2f(x - 3.6f*b.scale, y - 1.4f*b.scale);
        glEnd();
        if (b.hasPeople) {
            for (int p = 0; p < 2; p++) {
                float px = x + (p == 0 ? -0.7f : 0.9f) * b.scale;
                float py = y + 0.55f * b.scale;
                unsigned char cr = p == 0 ? 150 : 70, cg = p == 0 ? 60 : 90, cb = p == 0 ? 60 : 120;
                glColor3ub(cr, cg, cb);
                glBegin(GL_QUADS);
                    glVertex2f(px - 0.5f*b.scale, py);              glVertex2f(px + 0.5f*b.scale, py);
                    glVertex2f(px + 0.4f*b.scale, py + 1.1f*b.scale); glVertex2f(px - 0.4f*b.scale, py + 1.1f*b.scale);
                glEnd();
                FilledCircle4(px, py + 1.4f*b.scale,  0.34f*b.scale, 224, 186, 148, 255);
                FilledCircle4(px, py + 1.75f*b.scale, 0.30f*b.scale, cr, cg, cb, 255);
            }
        }
        if (b.hasLantern) {
            float lx = x + 2.6f*b.scale, lyBase = y + 0.35f*b.scale, lyTop = lyBase + 1.1f*b.scale;
            glColor3ub(40, 32, 26);
            glLineWidth(1.6f * b.scale);
            glBegin(GL_LINES); glVertex2f(lx, lyBase); glVertex2f(lx, lyTop); glEnd();
            float flick = 0.85f + 0.15f * sinf(riverPhase4 * 6.0f + b.phase * 3.0f);
            FilledCircle4(lx, lyTop, 0.28f*b.scale*flick, 255, 206, 132, (unsigned char)(230 * NightT4()));
            DrawSoftEllipse(lx, lyTop, 2.4f*b.scale, 2.4f*b.scale, 255, 196, 120, (unsigned char)(70 * NightT4()), 4);
        }
        glLineWidth(1.0f);
    }
}
// Stevedores and dockworkers unloading cargo barrels and crates from moored river barges
void DrawGoodsUnloading4()
{
    float dockX = stairX4[0] + 3.2f;
    float bx, by; BoatPose4(boats4[0], riverPhase4, bx, by);
    for (int i = 0; i < 3; i++) {
        float cx = dockX + 1.0f + i * 1.1f;
        float cy = dockY4 + 1.0f + (i == 1 ? 1.0f : 0.0f);
        glColor3ub(120, 84, 50);
        glBegin(GL_QUADS);
            glVertex2f(cx - 0.55f, cy);          glVertex2f(cx + 0.55f, cy);
            glVertex2f(cx + 0.55f, cy + 1.0f);   glVertex2f(cx - 0.55f, cy + 1.0f);
        glEnd();
        glColor3ub(80, 54, 30);
        glLineWidth(1.4f);
        glBegin(GL_LINES); glVertex2f(cx - 0.55f, cy + 0.5f); glVertex2f(cx + 0.55f, cy + 0.5f); glEnd();
        DrawGroundShadow(cx, cy, 0.6f, 0.1f, 60);
    }
    float wx = dockX + 0.6f, wy = dockY4 + 1.0f;
    float reach = 0.15f * sinf(pedTimer4 * 0.5f);
    glColor3ub(64, 78, 96);
    glBegin(GL_QUADS);
        glVertex2f(wx - 0.5f, wy);          glVertex2f(wx + 0.5f, wy);
        glVertex2f(wx + 0.4f, wy + 1.6f);   glVertex2f(wx - 0.4f, wy + 1.6f);
    glEnd();
    glLineWidth(3.4f);
    glBegin(GL_LINES);
        glVertex2f(wx, wy + 1.3f); glVertex2f(wx - 1.2f + reach, wy + 1.5f);
        glVertex2f(wx, wy + 1.3f); glVertex2f(wx + 1.2f,         wy + 1.5f);
    glEnd();
    FilledCircle4(wx, wy + 1.95f, 0.34f, 224, 186, 148, 255);
    FilledCircle4(wx, wy + 2.3f,  0.28f, 60, 50, 40, 255);
    float mx = bx + 1.0f, my = by + 0.9f;
    glColor3ub(90, 50, 50);
    glBegin(GL_QUADS);
        glVertex2f(mx - 0.5f, my);          glVertex2f(mx + 0.5f, my);
        glVertex2f(mx + 0.4f, my + 1.4f);   glVertex2f(mx - 0.4f, my + 1.4f);
    glEnd();
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(mx, my + 1.1f); glVertex2f(mx + 1.3f, my + 1.3f); glEnd();
    FilledCircle4(mx, my + 1.7f, 0.32f, 224, 186, 148, 255);
    FilledCircle4(mx, my + 2.0f, 0.26f, 90, 50, 50, 255);
    float tCx = (mx + 1.3f + wx - 1.2f + reach) * 0.5f;
    float tCy = (my + 1.3f + wy + 1.5f) * 0.5f;
    glColor3ub(120, 84, 50);
    glBegin(GL_QUADS);
        glVertex2f(tCx - 0.4f, tCy - 0.4f); glVertex2f(tCx + 0.4f, tCy - 0.4f);
        glVertex2f(tCx + 0.4f, tCy + 0.4f); glVertex2f(tCx - 0.4f, tCy + 0.4f);
    glEnd();
    glLineWidth(1.0f);
}
// Waterfront stroller or dockhand walking along the lower canal quayside
void DrawRiverPerson4(const RiverPed4& p, float t)
{
    float sc = 1.35f;
    DrawGroundShadow(p.x, p.laneY, 0.7f * sc, 0.25f, 85);
    BeginDepthSprite(p.x, p.laneY, sc);
    float bob = sinf(t * 3.0f + p.phase) * 0.1f;
    float hipX = p.x, hipY = p.laneY + 1.0f + bob;
    glColor3ub(30, 30, 35);
    glLineWidth(2.6f);
    glBegin(GL_LINES);
        glVertex2f(hipX, hipY); glVertex2f(hipX + 0.24f*sinf(t*4.0f+p.phase), p.laneY);
        glVertex2f(hipX, hipY); glVertex2f(hipX - 0.24f*sinf(t*4.0f+p.phase), p.laneY);
    glEnd();
    glColor3ub(p.coatR, p.coatG, p.coatB);
    glLineWidth(5.4f);
    glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(hipX, hipY + 1.2f); glEnd();
    FilledCircle4(hipX, hipY + 1.55f, 0.32f, 225, 185, 145, 255);
    glColor3ub(40, 40, 45);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX - 0.32f, hipY + 1.78f); glVertex2f(hipX + 0.32f, hipY + 1.78f);
        glVertex2f(hipX, hipY + 2.35f);
    glEnd();
    glColor4ub(246, 249, 255, 230);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX - 0.17f*SnowDepth4(), hipY + 2.1f);
        glVertex2f(hipX + 0.17f*SnowDepth4(), hipY + 2.1f);
        glVertex2f(hipX, hipY + 2.35f);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float bt = fmodf(firePhase4 * 0.26f + p.phase * 0.17f + i * 0.33f, 1.0f);
        FilledCircle4(hipX + p.dir * (0.36f + bt*1.3f), hipY + 1.5f + bt*0.45f,
                      0.09f + bt*0.22f, 226, 234, 244, (unsigned char)(105 * (1.0f - bt)));
    }
    EndDepthSprite();
}
// Renders people strolling along the lower waterfront promenade
void DrawRiverPedestrians4()
{
    int order[NUM_RIVER_PEDS4];
    for (int i = 0; i < NUM_RIVER_PEDS4; i++) order[i] = i;
    for (int i = 1; i < NUM_RIVER_PEDS4; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && riverPeds4[order[j]].laneY < riverPeds4[key].laneY) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_RIVER_PEDS4; i++) DrawRiverPerson4(riverPeds4[order[i]], pedTimer4);
}
// Timer callback: bobs moored river barges and launches on water waves
void UpdateBoats4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBoats4, 0); return; }
    if (isAnimating4) {
        riverPhase4 += 0.03f;
        for (int i = 0; i < NUM_BOATS4; i++) {
            if (boats4[i].anchored) continue;
            boats4[i].x += boats4[i].speed * boats4[i].dir;
            if (boats4[i].dir > 0 && boats4[i].x > 62.0f)  boats4[i].x = -62.0f;
            if (boats4[i].dir < 0 && boats4[i].x < -62.0f) boats4[i].x = 62.0f;
        }
    }
    glutTimerFunc(30, UpdateBoats4, 0);
}
// Timer callback: moves pedestrians along the quayside walkway
void UpdateRiverPedestrians4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateRiverPedestrians4, 0); return; }
    if (isAnimating4) {
        for (int i = 0; i < NUM_RIVER_PEDS4; i++) {
            riverPeds4[i].x += riverPeds4[i].speed * riverPeds4[i].dir;
            if (riverPeds4[i].dir > 0 && riverPeds4[i].x > 62.0f)  riverPeds4[i].x = -62.0f;
            if (riverPeds4[i].dir < 0 && riverPeds4[i].x < -62.0f) riverPeds4[i].x = 62.0f;
        }
    }
    glutTimerFunc(30, UpdateRiverPedestrians4, 0);
}
// High-detail foreground gas street lamp with ornate iron filigree and glowing glass bowl
void DrawForegroundLamp4(float x, float scale)
{
    float baseY = terraceY4 - 2.0f;
    float topY  = baseY + 26.0f * scale;
    glColor3ub(24, 28, 38);
    glLineWidth(6.0f * scale);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    glBegin(GL_QUADS);
        glVertex2f(x - 1.5f*scale, baseY);        glVertex2f(x + 1.5f*scale, baseY);
        glVertex2f(x + 1.0f*scale, baseY + 2.4f*scale); glVertex2f(x - 1.0f*scale, baseY + 2.4f*scale);
    glEnd();
    float ly = topY - 1.0f * scale;
    glColor3ub(24, 28, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.5f*scale, ly - 1.8f*scale); glVertex2f(x + 1.5f*scale, ly - 1.8f*scale);
        glVertex2f(x + 1.1f*scale, ly + 1.4f*scale); glVertex2f(x - 1.1f*scale, ly + 1.4f*scale);
    glEnd();
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.7f*scale, ly + 1.4f*scale);
        glVertex2f(x + 1.7f*scale, ly + 1.4f*scale);
        glVertex2f(x,              ly + 3.0f*scale);
    glEnd();
    unsigned char lr = 255, lg = 206, lb = 132;
    if (multicolorLights4) { lr = 235; lg = 170; lb = 215; }
    FilledCircle4(x, ly - 0.2f*scale, 1.15f*scale, lr, lg, lb, 235);
    DrawSoftEllipse(x, ly - 0.2f*scale, 6.0f*scale, 6.0f*scale, lr, lg, lb, 60, 4);
    glColor4ub(lr, lg, lb, 30);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.6f*scale, ly - 1.0f*scale);
        glVertex2f(x + 1.6f*scale, ly - 1.0f*scale);
        glVertex2f(x + 9.0f*scale, baseY);
        glVertex2f(x - 9.0f*scale, baseY);
    glEnd();
    for (int i = 0; i < 16; i++) {
        float h1 = sinf(i * 41.7f + twinklePhase4 * 0.3f) * 43758.5453f;
        float h2 = sinf(i * 13.3f) * 12345.6789f;
        float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2);
        float t  = fmodf(j2 + twinklePhase4 * 0.10f, 1.0f);
        float fy = ly - 1.0f*scale - t * (ly - 1.0f*scale - baseY);
        float spread = 1.6f + t * 7.4f;
        FilledCircle4(x + (j1 - 0.5f) * 2.0f * spread * scale, fy,
                      0.22f * scale, 255, 255, 255,
                      (unsigned char)(200 * (1.0f - t * 0.7f)));
    }
    DrawSoftEllipse(x, baseY, 9.5f*scale, 2.2f*scale, lr, lg, lb, 80, 4);
    glLineWidth(1.0f);
}
// Market square lamp column casting bright illumination over shoppers
void DrawPlazaLamp4(float x, float scale)
{
    float baseY = SHOP_BASE_Y4 - 2.0f;
    float topY  = baseY + 12.0f * scale;
    glColor3ub(24, 28, 38);
    glLineWidth(3.0f * scale);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    float ly = topY - 0.6f * scale;
    glColor3ub(24, 28, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.6f*scale, ly - 0.6f*scale); glVertex2f(x + 0.6f*scale, ly - 0.6f*scale);
        glVertex2f(x + 0.5f*scale, ly + 0.8f*scale); glVertex2f(x - 0.5f*scale, ly + 0.8f*scale);
    glEnd();
    unsigned char lr = 255, lg = 206, lb = 132;
    if (multicolorLights4) { lr = 235; lg = 170; lb = 215; }
    FilledCircle4(x, ly - 0.05f*scale, 0.5f*scale, lr, lg, lb, 220);
    DrawSoftEllipse(x, ly - 0.05f*scale, 2.4f*scale, 1.2f*scale, lr, lg, lb, 48, 3);
    glLineWidth(1.0f);
}
void DrawBackFigure4(float x, float scale,
                     unsigned char cr, unsigned char cg, unsigned char cb,
                     unsigned char hr, unsigned char hg, unsigned char hb,
                     float sway)
{
    float y = terraceY4 - 2.4f;
    DrawGroundShadow(x, y, 2.2f * scale, 0.4f, 90);
    glColor3ub(cr, cg, cb);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f*scale, y);
        glVertex2f(x + 1.55f*scale, y);
        glVertex2f(x + 1.25f*scale + sway, y + 6.4f*scale);
        glVertex2f(x - 1.25f*scale + sway, y + 6.4f*scale);
    glEnd();
    glColor4ub((unsigned char)(cr*0.75f), (unsigned char)(cg*0.75f), (unsigned char)(cb*0.75f), 255);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.30f*scale + sway*0.8f, y + 5.7f*scale);
        glVertex2f(x + 1.30f*scale + sway*0.8f, y + 5.7f*scale);
        glVertex2f(x + 1.25f*scale + sway,      y + 6.4f*scale);
        glVertex2f(x - 1.25f*scale + sway,      y + 6.4f*scale);
    glEnd();
    glColor3ub(28, 30, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.3f*scale, y - 0.5f*scale); glVertex2f(x - 0.2f*scale, y - 0.5f*scale);
        glVertex2f(x - 0.2f*scale, y + 0.5f*scale); glVertex2f(x - 1.3f*scale, y + 0.5f*scale);
        glVertex2f(x + 0.2f*scale, y - 0.5f*scale); glVertex2f(x + 1.3f*scale, y - 0.5f*scale);
        glVertex2f(x + 1.3f*scale, y + 0.5f*scale); glVertex2f(x + 0.2f*scale, y + 0.5f*scale);
    glEnd();
    FilledCircle4(x + sway, y + 7.5f*scale, 1.05f*scale, 214, 176, 140, 255);
    FilledCircle4(x + sway, y + 8.15f*scale, 1.00f*scale, hr, hg, hb, 255);
    FilledCircle4(x + sway, y + 9.05f*scale, 0.42f*scale, 250, 250, 255, 255);
    glColor3ub(hr, hg, hb);
    glLineWidth(3.4f * scale);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + sway, y + 6.9f*scale);
        glVertex2f(x + sway - 1.6f*scale, y + 6.2f*scale);
        glVertex2f(x + sway - 2.9f*scale, y + 6.5f*scale);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.22f + i * 0.33f, 1.0f);
        FilledCircle4(x + sway + (1.0f + t * 2.2f) * scale,
                      y + (7.4f + t * 0.7f) * scale,
                      (0.25f + t * 0.5f) * scale, 226, 234, 244,
                      (unsigned char)(95 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}
// Silhouetted spectators in near foreground observing the bustling plaza spectacle
void DrawForegroundCrowd4()
{
    float sculptureBaseY = terraceY4 - 2.4f;
    DrawIceSculptureAt(-38.0f, sculptureBaseY, 1.15f);
    DrawIceSculptureAt(-32.5f, sculptureBaseY, 0.95f);
    DrawIceSculptureAt( 44.0f, sculptureBaseY, 1.05f);
}
float sledRunX4 = -70.0f;

// Animated sled running across the scene
void DrawSledRun4()
{
    if (SeasonWinter4() < 0.02f) return;
    float x = sledRunX4;
    float y = terraceY4 - 4.6f + sinf(x * 0.09f) * 0.5f;
    for (int i = 0; i < 8; i++) {
        float t = fmodf(firePhase4 * 0.6f + i * 0.13f, 1.0f);
        FilledCircle4(x - 2.2f - t * 5.0f, y + 0.4f + t * 1.9f,
                      0.34f * (1.0f - t), 246, 250, 255,
                      (unsigned char)(185 * (1.0f - t)));
    }
    glColor3ub(150, 92, 48);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.0f, y);      glVertex2f(x + 2.0f, y);
        glVertex2f(x + 2.0f, y+0.7f); glVertex2f(x - 2.0f, y+0.7f);
    glEnd();
    glColor3ub(206, 214, 228);
    glLineWidth(2.6f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 2.2f, y - 0.15f);
        glVertex2f(x + 2.2f, y - 0.15f);
        glVertex2f(x + 2.9f, y + 0.65f);
    glEnd();
    glColor3ub(200, 70, 80);
    glLineWidth(5.5f);
    glBegin(GL_LINES); glVertex2f(x - 0.4f, y + 0.7f); glVertex2f(x + 0.7f, y + 2.4f); glEnd();
    glColor3ub(30, 34, 44);
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(x - 0.4f, y + 0.9f); glVertex2f(x + 1.6f, y + 0.6f); glEnd();
    FilledCircle4(x + 0.85f, y + 3.0f, 0.72f, 226, 188, 150, 255);
    FilledCircle4(x + 0.85f, y + 3.5f, 0.62f, 70, 150, 210, 255);
    FilledCircle4(x + 0.85f, y + 4.05f, 0.26f, 250, 250, 255, 255);
    glLineWidth(1.0f);
}
// Timer callback: animates child zooming past on a wooden sled across the plaza snow
void UpdateSledRun4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSledRun4, 0); return; }
    if (isAnimating4) {
        sledRunX4 += 0.55f;
        if (sledRunX4 > 72.0f) sledRunX4 = -72.0f - (rand() % 40);
    }
    glutTimerFunc(25, UpdateSledRun4, 0);
}
constexpr float plazaVanishX4 = 0.0f;
constexpr float plazaHorizonY4 = -6.0f;
constexpr float plazaNearY4    = -40.0f;
inline float PlazaCourseY4(float t)
{
    float k = 1.0f - (1.0f - t) * (1.0f - t);
    return plazaNearY4 + (plazaHorizonY4 - plazaNearY4) * k;
}

// Market plaza ground with perspective grid
void DrawPlazaGround4()
{
    glBegin(GL_QUADS);
        glColor3ub(MixSB4(198, 128), MixSB4(210, 116), MixSB4(226, 102));
        glVertex2f(-60, plazaNearY4); glVertex2f(60, plazaNearY4);
        glColor3ub(MixSB4(228, 164), MixSB4(233, 148), MixSB4(240, 126));
        glVertex2f(60, plazaHorizonY4);   glVertex2f(-60, plazaHorizonY4);
    glEnd();
    glColor4ub(MixSB4(178, 96), MixSB4(190, 86), MixSB4(208, 74), 150);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = -9; i <= 9; i++) {
        float xTop  = plazaVanishX4 + i * 6.5f;
        float xNear = plazaVanishX4 + (xTop - plazaVanishX4) * 3.6f;
        glVertex2f(xTop,  plazaHorizonY4);
        glVertex2f(xNear, plazaNearY4);
    }
    glEnd();
    glBegin(GL_LINES);
    for (int c = 1; c <= 9; c++) {
        float t = (float)c / 10.0f;
        float y = PlazaCourseY4(t);
        glVertex2f(-60.0f, y);
        glVertex2f( 60.0f, y);
    }
    glEnd();
    glColor4ub(MixSB4(236, 108), MixSB4(241, 98), MixSB4(248, 84), 120);
    glBegin(GL_QUADS);
        glVertex2f(-16.0f, plazaNearY4); glVertex2f(30.0f, plazaNearY4);
        glVertex2f( 10.0f, plazaHorizonY4); glVertex2f(-4.0f, plazaHorizonY4);
    glEnd();
    if (SeasonAutumn4() > 0.02f) {
        const unsigned char leafCols[4][3] = {
            {168, 82, 34}, {196, 128, 42}, {140, 62, 40}, {186, 156, 58}
        };
        for (int i = 0; i < 150; i++) {
            float h1 = sinf(i * 12.9898f) * 43758.5453f;
            float h2 = sinf(i * 78.2330f) * 12345.6789f;
            float h3 = sinf(i * 45.1640f) * 27182.8182f;
            float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2), j3 = h3 - floorf(h3);
            float f  = j2 * j2;
            float ly = plazaHorizonY4 + (plazaNearY4 - plazaHorizonY4) * f;
            float lx = -62.0f + j1 * 124.0f;
            float sz = 0.16f + 0.42f * f;
            const unsigned char* c = leafCols[i % 4];
            glColor4ub(c[0], c[1], c[2], AutumnA4(200));
            glBegin(GL_TRIANGLES);
                float a = j3 * 6.2831853f;
                glVertex2f(lx + sz * cosf(a),          ly + sz * 0.45f * sinf(a));
                glVertex2f(lx + sz * cosf(a + 2.3f),   ly + sz * 0.45f * sinf(a + 2.3f));
                glVertex2f(lx + sz * cosf(a + 4.1f),   ly + sz * 0.45f * sinf(a + 4.1f));
            glEnd();
        }
    }
    glLineWidth(1.0f);
}
constexpr float roadNearY4 = -20.4f;
inline float RoadY4 (float t)
{
    return roadFarY4 + (roadNearY4 - roadFarY4) * (0.42f * t + 0.58f * t * t);
}
inline float RoadCX4(float t)
{
    return roadFarX4 - 19.2f * powf(t, 1.70f);
}
inline float RoadHW4(float t)
{
    return roadFarHW4 + 10.2f * powf(t, 1.28f);
}
inline void RoadSurfaceColour4(float t, float y, float side,
                               unsigned char& r, unsigned char& g, unsigned char& b)
{
    float depth = 0.86f * (1.0f - t);
    int nr = 104, ng = 116, nb = 142, dr = 142, dg = 152, db = 170;
    if (side > 0.0f) { nr = 78; ng = 88; nb = 114; dr = 112; dg = 122; db = 142; }
    AirFade4(depth, y, nr, ng, nb, dr, dg, db, r, g, b);
}

// Perspective road with vanishing point
void DrawPerspectiveRoad4()
{
    const int N = 30;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float t  = (float)i / N;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        float bank = 0.6f + 2.4f * t;
        unsigned char br, bg, bb;
        AirFade4(0.85f * (1.0f - t), y, 224, 232, 246, 246, 250, 255, br, bg, bb);
        glColor3ub(br, bg, bb);
        glVertex2f(cx - hw - bank, y);
        glVertex2f(cx + hw + bank, y);
    }
    glEnd();
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float t  = (float)i / N;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        unsigned char lr, lg, lb, rr, rg, rb;
        RoadSurfaceColour4(t, y, -1.0f, lr, lg, lb);
        RoadSurfaceColour4(t, y,  1.0f, rr, rg, rb);
        glColor3ub(lr, lg, lb); glVertex2f(cx - hw, y);
        glColor3ub(rr, rg, rb); glVertex2f(cx + hw, y);
    }
    glEnd();
    for (int lane = -1; lane <= 1; lane += 2) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= N; i++) {
            float t  = (float)i / N;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            float c  = cx + lane * hw * 0.44f;
            float w  = hw * 0.17f;
            glColor4ub(MixB4(52, 104), MixB4(60, 112), MixB4(82, 130),
                       (unsigned char)(105.0f * (0.20f + 0.80f * t)));
            glVertex2f(c - w, y);
            glVertex2f(c + w, y);
        }
        glEnd();
    }
    for (int side = -1; side <= 1; side += 2) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= N; i++) {
            float t  = (float)i / N;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            float lip = 0.16f + 0.85f * t;
            glColor4ub(MixB4(228, 250), MixB4(238, 252), MixB4(250, 255),
                       (unsigned char)(230.0f * (0.40f + 0.60f * t)));
            glVertex2f(cx + side * hw, y);
            glVertex2f(cx + side * (hw + lip), y);
        }
        glEnd();
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= N; i++) {
            float t  = (float)i / N;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            float sh = 0.10f + 0.65f * t;
            glColor4ub(MixB4(24, 74), MixB4(30, 84), MixB4(52, 110),
                       (unsigned char)(MixB4(120, 80) * (0.30f + 0.70f * t)));
            glVertex2f(cx + side * hw, y);
            glColor4ub(MixB4(24, 74), MixB4(30, 84), MixB4(52, 110), 0);
            glVertex2f(cx + side * (hw - sh), y);
        }
        glEnd();
    }
    for (int d = 0; d < 7; d++) {
        float t0 = 0.34f + d * 0.095f;
        float t1 = t0 + 0.045f;
        if (t1 > 1.0f) break;
        float y0 = RoadY4(t0), y1 = RoadY4(t1);
        float c0 = RoadCX4(t0), c1 = RoadCX4(t1);
        float w0 = RoadHW4(t0) * 0.035f, w1 = RoadHW4(t1) * 0.035f;
        if (d == 2 || d == 5) continue;
        glColor4ub(MixB4(196, 226), MixB4(186, 214), MixB4(150, 178),
                   (unsigned char)(150.0f * t0));
        glBegin(GL_QUADS);
            glVertex2f(c0 - w0, y0); glVertex2f(c0 + w0, y0);
            glVertex2f(c1 + w1, y1); glVertex2f(c1 - w1, y1);
        glEnd();
    }
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= N; i++) {
        float t  = (float)i / N;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        float reach = hw * (0.85f - 0.35f * t);
        glColor4ub(MixB4(18, 58), MixB4(24, 68), MixB4(44, 92),
                   (unsigned char)(MixB4(58, 48) * (0.35f + 0.65f * t)));
        glVertex2f(cx + hw, y);
        glColor4ub(MixB4(8, 44), MixB4(10, 52), MixB4(20, 74), 0);
        glVertex2f(cx + hw - reach, y);
    }
    glEnd();
    for (int i = 0; i < 7; i++) {
        float t  = 0.18f + i * 0.13f;
        if (t > 1.0f) break;
        float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
        float sc = 0.35f + 0.95f * t;
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        DrawSoftEllipse(cx + side * (hw + 1.9f * sc), y, 2.2f * sc,
                        (0.35f + 0.55f * SnowDepth4()) * sc,
                        244, 248, 254, 215, 2);
    }
    float night = NightT4();
    if (night > 0.02f) {
        for (int i = 0; i < 4; i++) {
            float t  = 0.10f + i * 0.27f;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            DrawSoftEllipse(cx + hw * 0.35f, y, hw * 0.95f, 0.9f + 1.1f * t,
                            255, 202, 140, (unsigned char)((18 + 16 * t) * night), 4);
        }
    }
    if (dayT4 > 0.02f) {
        float y = RoadY4(0.62f), cx = RoadCX4(0.62f), hw = RoadHW4(0.62f);
        DrawSoftEllipse(cx, y, hw * 1.5f, 5.5f, 250, 246, 226,
                        (unsigned char)(52.0f * dayT4), 4);
    }
    float hr, hg, hb;
    SkyAirColour4(-5.0f, hr, hg, hb);
    glBegin(GL_QUADS);
        glColor4ub((unsigned char)hr, (unsigned char)hg, (unsigned char)hb, MixB4(210, 230));
        glVertex2f(RoadCX4(0.0f) - RoadHW4(0.0f) - 1.2f, roadFarY4);
        glVertex2f(RoadCX4(0.0f) + RoadHW4(0.0f) + 1.2f, roadFarY4);
        glColor4ub((unsigned char)hr, (unsigned char)hg, (unsigned char)hb, 0);
        glVertex2f(RoadCX4(0.22f) + RoadHW4(0.22f), RoadY4(0.22f));
        glVertex2f(RoadCX4(0.22f) - RoadHW4(0.22f), RoadY4(0.22f));
    glEnd();
}
constexpr float RICKSHAW_PARK_T4 = 0.80f;
float rickshawT4     = RICKSHAW_PARK_T4;
float rickshawWheel4 = 0.0f;
// Rickshaw parked or moving on the road
void DrawRickshaw4()
{
    const float t  = rickshawT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx + RoadHW4(t) * 1.62f;
    const float depth = 0.80f * (1.0f - t);
    unsigned char r, g, b;
    DrawGroundShadow(x, y, 2.4f * S, 0.35f, (unsigned char)(90 * (0.35f + 0.65f * t)));
    AirFade4(depth, y, 28, 30, 38, 74, 80, 92, r, g, b);
    glColor3ub(r, g, b);
    FilledCircle4(x - 1.30f * S, y + 0.92f * S, 0.86f * S, r, g, b, 255);
    AirFade4(depth, y, 52, 56, 66, 104, 110, 124, r, g, b);
    FilledCircle4(x - 1.30f * S, y + 0.92f * S, 0.60f * S, r, g, b, 255);
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 1.30f * S, y + 0.92f * S);
        glVertex2f(x - 0.55f * S, y + 0.86f * S);
    glEnd();
    AirFade4(depth, y, 104, 34, 44, 186, 66, 74, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(x - 2.10f * S, y + 1.05f * S);
        glVertex2f(x + 0.30f * S, y + 1.05f * S);
        glVertex2f(x + 0.42f * S, y + 2.30f * S);
        glVertex2f(x - 2.16f * S, y + 2.30f * S);
    glEnd();
    AirFade4(depth, y, 70, 22, 30, 132, 44, 52, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 2.10f * S, y + 1.62f * S);
        glVertex2f(x + 0.34f * S, y + 1.62f * S);
    glEnd();
    AirFade4(depth, y, 150, 152, 160, 206, 208, 214, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_LINES);
        glVertex2f(x - 2.12f * S, y + 1.05f * S);
        glVertex2f(x + 0.32f * S, y + 1.05f * S);
    glEnd();
    AirFade4(depth, y, 58, 40, 30, 112, 82, 58, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.00f * S, y + 2.30f * S);
        glVertex2f(x + 0.20f * S, y + 2.30f * S);
        glVertex2f(x + 0.20f * S, y + 2.74f * S);
        glVertex2f(x - 2.00f * S, y + 2.74f * S);
    glEnd();
    AirFade4(depth, y, 24, 20, 22, 58, 48, 50, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.94f * S, y + 2.40f * S);
        glVertex2f(x + 0.14f * S, y + 2.40f * S);
        glVertex2f(x + 0.14f * S, y + 2.74f * S);
        glVertex2f(x - 1.94f * S, y + 2.74f * S);
    glEnd();
    AirFade4(depth, y, 32, 34, 42, 62, 66, 78, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x - 0.90f * S, y + 2.74f * S);
        for (int i = 0; i <= 12; i++) {
            float a = PI4 * ((float)i / 12.0f);
            glVertex2f(x - 0.90f * S + 1.42f * S * cosf(a),
                       y + 2.74f * S + 2.00f * S * sinf(a));
        }
    glEnd();
    AirFade4(depth, y, 58, 60, 70, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.2f * S);
    glBegin(GL_LINES);
        for (int i = 1; i < 4; i++) {
            float a = PI4 * ((float)i / 4.0f);
            glVertex2f(x - 0.90f * S, y + 2.74f * S);
            glVertex2f(x - 0.90f * S + 1.42f * S * cosf(a),
                       y + 2.74f * S + 2.00f * S * sinf(a));
        }
    glEnd();
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glLineWidth(2.6f * S);
    glBegin(GL_LINE_STRIP);
        for (int i = 2; i <= 10; i++) {
            float a = PI4 * ((float)i / 12.0f);
            glVertex2f(x - 0.90f * S + 1.46f * S * cosf(a),
                       y + 2.74f * S + 2.04f * S * sinf(a));
        }
    glEnd();
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.2f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 0.55f * S, y + 0.86f * S);
        glVertex2f(x + 0.55f * S, y + 1.16f * S);
        glVertex2f(x + 1.70f * S, y + 1.90f * S);
        glVertex2f(x + 1.86f * S, y + 0.84f * S);
    glEnd();
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 1.52f * S, y + 2.18f * S);
        glVertex2f(x + 1.98f * S, y + 2.06f * S);
    glEnd();
    for (int w = 0; w < 2; w++) {
        float wx = (w == 0) ? (x - 0.55f * S) : (x + 1.86f * S);
        float wy = (w == 0) ? (y + 0.86f * S) : (y + 0.84f * S);
        float wr = (w == 0) ? (0.90f * S)     : (0.86f * S);
        AirFade4(depth, y, 22, 24, 30, 58, 62, 74, r, g, b);
        FilledCircle4(wx, wy, wr, r, g, b, 255);
        AirFade4(depth, y, 70, 74, 84, 128, 132, 146, r, g, b);
        FilledCircle4(wx, wy, wr * 0.70f, r, g, b, 255);
        AirFade4(depth, y, 30, 32, 40, 66, 70, 82, r, g, b);
        FilledCircle4(wx, wy, wr * 0.22f, r, g, b, 255);
        AirFade4(depth, y, 96, 100, 110, 156, 160, 172, r, g, b);
        glColor3ub(r, g, b);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int k = 0; k < 6; k++) {
                float a = rickshawWheel4 + k * (PI4 / 6.0f);
                glVertex2f(wx - wr * 0.68f * cosf(a), wy - wr * 0.68f * sinf(a));
                glVertex2f(wx + wr * 0.68f * cosf(a), wy + wr * 0.68f * sinf(a));
            }
        glEnd();
    }
    const float sway = sinf(pedTimer4 * 0.62f) * 0.07f * S;
    AirFade4(depth, y, 60, 52, 44, 104, 92, 78, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.92f * S, y + 2.10f * S);
        glVertex2f(x + 0.58f * S, y + 0.10f * S);
        glVertex2f(x + 1.04f * S, y + 2.10f * S);
        glVertex2f(x + 1.42f * S, y + 0.10f * S);
    glEnd();
    AirFade4(depth, y, 52, 74, 58, 92, 132, 100, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(4.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.98f * S, y + 2.10f * S);
        glVertex2f(x + 1.16f * S + sway, y + 3.24f * S);
    glEnd();
    glLineWidth(1.8f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 1.14f * S + sway, y + 3.06f * S);
        glVertex2f(x + 1.76f * S, y + 2.20f * S);
        glVertex2f(x + 1.10f * S + sway, y + 3.02f * S);
        glVertex2f(x + 0.92f * S, y + 2.06f * S);
    glEnd();
    AirFade4(depth, y, 150, 122, 100, 226, 188, 152, r, g, b);
    FilledCircle4(x + 1.22f * S + sway, y + 3.56f * S, 0.30f * S, r, g, b, 255);
    AirFade4(depth, y, 100, 60, 50, 176, 106, 88, r, g, b);
    FilledCircle4(x + 1.22f * S + sway, y + 3.76f * S, 0.27f * S, r, g, b, 255);
    float lampOn = NightT4();
    if (lampOn > 0.05f) {
        float lx = x + 2.10f * S, ly = y + 2.00f * S;
        DrawSoftEllipse(lx + 0.4f * S, y + 0.25f * S, 2.0f * S, 0.7f * S,
                        255, 222, 158, (unsigned char)(62 * lampOn), 3);
        FilledCircle4(lx, ly, 0.34f * S, 255, 216, 150, (unsigned char)(70 * lampOn));
        FilledCircle4(lx, ly, 0.16f * S, 255, 246, 214, (unsigned char)(255 * lampOn));
    }
    glLineWidth(1.0f);
}
constexpr float TEA_X4 = -23.0f;
constexpr float TEA_Y4 = -18.6f;
constexpr float TEA_S4 =  1.10f;
// Renders a market patron seated on a wooden stool sipping hot spiced tea
void DrawTeaSitter4(float x, float y, float sc, int seed,
                    unsigned char cr, unsigned char cg, unsigned char cb,
                    bool facingLeft)
{
    const float d = facingLeft ? -1.0f : 1.0f;
    float lean = sinf(pedTimer4 * 0.6f + seed * 1.7f) * 0.05f;
    float sip  = sinf(pedTimer4 * 0.9f + seed * 2.3f);
    bool  drinking = (sip > 0.72f);
    DrawGroundShadow(x, y, 0.86f * sc, 0.22f, 78);
    glColor3ub(38, 38, 46);
    glLineWidth(2.4f * sc);
    glBegin(GL_LINES);
        glVertex2f(x - 0.16f * sc, y + 1.06f * sc); glVertex2f(x - 0.16f * sc, y);
        glVertex2f(x + 0.20f * sc, y + 1.06f * sc); glVertex2f(x + 0.20f * sc, y);
    glEnd();
    glColor3ub(cr, cg, cb);
    glLineWidth(4.0f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + 0.02f * sc, y + 1.10f * sc);
        glVertex2f(x - d * 0.60f * sc, y + 1.14f * sc);
    glEnd();
    glLineWidth(5.2f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + 0.02f * sc, y + 1.10f * sc);
        glVertex2f(x + (0.10f + lean) * sc, y + 2.14f * sc);
    glEnd();
    float cupY = y + (drinking ? 2.18f : 1.86f) * sc;
    float cupX = x + (0.10f + lean) * sc + d * 0.34f * sc;
    glColor3ub((unsigned char)(cr * 0.82f), (unsigned char)(cg * 0.82f),
               (unsigned char)(cb * 0.82f));
    glLineWidth(2.2f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + (0.08f + lean) * sc, y + 1.94f * sc);
        glVertex2f(cupX, cupY);
    glEnd();
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.46f * sc, 0.30f * sc, 228, 190, 154, 255);
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.66f * sc, 0.28f * sc, cr, cg, cb, 255);
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.86f * sc, 0.11f * sc, 246, 246, 250, 255);
    glColor3ub(238, 238, 242);
    glBegin(GL_QUADS);
        glVertex2f(cupX - 0.15f * sc, cupY - 0.16f * sc);
        glVertex2f(cupX + 0.15f * sc, cupY - 0.16f * sc);
        glVertex2f(cupX + 0.13f * sc, cupY + 0.14f * sc);
        glVertex2f(cupX - 0.13f * sc, cupY + 0.14f * sc);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.30f + seed * 0.2f + i * 0.33f, 1.0f);
        FilledCircle4(cupX + sinf(t * 4.0f + seed) * 0.14f * sc,
                      cupY + (0.22f + t * 0.80f) * sc,
                      (0.06f + t * 0.13f) * sc, 232, 238, 246,
                      (unsigned char)(130 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

// Tea stall with seated customers
void DrawTeaStall4()
{
    const float x = TEA_X4, y = TEA_Y4, S = TEA_S4;
    DrawGroundGlow4(x, y - 0.2f, 11.0f * S, 2.6f * S,
                    255, 196, 118, (unsigned char)(96 * NightT4() + 26));
    glColor3ub(86, 62, 42);
    glLineWidth(2.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 3.9f * S, y);        glVertex2f(x - 3.9f * S, y + 5.0f * S);
        glVertex2f(x + 3.9f * S, y);        glVertex2f(x + 3.9f * S, y + 5.0f * S);
    glEnd();
    glColor3ub(128, 88, 54);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.3f * S, y + 0.30f * S);
        glVertex2f(x + 1.5f * S, y + 0.30f * S);
        glVertex2f(x + 1.5f * S, y + 2.15f * S);
        glVertex2f(x - 3.3f * S, y + 2.15f * S);
    glEnd();
    glColor4ub(88, 58, 34, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float px = x + (-3.3f + i * 0.96f) * S;
            glVertex2f(px, y + 0.30f * S); glVertex2f(px, y + 2.15f * S);
        }
    glEnd();
    glColor3ub(172, 128, 82);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.5f * S, y + 2.15f * S);
        glVertex2f(x + 1.7f * S, y + 2.15f * S);
        glVertex2f(x + 1.7f * S, y + 2.42f * S);
        glVertex2f(x - 3.5f * S, y + 2.42f * S);
    glEnd();
    float ux = x - 2.3f * S, uy = y + 2.42f * S;
    glColor3ub(196, 146, 62);
    glBegin(GL_POLYGON);
        glVertex2f(ux - 0.62f * S, uy);
        glVertex2f(ux + 0.62f * S, uy);
        glVertex2f(ux + 0.70f * S, uy + 0.95f * S);
        glVertex2f(ux + 0.42f * S, uy + 1.62f * S);
        glVertex2f(ux - 0.42f * S, uy + 1.62f * S);
        glVertex2f(ux - 0.70f * S, uy + 0.95f * S);
    glEnd();
    glColor3ub(232, 186, 96);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(ux - 0.34f * S, uy + 0.18f * S);
        glVertex2f(ux - 0.44f * S, uy + 1.36f * S);
    glEnd();
    glColor3ub(120, 84, 36);
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(ux + 0.60f * S, uy + 0.60f * S);
        glVertex2f(ux + 1.00f * S, uy + 0.42f * S);
    glEnd();
    FilledCircle4(ux, uy + 1.74f * S, 0.22f * S, 150, 108, 48, 255);
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.26f + i * 0.2f, 1.0f);
        FilledCircle4(ux + sinf(t * 3.4f + i) * 0.42f * S,
                      uy + (1.9f + t * 3.0f) * S,
                      (0.16f + t * 0.42f) * S, 236, 240, 246,
                      (unsigned char)(150 * (1.0f - t)));
    }
    float kx = x - 0.5f * S, ky = y + 2.42f * S;
    glColor3ub(58, 58, 66);
    glBegin(GL_POLYGON);
        glVertex2f(kx - 0.46f * S, ky);
        glVertex2f(kx + 0.46f * S, ky);
        glVertex2f(kx + 0.38f * S, ky + 0.68f * S);
        glVertex2f(kx - 0.38f * S, ky + 0.68f * S);
    glEnd();
    glColor3ub(40, 40, 48);
    glLineWidth(1.6f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(kx - 0.30f * S, ky + 0.66f * S);
        glVertex2f(kx,             ky + 1.02f * S);
        glVertex2f(kx + 0.30f * S, ky + 0.66f * S);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(kx + 0.44f * S, ky + 0.42f * S);
        glVertex2f(kx + 0.86f * S, ky + 0.62f * S);
    glEnd();
    float flick = 0.7f + 0.3f * sinf(firePhase4 * 3.4f);
    glColor4ub(255, 150, 60, (unsigned char)(200 * flick));
    glBegin(GL_TRIANGLES);
        glVertex2f(kx - 0.26f * S, ky - 0.30f * S);
        glVertex2f(kx + 0.26f * S, ky - 0.30f * S);
        glVertex2f(kx, ky + 0.06f * S * flick);
    glEnd();
    for (int i = 0; i < 5; i++) {
        float gx = x + (0.35f + i * 0.28f) * S;
        glColor3ub(216, 226, 232);
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.09f * S, ky);
            glVertex2f(gx + 0.09f * S, ky);
            glVertex2f(gx + 0.08f * S, ky + 0.34f * S);
            glVertex2f(gx - 0.08f * S, ky + 0.34f * S);
        glEnd();
        glColor3ub(178, 118, 58);
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.08f * S, ky + 0.02f * S);
            glVertex2f(gx + 0.08f * S, ky + 0.02f * S);
            glVertex2f(gx + 0.08f * S, ky + 0.18f * S);
            glVertex2f(gx - 0.08f * S, ky + 0.18f * S);
        glEnd();
    }
    float pour = sinf(pedTimer4 * 0.8f) * 0.10f;
    glColor3ub(74, 96, 120);
    glLineWidth(5.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.60f * S, y + 2.20f * S);
        glVertex2f(x + 0.52f * S, y + 3.62f * S);
    glEnd();
    glColor3ub(58, 78, 100);
    glLineWidth(2.2f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.54f * S, y + 3.42f * S);
        glVertex2f(x - 0.10f * S, y + (3.02f + pour) * S);
    glEnd();
    FilledCircle4(x + 0.52f * S, y + 3.96f * S, 0.32f * S, 230, 192, 156, 255);
    FilledCircle4(x + 0.52f * S, y + 4.16f * S, 0.30f * S, 190, 78, 66, 255);
    glColor3ub(168, 74, 62);
    glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 12; i++) {
            float t  = (float)i / 12.0f;
            float px = x + (-4.4f + 8.8f * t) * S;
            float sag = -sinf(t * PI4) * 0.55f * S;
            glVertex2f(px, y + 5.0f * S + sag);
            glVertex2f(px, y + 4.55f * S + sag);
        }
    glEnd();
    glColor3ub(196, 96, 80);
    for (int i = 0; i <= 9; i++) {
        float t  = (float)i / 9.0f;
        float px = x + (-4.4f + 8.8f * t) * S;
        float sag = -sinf(t * PI4) * 0.55f * S;
        FilledCircle4(px, y + 4.55f * S + sag, 0.22f * S, 196, 96, 80, 255);
    }
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glLineWidth(3.0f * S);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 12; i++) {
            float t  = (float)i / 12.0f;
            float px = x + (-4.4f + 8.8f * t) * S;
            float sag = -sinf(t * PI4) * 0.55f * S;
            glVertex2f(px, y + 5.08f * S + sag);
        }
    glEnd();
    float bx = x - 1.0f * S, by = y + 4.30f * S;
    glColor3ub(40, 40, 48);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(bx, y + 4.75f * S); glVertex2f(bx, by);
    glEnd();
    FilledCircle4(bx, by, 2.30f * S, 255, 206, 132, (unsigned char)(64 * NightT4()));
    FilledCircle4(bx, by, 0.26f * S, 255, 244, 206,
                  (unsigned char)(180 + 75 * NightT4()));
    glColor3ub(112, 78, 48);
    glBegin(GL_QUADS);
        glVertex2f(x - 4.6f * S, y + 0.86f * S);
        glVertex2f(x - 0.4f * S, y + 0.86f * S);
        glVertex2f(x - 0.4f * S, y + 1.12f * S);
        glVertex2f(x - 4.6f * S, y + 1.12f * S);
    glEnd();
    glColor3ub(84, 56, 34);
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 4.3f * S, y + 0.86f * S); glVertex2f(x - 4.3f * S, y);
        glVertex2f(x - 0.8f * S, y + 0.86f * S); glVertex2f(x - 0.8f * S, y);
    glEnd();
    for (int i = 0; i < 2; i++) {
        float sx = x + (2.9f + i * 1.9f) * S;
        glColor3ub(112, 78, 48);
        glBegin(GL_QUADS);
            glVertex2f(sx - 0.46f * S, y + 0.92f * S);
            glVertex2f(sx + 0.46f * S, y + 0.92f * S);
            glVertex2f(sx + 0.46f * S, y + 1.14f * S);
            glVertex2f(sx - 0.46f * S, y + 1.14f * S);
        glEnd();
        glColor3ub(84, 56, 34);
        glLineWidth(1.8f * S);
        glBegin(GL_LINES);
            glVertex2f(sx - 0.34f * S, y + 0.92f * S); glVertex2f(sx - 0.34f * S, y);
            glVertex2f(sx + 0.34f * S, y + 0.92f * S); glVertex2f(sx + 0.34f * S, y);
        glEnd();
    }
    DrawTeaSitter4(x - 3.9f * S, y, S * 0.92f, 0, 148,  58,  64, false);
    DrawTeaSitter4(x - 2.7f * S, y, S * 0.96f, 1,  56,  84, 128, false);
    DrawTeaSitter4(x - 1.5f * S, y, S * 0.90f, 2,  92, 112,  70, false);
    DrawTeaSitter4(x + 2.9f * S, y, S * 0.94f, 3, 132,  96,  52, true );
    DrawTeaSitter4(x + 4.8f * S, y, S * 0.88f, 4,  86,  70, 116, true );
    glLineWidth(1.0f);
}
float carT4      = 0.62f;
float carWheel4  = 0.0f;
// Car driving along the perspective road
void DrawCar4()
{
    const float t  = carT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx - RoadHW4(t) * 0.36f;
    const float depth = 0.80f * (1.0f - t);
    const float ahead = fminf(t + 0.07f, 1.0f);
    const float lean  = (RoadCX4(ahead) - cx) * 0.05f;
    #define CX4(hx, hy) ((hx) + lean * (hy))
    unsigned char r, g, b;
    DrawGroundShadow(x, y, 2.3f * S, 0.34f, (unsigned char)(95 * (0.35f + 0.65f * t)));
    for (int w = 0; w < 2; w++) {
        float wx = (w == 0) ? (x - 1.72f * S) : (x + 1.72f * S);
        float wy = y + 0.74f * S;
        float rx = 0.32f * S, ry = 0.78f * S;
        AirFade4(depth, y, 16, 18, 24, 44, 48, 58, r, g, b);
        glColor3ub(r, g, b);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx, wy);
            for (int k = 0; k <= 16; k++) {
                float a = (float)k / 16.0f * 2.0f * PI4;
                glVertex2f(wx + rx * cosf(a), wy + ry * sinf(a));
            }
        glEnd();
        AirFade4(depth, y, 76, 80, 90, 142, 146, 158, r, g, b);
        glColor3ub(r, g, b);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx, wy);
            for (int k = 0; k <= 12; k++) {
                float a = (float)k / 12.0f * 2.0f * PI4;
                glVertex2f(wx + rx * 0.58f * cosf(a), wy + ry * 0.58f * sinf(a));
            }
        glEnd();
        AirFade4(depth, y, 38, 40, 48, 80, 84, 96, r, g, b);
        glColor3ub(r, g, b);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            for (int k = 0; k < 3; k++) {
                float a = carWheel4 + k * (PI4 / 3.0f);
                glVertex2f(wx - rx * 0.52f * cosf(a), wy - ry * 0.52f * sinf(a));
                glVertex2f(wx + rx * 0.52f * cosf(a), wy + ry * 0.52f * sinf(a));
            }
        glEnd();
    }
    const float HWB = 1.86f * S;
    AirFade4(depth, y, 96, 42, 48, 178, 74, 78, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWB * 0.94f, 0.42f), y + 0.42f * S);
        glVertex2f(CX4(x + HWB * 0.94f, 0.42f), y + 0.42f * S);
        glVertex2f(CX4(x + HWB,         1.10f), y + 1.10f * S);
        glVertex2f(CX4(x + HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x - HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x - HWB,         1.10f), y + 1.10f * S);
    glEnd();
    AirFade4(depth, y, 116, 54, 60, 202, 92, 96, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x + HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x + HWB * 0.90f, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x - HWB * 0.90f, 2.06f), y + 2.06f * S);
    glEnd();
    const float HWC = 1.52f * S;
    AirFade4(depth, y, 86, 36, 42, 162, 66, 70, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWC,         2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC,         2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
    glEnd();
    AirFade4(depth, y, 38, 52, 70, 132, 158, 184, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWC * 0.86f, 2.22f), y + 2.22f * S);
        glVertex2f(CX4(x + HWC * 0.86f, 2.22f), y + 2.22f * S);
        glVertex2f(CX4(x + HWC * 0.76f, 3.10f), y + 3.10f * S);
        glVertex2f(CX4(x - HWC * 0.76f, 3.10f), y + 3.10f * S);
    glEnd();
    AirFade4(depth, y, 70, 28, 34, 138, 56, 60, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(CX4(x - HWC, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
    glEnd();
    AirFade4(depth, y, 26, 26, 34, 58, 58, 70, r, g, b);
    FilledCircle4(CX4(x - 0.66f * S, 2.72f), y + 2.72f * S, 0.28f * S, r, g, b, 235);
    FilledCircle4(CX4(x + 0.70f * S, 2.70f), y + 2.70f * S, 0.25f * S, r, g, b, 200);
    AirFade4(depth, y, 44, 46, 54, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.1f * S);
    glBegin(GL_LINES);
        glVertex2f(CX4(x - 1.06f * S, 2.24f), y + 2.24f * S);
        glVertex2f(CX4(x - 0.24f * S, 2.62f), y + 2.62f * S);
        glVertex2f(CX4(x + 0.30f * S, 2.24f), y + 2.24f * S);
        glVertex2f(CX4(x + 1.08f * S, 2.62f), y + 2.62f * S);
    glEnd();
    AirFade4(depth, y, 22, 22, 28, 52, 54, 62, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - 0.96f * S, 1.16f), y + 1.16f * S);
        glVertex2f(CX4(x + 0.96f * S, 1.16f), y + 1.16f * S);
        glVertex2f(CX4(x + 0.96f * S, 1.66f), y + 1.66f * S);
        glVertex2f(CX4(x - 0.96f * S, 1.66f), y + 1.66f * S);
    glEnd();
    AirFade4(depth, y, 118, 122, 132, 186, 190, 198, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int k = 0; k < 3; k++) {
            float gy = 1.26f + k * 0.16f;
            glVertex2f(CX4(x - 0.92f * S, gy), y + gy * S);
            glVertex2f(CX4(x + 0.92f * S, gy), y + gy * S);
        }
    glEnd();
    AirFade4(depth, y, 40, 42, 50, 88, 92, 102, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - HWB * 0.92f, 0.60f), y + 0.60f * S);
        glVertex2f(CX4(x + HWB * 0.92f, 0.60f), y + 0.60f * S);
        glVertex2f(CX4(x + HWB * 0.92f, 1.02f), y + 1.02f * S);
        glVertex2f(CX4(x - HWB * 0.92f, 1.02f), y + 1.02f * S);
    glEnd();
    glColor4ub(228, 232, 236, 230);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - 0.46f * S, 0.68f), y + 0.68f * S);
        glVertex2f(CX4(x + 0.46f * S, 0.68f), y + 0.68f * S);
        glVertex2f(CX4(x + 0.46f * S, 0.94f), y + 0.94f * S);
        glVertex2f(CX4(x - 0.46f * S, 0.94f), y + 0.94f * S);
    glEnd();
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC * 0.84f, 3.44f), y + 3.44f * S);
        glVertex2f(CX4(x - HWC * 0.84f, 3.44f), y + 3.44f * S);
    glEnd();
    const float lampOn = NightT4();
    const float hlY = y + 1.62f * S;
    const float hlX = 1.40f * S;
    for (int k = 0; k < 2; k++) {
        float lx = CX4(x + (k ? hlX : -hlX), 1.62f);
        FilledCircle4(lx, hlY, 0.30f * S, 226, 228, 224, 235);
        FilledCircle4(lx, hlY, 0.19f * S, 252, 248, 232, 255);
        if (lampOn > 0.05f) {
            FilledCircle4(lx, hlY, 0.78f * S, 255, 240, 200, (unsigned char)(90 * lampOn));
            FilledCircle4(lx, hlY, 0.30f * S, 255, 252, 238, (unsigned char)(255 * lampOn));
        }
    }
    if (lampOn > 0.05f) {
        for (int k = 0; k < 2; k++) {
            float dir = k ? 1.0f : -1.0f;
            DrawSoftEllipse(x + dir * 1.9f * S, y - 1.5f * S, 2.9f * S, 1.1f * S,
                            255, 236, 190, (unsigned char)(95 * lampOn), 3);
        }
        DrawSoftEllipse(x, y - 2.6f * S, 5.2f * S, 1.5f * S,
                        255, 236, 190, (unsigned char)(78 * lampOn), 4);
    }
    for (int i = 0; i < 4; i++) {
        float vt = fmodf(firePhase4 * 0.30f + i * 0.25f, 1.0f);
        float dir = (i & 1) ? 1.0f : -1.0f;
        FilledCircle4(x + dir * (1.4f + vt * 1.3f) * S, y + (1.9f + vt * 0.9f) * S,
                      (0.18f + vt * 0.42f) * S, 228, 234, 242,
                      (unsigned char)(105 * (1.0f - vt) * SeasonWinter4()));
    }
    glLineWidth(1.0f);
    #undef CX4
}
// Timer callback: advances vehicle down the perspective highway into the distance
void UpdateCar4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateCar4, 0); return; }
    if (isAnimating4) {
        float step = 0.0026f / (0.35f + 1.15f * carT4);
        carT4 += step;
        carWheel4 += step * (30.0f + 52.0f * carT4);
        if (carT4 > 1.16f) carT4 = -0.08f;
    }
    glutTimerFunc(30, UpdateCar4, 0);
}
float bikeT4     = 0.34f;
float bikeWheel4 = 0.0f;
// Bicycle on the road
void DrawBike4()
{
    const float t  = bikeT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx + RoadHW4(t) * 0.72f;
    const float depth = 0.80f * (1.0f - t);
    unsigned char r, g, b;
    DrawGroundShadow(x, y, 1.1f * S, 0.30f, (unsigned char)(85 * (0.35f + 0.65f * t)));
    const float wr = 0.80f * S;
    const float rx = 0.15f * S;
    const float wy = y + wr;
    const float sway = sinf(bikeWheel4) * 0.10f * S;
    AirFade4(depth, y, 18, 20, 26, 46, 50, 60, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x - sway * 0.5f, wy + 0.06f * S);
        for (int k = 0; k <= 14; k++) {
            float a = (float)k / 14.0f * 2.0f * PI4;
            glVertex2f(x - sway * 0.5f + rx * 1.55f * cosf(a),
                       wy + 0.06f * S  + wr * 0.94f * sinf(a));
        }
    glEnd();
    AirFade4(depth, y, 26, 28, 34, 60, 64, 74, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.2f * S);
    glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 18; k++) {
            float a = (float)k / 18.0f * 2.0f * PI4;
            glVertex2f(x + sway + rx * cosf(a), wy + wr * sinf(a));
        }
    glEnd();
    AirFade4(depth, y, 88, 92, 104, 150, 154, 166, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) {
            float a = bikeWheel4 + k * (PI4 / 4.0f);
            glVertex2f(x + sway - rx * 0.86f * cosf(a), wy - wr * 0.86f * sinf(a));
            glVertex2f(x + sway + rx * 0.86f * cosf(a), wy + wr * 0.86f * sinf(a));
        }
    glEnd();
    AirFade4(depth, y, 42, 88, 118, 84, 156, 196, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.7f * S);
    glBegin(GL_LINES);
        glVertex2f(x + sway - 0.20f * S, wy);
        glVertex2f(x + sway - 0.05f * S, y + 2.16f * S);
        glVertex2f(x + sway + 0.20f * S, wy);
        glVertex2f(x + sway + 0.05f * S, y + 2.16f * S);
        glVertex2f(x + sway, y + 2.16f * S);
        glVertex2f(x + sway, y + 2.40f * S);
        glVertex2f(x - 0.10f * S, y + 1.05f * S);
        glVertex2f(x + sway, y + 2.10f * S);
    glEnd();
    AirFade4(depth, y, 30, 30, 36, 62, 62, 72, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.4f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + sway - 0.66f * S, y + 2.30f * S);
        glVertex2f(x + sway - 0.20f * S, y + 2.42f * S);
        glVertex2f(x + sway + 0.20f * S, y + 2.42f * S);
        glVertex2f(x + sway + 0.66f * S, y + 2.30f * S);
    glEnd();
    const float pedA = sinf(bikeWheel4), pedB = sinf(bikeWheel4 + PI4);
    const float hipY = y + 2.58f * S, shoY = y + 3.62f * S;
    const float hx   = sway * 0.6f;
    AirFade4(depth, y, 44, 44, 54, 82, 82, 96, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.9f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + hx - 0.22f * S, hipY);
        glVertex2f(x + hx - 0.52f * S, y + 1.62f * S + 0.14f * S * pedA);
        glVertex2f(x + hx - 0.34f * S, y + 0.62f * S + 0.26f * S * pedA);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + hx + 0.22f * S, hipY);
        glVertex2f(x + hx + 0.52f * S, y + 1.62f * S + 0.14f * S * pedB);
        glVertex2f(x + hx + 0.34f * S, y + 0.62f * S + 0.26f * S * pedB);
    glEnd();
    AirFade4(depth, y, 116, 52, 60, 196, 92, 98, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(x + hx - 0.32f * S, hipY);
        glVertex2f(x + hx + 0.32f * S, hipY);
        glVertex2f(x + hx + 0.44f * S, shoY);
        glVertex2f(x + hx - 0.44f * S, shoY);
    glEnd();
    glLineWidth(1.7f * S);
    glBegin(GL_LINES);
        glVertex2f(x + hx - 0.42f * S, shoY - 0.10f * S);
        glVertex2f(x + sway - 0.62f * S, y + 2.34f * S);
        glVertex2f(x + hx + 0.42f * S, shoY - 0.10f * S);
        glVertex2f(x + sway + 0.62f * S, y + 2.34f * S);
    glEnd();
    AirFade4(depth, y, 150, 122, 100, 226, 188, 152, r, g, b);
    FilledCircle4(x + hx, shoY + 0.30f * S, 0.27f * S, r, g, b, 255);
    AirFade4(depth, y, 190, 168, 60, 236, 214, 92, r, g, b);
    FilledCircle4(x + hx, shoY + 0.46f * S, 0.28f * S, r, g, b, 255);
    float lampOn = NightT4();
    if (lampOn > 0.05f) {
        float lx = x + sway, ly = y + 1.78f * S;
        FilledCircle4(lx, ly, 0.34f * S, 255, 232, 176, (unsigned char)(80 * lampOn));
        FilledCircle4(lx, ly, 0.15f * S, 255, 250, 226, (unsigned char)(255 * lampOn));
        DrawSoftEllipse(lx, y - 1.5f * S, 3.0f * S, 1.0f * S,
                        255, 232, 176, (unsigned char)(72 * lampOn), 3);
    }
    glLineWidth(1.0f);
}
// Timer callback: moves winter bicyclist through traffic along perspective road
void UpdateBike4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBike4, 0); return; }
    if (isAnimating4) {
        float step = 0.0011f / (0.35f + 1.15f * bikeT4);
        bikeT4 += step;
        bikeWheel4 += step * (22.0f + 40.0f * bikeT4);
        if (bikeT4 > 1.14f) bikeT4 = -0.05f;
    }
    glutTimerFunc(30, UpdateBike4, 0);
}
// Timer callback: updates passenger cycle rickshaw movement along the street
void UpdateRickshaw4(int)
{
    rickshawT4     = RICKSHAW_PARK_T4;
    rickshawWheel4 = 0.0f;
    glutTimerFunc(120, UpdateRickshaw4, 0);
}

// Building shadow projections on the ground
void DrawBuildingShadows4()
{
    unsigned char sr = MixB4(10, 92), sg = MixB4(14, 104), sb = MixB4(28, 126);
    unsigned char a  = (unsigned char)(MixB4(96, 70));
    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        float half = sh.w * 0.5f + SHOP_EAVE_OVER4;
        float len  = 1.1f + sh.h * 0.055f;
        glBegin(GL_QUADS);
            glColor4ub(sr, sg, sb, a);
            glVertex2f(sh.x - half,          SHOP_BASE_Y4);
            glVertex2f(sh.x + half,          SHOP_BASE_Y4);
            glColor4ub(sr, sg, sb, 0);
            glVertex2f(sh.x + half - len * 0.8f, SHOP_BASE_Y4 - len);
            glVertex2f(sh.x - half - len * 1.5f, SHOP_BASE_Y4 - len);
        glEnd();
    }
    glBegin(GL_QUADS);
        glColor4ub(sr, sg, sb, (unsigned char)(a * 0.9f));
        glVertex2f(-49.0f, SHOP_BASE_Y4);
        glVertex2f(-28.0f, SHOP_BASE_Y4);
        glColor4ub(sr, sg, sb, 0);
        glVertex2f(-29.5f, SHOP_BASE_Y4 - 2.2f);
        glVertex2f(-51.5f, SHOP_BASE_Y4 - 2.2f);
    glEnd();
    glBegin(GL_QUADS);
        glColor4ub(MixB4(16, 120), MixB4(22, 132), MixB4(40, 152), (unsigned char)(MixB4(70, 44)));
        glVertex2f(-60.0f, SHOP_BASE_Y4);
        glVertex2f( 60.0f, SHOP_BASE_Y4);
        glColor4ub(MixB4(16, 120), MixB4(22, 132), MixB4(40, 152), 0);
        glVertex2f( 60.0f, SHOP_BASE_Y4 - 1.4f);
        glVertex2f(-60.0f, SHOP_BASE_Y4 - 1.4f);
    glEnd();
}
// Winter market visitor bundled in heavy winter coat, knit beanie hat, and scarf with walking gait
void DrawPerson4(const Ped4& p, float t)
{
    float sc = DepthScaleRange(p.y, -10.5f, -14.0f, 0.88f, 1.12f);
    DrawGroundShadow(p.x, p.y, 0.6f * sc, 0.25f, 78);
    BeginDepthSprite(p.x, p.y, sc);
    float bob = sinf(t*3.0f + p.phase) * 0.1f;
    float hipX = p.x, hipY = p.y + 1.0f + bob;
    glColor3ub(30, 30, 35);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2f(hipX, hipY); glVertex2f(hipX + 0.22f*sinf(t*4.0f+p.phase), p.y);
        glVertex2f(hipX, hipY); glVertex2f(hipX - 0.22f*sinf(t*4.0f+p.phase), p.y);
    glEnd();
    glColor3ub(p.coatR, p.coatG, p.coatB);
    glLineWidth(5.0f);
    glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(hipX, hipY+1.15f); glEnd();
    FilledCircle4(hipX, hipY+1.45f, 0.3f, 225, 185, 145, 255);
    glColor3ub(40, 40, 45);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX-0.3f, hipY+1.65f); glVertex2f(hipX+0.3f, hipY+1.65f); glVertex2f(hipX, hipY+2.2f);
    glEnd();
    glColor4ub(246, 249, 255, 230);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX-0.16f*SnowDepth4(), hipY+1.95f);
        glVertex2f(hipX+0.16f*SnowDepth4(), hipY+1.95f);
        glVertex2f(hipX, hipY+2.2f);
    glEnd();
    for (int i = 0; i < 3; i++) {
        float bt = fmodf(firePhase4 * 0.26f + p.phase * 0.17f + i * 0.33f, 1.0f);
        FilledCircle4(hipX + p.dir * (0.35f + bt * 1.3f), hipY + 1.42f + bt * 0.45f,
                      0.09f + bt * 0.22f, 226, 234, 244,
                      (unsigned char)(105 * (1.0f - bt)));
    }
    EndDepthSprite();
}
// Playful puppy frolicking in the fresh snow with wagging tail and bounding leaps
void DrawDogPlay4(float x, float y, float scale, float t)
{
    float sc = scale;
    float bodyX = x;
    float bodyY = y;
    float playPhase = t * 0.55f;
    DrawGroundShadow(bodyX, bodyY, 0.72f * sc, 0.18f, 68);
    BeginDepthSprite(bodyX, bodyY, sc);
    float lean = sinf(playPhase) * 8.0f;
    glPushMatrix();
        glTranslatef(bodyX, bodyY, 0.0f);
        glRotatef(lean, 0.0f, 0.0f, 1.0f);
        glTranslatef(-bodyX, -bodyY, 0.0f);
        glColor3ub(118, 90, 58);
        glBegin(GL_POLYGON);
            glVertex2f(bodyX - 0.80f, bodyY + 0.22f);
            glVertex2f(bodyX - 0.20f, bodyY + 0.70f);
            glVertex2f(bodyX + 0.70f, bodyY + 0.62f);
            glVertex2f(bodyX + 0.92f, bodyY + 0.18f);
            glVertex2f(bodyX + 0.72f, bodyY - 0.28f);
            glVertex2f(bodyX - 0.60f, bodyY - 0.30f);
            glVertex2f(bodyX - 0.88f, bodyY - 0.02f);
        glEnd();
        glColor3ub(78, 60, 42);
        glLineWidth(2.2f);
        glBegin(GL_LINES);
            glVertex2f(bodyX - 0.38f, bodyY + 0.10f); glVertex2f(bodyX - 0.44f, bodyY - 0.52f);
            glVertex2f(bodyX + 0.02f,  bodyY + 0.08f); glVertex2f(bodyX + 0.00f,  bodyY - 0.54f);
            glVertex2f(bodyX + 0.40f, bodyY + 0.00f); glVertex2f(bodyX + 0.42f, bodyY - 0.52f);
        glEnd();
        FilledCircle4(bodyX + 0.90f, bodyY + 0.42f, 0.28f, 132, 102, 66, 255);
        glColor3ub(220, 182, 140);
        glBegin(GL_POLYGON);
            glVertex2f(bodyX + 1.08f, bodyY + 0.34f);
            glVertex2f(bodyX + 1.38f, bodyY + 0.40f);
            glVertex2f(bodyX + 1.18f, bodyY + 0.16f);
            glVertex2f(bodyX + 1.02f, bodyY + 0.18f);
        glEnd();
        glColor3ub(90, 68, 42);
        glBegin(GL_TRIANGLES);
            glVertex2f(bodyX + 0.95f, bodyY + 0.70f); glVertex2f(bodyX + 1.02f, bodyY + 0.86f); glVertex2f(bodyX + 1.12f, bodyY + 0.64f);
            glVertex2f(bodyX + 1.15f, bodyY + 0.76f); glVertex2f(bodyX + 1.20f, bodyY + 0.90f); glVertex2f(bodyX + 1.30f, bodyY + 0.68f);
        glEnd();
        glColor3ub(32, 32, 32);
        FilledCircle4(bodyX + 0.98f, bodyY + 0.46f, 0.05f, 20, 20, 20, 255);
        FilledCircle4(bodyX + 1.12f, bodyY + 0.46f, 0.05f, 20, 20, 20, 255);
        glColor3ub(90, 68, 42);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(bodyX - 0.92f, bodyY + 0.18f);
            glVertex2f(bodyX - 1.28f + 0.18f * sinf(t * 2.4f), bodyY + 0.56f + 0.20f * cosf(t * 2.4f));
        glEnd();
    glPopMatrix();
    float bx = bodyX + 1.1f + 0.18f * sinf(playPhase * 1.7f);
    float by = bodyY + 1.05f + 0.10f * cosf(playPhase * 2.3f);
    glColor3ub(220, 90, 70);
    FilledCircle4(bx, by, 0.18f, 220, 90, 70, 255);
    glColor3ub(245, 200, 180);
    glBegin(GL_LINES);
        for (int i = 0; i < 6; i++) {
            float a = playPhase + i * (PI4 / 3.0f);
            glVertex2f(bx, by);
            glVertex2f(bx + 0.12f * cosf(a), by + 0.12f * sinf(a));
        }
    glEnd();
    EndDepthSprite();
}
// Draw all market pedestrians
void DrawPedestrians4()
{
    int order[NUM_PEDS4];
    for (int i = 0; i < NUM_PEDS4; i++) order[i] = i;
    for (int i = 1; i < NUM_PEDS4; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && peds4[order[j]].y < peds4[key].y) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_PEDS4; i++) DrawPerson4(peds4[order[i]], pedTimer4);
}
// Timer callback: updates market visitors browsing between stalls and greeting friends
void UpdatePedestrians4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdatePedestrians4, 0); return; }
    if (isAnimating4) {
        pedTimer4 += 0.12f;
        for (int i = 0; i < NUM_PEDS4; i++) {
            peds4[i].x += peds4[i].speed * peds4[i].dir;
            if (peds4[i].dir > 0 && peds4[i].x > 65.0f)  peds4[i].x = -65.0f;
            if (peds4[i].dir < 0 && peds4[i].x < -65.0f) peds4[i].x = 65.0f;
        }
    }
    glutTimerFunc(30, UpdatePedestrians4, 0);
}
constexpr int SNOW_FAR_END4  = 150;
constexpr int SNOW_MID_END4  = 250;
// Initializes 3 depth tiers of falling snowflakes (distant, midground, and large foreground flakes)
void InitSnow4()
{
    for (int i = 0; i < MAX_SNOW4; i++) {
        snowX4[i] = -62.0f + (rand()%1240)/10.0f;
        snowY4[i] = (rand()%800-400)/10.0f + 20.0f;
        snowDrift4[i] = ((rand()%100)-50)/500.0f;
        snowSize4[i] = 0.15f + (rand()%15)/100.0f;
    }
}
inline int SnowBandCount4(int first, int last)
{
    int n = last - first;
    if (snowIntensity4 == 0) return first + n / 3;
    if (snowIntensity4 == 1) return first + (n * 2) / 3;
    return last;
}
// Renders a specific depth tier of falling snowflakes with custom flake size and alpha
void DrawSnowBand4(int first, int last, float sizeMul,
                   unsigned char alpha, bool blurred)
{
    int end = SnowBandCount4(first, last);
    const float winter = SeasonWinter4();
    const float autumnV = SeasonAutumn4();
    const unsigned char leafCols[4][3] = {
        {186,  92,  38}, {214, 142,  48}, {154,  70,  44}, {198, 168,  62}
    };
    for (int i = first; i < end; i++) {
        float r = snowSize4[i] * sizeMul;
        if (winter > 0.02f) {
            unsigned char a = (unsigned char)(alpha * winter);
            if (blurred) {
                FilledCircle4(snowX4[i], snowY4[i], r * 1.9f, 255, 255, 255, (unsigned char)(a / 4));
                FilledCircle4(snowX4[i] + r * 0.3f, snowY4[i] - r * 0.2f, r, 255, 255, 255, a);
            } else {
                FilledCircle4(snowX4[i], snowY4[i], r, 255, 255, 255, a);
            }
        }
        if (autumnV > 0.02f) {
            unsigned char a = (unsigned char)(alpha * autumnV);
            const unsigned char* c = leafCols[i % 4];
            float spin = snowY4[i] * 0.55f + i;
            float lr   = r * 2.3f;
            glColor4ub(c[0], c[1], c[2], a);
            glBegin(GL_TRIANGLES);
                glVertex2f(snowX4[i] + lr * cosf(spin),
                           snowY4[i] + lr * 0.55f * sinf(spin));
                glVertex2f(snowX4[i] + lr * cosf(spin + 2.2f),
                           snowY4[i] + lr * 0.55f * sinf(spin + 2.2f));
                glVertex2f(snowX4[i] + lr * cosf(spin + 4.2f),
                           snowY4[i] + lr * 0.55f * sinf(spin + 4.2f));
            glEnd();
            glColor4ub((unsigned char)(c[0] * 0.6f), (unsigned char)(c[1] * 0.6f),
                       (unsigned char)(c[2] * 0.6f), a);
            glLineWidth(1.0f);
            glBegin(GL_LINES);
                glVertex2f(snowX4[i] + lr * cosf(spin),
                           snowY4[i] + lr * 0.55f * sinf(spin));
                glVertex2f(snowX4[i] + lr * cosf(spin + 3.2f) * 0.5f,
                           snowY4[i] + lr * 0.55f * sinf(spin + 3.2f) * 0.5f);
            glEnd();
        }
    }
}
// Renders dense veil of tiny, distant snowflakes falling gently in background
void DrawSnowFar4()
{
    DrawSnowBand4(0,              SNOW_FAR_END4, 0.55f,  95, false);
}
// Renders medium-depth snowflake particles drifting around market chalets
void DrawSnowMid4()
{
    DrawSnowBand4(SNOW_FAR_END4,  SNOW_MID_END4, 1.00f, 205, false);
}
// Renders large, soft, close-up snowflakes tumbling right past the camera lens
void DrawSnowNear4()
{
    DrawSnowBand4(SNOW_MID_END4,  MAX_SNOW4,     1.95f, 215, true );
}
// Timer callback: simulates snowflake physics (gravity, wind gusts, swirling turbulence)
void UpdateSnow4(int)
{
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSnow4, 0); return; }
    if (isAnimating4) {
        float tgt = SnowCoverTarget4();
        snowCover4 += (tgt - snowCover4) * 0.02f;
        float base = 0.22f + snowIntensity4 * 0.14f;
        for (int i = 0; i < MAX_SNOW4; i++) {
            float bandSpeed = (i < SNOW_FAR_END4) ? 0.50f
                            : (i < SNOW_MID_END4) ? 1.00f : 1.65f;
            snowY4[i] -= base * bandSpeed;
            snowX4[i] += snowDrift4[i] * bandSpeed;
            if (snowY4[i] < -40.0f) {
                snowY4[i] = 40.0f;
                snowX4[i] = -62.0f + (rand()%1240)/10.0f;
            }
            if (snowX4[i] >  63.0f) snowX4[i] = -63.0f;
            if (snowX4[i] < -63.0f) snowX4[i] =  63.0f;
        }
    }
    glutTimerFunc(25, UpdateSnow4, 0);
}
const char* kTitle = "Riverfront Market Life";
// Initializes scenario state, resets animation timers, and pre-allocates particle buffers
void Init()
{
    for (int i = 0; i < NUM_STARS4; i++) {
        stars4[i].x = -60.0f + (rand()%1200)/10.0f;
        stars4[i].y =   4.0f + (rand()%350)/10.0f;
        stars4[i].twinkle = (rand()%628)/100.0f;
        stars4[i].rate    = 0.9f + (rand()%220)/100.0f;
        int roll = rand() % 100;
        if      (roll < 62) { stars4[i].bucket = 0; stars4[i].mag = 0.22f + (rand()%26)/100.0f; }
        else if (roll < 90) { stars4[i].bucket = 1; stars4[i].mag = 0.45f + (rand()%32)/100.0f; }
        else                { stars4[i].bucket = 2; stars4[i].mag = 0.72f + (rand()%28)/100.0f; }
        int tint = rand() % 100;
        if      (tint < 55) { stars4[i].r = 255; stars4[i].g = 255; stars4[i].b = 255; }
        else if (tint < 80) { stars4[i].r = 206; stars4[i].g = 222; stars4[i].b = 255; }
        else if (tint < 93) { stars4[i].r = 255; stars4[i].g = 244; stars4[i].b = 214; }
        else                { stars4[i].r = 255; stars4[i].g = 216; stars4[i].b = 196; }
    }
    for (int i = 0; i < NUM_BRIGHT_STARS4; i++) {
        float bx, by;
        int guard = 0;
        do {
            bx = -56.0f + (rand()%1120)/10.0f;
            by =  12.0f + (rand()%240)/10.0f;
        } while (++guard < 20 &&
                 (bx-moonX4)*(bx-moonX4) + (by-moonY4)*(by-moonY4) < 100.0f);
        brightStars4[i].x = bx;
        brightStars4[i].y = by;
        brightStars4[i].r = 0.28f + (rand()%26)/100.0f;
        brightStars4[i].phase = (rand()%628)/100.0f;
    }
    for (int i = 0; i < NUM_BIRDS4; i++) {
        birds4[i].dir = (i % 4 == 0) ? -1 : 1;
        RespawnBird4(birds4[i], -66.0f + (rand()%1320)/10.0f);
    }
    for (int i = 0; i < NUM_SHOPS4; i++)
        for (int w = 0; w < 5; w++) shops4[i].lit[w] = (rand()%100) < 60;
    for (int s = 0; s < NUM_LIGHT_SPANS4; s++)
        for (int b = 0; b < BULBS_PER_SPAN4; b++) bulbLit4[s][b] = (rand()%100) < 70;
    for (int i = 0; i < MAX_SMOKE4; i++) chimneySmoke4[i].active = false;
    for (int i = 0; i < MAX_STEAM4; i++) stallSteam4[i].active = false;
    for (int i = 0; i < MAX_FIREWORKS4; i++) fireworks4[i].state = FW_INACTIVE;
    for (int i = 0; i < MAX_EMBERS4; i++)    embers4[i].active = false;
    snowCover4 = SnowCoverTarget4();
    InitSnow4();
    glutTimerFunc(0, UpdateSnow4, 0);
    glutTimerFunc(0, UpdateFerrisWheel4, 0);
    glutTimerFunc(0, UpdateTwinkle4, 0);
    glutTimerFunc(0, UpdateSkaters4, 0);
    glutTimerFunc(0, UpdateSled4, 0);
    glutTimerFunc(0, UpdatePedestrians4, 0);
    glutTimerFunc(0, UpdateFire4, 0);
    glutTimerFunc(0, UpdateStallSteam4, 0);
    glutTimerFunc(0, UpdateChimneySmoke4, 0);
    glutTimerFunc(0, UpdateSleigh4, 0);
    glutTimerFunc(0, UpdateBirds4, 0);
    glutTimerFunc(0, UpdateMetroRail4, 0);
    glutTimerFunc(0, UpdatePendulum4, 0);
    glutTimerFunc(0, UpdateFireworks4, 0);
    glutTimerFunc(0, UpdateAurora4, 0);
    glutTimerFunc(0, UpdateSledRun4, 0);
    glutTimerFunc(0, UpdateFog4, 0);
    glutTimerFunc(0, UpdateRickshaw4, 0);
    glutTimerFunc(0, UpdateCar4, 0);
    glutTimerFunc(0, UpdateBike4, 0);
    glutTimerFunc(0, UpdateSeason4, 0);
    glutTimerFunc(0, UpdateBoats4, 0);
    glutTimerFunc(0, UpdateRiverPedestrians4, 0);
}
// Master rendering routine: clears buffers, sets projection, and draws complete scenario composition
void Draw()
{
    glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    DrawSky4();
    DrawStars4();
    DrawMoon4();
    DrawAurora4();
    DrawBirds4();
    DrawSnowFar4();
    DrawDistantBuildings4();
    DrawRoadsideBlocks4();
    DrawMetroRail4();
    DrawFireworks4();
    DrawSleigh4();
    DrawShops4();
    DrawChimney4();
    DrawClockTower4();
    DrawPlazaLamp4(-10.0f, 0.90f);
    DrawPlazaLamp4(  6.0f, 0.72f);
    DrawPlazaLamp4( 30.0f, 0.58f);
    DrawLightSpans4();
    DrawFireworkGroundFlash4();
    DrawPlazaGround4();
    DrawPerspectiveRoad4();
    DrawFerrisWheel4();
    DrawBuildingShadows4();
    DrawLightPools4();
    DrawSnowAccumulation4();
    DrawRink4();
    DrawFountainAutumn4();
    DrawIceSculptures4();
    DrawStage4();
    DrawStalls4();
    DrawFirePit4();
    DrawChristmasTree4();
    DrawGiftBoxes4();
    DrawSanta4();
    DrawChestnutRoaster4();
    DrawSnowmanFamily4();
    DrawPlantersAutumn4();
    DrawNutcracker4(-9.0f);
    DrawSkaters4();
    DrawSled4();
    DrawChimneySmoke4();
    DrawStallSteam4();
    DrawCarolers4();
    DrawPedestrians4();
    DrawDogPlay4(32.0f, -15.8f, 1.20f, pedTimer4);
    DrawTeaStall4();
    {
        struct RoadUser
        {
            float t; void (*draw)();
        };
        RoadUser users[3] = { { rickshawT4, DrawRickshaw4 },
                              { carT4,      DrawCar4      },
                              { bikeT4,     DrawBike4     } };
        for (int i = 1; i < 3; i++) {
            RoadUser key = users[i];
            int j = i - 1;
            while (j >= 0 && users[j].t > key.t) { users[j+1] = users[j]; j--; }
            users[j+1] = key;
        }
        for (int i = 0; i < 3; i++) users[i].draw();
    }
    DrawFog4(-6.0f, 14.0f, 105);
    DrawSnowMid4();
    DrawTerraceWall4();
    DrawSledRun4();
    DrawStairsAll4();
    DrawRiver4();
    DrawBoats4();
    DrawGoodsUnloading4();
    DrawRiverPedestrians4();
    DrawForegroundCrowd4();
    DrawForegroundLamp4(-58.0f, 0.90f);
    DrawForegroundLamp4( 56.0f, 0.78f);
    DrawFog4(-40.0f, -20.0f, 70);
    DrawSnowNear4();
    static const char* const hud[] = {
        "1 light snow   2 medium   3 heavy",
        "4 daytime      5 night (season is kept)",
        "S  winter / autumn",
        "L  warm / multicolour lights    F  snow squall",
        "X  set off the sled, fireworks and aurora",
        "SPACE pause    H help    N/B change scene    ESC quit",
        nullptr
    };
    DrawSceneHUD(kTitle, hud);
}
// Scenario keyboard handler: dispatches scenario-specific hotkeys and feature toggles
void Keyboard(unsigned char key, int , int )
{
    switch (key) {
        case '1': snowIntensity4 = 0; break;
        case '2': snowIntensity4 = 1; break;
        case '3': snowIntensity4 = 2; break;
        case 'l': case 'L': multicolorLights4 = !multicolorLights4; break;
        case 'f': case 'F': fogMode4 = !fogMode4; break;
        case 's': case 'S':
            seasonTarget4 = (seasonTarget4 > 0.5f) ? 0.0f : 1.0f;
            break;
        case '4': dayTarget4 = 1.0f; break;
        case '5':
            dayTarget4        = 0.0f;
            multicolorLights4 = false;
            fogMode4          = false;
            break;
        case 'x': case 'X':
            sledActive4 = true;  sledX4 = -75.0f;
            auroraActive4 = true; auroraTimer4 = 0.0f;
            fireworkCooldown4 = 1;
            break;
    }
    glutPostRedisplay();
}
// Scenario mouse handler: toggles scene animation play/pause state
void Mouse(int button, int state, int , int )
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)  isAnimating4 = true;
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) isAnimating4 = false;
    glutPostRedisplay();
}
}

// ============================================================================
//  MAIN APPLICATION GLUE -- scenario switching, display loop, input handling
// ============================================================================
struct ScenarioInfo
{
    const char* title; const char* subtitle; AppScreen screen;
};
ScenarioInfo scenarios[NUM_SCENARIOS] = {
    { Scenario1_CoastalCity::kTitle, "Harbour city - day/night, storms, lighthouse",   SCENARIO_1 },
    { Scenario2::kTitle,             "Rainy neon downtown - train, storm, wet street", SCENARIO_2 },
    { Scenario3::kTitle,             "Riverside park - bridge, balloon, four phases",  SCENARIO_3 },
    { Scenario4::kTitle,             "Snowy night market - ferris wheel, sleigh, fog", SCENARIO_4 },
};
float switchFade  = 0.0f;
float titleCardT  = 0.0f;
bool  firstLaunch = true;
// Switches to a scenario by index with fade transition
void GoToScenarioIndex(int index, bool initial = false)
{
    if (index < 0) index = NUM_SCENARIOS - 1;
    if (index >= NUM_SCENARIOS) index = 0;
    currentScenarioIndex = index;
    currentScreen = scenarios[index].screen;
    isPaused    = false;
    switchFade  = initial ? 0.0f : 1.0f;
    titleCardT  = 0.0f;
    firstLaunch = initial;
    static char windowTitle[160];
    sprintf(windowTitle, "City Life  -  %d/%d  %s",
            index + 1, NUM_SCENARIOS, scenarios[index].title);
    glutSetWindowTitle(windowTitle);
    glutPostRedisplay();
}
// Advances to the next scenario in sequence (wraps 4 back to 1)
void NextScenario()
{
    GoToScenarioIndex(currentScenarioIndex + 1);
}
// Returns to the previous scenario in sequence (wraps 1 back to 4)
void PreviousScenario()
{
    GoToScenarioIndex(currentScenarioIndex - 1);
}
// Displays transient introductory scenario title card overlay with key instructions
void DrawTitleCard()
{
    float hold = firstLaunch ? 4.2f : 2.6f;
    if (titleCardT > hold) return;
    float a = 1.0f;
    if (titleCardT < 0.35f)        a = titleCardT / 0.35f;
    else if (titleCardT > hold - 0.7f) a = (hold - titleCardT) / 0.7f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    const ScenarioInfo& sc = scenarios[currentScenarioIndex];
    glColor4f(0.04f, 0.05f, 0.08f, 0.62f * a);
    glBegin(GL_QUADS);
        glVertex2f(WORLD_LEFT,  6.0f);  glVertex2f(WORLD_RIGHT, 6.0f);
        glVertex2f(WORLD_RIGHT, 20.0f); glVertex2f(WORLD_LEFT,  20.0f);
    glEnd();
    glColor4f(0.85f, 0.88f, 0.96f, 0.55f * a);
    glLineWidth(1.4f);
    glBegin(GL_LINES);
        glVertex2f(-34.0f, 19.4f); glVertex2f(34.0f, 19.4f);
        glVertex2f(-34.0f,  6.6f); glVertex2f(34.0f,  6.6f);
    glEnd();
    glLineWidth(1.0f);
    char counter[48];
    sprintf(counter, "SCENARIO %d OF %d", currentScenarioIndex + 1, NUM_SCENARIOS);
    glColor4f(0.62f, 0.72f, 0.88f, a);
    DrawTextCentered(0.0f, 16.4f, GLUT_BITMAP_HELVETICA_12, counter);
    glColor4f(1.0f, 1.0f, 1.0f, a);
    DrawTextCentered(0.0f, 12.4f, GLUT_BITMAP_TIMES_ROMAN_24, sc.title);
    glColor4f(0.78f, 0.82f, 0.90f, a);
    DrawTextCentered(0.0f, 8.8f, GLUT_BITMAP_HELVETICA_12, sc.subtitle);
    if (firstLaunch) {
        glColor4f(0.95f, 0.86f, 0.55f, a);
        DrawTextCentered(0.0f, 3.2f, GLUT_BITMAP_HELVETICA_12,
                         "press  N  for the next scenario     F1-F4 to jump     H for controls");
    }
}
// Renders full-screen black fade quad during smooth scenario transition cutscenes
void DrawSwitchFade()
{
    if (switchFade <= 0.01f) return;
    glColor4f(0.0f, 0.0f, 0.0f, switchFade);
    glBegin(GL_QUADS);
        glVertex2f(WORLD_LEFT,  WORLD_BOTTOM); glVertex2f(WORLD_RIGHT, WORLD_BOTTOM);
        glVertex2f(WORLD_RIGHT, WORLD_TOP);    glVertex2f(WORLD_LEFT,  WORLD_TOP);
    glEnd();
}
// Advances transition crossfade opacity and title banner display timer
void UpdateTransition(float dt)
{
    if (switchFade > 0.0f) {
        switchFade -= dt * 2.9f;
        if (switchFade < 0.0f) switchFade = 0.0f;
    }
    titleCardT += dt;
}
const unsigned int FRAME_MS = 16;
// Main animation timer driving transitions at ~60fps
void MasterTick(int)
{
    UpdateTransition(FRAME_MS / 1000.0f);
    glutPostRedisplay();
    glutTimerFunc(FRAME_MS, MasterTick, 0);
}
// GLUT display callback: clears and draws the active scenario
void display()
{
    switch (currentScreen) {
        case SCENARIO_1: Scenario1_CoastalCity::Draw();  break;
        case SCENARIO_2: Scenario2::Draw();               break;
        case SCENARIO_3: Scenario3::Draw();               break;
        case SCENARIO_4: Scenario4::Draw();               break;
    }
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    DrawTitleCard();
    DrawSwitchFade();
    glutSwapBuffers();
}
// GLUT keyboard callback: shared keys + per-scenario dispatch
void keyboard(unsigned char key, int x, int y)
{
    if (key == 'n' || key == 'N') { NextScenario();     return; }
    if (key == 'b' || key == 'B') { PreviousScenario(); return; }
    if (key == 27) {
        exit(0);
    }
    if (key == ' ') {
        isPaused = !isPaused;
        glutPostRedisplay();
        return;
    }
    if (key == 'h' || key == 'H') {
        showHelp = !showHelp;
        glutPostRedisplay();
        return;
    }
    switch (currentScreen) {
        case SCENARIO_1: Scenario1_CoastalCity::Keyboard(key, x, y); break;
        case SCENARIO_2: Scenario2::Keyboard(key, x, y);              break;
        case SCENARIO_3: Scenario3::Keyboard(key, x, y);              break;
        case SCENARIO_4: Scenario4::Keyboard(key, x, y);              break;
    }
}
// GLUT special key callback: arrow keys and F1-F4
void special(int key, int , int )
{
    switch (key) {
        case GLUT_KEY_F1: GoToScenarioIndex(0); break;
        case GLUT_KEY_F2: GoToScenarioIndex(1); break;
        case GLUT_KEY_F3: GoToScenarioIndex(2); break;
        case GLUT_KEY_F4: GoToScenarioIndex(3); break;
        case GLUT_KEY_RIGHT: NextScenario();     break;
        case GLUT_KEY_LEFT:  PreviousScenario(); break;
        default: break;
    }
}
// Global GLUT mouse callback: delegates mouse clicks to the active scenario
void mouse(int button, int state, int x, int y)
{
    switch (currentScreen) {
        case SCENARIO_1: Scenario1_CoastalCity::Mouse(button, state, x, y); break;
        case SCENARIO_2: Scenario2::Mouse(button, state, x, y);              break;
        case SCENARIO_3: Scenario3::Mouse(button, state, x, y);              break;
        case SCENARIO_4: Scenario4::Mouse(button, state, x, y);              break;
    }
}
// Configures core OpenGL state (background clear color, ortho projection matrix, blend mode)
void init()
{
    #ifndef GL_MULTISAMPLE
    #define GL_MULTISAMPLE 0x809D
    #endif
    glPointSize(2.0f);
    glLineWidth(2.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
}
// GLUT reshape callback: keeps viewport and projection in sync
void reshape(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    const float targetAspect = (WORLD_RIGHT - WORLD_LEFT) / (WORLD_TOP - WORLD_BOTTOM);
    float windowAspect = (float)w / (float)h;
    int vpW = w, vpH = h, vpX = 0, vpY = 0;
    if (windowAspect > targetAspect) {
        vpW = (int)(h * targetAspect);
        vpX = (w - vpW) / 2;
    } else {
        vpH = (int)(w / targetAspect);
        vpY = (h - vpH) / 2;
    }
    glViewport(vpX, vpY, vpW, vpH);
    viewportPixelWidth = vpW;
    glutPostRedisplay();
}

// Program entry point: sets up GLUT window and starts all scenario timers
int main(int argc, char** argv)
{
    srand((unsigned int)time(nullptr));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("City Life - Group Project (4 Scenarios)");
    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutTimerFunc(0, MasterTick, 0);
    Scenario1_CoastalCity::Init();
    Scenario2::Init();
    Scenario3::Init();
    Scenario4::Init();
    GoToScenarioIndex(0, true);
    glutMainLoop();
    return 0;
}
