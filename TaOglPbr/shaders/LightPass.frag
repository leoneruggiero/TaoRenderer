#version 430 core

#define GBUFF_READ

//! #include "GPassHelper.glsl"

out layout (location = 0) vec4 FragColor;

layout(location = 4) uniform sampler2D   envBrdfLut;
layout(location = 5) uniform samplerCube envIrradiance;
layout(location = 6) uniform samplerCube envPrefiltered;

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

    col = vec4(nrmWorld, 1.0);


    FragColor =  col;
}