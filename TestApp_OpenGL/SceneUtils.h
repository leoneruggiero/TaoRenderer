#ifndef SCENEUTILS_H
#define SCENEUTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


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
	Matrices = 0,
	Lights = 1,
	Shadows = 2,
	AmbientOcclusion = 3
};

#ifdef GFX_SHADER_DEBUG_VIZ
enum SSBOBinding
{
	Debug = 0
};
#endif

enum TextureBinding
{
	ShadowMapDirectional = 0,
	AoMap = 1,
	NoiseMap0 = 2,
	NoiseMap1 = 3,
	NoiseMap2 = 4,
	PoissonSamples = 5,
	Albedo = 6,
	Normals = 7,
	Roughness = 8,
	Metallic = 9,
	IrradianceMap = 10,
	RadianceMap = 11,
	BrdfLut = 12,
	ShadowMapPoint0 = 13,
	ShadowMapPoint1 = 14,
	ShadowMapPoint2 = 15
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