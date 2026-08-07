// ============================================================================
// Smart Medieval Castle Drawbridge Simulation
// CSE412 - Computer Graphics Lab Project
//
// The entire project has been consolidated into this single file.
// The code features clear logical sections. The ANIMATION SEPARATION course
// requirement holds: animationUpdate() advances state, and display() only READS
// state and draws it.
// ============================================================================

#include <windows.h>    // GLUT on Windows needs this first
#include <math.h>
#include <stdio.h>      // snprintf
#include <GL/glut.h>

// ---------------------------------------------------------------
// Window / viewport configuration
// ---------------------------------------------------------------
#define WIN_W 900          // window width  (pixels)
#define WIN_H 600          // window height (pixels)

static int currentWinW = WIN_W;
static int currentWinH = WIN_H;

// World coordinate extent of the scene (the "world window").
// The viewport demo (Z key) re-maps a sub-region of this onto the window.
#define WORLD_LEFT   -100.0f
#define WORLD_RIGHT   100.0f
#define WORLD_BOTTOM  -60.0f
#define WORLD_TOP      60.0f

// ---------------------------------------------------------------
// Drawbridge geometry
// ---------------------------------------------------------------
// The bridge is hinged on the LEFT castle wall and, when closed, spans
// the river to rest on the RIGHT castle wall -- a real medieval drawbridge.
//
//   hinge (-40,-4)  +----------------- deck 80 long ----------------+
//   left tower       bridge from x=-40 (hinge) to x=+40 (free end)   right tower
//   ====H=============== bridge ================T====   (closed road)
//   ~~~~~~~~~~~~~~~~~~~~~~~~ river ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// The hinge is FIXED in world coordinates; the deck rotates only about it.
#define BRIDGE_HINGE_X  -40.0f   // hinge x: on the left tower's wall face
#define BRIDGE_HINGE_Y   -4.0f   // hinge y: deck bottom corner at river level
#define BRIDGE_LEN       80.0f   // deck length: hinge (-40) .. free end (+40)
#define BRIDGE_TOP        0.0f   // deck top surface (the road level)
#define BRIDGE_BOTTOM    -4.0f   // deck thickness
#define BRIDGE_MAX_ANGLE 70.0f   // fully open angle (degrees), 65..75 target

// ---------------------------------------------------------------
// Time
// ---------------------------------------------------------------
#define TIME_STEP 16.0f   // ms per animation tick (~60 FPS)

// ---------------------------------------------------------------
// Animation speed multiplier (changed with +/- keys)
// ---------------------------------------------------------------
#define SPEED_MIN 0.25f
#define SPEED_MAX 4.0f

// ---------------------------------------------------------------
// Bridge / ship finite state machine (single enum, no scattered booleans)
// ---------------------------------------------------------------
//   APPROACHING -> OPENING -> WAITING_OPEN -> PASSING -> CLOSING -> RESET
//
// The ship only moves during APPROACHING and PASSING; the bridge only
// rotates during OPENING and CLOSING. WAITING_OPEN holds the ship still
// until the bridge is fully raised so the ship never collides with it.
typedef enum BridgeState
{
    BRIDGE_STATE_APPROACHING = 0,
    BRIDGE_STATE_WAITING_FOR_BRIDGE,
    BRIDGE_STATE_OPENING,
    BRIDGE_STATE_OPEN,
    BRIDGE_STATE_PASSING_UNDER_BRIDGE,
    BRIDGE_STATE_CLEARED_BRIDGE,
    BRIDGE_STATE_CLOSING,
    BRIDGE_STATE_RESET,
    BRIDGE_STATE_COUNT
} BridgeState;

// --- theme transition ---
#define THEME_TRANSITION_TIME 1200.0f   // ms for a full day->night blend

// ---------------------------------------------------------------
// Simulation state (the only global struct in the project)
// ---------------------------------------------------------------
typedef struct SimState
{
    // --- drawbridge state machine ---
    BridgeState state;    // bridge FSM state, see animation section
    float bridgeAngle;    // current bridge angle (degrees, 0..BRIDGE_MAX_ANGLE)
    float shipY;          // ship y position (world coords)
    float shipPhase;      // for the ship's slow bobbing on the water

    // --- bridge FSM timing ---
    float stateTimer;     // seconds spent in the current state (WAITING_OPEN hold)

    // --- independent environment animations ---
    float cloudOffset;    // accumulated cloud translation
    float birdPhase;      // bird flight phase (drives wing flap + position)
    float flagPhase;      // flag wave phase
    float wavePhase;      // river wave phase

    // --- theme & time of day ---
    int   theme;          // 0 = day, 1 = sunset, 2 = night
    float themeBlend;     // 0..1 blend factor when transitioning

    // --- viewport / zoom ---
    float zoomFactor;     // 1.0 = full scene, >1 = zoomed into bridge
    float zoomX, zoomY;   // center of the zoomed view

    // --- global animation control ---
    float speed;          // speed multiplier (SPEED_MIN..SPEED_MAX)
    int   paused;         // 1 = paused, 0 = running

    // --- graphics concepts demonstration ---
    int   mirrorMode;     // 1 = reflected horizontally, 0 = normal
    int   showDemoMode;   // 1 = draw F1 Concepts Demo Overlay, 0 = normal
} SimState;

// The one global state object (defined in Animation section).
extern SimState sim;

// ============================================================================
// Section: Theme Management
// Color themes (day / sunset / night) and theme blending.
//
// Demonstrates COLOR MANIPULATION: the whole scene is tinted by a
// theme, and pressing D smoothly blends the sky, river and castle lighting
// between day -> sunset -> night instead of switching instantly.
// ============================================================================

// Theme ids (also stored in sim.theme)
#define THEME_DAY    0
#define THEME_SUNSET 1
#define THEME_NIGHT  2
#define THEME_COUNT  3

// Colors for a single theme.
typedef struct Theme
{
    float skyTop[3];      // sky gradient, top
    float skyBottom[3];   // sky gradient, bottom
    float ground[3];      // grass / ground
    float river[3];       // river water
    float castle[3];      // castle stone
    float roof[3];        // tower roofs
    float sun[3];         // sun (or moon at night)
    float star[3];        // stars (night only)
} Theme;

// Number of themes.
int themeCount(void);

// Returns a pointer to one of the three themes.
const Theme* getTheme(int index);

// Blends two themes with factor t in [0,1] into out.
void blendThemes(const Theme* a, const Theme* b, float t, Theme* out);

// Returns the theme currently displayed (may be a blend of two).
void currentTheme(Theme* out);

// ---------------------------------------------------------------
// The three themes. Colors are picked to read clearly on a bright
// background. Sun/moon positions also change per theme (handled in
// the scene section).
// ---------------------------------------------------------------
static const Theme themes[THEME_COUNT] = {
    // --- DAY ---
    {
        { 0.45f, 0.72f, 1.00f },  // sky top: bright blue
        { 0.78f, 0.90f, 1.00f },  // sky bottom: pale blue
        { 0.30f, 0.65f, 0.25f },  // ground: grass green
        { 0.20f, 0.55f, 0.90f },  // river: vivid blue
        { 0.58f, 0.55f, 0.50f },  // castle: light gray stone
        { 0.45f, 0.18f, 0.10f },  // roof: dark red-brown
        { 1.00f, 0.90f, 0.30f },  // sun: warm yellow
        { 1.00f, 1.00f, 1.00f },  // star
    },
    // --- SUNSET ---
    {
        { 0.85f, 0.35f, 0.25f },  // sky top: deep orange-red
        { 1.00f, 0.72f, 0.35f },  // sky bottom: golden orange
        { 0.45f, 0.35f, 0.20f },  // ground: brownish
        { 0.85f, 0.45f, 0.20f },  // river: reflects orange sky
        { 0.45f, 0.35f, 0.30f },  // castle: darker stone
        { 0.25f, 0.12f, 0.08f },  // roof: near-black red
        { 1.00f, 0.65f, 0.25f },  // sun: low orange
        { 1.00f, 1.00f, 1.00f },
    },
    // --- NIGHT ---
    {
        { 0.02f, 0.05f, 0.18f },  // sky top: deep navy
        { 0.10f, 0.14f, 0.30f },  // sky bottom: dark blue
        { 0.10f, 0.22f, 0.12f },  // ground: very dark green
        { 0.05f, 0.12f, 0.30f },  // river: dark navy
        { 0.25f, 0.25f, 0.30f },  // castle: dark slate
        { 0.10f, 0.06f, 0.06f },  // roof: dark
        { 0.95f, 0.95f, 0.85f },  // moon: pale
        { 1.00f, 1.00f, 0.90f },
    },
};

// ---------------------------------------------------------------
int themeCount(void) { return THEME_COUNT; }

const Theme* getTheme(int index)
{
    if (index < 0) index = 0;
    if (index >= THEME_COUNT) index = THEME_COUNT - 1;
    return &themes[index];
}

