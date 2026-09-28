#version 120

varying vec3 fragPositionEye;
varying vec3 fragNormalEye;

void main()
{
    vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;
    fragPositionEye = eyePosition.xyz;
    fragNormalEye = normalize(gl_NormalMatrix * gl_Normal);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
