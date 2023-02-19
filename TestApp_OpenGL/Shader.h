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
#include "FileUtils.h"

// Texture loading
// ---------------
#include "stb_image.h"
#include <gli/gli.hpp>


namespace VertexSource_Shadows
{
    const std::string EXP_VERTEX =
        R"(
        #version 330 core

        layout(location = 0) in vec3 position;
        layout(location = 2) in vec2 textureCoordinates;
    
        uniform mat4 model;             
    
        void main()
        {
            gl_Position = model * vec4(position.x, position.y, position.z, 1.0);
        
            //vs_out.textureCoordinates = textureCoordinates;
        }
    )";
}

namespace GeometrySource_Shadows
{
    // From: https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
    const std::string EXP_GEOMETRY =
        R"(
        #version 330 core
        layout (triangles) in;
        layout (triangle_strip, max_vertices=18) out;

        uniform mat4 shadowTransforms[6];

        out vec4 FragPos; // FragPos from GS (output per emitvertex)

        void main()
        {
            for(int face = 0; face < 6; ++face)
            {
                gl_Layer = face; // built-in variable that specifies to which face we render.
                for(int i = 0; i < 3; ++i) // for each triangle vertex
                {
                    FragPos = gl_in[i].gl_Position;
                    gl_Position = shadowTransforms[face] * FragPos;
                    EmitVertex();
                }    
                EndPrimitive();
            }
        }  
        )";
}

namespace FragmentSource_Shadows
{
    // From: https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
    const std::string EXP_FRAGMENT =
        R"(
        #version 330 core
        in vec4 FragPos;

        uniform vec3 lightPos;
        uniform float far;

        void main()
        {
        // get distance between fragment and light source
        float lightDistance = length(FragPos.xyz - lightPos);
    
        // map to [0;1] range by dividing by far plane distance
        lightDistance = lightDistance / far;
    
        // write this as modified depth
        gl_FragDepth = lightDistance;
        }  
        )";
}

namespace VertexSource_Environment
{
    const std::string EXP_VERTEX =
        R"(
    #version 330 core

    #ifndef SQRT_TWO
    #define SQRT_TWO 1.41421356237;
    #endif

    layout(location = 0) in vec3 position;
    
    out vec3 fragPosWorld;

    uniform mat4 u_projection;
    uniform mat4 u_model;
    uniform bool u_hasEnvironmentMap;
    uniform vec3 u_equator_color;
    uniform vec3 u_south_color; 
    uniform vec3 u_north_color;

    uniform samplerCube EnvironmentMap;

    void main()
    {
    
        fragPosWorld = normalize(position);

        gl_Position = (u_projection * u_model * vec4(position.xyz , 1.0)).xyww;
    }
    )";
}
namespace FragmentSource_Environment
{
    const std::string EXP_FRAGMENT =
        R"(
    #version 330 core

    in vec3 fragPosWorld;

    uniform bool u_hasEnvironmentMap;
    uniform vec3 u_equator_color;
    uniform vec3 u_south_color; 
    uniform vec3 u_north_color;

    uniform bool u_doGammaCorrection;
    uniform float u_gamma;

    uniform samplerCube EnvironmentMap;

    #ifndef PI
    #define PI 3.14159265358979323846
    #endif
    
    #ifndef PI_2
    #define PI_2 1.57079632679
    #endif

    void main()
    {
        vec3 fpwN = normalize(fragPosWorld);
        float f = asin(-fpwN.y) / PI_2;

        vec3 poleColor = f > 0.0 ? u_north_color : u_south_color;

        vec3 col = 
            u_hasEnvironmentMap
                ? texture(EnvironmentMap, (fpwN * vec3(1.0, -1.0, -1.0))).rgb
                : mix(u_equator_color, poleColor, abs(f));
        
        if(u_doGammaCorrection/* && u_hasEnvironmentMap*/)
            col = pow(col, vec3(u_gamma));
        
        gl_FragColor = vec4(col, 1.0);
       
    }
    )";
}
namespace GeometrySource_Geometry
{
    const std::string THICK_POINTS =
        R"(

    #version 330 core

    layout(points) in;
    layout(triangle_strip, max_vertices = 4) out;

    // 1.0 / screen size 
    uniform vec2 u_screenToWorld;
    uniform float u_near;
    uniform uint u_thickness;

    void main() {

        float f = u_thickness;
        f *= gl_in[0].gl_Position.w;        

        // Top - Right
        gl_Position = gl_in[0].gl_Position + f * vec4(u_screenToWorld, 0.0, 0.0);
        EmitVertex();

        // Bottom - Right
        gl_Position = gl_in[0].gl_Position + f * vec4(u_screenToWorld.x, - u_screenToWorld.y, 0.0, 0.0);
        EmitVertex();

        // Top - Left
        gl_Position = gl_in[0].gl_Position + f * vec4(-u_screenToWorld.x, u_screenToWorld.y, 0.0, 0.0);
        EmitVertex();

        // Bottom - Left          
        gl_Position = gl_in[0].gl_Position + f * vec4(-u_screenToWorld, 0.0, 0.0);
        EmitVertex();

        EndPrimitive();
    }
    )";

    const std::string THICK_LINE_STRIP =
        R"(

    #version 330 core

    layout(lines_adjacency) in;
    layout(triangle_strip, max_vertices = 4) out;

    // 1.0 / screen size 
    uniform vec2 u_screenToWorld;
    uniform float u_near;
    uniform uint u_thickness;

    void main() {

        float f = u_thickness * gl_in[1].gl_Position.w;
        
        // Normal to the previous segment (adj)
        vec2 n0_ss = 
                    (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w) - 
                    (gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w);

        n0_ss = normalize((n0_ss.xy / u_screenToWorld.xy));
        n0_ss = vec2(-n0_ss.y, n0_ss.x);

        // Normal to the segment
        vec2 n1_ss = 
                    (gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w) - 
                    (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w);

        n1_ss = normalize((n1_ss.xy / u_screenToWorld.xy));
        n1_ss = vec2(-n1_ss.y, n1_ss.x);

        // Normal to the next segment (adj)
        vec2 n2_ss = 
                    (gl_in[3].gl_Position.xy/gl_in[3].gl_Position.w) - 
                    (gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w);

        n2_ss = normalize((n2_ss.xy / u_screenToWorld.xy));
        n2_ss = vec2(-n2_ss.y, n2_ss.x);

        // miter screen space
        vec2 m0_ss = normalize(n0_ss + n1_ss);
        vec2 m1_ss = normalize(n1_ss + n2_ss);

        // lower limit for the dot product to prevent miters from being too long
        float l1_ss = u_thickness/max(dot(m0_ss, n0_ss), 0.5);
        float l2_ss = u_thickness/max(dot(m1_ss, n1_ss), 0.5);


        vec4 m0_ndc = gl_in[1].gl_Position + l1_ss * (vec4(m0_ss * u_screenToWorld, 0, 0) * gl_in[1].gl_Position.w);
        vec4 m1_ndc = gl_in[1].gl_Position - l1_ss * (vec4(m0_ss * u_screenToWorld, 0, 0) * gl_in[1].gl_Position.w);
        vec4 m2_ndc = gl_in[2].gl_Position + l2_ss * (vec4(m1_ss * u_screenToWorld, 0, 0) * gl_in[2].gl_Position.w);
        vec4 m3_ndc = gl_in[2].gl_Position - l2_ss * (vec4(m1_ss * u_screenToWorld, 0, 0) * gl_in[2].gl_Position.w);

        gl_Position = l1_ss > 0
                        ? m0_ndc
                        : m1_ndc;
        EmitVertex();

        gl_Position = l1_ss > 0
                        ? m1_ndc
                        : m0_ndc;
        EmitVertex();
        
        gl_Position = l2_ss > 0
                        ? m2_ndc
                        : m3_ndc;
        EmitVertex();
   
        gl_Position = l2_ss > 0
                        ? m3_ndc
                        : m2_ndc;
        EmitVertex();

        EndPrimitive();
    }
    )";

    const std::string THICK_LINES =
        R"(

    #version 330 core

    layout(lines) in;
    layout(triangle_strip, max_vertices = 4) out;

    // 1.0 / screen size 
    uniform vec2 u_screenToWorld;
    uniform float u_near;
    uniform uint u_thickness;

    void main() {
        
        // Normal in screen space
        vec2 n0_ss = 
                    (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w) - 
                    (gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w);

        n0_ss = normalize((n0_ss.xy / u_screenToWorld.xy));
        n0_ss = vec2(-n0_ss.y, n0_ss.x);

        float f = u_thickness * gl_in[0].gl_Position.w;

        gl_Position = gl_in[0].gl_Position + f * vec4(n0_ss * u_screenToWorld, 0.0, 0.0);
        EmitVertex();

        gl_Position = gl_in[0].gl_Position - f * vec4(n0_ss * u_screenToWorld, 0.0, 0.0);
        EmitVertex();

        f = u_thickness * gl_in[1].gl_Position.w;

        gl_Position = gl_in[1].gl_Position + f * vec4(n0_ss * u_screenToWorld, 0.0, 0.0);
        EmitVertex();

        gl_Position = gl_in[1].gl_Position - f * vec4(n0_ss * u_screenToWorld, 0.0, 0.0);
        EmitVertex();

        EndPrimitive();
    }
    )";
}
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

    #ifndef MAX_POINT_LIGHTS
    #define MAX_POINT_LIGHTS 3  
    #endif

    layout (std140) uniform blk_PerFrameData_Shadows
    {
        uniform mat4 LightSpaceMatrix_directional;          // 64 byte
        uniform mat4 LightSpaceMatrixInv_directional;       // 64 byte
        uniform float bias_directional;                     // 4  byte
        uniform float slopeBias_directional;                // 4  byte
        //PAD                                               // 8  byte
        uniform float bias_point[MAX_POINT_LIGHTS];         // 48 byte
        uniform float slopeBias_point[MAX_POINT_LIGHTS];    // 48 byte
        uniform vec4  shadowCubeSize_directional;           // 16 byte
        uniform float softness;                             // 4  byte
                  
                                                            // => 260 byte
    };
)";

    const std::string CALC_SHADOWS =
        R"(
    posLightSpace = LightSpaceMatrix_directional * model * vec4(position.x, position.y, position.z, 1.0);
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

    // Diffuse IBL
    // -----------
    uniform bool u_hasIrradianceMap;
    uniform float u_environmentIntensity;
    uniform samplerCube IrradianceMap;

    // Specular IBL
    // -----------
    uniform bool u_hasRadianceMap; 
    uniform bool u_hasBrdfLut;
    uniform int u_radiance_minLevel;
    uniform int u_radiance_maxLevel;
    uniform samplerCube RadianceMap; // Convoluted radiance for specular indirect
    uniform sampler2D BrdfLut;
    

    uniform bool u_doGammaCorrection;
    uniform float u_gamma;
    
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
    vec3 ComputeDirectIllumination(vec3 Li, vec3  Wi, vec3 Wo, vec3 surfNormal, float surfRoughness, float surfMetallic, vec3 surfDiffuse)
    {
        // Halfway vector
	    vec3 h = normalize(Wo - Wi);
        float cosTheta = max(0.0, dot(h, Wo));

        vec3 F = Fresnel(cosTheta, vec3(F0_DIELECTRIC));
        F = mix(F, surfDiffuse, surfMetallic);

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
    vec3 ComputeDiffuseIndirectIllumination(vec3 normal, vec3 kd, vec3 albedo)
    {
        vec3 irradiance = vec3(1,1,1); // White environment by default

        if(u_hasIrradianceMap)
        {
            vec3 d = normalize(normal);
            d = vec3(d.x, -d.z, d.y);
            irradiance = texture(IrradianceMap, d).rbg;
        }

        return albedo * irradiance* kd * u_environmentIntensity;
    }

    // Specular indirect 
    // ---------------------------------------------------
    vec3 ComputeSpecularIndirectIllumination(vec3 N, vec3 V,  float roughness, vec3 ks)
    {
        vec3 specular = vec3(1,1,1); // White environment by default

        if(u_hasRadianceMap && u_hasBrdfLut)
        {
            vec3 d = reflect(normalize(V), normalize(N));
            d = vec3(d.x, -d.z, d.y);

            float LoD = mix(float(u_radiance_minLevel), float(u_radiance_maxLevel), roughness);
            
            vec3 Li = textureLod(RadianceMap, d, LoD).rgb;
            vec2 scaleBias = texture(BrdfLut, vec2(clamp(dot(N, V), 0.0, 1.0), roughness)).rg;

            specular = Li * (ks * scaleBias.x + scaleBias.y);

        }

        return specular * u_environmentIntensity;
    }

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

    #define BLOCKER_SEARCH_SAMPLES 16
    #define PCF_SAMPLES 64
    
    #ifndef PI
    #define PI 3.14159265358979323846
    #endif
    
    in vec4 posLightSpace;

    uniform sampler2D shadowMap_directional;
    uniform samplerCube shadowMap_point[MAX_POINT_LIGHTS];

    uniform sampler2D noiseTex_0;
    uniform sampler2D noiseTex_1;
    uniform sampler2D noiseTex_2;    
    uniform sampler1D poissonSamples;

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
        uniform mat4 LightSpaceMatrix_directional;          // 64 byte
        uniform mat4 LightSpaceMatrixInv_directional;       // 64 byte
        uniform float bias_directional;                     // 4  byte
        uniform float slopeBias_directional;                // 4  byte
        //PAD                                               // 8  byte
        uniform float bias_point[MAX_POINT_LIGHTS];         // 48 byte
        uniform float slopeBias_point[MAX_POINT_LIGHTS];    // 48 byte
        uniform vec4  shadowCubeSize_directional;           // 16 byte
        uniform float softness;                             // 4  byte
                  
                                                            // => 260 byte
    };

