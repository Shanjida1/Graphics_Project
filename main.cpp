#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <string>
#include "camera.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ------------------------------------------------------------
// Bedroom3D - course-project style OpenGL scene
// Features required by the assignment reference:
// 1) Viewing transformation / camera
// 2) 3D transformations (translate, rotate, scale)
// 3) Point light + spotlight
// 4) Moving object (rotating ceiling fan)
// 5) Different material colors
// 6) Mouse cursor controls the viewing direction
// ------------------------------------------------------------

static HDC   g_hDC = nullptr;
static HGLRC g_hRC = nullptr;
static HWND  g_hWnd = nullptr;
static int   g_width = 1280;
static int   g_height = 720;
static bool  g_running = true;
static bool  g_keys[256] = {};
static bool  g_mouseCaptured = true;
static bool  g_pointLightOn = true;
static bool  g_spotLightOn = true;
static bool  g_fanOn = true;
static bool  g_pcOn = true;
static float g_fanAngle = 0.0f;
static float g_rugOffsetX = 0.0f;
static float g_rugTargetX = 0.0f;
static float g_doorAngle = 0.0f;
static float g_doorTargetAngle = 0.0f;
// Double-casement window: both leaves open inward from their outside hinges.
static float g_windowLeftAngle = 0.0f;
static float g_windowRightAngle = 0.0f;
static float g_windowTargetAngle = 0.0f;
// Realistic PC + CPU power sequence.
static float g_pcPowerLevel = 1.0f;      // monitor/display power
static float g_cpuPowerLevel = 1.0f;     // tower power
static float g_cpuFanAngle = 0.0f;
static float g_pcBootTimer = 3.0f;
static float g_pcShutdownTimer = 0.0f;
static Camera g_camera(Vec3(0.0f, 1.65f, 5.15f), -90.0f, -5.0f);

// First-person player state, matching the movement/interaction style of the classroom project.
static const float g_playerEyeHeight = 1.65f;
static const float g_playerRadius = 0.23f;
// Human-scale movement: normal walk ~2.15 units/s, run ~3.85 units/s.
static const float g_walkSpeed = 2.15f;
static const float g_runSpeed = 3.85f;
static const float g_moveAcceleration = 9.5f;
static const float g_moveDeceleration = 12.0f;
static Vec3 g_playerVelocity(0.0f, 0.0f, 0.0f);
static float g_fanAngularSpeed = 165.0f;

enum class InteractionTarget { None, Door, Window, PC, Rug, FanSwitch, LightSwitch, SpotSwitch };
static InteractionTarget g_nearbyTarget = InteractionTarget::None;
static InteractionTarget g_lastPromptTarget = InteractionTarget::None;

static const float PI = 3.14159265358979323846f;

static float approachValue(float current, float target, float delta)
{
    if (current < target) return std::min(current + delta, target);
    if (current > target) return std::max(current - delta, target);
    return current;
}

static float distanceXZ(const Vec3& a, const Vec3& b)
{
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx*dx + dz*dz);
}

static float distance3D(const Vec3& a, const Vec3& b)
{
    Vec3 d = a - b;
    return vlength(d);
}

static float lookAlignmentTo(const Vec3& target)
{
    Vec3 toTarget = target - g_camera.Position;
    if (vlength(toTarget) < 0.0001f) return 1.0f;
    return vdot(vnormalize(toTarget), g_camera.Front());
}


static bool rayIntersectsAABB(const Vec3& origin, const Vec3& direction,
                              const Vec3& bmin, const Vec3& bmax,
                              float maxDistance, float& outT)
{
    float tmin = 0.05f;
    float tmax = maxDistance;
    const float o[3] = { origin.x, origin.y, origin.z };
    const float d[3] = { direction.x, direction.y, direction.z };
    const float mn[3] = { bmin.x, bmin.y, bmin.z };
    const float mx[3] = { bmax.x, bmax.y, bmax.z };

    for(int axis=0; axis<3; ++axis) {
        if(std::fabs(d[axis]) < 0.00001f) {
            if(o[axis] < mn[axis] || o[axis] > mx[axis]) return false;
        } else {
            float t1 = (mn[axis] - o[axis]) / d[axis];
            float t2 = (mx[axis] - o[axis]) / d[axis];
            if(t1 > t2) std::swap(t1,t2);
            tmin = std::max(tmin,t1);
            tmax = std::min(tmax,t2);
            if(tmin > tmax) return false;
        }
    }
    outT = tmin;
    return true;
}

static void considerInteractionAABB(InteractionTarget target,
                                    const Vec3& bmin, const Vec3& bmax,
                                    float maxDistance,
                                    float& bestT, InteractionTarget& bestTarget)
{
    float t = 0.0f;
    if(rayIntersectsAABB(g_camera.Position, g_camera.Front(), bmin, bmax, maxDistance, t) && t < bestT) {
        bestT = t;
        bestTarget = target;
    }
}

static bool playerHitsBoxXZ(float x, float z, float minX, float maxX, float minZ, float maxZ)
{
    return x > (minX - g_playerRadius) && x < (maxX + g_playerRadius) &&
           z > (minZ - g_playerRadius) && z < (maxZ + g_playerRadius);
}

static bool collidesWithFurniture(float x, float z)
{
    // Collision volumes are slightly smaller than the visual models so walking
    // beside furniture feels natural instead of getting caught on invisible corners.
    if (playerHitsBoxXZ(x,z,-1.28f, 2.38f,-4.90f,-0.10f)) return true; // bed
    if (playerHitsBoxXZ(x,z,-4.76f,-3.72f,-2.84f,-1.24f)) return true; // wardrobe
    if (playerHitsBoxXZ(x,z, 2.14f, 3.28f,-4.85f,-3.72f)) return true; // nightstand
    if (playerHitsBoxXZ(x,z, 3.26f, 4.76f,-2.98f, 0.20f)) return true; // desk / CPU zone
    if (playerHitsBoxXZ(x,z, 2.38f, 3.38f,-1.84f,-0.76f)) return true; // chair
    return false;
}

static Vec3 resolvePlayerCollision(const Vec3& oldPos, const Vec3& desired)
{
    Vec3 result = oldPos;

    // Resolve one axis at a time so the player slides naturally along furniture.
    float nextX = std::max(-4.55f, std::min(4.55f, desired.x));
    if (!collidesWithFurniture(nextX, result.z)) result.x = nextX;

    float nextZ = std::max(-5.35f, std::min(5.45f, desired.z));
    if (!collidesWithFurniture(result.x, nextZ)) result.z = nextZ;

    result.y = g_playerEyeHeight;
    return result;
}

static InteractionTarget findNearbyInteraction()
{
    // Comfortable first-person interaction range:
    // close enough to feel like a real person is operating the object,
    // but far enough that the object does not fill the whole camera view.
    float bestT = 999.0f;
    InteractionTarget best = InteractionTarget::None;

    // Wall switches: stand roughly one arm-step away and aim at the exact rocker.
    // Each hit-box stays separate, so switches never trigger together.
    const float switchReach = 1.35f;
    considerInteractionAABB(InteractionTarget::FanSwitch,
        Vec3(-4.92f,1.23f,2.205f), Vec3(-4.61f,1.78f,2.505f),
        switchReach,bestT,best);
    considerInteractionAABB(InteractionTarget::LightSwitch,
        Vec3(-4.92f,1.23f,2.505f), Vec3(-4.61f,1.78f,2.805f),
        switchReach,bestT,best);
    considerInteractionAABB(InteractionTarget::SpotSwitch,
        Vec3(-4.92f,1.23f,2.805f), Vec3(-4.61f,1.78f,3.105f),
        switchReach,bestT,best);

    // Door: can be operated from a natural standing position in front of it.
    considerInteractionAABB(InteractionTarget::Door,
        Vec3(-5.00f,0.15f,3.05f), Vec3(-4.24f,3.28f,5.08f),
        2.00f,bestT,best);

    // Window: enlarged interaction volume around both window leaves/handles.
    considerInteractionAABB(InteractionTarget::Window,
        Vec3(-3.94f,1.02f,-6.04f), Vec3(-0.56f,3.42f,-5.30f),
        2.00f,bestT,best);

    // PC: aim toward monitor / tower area from beside the chair.
    considerInteractionAABB(InteractionTarget::PC,
        Vec3(3.22f,0.10f,-2.28f), Vec3(4.78f,2.28f,0.08f),
        2.00f,bestT,best);

    // Rug uses the same balanced interaction rule. Its hit-box follows the rug.
    float rugCenterX = 0.55f + g_rugOffsetX;
    considerInteractionAABB(InteractionTarget::Rug,
        Vec3(rugCenterX - 2.45f,0.00f,0.05f),
        Vec3(rugCenterX + 2.45f,0.30f,3.05f),
        2.00f,bestT,best);

    return best;
}

