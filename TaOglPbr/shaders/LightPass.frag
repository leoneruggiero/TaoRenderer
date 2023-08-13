#version 430 core

#define GBUFF_READ

//! #include "GPassHelper.glsl"
//! #include "UboDefs.glsl"
//! #include "PbrHelper.glsl"

out layout (location = 0) vec4 FragColor;

layout(location = 4) uniform sampler2D   envBrdfLut;
layout(location = 5) uniform samplerCube envIrradiance;
layout(location = 6) uniform samplerCube envPrefiltered;


vec3 DiffuseDirectionalLight(float NoL, vec3 diffuse, float metalness, vec3 lightColor)
{
    vec3 c = diffuse.rgb * lightColor * NoL;
    return  mix(c, vec3(0.0), metalness);
}

vec3 SpecularDirectionalLight(float NoL, float NoV, float NoH, float VoH, vec3 F0, float roughness, vec3 lightColor)
{
    return  lightColor * NoL * SpecularBRDF(NoL, NoV, NoH, VoH, F0, roughness);
}

vec3 ComputeDirectionalLight(float NoL, float NoV, float NoH, float VoH, vec3 f0, vec3 diffuse, float roughness,float metalness, vec3 lightColor)
{
    vec3 kd = 1.0 - SpecularF(f0, NoV);
    vec3 directDiffuse  = DiffuseDirectionalLight(NoL, diffuse, metalness, lightColor);
    vec3 directSpecular = SpecularDirectionalLight(NoL, NoV, NoH, VoH, f0, roughness, lightColor);

    return kd * directDiffuse + directSpecular; // "ks" is already in the GGX brdf
}

vec3 ComputeAmbientLight(vec3 viewDir, vec3 normal, vec3 f0, vec3 diffuse, float roughness, float metalness, float environemtIntensity)
{
    // Diffuse
    // -------------
    vec3 ambientD = environemtIntensity * diffuse * texture(envIrradiance, normal.xzy * vec3(1,-1,1)).rgb;
    ambientD = mix(ambientD, vec3(0.0), metalness);

    // Spcular
    // -------------
    vec3 reflDir = reflect(-viewDir, normal);
    reflDir = reflDir.xzy * vec3(1,-1,1); // TODO

    float NoV = CLAMPED_DOT(normal, viewDir);
    float lod = f_radianceMinLod + roughness * (f_radianceMaxLod - f_radianceMinLod);
    vec3 irr       = textureLod(envPrefiltered, normalize(reflDir), lod).rgb;
    vec2 scaleBias = texture(envBrdfLut, vec2(roughness, NoV)).xy;
    vec3 ambientS  = environemtIntensity * irr * (f0 * scaleBias.x + scaleBias.y);

    return ambientS + ambientD;
}

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

    vec4 col = vec4(0.0, 0.0, 0.0, 1.0);

    vec3 LIGHT_DIR = vec3(1.0, 0.0, 0.5);
    vec3 LIGHT_COL = 1.0 * vec3(1.0, 1.0, 1.0);

    if(posWorld!=vec3(0.0))
    {
        // TODO: meh?
        nrmWorld = normalize(nrmWorld);

        vec3 lightDir = normalize(LIGHT_DIR);
        vec3 viewDir  = normalize(f_eyeWorldPos.xyz - posWorld);
        vec3 h        = normalize(viewDir + lightDir);

        float NoL = CLAMPED_DOT(nrmWorld, lightDir);
        float NoV = CLAMPED_DOT(nrmWorld, viewDir);
        float NoH = CLAMPED_DOT(nrmWorld, h);
        float VoH = CLAMPED_DOT(viewDir, h);

        vec3 f0 = mix(F0_DIELECTRIC, albedo.rgb, metalness);

        col.rgb += ComputeDirectionalLight(NoL, NoV, NoH, VoH, f0, albedo.rgb, roughness, metalness, LIGHT_COL);
        col.rgb += ComputeAmbientLight(viewDir, nrmWorld, f0, albedo.rgb, roughness, metalness, 0.3);

    }
    else
            discard;

    if(f_doGamma)
    {
        col=pow(col,vec4(1.0/f_gamma));
    }

    FragColor =  col;
}