#ifndef UTILITY
#define UTILITY

    // *** UTILITY ***//
    // --------------------------------------------------------- //

    vec2 PoissonSample(int index)
    {
        return texelFetch(poissonSamples, index, 0).rg;
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

  
    vec2 Rotate2D(vec2 direction, float angle)
    {
        float sina = sin(angle);
        float cosa = cos(angle);
        mat2 rotation = mat2 ( cosa, sina, -sina, cosa);
        return rotation * direction;
    }
    
    // --------------------------------------------------------- //
#endif

    float SearchWidth(float uvLightSize, float receiverDistance)
    {
	    return uvLightSize;
    }

     float ShadowBias(float angle, float bias, float slopeBias)
    {
        return max(bias, slopeBias*(1.0 - cos(angle)));
    } 
   
    // Assuming the shadowMap has equal Width and Height
    float SlopeBiasForMultisampling(float angle, float offsetMagnitude, float screenToWorldFactor)
    {
        return    (offsetMagnitude * sin(angle)) / screenToWorldFactor;
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
            

    float FindBlockerDistance_DirectionalLight(vec3 lightCoord, sampler2D shadowMap, float lightSizeWorld)
    {
	    int blockers = 0;
	    float avgBlockerDistance = 0;

        float noise = RandomValue(gl_FragCoord.xy) * PI;
	    float searchWidth = lightSizeWorld;

      
        vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
	    float angle = acos(dot(worldNormal_normalized, normalize(lights.Directional.Direction)));
        float biasFac_ls = SlopeBiasForPCF(worldNormal_normalized, normalize(lights.Directional.Direction), 1);

        for(int i = 0 ; i< BLOCKER_SEARCH_SAMPLES; i++)
        {
            vec2 sampleOffset_ls  = PoissonSample(i);
            sampleOffset_ls = Rotate2D(sampleOffset_ls, noise);
            sampleOffset_ls *= searchWidth;

            vec2 sampleOffset_tex = sampleOffset_ls / shadowCubeSize_directional.xy;
            vec2 offsetCoord = NdcToUnit(lightCoord.xy) + sampleOffset_tex;
            
		    float z=texture(shadowMap, offsetCoord).r;

            float bias_ls = bias_directional
                         + biasFac_ls * length(sampleOffset_ls);
            
            float bias_tex = bias_ls / (shadowCubeSize_directional.w - shadowCubeSize_directional.z);

            if (z + bias_tex <= NdcToUnit(lightCoord).z )
            {
                blockers++;
                avgBlockerDistance += NdcToUnit(lightCoord).z - z; 
            }

        }

#ifdef DEBUG_VIZ 

            if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy&& uDebug_viewport.z == DEBUG_DIRECTIONAL_LIGHT)
            {

                vec4 center_ndc = vec4(lightCoord, 1.0);
                vec4 center_ws = LightSpaceMatrixInv_directional*center_ndc;
                
                vec3 coneDir_ws =  -normalize(lights.Directional.Direction) * SlopeBiasForPCF(worldNormal_normalized, normalize(lights.Directional.Direction), lightSizeWorld);

                DViz_DrawCone((center_ws.xyz/center_ws.w) + coneDir_ws, -coneDir_ws, lightSizeWorld, vec4(0.0, 1.0, 0.0, 0.6));

            }
#endif

	    float avgDist = blockers > 0
                            ? avgBlockerDistance / blockers
                            : -1;
		 return avgDist;
    }

    float PCF_DirectionalLight(vec3 lightCoord, sampler2D shadowMap, float radiusWorld)
    {    
        float noise = RandomValue(gl_FragCoord.xy);     
        vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
        float angle = acos(dot(worldNormal_normalized, normalize(lights.Directional.Direction)));
        float biasFac_ls = SlopeBiasForPCF(worldNormal_normalized, normalize(lights.Directional.Direction), 1);

        float sum = 0;
        for (int i = 0; i < PCF_SAMPLES; i++)
        {
            
            vec2 sampleOffset_ls  = PoissonSample(i);
            sampleOffset_ls = Rotate2D(sampleOffset_ls, noise);
            sampleOffset_ls *= radiusWorld;

            vec2 sampleOffset_tex = sampleOffset_ls / shadowCubeSize_directional.xy;
            vec2 offsetCoord = NdcToUnit(lightCoord.xy) + sampleOffset_tex;
            
		    float closestDepth=texture(shadowMap, offsetCoord).r;

            float bias_ls = bias_directional
                         + biasFac_ls * length(sampleOffset_ls);
            
            float bias_tex = bias_ls / (shadowCubeSize_directional.w - shadowCubeSize_directional.z);

          
            sum+= 
                (closestDepth + bias_tex) <= NdcToUnit(lightCoord).z 
                ? 0.0 
                : 1.0;

        }

#ifdef DEBUG_VIZ 

            if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy && uDebug_viewport.z == DEBUG_DIRECTIONAL_LIGHT)
            {

                vec4 center_ndc = vec4(lightCoord, 1.0);
                vec4 center_ws = LightSpaceMatrixInv_directional*center_ndc;
                
                vec3 coneDir_ws =  -normalize(lights.Directional.Direction) * SlopeBiasForPCF(worldNormal_normalized, normalize(lights.Directional.Direction), radiusWorld);

                DViz_DrawCone((center_ws.xyz/center_ws.w) + coneDir_ws, -coneDir_ws, radiusWorld, vec4(0.0, 1.0, 1.0, 0.6));

            }
#endif

	    return sum / PCF_SAMPLES;
    }

    float PCSS_DirectionalLight(vec3 posLightSpace, sampler2D shadowMap, float softness)
    {

        float lightSize_ls = softness*
                (posLightSpace.z * 0.5 + 0.5) * (shadowCubeSize_directional.w - shadowCubeSize_directional.z); 

       
	    // blocker search
	    float blockerDistance = FindBlockerDistance_DirectionalLight(posLightSpace, shadowMap, lightSize_ls);
	    if (blockerDistance == -1)
		    return 1.0;

        float blockerDistance_ls = blockerDistance*(shadowCubeSize_directional.w - shadowCubeSize_directional.z);
		float pcfRadius_ls = softness*(abs(blockerDistance_ls)); 
        pcfRadius_ls = min(pcfRadius_ls, lightSize_ls );
	    return PCF_DirectionalLight(posLightSpace, shadowMap, pcfRadius_ls);
       
    }

    float ComputeDirectionalShadow(vec4 fragPosLightSpace)
    {
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
        return PCSS_DirectionalLight(projCoords, shadowMap_directional, softness);
    }

    float PCSS_PointLight(vec3 posWorld,vec3 normalWorld, int lightIndex, float lightSizeWorld)
    {
        if(lights.Points[lightIndex].Color.w == 0.0) return 1.0;

        float far = lights.Points[lightIndex].Radius;    
        float noise = RandomValue(gl_FragCoord.xy);     
        vec3 worldNormal_normalized = normalize(fs_in.worldNormal);
        float lightDirLen = length(lights.Points[lightIndex].Position - posWorld);
        vec3 lightDir_nrm = normalize(lights.Points[lightIndex].Position - posWorld);

        // Creating a reference frame with light direction as Z axis
        vec3 N = lightDir_nrm;
        vec3 T = normalize(cross(N, vec3(1.453,1.029,1.567) /* LOL */));
        vec3 B = normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        
        // Cone-shaped bias (see debugViz), compute once (linear)
        float biasFac_ws = SlopeBiasForPCF(worldNormal_normalized, -lightDir_nrm, 1.0);

        // Blocker search
        float blkSrcRadius_ws = lightSizeWorld * (lightDirLen - lightSizeWorld) /lightDirLen; //mmhh...

        float avgBlkDst = 0;
        int blockers = 0;
        for (int i = 0; i < BLOCKER_SEARCH_SAMPLES; i++)
        {
            vec3 sampleOffset_ws  = TBN * vec3(Rotate2D(PoissonSample(i), noise), 0.0);
            sampleOffset_ws *= blkSrcRadius_ws;
            vec3 samplePosition_ws = posWorld + sampleOffset_ws;
            
            vec3 lightDirFromSmp = lights.Points[lightIndex].Position - samplePosition_ws;
            vec3 lightDirFromSmp_nrm = normalize(lightDirFromSmp);
            float angle = acos(dot(worldNormal_normalized, lightDirFromSmp_nrm));
            float bias_ws = bias_point[lightIndex]+ biasFac_ws * length(sampleOffset_ws);          
            float closestPoint = texture(shadowMap_point[lightIndex], -lightDirFromSmp_nrm).r * far;
  
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
                DViz_DrawCone(posWorld + coneDir_ws, -coneDir_ws, blkSrcRadius_ws, vec4(0.0, 1.0, 0.0, 0.6));
            }
#endif

        // Fully lit 
        if(blockers == 0) return 1.0;

        avgBlkDst/=blockers;

        // Percentage closer filtering
        float filterRadiusWorld = lightSizeWorld * avgBlkDst/ (lightDirLen - avgBlkDst);
        filterRadiusWorld = min(lightSizeWorld, filterRadiusWorld); //mmmhh....
        float pcfSum = 0;
        for (int i = 0; i < PCF_SAMPLES; i++)
        {
            vec3 sampleOffset_ws  = TBN * vec3(Rotate2D(PoissonSample(i), noise), 0.0);
            sampleOffset_ws *= filterRadiusWorld;
            vec3 samplePosition_ws = posWorld + sampleOffset_ws;
            
            vec3 lightDirFromSmp = lights.Points[lightIndex].Position - samplePosition_ws;
            vec3 lightDirFromSmp_nrm = normalize(lightDirFromSmp);
            float angle = acos(dot(worldNormal_normalized, lightDirFromSmp_nrm));
            float bias_ws = bias_point[lightIndex]+ biasFac_ws * length(sampleOffset_ws);          
            float closestPoint = texture(shadowMap_point[lightIndex], -lightDirFromSmp_nrm).r * far;
  
            pcfSum+=   
                (closestPoint + bias_ws) <= length(lightDirFromSmp)
                ? 0.0 
                : 1.0;
        }

#ifdef DEBUG_VIZ 

            if(uvec2(gl_FragCoord.xy) == uDebug_mouse.xy && uDebug_viewport.z == DEBUG_POINT_LIGHT)
            {
                vec3 coneDir_ws =  lightDir_nrm * biasFac_ws * filterRadiusWorld;
                DViz_DrawCone(posWorld + coneDir_ws, -coneDir_ws, filterRadiusWorld, vec4(0.0, 1.0, 1.0, 0.6));
            }
#endif


	    return pcfSum / PCF_SAMPLES;
    }



    float ComputePointShadow(int index, vec3 fragPosWorld, vec3 worldNormal)
    {
        if(lights.Points[index].Color.w == 0.0) return 1.0;

        return PCSS_PointLight(fragPosWorld, worldNormal, index,  lights.Points[index].Size);

        //float far = lights.Points[index].Radius;
        //vec3 lightVec = fragPosWorld - lights.Points[index].Position;
        //float closestPoint = texture(shadowMap_point[index], lightVec).r * far;
        //
        //vec3 worldNormal_normalized = normalize(worldNormal);
        //float angle = acos(abs(dot(worldNormal_normalized, normalize(lightVec))));
        //
        //return  
        //        (closestPoint + ShadowBias(angle, bias_point[index], slopeBias_point[index])) <= length(lightVec)
        //        ? 0.0 
        //        : 1.0;
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

    #define MAX_POINT_LIGHTS 3

    #ifndef SQRT_TWO
    #define SQRT_TWO 1.41421356237
    #endif

    struct DirectionalLight
    {
	    vec3 Direction; // 16 byte
	    vec4 Diffuse;   // 16 byte
	    vec4 Specular;  // 16 byte
                        // => 48 byte
    };

    struct PointLight
    {
	    vec4 Color;             // 16 byte
	    vec3 Position;          // 12 byte
        float Radius;	        // 4  byte
        float Size;             // 12  byte
        float InvSqrRadius;     // 4  byte
                                // => 48 byte
    };

    struct SceneLights
    {
        DirectionalLight Directional;           // 48  byte
	    vec4 Ambient;                           // 16  byte
        PointLight[MAX_POINT_LIGHTS] Points;    // 144 byte
                                                // => 208 byte      
    };

    layout(std140) uniform blk_PerFrameData_Lights
    {
        uniform SceneLights lights;     // 208 byte
        uniform vec3 eyeWorldPos;       // 16  byte
                                        // => 224 byte
    };

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


        if(u_doGammaCorrection && material.hasAlbedo)
                    diffuseColor = vec4(pow(diffuseColor.rgb, vec3(u_gamma)), diffuseColor.a);

        vec3 viewDir=normalize(eyeWorldPos - fs_in.fragPosWorld);

        vec3 worldNormal = 
            material.hasNormals
            ? normalize(fs_in.TBN * (texture(Normals, fs_in.textureCoordinates).rgb*2.0-1.0))
            : normalize (fs_in.worldNormal);
        
        // Incoming light
        vec3 Li = vec3(0);

        // Incoming directionl light + shadow contribution
        Li = lights.Directional.Diffuse.rgb *  lights.Directional.Diffuse.a * shadow_dir;
        
        // Direct lighting contribution
        vec3 direct = ComputeDirectIllumination(Li, normalize(lights.Directional.Direction), viewDir, worldNormal, roughness, metallic, diffuseColor.rgb);
        
        for(int i=0; i<MAX_POINT_LIGHTS; i++)
        {
            vec3 lightVec = fs_in.fragPosWorld - lights.Points[i].Position;
            Li = lights.Points[i].Color.rgb *  lights.Points[i].Color.a * GetDistanceAtt(lightVec, lights.Points[i].InvSqrRadius) * shadow_point[i];
            direct += ComputeDirectIllumination(Li, normalize(lightVec), viewDir, worldNormal, roughness, metallic, diffuseColor.rgb);

        }

        // Indirect diffuse
        vec3 indirectFs = FresnelRoughness(max(0.0, dot(worldNormal, viewDir)), vec3(F0_DIELECTRIC), roughness);
        indirectFs = mix(indirectFs, diffuseColor.rgb, metallic);
        vec3 indirectFd = 1.0 - indirectFs;	

        ambient = 
            vec4(
                ComputeDiffuseIndirectIllumination(worldNormal, indirectFd, diffuseColor.rgb) +
                ComputeSpecularIndirectIllumination(worldNormal, viewDir, roughness, indirectFs), 1.0);

        finalColor = vec4( direct /*ambient will be added later in the shader*/, 1.0);

)";

    const std::string CALC_UNLIT_MAT_OLD =
        R"(
        vec4 baseColor=vec4(material.Diffuse.rgb, 1.0);
        finalColor=baseColor;
)";
    const std::string CALC_UNLIT_MAT =
        R"(
        vec4 baseColor=material.Albedo.rgba;
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
        shadow_dir = ComputeDirectionalShadow(posLightSpace);
        for(int i=0; i<MAX_POINT_LIGHTS; i++)
        {
            shadow_point[i] = ComputePointShadow(i, fs_in.fragPosWorld, fs_in.worldNormal);
        }
)";

    const std::string CALC_SSAO =
        R"(
        vec2 aoMapSize=textureSize(aoMap, 0);
        float aoFac = (1.0-texture(aoMap, gl_FragCoord.xy/aoMapSize).r) * aoStrength;
        ambient*= 1.0-aoFac;
)";



    const std::string DEFS_DEBUG_VIZ =
        R"(

        #define DEBUG_VIZ

        #define DEBUG_VIZ_CONE      0
        #define DEBUG_VIZ_SPHERE    1
        #define DEBUG_VIZ_POINT     2
        #define DEBUG_VIZ_LINE      3
            
        #define DEBUG_DIRECTIONAL_LIGHT 0
        #define DEBUG_POINT_LIGHT       1

        layout(early_fragment_tests) in;

        struct debugVizItem
        {
            uvec4 u4Data;
            vec4  f4Data0;
            vec4  f4Data1;
            vec4  f4Data2;
        };
        
        layout (std140) buffer buff_debugFeedback
        {
            uvec4 uDebug_viewport;
            uvec4 uDebug_mouse;
            uvec4 uDebug_cnt;
            debugVizItem uDebug_VizItems[];
        };

        void DViz_DrawCone(vec3 start, vec3 direction, float radius, vec4 color)
        {
            if(uDebug_cnt.x < uDebug_VizItems.length()-1)
            {
                uDebug_VizItems[uDebug_cnt.x].u4Data.x  = DEBUG_VIZ_CONE;
                uDebug_VizItems[uDebug_cnt.x].f4Data0   = vec4(start, 0.0);
                uDebug_VizItems[uDebug_cnt.x].f4Data1   = vec4(direction, radius);
                uDebug_VizItems[uDebug_cnt.x].f4Data2   = color;

                atomicAdd(uDebug_cnt.x, 1);
            }
           
        }

        void DViz_DrawLine(vec3 start, vec3 end, vec4 color)
        {
            if(uDebug_cnt.x < uDebug_VizItems.length()-1)
            {
                uDebug_VizItems[uDebug_cnt.x].u4Data.x  = DEBUG_VIZ_LINE;
                uDebug_VizItems[uDebug_cnt.x].f4Data0   = vec4(start, 0.0);
                uDebug_VizItems[uDebug_cnt.x].f4Data1   = vec4(end, 0.0);
                uDebug_VizItems[uDebug_cnt.x].f4Data2   = color;

                atomicAdd(uDebug_cnt.x, 1);
            }
           
        }

)";



    const std::string EXP_FRAGMENT =
    R"(
    #version 430 core
    
    #ifndef MAX_POINT_LIGHTS
    #define MAX_POINT_LIGHTS 3
    #endif

    in VS_OUT
    {

        vec3 worldNormal;
        vec3 fragPosWorld;
        vec2 textureCoordinates;
        mat3 TBN;
    }fs_in;

    out vec4 FragColor;
    
    //[DEFS_DEBUG_VIZ]
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

        // Shadow values for all the shadow casters
        float shadow_dir = 1.0;        
        float shadow_point[MAX_POINT_LIGHTS];

        //[CALC_SHADOWS]
        //[CALC_LIT_MAT]
        //[CALC_UNLIT_MAT]	
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
        { "CALC_NORMALS",   FragmentSource_Geometry::CALC_NORMALS      },
        { "DEFS_DEBUG_VIZ", FragmentSource_Geometry::DEFS_DEBUG_VIZ    }
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
    #version 430 core
    layout(location = 0) in vec3 position;
    
