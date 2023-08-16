#version 430 core

#define GBUFF_READ

//! #include "GPassHelper.glsl"
//! #include "UboDefs.glsl"
//! #include "PbrHelper.glsl"

out layout (location = 0) vec4 FragColor;

layout (std140, binding = 4) uniform blk_LightsData
{
    bool    u_doEnvironment;
    float   u_environmentIntensity;
    int     u_directionalLightsCnt;
    int     u_sphereLightsCnt;
    int     u_rectLightsCnt;
};

// -------------!!! IMPORTANT !!!------------------ //
// ------------------------------------------------ //
// All the following structures have vec4s          //
// instead of vec3 on the cpp side. std430          //
// still needs padding for vec3.                    //
// Here I left vec3s to avoid unnecessary .xyz      //
// in the shader code.                              //
// ------------------------------------------------ //
struct DirectionalLight
{
    vec3 direction;
    vec3 intensity;
};

struct SphereLight
{
    vec3  position;
    vec3  intensity;
    float radius;
};

struct RectLight
{
    vec3 position;
    vec3 intensity;
    vec3 axisX; // plane's x
    vec3 axisY; // plane's y
    vec3 axisZ; // plane's normal
    vec2 size;
};

#ifdef LIGHT_PASS_ENVIRONMENT
layout(binding = 4) uniform sampler2D   envBrdfLut;
layout(binding = 5) uniform samplerCube envIrradiance;
layout(binding = 6) uniform samplerCube envPrefiltered;

uniform int u_envPrefilteredMinLod;
uniform int u_envPrefilteredMaxLod;
#endif

#ifdef LIGHT_PASS_DIRECTIONAL
layout(binding = 9 ) uniform sampler2D       dirShadowMap;      // TODO: Array
layout(binding = 10) uniform sampler2DShadow dirShadowMapComp;  // TODO: Array
                     uniform mat4            u_dirShadowMatrix; // TODO: Array
                     uniform bool            u_doDirShadow;
layout(std430, binding = 5) buffer buff_directional_lights
{
    DirectionalLight directionalLights[];
};
#endif

#ifdef LIGHT_PASS_SPHERE
layout(std430, binding = 6) buffer buff_sphere_lights
{
    SphereLight sphereLights[];
};
#endif

#ifdef LIGHT_PASS_RECT
const float LTC_LUT_SIZE  = 64.0;
const float LTC_LUT_BIAS  = 0.5/LTC_LUT_SIZE;
const float LTC_LUT_SCALE = ((LTC_LUT_SIZE - 1.0)/LTC_LUT_SIZE);

//! #include "LtcHelper.glsl"

layout(binding = 7) uniform sampler2D ltcLut1;
layout(binding = 8) uniform sampler2D ltcLut2;

layout(std430, binding = 7) buffer buff_rect_lights
{
    RectLight rectLights[];
};
#endif

// Directional Light
// -------------------------------------------------------------------------------------------------------------------------
vec3 DiffuseDirectionalLight(vec3 lightDirection, vec3 surfNormal, vec3 diffuse, float metalness, vec3 lightColor)
{
    float NoL = CLAMPED_DOT(lightDirection, surfNormal);
    vec3 c = diffuse.rgb * lightColor * NoL;
    return  mix(c, vec3(0.0), metalness);
}

vec3 SpecularDirectionalLight(vec3 viewDirection, vec3 lightDirection, vec3 surfNormal,vec3 F0, float roughness, vec3 lightColor)
{
    return  lightColor * CLAMPED_DOT(lightDirection, surfNormal) * SpecularBRDF(viewDirection, lightDirection, surfNormal, F0, roughness);
}

vec3 ComputeDirectionalLight(vec3 viewDirection, vec3 lightDirection, vec3 surfPosition, vec3 surfNormal, vec3 f0, vec3 diffuse, float roughness,float metalness, vec3 lightColor)
{

    if(u_doDirShadow)
    {
        vec4 shadowPos = u_dirShadowMatrix*vec4(surfPosition, 1.0);
        shadowPos.xyz/=shadowPos.w;
        shadowPos.xyz=shadowPos.xyz*0.5+0.5;
        shadowPos.z-=0.01;
        float visibility = texture(dirShadowMapComp, shadowPos.xyz, 0.0).r;
        //float visibility = float(texture(dirShadowMap, shadowPos.xy).r>shadowPos.z);

        lightColor*=visibility;
    }

    vec3 kd = 1.0 - SpecularF(f0, CLAMPED_DOT(surfNormal, viewDirection));
    vec3 directDiffuse  = DiffuseDirectionalLight(lightDirection, surfNormal, diffuse, metalness, lightColor);
    vec3 directSpecular = SpecularDirectionalLight(viewDirection, lightDirection, surfNormal, f0, roughness, lightColor);

    return kd * directDiffuse + directSpecular; // "ks" is already in the GGX brdf
}
vec3 ComputeDirectionalLight(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal, vec3 f0, vec3 diffuse, float roughness,float metalness, DirectionalLight l)
{
    return ComputeDirectionalLight(viewDirection, -l.direction, surfPosition, surfNormal, f0, diffuse, roughness,metalness, l.intensity);
}


