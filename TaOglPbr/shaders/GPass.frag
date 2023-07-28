#version 430 core
    
//! #include "UboDefs.glsl"

in VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
    mat3 TBN;
}fs_in;

layout (location = 0) out vec4 gBuff0;
layout (location = 1) out vec4 gBuff1;
layout (location = 2) out vec4 gBuff2; 
layout (location = 3) out vec4 gBuff3;
layout (location = 4) out vec4 gBuff4;


uniform sampler2D t_Albedo;
uniform sampler2D t_Emission;
uniform sampler2D t_Normals;
uniform sampler2D t_Metalness;
uniform sampler2D t_Roughness;
uniform sampler2D t_Occlusion;
  

void WriteGBuff(vec3 albedo, vec3 emission, vec3 position, vec3 normal, float roughness, float metalness, float occlusion)
{
    // TODO: wasting a lot of space...
    gBuff0 = vec4(albedo, 0.0);
    gBuff1 = vec4(emission, 0.0);
    gBuff2 = vec4(roughness, metalness, occlusion, 0.0);
    gBuff3 = vec4(position, 1.0);
    gBuff4 = vec4(normal, 0.0);
}

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
            ? texture(t_Metalness, fs_in.textureCoordinates).r
            : o_material.Metalness;
}

float GetRoughness()
{
    return o_material.hasTex_Roughness
            ? texture(t_Roughness, fs_in.textureCoordinates).r
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