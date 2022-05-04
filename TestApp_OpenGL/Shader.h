#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <optional>
#include <map>
#include "SceneUtils.h"
#include "FrameBuffer.h"
#include "GeometryHelper.h"
#include "stb_image.h"

namespace VertexSource_Geometry
{
    const std::string EXP_VERTEX =
        R"(
    #version 330 core

    layout(location = 0) in vec3 position;
    layout(location = 1) in vec3 normal;
    layout(location = 2) in vec2 textureCoordinates;
    layout(location = 3) in vec3 tangent;
    layout(location = 4) in vec3 bitangent;

    layout (std140) uniform blk_PerFrameData
    {
        uniform mat4 view;              // 64 byte
        uniform mat4 proj;              // 64 byte
        uniform float near;             // 4 byte
        uniform float far;              // 4 byte
                                        // TOTAL => 136 byte
    };

    uniform mat4 model;             
    uniform mat4 normalMatrix;      

    out VS_OUT
    {

        vec3 worldNormal;
        vec3 fragPosWorld;
        vec2 textureCoordinates;
        mat3 TBN;
    }vs_out;

    //[DEFS_SHADOWS]
    void main()
    {
        vs_out.fragPosWorld = (model * vec4(position.x, position.y, position.z, 1.0)).xyz;
        gl_Position = proj * view * model * vec4(position.x, position.y, position.z, 1.0);
        vs_out.worldNormal = normalize((normalMatrix * vec4(normal, 0.0f)).xyz);

        vs_out.textureCoordinates = textureCoordinates;

        // *** TBN for normal mapping *** //
        // ----------------------------- //
        vec3 T = normalize(vec3(normalMatrix * vec4(tangent,   0.0)));
        vec3 N = normalize(vec3(normalMatrix * vec4(normal, 0.0)));

        // re-orthogonalize T with respect to N
        T = normalize(T - dot(T, N) * N);

        // then retrieve perpendicular vector B with the cross product of T and N
        vec3 B = cross(N, T);

        vs_out.TBN = mat3(T, B, N);        
        // ----------------------------- //

        //[CALC_SHADOWS]
    }
    )";

    const std::string DEFS_SHADOWS =
        R"(
    out vec4 posLightSpace;

layout (std140) uniform blk_PerFrameData_Shadows
    {
        uniform mat4 LightSpaceMatrix;          // 64 byte
        uniform float bias;                     // 4 byte
        uniform float slopeBias;                // 4 byte
        uniform float softness;                 // 4 byte
        uniform float shadowMapToWorldFactor;   // 4 byte
                                                // => 80 byte
    };
)";

    const std::string CALC_SHADOWS =
        R"(
    posLightSpace = LightSpaceMatrix * model * vec4(position.x, position.y, position.z, 1.0);
)";

    static std::map<std::string, std::string> Expansions = {

        { "DEFS_SHADOWS",  VertexSource_Geometry::DEFS_SHADOWS     },
        { "CALC_SHADOWS",  VertexSource_Geometry::CALC_SHADOWS     },
    };

    std::string Expand(std::string source, std::vector<std::string> expStrings)
    {
        std::string result = source;

        for (int i = 0; i < expStrings.size(); i++)
        {
            std::string token = "//[" + expStrings[i] + "]";
            size_t start_pos = result.find(token);
            std::string expansion = VertexSource_Geometry::Expansions[expStrings[i]];
            result.replace(start_pos, token.length(), expansion);
        }

        return result;
    }
}

namespace FragmentSource_Geometry
{
    
    const std::string DEFS_MATERIAL_OLD =
        R"(
    struct Material {
        vec4 Diffuse;
        vec4 Specular;
        float Shininess;
        bool hasAlbedo;
        bool hasNormals;
    };

    uniform Material material;
    uniform sampler2D Albedo;
    uniform sampler2D Normals;