// ---------------------------------------------------------------
// Linear interpolation of every color channel between two themes.
// t = 0 -> a, t = 1 -> b.
// ---------------------------------------------------------------
void blendThemes(const Theme* a, const Theme* b, float t, Theme* out)
{
    int i;
    float* dst[8] = {
        out->skyTop, out->skyBottom, out->ground, out->river,
        out->castle, out->roof,      out->sun,    out->star
    };
    const float* srcA[8] = {
        a->skyTop, a->skyBottom, a->ground, a->river,
        a->castle, a->roof,      a->sun,    a->star
    };
    const float* srcB[8] = {
        b->skyTop, b->skyBottom, b->ground, b->river,
        b->castle, b->roof,      b->sun,    b->star
    };

    for (i = 0; i < 8; i++)
    {
        dst[i][0] = srcA[i][0] + (srcB[i][0] - srcA[i][0]) * t;
        dst[i][1] = srcA[i][1] + (srcB[i][1] - srcA[i][1]) * t;
        dst[i][2] = srcA[i][2] + (srcB[i][2] - srcA[i][2]) * t;
    }
}

// ---------------------------------------------------------------
// The currently displayed theme. While a transition is in progress
// (sim.themeBlend in (0,1)) this returns a blend between the old and
// the new theme; otherwise it returns the active theme unchanged.
// ---------------------------------------------------------------
void currentTheme(Theme* out)
{
    int cur = sim.theme;
    int next = (cur + 1) % THEME_COUNT;
    float t = sim.themeBlend;

    if (t <= 0.0f)
        *out = *getTheme(cur);
    else if (t >= 1.0f)
        *out = *getTheme(next);
    else
        blendThemes(getTheme(cur), getTheme(next), t, out);
}

// ============================================================================
// Section: Primitive Drawing Helpers
// Small shared drawing utilities.
// Keeps primitive drawing code in ONE place so the rest of the code stays short
// and focused on composition, not on GL boilerplate.
// ============================================================================

#define TWO_PI (6.283185307179586f)

// Draw a filled polygon from a vertex list (world coords).
//  - verts : array of (x, y) pairs
//  - count : number of vertices
// The current glColor is used for the fill.
void drawPolygon(const float* verts, int count);

// Draw an axis-aligned rectangle (x0,y0)..(x1,y1) with the current color.
void drawRect(float x0, float y0, float x1, float y1);

// Draw a circle (filled, radius r) centered at (cx, cy) with the current color.
//  - segments : polygonal approximation resolution
void drawCircle(float cx, float cy, float r, int segments);

// Draw a filled ellipse (squashed circle) centered at (cx, cy).
//  - rx, ry : horizontal / vertical radius
void drawEllipse(float cx, float cy, float rx, float ry, int segments);

// ---------------------------------------------------------------
// Filled polygon from vertex array
// ---------------------------------------------------------------
void drawPolygon(const float* verts, int count)
{
    int i;
    glBegin(GL_POLYGON);
    for (i = 0; i < count; i++)
        glVertex2f(verts[i * 2], verts[i * 2 + 1]);
    glEnd();
}

// ---------------------------------------------------------------
// Axis-aligned rectangle
// ---------------------------------------------------------------
void drawRect(float x0, float y0, float x1, float y1)
{
    const float v[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    drawPolygon(v, 4);
}

// ---------------------------------------------------------------
// Filled circle (polygonal approximation)
// ---------------------------------------------------------------
void drawCircle(float cx, float cy, float r, int segments)
{
    int i;
    float a, x, y;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);          // center
    for (i = 0; i <= segments; i++)
    {
        a = TWO_PI * (float)i / (float)segments;
        x = cx + r * (float)cos(a);
        y = cy + r * (float)sin(a);
        glVertex2f(x, y);
    }
    glEnd();
}

// ---------------------------------------------------------------
// Filled ellipse (squashed circle) -- used for clouds, trees, water
// ---------------------------------------------------------------
void drawEllipse(float cx, float cy, float rx, float ry, int segments)
{
    int i;
    float a, x, y;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);          // center
    for (i = 0; i <= segments; i++)
    {
        a = TWO_PI * (float)i / (float)segments;
        x = cx + rx * (float)cos(a);
        y = cy + ry * (float)sin(a);
        glVertex2f(x, y);
    }
    glEnd();
}

// ---------------------------------------------------------------
// Midpoint Circle & Bresenham Line Algorithms
// ---------------------------------------------------------------

// Helper to draw circle points for symmetry
static void drawCircleMidpointPoints(float cx, float cy, float fx, float fy, bool filled)
{
    if (filled)
    {
        glBegin(GL_LINES);
        glVertex2f(cx - fx, cy + fy); glVertex2f(cx + fx, cy + fy);
        glVertex2f(cx - fx, cy - fy); glVertex2f(cx + fx, cy - fy);
        glVertex2f(cx - fy, cy + fx); glVertex2f(cx + fy, cy + fx);
        glVertex2f(cx - fy, cy - fx); glVertex2f(cx + fy, cy - fx);
        glEnd();
    }
    else
    {
        glBegin(GL_POINTS);
        glVertex2f(cx + fx, cy + fy);
        glVertex2f(cx - fx, cy + fy);
        glVertex2f(cx + fx, cy - fy);
        glVertex2f(cx - fx, cy - fy);
        glVertex2f(cx + fy, cy + fx);
        glVertex2f(cx - fy, cy + fx);
        glVertex2f(cx + fy, cy - fx);
        glVertex2f(cx - fy, cy - fx);
        glEnd();
    }
}

// Draws a circle using the Midpoint Circle Algorithm (custom rasterization)
static void drawCircleMidpoint(float cx, float cy, float r, bool filled)
{
    int r_int = (int)(r * 50.0f);
    if (r_int < 1) r_int = 1;
    float scale = 1.0f / 50.0f;

    int x = 0;
    int y = r_int;
    int p = 1 - r_int;

    drawCircleMidpointPoints(cx, cy, x * scale, y * scale, filled);

    while (x < y)
    {
        x++;
        if (p < 0)
        {
            p += 2 * x + 1;
        }
        else
        {
            y--;
            p += 2 * (x - y) + 1;
        }
        drawCircleMidpointPoints(cx, cy, x * scale, y * scale, filled);
    }
}

// Draws a line using Bresenham's Line Algorithm (custom rasterization)
static void drawLineBresenham(float x1, float y1, float x2, float y2)
{
    int x1_i = (int)(x1 * 50.0f);
    int y1_i = (int)(y1 * 50.0f);
    int x2_i = (int)(x2 * 50.0f);
    int y2_i = (int)(y2 * 50.0f);

    int dx = abs(x2_i - x1_i);
    int dy = abs(y2_i - y1_i);
    int sx = (x1_i < x2_i) ? 1 : -1;
    int sy = (y1_i < y2_i) ? 1 : -1;
    int err = dx - dy;

    float scale = 1.0f / 50.0f;

    glBegin(GL_POINTS);
    while (true)
    {
        glVertex2f(x1_i * scale, y1_i * scale);

        if (x1_i == x2_i && y1_i == y2_i)
            break;

        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x1_i += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1_i += sy;
        }
    }
    glEnd();
}

// ============================================================================
// Section: Animation & FSM Controller
// The animation update loop: a finite state machine for the bridge/ship
// sequence plus independent environment animations.
//
// ANIMATION SEPARATION (key course requirement):
//   - animationUpdate() is called by GLUT's timer callback
//   - display() NEVER contains animation logic -- it only reads the state
//     updated here and draws it.
//
// Two kinds of update live here:
//
//   A) THE FINITE STATE MACHINE (bridge + ship)
//      APPROACH -> OPENING -> PASSING -> CLOSING -> repeat
//      The ship only moves during APPROACH and PASSING; the bridge only
//      rotates during OPENING and CLOSING. States transition on thresholds.
//
//   B) INDEPENDENT ENVIRONMENT ANIMATIONS
//      clouds drift, birds flap, flags wave, water ripples, ship bobs.
//      These run continuously and are NOT part of the FSM -- they advance
//      with their own phases, demonstrating several objects animating
//      independently in one scene.
//
// All motion is scaled by sim.speed (the +/- keys) and suspended while
// sim.paused (the Space key).
// ============================================================================

// Public prototypes (inlined here since the file is single-unit):
void initSim(void);         // Initializes all simulation state
void animationUpdate(float dt);  // Advances the simulation by dt ms
void drawHUD(void);         // Draws the on-screen state label

// ---------------------------------------------------------------
// Global state (defined here)
// ---------------------------------------------------------------
SimState sim;