#define DEBUG_AO 1

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

#if DEBUG_AO
    uniform ivec2 u_mousePos;
#endif

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


#ifndef UTILITY
#define UTILITY

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

    vec2 Rotate2D(vec2 direction, float angle)
    {
        float sina = sin(angle);
        float cosa = cos(angle);
        mat2 rotation = mat2 ( cosa, sina, -sina, cosa);
        return rotation * direction;
    }
    
#endif

#if DEBUG_AO

    // DEBUG DRAW UTILS
    // ----------------

    vec4 Blend(vec4 src, vec4 dst)
    {
        return src.a * src + (1.0 - src.a) * dst; 
    }

    // Screen Space disk
    vec4 DrawDiskSS(float radius, ivec2 center, ivec2 coords, vec4 color)
    {
        float distance = length(center - coords) / radius;
        return  color * (1.0 - step(1.0, distance));
    }
    
    // Screen Space segment
    vec4 DrawSegmentSS(ivec2 p0, ivec2 p1, ivec2 coords, vec4 color)
    {
        vec2 segDir = normalize(p1-p0);
        float projDot = dot(segDir, coords-p0);
        
        if(projDot < 0.0 || projDot > length(p1-p0))
            return vec4(0.0); 

        vec2 proj = segDir * projDot;
        vec2 perp = (coords-p0) - proj;

        float distance = length(perp);
        return color * (1.0 - step(2.0, distance));
    }

    // View Space segment
    vec4 DrawPointVS(vec3 p, ivec2 coords, mat4 proj, vec2 screenSize, vec4 color)
    {
        vec4 pClip = proj * vec4(p, 1.0);
        ivec2 pProj = ivec2( ((pClip.xy / pClip.w) + vec2(1.0)) * 0.5 *  screenSize.xy);
       
        return DrawDiskSS(5.0, pProj, coords, color);
    }

    // View Space segment
    vec4 DrawSegmentVS(vec3 p0, vec3 p1, ivec2 coords, mat4 proj, vec2 screenSize, vec4 color)
    {
        vec4 p0Clip = proj * vec4(p0, 1.0);
        vec4 p1Clip = proj * vec4(p1, 1.0);

        ivec2 p0Proj = ivec2( ((p0Clip.xy / p0Clip.w).xy + 1.0) * 0.5 * screenSize.xy);
        ivec2 p1Proj = ivec2( ((p1Clip.xy / p1Clip.w).xy + 1.0) * 0.5 * screenSize.xy);

        return DrawSegmentSS(p0Proj, p1Proj, coords, color);
    }