static void interactWith(InteractionTarget target)
{
    if (target == InteractionTarget::Door) {
        g_doorTargetAngle = (g_doorTargetAngle > 36.0f) ? 0.0f : 72.0f;
    }
    else if (target == InteractionTarget::Window) {
        g_windowTargetAngle = (g_windowTargetAngle > 32.0f) ? 0.0f : 65.0f;
    }
    else if (target == InteractionTarget::PC) {
        g_pcOn = !g_pcOn;
        if (g_pcOn) {
            g_pcBootTimer = 0.0f;
            g_pcShutdownTimer = 0.0f;
        } else {
            g_pcShutdownTimer = 0.0f;
        }
    }
    else if (target == InteractionTarget::Rug) {
        // A real person first approaches the rug, then moves it. Each E press
        // shifts it smoothly to the opposite side; no room-wide shortcut is used.
        if (g_rugTargetX > 0.20f) g_rugTargetX = -0.90f;
        else if (g_rugTargetX < -0.20f) g_rugTargetX = 0.90f;
        else g_rugTargetX = 0.90f;
    }
    else if (target == InteractionTarget::FanSwitch) {
        g_fanOn = !g_fanOn;
    }
    else if (target == InteractionTarget::LightSwitch) {
        g_pointLightOn = !g_pointLightOn;
    }
    else if (target == InteractionTarget::SpotSwitch) {
        g_spotLightOn = !g_spotLightOn;
    }
}

static void updateInteractionTitle()
{
    if (!g_hWnd) return;
    g_lastPromptTarget = g_nearbyTarget;

    const wchar_t* title = L"Bedroom 3D | WASD Walk | Shift Run | Mouse Look | Aim + E to Interact";
    if (g_nearbyTarget == InteractionTarget::Door)
        title = (g_doorTargetAngle > 36.0f) ? L"Near Door - Press E to CLOSE | Mouse Look | WASD Walk" : L"Near Door - Press E to OPEN | Mouse Look | WASD Walk";
    else if (g_nearbyTarget == InteractionTarget::Window)
        title = (g_windowTargetAngle > 32.0f) ? L"Near Window - Press E to CLOSE | Mouse Look | WASD Walk" : L"Near Window - Press E to OPEN | Mouse Look | WASD Walk";
    else if (g_nearbyTarget == InteractionTarget::PC)
        title = g_pcOn ? L"Near PC - Press E to SHUT DOWN PC + CPU | Mouse Look | WASD Walk" : L"Near PC - Press E to POWER ON PC + CPU | Mouse Look | WASD Walk";
    else if (g_nearbyTarget == InteractionTarget::Rug)
        title = (g_rugTargetX > 0.20f) ? L"Near Rug - Press E to MOVE RUG LEFT" : L"Near Rug - Press E to MOVE RUG RIGHT";
    else if (g_nearbyTarget == InteractionTarget::FanSwitch)
        title = g_fanOn ? L"Fan Switch - Press E to turn FAN OFF" : L"Fan Switch - Press E to turn FAN ON";
    else if (g_nearbyTarget == InteractionTarget::LightSwitch)
        title = g_pointLightOn ? L"Main Light Switch - Press E to turn LIGHT OFF" : L"Main Light Switch - Press E to turn LIGHT ON";
    else if (g_nearbyTarget == InteractionTarget::SpotSwitch)
        title = g_spotLightOn ? L"Spotlight Switch - Press E to turn SPOTLIGHT OFF" : L"Spotlight Switch - Press E to turn SPOTLIGHT ON";

    SetWindowTextW(g_hWnd, title);
}

static void setMaterial(float r, float g, float b, float shininess = 24.0f,
                        float sr = 0.22f, float sg = 0.22f, float sb = 0.22f,
                        float alpha = 1.0f, float emission = 0.0f)
{
    GLfloat amb[]  = { r * 0.42f, g * 0.42f, b * 0.42f, alpha };
    GLfloat diff[] = { r, g, b, alpha };
    GLfloat spec[] = { sr, sg, sb, alpha };
    GLfloat emis[] = { r * emission, g * emission, b * emission, alpha };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

static void drawUnitBox()
{
    glBegin(GL_QUADS);
    // front +Z
    glNormal3f(0,0,1);  glVertex3f(-.5f,-.5f,.5f); glVertex3f(.5f,-.5f,.5f); glVertex3f(.5f,.5f,.5f); glVertex3f(-.5f,.5f,.5f);
    // back -Z
    glNormal3f(0,0,-1); glVertex3f(.5f,-.5f,-.5f); glVertex3f(-.5f,-.5f,-.5f); glVertex3f(-.5f,.5f,-.5f); glVertex3f(.5f,.5f,-.5f);
    // left -X
    glNormal3f(-1,0,0); glVertex3f(-.5f,-.5f,-.5f); glVertex3f(-.5f,-.5f,.5f); glVertex3f(-.5f,.5f,.5f); glVertex3f(-.5f,.5f,-.5f);
    // right +X
    glNormal3f(1,0,0);  glVertex3f(.5f,-.5f,.5f); glVertex3f(.5f,-.5f,-.5f); glVertex3f(.5f,.5f,-.5f); glVertex3f(.5f,.5f,.5f);
    // top +Y
    glNormal3f(0,1,0);  glVertex3f(-.5f,.5f,.5f); glVertex3f(.5f,.5f,.5f); glVertex3f(.5f,.5f,-.5f); glVertex3f(-.5f,.5f,-.5f);
    // bottom -Y
    glNormal3f(0,-1,0); glVertex3f(-.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,.5f); glVertex3f(-.5f,-.5f,.5f);
    glEnd();
}

static void box(float x, float y, float z, float sx, float sy, float sz,
                float r, float g, float b, float shininess = 24.0f,
                float sr = 0.20f, float sg = 0.20f, float sb = 0.20f)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glScalef(sx,sy,sz);
    setMaterial(r,g,b,shininess,sr,sg,sb);
    drawUnitBox();
    glPopMatrix();
}

static void rotatedBox(float x, float y, float z, float sx, float sy, float sz,
                       float angleY, float r, float g, float b, float shininess = 24.0f)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(angleY,0,1,0);
    glScalef(sx,sy,sz);
    setMaterial(r,g,b,shininess);
    drawUnitBox();
    glPopMatrix();
}

static void sphere(float x, float y, float z, float sx, float sy, float sz,
                   float r, float g, float b, int stacks=14, int slices=20,
                   float emission=0.0f)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    glScalef(sx,sy,sz);
    setMaterial(r,g,b,42.0f,0.35f,0.35f,0.35f,1.0f,emission);
    for (int i=0;i<stacks;i++) {
        float p0 = PI * (-0.5f + (float)i/stacks);
        float p1 = PI * (-0.5f + (float)(i+1)/stacks);
        glBegin(GL_QUAD_STRIP);
        for (int j=0;j<=slices;j++) {
            float t = 2.0f*PI*(float)j/slices;
            float c0=std::cos(p0), c1=std::cos(p1);
            float x0=c0*std::cos(t), y0=std::sin(p0), z0=c0*std::sin(t);
            float x1=c1*std::cos(t), y1=std::sin(p1), z1=c1*std::sin(t);
            glNormal3f(x0,y0,z0); glVertex3f(x0,y0,z0);
            glNormal3f(x1,y1,z1); glVertex3f(x1,y1,z1);
        }
        glEnd();
    }
    glPopMatrix();
}

static void cylinderY(float x, float y, float z, float radius, float height,
                      float r, float g, float b, int segments=24)
{
    glPushMatrix();
    glTranslatef(x,y,z);
    setMaterial(r,g,b,38.0f,0.28f,0.28f,0.28f);
    float h=height*0.5f;
    glBegin(GL_QUAD_STRIP);
    for(int i=0;i<=segments;i++){
        float a=2*PI*i/segments, c=std::cos(a), s=std::sin(a);
        glNormal3f(c,0,s); glVertex3f(radius*c,-h,radius*s); glVertex3f(radius*c,h,radius*s);
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN); glNormal3f(0,1,0); glVertex3f(0,h,0);
    for(int i=0;i<=segments;i++){ float a=2*PI*i/segments; glVertex3f(radius*std::cos(a),h,radius*std::sin(a)); } glEnd();
    glBegin(GL_TRIANGLE_FAN); glNormal3f(0,-1,0); glVertex3f(0,-h,0);
    for(int i=segments;i>=0;i--){ float a=2*PI*i/segments; glVertex3f(radius*std::cos(a),-h,radius*std::sin(a)); } glEnd();
    glPopMatrix();
}

static void coneY(float x,float y,float z,float r1,float r2,float height,float cr,float cg,float cb,int segments=24)
{
    glPushMatrix(); glTranslatef(x,y,z); setMaterial(cr,cg,cb,20.0f,0.12f,0.12f,0.12f);
    float h=height*0.5f;
    glBegin(GL_QUAD_STRIP);
    for(int i=0;i<=segments;i++){
        float a=2*PI*i/segments, c=std::cos(a), s=std::sin(a);
        float slope=(r1-r2)/height;
        Vec3 n=vnormalize(Vec3(c,slope,s));
        glNormal3f(n.x,n.y,n.z); glVertex3f(r1*c,-h,r1*s); glVertex3f(r2*c,h,r2*s);
    }
    glEnd();
    glPopMatrix();
}

static void drawShadowQuad(float x,float z,float sx,float sz,float alpha)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.03f,0.025f,0.03f,alpha);
    glBegin(GL_QUADS);
    glVertex3f(x-sx*.5f,0.018f,z-sz*.5f); glVertex3f(x+sx*.5f,0.018f,z-sz*.5f);
    glVertex3f(x+sx*.5f,0.018f,z+sz*.5f); glVertex3f(x-sx*.5f,0.018f,z+sz*.5f);
    glEnd();
    glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