// ---------------------------------------------------------------
// initSim: reset everything to the initial pose
// ---------------------------------------------------------------
void initSim(void)
{
    sim.state       = BRIDGE_STATE_APPROACHING;
    sim.bridgeAngle = 0.0f;
    sim.shipY       = -55.0f;   // start near bottom of the screen
    sim.shipPhase   = 0.0f;
    sim.stateTimer  = 0.0f;

    sim.cloudOffset = 0.0f;
    sim.birdPhase   = 0.0f;
    sim.flagPhase   = 0.0f;
    sim.wavePhase   = 0.0f;

    sim.theme       = THEME_DAY;
    sim.themeBlend  = 0.0f;

    sim.zoomFactor  = 1.0f;
    sim.zoomX       = 0.0f;
    sim.zoomY       = 0.0f;

    sim.speed       = 1.0f;
    sim.paused      = 0;

    sim.mirrorMode   = 0;
    sim.showDemoMode = 0;
}

// ---------------------------------------------------------------
// Animation state machine (ship + bridge)
// ---------------------------------------------------------------
static void updateBridgeStateMachine(float dt)
{
    // Named trigger lines & bounds
    const float APPROACH_LINE = -28.0f; // Stopped position for ship front
    const float BRIDGE_START  = -25.0f; // Beginning of bridge channel
    const float BRIDGE_END    = -15.0f; // End of bridge channel (clearance)
    const float CLEAR_LINE    = -15.0f; // Rear of ship must exceed this to clear channel

    // Ship and bridge speeds
    float shipSpeed = 15.0f;
    float bridgeSpeed = 25.0f;

    // Advance state timer
    sim.stateTimer += dt;

    // Helper to calculate ship front and rear using dynamic scale
    float t_scale = (sim.shipY - (-55.0f)) / (-5.0f - (-55.0f));
    if (t_scale < 0.0f) t_scale = 0.0f;
    if (t_scale > 1.0f) t_scale = 1.0f;
    float shipScale = 1.0f - t_scale * 0.8f;
    if (sim.shipY > -5.0f) {
        float t_inside = (sim.shipY - (-5.0f)) / (15.0f - (-5.0f));
        shipScale = 0.2f - t_inside * 0.19f;
    }
    if (shipScale < 0.01f) shipScale = 0.01f;

    float shipFront = sim.shipY + 5.0f * shipScale;
    float shipRear  = sim.shipY - 20.0f * shipScale;

    // Bounding Box / Channel check: is any part of the ship inside the channel?
    bool isShipInChannel = (shipFront >= BRIDGE_START && shipRear <= BRIDGE_END);

    // STATE MACHINE TRANSITIONS
    switch (sim.state)
    {
    case BRIDGE_STATE_APPROACHING:
        // Ship moves towards the bridge; bridge remains closed
        sim.shipY += shipSpeed * dt;
        if (sim.shipY >= APPROACH_LINE)
        {
            sim.shipY = APPROACH_LINE;
            sim.state = BRIDGE_STATE_WAITING_FOR_BRIDGE;
            sim.stateTimer = 0.0f;
        }
        break;

    case BRIDGE_STATE_WAITING_FOR_BRIDGE:
        // Ship is stopped. Transition to opening the bridge
        sim.state = BRIDGE_STATE_OPENING;
        sim.stateTimer = 0.0f;
        break;

    case BRIDGE_STATE_OPENING:
        // Ship is stopped outside the channel. Open the bridge
        if (!isShipInChannel)
        {
            sim.bridgeAngle += bridgeSpeed * dt;
            if (sim.bridgeAngle >= BRIDGE_MAX_ANGLE)
            {
                sim.bridgeAngle = BRIDGE_MAX_ANGLE;
                sim.state = BRIDGE_STATE_OPEN;
                sim.stateTimer = 0.0f;
            }
        }
        break;

    case BRIDGE_STATE_OPEN:
        // Bridge is fully open. Transition to ship passing
        sim.state = BRIDGE_STATE_PASSING_UNDER_BRIDGE;
        sim.stateTimer = 0.0f;
        break;

    case BRIDGE_STATE_PASSING_UNDER_BRIDGE:
        // Ship sails through the channel. Bridge is locked open (frozen)
        sim.shipY += shipSpeed * dt;
        if (shipRear >= CLEAR_LINE)
        {
            sim.state = BRIDGE_STATE_CLEARED_BRIDGE;
            sim.stateTimer = 0.0f;
        }
        break;

    case BRIDGE_STATE_CLEARED_BRIDGE:
        // Ship continues moving into the castle until it has completely vanished from sight
        sim.shipY += shipSpeed * dt;
        if (sim.shipY >= 15.0f)
        {
            sim.shipY = 15.0f;
            sim.state = BRIDGE_STATE_CLOSING;
            sim.stateTimer = 0.0f;
        }
        break;

    case BRIDGE_STATE_CLOSING:
        // Ship remains stopped inside the castle. Bridge closes only if it's safe (no overlap)
        if (!isShipInChannel)
        {
            sim.bridgeAngle -= bridgeSpeed * dt;
            if (sim.bridgeAngle <= 0.0f)
            {
                sim.bridgeAngle = 0.0f;
                sim.state = BRIDGE_STATE_RESET;
                sim.stateTimer = 0.0f;
            }
        }
        break;

    case BRIDGE_STATE_RESET:
        // Reset ship back to the foreground and repeat
        sim.shipY = -55.0f;
        sim.state = BRIDGE_STATE_APPROACHING;
        sim.stateTimer = 0.0f;
        break;

    default:
        sim.state = BRIDGE_STATE_APPROACHING;
        sim.stateTimer = 0.0f;
        break;
    }
}

// ---------------------------------------------------------------
// Independent environment animations (run every frame)
// ---------------------------------------------------------------
static void updateEnvironment(float dt)
{
    sim.cloudOffset += 6.0f * dt;                 // clouds drift right
    if (sim.cloudOffset > 200.0f)
        sim.cloudOffset -= 200.0f;                // wrap-around

    sim.birdPhase  += 2.2f * dt;                  // bird flight + wing flap
    sim.flagPhase  += 3.0f * dt;                  // flag wave
    sim.wavePhase  += 2.5f * dt;                  // water ripples
    sim.shipPhase  += 2.0f * dt;                  // ship bobbing + smoke
}

// ---------------------------------------------------------------
// Theme transition (day -> sunset -> night, back to day)
// ---------------------------------------------------------------
static void updateTheme(float dt)
{
    if (sim.themeBlend > 0.0f && sim.themeBlend < 1.0f)
    {
        sim.themeBlend += dt / THEME_TRANSITION_TIME;
        if (sim.themeBlend >= 1.0f)
        {
            sim.themeBlend = 0.0f;                // blend done
            sim.theme = (sim.theme + 1) % THEME_COUNT;  // move to next theme
        }
    }
}

// ---------------------------------------------------------------
// Public: the main animation entry point (called by GLUT timer/idle)
// ---------------------------------------------------------------
void animationUpdate(float dt)
{
    if (sim.paused)
        return;

    // apply the global speed multiplier
    dt *= sim.speed;

    updateBridgeStateMachine(dt);
    updateEnvironment(dt);
    updateTheme(dt);

    // request a repaint -- display() only reads state, never animates
    glutPostRedisplay();
}

// ---------------------------------------------------------------
// HUD: small text overlay showing the current sim state
// ---------------------------------------------------------------
void drawHUD(void)
{
    static const char* stateNames[BRIDGE_STATE_COUNT] = {
        "APPROACHING",
        "WAITING_FOR_BRIDGE",
        "OPENING",
        "OPEN",
        "PASSING_UNDER_BRIDGE",
        "CLEARED_BRIDGE",
        "CLOSING",
        "RESET"
    };
    char line[64];
    int i;

    // --- HUD text (screen-space, top-left) ---
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, currentWinW, 0, currentWinH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(12, currentWinH - 22);

    // first line: bridge state + pause flag
    snprintf(line, sizeof(line), "Bridge: %s  %s",
             stateNames[sim.state < BRIDGE_STATE_COUNT ? sim.state : 0],
             sim.paused ? "[PAUSED]" : "");
    for (i = 0; line[i]; i++)
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, line[i]);

    glRasterPos2i(12, currentWinH - 42);
    snprintf(line, sizeof(line), "Speed: %.2fx   Theme: %s   Zoom: %.1fx",
             sim.speed,
             sim.theme == THEME_DAY    ? "Day"    :
             sim.theme == THEME_SUNSET ? "Sunset" : "Night",
             sim.zoomFactor);
    for (i = 0; line[i]; i++)
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, line[i]);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// Section: Drawbridge Drawing
// The drawbridge: a hinged deck that rotates up around a hinge fixed on the
// LEFT castle wall, so the bridge reads as built into the castle gate.
//
// The deck is a rectangle from BRIDGE_HINGE_X to BRIDGE_HINGE_X + BRIDGE_LEN
// that rotates COUNTERCLOCKWISE about the fixed hinge (free end lifts UP).
// Rotation about an arbitrary fixed point uses the standard sequence:
//
//     glTranslatef(hingeX, hingeY, 0);   // move hinge to origin
//     glRotatef(-angle, 0, 0, 1);        // rotate about origin (free end up)
//     glTranslatef(-hingeX, -hingeY, 0); // move back to world coordinates
//
// The lifting cables start at the top of the LEFT tower and attach to the
// deck's FREE END. The free-end attachment points are recomputed every frame
// from sim.bridgeAngle, so the cables always follow the rotating bridge
// (they are never static).
//
// Demonstrates ROTATION about an arbitrary pivot (translate-to-pivot,
// rotate, translate-back) -- the classic OpenGL matrix sequence.
// ============================================================================

