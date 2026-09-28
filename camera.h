#pragma once
#include <cmath>

struct Vec3 {
    float x, y, z;
    Vec3(float X = 0, float Y = 0, float Z = 0) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
};

inline float vdot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 vcross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
inline float vlength(const Vec3& a) { return std::sqrt(vdot(a, a)); }
inline Vec3 vnormalize(const Vec3& a) {
    float l = vlength(a);
    return (l > 0.00001f) ? a * (1.0f / l) : Vec3();
}

class Camera {
public:
    Vec3 Position;
    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(Vec3 p = Vec3(0.0f, 1.65f, 5.2f), float yaw = -90.0f, float pitch = -6.0f)
        : Position(p), Yaw(yaw), Pitch(pitch), MovementSpeed(3.2f), MouseSensitivity(0.070f), Zoom(68.0f) {}

    Vec3 Front() const {
        const float d2r = 3.14159265358979323846f / 180.0f;
        float y = Yaw * d2r, p = Pitch * d2r;
        return vnormalize(Vec3(std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)));
    }

    Vec3 Right() const {
        return vnormalize(vcross(Front(), Vec3(0, 1, 0)));
    }

    void ProcessMouseMovement(float dx, float dy) {
        Yaw += dx * MouseSensitivity;
        Pitch -= dy * MouseSensitivity;
        if (Pitch > 82.0f) Pitch = 82.0f;
        if (Pitch < -82.0f) Pitch = -82.0f;
    }

    void ProcessScroll(float delta) {
        Zoom -= delta;
        if (Zoom < 50.0f) Zoom = 50.0f;
        if (Zoom > 72.0f) Zoom = 72.0f;
    }
};
