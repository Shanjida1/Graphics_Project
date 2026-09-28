#version 120

varying vec4 gouraudColor;

void main()
{
    vec3 N = normalize(gl_NormalMatrix * gl_Normal);
    vec3 P = vec3(gl_ModelViewMatrix * gl_Vertex);
    vec3 L = normalize(vec3(gl_LightSource[0].position) - P);
    vec3 V = normalize(-P);
    vec3 R = reflect(-L, N);

    float diffuseAmount = max(dot(N, L), 0.0);
    float specularAmount = 0.0;
    if (diffuseAmount > 0.0)
        specularAmount = pow(max(dot(R, V), 0.0), 24.0);

    vec4 ambient = gl_FrontMaterial.ambient * gl_LightSource[0].ambient;
    vec4 diffuse = gl_FrontMaterial.diffuse * gl_LightSource[0].diffuse * diffuseAmount;
    vec4 specular = gl_FrontMaterial.specular * gl_LightSource[0].specular * specularAmount;

    gouraudColor = ambient + diffuse + specular;
    gouraudColor.a = gl_FrontMaterial.diffuse.a;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