    uniform bool u_doGammaCorrection;
    uniform float u_gamma;
)";

    const std::string DEFS_MATERIAL =
        R"(
    struct Material {
        vec4 Albedo;
        float Roughness;
        float Metallic;
       
        bool hasAlbedo           ;
        bool hasNormals          ;
        bool hasRoughness        ;
        bool hasMetallic         ;
        bool hasAmbientOcclusion ;
    };

    uniform Material material;

    uniform sampler2D Albedo;
    uniform sampler2D Normals;
    uniform sampler2D Metallic;
    uniform sampler2D Roughness;
    uniform sampler2D AmbientOcclusion;
    
    #ifndef PI
    #define PI 3.14159265358979323846
    #endif

    #define F0_DIELECTRIC 0.04

    // Schlick's approximation
    // ---------------------------------------------------
    vec3 Fresnel(float cosTheta, vec3 F0)
    {
        // F0 + (1 - F0)(1 - (h*v))^5
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }
    // ---------------------------------------------------

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
    // ---------------------------------------------------

    // Schlick GGX geometry function
    // ---------------------------------------------------
    float G_GGXS(vec3 n, vec3 v, float roughness)
    {   
        float k = (roughness + 1.0);
        k = (k*k) / 8.0;     
        float dot = max(0.0, dot(n, v));

        return dot / (dot * (1.0 - k) + k);
    }
    // ---------------------------------------------------

    // Complete geometry function, product of occlusion-probability
    // from light and from view directions
    // ---------------------------------------------------
    float G_GGXS(vec3 n, vec3 l, vec3 v, float roughness)
    {
       return G_GGXS(n, v, roughness) * G_GGXS(n, l, roughness);    
    }
    // ---------------------------------------------------

)";
    const std::string DEFS_SSAO =
        R"(

    uniform sampler2D aoMap;      
    
    layout (std140) uniform blk_PerFrameData_Ao
    {
        uniform float aoStrength;       // 4 byte
                                        // => 4 byte
    };
)";

    const std::string DEFS_SHADOWS =
        R"(

    #define BLOCKER_SEARCH_SAMPLES 8
    #define PCF_SAMPLES 8
    #define PI 3.14159265358979323846
    
    in vec4 posLightSpace;

    uniform sampler2D shadowMap;
  
    uniform sampler2D noiseTex_0;
    uniform sampler2D noiseTex_1;
    uniform sampler2D noiseTex_2;    

    layout (std140) uniform blk_PerFrameData
    {
        uniform mat4 view;              // 64 byte
        uniform mat4 proj;              // 64 byte
        uniform float near;             // 4 byte
        uniform float far;              // 4 byte
                                        // TOTAL => 136 byte
    };

    layout (std140) uniform blk_PerFrameData_Shadows
    {
        uniform mat4 LightSpaceMatrix;          // 64 byte
        uniform float bias;                     // 4 byte
        uniform float slopeBias;                // 4 byte
        uniform float softness;                 // 4 byte
        uniform float shadowMapToWorldFactor;   // 4 byte
                                                // => 80 byte
    };

     vec2 poissonDisk[16] = vec2[](
     vec2( -0.94201624, -0.39906216 ),
     vec2( 0.94558609, -0.76890725 ),
     vec2( -0.094184101, -0.92938870 ),
     vec2( 0.34495938, 0.29387760 ),
     vec2( -0.91588581, 0.45771432 ),
     vec2( -0.81544232, -0.87912464 ),
     vec2( -0.38277543, 0.27676845 ),
     vec2( 0.97484398, 0.75648379 ),
     vec2( 0.44323325, -0.97511554 ),
     vec2( 0.53742981, -0.47373420 ),
     vec2( -0.26496911, -0.41893023 ),
     vec2( 0.79197514, 0.19090188 ),
     vec2( -0.24188840, 0.99706507 ),
     vec2( -0.81409955, 0.91437590 ),
     vec2( 0.19984126, 0.78641367 ),
     vec2( 0.14383161, -0.14100790 )
    ); 

    vec2 RandomDirection(int i)
    {
        return poissonDisk[i];
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
	    return uvLightSize ;// / receiverDistance;//(receiverDistance - near) / receiverDistance;
    }

     float ShadowBias(float angle)
    {
        return max(bias, slopeBias*(1.0 - cos(angle)));
    } 
   
    float SlopeBiasForMultisampling(float angle, float offsetMagnitude, float screenToWorldFactor)
    {
        return    offsetMagnitude * screenToWorldFactor * tan(angle);
    }

    float FindBlockerDistance_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvLightSize)
    {
	    int blockers = 0;
	    float avgBlockerDistance = 0;
        float dAngle = 2.0*PI / BLOCKER_SEARCH_SAMPLES;

        // Using 3 textures with convenient resolution (like 9x9, 10x10, 11x11) to get a non repeating (almost) noise pattern 
        // across a large region of the window (9x10x11 ^ 2)
        ivec2 noiseSize_0 = textureSize(noiseTex_0, 0);
        ivec2 noiseSize_1 = textureSize(noiseTex_1, 0);
        ivec2 noiseSize_2 = textureSize(noiseTex_2, 0);

        float noiseVal_0 = texelFetch(noiseTex_0, ivec2(mod(gl_FragCoord.xy,noiseSize_0.xy)), 0).r;
        float noiseVal_1 = texelFetch(noiseTex_1, ivec2(mod(gl_FragCoord.xy,noiseSize_1.xy)), 0).r;
        float noiseVal_2 = texelFetch(noiseTex_2, ivec2(mod(gl_FragCoord.xy,noiseSize_2.xy)), 0).r;

        // Needed to avoid obvious banding artifacts
        float noise = (noiseVal_0 + noiseVal_1 + noiseVal_2) * 2.0*PI;

	    float searchWidth = SearchWidth(uvLightSize, shadowCoords.z);
	    //for (int i = 0; i < BLOCKER_SEARCH_SAMPLES; i++)
	    //{
        //    float z=texture(shadowMap, shadowCoords.xy + RandomDirection(i) * searchWidth).r;
        //    
        //    vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
        //    float angle = acos(abs(dot(worldNormal_normalized, lights.Directional.Direction)));
		//    float bias =  ShadowBias(angle) + SlopeBiasForMultisampling(angle, length(RandomDirection(i)) * searchWidth, shadowMapToWorldFactor);
        //    if (z + bias < shadowCoords.z )
		//    {
		//      blockers++;
		//	    avgBlockerDistance += shadowCoords.z - z; 
		//	  }
		//    
	    //}

        int roundedNumSamples = (BLOCKER_SEARCH_SAMPLES/2) * 2;
        for (int i = -roundedNumSamples/2; i < roundedNumSamples/2; i++)
	    {
            for (int j = -roundedNumSamples/2; j < roundedNumSamples/2; j++)
	        {
        
		        float z=texture(shadowMap, shadowCoords.xy + vec2(i,j) * searchWidth).r;
        
                vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
                float angle = acos(abs(dot(worldNormal_normalized, lights.Directional.Direction)));
                float bias =  ShadowBias(angle) + SlopeBiasForMultisampling(angle, length(vec2(i, j)) * searchWidth, shadowMapToWorldFactor);
                if (z + bias < shadowCoords.z )
                {
                  blockers++;
                  avgBlockerDistance += shadowCoords.z - z; 
                }
                
            }
	    }
	    if (blockers > 0)
		    return avgBlockerDistance / blockers;
        
	    else
		    return -1;

        //if ((avgBlockerDistance / blockers) + max(bias, slopeBias*(1-abs(dot(fs_in.worldNormal, lights.Directional.Direction)))) < shadowCoords.z )
        //    return avgBlockerDistance / blockers;
    }

    float PCF_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvRadius)
    {
        float dAngle = 2.0*PI / PCF_SAMPLES;     

	    float sum = 0;

        //for (int i = 0; i < PCF_SAMPLES; i++)
        //{
        //    float closestDepth=texture(shadowMap, shadowCoords.xy + RandomDirection(i) * uvRadius).r;
        //    vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
        //    float angle = acos(abs(dot(worldNormal_normalized, lights.Directional.Direction)));
        //    sum+= (closestDepth + ShadowBias(angle) + SlopeBiasForMultisampling(angle, length(RandomDirection(i)) * uvRadius, shadowMapToWorldFactor)) < shadowCoords.z 
        //            ? 0.0 : 1.0;
        //}
        
        //return sum / (PCF_SAMPLES);

        int roundedNumSamples = (PCF_SAMPLES/2) * 2;
	    for (int i = -roundedNumSamples/2; i < roundedNumSamples/2; i++)
	    {
            for (int j = -roundedNumSamples/2; j < roundedNumSamples/2; j++)
	        {
        
		        float closestDepth=texture(shadowMap, shadowCoords.xy + vec2(i,j) * uvRadius).r;
                vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
                float angle = acos(abs(dot(worldNormal_normalized, lights.Directional.Direction)));
                sum+= (closestDepth + ShadowBias(angle) + SlopeBiasForMultisampling(angle, length(vec2(i, j)) * uvRadius, shadowMapToWorldFactor)) < shadowCoords.z 
                        ? 0.0 : 1.0;
            }
	    }
	    return sum / (roundedNumSamples*roundedNumSamples);
    }

    float PCSS_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvLightSize)
    {
	    // blocker search
	    float blockerDistance = FindBlockerDistance_DirectionalLight(shadowCoords, shadowMap, uvLightSize);
	    if (blockerDistance == -1)
		    return 1.0;		

	    // penumbra estimation
	    float penumbraWidth =  uvLightSize ; //(shadowCoords.z - blockerDistance) / blockerDistance;

	    // percentage-close filtering
	    float uvRadius = penumbraWidth * (blockerDistance / shadowCoords.z);// penumbraWidth * uvLightSize * near / shadowCoords.z;
	    return PCF_DirectionalLight(shadowCoords, shadowMap, uvRadius);
        //return (blockerDistance / shadowCoords.z);
    }

    float ShadowCalculation(vec4 fragPosLightSpace)
    {
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
        projCoords=projCoords*0.5 + vec3(0.5, 0.5, 0.5);

        return PCSS_DirectionalLight(projCoords, shadowMap, softness);
        //return PCF_DirectionalLight(projCoords, shadowMap, softness);
    }
    
)";

    const std::string DEFS_LIGHTS_OLD =
        R"(
    struct DirectionalLight
    {
	    vec3 Direction; // 16 byte
	    vec4 Diffuse;   // 16 byte
	    vec4 Specular;  // 16 byte
                        // => 48 byte
    };

    struct SceneLights
    {
        DirectionalLight Directional;    // 48 byte
	    vec4 Ambient;                    // 16 byte
                                         // => 64 byte      
    };

    struct CommonLightData
    {
	    vec4 baseColor;
	    vec4 baseSpecular;
	    vec3 eyeDir;
	    vec3 eyePos;
	    vec3 worldNormal;
    };

    layout(std140) uniform blk_PerFrameData_Lights
    {
        uniform SceneLights lights;     // 64 byte
        uniform vec3 eyeWorldPos;       // 16 byte
                                        // => 80 byte
    };

    vec4 computeLight_Directional(DirectionalLight d, CommonLightData data)
    {
	    float diffuseIntensity=max(0.0f, dot(-d.Direction, data.worldNormal));
	    vec4 diffuseColor=vec4(d.Diffuse.rgb*d.Diffuse.a, 1.0f)*data.baseColor*diffuseIntensity;
	    float specularIntensity=pow(max(0.0f, dot(data.eyeDir, reflect(normalize(d.Direction), data.worldNormal))), material.Shininess);
	    vec4 specularColor=vec4(d.Specular.rgb*d.Specular.a, 1.0f)*data.baseSpecular*specularIntensity;
	    return  diffuseColor+specularColor;
    }

    vec4 computeLight_Ambient(vec4 a, CommonLightData data)
    {
	    vec4 ambientColor=vec4(a.rgb*a.a, 1.0f)*data.baseColor;
	    return ambientColor;
    }
)";

    const std::string DEFS_LIGHTS =
        R"(
    struct DirectionalLight
    {
	    vec3 Direction; // 16 byte
	    vec4 Diffuse;   // 16 byte
	    vec4 Specular;  // 16 byte
                        // => 48 byte
    };

    struct SceneLights
    {
        DirectionalLight Directional;    // 48 byte
	    vec4 Ambient;                    // 16 byte
                                         // => 64 byte      
    };

   
    layout(std140) uniform blk_PerFrameData_Lights
    {
        uniform SceneLights lights;     // 64 byte
        uniform vec3 eyeWorldPos;       // 16 byte
                                        // => 80 byte
    };

)";

    const std::string CALC_LIT_MAT_OLD =
        R"(
        vec4 baseColor = 
            material.hasAlbedo 
            ? vec4(texture(Albedo, fs_in.textureCoordinates).rgb, 1.0)
            : vec4(material.Diffuse.rgb, 1.0);

        if(u_doGammaCorrection && material.hasAlbedo)
            baseColor = vec4(pow(baseColor.rgb, vec3(u_gamma)), baseColor.a);        

        vec4 baseSpecular=vec4(material.Specular.rgb, 1.0);
        vec3 viewDir=normalize(eyeWorldPos - fs_in.fragPosWorld);

        vec3 worldNormal = 
            material.hasNormals
            ? fs_in.TBN * (texture(Normals, fs_in.textureCoordinates).rgb*2.0-1.0)
            : fs_in.worldNormal;
        
	    CommonLightData commonData=CommonLightData(baseColor, baseSpecular, viewDir, eyeWorldPos, normalize(worldNormal));

	    // Ambient
	    ambient=computeLight_Ambient(lights.Ambient, commonData);

	    // Directional
	    directional=computeLight_Directional(lights.Directional, commonData);
)";

    const std::string CALC_LIT_MAT =
        R"(
        vec4 diffuseColor = 
            material.hasAlbedo 
            ? vec4(texture(Albedo, fs_in.textureCoordinates).rgb, 1.0)
            : vec4(material.Albedo.rgb, 1.0);

        float roughness = 
            material.hasRoughness
            ? texture(Roughness, fs_in.textureCoordinates).r
            : material.Roughness;

        float metallic = 
            material.hasMetallic
            ? texture(Metallic, fs_in.textureCoordinates).r
            : material.Metallic;

        vec3 viewDir=normalize(eyeWorldPos - fs_in.fragPosWorld);

        vec3 worldNormal = 
            material.hasNormals
            ? normalize(fs_in.TBN * (texture(Normals, fs_in.textureCoordinates).rgb*2.0-1.0))
            : normalize (fs_in.worldNormal);
        
        // Halfway vector
	    vec3 h = normalize(viewDir - normalize(lights.Directional.Direction));
        float cosTheta = max(0.0, dot(h, viewDir));

        vec3 F = Fresnel(cosTheta, vec3(F0_DIELECTRIC));
        F = mix(F, diffuseColor.rgb, metallic);

        float D = NDF_GGXTR(worldNormal, h, roughness);
        float G = G_GGXS(worldNormal,  normalize(-lights.Directional.Direction), viewDir, roughness);
        
        vec3 ks = F ;
        vec3 kd = 1.0 - ks; 
    
        float NdotL = max(0.0, dot(worldNormal, normalize(-lights.Directional.Direction)));
        float NdotV = max(0.0, dot(viewDir, worldNormal));

        vec3 Li = lights.Directional.Diffuse.rgb *  lights.Directional.Diffuse.a;
        
        vec3 diffuse = (kd  * diffuseColor.rgb / PI);
        vec3 reflected = ((F * D * G) / (4.0 * NdotL * NdotV + 0.0001));
        
        finalColor = vec4( (diffuse + reflected) * NdotL * Li , 1.0);
        
)";

    const std::string CALC_UNLIT_MAT_OLD =
        R"(
        vec4 baseColor=vec4(material.Diffuse.rgb, 1.0);
        finalColor=baseColor;
)";
    const std::string CALC_UNLIT_MAT =
        R"(
        vec4 baseColor=vec4(material.Albedo.rgb, 1.0);
        finalColor=baseColor;
)";

    const std::string DEFS_NORMALS =
        R"(
         layout (std140) uniform blk_PerFrameData
         {
             uniform mat4 view;              // 64 byte
             uniform mat4 proj;              // 64 byte
             uniform float near;             // 4 byte
             uniform float far;              // 4 byte
                                             // TOTAL => 136 byte
         };
)";

    const std::string CALC_NORMALS =
        R"(
        //vec4 baseColor=vec4(normalize((view * vec4(fs_in.worldNormal, 0.0)).xyz), 1.0);
        vec4 baseColor=vec4((view * vec4(fs_in.fragPosWorld, 1.0)).xyz, 1.0);
        finalColor=baseColor;
)";

    const std::string CALC_SHADOWS =
        R"(
        directional*= ShadowCalculation(posLightSpace);
)";

    const std::string CALC_SSAO =
        R"(
        vec2 aoMapSize=textureSize(aoMap, 0);
        float aoFac = (1.0-texture(aoMap, gl_FragCoord.xy/aoMapSize).r) * aoStrength;
        ambient*= 1.0-aoFac;
)";

    const std::string EXP_FRAGMENT =
    R"(
    #version 330 core

    in VS_OUT
    {

        vec3 worldNormal;
        vec3 fragPosWorld;
        vec2 textureCoordinates;
        mat3 TBN;
    }fs_in;

    out vec4 FragColor;
    
    //[DEFS_MATERIAL] 
    //[DEFS_LIGHTS]
    //[DEFS_SHADOWS]
    //[DEFS_NORMALS]
    //[DEFS_SSAO]
    void main()
    {
        vec4 finalColor=vec4(0.0f, 0.0f, 0.0f, 0.0f);
        vec4 ambient=vec4(0.0f, 0.0f, 0.0f, 0.0f);
        vec4 directional=vec4(0.0f, 0.0f, 0.0f, 0.0f);

        //[CALC_LIT_MAT]
        //[CALC_UNLIT_MAT]	
        //[CALC_SHADOWS]
        //[CALC_NORMALS]
        //[CALC_SSAO]

        FragColor = finalColor + ambient + directional;
    }
    )";

    static std::map<std::string, std::string> Expansions = {

        { "DEFS_MATERIAL",  FragmentSource_Geometry::DEFS_MATERIAL     },
        { "DEFS_LIGHTS",    FragmentSource_Geometry::DEFS_LIGHTS       },
        { "DEFS_SHADOWS",   FragmentSource_Geometry::DEFS_SHADOWS      },
        { "DEFS_SSAO",      FragmentSource_Geometry::DEFS_SSAO         },
        { "CALC_LIT_MAT",   FragmentSource_Geometry::CALC_LIT_MAT      },
        { "CALC_UNLIT_MAT", FragmentSource_Geometry::CALC_UNLIT_MAT    },
        { "CALC_SHADOWS",   FragmentSource_Geometry::CALC_SHADOWS      },
        { "CALC_SSAO",      FragmentSource_Geometry::CALC_SSAO         },
        { "DEFS_NORMALS",   FragmentSource_Geometry::DEFS_NORMALS      },
        { "CALC_NORMALS",   FragmentSource_Geometry::CALC_NORMALS      }
    };

    std::string Expand(std::string source, std::vector<std::string> expStrings)
    {
        std::string result = source;

        for (int i = 0; i < expStrings.size(); i++)
        {
            std::string token = "//[" + expStrings[i] + "]";
            size_t start_pos = result.find(token);
            std::string expansion = FragmentSource_Geometry::Expansions[expStrings[i]];
            result.replace(start_pos, token.length(), expansion);
        }

        return result;
    }
}

