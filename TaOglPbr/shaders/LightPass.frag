#version 430 core

#define GBUFF_READ

//! #include "GPassHelper.glsl"

out layout (location = 0) vec4 FragColor;

void main()
{
    // Gbuff data
    vec3 posWorld;
    vec3 nrmWorld;
    vec3 albedo;
    vec3 emission;
    float roughness;
    float metalness;
    float occlusion;

    ivec2 fragCoord = ivec2(gl_FragCoord.xy-0.5);
    ReadGBuff(fragCoord, albedo, emission, posWorld, nrmWorld, roughness, metalness, occlusion);

    vec4 col = vec4(albedo, 1.0);
    FragColor =  col;
}