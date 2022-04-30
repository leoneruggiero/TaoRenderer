#ifndef SCENEUTILS_H
#define SCENEUTILS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

enum class AOType
{
	SSAO,
	HBAO
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

enum TextureBinding
{
	ShadowMap = 0,
	AoMap = 1,
	NoiseMap0 = 2,
	NoiseMap1 = 3,
	NoiseMap2 = 4,
	Albedo = 5,
	Normals = 6
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
	R_16f = GL_R16F,
	R_16ui = GL_R16UI,     // Where do those fancy formats come from???
	Rgb_16f = GL_RGB16F,   // Where do those fancy formats come from???
	Rgba = GL_RGBA,
	Rgb = GL_RGB,
	Rgba_32f = GL_RGBA32F,
	Rgba_16f = GL_RGBA16F

};

struct PointLight
{
	glm::vec3 Position;

	// Alpha is intensity
	glm::vec4 Diffuse;
	glm::vec4 Specular;

	// Faloff
	float Constant = 1.0;
	float Linear;
	float Quadratic;
};

struct DirectionalLight
{
	glm::vec3 Direction;

	// Needed for shadowMap
	glm::vec3 Position;

	// Alpha is intensity
	glm::vec4 Diffuse;
	glm::vec4 Specular;

	// ShadowData
	unsigned int ShadowMapId;
	glm::mat4 LightSpaceMatrix;
	float ShadowMapToWorld;
	float Bias=0.002f;
	float SlopeBias=0.025f;
	float Softness = 0.0015;
};

struct AmbientLight
{
	glm::vec4 Ambient;

	float aoStrength;
	float aoRadius;
	int aoSamples;
	int aoSteps;
	int aoBlurAmount;
	// SSAO
	unsigned int AoMapId;
};

struct Spotlight
{
	glm::vec3 Position;
	glm::vec3 Direction;

	// Alpha is intensity
	glm::vec4 Diffuse;
	glm::vec4 Specular;

	// Cone
	float InnerRadius;
	float OuterRadius;
};

struct SceneLights
{
	// ambientLight
	AmbientLight Ambient;

	// directionalLight
	DirectionalLight Directional;
};

struct ScenePostProcessing
{
	bool ToneMapping;
	float Exposure;

	bool GammaCorrection;
};

struct DrawParams
{
	bool doShadows;
};

struct SceneParams
{
	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;
	float cameraNear, cameraFar;
	int viewportWidth, viewportHeight;
	SceneLights sceneLights;
	ScenePostProcessing postProcessing;
	DrawParams drawParams;


	//Utils....where to store them???
	unsigned int noiseTexId_0, noiseTexId_1, noiseTexId_2;
};
struct Texture
{
	unsigned char* data;
	int width;
	int height;
};

constexpr const char* MaterialsFolder = "./Assets/Materials";

struct Material
{
	glm::vec4 Diffuse;
	glm::vec4 Specular;

	float Shininess;

	const char* Textures;
};

struct MaterialsCollection
{
	static const Material NullMaterial;
	static const Material ShinyRed;
	static const Material PlasticGreen;
	static const Material Copper;
	static const Material PureWhite;
	static const Material MatteGray;
	static const Material ClayShingles;
	static const Material WornFactoryFloor;
	static const Material Cobblestone;
	static const Material WoodPlanks;
	static const Material BlackAndWhiteTiles;
};

const Material MaterialsCollection::NullMaterial = Material{ glm::vec4(1, 0, 1, 1), glm::vec4(1, 0, 1, 1), 64 };
const Material MaterialsCollection::ShinyRed = Material{ glm::vec4(1, 0, 0, 1), glm::vec4(1, 1, 1, 1), 256 };
const Material MaterialsCollection::PlasticGreen = Material{ glm::vec4(0, 0.8, 0, 1), glm::vec4(0.8, 0.8, 0.8, 1), 32 };
const Material MaterialsCollection::Copper = Material{ glm::vec4(0.8, 0.3, 0, 1), glm::vec4(0.8, 0.3, 0, 1), 128 };
const Material MaterialsCollection::PureWhite = Material{ glm::vec4(0.9, 0.9, 0.9, 1), glm::vec4(0.8, 0.3, 0, 1), 128 };
const Material MaterialsCollection::MatteGray = Material{ glm::vec4(0.5, 0.5, 0.5, 1), glm::vec4(0.5, 0.5, 0.5, 1), 64 };
const Material MaterialsCollection::ClayShingles = Material{ glm::vec4(0.5, 0.5, 0.5, 1), glm::vec4(0.5, 0.5, 0.5, 1), 16, "Shingles"};
const Material MaterialsCollection::WornFactoryFloor = Material{ glm::vec4(0.5, 0.5, 0.5, 1), glm::vec4(0.5, 0.5, 0.5, 1), 16, "WornFactoryFloor" };
const Material MaterialsCollection::WoodPlanks = Material{ glm::vec4(0.5, 0.5, 0.5, 1), glm::vec4(0.5, 0.5, 0.5, 1), 256, "WoodPlanks" };
const Material MaterialsCollection::Cobblestone = Material{ glm::vec4(0.5, 0.5, 0.5, 1), glm::vec4(0.5, 0.5, 0.5, 1), 128, "Cobblestone" };
const Material MaterialsCollection::BlackAndWhiteTiles = Material{ glm::vec4(0.5, 0.5, 0.5, 1), glm::vec4(0.5, 0.5, 0.5, 1), 128, "BlackAndWhiteTiles" };

#endif