namespace VertexSource_PostProcessing
{
    const std::string EXP_VERTEX =
        R"(
    #version 330 core
    layout(location = 0) in vec3 position;
    
    void main()
    {
        gl_Position = vec4(position.xyz, 1.0);
    }
    )";

    static std::map<std::string, std::string> Expansions = {


    };

    std::string Expand(std::string source, std::vector<std::string> expStrings)
    {
        std::string result = source;

        for (int i = 0; i < expStrings.size(); i++)
        {
            std::string token = "//[" + expStrings[i] + "]";
            size_t start_pos = result.find(token);
            std::string expansion = VertexSource_PostProcessing::Expansions[expStrings[i]];
            result.replace(start_pos, token.length(), expansion);
        }

        return result;
    }
}
namespace FragmentSource_PostProcessing
{

    const std::string DEFS_BLUR =
        R"(
    uniform sampler2D u_texture;
    uniform int u_size;
)";
    const std::string DEFS_GAUSSIAN_BLUR =
        R"(
    uniform sampler2D u_texture;
    uniform usampler2D u_weights_texture;
    uniform int u_radius;
    uniform bool u_hor;
)";

    const std::string DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION =
        R"(
    // Tone Mapping
    uniform sampler2D u_hdrBuffer;
    uniform bool u_doToneMapping;
    uniform float u_exposure;
    
    // Gamma correction
    uniform bool u_doGammaCorrection;
    uniform float u_gamma;
)";

    

    const std::string DEFS_AO =
        R"(
    uniform sampler2D u_depthTexture;
    uniform sampler2D u_viewPosTexture;
    
    // Samples randomization 
    uniform sampler2D noiseTex_0;
    uniform sampler2D noiseTex_1;

    uniform sampler2D noiseTex_2;
    uniform sampler2D u_rotVecs;
    uniform vec3[64]  u_rays;
    uniform float u_radius;
    uniform int u_numSamples;
    uniform int u_numSteps;
    #define NOISE_SIZE 4

    uniform float u_near;
    uniform float u_far;
    uniform mat4  u_proj;

    #define SQRT_TWO 1.41421356237;
    #define PI 3.14159265358979323846
    #define COS_PI4 0.70710678118
    vec2 offset[8] = vec2[](
        vec2(-COS_PI4,-COS_PI4),
        vec2(-1.0, 0.0),
        vec2(-COS_PI4, COS_PI4),
        vec2( 0.0, 1.0),
        vec2( COS_PI4, COS_PI4),
        vec2( 1.0, 0.0),
        vec2( COS_PI4,-COS_PI4),
        vec2( 0.0,-1.0)
        ); 

    float EyeDepth(float depthValue, float near, float far)
{
    //http://www.songho.ca/opengl/gl_projectionmatrix.html

    float d = (depthValue * 2.0) - 1.0;
    //float res = (2.0 * near) / ( far + near - d * ( far - near));

    float res = (2.0 * far * near) /  ( ( far - near ) * (d  - ((far + near)/(far - near)) ) );
   
    return -res;
}


float RandomValue()
{
    // Using 3 textures with convenient resolution (like 9x9, 10x10, 11x11) to get a non repeating (almost) noise pattern 
    // across a large region of the window (9x10x11 ^ 2)
    ivec2 noiseSize_0 = textureSize(noiseTex_0, 0);
    ivec2 noiseSize_1 = textureSize(noiseTex_1, 0);
    ivec2 noiseSize_2 = textureSize(noiseTex_2, 0);

    float noise0 = texelFetch(noiseTex_0, ivec2(mod(gl_FragCoord.xy,noiseSize_0.xy)), 0).r;
    float noise1 = texelFetch(noiseTex_1, ivec2(mod(gl_FragCoord.xy,noiseSize_1.xy)), 0).r;
    float noise2 = texelFetch(noiseTex_2, ivec2(mod(gl_FragCoord.xy,noiseSize_2.xy)), 0).r;

    return (noise0 + noise1 + noise2) / 3.0;
}

float RandomValue(vec2 fragCoord)
{
    // Using 3 textures with convenient resolution (like 9x9, 10x10, 11x11) to get a non repeating (almost) noise pattern 
    // across a large region of the window (9x10x11 ^ 2)
    ivec2 noiseSize_0 = textureSize(noiseTex_0, 0);
    ivec2 noiseSize_1 = textureSize(noiseTex_1, 0);
    ivec2 noiseSize_2 = textureSize(noiseTex_2, 0);

    float noise0 = texelFetch(noiseTex_0, ivec2(mod(fragCoord.xy,noiseSize_0.xy)), 0).r;
    float noise1 = texelFetch(noiseTex_1, ivec2(mod(fragCoord.xy,noiseSize_1.xy)), 0).r;
    float noise2 = texelFetch(noiseTex_2, ivec2(mod(fragCoord.xy,noiseSize_2.xy)), 0).r;

    return (noise0 + noise1 + noise2) / 3.0;
}

vec2 TexCoords(vec3 viewCoords, mat4 projMatrix)
{
        vec4 samplePoint_ndc = projMatrix * vec4(viewCoords, 1.0);
        samplePoint_ndc.xyz /= samplePoint_ndc.w;
        samplePoint_ndc.xyz = samplePoint_ndc.xyz * 0.5 + 0.5;
        return samplePoint_ndc.xy;
}

vec3 EyeCoords(float xNdc, float yNdc, float zEye)
{
    //http://www.songho.ca/opengl/gl_projectionmatrix.html

    float xC = xNdc * zEye;
    float yC = yNdc * zEye;
    float xEye= (xC - u_proj[0][2] * zEye) / u_proj[0][0];
    float yEye= (yC - u_proj[1][2] * zEye) / u_proj[1][1];

    return vec3(xEye, yEye, zEye);
}

vec3 EyeNormal(vec2 uvCoords, vec3 eyePos)
    {
        vec2 texSize=textureSize(u_viewPosTexture, 0);
        vec3 normal = vec3(0,0,0);

        // TODO: an incredible waste of resources....please fix this for loop
        for(int u = 7; u >= 1; u--)
        {
            vec2 offset_coords0=(gl_FragCoord.xy + offset[u]*0.8)/texSize;
            vec2 offset_coords1=(gl_FragCoord.xy + offset[u-1]*0.8)/texSize;

            vec3 eyePos_offset0 = texture(u_viewPosTexture, offset_coords0).rgb;
            vec3 eyePos_offset1 = texture(u_viewPosTexture, offset_coords1).rgb;

            vec3 pln_x = normalize(eyePos_offset0 - eyePos);
            vec3 pln_y = normalize(eyePos_offset1 - eyePos);


            normal+=normalize(cross(pln_y, pln_x));
        }

        normal = normalize(normal); 

        return normal; 
    }

float Attenuation(float distance, float radius)
{
    float x = (distance / radius);
    return max(0.0, (1.0-(x*x)));
}

//https://stackoverflow.com/questions/26070410/robust-atany-x-on-glsl-for-converting-xy-coordinate-to-angle
float atan2(in float y, in float x)
{
    bool s = (abs(x) > abs(y));
    return mix(PI/2.0 - atan(x,y), atan(y,x), s);
}


//https://www.researchgate.net/publication/215506032_Image-space_horizon-based_ambient_occlusion
//https://developer.download.nvidia.com/presentations/2008/SIGGRAPH/HBAO_SIG08b.pdf
float OcclusionInDirection(vec3 p, vec3 direction, float radius, int numSteps, sampler2D viewCoordsTex, mat4 projMatrix, float jitter)
{
       
       float t = atan2(direction.z,length(direction.xy));
       float increment = radius / numSteps;
       increment +=(0.2)*jitter*increment;
       float bias = (PI/32);
       float slopeBias  = bias * ((3*t)/PI);
       
       float m = t;
       float wao = 0;
       float maxLen = 0;

       vec2 pUV = gl_FragCoord.xy/textureSize(viewCoordsTex,0).xy;
       vec2 minIncrement = vec2(1.0)/textureSize(viewCoordsTex,0).xy;

       vec3 D = vec3(0.0, 0.0, 0.0);
       for (int i=0; i<numSteps; i++)
       {
            vec3 samplePosition = (p*vec3(1,1,-1)) + (direction*increment*(i+1));
            vec2 sampleUV = TexCoords(samplePosition, projMatrix);
            
            //D = texture(viewCoordsTex, sampleUV).xyz - p;
            D = texelFetch(viewCoordsTex, ivec2(round(sampleUV*textureSize(viewCoordsTex,0).xy)), 0).xyz - p;    
            // Ignore samples outside radius
            float l=length(D);
            if(l>radius || l<0.001)
                continue;

            float h = atan2(D.z,length(D.xy));

            if(length(sampleUV - pUV)<length(minIncrement)*2.0)
            {
                //continue;
            }

            if(h > m + bias + slopeBias)
            {
                m = h;
                maxLen = l;
            }
       }

       wao = (sin(m) - sin(t))* Attenuation(maxLen, radius);
        
        vec3 samplePosition = (p*vec3(1,1,-1)) + (direction*increment*(1));
            vec2 sampleUV = TexCoords(samplePosition, projMatrix);
            
       return wao;
       //return p.z/15.0;//texture(viewCoordsTex, sampleUV).z/15.0;

}

//https://www.researchgate.net/publication/215506032_Image-space_horizon-based_ambient_occlusion
//https://developer.download.nvidia.com/presentations/2008/SIGGRAPH/HBAO_SIG08b.pdf
float OcclusionInDirection_NEW(vec3 p, vec3 direction, vec2 directionImage, float radius, float radiusImage, int numSteps, sampler2D viewCoordsTex, mat4 projMatrix)
{
       
       float t = atan2(direction.z,length(direction.xy));
       float increment = radiusImage / numSteps;
       float bias = (PI/6);
       
       vec2 texSize = textureSize(viewCoordsTex, 0).xy;
       
       float m = t;
       float wao = 0;
       float ao = 0;
       vec2 fragCoord = TexCoords(p*vec3(1.0, 1.0, -1.0), projMatrix);    

       vec3 D = vec3(0.0, 0.0, 0.0);
       for (int i=1; i<=numSteps; i++)
       {
            float offset = RandomValue(vec2(fragCoord + (directionImage * increment * i)) * texSize) * increment; 
            vec2 sampleUV = fragCoord + (directionImage * (increment + abs(offset)) * i) * vec2(1.0, (texSize.x/texSize.y));
            
            D = texture(viewCoordsTex, sampleUV).xyz - p;
             
            float l=length(D);

            // Ignore samples outside radius
            if(l>radius)
                continue;

            float h = atan2(D.z,length(D.xy));

            if(h + bias <  m)
            {
                m = h;
                wao += ((sin(t) - sin(m)) - ao) * Attenuation(l, radius);
                ao = (sin(t) - sin(m));
            }
       }

       
       return wao;
}