// Draws the drawbridge deck at its current angle (sim.bridgeAngle).
void drawBridge(const Theme* theme);

// ---------------------------------------------------------------
// Public: draw the drawbridge deck + lifting cables
// ---------------------------------------------------------------
// Helper to project a 3D bridge point into 2D screen space
// Helper to project a 2D rotating bridge point into screen space
static void projectBridgePoint(float lx, float ly, float angle, float* sx, float* sy)
{
    float rad = angle * 0.0174532925f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    
    // Rotate local coordinate (lx, ly) around the hinge at (-15.0f, -20.0f)
    *sx = -15.0f + lx * cosA - ly * sinA;
    *sy = -20.0f + lx * sinA + ly * cosA;
}

// Helper to draw a taut chain with visible links
static void drawChainLine(float x0, float y0, float x1, float y1)
{
    int i;
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    
    // Draw the main iron wire using Bresenham's Line Algorithm
    glColor3f(0.30f, 0.30f, 0.32f);
    glPointSize(3.0f);
    drawLineBresenham(x0, y0, x1, y1);
    glPointSize(1.0f);
    
    // Draw overlapping oval links using the Midpoint Circle Algorithm
    int numLinks = (int)(len / 2.5f);
    if (numLinks < 3) numLinks = 3;
    for (i = 0; i <= numLinks; i++)
    {
        float t = (float)i / (float)numLinks;
        float lx = x0 + t * dx;
        float ly = y0 + t * dy;
        
        glColor3f(0.20f, 0.20f, 0.22f);
        drawCircleMidpoint(lx, ly, 0.8f, true);
        glColor3f(0.40f, 0.40f, 0.42f);
        drawCircleMidpoint(lx, ly, 0.4f, true);
    }
}

// Draws the drawbridge deck at its current angle (sim.bridgeAngle).
void drawBridge(const Theme* theme)
{
    int i;
    float angle = sim.bridgeAngle;
    (void)theme;

    // --- 1. Horizontal road segments on left and right banks ---
    // Road color: gravel gray
    glColor3f(0.38f, 0.38f, 0.40f);
    drawRect(WORLD_LEFT, -22.0f, -15.0f, -18.0f);
    drawRect( 15.0f, -22.0f, WORLD_RIGHT, -18.0f);

    // Road borders (dark gray trim)
    glColor3f(0.25f, 0.25f, 0.27f);
    drawRect(WORLD_LEFT, -22.5f, -15.0f, -22.0f);
    drawRect(WORLD_LEFT, -18.0f, -15.0f, -17.5f);
    drawRect( 15.0f, -22.5f, WORLD_RIGHT, -22.0f);
    drawRect( 15.0f, -18.0f, WORLD_RIGHT, -17.5f);

    // --- 2. Gallows Post (vertical wooden frame on the left bank to hold chains) ---
    // Post base is on the left bank road edge
    glColor3f(0.35f, 0.22f, 0.10f); // dark wood
    drawRect(-18.0f, -20.0f, -15.0f, 15.0f); // main vertical post
    glColor3f(0.25f, 0.15f, 0.05f);
    drawRect(-18.5f, 15.0f, -14.5f, 16.0f);  // post cap

    // --- 3. Top Deck Planks (alternating colors for texture) ---
    int numPlanks = 10;
    float bridgeLength = 30.0f; // distance between banks
    float halfThickness = 2.0f; // road half-width (along Y)

    for (i = 0; i < numPlanks; i++)
    {
        float u0 = ((float)i / (float)numPlanks) * bridgeLength;
        float u1 = ((float)(i + 1) / (float)numPlanks) * bridgeLength;
        
        float ax, ay, bx, by, cx, cy, dx, dy;
        projectBridgePoint(u0, -halfThickness, angle, &ax, &ay);
        projectBridgePoint(u0,  halfThickness, angle, &bx, &by);
        projectBridgePoint(u1,  halfThickness, angle, &cx, &cy);
        projectBridgePoint(u1, -halfThickness, angle, &dx, &dy);
        
        // Alternate plank colors
        if (i % 2 == 0)
            glColor3f(0.45f, 0.28f, 0.12f);
        else
            glColor3f(0.40f, 0.25f, 0.10f);
            
        float verts[8] = { ax, ay, bx, by, cx, cy, dx, dy };
        drawPolygon(verts, 4);
        
        // Draw plank separator lines using Bresenham's Line Algorithm
        glColor3f(0.25f, 0.15f, 0.05f);
        drawLineBresenham(ax, ay, bx, by);
    }

    // --- 4. Deck Front Lip (solid thickness face at the outer end) ---
    float ex, ey, fx, fy;
    projectBridgePoint(bridgeLength, -halfThickness, angle, &ex, &ey);
    projectBridgePoint(bridgeLength,  halfThickness, angle, &fx, &fy);
    
    glColor3f(0.30f, 0.18f, 0.08f); // Darker wood side
    float lipVerts[8] = {
        ex, ey,
        fx, fy,
        fx - 1.5f * cosf(angle * 0.01745f), fy - 1.5f * sinf(angle * 0.01745f),
        ex - 1.5f * cosf(angle * 0.01745f), ey - 1.5f * sinf(angle * 0.01745f)
    };
    drawPolygon(lipVerts, 4);

    // --- 5. Dynamic Taut Lifting Chains ---
    // Anchored at the top of the left bank post
    float anchorX = -16.5f, anchorY = 15.0f;
    
    // Connected to the outer corners of the deck (slightly indented)
    float chain1X, chain1Y, chain2X, chain2Y;
    projectBridgePoint(bridgeLength * 0.95f, -halfThickness, angle, &chain1X, &chain1Y);
    projectBridgePoint(bridgeLength * 0.95f,  halfThickness, angle, &chain2X, &chain2Y);
    
    drawChainLine(anchorX, anchorY, chain1X, chain1Y);
    drawChainLine(anchorX, anchorY, chain2X, chain2Y);
}

// ============================================================================
// Section: Castle Walls & Towers Drawing
// The castle: two flanking towers, connecting walls and battlements, with a
// GATE OPENING in each wall where the drawbridge road passes through.
//
// World layout (all coordinates in world units):
//
//   left tower   x: -90..-40   (wall face at x = -40  = bridge hinge)
//   right tower  x:  40.. 90   (wall face at x = +40  = bridge resting point)
//   river top edge is at y = 0; towers sit on the grass at y = -8.
//
// The bridge (drawn separately) spans x = -40..+40, hinged on the left wall
// face and resting on the right wall face when closed -- the road through
// the gate is continuous.
//
// The flags on the tower roofs wave using rotation: each flag is drawn as a
// triangle that rotates around the top of its pole. The rotation angle comes
// from a global phase (sim.flagPhase) so all flags wave in sync.
//
// Demonstrates:
//  - scene composition (nested shapes built from GL primitives)
//  - rotation (the flags wave by rotating around the flagpole top)
// ============================================================================

// Draws the gateway interior background.
void drawCastleBackground(const Theme* theme);

// Draws the flanking towers, walls, and arch over the gate.
void drawCastleForeground(const Theme* theme);

// ---------------------------------------------------------------
// Local helpers (static = private to this section)
// ---------------------------------------------------------------

// Draws a single crenellated battlement block centered on (x, y).
// Battlements are the little "teeth" on top of a castle wall.
static void drawBattlement(float x, float y, float w, float h)
{
    drawRect(x - w * 0.5f, y, x + w * 0.5f, y + h);
}

// Draws a row of battlements along the top of a wall.
static void drawBattlements(float x0, float x1, float topY, float h, float gap)
{
    float x;
    for (x = x0; x <= x1 - gap; x += gap * 2.0f)
        drawBattlement(x + gap, topY, gap, h);
}

