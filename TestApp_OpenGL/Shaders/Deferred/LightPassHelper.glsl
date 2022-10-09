
//? #version 430 core
//? #include "../Include/UboDefs.glsl"
//? #include "../Include/DebugViz.glsl"
//? uniform samplerCube t_IrradianceMap;
//? uniform samplerCube t_RadianceMap; 
//? uniform sampler2D   t_BrdfLut;
//? uniform sampler1D   t_PoissonSamples;
//? uniform sampler2D   t_NoiseTex_0;
//? uniform sampler2D   t_NoiseTex_1;
//? uniform sampler2D   t_NoiseTex_2;
//?uniform sampler2D    t_shadowMap_directional;
//?uniform samplerCube  t_shadowMap_point[3];

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 3
#endif

#define BLOCKER_SEARCH_SAMPLES 16
#define PCF_SAMPLES 64
    

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define F0_DIELECTRIC 0.04

// From: https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
// Compute attenuation for punctual light sources
float SmoothDistanceAtt ( float squaredDistance , float invSqrAttRadius )
{
    float factor = squaredDistance * invSqrAttRadius ;
    float smoothFactor = clamp (1.0 - factor * factor, 0.0, 1.0);
    return smoothFactor * smoothFactor;
}
    
float GetDistanceAtt ( vec3 unormalizedLightVector , float invSqrAttRadius )
{
    float sqrDist = dot ( unormalizedLightVector , unormalizedLightVector );
    float attenuation = 1.0 / (max ( sqrDist , 0.01 * 0.01) );
    attenuation *= SmoothDistanceAtt ( sqrDist , invSqrAttRadius );
    
    return attenuation;
}