vec3 OcclusionInDirection_DEBUG(vec3 p, vec3 direction, float radius, int numSteps, sampler2D viewCoordsTex, mat4 projMatrix, float jitter)
{
       
       float t = atan2(direction.z,length(direction.xy));
       float increment = radius / numSteps;
       increment +=(0.2)*jitter*increment;
       float bias = (PI/6);
       float slopeBias  = bias * ((3*t)/PI);
       
       float m = t;
       float wao = 0;
       float maxLen = 0;

        vec3 D = vec3(0.0, 0.0, 0.0);
       for (int i=1; i<=numSteps; i++)
       {
            vec3 samplePosition = (p*vec3(1,1,-1)) + (direction*increment*i);
            vec2 sampleUV = TexCoords(samplePosition, projMatrix);
                
           
            D = texture(viewCoordsTex, sampleUV).xyz - p;
            
            // Ignore samples outside radius
            float l=length(D);
            if(l>radius || l<0.001)
                continue;

            float h = atan2( -D.z,length(D.xy));

            if(h > m + bias + slopeBias)
            {
                m = h;
                maxLen = l;
            }
       }
wao = (sin(m) - sin(t))* Attenuation(maxLen, radius);
        
        //return vec3(D);

        vec3 samplePosition = (p*vec3(1,1,-1)) + (direction*increment*(1));
        vec2 sampleUV = TexCoords(samplePosition, projMatrix);

        vec2 pUV = TexCoords((p*vec3(1,1,-1)), projMatrix);
            
       
       //return vec3(p.z/15.0,texture(viewCoordsTex, sampleUV).z/15.0f,abs(p.z - texture(viewCoordsTex, sampleUV).z));
        return vec3((round(sampleUV.xy*textureSize(viewCoordsTex, 0).xy)-round(pUV.xy*textureSize(viewCoordsTex, 0).xy))/20.0, 0.0);
}

vec3 EyeNormal_dzOLD(vec2 uvCoords, vec3 eyePos)
    {
        vec2 texSize=textureSize(u_viewPosTexture, 0);

        
        float p_dzdx = texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x + 1), gl_FragCoord.y),0).z - eyePos.z;
        float p_dzdy = texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y + 1)),0).z - eyePos.z;

        float m_dzdx = -texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x - 1), gl_FragCoord.y),0).z + eyePos.z;
        float m_dzdy = -texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y - 1)),0).z + eyePos.z;
        
        bool px = abs(p_dzdx)<abs(m_dzdx);
        bool py = abs(p_dzdy)<abs(m_dzdy);

        float p_dx=texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x + 1.0 ), gl_FragCoord.y),0).x - eyePos.x;
        float p_dy=texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y + 1.0)),0).y  - eyePos.y; 

        float m_dx= - texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x - 1.0 ), gl_FragCoord.y),0).x + eyePos.x;
        float m_dy= - texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y - 1.0)),0).y  + eyePos.y; 

        vec3 normal = normalize(vec3(px?p_dzdx:m_dzdx, py?p_dzdy:m_dzdy, length(vec2(px?p_dx:m_dx, py?p_dy:m_dy)))); 

        return normal;
    }

    vec3 EyeNormal_dz(vec2 uvCoords, vec3 eyePos)
    {
        vec2 texSize=textureSize(u_viewPosTexture, 0);

        
        float p_dzdx = texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x + 1), gl_FragCoord.y),0).z - eyePos.z;
        float p_dzdy = texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y + 1)),0).z - eyePos.z;

        float m_dzdx = -texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x - 1), gl_FragCoord.y),0).z + eyePos.z;
        float m_dzdy = -texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y - 1)),0).z + eyePos.z;
        
        bool px = abs(p_dzdx)<abs(m_dzdx);
        bool py = abs(p_dzdy)<abs(m_dzdy);

        vec3 i = normalize( texelFetch(u_viewPosTexture, ivec2((gl_FragCoord.x + (px?+1:-1) ), gl_FragCoord.y),0).xyz - eyePos );
        vec3 j = normalize( texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.x, (gl_FragCoord.y + (py?+1:-1))),0).xyz  - eyePos ); 

        
        vec3 normal = normalize( cross( (px^^py ? j : i), (px^^py ? i : j)) ); 
        normal.z*=-1;
        return -normal;
    }

)";

    const std::string CALC_GAUSSIAN_BLUR =
        R"(
    
        vec2 texSize=textureSize(u_texture, 0);
        ivec2 weights_textureSize=textureSize(u_weights_texture, 0);        

        float sum=0;
        vec4 color=vec4(0.0,0.0,0.0,0.0);

        float weight=texelFetch(u_weights_texture, ivec2(0, u_radius), 0).r;
        color+=texture(u_texture, (gl_FragCoord.xy)/texSize) * weight;
        sum +=weight;

        for(int i = 1; i <= u_radius ; i++)
        {
            float weight=texelFetch(u_weights_texture, ivec2(i, u_radius), 0).r;

            vec2 offset = vec2(u_hor?1:0, u_hor?0:1);  

            color+=texture(u_texture, (gl_FragCoord.xy + i * offset)/texSize)* weight;
            color+=texture(u_texture, (gl_FragCoord.xy - i * offset)/texSize)* weight;

            sum+=2.0*weight;
        }
        
        color/=sum;

        FragColor=color;
)";

    const std::string CALC_BLUR =
        R"(
    
        vec2 texSize=textureSize(u_texture, 0);
        vec4 color=vec4(0.0,0.0,0.0,0.0);
        for(int u = -u_size; u <= u_size ; u++)
        {
           for(int v = -u_size; v <= u_size ; v++)
            {
               color+=texture(u_texture, (gl_FragCoord.xy + vec2(u, v))/texSize);
            }
        }
        color/=u_size*u_size*4.0;

        FragColor=color;
)";

    const std::string CALC_SSAO =
        R"(
    
    //TODO: please agree on what you consider view space, if you need to debug invert the z when showing the result
    vec2 texSize=textureSize(u_viewPosTexture, 0);
    vec2 uvCoords = gl_FragCoord.xy / texSize;

    vec3 eyePos = texelFetch(u_viewPosTexture, ivec2(gl_FragCoord.xy), 0).rgb;
    
    if(eyePos.z>u_far-0.001)
        {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
            return;
        }

    vec3 normal = EyeNormal_dz(uvCoords, eyePos) /** vec3(1.0, 1.0, -1.0)*/;
    //vec3 normal = texture(u_viewNormalsTexture, uvCoords).rgb;

    vec3 rotationVec = texture(u_rotVecs, uvCoords * texSize/NOISE_SIZE).rgb;
    vec3 tangent     = normalize(rotationVec - normal * dot(rotationVec, normal));
    vec3 bitangent   = cross(normal, tangent);
    mat3 TBN         = mat3(tangent, bitangent, normal); 
    
    float ao=0;
    for(int k=0; k < u_numSamples; k++)
    {
        vec3 samplePoint_view = vec3(eyePos.x, eyePos.y, eyePos.z*-1.0) + ( u_radius * ( TBN * u_rays[k]));
        vec4 samplePoint_ndc = u_proj * vec4(samplePoint_view, 1.0);
        samplePoint_ndc.xyz /= samplePoint_ndc.w;
        samplePoint_ndc.xyz = samplePoint_ndc.xyz * 0.5 + 0.5;
    
        if(!(samplePoint_view.z <u_far-0.1))
            continue;
        float delta = texture(u_viewPosTexture, samplePoint_ndc.xy).b + samplePoint_view.z;        
        ao+= (delta  < 0 ? 1.0 : 0.0) * smoothstep(0.0, 1.0, 1.0 - (abs(delta) / u_radius));       
    }
    ao /= u_numSamples;

    FragColor = vec4(normal, 1.0);// * (1-ao);
    //FragColor = vec4(1.0, 1.0, 1.0, 1.0) * (1-ao); 
)";

    const std::string CALC_HBAO =
        R"(
    
    //TODO: please agree on what you consider view space, if you need to debug invert the z when showing the result
    vec2 texSize=textureSize(u_viewPosTexture, 0);
    vec2 uvCoords = gl_FragCoord.xy / texSize;

    vec3 eyePos = texture(u_viewPosTexture, uvCoords).rgb;
    
    if(eyePos.z>u_far-0.001)
        {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
            return;
        }

    vec3 normal = EyeNormal_dz(uvCoords, eyePos)  * vec3(1.0, 1.0, -1.0);
    
    vec3 rotationVec = vec3(1.0,0.0,0.0); //texture(u_rotVecs, uvCoords * texSize/NOISE_SIZE).rgb;
    vec3 tangent     = normalize(rotationVec - normal * dot(rotationVec, normal));
    vec3 bitangent   = cross(normal, tangent);
    mat3 TBN         = mat3(tangent, bitangent, normal); 
    
    float ao;
    float increment = ((2.0*PI)/u_numSamples);
    
    vec4 projectedRadius = (u_proj * vec4(u_radius, 0, -eyePos.z, 1));
    float radiusImage = (projectedRadius.x/projectedRadius.w)*0.5;
    
    float offset = RandomValue() * PI;

    for(int k=0; k < u_numSamples; k++)
    {
        //TODO: pass the directions as uniforms???
        float angle=(increment * k) + offset;
        vec3 sampleDirection = TBN * vec3(cos(angle), sin(angle), 0.0);      
        ao += OcclusionInDirection_NEW(eyePos, sampleDirection,normalize(sampleDirection.xy),  u_radius, radiusImage, u_numSteps, u_viewPosTexture, u_proj);     
    }
    ao /= u_numSamples;

   
    FragColor = vec4(vec3(1.0-ao),1.0); 
)";
    const std::string CALC_POSITIONS =
        R"(

    ivec2 texSize=textureSize(u_depthTexture, 0);

    float depthValue = texture(u_depthTexture, gl_FragCoord.xy/vec2(texSize) ).r;
    float zEye =  EyeDepth( depthValue, u_near, u_far );
    vec3 eyePos = EyeCoords(((gl_FragCoord.x/float(texSize.x)) * 2.0)  - 1.0, ((gl_FragCoord.y/float(texSize.y)) * 2.0)  - 1.0, zEye);

    
    FragColor = vec4(eyePos, 1.0); 

)";

    const std::string CALC_TONE_MAPPING_AND_GAMMA_CORRECTION =
        R"(

    vec4 col = texelFetch(u_hdrBuffer, ivec2(gl_FragCoord.xy - vec2(0.5)), 0).rgba;
    
    if(u_doToneMapping)
        col = vec4(vec3(1.0) - exp(-col.rgb * u_exposure), col.a);
    
    if(u_doGammaCorrection)
        col = vec4(pow(col.rgb, vec3(1.0/u_gamma)), col.a);

    gl_FragColor = col;
)";

    const std::string EXP_FRAGMENT =
        R"(
    #version 330 core
    out vec4 FragColor;

    //[DEFS_SSAO]
    //[DEFS_BLUR]
    //[DEFS_GAUSSIAN_BLUR]
    //[DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION]

    void main()
    {
        //[CALC_POSITIONS]
        //[CALC_SSAO]
        //[CALC_HBAO]
        //[CALC_BLUR]
        //[CALC_GAUSSIAN_BLUR]
        //[CALC_TONE_MAPPING_AND_GAMMA_CORRECTION]
    }
    )";

    static std::map<std::string, std::string> Expansions = {

       { "DEFS_SSAO",                                   FragmentSource_PostProcessing::DEFS_AO                                  },
       { "DEFS_BLUR",                                   FragmentSource_PostProcessing::DEFS_BLUR                                },
       { "DEFS_GAUSSIAN_BLUR",                          FragmentSource_PostProcessing::DEFS_GAUSSIAN_BLUR                       },
       { "DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION",      FragmentSource_PostProcessing::DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION   },
       { "CALC_POSITIONS",                              FragmentSource_PostProcessing::CALC_POSITIONS                           },
       { "CALC_SSAO",                                   FragmentSource_PostProcessing::CALC_SSAO                                },
       { "CALC_HBAO",                                   FragmentSource_PostProcessing::CALC_HBAO                                },
       { "CALC_BLUR",                                   FragmentSource_PostProcessing::CALC_BLUR                                },
       { "CALC_GAUSSIAN_BLUR",                          FragmentSource_PostProcessing::CALC_GAUSSIAN_BLUR                       },
       { "CALC_TONE_MAPPING_AND_GAMMA_CORRECTION",      FragmentSource_PostProcessing::CALC_TONE_MAPPING_AND_GAMMA_CORRECTION   },

    };

    std::string Expand(std::string source, std::vector<std::string> expStrings)
    {
        std::string result = source;

        for (int i = 0; i < expStrings.size(); i++)
        {
            std::string token = "//[" + expStrings[i] + "]";
            size_t start_pos = result.find(token);
            std::string expansion = FragmentSource_PostProcessing::Expansions[expStrings[i]];
            result.replace(start_pos, token.length(), expansion);
        }

        return result;
    }
}