// Draws a triangle roof over a tower, plus the flagpole and waving flag.
static void drawTowerTop(float cx, float baseY, float halfW, float roofH,
                         const Theme* theme, int hasFlag)
{
    // --- roof (triangle) ---
    glColor3fv(theme->roof);
    {
        const float v[6] = {
            cx - halfW, baseY,
            cx + halfW, baseY,
            cx,         baseY + roofH
        };
        drawPolygon(v, 3);
    }

    if (hasFlag)
    {
        // --- flagpole (drawn using Bresenham Line Algorithm) ---
        glColor3f(0.35f, 0.3f, 0.25f);
        glPointSize(2.0f);
        drawLineBresenham(cx, baseY + roofH, cx, baseY + roofH + 9.0f);
        glPointSize(1.0f);

        // --- flag: waving cloth beside the pole top ---
        glPushMatrix();
        glTranslatef(cx, baseY + roofH + 9.0f, 0.0f);
        
        glColor3f(0.8f, 0.15f, 0.1f);   // red flag
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 8; i++)
        {
            float t = (float)i / 8.0f;
            float x = 0.7f + t * 8.0f;
            // Waving offset based on a travelling sine wave along the flag
            float wave = 0.8f * t * sinf(sim.flagPhase * 3.0f - t * 5.0f);
            float y_top = (0.0f * (1.0f - t) + (-1.25f) * t) + wave;
            float y_bottom = (-2.5f * (1.0f - t) + (-1.25f) * t) + wave;

            glVertex2f(x, y_top);
            glVertex2f(x, y_bottom);
        }
        glEnd();
        glPopMatrix();
    }
}

// ---------------------------------------------------------------
// Public: draw the gateway background interior
// ---------------------------------------------------------------
void drawCastleBackground(const Theme* theme)
{
    (void)theme;
    
    // 1. Floor of the gateway corridor (stone paving)
    glColor3f(0.20f, 0.18f, 0.16f);
    float floor[8] = { -9.0f, -5.0f, 9.0f, -5.0f, 5.0f, 12.0f, -5.0f, 12.0f };
    drawPolygon(floor, 4);

    // 2. Left interior corridor wall (stone)
    glColor3f(0.15f, 0.14f, 0.13f);
    float leftWall[8] = { -9.0f, -5.0f, -5.0f, 12.0f, -5.0f, 18.0f, -9.0f, 18.0f };
    drawPolygon(leftWall, 4);

    // 3. Right interior corridor wall (stone)
    glColor3f(0.12f, 0.11f, 0.10f);
    float rightWall[8] = { 9.0f, -5.0f, 9.0f, 18.0f, 5.0f, 18.0f, 5.0f, 12.0f };
    drawPolygon(rightWall, 4);

    // 4. Ceiling of the corridor (dark arch)
    glColor3f(0.10f, 0.09f, 0.08f);
    float ceiling[8] = { -9.0f, 18.0f, 9.0f, 18.0f, 5.0f, 18.0f, -5.0f, 18.0f };
    drawPolygon(ceiling, 4);

    // 5. Stone brick joint lines on the corridor walls for realism
    glColor3f(0.06f, 0.05f, 0.05f);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    // Left wall joints
    glVertex2f(-9.0f, 2.0f); glVertex2f(-7.0f, 12.0f);
    glVertex2f(-9.0f, 10.0f); glVertex2f(-6.0f, 12.0f);
    // Right wall joints
    glVertex2f(9.0f, 2.0f); glVertex2f(7.0f, 12.0f);
    glVertex2f(9.0f, 10.0f); glVertex2f(6.0f, 12.0f);
    glEnd();
    glLineWidth(1.0f);

    // 6. Glowing Torch / Lantern on the left wall
    // lantern glow (alpha-blended radial overlay)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.95f, 0.65f, 0.15f, 0.25f);
    drawCircle(-6.5f, 7.0f, 3.5f, 12);
    glDisable(GL_BLEND);

    // lantern flame
    glColor3f(0.95f, 0.70f, 0.20f);
    drawRect(-6.9f, 6.6f, -6.1f, 7.4f);

    // lantern frame (iron housing)
    glColor3f(0.20f, 0.18f, 0.15f);
    drawRect(-7.2f, 6.0f, -5.8f, 6.6f);  // bottom
    drawRect(-7.2f, 7.4f, -5.8f, 8.0f);  // top cap
    glBegin(GL_LINES);
    glVertex2f(-7.2f, 6.0f); glVertex2f(-7.2f, 8.0f);
    glVertex2f(-5.8f, 6.0f); glVertex2f(-5.8f, 8.0f);
    glEnd();

    // 7. Raised Iron Portcullis (grate) hanging from the ceiling
    glColor3f(0.15f, 0.15f, 0.18f); // dark metal
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    // vertical iron bars
    for (float bx = -8.0f; bx <= 8.0f; bx += 2.0f)
    {
        glVertex2f(bx, 18.0f);
        glVertex2f(bx * 0.9f, 13.0f); // hangs down slightly below arch
    }
    // horizontal iron bars
    glVertex2f(-8.0f, 16.5f); glVertex2f(8.0f, 16.5f);
    glVertex2f(-7.5f, 14.5f); glVertex2f(7.5f, 14.5f);
    glEnd();
    glLineWidth(1.0f);
}

// Public: draw the foreground wall, towers, and roofs
// ---------------------------------------------------------------
void drawCastleForeground(const Theme* theme)
{
    // ---- left wall ----
    glColor3fv(theme->castle);
    drawRect(-100.0f, -5.0f, -25.0f, 18.0f);
    drawBattlements(-98.0f, -27.0f, 18.0f, 4.0f, 4.0f);

    // ---- right wall ----
    glColor3fv(theme->castle);
    drawRect(25.0f, -5.0f, 100.0f, 18.0f);
    drawBattlements(27.0f, 98.0f, 18.0f, 4.0f, 4.0f);

    // ---- left gate tower ----
    glColor3fv(theme->castle);
    drawRect(-25.0f, -5.0f, -9.0f, 38.0f);
    drawBattlements(-23.0f, -11.0f, 38.0f, 4.0f, 4.0f);
    drawTowerTop(-17.0f, 38.0f, 8.0f, 15.0f, theme, 1); // Flags restored on top

    // ---- right gate tower ----
    glColor3fv(theme->castle);
    drawRect(9.0f, -5.0f, 25.0f, 38.0f);
    drawBattlements(11.0f, 23.0f, 38.0f, 4.0f, 4.0f);
    drawTowerTop(17.0f, 38.0f, 8.0f, 15.0f, theme, 1); // Flags restored on top

    // ---- wall arch connecting the towers above the gate ----
    glColor3fv(theme->castle);
    drawRect(-9.0f, 12.0f, 9.0f, 18.0f);
    drawBattlements(-7.0f, 7.0f, 18.0f, 3.0f, 3.0f);

    // Decorative shield in the center of the arch (rendered with Midpoint Circle Algorithm)
    glColor3f(0.85f, 0.70f, 0.20f); // gold trim
    drawCircleMidpoint(0.0f, 15.0f, 2.5f, true);
    glColor3f(0.80f, 0.15f, 0.10f); // red center
    drawCircleMidpoint(0.0f, 15.0f, 1.8f, true);
    glColor3f(0.85f, 0.70f, 0.20f); // gold inner ring
    drawCircleMidpoint(0.0f, 15.0f, 1.0f, false);

    // ---- small windows on both towers (glow at night) ----
    float wColor[3];
    if (sim.theme == THEME_DAY) {
        wColor[0] = 0.15f; wColor[1] = 0.12f; wColor[2] = 0.10f;
    } else if (sim.theme == THEME_SUNSET) {
        wColor[0] = 0.80f; wColor[1] = 0.50f; wColor[2] = 0.20f;
    } else { // Night
        wColor[0] = 0.95f; wColor[1] = 0.80f; wColor[2] = 0.30f;
    }
    // Handle theme transitions smoothly
    if (sim.themeBlend > 0.0f) {
        float nextColor[3];
        int nextTheme = (sim.theme + 1) % 3;
        if (nextTheme == THEME_DAY) {
            nextColor[0] = 0.15f; nextColor[1] = 0.12f; nextColor[2] = 0.10f;
        } else if (nextTheme == THEME_SUNSET) {
            nextColor[0] = 0.80f; nextColor[1] = 0.50f; nextColor[2] = 0.20f;
        } else {
            nextColor[0] = 0.95f; nextColor[1] = 0.80f; nextColor[2] = 0.30f;
        }
        wColor[0] += (nextColor[0] - wColor[0]) * sim.themeBlend;
        wColor[1] += (nextColor[1] - wColor[1]) * sim.themeBlend;
        wColor[2] += (nextColor[2] - wColor[2]) * sim.themeBlend;
    }

    glColor3fv(wColor);
    // Draw windows on left tower
    drawRect(-21.0f, 12.0f, -18.0f, 18.0f);
    drawRect(-15.0f, 22.0f, -12.0f, 28.0f);
    // Draw windows on right tower
    drawRect(12.0f, 12.0f, 15.0f, 18.0f);
    drawRect(18.0f, 22.0f, 21.0f, 28.0f);
}

// ============================================================================
// Section: River Water & Waves Drawing
// The river: a wide strip of water below the bridge.
//
// The river sits BELOW the road level (its top edge at y = -4, just under
// the bridge deck). The water is drawn as a big rectangle (base color from
// the theme) plus a band of animated "ripples": a strip of small light/dark
// sine waves that move with time (sim.wavePhase), so the water appears to
// flow without the geometry actually changing -- a classic animation trick
// in 2D graphics.
//
// Demonstrates COLOR MANIPULATION (the river tint follows the theme) and
// independent animation (a sine-based ripple band "flows" with sim.wavePhase).
// ============================================================================