#endif

float EyeDepth(float depthValue, float near, float far)
{
    //http://www.songho.ca/opengl/gl_projectionmatrix.html
    float d = (depthValue * 2.0) - 1.0;
    float res = (2.0 * far * near) /  ( ( far - near ) * (d  - ((far + near)/(far - near)) ) );
   
    return res;
}

vec2 TexCoords(vec3 viewCoords, mat4 projMatrix)
{
        vec4 samplePoint_ndc = projMatrix * vec4(viewCoords, 1.0);
        samplePoint_ndc.xyz /= samplePoint_ndc.w;
        samplePoint_ndc.xyz = samplePoint_ndc.xyz * 0.5 + 0.5;
        return samplePoint_ndc.xy;
}

vec3 EyeCoords(vec2 xyNdc, float zEye)
{
    //http://www.songho.ca/opengl/gl_projectionmatrix.html

    vec2 xyClip = xyNdc * zEye;
    float xEye= (- xyClip.x + u_proj[0][2] * zEye) / u_proj[0][0];
    float yEye= (- xyClip.y + u_proj[1][2] * zEye) / u_proj[1][1];

    return vec3(xEye, yEye, zEye);
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

float OcclusionInDirection_GTAO(ivec2 cTexCoord, vec3 cPosV, vec3 cNormalV, int sliceCount
#if DEBUG_AO
    , vec2 texSize, inout vec4 debugView
#endif
)
{
    // https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
    // Algorithm 1
    
    vec3 viewV = normalize(-cPosV); 
    float visibility = 0.0; // AO 
    
    for(int i = 0; i < sliceCount; i++)
    {
        float phi =  i * PI/sliceCount;
        vec2 omega = vec2(cos(phi), sin(phi));

        vec3 directionV = vec3(omega, 0.0);
        vec3 orthoDirectionV = directionV - dot(directionV, viewV)*viewV;
        vec3 axisV = cross(directionV, viewV);
        vec3 projNormalV = cNormalV - axisV*dot(cNormalV, axisV);
        
        float sgnGamma = sign(dot(orthoDirectionV, projNormalV));
        float cosGamma = clamp( dot(projNormalV, viewV)/length(projNormalV), 0.0, 1.0);
        float gamma = sgnGamma * acos(cosGamma); // TODO: fast acos

#if DEBUG_AO

        // Marching direction
        debugView = Blend(DrawSegmentVS(
                    cPosV, cPosV + directionV, 
                    ivec2(gl_FragCoord.xy), 
                    u_proj, texSize, vec4(0.0, 1.0, 0.0, 0.6)), debugView);

        // Normal
        debugView = Blend(DrawSegmentVS(
                    cPosV, cPosV + cNormalV, 
                    ivec2(gl_FragCoord.xy), 
                    u_proj, texSize, vec4(cNormalV, 1.0)), debugView);

        // Projected Normal
        debugView = Blend(DrawSegmentVS(
                    cPosV, cPosV + projNormalV, 
                    ivec2(gl_FragCoord.xy), 
                    u_proj, texSize, vec4(cNormalV, 0.6)), debugView);

#endif

    }

    return 0.0;   
}

//https://www.researchgate.net/publication/215506032_Image-space_horizon-based_ambient_occlusion
//https://developer.download.nvidia.com/presentations/2008/SIGGRAPH/HBAO_SIG08b.pdf
float OcclusionInDirection_NEW(vec3 p, vec3 normal, vec2 directionImage, float radius, float radiusImage, int numSteps, sampler2D viewCoordsTex, mat4 projMatrix)
{
       
       float increment = radiusImage / numSteps;
       float bias = (PI/6);
       
       vec2 texSize = textureSize(viewCoordsTex, 0).xy;
       
       float wao = 0;
       float ao = 0;
       vec2 fragCoord = TexCoords(p, projMatrix);    

       vec3 D = vec3(0.0, 0.0, 0.0);
       for (int i=1; i<=numSteps; i++)
       {
            float offset = 0.0;//RandomValue(vec2(fragCoord + (directionImage * increment * i)) * texSize) * increment; 
            vec2 sampleUV = fragCoord + (directionImage * (increment + abs(offset)) * i) * vec2(1.0, (texSize.x/texSize.y));
            
            vec3 smpl =  texture(viewCoordsTex, sampleUV).xyz;
            D = smpl - p;
            float l=length(D);

            vec3 directionImage3d = vec3(directionImage, 0.0);
            vec3 tangent = normalize(directionImage3d - normal * dot(directionImage3d, normal));

            float t = min(tangent.z,length(tangent.xy));
            float m = t;

            float h = min(-D.z,length(D.xy));

            if((h + bias <  m)&&dot(directionImage3d, normal) > 0.01)
            {
                m = h;
                wao += ((sin(t) - sin(m)) - ao) * Attenuation(l, radius);
                ao = (sin(t) - sin(m));
            }
       }

       
       return wao;
}

    // A lot of problems at grazing angles...
    vec3 EyeNormal_dz(vec2 uvCoords, vec3 eyePos)
    {
        vec2 texSize = textureSize(u_viewPosTexture, 0).xy;
        ivec2 fetchCoords = ivec2(uvCoords * texSize);
        
        int searchRadius = 3;
        
        float p_dzdx =  texelFetchOffset(u_viewPosTexture, fetchCoords , 0, ivec2( searchRadius,  0)).z - eyePos.z;
        float p_dzdy =  texelFetchOffset(u_viewPosTexture, fetchCoords , 0, ivec2( 0,  searchRadius)).z - eyePos.z;
        float m_dzdx = -texelFetchOffset(u_viewPosTexture, fetchCoords , 0, ivec2(-searchRadius,  0)).z + eyePos.z;
        float m_dzdy = -texelFetchOffset(u_viewPosTexture, fetchCoords , 0, ivec2( 0, -searchRadius)).z + eyePos.z;
        
        bool px = abs(p_dzdx)<abs(m_dzdx);
        bool py = abs(p_dzdy)<abs(m_dzdy);

        vec3 i = texelFetch(u_viewPosTexture, fetchCoords + ivec2(px ? +searchRadius : -searchRadius, 0),0).xyz - eyePos ;
        vec3 j = texelFetch(u_viewPosTexture, fetchCoords + ivec2(0, py ? +searchRadius : -searchRadius),0).xyz - eyePos ; 

                

        vec3 normal = normalize( cross( (px^^py ? j : i), (px^^py ? i : j)) ); 
        return  normal;
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
    
    vec2 texSize=textureSize(u_viewPosTexture, 0);
    vec2 uvCoords = gl_FragCoord.xy / texSize;

    vec3 eyePos = texture(u_viewPosTexture, uvCoords).rgb;
    
    if(-eyePos.z>u_far-0.1)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 normal = EyeNormal_dz(uvCoords, eyePos);
    
    vec3 rotationVec = vec3(1.0,0.0,0.0); //texture(u_rotVecs, uvCoords * texSize/NOISE_SIZE).rgb;
    vec3 tangent     = normalize(rotationVec - normal * dot(rotationVec, normal));
    vec3 bitangent   = cross(normal, tangent);
    mat3 TBN         = mat3(tangent, bitangent, normal); 
    
    float ao;
    float increment = ((2.0*PI)/u_numSamples);
    
    vec4 projectedRadius = (u_proj * vec4(u_radius, 0, eyePos.z, 1));
    float radiusImage = (projectedRadius.x/projectedRadius.w)*0.5;
    
    float offset = 0.0;//RandomValue() * increment;

    for(int k=0; k < u_numSamples; k++)
    {
        //TODO: pass the directions as uniforms???
        float angle= (increment * k) + offset;
        vec2 sampleDirectionImage = vec2(cos(angle), sin(angle));
        ao +=  OcclusionInDirection_NEW(eyePos, normal, sampleDirectionImage,  u_radius, radiusImage, u_numSteps, u_viewPosTexture, u_proj);
               
    }
    ao /= u_numSamples;

   
    FragColor = vec4(vec3(1.0-ao),1.0); 
)";

    const std::string CALC_GTAO =
        R"(
    
    vec2 texSize=textureSize(u_viewPosTexture, 0);
    vec2 uvCoords = gl_FragCoord.xy / texSize;

    vec3 eyePos = texture(u_viewPosTexture, uvCoords).rgb ;
    
    if( -eyePos.z > u_far - 0.001)
    {
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        //return;
    }

    vec3 normal = EyeNormal_dz(uvCoords, eyePos);
    
    vec4 projectedRadius = (u_proj * vec4(u_radius, 0, eyePos.z, 1));
    float radiusImage = (projectedRadius.x/projectedRadius.w)*0.5;
    
    vec4 res  = vec4(vec3(-eyePos.z/u_far), 1.0);    
    
#if DEBUG_AO

    vec2 mouseTexCoord = u_mousePos/texSize;
    vec3 mousePosV = texture(u_viewPosTexture, mouseTexCoord).rgb;
    vec3 mouseNormalV = EyeNormal_dz(mouseTexCoord, mousePosV);

    OcclusionInDirection_GTAO(u_mousePos, mousePosV, mouseNormalV, 1, texSize, res);
   
#endif

    FragColor = res; 
)";
    const std::string CALC_POSITIONS =
        R"(

    ivec2 texSize=textureSize(u_depthTexture, 0);

    float depthValue = texture(u_depthTexture, gl_FragCoord.xy/vec2(texSize) ).r;
    float zEye =  EyeDepth( depthValue, u_near, u_far );
    vec3 eyePos = EyeCoords((gl_FragCoord.xy/texSize.xy) * 2.0 - 1.0, zEye);

    FragColor = vec4(eyePos, 1.0); 

)";

    const std::string CALC_TONE_MAPPING_AND_GAMMA_CORRECTION =
        R"(

    vec4 col = texelFetch(u_hdrBuffer, ivec2(gl_FragCoord.xy - vec2(0.5)), 0).rgba;
    
    if(u_doToneMapping)
        col = vec4(vec3(1.0) - exp(-col.rgb * u_exposure), col.a);
    
    if(u_doGammaCorrection)
        col = vec4(pow(col.rgb, vec3(1.0/u_gamma)), col.a);

    FragColor = col;
)";

    const std::string DEFS_BRDF_LUT =