static void drawFloorAndRoom()
{
    // Tile floor: many slightly varied warm stone tiles.
    for(int ix=0;ix<10;ix++) for(int iz=0;iz<12;iz++) {
        float x=-4.5f+ix, z=-5.5f+iz;
        float tone=((ix+iz)&1)?0.00f:0.025f;
        box(x,0.0f,z,0.96f,0.035f,0.96f,0.66f+tone,0.62f+tone,0.55f+tone,18.0f,0.12f,0.12f,0.12f);
    }

    // Walls and ceiling
    box(0,2.05f,-6.03f,10.1f,4.15f,0.10f,0.70f,0.82f,0.94f,8.0f,0.05f,0.05f,0.05f);
    box(-5.03f,2.05f,0,0.10f,4.15f,12.0f,0.62f,0.76f,0.90f,8.0f,0.04f,0.04f,0.04f);
    box(5.03f,2.05f,0,0.10f,4.15f,12.0f,0.62f,0.76f,0.90f,8.0f,0.04f,0.04f,0.04f);
    box(0,4.10f,0,10.1f,0.10f,12.0f,0.90f,0.94f,0.98f,5.0f,0.02f,0.02f,0.02f);
    box(0,2.05f,6.03f,10.1f,4.15f,0.10f,0.58f,0.72f,0.86f,6.0f,0.03f,0.03f,0.03f);

    // Base boards
    box(0,0.13f,-5.92f,10.0f,0.22f,0.12f,0.30f,0.20f,0.14f);
    box(-4.92f,0.13f,0,0.12f,0.22f,11.8f,0.30f,0.20f,0.14f);
    box(4.92f,0.13f,0,0.12f,0.22f,11.8f,0.30f,0.20f,0.14f);
}