#define RIVER_TOP      -5.0f

// Draws the river (static base + animated ripple band).
void drawRiver(const Theme* theme);

// ---------------------------------------------------------------
// Public: draw the river
// ---------------------------------------------------------------
void drawRiver(const Theme* theme)
{
    int j, k;

    // --- base water (canal) ---
    glColor3fv(theme->river);
    float canalVerts[8] = { -64.0f, -150.0f, 64.0f, -150.0f, 9.0f, -5.0f, -9.0f, -5.0f };
    drawPolygon(canalVerts, 4);

    // --- animated perspective ripples ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    for (j = 0; j < 6; j++)
    {
        float v = (float)j / 5.0f;
        float ry_center = -140.0f + v * 133.0f;
        float W_y = 60.0f - v * 51.0f; // half-width of the canal at this depth
        
        glColor4f(0.25f, 0.40f, 0.75f, 0.6f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_STRIP);
        int numSegs = 24;
        for (k = 0; k <= numSegs; k++)
        {
            float t = (float)k / (float)numSegs;
            float rx = -W_y + t * (2.0f * W_y);
            float amp = 1.2f * (1.0f - v * 0.7f); // smaller amplitude in the distance
            float ry = ry_center + amp * sinf(rx * 0.4f + sim.wavePhase * 1.5f);
            glVertex2f(rx, ry);
        }
        glEnd();
        glLineWidth(1.0f);
    }
    
    glDisable(GL_BLEND);
}

// ============================================================================
// Section: Background Scene Environment (Sky, Sun/Moon, Clouds, Birds)
// The environment: sky, sun/moon, stars, clouds, ground, trees and birds.
//
// LAYERING ORDER (back to front):
//   1. sky gradient
//   2. stars (night only) + sun/moon
//   3. distant clouds
//   4. ground
//   5. trees
//   6. birds (drawn last so they fly in front of everything)
//
// Clouds drift with TRANSLATION (sim.cloudOffset). Birds fly in a circular
// pattern with ROTATION of their wings (sim.birdPhase). The sun/moon and
// star visibility depend on the active theme (COLOR MANIPULATION).
//
// Demonstrates:
//  - gradient sky (color interpolation between two theme colors)
//  - independent animation (clouds drift, birds flap + glide)
//  - scene composition (trees, ground, sky layered in the right order)
// ============================================================================

// Draws the whole background environment.
void drawScene(const Theme* theme);

// ---------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------

// A single cloud = three overlapping ellipses. Position is (cx, cy),
// scale is cloud size (1 = normal).
static void drawCloud(float cx, float cy, float s)
{
    glColor3f(1.0f, 1.0f, 1.0f);
    drawEllipse(cx, cy,        7.0f * s, 3.0f * s, 16);
    drawEllipse(cx - 5.0f * s, cy + 1.0f * s, 4.5f * s, 2.5f * s, 16);
    drawEllipse(cx + 5.0f * s, cy + 0.5f * s, 4.0f * s, 2.2f * s, 16);
}

// A tree = brown trunk + two green canopy circles.
static void drawTree(float cx, float cy, float s)
{
    glColor3f(0.45f, 0.30f, 0.12f);
    drawRect(cx - 1.5f * s, cy, cx + 1.5f * s, cy + 6.0f * s);

    glColor3f(0.12f, 0.45f, 0.12f);
    drawCircle(cx, cy + 7.0f * s, 4.5f * s, 16);
    drawCircle(cx - 2.5f * s, cy + 5.0f * s, 3.0f * s, 16);
    drawCircle(cx + 2.5f * s, cy + 5.0f * s, 3.0f * s, 16);
}

// A bird = two wing triangles that rotate around the body center.
static void drawBird(float cx, float cy, float flap)
{
    float angle = 25.0f * sinf(flap);   // flap angle

    glColor3f(0.15f, 0.15f, 0.15f);

    // body
    drawEllipse(cx, cy, 1.2f, 0.6f, 10);

    // left wing (rotates around the body)
    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
        glVertex2f( 0.0f, 0.0f);
        glVertex2f(-3.5f, 1.5f);
        glVertex2f(-3.5f, -1.5f);
    glEnd();
    glPopMatrix();

    // right wing
    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);
    glRotatef(-angle, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
        glVertex2f( 0.0f, 0.0f);
        glVertex2f( 3.5f, 1.5f);
        glVertex2f( 3.5f, -1.5f);
    glEnd();
    glPopMatrix();
}

// ---------------------------------------------------------------
// Public: draw the whole environment
// ---------------------------------------------------------------
void drawScene(const Theme* theme)
{
    int i;

    // ---- 1. sky gradient (top color -> bottom color) ----
    glBegin(GL_QUADS);
        glColor3fv(theme->skyTop);
        glVertex2f(WORLD_LEFT, 150.0f);
        glVertex2f(WORLD_RIGHT, 150.0f);
        glColor3fv(theme->skyBottom);
        glVertex2f(WORLD_RIGHT, -150.0f);
        glVertex2f(WORLD_LEFT, -150.0f);
    glEnd();

    // ---- 2. stars (night) + sun / moon ----
    if (sim.theme == THEME_NIGHT)
    {
        glColor3fv(theme->star);
        // a few fixed stars (small squares)
        {
            float starX[6] = { -80, -40, 0, 40, 70, -20 };
            float starY[6] = {  40,  30, 45, 25, 38, 48 };
            for (i = 0; i < 6; i++)
                drawRect(starX[i], starY[i], starX[i] + 1.5f, starY[i] + 1.5f);
        }
    }

    // sun (day/sunset) or moon (night): same position, different color,
    // demonstrating COLOR MANIPULATION (using Midpoint Circle Algorithm).
    glColor3fv(theme->sun);
    drawCircleMidpoint(60.0f, 45.0f, 8.0f, true);

    // ---- 3. clouds (drifting) ----
    // cloudOffset advances in the animation section; wrapping keeps them in
    // range.
    {
        float c = sim.cloudOffset;
        // Draw three clouds that drift completely off-screen (range -130 to 130) before wrapping
        drawCloud( fmodf(c, 260.0f) - 130.0f,          38.0f, 1.0f);
        drawCloud( fmodf(c + 80.0f, 260.0f) - 130.0f,   30.0f, 0.7f);
        drawCloud( fmodf(c + 160.0f, 260.0f) - 130.0f,  44.0f, 1.2f);
    }

    // ---- 4. ground (perspective banks extended to -150.0f to avoid aspect gaps) ----
    glColor3fv(theme->ground);
    // Left bank
    float leftBank[8] = { WORLD_LEFT, -150.0f, -64.0f, -150.0f, -9.0f, -5.0f, WORLD_LEFT, -5.0f };
    drawPolygon(leftBank, 4);
    // Right bank
    float rightBank[8] = { WORLD_RIGHT, -150.0f, 64.0f, -150.0f, 9.0f, -5.0f, WORLD_RIGHT, -5.0f };
    drawPolygon(rightBank, 4);
    // Distant background ground strip
    drawRect(WORLD_LEFT, -5.0f, WORLD_RIGHT, 0.0f);

    // Lighter grass borders along the river canal
    glColor3f(0.42f, 0.72f, 0.35f);
    float leftBorder[8] = { -64.0f, -150.0f, -60.0f, -150.0f, -8.0f, -5.0f, -9.0f, -5.0f };
    drawPolygon(leftBorder, 4);
    float rightBorder[8] = { 64.0f, -150.0f, 60.0f, -150.0f, 8.0f, -5.0f, 9.0f, -5.0f };
    drawPolygon(rightBorder, 4);

    // (Trees removed from here to be drawn on top of the castle walls in display)

    // ---- 6. birds (flying) ----
    // two birds follow a slow circular path around the sky, with flapping
    // wings (rotation driven by birdPhase).
    {
        float bx = 40.0f * cosf(sim.birdPhase * 0.3f);
        float by = 42.0f + 10.0f * sinf(sim.birdPhase * 0.3f);
        drawBird(bx, by, sim.birdPhase);

        float bx2 = -30.0f * cosf(sim.birdPhase * 0.22f);
        float by2 = 38.0f + 8.0f * sinf(sim.birdPhase * 0.22f);
        drawBird(bx2, by2, sim.birdPhase * 1.3f);
    }
}

// ---------------------------------------------------------------
// Helper to draw all environment trees on top of the castle walls
// ---------------------------------------------------------------
void drawTrees(void)
{
    drawTree(-60.0f, -45.0f, 1.3f);
    drawTree(-45.0f, -33.0f, 0.9f); // moved away from horizontal road
    drawTree(-25.0f, -10.0f, 0.5f);
    drawTree( 60.0f, -45.0f, 1.3f);
    drawTree( 45.0f, -33.0f, 0.9f); // moved away from horizontal road
    drawTree( 25.0f, -10.0f, 0.5f);
}