// Schlick's approximation
// ---------------------------------------------------
vec3 Fresnel(float cosTheta, vec3 F0)
{
    // F0 + (1 - F0)(1 - (h*v))^5
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
    
// Trowbridge - Reitz GGX normal distribution function
// ---------------------------------------------------
float NDF_GGXTR(vec3 n, vec3 h, float roughness)
{   
    float cosTheta = max(0.0, dot(n, h));
    float a = roughness*roughness;
    float a2 = a*a;
    float d = ((cosTheta*cosTheta) * (a2 - 1.0)) + 1.0;
    d = PI * d*d;
        
    return a2 / d ;
}
   
// Schlick GGX geometry function
// ---------------------------------------------------
float G_GGXS(vec3 n, vec3 v, float roughness)
{   
    float k = (roughness + 1.0);
    k = (k*k) / 8.0;     
    float dot = max(0.0, dot(n, v));

    return dot / (dot * (1.0 - k) + k);
}

    
// Complete geometry function, product of occlusion-probability
// from light and from view directions
// ---------------------------------------------------
float G_GGXS(vec3 n, vec3 l, vec3 v, float roughness)
{
    return G_GGXS(n, v, roughness) * G_GGXS(n, l, roughness);    
}

// Direct Illumination
// -------------------
vec3 ComputeDirectIllumination(vec3 Li, vec3  Wi, vec3 Wo, vec3 surfNormal, float surfRoughness, float surfmetalness, vec3 surfDiffuse)
{
    // Halfway vector
	vec3 h = normalize(Wo - Wi);
    float cosTheta = max(0.0, dot(h, Wo));

    vec3 F = Fresnel(cosTheta, vec3(F0_DIELECTRIC));
    F = mix(F, surfDiffuse, surfmetalness);

    float D = NDF_GGXTR(surfNormal, h, surfRoughness);
    float G = G_GGXS(surfNormal,  -Wi, Wo, surfRoughness);
        
    vec3 ks = F ;
    vec3 kd = 1.0 - ks; 
    
    float NdotL = max(0.0, dot(surfNormal, -Wi));
    float NdotV = max(0.0, dot(Wo, surfNormal));
        
    vec3 diffuse = (kd  * surfDiffuse / PI);
    vec3 reflected = ((F * D * G) / (4.0 * NdotL * NdotV + 0.0001));
        
    return (diffuse + reflected) * NdotL * Li ;
}

// Diffuse indirect 
// ---------------------------------------------------
vec3 ComputeDiffuseIndirectIllumination(vec3 normal, vec3 kd, vec3 albedo, samplerCube irradianceMap)
{
    vec3 irradiance = vec3(1,1,1); // White environment by default

    vec3 d = normalize(normal);
    d = vec3(d.x, -d.z, d.y);
    irradiance = texture(irradianceMap, d).rbg;
   
    return albedo * irradiance* kd;
}

// Specular indirect 
// ---------------------------------------------------
vec3 ComputeSpecularIndirectIllumination(vec3 N, vec3 V,  float roughness, vec3 ks, samplerCube radianceMap, int maxLod, sampler2D brdfLut)
{
    vec3 specular = vec3(1,1,1); // White environment by default

    
    vec3 d = reflect(normalize(V), normalize(N));
    d = vec3(d.x, -d.z, d.y);

    float LoD = mix(float(0), float(maxLod), roughness);
            
    vec3 Li = textureLod(radianceMap, d, LoD).rgb;
    vec2 scaleBias = texture(brdfLut, vec2(clamp(dot(N, V), 0.0, 1.0), roughness)).rg;

    specular = Li * (ks * scaleBias.x + scaleBias.y);

    return specular;
}

vec2 PoissonSample(int index)
{
    return texelFetch(t_PoissonSamples, index, 0).rg;
}
        
float RandomValue()
{
    // Using 3 textures with convenient resolution (like 9x9, 10x10, 11x11) to get a non repeating (almost) noise pattern 
    // across a large region of the window (9x10x11 ^ 2)
    ivec2 noiseSize_0 = textureSize(t_NoiseTex_0, 0);
    ivec2 noiseSize_1 = textureSize(t_NoiseTex_1, 0);
    ivec2 noiseSize_2 = textureSize(t_NoiseTex_2, 0);

    float noise0 = texelFetch(t_NoiseTex_0, ivec2(mod(gl_FragCoord.xy,noiseSize_0.xy)), 0).r;
    float noise1 = texelFetch(t_NoiseTex_1, ivec2(mod(gl_FragCoord.xy,noiseSize_1.xy)), 0).r;
    float noise2 = texelFetch(t_NoiseTex_2, ivec2(mod(gl_FragCoord.xy,noiseSize_2.xy)), 0).r;

    return (noise0 + noise1 + noise2) / 3.0;
}

float RandomValue(vec2 fragCoord)
{
    // Using 3 textures with convenient resolution (like 9x9, 10x10, 11x11) to get a non repeating (almost) noise pattern 
    // across a large region of the window (9x10x11 ^ 2)
    ivec2 noiseSize_0 = textureSize(t_NoiseTex_0, 0);
    ivec2 noiseSize_1 = textureSize(t_NoiseTex_1, 0);
    ivec2 noiseSize_2 = textureSize(t_NoiseTex_2, 0);

    float noise0 = texelFetch(t_NoiseTex_0, ivec2(mod(fragCoord.xy,noiseSize_0.xy)), 0).r;
    float noise1 = texelFetch(t_NoiseTex_1, ivec2(mod(fragCoord.xy,noiseSize_1.xy)), 0).r;
    float noise2 = texelFetch(t_NoiseTex_2, ivec2(mod(fragCoord.xy,noiseSize_2.xy)), 0).r;

    return (noise0 + noise1 + noise2) / 3.0;
}

  
vec2 Rotate2D(vec2 direction, float angle)
{
    float sina = sin(angle);
    float cosa = cos(angle);
    mat2 rotation = mat2 ( cosa, sina, -sina, cosa);
    return rotation * direction;
}
    

float SearchWidth(float uvLightSize, float receiverDistance)
{
	return uvLightSize;
}

float ShadowBias(float angle, float bias, float slopeBias)
{
    return max(bias, slopeBias*(1.0 - cos(angle)));
} 
 
    
float SlopeBiasForPCF(vec3 worldNormal, vec3 worldLightDirection, float sampleOffsetWorld)
{
    float angle = acos(dot(worldNormal, -worldLightDirection));
    float zOffsetWorld = sampleOffsetWorld * tan(clamp(angle, 0.0, PI/2.5)); 
    return zOffsetWorld; 
}

vec2 NdcToUnit(vec2 ndcCoord)
{
    return ndcCoord*0.5+0.5;
}
vec3 NdcToUnit(vec3 ndcCoord)
{
    return ndcCoord*0.5+0.5;
}
            

float FindBlockerDistance_DirectionalLight(
    vec3 fragPosNdc,vec3 fragNormalWorld, sampler2D shadowMap, 
    vec4 shadowFrustumSize /* (r-l, t-b, near, far) */, 
    vec3 lightDirWorld, float lightSizeWorld, 
    float bias /* base bias added to a slope bias computed here  */)
{
	int blockers = 0;
	float avgBlockerDistance = 0;

    float noise = RandomValue(gl_FragCoord.xy) * PI;
	float searchWidth = lightSizeWorld;

	float angle = acos(dot(fragNormalWorld, lightDirWorld));
    float biasFac_ls = SlopeBiasForPCF(fragNormalWorld, lightDirWorld, 1);

    for(int i = 0 ; i< BLOCKER_SEARCH_SAMPLES; i++)
    {
        vec2 sampleOffset_ls  = PoissonSample(i);
        sampleOffset_ls = Rotate2D(sampleOffset_ls, noise);
        sampleOffset_ls *= searchWidth;

        vec2 sampleOffset_tex = sampleOffset_ls / shadowFrustumSize.xy;
        vec2 offsetCoord = NdcToUnit(fragPosNdc.xy) + sampleOffset_tex;
            
		float z=texture(shadowMap, offsetCoord).r;

        float bias_ls = bias
                        + biasFac_ls * length(sampleOffset_ls);
            
        float bias_tex = bias_ls / (shadowFrustumSize.w - shadowFrustumSize.z);

        if (z + bias_tex <= NdcToUnit(fragPosNdc).z )
        {
            blockers++;
            avgBlockerDistance += NdcToUnit(fragPosNdc).z - z; 
        }

    }

#ifdef DEBUG_VIZ 
    if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy&& uDebug_viewport.z == DEBUG_DIRECTIONAL_LIGHT)
    {

        vec4 center_ndc = vec4(fragPosNdc, 1.0);
        vec4 center_ws = f_lightSpaceMatrixInv_directional*center_ndc;
                
        vec3 coneDir_ws =  -lightDirWorld * SlopeBiasForPCF(fragNormalWorld, lightDirWorld, lightSizeWorld);

        DViz_DrawCone((center_ws.xyz/center_ws.w) + coneDir_ws, -coneDir_ws, lightSizeWorld, vec4(0.0, 1.0, 0.0, 0.6));

    }
#endif

	float avgDist = blockers > 0
                        ? avgBlockerDistance / blockers
                        : -1;
		return avgDist;
}

