// ============================================================================
//  CityLife.cpp  --  Group Project: four scenarios in one program
// ----------------------------------------------------------------------------
//  Four independent scenes, one per team member, each living in its own C++
//  namespace so that four people's globals and functions -- which reuse very
//  generic names like Draw(), Update(), MAX_DROPS -- can share a translation
//  unit without colliding.
//
//    Scenario 1 : Dynamic Coastal City   - day/night coastal port, storm
//                                          weather, helicopter, lighthouse
//    Scenario 2 : Downtown Neon District - rainy night city, neon signage,
//                                          elevated train, wet near sidewalk
//    Scenario 3 : Riverside Park         - cherry blossom, jetty, kayaks,
//                                          Ferris wheel, balloon, four phases
//    Scenario 4 : Winter Night Market    - snowy plaza, Ferris wheel, ice
//                                          rink, river, street traffic,
//                                          winter/autumn seasons
//
//  HOW THIS FILE IS PUT TOGETHER
//  -----------------------------
//  Scenarios 1 and 2 come straight from the group file. Scenarios 3 and 4 are
//  merged in from the standalone builds scenario3final.cpp and
//  scenario4final.cpp, byte-for-byte inside their namespaces.
//
//  One thing to know if you edit this file: Scenario 3 keeps its OWN copies of
//  the shared helpers (DepthScale, DrawGroundShadow, DrawSoftEllipse, the
//  reflection pair and DrawSceneHUD) declared inside namespace Scenario3. The
//  signatures match the shared ones, but eight of the ten bodies differ and
//  that scene is tuned against its versions. C++ name lookup does the right
//  thing automatically: code inside Scenario3 finds Scenario3's, and
//  Scenarios 1, 2 and 4 find the shared ones declared above. Do not "tidy"
//  them away.
//
//  CONTROLS
//  --------
//    The program opens directly on Scenario 1. There is no menu screen.
//
//    Moving between scenarios
//      N  or RIGHT ARROW ... next scenario     (4 wraps round to 1)
//      B  or LEFT  ARROW ... previous scenario (1 wraps round to 4)
//      F1 F2 F3 F4 ......... jump straight to that scenario
//      ESC ................. quit
//
//    Navigation deliberately avoids the number keys: 1/2/3/4 are already
//    used INSIDE every scenario for its own modes, so reusing them would
//    break the day/night and weather controls. F1-F4 arrive through
//    glutSpecialFunc, a separate callback, so they cannot clash with
//    anything a scenario binds now or later.
//
//    Anywhere
//      SPACE ... pause / resume
//      H ....... show / hide the on-screen control panel
//      X ....... force this scenario's rare events (demo helper)
//
//    Per scenario
//      S1 Coastal City .. 1 day, 2 night, 3 cycle weather
//      S2 Downtown ...... 1 dusk, 2 night, 3 dawn, R rain, T storm
//      S3 Riverside Park. 1-4 morning/midday/golden hour/dusk,
//                         W wind level, A blossom/autumn colour
//      S4 Winter Market . 1-3 snow amount, 4/5 day/night, S season,
//                         L light colour, F snow squall
//
//  ############################################################################
//  #  BUILD REQUIREMENT -- READ THIS BEFORE COMPILING                         #
//  #                                                                          #
//  #  This file needs C++11. Code::Blocks defaults to the 1998 standard, and  #
//  #  if you build without changing that you get a wall of errors like:       #
//  #                                                                          #
//  #      error: 'constexpr' does not name a type                             #
//  #      error: 'nullptr' was not declared in this scope                     #
//  #      error: 'MAX_DROPS' was not declared in this scope                   #
//  #                                                                          #
//  #  Those are NOT bugs in the code. They all mean one thing: the C++11      #
//  #  flag is off. Turn it on:                                                #
//  #                                                                          #
//  #    Code::Blocks                                                          #
//  #      Settings -> Compiler... -> Global compiler settings                 #
//  #        -> GNU GCC Compiler -> Compiler settings -> Compiler Flags        #
//  #        -> tick "Have g++ follow the C++11 ISO C++ language standard"     #
//  #      then Build -> Rebuild  (a plain Build reuses stale object files)    #
//  #                                                                          #
//  #    If that checkbox is missing, use the "Other compiler options" tab     #
//  #    on the same dialog and type this on its own line:  -std=gnu++11       #
//  #                                                                          #
//  #    Command line                                                          #
//  #      g++ -std=gnu++11 CityLife.cpp -o CityLife \\                         #
//  #          -lfreeglut -lopengl32 -lglu32                                   #
//  #                                                                          #
//  #  gnu++11 is preferred over c++11 on MinGW: strict ISO mode hides some    #
//  #  C runtime declarations that the MinGW headers need.                     #
//  ############################################################################

#include <windows.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>   // strlen(), used by Scenario 3's HUD panel sizing
#include <ctime>
#include <iostream>

using namespace std;

// ============================================================================
//  SHARED APP STATE  (used by every scenario + the home menu)
// ============================================================================
// There is no home menu any more: the program opens directly on Scenario 1
// and the viewer moves between scenes with N / B / F1-F4. HOME_MENU was
// removed from this enum rather than merely left unused, so no stale code
// path can put the program back into a screen that no longer draws anything.
enum AppScreen { SCENARIO_1, SCENARIO_2, SCENARIO_3, SCENARIO_4 };
AppScreen currentScreen = SCENARIO_1;

// Which scenario is on screen, as a 0-based position in the running order.
// Declared here rather than beside the navigation code at the bottom because
// the shared HUD needs it to print "SCENARIO 2 / 4".
const int NUM_SCENARIOS = 4;
int currentScenarioIndex = 0;

const int WINDOW_WIDTH  = 1200;
const int WINDOW_HEIGHT = 800;

// Live width of the drawing viewport in pixels. reshape() keeps this in step
// with the window so bitmap text stays correctly centred after a resize.
int viewportPixelWidth = WINDOW_WIDTH;

// World-coordinate box every scenario renders into (matches the ortho range
// the original coastal-city scenario already used). Teammates are free to
// call glOrtho with different numbers inside their own Draw() if they need
// a different coordinate range -- just do it at the very start of Draw()
// every frame (see Scenario1_CoastalCity::Draw() below for the pattern) so
// switching scenarios never leaves a stale projection behind.
const float WORLD_LEFT   = -60.0f, WORLD_RIGHT = 60.0f;
const float WORLD_BOTTOM = -40.0f, WORLD_TOP   = 40.0f;

// ---- Small text-rendering helpers (available to every scenario) -----------
inline void DrawText(float x, float y, void* font, const char* text) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(font, *c);
    }
}

inline int TextPixelWidth(void* font, const char* text) {
    int w = 0;
    for (const char* c = text; *c != '\0'; ++c) {
        w += glutBitmapWidth(font, *c);
    }
    return w;
}

// Centers text horizontally on world-x `cx` (bitmap fonts are pixel-sized,
// so the pixel width has to be converted into world units using the ortho
// scale before it can be used to offset a raster position).
inline void DrawTextCentered(float cx, float y, void* font, const char* text) {
    int pixelWidth = TextPixelWidth(font, text);
    float worldWidth = pixelWidth * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
    DrawText(cx - worldWidth * 0.5f, y, font, text);
}

// ---- Shared depth + soft-shape helpers ------------------------------------
// Every scenario draws its ground plane between the horizon (y = -6) and the
// bottom of the world box (y = -40), but until now every figure was drawn at
// exactly the same size no matter how far down the frame it stood, so the
// ground read as a flat wall. DepthScale() converts a standing y into a size
// multiplier: small and pale near the horizon, large and close at the bottom.
//
// The invariant that goes with it: anything drawn LOWER in the frame must
// also be drawn LATER, or it will be composited behind something that is
// supposed to be further away.
const float DEPTH_HORIZON_Y = -6.0f;    // where the ground starts
const float DEPTH_NEAR_Y    = -26.0f;   // "as close as the camera gets"

inline float DepthScale(float y) {
    float t = (y - DEPTH_HORIZON_Y) / (DEPTH_NEAR_Y - DEPTH_HORIZON_Y);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 0.62f + t * (1.36f - 0.62f);
}

// Same idea with an explicit range, for scenarios whose ground band is
// shallower than the full world box (Scenario 2's road, for example).
inline float DepthScaleRange(float y, float farY, float nearY,
                             float farScale, float nearScale) {
    float t = (y - farY) / (nearY - farY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return farScale + t * (nearScale - farScale);
}

// A stack of concentric ellipses with alpha rising toward the middle. Used
// for two different jobs: contact shadows under figures, and the pools of
// light that lamps, fires and neon throw onto the ground.
inline void DrawSoftEllipse(float cx, float cy, float rx, float ry,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char a, int layers) {
    if (layers < 1) layers = 1;
    for (int L = layers; L >= 1; --L) {
        float f = (float)L / (float)layers;                 // 1 = outermost ring
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

// Scales a sprite about its own anchor point, so a drawing function can keep
// using its original world coordinates and still be resized for depth.
inline void BeginDepthSprite(float anchorX, float anchorY, float scale) {
    glPushMatrix();
    glTranslatef(anchorX, anchorY, 0.0f);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-anchorX, -anchorY, 0.0f);
}
inline void EndDepthSprite() { glPopMatrix(); }

// ---- Reflections -----------------------------------------------------------
//  Water is the one surface in this project that has to show what is above
//  it. The pattern is three calls:
//
//      BeginReflection(waterlineY, 0.62f, wobble);
//      DrawTheThing();                       // its own, unchanged geometry
//      EndReflection();
//      WashReflection(cx, halfWidth, waterlineY, depth, waterR,G,B, alpha,
//                     ripplePhase);
//
//  BeginReflection mirrors and squashes the drawing below the waterline;
//  WashReflection then knocks it back toward the water colour and lays
//  horizontal ripple bands across it, which is what stops a mirrored copy
//  looking like an upside-down twin and starts it looking like a reflection.
inline void BeginReflection(float surfaceY, float squash, float wobble) {
    glPushMatrix();
    glTranslatef(wobble, surfaceY, 0.0f);
    glScalef(1.0f, -squash, 1.0f);
    glTranslatef(-wobble, -surfaceY, 0.0f);
}
inline void EndReflection() { glPopMatrix(); }

inline void WashReflection(float cx, float halfW, float surfaceY, float depth,
                           unsigned char r, unsigned char g, unsigned char b,
                           unsigned char nearAlpha, float ripplePhase) {
    // Fade toward the water colour with depth.
    glBegin(GL_QUADS);
        glColor4ub(r, g, b, nearAlpha);
        glVertex2f(cx - halfW, surfaceY);
        glVertex2f(cx + halfW, surfaceY);
        glColor4ub(r, g, b, 255);
        glVertex2f(cx + halfW, surfaceY - depth);
        glVertex2f(cx - halfW, surfaceY - depth);
    glEnd();

    // Ripple bands slicing across the reflection.
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

// Contact shadow: a squashed dark ellipse sitting at a figure's feet. `lean`
// slides it sideways so shadows can fall away from a scene's light source.
inline void DrawGroundShadow(float cx, float cy, float rx, float lean,
                             unsigned char a) {
    DrawSoftEllipse(cx + lean, cy, rx, rx * 0.26f, 18, 22, 30, a, 3);
}

// ---- Shared HUD / help overlay --------------------------------------------
// Every scenario has keyboard controls that were previously undiscoverable.
// Each scenario calls DrawSceneHUD() at the very end of its Draw() with its
// own title and control lines; H toggles the panel, and a compact "H help"
// hint is shown when it is hidden so the user can always find it.
bool showHelp = true;   // toggled with 'H', shared by all scenarios
bool isPaused = false;  // toggled with SPACE, shared by all scenarios

// Draws a translucent panel in the lower-left corner listing this scenario's
// controls. `lines` is a NULL-terminated array of strings.
inline void DrawSceneHUD(const char* title, const char* const* lines) {
    int count = 0;
    while (lines[count] != nullptr) count++;

    // Position in the running order, always on screen. With the home menu
    // gone this is the only thing telling the viewer where they are in the
    // sequence and how to move on, so it is drawn whether or not the help
    // panel is open.
    // sprintf rather than snprintf: old MinGW builds (the toolchain that
    // ships with Code::Blocks) do not declare snprintf under strict ISO
    // mode. The buffer is fixed and the inputs are two small integers.
    char navLine[64];
    sprintf(navLine, "SCENARIO %d / %d", currentScenarioIndex + 1, NUM_SCENARIOS);
    glColor4ub(255, 255, 255, 210);
    DrawText(WORLD_RIGHT - 15.0f, WORLD_TOP - 2.6f, GLUT_BITMAP_HELVETICA_12, navLine);
    glColor4ub(200, 212, 230, 175);
    DrawText(WORLD_RIGHT - 15.0f, WORLD_TOP - 4.4f, GLUT_BITMAP_HELVETICA_10,
             "N next    B back");

    if (!showHelp) {
        // Collapsed state: just a small hint so the panel is rediscoverable.
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

    // Backing panel (kept dark and translucent so it never fights the scene)
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
//  SCENARIO 1  --  Dynamic Coastal City
// ----------------------------------------------------------------------------
//  This is the original CityLife.cpp scenario, unmodified except:
//   (a) it now lives inside its own namespace so it can share a file with
//       three other scenarios without any name collisions, and
//   (b) its 6 top-level #define constants became namespace-scoped
//       `constexpr` values (macros aren't namespaced, so they'd otherwise
//       leak past this "}" and could collide with another scenario's same-
//       named macro elsewhere in the file).
//  Everything else -- every struct, every global, every Draw/Update
//  function -- is byte-for-byte identical to the original file.
// ============================================================================
namespace Scenario1_CoastalCity {

constexpr float PI = 3.1416f;
bool isNight = false;
bool isAnimating = true;

// Dynamic lighting & atmospheric shading globals
float timeOfDay = 0.2f;        // 0.0f (noon/midday) -> 0.25f (sunset) -> 0.5f (midnight) -> 0.75f (sunrise) -> 1.0f (noon)
float stormFactor = 0.0f;      // 0.0f (clear/day/night) -> 1.0f (heavy storm)
float beaconPulse = 0.0f;      // controls pulsating warning beacon lights

// Advanced weather globals
int weatherMode = 0;           // 0: Clear, 1: Rain, 2: Snow
bool isSnowing = false;
float snowAccumulation = 0.0f;
float fogPos = 0.0f;

// Lightning globals
bool lightningActive = false;
float lightningIntensity = 0.0f;
float lightningX[15];
float lightningY[15];
int lightningPoints = 0;

// Astronomical positions for reflections
float moonX_glob = 0.0f;
float moonY_glob = -20.0f;

// Helicopter animation globals
float heliX = -45.0f;
float heliY = 14.0f;
float heliScale = 0.05f;
float heliPropAngle = 0.0f;
bool heliActive = true;

// Global pedestrian walk timer (drives arm/leg sine oscillation)
float pedWalkTimer = 0.0f;

// Traffic light state
int trafficLightState = 0; // 0: Green, 1: Yellow, 2: Red
int trafficLightTimer = 0;

// Billboard ad carousel globals
int activeAdSlide = 0;
int adSlideTimer = 0;

// Vehicle braking flags
bool isCarRedBraking     = false;
bool isCarGreenBraking   = false;
bool isCarYellowBraking  = false;
bool isBusBraking        = false;
bool isCargoTruckBraking = false;
bool isSmallTruckBraking = false;

// Exhaust Particle System
struct ExhaustParticle {
    float x, y;
    float vx, vy;
    float size;
    float alpha;
    int   life;
    bool  active;
};
constexpr int MAX_EXHAUST = 80;
ExhaustParticle exhaustParticles[MAX_EXHAUST];

// Seagull / Bird Flock System
struct Seagull {
    float x, y;
    float baseSpeed;
    float wingAngle;   // wing flap oscillation
    float flapSpeed;   // wing flap frequency
    float scale;
    float targetY;     // flight altitude (lowers in storm)
    int   dir;         // +1 = right, -1 = left
};
constexpr int NUM_SEAGULLS = 5;
Seagull seagulls[NUM_SEAGULLS] = {
    {-40.0f, 22.0f, 0.18f, 0.0f, 0.14f, 0.85f, 22.0f,  1},
    {-25.0f, 25.0f, 0.22f, 1.2f, 0.16f, 0.95f, 25.0f,  1},
    {-10.0f, 20.0f, 0.15f, 2.4f, 0.13f, 0.75f, 20.0f,  1},
    { 10.0f, 26.0f, 0.20f, 0.8f, 0.15f, 0.90f, 26.0f,  1},
    { 30.0f, 23.0f, 0.17f, 1.8f, 0.14f, 0.80f, 23.0f,  1}
};

// Glass Elevator globals (attached to Building2)
float elevatorY = -7.5f;
int elevatorState = 0; // 0: moving up, 1: pause top, 2: moving down, 3: pause bottom
int elevatorPauseTimer = 0;

// Pedestrian system
struct Pedestrian {
    float x;
    float y;
    float speed;
    float scale;
    float phase;     // walking phase offset
    int   dir;       // +1 = right, -1 = left
    // shirt, pants, skin colours
    unsigned char shirtR, shirtG, shirtB;
    unsigned char pantsR, pantsG, pantsB;
    unsigned char skinR,  skinG,  skinB;
    // crosswalk state
    bool crossing;       // currently crossing the road?
    float crossTimer;    // progress of crossing
    float savedX;        // x position before starting to cross
    bool  hasDog;        // NEW: this pedestrian is walking a dog on a leash
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
float fishingBoatX = 8.0f; // anchored fishing boat (bobs in place, does not drift)
float cargoShipPos = 35.0f;
float boatSmall2Pos = -45.0f;

inline unsigned char clampToUByte(float val) {
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

        // Alpha fades towards the lateral edges (t = -1, 1) and at the far distance
        float edgeFade = 1.0f - (t * t);
        float vertexAlpha = maxAlpha * 0.15f * edgeFade;

        glColor4ub(r, g, b, (unsigned char)vertexAlpha);
        glVertex2f(xVertex, yVertex);
    }
    glEnd();
}

//OBJECTS

struct SkyColor {
    float r, g, b;
};

SkyColor lerpColor(SkyColor c1, SkyColor c2, float t) {
    SkyColor result;
    result.r = c1.r + (c2.r - c1.r) * t;
    result.g = c1.g + (c2.g - c1.g) * t;
    result.b = c1.b + (c2.b - c1.b) * t;
    return result;
}

void DrawSky()
{
    // Define sky color presets (top and bottom of screen gradient)
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

    // Interpolate based on timeOfDay progress (0.0 to 1.0)
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

    // Blend in the storm color based on stormFactor when it rains
    if (stormFactor > 0.0f) {
        currentTop = lerpColor(currentTop, StormTop, stormFactor);
        currentBot = lerpColor(currentBot, StormBot, stormFactor);
    }

    // Blend in lightning flash intensity
    if (lightningActive) {
        SkyColor white = {255.0f, 255.0f, 255.0f};
        float flashBlend = lightningIntensity * 0.7f;
        currentTop = lerpColor(currentTop, white, flashBlend);
        currentBot = lerpColor(currentBot, white, flashBlend);
    }

    // Draw the sky gradient quad
    glBegin(GL_QUADS);
    glColor3ub((unsigned char)currentBot.r, (unsigned char)currentBot.g, (unsigned char)currentBot.b);
    glVertex2f(-60, -10);
    glVertex2f(60, -10);
    glColor3ub((unsigned char)currentTop.r, (unsigned char)currentTop.g, (unsigned char)currentTop.b);
    glVertex2f(60, 40);
    glVertex2f(-60, 40);
    glEnd();

    // Render lightning bolt if active
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

    // Draw Sun and Moon in continuous orbital arcs
    float sunAngle = (0.25f - timeOfDay) * 2.0f * PI;
    float sunX = 45.0f * cos(sunAngle);
    float sunY_pos = 28.0f * sin(sunAngle);

    float moonAngle = sunAngle + PI;
    float moonX = 45.0f * cos(moonAngle);
    float moonY_pos = 28.0f * sin(moonAngle);

    // Update global moon coordinates for ocean reflections
    moonX_glob = moonX;
    moonY_glob = moonY_pos;

    // Render Sun (only if above the horizon and not completely storming)
    if (sunY_pos > -12.0f && stormFactor < 1.0f) {
        float sunAlpha = (1.0f - stormFactor) * (sunY_pos > 0.0f ? 1.0f : (sunY_pos + 12.0f) / 12.0f);
        if (sunAlpha > 0.01f) {
            circle(5.5f, sunX, sunY_pos, 255, 255, 200, 50 * sunAlpha);
            circle(4.9f, sunX, sunY_pos, 255, 255, 140, 90 * sunAlpha);
            circle(4.0f, sunX, sunY_pos, 255, 250, 0, 255 * sunAlpha);
        }
    }

    // Render Moon (only if above the horizon and not completely storming)
    if (moonY_pos > -12.0f && stormFactor < 1.0f) {
        float moonAlpha = (1.0f - stormFactor) * (moonY_pos > 0.0f ? 1.0f : (moonY_pos + 12.0f) / 12.0f);
        if (moonAlpha > 0.01f) {
            circle(4.5f, moonX, moonY_pos, 255, 255, 255, 255 * moonAlpha);
            // Crater detailing
            circle(1.0f, moonX - 1.0f, moonY_pos + 1.0f, 200, 200, 200, 255 * moonAlpha);
            circle(0.7f, moonX + 2.0f, moonY_pos - 1.0f, 200, 200, 200, 255 * moonAlpha);
        }
    }
}

void UpdateSun(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSun, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        // Increment time of day (approx. 83 seconds per cycle)
        timeOfDay += 0.0003f;
        if (timeOfDay > 1.0f) timeOfDay -= 1.0f;

        // Smoothly adjust stormFactor when raining
        if (isRaining) {
            if (stormFactor < 1.0f) stormFactor += 0.01f;
        } else {
            if (stormFactor > 0.0f) stormFactor -= 0.01f;
        }

        // Increment beacon light pulsation angle
        beaconPulse += 0.1f;
        if (beaconPulse > 2.0f * PI) beaconPulse -= 2.0f * PI;

        // Maintain compatibility with isNight boolean checks across building/street assets
        isNight = (timeOfDay > 0.32f && timeOfDay < 0.82f);
    }
    glutTimerFunc(25, UpdateSun, 0);
}

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
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCloud, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        cloudPos += 0.1f;
        if (cloudPos > 120.0f) cloudPos = -120.0f;
    }
    glutTimerFunc(25, UpdateCloud, 0);
}

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
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTurbine, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        float speed = 2.0f;
        if (weatherMode == 1) speed = 5.5f;       // rain storm winds
        else if (weatherMode == 2) speed = 3.8f;  // snowy winds

        turbineAngle -= speed;
        if (turbineAngle < -360.0f) turbineAngle += 360.0f;
    }
    glutTimerFunc(25, UpdateTurbine, 0);
}

// ─── Pedestrian drawing ──────────────────────────────────────────────────────
// p    : pedestrian data
// walk : global animation time used to drive limb oscillation
void DrawPerson(const Pedestrian& p, float walk) {
    float lf = getLightFactor();
    float dim = 1.0f - 0.55f * lf;  // darken at night

    glPushMatrix();
    glTranslatef(p.x, p.y, 0.0f);
    glScalef(p.scale, p.scale, 1.0f);
    // flip if walking left
    if (p.dir < 0) glScalef(-1.0f, 1.0f, 1.0f);

    float t    = walk * 6.0f + p.phase;
    float swing = 0.35f * sin(t);       // limb swing angle
    float bob   = 0.05f * fabs(sin(t)); // vertical body bob

    // ── Legs ────────────────────────────────────────────────────────────────
    glLineWidth(2);
    glColor3ub((unsigned char)(p.pantsR*dim),(unsigned char)(p.pantsG*dim),(unsigned char)(p.pantsB*dim));

    // Left leg
    glPushMatrix();
    glTranslatef(-0.12f, bob, 0.0f);
    glRotatef(-swing * 50.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -0.55f);
    glEnd();
    // left shoe
    glBegin(GL_LINES);
    glVertex2f(0.0f, -0.55f);
    glVertex2f(0.15f, -0.55f);
    glEnd();
    glPopMatrix();

    // Right leg
    glPushMatrix();
    glTranslatef(0.12f, bob, 0.0f);
    glRotatef( swing * 50.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -0.55f);
    glEnd();
    // right shoe
    glBegin(GL_LINES);
    glVertex2f(0.0f, -0.55f);
    glVertex2f(0.15f, -0.55f);
    glEnd();
    glPopMatrix();

    // ── Torso ───────────────────────────────────────────────────────────────
    glColor3ub((unsigned char)(p.shirtR*dim),(unsigned char)(p.shirtG*dim),(unsigned char)(p.shirtB*dim));
    glBegin(GL_QUADS);
    glVertex2f(-0.2f, bob);
    glVertex2f( 0.2f, bob);
    glVertex2f( 0.2f, bob + 0.5f);
    glVertex2f(-0.2f, bob + 0.5f);
    glEnd();

    // ── Arms ────────────────────────────────────────────────────────────────
    glLineWidth(2);
    glColor3ub((unsigned char)(p.skinR*dim),(unsigned char)(p.skinG*dim),(unsigned char)(p.skinB*dim));

    // Left arm
    glPushMatrix();
    glTranslatef(-0.2f, bob + 0.4f, 0.0f);
    glRotatef( swing * 55.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-0.1f, -0.4f);
    glEnd();
    glPopMatrix();

    // Right arm
    glPushMatrix();
    glTranslatef( 0.2f, bob + 0.4f, 0.0f);
    glRotatef(-swing * 55.0f, 0,0,1);
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f( 0.1f, -0.4f);
    glEnd();
    glPopMatrix();

    // ── Head ────────────────────────────────────────────────────────────────
    circle(0.2f, 0.0f, bob + 0.7f,
           (unsigned char)(p.skinR*dim),
           (unsigned char)(p.skinG*dim),
           (unsigned char)(p.skinB*dim), 255);

    // Night glow halo from street light (subtle warm tint)
    if (lf > 0.3f) {
        circle(0.28f, 0.0f, bob + 0.7f, 255, 200, 100, (unsigned char)(25 * lf));
    }

    // ── Dog on a leash (only for pedestrians with hasDog) ────────────────────
    if (p.hasDog) {
        float dogT = t * 1.6f;
        float legPh = sin(dogT);
        float dogGround = -0.55f; // same ground line as the person's feet

        // Leash from near the trailing hand down to the collar
        glColor3ub((unsigned char)(90*dim), (unsigned char)(90*dim), (unsigned char)(95*dim));
        glLineWidth(1.2f);
        glBegin(GL_LINES);
        glVertex2f(-0.2f, bob + 0.55f);
        glVertex2f(-0.62f, dogGround + 0.22f);
        glEnd();

        // Body
        glColor3ub((unsigned char)(165*dim), (unsigned char)(120*dim), (unsigned char)(75*dim));
        glBegin(GL_QUADS);
        glVertex2f(-0.85f, dogGround + 0.03f);
        glVertex2f(-0.42f, dogGround + 0.03f);
        glVertex2f(-0.42f, dogGround + 0.22f);
        glVertex2f(-0.85f, dogGround + 0.22f);
        glEnd();

        // Head
        circle(0.10f, -0.90f, dogGround + 0.18f,
               (unsigned char)(165*dim), (unsigned char)(120*dim), (unsigned char)(75*dim), 255);

        // Legs (simple alternating trot)
        glLineWidth(1.3f);
        glBegin(GL_LINES);
        glVertex2f(-0.78f, dogGround + 0.03f); glVertex2f(-0.78f + 0.06f*legPh, dogGround - 0.05f);
        glVertex2f(-0.48f, dogGround + 0.03f); glVertex2f(-0.48f - 0.06f*legPh, dogGround - 0.05f);
        glEnd();

        // Tail (wagging)
        glBegin(GL_LINES);
        glVertex2f(-0.85f, dogGround + 0.18f);
        glVertex2f(-0.97f, dogGround + 0.26f + 0.06f*sin(dogT*2.0f));
        glEnd();
    }

    glPopMatrix();
}

// ─── Park Bench + Seated Pedestrian ─────────────────────────────────────────
void DrawBench(float x)
{
    float lf  = getLightFactor();
    float dim = 1.0f - 0.4f * lf;
    float y = -9.3f;

    glColor3ub((unsigned char)(90*dim), (unsigned char)(60*dim), (unsigned char)(35*dim));
    // Seat
    glBegin(GL_QUADS);
    glVertex2f(x-1.4f, y);
    glVertex2f(x+1.4f, y);
    glVertex2f(x+1.4f, y+0.25f);
    glVertex2f(x-1.4f, y+0.25f);
    glEnd();
    // Backrest
    glBegin(GL_QUADS);
    glVertex2f(x-1.4f, y+0.25f);
    glVertex2f(x+1.4f, y+0.25f);
    glVertex2f(x+1.4f, y+0.9f);
    glVertex2f(x-1.4f, y+0.9f);
    glEnd();
    // Legs
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

    // Bent legs
    glColor3ub((unsigned char)(60*dim), (unsigned char)(60*dim), (unsigned char)(110*dim));
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex2f(x-0.15f, y+0.25f); glVertex2f(x-0.15f, y+0.05f);
    glVertex2f(x-0.15f, y+0.05f); glVertex2f(x+0.15f, y+0.02f);
    glVertex2f(x+0.15f, y+0.25f); glVertex2f(x+0.15f, y+0.05f);
    glVertex2f(x+0.15f, y+0.05f); glVertex2f(x+0.35f, y+0.02f);
    glEnd();

    // Torso
    glColor3ub((unsigned char)(200*dim), (unsigned char)(90*dim), (unsigned char)(70*dim));
    glBegin(GL_QUADS);
    glVertex2f(x-0.18f, y+0.25f);
    glVertex2f(x+0.18f, y+0.25f);
    glVertex2f(x+0.18f, y+0.65f);
    glVertex2f(x-0.18f, y+0.65f);
    glEnd();

    // Arms resting on lap
    glColor3ub((unsigned char)(225*dim), (unsigned char)(180*dim), (unsigned char)(135*dim));
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(x-0.18f, y+0.5f); glVertex2f(x-0.05f, y+0.28f);
    glVertex2f(x+0.18f, y+0.5f); glVertex2f(x+0.20f, y+0.28f);
    glEnd();

    // Head
    circle(0.16f, x, y+0.85f, (unsigned char)(225*dim), (unsigned char)(180*dim), (unsigned char)(135*dim), 255);

    if (lf > 0.3f) {
        circle(0.22f, x, y+0.85f, 255, 200, 100, (unsigned char)(20 * lf));
    }
}

// ─── Seagull Rendering ───────────────────────────────────────────────────────
void DrawSeagull(const Seagull& g) {
    float lf = getLightFactor();
    float dim = 1.0f - 0.45f * lf;

    glPushMatrix();
    glTranslatef(g.x, g.y, 0.0f);
    glScalef(g.scale, g.scale, 1.0f);
    if (g.dir < 0) glScalef(-1.0f, 1.0f, 1.0f);

    float wingOffset = 0.45f * sin(g.wingAngle);

    // Body oval silhouette
    circle(0.25f, 0.0f, 0.0f, (unsigned char)(240*dim), (unsigned char)(240*dim), (unsigned char)(245*dim), 255);
    // Head circle
    circle(0.16f, 0.25f, 0.05f, (unsigned char)(250*dim), (unsigned char)(250*dim), (unsigned char)(255*dim), 255);
    // Beak triangle
    glColor3ub((unsigned char)(255*dim), (unsigned char)(190*dim), 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.38f, 0.08f);
    glVertex2f(0.55f, 0.03f);
    glVertex2f(0.38f, 0.0f);
    glEnd();

    // Left Wing (curved arc)
    glLineWidth(2.5f);
    glColor3ub((unsigned char)(230*dim), (unsigned char)(230*dim), (unsigned char)(235*dim));
    glBegin(GL_LINE_STRIP);
    glVertex2f(0.0f, 0.05f);
    glVertex2f(-0.5f, 0.3f + wingOffset);
    glVertex2f(-1.1f, 0.1f + wingOffset * 1.2f);
    glEnd();

    // Right Wing
    glBegin(GL_LINE_STRIP);
    glVertex2f(0.0f, 0.05f);
    glVertex2f(0.5f, 0.3f + wingOffset);
    glVertex2f(1.1f, 0.1f + wingOffset * 1.2f);
    glEnd();

    // Dark Wingtips
    glColor3ub((unsigned char)(70*dim), (unsigned char)(70*dim), (unsigned char)(80*dim));
    glBegin(GL_LINES);
    glVertex2f(-0.8f, 0.18f + wingOffset * 1.1f);
    glVertex2f(-1.1f, 0.1f + wingOffset * 1.2f);
    glVertex2f(0.8f, 0.18f + wingOffset * 1.1f);
    glVertex2f(1.1f, 0.1f + wingOffset * 1.2f);
    glEnd();

    // Tail feathers
    glColor3ub((unsigned char)(220*dim), (unsigned char)(220*dim), (unsigned char)(225*dim));
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.2f, 0.02f);
    glVertex2f(-0.55f, -0.05f);
    glVertex2f(-0.45f, 0.1f);
    glEnd();

    glPopMatrix();
}

// ─── Exhaust Particle System ─────────────────────────────────────────────────
void EmitExhaustSmoke(float x, float y, float intensity) {
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

void DrawExhaustSmoke() {
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

// ─── Glass Elevator ──────────────────────────────────────────────────────────
void DrawGlassElevator() {
    float lf = getLightFactor();

    // Vertical steel shaft guide cables (right edge of Building2: x = -21.0f)
    glColor3ub(70, 70, 80);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(-20.9f, -8.0f);
    glVertex2f(-20.9f, 12.0f);
    glVertex2f(-19.7f, -8.0f);
    glVertex2f(-19.7f, 12.0f);
    glEnd();

    // Pulley gear at top floor
    circle(0.35f, -20.3f, 12.1f, 100, 100, 110, 255);
    // Pulley gear at bottom floor
    circle(0.35f, -20.3f, -7.8f, 100, 100, 110, 255);

    // Elevator Cabin position
    float cabinX = -20.9f;
    float cabinY = elevatorY;
    float cabinW = 1.3f;
    float cabinH = 2.2f;

    // Interior glow light (ceiling lamp + floor illumination)
    circle(0.45f, cabinX + cabinW * 0.5f, cabinY + cabinH - 0.3f, 255, 235, 130, 255);
    // Translucent cabin illumination cone at night
    if (lf > 0.01f) {
        glColor4ub(255, 220, 110, (unsigned char)(110 * lf));
        glBegin(GL_TRIANGLES);
        glVertex2f(cabinX + cabinW * 0.5f, cabinY + cabinH - 0.3f);
        glVertex2f(cabinX - 0.6f, cabinY - 1.5f);
        glVertex2f(cabinX + cabinW + 0.6f, cabinY - 1.5f);
        glEnd();
    }

    // Glass Walls (translucent quad)
    glColor4ub(180, 230, 255, 170);
    glBegin(GL_QUADS);
    glVertex2f(cabinX, cabinY);
    glVertex2f(cabinX + cabinW, cabinY);
    glVertex2f(cabinX + cabinW, cabinY + cabinH);
    glVertex2f(cabinX, cabinY + cabinH);
    glEnd();

    // Metallic frame top & bottom caps
    glColor3ub(40, 45, 55);
    glBegin(GL_QUADS);
    // Bottom cap
    glVertex2f(cabinX - 0.1f, cabinY - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY + 0.2f);
    glVertex2f(cabinX - 0.1f, cabinY + 0.2f);
    // Top cap
    glVertex2f(cabinX - 0.1f, cabinY + cabinH - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY + cabinH - 0.2f);
    glVertex2f(cabinX + cabinW + 0.1f, cabinY + cabinH + 0.2f);
    glVertex2f(cabinX - 0.1f, cabinY + cabinH + 0.2f);
    glEnd();

    // Glass reflection diagonal line
    glColor4ub(255, 255, 255, (unsigned char)(160 * (1.0f - 0.5f * lf)));
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(cabinX + 0.2f, cabinY + 0.4f);
    glVertex2f(cabinX + cabinW - 0.3f, cabinY + cabinH - 0.4f);
    glEnd();

    // Directional indicator dot on top cap (Green = up, Red = down, Amber = pause)
    if (elevatorState == 0)      circle(0.12f, cabinX + cabinW * 0.5f, cabinY + cabinH, 30, 255, 30, 255);
    else if (elevatorState == 2) circle(0.12f, cabinX + cabinW * 0.5f, cabinY + cabinH, 255, 30, 30, 255);
    else                         circle(0.12f, cabinX + cabinW * 0.5f, cabinY + cabinH, 255, 200, 0, 255);
}

// ─── Neon Storefront Signs ───────────────────────────────────────────────────
void DrawHotelNeonSign(float x, float y) {
    float pulse = 0.5f + 0.5f * sin(beaconPulse * 6.0f);
    float lf = getLightFactor();

    // Canopy / Plaque
    glColor3ub(20, 20, 25);
    glBegin(GL_QUADS);
    glVertex2f(x - 0.2f, y - 0.2f);
    glVertex2f(x + 4.4f, y - 0.2f);
    glVertex2f(x + 4.4f, y + 1.2f);
    glVertex2f(x - 0.2f, y + 1.2f);
    glEnd();

    // Outer Neon Glow Halo (Cyan / Magenta)
    glColor4ub(255, 0, 180, (unsigned char)((70 + 70 * pulse) * (0.3f + 0.7f * lf)));
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.3f, y - 0.3f);
    glVertex2f(x + 4.5f, y - 0.3f);
    glVertex2f(x + 4.5f, y + 1.3f);
    glVertex2f(x - 0.3f, y + 1.3f);
    glEnd();

    // Neon Lettering "HOTEL"
    glColor3ub((unsigned char)(0.0f + 255.0f * pulse), 240, 255);
    glLineWidth(3);
    glBegin(GL_LINES);
    // H
    glVertex2f(x+0.2f, y+0.1f); glVertex2f(x+0.2f, y+0.9f);
    glVertex2f(x+0.7f, y+0.1f); glVertex2f(x+0.7f, y+0.9f);
    glVertex2f(x+0.2f, y+0.5f); glVertex2f(x+0.7f, y+0.5f);
    // O
    glVertex2f(x+1.0f, y+0.1f); glVertex2f(x+1.5f, y+0.1f);
    glVertex2f(x+1.5f, y+0.1f); glVertex2f(x+1.5f, y+0.9f);
    glVertex2f(x+1.5f, y+0.9f); glVertex2f(x+1.0f, y+0.9f);
    glVertex2f(x+1.0f, y+0.9f); glVertex2f(x+1.0f, y+0.1f);
    // T
    glVertex2f(x+1.8f, y+0.9f); glVertex2f(x+2.4f, y+0.9f);
    glVertex2f(x+2.1f, y+0.9f); glVertex2f(x+2.1f, y+0.1f);
    // E
    glVertex2f(x+2.7f, y+0.1f); glVertex2f(x+2.7f, y+0.9f);
    glVertex2f(x+2.7f, y+0.9f); glVertex2f(x+3.2f, y+0.9f);
    glVertex2f(x+2.7f, y+0.5f); glVertex2f(x+3.1f, y+0.5f);
    glVertex2f(x+2.7f, y+0.1f); glVertex2f(x+3.2f, y+0.1f);
    // L
    glVertex2f(x+3.5f, y+0.9f); glVertex2f(x+3.5f, y+0.1f);
    glVertex2f(x+3.5f, y+0.1f); glVertex2f(x+4.0f, y+0.1f);
    glEnd();
}

void DrawCoffeeNeonSign(float x, float y) {
    float pulse = 0.5f + 0.5f * sin(beaconPulse * 7.0f);
    float lf = getLightFactor();

    // Dark Backing plaque
    glColor3ub(25, 15, 10);
    glBegin(GL_QUADS);
    glVertex2f(x - 0.2f, y - 0.2f);
    glVertex2f(x + 4.8f, y - 0.2f);
    glVertex2f(x + 4.8f, y + 1.2f);
    glVertex2f(x - 0.2f, y + 1.2f);
    glEnd();

    // Warm Amber Neon Glow Halo
    glColor4ub(255, 160, 0, (unsigned char)((80 + 60 * pulse) * (0.3f + 0.7f * lf)));
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.3f, y - 0.3f);
    glVertex2f(x + 4.9f, y - 0.3f);
    glVertex2f(x + 4.9f, y + 1.3f);
    glVertex2f(x - 0.3f, y + 1.3f);
    glEnd();

    // "COFFEE" Neon Text
    glColor3ub(255, (unsigned char)(140 + 80 * pulse), 30);
    glLineWidth(3);
    glBegin(GL_LINES);
    // C
    glVertex2f(x+0.7f, y+0.9f); glVertex2f(x+0.2f, y+0.9f);
    glVertex2f(x+0.2f, y+0.9f); glVertex2f(x+0.2f, y+0.1f);
    glVertex2f(x+0.2f, y+0.1f); glVertex2f(x+0.7f, y+0.1f);
    // O
    glVertex2f(x+1.0f, y+0.1f); glVertex2f(x+1.5f, y+0.1f);
    glVertex2f(x+1.5f, y+0.1f); glVertex2f(x+1.5f, y+0.9f);
    glVertex2f(x+1.5f, y+0.9f); glVertex2f(x+1.0f, y+0.9f);
    glVertex2f(x+1.0f, y+0.9f); glVertex2f(x+1.0f, y+0.1f);
    // F
    glVertex2f(x+1.8f, y+0.1f); glVertex2f(x+1.8f, y+0.9f);
    glVertex2f(x+1.8f, y+0.9f); glVertex2f(x+2.3f, y+0.9f);
    glVertex2f(x+1.8f, y+0.5f); glVertex2f(x+2.2f, y+0.5f);
    // F
    glVertex2f(x+2.5f, y+0.1f); glVertex2f(x+2.5f, y+0.9f);
    glVertex2f(x+2.5f, y+0.9f); glVertex2f(x+3.0f, y+0.9f);
    glVertex2f(x+2.5f, y+0.5f); glVertex2f(x+2.9f, y+0.5f);
    // E
    glVertex2f(x+3.2f, y+0.1f); glVertex2f(x+3.2f, y+0.9f);
    glVertex2f(x+3.2f, y+0.9f); glVertex2f(x+3.7f, y+0.9f);
    glVertex2f(x+3.2f, y+0.5f); glVertex2f(x+3.6f, y+0.5f);
    glVertex2f(x+3.2f, y+0.1f); glVertex2f(x+3.7f, y+0.1f);
    // E
    glVertex2f(x+3.9f, y+0.1f); glVertex2f(x+3.9f, y+0.9f);
    glVertex2f(x+3.9f, y+0.9f); glVertex2f(x+4.4f, y+0.9f);
    glVertex2f(x+3.9f, y+0.5f); glVertex2f(x+4.3f, y+0.5f);
    glVertex2f(x+3.9f, y+0.1f); glVertex2f(x+4.4f, y+0.1f);
    glEnd();

    // Animated Steam Riser above cup icon
    glColor4ub(255, 230, 180, (unsigned char)(180 * pulse));
    glLineWidth(2);
    glBegin(GL_LINE_STRIP);
    float steamS = sin(waveMove * 8.0f);
    glVertex2f(x - 0.6f, y + 0.1f);
    glVertex2f(x - 0.6f + 0.15f * steamS, y + 0.6f);
    glVertex2f(x - 0.6f - 0.15f * steamS, y + 1.1f);
    glEnd();
}

void DrawCinemaNeonSign(float x, float y) {
    float lf = getLightFactor();
    float pulse = 0.5f + 0.5f * sin(beaconPulse * 8.0f);

    // Dark Backing Plaque
    glColor3ub(15, 15, 25);
    glBegin(GL_QUADS);
    glVertex2f(x - 0.2f, y - 0.2f);
    glVertex2f(x + 5.2f, y - 0.2f);
    glVertex2f(x + 5.2f, y + 1.2f);
    glVertex2f(x - 0.2f, y + 1.2f);
    glEnd();

    // Outer Neon Green Glow Halo
    glColor4ub(30, 255, 120, (unsigned char)((70 + 70 * pulse) * (0.3f + 0.7f * lf)));
    glLineWidth(4);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.3f, y - 0.3f);
    glVertex2f(x + 5.3f, y - 0.3f);
    glVertex2f(x + 5.3f, y + 1.3f);
    glVertex2f(x - 0.3f, y + 1.3f);
    glEnd();

    // Chasing Marquee Light Bulbs around border
    int chaseIndex = (int)(beaconPulse * 12.0f) % 8;
    for (int b = 0; b < 10; b++) {
        float bx = x - 0.1f + b * 0.54f;
        bool isOn = ((b + chaseIndex) % 2 == 0);
        if (isOn) circle(0.12f, bx, y + 1.15f, 255, 220, 50, 255);
        else      circle(0.12f, bx, y + 1.15f, 100, 80, 20, 255);

        if (isOn) circle(0.12f, bx, y - 0.15f, 255, 220, 50, 255);
        else      circle(0.12f, bx, y - 0.15f, 100, 80, 20, 255);
    }

    // "CINEMA" Neon Green Text
    glColor3ub(30, 255, 120);
    glLineWidth(3);
    glBegin(GL_LINES);
    // C
    glVertex2f(x+0.7f, y+0.8f); glVertex2f(x+0.2f, y+0.8f);
    glVertex2f(x+0.2f, y+0.8f); glVertex2f(x+0.2f, y+0.2f);
    glVertex2f(x+0.2f, y+0.2f); glVertex2f(x+0.7f, y+0.2f);
    // I
    glVertex2f(x+1.1f, y+0.8f); glVertex2f(x+1.1f, y+0.2f);
    // N
    glVertex2f(x+1.5f, y+0.2f); glVertex2f(x+1.5f, y+0.8f);
    glVertex2f(x+1.5f, y+0.8f); glVertex2f(x+2.1f, y+0.2f);
    glVertex2f(x+2.1f, y+0.2f); glVertex2f(x+2.1f, y+0.8f);
    // E
    glVertex2f(x+2.5f, y+0.2f); glVertex2f(x+2.5f, y+0.8f);
    glVertex2f(x+2.5f, y+0.8f); glVertex2f(x+3.0f, y+0.8f);
    glVertex2f(x+2.5f, y+0.5f); glVertex2f(x+2.9f, y+0.5f);
    glVertex2f(x+2.5f, y+0.2f); glVertex2f(x+3.0f, y+0.2f);
    // M
    glVertex2f(x+3.3f, y+0.2f); glVertex2f(x+3.3f, y+0.8f);
    glVertex2f(x+3.3f, y+0.8f); glVertex2f(x+3.7f, y+0.4f);
    glVertex2f(x+3.7f, y+0.4f); glVertex2f(x+4.1f, y+0.8f);
    glVertex2f(x+4.1f, y+0.8f); glVertex2f(x+4.1f, y+0.2f);
    // A
    glVertex2f(x+4.4f, y+0.2f); glVertex2f(x+4.7f, y+0.8f);
    glVertex2f(x+4.7f, y+0.8f); glVertex2f(x+5.0f, y+0.2f);
    glVertex2f(x+4.5f, y+0.5f); glVertex2f(x+4.9f, y+0.5f);
    glEnd();
}

void Draw7SegmentDigit(float x, float y, float size, int digit) {
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

void Billboard()
{
    // Pole 1
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(35.0, -10.0);
    glVertex2f(35.5, -10.0);
    glVertex2f(35.5, -2.0);
    glVertex2f(35.0, -2.0);
    glEnd();

    // Pole 2
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(40.5, -10.0);
    glVertex2f(41.0, -10.0);
    glVertex2f(41.0, -2.0);
    glVertex2f(40.5, -2.0);
    glEnd();

    // Frame
    glColor3ub(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(34.5, -2.0);
    glVertex2f(41.5, -2.0);
    glVertex2f(41.5, 2.0);
    glVertex2f(34.5, 2.0);
    glEnd();

    // Screen background based on slide
    if (activeAdSlide == 0) {
        glColor3ub(15, 15, 25); // neon slide dark background
    } else if (activeAdSlide == 1) {
        glColor3ub(5, 10, 5); // clock slide dark background
    } else {
        glColor3ub(35, 15, 0); // warning slide amber background
    }
    glBegin(GL_QUADS);
    glVertex2f(35.0, -1.8);
    glVertex2f(41.0, -1.8);
    glVertex2f(41.0, 1.8);
    glVertex2f(35.0, 1.8);
    glEnd();

    // Slide 0: Pulsating Neon Arrow Logo
    if (activeAdSlide == 0) {
        float neonPulseVal = 0.5f + 0.5f * sin(beaconPulse * 8.0f);

        // Glowing halo line
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

        // Inner arrow
        glColor4ub(30, 240, 255, 255);
        glBegin(GL_TRIANGLES);
        glVertex2f(38.0f, 0.7f);
        glVertex2f(36.7f, -0.5f);
        glVertex2f(39.3f, -0.5f);
        glEnd();

        // Horizontal rings
        glColor4ub(30, 220, 255, (unsigned char)(100 + 155 * neonPulseVal));
        glBegin(GL_LINE_LOOP);
        glVertex2f(36.0f, -0.9f);
        glVertex2f(40.0f, -0.9f);
        glVertex2f(39.5f, -1.3f);
        glVertex2f(36.5f, -1.3f);
        glEnd();
    }
    // Slide 1: Live Digital 7-segment Clock
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

        // Blinking colon dots
        if (fmod(beaconPulse * 4.0f, 2.0f) < 1.0f) {
            circle(0.1f, 37.75f, 0.2f, 50, 255, 50, 255);
            circle(0.1f, 37.75f, -0.2f, 50, 255, 50, 255);
        }

        Draw7SegmentDigit(38.3f, -0.5f, 1.0f, m1);
        Draw7SegmentDigit(39.3f, -0.5f, 1.0f, m2);
    }
    // Slide 2: Dynamic Weather Icon & Scrolling Warning Advisory
    else {
        if (weatherMode == 0) { // Sun
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
        } else if (weatherMode == 1) { // Rain
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
        } else { // Snow
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

        // Scrolling Ticker Strip
        glColor3ub(10, 5, 0);
        glBegin(GL_QUADS);
        glVertex2f(35.1f, -1.4f);
        glVertex2f(40.9f, -1.4f);
        glVertex2f(40.9f, -0.5f);
        glVertex2f(35.1f, -0.5f);
        glEnd();

        // Warning arrows
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

void RailwayTrack()
{
    // Track ballast bed (sitting behind pavement kerb y=-8.7f up to ground line y=-7.5f)
    glColor3ub(90, 90, 95);
    glBegin(GL_QUADS);
    glVertex2f(-60, -8.7f);
    glVertex2f(60, -8.7f);
    glVertex2f(60, -7.5f);
    glVertex2f(-60, -7.5f);
    glEnd();

    // Wooden sleepers/ties
    glColor3ub(70, 40, 20);
    glLineWidth(2);
    glBegin(GL_LINES);
    for (int i = -60; i < 60; i += 3) {
        glVertex2f(i, -8.6f);
        glVertex2f(i, -7.6f);
    }
    glEnd();

    // Steel rails
    glColor3ub(210, 210, 215);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2f(-60, -7.8f);
    glVertex2f(60, -7.8f);
    glVertex2f(-60, -8.3f);
    glVertex2f(60, -8.3f);
    glEnd();
}


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

    // Draw chimney tube
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(-10.4f, 23.5f);
    glVertex2f(-9.7f, 23.5f);
    glVertex2f(-9.7f, 26.0f);
    glVertex2f(-10.4f, 26.0f);
    glEnd();

    // Draw chimney smoke drifting with weather-dependent wind speed
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

    // Draw chimney tube
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(30.1f, 19.5f);
    glVertex2f(30.8f, 19.5f);
    glVertex2f(30.8f, 22.0f);
    glVertex2f(30.1f, 22.0f);
    glEnd();

    // Draw chimney smoke drifting with weather-dependent wind speed
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

// ─── Lighthouse (distant coastal point, far right) ──────────────────────────
void Lighthouse()
{
    float lf = getLightFactor();

    // Rocky outcrop base
    glColor3ub(90, 85, 80);
    glBegin(GL_TRIANGLES);
    glVertex2f(52.0f, 0.0f);
    glVertex2f(62.0f, 0.0f);
    glVertex2f(57.0f, 3.0f);
    glEnd();

    // Tower body (tapered)
    glColor3ub(235, 235, 230);
    glBegin(GL_QUADS);
    glVertex2f(55.6f, 3.0f);
    glVertex2f(58.4f, 3.0f);
    glVertex2f(57.9f, 16.0f);
    glVertex2f(56.1f, 16.0f);
    glEnd();

    // Red bands
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

    // Gallery deck
    glColor3ub(60, 60, 65);
    glBegin(GL_QUADS);
    glVertex2f(55.6f, 16.0f);
    glVertex2f(58.4f, 16.0f);
    glVertex2f(58.4f, 16.6f);
    glVertex2f(55.6f, 16.6f);
    glEnd();

    // Lamp room housing
    glColor3ub(40, 40, 45);
    glBegin(GL_QUADS);
    glVertex2f(56.1f, 16.6f);
    glVertex2f(57.9f, 16.6f);
    glVertex2f(57.9f, 18.6f);
    glVertex2f(56.1f, 18.6f);
    glEnd();

    // Glass glow (brighter at night)
    float glow = 0.4f + 0.6f * lf;
    glColor3ub((unsigned char)(255*glow), (unsigned char)(240*glow), (unsigned char)(180*glow));
    glBegin(GL_QUADS);
    glVertex2f(56.3f, 16.8f);
    glVertex2f(57.7f, 16.8f);
    glVertex2f(57.7f, 18.4f);
    glVertex2f(56.3f, 18.4f);
    glEnd();

    // Roof cap
    glColor3ub(150, 30, 30);
    glBegin(GL_TRIANGLES);
    glVertex2f(55.9f, 18.6f);
    glVertex2f(58.1f, 18.6f);
    glVertex2f(57.0f, 20.2f);
    glEnd();

    // Rotating beam (visible once dusk sets in), driven off the existing
    // waveMove clock so no new timer is needed
    if (lf > 0.1f) {
        float beamAngle = waveMove * 0.5f;
        float dx = cos(beamAngle);
        float dy = sin(beamAngle);
        unsigned char a = (unsigned char)(140 * lf);
        DrawLightCone(57.0f, 17.6f,  dx,  dy, 34.0f, 5.5f, 255, 250, 195, a);
        DrawLightCone(57.0f, 17.6f, -dx, -dy, 34.0f, 5.5f, 255, 250, 195, a);
    }
}

void addDrop() {
    if (totalDrops < MAX_DROPS) {
        // Random X position
        dropX[totalDrops] = (rand() % 120) - 60.0f;

        // Start at top of sky
        dropY[totalDrops] = 40.0f;

        // NEW: Randomize splash point between -24 (Top of Ocean) and -40 (Bottom)
        // This makes drops fall "into" the ocean at different distances
        dropTargetY[totalDrops] = -24.0f - (rand() % 16);

        totalDrops++;
    }
}

void updateRain(int) {
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, updateRain, 0); return; }  // idle while this scenario is off-screen
    // 1. Particle Physics (Rain or Snow)
    if (weatherMode == 1 || weatherMode == 2) {
        float fallSpeed = (weatherMode == 2) ? 0.22f : 1.0f;

        for (int i = 0; i < totalDrops; i++) {
            dropY[i] -= fallSpeed; // Particle falls downwards

            // If snow, add side-to-side sway using a sine wave
            if (weatherMode == 2) {
                dropX[i] += 0.06f * sin(beaconPulse * 1.5f + i * 0.5f);
            }

            // Check collision with specific target for this particle
            if (dropY[i] <= dropTargetY[i]) {
                // If rain, spawn a water splash ripple
                if (weatherMode == 1 && bubbleCount < MAX_BUBBLES) {
                    bubbleX[bubbleCount] = dropX[i];
                    bubbleY[bubbleCount] = dropTargetY[i]; // Use specific ocean height
                    bubbleRadius[bubbleCount] = 0.2f;
                    bubbleAlpha[bubbleCount] = 255.0f;
                    bubbleActive[bubbleCount] = true;
                    bubbleCount++;
                }

                // Remove the particle
                for (int j = i; j < totalDrops - 1; j++) {
                    dropX[j] = dropX[j + 1];
                    dropY[j] = dropY[j + 1];
                    dropTargetY[j] = dropTargetY[j + 1];
                }
                totalDrops--;
                i--;
            }
        }

        // Add new particles (fewer for snow to keep it gentle)
        int dropsToCreate = (weatherMode == 2) ? 4 : 10;
        for (int i = 0; i < dropsToCreate; i++) {
            addDrop();
        }
    } else {
        totalDrops = 0;
    }

    // 2. Update Splashes/Ripples
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

    // 3. Update Snow Accumulation
    if (weatherMode == 2) {
        if (snowAccumulation < 1.2f) snowAccumulation += 0.0006f; // accumulates slowly
    } else if (weatherMode == 1) {
        if (snowAccumulation > 0.0f) snowAccumulation -= 0.004f;  // rain melts snow quickly
        if (snowAccumulation < 0.0f) snowAccumulation = 0.0f;
    } else {
        if (snowAccumulation > 0.0f) snowAccumulation -= 0.0015f; // clear sky melts snow slowly
        if (snowAccumulation < 0.0f) snowAccumulation = 0.0f;
    }

    // 4. Update Fog Drifting Position
    if (isAnimating) {
        fogPos += 0.05f;
        if (fogPos > 120.0f) fogPos = -120.0f;
    }

    // 5. Procedural Lightning Strike Triggers (Rain only)
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

    // Decay active lightning intensity over time
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

void DrawRipples() {
    glLineWidth(2);
    for (int i = 0; i < bubbleCount; i++) {
        // Draw ellipse (flattened circle) for 3D effect
        glBegin(GL_LINE_LOOP);
        glColor4ub(200, 200, 255, (unsigned char)bubbleAlpha[i]);

        for (int j = 0; j < 30; j++) {
            float theta = 2.0f * 3.1416f * float(j) / float(30);
            float rx = bubbleRadius[i] * cosf(theta);
            float ry = (bubbleRadius[i] * 0.2f) * sinf(theta); // 0.2 flattens it
            glVertex2f(bubbleX[i] + rx, bubbleY[i] + ry);
        }
        glEnd();
    }
}

void drawWeatherParticles() {
    if (weatherMode == 1) { // Rain
        glLineWidth(1);
        glColor3ub(170, 200, 255); // Light Blue Rain color
        glBegin(GL_LINES);
        for (int i = 0; i < totalDrops; i++) {
            glVertex2f(dropX[i], dropY[i]);
            glVertex2f(dropX[i], dropY[i] - 1.5f); // Tail length of 1.5
        }
        glEnd();
    } else if (weatherMode == 2) { // Snow
        glColor3ub(255, 255, 255); // Pure white snow
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

void DrawFog() {
    float intensity = 0.0f;
    if (weatherMode == 1) { // Rain
        intensity = stormFactor;
    } else if (weatherMode == 2) { // Snow
        intensity = 0.6f * (snowAccumulation / 1.2f) + 0.4f * stormFactor;
    } else { // Clear
        intensity = stormFactor;
    }

    if (intensity < 0.05f) return;

    // Ambient lower mist gradient
    glBegin(GL_QUADS);
    glColor4ub(200, 200, 205, (unsigned char)(60.0f * intensity));
    glVertex2f(-60.0f, -28.0f);
    glVertex2f(60.0f, -28.0f);
    glColor4ub(220, 220, 225, 0); // fades out upward
    glVertex2f(60.0f, -14.0f);
    glVertex2f(-60.0f, -14.0f);
    glEnd();

    // Drifting fog puffs
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

void DrawTrafficLight() {
    // Pole
    glColor3ub(60, 60, 60);
    glBegin(GL_QUADS);
    glVertex2f(4.7f, -12.0f);
    glVertex2f(5.3f, -12.0f);
    glVertex2f(5.3f, -4.0f);
    glVertex2f(4.7f, -4.0f);
    glEnd();

    // Mast arm
    glBegin(GL_QUADS);
    glVertex2f(0.0f, -4.3f);
    glVertex2f(5.0f, -4.3f);
    glVertex2f(5.0f, -3.7f);
    glVertex2f(0.0f, -3.7f);
    glEnd();

    // Light box body
    glColor3ub(30, 30, 30);
    glBegin(GL_QUADS);
    glVertex2f(-0.6f, -6.5f);
    glVertex2f(0.6f, -6.5f);
    glVertex2f(0.6f, -3.8f);
    glVertex2f(-0.6f, -3.8f);
    glEnd();

    // Visors (small hoods above lights)
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

    // Draw the three lights
    // Red light (top)
    if (trafficLightState == 2) {
        circle(0.3f, 0.0f, -4.5f, 255, 30, 30, 255); // glowing
        circle(0.45f, 0.0f, -4.5f, 255, 30, 30, 100); // Glow halo
    } else {
        circle(0.3f, 0.0f, -4.5f, 80, 10, 10, 255); // dim
    }

    // Yellow light (middle)
    if (trafficLightState == 1) {
        circle(0.3f, 0.0f, -5.2f, 255, 200, 0, 255); // glowing
        circle(0.45f, 0.0f, -5.2f, 255, 200, 0, 100);
    } else {
        circle(0.3f, 0.0f, -5.2f, 80, 60, 0, 255); // dim
    }

    // Green light (bottom)
    if (trafficLightState == 0) {
        circle(0.3f, 0.0f, -5.9f, 30, 255, 30, 255); // glowing
        circle(0.45f, 0.0f, -5.9f, 30, 255, 30, 100);
    } else {
        circle(0.3f, 0.0f, -5.9f, 10, 80, 10, 255); // dim
    }
}

void DrawHelicopter() {
    if (!heliActive) return;

    float fadeAlpha = 1.0f;
    if (heliScale < 0.15f) {
        fadeAlpha = (heliScale - 0.05f) / 0.10f; // fade in from 0.0 to 1.0
    }
    if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
    if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;

    glPushMatrix();
    glTranslatef(heliX, heliY, 0.0f);
    glScalef(heliScale, heliScale, 1.0f);

    // Body (Cockpit capsule)
    circle(1.8f, 0.0f, 0.0f, (isNight ? 80.0f : 180.0f), (isNight ? 20.0f : 40.0f), (isNight ? 20.0f : 40.0f), 255.0f * fadeAlpha);

    // Windshield
    if (isNight) glColor4ub(100, 200, 255, (unsigned char)(180 * fadeAlpha));
    else glColor4ub(180, 230, 255, (unsigned char)(200 * fadeAlpha));
    glBegin(GL_POLYGON);
    glVertex2f(0.3f, 0.8f);
    glVertex2f(1.4f, 0.3f);
    glVertex2f(1.2f, -0.6f);
    glVertex2f(0.0f, -0.6f);
    glEnd();

    // Tail boom
    if (isNight) glColor4ub(60, 60, 60, (unsigned char)(255 * fadeAlpha));
    else glColor4ub(120, 120, 120, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_QUADS);
    glVertex2f(-1.6f, 0.2f);
    glVertex2f(-4.0f, 0.8f);
    glVertex2f(-4.0f, 0.5f);
    glVertex2f(-1.6f, -0.2f);
    glEnd();

    // Tail fin
    if (isNight) glColor4ub(60, 60, 60, (unsigned char)(255 * fadeAlpha));
    else glColor4ub(120, 120, 120, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_TRIANGLES);
    glVertex2f(-4.0f, 0.5f);
    glVertex2f(-4.3f, 1.8f);
    glVertex2f(-3.7f, 0.5f);
    glEnd();

    // Main rotor shaft
    glColor4ub(50, 50, 50, (unsigned char)(255 * fadeAlpha));
    glBegin(GL_QUADS);
    glVertex2f(-0.2f, 1.6f);
    glVertex2f(0.2f, 1.6f);
    glVertex2f(0.2f, 2.2f);
    glVertex2f(-0.2f, 2.2f);
    glEnd();

    // Landing skids
    glColor4ub(50, 50, 50, (unsigned char)(255 * fadeAlpha));
    glLineWidth(2);
    glBegin(GL_LINES);
    // struts
    glVertex2f(-0.8f, -1.6f);
    glVertex2f(-1.0f, -2.2f);
    glVertex2f(0.8f, -1.6f);
    glVertex2f(0.6f, -2.2f);
    // skid bar
    glVertex2f(-1.6f, -2.2f);
    glVertex2f(1.6f, -2.2f);
    glEnd();

    // Rotating Main Rotor Blades
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

    // Rotating Tail Rotor Blades
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

    // Flashing anti-collision light
    float flash = (0.5f + 0.5f * sin(beaconPulse * 8.0f));
    circle(0.3f, -0.2f, 1.7f, 255, 0, 0, (unsigned char)(255 * flash * fadeAlpha));

    // Headlight searchlight at night
    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(0.8f, -0.4f, 0.8f, -0.6f, 12.0f, 3.5f, 255, 255, 220, (unsigned char)(95 * lf * fadeAlpha));
    }

    glPopMatrix();
}

void GetDynamicWindowColor(float x, float y, float rDay, float gDay, float bDay, float &rOut, float &gOut, float &bOut) {
    int hashVal = (int)(fabsf(x) * 17.3f + fabsf(y) * 23.7f) % 100;
    float nightFactor = getLightFactor();
    float rCurrent = rDay, gCurrent = gDay, bCurrent = bDay;
    float rNight = 40.0f, gNight = 40.0f, bNight = 50.0f; // off

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

void Building1() {
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

    // Windows

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

            // White reflection fades out at night
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

void Building3() {
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

    // White reflection fades out at night
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

void Building4() {
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

void Building6() {
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

            // White reflection fades out at night
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

void Building2() {
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

    // Windows
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

            // White reflection fades out at night
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


void Building5() {
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

    // Windows
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

            // White reflection fades out at night
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

float getLightFactor() {
    if (timeOfDay > 0.25f && timeOfDay <= 0.35f) {
        return (timeOfDay - 0.25f) / 0.10f;
    } else if (timeOfDay > 0.35f && timeOfDay <= 0.75f) {
        return 1.0f;
    } else if (timeOfDay > 0.75f && timeOfDay <= 0.85f) {
        return 1.0f - (timeOfDay - 0.75f) / 0.10f;
    }
    return 0.0f;
}

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

void Train()
{
    // Locomotive main body
    glColor3ub(200, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(trainPos, -7.1f);
    glVertex2f(trainPos + 8.0f, -7.1f);
    glVertex2f(trainPos + 8.0f, -3.1f);
    glVertex2f(trainPos, -3.1f);
    glEnd();

    // Locomotive cabin
    glColor3ub(180, 40, 40);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 5.0f, -7.1f);
    glVertex2f(trainPos + 8.0f, -7.1f);
    glVertex2f(trainPos + 8.0f, -1.6f);
    glVertex2f(trainPos + 5.0f, -1.6f);
    glEnd();

    // Smokestack
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 1.0f, -3.1f);
    glVertex2f(trainPos + 2.0f, -3.1f);
    glVertex2f(trainPos + 2.0f, -1.6f);
    glVertex2f(trainPos + 1.0f, -1.6f);
    glEnd();

    // Cabin window
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(200, 240, 255);

    glBegin(GL_QUADS);
    glVertex2f(trainPos + 5.5f, -4.3f);
    glVertex2f(trainPos + 7.5f, -4.3f);
    glVertex2f(trainPos + 7.5f, -2.8f);
    glVertex2f(trainPos + 5.5f, -2.8f);
    glEnd();

    // Front cowcatcher plow
    glColor3ub(100, 100, 100);
    glBegin(GL_TRIANGLES);
    glVertex2f(trainPos, -7.1f);
    glVertex2f(trainPos, -5.3f);
    glVertex2f(trainPos - 2.0f, -7.1f);
    glEnd();

    // Locomotive Wheels
    circle(1, trainPos + 2.0f, -7.1f, 30, 30, 30, 255);
    circle(1, trainPos + 6.0f, -7.1f, 30, 30, 30, 255);

    // Passenger Carriage 1
    glColor3ub(0, 100, 180);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 9.0f, -7.1f);
    glVertex2f(trainPos + 18.0f, -7.1f);
    glVertex2f(trainPos + 18.0f, -3.1f);
    glVertex2f(trainPos + 9.0f, -3.1f);
    glEnd();

    // Carriage 1 windows
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(200, 240, 255);

    glBegin(GL_QUADS);
    glVertex2f(trainPos + 9.5f, -5.3f);
    glVertex2f(trainPos + 17.5f, -5.3f);
    glVertex2f(trainPos + 17.5f, -3.8f);
    glVertex2f(trainPos + 9.5f, -3.8f);
    glEnd();

    // Carriage connector 1
    glColor3ub(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 8.0f, -6.3f);
    glVertex2f(trainPos + 9.0f, -6.3f);
    glVertex2f(trainPos + 9.0f, -5.3f);
    glVertex2f(trainPos + 8.0f, -5.3f);
    glEnd();

    // Carriage 1 wheels
    circle(1, trainPos + 11.0f, -7.1f, 30, 30, 30, 255);
    circle(1, trainPos + 16.0f, -7.1f, 30, 30, 30, 255);

    // Passenger Carriage 2
    glColor3ub(0, 100, 180);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 19.0f, -7.1f);
    glVertex2f(trainPos + 28.0f, -7.1f);
    glVertex2f(trainPos + 28.0f, -3.1f);
    glVertex2f(trainPos + 19.0f, -3.1f);
    glEnd();

    // Carriage 2 windows
    if (isNight) glColor3ub(255, 255, 100);
    else glColor3ub(200, 240, 255);

    glBegin(GL_QUADS);
    glVertex2f(trainPos + 19.5f, -5.3f);
    glVertex2f(trainPos + 27.5f, -5.3f);
    glVertex2f(trainPos + 27.5f, -3.8f);
    glVertex2f(trainPos + 19.5f, -3.8f);
    glEnd();

    // Carriage connector 2
    glColor3ub(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(trainPos + 18.0f, -6.3f);
    glVertex2f(trainPos + 19.0f, -6.3f);
    glVertex2f(trainPos + 19.0f, -5.3f);
    glVertex2f(trainPos + 18.0f, -5.3f);
    glEnd();

    // Carriage 2 wheels
    circle(1, trainPos + 21.0f, -7.1f, 30, 30, 30, 255);
    circle(1, trainPos + 26.0f, -7.1f, 30, 30, 30, 255);

    float lf = getLightFactor();
    if (lf > 0.01f) {
        DrawLightCone(trainPos - 2.0f, -6.8f, -1.0f, 0.0f, 15.0f, 3.5f, 255, 255, 200, (unsigned char)(120 * lf));
    }

    // Dynamic train steam engine exhaust puffs
    if (isAnimating) {
        float stackX = trainPos + 1.5f;
        float stackY = -1.6f;
        for (int i = 0; i < 4; i++) {
            float t = waveMove * 15.0f + i * 2.0f;
            float life = fmod(t, 6.0f) / 6.0f; // 0.0 to 1.0 life cycle

            float xOffset = 3.2f * life; // drifts backward (right)
            float yOffset = 2.5f * life; // rises upward
            float scale = 0.25f + 0.7f * life; // grows in size
            unsigned char alpha = (unsigned char)(140.0f * (1.0f - life)); // fades out

            circle(scale, stackX + xOffset, stackY + yOffset, 220, 220, 225, alpha);
        }
    }
}

void UpdateTrain(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTrain, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        trainPos -= 0.6f;
        if (trainPos < -90.0f) trainPos = 70.0f;
    }
    glutTimerFunc(25, UpdateTrain, 0);
}

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


void CarYellow()
{
    glPushMatrix();
    glTranslatef(carYellowPos, 0.0f, 0.0f);   // ONLY movement

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




void UpdateCarRed(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCarRed, 0); return; }  // idle while this scenario is off-screen
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

void UpdateCarGreen(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCarGreen, 0); return; }  // idle while this scenario is off-screen
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

void UpdateCarYellow(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCarYellow, 0); return; }  // idle while this scenario is off-screen
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


void UpdateBus(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateBus, 0); return; }  // idle while this scenario is off-screen
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

void UpdateTruckCargo(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTruckCargo, 0); return; }  // idle while this scenario is off-screen
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


void UpdateTruckSmall(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTruckSmall, 0); return; }  // idle while this scenario is off-screen
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


void DrawWaveLayer(float baseY, float amp, float freq, float speedMult, float phaseShift, SkyColor topCol, SkyColor botCol) {
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

void DrawWavyReflection(float xLight, unsigned char r, unsigned char g, unsigned char b, float maxA) {
    float lf = getLightFactor();
    if (lf < 0.01f) return;
    float actualAlpha = maxA * lf;

    // Draw left half of reflection glow
    glBegin(GL_QUAD_STRIP);
    for (float y = -24.0f; y >= -40.0f; y -= 1.0f) {
        float depthFactor = (y + 40.0f) / 16.0f; // 0.0 at bottom, 1.0 at top
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

    // Draw right half of reflection glow
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

void Ocean()
{
    float lf = getLightFactor(); // 0 (noon) -> 1 (night)

    // Color definitions
    // 1. Back Layer
    SkyColor backNoonT = {0.0f, 70.0f, 130.0f};
    SkyColor backNoonB = {0.0f, 90.0f, 150.0f};
    SkyColor backNightT = {0.0f, 10.0f, 25.0f};
    SkyColor backNightB = {0.0f, 20.0f, 45.0f};
    SkyColor backStormT = {10.0f, 30.0f, 45.0f};
    SkyColor backStormB = {20.0f, 45.0f, 65.0f};

    // 2. Middle Layer
    SkyColor midNoonT = {0.0f, 90.0f, 160.0f};
    SkyColor midNoonB = {0.0f, 110.0f, 180.0f};
    SkyColor midNightT = {0.0f, 15.0f, 35.0f};
    SkyColor midNightB = {0.0f, 30.0f, 55.0f};
    SkyColor midStormT = {15.0f, 40.0f, 55.0f};
    SkyColor midStormB = {25.0f, 55.0f, 75.0f};

    // 3. Front Layer
    SkyColor frontNoonT = {0.0f, 110.0f, 190.0f};
    SkyColor frontNoonB = {0.0f, 140.0f, 220.0f};
    SkyColor frontNightT = {0.0f, 20.0f, 45.0f};
    SkyColor frontNightB = {0.0f, 40.0f, 70.0f};
    SkyColor frontStormT = {20.0f, 50.0f, 70.0f};
    SkyColor frontStormB = {30.0f, 70.0f, 95.0f};

    // Interpolations
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

    // Draw the 3 layers of waves
    DrawWaveLayer(-25.0f, 0.3f, 0.35f, 1.5f, 0.0f, backT, backB);
    DrawWaveLayer(-28.0f, 0.5f, 0.20f, -2.5f, 1.0f, midT, midB);
    DrawWaveLayer(-31.0f, 0.7f, 0.12f, 4.0f, 2.0f, frontT, frontB);

    // Draw Reflections at night
    if (lf > 0.01f) {
        // Moon reflection (silver-white)
        if (moonY_glob > -12.0f) {
            float moonAlpha = (1.0f - stormFactor) * (moonY_glob > 0.0f ? 1.0f : (moonY_glob + 12.0f) / 12.0f);
            if (moonAlpha > 0.01f) {
                DrawWavyReflection(moonX_glob, 230, 240, 255, 65.0f * moonAlpha);
            }
        }

        // Streetlights reflections (warm golden yellow)
        // Streetlights exist at: -42.8, -12.8, 17.2, 47.2
        DrawWavyReflection(-42.8f, 255, 230, 150, 50.0f);
        DrawWavyReflection(-12.8f, 255, 230, 150, 50.0f);
        DrawWavyReflection(17.2f, 255, 230, 150, 50.0f);
        DrawWavyReflection(47.2f, 255, 230, 150, 50.0f);
    }

    // Top surface line highlight
    glLineWidth(2);
    glColor4ub(255, 255, 255, 80);
    glBegin(GL_LINE_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 1.5f) {
        float yVal = -31.0f + 0.7f * sin(0.12f * x + waveMove * 4.0f + 2.0f);
        glVertex2f(x, yVal);
    }
    glEnd();
}

void UpdateWaves(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateWaves, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        waveMove += 0.04f;
    }
    glutTimerFunc(25, UpdateWaves, 0);
}

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

    // Dynamic bow wake foam
    if (isAnimating) {
        float bowX = 9.8f;
        float bowY = -29.0f;
        float direction = 1.0f; // moves left-to-right
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


void UpdateCruiseShip(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCruiseShip, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        cruisePos += 0.2f;
        if (cruisePos > 80.0f) cruisePos = -80.0f;
    }
    glutTimerFunc(25, UpdateCruiseShip, 0);
}

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

    // Dynamic bow wake foam
    if (isAnimating) {
        float bowX = -8.0f;
        float bowY = -31.0f;
        float direction = -1.0f; // moves right-to-left
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

void UpdateCargoShip(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateCargoShip, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        cargoShipPos -= 0.1f;
        if (cargoShipPos < -80.0f) cargoShipPos = 80.0f;
    }
    glutTimerFunc(25, UpdateCargoShip, 0);
}

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

    // Dynamic bow wake foam
    if (isAnimating) {
        float bowX = -14.0f;
        float bowY = -34.5f;
        float direction = -1.0f; // moves right-to-left
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


void UpdateYacht(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateYacht, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        yachtPos -= 0.3f;
        if (yachtPos < -80.0f) yachtPos = 80.0f;
    }
    glutTimerFunc(25, UpdateYacht, 0);
}

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

    // Dynamic bow wake foam
    if (isAnimating) {
        float bowX = 3.5f;
        float bowY = 0.5f;
        float direction = 1.0f; // moves left-to-right
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

    // Dynamic bow wake foam
    if (isAnimating) {
        float bowX = 3.5f;
        float bowY = 0.5f;
        float direction = 1.0f; // moves left-to-right
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

// ─── Fishing Boat (anchored near shore, gently bobbing) ─────────────────────
void FishingBoat()
{
    float bob = 0.3f * sin(waveMove * 2.0f + 1.7f);
    float lf  = getLightFactor();
    float dim = 1.0f - 0.5f * lf;

    glPushMatrix();
    glTranslatef(fishingBoatX, -27.0f + bob, 0.0f);

    // Hull
    glColor3ub((unsigned char)(140*dim), (unsigned char)(95*dim), (unsigned char)(55*dim));
    glBegin(GL_POLYGON);
    glVertex2f(-3.0f, 1.3f);
    glVertex2f(3.0f, 1.3f);
    glVertex2f(2.2f, 0.0f);
    glVertex2f(-2.2f, 0.0f);
    glEnd();

    // Rim
    glColor3ub((unsigned char)(190*dim), (unsigned char)(150*dim), (unsigned char)(90*dim));
    glBegin(GL_QUADS);
    glVertex2f(-2.9f, 1.0f);
    glVertex2f(2.9f, 1.0f);
    glVertex2f(2.9f, 1.3f);
    glVertex2f(-2.9f, 1.3f);
    glEnd();

    // Small crate/cabin at the back
    glColor3ub((unsigned char)(100*dim), (unsigned char)(70*dim), (unsigned char)(45*dim));
    glBegin(GL_QUADS);
    glVertex2f(1.4f, 1.3f);
    glVertex2f(2.5f, 1.3f);
    glVertex2f(2.5f, 2.2f);
    glVertex2f(1.4f, 2.2f);
    glEnd();

    // Fisherman, seated
    glColor3ub((unsigned char)(70*dim), (unsigned char)(110*dim), (unsigned char)(90*dim));
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, 1.3f);
    glVertex2f(-0.4f, 1.3f);
    glVertex2f(-0.4f, 2.2f);
    glVertex2f(-1.0f, 2.2f);
    glEnd();
    circle(0.22f, -0.7f, 2.5f, (unsigned char)(220*dim), (unsigned char)(175*dim), (unsigned char)(130*dim), 255);

    // Fishing rod, angled out over the water
    glColor3ub((unsigned char)(90*dim), (unsigned char)(60*dim), (unsigned char)(35*dim));
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glVertex2f(-0.7f, 2.0f);
    glVertex2f(-2.8f, 3.0f);
    glEnd();

    // Fishing line down to the water, with a small bobber
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

void UpdateSmallBoat1(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSmallBoat1, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        boatSmall1Pos += 0.15f;
        if (boatSmall1Pos > 70.0f) boatSmall1Pos = -70.0f;
    }
    glutTimerFunc(25, UpdateSmallBoat1, 0);
}

void UpdateSmallBoat2(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSmallBoat2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        boatSmall2Pos += 0.25f;
        if (boatSmall2Pos > 70.0f) boatSmall2Pos = -70.0f;
    }
    glutTimerFunc(25, UpdateSmallBoat2, 0);
}

void UpdateHelicopter(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateHelicopter, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        // Approach flight path
        heliX += 0.17f;
        heliY -= 0.03f;
        if (heliScale < 1.5f) {
            heliScale += 0.003f;
        }

        // Spin rotor blades
        heliPropAngle -= 25.0f;
        if (heliPropAngle < -360.0f) heliPropAngle += 360.0f;

        // Reset loop once completely off-screen to the right
        if (heliX > 75.0f) {
            heliX = -45.0f - (rand() % 20);
            heliY = 14.0f + (rand() % 6);
            heliScale = 0.05f;
        }
    }
    glutTimerFunc(25, UpdateHelicopter, 0);
}

void UpdateTrafficLight(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateTrafficLight, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        trafficLightTimer++;
        if (trafficLightState == 0) { // Green
            if (trafficLightTimer >= 320) {
                trafficLightState = 1; // Yellow
                trafficLightTimer = 0;
            }
        } else if (trafficLightState == 1) { // Yellow
            if (trafficLightTimer >= 100) {
                trafficLightState = 2; // Red
                trafficLightTimer = 0;
            }
        } else if (trafficLightState == 2) { // Red
            if (trafficLightTimer >= 320) {
                trafficLightState = 0; // Green
                trafficLightTimer = 0;
            }
        }
    }
    glutTimerFunc(25, UpdateTrafficLight, 0);
}

void UpdateBillboardAd(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateBillboardAd, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        adSlideTimer++;
        if (adSlideTimer >= 200) { // 5 seconds
            activeAdSlide = (activeAdSlide + 1) % 3;
            adSlideTimer = 0;
        }
    }
    glutTimerFunc(25, UpdateBillboardAd, 0);
}

void UpdatePedestrians(int)
{
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdatePedestrians, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        pedWalkTimer += 0.04f;
        for (int i = 0; i < NUM_PEDS; i++) {
            Pedestrian& p = peds[i];
            // Keep Y anchored to pavement baseline when not crossing
            if (!p.crossing) p.y = -10.5f;

            if (!p.crossing) {
                // Normal pavement walking
                float spd = p.speed;

                if (trafficLightState == 2) {
                    // Traffic is RED -> Pedestrians near zebra crossing cross the street!
                    if (p.x >= -4.0f && p.x <= 4.0f) {
                        p.crossing   = true;
                        p.crossTimer = 0.0f;
                        p.savedX     = p.x;
                    }
                } else {
                    // Traffic is GREEN or YELLOW -> Vehicles are moving, pedestrians halt before the crosswalk
                    float stopX = (p.dir > 0) ? -2.0f : 2.5f;
                    bool approachingCrosswalk = (p.dir > 0) ? (p.x > stopX - 6.0f && p.x <= stopX) :
                                                              (p.x < stopX + 6.0f && p.x >= stopX);
                    if (approachingCrosswalk) {
                        float distToStop = (p.dir > 0) ? (stopX - p.x) : (p.x - stopX);
                        spd = p.speed * (distToStop / 6.0f);
                        if (spd < 0.01f) spd = 0.0f; // Wait at curb until Red light
                    }
                }

                p.x += p.dir * spd;
                // wrap around
                if (p.x > 62.0f)  p.x = -62.0f;
                if (p.x < -62.0f) p.x =  62.0f;
            } else {
                // Pedestrian is crossing the road: walk across at normal
                // walking pace in the same direction they were already
                // heading, dipping down into the road and back up onto
                // the sidewalk once they've cleared the crosswalk.
                const float crossDistance = 11.0f; // total x distance to walk while crossing
                p.x += p.dir * p.speed;

                float progress = fabsf(p.x - p.savedX) / crossDistance;
                if (progress < 1.0f) {
                    p.y = -10.5f - 7.5f * sin(progress * 3.14159f);
                } else {
                    // Done crossing — arrived on the sidewalk on the far side;
                    // keep walking the same direction (no U-turn).
                    p.y = -10.5f;
                    p.crossing = false;
                    p.crossTimer = 0.0f;
                }
            }
        }
    }
    glutTimerFunc(25, UpdatePedestrians, 0);
}

void UpdateSeagulls(int) {
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateSeagulls, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        for (int i = 0; i < NUM_SEAGULLS; i++) {
            Seagull& g = seagulls[i];

            // Speed & flapping rate adjust to storm/rain weather
            float spd = g.baseSpeed;
            float flapSpd = g.flapSpeed;
            if (weatherMode == 1) { // Rain / storm
                g.targetY = 12.0f + i * 2.5f;
                spd *= 0.7f;
                flapSpd *= 1.8f;
            } else if (weatherMode == 2) { // Snow
                g.targetY = 15.0f + i * 2.2f;
                spd *= 0.85f;
                flapSpd *= 1.3f;
            } else { // Clear
                g.targetY = 22.0f + i * 2.2f;
            }

            // Smooth altitude adjustment
            g.y += (g.targetY - g.y) * 0.03f;

            // Advance flight position & wing flap
            g.x += g.dir * spd;
            g.wingAngle += flapSpd;

            // Screen wrap
            if (g.x > 70.0f) g.x = -70.0f;
            if (g.x < -70.0f) g.x = 70.0f;
        }
    }
    glutTimerFunc(25, UpdateSeagulls, 0);
}

void UpdateExhaustSmoke(int) {
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateExhaustSmoke, 0); return; }  // idle while this scenario is off-screen
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

void UpdateElevator(int) {
    if (currentScreen != SCENARIO_1 || isPaused) { glutTimerFunc(120, UpdateElevator, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating) {
        float speed = 0.09f;
        if (elevatorState == 0) { // Moving UP
            elevatorY += speed;
            if (elevatorY >= 9.8f) {
                elevatorY = 9.8f;
                elevatorState = 1; // pause at top
                elevatorPauseTimer = 0;
            }
        } else if (elevatorState == 1) { // Pause at top floor
            elevatorPauseTimer++;
            if (elevatorPauseTimer >= 60) { // 1.5 second pause
                elevatorState = 2; // start moving down
            }
        } else if (elevatorState == 2) { // Moving DOWN
            elevatorY -= speed;
            if (elevatorY <= -7.5f) {
                elevatorY = -7.5f;
                elevatorState = 3; // pause at bottom
                elevatorPauseTimer = 0;
            }
        } else if (elevatorState == 3) { // Pause at bottom floor
            elevatorPauseTimer++;
            if (elevatorPauseTimer >= 60) {
                elevatorState = 0; // start moving up
            }
        }
    }
    glutTimerFunc(25, UpdateElevator, 0);
}


void Scene()
{
    DrawSky();
    DrawStars();

    DrawCloud1();
    DrawCloud2();
    DrawCloud3();
    DrawCloud4();

    // ── SEAGULL FLOCK ────────────────────────────────────────────────────────
    for (int si = 0; si < NUM_SEAGULLS; si++) {
        DrawSeagull(seagulls[si]);
    }

    // Layer 1: Helicopter far away (behind mountains)
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

    // Layer 2: Helicopter mid-distance (in front of mountains, but behind buildings)
    if (heliScale >= 0.35f && heliScale < 0.85f) {
        DrawHelicopter();
    }

    //CITY LAYER (ground plane base sitting at y=-7.5f behind railway track)
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

    // Layer 3: Helicopter close (in front of buildings/trees)
    if (heliScale >= 0.85f) {
        DrawHelicopter();
    }

    // ── RAILWAY & TRAIN LAYER (drawn in full view behind pavement, above buildings)
    RailwayTrack();
    Train();

    //ROAD LAYER
    // 1. Asphalt Road Surface
    glColor3ub(50, 50, 50);
    glBegin(GL_QUADS);
    glVertex2f(-60, -24);
    glVertex2f(60, -24);
    glVertex2f(60, -12);
    glVertex2f(-60, -12);
    glEnd();

    // 2. Zebra Crossing & Stop Bars (painted directly on asphalt)
    // White zebra stripes across road height (y=-23.5 to y=-12.5)
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

    // Solid Stop Bars (where vehicles halt before the crosswalk)
    // Bottom lane stop bar (for right-bound traffic stopping at x=-5.5)
    glColor3ub(250, 250, 250);
    glBegin(GL_QUADS);
    glVertex2f(-6.2f, -23.8f);
    glVertex2f(-5.6f, -23.8f);
    glVertex2f(-5.6f, -18.2f);
    glVertex2f(-6.2f, -18.2f);
    glEnd();
    // Top lane stop bar (for left-bound traffic stopping at x=5.5)
    glBegin(GL_QUADS);
    glVertex2f( 5.6f, -17.8f);
    glVertex2f( 6.2f, -17.8f);
    glVertex2f( 6.2f, -12.2f);
    glVertex2f( 5.6f, -12.2f);
    glEnd();

    // 3. Center Dashed Line (skipping the zebra crosswalk zone x=-5 to 5)
    glColor3ub(255, 255, 255);
    glLineWidth(2);
    glBegin(GL_LINES);
    for (int i = -60; i < 60; i += 8) {
        if (i >= -8 && i <= 4) continue; // Keep zebra crossing clear
        glVertex2f(i, -18);
        glVertex2f(i + 4, -18);
    }
    glEnd();

    // 4. PAVEMENT STRIP (between road and buildings)
    // Sidewalk concrete (y=-12 down to -9)
    glColor3ub(185, 180, 170);
    glBegin(GL_QUADS);
    glVertex2f(-60.0f, -12.0f);
    glVertex2f( 60.0f, -12.0f);
    glVertex2f( 60.0f,  -9.0f);
    glVertex2f(-60.0f,  -9.0f);
    glEnd();

    // Pavement tile lines (horizontal grooves)
    glColor3ub(160, 155, 145);
    glLineWidth(1.0f);
    for (int tl = 0; tl < 3; tl++) {
        float ty = -12.0f + tl * 1.0f;
        glBegin(GL_LINES);
        glVertex2f(-60.0f, ty);
        glVertex2f( 60.0f, ty);
        glEnd();
    }
    // Pavement tile lines (vertical grooves every 6 units)
    for (int tv = -60; tv <= 60; tv += 6) {
        glBegin(GL_LINES);
        glVertex2f((float)tv, -12.0f);
        glVertex2f((float)tv,  -9.0f);
        glEnd();
    }

    // Kerb edge (bright top lip)
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

    // 5. VEHICLES (drawn on top of road markings)
    Bus();
    CarRed();
    CarGreen();
    CargoTruck();
    SmallTruck();
    CarYellow();

    // ── EXHAUST SMOKE PUFFS ──────────────────────────────────────────────────
    DrawExhaustSmoke();

    // ── STREET FURNITURE: benches + a seated pedestrian ─────────────────────
    DrawBench(-52.0f);
    DrawSeatedPerson(-52.0f);
    DrawBench(36.0f);

    // 6. PEDESTRIANS (drawn on top of pavement and road when crossing)
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
    // Drawn last among the boats so this anchored, near-shore boat is never
    // hidden behind a passing ship further out.
    FishingBoat();
    DrawFog();
    drawWeatherParticles();
    DrawNightOverlay();
}

void handleMouse(int button, int /*state*/, int /*x*/, int /*y*/)
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

void handleKeypress(unsigned char key, int /*x*/, int /*y*/)
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

// ---- Adapter layer: exposes the shared 4-scenario contract -----------------
// (Draw / Init / Keyboard / Mouse / kTitle). These are thin wrappers around
// the original functions above -- nothing above this line was changed to
// produce them.
const char* kTitle = "Dynamic Coastal City";

void Init() {
    init();  // original one-time setup (point/line size, initial ortho, blend mode)

    // Kick off this scenario's animation timers (moved here from the
    // original main(), unchanged otherwise).
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

void Draw() {
    // Every scenario clears the screen and (re)asserts its own projection at
    // the top of its own Draw(), every frame. That way it never matters what
    // order scenarios were switched in -- each one always looks right the
    // moment it's selected.
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

void Keyboard(unsigned char key, int x, int y) {
    // SPACE / H are handled globally in the dispatcher; everything else is
    // this scenario's own business.
    handleKeypress(key, x, y);
}

void Mouse(int button, int state, int x, int y) {
    handleMouse(button, state, x, y);
}

} // namespace Scenario1_CoastalCity

// ============================================================================
//  SCENARIO 2  --  placeholder for a teammate's scenario
// ----------------------------------------------------------------------------
//  Replace the TODOs below with your own scene. See TEAMMATE_GUIDE.md for a
//  step-by-step walkthrough. The program already compiles and runs with this
//  placeholder in place, so you can build/run at any time while you work.
// ============================================================================
namespace Scenario2 {

// ============================================================================
//  STATE
// ============================================================================
constexpr float PI2 = 3.1416f;

int  nightPhase2    = 1;     // 0 = dusk, 1 = night, 2 = dawn  (keys '1'/'2'/'3')
bool isRaining2     = true;
bool isAnimating2   = true;
int  activeAdSlide2 = 0;
float neonTime2      = 0.0f;
float pedWalkTimer2  = 0.0f;

// ---- Neon signage ----------------------------------------------------------
struct NeonSign2 {
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

// ---- Buildings / windows ----------------------------------------------------
struct Skyscraper2 {
    float x, baseY, w, topY;
    int   cols, rows;
    bool  lit[6][12];
};
constexpr int NUM_BUILDINGS2 = 6;
// The trailing {} zero-fills `lit`; Init() then randomises it. Without the
// explicit braces the compiler warns about the missing initialiser.
Skyscraper2 towers2[NUM_BUILDINGS2] = {
    { -40.0f, -8.0f, 16.0f, 24.0f, 4, 8 , {} },
    { -12.0f, -8.0f, 14.0f, 30.0f, 4, 10, {} },
    {  15.0f, -8.0f, 15.0f, 34.0f, 5, 11, {} },
    {  41.0f, -8.0f, 13.0f, 22.0f, 4, 7 , {} },
    { -54.0f, -8.0f,  9.0f, 18.0f, 3, 5 , {} },
    {  55.0f, -8.0f, 10.0f, 19.0f, 3, 5 , {} }
};

// ---- Stars -------------------------------------------------------------
constexpr int NUM_STARS2 = 30;
struct Star2 { float x, y; };
Star2 stars2[NUM_STARS2];

// ---- Elevated train ----------------------------------------------------
float trainX2        = -80.0f;
bool  trainActive2   = false;
int   trainCooldown2 = 200;

// ---- Vehicles (data-driven) ---------------------------------------------
// Named vehicle types -- the old code used bare ints 0..4 while the comment
// only documented 0..2, so types 3 and 4 were undocumented magic numbers.
enum VehicleType2 { VEH_TAXI = 0, VEH_BUS, VEH_VAN, VEH_MOTORCYCLE, VEH_LIMO };

struct Vehicle2 {
    int type;            // one of VehicleType2
    float x, laneY, speed;
    int dir;
    unsigned char r, g, b;
    bool braking;        // set by the traffic logic, drives the brake lights
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

// ---- Police car (unique lightbar animation) -----------------------------
float policeX2         = -62.0f;
float policeLightPhase2 = 0.0f;

// ---- Bicycle courier -- the unique detail for this scenario -------------
float bikeX2      = 68.0f;
float bikeSpeed2  = 0.14f;
float pedalAngle2 = 0.0f;

// ---- Pedestrians ---------------------------------------------------------
struct Ped2 {
    float x, y, speed, phase;
    int dir;
    bool umbrella;
    unsigned char shirtR, shirtG, shirtB;
    // Crosswalk behaviour: pedestrians walk the pavement until they reach
    // the crossing, then wait for the walk signal before stepping out.
    bool  wantsToCross;   // this pedestrian uses the crossing at all
    int   crossState;     // 0 = walking pavement, 1 = waiting, 2 = crossing
    float crossT;         // 0..1 progress across the road
};
constexpr int NUM_PEDS2 = 8;
Ped2 peds2[NUM_PEDS2] = {
    // The last three fields are the crosswalk state: whether this person
    // uses the crossing, their current crossing phase, and progress across.
    { -46.0f, -6.3f, 0.07f, 0.0f,  1, true,  200,  60,  60, true,  0, 0.0f },
    { -20.0f, -6.3f, 0.09f, 1.1f, -1, false,  60, 120, 200, false, 0, 0.0f },
    {   5.0f, -6.3f, 0.06f, 2.2f,  1, true,   90, 180, 100, false, 0, 0.0f },
    {  25.0f, -6.3f, 0.08f, 3.1f, -1, false, 220, 190,  60, true,  0, 0.0f },
    {  48.0f, -6.3f, 0.07f, 4.0f,  1, false, 180,  90, 200, false, 0, 0.0f },
    {  -2.0f, -6.3f, 0.00f, 5.0f,  1, false,  80,  80,  85, false, 0, 0.0f },
    {  15.0f, -6.3f, 0.085f,5.5f, -1, false, 160, 200, 210, true,  0, 0.0f },
    { -33.0f, -6.3f, 0.065f,6.0f,  1, true,   70, 130, 160, false, 0, 0.0f }
};

// ---- Street performer -----------------------------------------------------
float performerArmAngle2 = 0.0f;

// ---- Rain particles ---------------------------------------------------
constexpr int MAX_RAIN2 = 260;
float rainX2[MAX_RAIN2], rainY2[MAX_RAIN2], rainLen2[MAX_RAIN2];

// ---- Steam vent particles ------------------------------------------------
struct SteamPuff2 { float x, y, vy, alpha, size; bool active; };
constexpr int MAX_STEAM2 = 40;
SteamPuff2 steam2[MAX_STEAM2];

// ---- Traffic light -----------------------------------------------------
int trafficState2 = 0;
int trafficTimer2 = 0;

// ---- Ad carousel text -----------------------------------------------------
const char* adTexts2[3] = { "FRESH CITY EATS", "NOVA MOBILE 5G", "GRAND HOTEL - BOOK NOW" };

// ---- Helicopter -- the signature feature for this scenario -----------------
float heliX2          = -85.0f;
float heliY2          = 22.0f;
float heliScale2      = 0.4f;
float heliPropAngle2  = 0.0f;
float heliSearchAngle2 = 0.0f;

// ---- Subway glimpsed through a street-level grate --------------------------
float grateFlicker2   = 0.0f;
bool  grateRumbling2  = false;
int   grateCooldown2  = 220;

// ---- Rain-puddle ripple rings ---------------------------------------------
struct Ripple2 { float x, y, radius, alpha; bool active; };
constexpr int MAX_RIPPLES2 = 24;
Ripple2 ripples2[MAX_RIPPLES2];

// ---- Thunderstorm mode (toggled with 'T') ---------------------------------
bool  isThunderstorm2      = false;
float lightningFlash2      = 0.0f;
bool  lightningBoltActive2 = false;
float lightningBoltX2      = 0.0f;
int   lightningCooldown2   = 200;
// The bolt's zig-zag is baked once at strike time. It used to be generated
// with rand() inside the draw call, so the "bolt" redrew as a completely
// different shape on every frame of its ~15-frame life -- it read as noise
// rather than as one flash of lightning. A draw function must be pure.
constexpr int   BOLT_NODES2 = 9;
float boltX2[BOLT_NODES2], boltY2[BOLT_NODES2];
float boltBranchX2[3], boltBranchY2[3];   // short forks off the main channel
int   boltBranchFrom2[3];

// ============================================================================
//  SMALL HELPERS
// ============================================================================
// ============================================================================
//  DAY-PHASE LIGHTING
// ----------------------------------------------------------------------------
//  `nightPhase2` used to appear in exactly two places -- DrawSky2 and
//  DrawStars2 -- so pressing 1/2/3 changed the gradient behind the towers and
//  absolutely nothing else. The buildings, road, vehicles, rain and people
//  were pixel-identical at dusk, night and dawn, which made a three-way
//  control feel broken.
//
//  This is Scenario 3's tint system ported across: every draw call in the
//  namespace now goes through TintRGB2/TintRGBA2, which multiply the colour
//  by a per-phase factor. `tintOn2` lets things that EMIT light -- the sky,
//  neon, lit windows, headlights, lightning -- opt out, since they define the
//  light rather than receive it.
// ============================================================================
bool tintOn2 = true;

struct Tint2 { float r, g, b; };

Tint2 GetTint2() {
    if (nightPhase2 == 0) return { 1.14f, 0.92f, 0.80f };  // dusk: warm, low sun
    if (nightPhase2 == 1) return { 0.86f, 0.90f, 1.10f };  // night: cold and dim
    return                       { 0.98f, 1.00f, 1.12f };  // dawn: pale blue-grey
}

// Overall exposure, on top of the hue shift: dusk still has sky light in it,
// deep night does not.
float Exposure2() {
    if (nightPhase2 == 0) return 1.22f;
    if (nightPhase2 == 1) return 0.86f;
    return 1.05f;
}

inline unsigned char ClampByte2(float v) {
    if (v < 0.0f)   return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}

inline void TintRGBA2(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!tintOn2) { glColor4ub(r, g, b, a); return; }
    Tint2 t = GetTint2();
    float e = Exposure2();
    glColor4ub(ClampByte2(r * t.r * e), ClampByte2(g * t.g * e), ClampByte2(b * t.b * e), a);
}

inline void TintRGB2(unsigned char r, unsigned char g, unsigned char b) {
    TintRGBA2(r, g, b, 255);
}

// Small scope guard so an emissive draw function can opt out in one line and
// be guaranteed to restore the flag, even with early returns.
struct NoTint2 {
    bool saved;
    NoTint2() : saved(tintOn2) { tintOn2 = false; }
    ~NoTint2() { tintOn2 = saved; }
};

void FilledCircle2(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    TintRGBA2(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 24;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI2;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}

struct SkyColor2 { float r, g, b; };
SkyColor2 LerpC2(SkyColor2 a, SkyColor2 b, float t) {
    SkyColor2 o;
    o.r = a.r + (b.r - a.r) * t;
    o.g = a.g + (b.g - a.g) * t;
    o.b = a.b + (b.b - a.b) * t;
    return o;
}

// A light cone fanning out from (xSource,ySource) in direction (dx,dy),
// fading toward its edges. Self-contained copy of the same technique used
// in Scenario 1, so this namespace doesn't depend on another one's helpers.
void DrawLightCone2(float xSource, float ySource, float dx, float dy, float length, float spreadWidth, unsigned char r, unsigned char g, unsigned char b, unsigned char maxAlpha) {
    NoTint2 emissive;   // a light cone is light itself
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

// ============================================================================
//  SKY / SKYLINE / TRANSIT
// ============================================================================
void DrawSky2() {
    NoTint2 emissive;   // the sky IS the phase colour
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

void DrawStars2() {
    NoTint2 emissive;   // starlight is not lit by the scene
    // Stars fade rather than vanish: a few of the brightest still hang on at
    // dusk and dawn instead of the whole field switching off at once.
    int visible = (nightPhase2 == 1) ? NUM_STARS2 : NUM_STARS2 / 4;
    unsigned char alpha = (nightPhase2 == 1) ? 255 : 110;
    TintRGBA2(255, 255, 255, alpha);
    glPointSize(1.6f);
    glBegin(GL_POINTS);
        for (int i = 0; i < visible; i++) glVertex2f(stars2[i].x, stars2[i].y);
    glEnd();
    glPointSize(2.0f);
}

void DrawSkyline2() {
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

void DrawTrainTrack2() {
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

void DrawTrain2() {
    if (!trainActive2) return;
    float y = 9.0f;

    // Three separate cars with couplings, rather than one long grey box.
    for (int c = 0; c < 3; c++) {
        float cx = trainX2 + c * 10.2f;

        // Coupling to the car behind
        if (c > 0) {
            TintRGB2(70, 70, 78);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
                glVertex2f(cx - 0.2f, y + 1.2f); glVertex2f(cx, y + 1.2f);
            glEnd();
        }

        // Body, with a darker skirt so it doesn't read as a flat slab
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

        // Windows: brightness varies car to car and seat to seat, so the
        // train looks occupied rather than uniformly lit.
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

    // Headlamp on the leading car, throwing light along the track
    float leadX = trainX2 + 3 * 10.2f - 0.6f;
    FilledCircle2(leadX, y + 1.4f, 0.30f, 255, 250, 210, 255);
    DrawLightCone2(leadX, y + 1.4f, 1.0f, -0.05f, 11.0f, 1.7f, 255, 245, 200, 90);
    glLineWidth(1.0f);
}

void UpdateTrain2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateTrain2, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  HELICOPTER -- signature feature: a police/news helicopter with a
//  searchlight, sweeping across the night sky behind the elevated train
//  track and buildings, then in front, mirroring the cross-depth flight
//  technique Scenario 1 uses for its own helicopter.
// ============================================================================
void DrawHelicopter2() {
    glPushMatrix();
    glTranslatef(heliX2, heliY2, 0.0f);
    glScalef(heliScale2, heliScale2, 1.0f);

    // Body
    FilledCircle2(0.0f, 0.0f, 1.6f, 30, 35, 45, 255);
    // Windshield
    TintRGBA2(120, 200, 230, 220);
    glBegin(GL_POLYGON);
        glVertex2f(0.3f, 0.7f);
        glVertex2f(1.3f, 0.25f);
        glVertex2f(1.1f, -0.5f);
        glVertex2f(0.0f, -0.5f);
    glEnd();
    // Tail boom
    TintRGB2(25, 30, 38);
    glBegin(GL_QUADS);
        glVertex2f(-1.4f, 0.18f);
        glVertex2f(-3.6f, 0.7f);
        glVertex2f(-3.6f, 0.45f);
        glVertex2f(-1.4f, -0.18f);
    glEnd();
    // Tail fin
    glBegin(GL_TRIANGLES);
        glVertex2f(-3.6f, 0.45f);
        glVertex2f(-3.9f, 1.6f);
        glVertex2f(-3.3f, 0.45f);
    glEnd();
    // Rotor mast
    TintRGB2(20, 20, 24);
    glBegin(GL_QUADS);
        glVertex2f(-0.15f, 1.4f);
        glVertex2f(0.15f, 1.4f);
        glVertex2f(0.15f, 1.9f);
        glVertex2f(-0.15f, 1.9f);
    glEnd();
    // Skids
    glLineWidth(2.0f);
    TintRGB2(20, 20, 24);
    glBegin(GL_LINES);
        glVertex2f(-0.7f, -1.4f); glVertex2f(-0.9f, -1.9f);
        glVertex2f(0.7f, -1.4f);  glVertex2f(0.5f, -1.9f);
        glVertex2f(-1.4f, -1.9f); glVertex2f(1.4f, -1.9f);
    glEnd();
    // Main rotor (spinning)
    glPushMatrix();
        glTranslatef(0.0f, 1.9f, 0.0f);
        glRotatef(heliPropAngle2, 0.0f, 0.0f, 1.0f);
        TintRGB2(15, 15, 18);
        glBegin(GL_LINES);
            glVertex2f(0.0f, 0.0f); glVertex2f(4.2f, 0.0f);
            glVertex2f(0.0f, 0.0f); glVertex2f(-4.2f, 0.0f);
        glEnd();
    glPopMatrix();
    // Tail rotor (spinning)
    glPushMatrix();
        glTranslatef(-3.6f, 1.15f, 0.0f);
        glRotatef(heliPropAngle2 * 1.6f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_LINES);
            glVertex2f(0.0f, 0.0f); glVertex2f(0.9f, 0.0f);
            glVertex2f(0.0f, 0.0f); glVertex2f(-0.9f, 0.0f);
        glEnd();
    glPopMatrix();
    // Alternating red/white anti-collision beacon
    bool redPhase = fmodf(neonTime2, 1.2f) < 0.6f;
    float flash = 0.5f + 0.5f * sinf(heliPropAngle2 * 0.05f);
    if (redPhase) FilledCircle2(-0.2f, 1.5f, 0.25f, 255, 40, 40, (unsigned char)(220 * flash));
    else          FilledCircle2(-0.2f, 1.5f, 0.25f, 255, 255, 255, (unsigned char)(220 * flash));

    // Searchlight sweeping the street below
    float sweep = sinf(heliSearchAngle2) * 0.5f;
    DrawLightCone2(0.6f, -0.5f, 0.35f + sweep, -1.0f, 16.0f, 3.0f, 255, 255, 210, 150);

    glPopMatrix();
}

void UpdateHelicopter2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateHelicopter2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        heliX2 += 0.22f;
        heliPropAngle2 -= 30.0f;
        if (heliPropAngle2 < -360.0f) heliPropAngle2 += 360.0f;
        heliSearchAngle2 += 0.04f;
        // Grows as it "approaches", crossing from small/background to
        // large/foreground partway through its flight -- this is what
        // actually drives the behind-then-in-front layering in Draw().
        if (heliScale2 < 1.2f) heliScale2 += 0.0012f;
        if (heliX2 > 85.0f) {
            heliX2 = -85.0f - (rand() % 20);
            heliY2 = 18.0f + (rand() % 8);
            heliScale2 = 0.28f + (rand() % 10) / 100.0f;
        }
    }
    glutTimerFunc(25, UpdateHelicopter2, 0);
}

// ============================================================================
//  BUILDINGS
// ============================================================================
void DrawBuildingBody2(const Skyscraper2& t) {
    TintRGB2(30, 28, 42);
    glBegin(GL_QUADS);
        glVertex2f(t.x - t.w*0.5f, t.baseY);
        glVertex2f(t.x + t.w*0.5f, t.baseY);
        glVertex2f(t.x + t.w*0.5f, t.topY);
        glVertex2f(t.x - t.w*0.5f, t.topY);
    glEnd();
}

void DrawWindows2(const Skyscraper2& t) {
    float cellW = t.w / (t.cols + 1);
    float cellH = (t.topY - t.baseY) / (t.rows + 1);
    for (int c = 0; c < t.cols; c++) {
        for (int r = 0; r < t.rows; r++) {
            float wx = t.x - t.w*0.5f + cellW*(c+1) - cellW*0.3f;
            float wy = t.baseY + cellH*(r+1) - cellH*0.3f;
            // A lit window emits, so it keeps its own colour at every phase;
            // a dark one is just glass and takes the ambient tint.
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

void DrawACUnit2(float x, float y) {
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

void DrawFireEscape2(const Skyscraper2& t, float side) {
    // side = +1.0 attaches to the building's right edge, -1.0 to its left edge
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

void DrawRooftopParty2(float topX, float topY) {
    // Low railing around the roof edge
    TintRGB2(50, 50, 58);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(topX-3.2f, topY);     glVertex2f(topX+3.2f, topY);
        glVertex2f(topX-3.2f, topY);     glVertex2f(topX-3.2f, topY+0.5f);
        glVertex2f(topX+3.2f, topY);     glVertex2f(topX+3.2f, topY+0.5f);
    glEnd();
    // Tiny string of party lights strung across the roof
    unsigned char cols[3][3] = { {230, 90, 160}, {90, 200, 230}, {230, 200, 90} };
    for (int i = 0; i < 7; i++) {
        float t = (float)i / 6.0f;
        float lx = topX - 2.8f + t * 5.6f;
        float ly = topY + 0.6f + sinf(t * PI2) * 0.3f;
        int c = i % 3;
        float tw = 0.6f + 0.4f * sinf(neonTime2*2.0f + i*0.8f);
        FilledCircle2(lx, ly, 0.15f, cols[c][0], cols[c][1], cols[c][2], (unsigned char)(160 + 90*tw));
    }
    // Small bar counter
    TintRGB2(90, 60, 45);
    glBegin(GL_QUADS);
        glVertex2f(topX-1.2f, topY);      glVertex2f(topX+0.4f, topY);
        glVertex2f(topX+0.4f, topY+0.7f); glVertex2f(topX-1.2f, topY+0.7f);
    glEnd();
    // A few mingling silhouettes
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

void DrawRooftopProps2(int index, const Skyscraper2& t) {
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
        // Buildings 3, 5: a couple of rooftop AC units for skyline texture
        DrawACUnit2(topX - 1.5f, topY);
        DrawACUnit2(topX + 1.2f, topY);
    }
}

void DrawSubwayEntrance2() {
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

// ---- Graffiti / street art on a wall ---------------------------------------
void DrawGraffiti2() {
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

// ---- Diner interior, visible through its storefront window ----------------
void DrawDinerInterior2() {
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

// ============================================================================
//  NEON SIGNS / BILLBOARD / PUDDLE REFLECTIONS
// ============================================================================
void DrawNeonSign2(const NeonSign2& s, float pulse) {
    NoTint2 emissive;   // neon emits
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

void DrawAllNeon2() {
    for (int i = 0; i < NUM_NEON2; i++) {
        float pulse = 0.55f + 0.45f * sinf(neonTime2 + neonSigns2[i].phase);
        DrawNeonSign2(neonSigns2[i], pulse);
    }
}

void DrawPuddleGlow2() {
    NoTint2 emissive;   // this is reflected neon, not lit surface
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

// ---- Expanding rain-puddle ripple rings ------------------------------------
void InitRipples2() {
    for (int i = 0; i < MAX_RIPPLES2; i++) ripples2[i].active = false;
}

void SpawnRipple2() {
    for (int i = 0; i < MAX_RIPPLES2; i++) {
        if (!ripples2[i].active) {
            ripples2[i].active = true;
            ripples2[i].x = -58.0f + (rand() % 1160) / 10.0f;
            ripples2[i].y = -19.5f + (rand() % 100) / 10.0f; // puddles on the road surface
            ripples2[i].radius = 0.05f;
            ripples2[i].alpha = 160.0f;
            return;
        }
    }
}

void DrawRipples2() {
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

void UpdateRipples2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateRipples2, 0); return; }  // idle while this scenario is off-screen
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

// ---- Thunderstorm mode: occasional bolt + screen-wide flash ---------------
// Bakes one strike into boltX2/boltY2. Called from the update, never a draw.
void SpawnLightningBolt2() {
    float x = lightningBoltX2, y = 40.0f;
    for (int i = 0; i < BOLT_NODES2; i++) {
        boltX2[i] = x;
        boltY2[i] = y;
        x += (rand() % 100 - 50) / 22.0f;
        y -= 5.0f;
    }
    // Three forks peeling off random nodes of the main channel
    for (int b = 0; b < 3; b++) {
        int from = 2 + rand() % (BOLT_NODES2 - 4);
        boltBranchFrom2[b] = from;
        boltBranchX2[b] = boltX2[from] + ((rand() % 100) - 50) / 14.0f;
        boltBranchY2[b] = boltY2[from] - 2.0f - (rand() % 200) / 100.0f;
    }
}

void DrawLightningBolt2() {
    NoTint2 emissive;   // lightning emits
    if (!lightningBoltActive2) return;

    // Wide soft halo behind the channel, then the hot core on top.
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

void DrawLightningFlash2() {
    NoTint2 emissive;   // lightning emits
    if (lightningFlash2 <= 0.01f) return;
    TintRGBA2(220, 230, 255, (unsigned char)(lightningFlash2 * 170));
    glBegin(GL_QUADS);
        glVertex2f(-60, -40); glVertex2f(60, -40);
        glVertex2f(60, 40);   glVertex2f(-60, 40);
    glEnd();
}

void UpdateLightning2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateLightning2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2 && isThunderstorm2) {
        lightningCooldown2--;
        if (lightningCooldown2 <= 0) {
            lightningFlash2 = 1.0f;
            lightningBoltActive2 = true;
            lightningBoltX2 = -50.0f + (rand() % 1000) / 10.0f;
            SpawnLightningBolt2();            // shape fixed for the whole strike
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

void DrawBillboard2() {
    NoTint2 emissive;   // a lit screen emits
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

void UpdateNeon2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateNeon2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) neonTime2 += 0.05f;
    glutTimerFunc(20, UpdateNeon2, 0);
}

void UpdateBillboard2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateBillboard2, 0); return; }  // idle while this scenario is off-screen
    static int t = 0;
    if (isAnimating2) {
        t++;
        if (t > 150) { t = 0; activeAdSlide2 = (activeAdSlide2 + 1) % 3; }
    }
    glutTimerFunc(30, UpdateBillboard2, 0);
}

// ============================================================================
//  ROAD / TRAFFIC LIGHT / BIKE LANE / STREET VENT
// ============================================================================
void DrawBikeLane2() {
    TintRGB2(30, 110, 60);
    glBegin(GL_QUADS);
        glVertex2f(-60, -9.0f); glVertex2f(60, -9.0f);
        glVertex2f(60, -8.0f);  glVertex2f(-60, -8.0f);
    glEnd();
}

void DrawRoad2() {
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

void DrawStreetVent2() {
    TintRGB2(35, 35, 38);
    glBegin(GL_QUADS);
        glVertex2f(-2.8f, -8.3f); glVertex2f(-1.2f, -8.3f);
        glVertex2f(-1.2f, -8.0f); glVertex2f(-2.8f, -8.0f);
    glEnd();
}

void DrawTrafficLight2() {
    NoTint2 emissive;   // signal lamps emit
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

void UpdateTrafficLight2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateTrafficLight2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        trafficTimer2++;
        if      (trafficState2 == 0 && trafficTimer2 > 150) { trafficState2 = 1; trafficTimer2 = 0; }
        else if (trafficState2 == 1 && trafficTimer2 > 40)  { trafficState2 = 2; trafficTimer2 = 0; }
        else if (trafficState2 == 2 && trafficTimer2 > 150) { trafficState2 = 0; trafficTimer2 = 0; }
    }
    glutTimerFunc(30, UpdateTrafficLight2, 0);
}

// ============================================================================
//  STREET FURNITURE: crosswalk signal, parked cars, trash can, hydrant,
//  newsstand, awning, and a subway grate glimpsed from street level
// ============================================================================
void DrawCrosswalkSignal2() {
    NoTint2 emissive;   // signal lamps emit
    float x = -8.0f, y = -8.5f;
    TintRGB2(30, 30, 32);
    glBegin(GL_QUADS);
        glVertex2f(x-0.5f, y);      glVertex2f(x+0.5f, y);
        glVertex2f(x+0.5f, y+1.2f); glVertex2f(x-0.5f, y+1.2f);
    glEnd();
    bool walkSignal = (trafficState2 == 2); // cars stopped on red -> pedestrians may walk
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

void DrawParkedCar2(float x, unsigned char r, unsigned char g, unsigned char b) {
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

void DrawParkedCars2() {
    DrawParkedCar2(-28.0f, 150, 150, 155);
    DrawParkedCar2(-22.0f,  90,  40,  40);
}

void DrawTrashCan2(float x) {
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

void DrawFireHydrant2(float x) {
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

void DrawNewsstand2() {
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
    // Vendor standing behind the counter
    FilledCircle2(x-0.9f, y+2.0f, 0.28f, 220, 180, 140, 255);
    TintRGB2(70, 90, 130);
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(x-0.9f, y+1.75f); glVertex2f(x-0.9f, y+0.9f); glEnd();
}

void DrawAwning2(float x1, float x2, float y) {
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

void DrawSubwayGrate2() {
    float x = -16.0f, y = -10.6f;
    TintRGB2(20, 20, 22);
    glBegin(GL_QUADS);
        glVertex2f(x-1.8f, y-0.5f); glVertex2f(x+1.8f, y-0.5f);
        glVertex2f(x+1.8f, y+0.5f); glVertex2f(x-1.8f, y+0.5f);
    glEnd();

    // Flickering warm glow through the slats when a train rumbles by below
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

void UpdateSubwayGrate2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateSubwayGrate2, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  VEHICLES
// ============================================================================
// How far a vehicle's nose sticks out in front of its centre, so different
// vehicle lengths queue without visually overlapping.
inline float VehicleHalfLen2(int type) {
    switch (type) {
        case VEH_BUS:        return 4.6f;
        case VEH_LIMO:       return 4.3f;
        case VEH_VAN:        return 3.1f;
        case VEH_MOTORCYCLE: return 1.3f;
        default:             return 2.7f;   // taxi
    }
}

void DrawTaxiShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b, int dir) {
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

void DrawBusShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b) {
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

void DrawVanShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b) {
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

void DrawMotorcycleShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b, int dir) {
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

void DrawLimoShape2(float x, float y, unsigned char r, unsigned char g, unsigned char b) {
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

// A vehicle's headlights, thrown forward along the wet road. The cone is
// drawn BEFORE the body so the car sits inside its own light, and its
// brightness lifts in the rain because there is more in the air to catch it.
void DrawHeadlightCone2(const Vehicle2& v) {
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

    // The lamps themselves
    FilledCircle2(nose, ly, 0.85f, 255, 240, 200, 55);
    FilledCircle2(nose, ly, 0.20f, 255, 250, 230, 255);

    // Wet-road smear directly under the beam
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

void DrawVehicles2() {
    for (int i = 0; i < NUM_VEHICLES2; i++) DrawHeadlightCone2(vehicles2[i]);

    for (int i = 0; i < NUM_VEHICLES2; i++) {
        Vehicle2& v = vehicles2[i];
        if      (v.type == VEH_TAXI) DrawTaxiShape2(v.x, v.laneY, v.r, v.g, v.b, v.dir);
        else if (v.type == VEH_BUS)  DrawBusShape2(v.x, v.laneY, v.r, v.g, v.b);
        else if (v.type == VEH_VAN)  DrawVanShape2(v.x, v.laneY, v.r, v.g, v.b);
        else if (v.type == VEH_MOTORCYCLE) DrawMotorcycleShape2(v.x, v.laneY, v.r, v.g, v.b, v.dir);
        else                          DrawLimoShape2(v.x, v.laneY, v.r, v.g, v.b);

        // Brake lights: red lamps plus a soft halo on the rear of the car,
        // so a vehicle held at the red light visibly *reads* as braking.
        if (v.braking) {
            float rear = v.x - v.dir * (VehicleHalfLen2(v.type) - 0.2f);
            float ly   = v.laneY + 0.55f;
            FilledCircle2(rear, ly, 0.9f, 255, 40, 30, 55);
            FilledCircle2(rear, ly, 0.22f, 255, 70, 50, 245);
        }
    }
}

// ---- Traffic logic ---------------------------------------------------------
// trafficState2: 0 = green (traffic flows), 1 = amber, 2 = red (traffic
// stops, pedestrians get the walk signal). Previously the light cycled
// while every vehicle drove straight through it, which made the whole
// intersection meaningless.
inline float StopLineFor2(int dir) { return (dir > 0) ? -6.8f : 6.8f; }

void UpdateVehicles2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateVehicles2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        for (int i = 0; i < NUM_VEHICLES2; i++) {
            Vehicle2& v = vehicles2[i];
            float nextX = v.x + v.speed * v.dir;
            bool  braking = false;

            // 1. Obey the signal: creep up to the stop line but never cross
            //    it while the light is against us.
            if (trafficState2 != 0) {
                float line = StopLineFor2(v.dir);
                bool before     = (v.dir > 0) ? (v.x    <= line) : (v.x    >= line);
                bool wouldCross = (v.dir > 0) ? (nextX  >  line) : (nextX  <  line);
                if (before && wouldCross) { nextX = line; braking = true; }
            }

            // 2. Queue: never drive into the back of the vehicle ahead in
            //    the same lane travelling the same way.
            for (int j = 0; j < NUM_VEHICLES2; j++) {
                if (j == i) continue;
                const Vehicle2& o = vehicles2[j];
                if (o.dir != v.dir) continue;
                if (fabsf(o.laneY - v.laneY) > 0.5f) continue;

                float ahead = (o.x - v.x) * v.dir;          // >0 means in front
                if (ahead <= 0.0f) continue;
                float minGap = VehicleHalfLen2(v.type) + VehicleHalfLen2(o.type) + 0.6f;
                float nextAhead = (o.x - nextX) * v.dir;
                if (nextAhead < minGap) {
                    nextX = o.x - v.dir * minGap;            // hold station behind
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

void DrawPoliceCar2() {
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

void UpdatePoliceCar2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdatePoliceCar2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        policeX2 += 0.12f;
        if (policeX2 > 70.0f) policeX2 = -70.0f;
        policeLightPhase2 += 0.12f;
    }
    glutTimerFunc(20, UpdatePoliceCar2, 0);
}

// ---- Bicycle courier: the unique detail for Scenario 2 ---------------------
void DrawBicycle2() {
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

void UpdateBicycle2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateBicycle2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        bikeX2 -= bikeSpeed2;
        if (bikeX2 < -70.0f) bikeX2 = 70.0f;
        pedalAngle2 += 0.25f;
        if (pedalAngle2 > 2*PI2) pedalAngle2 -= 2*PI2;
    }
    glutTimerFunc(20, UpdateBicycle2, 0);
}

// ============================================================================
//  STEAM VENT PARTICLES
// ============================================================================
void InitSteam2() {
    for (int i = 0; i < MAX_STEAM2; i++) steam2[i].active = false;
}

void SpawnSteamPuff2() {
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

void DrawSteam2() {
    for (int i = 0; i < MAX_STEAM2; i++) {
        if (!steam2[i].active) continue;
        FilledCircle2(steam2[i].x, steam2[i].y, steam2[i].size, 200, 200, 210, (unsigned char)steam2[i].alpha);
    }
}

void UpdateSteam2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateSteam2, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  FOOD TRUCK / STREET PERFORMER / PEDESTRIANS
// ============================================================================
void DrawFoodTruck2() {
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

void DrawStreetPerformer2() {
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

void UpdateStreetPerformer2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateStreetPerformer2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) performerArmAngle2 += 0.15f;
    glutTimerFunc(30, UpdateStreetPerformer2, 0);
}

void DrawPerson2(const Ped2& p, float walkTimer) {
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

    // Umbrellas are weather, not decoration: they go up when it rains and
    // come down when it stops, so the R key visibly changes the street's
    // population instead of only the particles.
    if (p.umbrella && isRaining2) {
        // Each umbrella keeps its own colour, so the crowd is not uniform.
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
        // Scalloped rim, so it reads as fabric over ribs
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
        // Run-off from the rim
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
        // No umbrella: hood up and shoulders hunched against the rain.
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

void DrawPedestrians2() {
    for (int i = 0; i < NUM_PEDS2; i++) DrawPerson2(peds2[i], pedWalkTimer2);
}

void UpdatePedestrians2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdatePedestrians2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        pedWalkTimer2 += 0.12f;

        // The walk signal is green for pedestrians exactly when traffic is
        // held on red, matching DrawCrosswalkSignal2.
        bool walkSignal = (trafficState2 == 2);

        const float CROSS_X   = 0.2f;    // centre of the crossing
        const float PAVEMENT_Y = -6.3f;  // near pavement
        const float FAR_Y      = -19.0f; // far kerb, across the road

        for (int i = 0; i < NUM_PEDS2; i++) {
            Ped2& p = peds2[i];

            if (p.crossState == 2) {
                // Mid-crossing: keep going even if the light changes, which
                // is both safer-looking and what real people do.
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
                // Waiting at the kerb for the signal.
                if (walkSignal) p.crossState = 2;
                continue;
            }

            // Normal pavement walking
            p.x += p.speed * p.dir;

            // Arriving at the crossing: stop and wait rather than walking on.
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

// ============================================================================
//  RAIN / WINDOW FLICKER
// ============================================================================
void InitRain2() {
    for (int i = 0; i < MAX_RAIN2; i++) {
        rainX2[i] = -60.0f + (rand() % 1200) / 10.0f;
        rainY2[i] = (rand() % 800) / 10.0f;
        rainLen2[i] = 1.0f + (rand() % 10) / 10.0f;
    }
}

// ---- Wet road: a cold sheen plus splash marks while the rain is falling --
void DrawWetRoad2() {
    if (!isRaining2) return;

    // Cold sheen over the asphalt, strongest nearest the viewer.
    glBegin(GL_QUADS);
        TintRGBA2(90, 120, 155, 70);
        glVertex2f(-60, -20); glVertex2f(60, -20);
        TintRGBA2(70, 95, 130, 18);
        glVertex2f(60, -9);   glVertex2f(-60, -9);
    glEnd();

    // Splash marks where drops are striking the road. Positions are derived
    // from a stable hash of the index plus the rain clock, so they scatter
    // without flickering randomly every single frame.
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

void DrawRain2() {
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

void UpdateRain2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateRain2, 0); return; }  // idle while this scenario is off-screen
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

void UpdateWindowFlicker2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateWindowFlicker2, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating2) {
        int bIdx = rand() % NUM_BUILDINGS2;
        int c = rand() % towers2[bIdx].cols;
        int r = rand() % towers2[bIdx].rows;
        towers2[bIdx].lit[c][r] = !towers2[bIdx].lit[c][r];
    }
    glutTimerFunc(400, UpdateWindowFlicker2, 0);
}

// ============================================================================
//  ONE WINDOW WITH A STORY
// ----------------------------------------------------------------------------
//  A single lit apartment window with somebody living in it. They cross the
//  room, sit down at a desk, get up, water a plant and cross back, on a slow
//  loop. Viewers always find this kind of detail, and it costs very little.
// ============================================================================
float apartmentT2 = 0.0f;

void DrawApartmentWindow2() {
    NoTint2 emissive;                      // a lit room emits

    const float wx = 13.6f, wy = 12.0f, w = 3.4f, h = 4.2f;

    // Warm room light, and the spill onto the wall around the frame
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

    // Furniture silhouettes: a desk on the right, a plant on the left
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

    // ---- The occupant --------------------------------------------------
    //   0.00-0.25  walking right     0.25-0.55  sitting at the desk
    //   0.55-0.70  standing up       0.70-0.85  at the plant
    //   0.85-1.00  walking back left
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
    glBegin(GL_LINES);                                   // legs
        glVertex2f(fx, fy + bodyH * 0.45f); glVertex2f(fx + stride, fy);
        glVertex2f(fx, fy + bodyH * 0.45f); glVertex2f(fx - stride, fy);
    glEnd();
    glLineWidth(3.4f);
    glBegin(GL_LINES);                                   // torso
        glVertex2f(fx, fy + bodyH * 0.45f); glVertex2f(fx, fy + bodyH);
    glEnd();
    FilledCircle2(fx, fy + bodyH + 0.26f, 0.24f, 42, 34, 40, 255);
    glLineWidth(1.0f);

    // ---- Frame ----------------------------------------------------------
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

void UpdateApartment2(int) {
    if (currentScreen != SCENARIO_2 || isPaused) { glutTimerFunc(120, UpdateApartment2, 0); return; }
    if (isAnimating2) apartmentT2 += 0.0011f;
    glutTimerFunc(30, UpdateApartment2, 0);
}

// ============================================================================
//  NEAR SIDEWALK -- the foreground the street never had
// ----------------------------------------------------------------------------
//  The road stopped dead at y = -20 and nothing was drawn below it, so the
//  bottom quarter of the window was flat black. Rain-slicked pavement
//  reflecting the signs above is the single most recognisable image in this
//  genre, and every neon position needed to project one was already here.
// ============================================================================
constexpr float kerbY2      = -20.0f;
constexpr float nearWalkY2  = -40.0f;

// Vertical smears of each neon sign, stretched down the wet paving. Alpha
// falls with distance and the streak wanders as the surface water moves.
void DrawNeonReflections2() {
    NoTint2 emissive;   // reflected light is light

    for (int i = 0; i < NUM_NEON2; i++) {
        const NeonSign2& sg = neonSigns2[i];
        float pulse = 0.62f + 0.38f * sinf(neonTime2 * 2.0f + sg.phase);
        float wet   = isRaining2 ? 1.0f : 0.45f;   // dry asphalt barely reflects

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

void DrawNearSidewalk2() {
    // Wet paving, darker at the kerb and catching more light nearer camera
    glBegin(GL_QUADS);
        TintRGB2(26, 26, 32);
        glVertex2f(-60.0f, nearWalkY2); glVertex2f(60.0f, nearWalkY2);
        TintRGB2(15, 15, 20);
        glVertex2f(60.0f, kerbY2);      glVertex2f(-60.0f, kerbY2);
    glEnd();

    // Kerb stone
    TintRGB2(74, 74, 82);
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, kerbY2 - 1.1f); glVertex2f(60.0f, kerbY2 - 1.1f);
        glVertex2f(60.0f, kerbY2);         glVertex2f(-60.0f, kerbY2);
    glEnd();

    DrawNeonReflections2();

    // Paving joints, fanning toward a vanishing point on the horizon
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

    // Standing water: a couple of broad puddles with a bright rim
    if (isRaining2) {
        TintRGBA2(40, 48, 64, 150);
        DrawSoftEllipse(-24.0f, -28.0f, 15.0f, 3.0f, 40, 48, 64, 150, 2);
        DrawSoftEllipse( 22.0f, -33.0f, 18.0f, 3.4f, 40, 48, 64, 150, 2);
    }

    // Storm drain at the kerb, with water running into it
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

    // Bollards along the kerb, oversized because they are near the camera
    for (int i = -3; i <= 3; i++) {
        float bx = i * 17.0f + 6.0f;
        TintRGB2(46, 48, 56);
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.75f, kerbY2 - 6.0f); glVertex2f(bx + 0.75f, kerbY2 - 6.0f);
            glVertex2f(bx + 0.60f, kerbY2 - 1.2f); glVertex2f(bx - 0.60f, kerbY2 - 1.2f);
        glEnd();
        TintRGB2(190, 170, 60);                  // reflective band
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.68f, kerbY2 - 2.6f); glVertex2f(bx + 0.68f, kerbY2 - 2.6f);
            glVertex2f(bx + 0.66f, kerbY2 - 2.0f); glVertex2f(bx - 0.66f, kerbY2 - 2.0f);
        glEnd();
    }
    glLineWidth(1.0f);
}

// ---- Large foreground figures under umbrellas ----------------------------
float nearWalker2X[2]    = { -30.0f, 26.0f };
const float nearWalker2Y[2] = { -31.0f, -36.5f };
const int   nearWalker2Dir[2] = { 1, -1 };

void DrawNearWalker2(int idx) {
    float x  = nearWalker2X[idx];
    float y  = nearWalker2Y[idx];
    float sc = DepthScaleRange(y, kerbY2, nearWalkY2, 1.5f, 2.6f);
    int   dir = nearWalker2Dir[idx];
    float t  = pedWalkTimer2 + idx * 2.0f;

    unsigned char cr = (idx == 0) ? 46 : 70, cg = (idx == 0) ? 50 : 40, cb = (idx == 0) ? 68 : 52;

    BeginDepthSprite(x, y, sc);

    // Legs
    TintRGB2(24, 24, 30);
    glLineWidth(3.4f);
    glBegin(GL_LINES);
        glVertex2f(x, y + 1.9f); glVertex2f(x + 0.55f*sinf(t*3.0f), y);
        glVertex2f(x, y + 1.9f); glVertex2f(x - 0.55f*sinf(t*3.0f), y);
    glEnd();
    // Coat
    TintRGB2(cr, cg, cb);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.85f, y + 1.6f); glVertex2f(x + 0.85f, y + 1.6f);
        glVertex2f(x + 0.70f, y + 4.3f); glVertex2f(x - 0.70f, y + 4.3f);
    glEnd();
    FilledCircle2(x, y + 4.8f, 0.52f, 60, 58, 66, 255);

    // Umbrella, only while it is actually raining
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
        // Run-off dripping from the rim
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

void DrawNearWalkers2() {
    // Nearest last
    if (nearWalker2Y[0] < nearWalker2Y[1]) { DrawNearWalker2(1); DrawNearWalker2(0); }
    else                                   { DrawNearWalker2(0); DrawNearWalker2(1); }
}

void UpdateNearWalkers2(int) {
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

// ============================================================================
//  CONTRACT: Init / Draw / Keyboard / Mouse / kTitle
// ============================================================================
const char* kTitle = "Downtown Neon District";

void Init() {
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

void Draw() {
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

    // Helicopter, far away: drawn behind the buildings
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

    // Helicopter, close: drawn in front of the buildings
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

    // ---- NEAR SIDEWALK (foreground) ------------------------------------
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

void Keyboard(unsigned char key, int /*x*/, int /*y*/) {
    switch (key) {
        case '1': nightPhase2 = 0; break;
        case '2': nightPhase2 = 1; break;
        case '3': nightPhase2 = 2; break;
        // Rain and the storm are one weather system, not two independent
        // switches: a thunderstorm over a dry, clear street made no sense.
        case 'r': case 'R':
            isRaining2 = !isRaining2;
            if (!isRaining2) isThunderstorm2 = false;     // no storm without rain
            break;
        case 't': case 'T':
            isThunderstorm2 = !isThunderstorm2;
            if (isThunderstorm2) isRaining2 = true;       // a storm brings its rain
            break;

        // Lightning strikes on a 200-600 tick cooldown, so during a live demo
        // it can easily never fire. 'X' triggers one immediately.
        case 'x': case 'X':
            isThunderstorm2 = true;
            isRaining2 = true;
            lightningCooldown2 = 1;
            break;
    }
    glutPostRedisplay();
}

void Mouse(int button, int state, int /*x*/, int /*y*/) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        isAnimating2 = true;
    }
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        // Right-click pauses only. It used to also switch the rain off, which
        // was inconsistent with Scenarios 3 and 4 and made 'R' feel broken.
        isAnimating2 = false;
    }
    glutPostRedisplay();
}

} // namespace Scenario2

// ============================================================================
//  SCENARIO 3  --  Riverside Park
// ----------------------------------------------------------------------------
//  Lifted from scenario3final.cpp unchanged. This scenario carries its own
//  copies of DepthScale, DrawGroundShadow, DrawSoftEllipse, the reflection
//  helpers and DrawSceneHUD, declared INSIDE the namespace. Their signatures
//  match the shared ones above, but the bodies differ -- a shallower depth
//  band, a softer contact shadow, a HUD panel that sizes itself to its
//  longest line -- and the scene is tuned against those. Keeping them here
//  means name lookup inside Scenario3 finds its versions while Scenarios 1,
//  2 and 4 go on using the shared ones, with no renaming anywhere.
// ============================================================================
namespace Scenario3 {

// ---- Scenario 3's own copies of the shared helpers --------------------

const float SHARED_PI = 3.14159265f;

// ============================================================================
//  SHARED HELPERS
// ============================================================================

// ---- Soft ellipse: concentric rings fading outward ------------------------
//  Used for lamp pools and anything else that needs a glow with no hard edge.
void DrawSoftEllipse(float cx, float cy, float rx, float ry,
                     unsigned char r, unsigned char g, unsigned char b,
                     unsigned char alpha, int layers) {
    if (layers < 1) layers = 1;
    for (int L = layers; L >= 1; L--) {
        float f = (float)L / layers;                 // 1 = outermost
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

// ---- Contact shadow -------------------------------------------------------
//  `lean` is how far the shadow is thrown sideways; the caller works that out
//  from the sun, so this only has to draw a sheared, squashed blob.
void DrawGroundShadow(float x, float y, float rx, float lean, unsigned char alpha) {
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

// ---- Depth scaling --------------------------------------------------------
//  One rule for the whole ground plane: the further up the frame something
//  stands, the further away it is, so the smaller it is drawn.
float DepthScale(float y) {
    const float horizonY = -6.0f, nearY = -20.0f;
    float t = (y - horizonY) / (nearY - horizonY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 0.68f + 0.42f * t;
}

//  Same idea, but remapped into an explicit band -- used by the footpath so
//  the walkers scale only across the width of the path itself.
float DepthScaleRange(float y, float farY, float nearY, float minS, float maxS) {
    float t = (y - farY) / (nearY - farY);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return minS + (maxS - minS) * t;
}

//  Scales a sprite about its own base point, so feet stay planted.
void BeginDepthSprite(float x, float y, float scale) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-x, -y, 0.0f);
}
void EndDepthSprite() { glPopMatrix(); }

// ---- Reflections ----------------------------------------------------------
//  Mirror about the waterline, squashed vertically and nudged sideways by the
//  current swell, so the caller can draw the same object twice.
void BeginReflection(float waterlineY, float squash, float wobble) {
    glPushMatrix();
    glTranslatef(wobble, waterlineY, 0.0f);
    glScalef(1.0f, -squash, 1.0f);
    glTranslatef(0.0f, -waterlineY, 0.0f);
}
void EndReflection() { glPopMatrix(); }

//  Washes the mirrored copy back toward the water colour and chops it into
//  bands, which is what stops a reflection reading as a second solid object.
void WashReflection(float cx, float halfW, float waterlineY, float depth,
                    unsigned char r, unsigned char g, unsigned char b,
                    unsigned char alpha, float scroll) {
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

    // Broken bands riding the swell: the reflection is cut, not just faded.
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

// ---- HUD ------------------------------------------------------------------
void DrawBitmapText(float x, float y, const char* s, void* font) {
    glRasterPos2f(x, y);
    for (const char* c = s; *c; ++c) glutBitmapCharacter(font, *c);
}

// ---- Centred bitmap text ---------------------------------------------------
//  The sign boards were positioned by hand -- "x - 1.40f" for LEMONADE, a
//  different guess for TICKETS -- so no label actually sat in the middle of
//  its board, and any change to the wording threw it further off.
//
//  GLUT bitmap fonts are measured in PIXELS, while everything here is in world
//  units, so the width has to be converted through the ortho scale before it
//  can be used as an offset. Do that and the text centres itself, whatever it
//  says and whatever font it is in.
int BitmapTextWidthPx(const char* s, void* font) {
    int w = 0;
    for (const char* c = s; *c; ++c) w += glutBitmapWidth(font, *c);
    return w;
}

void DrawBitmapTextCentered(float cx, float y, const char* s, void* font) {
    float worldW = BitmapTextWidthPx(s, font)
                 * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
    DrawBitmapText(cx - worldW * 0.5f, y, s, font);
}

void DrawSceneHUD(const char* title, const char* const* lines) {
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

    // Backing panel, so white text stays readable against a bright sky.
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


// ============================================================================
//  STATE
// ============================================================================
constexpr float PI3 = 3.1416f;

int   dayPhase3      = 1;     // 0 = morning, 1 = midday, 2 = golden hour
bool  isAnimating3   = true;
float windPhase3      = 0.0f;
float windIntensity3  = 1.0f; // toggled low/high with 'w'
float pedWalkTimer3   = 0.0f;

// ---- Clouds ------------------------------------------------------------
struct Cloud3 { float x, y, scale, speed; };
constexpr int NUM_CLOUDS3 = 4;
// Roughly double the old scales. At the previous size they read as small
// puffs lost in a very large sky; a park sky wants a few big, slow masses.
// The biggest are also set a little lower, because a cloud that size sitting
// right at the top of the frame gets cropped and stops reading as a whole.
Cloud3 clouds3[NUM_CLOUDS3] = {
    { -50.0f, 28.0f, 2.10f, 0.03f  },
    { -10.0f, 33.0f, 2.70f, 0.02f  },
    {  20.0f, 26.0f, 1.70f, 0.035f },
    {  45.0f, 31.0f, 2.30f, 0.025f }
};

// ---- Birds ---------------------------------------------------------------
struct Bird3 { float cx, cy, radius, angle, speed; };
constexpr int NUM_BIRDS3 = 4;
Bird3 birds3[NUM_BIRDS3] = {
    { -20.0f, 25.0f, 10.0f, 0.0f, 0.02f  },
    { -20.0f, 25.0f, 10.0f, 1.5f, 0.018f },
    {  25.0f, 22.0f,  8.0f, 0.7f, 0.022f },
    {  25.0f, 22.0f,  8.0f, 3.0f, 0.02f  }
};

// ---- River ripple scroll --------------------------------------------------
float rippleScroll3 = 0.0f;

// ---- Kayak -- replaces the old rowboat -----------------------------------
//  The rowboat was a brown trapezoid with one stick for an oar. A kayak is a
//  far more distinctive shape and gives the river a proper piece of motion,
//  so it takes over the same route and speed.
float kayakX3      = -30.0f;
float kayakSpeed3  = 0.055f;
float kayakStroke3 = 0.0f;   // paddle cycle: one full turn = one left+right stroke

// Shared clock for anything that just needs to rise and fall with the water
// (the sailboat's bob, the moored kayak at the jetty). Named for the oars it
// used to swing, which no longer exist.
float waterClock3  = 0.0f;

// ---- Ducks ---------------------------------------------------------------
struct Duck3 { float x, y, speed, phase; bool flapping; float flapTimer; };
constexpr int NUM_DUCKS3 = 4;
Duck3 ducks3[NUM_DUCKS3] = {
    { -10.0f, -25.0f, 0.03f,  0.0f, false, 0.0f },
    {  -4.0f, -26.0f, 0.025f, 1.0f, false, 0.0f },
    {   3.0f, -25.5f, 0.028f, 2.0f, false, 0.0f },
    {  10.0f, -26.5f, 0.022f, 3.0f, false, 0.0f }
};

// ---- Trees -----------------------------------------------------------
struct Tree3 { float x, y, scale, swayPhase; };
constexpr int NUM_TREES3 = 6;
// Scattered through the depth of the back lawn instead of all six standing
// on the horizon line, and scaled to match where each one stands.
// Two hero cherries anchor the left and right thirds of the frame at close
// to the largest size the composition will take -- any bigger and the
// canopies meet over the middle and the park behind them stops reading.
// The other four are deliberately smaller and set further back.
Tree3 trees3[NUM_TREES3] = {
    { -44.0f, -8.8f, 1.95f, 0.0f },   // hero, left
    {  28.0f, -6.5f, 1.02f, 1.0f },
    {  47.0f, -9.4f, 1.78f, 2.0f },   // hero, right
    {  57.0f, -6.9f, 0.92f, 3.0f },
    { -27.0f, -7.4f, 1.16f, 4.0f },
    {   8.0f, -6.3f, 0.88f, 5.0f }
};

// ---- Ground bands ---------------------------------------------------------
//  The park used to be one flat lawn with the footpath painted across it and
//  the pedestrians drawn last, so walkers were composited on top of the
//  gazebo posts, the swing frame and the slide -- they appeared to stride
//  straight through solid objects. The lawn is now three explicit depth
//  bands, and the rule is simple:
//
//      FAR  (y -11 .. -6)      drawn BEFORE the pedestrians
//      PATH (y -13.2 .. -10.8) the pedestrians themselves
//      NEAR (y -20 .. -13.2)   drawn AFTER the pedestrians
//
//  Every near-lawn prop below has its base under the path and its top above
//  it, so it reads as standing in front of the walkers rather than around
//  them.
// Highest point the green hill range reaches, used by the balloon's depth
// test. DrawHills3 peaks at -6 + 3.5 + 2.5 + 1.0.
constexpr float hillCrestY3    =  1.0f;

constexpr float PATH_TOP_Y3    = -10.8f;   // far edge of the footpath
constexpr float PATH_BOTTOM_Y3 = -13.2f;   // near edge of the footpath
constexpr float LAWN_BOTTOM_Y3 = -20.0f;   // waterline

constexpr float GAZEBO_Y3      = -19.0f;
constexpr float SWING_BASE_Y3  = -18.5f;
constexpr float SWING_TOP_Y3   = -13.4f;
constexpr float SLIDE_BASE_Y3  = -18.5f;
constexpr float SLIDE_TOP_Y3   = -13.4f;
constexpr float SEESAW_Y3      = -17.5f;
constexpr float PICNIC_Y3      = -18.5f;
constexpr float FOUNTAIN_Y3    = -14.8f;
constexpr float BENCH_Y3       = -15.4f;

// ---- Benches ---------------------------------------------------------
constexpr int NUM_BENCHES3 = 3;
float benchX3[NUM_BENCHES3] = { -38.0f, 25.0f, 42.0f };

// ---- Swing / see-saw ---------------------------------------------------
float swingAngle3  = 0.0f;
float seesawAngle3 = 0.0f;

// ---- Kite -- the unique detail for this scenario --------------------------
float kiteBaseX3     = 18.0f;
float kiteBaseY3     = 20.0f;
float kiteBobPhase3  = 0.0f;

// ---- Pedestrians -----------------------------------------------------
struct Ped3 {
    float x, y, speed, phase;
    int dir;
    int kind; // 0 = stroller, 1 = jogger, 2 = dog walker,
              // 3 = cyclist,  4 = child running ahead of a parent
    unsigned char shirtR, shirtG, shirtB;
};
constexpr int NUM_PEDS3 = 9;
// Walkers are spread across the depth of the path rather than standing on a
// single line, and DrawPedestrians3() sorts them so the nearest is drawn last.
Ped3 peds3[NUM_PEDS3] = {
    { -40.0f, -11.3f, 0.05f, 0.0f,  1, 0, 200,  90,  90 },
    { -15.0f, -12.8f, 0.11f, 1.0f, -1, 1,  90, 140, 220 },
    {   8.0f, -11.7f, 0.06f, 2.0f,  1, 2, 210, 180,  60 },
    {  25.0f, -12.4f, 0.05f, 3.0f, -1, 0, 150,  90, 190 },
    {  42.0f, -11.2f, 0.10f, 4.0f,  1, 1,  90, 200, 130 },
    { -25.0f, -12.9f, 0.045f,5.0f, -1, 0, 100, 160, 170 },
    {  35.0f, -12.1f, 0.095f,6.0f,  1, 1, 210, 130,  80 },
    {   0.0f, -11.6f, 0.205f,2.5f, -1, 3,  70, 180, 190 },   // cyclist, quickest
    { -52.0f, -12.6f, 0.075f,4.5f,  1, 4, 240, 150,  70 }    // child + parent
};

// ---- Hot air balloon -- the signature feature for this scenario -----------
float balloonX3        = -95.0f;
float balloonY3        = 30.0f;
float balloonScale3    = 0.5f;
float balloonBobPhase3 = 0.0f;
// Cruising height when it is far away, and the height it settles to as it
// comes past the camera. The descent between the two is what carries it
// through the hills and the treeline (see UpdateBalloon3).
float balloonCruiseY3   = 30.0f;
const float balloonApproachY3 = -1.5f;

// ---- Fountain ---------------------------------------------------------
float fountainPhase3 = 0.0f;

// ---- Sailboat, alongside the rowboat ---------------------------------------
// ---- Second kayak (replaces the old sailboat on this route) ---------------
float kayak2X3      = 45.0f;
float kayak2Stroke3 = 2.1f;   // offset from the first, so they are out of step

// ---- Squirrel -----------------------------------------------------------
float squirrelPhase3 = 0.0f;

// ---- Shared clock for the cafe, the bandstand and the new games ----------
float playPhase3 = 0.0f;

// ---- Butterflies near the flower beds --------------------------------
struct Butterfly3 { float baseX, baseY, phase; };
constexpr int NUM_BUTTERFLIES3 = 3;
Butterfly3 butterflies3[NUM_BUTTERFLIES3] = {
    { -45.0f, -13.0f, 0.0f },
    { -43.0f, -13.5f, 2.0f },
    {  35.0f, -13.0f, 4.0f }
};

// ---- Goose family, waddling on the grass -----------------------------
float gooseX3    = -5.0f;
float gooseSpeed3 = 0.02f;
int   gooseDir3   = 1;

// ---- Duck family with trailing ducklings -----------------------------
float duckFamilyX3     = -20.0f;
float duckFamilySpeed3 = 0.018f;

// ---- Frisbee / fetch scene ---------------------------------------------
float frisbeeT3 = 0.0f;
constexpr float frisbeeBaseX3 = -8.0f;

// ---- Fish jumping in the river ---------------------------------------
struct FishJump3 { float x, phase; bool active; };
constexpr int MAX_FISH3 = 2;
FishJump3 fishJumps3[MAX_FISH3];
int fishCooldown3 = 150;

// ---- Autumn toggle -- toggled with 'A' ------------------------------------
//  The trees are cherries now, so 'A' recolours the blossom and the petals
//  falling from it rather than swapping one set of leaf art for another.
bool autumnMode3 = false;

// ---- Falling blossom petals ----------------------------------------------
//  Every petal belongs to a tree. It spawns somewhere inside that tree's
//  canopy and goes back there the moment it reaches the grass underneath,
//  so the fall stays a pool of colour beneath the branches instead of
//  drifting across the whole frame like snow.
//
//  The exception is a small foreground group (tree = -1) that crosses close
//  to the camera, larger and slightly translucent. That is what sells the
//  depth: petals passing in front of the river read as being between you
//  and the park.
constexpr int MAX_PETALS3 = 170;
struct Petal3 {
    float x, y, fall, phase, swing, rot, spin, size, jit;
    int   tree;          // index into trees3, or -1 for a foreground drifter
    float landY;         // the water line this one settles on (drifters only)
    bool  afloat;        // reached the river and is now riding the current
};
Petal3 petals3[MAX_PETALS3];

// ---- Sliding child on the playground slide -------------------------------
float slideT3 = 0.0f;

// ---- Fireflies (golden hour only) ----------------------------------------
constexpr int NUM_FIREFLIES3 = 16;
struct Firefly3 { float x, y, phase, speed; };
Firefly3 fireflies3[NUM_FIREFLIES3];
float fireflyClock3 = 0.0f;

// ============================================================================
//  SMALL HELPERS
// ============================================================================
// ============================================================================
//  DAY-PHASE LIGHTING
// ----------------------------------------------------------------------------
//  Previously `dayPhase3` only recoloured the sky and sun, so the park itself
//  looked identical at morning, midday and golden hour. This gives the whole
//  scene a per-phase colour multiplier -- the same idea Scenario 1 uses via
//  getLightFactor() -- so grass, water, trees, props and people all shift
//  with the light.
//
//  Every drawing call in this namespace goes through TintCol3/TintCol4
//  instead of glColor3ub/glColor4ub. `tintOn3` lets the sky and sun opt out,
//  since they define the light rather than receive it.
// ============================================================================
bool tintOn3 = true;

struct Tint3 { float r, g, b; };

Tint3 GetTint3() {
    if (dayPhase3 == 0) return { 0.80f, 0.88f, 1.08f };  // morning: cool blue
    if (dayPhase3 == 1) return { 1.00f, 1.00f, 1.00f };  // midday: neutral
    if (dayPhase3 == 2) return { 1.16f, 0.90f, 0.68f };  // golden hour: warm
    return                     { 0.54f, 0.58f, 0.86f };  // dusk: cold and dim
}

// ---- One sun, one arc -----------------------------------------------------
//  The sun used to jump between two hard-coded positions, which meant the
//  specular column on the river, the direction of every shadow and the sky
//  gradient all had to be special-cased per phase. Now a single angle drives
//  the position, and everything that depends on the light reads it from here.
//  a = 0 is due west (right), a = PI is due east (left).
constexpr float SUN_ARC_RX3 = 44.0f;
constexpr float SUN_ARC_RY3 = 40.0f;
constexpr float SUN_ARC_CY3 = -4.0f;

inline float SunAngle3() {
    if (dayPhase3 == 0) return 2.42f;    // low in the east  -- morning
    if (dayPhase3 == 1) return 1.57f;    // overhead         -- midday
    if (dayPhase3 == 2) return 0.42f;    // low in the west  -- golden hour
    return                     0.08f;    // touching the horizon -- dusk
}
inline float SunX3() { return SUN_ARC_RX3 * cosf(SunAngle3()); }
inline float SunY3() { return SUN_ARC_CY3 + SUN_ARC_RY3 * sinf(SunAngle3()); }

// 0 when the sun is overhead, 1 when it is on the horizon. Drives shadow
// length, ray strength and how warm the light reads.
inline float SunLowness3() {
    float h = (SunY3() - (-6.0f)) / (SUN_ARC_CY3 + SUN_ARC_RY3 - (-6.0f));
    if (h < 0.0f) h = 0.0f;
    if (h > 1.0f) h = 1.0f;
    return 1.0f - h;
}

inline bool IsDusk3()   { return dayPhase3 == 3; }
inline bool LampsOn3()  { return dayPhase3 == 3; }

inline unsigned char ClampByte3(float v) {
    if (v < 0.0f)   return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}

inline void TintCol4(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!tintOn3) { glColor4ub(r, g, b, a); return; }
    Tint3 t = GetTint3();
    glColor4ub(ClampByte3(r * t.r), ClampByte3(g * t.g), ClampByte3(b * t.b), a);
}

inline void TintCol3(unsigned char r, unsigned char g, unsigned char b) {
    TintCol4(r, g, b, 255);
}

void FilledCircle3(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    TintCol4(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 22;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI3;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}

// Contact shadow for anything standing on the park lawn. Direction and
// softness follow the day phase: a long warm-side shadow at golden hour, a
// short one at midday, thrown the other way in the morning.
void DrawFigureShadow3(float x, float y, float rx) {
    // Direction and length come straight off the sun arc: the shadow always
    // falls away from the sun, and stretches as the sun drops.
    float low  = SunLowness3();
    float lean = -(SunX3() / SUN_ARC_RX3) * (0.35f + 3.2f * low) * rx;
    unsigned char a = (unsigned char)(78 - 34 * low);
    if (IsDusk3()) a = 26;                    // almost no sun left to cast one
    DrawGroundShadow(x, y, rx * (1.0f + 0.5f * low), lean, a);
}

struct SkyColor3 { float r, g, b; };
SkyColor3 LerpC3(SkyColor3 a, SkyColor3 b, float t) {
    SkyColor3 o;
    o.r = a.r + (b.r - a.r) * t;
    o.g = a.g + (b.g - a.g) * t;
    o.b = a.b + (b.b - a.b) * t;
    return o;
}

// ============================================================================
//  SKY / SUN / CLOUDS / BIRDS / HILLS
// ============================================================================
void DrawSky3() {
    SkyColor3 mornTop = {140, 190, 230}, mornBot = {255, 235, 210};
    SkyColor3 middTop = { 70, 160, 235}, middBot = {200, 230, 250};
    SkyColor3 goldTop = {255, 170,  90}, goldBot = {255, 220, 150};
    SkyColor3 duskTop = { 26,  32,  74}, duskBot = {226, 116,  86};

    SkyColor3 top, bot;
    if      (dayPhase3 == 0) { top = mornTop; bot = mornBot; }
    else if (dayPhase3 == 1) { top = middTop; bot = middBot; }
    else if (dayPhase3 == 2) { top = goldTop; bot = goldBot; }
    else                     { top = duskTop; bot = duskBot; }

    // The sky already *is* the phase colour, so it must not be tinted again.
    tintOn3 = false;
    glBegin(GL_QUADS);
        TintCol3((unsigned char)top.r, (unsigned char)top.g, (unsigned char)top.b);
        glVertex2f(-60, 40); glVertex2f(60, 40);
        TintCol3((unsigned char)bot.r, (unsigned char)bot.g, (unsigned char)bot.b);
        glVertex2f(60, -6);  glVertex2f(-60, -6);
    glEnd();
    tintOn3 = true;
}

void DrawSun3() {
    float sx = SunX3(), sy = SunY3();
    float low = SunLowness3();

    // The sun is the light source, so it is drawn untinted.
    tintOn3 = false;

    // Stars, and a thin moon opposite the sun, once the light has gone.
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
        FilledCircle3(mx + 0.9f, my + 0.4f, 1.9f, 26, 32, 74, 255);   // crescent bite
    }

    // The lower the sun, the longer and stronger the rays fanning off it.
    if (low > 0.35f) {
        unsigned char ra = (unsigned char)(34 * low);
        unsigned char rr = 255, rg = IsDusk3() ? 140 : 190, rb = IsDusk3() ? 90 : 110;
        float base = atan2f(SUN_ARC_CY3 - sy, 0.0f - sx);     // aim across the sky
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

    // Disc colour warms and reddens as it drops toward the horizon.
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

// Four circles was enough when a cloud was two units across. At this size the
// gaps between them show, so the form is built from nine lobes instead: a
// flatter base, a piled-up crown, and a shaded underside. The shading is what
// stops a big white mass reading as a flat sticker.
void DrawCloudShape3(float x, float y, float scale) {
    // Shaded underside first, so the lit lobes sit on top of it.
    FilledCircle3(x - 2.2f*scale, y - 0.30f*scale, 1.55f*scale, 214, 222, 236, 205);
    FilledCircle3(x + 0.2f*scale, y - 0.42f*scale, 1.95f*scale, 214, 222, 236, 205);
    FilledCircle3(x + 2.4f*scale, y - 0.26f*scale, 1.45f*scale, 214, 222, 236, 205);

    // Body: a long, slightly uneven base
    FilledCircle3(x - 2.6f*scale, y + 0.10f*scale, 1.50f*scale, 252, 253, 255, 225);
    FilledCircle3(x - 0.9f*scale, y + 0.15f*scale, 1.90f*scale, 252, 253, 255, 225);
    FilledCircle3(x + 0.9f*scale, y + 0.05f*scale, 1.80f*scale, 252, 253, 255, 225);
    FilledCircle3(x + 2.7f*scale, y + 0.14f*scale, 1.35f*scale, 252, 253, 255, 225);

    // Crown, piled toward one side so it is not symmetrical
    FilledCircle3(x - 0.4f*scale, y + 1.35f*scale, 1.55f*scale, 255, 255, 255, 232);
    FilledCircle3(x + 1.3f*scale, y + 1.15f*scale, 1.25f*scale, 255, 255, 255, 232);
    FilledCircle3(x + 0.4f*scale, y + 2.25f*scale, 0.95f*scale, 255, 255, 255, 236);
}

void DrawClouds3() {
    for (int i = 0; i < NUM_CLOUDS3; i++) DrawCloudShape3(clouds3[i].x, clouds3[i].y, clouds3[i].scale);
}

void UpdateClouds3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateClouds3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        for (int i = 0; i < NUM_CLOUDS3; i++) {
            // Wind drives the clouds too, so pressing 'W' visibly changes
            // the whole sky rather than only the trees and flag.
            clouds3[i].x += clouds3[i].speed * windIntensity3;
            if (clouds3[i].x > 70.0f) clouds3[i].x = -70.0f;
        }
    }
    glutTimerFunc(30, UpdateClouds3, 0);
}

// ============================================================================
//  HIGH-ALTITUDE JET
// ----------------------------------------------------------------------------
//  Deliberately tiny. At cruising altitude an airliner is a speck -- what you
//  actually see from the ground is the contrail, and the aircraft is just the
//  bright point at the head of it. Drawing the jet any bigger would put it at
//  a few thousand feet, which would make it enormous and loud and completely
//  change the mood of a quiet park.
//
//  So: the plane is about a unit long, and all the work goes into the trail.
//  Two exhaust ribbons leave the engines, widen as they age, drift apart,
//  break into billows and fade out, which is exactly what a real contrail does
//  over a couple of minutes.
constexpr int   JET_TRAIL_PTS3 = 46;
constexpr float JET_Y3         = 35.2f;   // well above the clouds
constexpr float JET_LEN3       = 52.0f;   // how far back the trail survives

float jetX3      = -90.0f;
float jetCooldown3 = 0.0f;    // pause between passes, so it is an event

void DrawJet3() {
    if (jetX3 < -84.0f || jetX3 > 84.0f) return;

    // The sky is the light source here, so the jet and its trail do not take
    // the ground tint.
    tintOn3 = false;

    const float x = jetX3, y = JET_Y3;

    // ---- Contrail ---------------------------------------------------------
    // Age runs 0 at the engines to 1 at the far end of the trail. Width grows
    // with age, alpha falls away, and the whole ribbon sags very slightly as
    // it is left behind.
    for (int lane = 0; lane < 2; lane++) {
        float side = (lane == 0) ? 1.0f : -1.0f;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i < JET_TRAIL_PTS3; i++) {
            float age = (float)i / (JET_TRAIL_PTS3 - 1);
            float tx  = x - 0.9f - age * JET_LEN3;
            if (tx < -62.0f) break;

            // Ribbons start close together at the engines and drift apart.
            float sep = (0.26f + age * 1.05f) * side;
            // Billowing: the trail is not a smooth tube, it is a row of puffs.
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

    // A fresh, very bright stub right behind the engines: the part of the
    // trail that has only just condensed.
    glColor4ub(255, 255, 255, 235);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.8f, y + 0.16f); glVertex2f(x - 2.6f, y + 0.30f);
        glVertex2f(x - 2.6f, y - 0.34f); glVertex2f(x - 0.8f, y - 0.20f);
    glEnd();

    // ---- The aircraft -----------------------------------------------------
    // About one world unit nose to tail, which at this distance is right.
    glColor4ub(246, 248, 252, 255);
    glBegin(GL_TRIANGLES);                       // fuselage, nose to the right
        glVertex2f(x + 0.62f, y);
        glVertex2f(x - 0.40f, y + 0.13f);
        glVertex2f(x - 0.40f, y - 0.11f);
    glEnd();
    glBegin(GL_TRIANGLES);                       // swept wing
        glVertex2f(x + 0.10f, y + 0.02f);
        glVertex2f(x - 0.34f, y + 0.46f);
        glVertex2f(x - 0.20f, y - 0.02f);
    glEnd();
    glBegin(GL_TRIANGLES);                       // and the far one, foreshortened
        glVertex2f(x + 0.10f, y - 0.02f);
        glVertex2f(x - 0.30f, y - 0.30f);
        glVertex2f(x - 0.20f, y + 0.02f);
    glEnd();
    glBegin(GL_TRIANGLES);                       // tail fin
        glVertex2f(x - 0.34f, y + 0.02f);
        glVertex2f(x - 0.52f, y + 0.26f);
        glVertex2f(x - 0.44f, y + 0.01f);
    glEnd();

    // Sun glint off the fuselage, brightest when the sun is high
    float glint = 1.0f - SunLowness3();
    glColor4ub(255, 255, 245, (unsigned char)(220 * glint));
    FilledCircle3(x + 0.34f, y + 0.04f, 0.10f, 255, 255, 245,
                  (unsigned char)(220 * glint));

    tintOn3 = true;
}

void UpdateJet3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateJet3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        if (jetCooldown3 > 0.0f) {
            jetCooldown3 -= 1.0f;
            if (jetCooldown3 <= 0.0f) jetX3 = -90.0f;   // start the next pass
        } else {
            jetX3 += 0.16f;
            // Once the tail of the trail has cleared the frame, wait a while
            // before the next one. An aircraft every few seconds would be an
            // airport; every minute or so is a sky.
            if (jetX3 - JET_LEN3 > 70.0f) {
                jetX3 = 200.0f;                          // parked off-screen
                jetCooldown3 = 900.0f + (rand() % 900);  // roughly 30-60 s
            }
        }
    }
    glutTimerFunc(30, UpdateJet3, 0);
}

void DrawBirds3() {
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

void UpdateBirds3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateBirds3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        // Birds work harder into a gust, so they circle faster.
        for (int i = 0; i < NUM_BIRDS3; i++)
            birds3[i].angle += birds3[i].speed * (0.7f + 0.3f * windIntensity3);
    }
    glutTimerFunc(30, UpdateBirds3, 0);
}

// ============================================================================
//  HOT AIR BALLOON -- signature feature: drifts slowly across the sky,
//  growing as it "approaches" so it crosses from behind the hills to in
//  front of the foreground trees, the same cross-depth technique used by
//  the helicopters in Scenarios 1 and 2.
// ============================================================================
void DrawBalloon3() {
    float bob = sinf(balloonBobPhase3) * 0.5f;
    glPushMatrix();
    glTranslatef(balloonX3, balloonY3 + bob, 0.0f);
    glScalef(balloonScale3, balloonScale3, 1.0f);

    // ---- Envelope -------------------------------------------------------
    // A real balloon is a teardrop: round on top, tapering to the throat.
    // This shapes each gore by scaling the radius with height rather than
    // drawing a plain circle of triangles.
    unsigned char cols[4][3] = { {230, 80, 80}, {250, 210, 80}, {80, 160, 230}, {250, 250, 250} };
    const int segs = 20;
    const float cy = 1.2f, rx = 3.0f, ry = 3.4f;

    // Envelope outline as a function of angle: full width at the top,
    // pinched in toward the burner throat at the bottom.
    auto envX = [&](float a) -> float {
        float taper = 0.40f + 0.60f * (0.5f + 0.5f * cosf(a));  // 1 at top, 0.4 at base
        return rx * sinf(a) * taper;
    };
    auto envY = [&](float a) -> float { return cy + ry * cosf(a); };

    for (int i = 0; i < segs; i++) {
        float a0 = (float)i     / segs * PI3;   // 0 = top, PI = throat
        float a1 = (float)(i+1) / segs * PI3;
        int c = i % 4;
        TintCol3(cols[c][0], cols[c][1], cols[c][2]);
        // Left and right halves of this horizontal band
        glBegin(GL_QUADS);
            glVertex2f(-envX(a0), envY(a0)); glVertex2f( envX(a0), envY(a0));
            glVertex2f( envX(a1), envY(a1)); glVertex2f(-envX(a1), envY(a1));
        glEnd();
    }

    // Vertical panel seams, curving with the envelope
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

    // ---- Burner: pulses, and throws light up into the envelope ----------
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

    // ---- Bridle ropes: four lines from the throat to the basket corners --
    TintCol3(90, 70, 50);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(-1.05f, -2.35f); glVertex2f(-0.62f, -3.25f);
        glVertex2f(-0.45f, -2.50f); glVertex2f(-0.50f, -3.25f);
        glVertex2f( 0.45f, -2.50f); glVertex2f( 0.50f, -3.25f);
        glVertex2f( 1.05f, -2.35f); glVertex2f( 0.62f, -3.25f);
    glEnd();

    // ---- Wicker basket ---------------------------------------------------
    TintCol3(150, 110, 70);
    glBegin(GL_QUADS);
        glVertex2f(-0.8f, -4.0f); glVertex2f(0.8f, -4.0f);
        glVertex2f(0.65f, -3.25f); glVertex2f(-0.65f, -3.25f);
    glEnd();
    // Weave: crosshatch so it reads as wicker rather than a flat box
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
    // Rim
    TintCol3(185, 140, 90);
    glLineWidth(2.0f);
    glBegin(GL_LINES); glVertex2f(-0.68f, -3.25f); glVertex2f(0.68f, -3.25f); glEnd();

    // Two passengers looking out over the rim
    FilledCircle3(-0.28f, -3.05f, 0.19f, 235, 195, 155, 255);
    FilledCircle3( 0.26f, -3.08f, 0.17f, 228, 186, 146, 255);

    glLineWidth(1.0f);
    glPopMatrix();
}

void UpdateBalloon3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateBalloon3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        // A balloon has no engine: the wind IS its speed.
        balloonX3 += 0.045f + 0.045f * windIntensity3;
        balloonBobPhase3 += 0.02f + 0.006f * windIntensity3;
        if (balloonScale3 < 1.30f) balloonScale3 += 0.0008f;

        // The balloon used to fly a dead-flat line at y = 26..36 while
        // Draw() claimed it passed "behind the hills" and then "in front of
        // the trees". It could do neither: the hills top out at y = +1 and
        // the tallest tree crown at y = 0, twenty-five units below it, so
        // both branches drew the identical picture.
        //
        // Now it descends on approach. Height is driven by the same scale
        // that represents distance, so as it grows it drops on a smooth
        // curve from cruising altitude down to just above the ridge line --
        // sinking behind the hills, re-emerging, and finally drifting past
        // in front of the whole park. That makes it the strongest depth cue
        // in the scene instead of a sticker on the sky.
        float approach = (balloonScale3 - 0.42f) / (1.30f - 0.42f);   // 0..1
        if (approach < 0.0f) approach = 0.0f;
        if (approach > 1.0f) approach = 1.0f;
        float eased = approach * approach;                             // slow, then steep
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

// ---- Distant hill range, drawn behind the main hills for parallax depth ---
void DrawDistantHills3() {
    // Pale and desaturated so it reads as far away; the main green hills in
    // front of it then feel genuinely closer.
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

void DrawHills3() {
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

// ---- River -----------------------------------------------------------------
// Surface height at a given x, so boats, wakes and the bank all agree on
// where the waterline actually is.
inline float RiverSurfaceY3(float x) {
    // Chop grows with the wind: a gusty day gives visibly bigger waves, and
    // everything that reads the waterline -- bank, foam, boats, fish, the
    // reflections -- follows automatically.
    float chop = 0.55f + 0.45f * windIntensity3;
    return LAWN_BOTTOM_Y3
         + 0.45f * chop * sinf(0.18f * x + rippleScroll3 * 0.12f)
         + 0.20f * chop * sinf(0.40f * x - rippleScroll3 * 0.09f);
}

void DrawRiver3() {
    // Water reflects the sky, so its base colour is chosen per phase rather
    // than just being a tinted blue. Golden hour in particular needs warm
    // water -- a cold teal river under an orange sky looks wrong.
    SkyColor3 deep, surf, crestLo, crestHi;
    if (dayPhase3 == 0) {          // morning: cool, slightly misty
        deep = {26, 62, 96};   surf = {74, 124, 168};
        crestLo = {96, 158, 200}; crestHi = {200, 228, 245};
    } else if (dayPhase3 == 1) {   // midday: clean blue
        deep = {22, 62, 104};  surf = {58, 122, 178};
        crestLo = {90, 165, 215}; crestHi = {200, 235, 255};
    } else if (dayPhase3 == 2) {   // golden hour: warm orange reflection
        deep = {44, 54, 78};   surf = {132, 108, 104};
        crestLo = {190, 140, 100}; crestHi = {248, 214, 170};
    } else {                       // dusk: near-black water, one hot streak
        deep = {10, 14, 30};   surf = {38, 44, 76};
        crestLo = {70, 74, 118};  crestHi = {186, 122, 104};
    }

    // The phase colour is already baked in above, so don't multiply it again.
    tintOn3 = false;

    // Vertical depth grade: darker in the deep foreground, lighter up near
    // the far bank.
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 2.5f) {
        float ys = RiverSurfaceY3(x);
        glColor3ub((unsigned char)deep.r, (unsigned char)deep.g, (unsigned char)deep.b);
        glVertex2f(x, -40.0f);
        glColor3ub((unsigned char)surf.r, (unsigned char)surf.g, (unsigned char)surf.b);
        glVertex2f(x, ys);
    }
    glEnd();

    // Bright crest line riding the surface
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

    // Specular sun path: a scattered column of glitter under the sun's x
    // position, brighter and much longer at golden hour when the sun is low.
    // Streaks are deliberately uneven in width, offset and alpha -- uniform
    // full-width bands read as a printed ladder rather than sunlight.
    // The glitter column now sits under the sun wherever the arc has put it,
    // and stretches further the lower the sun gets.
    float low     = SunLowness3();
    float sunX    = SunX3();
    float spread  = 4.5f + 5.0f * low;
    float depth   = 9.0f + 11.0f * low;
    int   streaks = 20 + (int)(18.0f * low);
    for (int i = 0; i < streaks; i++) {
        float t   = (float)i / streaks;                  // 0 at surface, 1 deep
        float by  = -20.6f - t * depth;
        // Pseudo-random but stable per-streak scatter (no rand() in a draw).
        float h1  = sinf(i * 12.9898f) * 43758.5453f;
        float h2  = sinf(i * 78.233f)  * 12345.6789f;
        float j1  = h1 - floorf(h1);                     // 0..1
        float j2  = h2 - floorf(h2);                     // 0..1
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

    // Scrolling highlight streaks at two different speeds for parallax
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

    // ---- Bank ---------------------------------------------------------
    // This strip used to be drawn inside DrawRiver3 *before* the lawn quad,
    // so the grass painted over it and only the fragments where a wave
    // trough dipped below y = -20 survived -- the sand appeared to blink as
    // the water scrolled. The river is now drawn after the lawn, so the bank
    // composites on top of the grass where it belongs.
    //
    // Three parts: dry sand above the waterline, a darker wet band that the
    // water has just left, and a bright foam lip riding the wave itself.
    glBegin(GL_QUAD_STRIP);
    for (float x = -60.0f; x <= 60.5f; x += 2.0f) {
        float ys = RiverSurfaceY3(x);
        TintCol3(216, 196, 148);
        glVertex2f(x, ys + 1.05f);
        TintCol3(178, 154, 112);
        glVertex2f(x, ys + 0.10f);
    }
    glEnd();

    // Foam lip: brightest exactly on the waterline, fading upward.
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

    // Pebbles along the tideline. Deterministic scatter -- no rand() in a
    // draw call, or they would jitter every frame.
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

// The water colour under the boats, so a reflection can be washed back
// toward the surface it sits in rather than toward a guessed blue.
void RiverSurfaceColour3(unsigned char& r, unsigned char& g, unsigned char& b) {
    if      (dayPhase3 == 0) { r =  74; g = 124; b = 168; }
    else if (dayPhase3 == 1) { r =  58; g = 122; b = 178; }
    else if (dayPhase3 == 2) { r = 132; g = 108; b = 104; }
    else                     { r =  38; g =  44; b =  76; }
}

// Reflects one water-borne object. `drawFn` is called twice: once mirrored
// under the hull, once normally on top.
void DrawWithReflection3(void (*drawFn)(), float cx, float halfW,
                         float waterlineY, float depth) {
    unsigned char wr, wg, wb;
    RiverSurfaceColour3(wr, wg, wb);
    float wobble = sinf(rippleScroll3 * 0.11f + cx * 0.05f) * 0.35f;

    BeginReflection(waterlineY, 0.58f, wobble);
    drawFn();
    EndReflection();
    WashReflection(cx + wobble, halfW, waterlineY, depth, wr, wg, wb, 120, rippleScroll3);

    drawFn();
}

// ---- Wakes: V-shaped foam trails behind anything moving on the water ------
void DrawWake3(float x, float y, float dir, float width, float length, unsigned char alpha) {
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

// ============================================================================
//  RIVER / DOCK / ROWBOAT / DUCKS
// ============================================================================
void UpdateRiver3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateRiver3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) rippleScroll3 += 0.18f + 0.12f * windIntensity3;
    glutTimerFunc(30, UpdateRiver3, 0);
}

// ---- Fish occasionally jumping out of the river ---------------------------
void DrawFishJumps3() {
    for (int i = 0; i < MAX_FISH3; i++) {
        if (!fishJumps3[i].active) continue;
        float t = fishJumps3[i].phase;
        // Apex clamped so the arc stays below the lawn: the fish used to
        // reach y = -17.5, a full 2.5 units up into the grass band, which
        // read as a fish flying over the park.
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
        // Splash rings at the surface when it's near the water line
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

void UpdateFishJumps3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFishJumps3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        fishCooldown3--;
        if (fishCooldown3 <= 0) {
            for (int i = 0; i < MAX_FISH3; i++) {
                if (!fishJumps3[i].active) {
                    fishJumps3[i].active = true;
                    // Keep clear of the dock at x = 20 (+/- 6) so a fish can
                    // never appear to leap through the planking.
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

// ---- Frisbee / fetch scene ---------------------------------------------
void DrawFrisbeeScene3() {
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

void UpdateFrisbee3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFrisbee3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) frisbeeT3 += 0.006f;
    glutTimerFunc(30, UpdateFrisbee3, 0);
}

void DrawDock3() {
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

// ============================================================================
//  KAYAK
// ----------------------------------------------------------------------------
//  Everything that makes a kayak recognisable, in one reusable body so the
//  paddling kayak on the river and the one moored at the jetty are provably
//  the same craft:
//
//    * a double-ended hull -- pointed at BOTH ends -- with real rocker, the
//      keel curving up toward bow and stern. Nothing else on this river has
//      that silhouette.
//    * low freeboard: it rides deep, unlike the tall-sided dinghy it replaces
//    * a raised cockpit coaming with a spray deck stretched over it
//    * deck lines and bungee rigging fore and aft
//    * a double-bladed paddle with the blades FEATHERED about 45 degrees, so
//      the planted blade shows its face while the airborne one is edge-on
//    * the paddler rotating from the torso, not the arms, wearing a buoyancy
//      aid -- which is how paddling actually looks
//    * water streaming off the raised blade
//
//  `paddling` is false for the moored one: it keeps the hull and rigging and
//  drops the crew, the paddle and the wake.
// ============================================================================
void DrawKayakBody3(float x, float y, float dir, float stroke,
                    unsigned char hullR, unsigned char hullG, unsigned char hullB,
                    bool paddling) {
    const float HALF_LEN = 3.6f;

    // ---- Hull outline ----------------------------------------------------
    // Two curves that meet at the tips: the sheer (deck edge) dips low at the
    // cockpit and sweeps up at both ends; the keel is deepest amidships and
    // lifts away at bow and stern. That lift IS the rocker.
    // upper(t) = 0.30 + 0.60 t^2 and lower(t) = -0.62 + 1.52 t^2 both reach
    // +0.90 at t = +/-1, so the hull closes to a point at each end.
    const int SEG = 26;

    // Submerged part first, in a colder, darker version of the hull colour,
    // so the boat sits IN the water rather than on top of it.
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

    // Topsides
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

    // Racing stripe along the topsides -- kayaks are almost always two-tone,
    // and it reads the sheer curve back to the viewer.
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

    // Sheer line, crisping the deck edge
    TintCol4(60, 50, 40, 190);
    glLineWidth(1.4f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t = -1.0f + 2.0f * (float)i / SEG;
        glVertex2f(x + t * HALF_LEN, y + 0.30f + 0.60f * t * t);
    }
    glEnd();

    // ---- Deck fittings ---------------------------------------------------
    const float cockpitX = x - dir * 0.35f;          // cockpit sits just aft
    const float deckY    = y + 0.32f;

    // Bungee rigging: cross-lacing over the fore deck, straight lines aft
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

    // Toggle handles at bow and stern
    TintCol3(210, 70, 60);
    FilledCircle3(x + HALF_LEN * 0.96f, y + 0.80f, 0.12f, 210, 70, 60, 255);
    FilledCircle3(x - HALF_LEN * 0.96f, y + 0.80f, 0.12f, 210, 70, 60, 255);

    // ---- Cockpit ---------------------------------------------------------
    // Spray deck first (the dark hole), then the raised coaming rim over it.
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

    // ---- Paddler ---------------------------------------------------------
    // The whole point of paddling technique is that power comes from torso
    // rotation, so the shoulders swing while the hips stay put.
    float swing   = sinf(stroke);
    float torsoLn = swing * 0.30f;                   // shoulder travel
    float hipX    = cockpitX;
    float hipY    = deckY + 0.18f;
    float shX     = hipX + dir * 0.10f + torsoLn * 0.45f;
    float shY     = hipY + 1.02f;

    // Buoyancy aid over the torso: a thick stroke, with the panel seam
    TintCol3(232, 96, 40);
    glLineWidth(6.5f);
    glBegin(GL_LINES); glVertex2f(hipX, hipY); glVertex2f(shX, shY); glEnd();
    TintCol4(180, 60, 24, 220);
    glLineWidth(1.4f);
    glBegin(GL_LINES);
        glVertex2f(hipX + (shX - hipX) * 0.45f, hipY + 0.46f);
        glVertex2f(shX, shY - 0.12f);
    glEnd();

    // Neck, head, cap
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
    // Cap peak, pointing the way they are going
    glBegin(GL_TRIANGLES);
        glVertex2f(headX + dir * 0.10f, headY + 0.14f);
        glVertex2f(headX + dir * 0.10f, headY + 0.02f);
        glVertex2f(headX + dir * 0.62f, headY + 0.06f);
    glEnd();

    // ---- Paddle ----------------------------------------------------------
    // One shaft pivoting in front of the chest. The blade on the way down is
    // shown face-on; the one coming up is edge-on, which is exactly what a
    // feathered paddle does.
    float pivX  = shX + dir * 0.30f;
    float pivY  = shY - 0.16f;
    float ang   = swing * 0.92f;                       // shaft tilt
    float shaft = 2.05f;
    float ax = pivX + cosf(ang) * shaft * dir, ay = pivY + sinf(ang) * shaft;
    float bx = pivX - cosf(ang) * shaft * dir, by = pivY - sinf(ang) * shaft;

    // Hands on the shaft
    TintCol3(60, 56, 52);
    glLineWidth(2.6f);
    glBegin(GL_LINES); glVertex2f(ax, ay); glVertex2f(bx, by); glEnd();
    TintCol3(232, 194, 156);
    FilledCircle3(pivX + cosf(ang) * 0.55f * dir, pivY + sinf(ang) * 0.55f, 0.13f, 232, 194, 156, 255);
    FilledCircle3(pivX - cosf(ang) * 0.55f * dir, pivY - sinf(ang) * 0.55f, 0.13f, 232, 194, 156, 255);

    // Which end is going into the water this half-stroke
    bool aIsLow = (ay < by);
    float lowX = aIsLow ? ax : bx, lowY = aIsLow ? ay : by;
    float hiX  = aIsLow ? bx : ax, hiY  = aIsLow ? by : ay;

    // Planted blade: face-on, so a broad leaf shape
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

    // Airborne blade: feathered, so almost edge-on -- a thin sliver
    TintCol3(238, 240, 245);
    glBegin(GL_QUADS);
        glVertex2f(hiX - 0.09f, hiY - 0.66f); glVertex2f(hiX + 0.09f, hiY - 0.66f);
        glVertex2f(hiX + 0.09f, hiY + 0.66f); glVertex2f(hiX - 0.09f, hiY + 0.66f);
    glEnd();

    // Water streaming off the blade that just came up
    if (hiY > y + 1.2f) {
        for (int i = 0; i < 4; i++) {
            float dt = fmodf(stroke * 0.5f + i * 0.25f, 1.0f);
            float dx = hiX + (i - 1.5f) * 0.16f;
            float dy = hiY - 0.55f - dt * (hiY - y - 0.4f);
            TintCol4(205, 232, 248, (unsigned char)(215 * (1.0f - dt)));
            FilledCircle3(dx, dy, 0.075f, 205, 232, 248, (unsigned char)(215 * (1.0f - dt)));
        }
    }

    // Catch: a burst of white where the planted blade bites the water
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

void DrawKayak3() {
    float bob  = sinf(waterClock3 * 0.5f) * 0.20f;
    float x    = kayakX3;
    float y    = -26.0f + bob;
    float dir  = 1.0f;

    // Wake first, so the hull sits on top of its own foam.
    DrawWake3(x - 3.4f, y + 0.15f, 1.0f, 1.4f, 8.0f, 150);

    // Bow wave: a small chevron pushed out ahead of the stem.
    TintCol4(235, 250, 255, 170);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + 4.4f, y + 0.55f);
        glVertex2f(x + 3.5f, y + 0.02f);
        glVertex2f(x + 4.3f, y - 0.42f);
    glEnd();
    glLineWidth(1.0f);

    // Every stroke rolls the boat a couple of degrees -- kayaks are tippy and
    // a still hull under a working paddler looks wrong.
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(sinf(kayakStroke3) * 2.6f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-x, -y, 0.0f);
    DrawKayakBody3(x, y, dir, kayakStroke3, 250, 196, 52, true);
    glPopMatrix();
}

void UpdateKayak3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateKayak3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        kayakX3 += kayakSpeed3;
        if (kayakX3 > 70.0f) kayakX3 = -70.0f;
        // Cadence: a little over one stroke per second, and the paddler works
        // harder into a headwind.
        kayakStroke3 += 0.105f + 0.02f * (windIntensity3 - 1.0f);
        waterClock3  += 0.08f;
    }
    glutTimerFunc(30, UpdateKayak3, 0);
}

// ---- Second kayak, upstream -----------------------------------------------
//  This route used to carry a sailboat with a white triangular sail. A dinghy
//  under sail on a narrow park river never quite made sense next to the
//  paddler, so it is a second kayak now -- same hull, different colour, its
//  own paddler and its own stroke phase, heading the other way.
//
//  Both kayaks come from DrawKayakBody3, so the two craft are provably the
//  same design seen from two directions rather than two separate drawings
//  that happen to look similar.
void DrawKayak2_3() {
    float bob = 0.20f * sinf(waterClock3 * 0.6f + 2.0f);
    float x   = kayak2X3;
    float y   = -25.5f + bob;
    float dir = -1.0f;                       // travelling right to left

    // Wake trails to the RIGHT, because this one is heading left.
    DrawWake3(x + 3.4f, y + 0.15f, -1.0f, 1.4f, 8.0f, 150);

    // Bow chevron, pushed out ahead of the stem on the left.
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

void UpdateKayak2_3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateKayak2_3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        // Paddling upstream against the wind, so this one is the slower of the
        // two and works harder for it.
        kayak2X3 -= 0.042f + 0.008f * (windIntensity3 - 1.0f);
        if (kayak2X3 < -70.0f) kayak2X3 = 70.0f;
        kayak2Stroke3 += 0.118f + 0.024f * (windIntensity3 - 1.0f);
    }
    glutTimerFunc(30, UpdateKayak2_3, 0);
}

void DrawDuckShape3(float x, float y, bool flap) {
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

void DrawDucks3() {
    for (int i = 0; i < NUM_DUCKS3; i++) {
        float bob = sinf(ducks3[i].phase) * 0.1f;
        DrawDuckShape3(ducks3[i].x, ducks3[i].y + bob, ducks3[i].flapping);
    }
}

void UpdateDucks3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateDucks3, 0); return; }  // idle while this scenario is off-screen
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

// ---- Goose family, waddling on the grass -----------------------------------
void DrawGooseFamily3() {
    float bob = sinf(squirrelPhase3 * 2.2f) * 0.08f; // reuse the shared idle clock
    // Parent
    FilledCircle3(gooseX3, -13.0f + bob, 0.42f, 235, 235, 230, 255);
    FilledCircle3(gooseX3 + gooseDir3*0.42f, -12.75f + bob, 0.20f, 235, 235, 230, 255);
    TintCol3(230, 140, 40);
    glBegin(GL_TRIANGLES);
        glVertex2f(gooseX3 + gooseDir3*0.60f, -12.75f+bob);
        glVertex2f(gooseX3 + gooseDir3*0.80f, -12.70f+bob);
        glVertex2f(gooseX3 + gooseDir3*0.60f, -12.85f+bob);
    glEnd();
    // Three goslings trailing behind
    for (int i = 0; i < 3; i++) {
        float gx = gooseX3 - gooseDir3 * (1.0f + i*0.6f);
        float gy = -12.7f + bob * 1.3f + 0.05f*sinf(squirrelPhase3*3.0f + i);
        FilledCircle3(gx, gy, 0.22f, 240, 225, 130, 255);
    }
}

void UpdateGooseFamily3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateGooseFamily3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        gooseX3 += gooseSpeed3 * gooseDir3;
        if (gooseX3 > 15.0f)  gooseDir3 = -1;
        if (gooseX3 < -15.0f) gooseDir3 = 1;
    }
    glutTimerFunc(35, UpdateGooseFamily3, 0);
}

// ---- Duck family, with trailing ducklings ----------------------------
void DrawDuckFamily3() {
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

void UpdateDuckFamily3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateDuckFamily3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        duckFamilyX3 += duckFamilySpeed3;
        if (duckFamilyX3 > 60.0f) duckFamilyX3 = -60.0f;
    }
    glutTimerFunc(30, UpdateDuckFamily3, 0);
}

// ============================================================================
//  GRASS / PATH / FLOWER BEDS / BENCHES / GAZEBO
// ============================================================================
void DrawGrass3() {
    // Two-tone lawn: cooler and darker toward the horizon, warmer and
    // brighter in the near band, so the ground plane recedes instead of
    // reading as one flat slab of green.
    unsigned char farR = 68,  farG = 138, farB = 84;
    unsigned char nrR  = 104, nrG  = 182, nrB  = 96;
    if (autumnMode3) { farR=134; farG=112; farB=56;  nrR=176; nrG=142; nrB=66; }

    // The bottom edge runs below the waterline so a wave trough can never
    // expose the clear colour between the lawn and the river.
    glBegin(GL_QUADS);
        TintCol3(nrR, nrG, nrB);
        glVertex2f(-60, LAWN_BOTTOM_Y3 - 1.5f); glVertex2f(60, LAWN_BOTTOM_Y3 - 1.5f);
        TintCol3(farR, farG, farB);
        glVertex2f(60, -6);   glVertex2f(-60, -6);
    glEnd();

    // Mown stripes running away from the camera, compressed toward the
    // horizon -- a cheap, very effective ground-plane cue.
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

void DrawPath3() {
    // Widened from 1.6 to 2.4 units deep so the walkers have somewhere to
    // spread out in depth instead of standing on a single line, and given a
    // gravel edge on each side rather than two hard cuts into the grass.
    TintCol3(215, 195, 160);
    glBegin(GL_QUADS);
        glVertex2f(-60, PATH_BOTTOM_Y3); glVertex2f(60, PATH_BOTTOM_Y3);
        glVertex2f(60, PATH_TOP_Y3);     glVertex2f(-60, PATH_TOP_Y3);
    glEnd();

    // Soft shoulders where the gravel meets the grass
    TintCol4(196, 178, 144, 190);
    glBegin(GL_QUADS);
        glVertex2f(-60, PATH_TOP_Y3); glVertex2f(60, PATH_TOP_Y3);
        glVertex2f(60, PATH_TOP_Y3 + 0.35f); glVertex2f(-60, PATH_TOP_Y3 + 0.35f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(-60, PATH_BOTTOM_Y3 - 0.35f); glVertex2f(60, PATH_BOTTOM_Y3 - 0.35f);
        glVertex2f(60, PATH_BOTTOM_Y3); glVertex2f(-60, PATH_BOTTOM_Y3);
    glEnd();

    // Scattered gravel speckle, deterministic so it does not crawl per frame
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

// A bed can now sit in either ground band; `sc` shrinks the far one so the
// two read as different distances rather than two copies of one object.
void DrawFlowerBed3(float x, float y, float sc) {
    TintCol3(70, 120, 60);
    glBegin(GL_QUADS);
        glVertex2f(x-3.0f*sc, y-0.5f*sc); glVertex2f(x+3.0f*sc, y-0.5f*sc);
        glVertex2f(x+3.0f*sc, y+1.5f*sc); glVertex2f(x-3.0f*sc, y+1.5f*sc);
    glEnd();
    unsigned char cols[4][3] = { {230,80,100}, {240,200,60}, {230,120,200}, {255,255,255} };
    for (int i = 0; i < 8; i++) {
        float fx = x + (-2.5f + (i % 4) * 1.5f) * sc;
        float fy = y + (0.0f + (i / 4) * 1.2f) * sc;
        // Stem, so the blooms are planted rather than floating
        TintCol3(60, 110, 55);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(fx, fy); glVertex2f(fx, fy - 0.45f*sc);
        glEnd();
        FilledCircle3(fx, fy, 0.3f*sc, cols[i%4][0], cols[i%4][1], cols[i%4][2], 255);
    }
    glLineWidth(1.0f);
}

// Far-lawn beds (behind the path) and near-lawn beds (in front of it).
void DrawFlowerBedsFar3() {
    DrawFlowerBed3( 35.0f, -9.6f,  0.78f);
    DrawFlowerBed3(-18.0f, -8.9f,  0.70f);
}
void DrawFlowerBedsNear3() {
    DrawFlowerBed3(-45.0f, -15.2f, 1.15f);
}

// ---- Gardener tending the flower bed ---------------------------------
void DrawGardener3() {
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
    // Watering can
    TintCol3(140, 140, 150);
    glBegin(GL_QUADS);
        glVertex2f(x+0.3f, y+0.35f); glVertex2f(x+0.7f, y+0.35f);
        glVertex2f(x+0.7f, y+0.6f);  glVertex2f(x+0.3f, y+0.6f);
    glEnd();
    // Falling water droplets
    float t = fmodf(squirrelPhase3 * 2.0f, 1.0f);
    TintCol4(180, 210, 240, 200);
    FilledCircle3(x+0.85f, y+0.4f - t*0.3f, 0.06f, 180, 210, 240, 200);
}

void DrawBench3(float x, float y, float sc) {
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
    // Slat lines on the backrest
    TintCol4(80, 52, 30, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.6f, y+0.72f); glVertex2f(x+1.6f, y+0.72f);
        glVertex2f(x-1.6f, y+0.92f); glVertex2f(x+1.6f, y+0.92f);
    glEnd();
    EndDepthSprite();
}

// Two benches in the near lawn at full size; one pushed back behind the path
// and drawn smaller, so the seating reads as having depth.
void DrawBenchesNear3() {
    DrawBench3(benchX3[0], BENCH_Y3,        1.08f);
    DrawBench3(benchX3[1], BENCH_Y3 + 1.4f, 1.00f);
}
void DrawBenchesFar3() {
    DrawBench3(benchX3[2], -9.7f, 0.72f);
}

void DrawGazebo3() {
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

// ---- Fountain -----------------------------------------------------------
void DrawFountain3() {
    // Kept clear of the gazebo, whose roof spans x = -16 .. -8.
    float x = -4.5f, y = FOUNTAIN_Y3;

    // Stone rim (outer ring), then the water in the basin
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

    // Centre pedestal
    TintCol3(160, 160, 170);
    glBegin(GL_QUADS);
        glVertex2f(x-0.22f, y); glVertex2f(x+0.22f, y);
        glVertex2f(x+0.16f, y+1.1f); glVertex2f(x-0.16f, y+1.1f);
    glEnd();
    FilledCircle3(x, y+1.15f, 0.28f, 175, 175, 185, 255);

    // ---- Parabolic jets --------------------------------------------------
    // Seven streams launched at different angles, each following a real
    // projectile arc (x = vx*t, y = vy*t - 0.5*g*t^2) instead of the old
    // circles that just looped up and down.
    const int   JETS = 7;
    const float g    = 26.0f;
    const float nozzleY = y + 1.25f;
    for (int j = 0; j < JETS; j++) {
        float spread = -1.0f + 2.0f * (float)j / (JETS - 1);   // -1 .. +1
        // The jets blow downwind, harder in a gust.
        float vx = spread * 3.4f + 0.55f * (windIntensity3 - 1.0f);
        float vy = 5.6f - fabsf(spread) * 1.1f;                // centre jet highest
        float wob = 1.0f + 0.05f * sinf(fountainPhase3 * 6.0f + j);
        vx *= wob; vy *= wob;

        TintCol4(215, 240, 255, 190);
        glLineWidth(1.6f);
        glBegin(GL_LINE_STRIP);
        for (int s = 0; s <= 12; s++) {
            float t  = s / 12.0f * 0.45f;
            float px = x + vx * t;
            float py = nozzleY + vy * t - 0.5f * g * t * t;
            if (py < y + 0.1f) break;      // stop at the water line
            glVertex2f(px, py);
        }
        glEnd();

        // Droplet travelling along this arc, so the water reads as moving
        float dt = fmodf(fountainPhase3 * 0.55f + j * 0.13f, 0.45f);
        float dx = x + vx * dt;
        float dy = nozzleY + vy * dt - 0.5f * g * dt * dt;
        if (dy > y + 0.1f) FilledCircle3(dx, dy, 0.12f, 235, 248, 255, 220);
    }

    // Splash rings expanding in the basin where the jets land
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

void UpdateFountain3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFountain3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) fountainPhase3 += 0.03f;
    glutTimerFunc(30, UpdateFountain3, 0);
}

// ---- Picnic scene -----------------------------------------------------
void DrawPicnic3(float x) {
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

// ============================================================================
//  PLAYGROUND: SWING + SEE-SAW
// ============================================================================
// ---- A small seated/standing child, reused by the playground equipment ----
// `lean` tilts the whole figure (radians) so a child on the swing leans with
// the arc instead of staying stubbornly vertical.
void DrawChild3(float x, float y, float lean,
                unsigned char r, unsigned char g, unsigned char b,
                bool seated, float legKick) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(lean * 57.2958f, 0.0f, 0.0f, 1.0f);

    // Legs: seated children stick their legs forward and kick.
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

    // Torso
    TintCol3(r, g, b);
    glLineWidth(3.5f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, seated ? 0.15f : 0.45f);
        glVertex2f(0.0f, seated ? 0.72f : 1.02f);
    glEnd();

    // Arms out (holding the rope / the handle)
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, seated ? 0.60f : 0.90f);
        glVertex2f(0.30f, seated ? 0.78f : 1.00f);
        glVertex2f(0.0f, seated ? 0.60f : 0.90f);
        glVertex2f(-0.28f, seated ? 0.74f : 0.98f);
    glEnd();

    // Head
    FilledCircle3(0.0f, seated ? 0.94f : 1.24f, 0.22f, 228, 190, 150, 255);

    glLineWidth(1.0f);
    glPopMatrix();
}

void DrawSwing3() {
    float x = 2.0f, topY = SWING_TOP_Y3;
    // Feet in the near lawn, crossbar below the footpath: the frame now sits
    // wholly in front of the walkers instead of straddling their lane.
    TintCol3(90, 90, 95);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-3.0f, SWING_BASE_Y3); glVertex2f(x-1.0f, topY);
        glVertex2f(x+3.0f, SWING_BASE_Y3); glVertex2f(x+1.0f, topY);
        glVertex2f(x-1.0f, topY);          glVertex2f(x+1.0f, topY);
    glEnd();

    float ropeLen = 3.6f;
    // Amplitude eases up and down instead of swinging forever at one height,
    // which reads as someone actually pumping the swing.
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

    // The swing is occupied: the child leans with the rope angle.
    DrawChild3(seatX, seatY + 0.25f, theta, 90, 150, 210, true,
               sinf(swingAngle3 * 2.0f));
}

void UpdateSwing3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateSwing3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) swingAngle3 += 0.04f;
    glutTimerFunc(20, UpdateSwing3, 0);
}

void DrawSeesaw3() {
    float x = 10.0f, y = SEESAW_Y3;
    FilledCircle3(x, y, 0.6f, 120, 90, 60, 255);
    float ang = sinf(seesawAngle3) * 0.35f;
    TintCol3(200, 150, 60);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 3.5f*cosf(ang), y + 0.6f - 3.5f*sinf(ang));
        glVertex2f(x + 3.5f*cosf(ang), y + 0.6f + 3.5f*sinf(ang));
    glEnd();

    // A child on each end -- the plank was previously tipping by itself.
    float lx = x - 3.2f*cosf(ang), ly = y + 0.6f - 3.2f*sinf(ang);
    float rx = x + 3.2f*cosf(ang), ry = y + 0.6f + 3.2f*sinf(ang);
    DrawChild3(lx, ly + 0.15f, ang, 210, 90, 110, true,  sinf(seesawAngle3*2.0f));
    DrawChild3(rx, ry + 0.15f, ang, 110, 190, 130, true, -sinf(seesawAngle3*2.0f));
    glLineWidth(1.0f);
}

void UpdateSeesaw3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateSeesaw3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) seesawAngle3 += 0.03f;
    glutTimerFunc(20, UpdateSeesaw3, 0);
}

// ---- Slide -----------------------------------------------------------
void DrawSlide3() {
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

    // A child rides the chute on a loop: 0.0-0.25 waits at the top, then
    // 0.25-1.0 slides down. Without the pause it looks like a conveyor belt.
    float loop = fmodf(slideT3, 1.0f);
    if (loop < 0.25f) {
        DrawChild3(x - 0.35f, topY + 0.3f, 0.0f, 240, 190, 70, false, 0.0f);
    } else {
        float s  = (loop - 0.25f) / 0.75f;              // 0..1 down the slide
        float cx = (x + 0.7f) + s * 2.6f;
        float cy = topY - s * (topY - baseY);
        // Leaning back slightly, matching the chute's slope.
        DrawChild3(cx, cy + 0.15f, -0.5f, 240, 190, 70, true, 0.0f);
    }
    glLineWidth(1.0f);
}

// ============================================================================
//  LAKESIDE CAFE
// ----------------------------------------------------------------------------
//  The right third of the near lawn was empty grass between the slide and
//  the bush at x = 52. A cafe fills it, gives the pedestrians on the path
//  somewhere to be walking to, and -- because of the string lights -- gives
//  the dusk phase a second source of warm light besides the park lamps.
//
//  Like the gazebo it is built entirely between the waterline and the near
//  edge of the footpath, so it never overlaps the walkers' lane.
// ============================================================================
constexpr float CAFE_X3 = 40.5f;
constexpr float CAFE_Y3 = -19.0f;

// A parasol and its table, used three times along the terrace.
void DrawCafeTable3(float x, float y, int seed) {
    // ---- Table ----------------------------------------------------------
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

    // A cup on the table, still steaming.
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

    // ---- Two customers, one either side ---------------------------------
    for (int i = 0; i < 2; i++) {
        float cx = x + (i == 0 ? -1.55f : 1.55f);
        // Chair
        TintCol3(96, 78, 62);
        glLineWidth(1.8f);
        glBegin(GL_LINES);
            glVertex2f(cx, y); glVertex2f(cx, y + 0.72f);
            glVertex2f(cx - 0.34f, y + 0.72f); glVertex2f(cx + 0.34f, y + 0.72f);
        glEnd();
        // Seated adult: the child figure, scaled up about its own base.
        unsigned char r = (i == 0) ? 210 : 90;
        unsigned char g = (i == 0) ? 120 : 150;
        unsigned char b = (i == 0) ? 140 : 190;
        BeginDepthSprite(cx, y + 0.72f, 1.32f);
        DrawChild3(cx, y + 0.72f, 0.0f, r, g, b, true,
                   0.15f * sinf(playPhase3 * 0.7f + seed));
        EndDepthSprite();
    }

    // ---- Parasol --------------------------------------------------------
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
    // Scalloped hem
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

void DrawCafe3() {
    const float x = CAFE_X3, y = CAFE_Y3;

    // ---- Timber deck ----------------------------------------------------
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

    // ---- Hut ------------------------------------------------------------
    TintCol3(238, 230, 214);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.4f, y);         glVertex2f(x + 3.4f, y);
        glVertex2f(x + 3.4f, y + 3.30f); glVertex2f(x - 3.4f, y + 3.30f);
    glEnd();
    // Plank shading
    TintCol4(198, 188, 170, 130);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float py = y + i * 0.66f;
            glVertex2f(x - 3.4f, py); glVertex2f(x + 3.4f, py);
        }
    glEnd();

    // Serving window, with the counter ledge under it
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

    // The person behind the counter, leaning in and out of the hatch
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

    // A customer at the window, waiting
    DrawChild3(x + 2.55f, y, 0.0f, 220, 160, 80, false,
               0.25f * sinf(playPhase3 * 0.5f));

    // ---- Awning over the window ------------------------------------------
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
        // Scallop along the bottom edge
        glBegin(GL_TRIANGLES);
            glVertex2f(x0, y + 2.62f); glVertex2f(x1, y + 2.62f);
            glVertex2f((x0 + x1) * 0.5f, y + 2.34f);
        glEnd();
    }

    // ---- Pitched roof ----------------------------------------------------
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

    // Sign board on the gable
    TintCol3(58, 46, 40);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f, y + 3.55f); glVertex2f(x + 1.55f, y + 3.55f);
        glVertex2f(x + 1.55f, y + 4.35f); glVertex2f(x - 1.55f, y + 4.35f);
    glEnd();
    glColor4ub(250, 228, 178, 255);
    DrawBitmapTextCentered(x, y + 3.80f, "CAFE", GLUT_BITMAP_HELVETICA_12);

    // ---- Chalkboard A-frame by the deck ----------------------------------
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

    // ---- Terrace tables --------------------------------------------------
    DrawCafeTable3(x - 7.2f, y, 0);
    DrawCafeTable3(x + 6.9f, y, 1);

    // ---- String lights ---------------------------------------------------
    //  Two swags from the roof out to a post at each end. Dark by day, and
    //  the reason this corner is worth looking at once the sun has gone.
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
                    // A bulb emits light, so it is drawn untinted, and each
                    // one breathes on its own offset.
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

// ============================================================================
//  BANDSTAND -- three children playing in the gazebo
// ----------------------------------------------------------------------------
//  The gazebo was an empty roof on posts. It is a bandstand now, which is
//  what a gazebo in a park is actually for.
// ============================================================================
void DrawBandstand3() {
    const float gx = -12.0f;
    // The bank sand is painted on top of the lawn up to about y = -18.9,
    // so anything standing at the gazebo's own base line gets buried by it.
    const float gy = GAZEBO_Y3 + 1.05f;
    float beat = sinf(playPhase3 * 3.2f);

    // ---- Violinist -------------------------------------------------------
    {
        float px = gx - 2.05f;
        DrawChild3(px, gy, 0.0f, 210, 90, 110, false, 0.06f * beat);
        // Violin tucked under the chin
        TintCol3(126, 66, 34);
        glBegin(GL_QUADS);
            glVertex2f(px + 0.10f, gy + 1.00f); glVertex2f(px + 0.70f, gy + 1.14f);
            glVertex2f(px + 0.66f, gy + 1.36f); glVertex2f(px + 0.06f, gy + 1.22f);
        glEnd();
        // Bow, sawing across the strings
        TintCol3(226, 214, 190);
        glLineWidth(1.4f);
        glBegin(GL_LINES);
            glVertex2f(px + 0.18f, gy + 1.42f + 0.14f * beat);
            glVertex2f(px + 0.78f, gy + 0.96f + 0.14f * beat);
        glEnd();
        glLineWidth(1.0f);
    }

    // ---- Guitarist -------------------------------------------------------
    {
        float px = gx + 0.10f;
        DrawChild3(px, gy, 0.0f, 110, 170, 210, false, -0.06f * beat);
        TintCol3(178, 116, 56);
        FilledCircle3(px + 0.40f, gy + 0.76f, 0.30f, 178, 116, 56, 255);
        FilledCircle3(px + 0.28f, gy + 0.96f, 0.22f, 178, 116, 56, 255);
        FilledCircle3(px + 0.40f, gy + 0.76f, 0.09f, 74, 48, 26, 255);  // sound hole
        TintCol3(96, 66, 38);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px + 0.18f, gy + 1.06f); glVertex2f(px - 0.42f, gy + 1.44f);
        glEnd();
        // Strumming hand
        TintCol3(228, 190, 152);
        FilledCircle3(px + 0.46f, gy + 0.70f + 0.12f * beat, 0.10f, 228, 190, 152, 255);
        glLineWidth(1.0f);
    }

    // ---- Drummer ---------------------------------------------------------
    {
        float px = gx + 2.30f;
        DrawChild3(px, gy, 0.0f, 120, 190, 130, false, 0.0f);
        // Drum slung at the waist
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
        // Sticks, out of phase with each other
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

    // ---- Notes drifting up out of the bandstand ---------------------------
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

    // ---- Two listeners sitting on the grass, flanking the dog's run -------
    for (int i = 0; i < 2; i++) {
        float lx = (i == 0) ? gx - 5.6f : gx + 5.4f;
        DrawFigureShadow3(lx, gy + 1.2f, 0.5f);
        DrawChild3(lx, gy + 1.2f, 0.0f, (i == 0) ? 230 : 150,
                   (i == 0) ? 170 : 130, (i == 0) ? 90 : 200, true,
                   0.10f * sinf(playPhase3 * 0.9f + i));
    }
}

// ============================================================================
//  MORE PLAY: SKIPPING ROPE + BALL GAME
// ============================================================================
// ---- Three children and a turning rope ------------------------------------
//  The rope is one curve whose sag is driven by a cosine: above their heads
//  at the top of the turn, under the jumper's feet at the bottom. The jumper
//  leaves the ground only while the rope is below her, which is the detail
//  that makes it read as skipping rather than as three figures and a line.
void DrawSkipping3() {
    const float y = -16.9f, cx = -21.0f;
    float turn = playPhase3 * 2.4f;
    float arc  = cosf(turn);                 // +1 overhead, -1 under the feet
    float handY = y + 1.30f;

    // Turners
    for (int i = 0; i < 2; i++) {
        float tx = cx + (i == 0 ? -2.5f : 2.5f);
        DrawFigureShadow3(tx, y, 0.44f);
        DrawChild3(tx, y, 0.0f, (i == 0) ? 240 : 120, (i == 0) ? 140 : 190,
                   (i == 0) ? 90 : 210, false, 0.0f);
    }

    // The rope
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

    // The jumper: off the ground only while the rope is passing under her
    float lift = (arc < 0.0f) ? (-arc) * 0.42f : 0.0f;
    DrawFigureShadow3(cx, y, 0.40f + lift * 0.3f);
    DrawChild3(cx, y + lift, 0.0f, 230, 120, 170, false, 0.8f * lift);
}

// ---- Two children knocking a ball back and forth --------------------------
void DrawBallGame3() {
    const float y = -16.4f, lx = 22.8f, rx = 29.2f;

    // The ball follows a real arc, and the volley reverses each pass.
    float cycle = fmodf(playPhase3 * 0.34f, 2.0f);
    bool  toRight = (cycle < 1.0f);
    float t  = toRight ? cycle : (cycle - 1.0f);
    float bx = toRight ? (lx + (rx - lx) * t) : (rx - (rx - lx) * t);
    float by = y + 0.55f + sinf(PI3 * t) * 2.35f;

    // Arms go up on whichever child the ball is heading toward.
    float reachL = toRight ? 0.0f : (t * t);
    float reachR = toRight ? (t * t) : 0.0f;

    DrawFigureShadow3(lx, y, 0.46f);
    DrawChild3(lx, y, 0.0f, 90, 180, 140, false, 0.25f * reachL);
    DrawFigureShadow3(rx, y, 0.46f);
    DrawChild3(rx, y, 0.0f, 220, 120, 110, false, 0.25f * reachR);

    // Raised arms, drawn over the figures
    TintCol3(228, 190, 152);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(lx, y + 0.92f);
        glVertex2f(lx + 0.42f, y + 1.05f + 0.55f * reachL);
        glVertex2f(rx, y + 0.92f);
        glVertex2f(rx - 0.42f, y + 1.05f + 0.55f * reachR);
    glEnd();
    glLineWidth(1.0f);

    // Ball: a panelled beach ball, spinning with its travel
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
    // Shadow of the ball tracks under it, tightening as it comes down
    float h = (by - (y + 0.55f)) / 2.35f;
    DrawGroundShadow(bx, y, 0.42f * (1.0f - 0.35f * h), 0.0f,
                     (unsigned char)(70 * (1.0f - 0.6f * h)));
}

// ============================================================================
//  TREES / FLAGPOLE (shared wind system)
// ============================================================================
// A tree used to be a tapered trunk with nine blobs balanced on it. That
// reads as a lollipop at any size, and these are now the largest objects in
// the park, so the silhouette has to survive being looked at.
//
// Each tree is grown recursively instead: a gnarled trunk forking into
// limbs, each limb forking again, five levels deep, with every fork angle
// and length pulled from a hash of the tree's own seed -- so the shape is
// identical frame to frame but different tree to tree. Blossom is collected
// into a buffer as the recursion runs and painted afterwards, which is what
// keeps the canopy sitting in front of the whole branchwork rather than
// being sliced up by whichever limb happened to be drawn last.
constexpr int MAX_BLOSSOMS3 = 800;
struct BlossomBlob3 { float x, y, r, jit; };
BlossomBlob3 blossomBuf3[MAX_BLOSSOMS3];
int   blossomCount3 = 0;
float blossomLo3 = 0.0f, blossomHi3 = 1.0f;
float blossomLx3 = 0.0f, blossomRx3 = 1.0f;

// Stable pseudo-random from an integer seed. No rand() inside a draw call,
// or the tree would re-grow itself sixty times a second.
inline float TreeRand3(int seed) {
    float s = sinf((float)seed * 12.9898f) * 43758.5453f;
    return s - floorf(s);
}

inline int TreeSeed3(const Tree3& t) {
    return (int)(t.x * 7.0f) + (int)(t.swayPhase * 131.0f) + 1009;
}

// Roughly where the canopy sits and how wide it is, so the petal system
// knows where petals should come from and where they stop being that
// tree's problem.
inline float TreeCanopyY3(const Tree3& t) { return t.y + 6.4f * t.scale; }
inline float TreeCanopyR3(const Tree3& t) { return 5.6f * t.scale; }

void PushBlossom3(float x, float y, float r, float jit) {
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

// One puff of blossom at a branch tip: a handful of overlapping blobs,
// wider than they are tall, so the canopy reads as cloud and not as beads.
void PushCluster3(float x, float y, float r, int seed) {
    int n = 7 + (int)(TreeRand3(seed * 3 + 7) * 4.0f);
    for (int i = 0; i < n; i++) {
        float a  = TreeRand3(seed *  5 + i * 17 + 1) * 2.0f * PI3;
        float d  = TreeRand3(seed * 11 + i * 23 + 2) * r * 0.85f;
        float rr = r * (0.45f + 0.55f * TreeRand3(seed * 13 + i * 29 + 3));
        PushBlossom3(x + cosf(a) * d * 1.25f, y + sinf(a) * d * 0.80f, rr,
                     TreeRand3(seed * 17 + i * 31 + 4));
    }
}

// Four tones, darkest to lightest. Blossom is lit from above, so tone is
// chosen by how high a blob sits in the canopy: deep rose in the shaded
// underside, near-white on the crown.
void BlossomTone3(int tone, unsigned char& r, unsigned char& g, unsigned char& b) {
    if (autumnMode3) {
        if      (tone == 0) { r = 150; g =  68; b =  34; }
        else if (tone == 1) { r = 206; g = 110; b =  44; }
        else if (tone == 2) { r = 238; g = 162; b =  62; }
        else                { r = 250; g = 214; b = 148; }
        return;
    }
    if      (tone == 0) { r = 196; g =  94; b = 140; }   // deep rose, underside
    else if (tone == 1) { r = 234; g = 128; b = 176; }   // mid pink
    else if (tone == 2) { r = 250; g = 180; b = 212; }   // pale pink
    else                { r = 255; g = 240; b = 248; }   // white, sun side
}

// One limb, drawn as a tapered strip along a curving polyline, then forked.
// `curl` is what makes it gnarled rather than a straight stick; the sway
// term is scaled by how far out along the branching this limb sits, so the
// tips move and the trunk barely does.
void DrawBranch3(float bx, float by, float ang, float len, float w,
                 int depth, int seed, float gust) {
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

    // A lit edge down one side of the heavy limbs, so the bark has a round
    // rather than flat.
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

    // A little blossom partway out on the smaller limbs, so the canopy has
    // depth inside it instead of being a ring of puffs around the outside.
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
        // No limb is ever allowed to point below the horizontal.
        if (na < 0.12f) na = 0.12f;
        if (na > 3.02f) na = 3.02f;
        DrawBranch3(tipX, tipY, na, len * (0.64f + 0.16f * r2), w * 0.56f,
                    depth - 1, seed * 31 + k * 13 + 17, gust);
    }
}

void DrawTree3(const Tree3& t) {
    float sc   = t.scale;
    float gust = sinf(windPhase3 + t.swayPhase) * 0.42f * windIntensity3;
    int   seed = TreeSeed3(t);

    // A canopy twenty units across was casting the same small blob a
    // pedestrian does. The pool now matches the crown, and stretches and
    // swings with the sun like every other shadow in the scene.
    DrawFigureShadow3(t.x, t.y, 4.0f * sc);

    // ---- Grass mound the roots stand on ---------------------------------
    TintCol3(78, 140, 76);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(t.x, t.y - 0.15f * sc);
        for (int i = 0; i <= 16; i++) {
            float a = PI3 * (float)i / 16.0f;
            glVertex2f(t.x + 2.8f * sc * cosf(a),
                       t.y - 0.15f * sc + 0.75f * sc * sinf(a));
        }
    glEnd();

    // ---- Petals already fallen, pooled on the grass ----------------------
    //  The fall had nowhere to end: a petal simply vanished at the grass.
    //  This is where it went.
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

    // ---- Root flare ------------------------------------------------------
    TintCol3(50, 36, 30);
    for (int i = -2; i <= 2; i++) {
        float rx = t.x + i * 0.62f * sc;
        glBegin(GL_TRIANGLES);
            glVertex2f(t.x, t.y + 1.7f * sc);
            glVertex2f(rx - 0.38f * sc, t.y);
            glVertex2f(rx + 0.38f * sc, t.y);
        glEnd();
    }

    // ---- Grow it ---------------------------------------------------------
    blossomCount3 = 0;
    blossomLo3 =  1e9f;
    blossomHi3 = -1e9f;
    blossomLx3 =  1e9f;
    blossomRx3 = -1e9f;

    // A touch of lean off vertical, so no two trees stand to attention.
    float lean = (TreeRand3(seed + 3) - 0.5f) * 0.30f;
    DrawBranch3(t.x, t.y, PI3 * 0.5f + lean, 3.4f * sc, 2.3f * sc, 4, seed, gust);

    // ---- Canopy, darkest pass first -------------------------------------
    float span = blossomHi3 - blossomLo3;
    if (span < 0.001f) span = 0.001f;
    float wide = blossomRx3 - blossomLx3;
    if (wide < 0.001f) wide = 0.001f;

    // Rim light: the lower the sun, the more the windward edge of the
    // canopy burns warm while the far side stays rose. Costs two lines and
    // is the difference between a pink shape and a lit object.
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

void DrawTrees3() {
    // Back-to-front inside the far lawn, so overlapping canopies layer
    // correctly now that they no longer share one y.
    int order[NUM_TREES3];
    for (int i = 0; i < NUM_TREES3; i++) order[i] = i;
    for (int i = 1; i < NUM_TREES3; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && trees3[order[j]].y < trees3[key].y) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_TREES3; i++) DrawTree3(trees3[order[i]]);
}

// ---- Falling blossom ------------------------------------------------------
//  Petals are the thing that makes a cherry read as a cherry, so they run
//  all the time rather than only in autumn mode.
void SpawnPetal3(int i, bool initial) {
    Petal3& p = petals3[i];

    if (p.tree >= 0) {
        const Tree3& t = trees3[p.tree];
        float cy = TreeCanopyY3(t), cr = TreeCanopyR3(t);
        float a  = (rand() % 628) / 100.0f;
        float d  = (rand() % 100) / 100.0f;
        p.x = t.x + cosf(a) * cr * (0.25f + 0.75f * d);
        p.y = cy  + sinf(a) * cr * 0.55f;
        // On the first frame, scatter them down the whole height of the
        // fall instead of releasing all 170 from the canopy at once.
        if (initial) p.y = t.y + ((rand() % 100) / 100.0f) * (cy + cr * 0.5f - t.y);
        p.size = 0.16f + (rand() % 12) / 100.0f;
    } else {
        p.x    = -62.0f + (rand() % 1240) / 10.0f;
        p.y    = -4.0f + (rand() % 1100) / 100.0f;
        p.size = 0.30f + (rand() % 24) / 100.0f;
        // Where this one will touch down. Spread through the depth of the
        // near water so they do not all settle on a single line.
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

void InitPetals3() {
    float total = 0.0f;
    for (int i = 0; i < NUM_TREES3; i++) total += trees3[i].scale;

    for (int i = 0; i < MAX_PETALS3; i++) {
        if (i % 6 == 5) {
            petals3[i].tree = -1;                    // foreground drifter
        } else {
            // Weighted by canopy size: the two hero trees shed most of it,
            // which is what you would actually see.
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

// layer 0 = falling from a tree, 1 = falling past the camera,
// 2 = landed and riding the river.
void DrawPetals3(int layer) {
    for (int i = 0; i < MAX_PETALS3; i++) {
        const Petal3& p = petals3[i];
        int mine = p.afloat ? 2 : (p.tree < 0 ? 1 : 0);
        if (mine != layer) continue;

        unsigned char r, g, b;
        BlossomTone3(1 + (int)(p.jit * 2.99f), r, g, b);

        // Flutter: the petal turns edge-on and back as it falls, so it
        // narrows and widens rather than spinning like a flat coin.
        float sq = 0.25f + 0.75f * fabsf(sinf(p.phase * 1.7f));

        glPushMatrix();
        if (layer == 2) {
            // Lying flat on the water, riding the swell.
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
                // Notch at the tip, so it is a petal and not a dot.
                float rr = p.size * (1.0f - 0.30f * fabsf(cosf(a)));
                glVertex2f(rr * cosf(a), rr * sinf(a) * 1.30f);
            }
        glEnd();
        glPopMatrix();
    }
}

void UpdatePetals3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdatePetals3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        for (int i = 0; i < MAX_PETALS3; i++) {
            Petal3& p = petals3[i];

            // Already down: the river carries it, it no longer falls.
            if (p.afloat) {
                p.x   += 0.045f + 0.028f * windIntensity3;
                p.rot += p.spin * 0.05f;
                if (p.x > 64.0f) SpawnPetal3(i, false);
                continue;
            }

            p.phase += p.swing;
            // A petal does not drop, it sideslips. The swing is what makes
            // it read as floating rather than raining.
            p.x   += sinf(p.phase) * 0.055f + 0.010f * windIntensity3;
            p.y   -= p.fall * (0.55f + 0.45f * windIntensity3);
            p.rot += p.spin * (0.6f + 0.4f * windIntensity3);

            if (p.tree >= 0) {
                const Tree3& t = trees3[p.tree];
                // Back onto the branch the moment it reaches the grass, or
                // if a gust has carried it clear of the canopy's footprint.
                // This is the one rule keeping the drift under the trees.
                if (p.y < t.y - 0.4f ||
                    fabsf(p.x - t.x) > TreeCanopyR3(t) * 2.1f) SpawnPetal3(i, false);
            } else if (p.y <= p.landY) {
                // Touchdown. This is what ties the trees to the river
                // instead of leaving them as two unrelated halves.
                p.afloat = true;
                p.y      = p.landY;
            } else if (p.x > 64.0f) {
                SpawnPetal3(i, false);
            }
        }
    }
    glutTimerFunc(30, UpdatePetals3, 0);
}

// ============================================================================
//  FERRIS WHEEL
// ----------------------------------------------------------------------------
//  The tree line had one real hole in it. The two hero cherries close at
//  x = -20.5 and x = +3.1, and the 23 units between them held nothing but a
//  lamp post -- the flattest stretch of the whole far lawn. The wheel fills
//  it exactly, and because it stands 12 units clear of the hill crest it is
//  the only park structure that reads against open sky.
//
//  It is drawn with the far-lawn band, before DrawHaze3, so it picks up the
//  same aerial perspective as everything else at that distance.
// ============================================================================
constexpr float FERRIS_CX3     = -8.0f;
constexpr float FERRIS_CY3     =  3.5f;
constexpr float FERRIS_R3      =  7.8f;
constexpr float FERRIS_GROUND3 = -8.6f;
constexpr int   NUM_GONDOLAS3  = 10;
constexpr int   FERRIS_BULBS3  = 34;

float ferrisAngle3 = 0.0f;

void DrawFerrisWheel3() {
    // ---- A-frame --------------------------------------------------------
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
    // Concrete footings
    TintCol3(150, 148, 142);
    for (int i = -1; i <= 1; i += 2) {
        glBegin(GL_QUADS);
            glVertex2f(FERRIS_CX3 + i * 5.5f, FERRIS_GROUND3 - 0.30f);
            glVertex2f(FERRIS_CX3 + i * 3.7f, FERRIS_GROUND3 - 0.30f);
            glVertex2f(FERRIS_CX3 + i * 3.9f, FERRIS_GROUND3 + 0.55f);
            glVertex2f(FERRIS_CX3 + i * 5.3f, FERRIS_GROUND3 + 0.55f);
        glEnd();
    }

    // ---- Spokes ---------------------------------------------------------
    TintCol4(120, 124, 134, 235);
    glLineWidth(1.3f);
    glBegin(GL_LINES);
    for (int i = 0; i < NUM_GONDOLAS3 * 2; i++) {
        float a = ferrisAngle3 + i * (PI3 / NUM_GONDOLAS3);
        glVertex2f(FERRIS_CX3, FERRIS_CY3);
        glVertex2f(FERRIS_CX3 + FERRIS_R3 * cosf(a), FERRIS_CY3 + FERRIS_R3 * sinf(a));
    }
    glEnd();

    // ---- Rim: two hoops with lattice between them -----------------------
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

    // ---- Rim bulbs ------------------------------------------------------
    // Dull grey beads by day. At dusk they light, running a chase around the
    // rim, so the wheel joins the park lamps on key 4 instead of going dark
    // with everything else.
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

    // ---- Gondolas -------------------------------------------------------
    // Hung from the rim and drawn axis-aligned, so the floors stay level as
    // the wheel turns -- which is the whole point of a Ferris wheel.
    const unsigned char cabCols[5][3] = {
        {214, 76, 84}, {236, 176, 56}, {84, 156, 204}, {108, 176, 96}, {196, 116, 190}
    };
    for (int i = 0; i < NUM_GONDOLAS3; i++) {
        float a  = ferrisAngle3 + i * (2.0f * PI3 / NUM_GONDOLAS3);
        float cx = FERRIS_CX3 + FERRIS_R3 * cosf(a);
        float cy = FERRIS_CY3 + FERRIS_R3 * sinf(a);
        const unsigned char* c = cabCols[i % 5];

        // Hanger
        TintCol3(110, 112, 120);
        glLineWidth(1.4f);
        glBegin(GL_LINES); glVertex2f(cx, cy); glVertex2f(cx, cy - 0.62f); glEnd();

        // Tub
        TintCol3(c[0], c[1], c[2]);
        glBegin(GL_QUADS);
            glVertex2f(cx - 0.70f, cy - 1.52f); glVertex2f(cx + 0.70f, cy - 1.52f);
            glVertex2f(cx + 0.80f, cy - 0.66f); glVertex2f(cx - 0.80f, cy - 0.66f);
        glEnd();
        // Safety bar
        TintCol4(40, 42, 48, 220);
        glLineWidth(1.6f);
        glBegin(GL_LINES);
            glVertex2f(cx - 0.78f, cy - 0.86f); glVertex2f(cx + 0.78f, cy - 0.86f);
        glEnd();
        // Canopy
        TintCol3((unsigned char)(c[0] * 0.72f), (unsigned char)(c[1] * 0.72f),
                 (unsigned char)(c[2] * 0.72f));
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy - 0.62f);
            for (int k = 0; k <= 10; k++) {
                float a2 = PI3 * ((float)k / 10.0f);
                glVertex2f(cx + 0.86f * cosf(a2), cy - 0.62f + 0.42f * sinf(a2));
            }
        glEnd();

        // Riders in most of the cabins
        if (i % 3 != 2) {
            FilledCircle3(cx - 0.24f, cy - 1.00f, 0.17f, 236, 198, 160, 255);
            FilledCircle3(cx - 0.24f, cy - 1.18f, 0.20f, 90, 130, 200, 255);
            if (i % 2 == 0) {
                FilledCircle3(cx + 0.26f, cy - 1.04f, 0.15f, 232, 190, 152, 255);
                FilledCircle3(cx + 0.26f, cy - 1.20f, 0.18f, 206, 96, 110, 255);
            }
        }
    }

    // ---- Hub ------------------------------------------------------------
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

// ============================================================================
//  FAIRGROUND DRESSING
// ----------------------------------------------------------------------------
//  A wheel standing on bare grass reads as a model of a wheel. A booth, a
//  stall, bunting and a roped queue lane turn it into somewhere people go.
//  All of it lives in the far lawn, tucked into the slots the cherry canopies
//  leave free: the booth at x = -18, the stall at x = 15.
// ============================================================================

// One striped canvas roof, used by both the booth and the stall.
void DrawStripedAwning3(float cx, float topY, float halfW, float rise,
                        unsigned char r, unsigned char g, unsigned char b) {
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
    // Scalloped valance along the edge
    TintCol3(r, g, b);
    for (int i = 0; i <= 7; i++)
        FilledCircle3(cx - halfW + i * (2.0f * halfW / 7.0f), topY - 0.04f,
                      halfW * 0.13f, r, g, b, 255);
}

// ---- Ticket booth ----------------------------------------------------------
constexpr float BOOTH_X3 = -18.2f;

void DrawTicketBooth3() {
    const float x = BOOTH_X3, y = FERRIS_GROUND3 - 0.2f;

    DrawFigureShadow3(x, y, 1.8f);

    // Body
    TintCol3(228, 226, 218);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f, y);         glVertex2f(x + 1.55f, y);
        glVertex2f(x + 1.55f, y + 3.10f); glVertex2f(x - 1.55f, y + 3.10f);
    glEnd();
    // Panelling
    TintCol4(150, 146, 136, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.52f, y); glVertex2f(x - 0.52f, y + 3.10f);
        glVertex2f(x + 0.52f, y); glVertex2f(x + 0.52f, y + 3.10f);
        glVertex2f(x - 1.55f, y + 0.55f); glVertex2f(x + 1.55f, y + 0.55f);
    glEnd();

    // Serving window, with the counter shelf and a dark interior
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

    // Roof and sign
    DrawStripedAwning3(x, y + 3.10f, 1.95f, 1.05f, 206, 68, 74);
    TintCol3(62, 48, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.45f, y + 2.45f); glVertex2f(x + 1.45f, y + 2.45f);
        glVertex2f(x + 1.45f, y + 3.05f); glVertex2f(x - 1.45f, y + 3.05f);
    glEnd();
    TintCol3(255, 228, 168);
    DrawBitmapTextCentered(x, y + 2.62f, "TICKETS", GLUT_BITMAP_HELVETICA_10);

    // Lamp over the window -- lit at dusk with the rest of the park
    if (LampsOn3()) {
        tintOn3 = false;
        FilledCircle3(x, y + 2.60f, 1.55f, 255, 206, 132, 46);
        FilledCircle3(x, y + 2.60f, 0.24f, 255, 244, 206, 255);
        tintOn3 = true;
    }
    glLineWidth(1.0f);
}

// ---- Bunting ---------------------------------------------------------------
//  Two spans: booth to the wheel's left leg, and the right leg out to the
//  park lamp at x = 0. Each hangs in a catenary and the flags swing with the
//  same wind that moves the trees.
void DrawBuntingSpan3(float x1, float y1, float x2, float y2, int flags) {
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

void DrawBunting3() {
    DrawBuntingSpan3(BOOTH_X3 + 1.6f, FERRIS_GROUND3 + 3.9f,
                     FERRIS_CX3 - 4.4f, FERRIS_GROUND3 + 0.6f, 7);
    DrawBuntingSpan3(FERRIS_CX3 + 4.4f, FERRIS_GROUND3 + 0.6f,
                     0.0f, FERRIS_GROUND3 + 3.4f, 7);
}

// ---- Roped queue lane ------------------------------------------------------
void DrawQueueLane3() {
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

// ---- Refreshment stall, filling the second gap in the tree line -----------
constexpr float STALL_X3 = 15.0f;

void DrawRefreshmentStall3() {
    const float x = STALL_X3, y = -9.3f;

    DrawFigureShadow3(x, y, 2.1f);

    // Counter
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

    // Corner posts up to the awning
    TintCol3(120, 88, 56);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 2.05f, y + 1.55f); glVertex2f(x - 2.05f, y + 3.05f);
        glVertex2f(x + 2.05f, y + 1.55f); glVertex2f(x + 2.05f, y + 3.05f);
    glEnd();

    // Jars and cups on the counter
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

void DrawFairground3() {
    DrawTicketBooth3();
    DrawQueueLane3();
    DrawBunting3();
}

void UpdateFerrisWheel3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFerrisWheel3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) ferrisAngle3 += 0.0042f;   // one turn every ~75 seconds
    glutTimerFunc(30, UpdateFerrisWheel3, 0);
}

// ---- Small decorative bushes -----------------------------------------------
void DrawBush3(float x, float y, float scale) {
    float sway = sinf(windPhase3 * 0.8f) * 0.15f * windIntensity3;
    FilledCircle3(x+sway, y, 0.9f*scale, 60, 130, 60, 255);
    FilledCircle3(x+sway-0.6f*scale, y-0.1f, 0.65f*scale, 65, 140, 65, 255);
    FilledCircle3(x+sway+0.6f*scale, y-0.1f, 0.65f*scale, 65, 140, 65, 255);
}

void DrawBushes3() {
    // Far lawn: these sit behind the footpath, so walkers correctly pass in
    // front of them. Scaled by depth like everything else on the back lawn.
    DrawBush3(-55.0f,  -9.4f, 1.0f * DepthScale(-9.4f) / DepthScale(-13.8f));
    DrawBush3(-17.0f, -10.1f, 0.8f * DepthScale(-10.1f) / DepthScale(-13.8f));
    DrawBush3( 37.0f,  -9.4f, 0.9f * DepthScale(-9.4f) / DepthScale(-13.8f));
    DrawBush3( 52.0f, -16.8f, 1.25f);          // one in the near lawn for depth
}

// ============================================================================
//  GAP FILLING -- park furniture and planting
// ----------------------------------------------------------------------------
//  Four stretches beside the footpath held nothing at all:
//
//      far   -60 .. -55.5   left of the first cherry
//      far    13 .. 22      between two cherries (the stall now sits here)
//      near  -60 .. -50     left of the gardener
//      near   46 .. 60      right of the cafe
//
//  These pieces fill them with things a real park has, keeping structures on
//  the near side where the eye can read the detail and planting on the far
//  side where it only needs to break the skyline.
// ============================================================================

// ---- Clipped hedge run -----------------------------------------------------
//  Drawn as a row of overlapping lobes with a flat clipped top, which is what
//  separates a hedge from a line of bushes.
void DrawHedgeRun3(float x0, float x1, float y, float h, float scale) {
    int lobes = (int)((x1 - x0) / (0.85f * scale)) + 1;
    if (lobes < 2) lobes = 2;

    // Shadow pooled under the whole run
    DrawGroundShadow((x0 + x1) * 0.5f, y, (x1 - x0) * 0.5f, 0.3f, 60);

    // Body: darker underside first, then the sunlit crown
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
    // Clipped flat top
    TintCol3(92, 158, 78);
    glBegin(GL_QUADS);
        glVertex2f(x0 - 0.4f * scale, y + h * 0.62f + 0.30f * scale);
        glVertex2f(x1 + 0.4f * scale, y + h * 0.62f + 0.30f * scale);
        glVertex2f(x1 + 0.4f * scale, y + h * 0.62f + 0.46f * scale);
        glVertex2f(x0 - 0.4f * scale, y + h * 0.62f + 0.46f * scale);
    glEnd();
    // Trimmed base
    TintCol3(38, 84, 46);
    glBegin(GL_QUADS);
        glVertex2f(x0 - 0.3f * scale, y);
        glVertex2f(x1 + 0.3f * scale, y);
        glVertex2f(x1 + 0.3f * scale, y + h * 0.30f);
        glVertex2f(x0 - 0.3f * scale, y + h * 0.30f);
    glEnd();
}

// ---- Ornamental grasses ----------------------------------------------------
//  Tall arching blades that catch the wind, so the planting is not all static
//  round shapes.
void DrawOrnamentalGrass3(float x, float y, float scale, int seed) {
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
        // Seed head on the taller blades
        if (j > 0.62f) {
            TintCol3(206, 188, 138);
            FilledCircle3(bx + lean + sway, y + len, 0.13f * scale, 206, 188, 138, 255);
        }
    }
    glLineWidth(1.0f);
}

// ---- Shrub cluster ---------------------------------------------------------
void DrawShrubCluster3(float x, float y, float scale) {
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
    // A few berries for a spot of colour
    TintCol3(206, 72, 66);
    for (int i = 0; i < 4; i++)
        FilledCircle3(x + (-0.8f + i * 0.55f) * scale, y + (0.5f + (i % 2) * 0.4f) * scale,
                      0.11f * scale, 206, 72, 66, 255);
}

// ---- Litter bin ------------------------------------------------------------
void DrawLitterBin3(float x, float y, float scale) {
    DrawGroundShadow(x, y, 0.7f * scale, 0.25f, 70);
    TintCol3(62, 76, 64);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.44f * scale, y);
        glVertex2f(x + 0.44f * scale, y);
        glVertex2f(x + 0.38f * scale, y + 1.35f * scale);
        glVertex2f(x - 0.38f * scale, y + 1.35f * scale);
    glEnd();
    // Slatted body
    TintCol4(34, 44, 36, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 4; i++) {
            float px = x + (-0.44f + i * 0.22f) * scale;
            glVertex2f(px, y + 0.10f * scale); glVertex2f(px, y + 1.25f * scale);
        }
    glEnd();
    // Domed lid with the posting slot
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

// ---- Park noticeboard ------------------------------------------------------
//  A map of the park under a little pitched roof. Reads instantly as park
//  furniture and gives the empty left-hand corner something man-made.
void DrawNoticeboard3(float x, float y, float scale) {
    DrawGroundShadow(x, y, 1.5f * scale, 0.3f, 66);

    TintCol3(104, 76, 48);
    glLineWidth(2.6f * scale);
    glBegin(GL_LINES);
        glVertex2f(x - 1.05f * scale, y); glVertex2f(x - 1.05f * scale, y + 2.0f * scale);
        glVertex2f(x + 1.05f * scale, y); glVertex2f(x + 1.05f * scale, y + 2.0f * scale);
    glEnd();

    // Board
    TintCol3(126, 92, 58);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.35f * scale, y + 1.85f * scale);
        glVertex2f(x + 1.35f * scale, y + 1.85f * scale);
        glVertex2f(x + 1.35f * scale, y + 3.70f * scale);
        glVertex2f(x - 1.35f * scale, y + 3.70f * scale);
    glEnd();
    // The map behind glass: green ground, a blue river, a pale path
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
    // "You are here"
    TintCol3(214, 62, 58);
    FilledCircle3(x - 0.30f * scale, y + 2.82f * scale, 0.11f * scale, 214, 62, 58, 255);

    // Pitched roof
    TintCol3(86, 62, 42);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.60f * scale, y + 3.70f * scale);
        glVertex2f(x + 1.60f * scale, y + 3.70f * scale);
        glVertex2f(x, y + 4.40f * scale);
    glEnd();
    glLineWidth(1.0f);
}

// ---- Ornamental pond -------------------------------------------------------
//  Stone-edged and formal, so it reads as a garden feature rather than a
//  second stretch of river.
void DrawOrnamentalPond3(float x, float y, float rx, float ry) {
    // Stone coping
    TintCol3(168, 164, 156);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= 28; i++) {
            float a = (float)i / 28.0f * 2.0f * PI3;
            glVertex2f(x + (rx + 0.55f) * cosf(a), y + (ry + 0.30f) * sinf(a));
        }
    glEnd();
    // Individual coping stones
    TintCol4(120, 116, 108, 180);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 18; i++) {
            float a = (float)i / 18.0f * 2.0f * PI3;
            glVertex2f(x + rx * cosf(a), y + ry * sinf(a));
            glVertex2f(x + (rx + 0.55f) * cosf(a), y + (ry + 0.30f) * sinf(a));
        }
    glEnd();

    // Water: darker at the rim, lighter in the middle
    glBegin(GL_TRIANGLE_FAN);
        TintCol3(118, 172, 186);
        glVertex2f(x, y);
        TintCol3(52, 104, 128);
        for (int i = 0; i <= 28; i++) {
            float a = (float)i / 28.0f * 2.0f * PI3;
            glVertex2f(x + rx * cosf(a), y + ry * sinf(a));
        }
    glEnd();

    // Koi cruising under the surface
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

    // Lily pads, each with a notch cut out of it
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
        if (i < 2) {   // a bloom on two of them
            TintCol3(246, 186, 208);
            FilledCircle3(px + pr * 0.2f, py + 0.12f, 0.17f, 246, 186, 208, 255);
            TintCol3(252, 228, 150);
            FilledCircle3(px + pr * 0.2f, py + 0.12f, 0.07f, 252, 228, 150, 255);
        }
    }

    // A slow ring spreading from the middle, so the water is not dead still
    float rt = fmodf(playPhase3 * 0.10f, 1.0f);
    TintCol4(226, 244, 250, (unsigned char)(140 * (1.0f - rt)));
    glLineWidth(1.2f);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 20; i++) {
            float a = (float)i / 20.0f * 2.0f * PI3;
            glVertex2f(x + rx * 0.75f * rt * cosf(a), y + ry * 0.75f * rt * sinf(a));
        }
    glEnd();

    // Bulrushes at the back edge
    DrawOrnamentalGrass3(x - rx * 0.75f, y + ry * 0.55f, 0.8f, 7);
    DrawOrnamentalGrass3(x + rx * 0.70f, y + ry * 0.50f, 0.7f, 11);
    glLineWidth(1.0f);
}

// ---- The two far-lawn fillers ---------------------------------------------
void DrawFarFillers3() {
    // Left edge, beyond the first cherry
    DrawHedgeRun3(-60.0f, -56.0f, -8.8f, 1.5f, 0.92f);
    DrawOrnamentalGrass3(-57.0f, -8.4f, 0.85f, 3);

    // Right-hand gap in the tree line, beside the lemonade stall
    DrawHedgeRun3(19.2f, 22.0f, -9.1f, 1.4f, 0.88f);
    DrawOrnamentalGrass3(12.6f, -9.0f, 0.90f, 5);
    DrawOrnamentalGrass3(20.6f, -8.7f, 0.80f, 9);
}

// ---- The two near-lawn fillers --------------------------------------------
void DrawNearFillersLeft3() {
    DrawOrnamentalPond3(-55.0f, -17.4f, 3.6f, 1.15f);
    DrawNoticeboard3(-59.4f, -14.4f, 1.0f);
    DrawShrubCluster3(-50.6f, -19.2f, 1.0f);
}

void DrawNearFillersRight3() {
    DrawBench3(47.0f, -17.6f, 1.06f);
    DrawLitterBin3(50.6f, -18.2f, 1.1f);
    DrawHedgeRun3(54.5f, 60.0f, -19.0f, 1.6f, 1.15f);
    DrawShrubCluster3(57.0f, -15.4f, 1.05f);
}

// ---- Squirrel near the trees ---------------------------------------------
void DrawSquirrel3() {
    float x = 25.0f, y = -14.5f;
    float scurry = sinf(squirrelPhase3) * 1.5f;
    x += scurry;
    FilledCircle3(x, y, 0.28f, 150, 100, 60, 255);
    FilledCircle3(x + (scurry>=0?0.28f:-0.28f), y+0.18f, 0.18f, 150, 100, 60, 255);
    // bushy tail, curled up behind
    float tailX = x - (scurry>=0?0.35f:-0.35f);
    FilledCircle3(tailX, y+0.35f, 0.24f, 160, 110, 65, 220);
}

// ---- Butterflies near the flower beds -------------------------------------
void DrawButterflies3() {
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

// ---- Fireflies: only come out at golden hour ------------------------------
void InitFireflies3() {
    for (int i = 0; i < NUM_FIREFLIES3; i++) {
        fireflies3[i].x = -52.0f + (rand() % 1040) / 10.0f;
        fireflies3[i].y = -18.5f + (rand() % 100) / 10.0f;
        fireflies3[i].phase = (rand() % 628) / 100.0f;
        fireflies3[i].speed = 0.4f + (rand() % 60) / 100.0f;
    }
}

void DrawFireflies3() {
    // Golden hour and dusk. They used to be locked to golden hour alone,
    // which wasted them: dusk is when fireflies actually read.
    if (dayPhase3 < 2) return;
    for (int i = 0; i < NUM_FIREFLIES3; i++) {
        const Firefly3& f = fireflies3[i];
        float dx = sinf(fireflyClock3 * f.speed + f.phase) * 1.6f;
        float dy = cosf(fireflyClock3 * f.speed * 0.7f + f.phase * 1.3f) * 0.9f;
        // Each one blinks on its own cycle, and some are dark at any moment.
        float blink = sinf(fireflyClock3 * 2.6f + f.phase * 2.0f);
        if (blink <= 0.0f) continue;
        unsigned char a = (unsigned char)((IsDusk3() ? 255 : 190) * blink);
        // Untinted: a firefly emits its own light, it doesn't reflect the sky.
        tintOn3 = false;
        FilledCircle3(f.x + dx, f.y + dy, 0.30f, 210, 255, 130, (unsigned char)(a / 5));
        FilledCircle3(f.x + dx, f.y + dy, 0.12f, 235, 255, 170, a);
        tintOn3 = true;
    }
}

void UpdateFireflies3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateFireflies3, 0); return; }
    if (isAnimating3) {
        fireflyClock3 += 0.05f;
        slideT3       += 0.004f;   // drives the child riding the slide
    }
    glutTimerFunc(30, UpdateFireflies3, 0);
}

void DrawFlagpole3() {
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

void UpdateWind3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateWind3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        windPhase3 += 0.05f;
        squirrelPhase3 += 0.04f; // shared idle clock for squirrel/butterflies/goose
        playPhase3 += 0.045f;    // cafe steam, the band, skipping and the ball
    }
    glutTimerFunc(30, UpdateWind3, 0);
}

// ============================================================================
//  KITE (unique detail) + HOLDER
// ============================================================================
void DrawKiteHolder3() {
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

void DrawKiteString3() {
    float holderX = 18.0f, holderY = PATH_BOTTOM_Y3 + 1.9f;
    float kx = kiteBaseX3 + sinf(windPhase3*0.8f) * 1.5f * windIntensity3;
    float ky = kiteBaseY3 + sinf(kiteBobPhase3) * 0.6f;

    // In a gust the line pulls taut; in calm air it hangs in a slack catenary.
    // `slack` is 1 at calm and drops toward 0 as the wind rises, which is
    // what makes a kite read as actually being flown.
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
            sy -= 4.2f * slack * t * (1.0f - t);                       // sag
            sy += sinf(t*6.0f + windPhase3) * 0.35f * (1.0f - t) * windIntensity3;
            glVertex2f(sx, sy);
        }
    glEnd();
}

void DrawKite3() {
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

void UpdateKite3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdateKite3, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating3) {
        kiteBobPhase3 += 0.04f + 0.03f * windIntensity3;
        // More wind, more lift: the kite rides higher and leans downwind.
        float target = 16.0f + 3.4f * windIntensity3;
        kiteBaseY3 += (target - kiteBaseY3) * 0.02f;
    }
    glutTimerFunc(30, UpdateKite3, 0);
}

// ============================================================================
//  FOREGROUND: STONE BRIDGE + REEDS
// ----------------------------------------------------------------------------
//  The band from y = -20 down to y = -40 is a quarter of the window. It had
//  colour -- the river fills it -- but nothing lived in it, so every object a
//  viewer could identify was crammed into the narrow strip between y = -17
//  and y = -6 and the scene read as a flat frieze.
//
//  The jetty gives the near water something built on it, and the reeds put
//  a screen of tall planting right in front of the camera.
// ============================================================================

// ---- Timber jetty, with a kayak moored alongside --------------------------
//  This stands where the stone bridge used to. A bridge that size dominated
//  the bottom of the frame; a low jetty holds the same band without competing
//  with the river traffic, and it gives the new kayak somewhere to belong
//  when it is not being paddled.
constexpr float JETTY_LEFT_X3  = -52.0f;
constexpr float JETTY_RIGHT_X3 = -22.0f;
constexpr float JETTY_DECK_Y3  = -28.5f;

void DrawJettyStructure3() {
    const float L = JETTY_LEFT_X3, R = JETTY_RIGHT_X3, D = JETTY_DECK_Y3;

    // ---- Pilings --------------------------------------------------------
    // Driven well below the surface, with the darker band where the water
    // has soaked and stained them.
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
        // A little green weed on the wet band
        TintCol4(72, 104, 62, 150);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(px - 0.30f, D - 2.9f); glVertex2f(px - 0.62f, D - 2.1f);
            glVertex2f(px + 0.30f, D - 2.8f); glVertex2f(px + 0.60f, D - 2.0f);
        glEnd();
    }

    // Cross-bracing between the pilings
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

    // ---- Deck -----------------------------------------------------------
    // Laid as individual boards with gaps, because one long grey bar reads as
    // concrete rather than timber.
    const int BOARDS = 26;
    for (int i = 0; i < BOARDS; i++) {
        float t0 = (float)i / BOARDS, t1 = (float)(i + 1) / BOARDS;
        float bx0 = L + (R - L) * t0;
        float bx1 = L + (R - L) * t1 - 0.10f;      // the gap between boards
        // Deterministic per-board weathering
        float h = sinf(i * 21.7f) * 43758.5453f;
        float j = h - floorf(h);
        unsigned char base = (unsigned char)(148 + j * 34);
        TintCol3(base, (unsigned char)(base * 0.82f), (unsigned char)(base * 0.62f));
        glBegin(GL_QUADS);
            glVertex2f(bx0, D);         glVertex2f(bx1, D);
            glVertex2f(bx1, D + 0.80f); glVertex2f(bx0, D + 0.80f);
        glEnd();
        // Grain
        TintCol4(96, 74, 50, 110);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            glVertex2f(bx0 + 0.06f, D + 0.28f); glVertex2f(bx1 - 0.06f, D + 0.34f);
        glEnd();
    }
    // Front edge board, catching the light
    TintCol3(186, 156, 112);
    glBegin(GL_QUADS);
        glVertex2f(L, D + 0.80f); glVertex2f(R, D + 0.80f);
        glVertex2f(R, D + 1.02f); glVertex2f(L, D + 1.02f);
    glEnd();

    // ---- Mooring bollards, with rope coiled at the foot of one ----------
    for (int i = 0; i < 2; i++) {
        float bx = R - 1.2f - i * 3.4f;
        TintCol3(72, 54, 38);
        glBegin(GL_QUADS);
            glVertex2f(bx - 0.34f, D + 1.02f); glVertex2f(bx + 0.34f, D + 1.02f);
            glVertex2f(bx + 0.28f, D + 2.30f); glVertex2f(bx - 0.28f, D + 2.30f);
        glEnd();
        FilledCircle3(bx, D + 2.38f, 0.40f, 92, 70, 50, 255);   // mushroom head
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

    // A lifebuoy on a post -- the detail that says "this is a public jetty"
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

// The moored kayak: same hull, no crew, rocking on the swell the passing
// traffic leaves behind, with its bow line running up to the bollard.
void DrawMooredKayak3() {
    const float mx = JETTY_RIGHT_X3 + 4.2f;
    const float my = JETTY_DECK_Y3 - 1.55f + sinf(waterClock3 * 0.42f + 1.1f) * 0.16f;

    glPushMatrix();
    glTranslatef(mx, my, 0.0f);
    glRotatef(sinf(waterClock3 * 0.33f) * 1.8f, 0.0f, 0.0f, 1.0f);
    glTranslatef(-mx, -my, 0.0f);
    DrawKayakBody3(mx, my, -1.0f, 0.0f, 74, 176, 206, false);
    glPopMatrix();

    // Bow line up to the nearer bollard, with a little sag in it
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

    // A paddle stowed flat on the deck boards above it
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

void DrawJetty3() {
    // Reflection of the structure, washed back toward the water colour.
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

// ---- Foreground reeds and cattails ---------------------------------------
//  Drawn last of all the water elements, oversized and a touch desaturated,
//  so they sit convincingly close to the camera.
constexpr int NUM_REEDS3 = 58;
float reedX3[NUM_REEDS3], reedY3[NUM_REEDS3], reedH3[NUM_REEDS3], reedPhase3[NUM_REEDS3];
bool  reedCattail3[NUM_REEDS3];

void InitReeds3() {
    for (int i = 0; i < NUM_REEDS3; i++) {
        reedX3[i]       = -64.0f + (rand() % 12800) / 100.0f;
        reedY3[i]       = -40.0f + (rand() % 900) / 100.0f;   // -40 .. -31
        reedH3[i]       = 5.0f + (rand() % 700) / 100.0f;
        reedPhase3[i]   = (rand() % 628) / 100.0f;
        reedCattail3[i] = (rand() % 100) < 34;
    }
}

void DrawReeds3() {
    for (int i = 0; i < NUM_REEDS3; i++) {
        float bx = reedX3[i], by = reedY3[i], h = reedH3[i];
        // Bend increases toward the tip, and with the wind.
        float bend = sinf(windPhase3 * 1.4f + reedPhase3[i]) * 0.9f * windIntensity3;

        TintCol3(52, 96, 58);
        glLineWidth(2.6f);
        glBegin(GL_LINE_STRIP);
            for (int k = 0; k <= 6; k++) {
                float t = (float)k / 6.0f;
                glVertex2f(bx + bend * t * t, by + h * t);
            }
        glEnd();

        // Blade leaves peeling off partway up
        TintCol3(62, 112, 64);
        glLineWidth(1.8f);
        glBegin(GL_LINE_STRIP);
            glVertex2f(bx + bend * 0.16f, by + h * 0.40f);
            glVertex2f(bx + bend * 0.5f + 1.5f, by + h * 0.62f);
            glVertex2f(bx + bend * 0.7f + 2.4f, by + h * 0.55f);
        glEnd();

        // Cattail head on some of them
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

// ============================================================================
//  PARK LAMPS
// ----------------------------------------------------------------------------
//  Cast-iron lamps along the footpath. Dark by day, lit at dusk, each one
//  throwing a pool of warm light onto the path and a soft halo into the air.
//  They are what makes the fourth day phase worth having rather than just a
//  different sky colour.
// ============================================================================
constexpr int NUM_LAMPS3 = 5;
// Chosen to sit in the gaps between the tree crowns (trees stand at
// x = -48, -33, 6, 28, 45 and 55), so no post grows out of a canopy.
const float lampX3[NUM_LAMPS3] = { -41.0f, -22.0f, 0.0f, 34.0f, 50.0f };

void DrawParkLamp3(float x) {
    const float baseY = PATH_TOP_Y3 + 0.2f;
    const float topY  = baseY + 6.4f;

    // Pool of light on the gravel, drawn first so the post sits inside it.
    if (LampsOn3()) {
        tintOn3 = false;
        DrawSoftEllipse(x, baseY - 0.2f, 6.2f, 1.5f, 255, 206, 130, 78, 4);
        tintOn3 = true;
    }

    TintCol3(52, 54, 58);
    glLineWidth(2.4f);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    // Base plinth
    glBegin(GL_QUADS);
        glVertex2f(x-0.42f, baseY);        glVertex2f(x+0.42f, baseY);
        glVertex2f(x+0.30f, baseY+0.75f);  glVertex2f(x-0.30f, baseY+0.75f);
    glEnd();
    // Scrolled arm and the lantern cage
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
    // Cage: cap above, taper below
    TintCol3(52, 54, 58);
    glBegin(GL_TRIANGLES);
        glVertex2f(lx-0.55f, ly+0.35f); glVertex2f(lx+0.55f, ly+0.35f); glVertex2f(lx, ly+0.95f);
    glEnd();
    glLineWidth(1.0f);
}

// Far band: the lamp posts themselves stand behind the path.
void DrawParkLamps3() {
    for (int i = 0; i < NUM_LAMPS3; i++) DrawParkLamp3(lampX3[i]);
}

// ---- Aerial perspective ---------------------------------------------------
//  The far lawn, the trees and the foreground reeds were all equally
//  saturated, so the depth bands never really separated. One wash of sky
//  colour, strongest along the horizon and fading out both up through the
//  canopies and down toward the footpath, pushes the background back.
// ============================================================================
void DrawHaze3() {
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

// ============================================================================
//  VENDOR CART / PEDESTRIANS
// ============================================================================
void DrawVendorCart3() {
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

// ---- Painter at an easel, facing the river ---------------------------------
void DrawPainter3() {
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

// ---- Fisherman standing on the riverbank -----------------------------------
void DrawFisherman3() {
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

    // Bobber: gently rides the surface, and every so often gets yanked
    // under by a bite before springing back up.
    float lineWave = 0.1f * sinf(squirrelPhase3 * 2.0f);
    float bobY     = 0.10f * sinf(squirrelPhase3 * 3.2f);
    float biteCycle = fmodf(squirrelPhase3 * 0.16f, 1.0f);
    bool  biting    = (biteCycle > 0.86f);            // short, occasional
    float dip       = biting ? -0.55f * sinf((biteCycle - 0.86f) / 0.14f * PI3) : 0.0f;

    float bx = x + 2.6f + lineWave;
    float by = y - 0.95f + bobY + dip;

    TintCol4(220, 220, 230, 180);
    glBegin(GL_LINES); glVertex2f(x+2.2f, y+0.3f); glVertex2f(bx, by + 0.05f); glEnd();
    FilledCircle3(bx, by, 0.12f, 255, 80, 60, 255);

    // Ripple rings spreading from the bobber during a bite
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

void DrawPerson3(const Ped3& p, float t) {
    // Depth: walkers further up the path are smaller. The path spans
    // PATH_BOTTOM_Y3 .. PATH_TOP_Y3, so the range is deliberately narrow --
    // enough to read, not enough to look like a scaling glitch.
    float sc = DepthScaleRange(p.y, PATH_TOP_Y3, PATH_BOTTOM_Y3, 0.86f, 1.08f);
    DrawFigureShadow3(p.x, p.y, 0.55f * sc);
    BeginDepthSprite(p.x, p.y, sc);

    float bob = sinf(t*3.0f + p.phase) * 0.12f;
    float hipX = p.x, hipY = p.y + 1.0f + bob;
    float legSwing = (p.kind == 1) ? 0.45f : 0.25f;

    // The cyclist is seated, so it replaces the standing body entirely
    // rather than being drawn on top of it.
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
        // ---- Cyclist ----------------------------------------------------
        // A seated rider leaning forward over the bars, legs turning with
        // distance travelled so the pedalling matches the speed.
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
        // Frame, bars and saddle
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
        // Rider: leaning forward over the bars, legs on the pedals
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
        TintCol3(230, 220, 60);                       // helmet
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
        // ---- Child running ahead of a parent -----------------------------
        // The parent is the figure already drawn; the child is a smaller
        // version out in front, arms up, out of step with them.
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
        glBegin(GL_LINES);          // arms up, the way children run
            glVertex2f(cx, chipY+0.60f); glVertex2f(cx + 0.45f, chipY + 1.05f);
            glVertex2f(cx, chipY+0.60f); glVertex2f(cx - 0.45f, chipY + 1.00f);
        glEnd();
        FilledCircle3(cx, chipY+0.95f, 0.24f, 235, 198, 160, 255);
        glLineWidth(1.0f);
    }

    if (p.kind == 0) {
        // Stroller. This pedestrian type was already *labelled* "stroller"
        // but drew as an ordinary walker, so the label meant nothing.
        float sx = p.x + p.dir * 1.15f;      // pushed out in front
        float sy = p.y;

        // Push handle, angled back to the walker's hands
        TintCol3(70, 70, 80);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(sx - p.dir*0.55f, sy + 1.15f);
            glVertex2f(hipX + p.dir*0.15f, hipY + 0.75f);
        glEnd();

        // Basket body
        TintCol3(60, 90, 150);
        glBegin(GL_QUADS);
            glVertex2f(sx - 0.55f, sy + 0.42f);
            glVertex2f(sx + 0.55f, sy + 0.42f);
            glVertex2f(sx + 0.48f, sy + 1.02f);
            glVertex2f(sx - 0.48f, sy + 1.02f);
        glEnd();

        // Hood/canopy over the front half
        TintCol3(40, 65, 115);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(sx + p.dir*0.05f, sy + 1.00f);
            for (int i = 0; i <= 8; i++) {
                float a = PI3 * ((float)i / 8.0f);
                glVertex2f(sx + p.dir*0.05f + p.dir * 0.62f * cosf(a),
                           sy + 1.00f + 0.50f * sinf(a));
            }
        glEnd();

        // Baby's head peeking out
        FilledCircle3(sx + p.dir*0.30f, sy + 1.05f, 0.17f, 242, 205, 170, 255);

        // Wheels (small front, larger rear, like a real buggy)
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

void DrawPedestrians3() {
    // Painter's algorithm across the path's depth: the walker standing
    // furthest back is drawn first, so people correctly overlap each other
    // when they pass. Seven items, so a plain insertion sort is fine.
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

void UpdatePedestrians3(int) {
    if (currentScreen != SCENARIO_3 || isPaused) { glutTimerFunc(120, UpdatePedestrians3, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  CONTRACT: Init / Draw / Keyboard / Mouse / kTitle
// ============================================================================
const char* kTitle = "Lakeside Park View";

void Init() {
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

void Draw() {
    glClearColor(0.5f, 0.75f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // ------------------------------------------------------------------
    //  Strict back-to-front order. The bands are: sky, hills, far lawn,
    //  the footpath and the people on it, the near lawn, then the river
    //  (which is the nearest thing to the camera) and its traffic.
    //  Anything lower in the frame is drawn later -- that single rule is
    //  what stopped walkers being composited over the playground.
    // ------------------------------------------------------------------

    // ---- SKY ---------------------------------------------------------
    DrawSky3();
    DrawSun3();
    // Jet before the clouds: it is far higher than they are, so a cloud
    // crossing in front of it is correct.
    DrawJet3();
    DrawClouds3();
    DrawBirds3();

    // Balloon while it is still high and distant: behind the ridge line.
    if (balloonY3 > hillCrestY3 + 2.0f) DrawBalloon3();

    // ---- HILLS -------------------------------------------------------
    DrawDistantHills3();
    DrawHills3();

    // ---- LAWN --------------------------------------------------------
    DrawGrass3();

    // ---- FAR LAWN (behind the footpath) ------------------------------
    // Planting first: hedges and grasses sit hard against the back of the
    // band, behind every structure.
    DrawFarFillers3();

    // The Ferris wheel and its fairground occupy the hole in the tree line
    // between x = -20.5 and x = +3.1. They are drawn before the cherries so
    // the two hero canopies, which stand nearer the camera, overlap them.
    DrawFerrisWheel3();
    DrawFairground3();
    DrawRefreshmentStall3();

    // Flagpole before the trees: a canopy this size would otherwise have
    // the flag painted on top of it.
    DrawFlagpole3();
    DrawTrees3();
    DrawPetals3(0);              // petals falling through the far lawn
    DrawBushes3();
    DrawFlowerBedsFar3();
    DrawBenchesFar3();
    DrawParkLamps3();
    DrawHaze3();                 // aerial perspective over everything far

    // Balloon once it has descended past the ridge but is still beyond the
    // path: it now genuinely crosses depth instead of floating on top.
    if (balloonY3 <= hillCrestY3 + 2.0f && balloonScale3 < 1.05f) DrawBalloon3();

    // ---- PATH + PEOPLE ------------------------------------------------
    DrawPath3();
    DrawVendorCart3();
    DrawPedestrians3();

    // ---- NEAR LAWN (in front of the footpath) -------------------------
    DrawFlowerBedsNear3();
    DrawNearFillersLeft3();      // pond, noticeboard and shrubs, x -60 .. -50
    DrawGardener3();
    DrawButterflies3();
    DrawGooseFamily3();
    DrawSquirrel3();
    DrawBenchesNear3();
    DrawFountain3();
    DrawGazebo3();
    DrawBandstand3();            // the band, drawn inside the gazebo
    DrawSkipping3();
    DrawSwing3();
    DrawSeesaw3();
    DrawSlide3();
    DrawPicnic3(-33.0f);
    DrawFrisbeeScene3();
    DrawBallGame3();
    DrawPainter3();
    DrawCafe3();
    DrawNearFillersRight3();     // bench, bin, hedge and shrubs, x 46 .. 60

    // Kite and its holder stand in the near lawn; the kite itself flies high
    // above everything, so the string is drawn first and the kite last.
    DrawKiteString3();
    DrawKite3();
    DrawKiteHolder3();

    // Balloon at its closest: passes in front of the whole park.
    if (balloonScale3 >= 1.05f) DrawBalloon3();

    // ---- WATER (nearest band) -----------------------------------------
    DrawRiver3();
    DrawPetals3(2);              // blossom that has landed on the water
    DrawFisherman3();

    // Everything floating carries its own reflection. The dock reflects
    // about its planking, each boat about its own hull line.
    DrawWithReflection3(DrawDock3, 20.0f, 7.0f, -21.0f, 3.4f);
    DrawDuckFamily3();
    DrawDucks3();
    DrawFishJumps3();
    DrawWithReflection3(DrawKayak2_3,  kayak2X3,   3.9f, -25.5f, 2.4f);
    // A kayak sits far lower in the water than the old rowboat did, so its
    // reflection is shallower.
    DrawWithReflection3(DrawKayak3,    kayakX3,    3.9f, -26.0f, 2.4f);

    // ---- FOREGROUND (nearest of all) ----------------------------------
    DrawJetty3();
    DrawReeds3();

    // ---- ATMOSPHERE ----------------------------------------------------
    DrawFireflies3();
    DrawPetals3(1);              // the near group, passing the camera

    static const char* const hud[] = {
        "1 morning   2 midday   3 golden hour   4 dusk",
        "W  wind calm/breezy/gusty    A  blossom/autumn colour",
        "X  bring the balloon in close, jump a fish",
        "SPACE pause    H help    ESC quit",
        nullptr
    };
    DrawSceneHUD(kTitle, hud);
}

void Keyboard(unsigned char key, int /*x*/, int /*y*/) {
    switch (key) {
        case '1': dayPhase3 = 0; break;
        case '2': dayPhase3 = 1; break;
        case '3': dayPhase3 = 2; break;
        case '4': dayPhase3 = 3; break;   // dusk: lamps on, fireflies out
        // Three wind levels rather than two: calm, breezy, gusty. The value
        // now feeds the river, the fountain, the balloon, the birds, the kite
        // string and the leaves as well as the trees and the flag, so one key
        // moves the whole scene instead of a few props.
        case 'w': case 'W':
            if      (windIntensity3 < 1.5f) windIntensity3 = 2.5f;
            else if (windIntensity3 < 3.2f) windIntensity3 = 4.0f;
            else                            windIntensity3 = 1.0f;
            break;
        case 'a': case 'A': autumnMode3 = !autumnMode3; break;

        // Fish jump on a 150-400 tick cooldown and the balloon takes a full
        // pass to descend, so 'X' brings both on now for a demo.
        case 'x': case 'X':
            fishCooldown3 = 1;
            balloonX3 = -20.0f;
            balloonScale3 = 1.02f;
            break;
    }
    glutPostRedisplay();
}

void Mouse(int button, int state, int /*x*/, int /*y*/) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)  isAnimating3 = true;
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) isAnimating3 = false;
    glutPostRedisplay();
}

}  // namespace Scenario3

// ============================================================================
//  SCENARIO 4  --  Winter Night Market
// ----------------------------------------------------------------------------
//  Lifted from scenario4final.cpp unchanged. Its shared-helper block was
//  already identical to the group file's, so it needs nothing of its own.
// ============================================================================
namespace Scenario4 {

// ============================================================================
//  STATE
// ============================================================================
constexpr float PI4 = 3.1416f;

bool  isAnimating4      = true;
int   snowIntensity4    = 1;     // 0 = light, 1 = medium, 2 = heavy (keys 1/2/3)
bool  multicolorLights4 = false; // toggled with 'L'

// ---- Time of day -----------------------------------------------------------
// The scene has one master parameter for daylight: 0 is the default night,
// 1 is full day. Key 4 raises it, key 5 drops it, and dayT4 eases toward the
// target a step per tick so the change plays as a sunrise or a sunset instead
// of a cut -- which also means every value derived from it can be a plain
// linear blend and will animate for free.
//
// Two rules keep the rest of the file consistent:
//   * anything made of MATTER (walls, ice, stone, cloth) picks its colour with
//     ColourDN4 / ColourDN4A, which crossfades a night palette entry into a
//     day one;
//   * anything that is LIGHT (bulbs, windows, halos, light pools, the glow on
//     the snow) multiplies its alpha or brightness by NightT4, so every
//     artificial source fades out as the sun comes up and no daylight frame
//     contains a glow that the sun would have washed away.
float dayT4      = 0.0f;
float dayTarget4 = 0.0f;

inline float MixF4(float night, float day) { return night + (day - night) * dayT4; }

inline unsigned char MixB4(int night, int day) {
    float v = night + (day - night) * dayT4;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (unsigned char)(v + 0.5f);
}

inline unsigned char DayGlow4(int night, int day, float strength = 1.0f) {
    return (unsigned char)((night + (day - night) * dayT4) * strength + 0.5f);
}

// Set the current colour from a night/day pair.
inline void ColourDN4(int nr, int ng, int nb, int dr, int dg, int db) {
    glColor3ub(MixB4(nr, dr), MixB4(ng, dg), MixB4(nb, db));
}
inline void ColourDN4A(int nr, int ng, int nb, int na,
                       int dr, int dg, int db, int da) {
    glColor4ub(MixB4(nr, dr), MixB4(ng, dg), MixB4(nb, db), MixB4(na, da));
}

// How much of the scene's artificial lighting still reads: 1 at night, 0 by day.
inline float NightT4() { return 1.0f - dayT4; }
inline unsigned char NightA4(int nightAlpha) {
    return (unsigned char)(nightAlpha * NightT4());
}
// Dims a lit colour channel toward an unlit daylight value.
inline unsigned char LampB4(int lit, int unlit) {
    return (unsigned char)(unlit + (lit - unlit) * NightT4());
}

// ============================================================================
//  SEASON  --  winter <-> autumn, toggled with 'S'
// ----------------------------------------------------------------------------
//  Built on exactly the same pattern as the day/night mix above: one value
//  eases between 0 and 1 and every colour in the scene is written as a pair.
//  Nothing snaps; the whole plaza crossfades over about three seconds.
//
//      seasonT4 = 0   deep winter -- snow, ice, the market in full swing
//      seasonT4 = 1   late autumn -- wet ochre paving, fallen leaves, open
//                     water, the same market dressed for a different month
//
//  Autumn was chosen over summer deliberately. It is still cold enough that
//  the coats, the braziers, the string lights and the night market itself all
//  still make sense -- a summer version would have left an ice rink, a
//  snowman family and Santa standing in the sun with no explanation.
float seasonT4      = 0.0f;
float seasonTarget4 = 0.0f;

// Winter <-> autumn mixers, mirroring MixF4 / MixB4 / ColourDN4.
inline float MixSF4(float winter, float autumnV) {
    return winter + (autumnV - winter) * seasonT4;
}
inline unsigned char MixSB4(int winter, int autumnV) {
    float v = winter + (autumnV - winter) * seasonT4;
    if (v < 0.0f)   v = 0.0f;
    if (v > 255.0f) v = 255.0f;
    return (unsigned char)(v + 0.5f);
}
inline void ColourSeason4(int wr, int wg, int wb, int ar, int ag, int ab) {
    glColor3ub(MixSB4(wr, ar), MixSB4(wg, ag), MixSB4(wb, ab));
}

// How much of winter still reads: 1 in winter, 0 in autumn. Anything made of
// snow or ice fades out through this, so nothing has to be branched on.
inline float SeasonWinter4() { return 1.0f - seasonT4; }
inline float SeasonAutumn4() { return seasonT4; }

// Props that only belong to one season fade rather than vanish, so the
// crossfade never shows an object popping out of existence.
inline unsigned char WinterA4(int alpha) {
    return (unsigned char)(alpha * SeasonWinter4());
}
inline unsigned char AutumnA4(int alpha) {
    return (unsigned char)(alpha * SeasonAutumn4());
}

// Settled snow depth. Declared here rather than beside UpdateSnow4 because the
// season helpers below need it; SnowDepth4() zeroes it out of season, so roof
// caps and drifts melt away as autumn comes in.
float snowCover4 = 0.55f;

inline float SnowDepth4() { return snowCover4 * SeasonWinter4(); }
float twinklePhase4     = 0.0f;
float pedTimer4         = 0.0f;

// ---- Stars -----------------------------------------------------------
// A real winter sky is mostly faint pinpricks with a handful of bright ones,
// so every star carries its own size, brightness, colour temperature and
// twinkle rate. Drawing the whole field at one size and one alpha is what
// made the old 25-star scatter read as a row of identical dots.
constexpr int NUM_STARS4 = 190;
struct Star4 { float x, y, twinkle, mag, rate; int bucket; unsigned char r, g, b; };
Star4 stars4[NUM_STARS4];

// The few stars bright enough to earn a halo and a cross flare. Without them
// a star field has no focal points and reads as uniform noise.
constexpr int NUM_BRIGHT_STARS4 = 8;
struct BrightStar4 { float x, y, r, phase; };
BrightStar4 brightStars4[NUM_BRIGHT_STARS4];

// ---- Flying birds -----------------------------------------------------
// Small birds crossing the distant sky. Each one gets its own speed, height,
// wing rate and vertical drift, so the group never moves as one rigid block
// -- which is the single thing that makes flocking animation look fake.
constexpr int NUM_BIRDS4 = 12;
struct Bird4 {
    float x, y, speed, scale;
    float flapPhase, flapRate;   // wing beat
    float bobPhase,  bobAmp;     // slow rise and fall along the flight path
    int   dir;                   // +1 flying right, -1 flying left
};
Bird4 birds4[NUM_BIRDS4];

// ---- Shop backdrop -----------------------------------------------------
// x is the centre of the wall, w its width, h its height above the plaza
// horizon at y = -6. The roof overhangs the wall by SHOP_EAVE_OVER4 at each
// end, so two shops only read as separate buildings if their ROOF spans clear
// each other -- that is the figure this layout is spaced on, not w.
//
//   shop      wall span          roof span          gap to the next roof
//   (corner)  -61.0 .. -49.0     -61.6 .. -48.4     19.4  (ferris wheel)
//   TOYS      -28.4 .. -17.0     -29.0 .. -16.4      2.3
//   BAKERY    -13.5 ..  -0.9     -14.1 ..  -0.3      2.3
//   GIFTS       2.6 ..  15.8       2.0 ..  16.4      2.2
//   CAFE       19.2 ..  31.2      18.6 ..  31.8      2.4
//   SKATES     34.8 ..  45.6      34.2 ..  46.2      3.2  (clock tower)
//   CIDER      50.0 ..  62.0      49.4 ..  62.6      --   (runs off frame)
//
// Two things in front of the row would otherwise sit on a name board, so the
// named shops are kept out of their reach entirely rather than nudged to a
// margin that one frame of animation can close:
//
//   * the ferris wheel sweeps x [-54.5, -29.5], and a cabin crossing a board
//     hides most of it for well over half of every revolution;
//   * the street lamp at x = -58 is solid up to y = 3.2, which is exactly the
//     height a board over an awning wants.
//
// Between them they rule out a signed shop anywhere left of about -29, so the
// far-left corner gets an unnamed end-of-terrace instead: it keeps the
// skyline from stopping dead at the wheel, and having no board there is
// nothing to obscure. The clock tower (x 47.2 .. 52.8) stands in the wide gap
// on the right, in front of CIDER's blank left end and clear of both boards.
struct Shop4 { float x, w, h; const char* sign; bool lit[5]; };
constexpr int NUM_SHOPS4 = 7;
Shop4 shops4[NUM_SHOPS4] = {
    { -22.7f, 11.4f, 17.0f, "TOYS",   {} },
    {  -7.2f, 12.6f, 21.0f, "BAKERY", {} },
    {   9.2f, 13.2f, 18.0f, "GIFTS",  {} },
    {  25.2f, 12.0f, 22.0f, "CAFE",   {} },
    {  40.2f, 10.8f, 19.0f, "SKATES", {} },
    {  56.0f, 12.0f, 18.0f, "CIDER",  {} },
    { -55.0f, 12.0f, 17.0f, nullptr,  {} }   // behind the wheel: no board
};

// ---- String lights ---------------------------------------------------
constexpr int NUM_LIGHT_SPANS4 = 5;
constexpr int BULBS_PER_SPAN4  = 8;
struct LightSpan4 { float x1, y1, x2, y2; };
// One continuous garland: each span ends where the next begins, and every
// endpoint is tied just under the eaves of the shop it belongs to, so the run
// steps up and down with the roofline instead of cutting across it.
LightSpan4 lightSpans4[NUM_LIGHT_SPANS4] = {
    { -28.4f, 10.0f, -13.5f, 14.0f },   // TOYS   -> BAKERY
    { -13.5f, 14.0f,   2.6f, 11.0f },   // BAKERY -> GIFTS
    {   2.6f, 11.0f,  19.2f, 15.0f },   // GIFTS  -> CAFE
    {  19.2f, 15.0f,  34.8f, 12.0f },   // CAFE   -> SKATES
    {  34.8f, 12.0f,  50.0f, 11.0f }    // SKATES -> CIDER
};
bool bulbLit4[NUM_LIGHT_SPANS4][BULBS_PER_SPAN4];

// ---- Fixed scene anchors --------------------------------------------------
// Declared up here (rather than beside their draw functions) because the
// lighting pass needs them before those functions appear.
constexpr float treeX4  =  0.0f;    // Christmas tree centrepiece
constexpr float clockX4 = 50.0f;    // clock tower

// ---- Market stalls ---------------------------------------------------------
// The stalls stand on the open plaza in front of the shop row, which makes
// them the nearest built thing in the scene -- so they have to read as bigger
// than the shops behind them, not smaller. stallScale4 sizes the whole stall
// from its feet up; at 1.0 the canopy came out 3.6 tall, exactly the height of
// a pedestrian, which is what made them look like toys.
//
// The x positions are spaced on the canopy width (2*StallHalfW4() = 8.06) so
// no two stalls touch, and the pairs stay clear of what shares the plaza with
// them: the ferris wheel reaches x = -30.8 at canopy height, the snowmen end
// at 22.6 and the nutcracker stands at -9.
constexpr int   NUM_STALLS4   = 4;
constexpr float stallScale4   = 1.55f;
// Dropped from -11.5: the canopy apex sits 3.6 * stallScale4 above this, which
// put it at y = -5.92 -- just over the shop row's base line at y = -6, so every
// stall roof cut a notch into the shopfront behind it.
constexpr float stallGroundY4 = -12.1f;   // where the stall meets the snow

// Canopy geometry, in world units, derived from the scale so the snow cap,
// the light pool and the name board all follow when stallScale4 changes.
inline float StallHalfW4()       { return 2.60f * stallScale4; }
inline float StallCounterTopY4() { return stallGroundY4 + 2.0f * stallScale4; }
inline float StallApexY4()       { return stallGroundY4 + 3.6f * stallScale4; }

struct Stall4 { float x; unsigned char r, g, b; const char* sign; int goods; };
Stall4 stalls4[NUM_STALLS4] = {
    // COCOA used to be held at -24 to keep its name board out of the swing of
    // a ferris-wheel cabin. The wheel has since moved to x = -52, so nothing
    // reaches this far right any more and the position is now free.
    { -24.0f, 130,  80, 170, "COCOA",     0 },
    { -15.0f, 200,  60,  60, "WREATHS",   1 },
    {  28.0f,  60, 140,  90, "CHESTNUTS", 2 },
    // Renamed from TOYS: now that every board is legible, it read as a
    // duplicate of the TOYS shop on the other side of the plaza.
    {  38.0f, 210, 160,  60, "TRINKETS",  3 }
};

// ---- Ferris wheel -- the unique detail for this scenario ------------------
float ferrisAngle4 = 0.0f;
// Front-left of the plaza, standing on the near paving just behind the main
// road. The ground line is the fixed anchor and the hub is derived from the
// radius, so changing ferrisR4 alone resizes the ride without lifting it off
// the snow or burying its feet.
// Moved well left and pushed BACK up the plaza.
//
//  The wheel used to stand at x = -28 with a 12.5 radius and its feet on
//  y = -16.5. That put it squarely over the TOYS shopfront (x -28.7 .. -16.7),
//  hiding a whole shop -- and it could not simply slide left, because at foot
//  height the road spans x -54.8 .. -38.0 and the wheel would have been
//  standing in the middle of the street.
//
//  The fix is to move it left AND back: at y = -10 the road has narrowed to
//  x -46.4 .. -35.3, so a wheel centred on -52 puts its right foot at about
//  -46.8, clear of the kerb. Standing further back also makes it smaller and
//  hazier, which is correct for the extra distance, and the only shop now
//  behind it is shops4[6] -- the one deliberately left without a name board
//  precisely because it sits behind the ride.
constexpr float ferrisCX4    = -52.0f;
constexpr float ferrisR4     =  10.5f;
constexpr float ferrisBaseY4 = -10.0f;                          // A-frame feet on the snow
constexpr float ferrisCY4    = ferrisBaseY4 + ferrisR4 + 1.2f;  // hub: lowest cabin clears the ground
// Cabins, bulbs, hub lamp and frame are all drawn at this scale, so the ride
// keeps its proportions at any radius. 9.0 is the radius the parts below were
// originally drawn against.
constexpr float ferrisScale4 = ferrisR4 / 9.0f;
constexpr int   NUM_CABINS4 = 8;

// ---- Ice rink + skaters ------------------------------------------------
constexpr float rinkCX4 = 10.0f, rinkCY4 = -14.5f;
struct Skater4 { float angle, speed, radiusX, radiusY; unsigned char r, g, b; };
// Five skaters on deliberately different radii and speeds so they read as
// individuals rather than one ellipse: beginners hug the outside slowly,
// the fast skater takes a tight inside line.
constexpr int NUM_SKATERS4 = 5;
// Skaters 3 and 4 deliberately share a radius and a speed with a small angle
// offset between them, so they travel side by side and can hold hands -- five
// independent ellipses never meet, and a rink where nobody interacts reads as
// five separate animations rather than one crowd.
Skater4 skaters4[NUM_SKATERS4] = {
    { 0.0f,  0.013f, 5.1f, 1.95f, 210, 70,  90  },  // beginner, wide + slow
    { 3.14f, 0.017f, 4.2f, 1.60f,  70, 110, 200 },  // the one who falls over
    { 1.6f,  0.030f, 2.4f, 0.95f, 240, 200,  60 },  // fast, tight line
    { 4.4f,  0.019f, 3.6f, 1.38f, 120, 200, 140 },  // pair, inside partner
    { 4.68f, 0.019f, 3.6f, 1.38f, 200, 130, 220 }   // pair, outside partner
};

// The blue skater takes a tumble every so often: 0 = upright, then a short
// fall, a beat sitting on the ice, and a scramble back up.
float skaterFallT4     = 0.0f;
int   skaterFallCool4  = 420;
constexpr int FALLING_SKATER4 = 1;

// ---- Horse-drawn sled --------------------------------------------------
float sledX4        = -80.0f;
bool  sledActive4    = false;
int   sledCooldown4  = 300;

// ---- Fire pit ------------------------------------------------------
constexpr float fireX4 = -5.0f, fireY4 = -10.5f;
float firePhase4 = 0.0f;

// ---- Chimney smoke / stall steam (shared particle type) --------------
struct SmokePuff4 { float x, y, vy, alpha, size; bool active; };
constexpr int MAX_SMOKE4 = 30;
SmokePuff4 chimneySmoke4[MAX_SMOKE4];
constexpr int MAX_STEAM4 = 30;
SmokePuff4 stallSteam4[MAX_STEAM4];

// ---- Snow particles ----------------------------------------------------
constexpr int MAX_SNOW4 = 300;
float snowX4[MAX_SNOW4], snowY4[MAX_SNOW4], snowDrift4[MAX_SNOW4], snowSize4[MAX_SNOW4];

// ---- Pedestrians -----------------------------------------------------
struct Ped4 { float x, y, speed, phase; int dir; unsigned char coatR, coatG, coatB; };
constexpr int NUM_PEDS4 = 7;
// Spread across the depth of the plaza rather than standing on one line.
Ped4 peds4[NUM_PEDS4] = {
    { -30.0f, -10.4f, 0.05f, 0.0f,  1, 150,  40,  50 },
    {  -5.0f, -13.6f, 0.07f, 1.0f, -1,  40,  60, 120 },
    {  20.0f, -10.8f, 0.04f, 2.0f,  1,  90,  90,  90 },
    {  35.0f, -12.4f, 0.06f, 3.0f, -1, 120,  70,  40 },
    // Was y = -10.2: at that depth the hat tip reached y = -7.38, half a
    // whisker above the shopfront sills at -7.4, so this walker clipped the
    // bottom of every front it passed.
    { -45.0f, -10.9f, 0.05f, 4.0f,  1, 200, 170,  60 },
    {  10.0f, -13.9f, 0.055f,5.0f,  1, 180,  60, 130 },
    { -18.0f, -12.9f, 0.065f,6.0f, -1,  60, 130, 110 }
};

// ---- Flying sleigh -- the signature feature for this scenario -------------
float sleighX4          = -100.0f;
float sleighY4          = 27.0f;
// Was 0.6, which put the whole team and sleigh inside about six world units --
// a smudge crossing a very large sky. At 1.05 it is roughly eleven units nose
// to tail, big enough to read as Santa at a glance without dominating the
// frame or dropping into the shop roofline.
float sleighScale4      = 1.05f;
float sleighTrailPhase4 = 0.0f;

// ---- Metro rail animation state -----------------------------------------
// `metroCarX4` is the horizontal centre of the passing carriage. The rail
// will trigger after `metroNextStart4` seconds and then move across the
// scene; it repeats on a longer interval after the first pass.
float metroCarX4      = -200.0f;
float metroTime4      = 0.0f;
float metroNextStart4 = 5.0f;   // first pass at 5 seconds
bool  metroActive4    = false;
float metroSpeed4     = 1.8f;   // world units per update tick

// ---- Clock tower pendulum ------------------------------------------------
float pendulumAngle4 = 0.0f;

// ---- Fireworks -----------------------------------------------------------
// Staged: a rocket climbs, hangs for a beat, then bursts. Previously bursts
// simply appeared in empty sky with no launch.
enum FireworkState4 { FW_INACTIVE, FW_RISING, FW_BURSTING };
struct Firework4 {
    float x, y;            // current position
    float targetY;         // apex where it bursts
    float phase;           // 0..1 burst progress
    FireworkState4 state;
    unsigned char r, g, b;
    float sparkAng[18];    // per-spark direction, so bursts differ
    float sparkSpd[18];
};
constexpr int MAX_FIREWORKS4 = 3;
Firework4 fireworks4[MAX_FIREWORKS4];
int fireworkCooldown4 = 200;

// ---- Snow accumulation ---------------------------------------------------
// Eases toward a target set by the 1/2/3 keys, so settled snow thickens or
// melts back gradually instead of snapping. (snowCover4 itself is declared up
// with the season helpers, which need it.)

inline float SnowCoverTarget4() {
    if (snowIntensity4 == 0) return 0.20f;
    if (snowIntensity4 == 1) return 0.55f;
    return 0.90f;
}

// ---- Fog / snow-squall mode (toggled with 'F') ----------------------------
// A drifting translucent band that dims whatever is behind it and makes every
// ground glow bloom. It also gives the aurora something to contrast against.
bool  fogMode4  = false;
float fogPhase4 = 0.0f;
float fogAmount4 = 0.0f;      // eases in and out so the toggle is not a jump

// ---- Aurora / northern lights (rare sky effect) ---------------------------
bool  auroraActive4   = false;
float auroraPhase4    = 0.0f;
float auroraTimer4    = 0.0f;
int   auroraCooldown4 = 600;

// ============================================================================
//  SMALL HELPERS
// ============================================================================
void FilledCircle4(float xc, float yc, float radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    glColor4ub(r, g, b, a);
    glBegin(GL_POLYGON);
    const int seg = 20;
    for (int i = 0; i < seg; i++) {
        float ang = (float)i / seg * 2.0f * PI4;
        glVertex2f(xc + radius * cos(ang), yc + radius * sin(ang));
    }
    glEnd();
}

// ============================================================================
//  SKY / STARS / SHOPS / STRING LIGHTS
// ============================================================================
// A single place to ask "what colour is the air at height y right now?".
// The distant buildings, the road haze and the atmospheric fade on the shop
// row all wash toward this, so every layer of the scene agrees on where the
// horizon sits and how thick the air in front of it is.
inline void SkyAirColour4(float y, float& r, float& g, float& b) {
    float t = (y + 6.0f) / 46.0f;            // 0 at the horizon, 1 at the zenith
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    r = MixF4(44.0f - 30.0f * t, 216.0f - 150.0f * t);
    g = MixF4(50.0f - 34.0f * t, 230.0f - 100.0f * t);
    b = MixF4(80.0f - 52.0f * t, 244.0f -  30.0f * t);
}

// Blends a surface colour toward the air in front of it. `depth` is 0 for
// something at arm's length and 1 for something on the horizon: the further
// back a mass stands, the more air is between it and the camera, so it loses
// contrast and saturation rather than simply getting darker. This is the one
// cue that separates the three building rows from each other.
inline void AirFade4(float depth, float atY,
                     int nr, int ng, int nb, int dr, int dg, int db,
                     unsigned char& outR, unsigned char& outG, unsigned char& outB) {
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

// A smooth radial falloff, as one triangle fan with a bright centre and a
// fully transparent rim. DrawSoftEllipse() stacks 24-sided polygons, which is
// invisible at the size of a footprint shadow and glaringly obvious at the
// size of a moon halo -- this is the version for anything large.
inline void DrawRadialGlow4(float cx, float cy, float rx, float ry,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char centreA) {
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

// One drifting bank of haze: a horizontal band whose alpha rises to a peak in
// the middle and falls to nothing at both ends and both edges, so it has no
// boundary anywhere. Built from quad strips rather than stacked ellipses for
// exactly the same reason DrawRadialGlow4 exists.
inline void DrawHazeBank4(float cx, float cy, float rx, float ry,
                          unsigned char r, unsigned char g, unsigned char b,
                          float peakA) {
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

void DrawSky4() {
    float dayBlend = dayT4;

    // ---- Vertical gradient -------------------------------------------------
    // Five stops rather than two. A real sky loses most of its depth in the
    // top third and then flattens out toward the horizon, which a single
    // linear ramp between two colours cannot do -- that flat ramp is what
    // made the old backdrop read as a painted board. Every stop is a
    // night/day pair, so the whole gradient crossfades with the 4 / 5 keys.
    const int   NSTOP = 6;
    const float stopY [NSTOP]    = { 40.0f, 30.0f, 21.0f, 13.0f, 4.0f, -6.0f };
    // Two sets of stops per time of day, one per season: winter skies stay
    // blue and hard, autumn skies go warm and dusty, and both crossfade with
    // the 4 / 5 keys exactly as before.
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

    // ---- Sideways variation -----------------------------------------------
    // A sky that is identical at every x still reads as wallpaper. The light
    // source sits up and to the right (moon by night, sun by day), so the air
    // on that side is a touch brighter and warmer.
    glBegin(GL_QUADS);
        glColor4ub(MixB4(70, 255), MixB4(86, 240), MixB4(126, 214), MixB4(30, 46));
        glVertex2f(60.0f, 40.0f); glVertex2f(60.0f, -6.0f);
        glColor4ub(MixB4(70, 255), MixB4(86, 240), MixB4(126, 214), 0);
        glVertex2f(-6.0f, -6.0f); glVertex2f(-6.0f, 40.0f);
    glEnd();

    // ---- Haze banks --------------------------------------------------------
    // Long, very soft horizontal smears drifting at different rates. They are
    // deliberately far too faint to read as "clouds"; their job is to break up
    // the flat gradient so the eye finds something to settle on.
    for (int i = 0; i < 6; i++) {
        float span  = 260.0f;
        float drift = fmodf(twinklePhase4 * (0.22f + i * 0.09f) + i * 43.0f, span) - span * 0.5f;
        float cy    = 33.0f - i * 5.4f;
        float rx    = 24.0f + i * 7.0f;
        float ry    = 1.6f  + i * 0.7f;
        float a     = MixB4(20, 40) * (1.0f - i * 0.09f);
        unsigned char cr = MixB4(92, 244), cg = MixB4(108, 248), cb = MixB4(148, 252);
        // Two copies a full span apart, so a bank never pops in at the edge
        // of the frame -- one is always sliding in as the other slides out.
        DrawHazeBank4(drift,        cy, rx, ry, cr, cg, cb, a);
        DrawHazeBank4(drift - span, cy, rx, ry, cr, cg, cb, a);
    }

    // Warm city glow sitting on the horizon: cold night above, distant
    // sodium-lit town below, which gives the sky somewhere to end.
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

// ---- Moon: the scene's cool light source ---------------------------------
constexpr float moonX4 = 34.0f, moonY4 = 27.0f, moonR4 = 3.1f;

void DrawMoon4() {
    float nightFade = NightT4();
    if (dayT4 > 0.05f) {
        float sunA = dayT4 * 255.0f;
        // Wide, very soft daylight bloom: the sun should light the air around
        // it, not sit on the sky as a flat yellow disc.
        // Radius grows quadratically while alpha falls off smoothly; equal
            // steps in both produced a visible set of concentric rings.
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

    // Atmospheric illumination: a very wide disc of faintly lit air, then a
    // tighter halo on top of it. Both are smooth radial falloffs -- the old
    // stack of twelve 20-sided circles put visible rings round the moon.
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 8.5f, moonR4 * 8.5f,
                    150, 178, 232, (unsigned char)(40 * nightFade));
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 3.6f, moonR4 * 3.6f,
                    196, 216, 248, (unsigned char)(70 * nightFade));
    DrawRadialGlow4(moonX4, moonY4, moonR4 * 1.85f, moonR4 * 1.85f,
                    226, 238, 254, (unsigned char)(105 * nightFade));

    FilledCircle4(moonX4, moonY4, moonR4, 238, 243, 252, (unsigned char)(255 * nightFade));
    // The limb picks up a touch less light than the centre, which is what
    // keeps the disc from reading as a paper cut-out.
    FilledCircle4(moonX4 + moonR4 * 0.16f, moonY4 + moonR4 * 0.14f, moonR4 * 0.82f,
                  248, 251, 255, (unsigned char)(190 * nightFade));
    // Craters
    FilledCircle4(moonX4 - 0.9f, moonY4 + 0.7f, 0.62f, 214, 222, 236, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 + 1.0f, moonY4 - 0.5f, 0.45f, 218, 226, 239, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 + 0.2f, moonY4 + 1.5f, 0.30f, 220, 228, 241, (unsigned char)(235 * nightFade));
    FilledCircle4(moonX4 - 1.3f, moonY4 - 1.2f, 0.26f, 220, 228, 241, (unsigned char)(235 * nightFade));
}

void DrawStars4() {
    float night = NightT4();
    if (night <= 0.02f) return;      // daylight washes the field out entirely

    // glPointSize cannot change inside a glBegin block, so the field is drawn
    // in three passes bucketed by size rather than one pass of equal dots.
    const float sizes[3] = { 1.1f, 1.8f, 2.7f };
    for (int pass = 0; pass < 3; pass++) {
        glPointSize(sizes[pass]);
        glBegin(GL_POINTS);
        for (int i = 0; i < NUM_STARS4; i++) {
            const Star4& st = stars4[i];
            if (st.bucket != pass) continue;
            float tw = 0.55f + 0.45f * sinf(twinklePhase4 * st.rate + st.twinkle);
            // Stars thin out into the horizon haze rather than stopping dead
            // at the top of the rooflines.
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

    // The bright few, each with a halo and a faint cross flare.
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

// ============================================================================
//  FLYING BIRDS -- distant silhouettes crossing the sky
// ----------------------------------------------------------------------------
//  Drawn after the sky and before the skyline, so the rooflines cut them off
//  and they stay where they belong: in the air behind the town, never over
//  the market. Each bird is a shallow "M" whose wing tips rise and fall; at
//  this size that is all a flapping bird needs to be.
// ============================================================================
void RespawnBird4(Bird4& b, float atX) {
    b.x         = atX;
    b.y         = 12.0f + (rand() % 210) / 10.0f;        // 12 .. 33
    b.speed     = 0.10f + (rand() % 22) / 100.0f;
    b.scale     = 0.55f + (rand() % 70) / 100.0f;
    b.flapRate  = 0.16f + (rand() % 16) / 100.0f;
    b.flapPhase = (rand() % 628) / 100.0f;
    b.bobPhase  = (rand() % 628) / 100.0f;
    b.bobAmp    = 0.25f + (rand() % 60) / 100.0f;
}

void DrawBirds4() {
    // By day they are legible dark-grey silhouettes against a bright sky. By
    // night they drop to a faint shade barely separated from the air, which is
    // all a bird would be at that distance -- solid black shapes on a night
    // sky read as confetti, not wildlife.
    // At night they are a shade LIGHTER than the sky, not darker: at this
    // distance what you actually see is moonlight catching the underside of a
    // wing. A near-black bird on a near-black sky is simply not there.
    unsigned char br = MixB4(112, 48), bg = MixB4(124, 52), bb = MixB4(152, 62);
    unsigned char alpha = MixB4(120, 215);

    glLineWidth(1.4f);
    for (int i = 0; i < NUM_BIRDS4; i++) {
        const Bird4& bd = birds4[i];
        float sp   = bd.scale;
        float y    = bd.y + sinf(bd.bobPhase) * bd.bobAmp;
        float flap = sinf(bd.flapPhase);                  // -1 .. 1
        float up   = 0.30f + 0.70f * (0.5f + 0.5f * flap);// wing tip height
        // Distant birds are fainter as well as smaller: one more depth cue
        // for almost no cost.
        unsigned char a = (unsigned char)(alpha * (0.45f + 0.55f * sp));

        glColor4ub(br, bg, bb, a);
        glBegin(GL_LINE_STRIP);
            glVertex2f(bd.x - 1.20f * sp, y + up * 0.80f * sp);
            glVertex2f(bd.x - 0.50f * sp, y + (up * 0.16f - 0.06f) * sp);
            glVertex2f(bd.x,              y + 0.10f * sp);
            glVertex2f(bd.x + 0.50f * sp, y + (up * 0.16f - 0.06f) * sp);
            glVertex2f(bd.x + 1.20f * sp, y + up * 0.80f * sp);
        glEnd();
        // A short body stub ahead of the wings, so the shape has a direction
        // of travel instead of being a symmetrical tick mark.
        glBegin(GL_LINES);
            glVertex2f(bd.x, y + 0.10f * sp);
            glVertex2f(bd.x + 0.34f * sp * bd.dir, y + 0.14f * sp);
        glEnd();
    }
    glLineWidth(1.0f);
}

void UpdateBirds4(int) {
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

// ============================================================================
//  FLYING SLEIGH -- signature feature: Santa's sleigh and reindeer, always
//  silhouetted against the night sky with a sparkling trail behind it.
// ============================================================================
void DrawSleigh4() {
    glPushMatrix();
    glTranslatef(sleighX4, sleighY4, 0.0f);
    glScalef(sleighScale4, sleighScale4, 1.0f);
    glColor3ub(15, 15, 22);

    // Three reindeer, in a line ahead of the sleigh
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
    // Traces linking each reindeer back to the sleigh -- previously the team
    // and the sleigh were simply adjacent with nothing connecting them.
    glColor3ub(120, 85, 50);
    glLineWidth(1.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(1.2f, 0.6f);
        glVertex2f(-1.0f, 0.6f);
        glVertex2f(-3.2f, 0.6f);
        glVertex2f(-5.4f, 0.6f);
        glVertex2f(-6.2f, 0.7f);
    glEnd();

    // Rudolph leads: red nose plus a glow that lights the air ahead.
    FilledCircle4(1.55f, 1.12f, 0.55f, 255, 70, 60, 60);
    FilledCircle4(1.55f, 1.12f, 0.28f, 255, 90, 70, 130);
    FilledCircle4(1.55f, 1.12f, 0.15f, 255, 160, 140, 255);

    // Sleigh body
    glColor3ub(15, 15, 22);
    glBegin(GL_POLYGON);
        glVertex2f(-8.0f, 0.3f); glVertex2f(-6.0f, 0.3f);
        glVertex2f(-6.0f, 1.0f); glVertex2f(-7.0f, 1.0f);
        glVertex2f(-8.5f, 0.6f);
    glEnd();
    // Sparkling trail
    for (int i = 0; i < 6; i++) {
        float t = fmodf(sleighTrailPhase4 + i*0.3f, 1.0f);
        float tx = -8.0f - t*6.0f;
        float ty = 0.6f + sinf(t*10.0f)*0.3f;
        FilledCircle4(tx, ty, 0.15f*(1.0f-t), 255, 240, 200, (unsigned char)(220*(1.0f-t)));
    }
    glPopMatrix();
}

void UpdateSleigh4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSleigh4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        sleighX4 += 0.25f;
        sleighTrailPhase4 += 0.05f;
        if (sleighX4 > 100.0f) {
            sleighX4 = -100.0f - (rand() % 30);
            // Flies a little higher than before, because at the larger size
            // the reindeer team would otherwise graze the tallest shop roof
            // (the CAFE eave reaches y = +16).
            sleighY4 = 25.0f + (rand() % 9);
        }
    }
    glutTimerFunc(30, UpdateSleigh4, 0);

}

void UpdateMetroRail4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(60, UpdateMetroRail4, 0); return; }
    // Advance global time (approx seconds per 60ms tick)
    if (isAnimating4) metroTime4 += 0.06f;

    if (!metroActive4) {
        if (metroTime4 >= metroNextStart4) {
            metroActive4 = true;
            metroCarX4 = -80.0f; // start well off-screen left
        }
    } else {
        metroCarX4 += metroSpeed4; // move rightwards
        if (metroCarX4 > 80.0f) {
            metroActive4 = false;
            // schedule next pass in ~20 seconds
            metroNextStart4 = metroTime4 + 20.0f;
        }
    }
    glutPostRedisplay();
    glutTimerFunc(60, UpdateMetroRail4, 0);
}

// ---- Shop facade ---------------------------------------------------------
// The row is the backdrop the whole market stands against, so each building
// is drawn the way one actually stacks, from the street up: glazed shopfront
// and door, awning, name board, two storeys of windows, then the gable and
// the snow on it. Every dimension is a fraction of the entry's w and h, so
// resizing a shop in shops4[] rescales the whole facade instead of stretching
// one band of it out of proportion.
constexpr float SHOP_BASE_Y4    = -6.0f;   // the row stands on the plaza horizon
constexpr float SHOP_EAVE_OVER4 =  0.6f;   // roof overhang past each end of the wall

inline float ShopEaveY4(const Shop4& s)    { return SHOP_BASE_Y4 + s.h; }
inline float ShopRoofRise4(const Shop4& s) { return 2.1f + s.w * 0.10f; }

// Height of the roof surface above world x. The chimney and its smoke use it
// so they sit on the slope instead of floating at the eave line.
inline float ShopRoofY4(const Shop4& s, float x) {
    float half = s.w * 0.5f + SHOP_EAVE_OVER4;
    float t    = fabsf(x - s.x) / half;
    if (t > 1.0f) t = 1.0f;
    return ShopEaveY4(s) + ShopRoofRise4(s) * (1.0f - t);
}

// One storey of windows. `shift` rotates which of the five lit[] flags each
// window takes, so the two rows light up in different patterns from a single
// five-bit state instead of every floor matching.
void DrawShopWindowRow4(const Shop4& s, float y, float wh, int shift) {
    const int   n    = 5;
    const float slot = s.w / (float)(n + 1);
    const float ww   = slot * 0.52f;
    for (int i = 0; i < n; i++) {
        float wx = s.x - s.w*0.5f + slot*(i+1) - ww*0.5f;
        bool  on = s.lit[(i + shift) % 5];
        if (on) {
            // A lit room throws light onto the wall around its opening --
            // without this the windows read as stickers on a flat board.
            glColor4ub(255, 196, 110, (unsigned char)(38.0f * (0.35f + 0.65f * NightT4())));
            glBegin(GL_QUADS);
                glVertex2f(wx-0.7f,    y-0.7f);     glVertex2f(wx+ww+0.7f, y-0.7f);
                glVertex2f(wx+ww+0.7f, y+wh+0.7f);  glVertex2f(wx-0.7f,    y+wh+0.7f);
            glEnd();
            glColor3ub(255, 210, 134);
        } else {
            // Unlit glass: near-black at night, a cool reflection of the sky
            // by day. Leaving it black through a daylight frame is what made
            // the row look like it had been cut out of the night sky.
            glColor3ub(MixB4(17, 74), MixB4(15, 88), MixB4(25, 112));
        }
        glBegin(GL_QUADS);
            glVertex2f(wx,    y);      glVertex2f(wx+ww, y);
            glVertex2f(wx+ww, y+wh);   glVertex2f(wx,    y+wh);
        glEnd();
        // Frame and glazing bars. At this size they are what makes the
        // opening read as a sash window rather than a yellow rectangle.
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
        // Sill, so each storey has a horizontal line to sit on
        glColor3ub(MixB4(96, 196), MixB4(88, 188), MixB4(96, 190));
        glBegin(GL_QUADS);
            glVertex2f(wx-0.25f, y-0.30f); glVertex2f(wx+ww+0.25f, y-0.30f);
            glVertex2f(wx+ww+0.25f, y);    glVertex2f(wx-0.25f,    y);
        glEnd();
        glLineWidth(1.0f);
    }
}

void DrawShop4(const Shop4& s) {
    const float baseY = SHOP_BASE_Y4;
    const float halfW = s.w * 0.5f;
    const float eaveY = ShopEaveY4(s);
    const float rise  = ShopRoofRise4(s);
    const float over  = SHOP_EAVE_OVER4;

    // ---- Receding side wall ------------------------------------------------
    // The row used to be seven flat boards standing edge-on to the camera. A
    // building only reads as a solid once you can see round one corner of it,
    // so each shop now shows the flank that faces the middle of the frame,
    // thrown toward the vanishing point at x = 0. Shops far out to the sides
    // show a wide flank, shops near the centre almost none -- which is what
    // single-point perspective actually does, and it costs one quad.
    //
    // The gaps between roof spans are 2.2 units at the tightest, and sideW is
    // capped at 2.0, so a flank can never reach under its neighbour's wall.
    {
        float dir  = (s.x < 0.0f) ? 1.0f : -1.0f;
        float fx   = s.x + dir * halfW;                  // the corner we see round
        float sideW = fabsf(fx) * 0.030f;
        if (sideW > 2.0f)  sideW = 2.0f;
        if (sideW > 0.22f) {
            float k       = sideW / (fabsf(fx) + 0.001f);
            float backEave = eaveY + (baseY - eaveY) * k;   // converges on the horizon
            // The light is up and to the right in both day and night, so a
            // right-facing flank is lit and a left-facing one is in shade.
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
            // Snow on the flank's own eave, following the same convergence.
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

    // ---- Wall -------------------------------------------------------------
    // Graded dark-to-light going up: a single flat fill on a wall this tall
    // reads as a hole cut in the sky rather than as masonry. The pair of
    // colours is a night/day blend now, so by daylight the row is warm plaster
    // under a blue sky instead of the same midnight browns lit from nowhere.
    glBegin(GL_QUADS);
        glColor3ub(MixB4(29, 118), MixB4(25, 110), MixB4(37, 116));
        glVertex2f(s.x-halfW, baseY);  glVertex2f(s.x+halfW, baseY);
        glColor3ub(MixB4(48, 168), MixB4(42, 160), MixB4(60, 164));
        glVertex2f(s.x+halfW, eaveY);  glVertex2f(s.x-halfW, eaveY);
    glEnd();

    // ---- Directional shading on the front face ------------------------------
    // Three washes, all keyed to the same light: a lit band down the right of
    // the wall, a shadow band down the left, and the shadow the roof overhang
    // drops onto the top of the wall. Together they are what stops the facade
    // from reading as a printed panel.
    glBegin(GL_QUADS);                                  // lit edge, from the right
        glColor4ub(255, MixB4(226, 244), MixB4(190, 226), (unsigned char)(MixB4(30, 54)));
        glVertex2f(s.x+halfW, baseY); glVertex2f(s.x+halfW, eaveY);
        glColor4ub(255, MixB4(226, 244), MixB4(190, 226), 0);
        glVertex2f(s.x+halfW-s.w*0.34f, eaveY); glVertex2f(s.x+halfW-s.w*0.34f, baseY);
    glEnd();
    glBegin(GL_QUADS);                                  // shaded edge, on the left
        glColor4ub(MixB4(4, 40), MixB4(6, 48), MixB4(14, 70), (unsigned char)(MixB4(120, 86)));
        glVertex2f(s.x-halfW, baseY); glVertex2f(s.x-halfW, eaveY);
        glColor4ub(MixB4(4, 40), MixB4(6, 48), MixB4(14, 70), 0);
        glVertex2f(s.x-halfW+s.w*0.40f, eaveY); glVertex2f(s.x-halfW+s.w*0.40f, baseY);
    glEnd();
    glBegin(GL_QUADS);                                  // roof overhang shadow
        glColor4ub(MixB4(4, 38), MixB4(6, 44), MixB4(14, 62), (unsigned char)(MixB4(135, 105)));
        glVertex2f(s.x-halfW, eaveY); glVertex2f(s.x+halfW, eaveY);
        glColor4ub(MixB4(4, 38), MixB4(6, 44), MixB4(14, 62), 0);
        glVertex2f(s.x+halfW, eaveY-2.6f); glVertex2f(s.x-halfW, eaveY-2.6f);
    glEnd();
    glBegin(GL_QUADS);                                  // ambient occlusion at the foot
        glColor4ub(MixB4(4, 44), MixB4(6, 50), MixB4(14, 68), (unsigned char)(MixB4(105, 72)));
        glVertex2f(s.x-halfW, baseY); glVertex2f(s.x+halfW, baseY);
        glColor4ub(MixB4(4, 44), MixB4(6, 50), MixB4(14, 68), 0);
        glVertex2f(s.x+halfW, baseY+1.8f); glVertex2f(s.x-halfW, baseY+1.8f);
    glEnd();

    // Corner pilasters. Neighbouring shops are only ~2.6 apart, so without a
    // lit edge on each end the row merges back into one dark terrace.
    glColor3ub(MixB4(64, 186), MixB4(56, 178), MixB4(74, 182));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW,       baseY); glVertex2f(s.x-halfW+0.45f, baseY);
        glVertex2f(s.x-halfW+0.45f, eaveY); glVertex2f(s.x-halfW,       eaveY);
        glVertex2f(s.x+halfW-0.45f, baseY); glVertex2f(s.x+halfW,       baseY);
        glVertex2f(s.x+halfW,       eaveY); glVertex2f(s.x+halfW-0.45f, eaveY);
    glEnd();

    // ---- Upper storeys ----------------------------------------------------
    float winH = s.h * 0.125f;
    DrawShopWindowRow4(s, baseY + s.h * 0.50f, winH, 0);
    DrawShopWindowRow4(s, baseY + s.h * 0.72f, winH, 2);

    // Storey band dividing the shopfront from the rooms above it
    float bandY = baseY + s.h * 0.41f;
    glColor3ub(MixB4(66, 176), MixB4(58, 168), MixB4(76, 172));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW, bandY);       glVertex2f(s.x+halfW, bandY);
        glVertex2f(s.x+halfW, bandY+0.4f);  glVertex2f(s.x-halfW, bandY+0.4f);
    glEnd();

    // ---- Street level -----------------------------------------------------
    // The glazed front and its door are the part that says "shop": a tall
    // wall of windows on its own would just be a tenement.
    float frontTop = baseY + s.h * 0.26f;
    float frontIn  = halfW - 0.9f;
    float doorW    = s.w * 0.16f;
    float glassL   = s.x - frontIn;
    float glassR   = s.x + frontIn - doorW - 0.7f;

    glColor4ub(255, 198, 118, (unsigned char)(66.0f * (0.30f + 0.70f * NightT4())));  // warm spill onto the paving
    glBegin(GL_QUADS);
        glVertex2f(glassL-1.2f, baseY-1.4f); glVertex2f(s.x+frontIn+1.2f, baseY-1.4f);
        glVertex2f(s.x+frontIn+1.2f, frontTop+0.6f); glVertex2f(glassL-1.2f, frontTop+0.6f);
    glEnd();

    glColor3ub(255, 214, 148);                      // the glazing
    glBegin(GL_QUADS);
        glVertex2f(glassL, baseY+0.35f); glVertex2f(glassR, baseY+0.35f);
        glVertex2f(glassR, frontTop);    glVertex2f(glassL, frontTop);
    glEnd();

    // Shoppers moving about inside, seen as silhouettes through the glass
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

    // Mullions, drawn over the silhouettes so the glass stays in front
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

    // Door, with a lit fanlight and a wreath on it
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

    // ---- Awning -----------------------------------------------------------
    // Striped, in a colour taken from the shop's index so the row does not
    // repeat, and scalloped along its lower edge.
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
        glBegin(GL_TRIANGLES);              // scallop hanging below the awning
            glVertex2f(x0, awnY); glVertex2f(x1, awnY);
            glVertex2f((x0+x1)*0.5f, awnY-0.45f);
        glEnd();
    }
    glColor3ub(46, 40, 52);                 // the rail the awning hangs from
    glBegin(GL_QUADS);
        glVertex2f(s.x-awnHalf, awnY+awnH);       glVertex2f(s.x+awnHalf, awnY+awnH);
        glVertex2f(s.x+awnHalf, awnY+awnH+0.28f); glVertex2f(s.x-awnHalf, awnY+awnH+0.28f);
    glEnd();

    // ---- Name board -------------------------------------------------------
    // The board is sized from the text it has to hold, not from the shop, so
    // a long name can never run past its own frame or off the side of the
    // building the way a fixed fraction of s.w did.
    if (s.sign != nullptr) {
        const char* name = s.sign;
        float ty    = awnY + awnH + 0.85f;
        float signH = 2.2f;
        float txtW  = TextPixelWidth(GLUT_BITMAP_HELVETICA_12, name)
                    * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        float half  = txtW * 0.5f + 1.4f;

        glColor4ub(255, 198, 112, 52);      // the board's own light on the wall
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
        // Brackets tying the board back to the wall
        glBegin(GL_LINES);
            glVertex2f(s.x-half, ty+signH); glVertex2f(s.x-half-0.9f, ty+signH+0.9f);
            glVertex2f(s.x+half, ty+signH); glVertex2f(s.x+half+0.9f, ty+signH+0.9f);
        glEnd();
        glLineWidth(1.0f);
        // The name itself, one point larger than the stall labels in front of
        // it so the backdrop row stays legible behind the crowd.
        glColor3ub(255, 232, 178);
        DrawTextCentered(s.x, ty + signH*0.5f - 0.55f, GLUT_BITMAP_HELVETICA_12, name);
    }

    // ---- Roof -------------------------------------------------------------
    // Dark fascia under the eaves first, so the wall and the snow cap that
    // DrawSnowAccumulation4 lays along the eave line never meet directly.
    glColor3ub(MixB4(38, 82), MixB4(33, 76), MixB4(48, 88));
    glBegin(GL_QUADS);
        glVertex2f(s.x-halfW-over, eaveY-0.6f); glVertex2f(s.x+halfW+over, eaveY-0.6f);
        glVertex2f(s.x+halfW+over, eaveY);      glVertex2f(s.x-halfW-over, eaveY);
    glEnd();
    // Gable. The rise scales with the width, so a wide shop gets a roof in
    // proportion instead of the same small cap a narrow one wears. The two
    // slopes are drawn separately because they face opposite ways: with the
    // light up and to the right, one of them catches it and one does not, and
    // a single flat triangle across both is the fastest way to lose the roof's
    // shape entirely.
    glColor3ub(MixB4(40, 104), MixB4(35, 96), MixB4(50, 108));      // shaded slope
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x-halfW-over, eaveY);
        glVertex2f(s.x,            eaveY);
        glVertex2f(s.x,            eaveY+rise);
    glEnd();
    glColor3ub(MixB4(66, 150), MixB4(58, 142), MixB4(80, 150));      // lit slope
    glBegin(GL_TRIANGLES);
        glVertex2f(s.x,            eaveY);
        glVertex2f(s.x+halfW+over, eaveY);
        glVertex2f(s.x,            eaveY+rise);
    glEnd();
    // Snow lying on both slopes, again split: settled snow in shade goes blue,
    // snow facing the light stays near white.
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
    // Attic light in the gable, lit whenever the middle window is
    if (s.lit[2]) FilledCircle4(s.x, eaveY + rise*0.42f, 0.45f, 255, 206, 128, 255);
    else          FilledCircle4(s.x, eaveY + rise*0.42f, 0.45f,  26,  23,  34, 255);
}

void DrawShops4() { for (int i = 0; i < NUM_SHOPS4; i++) DrawShop4(shops4[i]); }

// The chimney stands on GIFTS. Now that the roof is a gable rather than a
// flat top, its foot has to follow the slope at that x or it hangs in the air
// off the side of the ridge.
inline float ChimneyX4() { return shops4[1].x + shops4[1].w * 0.26f; }

void DrawChimney4() {
    float x    = ChimneyX4();
    float foot = ShopRoofY4(shops4[1], x) - 0.3f;       // buried a little in the slope
    float top  = foot + 2.8f;
    glColor3ub(58, 52, 62);
    glBegin(GL_QUADS);
        glVertex2f(x-0.7f, foot); glVertex2f(x+0.7f, foot);
        glVertex2f(x+0.7f, top);  glVertex2f(x-0.7f, top);
    glEnd();
    glColor3ub(74, 66, 78);                             // capping course
    glBegin(GL_QUADS);
        glVertex2f(x-0.95f, top);       glVertex2f(x+0.95f, top);
        glVertex2f(x+0.95f, top+0.35f); glVertex2f(x-0.95f, top+0.35f);
    glEnd();
    glColor3ub(238, 243, 252);                          // snow on the cap
    glBegin(GL_QUADS);
        glVertex2f(x-0.95f, top+0.35f); glVertex2f(x+0.95f, top+0.35f);
        glVertex2f(x+0.75f, top+0.75f); glVertex2f(x-0.75f, top+0.75f);
    glEnd();
}

void DrawLightSpan4(const LightSpan4& span, int spanIdx) {
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

void DrawLightSpans4() {
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

// ---- Distant skyline ------------------------------------------------------
//  Three rows of town standing behind the market, each one further off than
//  the last. What separates them is not just size: every row is faded toward
//  the air in front of it by AirFade4(), so the back row is pale and low
//  contrast and the front row is dark and crisp. That -- plus the fact that
//  each block shows a receding side face -- is what turns the old flat
//  silhouette band into something with depth in it.
//
//  All of it is day/night aware. By day the blocks are cool grey masonry
//  under a bright sky; by night they are dark but never black, with a
//  scattering of lit windows so the town still reads as inhabited.

// Deterministic "is this window lit" so the pattern is stable frame to frame
// (rand() here would make the whole town flicker) but still looks scattered.
inline bool FarWindowLit4(int seed, int col, int row) {
    int h = seed * 7919 + col * 733 + row * 197;
    h ^= (h >> 7);
    return (h % 100) < 34;
}

// One mass of building. `depth` 0 = right behind the shops, 1 = on the
// horizon. Everything else -- colour, window contrast, whether it gets a
// side face at all -- falls out of that one number.
void DrawFarBlock4(float cx, float w, float h, float depth, float baseY,
                   int seed, bool pitchedRoof) {
    float half = w * 0.5f;
    float topY = baseY + h;

    unsigned char botR, botG, botB, topR, topG, topB;
    AirFade4(depth, baseY, 24, 26, 40,  78,  90, 116, botR, botG, botB);
    AirFade4(depth, topY,  42, 45, 64, 120, 134, 160, topR, topG, topB);

    // ---- Front face: graded, darker at the foot ---------------------------
    glBegin(GL_QUADS);
        glColor3ub(botR, botG, botB);
        glVertex2f(cx - half, baseY); glVertex2f(cx + half, baseY);
        glColor3ub(topR, topG, topB);
        glVertex2f(cx + half, topY);  glVertex2f(cx - half, topY);
    glEnd();

    // ---- Receding side face -----------------------------------------------
    // Thrown toward the vanishing point at x = 0 so the whole row shares one
    // camera instead of each block facing its own way. A block left of centre
    // shows its right flank, one on the right shows its left flank.
    float dir = (cx < 0.0f) ? 1.0f : -1.0f;
    float fx  = cx + dir * half;                       // the corner we see round
    float sw  = fabsf(fx) * 0.048f * (1.0f - depth * 0.55f);
    if (sw > 2.0f) sw = 2.0f;
    if (sw > 0.12f) {
        float k       = sw / (fabsf(fx) + 0.001f);     // fraction of the way to the VP
        float backTop = topY + (baseY - topY) * k;
        // The light sits up and to the right (moon by night, sun by day), so a
        // right-facing flank catches it and a left-facing one is in shadow.
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

    // ---- Windows -----------------------------------------------------------
    // Skipped on the furthest row: at that distance individual windows would
    // only be noise, and leaving them off is itself a depth cue.
    if (depth < 0.82f && w > 3.0f && h > 3.0f) {
        int cols = (int)(w / 1.9f); if (cols > 7) cols = 7;
        int rows = (int)(h / 2.0f); if (rows > 6) rows = 6;
        if (cols > 0 && rows > 0) {
            float slot = w / (cols + 1.0f);
            float ww   = slot * 0.42f;
            float wh   = (h / (rows + 1.0f)) * 0.44f;
            float fade = 1.0f - depth;                  // far windows lose contrast
            for (int c = 0; c < cols; c++) {
                for (int rw = 0; rw < rows; rw++) {
                    float wx = cx - half + slot * (c + 1) - ww * 0.5f;
                    float wy = baseY + (h / (rows + 1.0f)) * (rw + 1) - wh * 0.5f;
                    bool on = FarWindowLit4(seed, c, rw);
                    if (on) {
                        // Warm rooms, only at night; by day the glass is just
                        // a slightly cooler patch of wall.
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

    // ---- Roof --------------------------------------------------------------
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
    // Snow cap. Even on the back row it stays visible: a winter skyline with
    // bare roofs against a snowing sky does not hold together.
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

    // Contact shade where the block meets whatever is behind the row in front
    // of it -- stops two rows of flat colour from butting into one shape.
    glColor4ub(MixB4(6, 60), MixB4(8, 68), MixB4(16, 86), (unsigned char)(70 * (1.0f - depth)));
    glBegin(GL_QUADS);
        glVertex2f(cx - half, baseY);
        glVertex2f(cx + half, baseY);
        glVertex2f(cx + half, baseY + 0.9f);
        glVertex2f(cx - half, baseY + 0.9f);
    glEnd();
}

struct FarBlock4 { float x, w, h; bool pitched; };

void DrawDistantBuildings4() {
    // Back row: pale, low contrast, no windows. Deliberately the vaguest
    // thing in the frame.
    static const FarBlock4 rowBack[] = {
        { -54.0f, 15.0f,  7.5f, false }, { -40.0f, 11.0f, 10.5f, false },
        { -29.0f, 10.0f,  6.2f, true  }, { -16.0f, 13.0f,  9.0f, false },
        {  -2.0f, 11.0f, 11.5f, false }, {  11.0f, 12.0f,  7.8f, true  },
        {  24.0f, 13.0f, 12.2f, false }, {  38.0f, 11.0f,  8.6f, false },
        {  51.0f, 15.0f, 10.0f, false }
    };
    // Mid row: a storey lower and a step nearer, so its rooflines break the
    // back row's instead of echoing it.
    static const FarBlock4 rowMid[] = {
        { -57.0f, 12.0f,  5.4f, true  }, { -45.0f,  9.0f,  8.2f, false },
        { -33.5f, 10.0f,  4.6f, true  }, { -21.0f, 11.0f,  7.4f, false },
        {  -9.0f,  9.5f,  9.6f, false }, {   3.0f, 10.0f,  5.2f, true  },
        {  16.0f, 11.0f,  8.8f, false }, {  30.0f,  9.5f,  6.0f, true  },
        {  44.0f, 12.0f,  9.2f, false }, {  57.0f, 10.0f,  6.6f, true  }
    };
    // Front row: nearest, darkest, most detail. It tucks in just behind the
    // shop roofline so the two layers overlap rather than stacking.
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

    // A band of air between the rows. Very cheap, and it is what stops the
    // three rows reading as one cut-out with notches in it. It has to fade
    // out at BOTH ends: a flat-alpha quad leaves two horizontal seams across
    // the skyline, which reads as a mistake rather than as distance.
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

// ---- Street canyon framing the road ---------------------------------------
//  The road runs out of the frame through the gap between the corner shop and
//  TOYS. A road needs walls or it is only a pale shape painted on the snow, so
//  two runs of facade recede up that gap toward the point the road converges
//  on. They are drawn before the shop row, so the shops crop them cleanly at
//  each side and the whole thing reads as a side street seen end-on.
constexpr float roadFarX4  = -35.4f;   // centre of the road where it meets the horizon
constexpr float roadFarY4  =  -6.0f;   // the plaza horizon: where the road starts
constexpr float roadFarHW4 =   1.70f;  // half-width at that far end

void DrawRoadsideRun4(float nearX, float nearTop, float farX, float farTop,
                      int seed, int nBays) {
    // One run of facade: a wedge from a tall near edge down to a short far
    // edge, cut into bays so it reads as several buildings rather than one
    // ramp. Everything about a bay -- height, colour, window size -- is driven
    // by how far along the wedge it sits, which is what gives the run its
    // perspective.
    for (int i = 0; i < nBays; i++) {
        float t0 = (float)i       / nBays;
        float t1 = (float)(i + 1) / nBays;
        float x0 = nearX + (farX - nearX) * t0;
        float x1 = nearX + (farX - nearX) * t1;
        // Each bay steps down, with a little variation so the ridge is not a
        // perfectly straight diagonal.
        float jitter = sinf((float)(seed + i) * 2.3f) * 0.9f * (1.0f - t0);
        float y0 = nearTop + (farTop - nearTop) * t0 + jitter;
        float y1 = nearTop + (farTop - nearTop) * t1 + jitter;
        float depth = 0.30f + 0.55f * t0;

        unsigned char botR, botG, botB, topR, topG, topB;
        // The canyon wall is turned away from the plaza, so it sits in its own
        // shade: darker than the shop fronts either side of it, which is what
        // makes the gap read as a gap.
        AirFade4(depth, roadFarY4, 27, 29, 42,  86,  94, 114, botR, botG, botB);
        AirFade4(depth, y0,        50, 53, 72, 126, 134, 152, topR, topG, topB);
        glBegin(GL_QUADS);
            glColor3ub(botR, botG, botB);
            glVertex2f(x0, roadFarY4); glVertex2f(x1, roadFarY4);
            glColor3ub(topR, topG, topB);
            glVertex2f(x1, y1);        glVertex2f(x0, y0);
        glEnd();

        // Party wall between bays, catching a sliver of light on its edge.
        unsigned char er, eg, eb;
        AirFade4(depth, y0, 72, 76, 96, 158, 166, 180, er, eg, eb);
        glColor3ub(er, eg, eb);
        glBegin(GL_QUADS);
            glVertex2f(x1 - (x1 - x0) * 0.10f, roadFarY4);
            glVertex2f(x1,                     roadFarY4);
            glVertex2f(x1,                     y1);
            glVertex2f(x1 - (x1 - x0) * 0.10f, y1 + (y0 - y1) * 0.10f);
        glEnd();

        // Windows: two ranks, shrinking and dimming down the run.
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

        // Snow along the bay's ridge.
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

void DrawRoadsideBlocks4() {
    // Left-hand wall: runs from the corner shop's near edge down to the point
    // the road vanishes.
    DrawRoadsideRun4(-48.6f, 7.8f, roadFarX4 - roadFarHW4 - 0.2f, -3.0f, 5, 5);
    // Right-hand wall: from TOYS' near edge back to the same point. It is a
    // touch lower, so the two sides do not mirror each other.
    DrawRoadsideRun4(-28.8f, 6.4f, roadFarX4 + roadFarHW4 + 0.2f, -3.2f, 12, 3);

    // Depth haze in the mouth of the street. A stack of soft ellipses read as
    // a grey smudge sitting on the buildings; one wedge, strongest right at
    // the vanishing point and gone by the time it reaches the shopfronts
    // either side, is what actually sells the distance.
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

    // A far-end lamp glow at night, so the street has a reason to be lit.
    float night = NightT4();
    if (night > 0.02f) {
        DrawSoftEllipse(roadFarX4, roadFarY4 + 1.6f, 2.6f, 1.6f,
                        255, 198, 128, (unsigned char)(78 * night), 4);
    }
}

// ---- Elevated metro rail behind the shops ------------------------------
void DrawMetroRail4() {
    // determine a sensible y above the tallest shop eave so the rail sits
    float maxEave = -999.0f;
    for (int i = 0; i < NUM_SHOPS4; i++) {
        float ey = ShopEaveY4(shops4[i]);
        if (ey > maxEave) maxEave = ey;
    }
    float railY = maxEave + 1.6f;

    // Make the rail visible both by night and by day: keep a base alpha
    float night = NightT4();
    unsigned char beamA = (unsigned char)fminf(255.0f, 90.0f + night * 140.0f);
    // Main elevated beam
    glColor4ub(48, 48, 56, beamA);
    glBegin(GL_QUADS);
        glVertex2f(-62.0f, railY + 0.45f); glVertex2f(62.0f, railY + 0.45f);
        glVertex2f(62.0f, railY - 0.05f);  glVertex2f(-62.0f, railY - 0.05f);
    glEnd();

    // Vertical supports spaced across the width — deeper and thicker for realism
    unsigned char supportA = (unsigned char)fminf(255.0f, 70.0f + night * 150.0f);
    glColor4ub(36, 36, 44, supportA);
    for (float x = -52.0f; x <= 52.0f; x += 8.0f) {
        glBegin(GL_QUADS);
            glVertex2f(x - 0.6f, railY - 0.05f); glVertex2f(x + 0.6f, railY - 0.05f);
            glVertex2f(x + 1.2f, railY - 8.0f);   glVertex2f(x - 1.2f, railY - 8.0f);
        glEnd();
    }

    // Carriage: larger, train-like silhouette with roof and wheels
    unsigned char carA = (unsigned char)fminf(255.0f, 120.0f + night * 120.0f);
    float carHalf = 18.0f; // half-length for the carriage silhouette
    float carTop = railY + 0.9f, carBot = railY - 0.05f;
    float carCenter = metroCarX4; // global position (updated by timer)
    // body
    glColor4ub(80, 78, 90, carA);
    glBegin(GL_QUADS);
        glVertex2f(carCenter - carHalf, carTop); glVertex2f(carCenter + carHalf, carTop);
        glVertex2f(carCenter + carHalf, carBot); glVertex2f(carCenter - carHalf, carBot);
    glEnd();
    // roof cap
    glColor4ub(70, 68, 78, (unsigned char)fminf(220.0f, 90.0f + night * 120.0f));
    glBegin(GL_QUADS);
        glVertex2f(carCenter - carHalf - 0.6f, carTop); glVertex2f(carCenter + carHalf + 0.6f, carTop);
        glVertex2f(carCenter + carHalf + 0.6f, carTop + 0.5f); glVertex2f(carCenter - carHalf - 0.6f, carTop + 0.5f);
    glEnd();
    // windows (brighter in day)
    unsigned char winA = (unsigned char)fminf(255.0f, 140.0f + dayT4 * 80.0f);
    glColor4ub(200, 210, 220, winA);
    float w = 3.2f;
    for (float x = carCenter - carHalf + 2.0f; x < carCenter + carHalf - 1.0f; x += 5.0f) {
        glBegin(GL_QUADS);
            glVertex2f(x, carTop - 0.15f); glVertex2f(x + w, carTop - 0.15f);
            glVertex2f(x + w, carBot + 0.08f); glVertex2f(x, carBot + 0.08f);
        glEnd();
    }
    // wheels removed per user request: skip drawing small circles here
}

void UpdateTwinkle4(int) {
    // The sunrise runs before the pause check: pressing 4 while the scene is
    // paused should still get you to daylight, it just gets there without the
    // snow and the wheel moving.
    if (dayT4 != dayTarget4) {
        const float step = 0.018f;              // ~3.3 s for a full crossfade
        if (dayT4 < dayTarget4) dayT4 = (dayT4 + step > dayTarget4) ? dayTarget4 : dayT4 + step;
        else                    dayT4 = (dayT4 - step < dayTarget4) ? dayTarget4 : dayT4 - step;
        glutPostRedisplay();
    }
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateTwinkle4, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  LIGHT ON THE SNOW
// ----------------------------------------------------------------------------
//  Snow is the most reflective surface in the project and, until now, nothing
//  in this scene lit it. Every lamp, window and bulb was a bright dot sitting
//  on a flat grey field with no relationship to the ground under it -- which
//  is the main reason the plaza read as dead space.
//
//  DrawGroundGlow4 lays a soft pool of light on the paving; DrawLightPools4
//  places one under every source, in one pass, before any prop is drawn.
// ============================================================================
void DrawGroundGlow4(float x, float y, float rx, float ry,
                     unsigned char r, unsigned char g, unsigned char b,
                     unsigned char a) {
    DrawSoftEllipse(x, y, rx, ry, r, g, b, a, 4);
}

void DrawLightPools4() {
    float nightGlow = NightT4();
    // Cool moonlight wash first. The moon sits at (34, 27), so the wash is
    // centred left of it and the whole plaza picks up a faint blue sheen.
    DrawGroundGlow4(18.0f, -18.0f, 62.0f, 17.0f, 150, 178, 225, (unsigned char)(34 * nightGlow));

    // Warm spill from the shopfronts along the back of the plaza
    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        DrawGroundGlow4(sh.x, -7.4f, sh.w * 0.62f, 2.4f, 255, 196, 120, (unsigned char)(62 * nightGlow));
    }

    // Pools under each string-light span, at the low point of its catenary
    for (int i = 0; i < NUM_LIGHT_SPANS4; i++) {
        const LightSpan4& sp = lightSpans4[i];
        float mx = (sp.x1 + sp.x2) * 0.5f;
        unsigned char r = 255, g = 205, b = 140;
        if (multicolorLights4) { r = 225; g = 175; b = 205; }
        DrawGroundGlow4(mx, -8.6f, fabsf(sp.x2 - sp.x1) * 0.42f, 2.8f, r, g, b, (unsigned char)(46 * nightGlow));
    }

    // Market stalls: each awning lamp throws a small warm pool at its feet
    for (int i = 0; i < NUM_STALLS4; i++)
        DrawGroundGlow4(stalls4[i].x, stallGroundY4 - 0.2f,
                        StallHalfW4() + 1.2f, 1.7f, 255, 190, 110, (unsigned char)(88 * nightGlow));

    // The three biggest sources in the scene
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

    // When the aurora is up it has to reach the ground or it is only a
    // wallpaper effect: a wide, very faint green wash over the whole plaza,
    // rising and falling with the same envelope the curtains use.
    if (auroraActive4) {
        float env = sinf(auroraTimer4 * PI4);
        if (env > 0.0f)
            DrawGroundGlow4(-4.0f, -13.0f, 66.0f, 13.0f, 90, 230, 170,
                            (unsigned char)(30.0f * env * nightGlow));
    }
}

// ============================================================================
//  FERRIS WHEEL
// ============================================================================
// A bulb's colour: warm by default, cycling through four colours when the
// 'L' toggle is on, so the whole fairground responds to one key.
void FerrisBulbColour4(int index, unsigned char& r, unsigned char& g, unsigned char& b) {
    if (multicolorLights4) {
        const unsigned char cols[4][3] = { {255,70,70}, {70,225,110}, {80,160,255}, {255,225,90} };
        int c = index % 4;
        r = cols[c][0]; g = cols[c][1]; b = cols[c][2];
    } else { r = 255; g = 208; b = 135; }
}

constexpr int FERRIS_RIM_BULBS4 = 30;

void DrawFerrisWheel4() {
    float nightGlow = NightT4();
    // ---- Halo -----------------------------------------------------------
    // A fairground wheel at a night market has to glow. This wide, very low
    // alpha disc sits behind the whole structure and lifts it off the sky.
    unsigned char hr, hg, hb;
    FerrisBulbColour4(0, hr, hg, hb);
    DrawSoftEllipse(ferrisCX4, ferrisCY4, ferrisR4 * 1.75f, ferrisR4 * 1.75f,
                    hr, hg, hb, (unsigned char)(34 * nightGlow), 4);

    // ---- A-frame --------------------------------------------------------
    // The legs splay in proportion to the height they carry, so a taller wheel
    // gets a wider, still-plausible stance instead of stilts.
    const float frameH  = ferrisCY4 - ferrisBaseY4;
    const float legHalf = frameH * 0.46f;
    const float braceY  = ferrisBaseY4 + frameH * 0.38f;
    glColor3ub(MixB4(70, 110), MixB4(70, 118), MixB4(80, 120));
    glLineWidth(2.6f * ferrisScale4);
    glBegin(GL_LINES);
        glVertex2f(ferrisCX4-legHalf, ferrisBaseY4); glVertex2f(ferrisCX4, ferrisCY4);
        glVertex2f(ferrisCX4+legHalf, ferrisBaseY4); glVertex2f(ferrisCX4, ferrisCY4);
        glVertex2f(ferrisCX4-legHalf*0.62f, braceY);
        glVertex2f(ferrisCX4+legHalf*0.62f, braceY);   // cross brace
    glEnd();
    // Feet, so the legs land on something rather than stopping in mid-snow
    glLineWidth(3.4f * ferrisScale4);
    glBegin(GL_LINES);
        glVertex2f(ferrisCX4-legHalf-1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4-legHalf+1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4+legHalf-1.0f, ferrisBaseY4);
        glVertex2f(ferrisCX4+legHalf+1.0f, ferrisBaseY4);
    glEnd();

    // ---- Rim ------------------------------------------------------------
    glColor3ub(MixB4(150, 135), MixB4(52, 110), MixB4(74, 90));
    glLineWidth(2.4f * ferrisScale4);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 28; i++) {
            float a = (float)i / 28 * 2 * PI4;
            glVertex2f(ferrisCX4 + ferrisR4*cos(a), ferrisCY4 + ferrisR4*sin(a));
        }
    glEnd();

    // Rim bulbs running a chase pattern. The wave travels around the wheel
    // rather than every bulb blinking together, which is what a real ride
    // does and what makes it read as lit rather than decorated.
    for (int i = 0; i < FERRIS_RIM_BULBS4; i++) {
        float a = (float)i / FERRIS_RIM_BULBS4 * 2.0f * PI4 + ferrisAngle4 * 0.25f;
        float bx = ferrisCX4 + ferrisR4 * cosf(a);
        float by = ferrisCY4 + ferrisR4 * sinf(a);
        float chase = 0.35f + 0.65f * (0.5f + 0.5f * sinf(twinklePhase4 * 4.0f - i * 0.62f));
        unsigned char r, g, b;
        FerrisBulbColour4(i, r, g, b);
        // glow, then the filament
        FilledCircle4(bx, by, 0.62f * ferrisScale4 * chase, r, g, b, (unsigned char)(70 * chase * nightGlow));
        FilledCircle4(bx, by, 0.20f * ferrisScale4,
                      (unsigned char)(r * chase * nightGlow), (unsigned char)(g * chase * nightGlow),
                      (unsigned char)(b * chase * nightGlow), (unsigned char)(255 * nightGlow));
    }

    // Bulbs marching down alternate spokes
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

void UpdateFerrisWheel4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFerrisWheel4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) ferrisAngle4 += 0.01f;
    glutTimerFunc(30, UpdateFerrisWheel4, 0);
}

// ============================================================================
//  ICE RINK + SKATERS
// ============================================================================
void DrawRink4() {
    // The rink is ice: out of season it dissolves and DrawFountainAutumn4()
    // takes over the same ground.
    if (SeasonWinter4() < 0.02f) return;
    // Ice sheet, graded so the far edge is paler than the near edge
    glBegin(GL_TRIANGLE_FAN);
        glColor3ub(MixB4(196, 160), MixB4(224, 220), MixB4(242, 245));
        glVertex2f(rinkCX4, rinkCY4);
        glColor3ub(MixB4(168, 140), MixB4(202, 202), MixB4(226, 206));
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(rinkCX4 + 6.0f*cos(a), rinkCY4 + 2.2f*sin(a));
        }
    glEnd();

    // Moon specular: a pale streak lying across the ice from the moon side
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

    // Carved skate scratches
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

    // Boards around the rink, with corner posts and a gate gap at the front
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
    // Gate posts either side of the entrance
    FilledCircle4(rinkCX4 + 6.0f*cosf(-0.35f), rinkCY4 + 2.2f*sinf(-0.35f), 0.20f, 210, 200, 120, 255);
    FilledCircle4(rinkCX4 + 6.0f*cosf(0.35f),  rinkCY4 + 2.2f*sinf(0.35f),  0.20f, 210, 200, 120, 255);
    glLineWidth(1.0f);
}

void DrawSkater4(const Skater4& sk) {
    float x = rinkCX4 + sk.radiusX * cosf(sk.angle);
    float y = rinkCY4 + sk.radiusY * sinf(sk.angle);

    // Is this the one currently on the ice? `fall` is 0 upright, 1 fully down.
    bool  isFaller = (&sk == &skaters4[FALLING_SKATER4]);
    float fall = 0.0f;
    if (isFaller && skaterFallT4 > 0.0f) {
        if      (skaterFallT4 < 0.18f) fall = skaterFallT4 / 0.18f;            // going down
        else if (skaterFallT4 < 0.70f) fall = 1.0f;                            // sat on the ice
        else                            fall = (1.0f - skaterFallT4) / 0.30f;  // getting up
        if (fall < 0.0f) fall = 0.0f;
        if (fall > 1.0f) fall = 1.0f;
    }

    // Lean into the curve: proportional to speed and inverse to radius, so
    // the fast skater on a tight path banks hardest.
    float leanDeg = -cosf(sk.angle) * (sk.speed * 900.0f) / (sk.radiusX + 1.0f);
    if (leanDeg >  22.0f) leanDeg =  22.0f;
    if (leanDeg < -22.0f) leanDeg = -22.0f;

    // Which way they're travelling, for arms and blade orientation
    float dirSign = (-sinf(sk.angle) >= 0.0f) ? 1.0f : -1.0f;

    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    // A fall tips the whole figure over and drops it toward the ice.
    glRotatef(leanDeg + fall * 72.0f * dirSign, 0.0f, 0.0f, 1.0f);
    glScalef(1.0f, 1.0f - fall * 0.35f, 1.0f);

    // Spray of ice kicked up at the moment of the fall
    if (fall > 0.0f && skaterFallT4 < 0.30f) {
        for (int i = 0; i < 5; i++) {
            float t = fmodf(skaterFallT4 * 3.0f + i * 0.2f, 1.0f);
            FilledCircle4(-0.6f * dirSign - t * 1.4f, 0.15f + t * 0.8f,
                          0.16f * (1.0f - t), 244, 250, 255,
                          (unsigned char)(190 * (1.0f - t)));
        }
    }

    // Blade: a bright line at the ice contact point
    glColor3ub(238, 246, 255);
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(-0.30f * dirSign, 0.02f); glVertex2f(0.34f * dirSign, 0.02f);
    glEnd();

    // Legs
    glColor3ub(40, 40, 52);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, 1.0f); glVertex2f(-0.14f * dirSign, 0.05f);
        glVertex2f(0.0f, 1.0f); glVertex2f( 0.20f * dirSign, 0.05f);
    glEnd();

    // Torso
    glColor3ub(sk.r, sk.g, sk.b);
    glLineWidth(3.5f);
    glBegin(GL_LINES); glVertex2f(0.0f, 1.0f); glVertex2f(0.0f, 1.9f); glEnd();

    // Arms out for balance, swinging opposite the stride
    float swing = sinf(sk.angle * 6.0f) * 0.22f;
    glLineWidth(1.8f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, 1.70f); glVertex2f( 0.62f * dirSign, 1.55f + swing);
        glVertex2f(0.0f, 1.70f); glVertex2f(-0.58f * dirSign, 1.60f - swing);
    glEnd();

    // Head with a bobble hat
    FilledCircle4(0.0f, 2.20f, 0.28f, 225, 185, 145, 255);
    FilledCircle4(0.0f, 2.44f, 0.22f, sk.r, sk.g, sk.b, 255);
    FilledCircle4(0.0f, 2.64f, 0.09f, 250, 250, 255, 255);

    // Scarf trailing behind
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

void DrawSkaters4() {
    if (SeasonWinter4() < 0.02f) return;   // no ice, no skaters
    for (int i = 0; i < NUM_SKATERS4; i++) DrawSkater4(skaters4[i]);

    // Joined hands between the pair, drawn after both so the arm sits on top.
    const Skater4& a = skaters4[3];
    const Skater4& b = skaters4[4];
    float ax = rinkCX4 + a.radiusX * cosf(a.angle), ay = rinkCY4 + a.radiusY * sinf(a.angle);
    float bx = rinkCX4 + b.radiusX * cosf(b.angle), by = rinkCY4 + b.radiusY * sinf(b.angle);
    glColor3ub(232, 196, 158);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(ax, ay + 1.68f);
        glVertex2f((ax + bx) * 0.5f, (ay + by) * 0.5f + 1.48f);   // hands dip between them
        glVertex2f(bx, by + 1.68f);
    glEnd();
    glLineWidth(1.0f);
}

void UpdateSkaters4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSkaters4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        for (int i = 0; i < NUM_SKATERS4; i++) {
            // Someone sprawled on the ice is not also gliding.
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

// ============================================================================
//  HORSE-DRAWN SLED
// ============================================================================
// ---- Horse: body, neck, head, gallop cycle, breath ----------------------
void DrawHorse4(float x, float y, float gait) {
    float legA = sinf(gait), legB = sinf(gait + PI4);

    // Hind and fore legs, alternating pairs
    glColor3ub(58, 40, 30);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
        glVertex2f(x - 0.75f, y + 0.55f); glVertex2f(x - 0.75f + 0.34f*legA, y - 0.35f);
        glVertex2f(x - 0.55f, y + 0.55f); glVertex2f(x - 0.55f + 0.30f*legB, y - 0.35f);
        glVertex2f(x + 0.70f, y + 0.55f); glVertex2f(x + 0.70f + 0.34f*legB, y - 0.35f);
        glVertex2f(x + 0.50f, y + 0.55f); glVertex2f(x + 0.50f + 0.30f*legA, y - 0.35f);
    glEnd();

    // Barrel
    glColor3ub(78, 54, 38);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.95f, y + 0.55f);
        glVertex2f(x + 0.95f, y + 0.55f);
        glVertex2f(x + 1.00f, y + 1.30f);
        glVertex2f(x - 0.90f, y + 1.35f);
    glEnd();

    // Neck and head
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
    // Ear + mane
    glColor3ub(44, 30, 22);
    glBegin(GL_TRIANGLES);
        glVertex2f(x + 1.42f, y + 2.34f); glVertex2f(x + 1.56f, y + 2.34f); glVertex2f(x + 1.48f, y + 2.62f);
    glEnd();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x + 1.30f, y + 2.05f); glVertex2f(x + 0.95f, y + 1.35f);
    glEnd();
    // Tail
    glBegin(GL_LINES);
        glVertex2f(x - 0.92f, y + 1.25f); glVertex2f(x - 1.45f, y + 0.62f + 0.12f*legA);
    glEnd();

    // Harness strap + trace line back toward the sled
    glColor3ub(120, 80, 45);
    glLineWidth(1.6f);
    glBegin(GL_LINES);
        glVertex2f(x + 0.85f, y + 1.20f); glVertex2f(x + 0.85f, y + 0.55f);
        glVertex2f(x - 0.95f, y + 0.95f); glVertex2f(x - 2.10f, y + 0.80f);
    glEnd();

    // Breath puffs from the nostrils in the cold
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.30f + i * 0.33f, 1.0f);
        FilledCircle4(x + 2.05f + t * 1.5f, y + 2.25f + t * 0.35f,
                      0.12f + t * 0.20f, 225, 232, 240,
                      (unsigned char)(120 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void DrawSled4() {
    if (!sledActive4) return;
    if (SeasonWinter4() < 0.02f) return;   // a sled needs snow to run on
    float x = sledX4, y = -11.0f;
    float gait = sledX4 * 1.6f;   // legs cycle with distance travelled

    // Horse out in front, pulling
    DrawHorse4(x + 3.4f, y, gait);

    // Sled body
    glColor3ub(112, 72, 46);
    glBegin(GL_QUADS);
        glVertex2f(x-1.3f, y+0.15f);  glVertex2f(x+1.5f, y+0.15f);
        glVertex2f(x+1.5f, y+1.25f);  glVertex2f(x-1.3f, y+1.25f);
    glEnd();
    // Curled front rail
    glColor3ub(140, 92, 58);
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x+1.5f, y+1.25f); glVertex2f(x+1.9f, y+1.15f); glVertex2f(x+2.0f, y+0.70f);
    glEnd();
    // Runners
    glColor3ub(196, 202, 214);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-1.5f, y+0.05f); glVertex2f(x+1.9f, y+0.05f);
        glVertex2f(x-1.2f, y+0.15f); glVertex2f(x-1.2f, y+0.05f);
        glVertex2f(x+1.3f, y+0.15f); glVertex2f(x+1.3f, y+0.05f);
    glEnd();

    // Two riders under a blanket
    glColor3ub(150, 40, 50);
    glBegin(GL_QUADS);
        glVertex2f(x-1.1f, y+1.25f); glVertex2f(x+1.0f, y+1.25f);
        glVertex2f(x+1.0f, y+1.70f); glVertex2f(x-1.1f, y+1.70f);
    glEnd();
    FilledCircle4(x-0.45f, y+2.00f, 0.30f, 232, 194, 154, 255);
    FilledCircle4(x+0.45f, y+1.95f, 0.27f, 224, 186, 146, 255);
    // Driver's hat
    glColor3ub(40, 40, 50);
    glBegin(GL_TRIANGLES);
        glVertex2f(x-0.72f, y+2.22f); glVertex2f(x-0.18f, y+2.22f); glVertex2f(x-0.45f, y+2.62f);
    glEnd();
    // Reins running forward to the harness
    glColor3ub(90, 62, 38);
    glLineWidth(1.2f);
    glBegin(GL_LINES);
        glVertex2f(x-0.20f, y+1.95f); glVertex2f(x+4.25f, y+1.20f);
    glEnd();

    // Snow spray kicked up by the runners
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.5f + i * 0.2f, 1.0f);
        FilledCircle4(x - 1.6f - t * 1.8f, y + 0.08f + t * 0.5f,
                      0.14f * (1.0f - t), 245, 248, 255,
                      (unsigned char)(170 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void UpdateSled4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSled4, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  FIRE PIT / MARKET STALLS / SMOKE + STEAM
// ============================================================================
// ---- Fire pit: stones, logs, layered flame, embers, ground bounce --------
constexpr int MAX_EMBERS4 = 14;
struct Ember4 { float x, y, vy, drift, life; bool active; };
Ember4 embers4[MAX_EMBERS4];

void DrawFirePit4() {
    float flick = 0.72f + 0.28f * sinf(firePhase4 * 3.0f);

    // Warm light bouncing off the snow around the pit, drawn first so
    // everything else sits on top of it.
    glBegin(GL_TRIANGLE_FAN);
        glColor4ub(255, 145, 45, (unsigned char)(95 * flick));
        glVertex2f(fireX4, fireY4);
        glColor4ub(255, 120, 40, 0);
        for (int i = 0; i <= 26; i++) {
            float a = (float)i / 26.0f * 2.0f * PI4;
            glVertex2f(fireX4 + 6.2f * cosf(a), fireY4 + 2.4f * sinf(a));
        }
    glEnd();

    // Stone ring
    for (int i = -3; i <= 3; i++) {
        float sx = fireX4 + i * 0.45f;
        FilledCircle4(sx, fireY4 - 0.25f, 0.26f, 96, 96, 104, 255);
        FilledCircle4(sx, fireY4 - 0.20f, 0.18f, 122, 122, 130, 255);
    }

    // Crossed logs
    glColor3ub(96, 64, 40);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(fireX4 - 1.0f, fireY4 - 0.05f); glVertex2f(fireX4 + 0.9f, fireY4 + 0.35f);
        glVertex2f(fireX4 + 1.0f, fireY4 - 0.05f); glVertex2f(fireX4 - 0.9f, fireY4 + 0.35f);
    glEnd();

    // Flame: five tapering tongues, each on its own sine phase, hottest and
    // palest at the core. Replaces the old two overlapping circles.
    struct FlameLayer { float w, h, ph; unsigned char r, g, b, a; };
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

    // Embers drifting up out of the flame
    for (int i = 0; i < MAX_EMBERS4; i++) {
        if (!embers4[i].active) continue;
        unsigned char a = (unsigned char)(230 * embers4[i].life);
        FilledCircle4(embers4[i].x, embers4[i].y, 0.09f * embers4[i].life,
                      255, 180, 80, a);
    }

    // Two people warming their hands at the pit
    for (int s = -1; s <= 1; s += 2) {
        float px = fireX4 + s * 2.6f;
        FilledCircle4(px, fireY4 + 1.55f, 0.28f, 228, 190, 150, 255);
        glColor3ub(s < 0 ? 150 : 60, s < 0 ? 50 : 90, s < 0 ? 60 : 140);
        glBegin(GL_QUADS);
            glVertex2f(px - 0.26f, fireY4 + 0.35f); glVertex2f(px + 0.26f, fireY4 + 0.35f);
            glVertex2f(px + 0.22f, fireY4 + 1.30f); glVertex2f(px - 0.22f, fireY4 + 1.30f);
        glEnd();
        // Arms reaching toward the warmth
        glColor3ub(228, 190, 150);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(px - s * 0.22f, fireY4 + 1.05f);
            glVertex2f(px - s * 1.05f, fireY4 + 0.80f);
        glEnd();
    }
    glLineWidth(1.0f);
}

void UpdateFire4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFire4, 0); return; }
    if (isAnimating4) {
        firePhase4 += 0.15f;
        // Spawn embers
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

// A market stall: striped awning, a named signboard, a vendor behind the
// counter and goods on it. Two of the four stalls used to be blank boxes
// with no sign, no vendor and nothing for sale.
// The body below was drawn against a 1.0 scale, so rather than rescale forty
// vertex literals by hand it is wrapped in one transform anchored at the foot
// of the stall -- the stall then grows upward and outward from the snow it
// stands on. The name board is drawn afterwards, outside the transform,
// because glutBitmapCharacter is sized in pixels and would not scale with it.
void DrawStall4(const Stall4& st) {
    const float x  = st.x;
    const unsigned char ar = st.r, ag = st.g, ab = st.b;
    const char* sign = st.sign;
    const int   goods = st.goods;
    float y = -9.5f;

    BeginDepthSprite(x, y - 2.0f, stallScale4);

    // Counter
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

    // Vendor behind the counter, rocking gently on their feet
    float rock = sinf(pedTimer4 * 0.8f + x) * 0.10f;
    glColor3ub(70, 60, 88);
    glLineWidth(5.0f);
    glBegin(GL_LINES); glVertex2f(x - 0.6f + rock, y + 0.1f); glVertex2f(x - 0.6f + rock, y + 1.3f); glEnd();
    FilledCircle4(x - 0.6f + rock, y + 1.65f, 0.32f, 228, 190, 152, 255);
    FilledCircle4(x - 0.6f + rock, y + 1.92f, 0.30f, ar, ag, ab, 255);      // matching hat
    glLineWidth(1.0f);

    // Goods on the counter: 0 = mugs, 1 = wreaths, 2 = roasted chestnuts,
    // 3 = wrapped toys
    for (int i = 0; i < 4; i++) {
        float gx = x - 1.5f + i * 1.0f;
        if (goods == 0) {                                   // steaming mugs
            glColor3ub(240, 240, 245);
            glBegin(GL_QUADS);
                glVertex2f(gx-0.20f, y); glVertex2f(gx+0.20f, y);
                glVertex2f(gx+0.20f, y+0.42f); glVertex2f(gx-0.20f, y+0.42f);
            glEnd();
            float t = fmodf(firePhase4 * 0.35f + i * 0.25f, 1.0f);
            FilledCircle4(gx, y + 0.5f + t * 0.9f, 0.10f + t * 0.16f,
                          235, 240, 248, (unsigned char)(120 * (1.0f - t)));
        } else if (goods == 1) {                            // wreaths
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
        } else if (goods == 2) {                            // chestnut pan
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
        } else {                                            // wrapped toys
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

    // Striped awning
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
    // Scalloped fringe
    glColor3ub(ar, ag, ab);
    for (int i = 0; i < 7; i++)
        FilledCircle4(x - 2.5f + i * 0.85f, y - 0.05f, 0.22f, ar, ag, ab, 255);

    // Awning lamp and the warm patch it throws on the counter
    FilledCircle4(x, y + 1.05f, 0.9f, 255, 210, 140, 50);
    FilledCircle4(x, y + 1.05f, 0.22f, 255, 244, 205, 255);

    EndDepthSprite();

    // ---- Name board -------------------------------------------------------
    // Drawn in world space above the scaled canopy. Its width comes from the
    // name rather than a fixed 1.9, which is what let "CHESTNUTS" and
    // "WREATHS" run off both ends of their boards; now that the stall itself
    // is 1.55x, a board sized to its text no longer dwarfs the tent under it.
    if (sign != nullptr) {
        float apex  = StallApexY4();
        // Sitting the board clear above the ridge left it floating: the canopy
        // comes to a point, so there was nothing under most of its width. It
        // is dropped far enough to bite into the peak, and carried on two
        // posts that stand on the slopes, but stays above the awning lamp at
        // StallCounterTopY4() + 1.05*stallScale4 so it cannot black it out.
        float signH = 1.70f;
        float ty    = apex - 0.35f;
        float txtW  = TextPixelWidth(GLUT_BITMAP_HELVETICA_12, sign)
                    * (WORLD_RIGHT - WORLD_LEFT) / (float)viewportPixelWidth;
        float half  = txtW * 0.5f + 0.8f;

        // Posts, drawn before the board so they run behind it
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

        glColor4ub(255, 198, 112, 48);          // the board catches the lamp below it
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

// A short queue waiting at a stall, so the market looks used rather than set up.
// A short queue waiting at a stall, so the market looks used rather than set
// up. These are built to DrawPerson4's proportions -- feet on the ground,
// hips at +1.0, hat at +2.2 above that -- because the old build topped out at
// 2.2 overall and left the customers a head shorter than the crowd walking
// past them.
void DrawStallQueue4(float x, int n) {
    for (int i = 0; i < n; i++) {
        float qx  = x - StallHalfW4() - 1.1f - i * 1.9f;
        float bob = sinf(pedTimer4 * 1.1f + i * 1.7f) * 0.09f;
        float fy  = stallGroundY4 + bob;
        float hipY = fy + 1.0f;
        unsigned char cols[3][3] = { {150, 60, 70}, {50, 80, 130}, {90, 110, 70} };
        unsigned char cr = cols[i%3][0], cg = cols[i%3][1], cb = cols[i%3][2];

        DrawGroundShadow(qx, stallGroundY4, 0.62f, 0.25f, 80);
        glColor3ub(30, 30, 35);                       // legs
        glLineWidth(2.5f);
        glBegin(GL_LINES);
            glVertex2f(qx, hipY); glVertex2f(qx-0.16f, fy);
            glVertex2f(qx, hipY); glVertex2f(qx+0.16f, fy);
        glEnd();
        glColor3ub(cr, cg, cb);                       // coat
        glLineWidth(5.0f);
        glBegin(GL_LINES); glVertex2f(qx, hipY); glVertex2f(qx, hipY+1.15f); glEnd();
        FilledCircle4(qx, hipY+1.45f, 0.30f, 226, 188, 150, 255);
        glColor3ub(cr, cg, cb);                       // hat
        glBegin(GL_TRIANGLES);
            glVertex2f(qx-0.30f, hipY+1.65f); glVertex2f(qx+0.30f, hipY+1.65f);
            glVertex2f(qx,       hipY+2.20f);
        glEnd();
        glColor4ub(246, 249, 255, 230);               // snow settling on it
        glBegin(GL_TRIANGLES);
            glVertex2f(qx-0.16f*SnowDepth4(), hipY+1.95f);
            glVertex2f(qx+0.16f*SnowDepth4(), hipY+1.95f);
            glVertex2f(qx,                  hipY+2.20f);
        glEnd();
        // Breath, drifting toward the counter they are waiting at
        for (int k = 0; k < 2; k++) {
            float t = fmodf(firePhase4 * 0.25f + k * 0.5f + i * 0.2f, 1.0f);
            FilledCircle4(qx + 0.35f + t * 0.9f, hipY + 1.40f + t * 0.35f,
                          0.10f + t * 0.18f, 226, 234, 244,
                          (unsigned char)(95 * (1.0f - t)));
        }
        glLineWidth(1.0f);
    }
}

void DrawStalls4() {
    for (int i = 0; i < NUM_STALLS4; i++) DrawStall4(stalls4[i]);
    // Both queues form on the stall's left, into open paving rather than back
    // under the ferris wheel, so the customers stay visible.
    DrawStallQueue4(stalls4[1].x, 2);
    DrawStallQueue4(stalls4[3].x, 2);
}

// ---- Christmas tree centerpiece, with twinkling ornaments ------------------
// In autumn this is the same tree in the same place, but the needles turn and
// the baubles become paper lanterns -- the market stays, it is just a
// different month.
void DrawChristmasTree4() {
    float x = treeX4, baseY = -9.0f;
    glColor3ub(80, 55, 35);
    glBegin(GL_QUADS);
        glVertex2f(x-0.4f, baseY-0.5f); glVertex2f(x+0.4f, baseY-0.5f);
        glVertex2f(x+0.4f, baseY);      glVertex2f(x-0.4f, baseY);
    glEnd();
    // Needles turn from winter green to a burnt autumn gold. Each tier is
    // shaded slightly differently so the cone has some form to it.
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
    // Snow settled on each tier -- melts out of season with everything else
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
    // Star in winter, a simple finial in autumn
    FilledCircle4(x, baseY+7.7f, 0.35f, MixSB4(255, 210), MixSB4(220, 170), MixSB4(80, 96), 255);

    float ornX[12] = { -2.5f, 1.8f, -1.0f, 2.6f, -3.2f, 0.5f, -1.6f, 2.0f, -0.6f, 1.2f, -2.0f, 0.2f };
    float ornY[12] = {  0.6f, 1.0f,  1.8f, 2.2f,  2.8f, 3.2f,  3.9f, 4.3f,  4.9f, 5.3f,  5.9f, 6.3f };
    // The 'L' toggle reaches the tree too, so one key shifts the mood of the
    // whole plaza rather than only the string lights.
    unsigned char warm[3][3] = { {255, 196, 120}, {255, 214, 150}, {255, 176,  96} };
    unsigned char many[3][3] = { {230,  60,  60}, { 60, 140, 230}, {240, 210,  60} };
    for (int i = 0; i < 12; i++) {
        float tw = 0.5f + 0.5f * sinf(twinklePhase4*2.0f + i*0.7f);
        int c = i % 3;
        unsigned char r = multicolorLights4 ? many[c][0] : warm[c][0];
        unsigned char g = multicolorLights4 ? many[c][1] : warm[c][1];
        unsigned char b = multicolorLights4 ? many[c][2] : warm[c][2];
        // Halo first, then the bauble, so each ornament reads as a light.
        FilledCircle4(x+ornX[i], baseY+ornY[i], 0.75f, r, g, b, (unsigned char)(60 * tw));
        FilledCircle4(x+ornX[i], baseY+ornY[i], 0.22f, r, g, b, (unsigned char)(150 + 105*tw));

        // In autumn the baubles hang as little paper lanterns instead: the
        // same lights, dressed for a different festival.
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

void DrawGiftBox4(float x, float y, unsigned char r, unsigned char g, unsigned char b) {
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

void DrawGiftBoxes4() {
    DrawGiftBox4(-2.0f, -9.0f, 200, 60, 70);
    DrawGiftBox4(-1.0f, -9.0f, 60, 140, 200);
    DrawGiftBox4( 2.2f, -9.0f, 220, 180, 60);
}

// ---- Snowman family --------------------------------------------------
void DrawSnowman4(float x, float scale) {
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

void DrawSnowmanFamily4() {
    if (SeasonWinter4() < 0.02f) return;   // replaced by the planters
    DrawSnowman4(20.0f, 1.0f);
    DrawSnowman4(21.6f, 0.6f);
    DrawSnowman4(18.6f, 0.5f);
}

// ---- Santa, already delivered, standing out on the plaza ------------------
//  Two things were wrong with the old figure.
//
//  Size: he was 2.5 units tall with a 0.32 head, which made him SMALLER than
//  the pedestrians walking in front of him -- backwards, since he stands
//  further back and everything further back should be smaller, not larger.
//  He is now built at roughly 1.35x with proportions that read at this scale:
//  a barrel torso, a real beard, boots and a proper sack.
//
//  Position: at x = 4.5 on y = -9 his hat reached y = -6.5, which is inside
//  the GIFTS shopfront band (x 2.3 .. 16.1, y -7.4 .. -1.0), so he appeared to
//  be standing INSIDE the shop window. Moving him forward to y = -10.2 puts
//  him on the plaza in front of the row, where he belongs.
//  Only the part of a figure ABOVE y = -7.4 can overlap a shopfront at all,
//  and at this size that is just the tip of his hat -- so he is parked in the
//  clear gap between the BAKERY front (ends x = -0.6) and the GIFTS front
//  (starts x = 2.3). Nothing of him crosses a window.
constexpr float SANTA_X4 = 0.85f;
constexpr float SANTA_Y4 = -10.2f;

void DrawSanta4() {
    if (SeasonWinter4() < 0.02f) return;   // replaced by the chestnut roaster
    const float x = SANTA_X4, y = SANTA_Y4;
    // 1.05 rather than the 1.35 first tried: he has to stand a little taller
    // than the crowd without out-scaling the walkers who are NEARER than he
    // is, at y = -13.9.
    const float S = 1.05f;

    DrawGroundShadow(x, y, 1.05f * S, 0.30f, 86);

    // Boots
    glColor3ub(26, 26, 30);
    for (int i = -1; i <= 1; i += 2) {
        glBegin(GL_QUADS);
            glVertex2f(x + i*0.10f*S - 0.24f*S, y);
            glVertex2f(x + i*0.10f*S + 0.24f*S, y);
            glVertex2f(x + i*0.10f*S + 0.24f*S, y + 0.42f*S);
            glVertex2f(x + i*0.10f*S - 0.24f*S, y + 0.42f*S);
        glEnd();
    }

    // Coat: barrel-shaped, widest at the belly
    glColor3ub(196, 44, 44);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.52f*S, y + 0.30f*S);
        glVertex2f(x + 0.52f*S, y + 0.30f*S);
        glVertex2f(x + 0.60f*S, y + 0.95f*S);
        glVertex2f(x + 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.60f*S, y + 0.95f*S);
    glEnd();
    // Shading down the left side, so he is not a flat red slab
    glColor4ub(150, 30, 32, 170);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.52f*S, y + 0.30f*S);
        glVertex2f(x - 0.22f*S, y + 0.30f*S);
        glVertex2f(x - 0.20f*S, y + 1.62f*S);
        glVertex2f(x - 0.46f*S, y + 1.62f*S);
        glVertex2f(x - 0.60f*S, y + 0.95f*S);
    glEnd();

    // Fur hem and cuffs
    glColor3ub(248, 248, 250);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.56f*S, y + 0.28f*S); glVertex2f(x + 0.56f*S, y + 0.28f*S);
        glVertex2f(x + 0.56f*S, y + 0.50f*S); glVertex2f(x - 0.56f*S, y + 0.50f*S);
    glEnd();

    // Belt and buckle
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

    // Arms: one down at his side, one raised in a wave
    glColor3ub(196, 44, 44);
    glLineWidth(5.4f * S);
    float wave = sinf(twinklePhase4 * 1.6f) * 0.16f;
    glBegin(GL_LINES);
        glVertex2f(x - 0.48f*S, y + 1.46f*S); glVertex2f(x - 0.88f*S, y + 0.86f*S);
        glVertex2f(x + 0.48f*S, y + 1.46f*S);
        glVertex2f(x + 1.00f*S, y + (2.06f + wave)*S);
    glEnd();
    glColor3ub(248, 248, 250);                       // mittens
    FilledCircle4(x - 0.92f*S, y + 0.80f*S, 0.17f*S, 248, 248, 250, 255);
    FilledCircle4(x + 1.04f*S, y + (2.12f + wave)*S, 0.17f*S, 248, 248, 250, 255);

    // Head, beard, moustache
    FilledCircle4(x, y + 1.92f*S, 0.34f*S, 244, 202, 166, 255);
    glColor3ub(250, 250, 252);
    glBegin(GL_POLYGON);                             // beard, tapering to a point
        glVertex2f(x - 0.36f*S, y + 1.96f*S);
        glVertex2f(x + 0.36f*S, y + 1.96f*S);
        glVertex2f(x + 0.24f*S, y + 1.58f*S);
        glVertex2f(x,           y + 1.40f*S);
        glVertex2f(x - 0.24f*S, y + 1.58f*S);
    glEnd();
    FilledCircle4(x - 0.15f*S, y + 1.99f*S, 0.11f*S, 250, 250, 252, 255);   // moustache
    FilledCircle4(x + 0.15f*S, y + 1.99f*S, 0.11f*S, 250, 250, 252, 255);
    FilledCircle4(x,           y + 2.02f*S, 0.09f*S, 232, 158, 132, 255);   // nose
    glColor3ub(40, 34, 34);
    FilledCircle4(x - 0.13f*S, y + 2.12f*S, 0.045f*S, 40, 34, 34, 255);
    FilledCircle4(x + 0.13f*S, y + 2.12f*S, 0.045f*S, 40, 34, 34, 255);

    // Hat: a slumped cone, not a rigid triangle
    glColor3ub(196, 44, 44);
    glBegin(GL_POLYGON);
        glVertex2f(x - 0.36f*S, y + 2.20f*S);
        glVertex2f(x + 0.36f*S, y + 2.20f*S);
        glVertex2f(x + 0.52f*S, y + 2.72f*S);
        glVertex2f(x + 0.46f*S, y + 2.94f*S);
    glEnd();
    glColor3ub(248, 248, 250);
    glBegin(GL_QUADS);                               // hat band
        glVertex2f(x - 0.40f*S, y + 2.16f*S); glVertex2f(x + 0.40f*S, y + 2.16f*S);
        glVertex2f(x + 0.40f*S, y + 2.34f*S); glVertex2f(x - 0.40f*S, y + 2.34f*S);
    glEnd();
    FilledCircle4(x + 0.50f*S, y + 2.98f*S, 0.16f*S, 248, 248, 250, 255);   // bobble

    // Sack, leaning against his leg with a couple of parcels showing
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
    glBegin(GL_LINES);                               // the tie at the neck
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

// ---- Ice sculptures near the rink -------------------------------------
void DrawIceSculpture4(float x, float scale) {
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

void DrawIceSculptures4() {
    if (SeasonWinter4() < 0.02f) return;   // ice sculptures do not survive autumn
    DrawIceSculpture4(13.0f, 1.0f);
    DrawIceSculpture4(17.5f, 0.8f);
}

// Variant that places an ice sculpture on an arbitrary base Y (useful for
// putting sculptures on the terrace/footpath rather than the rink area).
void DrawIceSculptureAt(float x, float baseY, float scale) {
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

// ---- Small stage with a live band -------------------------------------
void DrawStage4() {
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

// ---- Nutcracker soldier statue -----------------------------------------
// Moved forward from y = -9 to y = -10.4: at the old height his shako reached
// y = -6.1, inside the BAKERY shopfront band (x -13.8 .. -0.6, y -7.4 .. -1.0),
// so the figure stood inside the shop window instead of in front of it.
void DrawNutcracker4(float x) {
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

// ---- Clock tower with a swinging pendulum ---------------------------------
void DrawClockTower4() {
    // topY has to clear the shop roofs on either side (SKATES peaks at 16.6,
    // CIDER at 15.3) or the "tower" reads as the shortest thing on the row.
    float x = clockX4, baseY = -6.0f, topY = 21.0f;
    glColor3ub(60, 55, 65);
    glBegin(GL_QUADS);
        glVertex2f(x-1.5f, baseY); glVertex2f(x+1.5f, baseY);
        glVertex2f(x+1.5f, topY);  glVertex2f(x-1.5f, topY);
    glEnd();
    // Backlit dial: it is a night scene, so the face has to be a light source.
    FilledCircle4(x, topY-2.0f, 2.8f, 255, 226, 170, 34);
    FilledCircle4(x, topY-2.0f, 1.3f, 250, 242, 214, 255);
    // Tick marks around the dial
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

    // The hands show the real wall-clock time, so a viewer who glances at
    // the tower gets something true rather than an arbitrary sweep -- and
    // the second hand gives the scene a visible, steady tick.
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
    // Pendulum, glimpsed through a slit below the clock face
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

void UpdatePendulum4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdatePendulum4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) pendulumAngle4 += 0.05f;
    glutTimerFunc(20, UpdatePendulum4, 0);
}

// ---- Small caroling group --------------------------------------------
void DrawCarolers4() {
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
    // Music notes rising off the group. They used to climb to y = -4.5, which
    // is inside the SKATES shopfront band (x 34.5 .. 45.9, y -7.4 .. -1.0), so
    // they drifted up through the shop window. The rise is now clamped to stay
    // under the fronts, and they fade out before they get there anyway.
    const float noteCeil = -7.7f;
    for (int i = 0; i < 3; i++) {
        float t = fmodf(twinklePhase4*3.0f + i*0.7f, 2.0f);
        float nx = baseX - 0.5f + i*0.6f + sinf(t*3.0f)*0.3f;
        float ny = y + 1.9f + t*0.55f;
        if (ny > noteCeil) ny = noteCeil;
        unsigned char alpha = (unsigned char)(200 * (1.0f - t/2.0f));
        FilledCircle4(nx, ny, 0.12f, 255, 255, 255, alpha);
        // A stem, so they read as notes rather than snowflakes
        glColor4ub(255, 255, 255, alpha);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
            glVertex2f(nx + 0.11f, ny); glVertex2f(nx + 0.11f, ny + 0.34f);
        glEnd();
        glLineWidth(1.0f);
    }
}

// ---- Occasional firework bursts --------------------------------------
void DrawFireworks4() {
    for (int i = 0; i < MAX_FIREWORKS4; i++) {
        const Firework4& f = fireworks4[i];
        if (f.state == FW_INACTIVE) continue;

        if (f.state == FW_RISING) {
            // Rocket: bright head plus a short fading exhaust tail.
            FilledCircle4(f.x, f.y, 0.18f, 255, 240, 200, 255);
            for (int t = 1; t <= 5; t++) {
                FilledCircle4(f.x, f.y - t * 0.5f, 0.13f - t * 0.02f,
                              255, 190, 110, (unsigned char)(150 - t * 28));
            }
            continue;
        }

        // Bursting: each spark flies along its own stored direction and
        // speed, with gravity pulling the whole shell down over time. The
        // old version spread sparks purely radially, which read as a comet.
        float t = f.phase;
        for (int s = 0; s < 18; s++) {
            float d  = f.sparkSpd[s] * t * 7.0f;
            float px = f.x + d * cosf(f.sparkAng[s]);
            float py = f.y + d * sinf(f.sparkAng[s]) - t * t * 5.0f;
            unsigned char a = (unsigned char)(255 * (1.0f - t));
            FilledCircle4(px, py, 0.16f * (1.0f - t * 0.5f), f.r, f.g, f.b, a);
        }
        // Flash halo at the burst centre early on
        if (t < 0.25f) {
            FilledCircle4(f.x, f.y, 2.2f * t / 0.25f, f.r, f.g, f.b,
                          (unsigned char)(90 * (1.0f - t / 0.25f)));
        }
    }
}

// Bursting fireworks briefly light the snow below them.
void DrawFireworkGroundFlash4() {
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

void UpdateFireworks4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFireworks4, 0); return; }
    if (isAnimating4) {
        fireworkCooldown4--;
        if (fireworkCooldown4 <= 0) {
            for (int i = 0; i < MAX_FIREWORKS4; i++) {
                if (fireworks4[i].state == FW_INACTIVE) {
                    Firework4& f = fireworks4[i];
                    f.state   = FW_RISING;
                    f.x       = -45.0f + (rand() % 900) / 10.0f;
                    f.y       = -5.0f;                       // launches from the plaza
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

// ---- Aurora / northern lights -- a rare, slow-fading sky effect -----------
//  Unchanged in behaviour: X still forces it, the timer still runs it on its
//  own cooldown. What changed is that it is now part of the sky rather than
//  three flat ribbons laid over it. Each curtain is drawn as a bright core
//  with two wider, fainter shells either side of it, so the edges bleed into
//  the air instead of stopping on a hard line, and the whole effect is scaled
//  by NightT4() so a daylight sky does not end up with ribbons stapled to it.
void DrawAurora4() {
    if (!auroraActive4) return;
    float envelope = sinf(auroraTimer4 * PI4);   // fades in, peaks, fades out
    if (envelope <= 0.0f) return;
    envelope *= 0.18f + 0.82f * NightT4();

    const unsigned char cols[4][3] = {
        { 56, 214, 142 }, { 74, 202, 188 }, { 82, 142, 230 }, { 168, 104, 220 }
    };
    for (int band = 0; band < 4; band++) {
        float baseY = 28.5f + band * 2.7f;
        float h     = 5.4f + band * 1.2f;
        for (int shell = 0; shell < 3; shell++) {
            float spread = 1.0f + shell * 1.05f;                 // wider, fainter
            float aMul   = (shell == 0) ? 1.0f : 0.34f / (float)shell;
            glBegin(GL_QUAD_STRIP);
            for (int i = 0; i <= 30; i++) {
                float x    = -62.0f + i * 4.2f;
                float wave = sinf(auroraPhase4 + i * 0.30f + band * 1.45f) * 2.7f
                           + sinf(auroraPhase4 * 0.55f + i * 0.12f) * 1.5f;
                float yc   = baseY + wave;
                // Vertical rays: the brightness ripples along the curtain,
                // which is what an aurora does and a flat ribbon does not.
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
    // A very faint green cast on the air below the curtains, so they light
    // the sky they hang in rather than floating clear of it.
    glBegin(GL_QUADS);
        glColor4ub(70, 190, 150, (unsigned char)(26.0f * envelope));
        glVertex2f(-60.0f, 30.0f); glVertex2f(60.0f, 30.0f);
        glColor4ub(70, 190, 150, 0);
        glVertex2f( 60.0f,  8.0f); glVertex2f(-60.0f, 8.0f);
    glEnd();
}

void UpdateAurora4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateAurora4, 0); return; }  // idle while this scenario is off-screen
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

void SpawnStallSteam4() {
    for (int i = 0; i < MAX_STEAM4; i++) {
        if (!stallSteam4[i].active) {
            stallSteam4[i].active = true;
            stallSteam4[i].x = stalls4[0].x + (rand()%40-20)/100.0f;  // the cocoa urn
            stallSteam4[i].y = StallCounterTopY4() + 0.4f;
            stallSteam4[i].vy = 0.04f + (rand()%20)/1000.0f;
            stallSteam4[i].alpha = 130.0f;
            stallSteam4[i].size = 0.35f + (rand()%15)/100.0f;
            return;
        }
    }
}

void DrawStallSteam4() {
    for (int i = 0; i < MAX_STEAM4; i++) {
        if (!stallSteam4[i].active) continue;
        FilledCircle4(stallSteam4[i].x, stallSteam4[i].y, stallSteam4[i].size, 220, 220, 225, (unsigned char)stallSteam4[i].alpha);
    }
}

void UpdateStallSteam4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateStallSteam4, 0); return; }  // idle while this scenario is off-screen
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

void SpawnChimneySmoke4() {
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

void DrawChimneySmoke4() {
    for (int i = 0; i < MAX_SMOKE4; i++) {
        if (!chimneySmoke4[i].active) continue;
        FilledCircle4(chimneySmoke4[i].x, chimneySmoke4[i].y, chimneySmoke4[i].size, 180, 180, 190, (unsigned char)chimneySmoke4[i].alpha);
    }
}

void UpdateChimneySmoke4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateChimneySmoke4, 0); return; }  // idle while this scenario is off-screen
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

// ---- Settled snow: caps on roofs, tree tiers, stalls, clock spire -------
void DrawSnowAccumulation4() {
    // Every cap, ledge and drift in here is settled SNOW, so the whole pass
    // fades out as autumn comes in rather than each piece being branched.
    if (SeasonWinter4() < 0.02f) return;
    float c = SnowDepth4();                 // 0.20 .. 0.90
    glColor3ub(246, 249, 255);

    // Shop roof caps, thickness scaling with cover
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
        // Icicles hanging off the eaves once there is real snow
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

    // Snow lying along the two slopes of each stall canopy. It used to be one
    // flat bar drawn at the apex height across the full canopy width, which
    // floated clear of the tent below it.
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

    // Snow banks along the left and right edges of the plaza
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

    // Clock tower spire cap
    glBegin(GL_TRIANGLES);
        glVertex2f(clockX4 - 1.5f * c, 16.4f);
        glVertex2f(clockX4 + 1.5f * c, 16.4f);
        glVertex2f(clockX4,            16.4f + 1.2f * c);
    glEnd();
}

// ---- Squall: layered drifting bands of snow-haze --------------------------
// ============================================================================
//  AUTUMN EQUIVALENTS
// ----------------------------------------------------------------------------
//  The four things in this plaza that only make sense in deep winter -- the
//  ice rink, the snowman family, the dressed Christmas tree and Santa -- each
//  get a counterpart that occupies the same ground. Hiding them instead would
//  have left four visible holes in the square; swapping them means both
//  seasons look deliberate.
//
//  Each pair crossfades on seasonT4, so for a second or two mid-transition the
//  rink really is dissolving into the fountain rather than one popping out.
// ============================================================================

// ---- Plaza fountain, where the rink is in winter --------------------------
void DrawFountainAutumn4() {
    if (SeasonAutumn4() < 0.02f) return;
    const unsigned char A = AutumnA4(255);
    const float cx = rinkCX4, cy = rinkCY4;

    // Wet apron where splash has darkened the paving
    glColor4ub(MixB4(74, 108), MixB4(80, 100), MixB4(72, 86), AutumnA4(120));
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 7.4f*cosf(a), cy + 2.7f*sinf(a));
        }
    glEnd();

    // Stone basin rim
    glColor4ub(MixB4(112, 156), MixB4(110, 150), MixB4(104, 138), A);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 6.0f*cosf(a), cy + 2.2f*sinf(a));
        }
    glEnd();
    // Water inside it
    glColor4ub(MixB4(30, 58), MixB4(62, 104), MixB4(74, 118), A);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 30; i++) {
            float a = (float)i / 30 * 2 * PI4;
            glVertex2f(cx + 5.1f*cosf(a), cy + 1.75f*sinf(a));
        }
    glEnd();
    // Coping stones round the rim
    glColor4ub(MixB4(86, 124), MixB4(84, 118), MixB4(80, 108), A);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 20; i++) {
            float a = (float)i / 20 * 2 * PI4;
            glVertex2f(cx + 5.1f*cosf(a), cy + 1.75f*sinf(a));
            glVertex2f(cx + 6.0f*cosf(a), cy + 2.2f*sinf(a));
        }
    glEnd();

    // Central pedestal and bowl
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

    // Jets arcing out of the bowl and falling back into the basin
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
        // A droplet running the arc, so the water reads as moving
        float dt = fmodf(firePhase4 * 0.5f + j * 0.14f, 0.55f);
        float dx = cx + vx * dt;
        float dy = cy + 2.5f + vy * dt - 0.5f * 22.0f * dt * dt;
        if (dy > cy + 0.1f) FilledCircle4(dx, dy, 0.13f, 236, 248, 255, AutumnA4(220));
    }

    // Expanding rings where the jets land
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

    // Two people sitting on the rim with their backs to us
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

// ---- Planters, where the snowmen stand in winter --------------------------
void DrawPlanter4(float x, float scale) {
    const unsigned char A = AutumnA4(255);
    const float y = -9.0f;

    DrawGroundShadow(x, y, 1.5f * scale, 0.28f, AutumnA4(80));

    // Tapered terracotta tub
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

    // Autumn planting spilling over the rim
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

void DrawPlantersAutumn4() {
    if (SeasonAutumn4() < 0.02f) return;
    DrawPlanter4(20.0f, 1.0f);
    DrawPlanter4(21.9f, 0.7f);
    DrawPlanter4(18.4f, 0.6f);

    // A stack of pumpkins beside them -- the one prop that says autumn on
    // sight, the way a snowman says winter.
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

// ---- Chestnut roaster, where Santa stands in winter -----------------------
void DrawChestnutRoaster4() {
    if (SeasonAutumn4() < 0.02f) return;
    const unsigned char A = AutumnA4(255);
    const float x = SANTA_X4, y = SANTA_Y4;

    DrawGroundShadow(x, y, 1.5f, 0.30f, AutumnA4(86));

    // Brazier on legs
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

    // Coals and the heat coming off them
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

    // Chestnuts on the grill
    for (int i = 0; i < 6; i++)
        FilledCircle4(x - 0.62f + i * 0.25f, y + 1.84f, 0.13f,
                      MixB4(92, 128), MixB4(52, 74), MixB4(34, 44), A);

    // The vendor, turning them over
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

    // A paper cone of them on the brazier edge
    glColor4ub(224, 208, 178, A);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 1.20f, y + 1.78f);
        glVertex2f(x - 0.72f, y + 1.78f);
        glVertex2f(x - 0.96f, y + 1.06f);
    glEnd();
    glLineWidth(1.0f);
}

// ---- Snow squall ----------------------------------------------------------
//  The old version drew four 140-unit quads sliding sideways on fmodf. Three
//  things gave it away as fake:
//    * the top edge of every band was a dead-straight horizontal line
//    * all four wrapped at the same moment, so the whole squall visibly
//      jumped every time the modulo came round
//    * they were flat lateral slabs with no internal structure, so nothing
//      ever looked like it was moving THROUGH the scene
//
//  This builds the squall out of soft overlapping blobs instead. Each one
//  carries its own drift rate, size, height and phase and wraps on its own, so
//  the mass churns continuously and never pops. A slow breathing term thickens
//  and thins the whole bank, and heavier snow settings put more of it in the
//  air.
constexpr int FOG_BLOBS4 = 15;

void DrawFog4(float y0, float y1, unsigned char alpha) {
    if (fogAmount4 < 0.01f) return;

    // Denser squall when the snow is heavier -- the two controls belong to the
    // same weather rather than being independent switches.
    float density = 0.72f + 0.20f * snowIntensity4;
    float breathe = 0.86f + 0.14f * sinf(fogPhase4 * 0.21f);
    float band    = y1 - y0;

    for (int i = 0; i < FOG_BLOBS4; i++) {
        // Stable per-blob character, so a blob keeps its identity frame to
        // frame instead of being reshuffled.
        float h1 = sinf(i * 12.9898f) * 43758.5453f;
        float h2 = sinf(i * 78.2330f) * 12345.6789f;
        float h3 = sinf(i * 45.1640f) * 27182.8182f;
        float j1 = h1 - floorf(h1);
        float j2 = h2 - floorf(h2);
        float j3 = h3 - floorf(h3);

        // Each blob has its own speed, so they shear past one another and the
        // bank never moves as one rigid sheet.
        float speed = 0.30f + 0.85f * j1;
        float span  = 150.0f;
        float cx    = fmodf(j2 * span + fogPhase4 * speed, span) - span * 0.5f;

        // Vertical drift on its own slow cycle
        float cy = y0 + band * (0.15f + 0.75f * j3)
                      + sinf(fogPhase4 * 0.13f + i * 1.7f) * band * 0.10f;

        float rx = (9.0f + 16.0f * j3) * (0.85f + 0.30f * j1);
        float ry = band * (0.16f + 0.20f * j2) * breathe;

        unsigned char a = (unsigned char)(alpha * fogAmount4 * density
                                          * (0.34f + 0.40f * j1));

        // Slightly warmer low down where the market light reaches into it,
        // colder higher up.
        float warm = 1.0f - (cy - y0) / (band + 0.001f);
        DrawSoftEllipse(cx, cy, rx, ry,
                        (unsigned char)(196 + 22 * warm),
                        (unsigned char)(208 + 12 * warm),
                        (unsigned char)(230 - 10 * warm),
                        a, 3);
    }

    // A thin veil tying the blobs together, so the bank reads as one body of
    // air rather than a row of separate clouds. Its edge is broken by a
    // shallow wave instead of being a ruled line.
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

void UpdateSeason4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSeason4, 0); return; }
    if (isAnimating4) {
        // About three seconds end to end, matching the day/night ease so the
        // two controls feel like parts of the same system.
        seasonT4 += (seasonTarget4 - seasonT4) * 0.011f;
        if (fabsf(seasonTarget4 - seasonT4) < 0.002f) seasonT4 = seasonTarget4;
    }
    glutTimerFunc(30, UpdateSeason4, 0);
}

void UpdateFog4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateFog4, 0); return; }
    if (isAnimating4) {
        fogPhase4 += 0.13f;
        float tgt = fogMode4 ? 1.0f : 0.0f;
        fogAmount4 += (tgt - fogAmount4) * 0.02f;
    }
    glutTimerFunc(30, UpdateFog4, 0);
}

// ============================================================================
//  NEAR TERRACE -- the foreground the plaza never had
// ----------------------------------------------------------------------------
//  Everything in this scenario stood on one line: shops, stalls, tree, Santa,
//  snowmen and stage all based at y = -9, walkers at y = -12, the rink
//  bottoming out at -16.7. Below that, 23 world units -- 29% of the window --
//  was a single flat grey quad.
//
//  A low wall at y = -20 splits that dead band into a real near terrace, and
//  everything below it now belongs to the foreground: a frozen canal that
//  reflects the market, an oversized cropped lamp post, large figures seen
//  from behind, and a sledding run.
// ============================================================================
constexpr float terraceY4 = -20.0f;
constexpr float canalTopY4 = -27.0f;
constexpr float canalBotY4 = -36.0f;

// ---- Low stone wall with a snow cap --------------------------------------
void DrawTerraceWall4() {
    // Wall face
    glBegin(GL_QUADS);
        glColor3ub(96, 104, 120);
        glVertex2f(-60.0f, terraceY4 - 2.6f); glVertex2f(60.0f, terraceY4 - 2.6f);
        glColor3ub(126, 134, 150);
        glVertex2f(60.0f, terraceY4);         glVertex2f(-60.0f, terraceY4);
    glEnd();

    // Coursing
    glColor4ub(72, 80, 96, 170);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = -30; i <= 30; i++) {
            float x = i * 4.0f;
            glVertex2f(x, terraceY4 - 2.6f); glVertex2f(x, terraceY4);
        }
        glVertex2f(-60.0f, terraceY4 - 1.3f); glVertex2f(60.0f, terraceY4 - 1.3f);
    glEnd();

    // Snow sitting on the coping, thickness following the snow cover. Out of
    // season there is none, so the cap thins to nothing.
    float cap = 0.45f * SeasonWinter4() + 1.1f * SnowDepth4();
    glColor3ub(246, 249, 255);
    glBegin(GL_QUADS);
        glVertex2f(-60.0f, terraceY4);
        glVertex2f( 60.0f, terraceY4);
        glVertex2f( 60.0f, terraceY4 + cap);
        glVertex2f(-60.0f, terraceY4 + cap);
    glEnd();

    // Drifts pushed up against the base of the wall
    glColor4ub(238, 243, 252, 220);
    for (int i = 0; i < 9; i++) {
        float cx = -54.0f + i * 13.5f + sinf(i * 2.7f) * 3.0f;
        DrawSoftEllipse(cx, terraceY4 - 2.6f, 9.0f, 1.6f * SnowDepth4() + 0.6f,
                        238, 243, 252, 200, 2);
    }
    glLineWidth(1.0f);
}

// ---- Frozen canal --------------------------------------------------------
//  The market's lights smear down its surface as vertical streaks, which is
//  what ties the foreground to the lit scene above it.
void DrawFrozenCanal4() {
    // Ice sheet
    glBegin(GL_QUADS);
        glColor3ub(MixB4(38, 90), MixB4(52, 140), MixB4(76, 160));
        glVertex2f(-60.0f, canalBotY4); glVertex2f(60.0f, canalBotY4);
        glColor3ub(MixB4(66, 120), MixB4(86, 160), MixB4(116, 190));
        glVertex2f(60.0f, canalTopY4);  glVertex2f(-60.0f, canalTopY4);
    glEnd();

    // Vertical light smears: one per source above, wobbling as the ice
    // catches them at slightly different angles.
    struct Smear { float x; unsigned char r, g, b; float w; };
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

    // Skate scars and a crack or two across the ice
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

// ============================================================================
//  RIVERSIDE  (added on top of the baseline market/canal scene)
// ----------------------------------------------------------------------------
//  Replaces the frozen canal with a flowing river at the very front of the
//  scene, reusing its exact footprint -- riverTopY4/riverBotY4 below are
//  just the old canalTopY4/canalBotY4 -- so nothing else in the layout had
//  to move. New depth layers, back to front, matching the existing ones:
//      1. market/ground    -- unchanged
//      2. footpath         -- the existing terrace top (terraceY4), unchanged
//      3. street lights    -- the existing DrawForegroundLamp4 posts, unchanged
//      4. stairs           -- new: DrawStairsAll4()
//      5. river / boats    -- new: DrawRiver4() + DrawBoats4() +
//                             DrawGoodsUnloading4() + DrawRiverPedestrians4()
//
//  VERSION CONTROL: to undo just this addition, in Draw() swap the new
//  near-terrace block back for the single `DrawFrozenCanal4();` call it
//  replaced (that function is left intact above, just unused), remove the
//  two glutTimerFunc lines for UpdateBoats4 / UpdateRiverPedestrians4 from
//  Init(), and drop this whole RIVERSIDE section. Nothing outside it changed.
// ============================================================================
// Make the river visually wider than the old canal footprint so boats
// and dock read better in the foreground.
constexpr float riverTopY4 = canalTopY4 + 1.5f;  // far (bank) edge of the water
constexpr float riverBotY4 = canalBotY4 - 3.0f;  // near edge -- expanded deeper than the old ice
constexpr float dockY4     = riverTopY4 + 1.0f;  // narrow bank/dock ledge the stairs land on
constexpr float wallBotY4  = terraceY4 - 2.6f;   // bottom of the retaining wall (matches DrawTerraceWall4)
constexpr float stairX4[2] = { -48.0f, 24.0f };  // two staircases down to the dock

struct Boat4 {
    float x, speed, dir;        // dir/speed ignored while anchored
    float scale, phase;
    unsigned char hullR, hullG, hullB;
    bool anchored;               // sits still at the dock (the unloading boat)
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

struct RiverPed4 { float x, speed, dir, phase, laneY; unsigned char coatR, coatG, coatB; };
constexpr int NUM_RIVER_PEDS4 = 4;
RiverPed4 riverPeds4[NUM_RIVER_PEDS4] = {
    { -20.0f, 0.11f,  1, 0.0f, dockY4 + 0.2f, 150,  40,  50 },
    {   5.0f, 0.09f, -1, 1.4f, dockY4 + 0.9f,  40,  70, 120 },
    {  32.0f, 0.13f,  1, 2.6f, dockY4 + 0.2f, 190, 150,  60 },
    { -55.0f, 0.10f, -1, 3.9f, dockY4 + 0.9f,  70,  90,  80 }
};

float riverPhase4 = 0.0f;   // drives ripples, boat rocking and light flicker

// World position of a boat's hull anchor this frame (gentle rocking on top
// of whatever drift its Update function has already applied to b.x).
inline void BoatPose4(const Boat4& b, float t, float& x, float& y) {
    x = b.x;
    y = riverTopY4 - 1.2f + sinf(t * 1.2f + b.phase) * 0.28f;
}

// ---- Rowing skiff ---------------------------------------------------------
//  The old hull was a six-vertex lozenge and it did not read as a boat:
//    * the bow tip sat at y +0.25 and the stern tip at +0.35, two different
//      heights for no reason
//    * both "tips" were thin horizontal spikes poking out at mid-hull height
//    * the bottom was a dead-straight line from -2.8 to +2.6, so it stopped
//      short of both ends and the hull had no keel running into the stems
//    * no transom, no gunwale, no thwarts, no rocker
//
//  This is a proper clinker skiff instead: a keel with real rocker sweeping up
//  into a raked stem at the bow, a flat transom at the stern, a gunwale strake
//  along the sheer, two thwarts and shipped oars. `a` still carries through to
//  every part so the whole thing can be faded as one object.
void DrawBoatShape4(float x, float y, float scale,
                     unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    const int SEG = 22;

    // Sheer (gunwale line) and keel, as functions of t = -1 at the bow to
    // +1 at the stern. Both curves meet at the bow; at the stern the keel
    // stops short and the transom closes the gap.
    // sheer(t) = 0.62 + 0.34 t^2   : lowest amidships, lifting to both ends
    // keel(t)  = -0.60 + 1.22 t^2  : rocker, rising away from the middle
    const float STERN_T = 0.88f;          // where the transom cuts the hull off

    // ---- Hull ------------------------------------------------------------
    glColor4ub(r, g, b, a);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= SEG; i++) {
        float t  = -1.0f + (1.0f + STERN_T) * (float)i / SEG;
        float hx = x + t * 3.2f * scale;
        float sheer = ( 0.62f + 0.34f * t * t) * scale;
        float keel  = (-0.60f + 1.22f * t * t) * scale;
        if (keel > sheer) keel = sheer;                  // closed point at the bow
        glVertex2f(hx, y + sheer);
        glVertex2f(hx, y + keel);
    }
    glEnd();

    // Planking shadow along the bottom third, which is what makes a hull look
    // round rather than like a flat cut-out.
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

    // ---- Transom ---------------------------------------------------------
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

    // ---- Raked stem at the bow -------------------------------------------
    float bx = x - 3.2f * scale;
    glColor4ub((unsigned char)(r * 0.74f), (unsigned char)(g * 0.74f),
               (unsigned char)(b * 0.74f), a);
    glBegin(GL_TRIANGLES);
        glVertex2f(bx,                 y + 0.96f * scale);
        glVertex2f(bx - 0.34f * scale, y + 1.34f * scale);
        glVertex2f(bx + 0.16f * scale, y + 0.70f * scale);
    glEnd();

    // ---- Gunwale strake --------------------------------------------------
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

    // ---- Thwarts ---------------------------------------------------------
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

    // ---- Oarlock and a shipped oar ---------------------------------------
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
    glBegin(GL_TRIANGLES);                                // blade
        glVertex2f(ox + 1.70f * scale, oy + 0.10f * scale);
        glVertex2f(ox + 2.34f * scale, oy + 0.26f * scale);
        glVertex2f(ox + 2.30f * scale, oy - 0.10f * scale);
    glEnd();

    // ---- Waterline -------------------------------------------------------
    // A pale line where the hull meets the surface, so the boat sits IN the
    // water instead of floating above it.
    glColor4ub(232, 244, 250, (unsigned char)(a * 0.55f));
    glLineWidth(1.3f * scale);
    glBegin(GL_LINES);
        glVertex2f(x - 3.0f * scale, y - 0.30f * scale);
        glVertex2f(tx + 0.24f * scale, y - 0.30f * scale);
    glEnd();
    glLineWidth(1.0f);
}

// ---- Stairs down to the river dock ---------------------------------------
void DrawStairs4(float x) {
    const int STEPS = 6;
    // Connect the top of the stairs to the terrace so there is no gap
    // between the plaza/footpath and the first step.
    float topY = terraceY4; // previously used wallBotY4 which left a visible gap
    float botY = dockY4;
    float dropPer = (topY - botY) / STEPS;
    for (int i = 0; i < STEPS; i++) {
        float y0 = topY - i * dropPer;
        float y1 = y0 - dropPer;
        float w  = 3.2f + i * 0.12f;              // widens slightly toward the viewer
        glColor3ub(MixB4(58, 132), MixB4(60, 118), MixB4(64, 104));   // tread
        glBegin(GL_QUADS);
            glVertex2f(x - w, y1);         glVertex2f(x + w, y1);
            glVertex2f(x + w, y1 + 0.55f); glVertex2f(x - w, y1 + 0.55f);
        glEnd();
        glColor3ub(MixB4(30, 84), MixB4(32, 74), MixB4(36, 66));      // riser, in shadow
        glBegin(GL_QUADS);
            glVertex2f(x - w, y0 - 0.05f); glVertex2f(x + w, y0 - 0.05f);
            glVertex2f(x + w, y1 + 0.55f); glVertex2f(x - w, y1 + 0.55f);
        glEnd();
        float cap = 0.18f + 0.35f * SnowDepth4();                        // snow on the tread edge
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

void DrawStairsAll4() {
    for (int i = 0; i < 2; i++) DrawStairs4(stairX4[i]);

    // Dock ledges the stairs land on -- somewhere for a boat to tie up.
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

// ---- The river itself: base, boat reflections, ripple wash, light smears --
void DrawRiver4() {
    // Winter water runs cold blue; autumn water carries silt off the hills and
    // goes brown-green. Both still mix with the day/night value, so the four
    // combinations (winter night, winter day, autumn night, autumn day) all
    // come out of the same two lines.
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

    // ---- No reflections ---------------------------------------------------
    //  The mirrored boats were removed earlier; the seven vertical light
    //  streaks that stood in for reflections of the wheel, the tree, the
    //  clock, the moon and the boat lanterns have now gone too. Pulling them
    //  out would have left a flat colour wash, so what replaces them is
    //  surface motion rather than surface light: cross-current ripple lines
    //  that bunch toward the far bank, a slow chop drifting downstream, and a
    //  soft foam edge where the water meets the dock.

    // ---- Cross-current ripple lines ---------------------------------------
    //  Spacing follows a squared ramp so the lines crowd together near the far
    //  bank and open out toward the camera, which is what gives a flat band of
    //  colour a sense of lying down and receding.
    const int RIPPLE_ROWS = 16;
    for (int i = 0; i < RIPPLE_ROWS; i++) {
        float u  = (float)i / (RIPPLE_ROWS - 1);        // 0 at the near edge
        float f  = 1.0f - (1.0f - u) * (1.0f - u);      // packed toward the far bank
        float y  = riverBotY4 + f * (riverTopY4 - riverBotY4);
        float amp = 0.30f * (1.0f - f) + 0.06f;         // bigger swell up close
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

    // ---- Chop: short dark troughs drifting with the current ---------------
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

    // ---- Foam line along the far bank -------------------------------------
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

    // Small sliding glints -- what actually reads as "flowing" rather than
    // "still and lit". A handful of short bright dashes drift downstream.
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

// ---- Boats themselves, above the waterline --------------------------------
void DrawBoats4() {
    for (int i = 0; i < NUM_BOATS4; i++) {
        Boat4& b = boats4[i];
        float x, y;
        BoatPose4(b, riverPhase4, x, y);

        DrawGroundShadow(x, riverTopY4 + 0.2f, 2.4f * b.scale, 0.0f, 40);
        DrawBoatShape4(x, y, b.scale, b.hullR, b.hullG, b.hullB, 255);

        glColor4ub(60, 40, 26, 200);                 // thwart (bench)
        glLineWidth(1.6f * b.scale);
        glBegin(GL_LINES);
            glVertex2f(x - 0.6f*b.scale, y + 0.5f*b.scale);
            glVertex2f(x - 0.6f*b.scale, y + 0.9f*b.scale);
        glEnd();

        glColor3ub(90, 64, 40);                       // oar, dipped in the water
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

// ---- Small goods-unloading vignette at the left-hand dock -----------------
void DrawGoodsUnloading4() {
    float dockX = stairX4[0] + 3.2f;
    float bx, by; BoatPose4(boats4[0], riverPhase4, bx, by);

    for (int i = 0; i < 3; i++) {                    // crates already ashore
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

    float wx = dockX + 0.6f, wy = dockY4 + 1.0f;      // dock worker, receiving
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

    float mx = bx + 1.0f, my = by + 0.9f;             // boatman, handing a crate over
    glColor3ub(90, 50, 50);
    glBegin(GL_QUADS);
        glVertex2f(mx - 0.5f, my);          glVertex2f(mx + 0.5f, my);
        glVertex2f(mx + 0.4f, my + 1.4f);   glVertex2f(mx - 0.4f, my + 1.4f);
    glEnd();
    glLineWidth(3.0f);
    glBegin(GL_LINES); glVertex2f(mx, my + 1.1f); glVertex2f(mx + 1.3f, my + 1.3f); glEnd();
    FilledCircle4(mx, my + 1.7f, 0.32f, 224, 186, 148, 255);
    FilledCircle4(mx, my + 2.0f, 0.26f, 90, 50, 50, 255);

    float tCx = (mx + 1.3f + wx - 1.2f + reach) * 0.5f;   // the crate mid-handoff
    float tCy = (my + 1.3f + wy + 1.5f) * 0.5f;
    glColor3ub(120, 84, 50);
    glBegin(GL_QUADS);
        glVertex2f(tCx - 0.4f, tCy - 0.4f); glVertex2f(tCx + 0.4f, tCy - 0.4f);
        glVertex2f(tCx + 0.4f, tCy + 0.4f); glVertex2f(tCx - 0.4f, tCy + 0.4f);
    glEnd();
    glLineWidth(1.0f);
}

// ---- People walking the riverside dock/footpath ---------------------------
void DrawRiverPerson4(const RiverPed4& p, float t) {
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

void DrawRiverPedestrians4() {
    int order[NUM_RIVER_PEDS4];
    for (int i = 0; i < NUM_RIVER_PEDS4; i++) order[i] = i;
    for (int i = 1; i < NUM_RIVER_PEDS4; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && riverPeds4[order[j]].laneY < riverPeds4[key].laneY) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_RIVER_PEDS4; i++) DrawRiverPerson4(riverPeds4[order[i]], pedTimer4);
}

void UpdateBoats4(int) {
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

void UpdateRiverPedestrians4(int) {
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
// ============================================================================
//  END RIVERSIDE
// ============================================================================

// ---- Oversized lamp post, deliberately cropped by the frame edge ---------
//  Nothing sells depth faster than an object too close to fully fit.
void DrawForegroundLamp4(float x, float scale) {
    float baseY = terraceY4 - 2.0f;
    float topY  = baseY + 26.0f * scale;

    glColor3ub(24, 28, 38);
    glLineWidth(6.0f * scale);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();
    // Plinth
    glBegin(GL_QUADS);
        glVertex2f(x - 1.5f*scale, baseY);        glVertex2f(x + 1.5f*scale, baseY);
        glVertex2f(x + 1.0f*scale, baseY + 2.4f*scale); glVertex2f(x - 1.0f*scale, baseY + 2.4f*scale);
    glEnd();

    // Lantern head
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

    // Glass, and the cone of light it throws down onto the snow
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

    // Snowflakes caught crossing the beam
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

    // Pool of light where the beam lands
    DrawSoftEllipse(x, baseY, 9.5f*scale, 2.2f*scale, lr, lg, lb, 80, 4);
    glLineWidth(1.0f);
}

// Smaller plaza lamp placed on the market plaza in front of the shops.
// `baseY` is derived from the shop base so the posts sit on the plaza.
void DrawPlazaLamp4(float x, float scale) {
    float baseY = SHOP_BASE_Y4 - 2.0f; // slightly below the shop base so the lamp stands on the paving
    float topY  = baseY + 12.0f * scale; // shorter than the foreground cropped lamp

    glColor3ub(24, 28, 38);
    glLineWidth(3.0f * scale);
    glBegin(GL_LINES); glVertex2f(x, baseY); glVertex2f(x, topY); glEnd();

    // Lantern head
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

// ---- Foreground figures, seen from behind, watching the market -----------
void DrawBackFigure4(float x, float scale,
                     unsigned char cr, unsigned char cg, unsigned char cb,
                     unsigned char hr, unsigned char hg, unsigned char hb,
                     float sway) {
    float y = terraceY4 - 2.4f;

    // Contact shadow on the snow
    DrawGroundShadow(x, y, 2.2f * scale, 0.4f, 90);

    // Coat: a simple tapered silhouette reads better than a stick figure at
    // this size, and no face is needed because they are turned away.
    glColor3ub(cr, cg, cb);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.55f*scale, y);
        glVertex2f(x + 1.55f*scale, y);
        glVertex2f(x + 1.25f*scale + sway, y + 6.4f*scale);
        glVertex2f(x - 1.25f*scale + sway, y + 6.4f*scale);
    glEnd();
    // Shoulder line
    glColor4ub((unsigned char)(cr*0.75f), (unsigned char)(cg*0.75f), (unsigned char)(cb*0.75f), 255);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.30f*scale + sway*0.8f, y + 5.7f*scale);
        glVertex2f(x + 1.30f*scale + sway*0.8f, y + 5.7f*scale);
        glVertex2f(x + 1.25f*scale + sway,      y + 6.4f*scale);
        glVertex2f(x - 1.25f*scale + sway,      y + 6.4f*scale);
    glEnd();

    // Boots
    glColor3ub(28, 30, 38);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.3f*scale, y - 0.5f*scale); glVertex2f(x - 0.2f*scale, y - 0.5f*scale);
        glVertex2f(x - 0.2f*scale, y + 0.5f*scale); glVertex2f(x - 1.3f*scale, y + 0.5f*scale);
        glVertex2f(x + 0.2f*scale, y - 0.5f*scale); glVertex2f(x + 1.3f*scale, y - 0.5f*scale);
        glVertex2f(x + 1.3f*scale, y + 0.5f*scale); glVertex2f(x + 0.2f*scale, y + 0.5f*scale);
    glEnd();

    // Head, back of a hat, and a scarf tail lifting in the wind
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

    // Breath, because it is cold enough for the horse to have it
    for (int i = 0; i < 3; i++) {
        float t = fmodf(firePhase4 * 0.22f + i * 0.33f, 1.0f);
        FilledCircle4(x + sway + (1.0f + t * 2.2f) * scale,
                      y + (7.4f + t * 0.7f) * scale,
                      (0.25f + t * 0.5f) * scale, 226, 234, 244,
                      (unsigned char)(95 * (1.0f - t)));
    }
    glLineWidth(1.0f);
}

void DrawForegroundCrowd4() {
    // Replace the three oversized back-facing figures on the terrace with
    // permanent ice sculptures sitting on the footpath.
    float sculptureBaseY = terraceY4 - 2.4f;
    DrawIceSculptureAt(-38.0f, sculptureBaseY, 1.15f);
    DrawIceSculptureAt(-32.5f, sculptureBaseY, 0.95f);
    DrawIceSculptureAt( 44.0f, sculptureBaseY, 1.05f);
}

// ---- Sledding run across the near terrace -------------------------------
float sledRunX4 = -70.0f;

void DrawSledRun4() {
    if (SeasonWinter4() < 0.02f) return;   // no snow on the terrace to sled down
    float x = sledRunX4;
    float y = terraceY4 - 4.6f + sinf(x * 0.09f) * 0.5f;   // gentle undulation

    // Snow spray thrown up behind the runners
    for (int i = 0; i < 8; i++) {
        float t = fmodf(firePhase4 * 0.6f + i * 0.13f, 1.0f);
        FilledCircle4(x - 2.2f - t * 5.0f, y + 0.4f + t * 1.9f,
                      0.34f * (1.0f - t), 246, 250, 255,
                      (unsigned char)(185 * (1.0f - t)));
    }

    // Sled
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

    // Child aboard, leaning into the run
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

void UpdateSledRun4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSledRun4, 0); return; }
    if (isAnimating4) {
        sledRunX4 += 0.55f;
        if (sledRunX4 > 72.0f) sledRunX4 = -72.0f - (rand() % 40);
    }
    glutTimerFunc(25, UpdateSledRun4, 0);
}

// ============================================================================
//  PLAZA GROUND / PEDESTRIANS
// ============================================================================
// ---- Plaza paving ---------------------------------------------------------
//  The old version stepped x from -56 to 56 and drew every line from (i, -6)
//  to (i - 3, -40). Because each line shifted left by exactly the same 3
//  units, all eight stayed parallel: a uniform shear, which the eye reads as
//  a tilted wall rather than a floor going away from you. There were also no
//  cross-lines at all, so nothing established scale.
//
//  Now the courses radiate from a vanishing point on the horizon and the
//  horizontal joints compress toward it, which is what actually makes a
//  ground plane recede.
constexpr float plazaVanishX4 = 0.0f;
constexpr float plazaHorizonY4 = -6.0f;
constexpr float plazaNearY4    = -40.0f;

// Maps a 0..1 "distance from the camera" onto a world y, packing the courses
// closer together as they approach the horizon.
inline float PlazaCourseY4(float t) {
    float k = 1.0f - (1.0f - t) * (1.0f - t);          // ease toward the horizon
    return plazaNearY4 + (plazaHorizonY4 - plazaNearY4) * k;
}

void DrawPlazaGround4() {
    // Winter: snow-lit paving, cold and blue near the camera, picking up the
    // town glow at the far edge. Autumn: the snow is gone and the same stone
    // shows through, damp and ochre.
    glBegin(GL_QUADS);
        glColor3ub(MixSB4(198, 128), MixSB4(210, 116), MixSB4(226, 102));
        glVertex2f(-60, plazaNearY4); glVertex2f(60, plazaNearY4);
        glColor3ub(MixSB4(228, 164), MixSB4(233, 148), MixSB4(240, 126));
        glVertex2f(60, plazaHorizonY4);   glVertex2f(-60, plazaHorizonY4);
    glEnd();

    glColor4ub(MixSB4(178, 96), MixSB4(190, 86), MixSB4(208, 74), 150);
    glLineWidth(1.0f);

    // Courses running away from the camera, fanning out from the vanishing
    // point so they converge properly at the horizon.
    glBegin(GL_LINES);
    for (int i = -9; i <= 9; i++) {
        float xTop  = plazaVanishX4 + i * 6.5f;                       // at the horizon
        float xNear = plazaVanishX4 + (xTop - plazaVanishX4) * 3.6f;  // spread out near
        glVertex2f(xTop,  plazaHorizonY4);
        glVertex2f(xNear, plazaNearY4);
    }
    glEnd();

    // Cross joints, spaced so they bunch up toward the horizon.
    glBegin(GL_LINES);
    for (int c = 1; c <= 9; c++) {
        float t = (float)c / 10.0f;
        float y = PlazaCourseY4(t);
        glVertex2f(-60.0f, y);
        glVertex2f( 60.0f, y);
    }
    glEnd();

    // Trodden snow: a broad scuffed path worn between the shops and the
    // rink, wider where it is nearer the camera. In autumn the same route is
    // a wet patch rather than a pale one, so it darkens instead of vanishing.
    glColor4ub(MixSB4(236, 108), MixSB4(241, 98), MixSB4(248, 84), 120);
    glBegin(GL_QUADS);
        glVertex2f(-16.0f, plazaNearY4); glVertex2f(30.0f, plazaNearY4);
        glVertex2f( 10.0f, plazaHorizonY4); glVertex2f(-4.0f, plazaHorizonY4);
    glEnd();

    // ---- Fallen leaves, drifted across the paving -------------------------
    // Only in autumn, and only settled ones: the falling leaves are the snow
    // particle bands doing double duty (see DrawSnowBand4).
    if (SeasonAutumn4() > 0.02f) {
        const unsigned char leafCols[4][3] = {
            {168, 82, 34}, {196, 128, 42}, {140, 62, 40}, {186, 156, 58}
        };
        for (int i = 0; i < 150; i++) {
            float h1 = sinf(i * 12.9898f) * 43758.5453f;
            float h2 = sinf(i * 78.2330f) * 12345.6789f;
            float h3 = sinf(i * 45.1640f) * 27182.8182f;
            float j1 = h1 - floorf(h1), j2 = h2 - floorf(h2), j3 = h3 - floorf(h3);

            // Denser toward the camera, where the paving is bigger on screen.
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

// ============================================================================
//  PERSPECTIVE ROAD
// ----------------------------------------------------------------------------
//  A side street opening in the gap between the corner shop and TOYS, running
//  out of the distance and sweeping away to the left as it reaches the camera.
//
//  Three things make it read as a road going back rather than a pale trapezoid
//  lying on the snow:
//
//    * width. RoadHW4() grows from under two units at the far end to a dozen
//      at the near end, so the carriageway converges hard toward the gap;
//    * curvature. RoadCX4() pulls the centreline left as it comes forward, so
//      the road turns out of the frame instead of pointing at the viewer. A
//      straight road drawn in perspective still reads flat; a curved one does
//      not;
//    * spacing. RoadY4() packs the courses together toward the horizon, which
//      is what a constant-length stretch of road does as it recedes.
//
//  It is drawn straight after the plaza so it is part of the ground: every
//  stall, figure, lamp and the ferris wheel itself all come later and sit on
//  top of it, exactly as they sit on top of the snow.
// ============================================================================
constexpr float roadNearY4 = -20.4f;    // the terrace coping crops it here

// 0 at the horizon, 1 at the near end.
inline float RoadY4 (float t) { return roadFarY4 + (roadNearY4 - roadFarY4) * (0.42f * t + 0.58f * t * t); }
inline float RoadCX4(float t) { return roadFarX4 - 19.2f * powf(t, 1.70f); }
inline float RoadHW4(float t) { return roadFarHW4 + 10.2f * powf(t, 1.28f); }

// Packed-snow road surface at a given point along the run. `side` is -1 on the
// left kerb and +1 on the right: the buildings on the right of the street are
// between the road and the light, so that half sits in their shade.
inline void RoadSurfaceColour4(float t, float y, float side,
                               unsigned char& r, unsigned char& g, unsigned char& b) {
    float depth = 0.86f * (1.0f - t);
    // Packed, part-cleared snow rather than asphalt: this is a winter town, and
    // a genuinely dark ribbon here reads as a hole cut in the plaza instead of
    // as a street. Still clearly darker and greyer than the untouched snow
    // either side, which is what makes the road legible at a glance.
    int nr = 104, ng = 116, nb = 142, dr = 142, dg = 152, db = 170;
    if (side > 0.0f) { nr = 78; ng = 88; nb = 114; dr = 112; dg = 122; db = 142; }
    AirFade4(depth, y, nr, ng, nb, dr, dg, db, r, g, b);
}

void DrawPerspectiveRoad4() {
    const int N = 30;

    // ---- Ploughed verges ---------------------------------------------------
    // Banked snow either side, drawn first and slightly wider than the
    // carriageway, so the road sits down INTO the snow instead of being
    // painted on top of it.
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

    // ---- Carriageway -------------------------------------------------------
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

    // ---- Wheel ruts --------------------------------------------------------
    // Two darker ribbons following the same curve. They are the clearest
    // signal that the surface is a road and not a frozen pond.
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

    // ---- Kerb lines --------------------------------------------------------
    // A bright snow lip on each edge: a hard boundary is what tells the eye
    // where the road surface stops and the verge begins.
    for (int side = -1; side <= 1; side += 2) {
        // The bright lip itself...
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
        // ...and the shadow the banked snow drops onto the carriageway beside
        // it. A bright line alone floats; a bright line with a dark line under
        // it reads as a raised kerb.
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

    // ---- Centre markings ---------------------------------------------------
    // Only the near half: further back the dashes would be sub-pixel, and
    // showing them anyway is the classic way to flatten a perspective road.
    for (int d = 0; d < 7; d++) {
        float t0 = 0.34f + d * 0.095f;
        float t1 = t0 + 0.045f;
        if (t1 > 1.0f) break;
        float y0 = RoadY4(t0), y1 = RoadY4(t1);
        float c0 = RoadCX4(t0), c1 = RoadCX4(t1);
        float w0 = RoadHW4(t0) * 0.035f, w1 = RoadHW4(t1) * 0.035f;
        // Snow keeps covering and uncovering them, so a couple are missing.
        if (d == 2 || d == 5) continue;
        glColor4ub(MixB4(196, 226), MixB4(186, 214), MixB4(150, 178),
                   (unsigned char)(150.0f * t0));
        glBegin(GL_QUADS);
            glVertex2f(c0 - w0, y0); glVertex2f(c0 + w0, y0);
            glVertex2f(c1 + w1, y1); glVertex2f(c1 - w1, y1);
        glEnd();
    }

    // ---- Shade thrown across the road by the buildings on its right --------
    // The light sits up and to the right, so the right-hand terrace lays a
    // wedge of shadow over the carriageway that narrows with distance.
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

    // ---- Drifts caught against the verges -----------------------------------
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

    // ---- Night lighting on the surface --------------------------------------
    // Warm pools from the street's own windows and the market spilling in at
    // the near end, so the road is lit by the scene rather than by nothing.
    float night = NightT4();
    if (night > 0.02f) {
        for (int i = 0; i < 4; i++) {
            float t  = 0.10f + i * 0.27f;
            float y  = RoadY4(t), cx = RoadCX4(t), hw = RoadHW4(t);
            DrawSoftEllipse(cx + hw * 0.35f, y, hw * 0.95f, 0.9f + 1.1f * t,
                            255, 202, 140, (unsigned char)((18 + 16 * t) * night), 4);
        }
    }
    // By day the open street is the brightest strip of ground in the frame.
    if (dayT4 > 0.02f) {
        float y = RoadY4(0.62f), cx = RoadCX4(0.62f), hw = RoadHW4(0.62f);
        DrawSoftEllipse(cx, y, hw * 1.5f, 5.5f, 250, 246, 226,
                        (unsigned char)(52.0f * dayT4), 4);
    }

    // Where the road meets the horizon it should dissolve, not butt into the
    // buildings with a visible seam.
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

// ---- Shadows the buildings lay on the snow --------------------------------
//  Every mass in the scene was previously floating on an unbroken white
//  plaza. The light is up and to the right in both modes, so each facade
//  throws a soft wedge down and to the left of itself. They are deliberately
//  weak: the point is to anchor the row to the ground, not to darken it.
// ============================================================================
//  CYCLE RICKSHAW
// ----------------------------------------------------------------------------
//  The perspective street was built and then left empty. This puts a
//  three-wheeler on it, and takes its position AND its size straight from the
//  road's own functions -- RoadCX4/RoadY4/RoadHW4 -- so it tracks the curve and
//  grows at exactly the rate the carriageway does. Nothing about it is
//  hand-tuned against the road; change the road and the rickshaw follows.
//
//  Three wheels, and all three are meant to be legible: one steered wheel at
//  the front, then a rear axle whose near wheel is drawn full strength and
//  whose far wheel is smaller, darker and offset, so the pair does not
//  collapse into a single bicycle wheel.
// ============================================================================
//  PARKED. Unlike the car and the bicycle, this one does not run: it is stood
//  off the carriageway near the bottom of the street, waiting for a fare.
//  Because it is stationary it is still drawn in profile -- that is how a
//  parked vehicle at a kerb is actually seen, and the side view is what makes
//  it read as waiting rather than as traffic.
constexpr float RICKSHAW_PARK_T4 = 0.80f;   // how far down the street it waits

float rickshawT4     = RICKSHAW_PARK_T4;  // fixed: it never moves
float rickshawWheel4 = 0.0f;              // fixed: the wheels never turn

void DrawRickshaw4() {
    const float t  = rickshawT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);

    // Size from the road's own width: at the near end the carriageway is about
    // seven times wider than at the horizon, and the rickshaw grows with it.
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);

    // Stood well past the right-hand kerb, on the verge. The offset is 1.62
    // rather than something closer to 1.0 because the street CURVES: the
    // cyclist keeps to 0.72 of the half-width, but further up the road that
    // line sits further right in world x than the kerb does down here, so a
    // rickshaw parked just off the kerb still gets ridden through. 1.62
    // clears the cyclist's whole swept path and still leaves the tea stall
    // (x = -23) a couple of units of air.
    const float x  = cx + RoadHW4(t) * 1.62f;

    // Everything further away is hazier: the same air the rest of the street
    // is fading through.
    const float depth = 0.80f * (1.0f - t);

    unsigned char r, g, b;

    DrawGroundShadow(x, y, 2.4f * S, 0.35f, (unsigned char)(90 * (0.35f + 0.65f * t)));

    // ---- Far rear wheel ---------------------------------------------------
    // Drawn first, smaller and darker: it is on the other side of the axle.
    AirFade4(depth, y, 28, 30, 38, 74, 80, 92, r, g, b);
    glColor3ub(r, g, b);
    FilledCircle4(x - 1.30f * S, y + 0.92f * S, 0.86f * S, r, g, b, 255);
    AirFade4(depth, y, 52, 56, 66, 104, 110, 124, r, g, b);
    FilledCircle4(x - 1.30f * S, y + 0.92f * S, 0.60f * S, r, g, b, 255);

    // ---- Rear axle --------------------------------------------------------
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 1.30f * S, y + 0.92f * S);
        glVertex2f(x - 0.55f * S, y + 0.86f * S);
    glEnd();

    // ---- Passenger cab ----------------------------------------------------
    // Body
    AirFade4(depth, y, 104, 34, 44, 186, 66, 74, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(x - 2.10f * S, y + 1.05f * S);
        glVertex2f(x + 0.30f * S, y + 1.05f * S);
        glVertex2f(x + 0.42f * S, y + 2.30f * S);
        glVertex2f(x - 2.16f * S, y + 2.30f * S);
    glEnd();
    // Panel line and a chrome strip, so the cab is not one flat block
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

    // Bench seat back
    AirFade4(depth, y, 58, 40, 30, 112, 82, 58, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x - 2.00f * S, y + 2.30f * S);
        glVertex2f(x + 0.20f * S, y + 2.30f * S);
        glVertex2f(x + 0.20f * S, y + 2.74f * S);
        glVertex2f(x - 2.00f * S, y + 2.74f * S);
    glEnd();

    // ---- Empty cab --------------------------------------------------------
    // No passenger: it is waiting FOR a fare, so the bench is empty and the
    // shadow inside the hood is all that shows.
    AirFade4(depth, y, 24, 20, 22, 58, 48, 50, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.94f * S, y + 2.40f * S);
        glVertex2f(x + 0.14f * S, y + 2.40f * S);
        glVertex2f(x + 0.14f * S, y + 2.74f * S);
        glVertex2f(x - 1.94f * S, y + 2.74f * S);
    glEnd();

    // ---- Folding hood -----------------------------------------------------
    // A half-dome over the bench, with its ribs showing.
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

    // Snow settled along the top of the hood
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glLineWidth(2.6f * S);
    glBegin(GL_LINE_STRIP);
        for (int i = 2; i <= 10; i++) {
            float a = PI4 * ((float)i / 12.0f);
            glVertex2f(x - 0.90f * S + 1.46f * S * cosf(a),
                       y + 2.74f * S + 2.04f * S * sinf(a));
        }
    glEnd();

    // ---- Frame down to the front fork -------------------------------------
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.2f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 0.55f * S, y + 0.86f * S);
        glVertex2f(x + 0.55f * S, y + 1.16f * S);
        glVertex2f(x + 1.70f * S, y + 1.90f * S);   // steering head
        glVertex2f(x + 1.86f * S, y + 0.84f * S);   // fork down to the hub
    glEnd();

    // Handlebars
    AirFade4(depth, y, 46, 48, 56, 96, 100, 112, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 1.52f * S, y + 2.18f * S);
        glVertex2f(x + 1.98f * S, y + 2.06f * S);
    glEnd();

    // ---- Near rear wheel and front wheel ----------------------------------
    // Drawn last so their spokes sit over the frame.
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

        // Spokes, turning with the distance travelled
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

    // ---- Driver -----------------------------------------------------------
    // Waiting, not working: he is stood astride the frame with both feet on
    // the road and his back straight, rather than up on the pedals leaning
    // into it. A slow idle sway keeps him from looking like a frozen frame.
    const float sway = sinf(pedTimer4 * 0.62f) * 0.07f * S;

    AirFade4(depth, y, 60, 52, 44, 104, 92, 78, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);                                   // both legs planted
        glVertex2f(x + 0.92f * S, y + 2.10f * S);
        glVertex2f(x + 0.58f * S, y + 0.10f * S);
        glVertex2f(x + 1.04f * S, y + 2.10f * S);
        glVertex2f(x + 1.42f * S, y + 0.10f * S);
    glEnd();
    // Torso, near upright
    AirFade4(depth, y, 52, 74, 58, 92, 132, 100, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(4.6f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.98f * S, y + 2.10f * S);
        glVertex2f(x + 1.16f * S + sway, y + 3.24f * S);
    glEnd();
    // One arm resting on the handlebars, the other hanging at his side
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

    // ---- Lamp -------------------------------------------------------------
    // Lit at night, and it throws a patch of light onto the road ahead.
    float lampOn = NightT4();
    if (lampOn > 0.05f) {
        // Parked, so it is a sidelight, not a driving beam: a small halo and
        // a patch on the verge under it rather than a wash thrown up the
        // road. At the old size the halo swallowed the whole front wheel.
        float lx = x + 2.10f * S, ly = y + 2.00f * S;
        DrawSoftEllipse(lx + 0.4f * S, y + 0.25f * S, 2.0f * S, 0.7f * S,
                        255, 222, 158, (unsigned char)(62 * lampOn), 3);
        FilledCircle4(lx, ly, 0.34f * S, 255, 216, 150, (unsigned char)(70 * lampOn));
        FilledCircle4(lx, ly, 0.16f * S, 255, 246, 214, (unsigned char)(255 * lampOn));
    }
    glLineWidth(1.0f);
}

// ============================================================================
//  ROADSIDE TEA STALL
// ----------------------------------------------------------------------------
//  The band between the road's near kerb and the ice rink -- roughly
//  x -34 .. -12 on y -16 .. -20 -- was open snow with nothing on it, right in
//  the front third of the frame where the eye spends most of its time.
//
//  A tea stall fills it with the one thing the plaza did not have: people
//  sitting still. Everything else in this scene is walking, skating, turning
//  or flying, so a group settled on benches with cups in their hands gives the
//  whole square somewhere to rest.
// ============================================================================
constexpr float TEA_X4 = -23.0f;
constexpr float TEA_Y4 = -18.6f;
// 1.10 rather than the 1.28 first tried: at the larger size the top of the
// awning reached y = -12.1, exactly the ground line of the market stalls
// behind it, and the two crowded each other. This keeps a clear unit of air
// between them while the stall still reads as a big foreground object.
constexpr float TEA_S4 =  1.10f;

// One seated customer, cup in hand, steam rising off it.
void DrawTeaSitter4(float x, float y, float sc, int seed,
                    unsigned char cr, unsigned char cg, unsigned char cb,
                    bool facingLeft) {
    const float d = facingLeft ? -1.0f : 1.0f;
    float lean = sinf(pedTimer4 * 0.6f + seed * 1.7f) * 0.05f;
    float sip  = sinf(pedTimer4 * 0.9f + seed * 2.3f);
    bool  drinking = (sip > 0.72f);              // every so often, a mouthful

    DrawGroundShadow(x, y, 0.86f * sc, 0.22f, 78);

    // Lower legs down from the bench, feet on the snow
    glColor3ub(38, 38, 46);
    glLineWidth(2.4f * sc);
    glBegin(GL_LINES);
        glVertex2f(x - 0.16f * sc, y + 1.06f * sc); glVertex2f(x - 0.16f * sc, y);
        glVertex2f(x + 0.20f * sc, y + 1.06f * sc); glVertex2f(x + 0.20f * sc, y);
    glEnd();
    // Thighs along the bench
    glColor3ub(cr, cg, cb);
    glLineWidth(4.0f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + 0.02f * sc, y + 1.10f * sc);
        glVertex2f(x - d * 0.60f * sc, y + 1.14f * sc);
    glEnd();
    // Torso, hunched a little over the cup
    glLineWidth(5.2f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + 0.02f * sc, y + 1.10f * sc);
        glVertex2f(x + (0.10f + lean) * sc, y + 2.14f * sc);
    glEnd();

    // Arm bringing the cup up
    float cupY = y + (drinking ? 2.18f : 1.86f) * sc;
    float cupX = x + (0.10f + lean) * sc + d * 0.34f * sc;
    glColor3ub((unsigned char)(cr * 0.82f), (unsigned char)(cg * 0.82f),
               (unsigned char)(cb * 0.82f));
    glLineWidth(2.2f * sc);
    glBegin(GL_LINES);
        glVertex2f(x + (0.08f + lean) * sc, y + 1.94f * sc);
        glVertex2f(cupX, cupY);
    glEnd();

    // Head and woolly hat
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.46f * sc, 0.30f * sc, 228, 190, 154, 255);
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.66f * sc, 0.28f * sc, cr, cg, cb, 255);
    FilledCircle4(x + (0.10f + lean) * sc, y + 2.86f * sc, 0.11f * sc, 246, 246, 250, 255);

    // The cup itself, and the steam off it
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

void DrawTeaStall4() {
    const float x = TEA_X4, y = TEA_Y4, S = TEA_S4;

    // ---- Warm pool of light on the snow underneath it ---------------------
    DrawGroundGlow4(x, y - 0.2f, 11.0f * S, 2.6f * S,
                    255, 196, 118, (unsigned char)(96 * NightT4() + 26));

    // ---- Awning poles ------------------------------------------------------
    glColor3ub(86, 62, 42);
    glLineWidth(2.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x - 3.9f * S, y);        glVertex2f(x - 3.9f * S, y + 5.0f * S);
        glVertex2f(x + 3.9f * S, y);        glVertex2f(x + 3.9f * S, y + 5.0f * S);
    glEnd();

    // ---- Counter -----------------------------------------------------------
    glColor3ub(128, 88, 54);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.3f * S, y + 0.30f * S);
        glVertex2f(x + 1.5f * S, y + 0.30f * S);
        glVertex2f(x + 1.5f * S, y + 2.15f * S);
        glVertex2f(x - 3.3f * S, y + 2.15f * S);
    glEnd();
    // Plank lines
    glColor4ub(88, 58, 34, 190);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int i = 1; i < 5; i++) {
            float px = x + (-3.3f + i * 0.96f) * S;
            glVertex2f(px, y + 0.30f * S); glVertex2f(px, y + 2.15f * S);
        }
    glEnd();
    // Worktop
    glColor3ub(172, 128, 82);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.5f * S, y + 2.15f * S);
        glVertex2f(x + 1.7f * S, y + 2.15f * S);
        glVertex2f(x + 1.7f * S, y + 2.42f * S);
        glVertex2f(x - 3.5f * S, y + 2.42f * S);
    glEnd();

    // ---- Brass urn, the centrepiece ---------------------------------------
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
    glColor3ub(232, 186, 96);                       // highlight down one side
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(ux - 0.34f * S, uy + 0.18f * S);
        glVertex2f(ux - 0.44f * S, uy + 1.36f * S);
    glEnd();
    glColor3ub(120, 84, 36);                        // tap and lid
    glLineWidth(2.0f * S);
    glBegin(GL_LINES);
        glVertex2f(ux + 0.60f * S, uy + 0.60f * S);
        glVertex2f(ux + 1.00f * S, uy + 0.42f * S);
    glEnd();
    FilledCircle4(ux, uy + 1.74f * S, 0.22f * S, 150, 108, 48, 255);
    // Steam off the urn -- the tallest plume on the stall
    for (int i = 0; i < 5; i++) {
        float t = fmodf(firePhase4 * 0.26f + i * 0.2f, 1.0f);
        FilledCircle4(ux + sinf(t * 3.4f + i) * 0.42f * S,
                      uy + (1.9f + t * 3.0f) * S,
                      (0.16f + t * 0.42f) * S, 236, 240, 246,
                      (unsigned char)(150 * (1.0f - t)));
    }

    // ---- Kettle on a burner ------------------------------------------------
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
    glBegin(GL_LINE_STRIP);                          // handle
        glVertex2f(kx - 0.30f * S, ky + 0.66f * S);
        glVertex2f(kx,             ky + 1.02f * S);
        glVertex2f(kx + 0.30f * S, ky + 0.66f * S);
    glEnd();
    glBegin(GL_LINES);                               // spout
        glVertex2f(kx + 0.44f * S, ky + 0.42f * S);
        glVertex2f(kx + 0.86f * S, ky + 0.62f * S);
    glEnd();
    // Burner flame under it
    float flick = 0.7f + 0.3f * sinf(firePhase4 * 3.4f);
    glColor4ub(255, 150, 60, (unsigned char)(200 * flick));
    glBegin(GL_TRIANGLES);
        glVertex2f(kx - 0.26f * S, ky - 0.30f * S);
        glVertex2f(kx + 0.26f * S, ky - 0.30f * S);
        glVertex2f(kx, ky + 0.06f * S * flick);
    glEnd();

    // ---- Glasses lined up on the worktop ----------------------------------
    for (int i = 0; i < 5; i++) {
        float gx = x + (0.35f + i * 0.28f) * S;
        glColor3ub(216, 226, 232);
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.09f * S, ky);
            glVertex2f(gx + 0.09f * S, ky);
            glVertex2f(gx + 0.08f * S, ky + 0.34f * S);
            glVertex2f(gx - 0.08f * S, ky + 0.34f * S);
        glEnd();
        glColor3ub(178, 118, 58);                    // tea in the bottom half
        glBegin(GL_QUADS);
            glVertex2f(gx - 0.08f * S, ky + 0.02f * S);
            glVertex2f(gx + 0.08f * S, ky + 0.02f * S);
            glVertex2f(gx + 0.08f * S, ky + 0.18f * S);
            glVertex2f(gx - 0.08f * S, ky + 0.18f * S);
        glEnd();
    }

    // ---- Vendor behind the counter, pouring -------------------------------
    float pour = sinf(pedTimer4 * 0.8f) * 0.10f;
    glColor3ub(74, 96, 120);
    glLineWidth(5.4f * S);
    glBegin(GL_LINES);
        glVertex2f(x + 0.60f * S, y + 2.20f * S);
        glVertex2f(x + 0.52f * S, y + 3.62f * S);
    glEnd();
    glColor3ub(58, 78, 100);
    glLineWidth(2.2f * S);
    glBegin(GL_LINES);                               // arm out over the glasses
        glVertex2f(x + 0.54f * S, y + 3.42f * S);
        glVertex2f(x - 0.10f * S, y + (3.02f + pour) * S);
    glEnd();
    FilledCircle4(x + 0.52f * S, y + 3.96f * S, 0.32f * S, 230, 192, 156, 255);
    FilledCircle4(x + 0.52f * S, y + 4.16f * S, 0.30f * S, 190, 78, 66, 255);

    // ---- Awning ------------------------------------------------------------
    // A sagging canvas sheet rather than a rigid triangle: two curves, so it
    // reads as cloth strung between poles.
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
    // Scalloped fringe along the front edge
    glColor3ub(196, 96, 80);
    for (int i = 0; i <= 9; i++) {
        float t  = (float)i / 9.0f;
        float px = x + (-4.4f + 8.8f * t) * S;
        float sag = -sinf(t * PI4) * 0.55f * S;
        FilledCircle4(px, y + 4.55f * S + sag, 0.22f * S, 196, 96, 80, 255);
    }
    // Snow lying along the top of the canvas, in winter
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

    // ---- Bulb hanging under the awning ------------------------------------
    float bx = x - 1.0f * S, by = y + 4.30f * S;
    glColor3ub(40, 40, 48);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(bx, y + 4.75f * S); glVertex2f(bx, by);
    glEnd();
    FilledCircle4(bx, by, 2.30f * S, 255, 206, 132, (unsigned char)(64 * NightT4()));
    FilledCircle4(bx, by, 0.26f * S, 255, 244, 206,
                  (unsigned char)(180 + 75 * NightT4()));

    // ---- Bench, stools and the people on them -----------------------------
    // Bench runs along the front of the counter, facing it.
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

    // Two stools off to the right
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

    // Five customers: three along the bench facing the counter, two on the
    // stools turned the other way.
    DrawTeaSitter4(x - 3.9f * S, y, S * 0.92f, 0, 148,  58,  64, false);
    DrawTeaSitter4(x - 2.7f * S, y, S * 0.96f, 1,  56,  84, 128, false);
    DrawTeaSitter4(x - 1.5f * S, y, S * 0.90f, 2,  92, 112,  70, false);
    DrawTeaSitter4(x + 2.9f * S, y, S * 0.94f, 3, 132,  96,  52, true );
    DrawTeaSitter4(x + 4.8f * S, y, S * 0.88f, 4,  86,  70, 116, true );

    glLineWidth(1.0f);
}

// ---- Car ------------------------------------------------------------------
//  Same trick as the rickshaw: position and size come straight from the road
//  functions, so it keeps to the carriageway and grows at the road's own rate.
//  It runs in the LEFT-hand wheel track.
//
//  FRONT VIEW. It travels from the far end of the street down toward the
//  camera and the river, so what the viewer is looking at is its nose, not its
//  flank -- grille, bonnet, windscreen, two headlamps and two front wheels
//  set either side. A side profile on this path read as a car sliding
//  sideways across the street; head-on it reads as coming down it.
//
//  Nothing here is hand-placed against the road. Every coordinate is the
//  centreline x, the road's y, and the road-derived scale S, so the car still
//  tracks the curve and grows exactly as the carriageway does.
float carT4      = 0.62f;
float carWheel4  = 0.0f;

void DrawCar4() {
    const float t  = carT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx - RoadHW4(t) * 0.36f;        // left-hand track
    const float depth = 0.80f * (1.0f - t);

    // The street bends left as it comes forward. Sampling the centreline a
    // little further down the road gives the direction the car is pointing,
    // and shearing the upper bodywork by that amount leans it into the bend
    // instead of leaving it square to the camera on a curving road.
    const float ahead = fminf(t + 0.07f, 1.0f);
    const float lean  = (RoadCX4(ahead) - cx) * 0.05f;   // x shift per unit height
    #define CX4(hx, hy) ((hx) + lean * (hy))             // hy in S-units above the road

    unsigned char r, g, b;
    DrawGroundShadow(x, y, 2.3f * S, 0.34f, (unsigned char)(95 * (0.35f + 0.65f * t)));

    // ---- Wheels -----------------------------------------------------------
    // Head-on a wheel is the tread face: a tall narrow ellipse, not a disc.
    // Drawn before the body so the arches crop them.
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
        // Hub, and a couple of spokes so the wheel still reads as turning.
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

    // ---- Body, seen nose-on -----------------------------------------------
    // A car is far narrower than it is long, so the front is a squat block
    // roughly two thirds the width the old side view was long.
    const float HWB = 1.86f * S;         // half-width at the waist
    AirFade4(depth, y, 96, 42, 48, 178, 74, 78, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWB * 0.94f, 0.42f), y + 0.42f * S);   // valance
        glVertex2f(CX4(x + HWB * 0.94f, 0.42f), y + 0.42f * S);
        glVertex2f(CX4(x + HWB,         1.10f), y + 1.10f * S);
        glVertex2f(CX4(x + HWB,         1.78f), y + 1.78f * S);   // shoulder
        glVertex2f(CX4(x - HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x - HWB,         1.10f), y + 1.10f * S);
    glEnd();

    // Bonnet: the top surface of the nose, catching the sky, so a shade
    // lighter than the front panel below it.
    AirFade4(depth, y, 116, 54, 60, 202, 92, 96, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x + HWB,         1.78f), y + 1.78f * S);
        glVertex2f(CX4(x + HWB * 0.90f, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x - HWB * 0.90f, 2.06f), y + 2.06f * S);
    glEnd();

    // ---- Cabin ------------------------------------------------------------
    // Narrower than the body and set back, with the roof narrower again.
    const float HWC = 1.52f * S;
    AirFade4(depth, y, 86, 36, 42, 162, 66, 70, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWC,         2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC,         2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
    glEnd();

    // Windscreen
    AirFade4(depth, y, 38, 52, 70, 132, 158, 184, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(CX4(x - HWC * 0.86f, 2.22f), y + 2.22f * S);
        glVertex2f(CX4(x + HWC * 0.86f, 2.22f), y + 2.22f * S);
        glVertex2f(CX4(x + HWC * 0.76f, 3.10f), y + 3.10f * S);
        glVertex2f(CX4(x - HWC * 0.76f, 3.10f), y + 3.10f * S);
    glEnd();
    // A-pillars, so the glass is framed rather than floating
    AirFade4(depth, y, 70, 28, 34, 138, 56, 60, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.6f * S);
    glBegin(GL_LINES);
        glVertex2f(CX4(x - HWC, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC, 2.06f), y + 2.06f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
    glEnd();

    // Driver and passenger behind the glass, and the wiper sweep below them
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

    // ---- Grille and bumper ------------------------------------------------
    AirFade4(depth, y, 22, 22, 28, 52, 54, 62, r, g, b);
    glColor3ub(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - 0.96f * S, 1.16f), y + 1.16f * S);
        glVertex2f(CX4(x + 0.96f * S, 1.16f), y + 1.16f * S);
        glVertex2f(CX4(x + 0.96f * S, 1.66f), y + 1.66f * S);
        glVertex2f(CX4(x - 0.96f * S, 1.66f), y + 1.66f * S);
    glEnd();
    AirFade4(depth, y, 118, 122, 132, 186, 190, 198, r, g, b);   // grille bars
    glColor3ub(r, g, b);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int k = 0; k < 3; k++) {
            float gy = 1.26f + k * 0.16f;
            glVertex2f(CX4(x - 0.92f * S, gy), y + gy * S);
            glVertex2f(CX4(x + 0.92f * S, gy), y + gy * S);
        }
    glEnd();
    // Bumper across the bottom, and a number plate on it
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

    // Snow on the roof, in winter
    glColor4ub(246, 249, 255, (unsigned char)(235 * SnowDepth4()));
    glBegin(GL_QUADS);
        glVertex2f(CX4(x - HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC * 0.88f, 3.24f), y + 3.24f * S);
        glVertex2f(CX4(x + HWC * 0.84f, 3.44f), y + 3.44f * S);
        glVertex2f(CX4(x - HWC * 0.84f, 3.44f), y + 3.44f * S);
    glEnd();

    // ---- Headlamps --------------------------------------------------------
    // Both of them face the viewer now, so at night this is the car's whole
    // signature: two lamps and the wash they throw on the road in front.
    const float lampOn = NightT4();
    const float hlY = y + 1.62f * S;
    const float hlX = 1.40f * S;
    for (int k = 0; k < 2; k++) {
        float lx = CX4(x + (k ? hlX : -hlX), 1.62f);
        // Lens is visible even by day
        FilledCircle4(lx, hlY, 0.30f * S, 226, 228, 224, 235);
        FilledCircle4(lx, hlY, 0.19f * S, 252, 248, 232, 255);
        if (lampOn > 0.05f) {
            FilledCircle4(lx, hlY, 0.78f * S, 255, 240, 200, (unsigned char)(90 * lampOn));
            FilledCircle4(lx, hlY, 0.30f * S, 255, 252, 238, (unsigned char)(255 * lampOn));
        }
    }
    if (lampOn > 0.05f) {
        // A pair of beams spilling down onto the road between the viewer and
        // the car, widening as they come forward.
        for (int k = 0; k < 2; k++) {
            float dir = k ? 1.0f : -1.0f;
            DrawSoftEllipse(x + dir * 1.9f * S, y - 1.5f * S, 2.9f * S, 1.1f * S,
                            255, 236, 190, (unsigned char)(95 * lampOn), 3);
        }
        DrawSoftEllipse(x, y - 2.6f * S, 5.2f * S, 1.5f * S,
                        255, 236, 190, (unsigned char)(78 * lampOn), 4);
    }

    // Exhaust vapour, drifting out from behind the car rather than off one end
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

void UpdateCar4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateCar4, 0); return; }
    if (isAnimating4) {
        float step = 0.0026f / (0.35f + 1.15f * carT4);
        carT4 += step;
        carWheel4 += step * (30.0f + 52.0f * carT4);
        if (carT4 > 1.16f) carT4 = -0.08f;
    }
    glutTimerFunc(30, UpdateCar4, 0);
}

// ---- Bicycle --------------------------------------------------------------
//  The slowest thing on the street, keeping to the right-hand edge where a
//  cyclist actually rides.
//
//  FRONT VIEW, for the same reason as the car: it is riding down the street
//  toward the camera and the river, so the viewer sees the front wheel edge-on,
//  the handlebars spread across, and the rider facing them. The rear wheel is
//  directly behind the front one and shows only as a sliver, which is exactly
//  what makes the bike read as coming at you rather than crossing.
float bikeT4     = 0.34f;
float bikeWheel4 = 0.0f;

void DrawBike4() {
    const float t  = bikeT4;
    const float y  = RoadY4(t);
    const float cx = RoadCX4(t);
    const float S  = 0.30f + 1.25f * powf(t, 1.28f);
    const float x  = cx + RoadHW4(t) * 0.72f;        // hugging the right kerb
    const float depth = 0.80f * (1.0f - t);

    unsigned char r, g, b;
    DrawGroundShadow(x, y, 1.1f * S, 0.30f, (unsigned char)(85 * (0.35f + 0.65f * t)));

    const float wr = 0.80f * S;          // wheel radius (its tall axis)
    const float rx = 0.15f * S;          // and its width, seen edge-on
    const float wy = y + wr;

    // A rider out of the saddle rocks the bike under them. One sway value
    // drives the bars, the frame and the torso together so the whole machine
    // leans as a unit rather than coming apart.
    const float sway = sinf(bikeWheel4) * 0.10f * S;

    // ---- Rear wheel, directly behind and mostly hidden --------------------
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

    // ---- Front wheel, edge-on ---------------------------------------------
    AirFade4(depth, y, 26, 28, 34, 60, 64, 74, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.2f * S);
    glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 18; k++) {
            float a = (float)k / 18.0f * 2.0f * PI4;
            glVertex2f(x + sway + rx * cosf(a), wy + wr * sinf(a));
        }
    glEnd();
    // Spokes, squashed onto the same narrow ellipse so they turn with it
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

    // ---- Frame: fork, head tube and bars ----------------------------------
    AirFade4(depth, y, 42, 88, 118, 84, 156, 196, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.7f * S);
    glBegin(GL_LINES);
        glVertex2f(x + sway - 0.20f * S, wy);                 // fork legs,
        glVertex2f(x + sway - 0.05f * S, y + 2.16f * S);      // splayed at the
        glVertex2f(x + sway + 0.20f * S, wy);                 // hub and closing
        glVertex2f(x + sway + 0.05f * S, y + 2.16f * S);      // at the crown
        glVertex2f(x + sway, y + 2.16f * S);                  // stem
        glVertex2f(x + sway, y + 2.40f * S);
        glVertex2f(x - 0.10f * S, y + 1.05f * S);             // down tube back
        glVertex2f(x + sway, y + 2.10f * S);                  // to the head
    glEnd();
    // Handlebars, spread across the frame -- the giveaway that this is a
    // bicycle seen head-on.
    AirFade4(depth, y, 30, 30, 36, 62, 62, 72, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(2.4f * S);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x + sway - 0.66f * S, y + 2.30f * S);
        glVertex2f(x + sway - 0.20f * S, y + 2.42f * S);
        glVertex2f(x + sway + 0.20f * S, y + 2.42f * S);
        glVertex2f(x + sway + 0.66f * S, y + 2.30f * S);
    glEnd();

    // ---- Rider ------------------------------------------------------------
    // Legs alternate with the crank. Seen from the front the knees swing out
    // to the sides rather than back and forth, so the drive is in x as much
    // as in y.
    const float pedA = sinf(bikeWheel4), pedB = sinf(bikeWheel4 + PI4);
    const float hipY = y + 2.58f * S, shoY = y + 3.62f * S;
    const float hx   = sway * 0.6f;

    AirFade4(depth, y, 44, 44, 54, 82, 82, 96, r, g, b);
    glColor3ub(r, g, b);
    glLineWidth(1.9f * S);
    glBegin(GL_LINE_STRIP);                                   // left leg
        glVertex2f(x + hx - 0.22f * S, hipY);
        glVertex2f(x + hx - 0.52f * S, y + 1.62f * S + 0.14f * S * pedA);
        glVertex2f(x + hx - 0.34f * S, y + 0.62f * S + 0.26f * S * pedA);
    glEnd();
    glBegin(GL_LINE_STRIP);                                   // right leg
        glVertex2f(x + hx + 0.22f * S, hipY);
        glVertex2f(x + hx + 0.52f * S, y + 1.62f * S + 0.14f * S * pedB);
        glVertex2f(x + hx + 0.34f * S, y + 0.62f * S + 0.26f * S * pedB);
    glEnd();

    // Torso, squared to the viewer: widest at the shoulders.
    AirFade4(depth, y, 116, 52, 60, 196, 92, 98, r, g, b);   // jacket
    glColor3ub(r, g, b);
    glBegin(GL_POLYGON);
        glVertex2f(x + hx - 0.32f * S, hipY);
        glVertex2f(x + hx + 0.32f * S, hipY);
        glVertex2f(x + hx + 0.44f * S, shoY);
        glVertex2f(x + hx - 0.44f * S, shoY);
    glEnd();
    // Arms out and down to the grips
    glLineWidth(1.7f * S);
    glBegin(GL_LINES);
        glVertex2f(x + hx - 0.42f * S, shoY - 0.10f * S);
        glVertex2f(x + sway - 0.62f * S, y + 2.34f * S);
        glVertex2f(x + hx + 0.42f * S, shoY - 0.10f * S);
        glVertex2f(x + sway + 0.62f * S, y + 2.34f * S);
    glEnd();
    AirFade4(depth, y, 150, 122, 100, 226, 188, 152, r, g, b);
    FilledCircle4(x + hx, shoY + 0.30f * S, 0.27f * S, r, g, b, 255);   // face
    AirFade4(depth, y, 190, 168, 60, 236, 214, 92, r, g, b);            // helmet
    FilledCircle4(x + hx, shoY + 0.46f * S, 0.28f * S, r, g, b, 255);

    // ---- Front lamp -------------------------------------------------------
    // Pointing straight at the viewer now, so it is a disc rather than a beam
    // raking off to one side.
    float lampOn = NightT4();
    if (lampOn > 0.05f) {
        // On the fork crown, below the bars: a halo up at bar height washed
        // the whole machine out at the far end of the street, where the bike
        // is only a few pixels tall.
        float lx = x + sway, ly = y + 1.78f * S;
        FilledCircle4(lx, ly, 0.34f * S, 255, 232, 176, (unsigned char)(80 * lampOn));
        FilledCircle4(lx, ly, 0.15f * S, 255, 250, 226, (unsigned char)(255 * lampOn));
        DrawSoftEllipse(lx, y - 1.5f * S, 3.0f * S, 1.0f * S,
                        255, 232, 176, (unsigned char)(72 * lampOn), 3);
    }
    glLineWidth(1.0f);
}

void UpdateBike4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateBike4, 0); return; }
    if (isAnimating4) {
        float step = 0.0011f / (0.35f + 1.15f * bikeT4);
        bikeT4 += step;
        bikeWheel4 += step * (22.0f + 40.0f * bikeT4);
        if (bikeT4 > 1.14f) bikeT4 = -0.05f;
    }
    glutTimerFunc(30, UpdateBike4, 0);
}

// The rickshaw is parked, so this no longer advances it: rickshawT4 stays at
// RICKSHAW_PARK_T4 and the wheels stay still. The timer itself is kept so the
// call in InitScenario4() and the depth-sort entry both remain valid.
void UpdateRickshaw4(int) {
    rickshawT4     = RICKSHAW_PARK_T4;
    rickshawWheel4 = 0.0f;
    glutTimerFunc(120, UpdateRickshaw4, 0);
}

void DrawBuildingShadows4() {
    unsigned char sr = MixB4(10, 92), sg = MixB4(14, 104), sb = MixB4(28, 126);
    unsigned char a  = (unsigned char)(MixB4(96, 70));

    for (int i = 0; i < NUM_SHOPS4; i++) {
        const Shop4& sh = shops4[i];
        float half = sh.w * 0.5f + SHOP_EAVE_OVER4;
        // Length scales with height, and leans left, away from the light.
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

    // The street canyon lays its own shadow into the mouth of the road.
    glBegin(GL_QUADS);
        glColor4ub(sr, sg, sb, (unsigned char)(a * 0.9f));
        glVertex2f(-49.0f, SHOP_BASE_Y4);
        glVertex2f(-28.0f, SHOP_BASE_Y4);
        glColor4ub(sr, sg, sb, 0);
        glVertex2f(-29.5f, SHOP_BASE_Y4 - 2.2f);
        glVertex2f(-51.5f, SHOP_BASE_Y4 - 2.2f);
    glEnd();

    // A cool ambient occlusion band right where the row meets the snow, so
    // nothing in the back line appears to hover.
    glBegin(GL_QUADS);
        glColor4ub(MixB4(16, 120), MixB4(22, 132), MixB4(40, 152), (unsigned char)(MixB4(70, 44)));
        glVertex2f(-60.0f, SHOP_BASE_Y4);
        glVertex2f( 60.0f, SHOP_BASE_Y4);
        glColor4ub(MixB4(16, 120), MixB4(22, 132), MixB4(40, 152), 0);
        glVertex2f( 60.0f, SHOP_BASE_Y4 - 1.4f);
        glVertex2f(-60.0f, SHOP_BASE_Y4 - 1.4f);
    glEnd();
}

void DrawPerson4(const Ped4& p, float t) {
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
    // A little snow settles on the hat
    glColor4ub(246, 249, 255, 230);
    glBegin(GL_TRIANGLES);
        glVertex2f(hipX-0.16f*SnowDepth4(), hipY+1.95f);
        glVertex2f(hipX+0.16f*SnowDepth4(), hipY+1.95f);
        glVertex2f(hipX, hipY+2.2f);
    glEnd();

    // Breath. The horse already had this and it was the single best detail
    // in the scenario -- it costs two lines to give it to everyone, and it
    // is the cheapest possible way to say "cold".
    for (int i = 0; i < 3; i++) {
        float bt = fmodf(firePhase4 * 0.26f + p.phase * 0.17f + i * 0.33f, 1.0f);
        FilledCircle4(hipX + p.dir * (0.35f + bt * 1.3f), hipY + 1.42f + bt * 0.45f,
                      0.09f + bt * 0.22f, 226, 234, 244,
                      (unsigned char)(105 * (1.0f - bt)));
    }

    EndDepthSprite();
}

void DrawDogPlay4(float x, float y, float scale, float t) {
    // Small dog on the main plaza, right side. It stays out of the river and
    // the near footpath and is deliberately sized to sit at roughly 60% of a

    float sc = scale;
    float bodyX = x;
    float bodyY = y;
    float playPhase = t * 0.55f;

    DrawGroundShadow(bodyX, bodyY, 0.72f * sc, 0.18f, 68);
    BeginDepthSprite(bodyX, bodyY, sc);

    // Dog stays in the same place but shifts its body slightly to keep the
    // play animation alive while it watches and nudges a nearby ball.
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

        // Legs planted with a soft trot.
        glColor3ub(78, 60, 42);
        glLineWidth(2.2f);
        glBegin(GL_LINES);
            glVertex2f(bodyX - 0.38f, bodyY + 0.10f); glVertex2f(bodyX - 0.44f, bodyY - 0.52f);
            glVertex2f(bodyX + 0.02f,  bodyY + 0.08f); glVertex2f(bodyX + 0.00f,  bodyY - 0.54f);
            glVertex2f(bodyX + 0.40f, bodyY + 0.00f); glVertex2f(bodyX + 0.42f, bodyY - 0.52f);
        glEnd();

        // Head and snout.
        FilledCircle4(bodyX + 0.90f, bodyY + 0.42f, 0.28f, 132, 102, 66, 255);
        glColor3ub(220, 182, 140);
        glBegin(GL_POLYGON);
            glVertex2f(bodyX + 1.08f, bodyY + 0.34f);
            glVertex2f(bodyX + 1.38f, bodyY + 0.40f);
            glVertex2f(bodyX + 1.18f, bodyY + 0.16f);
            glVertex2f(bodyX + 1.02f, bodyY + 0.18f);
        glEnd();

        // Ears and eyes.
        glColor3ub(90, 68, 42);
        glBegin(GL_TRIANGLES);
            glVertex2f(bodyX + 0.95f, bodyY + 0.70f); glVertex2f(bodyX + 1.02f, bodyY + 0.86f); glVertex2f(bodyX + 1.12f, bodyY + 0.64f);
            glVertex2f(bodyX + 1.15f, bodyY + 0.76f); glVertex2f(bodyX + 1.20f, bodyY + 0.90f); glVertex2f(bodyX + 1.30f, bodyY + 0.68f);
        glEnd();
        glColor3ub(32, 32, 32);
        FilledCircle4(bodyX + 0.98f, bodyY + 0.46f, 0.05f, 20, 20, 20, 255);
        FilledCircle4(bodyX + 1.12f, bodyY + 0.46f, 0.05f, 20, 20, 20, 255);

        // Tail wagging in a happy little arc.
        glColor3ub(90, 68, 42);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(bodyX - 0.92f, bodyY + 0.18f);
            glVertex2f(bodyX - 1.28f + 0.18f * sinf(t * 2.4f), bodyY + 0.56f + 0.20f * cosf(t * 2.4f));
        glEnd();
    glPopMatrix();

    // Ball sits close by, with only a tiny wobble to suggest play without
    // moving the dog itself across the plaza.
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

void DrawPedestrians4() {
    // Nearest walker drawn last, so the crowd overlaps correctly.
    int order[NUM_PEDS4];
    for (int i = 0; i < NUM_PEDS4; i++) order[i] = i;
    for (int i = 1; i < NUM_PEDS4; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 && peds4[order[j]].y < peds4[key].y) { order[j+1] = order[j]; j--; }
        order[j+1] = key;
    }
    for (int i = 0; i < NUM_PEDS4; i++) DrawPerson4(peds4[order[i]], pedTimer4);
}

void UpdatePedestrians4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdatePedestrians4, 0); return; }  // idle while this scenario is off-screen
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

// ============================================================================
//  SNOW
// ============================================================================
// ============================================================================
//  SNOW IN THREE DEPTH BANDS
// ----------------------------------------------------------------------------
//  Snowfall used to be one flat layer: every flake the same size, the same
//  brightness and the same speed, all drawn in front of everything else. Now
//  the array is split into three bands drawn at three different points in the
//  frame, which costs nothing and gives the whole scene depth for free.
//
//      FAR  drawn right after the sky   small, dim, slow  -- behind the shops
//      MID  drawn after the props       as before
//      NEAR drawn last of all           large, bright, fast, slightly blurred
// ============================================================================
constexpr int SNOW_FAR_END4  = 150;
constexpr int SNOW_MID_END4  = 250;   // NEAR runs from here to MAX_SNOW4

void InitSnow4() {
    for (int i = 0; i < MAX_SNOW4; i++) {
        snowX4[i] = -62.0f + (rand()%1240)/10.0f;
        snowY4[i] = (rand()%800-400)/10.0f + 20.0f;
        snowDrift4[i] = ((rand()%100)-50)/500.0f;
        snowSize4[i] = 0.15f + (rand()%15)/100.0f;
    }
}

// How many flakes of a band are active at the current intensity.
inline int SnowBandCount4(int first, int last) {
    int n = last - first;
    if (snowIntensity4 == 0) return first + n / 3;
    if (snowIntensity4 == 1) return first + (n * 2) / 3;
    return last;
}

// The same particle arrays carry both seasons: in winter each one is a snow
// flake, in autumn the identical particle is drawn as a tumbling leaf. Doing
// it this way means the depth banding, the fall speeds and the density keys
// all keep working unchanged, and the crossfade dissolves one into the other
// rather than swapping two separate systems.
void DrawSnowBand4(int first, int last, float sizeMul,
                   unsigned char alpha, bool blurred) {
    int end = SnowBandCount4(first, last);
    const float winter = SeasonWinter4();
    const float autumnV = SeasonAutumn4();

    const unsigned char leafCols[4][3] = {
        {186,  92,  38}, {214, 142,  48}, {154,  70,  44}, {198, 168,  62}
    };

    for (int i = first; i < end; i++) {
        float r = snowSize4[i] * sizeMul;

        // ---- Snow ---------------------------------------------------------
        if (winter > 0.02f) {
            unsigned char a = (unsigned char)(alpha * winter);
            if (blurred) {
                // Near flakes are out of focus: a soft halo plus an offset core.
                FilledCircle4(snowX4[i], snowY4[i], r * 1.9f, 255, 255, 255, (unsigned char)(a / 4));
                FilledCircle4(snowX4[i] + r * 0.3f, snowY4[i] - r * 0.2f, r, 255, 255, 255, a);
            } else {
                FilledCircle4(snowX4[i], snowY4[i], r, 255, 255, 255, a);
            }
        }

        // ---- Leaf ---------------------------------------------------------
        if (autumnV > 0.02f) {
            unsigned char a = (unsigned char)(alpha * autumnV);
            const unsigned char* c = leafCols[i % 4];
            // A leaf spins as it falls, and it is wider than a flake.
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
            // Midrib, which is what stops it reading as a coloured blob
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

void DrawSnowFar4()  { DrawSnowBand4(0,              SNOW_FAR_END4, 0.55f,  95, false); }
void DrawSnowMid4()  { DrawSnowBand4(SNOW_FAR_END4,  SNOW_MID_END4, 1.00f, 205, false); }
void DrawSnowNear4() { DrawSnowBand4(SNOW_MID_END4,  MAX_SNOW4,     1.95f, 215, true ); }

void UpdateSnow4(int) {
    if (currentScreen != SCENARIO_4 || isPaused) { glutTimerFunc(120, UpdateSnow4, 0); return; }  // idle while this scenario is off-screen
    if (isAnimating4) {
        // Settled snow eases toward the depth implied by the intensity key.
        float tgt = SnowCoverTarget4();
        snowCover4 += (tgt - snowCover4) * 0.02f;

        float base = 0.22f + snowIntensity4 * 0.14f;
        for (int i = 0; i < MAX_SNOW4; i++) {
            // Near flakes fall visibly faster than far ones -- motion
            // parallax, which is what actually sells the depth.
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

// ============================================================================
//  CONTRACT: Init / Draw / Keyboard / Mouse / kTitle
// ============================================================================
const char* kTitle = "Riverfront Market Life";

void Init() {
    // ---- Star field -------------------------------------------------------
    // Mostly faint, small and slow; the size bucket and the brightness are
    // rolled together so a big star is also a bright one. Colours drift
    // between a cold blue-white and a warm cream, which is what keeps a field
    // this dense from looking like a spray of identical pixels.
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
    // The bright few, kept clear of the moon so they do not sit inside its
    // halo where nothing would be visible anyway.
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
    // Birds start scattered across the sky rather than all entering together.
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
    glutTimerFunc(0, UpdateBoats4, 0);              // RIVERSIDE (added)
    glutTimerFunc(0, UpdateRiverPedestrians4, 0);   // RIVERSIDE (added)
}

void Draw() {
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
    DrawBirds4();                // distant birds: sky only, cropped by the roofs
    DrawSnowFar4();             // far band: behind the whole market
    DrawDistantBuildings4();     // dim skyline behind the market
    DrawRoadsideBlocks4();       // the street canyon framing the new road
    DrawMetroRail4();            // elevated metro rail behind the shops
    DrawFireworks4();
    DrawSleigh4();
    DrawShops4();
    DrawChimney4();
    DrawClockTower4();
    // Plaza lamps in front of the shop row (varying scale for perspective)
    DrawPlazaLamp4(-10.0f, 0.90f);
    DrawPlazaLamp4(  6.0f, 0.72f);
    DrawPlazaLamp4( 30.0f, 0.58f);
    DrawLightSpans4();

    DrawFireworkGroundFlash4();
    DrawPlazaGround4();
    DrawPerspectiveRoad4();     // the side street, part of the ground itself

    // The wheel's feet are on y = -10, so everything standing NEARER than
    // that -- the stalls, the crowd, the rink -- has to draw after it. It
    // used to be drawn last of the plaza, which put a structure standing at
    // y = -10 in front of walkers standing at y = -13.9.
    DrawFerrisWheel4();

    DrawBuildingShadows4();     // what the back row lays on the snow
    DrawLightPools4();          // every light source spills onto the snow
    DrawSnowAccumulation4();
    DrawRink4();
    DrawFountainAutumn4();      // takes the rink's ground out of season
    DrawIceSculptures4();
    DrawStage4();
    DrawStalls4();
    DrawFirePit4();
    DrawChristmasTree4();
    DrawGiftBoxes4();
    DrawSanta4();
    DrawChestnutRoaster4();     // stands where Santa does, out of season
    DrawSnowmanFamily4();
    DrawPlantersAutumn4();      // takes the snowmen's ground out of season
    DrawNutcracker4(-9.0f);
    DrawSkaters4();
    DrawSled4();
    DrawChimneySmoke4();
    DrawStallSteam4();
    DrawCarolers4();
    DrawPedestrians4();
    // Small dog playing on the main plaza at the right side, away from the
    // river and the terrace/footpath. Increased to 2x the previous size.
    DrawDogPlay4(32.0f, -15.8f, 1.20f, pedTimer4);
    DrawTeaStall4();            // roadside tea stall in the empty near band

    // The rickshaw runs from the horizon down to the terrace coping, so for
    // most of its journey it is nearer the camera than the plaza crowd. Drawn
    // here it correctly passes in front of them down the near half of the
    // street, which is where it spends most of its run.
    // Three vehicles on one street, so they must be drawn in depth order:
    // whichever is furthest down the road is nearest the camera and goes last.
    {
        struct RoadUser { float t; void (*draw)(); };
        RoadUser users[3] = { { rickshawT4, DrawRickshaw4 },
                              { carT4,      DrawCar4      },
                              { bikeT4,     DrawBike4     } };
        for (int i = 1; i < 3; i++) {          // tiny insertion sort by t
            RoadUser key = users[i];
            int j = i - 1;
            while (j >= 0 && users[j].t > key.t) { users[j+1] = users[j]; j--; }
            users[j+1] = key;
        }
        for (int i = 0; i < 3; i++) users[i].draw();
    }

    DrawFog4(-6.0f, 14.0f, 105);   // haze hangs over the market itself
    DrawSnowMid4();             // mid band: between the market and the terrace

    // ---- NEAR TERRACE (foreground) -------------------------------------
    DrawTerraceWall4();
    DrawSledRun4();
    // ---- RIVERSIDE (added) ---------------------------------------------
    // Depth order below: stairs -> river/reflections -> boats -> the
    // unloading vignette -> people walking the dock. This replaces the old
    // single DrawFrozenCanal4() call; see the RIVERSIDE comment block above
    // DrawStairs4() for how to undo just this change.
    DrawStairsAll4();
    DrawRiver4();
    DrawBoats4();
    DrawGoodsUnloading4();
    DrawRiverPedestrians4();
    // ----------------------------------------------------------------------
    DrawForegroundCrowd4();
    DrawForegroundLamp4(-58.0f, 0.90f);
    DrawForegroundLamp4( 56.0f, 0.78f);
    DrawFog4(-40.0f, -20.0f, 70);  // thinner haze across the near terrace

    DrawSnowNear4();            // near band: big, bright, in front of everything

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

void Keyboard(unsigned char key, int /*x*/, int /*y*/) {
    switch (key) {
        case '1': snowIntensity4 = 0; break;
        case '2': snowIntensity4 = 1; break;
        case '3': snowIntensity4 = 2; break;
        case 'l': case 'L': multicolorLights4 = !multicolorLights4; break;
        case 'f': case 'F': fogMode4 = !fogMode4; break;

        // Season. 1-5, L, F and X were all taken, so S it is.
        case 's': case 'S':
            seasonTarget4 = (seasonTarget4 > 0.5f) ? 0.0f : 1.0f;
            break;

        // Time of day. 4 brings the sun up, 5 puts the scene back to night.
        // It clears the two mood toggles, but it deliberately does NOT touch
        // seasonTarget4: the season is the viewer's own choice and switching
        // the lights off at dusk should not drag autumn back into winter.
        case '4': dayTarget4 = 1.0f; break;
        case '5':
            dayTarget4        = 0.0f;
            multicolorLights4 = false;
            fogMode4          = false;
            break;

        // The rare events are genuinely rare -- the sled has a 300-tick
        // cooldown and the aurora a 600-tick one -- so during a live demo
        // they may simply never fire while anyone is watching. 'X' forces
        // all three at once.
        case 'x': case 'X':
            sledActive4 = true;  sledX4 = -75.0f;
            auroraActive4 = true; auroraTimer4 = 0.0f;
            fireworkCooldown4 = 1;
            break;
    }
    glutPostRedisplay();
}

void Mouse(int button, int state, int /*x*/, int /*y*/) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)  isAnimating4 = true;
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) isAnimating4 = false;
    glutPostRedisplay();
}

} // namespace Scenario4

// ============================================================================
//  SCENARIO NAVIGATION
// ----------------------------------------------------------------------------
//  The program used to open on a clickable home menu. That menu is gone: the
//  window now opens directly on Scenario 1 and the viewer walks the scenes
//  with the keyboard.
//
//      N / RIGHT ARROW   next scenario      (4 wraps back round to 1)
//      B / LEFT ARROW    previous scenario  (1 wraps back round to 4)
//      F1 .. F4          jump straight to that scenario
//
//  Why two mechanisms, and why these keys: 1/2/3/4 are already spoken for
//  inside every scenario (day phase, weather, snow depth), so the numbers
//  cannot be reused for navigation without breaking those controls. N and B
//  give the natural presentation order; F1-F4 arrive through glutSpecialFunc,
//  a separate GLUT callback this program does not otherwise use, so direct
//  jumps are possible with no risk of colliding with any scenario's keys.
// ============================================================================
struct ScenarioInfo { const char* title; const char* subtitle; AppScreen screen; };

// Titles come straight from each scenario's own kTitle, so renaming a scene
// in its namespace updates the title card automatically.
ScenarioInfo scenarios[NUM_SCENARIOS] = {
    { Scenario1_CoastalCity::kTitle, "Harbour city - day/night, storms, lighthouse",   SCENARIO_1 },
    { Scenario2::kTitle,             "Rainy neon downtown - train, storm, wet street", SCENARIO_2 },
    { Scenario3::kTitle,             "Riverside park - bridge, balloon, four phases",  SCENARIO_3 },
    { Scenario4::kTitle,             "Snowy night market - ferris wheel, sleigh, fog", SCENARIO_4 },
};

// ---- Transition state ------------------------------------------------------
//  Switching used to be a hard cut between two completely different scenes.
//  `switchFade` dips the screen through black across the change, and
//  `titleCardT` runs a name card in afterwards -- between them they do the
//  job the menu used to do, which was telling the viewer what they are
//  looking at.
float switchFade  = 0.0f;    // 1 = fully black, decays to 0
float titleCardT  = 0.0f;    // counts up from 0; card is visible below ~2.6s
bool  firstLaunch = true;    // the opening card lingers and adds a key hint

// `initial` is true only for the opening call from main(). That card holds
// longer and spells out the navigation keys; once the viewer has moved for
// themselves the hint has done its job and every later card is short.
void GoToScenarioIndex(int index, bool initial = false) {
    if (index < 0) index = NUM_SCENARIOS - 1;
    if (index >= NUM_SCENARIOS) index = 0;

    currentScenarioIndex = index;
    currentScreen = scenarios[index].screen;

    isPaused    = false;       // never carry a pause into the next scene
    switchFade  = initial ? 0.0f : 1.0f;   // no fade-in from black on launch
    titleCardT  = 0.0f;
    firstLaunch = initial;

    // The window title bar follows the scene, which helps when the demo is
    // being screen-recorded.
    static char windowTitle[160];
    sprintf(windowTitle, "City Life  -  %d/%d  %s",
            index + 1, NUM_SCENARIOS, scenarios[index].title);
    glutSetWindowTitle(windowTitle);

    glutPostRedisplay();
}

void NextScenario()     { GoToScenarioIndex(currentScenarioIndex + 1); }
void PreviousScenario() { GoToScenarioIndex(currentScenarioIndex - 1); }

// ---- Title card ------------------------------------------------------------
//  Scenario name and one-line description, fading in and then out again.
void DrawTitleCard() {
    float hold = firstLaunch ? 4.2f : 2.6f;
    if (titleCardT > hold) return;

    // Fade in over the first 0.35s, hold, then fade out over the last 0.7s.
    float a = 1.0f;
    if (titleCardT < 0.35f)        a = titleCardT / 0.35f;
    else if (titleCardT > hold - 0.7f) a = (hold - titleCardT) / 0.7f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;

    const ScenarioInfo& sc = scenarios[currentScenarioIndex];

    // Dark plate behind the text so the card reads over any scene, bright
    // daytime park or black winter sky alike.
    glColor4f(0.04f, 0.05f, 0.08f, 0.62f * a);
    glBegin(GL_QUADS);
        glVertex2f(WORLD_LEFT,  6.0f);  glVertex2f(WORLD_RIGHT, 6.0f);
        glVertex2f(WORLD_RIGHT, 20.0f); glVertex2f(WORLD_LEFT,  20.0f);
    glEnd();

    // Hairlines top and bottom
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

    // On the very first card only, spell out how to move on. After that the
    // permanent HUD line carries it.
    if (firstLaunch) {
        glColor4f(0.95f, 0.86f, 0.55f, a);
        DrawTextCentered(0.0f, 3.2f, GLUT_BITMAP_HELVETICA_12,
                         "press  N  for the next scenario     F1-F4 to jump     H for controls");
    }
}

// ---- Cross-fade ------------------------------------------------------------
void DrawSwitchFade() {
    if (switchFade <= 0.01f) return;
    glColor4f(0.0f, 0.0f, 0.0f, switchFade);
    glBegin(GL_QUADS);
        glVertex2f(WORLD_LEFT,  WORLD_BOTTOM); glVertex2f(WORLD_RIGHT, WORLD_BOTTOM);
        glVertex2f(WORLD_RIGHT, WORLD_TOP);    glVertex2f(WORLD_LEFT,  WORLD_TOP);
    glEnd();
}

// Advances both effects. Driven from the master tick, so it runs at a fixed
// rate regardless of what the active scenario is doing.
void UpdateTransition(float dt) {
    if (switchFade > 0.0f) {
        switchFade -= dt * 2.9f;                 // ~0.35s to clear
        if (switchFade < 0.0f) switchFade = 0.0f;
    }
    titleCardT += dt;
}

// ============================================================================
// ============================================================================
//  MASTER CLOCK
// ----------------------------------------------------------------------------
//  Every animation used to end with its own glutPostRedisplay(). With 13-24
//  timers live in the active scenario, all firing on ~30 ms, GLUT was being
//  asked to redraw roughly 600 times a second and the main loop spun as fast
//  as the machine allowed -- lots of heat, no extra smoothness, and a frame
//  rate that varied with whichever scenario was on screen.
//
//  Now the timers only advance state. This single tick issues exactly one
//  redisplay per frame, which pins the whole program to a steady ~62 fps on
//  every machine without touching any animation's speed.
// ============================================================================
const unsigned int FRAME_MS = 16;   // ~62.5 frames per second

void MasterTick(int) {
    UpdateTransition(FRAME_MS / 1000.0f);
    glutPostRedisplay();
    glutTimerFunc(FRAME_MS, MasterTick, 0);
}

void display() {
    switch (currentScreen) {
        case SCENARIO_1: Scenario1_CoastalCity::Draw();  break;
        case SCENARIO_2: Scenario2::Draw();               break;
        case SCENARIO_3: Scenario3::Draw();               break;
        case SCENARIO_4: Scenario4::Draw();               break;
    }

    // Drawn over whichever scene just rendered, in its own clean projection
    // so a scenario that changed the ortho box cannot shift the overlay.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    DrawTitleCard();
    DrawSwitchFade();

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    // ---- Navigation -------------------------------------------------------
    // N and B walk the scenes in order. They are letters, not numbers,
    // because 1/2/3/4 belong to whichever scenario is on screen.
    if (key == 'n' || key == 'N') { NextScenario();     return; }
    if (key == 'b' || key == 'B') { PreviousScenario(); return; }

    // ESC used to return to the home menu. With no menu to return to it
    // would be a dead key, so it now closes the program -- which is what
    // anyone sitting down in front of this will press and expect.
    if (key == 27) {
        exit(0);
    }

    // ---- Controls that mean the same thing in every scenario --------------
    if (key == ' ') {       // SPACE: pause / resume
        isPaused = !isPaused;
        glutPostRedisplay();
        return;
    }
    if (key == 'h' || key == 'H') {  // H: show / hide the help panel
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

// ---- Special keys ----------------------------------------------------------
//  F1-F4 jump straight to a scenario and the arrow keys mirror N and B.
//  These arrive through a different GLUT callback from the ordinary keyboard
//  handler, so they cannot clash with any scenario's own controls.
void special(int key, int /*x*/, int /*y*/) {
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

void mouse(int button, int state, int x, int y) {
    switch (currentScreen) {
        case SCENARIO_1: Scenario1_CoastalCity::Mouse(button, state, x, y); break;
        case SCENARIO_2: Scenario2::Mouse(button, state, x, y);              break;
        case SCENARIO_3: Scenario3::Mouse(button, state, x, y);              break;
        case SCENARIO_4: Scenario4::Mouse(button, state, x, y);              break;
    }
}

// ============================================================================
//  SHARED ONE-TIME GL SETUP + ENTRY POINT
// ============================================================================
void init() {
    #ifndef GL_MULTISAMPLE
    #define GL_MULTISAMPLE 0x809D
    #endif
    glPointSize(2.0f);
    glLineWidth(2.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Smooths every circle, sail, kite and rooftop edge in the project. The
    // buffer is requested in main() via GLUT_MULTISAMPLE; enabling it here is
    // harmless on drivers that could not supply one.
    glEnable(GL_MULTISAMPLE);
}

// ---- Window resize ---------------------------------------------------------
// Without this, resizing stretches every scene and breaks DrawTextCentered(),
// which converts pixel widths using the fixed WINDOW_WIDTH. The viewport is
// letterboxed so the world box keeps its 3:2 shape at any window size.
void reshape(int w, int h) {
    if (w <= 0 || h <= 0) return;

    const float targetAspect = (WORLD_RIGHT - WORLD_LEFT) / (WORLD_TOP - WORLD_BOTTOM);
    float windowAspect = (float)w / (float)h;

    int vpW = w, vpH = h, vpX = 0, vpY = 0;
    if (windowAspect > targetAspect) {         // too wide: bars left and right
        vpW = (int)(h * targetAspect);
        vpX = (w - vpW) / 2;
    } else {                                   // too tall: bars top and bottom
        vpH = (int)(w / targetAspect);
        vpY = (h - vpH) / 2;
    }
    glViewport(vpX, vpY, vpW, vpH);
    viewportPixelWidth = vpW;                  // text centring follows the viewport
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    // Without a seed every run produces the identical star field, window
    // pattern and firework timing.
    srand((unsigned int)time(nullptr));

    glutInit(&argc, argv);
    // GLUT_MULTISAMPLE asks for an anti-aliased buffer; drivers that cannot
    // supply one simply ignore it, so this is safe everywhere.
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("City Life - Group Project (4 Scenarios)");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    // F1-F4 and the arrow keys. Remove this one line if the navigation
    // should be N / B only.
    glutSpecialFunc(special);
    glutMouseFunc(mouse);

    // One redisplay per frame for the whole program (see MasterTick).
    glutTimerFunc(0, MasterTick, 0);

    // Each scenario's own one-time setup (also starts that scenario's
    // animation timers, so all four keep animating in the background
    // regardless of which one is currently on screen).
    Scenario1_CoastalCity::Init();
    Scenario2::Init();
    Scenario3::Init();
    Scenario4::Init();

    // Open on Scenario 1, with its title card up and the key hint showing.
    GoToScenarioIndex(0, true);

    glutMainLoop();
    return 0;
}