// ============================================================================
// Section: Sailing Ship Drawing
// A sailing ship with a hull, mast, sail and chimney smoke.
//
// The whole ship is drawn in local coordinates centered on the hull, then
// translated to (shipX, sim.shipY) for perspective diagonal depth translation.
// The hull also bobs gently with the waves (tiny rotation about the hull center,
// driven by sim.shipPhase). Smoke puffs are emitted along the ship's path and
// drift up, demonstrating independent particle-style translation.
//
// Demonstrates TRANSLATION (glTranslatef moves the whole ship along the
// river) and scene composition (hull + mast + sail + smoke).
// ============================================================================

#define SMOKE_COUNT 5
#define SMOKE_STEP  3.0f   // world units between smoke puffs

// Draws the ship at its current position (sim.shipY, on the water).
void drawShip(const Theme* theme);

// ---------------------------------------------------------------
// Public: draw the ship at its current position
// ---------------------------------------------------------------
void drawShip(const Theme* theme)
{
    int i;

    // Hide the ship when it has entered the castle and the bridge is closing or resetting
    if (sim.state == BRIDGE_STATE_CLOSING || sim.state == BRIDGE_STATE_RESET)
        return;

    glPushMatrix();
    
    // Calculate diagonal path progress (t from 0 to 1 as it approaches the gate)
    float t = (sim.shipY - (-55.0f)) / (-5.0f - (-55.0f));
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    // Diagonal path: start at x = -8.0f in foreground, sail straight/diagonally to x = 0.0f at gate
    float shipX = -8.0f * (1.0f - t);
    
    // Perspective scale: scale down from 1.0f to 0.2f
    float shipScale = 1.0f - t * 0.8f;
    if (sim.shipY > -5.0f)
    {
        // Continue to shrink inside the gateway towards the vanishing point
        float t_inside = (sim.shipY - (-5.0f)) / (15.0f - (-5.0f));
        shipScale = 0.2f - t_inside * 0.19f;
    }
    if (shipScale < 0.01f) shipScale = 0.01f;

    // Apply main translation and perspective scale
    glTranslatef(shipX, sim.shipY, 0.0f);
    glScalef(shipScale, shipScale, 1.0f);

    // 2D Reflection - Horizontal Mirror Mode (demonstrating reflection)
    if (sim.mirrorMode)
    {
        glScalef(-1.0f, 1.0f, 1.0f);
    }

    // Gentle bobbing on the water (rotation about local bottom center)
    glTranslatef(0.0f, -10.0f, 0.0f);
    glRotatef(1.5f * sinf(sim.shipPhase), 0.0f, 0.0f, 1.0f);
    glTranslatef(0.0f, 10.0f, 0.0f);

    // --- hull (symmetrical stern view) ---
    glColor3f(0.38f, 0.20f, 0.08f); // dark wood
    {
        const float v[8] = {
            -18.0f, -8.0f,   // top left
             18.0f, -8.0f,   // top right
             13.0f, -18.0f,  // bottom right
            -13.0f, -18.0f   // bottom left
        };
        drawPolygon(v, 4);
    }
    // Hull trim / stern rim (adds detail)
    glColor3f(0.48f, 0.30f, 0.15f);
    drawRect(-19.0f, -9.0f, 19.0f, -8.0f);

    // --- stern castle / cabin (centered) ---
    glColor3f(0.45f, 0.25f, 0.12f);
    drawRect(-14.0f, -8.0f, 14.0f, 2.0f);
    
    // Cabin roof
    glColor3f(0.45f, 0.18f, 0.10f); // roof color
    float roof[8] = { -14.0f, 2.0f, 14.0f, 2.0f, 10.0f, 6.0f, -10.0f, 6.0f };
    drawPolygon(roof, 4);

    // Symmetrical glowing cabin windows
    glColor3f(0.95f, 0.80f, 0.30f); // glowing yellow
    drawRect(-9.0f, -4.0f, -3.0f, 0.0f);
    drawRect( 3.0f, -4.0f,  9.0f, 0.0f);
    // window frames
    glColor3f(0.20f, 0.10f, 0.05f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(-6.0f, -4.0f); glVertex2f(-6.0f, 0.0f);
    glVertex2f(-9.0f, -2.0f); glVertex2f(-3.0f, -2.0f);
    glVertex2f( 6.0f, -4.0f); glVertex2f( 6.0f, 0.0f);
    glVertex2f( 3.0f, -2.0f); glVertex2f( 9.0f, -2.0f);
    glEnd();

    // --- center mast (tall, single) ---
    glColor3f(0.35f, 0.22f, 0.10f);
    drawRect(-1.5f, 2.0f, 1.5f, 32.0f);

    // --- yardarm (horizontal mast beam) ---
    glColor3f(0.35f, 0.22f, 0.10f);
    drawRect(-20.0f, 27.0f, 20.0f, 28.5f);

    // --- main sail (symmetrical, square rig) ---
    glColor3f(0.95f, 0.95f, 0.90f);
    {
        float sail[8] = {
            -18.0f, 27.0f,
             18.0f, 27.0f,
             15.0f, 10.0f,
            -15.0f, 10.0f
        };
        drawPolygon(sail, 4);
    }

    // --- pennant (waving cloth flying to the right from the mast top) ---
    glColor3f(0.85f, 0.15f, 0.10f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 8; i++)
    {
        float t = (float)i / 8.0f;
        float x = 1.5f + t * 8.0f;
        // Waving offset based on traveling sine wave
        float wave = 0.8f * t * sinf(sim.flagPhase * 3.0f - t * 5.0f);
        float y_top = (32.0f * (1.0f - t) + 30.75f * t) + wave;
        float y_bottom = (29.5f * (1.0f - t) + 30.75f * t) + wave;

        glVertex2f(x, y_top);
        glVertex2f(x, y_bottom);
    }
    glEnd();

    // --- steam chimney (placed on the side deck) ---
    glColor3f(0.20f, 0.20f, 0.20f);
    drawRect(-7.0f, 2.0f, -5.0f, 10.0f);
    glColor3f(0.10f, 0.10f, 0.10f);
    drawRect(-7.5f, 10.0f, -4.5f, 11.0f); // chimney lip

    // --- smoke puffs ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    for (i = 0; i < SMOKE_COUNT; i++)
    {
        float back = (float)i * SMOKE_STEP;
        float t_smoke = sim.shipPhase * 0.8f + back * 0.25f;
        float px   = -6.0f; // rises straight up from chimney
        float py   = 11.0f + fmodf(t_smoke, 12.0f);

        float alpha = 0.40f - (fmodf(t_smoke, 12.0f) / 12.0f) * 0.35f;
        glColor4f(0.70f, 0.70f, 0.72f, alpha);
        drawCircle(px, py, 1.5f + 0.5f * sinf(t_smoke), 8);
    }
    glDisable(GL_BLEND);

    glPopMatrix();
}

// ============================================================================
// Section: Keyboard Input Handlers
// All keyboard interaction. The viewport / zoom demo lives here too:
//
//   Z  -> zoom into the bridge (view a sub-region of the world)
//   X  -> reset to the full scene
//
// Zoom is implemented as a VIEWPORT TRANSFORMATION: we change the ortho
// projection (the world window) while keeping the pixel viewport fixed.
// The ship/bridge keep animating -- only the viewing region changes.
//
// Demonstrates USER INTERACTION -- the demo is driven by a handful of keys
// that change animation, color theme and the viewport.
// ============================================================================
// ============================================================================

#define ZOOM_FACTOR 2.2f   // how much Z zooms in

// Registers GLUT keyboard callbacks (normal + special keys).
void initInput(void);

// ---------------------------------------------------------------
// Camera / viewport helpers
// ---------------------------------------------------------------
static void zoomToBridge(void)
{
    // center on the bridge region (between the castle walls)
    sim.zoomX = 0.0f;
    sim.zoomY = 5.0f;
    sim.zoomFactor = ZOOM_FACTOR;
}

static void resetZoom(void)
{
    sim.zoomX = 0.0f;
    sim.zoomY = 0.0f;
    sim.zoomFactor = 1.0f;
}

// ---------------------------------------------------------------
// Normal keyboard
// ---------------------------------------------------------------
static void keyboard(unsigned char key, int x, int y)
{
    (void)x; (void)y;   // unused

    switch (key)
    {
    case ' ':
        sim.paused = !sim.paused;
        break;

    case 'r':
    case 'R':
        initSim();
        glutPostRedisplay();
        break;

    case 'd':
    case 'D':
        // start a smooth theme transition to the next theme
        sim.themeBlend = 0.001f;   // tiny non-zero -> blend begins
        break;

    case 'm':
    case 'M':
        sim.mirrorMode = !sim.mirrorMode;
        glutPostRedisplay();
        break;

    case 'z':
    case 'Z':
        zoomToBridge();
        glutPostRedisplay();
        break;

    case 'x':
    case 'X':
        resetZoom();
        glutPostRedisplay();
        break;

    case '+':
    case '=':
        sim.speed *= 1.25f;
        if (sim.speed > SPEED_MAX) sim.speed = SPEED_MAX;
        break;

    case '-':
    case '_':
        sim.speed /= 1.25f;
        if (sim.speed < SPEED_MIN) sim.speed = SPEED_MIN;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------
// Special keys (arrows etc.) -- reserved for future use
// ---------------------------------------------------------------
static void specialKeys(int key, int x, int y)
{
    (void)x; (void)y;
    if (key == GLUT_KEY_F1)
    {
        sim.showDemoMode = !sim.showDemoMode;
        glutPostRedisplay();
    }
}

// ---------------------------------------------------------------
void initInput(void)
{
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
}

// ============================================================================
// Section: Main Application, GLUT Setup, and Main Loops
// Smart Medieval Castle Drawbridge Simulation
//
// An OpenGL + GLUT 2D scene that demonstrates:
//   - 2D graphics with GL primitives (GL_QUADS, GL_TRIANGLES, GL_POLYGON)
//   - 2D transformations: translation (ship, clouds, birds), rotation
//     (bridge, flags, bird wings), scaling (zoom)
//   - continuous animation via a finite state machine + timer callback
//   - color manipulation (day / sunset / night themes)
//   - window -> viewport transformation (Z zoom, X reset)
//   - user interaction (Space / R / D / Z / X / +/-)
//
// The animation logic lives in the Animation section (NOT in display()).
// This section only wires up GLUT and draws the current state.
// ============================================================================

// ---------------------------------------------------------------
// Projection / viewport setup
// ---------------------------------------------------------------
// World window: the region of the world we currently view.
//  - zoomFactor == 1 : the full scene (WORLD_LEFT..WORLD_RIGHT etc.)
//  - zoomFactor > 1  : a smaller sub-region centered on (zoomX, zoomY)
//                       mapped onto the same pixel viewport -> magnification.
//
// This is a VIEWPORT TRANSFORMATION demo: same window, same scene, we just
// change which part of the world lands on the GLUT window.
static void setupProjection(void)
{
    float worldW = WORLD_RIGHT - WORLD_LEFT;
    float worldH = WORLD_TOP - WORLD_BOTTOM;
    float aspect = (float)currentWinW / (float)currentWinH;

    float viewW, viewH;
    if (aspect > worldW / worldH)
    {
        viewW = worldH * aspect;
        viewH = worldH;
    }
    else
    {
        viewW = worldW;
        viewH = worldW / aspect;
    }

    float halfW = viewW * 0.5f / sim.zoomFactor;
    float halfH = viewH * 0.5f / sim.zoomFactor;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(sim.zoomX - halfW, sim.zoomX + halfW,
               sim.zoomY - halfH, sim.zoomY + halfH);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ---------------------------------------------------------------
// Helper to render text on the HUD/Overlay
// ---------------------------------------------------------------
static void renderOverlayString(float x, float y, const char* text, float r, float g, float b)
{
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (int i = 0; text[i]; i++)
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, text[i]);
}

// ---------------------------------------------------------------
// Helper to draw the F1 Computer Graphics Concepts Demonstration Overlay
// ---------------------------------------------------------------
static void drawDemoOverlay(void)
{
    // Switch to HUD ortho coordinate system (pixel coordinates)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, currentWinW, 0, currentWinH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Semi-transparent black background
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.78f);
    drawRect(50.0f, 50.0f, (float)currentWinW - 50.0f, (float)currentWinH - 50.0f);
    glDisable(GL_BLEND);

    // Border outline (Gold)
    glColor3f(0.85f, 0.70f, 0.20f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(50.0f, 50.0f);
    glVertex2f((float)currentWinW - 50.0f, 50.0f);
    glVertex2f((float)currentWinW - 50.0f, (float)currentWinH - 50.0f);
    glVertex2f(50.0f, (float)currentWinH - 50.0f);
    glEnd();
    glLineWidth(1.0f);

    // Title and Concepts List
    float startY = (float)currentWinH - 100.0f;
    float stepY = 28.0f;
    int lineIndex = 0;

    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "=== COMPUTER GRAPHICS CONCEPTS DEMONSTRATED ===", 0.9f, 0.7f, 0.1f);
    lineIndex++; // spacer

    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] 2D Primitive Assembly (Quads, Triangles, Polygons, Lines)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Affine Transformations: 2D Translation (Ship, Clouds, Birds)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Affine Transformations: 2D Rotation (Drawbridge, Bobbing Hull, Flags)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Affine Transformations: 2D Perspective Scaling (Ship depth shrink)", 1.0f, 1.0f, 1.0f);
    
    char mirrorBuf[80];
    snprintf(mirrorBuf, sizeof(mirrorBuf), "[%s] 2D Reflection - Horizontal Mirror Mode (Press M to Toggle)",
             sim.mirrorMode ? "X" : " ");
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, mirrorBuf, 
                        sim.mirrorMode ? 0.2f : 1.0f, sim.mirrorMode ? 1.0f : 1.0f, sim.mirrorMode ? 0.2f : 1.0f);

    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Midpoint Circle Algorithm (Sun, Moon, Castle Shield Emblem)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Bresenham's Line Algorithm (Chains, Tower Flagpoles, Plank Joints)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Finite State Machine (FSM) Animation (8-state sequence loop)", 1.0f, 1.0f, 1.0f);
    
    char zoomBuf[80];
    snprintf(zoomBuf, sizeof(zoomBuf), "[%s] Window-to-Viewport Zoom (Press Z/X to Toggle)",
             sim.zoomFactor > 1.0f ? "X" : " ");
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, zoomBuf, 
                        sim.zoomFactor > 1.0f ? 0.2f : 1.0f, sim.zoomFactor > 1.0f ? 1.0f : 1.0f, sim.zoomFactor > 1.0f ? 0.2f : 1.0f);

    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Color Interpolation Themes (Press D to cycle Day / Sunset / Night)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Dynamic Depth Sorting & Vanishing Occlusion (Painter's Algorithm)", 1.0f, 1.0f, 1.0f);
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "[X] Procedural Wave & Waving Cloth Physics (Chains & Flags)", 1.0f, 1.0f, 1.0f);
    
    lineIndex++; // spacer
    renderOverlayString(80.0f, startY - (lineIndex++) * stepY, "Press F1 again to exit concepts overlay.", 0.6f, 0.6f, 0.6f);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ---------------------------------------------------------------