float PCF_DirectionalLight(
    vec3 fragPosNdc,vec3 fragNormalWorld, sampler2D shadowMap, 
    vec4 shadowFrustumSize /* (r-l, t-b, near, far) */, 
    vec3 lightDirWorld, float filterSizeWorld, 
    float bias /* base bias added to a slope bias computed here  */)
{    
    float noise = RandomValue(gl_FragCoord.xy);     
    float biasFac_ls = SlopeBiasForPCF(fragNormalWorld,lightDirWorld, 1);

    float sum = 0;
    for (int i = 0; i < PCF_SAMPLES; i++)
    {
            
        vec2 sampleOffset_ls  = PoissonSample(i);
        sampleOffset_ls = Rotate2D(sampleOffset_ls, noise);
        sampleOffset_ls *= filterSizeWorld;

        vec2 sampleOffset_tex = sampleOffset_ls / shadowFrustumSize.xy;
        vec2 offsetCoord = NdcToUnit(fragPosNdc.xy) + sampleOffset_tex;
            
		float closestDepth=texture(shadowMap, offsetCoord).r;

        float bias_ls = bias
                        + biasFac_ls * length(sampleOffset_ls);
            
        float bias_tex = bias_ls / (shadowFrustumSize.w - shadowFrustumSize.z);

          
        sum+= 
            (closestDepth + bias_tex) <= NdcToUnit(fragPosNdc).z 
            ? 0.0 
            : 1.0;

    }

#ifdef DEBUG_VIZ 
    if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy && uDebug_viewport.z == DEBUG_DIRECTIONAL_LIGHT)
    {

        vec4 center_ndc = vec4(fragPosNdc, 1.0);
        vec4 center_ws = f_lightSpaceMatrixInv_directional*center_ndc;
                
        vec3 coneDir_ws =  -lightDirWorld * SlopeBiasForPCF(fragNormalWorld, lightDirWorld, filterSizeWorld);

        DViz_DrawCone((center_ws.xyz/center_ws.w) + coneDir_ws, -coneDir_ws, filterSizeWorld, vec4(0.0, 1.0, 1.0, 0.6));

    }
