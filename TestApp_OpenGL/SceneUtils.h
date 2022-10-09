#ifndef SCENEUTILS_H
#define SCENEUTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct matData
{
	glm::vec4		Albedo;
	glm::vec4		Emission;
	float			Roughness;
	float			Metalness;
	unsigned int	hasTex_Albedo;
	unsigned int	hasTex_Emission;
	unsigned int	hasTex_Normals;
	unsigned int	hasTex_Roughness;
	unsigned int	hasTex_Metalness;
	unsigned int	hasTex_Occlusion;
};

struct viewData
{
	glm::mat4	viewMat;
	glm::mat4	projMat;
	float		near;
	float		far;
};

struct dirLightData
{
	glm::vec4 direction;
	glm::vec4 diffuse;
	float	  softness;
	float	  bias;
	float	  pad0;
	float	  pad1;
};

struct pointLightData
{
	glm::vec4 color;        
	glm::vec4 position;     
	float	  radius;	        
	float	  size;             
	float	  invSqrRadius;     
	float	  bias;
};

struct dataPerObject
{
	glm::mat4	modelMat;
	glm::mat4	normalMat;
	matData		material;
};

struct dataPerFrame
{
	viewData		vData;
	unsigned int	doGamma;
	float			gamma;
	pointLightData	pointLight0;
	pointLightData	pointLight1;
	pointLightData	pointLight2;
	dirLightData	dirLight;	
	glm::vec4		eyeWorldPos;
	glm::vec4		dirShadowFrusumSize;
	unsigned int	hasIrradianceMap;
	float			environmentIntensity;
	unsigned int	hasRadianceMap;
	unsigned int	hasBrdfLut;
	glm::mat4		dirLightSpaceMat;
	glm::mat4		dirLightSpaceMatInv;
	int				radianceMapMinLevel;
	int				radianceMapMaxLevel;
};

enum class AOType
{
	SSAO,
	HBAO,
	NONE
};

enum UBOBinding
{
	// NOTE: should always start from 0 and continue consecutively 
	// (see the ugly code that calls this enum)
	PerFrameData = 0,
	PerObjectData = 1,
	Shadows = 2,
	AmbientOcclusion = 3
};

enum SSBOBinding
{
	Debug = 0
};


enum TextureBinding
{
	Albedo					= 0,
	Normals					= 1,
	Roughness				= 2,
	Metallic				= 3,
	AoMap					= 4,
	ShadowMapDirectional	= 5,
	NoiseMap0				= 6,
	NoiseMap1				= 7,
	NoiseMap2				= 8,
	PoissonSamples			= 9,
	IrradianceMap			= 10,
	RadianceMap				= 11,
	BrdfLut					= 12,
	ShadowMapPoint0			= 13,
	ShadowMapPoint1			= 14,
	ShadowMapPoint2			= 15
};

enum VertexInputType
{
	Position = 0,
	Normal = 1,
	TextureCoordinates = 2,
	Tangent = 3,
	Bitangent = 4,
	Last = 5 // wow
};

#endif