R"(
   
    uniform ivec2 u_screenSize;    

    #define NUM_SAMPLES_BRDF_LUT 1024

    #ifndef PI
    #define PI 3.14159265358979323846
    #endif

    // Keep in sync with the functions used for direct illumination 
    float G_GGXS_IBL(float NoV, float roughness)
    {   
        float k = (roughness*roughness) / 2.0;     
        float dot = clamp(NoV, 0.0, 1.0);

        return dot / (dot * (1.0 - k) + k);
    }
	
	float G_GGXS_IBL(float NoV, float NoL, float roughness)
    {
       return G_GGXS_IBL(NoV, roughness) * G_GGXS_IBL(NoL, roughness);    
    }
    
	// Importance sampling (based on roughness)
    // ----------------------------------------
    vec3 ImportanceSampleGGX( vec2 Xi, float Roughness, vec3 N )
    {
        // Source: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

        float a = Roughness * Roughness;
        float Phi = 2 * PI * Xi.x;
        float CosTheta = sqrt( (1 - Xi.y) / ( 1 + (a*a - 1) * Xi.y ) );
        float SinTheta = sqrt( 1 - CosTheta * CosTheta );
        
        vec3 H;
        H.x = SinTheta * cos( Phi );
        H.y = SinTheta * sin( Phi );
        H.z = CosTheta;

        vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
        vec3 TangentX = normalize( cross( UpVector, N ) );
        vec3 TangentY = cross( N, TangentX );

        // Tangent to world space
        return TangentX * H.x + TangentY * H.y + N * H.z;
    }

    float RadicalInverse_VdC(uint bits) 
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10; // / 0x100000000
    }
    
    vec2 Hammersley(uint i, uint N)
    {
        return vec2(float(i)/float(N), RadicalInverse_VdC(i));
    } 

)";

    const std::string CALC_BRDF_LUT =
R"(
    // Source: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
    // Schlick's approximation
    // F0 + (1 - F0)(1 - (h*v))^5 => F0(1 - (1-(h*v)^5)) + (1-(h*v)^5) 
    
    
    vec2 nrmWinCoord = gl_FragCoord.xy/u_screenSize;

    // Range [0-1]
    float roughness = nrmWinCoord.y;
    float NoV = nrmWinCoord.x;

    vec3 V = vec3(0.0);
    V.x = sqrt( 1.0f - NoV * NoV ); // sin
    V.y = 0.0;
    V.z = NoV;                      // cos

    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;

    
    for( uint i = 0; i < NUM_SAMPLES_BRDF_LUT; i++ )
    {
        vec2 Xi = Hammersley( i, NUM_SAMPLES_BRDF_LUT ); // generate a low discrepance sequence
        vec3 H = ImportanceSampleGGX( Xi, roughness, N ); // use the sequence element to get vectors biased towards the halfway vec
        vec3 L = -reflect(V, H);

        float NoL = clamp( L.z, 0.0, 1.0 );
        float NoH = clamp( H.z, 0.0, 1.0 );
        float VoH = clamp( dot( V, H ), 0.0, 1.0);

        if( NoL > 0 )
        {
            float G = G_GGXS_IBL( NoV, NoL, roughness);
            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = pow( 1 - VoH, 5 );
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    vec2 val = vec2( A, B ) / NUM_SAMPLES_BRDF_LUT;

    FragColor = vec4(val, 0.0, 0.0);
)";

    const std::string EXP_FRAGMENT =
        R"(
    #version 430 core

    #define DEBUG_AO 1

    out vec4 FragColor;

    //[DEFS_SSAO]
    //[DEFS_BLUR]
    //[DEFS_GAUSSIAN_BLUR]
    //[DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION]  
    //[DEFS_MATERIAL]
    //[DEFS_BRDF_LUT]

    void main()
    {
        //[CALC_POSITIONS]
        //[CALC_SSAO]
        //[CALC_HBAO]
        //[CALC_GTAO]
        //[CALC_BLUR]
        //[CALC_GAUSSIAN_BLUR]
        //[CALC_TONE_MAPPING_AND_GAMMA_CORRECTION]
        //[CALC_BRDF_LUT]
    }
    )";

    static std::map<std::string, std::string> Expansions = {

       { "DEFS_SSAO",                                   FragmentSource_PostProcessing::DEFS_AO                                  },
       { "DEFS_BLUR",                                   FragmentSource_PostProcessing::DEFS_BLUR                                },
       { "DEFS_GAUSSIAN_BLUR",                          FragmentSource_PostProcessing::DEFS_GAUSSIAN_BLUR                       },
       { "DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION",      FragmentSource_PostProcessing::DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION   },
       { "DEFS_BRDF_LUT",                               FragmentSource_PostProcessing::DEFS_BRDF_LUT                            },
       { "CALC_POSITIONS",                              FragmentSource_PostProcessing::CALC_POSITIONS                           },
       { "CALC_SSAO",                                   FragmentSource_PostProcessing::CALC_SSAO                                },
       { "CALC_HBAO",                                   FragmentSource_PostProcessing::CALC_HBAO                                },
       { "CALC_GTAO",                                   FragmentSource_PostProcessing::CALC_GTAO                                },
       { "CALC_BLUR",                                   FragmentSource_PostProcessing::CALC_BLUR                                },
       { "CALC_GAUSSIAN_BLUR",                          FragmentSource_PostProcessing::CALC_GAUSSIAN_BLUR                       },
       { "CALC_TONE_MAPPING_AND_GAMMA_CORRECTION",      FragmentSource_PostProcessing::CALC_TONE_MAPPING_AND_GAMMA_CORRECTION   },
       { "CALC_BRDF_LUT",                               FragmentSource_PostProcessing::CALC_BRDF_LUT                            }

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
    GEOMETRY_SHADER,
    SHADER_PROGRAM,
    VERTEX_BUFFER_OBJECT,
    VERTEX_ATTRIB_ARRAY,
    INDEX_BUFFER,
    UNIFORM_BUFFER,
    SHADER_STORAGE_BUFFER,
    TEXTURE,
    FRAMEBUFFER,
    SAMPLER
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


namespace OGLTextureUtils
{


    enum TextureFiltering
    {
        Nearest = GL_NEAREST,
        Linear = GL_LINEAR,

        // NOT ALLOWED FOR MAGNIFICATION
        Nearest_Mip_Nearest = GL_NEAREST_MIPMAP_NEAREST,
        Nearest_Mip_Linear = GL_NEAREST_MIPMAP_LINEAR,
        Linear_Mip_Nearest = GL_LINEAR_MIPMAP_NEAREST,
        Linear_Mip_Linear = GL_LINEAR_MIPMAP_LINEAR
    };

    enum TextureWrap
    {
        Clamp_To_Edge = GL_CLAMP_TO_EDGE,
        Repeat = GL_REPEAT

    };

    enum TextureInternalFormat
    {
        Depth_Component = GL_DEPTH_COMPONENT,
        Depth_Stencil = GL_DEPTH_STENCIL,
        R = GL_RED,
        R_16f = GL_R16F,
        R_32f = GL_R32F,
        R_16ui = GL_R16UI,
        Rg = GL_RG,
        Rg_16f = GL_RG16F,
        Rgb_16f = GL_RGB16F,
        Rgba = GL_RGBA,
        Rgb = GL_RGB,
        Rgba_32f = GL_RGBA32F,
        Rgba_16f = GL_RGBA16F

    };

    enum TextureType
    {
        Texture1D = GL_TEXTURE_1D,
        Texture2D = GL_TEXTURE_2D,
        CubeMap = GL_TEXTURE_CUBE_MAP
    };


    void Init(unsigned int texture, TextureType textureType, GLenum target, int level, TextureInternalFormat internalFormat, int width, int height, GLenum format, GLenum type, const void* data)
    {

        glBindTexture(textureType, texture);

        switch (textureType)
        {
        case(TextureType::Texture2D):
        case(TextureType::CubeMap):

            //if (level == 0)
                glTexImage2D(target, level, internalFormat,
                    width, height, 0, format, type, data);
            //else
            //    glTexSubImage2D(target, level, 0, 0,
            //        width, height, format, type, data);

            break;

        case(TextureType::Texture1D):

            glTexImage1D(target, level, internalFormat,
                width, 0, format, type, data);
            break;

        default:
            throw "Unsupported texture type. Allowed typese are: Texture1D, Texture2D, Cubemap.";
        }

        OGLUtils::CheckOGLErrors();

        glBindTexture(textureType, 0);
    }

    void Init(unsigned int texture, TextureType textureType, int level, TextureInternalFormat internalFormat, int width, int height, GLenum format, GLenum type, const void* data)
    {
        Init(texture, textureType, textureType, level, internalFormat, width, height, format, type, data);
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

    void SetMinMaxLevel(unsigned int texture, TextureType textureType, int minLevel, int maxLevel)
    {
        glBindTexture(textureType, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, minLevel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxLevel);

        glBindTexture(textureType, 0);
    }

    void SetParameters(unsigned int texture, TextureType textureType, TextureFiltering minfilter, TextureFiltering magfilter, TextureWrap wrapS, TextureWrap wrapT)
    {
        glBindTexture(textureType, texture);

        glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, minfilter);
        glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, magfilter);
        glTexParameteri(textureType, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(textureType, GL_TEXTURE_WRAP_T, wrapT);


        if (NeedsMips(minfilter))
            glGenerateMipmap(textureType);

        OGLUtils::CheckOGLErrors();

        glBindTexture(textureType, 0);
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
        case(TextureInternalFormat::Rg_16f):
            return GL_RG;
            break;
        case(TextureInternalFormat::R_32f):
        case(TextureInternalFormat::R_16f):
        case(TextureInternalFormat::R_16ui):
            return GL_RED;
            break;
        default:
            return internal;
            break;
        }
    }
}

