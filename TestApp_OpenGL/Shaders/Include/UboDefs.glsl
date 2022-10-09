//? #version 430 core

struct Material 
{
    vec4 Albedo;
    vec4 Emission;
    float Roughness;
    float Metalness;
       
    bool hasTex_Albedo           ;
    bool hasTex_Emission         ;
    bool hasTex_Normals          ;
    bool hasTex_Roughness        ;
    bool hasTex_Metalness        ;
    bool hasTex_Occlusion        ;
};
   
struct DirectionalLight
{
	vec4 Direction; // 16 byte
	vec4 Diffuse;   // 16 byte

    // (softness, bias, UNUSED, UNUSED)
	vec4 Data;      // 16 byte
                    // => 48 byte
};

struct PointLight
{
	vec4  Color;            // 16 byte
	vec4  Position;         // 16 byte
    float Radius;	        // 4  byte
    float Size;             // 4  byte
    float InvSqrRadius;     // 4  byte
    float Bias;             // 4  byte
                            // => 48 byte
};

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 3
#endif

   
layout (std140) uniform blk_PerObjectData
{
    uniform mat4        o_modelMat;     // 64 byte
    uniform mat4        o_normalMat;    // 64 byte
    uniform Material    o_material;     // 64 byte  
                                        // TOTAL => 192 byte
};
    
layout (std140) uniform blk_PerFrameData
{
    uniform mat4             f_viewMat;                                    // 64  byte
    uniform mat4             f_projMat;                                    // 64  byte
    uniform float            f_near;                                       // 4   byte
    uniform float            f_far;                                        // 4   byte
    uniform bool             f_doGamma;                                    // 4   byte
    uniform float            f_gamma;                                      // 4   byte
    uniform PointLight       f_pointLight[MAX_POINT_LIGHTS];               // 144 byte
    uniform DirectionalLight f_dirLight;                                   // 48  byte
    uniform vec4             f_eyeWorldPos;                                // 16  byte
    uniform vec4             f_shadowCubeSize_directional;                 // 16  byte
    uniform bool             f_hasIrradianceMap;                           // 4   byte
    uniform float            f_environmentIntensity;                       // 4   byte
    uniform bool             f_hasRadianceMap;                             // 4   byte
    uniform bool             f_hasBrdfLut;                                 // 4   byte
    uniform mat4             f_lightSpaceMatrix_directional;               // 64  byte
    uniform mat4             f_lightSpaceMatrixInv_directional;            // 64  byte
    uniform int              f_radiance_minLevel;                          // 4   byte
    uniform int              f_radiance_maxLevel;                          // 4   byte

                                                                           // TOTAL => 504 byte
};
