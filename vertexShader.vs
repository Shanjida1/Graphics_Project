#version 120

varying vec3 vNormal;
varying vec3 vEyePosition;

void main()
{
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    vEyePosition = eyePosition.xyz;
    vNormal = normalize(gl_NormalMatrix * gl_Normal);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