namespace OGLResources
{

    namespace OglBuffer
    {

        enum class Usage
        {
            Undefined,
            StaticDraw,
            DynamicDraw,
            StreamDraw,
            StaticRead,
            DynamicRead,
            StreamRead
        };


        GLenum ResolveUsage(Usage usage)
        {
            switch (usage)
            {
            case(Usage::StaticDraw):  return GL_STATIC_DRAW;  break;
            case(Usage::DynamicDraw): return GL_DYNAMIC_DRAW; break;
            case(Usage::StreamDraw):  return GL_STREAM_DRAW;  break;
            case(Usage::StaticRead):  return GL_STATIC_READ;  break;
            case(Usage::DynamicRead): return GL_DYNAMIC_READ; break;
            case(Usage::StreamRead):  return GL_STREAM_READ;  break;
            default:
                return -1;
                break;
            }
        }

        void Bind(unsigned int target, unsigned int id)
        {
            glBindBuffer(target, id);

            OGLUtils::CheckOGLErrors();
        }

        void UnBind(unsigned int target)
        {
            glBindBuffer(target, 0);

            OGLUtils::CheckOGLErrors();
        }

        template<typename T>
        void SetData(unsigned int target, unsigned int id, unsigned int count, const T* data, unsigned int usage)
        {
            Bind(target, id);
            glBufferData(target, count * sizeof(T), data != nullptr ? data : NULL, usage);
            UnBind(target);

            OGLUtils::CheckOGLErrors();
        }

        template<typename T>
        void SetSubData(unsigned int target, unsigned int id, unsigned int startIndex, unsigned int count, const T* data)
        {
            OGLUtils::CheckOGLErrors();

            Bind(target, id);
            glBufferSubData(target, startIndex * sizeof(T), count * sizeof(T), data != nullptr ? data : NULL);
            UnBind(target);

            OGLUtils::CheckOGLErrors();
        }

        template<typename T>
        void ReadData(unsigned int target, unsigned int id, unsigned int startIndex, unsigned int count, T *destination)
        {
            OGLUtils::CheckOGLErrors();

            Bind(target, id);
            memcpy(destination, (T*) glMapBufferRange(target, startIndex * sizeof(T), count * sizeof(T), GL_MAP_READ_BIT), count * sizeof(T));
            glUnmapBuffer(target);
            UnBind(target);

            OGLUtils::CheckOGLErrors();
        }

        // Since we need a format, a generic implementation may not be so flexible? better to be explicit with the format? 
        void ClearData(unsigned int target, unsigned int id, unsigned int internalFormat, unsigned int format, unsigned int type, const void* data)
        {
            OGLUtils::CheckOGLErrors();

            Bind(target, id);
            glClearBufferData(target, internalFormat, format, type, data);
            UnBind(target);

            OGLUtils::CheckOGLErrors();
        }

        void SetBindingPoint(unsigned int target, unsigned int id, unsigned int index)
        {
            glBindBufferBase(target, index, id);
        }
    }

    using namespace OglBuffer;

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
            if (_id)
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