enum class OGLResourceType
{
    UNDEFINED,
    VERTEX_SHADER,
    FRAGMENT_SHADER,
    SHADER_PROGRAM,
    VERTEX_BUFFER_OBJECT,
    VERTEX_ATTRIB_ARRAY,
    INDEX_BUFFER,
    UNIFORM_BUFFER,
    TEXTURE,
    FRAMEBUFFER
};

namespace OGLUtils
{
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void CheckCompileErrors(unsigned int shader, std::string type)
    {
        int success;
        char infoLog[1024];

        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }

    }

    void CheckLinkErrors(unsigned int program, std::string type)
    {
        int success;
        char infoLog[1024];

        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(program, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }

    void CheckOGLErrors()
    {
        GLenum error = 0;
        std::string log = "";
        while ((error = glGetError()) != GL_NO_ERROR)
        {
            
            log += error;
            log += ' ,';
        }
        if (!log.empty())
            throw log;
    }
}

class OGLResource
{

public:
    OGLResource()
        : _id(0), _type(OGLResourceType::UNDEFINED) {};

    // No Copy Constructor/Assignment allowed
    OGLResource(const OGLResource&) = delete;
    OGLResource& operator=(const OGLResource&) = delete;

    OGLResource(OGLResource&& other) noexcept
    {
        _id = other._id;
        _type = other._type;
        other._id = 0;
        other._type = OGLResourceType::UNDEFINED;
    };

    OGLResource& operator=(OGLResource&& other) noexcept
    {
        if (this != &other)
        {
            NotifyDestruction();
            FreeResources(_type);
            OGLUtils::CheckOGLErrors();

            _id = other._id;
            other._id = 0;
        }
        return *this;
    };

    ~OGLResource()
    {
        if(_id)
            throw "Trying to destroy an OpenGL resource without deleting it first";
    }
    
    unsigned int ID() const
    {
        if (!_id)
            throw "Trying to use an uninitialized/deleted OpenGL resource";

        return _id;
    }

private:
    unsigned int _id;
    OGLResourceType _type;
    void InitResources(OGLResourceType type)
    {
        ResourceTypeSwitch(type, false);
    };
    void FreeResources(OGLResourceType type)
    {
        ResourceTypeSwitch(type, true);
    };
    void ResourceTypeSwitch(OGLResourceType type, bool destroy)
    {
        switch (type)
        {
        case (OGLResourceType::VERTEX_SHADER):
            if (!destroy)
                _id = glCreateShader(GL_VERTEX_SHADER);
            else
            {
                glDeleteShader(_id);
                _id = 0;
            }
            break;

        case (OGLResourceType::FRAGMENT_SHADER):
            if (!destroy)
                _id = glCreateShader(GL_FRAGMENT_SHADER);
            else
            {
                glDeleteShader(_id);
                _id = 0;
            }
            break;

        case (OGLResourceType::SHADER_PROGRAM):
            if (!destroy)
                _id = glCreateProgram();
            else
            {
                glDeleteProgram(_id);
                _id = 0;
            }
            break;

        case (OGLResourceType::VERTEX_BUFFER_OBJECT):
        case (OGLResourceType::INDEX_BUFFER):
        case (OGLResourceType::UNIFORM_BUFFER):
            if (!destroy)
                glGenBuffers(1, &_id);
            else
            {
                glDeleteBuffers(1, &_id);
                _id = 0;
            }
                break;

        case(OGLResourceType::VERTEX_ATTRIB_ARRAY):
                if (!destroy)
                    glGenVertexArrays(1, &_id);
                else
                {
                    glDeleteVertexArrays(1, &_id);
                    _id = 0;
                }
                break;
        case(OGLResourceType::TEXTURE):
            if (!destroy)
                glGenTextures(1, &_id);
            else
            {
                glDeleteTextures(1, &_id);
                _id = 0;
            }
            break;
        case(OGLResourceType::FRAMEBUFFER):
            if (!destroy)
                glGenFramebuffers(1, &_id);
            else
            {
                glDeleteFramebuffers(1, &_id);
                _id = 0;
            }
            break;
        default:
            throw "Unhandled Resource Type";
            break;
        }
    }

protected:
    void Create(OGLResourceType type)
    {
        _type = type;
        InitResources(_type);
        OGLUtils::CheckOGLErrors();

        NotifyCreation();
    }

    void Destroy()
    {
        // Skip if the resources has already been freed (like after a move assignemnt)
        if (_id)
        {
            NotifyDestruction();
            FreeResources(_type);
            OGLUtils::CheckOGLErrors();
        }
    }
    std::string ResourceType()
    { 
        switch (_type)
        {
        case (OGLResourceType::VERTEX_SHADER):
            return "VERTEX_SHADER";
            break;

        case (OGLResourceType::FRAGMENT_SHADER):
            return "FRAGMENT_SHADER";
            break;

        case (OGLResourceType::SHADER_PROGRAM):
            return "SHADER_PROGRAM";
            break;

        case (OGLResourceType::VERTEX_BUFFER_OBJECT):
            return "VERTEX_BUFFER_OBJECT";
            break;

        case (OGLResourceType::INDEX_BUFFER):
            return "INDEX_BUFFER";
            break;

        case (OGLResourceType::UNIFORM_BUFFER):
            return "UNIFORM_BUFFER";
            break;

        case(OGLResourceType::VERTEX_ATTRIB_ARRAY):
            return "VERTEX_ATTRIB_ARRAY";
            break;

        case(OGLResourceType::TEXTURE):
            return "TEXTURE";
            break;

        case(OGLResourceType::FRAMEBUFFER):
            return "FRAMEBUFFER";
            break;
        }
    }
    void NotifyCreation() { std::cout << std::endl << "Creating OGL Object: " + ResourceType() + "_" + std::to_string(ID()); }
    void NotifyDestruction() { std::cout << std::endl << "Destroying OGL Object: " + ResourceType() + "_" + std::to_string(ID()); }

};

class VertexBufferObject : public OGLResource
{
public:
    enum class VBOType
    {
        Undefined,
        StaticDraw,
        DynamicDraw,
        StreamDraw
    };

public:

    VertexBufferObject(VBOType type) : _type(type)
    {
        OGLResource::Create(OGLResourceType::VERTEX_BUFFER_OBJECT);
    }

    VertexBufferObject(VertexBufferObject&& other) noexcept : OGLResource(std::move(other))
    {
        this->_type = other._type;
        other._type = VBOType::Undefined;
    };

    VertexBufferObject& operator=(VertexBufferObject&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
            this->_type = other._type;
            other._type = VBOType::Undefined;
        }
        return *this;
    };

    ~VertexBufferObject()
    {
        OGLResource::Destroy();
    }

    GLenum ResolveUsage(VBOType type)
    {
        switch (type)
        {
        case(VBOType::StaticDraw):
            return GL_STATIC_DRAW;
            break;

        case(VBOType::DynamicDraw):
            return GL_DYNAMIC_DRAW;
            break;

        case(VBOType::StreamDraw):
            GL_STREAM_DRAW;
            break;
        default:
            return -1;
            break;
        }
    }

    void Bind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, OGLResource::ID());

        OGLUtils::CheckOGLErrors();
    }

    void UnBind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        OGLUtils::CheckOGLErrors();
    }

    void SetData(unsigned int count, const float* data)
    {
        Bind();
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(float), data != nullptr ? data : NULL, ResolveUsage(_type));
        UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void SetSubData(unsigned int startIndex, unsigned int count, const float* data)
    {
        Bind();
        glBufferSubData(GL_ARRAY_BUFFER, startIndex * sizeof(float), count * sizeof(float), data != nullptr ? data : NULL);
        UnBind();

        OGLUtils::CheckOGLErrors();
    }
private:
    VBOType _type;
};

class IndexBufferObject : public OGLResource
{
public:
    enum class EBOType
    {
        Undefined,
        StaticDraw,
        DynamicDraw,
        StreamDraw
    };

public:
    IndexBufferObject(EBOType type) : _type(type)
    {
        OGLResource::Create(OGLResourceType::INDEX_BUFFER);
    }

    IndexBufferObject(IndexBufferObject&& other) noexcept : OGLResource(std::move(other))
    {
        this->_type = other._type;
        other._type = EBOType::Undefined;
    };

