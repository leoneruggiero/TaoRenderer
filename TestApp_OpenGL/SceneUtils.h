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
	glm::mat4 viewMatrix;
	glm::mat4 projMatrix;
	float	  near;
	float	  far;
};

struct dirLightData
{
	glm::vec4 direction;
	glm::vec4 diffuse;
	float	  softness;
	float	  bias;
	float	  doSplits;
	float	  pad1;
	glm::vec4 splits;
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
	// One transf per split (1 or 4 splits)
	glm::mat4		dirLightSpaceMat0;
	glm::mat4		dirLightSpaceMat1;
	glm::mat4		dirLightSpaceMat2;
	glm::mat4		dirLightSpaceMat3;

	glm::mat4		dirLightSpaceMatInv0;
	glm::mat4		dirLightSpaceMatInv1;
	glm::mat4		dirLightSpaceMatInv2;
	glm::mat4		dirLightSpaceMatInv3;

	dirLightData	dirLight;

	pointLightData	pointLight0;
	pointLightData	pointLight1;
	pointLightData	pointLight2;
	
	glm::vec4		eyeWorldPos;

	glm::vec4		dirShadowFrusumSize;

	unsigned int	doGamma;
	float			gamma;
	
	unsigned int	hasIrradianceMap;
	float			environmentIntensity;
	unsigned int	hasRadianceMap;
	unsigned int	hasBrdfLut;
	int				radianceMapMinLevel;
	int				radianceMapMaxLevel;
	glm::vec2		taa_jitter;
	glm::vec2		viewportSize;
	unsigned int	doTaa;
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
	PerFrameData		= 0,
	ViewData			= 1,
	PerObjectData		= 2,
	Shadows				= 3,
	AmbientOcclusion	= 4
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