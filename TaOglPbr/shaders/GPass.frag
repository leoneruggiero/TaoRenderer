#version 430 core

#define GPASS
#define GBUFF_WRITE

//! #include "UboDefs.glsl"
//! #include "GPassHelper.glsl"

in VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
    mat3 TBN;
}fs_in;

layout(binding=0) uniform sampler2D t_Albedo;
layout(binding=1) uniform sampler2D t_Emission;
layout(binding=2) uniform sampler2D t_Normals;
layout(binding=3) uniform sampler2D t_Metalness;
layout(binding=4) uniform sampler2D t_Roughness;
layout(binding=5) uniform sampler2D t_Occlusion;

vec3 GetAlbedo()
{
    vec3 albedo =  
        o_material.hasTex_Albedo
            ? texture(t_Albedo, fs_in.textureCoordinates).rgb
            : o_material.Albedo.rgb;

    return (f_doGamma && o_material.hasTex_Albedo)
            ? vec3(pow(albedo.rgb, vec3(f_gamma)))
            : albedo;
}

vec3 GetEmission()
{
    vec3 emission =  
        o_material.hasTex_Emission
            ? texture(t_Emission, fs_in.textureCoordinates).rgb
            : o_material.Emission.rgb;

    return (f_doGamma && o_material.hasTex_Emission)
            ? vec3(pow(emission.rgb, vec3(f_gamma)))
            : emission;
}

vec3 GetNormal()
{
        return o_material.hasTex_Normals
            ? normalize (fs_in.TBN * (texture(t_Normals, fs_in.textureCoordinates).rgb*2.0-1.0))
            : normalize (fs_in.worldNormal);
}

float GetMetalness()
{
    return o_material.hasTex_Metalness
            ? (o_material.has_merged_MetalRough ? texture(t_Metalness, fs_in.textureCoordinates).b : texture(t_Metalness, fs_in.textureCoordinates).r)
            : o_material.Metalness;
}

float GetRoughness()
{
    return o_material.hasTex_Roughness
            ? (o_material.has_merged_MetalRough ? texture(t_Roughness, fs_in.textureCoordinates).g : texture(t_Roughness, fs_in.textureCoordinates).r)
            : o_material.Roughness;
}

float GetOcclusion()
{
    return o_material.hasTex_Occlusion
            ? texture(t_Occlusion, fs_in.textureCoordinates).r
            : 1.0;
}

void main()
{
    vec3 position = fs_in.fragPosWorld;
    vec3 normal     = GetNormal();
    vec3 albedo     = GetAlbedo();
    vec3 emission   = GetEmission();
    float roughness = GetRoughness();
    float metalness = GetMetalness();
    float occlusion = GetOcclusion();

    WriteGBuff(
        albedo,
        emission,
        position, 
        normal,
        roughness, metalness, occlusion
    );
}