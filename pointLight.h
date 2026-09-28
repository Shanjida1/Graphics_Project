#pragma once

// Small course-style point-light data container.
// main.cpp currently sends equivalent values through the OpenGL compatibility lighting API.
struct PointLight
{
    float position[3];
    float ambient[3];
    float diffuse[3];
    float specular[3];
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;

    PointLight(
        float px, float py, float pz,
        float ar, float ag, float ab,
        float dr, float dg, float db,
        float sr, float sg, float sb,
        float kc = 1.0f, float kl = 0.09f, float kq = 0.032f)
        : position{ px, py, pz },
          ambient{ ar, ag, ab },
          diffuse{ dr, dg, db },
          specular{ sr, sg, sb },
          constantAttenuation(kc),
          linearAttenuation(kl),
          quadraticAttenuation(kq)
    {
    }
};