    IndexBufferObject& operator=(IndexBufferObject&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
            this->_type = other._type;
            other._type = EBOType::Undefined;
        }
        return *this;
    };

    ~IndexBufferObject()
    {
        OGLResource::Destroy();
    }

    GLenum ResolveUsage(EBOType type)
    {
        switch (type)
        {
        case(EBOType::StaticDraw):
            return GL_STATIC_DRAW;
            break;

        case(EBOType::DynamicDraw):
            return GL_DYNAMIC_DRAW;
            break;

        case(EBOType::StreamDraw):
            GL_STREAM_DRAW;
            break;
        default:
            return -1;
            break;
        }
    }

    void Bind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLResource::ID());

        OGLUtils::CheckOGLErrors();
    }

    void UnBind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        OGLUtils::CheckOGLErrors();
    }

    void SetData(unsigned int count, const int* data)
    {
        Bind();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(int), data != nullptr ? data : NULL, ResolveUsage(_type));
        UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void SetSubData(unsigned int startIndex, unsigned int count, const int* data)
    {
        Bind();
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, startIndex * sizeof(int), count * sizeof(int), data != nullptr ? data : NULL);
        UnBind();

        OGLUtils::CheckOGLErrors();
    }
private:
    EBOType _type;
};

class UniformBufferObject : public OGLResource
{
public:
    enum class UBOType
    {
        Undefined,
        StaticDraw,
        DynamicDraw,
        StreamDraw
    };


public:
    UniformBufferObject(UBOType type) : _type(type)
    {
        OGLResource::Create(OGLResourceType::UNIFORM_BUFFER);
    }

    UniformBufferObject(UniformBufferObject&& other) noexcept : OGLResource(std::move(other))
    {
        this->_type = other._type;
        other._type = UBOType::Undefined;
    };

    UniformBufferObject& operator=(UniformBufferObject&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
            this->_type = other._type;
            other._type = UBOType::Undefined;
        }
        return *this;
    };

    ~UniformBufferObject()
    {
        OGLResource::Destroy();
    }

    GLenum ResolveUsage(UBOType type)
    {
        switch (type)
        {
        case(UBOType::StaticDraw):
            return GL_STATIC_DRAW;
            break;

        case(UBOType::DynamicDraw):
            return GL_DYNAMIC_DRAW;
            break;

        case(UBOType::StreamDraw):
            GL_STREAM_DRAW;
            break;
        default:
            return -1;
            break;
        }
    }

    void Bind()
    {
        glBindBuffer(GL_UNIFORM_BUFFER, OGLResource::ID());

        OGLUtils::CheckOGLErrors();
    }

    void UnBind()
    {
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        OGLUtils::CheckOGLErrors();
    }

    template<typename T>
    void SetData(unsigned int count, const T* data)
    {
        Bind();
        glBufferData(GL_UNIFORM_BUFFER, count * sizeof(T), data != nullptr ? data : NULL, ResolveUsage(_type));
        UnBind();

        OGLUtils::CheckOGLErrors();
    }

    template<typename T>
    void SetSubData(unsigned int startIndex, unsigned int count, const T* data)
    {
        OGLUtils::CheckOGLErrors();

        Bind();
        glBufferSubData(GL_UNIFORM_BUFFER, startIndex * sizeof(T), count * sizeof(T), data != nullptr ? data : NULL);
        UnBind();

        OGLUtils::CheckOGLErrors();
    }

    void BindingPoint(UBOBinding binding)
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, OGLResource::ID());
    }

private:
    UBOType _type;
};

class VertexAttribArray : public OGLResource
{

public:
    VertexAttribArray()
    {
        OGLResource::Create(OGLResourceType::VERTEX_ATTRIB_ARRAY);
    }

    VertexAttribArray(VertexAttribArray&& other) noexcept : OGLResource(std::move(other)) {};

    VertexAttribArray& operator=(VertexAttribArray&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
        }
        return *this;
    };

    ~VertexAttribArray()
    {
        OGLResource::Destroy();
    }

    void Bind() const
    {
        OGLUtils::CheckOGLErrors();

        glBindVertexArray(OGLResource::ID());

        OGLUtils::CheckOGLErrors();
    }

    void UnBind() const
    {
        glBindVertexArray(0);

        OGLUtils::CheckOGLErrors();
    }


    void SetAndEnableAttrib(unsigned int buffer, unsigned int index, int components, bool normalized, unsigned int stride, unsigned int countOffset)
    {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);

        Bind();
        glVertexAttribPointer(index, components, GL_FLOAT, normalized ? GL_TRUE : GL_FALSE, stride, (void*)(countOffset * sizeof(float)));
        glEnableVertexAttribArray(index);
        UnBind();

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        OGLUtils::CheckOGLErrors();
    }

    void SetIndexBuffer(unsigned int indexBuffer)
    {

        Bind();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        UnBind();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    }
};


class OGLFragmentShader : public OGLResource
{
    private:
        std::string _source;

    public:
        OGLFragmentShader(std::string source) : _source{ source } 
        {
            OGLResource::Create(OGLResourceType::FRAGMENT_SHADER);

            const char* fsc = source.data();
            glShaderSource(OGLResource::ID(), 1, &fsc, NULL);
            glCompileShader(OGLResource::ID());
            OGLUtils::CheckCompileErrors(OGLResource::ID(), ResourceType());
            OGLUtils::CheckOGLErrors();
        }

        OGLFragmentShader(OGLFragmentShader&& other) noexcept : OGLResource(std::move(other))
        {
            _source = std::move(other._source);
        };

        OGLFragmentShader& operator=(OGLFragmentShader&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                _source = std::move(other._source);
            }
            return *this;
        };

        ~OGLFragmentShader()
        {
            OGLResource::Destroy();
        }
};

class OGLVertexShader : public OGLResource
{
private:
    std::string _source;

public:
    OGLVertexShader(std::string source) : _source{ source }
    {
        OGLResource::Create(OGLResourceType::VERTEX_SHADER);

        const char* vsc = source.data();
        glShaderSource(OGLResource::ID(), 1, &vsc, NULL);
        glCompileShader(OGLResource::ID());
        OGLUtils::CheckCompileErrors(OGLResource::ID(), ResourceType());
        OGLUtils::CheckOGLErrors();
    }

    OGLVertexShader(OGLVertexShader&& other) noexcept : OGLResource(std::move(other))
    {
        _source = std::move(other._source);
    };

    OGLVertexShader& operator=(OGLVertexShader&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
            _source = std::move(other._source);
        }
        return *this;
    };

    ~OGLVertexShader()
    {
        OGLResource::Destroy();
    }

};

class ShaderProgram : public OGLResource
{

public:
    // constructor generates the shader on the fly
    // ------------------------------------------------------------------------
    ShaderProgram(const std::string vShaderCode, std::string fShaderCode)
    {

        OGLVertexShader vertex(vShaderCode);
        OGLFragmentShader fragment(fShaderCode);

        OGLResource::Create(OGLResourceType::SHADER_PROGRAM);

        glAttachShader(OGLResource::ID(), vertex.ID());
        glAttachShader(OGLResource::ID(), fragment.ID());

        glLinkProgram(OGLResource::ID());
        OGLUtils::CheckLinkErrors(OGLResource::ID(), ResourceType());

        OGLUtils::CheckOGLErrors();
    }
    
    ShaderProgram(OGLVertexShader&& other) : OGLResource(std::move(other)) {};

    ShaderProgram& operator=(ShaderProgram&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
        }
        return *this;
    };

    void  Enable() const { glUseProgram(OGLResource::ID()); }

    std::vector<std::pair<unsigned int, std::string>> GetInput()
    {
        unsigned int id = OGLResource::ID();

        int numAttribs;
        glGetProgramInterfaceiv(id, GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &numAttribs);
        unsigned int properties[] = { GL_NAME_LENGTH, GL_TYPE, GL_LOCATION };

        std::vector<std::pair<unsigned int, std::string>> res{};
        for (int i = 0; i < numAttribs; ++i) 
        {
            GLint results[3];
            glGetProgramResourceiv(id, GL_PROGRAM_INPUT, i, 3, properties, 3, NULL, results);

            GLint nameBufSize = results[0];
            std::unique_ptr<char[]> name(std::make_unique<char[]>(nameBufSize));
            glGetProgramResourceName(id, GL_PROGRAM_INPUT, i, nameBufSize, NULL, name.get());
            
            res.push_back(std::pair<unsigned int, std::string>(results[2], std::string(name.get())));
            
        }
        return res;
    }

    ~ShaderProgram()
    {
        OGLResource::Destroy();
    }


};


class OGLTexture2D : public OGLResource
{
private:
    int
        _width, _height;

    void Init(int level, TextureInternalFormat internalFormat, int width, int height, GLenum format, GLenum type, const void* data)
    {
       
        Bind();

        glTexImage2D(GL_TEXTURE_2D, level, internalFormat,
            width, height, 0, format, type, data);

        OGLUtils::CheckOGLErrors();

        UnBind();
    }

    bool NeedsMips(TextureFiltering filter)
    {
        switch (filter)
        {
        case(TextureFiltering::Linear_Mip_Linear):
        case(TextureFiltering::Linear_Mip_Nearest):
        case(TextureFiltering::Nearest_Mip_Linear):
        case(TextureFiltering::Nearest_Mip_Nearest):
            return true;
            break;

        default:
            return false;
            break;
        }
    }

    void SetParameters(TextureFiltering minfilter, TextureFiltering magfilter, TextureWrap wrapS, TextureWrap wrapT)
    {
        Bind();

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);


        if (NeedsMips(minfilter) || NeedsMips(magfilter))
            glGenerateMipmap(GL_TEXTURE_2D);

        OGLUtils::CheckOGLErrors();

        UnBind();
    }

    GLenum ResolveFormat(TextureInternalFormat internal)
    {
        switch (internal)
        {
        case(TextureInternalFormat::Rgba_32f): 
        case(TextureInternalFormat::Rgba_16f):
            return GL_RGBA;
            break;
        case(TextureInternalFormat::Rgb_16f):
            return GL_RGB;
            break;
        case(TextureInternalFormat::R_16ui):
            return GL_RED;
            break;
        default:
            return internal;
            break;
        }
    }

