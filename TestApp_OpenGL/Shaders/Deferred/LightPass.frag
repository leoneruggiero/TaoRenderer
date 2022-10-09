#version 430 core

//! #include "../Include/UboDefs.glsl"
//! #include "../Include/DebugViz.glsl"

uniform sampler2D gBuff0;
uniform sampler2D gBuff1;
uniform sampler2D gBuff2; 
uniform sampler2D gBuff3;
uniform sampler2D gBuff4;

// Shadow maps
uniform sampler2D    t_shadowMap_directional;
uniform samplerCube  t_shadowMap_point[3];

// IBL
uniform samplerCube t_IrradianceMap;
uniform samplerCube t_RadianceMap; 
uniform sampler2D   t_BrdfLut;

// Utils
uniform sampler1D   t_PoissonSamples;
uniform sampler2D   t_NoiseTex_0;
uniform sampler2D   t_NoiseTex_1;
uniform sampler2D   t_NoiseTex_2;

//! #include "LightPassHelper.glsl"

void ReadGBuff(
ivec2 texCoord,
out vec3 albedo, out vec3 emission, out vec3 position, out vec3 normal, out float roughness, out float metalness, out float occlusion, out bool containsData)
{
    albedo      = texelFetch(gBuff0, texCoord, 0).rgb;
    emission    = texelFetch(gBuff1, texCoord, 0).rgb;
    vec3 buff2  = texelFetch(gBuff2, texCoord, 0).rgb;
    roughness   = buff2.x;
    metalness   = buff2.y;
    occlusion   = buff2.z;
    vec4 buff3  = texelFetch(gBuff3, texCoord, 0).rgba;
    position    = buff3.rgb;
    containsData= buff3.a!=0.0;
    normal      = texelFetch(gBuff4, texCoord, 0).rgb;
}

out vec4 FragColor;

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
    bool validSample;

    ivec2 fragCoord = ivec2(gl_FragCoord.xy-0.5);
    ReadGBuff(fragCoord, albedo, emission, posWorld, nrmWorld, roughness, metalness, occlusion, validSample);

    if(!validSample) discard;

    // Shadow values for all the shadow casters
    float shadow_dir = 1.0;        
    float shadow_point[MAX_POINT_LIGHTS];
    for(int i=0; i<MAX_POINT_LIGHTS; i++)
        shadow_point[i] = 1.0;
    
    shadow_dir = ComputeDirectionalShadow(f_lightSpaceMatrix_directional * vec4(posWorld, 1.0), nrmWorld);
    for(int i=0; i<MAX_POINT_LIGHTS; i++)
        shadow_point[i] = ComputePointShadow(i, posWorld, nrmWorld);

    // Direct and Ambient light computation.
    vec3 direct;
    vec3 ambient;

    ComputeLighting(
    f_eyeWorldPos.xyz, // camera position in world space
    posWorld, nrmWorld, emission, albedo, roughness, metalness,
    shadow_dir, shadow_point,
    // Out params
    direct, ambient);


    FragColor = vec4(direct + ambient * occlusion, 1.0);
}