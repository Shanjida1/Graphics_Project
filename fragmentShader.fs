#version 120

varying vec3 vNormal;
varying vec3 vEyePosition;

void main()
{
    vec3 N = normalize(vNormal);
    float facing = 0.55 + 0.45 * max(N.z, 0.0);
    gl_FragColor = vec4(vec3(0.72, 0.76, 0.82) * facing, 1.0);
}