#endif

	    return sum / PCF_SAMPLES;
    }

float PCSS_DirectionalLight(
    vec3 fragPosNdc,vec3 fragNormalWorld, sampler2D shadowMap, 
    vec4 shadowFrustumSize /* (r-l, t-b, near, far) */, 
    vec3 lightDirWorld, float softness, 
    float bias /* base bias added to a slope bias computed here  */)
{

    float lightSize_ls = softness*
            NdcToUnit(fragPosNdc).z * (shadowFrustumSize.w - shadowFrustumSize.z); 

	// blocker search
	float blockerDistance = 
        FindBlockerDistance_DirectionalLight(
            fragPosNdc, fragNormalWorld, shadowMap, 
            shadowFrustumSize , 
            lightDirWorld, lightSize_ls, 
            bias);

	if (blockerDistance == -1)
		return 1.0;

    float blockerDistance_ls = blockerDistance*(shadowFrustumSize.w - shadowFrustumSize.z);
	float pcfRadius_ls = softness*(abs(blockerDistance_ls)); 
    pcfRadius_ls = min(pcfRadius_ls, lightSize_ls );

    // filtering
	return PCF_DirectionalLight(
        fragPosNdc, fragNormalWorld, shadowMap, 
        shadowFrustumSize , 
        lightDirWorld, pcfRadius_ls, 
        bias);
       
}

float ComputeDirectionalShadow(vec4 fragPosLightSpace, vec3 fragNormalWorld)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    return PCSS_DirectionalLight(
        projCoords, fragNormalWorld, 
            t_shadowMap_directional, f_shadowCubeSize_directional,f_dirLight.Direction.xyz, 
            f_dirLight.Data.x, f_dirLight.Data.y);
}

float PCSS_PointLight(
    vec3 fragPosWorld,vec3 fragNormalWorld, samplerCube shadowMap, 
    vec3 lightPosWorld, float radius, float size, 
    float bias /* base bias added to a slope bias computed here  */)
{
   
    float far = radius;    
    float noise = RandomValue(gl_FragCoord.xy);     
    vec3 worldNormal_normalized = fragNormalWorld;
    vec3 lightDir_nrm = lightPosWorld - fragPosWorld;
    float lightDirLen = length(lightDir_nrm);
    lightDir_nrm/=lightDirLen;
    
    // Creating a reference frame with light direction as Z axis
    vec3 N = lightDir_nrm;
    vec3 T = normalize(cross(N, vec3(1.453,1.029,1.567) /* LOL */));
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
        
    // Cone-shaped bias (see debugViz), compute once (linear)
    float biasFac_ws = SlopeBiasForPCF(worldNormal_normalized, -lightDir_nrm, 1.0);

    // Blocker search
    float blkSrcRadius_ws = size * (lightDirLen - size) /lightDirLen; //mmhh...

    float avgBlkDst = 0;
    int blockers = 0;
    for (int i = 0; i < BLOCKER_SEARCH_SAMPLES; i++)
    {
        vec3 sampleOffset_ws  = TBN * vec3(Rotate2D(PoissonSample(i), noise), 0.0);
        sampleOffset_ws *= blkSrcRadius_ws;
        vec3 samplePosition_ws = fragPosWorld + sampleOffset_ws;
            
        vec3 lightDirFromSmp = lightPosWorld - samplePosition_ws;
        vec3 lightDirFromSmp_nrm = normalize(lightDirFromSmp);
        float angle = acos(dot(worldNormal_normalized, lightDirFromSmp_nrm));
        float bias_ws = bias + biasFac_ws * length(sampleOffset_ws);          
        float closestPoint = texture(shadowMap, -lightDirFromSmp_nrm).r * far;
  
        if ((closestPoint + bias_ws) <= length(lightDirFromSmp))
        {
            blockers++;
            avgBlkDst += lightDirLen - closestPoint*dot(lightDirFromSmp_nrm, lightDir_nrm); 
        }     
    }

#ifdef DEBUG_VIZ 

        if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy && uDebug_viewport.z == DEBUG_POINT_LIGHT)
        {
            vec3 coneDir_ws =  lightDir_nrm * biasFac_ws * blkSrcRadius_ws;
            DViz_DrawCone(fragPosWorld + coneDir_ws, -coneDir_ws, blkSrcRadius_ws, vec4(0.0, 1.0, 0.0, 0.6));
        }