public:
    OGLTexture2D(int width, int height, TextureInternalFormat internalFormat) : _width(width), _height(height)
    {
        OGLResource::Create(OGLResourceType::TEXTURE);

        Init(0, internalFormat, width, height, ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, NULL);

        SetParameters(
            TextureFiltering::Nearest, TextureFiltering::Nearest,
            TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);

        OGLUtils::CheckOGLErrors();
    }

    OGLTexture2D(const char* path, TextureFiltering minFilter, TextureFiltering magFilter)
    {

        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

        if (!data)
        {
            std::cout << "\n" << "Texture data loading failed: " << stbi_failure_reason() << "\n" << std::endl;
            throw "Texture data loading failed";
        }

        OGLResource::Create(OGLResourceType::TEXTURE);

        OGLUtils::CheckOGLErrors();

        TextureInternalFormat internalFormat;
        switch (nrChannels)
        {
            case(3): internalFormat = TextureInternalFormat::Rgb; break;
            case(4): internalFormat = TextureInternalFormat::Rgba; break;
            default:
                throw "Unsupported texture format."; break;
        }

        Init(0, internalFormat, width, height, ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, data);

        OGLUtils::CheckOGLErrors();

        SetParameters(
            minFilter, magFilter,
            TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);

        OGLUtils::CheckOGLErrors();

        stbi_image_free(data);
    }

    OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, void* data, GLenum dataFormat, GLenum type)
    {
        OGLResource::Create(OGLResourceType::TEXTURE);

        Init(0, internalFormat, width, height, dataFormat, type, data);

        SetParameters(
            TextureFiltering::Nearest, TextureFiltering::Nearest,
            TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);

        OGLUtils::CheckOGLErrors();
    }

    OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, void* data, GLenum dataFormat, GLenum type,
        TextureFiltering minFilter, TextureFiltering magFilter, TextureWrap wrapS, TextureWrap wrapT)
    {
        OGLResource::Create(OGLResourceType::TEXTURE);

        Init(0, internalFormat, width, height, dataFormat, type, data);

        SetParameters(minFilter, magFilter, wrapS, wrapT);

        OGLUtils::CheckOGLErrors();
    }

    OGLTexture2D(OGLTexture2D&& other) noexcept : OGLResource(std::move(other))
    {
        _width = other._width;
        _height = other._height;
    };

    void Bind() const
    {
        glBindTexture(GL_TEXTURE_2D, OGLResource::ID());
    }

    void UnBind() const
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    OGLTexture2D& operator=(OGLTexture2D&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));
            _height = other._height;
            _width = other._width;
        }
        return *this;
    };

    ~OGLTexture2D()
    {
        OGLResource::Destroy();
    }

};
class FrameBuffer : public OGLResource
{
private:
    std::optional<OGLTexture2D> _depthTexture;
    std::vector<OGLTexture2D> _colorTextures;
    int
        _width, _height;


    void Initialize(int width, int height, bool depth, bool color, int colorAttachments, TextureInternalFormat colorTextureFormat)
    {

        if (depth)
            _depthTexture = OGLTexture2D(width, height, TextureInternalFormat::Depth_Component);

        if (color)
        {
            for (int i = 0; i < colorAttachments; i++)
                _colorTextures.push_back(OGLTexture2D(width, height, colorTextureFormat));
        }

        Bind();

        if (color)
        {
            for (int i = 0; i < colorAttachments; i++)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, _colorTextures.at(i).ID(), 0);
            }

            if (depth)
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture.value().ID(), 0);

        }
        else if (depth)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture.value().ID(), 0);

            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw "Framebuffer" + std::to_string(ID()) +" is not complete!" ;

        UnBind();

    }
public:
    FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth)
        :_width(width), _height(height)
    {
        OGLResource::Create(OGLResourceType::FRAMEBUFFER);

        Initialize(_width, _height, depth, color, colorAttachments, TextureInternalFormat::Rgba_32f);
    }

    FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth, TextureInternalFormat colorTextureFormat)
        :_width(width), _height(height)
    {
        OGLResource::Create(OGLResourceType::FRAMEBUFFER);

        Initialize(_width, _height, depth, color, colorAttachments, colorTextureFormat);
    }

    FrameBuffer(FrameBuffer&& other) noexcept : OGLResource(std::move(other))
    {
        _width = other._width;
        _height = other._height;

        _colorTextures = std::vector<OGLTexture2D>(std::move(other._colorTextures));
        _depthTexture = std::optional<OGLTexture2D>(std::move(other._depthTexture));
    };


    FrameBuffer& operator=(FrameBuffer&& other) noexcept
    {
        if (this != &other)
        {
            OGLResource::operator=(std::move(other));

            _height = other._height;
            _width = other._width;

            _colorTextures = std::vector<OGLTexture2D>(std::move(other._colorTextures));
            _depthTexture = std::optional<OGLTexture2D>(std::move(other._depthTexture));
        }
        return *this;

    };
    ~FrameBuffer()
    {
        OGLResource::Destroy();
    }


    void Bind(bool read, bool write) const
    {
        int mask = (read ? GL_READ_FRAMEBUFFER : 0) | (write ? GL_DRAW_FRAMEBUFFER : 0);
        glBindFramebuffer(mask, OGLResource::ID());
    }

    void Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, OGLResource::ID());
    }

    void UnBind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }


    unsigned int DepthTextureId() const
    {
        return _depthTexture.value().ID();
    }

    unsigned int ColorTextureId(int attachment = 0) const
    {
        return _colorTextures.at(attachment).ID();
    }

    unsigned int Width() const
    {
        return _width;
    }

    unsigned int Height() const
    {
        return _height;
    }

    void CopyFromOtherFbo(const FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1) const
    {

        unsigned int id = other != nullptr ? other->ID() : 0;

        unsigned int flag = (color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0);

        // Bind read buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, id);

        // Bind draw buffer
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLResource::ID());

        if (color)
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment);

        // Blit
        glBlitFramebuffer(rect0.x, rect0.y, rect1.x, rect1.y, rect0.x, rect0.y, rect1.x, rect1.y, flag, GL_NEAREST);

        // Unbind
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void CopyToOtherFbo(const FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1) const
    {

        unsigned int id = other != nullptr ? other->ID() : 0;

        unsigned int flag = (color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0);

        // Bind read buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLResource::ID());

        // Bind draw buffer
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);

        if (color)
        {
            if (id != 0)
                glDrawBuffer(GL_COLOR_ATTACHMENT0 + attachment);
            else
                glDrawBuffer(GL_BACK);
        }

        // Blit
        glBlitFramebuffer(rect0.x, rect0.y, rect1.x, rect1.y, rect0.x, rect0.y, rect1.x, rect1.y, flag, GL_NEAREST);

        // Unbind
        if (color && id != 0)
            glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

};


class RenderableBasic
{
public:
    virtual std::vector<glm::vec3> GetPositions() { return std::vector<glm::vec3>(1); };
    virtual std::vector<glm::vec3> GetNormals() { return std::vector<glm::vec3>(0); };
    virtual std::vector<glm::vec2> GetTextureCoordinates() { return std::vector<glm::vec2>(0); };
    virtual std::vector<glm::vec3> GetTangents() { return std::vector<glm::vec3>(0); };
    virtual std::vector<glm::vec3> GetBitangents() { return std::vector<glm::vec3>(0); };
    virtual std::vector<int> GetIndices() { return std::vector<int>(0); };
};

class ShaderBase
{
private:
    ShaderProgram _shaderProgram;
    std::string _vertexCode;
    std::string _fragmentCode;

    
public:
  
    ShaderBase( std::string vertexSource, std::string fragmentSource) :
        _vertexCode(vertexSource), _fragmentCode(fragmentSource), _shaderProgram(vertexSource, fragmentSource)
    {
    }

    void SetCurrent() const { _shaderProgram.Enable(); }

    std::vector<std::pair<unsigned int, std::string>> GetVertexInput() { return _shaderProgram.GetInput(); }

    int UniformLocation(std::string name) const { return glGetUniformLocation(_shaderProgram.ID(), name.c_str()); };

    unsigned int ShaderProgramId() { return _shaderProgram.ID(); };
};

class MeshShader : public ShaderBase
{

public:
    MeshShader(std::vector<std::string> vertexExpansions, std::vector<std::string> fragmentExpansions) :
        ShaderBase(

            VertexSource_Geometry::Expand(
                VertexSource_Geometry::EXP_VERTEX,
                vertexExpansions),

            FragmentSource_Geometry::Expand(
                FragmentSource_Geometry::EXP_FRAGMENT,
                fragmentExpansions)) 
    {
        SetUBOBindings();
    };
    
    void SetMatrices(glm::mat4 modelMatrix) const
    {
        glUniformMatrix4fv(UniformLocation("model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniformMatrix4fv(UniformLocation("normalMatrix"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(modelMatrix))));

    }

    void SetMaterial(const Material& mat, 
        const std::optional<OGLTexture2D>& albedoTexture, 
        const std::optional<OGLTexture2D>& normalsTexture,
        const std::optional<OGLTexture2D>& roughnessTexture,
        const std::optional<OGLTexture2D>& metallicTexture,
        const SceneParams& sceneParams) const
    {
        // TODO: uniform block?
        glUniform4fv(UniformLocation("material.Albedo"), 1, glm::value_ptr(mat.Albedo));
        glUniform1f(UniformLocation("material.Roughness"), mat.Roughness);
        glUniform1f (UniformLocation("material.Metallic"), mat.Metallic);

        // ***  ALBEDO color texture *** //
        // ----------------------------- //
        if (albedoTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasAlbedo"), true);

            glUniform1ui(UniformLocation("u_doGammaCorrection"), sceneParams.postProcessing.GammaCorrection);
            glUniform1f(UniformLocation("u_gamma"), 2.2f);
            
            glActiveTexture(GL_TEXTURE0 + TextureBinding::Albedo);
            albedoTexture.value().Bind();
            glUniform1i(UniformLocation("Albedo"), TextureBinding::Albedo);
        }
        else
            glUniform1ui(UniformLocation("material.hasAlbedo"), false);
        // ----------------------------- //


        // ***  NORMAL map texture *** //
        // --------------------------- //
        if (normalsTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasNormals"), true);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Normals);
            normalsTexture.value().Bind();
            glUniform1i(UniformLocation("Normals"), TextureBinding::Normals);
        }
        else
            glUniform1ui(UniformLocation("material.hasNormals"), false);
        // --------------------------- //

        // ***  ROUGHNESS map texture *** //
        // ------------------------------ //
        if (roughnessTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasRoughness"), true);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Roughness);
            roughnessTexture.value().Bind();
            glUniform1i(UniformLocation("Roughness"), TextureBinding::Roughness);
        }
        else
            glUniform1ui(UniformLocation("material.hasRoughness"), false);
        // ------------------------------ //

        // ***  METALLIC map texture *** //
        // ----------------------------- //
        if (metallicTexture.has_value())
        {
            glUniform1ui(UniformLocation("material.hasMetallic"), true);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::Metallic);
            metallicTexture.value().Bind();
            glUniform1i(UniformLocation("Metallic"), TextureBinding::Metallic);
        }
        else
            glUniform1ui(UniformLocation("material.hasMetallic"), false);
        // ----------------------------- //
    }

    void SetMaterial(const glm::vec4& diffuse, const glm::vec4& specular, float shininess) const
    {
        // TODO: uniform block?
        glUniform4fv(UniformLocation("material.Diffuse"), 1, glm::value_ptr(diffuse));
        glUniform4fv(UniformLocation("material.Specular"), 1, glm::value_ptr(specular));
        glUniform1f(UniformLocation("material.Shininess"), shininess);

    }

    void SetSamplers(SceneParams sceneParams)
    {
        // TODO bind texture once for all
        if (sceneParams.sceneLights.Directional.ShadowMapId)
        {
            glActiveTexture(GL_TEXTURE0 + TextureBinding::ShadowMap); 
            glBindTexture(GL_TEXTURE_2D, sceneParams.sceneLights.Directional.ShadowMapId);
            glUniform1i(UniformLocation("shadowMap"), TextureBinding::ShadowMap);

            // Noise textures for PCSS calculations
            glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap0);
            glBindTexture(GL_TEXTURE_2D, sceneParams.noiseTexId_0);
            glUniform1i(UniformLocation("noiseTex_0"), TextureBinding::NoiseMap0);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap1);
            glBindTexture(GL_TEXTURE_2D, sceneParams.noiseTexId_1);
            glUniform1i(UniformLocation("noiseTex_1"), TextureBinding::NoiseMap1);

            glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap2);
            glBindTexture(GL_TEXTURE_2D, sceneParams.noiseTexId_2);
            glUniform1i(UniformLocation("noiseTex_2"), TextureBinding::NoiseMap2);
        }

        if (sceneParams.sceneLights.Ambient.AoMapId)
        {
            glActiveTexture(GL_TEXTURE0 + TextureBinding::AoMap);
            glBindTexture(GL_TEXTURE_2D, sceneParams.sceneLights.Ambient.AoMapId);
            glUniform1i(UniformLocation("aoMap"), TextureBinding::AoMap);
        }
    }
    