// Sphere Light (area)
// -------------------------------------------------------------------------------------------------------------------------
vec3 ComputeSphereLight(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal , vec3 surfDiffuse, float surfRoughness, float surfMetalness, vec3 surfF0, vec3 lightPosition, vec3 lightColor, float lightRadius)
{
    // avoid problems with radius = 0.0
    lightRadius = max(lightRadius, 1e-4);

    // Diffuse
    // --------------------------------------------------
    vec3 kd = 1.0 - SpecularF(surfF0, CLAMPED_DOT(viewDirection, surfNormal));

    float ill = ComputeSphereLightIlluminanceW(surfPosition, surfNormal, lightPosition, lightRadius);
    ill*= 1.0/(4.0*PI*lightRadius*lightRadius);

    vec3 c = surfDiffuse.rgb * lightColor * ill;
    vec3 diffuse =  mix(c, vec3(0.0), surfMetalness);


    // Specular
    // --------------------------------------------------
    // see Karis notes "Representative point method"
    vec3 mrp = RepresentativePointOnSphere(viewDirection, surfPosition, surfNormal, surfRoughness, lightPosition, lightRadius);
    vec3 dominantDir = normalize(mrp -surfPosition);

    float lightDistance = length(surfPosition - lightPosition);
    float a0 = surfRoughness*surfRoughness;
    float a1 = SATURATE(a0 + (lightRadius)/(3.0*lightDistance));
    float a  = a1*a1;

    // Instead of a falloff I'm using the illuminance computed earlier
    vec3 lIn = lightColor*CLAMPED_DOT(surfNormal, dominantDir)*ill;

    vec3 specular = lIn*SpecularBRDF_Area(viewDirection, dominantDir, surfNormal, surfF0, surfRoughness, a);

    return  kd*diffuse + specular;

}

vec3 ComputeSphereLight(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal , vec3 surfDiffuse, float surfRoughness, float surfMetalness, vec3 surfF0, SphereLight l)
{
    return ComputeSphereLight(viewDirection,surfPosition,surfNormal,surfDiffuse,surfRoughness,surfMetalness,surfF0, l.position, l.intensity, l.radius);
}

// Rect Light (area)
// -------------------------------------------------------------------------------------------------------------------------

// from: https://github.com/selfshadow/ltc_code
vec3 ComputeRectLightLTC(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal , vec3 surfDiffuse, float surfRoughness, float surfMetalness, vec3 surfF0, RectLight l)
{
    l.size = max(l.size, vec2(1e-4));
    bool twoSided = false;

    vec3 pos = surfPosition;
    vec3 N = surfNormal;
    vec3 V = viewDirection;

    vec3 points[4];
    LTC_InitRectPoints(l, points);

    float ndotv = SATURATE(dot(N, V));
    vec2 uv = vec2(surfRoughness, sqrt(1.0 - ndotv));
    uv = uv*LTC_LUT_SCALE + LTC_LUT_BIAS;

    vec4 t1 = texture(ltcLut1, uv);
    vec4 t2 = texture(ltcLut2, uv);

    mat3 Minv = mat3(
        vec3(t1.x, 0, t1.y),
        vec3(  0,  1,    0),
        vec3(t1.z, 0, t1.w)
    );

    vec3 spec = LTC_Evaluate(N, V, pos, Minv, points, twoSided, ltcLut2);
    // BRDF shadowing and Fresnel
    spec *= surfF0*t2.x + (1.0 - surfF0)*t2.y;

    vec3 diff = mix(LTC_Evaluate(N, V, pos, mat3(1), points, twoSided, ltcLut2), vec3(0.0), surfMetalness);

    return  (l.intensity/(l.size.x*l.size.y))*(spec + surfDiffuse*diff);
}