#endif

    // Fully lit 
    if(blockers == 0) return 1.0;

    avgBlkDst/=blockers;

    // Percentage closer filtering
    float filterRadiusWorld = size * avgBlkDst/ (lightDirLen - avgBlkDst);
    filterRadiusWorld = min(size, filterRadiusWorld); //mmmhh....
    float pcfSum = 0;
    for (int i = 0; i < PCF_SAMPLES; i++)
    {
        vec3 sampleOffset_ws  = TBN * vec3(Rotate2D(PoissonSample(i), noise), 0.0);
        sampleOffset_ws *= filterRadiusWorld;
        vec3 samplePosition_ws = fragPosWorld + sampleOffset_ws;
            
        vec3 lightDirFromSmp = lightPosWorld - samplePosition_ws;
        vec3 lightDirFromSmp_nrm = normalize(lightDirFromSmp);
        float angle = acos(dot(worldNormal_normalized, lightDirFromSmp_nrm));
        float bias_ws = bias + biasFac_ws * length(sampleOffset_ws);          
        float closestPoint = texture(shadowMap, -lightDirFromSmp_nrm).r * far;
  
        pcfSum+=   
            (closestPoint + bias_ws) <= length(lightDirFromSmp)
            ? 0.0 
            : 1.0;
    }

#ifdef DEBUG_VIZ 

        if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy && uDebug_viewport.z == DEBUG_POINT_LIGHT)
        {
            vec3 coneDir_ws =  lightDir_nrm * biasFac_ws * filterRadiusWorld;
            DViz_DrawCone(fragPosWorld + coneDir_ws, -coneDir_ws, filterRadiusWorld, vec4(0.0, 1.0, 1.0, 0.6));
        }
#endif


	return pcfSum / PCF_SAMPLES;
}


float ComputePointShadow(int index, vec3 fragPosWorld, vec3 fragNormalWorld)
{
    if(f_pointLight[index].Color.w == 0.0) return 1.0;

    return PCSS_PointLight(
        fragPosWorld, fragNormalWorld, t_shadowMap_point[index],
        f_pointLight[index].Position.xyz, f_pointLight[index].Radius, 
        f_pointLight[index].Size, f_pointLight[index].Bias);
}

void ComputeLighting(
    vec3 eyePosWorld,
    vec3 fragPosWorld,
    vec3 normalWorld,
    vec3 emission,
    vec3 albedo,
    float roughness,
    float metalness,
    float dirLighShadow,
    float[MAX_POINT_LIGHTS] pointLightShadow,

    out vec3 direct,
    out vec3 ambient
)
{
    vec3 viewDir=normalize(eyePosWorld - fragPosWorld);

    // Incoming light
    vec3 Li = vec3(0);

    // Directional light
    Li = f_dirLight.Diffuse.rgb *  f_dirLight.Diffuse.a * dirLighShadow;
    direct = ComputeDirectIllumination(Li, normalize(f_dirLight.Direction.xyz), viewDir, normalWorld, roughness, metalness, albedo);

    // Point lights      
    for(int i=0; i<MAX_POINT_LIGHTS; i++)
    {
        vec3 lightVec = fragPosWorld - f_pointLight[i].Position.xyz;
        Li = f_pointLight[i].Color.rgb *  f_pointLight[i].Color.a * pointLightShadow[i] * GetDistanceAtt(lightVec, f_pointLight[i].InvSqrRadius);
        direct += ComputeDirectIllumination(Li, normalize(lightVec), viewDir, normalWorld, roughness, metalness, albedo);
    }

    // Ambient light
    vec3 indirectFs = FresnelRoughness(max(0.0, dot(normalWorld, viewDir)), vec3(F0_DIELECTRIC), roughness);
    indirectFs = mix(indirectFs, albedo, metalness);
    vec3 indirectFd = 1.0 - indirectFs;	
    ambient = 

            (f_hasIrradianceMap && f_hasBrdfLut)
                ? ComputeDiffuseIndirectIllumination(normalWorld, indirectFd, albedo, t_IrradianceMap) +
                  ComputeSpecularIndirectIllumination(normalWorld, viewDir, roughness, indirectFs, t_RadianceMap, f_radiance_maxLevel, t_BrdfLut)
                
                // White ambient as default
                : vec3(1.0);

    ambient*=f_environmentIntensity;
}