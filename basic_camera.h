#pragma once
#include "camera.h"

// Simple look-at camera data, kept as a separate helper like the teacher project.
class BasicCamera
{
public:
    Vec3 eye;
    Vec3 target;
    Vec3 up;

    BasicCamera(
        float eyeX, float eyeY, float eyeZ,
        float targetX, float targetY, float targetZ,
        const Vec3& upVector = Vec3(0.0f, 1.0f, 0.0f))
        : eye(eyeX, eyeY, eyeZ),
          target(targetX, targetY, targetZ),
          up(upVector)
    {
    }

    Vec3 Direction() const
    {
        return vnormalize(target - eye);
    }
};