// Display: draw the whole scene for the current state
// ---------------------------------------------------------------
void display(void)
{
    Theme theme;

    // apply the current zoom / viewport transformation BEFORE drawing
    setupProjection();

    // figure out the active (possibly blended) color theme
    currentTheme(&theme);

    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();

    // ---- environment (sky, stars, sun/moon, clouds, ground, trees, birds) ----
    drawScene(&theme);

    // ---- river ----
    drawRiver(&theme);

    // Dynamic depth sorting based on ship Y position
    if (sim.shipY < -20.0f)
    {
        // Ship is in the foreground (closer than the bridge and castle)
        drawCastleBackground(&theme);
        drawCastleForeground(&theme);
        drawBridge(&theme);
        drawShip(&theme);
    }
    else
    {
        // Ship has passed the bridge and is heading into the castle
        drawCastleBackground(&theme);
        drawShip(&theme);
        drawCastleForeground(&theme);
        drawBridge(&theme);
    }

    // Draw all trees on top of the castle walls (foreground depth layering)
    drawTrees();

    glPopMatrix();

    // ---- HUD (bridge state, speed, theme, zoom) ----
    drawHUD();

    // ---- F1 Concepts Demo Overlay ----
    if (sim.showDemoMode)
    {
        drawDemoOverlay();
    }

    glutSwapBuffers();
}

// ---------------------------------------------------------------
// Timer callback: drives the animation loop (~60 updates per second)
// ---------------------------------------------------------------
void timer(int value)
{
    (void)value;
    animationUpdate(TIME_STEP / 1000.0f);   // dt in seconds
    glutTimerFunc((int)TIME_STEP, timer, 0);
}

// ---------------------------------------------------------------
// Reshape: keep the world window fixed; the pixel viewport = whole window
// ---------------------------------------------------------------
void reshape(int w, int h)
{
    currentWinW = w;
    currentWinH = h;
    glViewport(0, 0, w, h);
}

// ---------------------------------------------------------------
// GL setup
// ---------------------------------------------------------------
static void initGL(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // black letterbox margins
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
}

// ---------------------------------------------------------------
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(80, 60);
    glutCreateWindow("Medieval Castle Drawbridge Simulation");

    initGL();
    initSim();
    initInput();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc((int)TIME_STEP, timer, 0);

    glutMainLoop();
    return 0;
}