static void drawWindow()
{
    // REAL DOUBLE CASEMENT WINDOW
    // K opens/closes BOTH leaves. Each leaf is hinged on the outside edge,
    // so the two panels swing inward naturally like a real bedroom window.
    const float outsideZ = -5.968f;
    const float glassZ   = -5.775f;
    const float frameZ   = -5.690f;

    // Recess / opening shadow.
    box(-2.25f,2.25f,-5.955f,3.42f,2.18f,.045f,.050f,.055f,.060f,8.0f,.02f,.02f,.02f);

    // Outdoor scene stays behind the wall.
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glColor3f(.57f,.80f,.97f); glVertex3f(-3.78f,3.16f,outsideZ);
    glColor3f(.66f,.86f,.99f); glVertex3f(-.72f,3.16f,outsideZ);
    glColor3f(.84f,.94f,1.00f); glVertex3f(-.72f,2.14f,outsideZ);
    glColor3f(.79f,.91f,.99f); glVertex3f(-3.78f,2.14f,outsideZ);
    glColor3f(.49f,.72f,.39f); glVertex3f(-3.78f,2.15f,outsideZ+.0002f);
    glColor3f(.54f,.77f,.42f); glVertex3f(-.72f,2.15f,outsideZ+.0002f);
    glColor3f(.37f,.59f,.29f); glVertex3f(-.72f,1.34f,outsideZ+.0002f);
    glColor3f(.40f,.62f,.31f); glVertex3f(-3.78f,1.34f,outsideZ+.0002f);
    glEnd();

    glBegin(GL_TRIANGLES);
    glColor3f(.35f,.57f,.33f);
    glVertex3f(-3.75f,1.80f,outsideZ+.0004f); glVertex3f(-2.92f,2.48f,outsideZ+.0004f); glVertex3f(-2.05f,1.80f,outsideZ+.0004f);
    glColor3f(.43f,.66f,.37f);
    glVertex3f(-2.72f,1.80f,outsideZ+.0006f); glVertex3f(-1.72f,2.34f,outsideZ+.0006f); glVertex3f(-.78f,1.80f,outsideZ+.0006f);
    glEnd();

    for (int c=0;c<3;c++) {
        float cx[3]={-3.12f,-2.18f,-1.18f};
        float cy[3]={2.03f,2.12f,1.96f};
        float rx[3]={.30f,.37f,.27f};
        float ry[3]={.28f,.34f,.25f};
        glBegin(GL_QUADS);
        glColor3f(.36f,.23f,.13f);
        glVertex3f(cx[c]-.025f,1.53f,outsideZ+.0010f); glVertex3f(cx[c]+.025f,1.53f,outsideZ+.0010f);
        glVertex3f(cx[c]+.025f,1.91f,outsideZ+.0010f); glVertex3f(cx[c]-.025f,1.91f,outsideZ+.0010f);
        glEnd();
        glColor3f(.18f+.035f*c,.48f+.045f*c,.20f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(cx[c],cy[c],outsideZ+.0012f);
        for(int i=0;i<=28;i++) {
            float a=2.0f*PI*(float)i/28.0f;
            glVertex3f(cx[c]+rx[c]*std::cos(a),cy[c]+ry[c]*std::sin(a),outsideZ+.0012f);
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);

    // Main frame and sill.
    box(-2.25f,3.19f,frameZ,3.34f,.16f,.18f,.25f,.16f,.095f,34.0f,.18f,.12f,.08f);
    box(-2.25f,1.31f,frameZ,3.34f,.16f,.18f,.25f,.16f,.095f,34.0f,.18f,.12f,.08f);
    box(-3.84f,2.25f,frameZ,.16f,2.04f,.18f,.25f,.16f,.095f,34.0f,.18f,.12f,.08f);
    box(-.66f,2.25f,frameZ,.16f,2.04f,.18f,.25f,.16f,.095f,34.0f,.18f,.12f,.08f);
    box(-2.25f,1.20f,-5.48f,3.55f,.12f,.52f,.63f,.59f,.54f,24.0f,.14f,.12f,.10f);
    box(-2.25f,1.14f,-5.57f,3.38f,.08f,.34f,.46f,.42f,.39f,18.0f,.08f,.08f,.08f);

    // Center weather seal visible when closed.
    box(-2.25f,2.25f,frameZ-.010f,.045f,1.76f,.10f,.13f,.13f,.13f,18.0f,.08f,.08f,.08f);

    // LEFT LEAF: hinge at x=-3.68, swings inward with negative Y rotation.
    glPushMatrix();
    glTranslatef(-3.68f,0.0f,frameZ+.060f);
    glRotatef(-g_windowLeftAngle,0,1,0);
    box(.70f,3.08f,0.0f,1.48f,.075f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(.70f,1.42f,0.0f,1.48f,.075f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(0.00f,2.25f,0.0f,.075f,1.72f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(1.40f,2.25f,0.0f,.075f,1.72f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(.70f,2.25f,.025f,1.31f,1.56f,.020f,.70f,.84f,.93f,100.0f,.88f,.90f,.94f);
    // left handle at meeting edge
    box(1.27f,2.16f,.095f,.055f,.24f,.060f,.68f,.67f,.64f,76.0f,.76f,.76f,.76f);
    box(1.20f,2.16f,.115f,.11f,.055f,.075f,.64f,.63f,.60f,76.0f,.76f,.76f,.76f);
    // reflection
    glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,1,1,.14f); glBegin(GL_QUADS);
    glVertex3f(.25f,2.94f,.041f); glVertex3f(.41f,2.94f,.041f);
    glVertex3f(1.12f,1.70f,.041f); glVertex3f(.96f,1.70f,.041f);
    glEnd(); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    glPopMatrix();

    // RIGHT LEAF: hinge at x=-0.82, swings inward with positive Y rotation.
    glPushMatrix();
    glTranslatef(-.82f,0.0f,frameZ+.060f);
    glRotatef(g_windowRightAngle,0,1,0);
    box(-.70f,3.08f,0.0f,1.48f,.075f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(-.70f,1.42f,0.0f,1.48f,.075f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(-1.40f,2.25f,0.0f,.075f,1.72f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(0.00f,2.25f,0.0f,.075f,1.72f,.14f,.22f,.21f,.20f,30.0f,.14f,.14f,.14f);
    box(-.70f,2.25f,.025f,1.31f,1.56f,.020f,.70f,.84f,.93f,100.0f,.88f,.90f,.94f);
    // right handle at meeting edge
    box(-1.27f,2.16f,.095f,.055f,.24f,.060f,.68f,.67f,.64f,76.0f,.76f,.76f,.76f);
    box(-1.20f,2.16f,.115f,.11f,.055f,.075f,.64f,.63f,.60f,76.0f,.76f,.76f,.76f);
    glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,1,1,.14f); glBegin(GL_QUADS);
    glVertex3f(-1.15f,2.94f,.041f); glVertex3f(-.99f,2.94f,.041f);
    glVertex3f(-.28f,1.70f,.041f); glVertex3f(-.44f,1.70f,.041f);
    glEnd(); glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    glPopMatrix();

    // Three hinges on each outside edge.
    for(int i=0;i<3;i++) {
        float hy=1.64f+i*.61f;
        cylinderY(-3.72f,hy,frameZ+.105f,.028f,.18f,.52f,.51f,.48f,14);
        cylinderY(-.78f,hy,frameZ+.105f,.028f,.18f,.52f,.51f,.48f,14);
    }

    // Curtains remain independent of the opening leaves.
    box(-2.25f,3.44f,-5.53f,3.72f,.075f,.075f,.30f,.19f,.10f,28.0f,.18f,.12f,.08f);
    sphere(-4.12f,3.44f,-5.53f,.10f,.10f,.10f,.34f,.22f,.12f,10,14);
    sphere(-.38f,3.44f,-5.53f,.10f,.10f,.10f,.34f,.22f,.12f,10,14);
    for(int i=0;i<4;i++) {
        cylinderY(-3.78f+.12f*i,3.38f,-5.50f,.032f,.10f,.72f,.70f,.66f,12);
        cylinderY(-1.08f+.12f*i,3.38f,-5.50f,.032f,.10f,.72f,.70f,.66f,12);
    }
    box(-3.55f,2.30f,-5.43f,.62f,2.08f,.18f,.035f,.12f,.32f,20.0f,.08f,.10f,.17f);
    box(-.95f,2.30f,-5.43f,.62f,2.08f,.18f,.035f,.12f,.32f,20.0f,.08f,.10f,.17f);
    for(int i=0;i<5;i++) {
        float ox=.095f*i;
        box(-3.76f+ox,2.30f,-5.31f,.048f,2.00f,.075f,.055f,.18f,.44f,14.0f,.06f,.08f,.13f);
        box(-1.16f+ox,2.30f,-5.31f,.048f,2.00f,.075f,.055f,.18f,.44f,14.0f,.06f,.08f,.13f);
    }
}

static void drawBed()
{
    drawShadowQuad(0.55f,-2.45f,3.7f,5.0f,.20f);
    // wooden frame and legs
    box(.55f,.36f,-2.45f,3.55f,.40f,4.72f,.31f,.12f,.055f,28.0f,.18f,.12f,.08f);
    box(.55f,.64f,-4.85f,3.68f,1.55f,.16f,.35f,.14f,.065f,34.0f,.22f,.14f,.09f);
    box(.55f,.52f,-.08f,3.65f,.72f,.15f,.29f,.105f,.045f,28.0f);
    for(float x: {-1.05f,2.15f}) for(float z: {-4.55f,-.35f}) box(x,.19f,z,.18f,.38f,.18f,.20f,.07f,.03f,22.0f);

    // mattress + sheet
    box(.55f,.71f,-2.40f,3.36f,.30f,4.42f,.86f,.86f,.82f,10.0f,.05f,.05f,.05f);
    box(.55f,.89f,-1.74f,3.30f,.12f,3.02f,.055f,.19f,.48f,20.0f,.08f,.09f,.16f);
    // folded blanket at foot
    box(.55f,.96f,-.42f,3.32f,.11f,.85f,.08f,.23f,.58f,22.0f,.10f,.12f,.22f);
    // pillows (ellipsoid spheres)
    sphere(-.28f,1.03f,-4.12f,.72f,.23f,.46f,.86f,.84f,.78f,12,20);
    sphere(1.40f,1.03f,-4.12f,.72f,.23f,.46f,.86f,.84f,.78f,12,20);
    // blue accent pillow
    sphere(.55f,1.08f,-3.88f,.62f,.20f,.30f,.10f,.22f,.48f,12,18);
    // headboard inset
    box(.55f,.90f,-4.755f,3.15f,.55f,.045f,.46f,.19f,.08f,30.0f,.20f,.13f,.08f);
}

static void drawWardrobe()
{
    // Wardrobe kept beside the bed along the left wall, with its doors opening toward the bedside.
    drawShadowQuad(-4.25f,-2.05f,1.15f,1.70f,.20f);

    // main body placed close to the left wall
    box(-4.28f,1.63f,-2.05f,0.96f,3.08f,1.58f,.34f,.16f,.075f,30.0f,.22f,.14f,.08f);

    // front doors on the +X side so they face the bed / bedside area
    box(-3.82f,1.66f,-2.05f,.05f,2.90f,1.48f,.41f,.20f,.10f,30.0f);
    box(-3.795f,1.66f,-2.05f,.03f,2.84f,.045f,.15f,.065f,.03f,12.0f); // middle split between two doors

    // handles positioned on each wardrobe door
    box(-3.76f,1.66f,-2.32f,.06f,.48f,.04f,.76f,.77f,.74f,60.0f,.75f,.75f,.74f);
    box(-3.76f,1.66f,-1.78f,.06f,.48f,.04f,.76f,.77f,.74f,60.0f,.75f,.75f,.74f);

    // top and base details
    box(-4.28f,3.17f,-2.05f,1.00f,.10f,1.62f,.28f,.12f,.05f);
    box(-4.28f,.10f,-2.05f,1.02f,.12f,1.62f,.21f,.085f,.040f);
}

static void drawNightstandAndLamp()
{
    drawShadowQuad(2.72f,-4.28f,1.15f,1.1f,.16f);
    box(2.72f,.54f,-4.30f,1.08f,.86f,1.02f,.34f,.13f,.055f,28.0f);
    box(2.72f,.72f,-3.77f,.93f,.25f,.055f,.42f,.18f,.075f,30.0f);
    box(2.72f,.72f,-3.73f,.16f,.035f,.07f,.72f,.70f,.66f,60.0f,.8f,.8f,.8f);
    cylinderY(2.72f,1.24f,-4.30f,.045f,.55f,.69f,.55f,.35f);
    sphere(2.72f,1.01f,-4.30f,.28f,.08f,.28f,.42f,.25f,.12f,10,18);
    coneY(2.72f,1.65f,-4.30f,.42f,.26f,.52f,.88f,.75f,.46f,28);
    sphere(2.72f,1.39f,-4.30f,.09f,.09f,.09f,1.0f,.72f,.28f,10,16,0.65f);
}

static void drawDeskChairLaptop()
{
    drawShadowQuad(3.92f,-1.30f,1.75f,3.65f,.18f);
    box(4.02f,1.04f,-1.35f,1.58f,.14f,3.45f,.40f,.17f,.075f,35.0f,.24f,.16f,.09f);
    for(float z: {-2.70f,-0.05f}) {
        box(4.48f,.51f,z,.13f,1.00f,.13f,.31f,.12f,.055f);
        box(3.58f,.51f,z,.13f,1.00f,.13f,.31f,.12f,.055f);
    }
    box(4.34f,.68f,-2.30f,.82f,.62f,1.08f,.35f,.14f,.06f,25.0f);
    box(3.90f,.72f,-1.78f,.06f,.24f,.90f,.44f,.20f,.09f,30.0f);
    box(3.86f,.72f,-1.78f,.05f,.035f,.18f,.78f,.77f,.73f,60.0f,.8f,.8f,.8f);

    float monitorPwr = std::max(0.0f,std::min(1.0f,g_pcPowerLevel));
    float cpuPwr = std::max(0.0f,std::min(1.0f,g_cpuPowerLevel));

    // Monitor body / stand.
    box(4.14f,1.58f,-1.18f,.10f,.82f,1.08f,.065f,.070f,.080f,85.0f,.65f,.65f,.70f);
    box(4.22f,1.16f,-1.18f,.10f,.07f,.28f,.22f,.22f,.23f,30.0f);
    box(4.28f,1.08f,-1.18f,.22f,.03f,.52f,.18f,.18f,.19f,18.0f);

    // Screen gradually powers up/down.
    glPushMatrix();
    glTranslatef(4.08f,1.58f,-1.18f);
    glScalef(.022f,.66f,.90f);
    setMaterial(.010f + .030f*monitorPwr,
                .012f + .085f*monitorPwr,
                .016f + .205f*monitorPwr,
                95.0f,.50f,.58f,.72f,1.0f,.40f*monitorPwr);
    drawUnitBox();
    glPopMatrix();

    // Monitor power LED follows the actual display power.
    sphere(4.065f,1.235f,-.755f,.012f,.012f,.012f,
           .03f+.10f*monitorPwr,.05f+.62f*monitorPwr,.04f+.18f*monitorPwr,
           8,10,.80f*monitorPwr);

    if(monitorPwr > .035f) {
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        float alpha=std::min(1.0f,monitorPwr*1.35f);

        if(g_pcOn && g_pcBootTimer < 2.0f) {
            // Realistic boot stage.
            glColor4f(.010f,.018f,.036f,alpha);
            glBegin(GL_QUADS);
            glVertex3f(4.066f,1.31f,-1.58f); glVertex3f(4.066f,1.85f,-1.58f);
            glVertex3f(4.066f,1.85f,-.78f);  glVertex3f(4.066f,1.31f,-.78f);
            glEnd();

            glColor4f(.65f,.84f,1.0f,alpha);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(4.062f,1.61f,-1.18f);
            for(int i=0;i<=24;i++) {
                float a=2.0f*PI*(float)i/24.0f;
                glVertex3f(4.062f,1.61f+.060f*std::cos(a),-1.18f+.080f*std::sin(a));
            }
            glEnd();

            // loading dots
            for(int i=0;i<4;i++) {
                float phase=std::fmod(g_pcBootTimer*1.8f + i*.22f,1.0f);
                float z=-1.31f + phase*.26f;
                glColor4f(.90f,.95f,1.0f,alpha*(.35f+.65f*phase));
                glBegin(GL_QUADS);
                glVertex3f(4.061f,1.46f,z-.010f); glVertex3f(4.061f,1.48f,z-.010f);
                glVertex3f(4.061f,1.48f,z+.010f); glVertex3f(4.061f,1.46f,z+.010f);
                glEnd();
            }
        }
        else if(g_pcOn) {
            // Desktop after boot.
            glColor4f(.05f,.18f,.38f,alpha);
            glBegin(GL_QUADS);
            glVertex3f(4.066f,1.31f,-1.58f); glVertex3f(4.066f,1.85f,-1.58f);
            glColor4f(.13f,.45f,.73f,alpha);
            glVertex3f(4.066f,1.85f,-.78f); glVertex3f(4.066f,1.31f,-.78f);
            glEnd();
            glColor4f(.025f,.038f,.060f,.94f*alpha);
            glBegin(GL_QUADS);
            glVertex3f(4.064f,1.31f,-1.58f); glVertex3f(4.064f,1.37f,-1.58f);
            glVertex3f(4.064f,1.37f,-.78f); glVertex3f(4.064f,1.31f,-.78f);
            glEnd();
            for(int i=0;i<3;i++) {
                float z=-1.48f+i*.17f;
                glColor4f(.82f,.90f,.98f,alpha);
                glBegin(GL_QUADS);
                glVertex3f(4.063f,1.70f,z); glVertex3f(4.063f,1.79f,z);
                glVertex3f(4.063f,1.79f,z+.10f); glVertex3f(4.063f,1.70f,z+.10f);
                glEnd();
            }
            glColor4f(.90f,.93f,.96f,.90f*alpha);
            glBegin(GL_QUADS);
            glVertex3f(4.062f,1.45f,-1.28f); glVertex3f(4.062f,1.72f,-1.28f);
            glVertex3f(4.062f,1.72f,-.88f); glVertex3f(4.062f,1.45f,-.88f);
            glEnd();
            glColor4f(.12f,.35f,.58f,.95f*alpha);
            glBegin(GL_QUADS);
            glVertex3f(4.061f,1.67f,-1.28f); glVertex3f(4.061f,1.72f,-1.28f);
            glVertex3f(4.061f,1.72f,-.88f); glVertex3f(4.061f,1.67f,-.88f);
            glEnd();
        }
        else {
            // Shutdown stage: display dims before the CPU turns off.
            glColor4f(.012f,.020f,.040f,alpha);
            glBegin(GL_QUADS);
            glVertex3f(4.066f,1.31f,-1.58f); glVertex3f(4.066f,1.85f,-1.58f);
            glVertex3f(4.066f,1.85f,-.78f); glVertex3f(4.066f,1.31f,-.78f);
            glEnd();
            glColor4f(.45f,.63f,.82f,alpha*.75f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(4.062f,1.59f,-1.18f);
            for(int i=0;i<=20;i++) {
                float a=2.0f*PI*(float)i/20.0f;
                glVertex3f(4.062f,1.59f+.035f*std::cos(a),-1.18f+.050f*std::sin(a));
            }
            glEnd();
        }
        glDisable(GL_BLEND); glEnable(GL_LIGHTING);
    }

    // Keyboard / mouse.
    box(3.72f,1.12f,-1.18f,.44f,.025f,.88f,.15f,.15f,.16f,20.0f);
    for(int row=0;row<4;row++) for(int col=0;col<9;col++)
        box(3.705f,1.139f,-1.50f+col*.075f,.015f,.008f,.050f,.29f,.29f,.30f,10.0f);
    sphere(3.58f,1.145f,-.60f,.060f,.028f,.090f,.16f,.16f,.17f,8,12);

    // CPU tower. CPU and monitor are controlled together by P.
    box(4.22f,.54f,-.36f,.46f,.98f,.30f,.12f,.13f,.15f,30.0f,.28f,.28f,.30f);
    box(4.005f,.55f,-.36f,.018f,.86f,.24f,.075f,.080f,.090f,28.0f,.18f,.18f,.20f);

    // Real visible case fan: spins while CPU is powered and slows during shutdown.
    sphere(3.989f,.55f,-.36f,.012f,.115f,.115f,.028f,.032f,.038f,12,18);
    glPushMatrix();
    glTranslatef(3.974f,.55f,-.36f);
    glRotatef(g_cpuFanAngle,1,0,0);
    for(int i=0;i<4;i++) {
        glPushMatrix();
        glRotatef(i*90.0f,1,0,0);
        glTranslatef(0.0f,.065f,0.0f);
        glScalef(.012f,.11f,.035f);
        setMaterial(.10f+.10f*cpuPwr,.20f+.18f*cpuPwr,.28f+.32f*cpuPwr,34.0f,.20f,.25f,.30f,1.0f,.08f*cpuPwr);
        drawUnitBox();
        glPopMatrix();
    }
    glPopMatrix();
    sphere(3.970f,.55f,-.36f,.018f,.030f,.030f,.20f,.23f,.25f,8,10);

    // CPU power button / LED. It stays on through the shutdown delay, then fades out.
    sphere(3.984f,.92f,-.36f,.025f,.025f,.025f,
           .05f+.10f*cpuPwr,.06f+.62f*cpuPwr,.08f+.88f*cpuPwr,10,12,.72f*cpuPwr);
    // drive/activity LED flickers only while CPU is running
    float activity = (cpuPwr>.7f && g_pcOn) ? (.35f+.65f*std::fabs(std::sin(g_pcBootTimer*8.0f))) : 0.0f;
    sphere(3.983f,.84f,-.36f,.010f,.010f,.010f,.05f+.15f*activity,.07f+.78f*activity,.04f+.25f*activity,8,10,.70f*activity);

    // books + pen cup
    box(4.30f,1.18f,-.46f,.48f,.09f,.48f,.52f,.10f,.06f,18.0f);
    box(4.30f,1.28f,-.46f,.44f,.08f,.44f,.10f,.30f,.48f,18.0f);
    cylinderY(4.46f,1.27f,-1.95f,.12f,.34f,.08f,.18f,.38f,18);
    cylinderY(4.43f,1.51f,-1.95f,.012f,.30f,.85f,.14f,.10f,10);
    cylinderY(4.49f,1.51f,-1.96f,.012f,.30f,.12f,.18f,.75f,10);

    // chair facing desk.
    drawShadowQuad(2.88f,-1.30f,1.15f,1.20f,.18f);
    box(2.88f,.62f,-1.30f,.86f,.15f,.94f,.27f,.12f,.07f,25.0f);
    box(2.46f,1.15f,-1.30f,.14f,1.15f,.94f,.28f,.12f,.07f,25.0f);
    for(float z: {-1.66f,-.94f}) for(float x:{2.58f,3.18f}) box(x,.30f,z,.10f,.62f,.10f,.18f,.075f,.04f,18.0f);
    box(2.89f,.69f,-1.30f,.72f,.10f,.80f,.055f,.19f,.43f,22.0f);
    box(2.52f,1.16f,-1.30f,.06f,.95f,.76f,.055f,.19f,.43f,22.0f);
}

static void drawShelfAndPlant()
{
    // A deeper, level floating shelf so every object rests fully on the board.
    const float shelfX = 4.68f;
    const float shelfY = 2.72f;
    const float shelfZ = -3.34f;
    const float shelfTop = 2.80f;

    // main wooden shelf board: deep enough in X and long enough in Z
    box(shelfX,shelfY,shelfZ,.62f,.16f,2.82f,.34f,.14f,.055f,30.0f,.18f,.12f,.08f);

    // small wall support blocks under the shelf for a more believable installation
    box(4.88f,2.56f,-4.18f,.18f,.32f,.12f,.27f,.105f,.045f,24.0f);
    box(4.88f,2.56f,-2.50f,.18f,.32f,.12f,.27f,.105f,.045f,24.0f);

    // books: all bottoms sit exactly on the shelf surface and all are fully supported
    const float cols[6][3] = {
        {.52f,.10f,.08f}, {.72f,.38f,.07f}, {.08f,.27f,.35f},
        {.12f,.36f,.24f}, {.36f,.09f,.24f}, {.48f,.16f,.18f}
    };
    const float bookZ[6] = {-4.26f,-4.02f,-3.78f,-3.54f,-3.30f,-3.06f};
    const float bookH[6] = {.50f,.56f,.53f,.58f,.51f,.55f};
    for(int i=0;i<6;i++) {
        float cy = shelfTop + bookH[i]*0.5f;
        box(4.60f,cy,bookZ[i],.27f,bookH[i],.18f,
            cols[i][0],cols[i][1],cols[i][2],18.0f);
    }

    // short book-end block keeps the books visually contained
    box(4.60f,3.03f,-2.89f,.28f,.46f,.09f,.24f,.10f,.045f,22.0f);

    // flower pot: lowered so the base touches the shelf instead of floating over it
    const float potX = 4.56f;
    const float potZ = -2.35f;
    const float potH = .46f;
    coneY(potX, shelfTop + potH*0.5f, potZ,
          .24f,.31f,potH,.62f,.43f,.24f,24);

    // darker rim and saucer make the pot sit naturally on the shelf
    cylinderY(potX,shelfTop + .025f,potZ,.29f,.05f,.42f,.25f,.12f,24);
    cylinderY(potX,shelfTop + potH + .015f,potZ,.315f,.055f,.52f,.34f,.18f,24);

    // stems start inside the pot, not below or outside it
    cylinderY(potX,shelfTop + potH + .27f,potZ,.024f,.54f,.10f,.28f,.08f,10);
    cylinderY(potX-.10f,shelfTop + potH + .24f,potZ+.02f,.018f,.44f,.10f,.26f,.08f,10);
    cylinderY(potX+.10f,shelfTop + potH + .23f,potZ-.02f,.018f,.42f,.10f,.26f,.08f,10);

    // compact leaves arranged around the stems so the plant looks planted, not floating
    sphere(potX-.13f,3.55f,potZ,.10f,.31f,.075f,.15f,.48f,.16f,10,14);
    sphere(potX+.13f,3.53f,potZ,.10f,.30f,.075f,.12f,.42f,.14f,10,14);
    sphere(potX,3.66f,potZ+.06f,.105f,.34f,.075f,.19f,.55f,.18f,10,14);
    sphere(potX-.03f,3.43f,potZ-.06f,.10f,.27f,.070f,.13f,.44f,.14f,10,14);
}

static void drawPicture()
{
    // frame and art on back wall to receive the spotlight
    box(1.85f,2.65f,-5.86f,1.45f,1.45f,.08f,.21f,.10f,.045f,35.0f,.20f,.14f,.09f);
    box(1.85f,2.65f,-5.805f,1.20f,1.19f,.025f,.88f,.82f,.67f,10.0f);

    // simple colored landscape drawn in front of picture panel
    glDisable(GL_LIGHTING);
    glBegin(GL_TRIANGLES);
    glColor3f(.30f,.46f,.36f); glVertex3f(1.30f,2.34f,-5.785f); glVertex3f(1.68f,2.88f,-5.785f); glVertex3f(1.96f,2.34f,-5.785f);
    glColor3f(.21f,.36f,.30f); glVertex3f(1.70f,2.34f,-5.782f); glVertex3f(2.18f,2.78f,-5.782f); glVertex3f(2.42f,2.34f,-5.782f);
    glEnd();
    glColor3f(.93f,.67f,.18f); glBegin(GL_TRIANGLE_FAN); glVertex3f(1.52f,2.88f,-5.775f);
    for(int i=0;i<=20;i++){ float a=2*PI*i/20; glVertex3f(1.52f+.11f*std::cos(a),2.88f+.11f*std::sin(a),-5.775f);} glEnd();
    glColor3f(1,1,1); glEnable(GL_LIGHTING);
}

static void drawRug()
{
    // Rug can slide left and right with keyboard input.
    float rx = .55f + g_rugOffsetX;
    // layered rectangular rug for a thick bordered look
    box(rx,.055f,1.55f,4.65f,.055f,2.75f,.33f,.065f,.18f,8.0f,.04f,.04f,.04f);
    box(rx,.087f,1.55f,4.20f,.035f,2.30f,.48f,.10f,.26f,8.0f,.04f,.04f,.04f);
    box(rx,.108f,1.55f,3.82f,.025f,1.92f,.36f,.07f,.20f,8.0f,.04f,.04f,.04f);
}

static void drawDoor()
{
    // Realistic bedroom door: correct proportions, casing, jamb, raised panels,
    // hinges and a lever handle. The whole leaf rotates from a true hinge edge.

    const float frameX = -4.88f;
    const float doorCenterY = 1.58f;
    const float hingeZ = 3.36f;
    const float doorHalfWidth = 0.75f;

    // Dark opening behind the leaf. It is only slightly larger than the door,
    // avoiding the large black gaps visible in the previous version.
    box(-4.975f,1.61f,4.11f,.10f,3.18f,1.62f,
        .055f,.050f,.045f,5.0f,.01f,.01f,.01f);

    // Inner jambs and header.
    box(frameX,1.61f,3.31f,.12f,3.18f,.10f,.30f,.17f,.095f,24.0f,.12f,.09f,.06f);
    box(frameX,1.61f,4.91f,.12f,3.18f,.10f,.30f,.17f,.095f,24.0f,.12f,.09f,.06f);
    box(frameX,3.18f,4.11f,.12f,.12f,1.70f,.30f,.17f,.095f,24.0f,.12f,.09f,.06f);

    // Room-side decorative casing around the frame.
    box(-4.78f,1.61f,3.23f,.055f,3.30f,.16f,.39f,.23f,.13f,28.0f,.14f,.10f,.07f);
    box(-4.78f,1.61f,4.99f,.055f,3.30f,.16f,.39f,.23f,.13f,28.0f,.14f,.10f,.07f);
    box(-4.78f,3.27f,4.11f,.055f,.16f,1.92f,.39f,.23f,.13f,28.0f,.14f,.10f,.07f);

    // Small threshold at the floor.
    box(-4.79f,.075f,4.11f,.12f,.07f,1.64f,.22f,.13f,.075f,18.0f,.07f,.05f,.04f);

    // Door leaf. At angle 0 it sits properly inside the frame.
    glPushMatrix();
    glTranslatef(-4.715f,doorCenterY,hingeZ);
    glRotatef(g_doorAngle,0,1,0);
    glTranslatef(0.0f,0.0f,doorHalfWidth);

    // Main solid wooden slab.
    glPushMatrix();
    glScalef(.085f,3.02f,1.50f);
    setMaterial(.34f,.16f,.075f,32.0f,.20f,.13f,.08f);
    drawUnitBox();
    glPopMatrix();

    // Slightly darker door edges create visible thickness.
    box(.049f,0.0f,-.735f,.018f,2.98f,.035f,.21f,.095f,.040f,18.0f,.05f,.04f,.03f);
    box(.049f,0.0f, .735f,.018f,2.98f,.035f,.21f,.095f,.040f,18.0f,.05f,.04f,.03f);
    box(.049f,1.47f,0.0f,.018f,.035f,1.44f,.21f,.095f,.040f,18.0f,.05f,.04f,.03f);
    box(.049f,-1.47f,0.0f,.018f,.035f,1.44f,.21f,.095f,.040f,18.0f,.05f,.04f,.03f);

    // Raised outer moulding gives the leaf the depth of a real panel door.
    box(.052f, 1.08f,0.0f,.024f,.055f,1.18f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f, 0.08f,0.0f,.024f,.055f,1.18f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f,-0.18f,0.0f,.024f,.055f,1.18f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f,-1.14f,0.0f,.024f,.055f,1.18f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f, .58f,-.58f,.024f,.94f,.055f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f, .58f, .58f,.024f,.94f,.055f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f,-.66f,-.58f,.024f,.90f,.055f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);
    box(.052f,-.66f, .58f,.024f,.90f,.055f,.46f,.25f,.13f,24.0f,.12f,.08f,.05f);

    // Recessed/raised inner panels: one upper and one lower.
    box(.058f, .58f,0.0f,.025f,.82f,1.02f,.285f,.125f,.055f,20.0f,.07f,.05f,.035f);
    box(.058f,-.66f,0.0f,.025f,.78f,1.02f,.285f,.125f,.055f,20.0f,.07f,.05f,.035f);
    box(.069f, .58f,0.0f,.012f,.68f,.88f,.37f,.18f,.085f,18.0f,.06f,.045f,.03f);
    box(.069f,-.66f,0.0f,.012f,.64f,.88f,.37f,.18f,.085f,18.0f,.06f,.045f,.03f);

    // Three visible metal hinges on the hinge side.
    for(float hy : {-1.02f, 0.0f, 1.02f}) {
        box(.068f,hy,-.724f,.035f,.22f,.055f,.54f,.50f,.43f,55.0f,.65f,.62f,.56f);
    }

    // Realistic lever-handle assembly at normal handle height, close to latch edge.
    box(.073f,-.43f,.565f,.035f,.23f,.13f,.50f,.49f,.48f,70.0f,.76f,.76f,.76f); // back plate
    sphere(.095f,-.43f,.565f,.050f,.050f,.050f,.64f,.63f,.61f,10,16);              // spindle hub
    box(.100f,-.43f,.435f,.045f,.050f,.31f,.63f,.62f,.60f,75.0f,.82f,.82f,.82f); // lever
    box(.100f,-.43f,.285f,.045f,.050f,.08f,.52f,.51f,.50f,60.0f,.72f,.72f,.72f); // lever tip

    // Latch plate on the closing edge.
    box(.010f,-.43f,.746f,.055f,.20f,.018f,.62f,.60f,.55f,60.0f,.75f,.72f,.68f);

    glPopMatrix();

    // Strike plate on the frame, visible when the door opens.
    box(-4.70f,1.15f,4.905f,.040f,.22f,.035f,.58f,.56f,.52f,60.0f,.72f,.70f,.66f);
}

static void drawWallSwitchPanel()
{
    // A realistic three-gang switch board beside the bedroom door.
    // From left-to-right along the wall: FAN, MAIN LIGHT, SPOTLIGHT.
    const float panelX = -4.89f;
    const float panelY = 1.50f;
    const float zs[3] = { 2.46f, 2.66f, 2.86f };
    const bool states[3] = { g_fanOn, g_pointLightOn, g_spotLightOn };

    // recessed shadow + outer plastic plate
    box(-4.925f,panelY,2.66f,.035f,.38f,.62f,.18f,.18f,.18f,12.0f,.04f,.04f,.04f);
    box(panelX,panelY,2.66f,.075f,.36f,.60f,.90f,.89f,.86f,48.0f,.24f,.24f,.24f);
    // inner raised border
    box(-4.845f,panelY,2.66f,.018f,.30f,.52f,.78f,.77f,.74f,35.0f,.16f,.16f,.16f);

    for (int i=0; i<3; ++i) {
        // switch socket / recess
        box(-4.825f,panelY,zs[i],.030f,.22f,.15f,.23f,.23f,.22f,16.0f,.06f,.06f,.06f);

        // rocker switch: ON tilts top inward, OFF tilts bottom inward.
        glPushMatrix();
        glTranslatef(-4.795f,panelY,zs[i]);
        glRotatef(states[i] ? -8.0f : 8.0f,0,0,1);
        glScalef(.045f,.17f,.11f);
        setMaterial(.93f,.92f,.89f,55.0f,.30f,.30f,.30f);
        drawUnitBox();
        glPopMatrix();

        // tiny status LED on each gang
        float er = states[i] ? 0.12f : 0.24f;
        float eg = states[i] ? 0.72f : 0.08f;
        float eb = states[i] ? 0.22f : 0.05f;
        sphere(-4.755f,panelY+.205f,zs[i],.030f,.030f,.030f,er,eg,eb,8,12,states[i] ? .75f : .08f);
    }

    // small lower icon bars to visually distinguish the controls.
    // Fan symbol: four tiny blades.
    glPushMatrix();
    glTranslatef(-4.75f,panelY-.205f,zs[0]);
    for(int i=0;i<4;i++){
        glPushMatrix(); glRotatef(i*90.0f,1,0,0); glTranslatef(0,.035f,0);
        glScalef(.018f,.075f,.028f); setMaterial(.25f,.25f,.24f,12.0f); drawUnitBox(); glPopMatrix();
    }
    glPopMatrix();
    // Main light icon / spot icon as simple dark indicator bars.
    box(-4.75f,panelY-.205f,zs[1],.018f,.055f,.10f,.25f,.25f,.24f,12.0f);
    box(-4.75f,panelY-.205f,zs[2],.018f,.035f,.12f,.25f,.25f,.24f,12.0f);
}

static void drawFan()
{
    // ceiling rod and hub
    cylinderY(-.45f,3.72f,-.15f,.075f,.50f,.08f,.20f,.24f,18);
    sphere(-.45f,3.45f,-.15f,.24f,.14f,.24f,.055f,.17f,.22f,10,18);

    glPushMatrix();
    glTranslatef(-.45f,3.45f,-.15f);
    glRotatef(g_fanAngle,0,1,0);
    for(int i=0;i<4;i++) {
        glPushMatrix();
        glRotatef(i*90.0f,0,1,0);
        glTranslatef(1.00f,0,0);
        glRotatef(-6.0f,0,0,1);
        glScalef(1.65f,.075f,.34f);
        setMaterial(.045f,.16f,.20f,48.0f,.32f,.35f,.38f);
        drawUnitBox();
        glPopMatrix();
    }
    glPopMatrix();
}

static void drawCeilingLightFixture()
{
    cylinderY(2.75f,4.00f,1.10f,.36f,.12f,.76f,.74f,.69f,30);
    sphere(2.75f,3.88f,1.10f,.27f,.10f,.27f,1.00f,.82f,.50f,10,20, g_pointLightOn ? .80f : .08f);

    // spotlight fixture near picture
    cylinderY(1.85f,3.95f,-3.70f,.13f,.24f,.16f,.15f,.14f,18);
    glPushMatrix(); glTranslatef(1.85f,3.78f,-3.72f); glRotatef(32.0f,1,0,0);
    coneY(0,0,0,.18f,.11f,.34f,.20f,.18f,.16f,18); glPopMatrix();
}

static void setupLights()
{
    GLfloat globalAmbient[] = { .16f,.16f,.18f,1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // Light 1: warm ceiling point light
    if(g_pointLightOn) glEnable(GL_LIGHT0); else glDisable(GL_LIGHT0);
    GLfloat p0[] = {2.75f,3.72f,1.10f,1.0f};
    GLfloat a0[] = {.13f,.105f,.08f,1};
    GLfloat d0[] = {1.00f,.78f,.52f,1};
    GLfloat s0[] = {1.00f,.88f,.70f,1};
    glLightfv(GL_LIGHT0,GL_POSITION,p0); glLightfv(GL_LIGHT0,GL_AMBIENT,a0); glLightfv(GL_LIGHT0,GL_DIFFUSE,d0); glLightfv(GL_LIGHT0,GL_SPECULAR,s0);
    glLightf(GL_LIGHT0,GL_CONSTANT_ATTENUATION,.72f); glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,.07f); glLightf(GL_LIGHT0,GL_QUADRATIC_ATTENUATION,.016f);

    // Light 2: spotlight focused on the framed picture
    if(g_spotLightOn) glEnable(GL_LIGHT1); else glDisable(GL_LIGHT1);
    GLfloat p1[] = {1.85f,3.78f,-3.68f,1.0f};
    Vec3 target(1.85f,2.62f,-5.78f); Vec3 from(1.85f,3.78f,-3.68f); Vec3 d= vnormalize(target-from);
    GLfloat dir[] = {d.x,d.y,d.z};
    GLfloat a1[] = {.01f,.008f,.004f,1};
    GLfloat d1[] = {1.0f,.67f,.32f,1};
    GLfloat s1[] = {1.0f,.82f,.50f,1};
    glLightfv(GL_LIGHT1,GL_POSITION,p1); glLightfv(GL_LIGHT1,GL_SPOT_DIRECTION,dir);
    glLightfv(GL_LIGHT1,GL_AMBIENT,a1); glLightfv(GL_LIGHT1,GL_DIFFUSE,d1); glLightfv(GL_LIGHT1,GL_SPECULAR,s1);
    glLightf(GL_LIGHT1,GL_SPOT_CUTOFF,24.0f); glLightf(GL_LIGHT1,GL_SPOT_EXPONENT,22.0f);
    glLightf(GL_LIGHT1,GL_CONSTANT_ATTENUATION,.85f); glLightf(GL_LIGHT1,GL_LINEAR_ATTENUATION,.055f); glLightf(GL_LIGHT1,GL_QUADRATIC_ATTENUATION,.012f);
}

static void setProjection()
{
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = (g_height>0) ? (float)g_width/(float)g_height : 1.0f;
    gluPerspective(g_camera.Zoom,aspect,.10,100.0);
    glMatrixMode(GL_MODELVIEW);
}

static void applyCamera()
{
    Vec3 f=g_camera.Front(); Vec3 p=g_camera.Position;
    gluLookAt(p.x,p.y,p.z, p.x+f.x,p.y+f.y,p.z+f.z, 0,1,0);
}

static void drawInteractionCrosshair()
{
    // Disabled: bedroom walkthrough does not use FPS aiming markers.
}

static void renderScene()
{
    glViewport(0,0,g_width,g_height);
    glClearColor(.055f,.065f,.080f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    setProjection();
    glLoadIdentity(); applyCamera();
    setupLights();

    drawFloorAndRoom();
    drawWindow();
    drawRug();
    drawWardrobe();
    drawBed();
    drawNightstandAndLamp();
    drawDeskChairLaptop();
    drawShelfAndPlant();
    drawPicture();
    drawDoor();
    drawWallSwitchPanel();
    drawFan();
    drawCeilingLightFixture();

    SwapBuffers(g_hDC);
}

static void clampCamera()
{
    g_camera.Position.x=std::max(-4.55f,std::min(4.55f,g_camera.Position.x));
    g_camera.Position.z=std::max(-5.35f,std::min(5.45f,g_camera.Position.z));
    g_camera.Position.y=g_playerEyeHeight;
}

static void update(float dt)
{
    // Ceiling fan accelerates and coasts down like a real fan instead of stopping instantly.
    float fanTargetSpeed = g_fanOn ? 165.0f : 0.0f;
    g_fanAngularSpeed = approachValue(g_fanAngularSpeed, fanTargetSpeed, 105.0f * dt);
    if(g_fanAngularSpeed > 0.01f) {
        g_fanAngle += g_fanAngularSpeed * dt;
        if(g_fanAngle > 360.0f) g_fanAngle -= 360.0f;
    }

    // smooth transformation animations
    g_rugOffsetX = approachValue(g_rugOffsetX, g_rugTargetX, 1.60f * dt);
    g_doorAngle  = approachValue(g_doorAngle,  g_doorTargetAngle, 110.0f * dt);
// Both window leaves move together, each toward the same opening amount.
    g_windowLeftAngle  = approachValue(g_windowLeftAngle,  g_windowTargetAngle, 92.0f * dt);
    g_windowRightAngle = approachValue(g_windowRightAngle, g_windowTargetAngle, 92.0f * dt);

    // Real PC sequence: CPU starts first, then monitor boots.
    // On shutdown, the monitor goes dark first and CPU/fan stop shortly after.
    if(g_pcOn) {
        g_pcShutdownTimer = 0.0f;
        g_cpuPowerLevel = approachValue(g_cpuPowerLevel, 1.0f, 2.20f * dt);
        float monitorTarget = (g_cpuPowerLevel > .48f) ? 1.0f : 0.0f;
        g_pcPowerLevel = approachValue(g_pcPowerLevel, monitorTarget, 1.35f * dt);
        if(g_pcPowerLevel > .08f) g_pcBootTimer += dt;
    } else {
        g_pcShutdownTimer += dt;
        g_pcPowerLevel = approachValue(g_pcPowerLevel, 0.0f, 2.35f * dt);
        float cpuTarget = (g_pcShutdownTimer < .85f) ? 1.0f : 0.0f;
        g_cpuPowerLevel = approachValue(g_cpuPowerLevel, cpuTarget, 1.55f * dt);
        if(g_pcPowerLevel < .03f && g_cpuPowerLevel < .03f) g_pcBootTimer = 0.0f;
    }
    if(g_cpuPowerLevel > .04f) {
        g_cpuFanAngle += (120.0f + 720.0f*g_cpuPowerLevel) * dt;
        if(g_cpuFanAngle > 360.0f) g_cpuFanAngle -= 360.0f;
    }

    // Smooth human-style first-person movement.
    // WASD changes desired direction; velocity accelerates/decelerates instead of snapping.
    Vec3 f=g_camera.Front(); f.y=0; f=vnormalize(f);
    Vec3 r=vnormalize(vcross(f,Vec3(0,1,0)));
    Vec3 moveDir(0,0,0);
    if(g_mouseCaptured) {
        if(g_keys['W']) moveDir = moveDir + f;
        if(g_keys['S']) moveDir = moveDir - f;
        if(g_keys['A']) moveDir = moveDir - r;
        if(g_keys['D']) moveDir = moveDir + r;
    }

    bool hasInput = vlength(moveDir) > 0.0001f;
    bool running = (g_keys[VK_SHIFT] || g_keys[VK_LSHIFT] || g_keys[VK_RSHIFT]) && hasInput;
    if(hasInput) moveDir = vnormalize(moveDir);

    float targetSpeed = hasInput ? (running ? g_runSpeed : g_walkSpeed) : 0.0f;
    Vec3 targetVelocity = hasInput ? moveDir * targetSpeed : Vec3(0,0,0);
    float velocityStep = (hasInput ? g_moveAcceleration : g_moveDeceleration) * dt;
    g_playerVelocity.x = approachValue(g_playerVelocity.x, targetVelocity.x, velocityStep);
    g_playerVelocity.z = approachValue(g_playerVelocity.z, targetVelocity.z, velocityStep);

    if(std::fabs(g_playerVelocity.x) > 0.001f || std::fabs(g_playerVelocity.z) > 0.001f) {
        Vec3 desired = g_camera.Position + g_playerVelocity * dt;
        Vec3 resolved = resolvePlayerCollision(g_camera.Position, desired);

        // Stop only the velocity component that actually hit furniture; the other
        // component continues, giving a natural wall/furniture sliding feel.
        if(std::fabs(resolved.x - desired.x) > 0.002f) g_playerVelocity.x = 0.0f;
        if(std::fabs(resolved.z - desired.z) > 0.002f) g_playerVelocity.z = 0.0f;
        g_camera.Position = resolved;
    }
    clampCamera();

    // The person can interact only while physically near the object.
    g_nearbyTarget = findNearbyInteraction();
    updateInteractionTitle();
}

static void captureMouse(bool capture)
{
    g_mouseCaptured=capture;
    if(!g_hWnd) return;
    if(capture){
        RECT rc; GetClientRect(g_hWnd,&rc);
        POINT tl={rc.left,rc.top}, br={rc.right,rc.bottom};
        ClientToScreen(g_hWnd,&tl); ClientToScreen(g_hWnd,&br);
        RECT clip={tl.x,tl.y,br.x,br.y}; ClipCursor(&clip);
        while(ShowCursor(FALSE)>=0){}
        POINT c={(rc.right-rc.left)/2,(rc.bottom-rc.top)/2}; ClientToScreen(g_hWnd,&c); SetCursorPos(c.x,c.y);
    } else {
        ClipCursor(nullptr); while(ShowCursor(TRUE)<0){}
    }
}

static bool setupPixelFormat(HDC dc)
{
    PIXELFORMATDESCRIPTOR pfd={};
    pfd.nSize=sizeof(pfd); pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=32; pfd.cDepthBits=24; pfd.cStencilBits=8; pfd.iLayerType=PFD_MAIN_PLANE;
    int pf=ChoosePixelFormat(dc,&pfd); if(!pf) return false;
    return SetPixelFormat(dc,pf,&pfd)==TRUE;
}

static void initializeOpenGL()
{
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glEnable(GL_LIGHTING); glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_BLEND);
}

static void resetCamera()
{
    g_camera.Position=Vec3(0.0f,g_playerEyeHeight,5.15f); g_camera.Yaw=-90.0f; g_camera.Pitch=-5.0f; g_camera.Zoom=62.0f;
    g_rugOffsetX = g_rugTargetX = 0.0f;
    g_doorAngle = g_doorTargetAngle = 0.0f;
    g_windowLeftAngle = g_windowRightAngle = g_windowTargetAngle = 0.0f;
    g_pcOn = true;
    g_pcPowerLevel = 1.0f;
    g_cpuPowerLevel = 1.0f;
    g_cpuFanAngle = 0.0f;
    g_pcBootTimer = 3.0f;
    g_pcShutdownTimer = 0.0f;
    g_nearbyTarget = InteractionTarget::None;
    g_lastPromptTarget = InteractionTarget::Door; // force title refresh on next frame
    g_playerVelocity = Vec3(0.0f,0.0f,0.0f);
    g_fanAngularSpeed = g_fanOn ? 165.0f : 0.0f;
}

static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    switch(msg){
    case WM_SIZE:
        g_width=LOWORD(lParam); g_height=std::max(1,(int)HIWORD(lParam)); return 0;
    case WM_CLOSE:
        g_running=false; DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        g_running=false; PostQuitMessage(0); return 0;
    case WM_KILLFOCUS:
        for(bool &k:g_keys) k=false; g_playerVelocity=Vec3(0.0f,0.0f,0.0f); if(g_mouseCaptured){ ClipCursor(nullptr); while(ShowCursor(TRUE)<0){} } return 0;
    case WM_SETFOCUS:
        if(g_mouseCaptured) captureMouse(true); return 0;
    case WM_KEYDOWN: {
        int k=(int)wParam; if(k>=0&&k<256) g_keys[k]=true;
        bool first=(lParam & (1LL<<30))==0;
        if(first){
            if(k==VK_ESCAPE){ g_running=false; DestroyWindow(hwnd); }
            else if(k=='M') captureMouse(!g_mouseCaptured);
            else if(k=='E') { InteractionTarget t=findNearbyInteraction(); if(t!=InteractionTarget::None) interactWith(t); }
            // All interactive objects use one consistent rule: be within a balanced
            // distance, aim with the crosshair, then press E. No room-wide shortcuts.
            else if(k=='R') resetCamera();
        }
        return 0; }
    case WM_KEYUP: { int k=(int)wParam; if(k>=0&&k<256) g_keys[k]=false; return 0; }
    case WM_MOUSEWHEEL:
        g_camera.ProcessScroll((float)GET_WHEEL_DELTA_WPARAM(wParam)/WHEEL_DELTA*2.2f); return 0;
    case WM_MOUSEMOVE:
        if(g_mouseCaptured && GetForegroundWindow()==hwnd){
            RECT rc; GetClientRect(hwnd,&rc); int cx=(rc.right-rc.left)/2, cy=(rc.bottom-rc.top)/2;
            int mx=(short)LOWORD(lParam), my=(short)HIWORD(lParam); int dx=mx-cx, dy=my-cy;
            if(dx!=0||dy!=0){
                g_camera.ProcessMouseMovement((float)dx,(float)dy);
                POINT c={cx,cy}; ClientToScreen(hwnd,&c); SetCursorPos(c.x,c.y);
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int nCmdShow)
{
    WNDCLASS wc={}; wc.style=CS_OWNDC|CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc=WndProc; wc.hInstance=hInst;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.lpszClassName=L"Bedroom3DOpenGLWindow";
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    if(!RegisterClass(&wc)) return 1;

    RECT rc={0,0,g_width,g_height}; AdjustWindowRect(&rc,WS_OVERLAPPEDWINDOW,FALSE);
    g_hWnd=CreateWindow(wc.lpszClassName,L"Bedroom 3D | WASD Walk | Shift Run | Mouse Look | Aim at nearby object and press E",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,rc.right-rc.left,rc.bottom-rc.top,nullptr,nullptr,hInst,nullptr);
    if(!g_hWnd) return 2;

    g_hDC=GetDC(g_hWnd); if(!setupPixelFormat(g_hDC)) return 3;
    g_hRC=wglCreateContext(g_hDC); if(!g_hRC) return 4;
    wglMakeCurrent(g_hDC,g_hRC); initializeOpenGL();

    ShowWindow(g_hWnd,nCmdShow); UpdateWindow(g_hWnd); SetFocus(g_hWnd); captureMouse(true);

    MSG msg={};
    auto last=std::chrono::high_resolution_clock::now();
    while(g_running){
        while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){ g_running=false; break; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        auto now=std::chrono::high_resolution_clock::now();
        float dt=std::chrono::duration<float>(now-last).count(); last=now;
        if(dt>0.05f) dt=0.05f;
        update(dt); renderScene(); Sleep(1);
    }

    captureMouse(false);
    if(g_hRC){ wglMakeCurrent(nullptr,nullptr); wglDeleteContext(g_hRC); }
    if(g_hDC&&g_hWnd) ReleaseDC(g_hWnd,g_hDC);
    return 0;
}