private:
    void SetUBOBindings()
    {
        OGLUtils::CheckOGLErrors();

        unsigned int blkIndex = -1;

        constexpr static const char* uboNames[] =
        {
            "blk_PerFrameData",
            "blk_PerFrameData_Lights",
            "blk_PerFrameData_Shadows",
            "blk_PerFrameData_Ao"
        };

        for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
        {
            if ((blkIndex = glGetUniformBlockIndex(ShaderBase::ShaderProgramId(), uboNames[i])) != GL_INVALID_INDEX)
                glUniformBlockBinding(ShaderBase::ShaderProgramId(), blkIndex, i);
        };

    }

};

class PostProcessingShader : public ShaderBase
{
public:
    PostProcessingShader(std::vector<std::string> vertexExpansions, std::vector<std::string> fragmentExpansions) :
        ShaderBase(

            VertexSource_PostProcessing::Expand(
                VertexSource_PostProcessing::EXP_VERTEX,
                vertexExpansions),

            FragmentSource_PostProcessing::Expand(
                FragmentSource_PostProcessing::EXP_FRAGMENT,
                fragmentExpansions)) {};

    virtual int PositionLayout() { return 0; };
};

class PostProcessingUnit
{
 
private:
    PostProcessingShader 
        _blurShader, _viewFromDepthShader, _ssaoShader, _hbaoShader, _toneMappingAndGammaCorrection;

    std::optional<OGLTexture2D> 
        _gaussianBlurValuesTexture, _aoNoiseTexture;

    std::vector<glm::vec3> _ssaoDirectionsToSample;

    /*??? static ???*/ VertexBufferObject _vbo;
    /*??? static ???*/ VertexAttribArray _vao;

    
    void DrawQuad() const
    {
        _vao.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        _vao.UnBind();
    }

    void InitBlur(int maxRadius)
    {
        // Create a 2D texture to store gaussian blur kernel values
        // https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
        std::vector<std::vector<int>> pascalValues = ComputePascalOddRows(maxRadius);
        std::vector<int> flattenedPascalValues = Utils::flatten(pascalValues);

        _gaussianBlurValuesTexture = OGLTexture2D(
            maxRadius, maxRadius,
            TextureInternalFormat::R_16ui,
            flattenedPascalValues.data(), GL_RED_INTEGER, GL_INT);
    }


    void InitAo(int maxDirectionsToSample)
    {
        // ssao random rotation texture
        std::default_random_engine generator;
        std::uniform_real_distribution<float> distribution(-1.0, 1.0);
        std::vector<glm::vec3> ssaoNoise;
        for (unsigned int i = 0; i < 16; i++)
        {
            glm::vec3 noise(
                distribution(generator),
                distribution(generator),
                0.0f);
            ssaoNoise.push_back(noise);
        }
        std::vector<float> ssaoNoiseFlat(Utils::flatten3(ssaoNoise));

        _aoNoiseTexture = OGLTexture2D(
            4, 4,
            TextureInternalFormat::Rgb_16f,
            ssaoNoiseFlat.data(), GL_RGB, GL_FLOAT,
            TextureFiltering::Nearest, TextureFiltering::Nearest,
            TextureWrap::Repeat, TextureWrap::Repeat);


        // ssao directions (set as uniform when executing)
        _ssaoDirectionsToSample.reserve(maxDirectionsToSample);

        for (unsigned int i = 0; i < maxDirectionsToSample; i++)
        {
            glm::vec3 noise(
                distribution(generator),
                distribution(generator),
                distribution(generator) * 0.35f + 0.65f);
            noise = glm::normalize(noise);

            float x = distribution(generator) * 0.5f + 0.5f;
            noise *= Utils::Lerp(0.1, 1.0, x * x);
            _ssaoDirectionsToSample.push_back(noise);
        }

    }

public:
    PostProcessingUnit(int maxBlurRadius, int maxAODirectionsToSample) :
        _blurShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_GAUSSIAN_BLUR", "CALC_GAUSSIAN_BLUR"}),
        _viewFromDepthShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_SSAO","CALC_POSITIONS"}),
        _ssaoShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_SSAO", "CALC_SSAO"}),
        _hbaoShader(std::vector<std::string>{}, std::vector<std::string>{"DEFS_SSAO", "CALC_HBAO"}),
        _toneMappingAndGammaCorrection(
            std::vector<std::string>{},
            std::vector<std::string>{"DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION", "CALC_TONE_MAPPING_AND_GAMMA_CORRECTION"}),
        _gaussianBlurValuesTexture(),
        _vbo(VertexBufferObject::VBOType::StaticDraw),
        _vao()
    {
        // FIl VBO and VAO for the full screen quad used to trigger the fragment shader execution
        _vbo.SetData(18, Utils::fullScreenQuad_verts);
        _vao.SetAndEnableAttrib(_vbo.ID(), 0, 3, false, 0, 0);

        InitBlur(maxBlurRadius);
        InitAo(maxAODirectionsToSample);
    }

    void BlurTexture(const FrameBuffer& fbo, int attachment, int radius, int width, int height) const
    {
        // Blur pass
        glDepthMask(GL_FALSE);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.ColorTextureId(attachment));

        glActiveTexture(GL_TEXTURE1);
        _gaussianBlurValuesTexture.value().Bind();

        _blurShader.SetCurrent();

        //TODO: block?
        glUniform1i(_blurShader.UniformLocation("u_texture"), 0);
        glUniform1i(_blurShader.UniformLocation("u_weights_texture"), 1);
        glUniform1i(_blurShader.UniformLocation("u_radius"), radius);

        glUniform1i(_blurShader.UniformLocation("u_hor"), 1); // => HORIZONTAL PASS

        DrawQuad();

        fbo.CopyFromOtherFbo( nullptr, true, attachment, false, glm::vec2(0.0, 0.0), glm::vec2(width, height));

        glUniform1i(_blurShader.UniformLocation("u_hor"), 0); // => VERTICAL PASS
        
        DrawQuad();

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDepthMask(GL_TRUE);

        fbo.CopyFromOtherFbo(nullptr, true, attachment, false, glm::vec2(0.0, 0.0), glm::vec2(width, height));

    }

    void ApplyToneMappingAndGammaCorrection(const FrameBuffer& fbo, int attachment, bool toneMapping, float exposure, bool gammaCorrection, float gamma) const
    {

        glDepthMask(GL_FALSE);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.ColorTextureId(attachment));
      
        _toneMappingAndGammaCorrection.SetCurrent();

        glUniform1i(_toneMappingAndGammaCorrection.UniformLocation(" u_hdrBuffer"), 0);
        glUniform1i(_toneMappingAndGammaCorrection.UniformLocation("u_doToneMapping"), toneMapping);
        glUniform1i(_toneMappingAndGammaCorrection.UniformLocation("u_doGammaCorrection"), gammaCorrection);
        glUniform1f(_toneMappingAndGammaCorrection.UniformLocation("u_exposure"), exposure);
        glUniform1f(_toneMappingAndGammaCorrection.UniformLocation("u_gamma"), gamma);

        DrawQuad();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDepthMask(GL_TRUE);

    }

    void ComputeAO(AOType aoType, const FrameBuffer& fbo, const SceneParams& sceneParams, int width, int height) const
    {
        // Extract view positions from depth
        // the fbo should contain relevant depth information
        // at the end the fbo contains the same depth information + not blurred ao map in COLOR_ATTACHMENT0
        glDepthMask(GL_FALSE);

        fbo.Bind(false, true);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.DepthTextureId());

        _viewFromDepthShader.SetCurrent();
        glUniform1i(_viewFromDepthShader.UniformLocation("u_depthTexture"), 0);
        glUniform1f(_viewFromDepthShader.UniformLocation("u_near"), sceneParams.cameraNear);
        glUniform1f(_viewFromDepthShader.UniformLocation("u_far"), sceneParams.cameraFar);
        glUniformMatrix4fv(_viewFromDepthShader.UniformLocation("u_proj"), 1, GL_FALSE, glm::value_ptr(sceneParams.projectionMatrix));
        
        DrawQuad();

        glBindTexture(GL_TEXTURE_2D, 0);
        fbo.UnBind();

        // Compute SSAO
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.ColorTextureId());     // eye fragment positions => TEXTURE0

        glActiveTexture(GL_TEXTURE1);
        _aoNoiseTexture.value().Bind();                         // random rotation        => TEXTURE1

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, sceneParams.noiseTexId_0); // noise 0                => TEXTURE2
        glActiveTexture(GL_TEXTURE3);                           
        glBindTexture(GL_TEXTURE_2D, sceneParams.noiseTexId_1); // noise 1                => TEXTURE3
        glActiveTexture(GL_TEXTURE4);                           
        glBindTexture(GL_TEXTURE_2D, sceneParams.noiseTexId_2); // noise 2                => TEXTURE4

        const PostProcessingShader& aoShader = (aoType == AOType::SSAO ? _ssaoShader : _hbaoShader);

        aoShader.SetCurrent();

        glUniform1i(aoShader.UniformLocation("u_viewPosTexture"), 0);
        glUniform1i(aoShader.UniformLocation("u_rotVecs"), 1);
        glUniform1i(aoShader.UniformLocation("noiseTex_0"), 2);
        glUniform1i(aoShader.UniformLocation("noiseTex_1"), 3);
        glUniform1i(aoShader.UniformLocation("noiseTex_2"), 4);

        glUniform1f(aoShader.UniformLocation("u_ssao_radius"), sceneParams.sceneLights.Ambient.aoRadius);
        glUniform3fv(aoShader.UniformLocation("u_rays"), sceneParams.sceneLights.Ambient.aoSamples, &_ssaoDirectionsToSample[0].x);
        glUniform1i(aoShader.UniformLocation("u_numSamples"), sceneParams.sceneLights.Ambient.aoSamples);
        glUniform1i(aoShader.UniformLocation("u_numSteps"), sceneParams.sceneLights.Ambient.aoSteps);
        glUniform1f(aoShader.UniformLocation("u_radius"), sceneParams.sceneLights.Ambient.aoRadius);
        glUniform1f(aoShader.UniformLocation("u_near"), sceneParams.cameraNear);
        glUniform1f(aoShader.UniformLocation("u_far"), sceneParams.cameraFar);
        glUniformMatrix4fv(aoShader.UniformLocation("u_proj"), 1, GL_FALSE, glm::value_ptr(sceneParams.projectionMatrix));
        
        DrawQuad();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);

        fbo.CopyFromOtherFbo(nullptr, true, 0, false, glm::vec2(0.0, 0.0), glm::vec2(width, height));
    }

};
#endif