// This was not great and is not used anymore....
// See ComputeRectLightLTC instead.
//vec3 ComputeRectLight(vec3 viewDirection, vec3 surfPosition, vec3 surfNormal , vec3 surfDiffuse, float surfRoughness, float surfMetalness, vec3 surfF0, RectLight l)
//{
//    // Diffuse
//    // --------------------------------------------------
//    vec3 kd = 1.0 - SpecularF(surfF0, CLAMPED_DOT(viewDirection, surfNormal));
//
//    if ( dot ( surfPosition - l.position , l.axisZ) <= 0.0) return vec3(0.0);
//
//    // most representative pointS, see Lagarde's presentation:
//    // https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
//    vec3 p0 = l.position - 0.5 * l.size.x * l.axisX + 0.5 * l.size.y * l.axisY;
//    vec3 p1 = l.position - 0.5 * l.size.x * l.axisX - 0.5 * l.size.y * l.axisY;
//    vec3 p2 = l.position + 0.5 * l.size.x * l.axisX - 0.5 * l.size.y * l.axisY;
//    vec3 p3 = l.position + 0.5 * l.size.x * l.axisX + 0.5 * l.size.y * l.axisY;
//    float solidAngle = RectangleSolidAngle (surfPosition , p0 , p1 , p2 , p3 );
//
//    float ill = solidAngle * 0.2 * (
//    SATURATE( dot( normalize ( p0 - surfPosition ) , surfNormal ) )+
//    SATURATE( dot( normalize ( p1 - surfPosition ) , surfNormal ) )+
//    SATURATE( dot( normalize ( p2 - surfPosition ) , surfNormal ) )+
//    SATURATE( dot( normalize ( p3 - surfPosition ) , surfNormal ) )+
//    SATURATE( dot( normalize ( l.position - surfPosition ) , surfNormal )));
//
//
//    ill*= 1.0/(l.size.x*l.size.y);
//
//    vec3 c = surfDiffuse.rgb * l.intensity * ill;
//    vec3 diffuse =  mix(c, vec3(0.0), surfMetalness);
//
//
//    ReprPointRectRes mrp = RepresentativePointOnRect(viewDirection, surfPosition, surfNormal, surfRoughness, l.position, l.axisX, l.axisY, l.axisZ, l.size);
//    vec3 mrd = normalize(mrp.pointClamped-surfPosition);
//
//    float fac = mrp.overlap;
//
//    vec3 specular = fac*ill*l.intensity* CLAMPED_DOT(surfNormal, mrd)* SpecularBRDF(viewDirection, mrd, surfNormal, surfF0, surfRoughness);
//
//    return kd*diffuse + specular;
//}


// Ambient Light
// -------------------------------------------------------------------------------------------------------------------------
vec3 ComputeAmbientLight(vec3 viewDir, vec3 normal, vec3 f0, vec3 diffuse, float roughness, float metalness,
                         float environemtIntensity, float radianceMinLod, float radianceMaxLod,
                         samplerCube irradianceMap, samplerCube prefilteredEnv, sampler2D brdfLut)
{
    // Diffuse
    // -------------
    vec3 ambientD = environemtIntensity * diffuse * texture(irradianceMap, normal.xzy * vec3(1,-1,1)).rgb;
    ambientD = mix(ambientD, vec3(0.0), metalness);

    // Specular
    // -------------
    vec3 reflDir = reflect(-viewDir, normal);
    reflDir = reflDir.xzy * vec3(1,-1,1); // TODO

    float NoV = CLAMPED_DOT(normal, viewDir);
    float lod = radianceMinLod + roughness * (radianceMaxLod - radianceMinLod);
    vec3 irr       = textureLod(prefilteredEnv, normalize(reflDir), lod).rgb;
    vec2 scaleBias = texture(brdfLut, vec2(roughness, NoV)).xy;
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

    if(posWorld!=vec3(0.0))
    {
        // TODO: meh?
        nrmWorld     = normalize(nrmWorld);
        vec3 viewDir = normalize(f_eyeWorldPos.xyz - posWorld);
        vec3 f0      = mix(F0_DIELECTRIC, albedo.rgb, metalness);

#ifdef LIGHT_PASS_DIRECTIONAL
        for(int i=0;i<u_directionalLightsCnt;i++)
            col.rgb += ComputeDirectionalLight(viewDir, posWorld, nrmWorld, f0, albedo.rgb, roughness, metalness, directionalLights[i]);
#endif

#ifdef LIGHT_PASS_SPHERE
        for(int i=0;i<u_sphereLightsCnt;i++)
            col.rgb += ComputeSphereLight(viewDir, posWorld, nrmWorld , albedo.rgb, roughness, metalness, f0, sphereLights[i]);
#endif

#ifdef LIGHT_PASS_RECT
        for(int i=0;i<u_rectLightsCnt;i++)
            col.rgb += ComputeRectLightLTC(viewDir, posWorld, nrmWorld , albedo.rgb, roughness, metalness, f0, rectLights[i]);
#endif

#ifdef LIGHT_PASS_ENVIRONMENT
        if(u_doEnvironment)
            col.rgb += ComputeAmbientLight(viewDir, nrmWorld, f0, albedo.rgb, roughness, metalness, u_environmentIntensity,
                                           u_envPrefilteredMinLod, u_envPrefilteredMaxLod, envIrradiance, envPrefiltered, envBrdfLut);
#endif
    }


    if(f_doGamma)
    {
        col=pow(col,vec4(1.0/f_gamma));
    }

    FragColor =  col;
}