            case (OGLResourceType::GEOMETRY_SHADER):
                if (!destroy)
                    _id = glCreateShader(GL_GEOMETRY_SHADER);
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
            case (OGLResourceType::SHADER_STORAGE_BUFFER):
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
            case(OGLResourceType::SAMPLER):
                if (!destroy)
                    glGenSamplers(1, &_id);
                else
                {
                    glDeleteSamplers(1, &_id);
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

            case (OGLResourceType::GEOMETRY_SHADER):
                return "GEOMETRY_SHADER";
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

            case (OGLResourceType::SHADER_STORAGE_BUFFER):
                return "SHADER_STORAGE_BUFFER";
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

            case(OGLResourceType::SAMPLER):
                return "SAMPLER";
                break;

            default:
                throw "Unknown ogl resource type.";
            }
        }
        void NotifyCreation()    { std::cout << std::endl << "Creating OGL Object: "   + ResourceType() + "_" + std::to_string(ID()); }
        void NotifyDestruction() { std::cout << std::endl << "Destroying OGL Object: " + ResourceType() + "_" + std::to_string(ID()); }

    };

    class VertexBufferObject : public OGLResource
    {
   

    public:

        VertexBufferObject(Usage type) : _type(type)
        {
            OGLResource::Create(OGLResourceType::VERTEX_BUFFER_OBJECT);
        }

        VertexBufferObject(VertexBufferObject&& other) noexcept : OGLResource(std::move(other))
        {
            this->_type = other._type;
            other._type = Usage::Undefined;
        };

        VertexBufferObject& operator=(VertexBufferObject&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                this->_type = other._type;
                other._type = Usage::Undefined;
            }
            return *this;
        };

        ~VertexBufferObject()
        {
            OGLResource::Destroy();
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
        Usage _type;
    };

    class IndexBufferObject : public OGLResource
    {
    
    public:
        IndexBufferObject(Usage type) : _type(type)
        {
            OGLResource::Create(OGLResourceType::INDEX_BUFFER);
        }

        IndexBufferObject(IndexBufferObject&& other) noexcept : OGLResource(std::move(other))
        {
            this->_type = other._type;
            other._type = Usage::Undefined;
        };

        IndexBufferObject& operator=(IndexBufferObject&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                this->_type = other._type;
                other._type = Usage::Undefined;
            }
            return *this;
        };

        ~IndexBufferObject()
        {
            OGLResource::Destroy();
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
        Usage _type;
    };

    class UniformBufferObject : public OGLResource
    {
    
    public:
        UniformBufferObject(Usage type) : _type(type)
        {
            OGLResource::Create(OGLResourceType::UNIFORM_BUFFER);
        }

        UniformBufferObject(UniformBufferObject&& other) noexcept : OGLResource(std::move(other))
        {
            this->_type = other._type;
            other._type = Usage::Undefined;
        };

        UniformBufferObject& operator=(UniformBufferObject&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                this->_type = other._type;
                other._type = Usage::Undefined;
            }
            return *this;
        };

        ~UniformBufferObject()
        {
            OGLResource::Destroy();
        }


        void Bind()
        {
            OglBuffer::Bind(GL_UNIFORM_BUFFER, ID());
        }

        void UnBind()
        {
            OglBuffer::UnBind(GL_UNIFORM_BUFFER);
        }

        template<typename T>
        void SetData(unsigned int count, const T* data)
        {   
            OglBuffer::SetData(GL_UNIFORM_BUFFER,ID(),  count, data != nullptr ? data : NULL, ResolveUsage(_type));        
        }

        template<typename T>
        void SetSubData(unsigned int startIndex, unsigned int count, const T* data)
        {
            OglBuffer::SetSubData(GL_UNIFORM_BUFFER,ID(),  startIndex , count , data != nullptr ? data : NULL);
        }

        void SetBindingPoint(unsigned int index)
        {
            OglBuffer::SetBindingPoint(GL_UNIFORM_BUFFER, ID(), index);
        }

    private:
        Usage _type;
    };

    class ShaderStorageBufferObject : public OGLResource
    {

    public:
        ShaderStorageBufferObject(Usage type) : _type(type)
        {
            OGLResource::Create(OGLResourceType::SHADER_STORAGE_BUFFER);
        }

        ShaderStorageBufferObject(ShaderStorageBufferObject&& other) noexcept : OGLResource(std::move(other))
        {
            this->_type = other._type;
            other._type = Usage::Undefined;
        };

        ShaderStorageBufferObject& operator=(ShaderStorageBufferObject&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                this->_type = other._type;
                other._type = Usage::Undefined;
            }
            return *this;
        };

        ~ShaderStorageBufferObject()
        {
            OGLResource::Destroy();
        }

        void Bind()
        {
            OglBuffer::Bind(GL_SHADER_STORAGE_BUFFER, ID());
        }

        void UnBind()
        {
            OglBuffer::UnBind(GL_SHADER_STORAGE_BUFFER);
        }

        template<typename T>
        void SetData(unsigned int count, const T* data)
        {
            OglBuffer::SetData(GL_SHADER_STORAGE_BUFFER, ID(), count , data != nullptr ? data : NULL, OglBuffer::ResolveUsage(_type));
        }

        template<typename T>
        void SetSubData(unsigned int startIndex, unsigned int count, const T* data)
        {
            OglBuffer::SetSubData(GL_SHADER_STORAGE_BUFFER, ID(), startIndex , count , data != nullptr ? data : NULL);
        }

        void SetBindingPoint(unsigned int index)
        {
            OglBuffer::SetBindingPoint(GL_SHADER_STORAGE_BUFFER, ID(), index);
        }

        template<typename T>
        void ReadData(unsigned int startIndex, unsigned int count, T* destination)
        {
            OglBuffer::ReadData(GL_SHADER_STORAGE_BUFFER, ID(), startIndex, count, destination);
        }

        void ClearData(OGLTextureUtils::TextureInternalFormat internalFormat, unsigned int type, const void* data)
        {
            OglBuffer::ClearData(GL_SHADER_STORAGE_BUFFER, ID(), internalFormat, OGLTextureUtils::ResolveFormat(internalFormat), type, data);
        }

    private:
        Usage _type;
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

    class OGLGeometryShader : public OGLResource
    {
    private:
        std::string _source;

    public:
        OGLGeometryShader(std::string source) : _source{ source }
        {
            OGLResource::Create(OGLResourceType::GEOMETRY_SHADER);

            const char* gsc = source.data();
            glShaderSource(OGLResource::ID(), 1, &gsc, NULL);
            glCompileShader(OGLResource::ID());
            OGLUtils::CheckCompileErrors(OGLResource::ID(), ResourceType());
            OGLUtils::CheckOGLErrors();
        }

        OGLGeometryShader(OGLGeometryShader&& other) noexcept : OGLResource(std::move(other))
        {
            _source = std::move(other._source);
        };

        OGLGeometryShader& operator=(OGLGeometryShader&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                _source = std::move(other._source);
            }
            return *this;
        };

        ~OGLGeometryShader()
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
        ShaderProgram(const std::string vShaderCode, std::string fShaderCode, std::string gShaderCode)
        {

            OGLVertexShader vertex(vShaderCode);
            OGLFragmentShader fragment(fShaderCode);

            OGLResource::Create(OGLResourceType::SHADER_PROGRAM);

            glAttachShader(OGLResource::ID(), vertex.ID());
            glAttachShader(OGLResource::ID(), fragment.ID());

            if (!gShaderCode.empty())
            {
                OGLGeometryShader geometry(gShaderCode);
                glAttachShader(OGLResource::ID(), geometry.ID());
            }

            glLinkProgram(OGLResource::ID());
            OGLUtils::CheckLinkErrors(OGLResource::ID(), ResourceType());

            OGLUtils::CheckOGLErrors();
        }

        ShaderProgram(const std::string vShaderCode, std::string fShaderCode) : ShaderProgram(vShaderCode, fShaderCode, "")
        {

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

    using  namespace OGLTextureUtils;

    class OGLTexture2D : public OGLResource
    {
        

    private:
        int
            _width, _height;

        static const TextureType _textureType = TextureType::Texture2D;

    public:
        OGLTexture2D(int width, int height, TextureInternalFormat internalFormat) :
            OGLTexture2D(width, height, internalFormat, TextureFiltering::Nearest, TextureFiltering::Nearest)
        { 
        
        }

        OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, TextureFiltering minFilter, TextureFiltering magFilter,
            TextureWrap wrapS, TextureWrap wrapT) 
            : _width(width), _height(height)
        {
            OGLResource::Create(OGLResourceType::TEXTURE);

            OGLTextureUtils::Init(
                OGLResource::ID(), _textureType,
                0, internalFormat, width, height, OGLTextureUtils::ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, NULL);

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                minFilter, magFilter,
                wrapS, wrapT);

            OGLUtils::CheckOGLErrors();
        }

        OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, TextureFiltering minFilter, TextureFiltering magFilter)
            : OGLTexture2D(width, height, internalFormat, minFilter, magFilter, Clamp_To_Edge, Clamp_To_Edge)
        {
        }

        OGLTexture2D(const char* path, TextureFiltering minFilter, TextureFiltering magFilter, TextureWrap wrapS, TextureWrap wrapT)
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
            case(1): internalFormat = TextureInternalFormat::R; break;
            case(3): internalFormat = TextureInternalFormat::Rgb; break;
            case(4): internalFormat = TextureInternalFormat::Rgba; break;
            default:
                throw "Unsupported texture format."; break;
            }

            OGLTextureUtils::Init(
                OGLResource::ID(), _textureType,
                0, internalFormat, width, height, OGLTextureUtils::ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, data);

            OGLUtils::CheckOGLErrors();

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                minFilter, magFilter,
                wrapS, wrapT);

            OGLUtils::CheckOGLErrors();

            stbi_image_free(data);
        }

        OGLTexture2D(const char* path, TextureFiltering minFilter, TextureFiltering magFilter)
	        :OGLTexture2D(path, minFilter, magFilter, Clamp_To_Edge, Clamp_To_Edge)
        {
        }

        OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, void* data, GLenum dataFormat, GLenum type)
        {
            OGLResource::Create(OGLResourceType::TEXTURE);

            OGLTextureUtils::Init(
                OGLResource::ID(), _textureType,
                0, internalFormat, width, height, dataFormat, type, data);

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                TextureFiltering::Nearest, TextureFiltering::Nearest,
                TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);

            OGLUtils::CheckOGLErrors();
        }

        OGLTexture2D(int width, int height, TextureInternalFormat internalFormat, void* data, GLenum dataFormat, GLenum type,
            TextureFiltering minFilter, TextureFiltering magFilter, TextureWrap wrapS, TextureWrap wrapT)
        {
            OGLResource::Create(OGLResourceType::TEXTURE);

            OGLTextureUtils::Init(
                OGLResource::ID(), _textureType,
                0, internalFormat, width, height, dataFormat, type, data);

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                minFilter, magFilter, wrapS, wrapT);

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

    class OGLTexture1D : public OGLResource
    {

    private:
        int _width;
        static const TextureType _textureType = TextureType::Texture1D;

    public:
        OGLTexture1D(int width, TextureInternalFormat internalFormat, float* data) : _width(width)
        {
            OGLResource::Create(OGLResourceType::TEXTURE);

            OGLTextureUtils::Init(
                OGLResource::ID(), _textureType,
                0, internalFormat, width, 0, OGLTextureUtils::ResolveFormat(internalFormat), GL_FLOAT, data);

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                TextureFiltering::Nearest, TextureFiltering::Nearest,
                TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);

            OGLUtils::CheckOGLErrors();
        }

        OGLTexture1D(OGLTexture1D&& other) noexcept : OGLResource(std::move(other))
        {
            _width = other._width;
        };

        void Bind() const
        {
            glBindTexture(GL_TEXTURE_1D, OGLResource::ID());
        }

        void UnBind() const
        {
            glBindTexture(GL_TEXTURE_1D, 0);
        }

        OGLTexture1D& operator=(OGLTexture1D&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                _width = other._width;
            }
            return *this;
        };

        ~OGLTexture1D()
        {
            OGLResource::Destroy();
        }

    };

    class OGLTextureCubemap : public OGLResource
    {

    private:
        int
            _width, _height, _minLevel, _maxLevel;

        static const TextureType _textureType = TextureType::CubeMap;


        TextureInternalFormat ResolveFormat(int numChannels)
        {
            // LOL...
            switch (numChannels)
            {
            case(1): return TextureInternalFormat::R_16f; break;
            case(3): return TextureInternalFormat::Rgb_16f; break;
            case(4): return TextureInternalFormat::Rgba_16f; break;
            default:
                return (TextureInternalFormat)0;
                throw "Unsupported texture format."; break;
            }
        }

        void LoadFromPath_HDR(const char* folderPath)
        {
            std::string path(folderPath);

            for (int i = 0; i < 6; i++)
            {
                std::string fileName;
                switch (i)
                {
                case(0): fileName = "/posx.hdr"; break;
                case(1): fileName = "/negx.hdr"; break;
                case(2): fileName = "/posy.hdr"; break;
                case(3): fileName = "/negy.hdr"; break;
                case(4): fileName = "/posz.hdr"; break;
                case(5): fileName = "/negz.hdr"; break;
                default: throw "no...."; break;
                }

                int width, height, nrChannels;
                unsigned char*
                    data = stbi_load((path + fileName).c_str(), &width, &height, &nrChannels, 0);

                if (!data)
                {
                    std::cout << "\n" << "Texture data loading failed: " << stbi_failure_reason() << "\n" << "PATH: " << (path + fileName) << "\n" << std::endl;;
                    throw "Texture data loading failed";
                }

                TextureInternalFormat internalFormat = ResolveFormat(nrChannels);

                OGLTextureUtils::Init(
                    OGLResource::ID(), _textureType, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0, internalFormat, width, height, OGLTextureUtils::ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, data);

                stbi_image_free(data);
            }

            
            OGLUtils::CheckOGLErrors();
        }

        void LoadFromPath_DDS(const char* folderPath)
        {
            
            std::string path(folderPath);
            
            for (int i = 0; i < 6; i++)
            {
                std::string fileName;
                switch (i)
                {
                case(0): fileName = "/posx.dds"; break;
                case(1): fileName = "/negx.dds"; break;
                case(2): fileName = "/posy.dds"; break;
                case(3): fileName = "/negy.dds"; break;
                case(4): fileName = "/posz.dds"; break;
                case(5): fileName = "/negz.dds"; break;
                default: throw "no...."; break;
                }
                
                gli::texture2d Texture(gli::load_dds((path + "\\" + fileName).c_str()));

                bool const compressed(gli::is_compressed(Texture.format()));
                if (compressed)
                    throw "Compressed formats are not allowed.";

                //Texture = flip(Texture);

                int width = Texture.extent().x;
                int height = Texture.extent().y;
                int ncolors = component_count(Texture.format());

                TextureInternalFormat internalFormat = ResolveFormat(ncolors);

                gli::gl GL(gli::gl::PROFILE_GL33);
                gli::gl::format const Format(GL.translate(Texture.format(), Texture.swizzles()));

                _width = width; _height = height;

                if ((_minLevel == 0 && _maxLevel == 0) &&
                    (Texture.base_level()!= 0 || Texture.max_level()!=0))
                {
                    _minLevel = Texture.base_level();
                    _maxLevel = Texture.max_level();

                    SetMinMaxLevel(OGLResource::ID(), _textureType, _minLevel, _maxLevel);
                }


                for (int level = _minLevel; level <= _maxLevel; level++)
                {
                    
                    OGLTextureUtils::Init(
                        OGLResource::ID(), _textureType, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                        level, internalFormat, Texture[level].extent().x, Texture[level].extent().y, OGLTextureUtils::ResolveFormat(internalFormat), Format.Type, Texture.data(0,0,level));
                    
                }

                OGLUtils::CheckOGLErrors();
            }
        }

    public:

        OGLTextureCubemap(const char* folderPath, TextureFiltering minFilter, TextureFiltering magFilter) 
            : _width(0), _height(0), _minLevel(0), _maxLevel(0)
        {

            OGLResource::Create(OGLResourceType::TEXTURE);

            if (ContainsExtension(folderPath, ".hdr"))
                LoadFromPath_HDR(folderPath);

            else if (ContainsExtension(folderPath, ".dds"))
                LoadFromPath_DDS(folderPath);
            else
                throw "Unsupported file format.";

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                minFilter, magFilter,
                TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge);

            OGLUtils::CheckOGLErrors();

        }

        OGLTextureCubemap(int width, int height, TextureInternalFormat internalFormat, TextureFiltering minFilter, TextureFiltering magFilter, TextureWrap wrapU, TextureWrap wrapV)
            : _width(width), _height(height), _minLevel(0), _maxLevel(0)
        {

            OGLResource::Create(OGLResourceType::TEXTURE);

            for (int i = 0; i < 6; i++)
            {
                OGLTextureUtils::Init(
                    OGLResource::ID(), _textureType, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0, internalFormat, width, height, OGLTextureUtils::ResolveFormat(internalFormat), GL_UNSIGNED_BYTE, NULL);
            }

            OGLTextureUtils::SetParameters(
                OGLResource::ID(), _textureType,
                minFilter, magFilter,
                wrapU, wrapV);

            OGLUtils::CheckOGLErrors();

        }

        OGLTextureCubemap(int width, int height, TextureInternalFormat internalFormat)
            : OGLTextureCubemap(width, height, internalFormat, TextureFiltering::Nearest, TextureFiltering::Nearest, TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge)
        {

        }

        OGLTextureCubemap(int width, int height, TextureInternalFormat internalFormat, TextureFiltering minFilter, TextureFiltering magFilter)
            : OGLTextureCubemap(width, height, internalFormat, minFilter, magFilter, TextureWrap::Clamp_To_Edge, TextureWrap::Clamp_To_Edge)
        {

        }

        OGLTextureCubemap(OGLTextureCubemap&& other) noexcept : OGLResource(std::move(other))
        {
            _width = other._width;
            _height = other._height;
            _minLevel = other._minLevel;
            _maxLevel = other._maxLevel;
        };

        int Width() const { return _width; }
        int Height() const { return _height; }
        int MinLevel() const { return _minLevel; }
        int MaxLevel() const { return _maxLevel; }

        void Bind() const
        {
            glBindTexture(_textureType, OGLResource::ID());
        }

        void UnBind() const
        {
            glBindTexture(_textureType, 0);
        }

        OGLTextureCubemap& operator=(OGLTextureCubemap&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                _height = other._height;
                _width = other._width;
                _minLevel = other._minLevel;
                _maxLevel = other._maxLevel;
            }
            return *this;
        };

        ~OGLTextureCubemap()
        {
            OGLResource::Destroy();
        }

    };


    struct OGLSamplerDesc
    {
        // Right: defualt values 
        // from : https://registry.khronos.org/OpenGL-Refpages/gl4/html/glSamplerParameter.xhtml

        TextureFiltering MinFilter = TextureFiltering::Nearest_Mip_Linear;
        TextureFiltering MagFilter = TextureFiltering::Linear;

        float MinLod = -1000.f;
        float MaxLod =  1000.f;

        TextureWrap WrapS = TextureWrap::Repeat;
        TextureWrap WrapT = TextureWrap::Repeat;
        TextureWrap WrapR = TextureWrap::Repeat;

        float BorderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        // TODO: COMAPRE MODE and FUNC
    };

    class OGLSampler : public OGLResource
    {

    private:
        OGLSamplerDesc _samplerDesc;

        void SetSamplerParams()
        {
            unsigned int id = ID();

            glSamplerParameteri (id, GL_TEXTURE_MIN_FILTER,   _samplerDesc.MinFilter   );
            glSamplerParameteri (id, GL_TEXTURE_MAG_FILTER,   _samplerDesc.MagFilter   );
            glSamplerParameterf (id, GL_TEXTURE_MIN_LOD,      _samplerDesc.MinLod      );
            glSamplerParameterf (id, GL_TEXTURE_MAX_LOD,      _samplerDesc.MaxLod      );
            glSamplerParameteri (id, GL_TEXTURE_WRAP_S,       _samplerDesc.WrapS       );
            glSamplerParameteri (id, GL_TEXTURE_WRAP_T,       _samplerDesc.WrapT       );
            glSamplerParameteri (id, GL_TEXTURE_WRAP_R,       _samplerDesc.WrapR       );
            glSamplerParameterfv(id, GL_TEXTURE_BORDER_COLOR, _samplerDesc.BorderColor );
        }

    public:
        OGLSampler(OGLSamplerDesc desc) : _samplerDesc(desc)
        {
            OGLResource::Create(OGLResourceType::SAMPLER);

            SetSamplerParams();

            OGLUtils::CheckOGLErrors();
        }

        OGLSampler(OGLSampler&& other) noexcept : OGLResource(std::move(other))
        {
            _samplerDesc = other._samplerDesc;
        };

        void Bind(unsigned int unit) const
        {
            glBindSampler(unit, OGLResource::ID());
        }

        static void UnBind(unsigned int unit)
        {
            // If the sampler name zero is bound to a texture unit, 
            // the currently bound texture's sampler state becomes active.
            glBindSampler(unit, 0);
        }

        OGLSampler& operator=(OGLSampler&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));
                _samplerDesc = other._samplerDesc;
            }
            return *this;
        };

        ~OGLSampler()
        {
            OGLResource::Destroy();
        }

    };

    template <typename T = OGLTexture2D>
    class FrameBuffer : public OGLResource
    {
    private:
        std::optional<T> _depthTexture;
        std::vector<T> _colorTextures;
        
        int
            _width, _height;

        
        T GetTexture(TextureInternalFormat format, TextureFiltering minFilter, TextureFiltering magFilter) 
        {
            if constexpr (std::is_same_v<T, OGLTexture2D>)
            {
                return OGLTexture2D(_width, _height, format, minFilter, magFilter);
            }
            else if constexpr (std::is_same_v<T, OGLTextureCubemap>)
            {
                return OGLTextureCubemap(_width, _height, format, minFilter, magFilter);
            }
            else
                throw "really don't know what I'm doing...";
        };

        T GetTexture(TextureInternalFormat format)
        {
            return GetTexture(format, TextureFiltering::Nearest, TextureFiltering::Nearest);
        }
        
        void GenTextures(bool depth, bool color, int colorTexturesCount, TextureInternalFormat colorTextureFormat, TextureFiltering minFilter, TextureFiltering magFilter)
        {
            if (depth)
                _depthTexture = GetTexture(TextureInternalFormat::Depth_Component, minFilter, magFilter);

            if (color)
            {
                for (int i = 0; i < colorTexturesCount; i++)
                    _colorTextures.push_back(GetTexture(colorTextureFormat, minFilter, magFilter));
            }
        }

        void Initialize(bool depth, bool color, int colorAttachments)
        {

            Bind();

            if (color)
            {
                std::vector<unsigned int> drawBuffers{};
                for (int i = 0; i < colorAttachments; i++)
                {
                    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, _colorTextures.at(i).ID(), 0);
                    drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
                }

                if (depth)
                    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depthTexture.value().ID(), 0);

                glDrawBuffers(drawBuffers.size(), drawBuffers.data());

            }
            else if (depth)
            {
                glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depthTexture.value().ID(), 0);

                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            }

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw "Framebuffer" + std::to_string(ID()) + " is not complete!";

            UnBind();

        }


    public:
        
        FrameBuffer(unsigned int width, unsigned int height,
            T depthTexture, std::vector<T> colorTextures)
            :_width(width), _height(height)
        {
            OGLResource::Create(OGLResourceType::FRAMEBUFFER);

            _depthTexture = std::optional<T>(std::move(depthTexture));
            _colorTextures = std::vector<T>(std::move(colorTextures));
            Initialize(true, true, _colorTextures.size());
        }

        FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth, 
            TextureInternalFormat colorTextureFormat, TextureFiltering minFilter, TextureFiltering magFilter)
            :_width(width), _height(height)
        {
            OGLResource::Create(OGLResourceType::FRAMEBUFFER);

            GenTextures(depth, color, colorAttachments, colorTextureFormat, minFilter, magFilter);
            Initialize(depth, color, colorAttachments);
        }

        FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth, TextureInternalFormat colorTextureFormat)
            :FrameBuffer(width, height, color, colorAttachments, depth, colorTextureFormat, TextureFiltering::Nearest, TextureFiltering::Nearest)
        {
        }

        FrameBuffer(unsigned int width, unsigned int height, bool color, int colorAttachments, bool depth)
            :FrameBuffer(width, height, color, colorAttachments, depth, TextureInternalFormat::Rgba_16f, TextureFiltering::Nearest, TextureFiltering::Nearest)
        {
        }



        FrameBuffer(FrameBuffer&& other) noexcept : OGLResource(std::move(other))
        {
            _width = other._width;
            _height = other._height;

            _colorTextures = std::vector<T>(std::move(other._colorTextures));
            _depthTexture = std::optional<T>(std::move(other._depthTexture));
        };


        FrameBuffer& operator=(FrameBuffer&& other) noexcept
        {
            if (this != &other)
            {
                OGLResource::operator=(std::move(other));

                _height = other._height;
                _width = other._width;

                _colorTextures = std::vector<T>(std::move(other._colorTextures));
                _depthTexture = std::optional<T>(std::move(other._depthTexture));
            }
            return *this;

        };
        ~FrameBuffer()
        {
            OGLResource::Destroy();
        }

        void SetDrawBuffers(std::vector<unsigned int> drawBuffers)
        {
            for (auto index : drawBuffers)
                if (index >= _colorTextures.size()) throw "Invalid color attchment index.";
            
            std::vector<unsigned int> drawBuffersOffset;
            
            for(auto index : drawBuffers)
            drawBuffersOffset.push_back(GL_COLOR_ATTACHMENT0 + index);

            glDrawBuffers(drawBuffersOffset.size(), drawBuffersOffset.data());

            OGLUtils::CheckOGLErrors();
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

        static void UnBind() 
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void BindColorAttachment(int attachment = 0) const
        {
            _colorTextures.at(attachment).Bind();
        }

        void BindDepthAttachment() const
        {
            if (_depthTexture.has_value())
                _depthTexture.value().Bind();

            else
                throw "Cannot bind depth attachment.";
        }

        void UnBindColorAttachment(int attachment = 0) const
        {
            _colorTextures.at(attachment).UnBind();
        }

        void UnBindDepthAttachment() const
        {
            if (_depthTexture.has_value())
                _depthTexture.value().UnBind();

            else
                throw "Cannot unbind depth attachment.";
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

        void CopyToOtherFbo(std::stack<FrameBuffer<T>*> stack, const FrameBuffer* other, bool color, int attachment, bool depth, glm::ivec2 rect0, glm::ivec2 rect1) const
        {
            CopyToOtherFbo(other, color, attachment, depth, rect0, rect1);
            if (!stack.empty())
                stack.top()->Bind();
        }

    };

    

    
}
#endif