#version 120

varying vec3 fragPositionEye;
varying vec3 fragNormalEye;

void main()
{
    vec3 N = normalize(fragNormalEye);
    vec3 lightPositionEye = vec3(gl_LightSource[0].position);
    vec3 L = normalize(lightPositionEye - fragPositionEye);
    vec3 V = normalize(-fragPositionEye);
    vec3 R = reflect(-L, N);

    float diffuseAmount = max(dot(N, L), 0.0);
    float specularAmount = 0.0;
    if (diffuseAmount > 0.0)
        specularAmount = pow(max(dot(R, V), 0.0), gl_FrontMaterial.shininess);

    vec4 ambient = gl_FrontMaterial.ambient * gl_LightSource[0].ambient;
    vec4 diffuse = gl_FrontMaterial.diffuse * gl_LightSource[0].diffuse * diffuseAmount;
    vec4 specular = gl_FrontMaterial.specular * gl_LightSource[0].specular * specularAmount;

    gl_FragColor = ambient + diffuse + specular + gl_FrontMaterial.emission;
    gl_FragColor.a = gl_FrontMaterial.diffuse.a;
}
