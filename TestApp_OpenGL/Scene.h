#ifndef SCENE_H
#define SCENE_H

#include "GeometryHelper.h"
#include <vector>
#include <optional>
#include "Shader.h"

struct PointLight
{
	static const int MAX_POINT_LIGHTS = 3;
	
	float Bias;

	// Alpha is intensity
	glm::vec4 Color;
	glm::vec3 Position;
	float Radius;
	float Size;

	// ShadowData
	std::vector<glm::mat4> LightSpaceMatrix;
	unsigned int ShadowMapId;
	
	void ComputeRadius()
	{
		// Using attenuation formula from: https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
		float threshold = 5E-3;
		Radius = glm::sqrt(glm::max(Color.x, Color.y, Color.z) * Color.w / threshold);
	}

	PointLight() : Color(0.0, 0.0, 0.0, 0.0), Position(0.0, 0.0, 0.0), Radius(0.0), ShadowMapId(0), Size(0.0)
	{
		for(auto &m: LightSpaceMatrix)
			m = glm::mat4(1.0f);
	}

	PointLight(glm::vec4 color, glm::vec3 position) : PointLight()
	{
		Color = color;
		Position = position;

		Bias = 0.05f;
		Size = 0.05f;
		
		ComputeRadius();
	}
};

struct DirectionalLight
{
	static const int SPLITS = 4;

	glm::vec3 Direction;

	// Needed for shadowMap
	glm::vec3 Position;

	// Alpha is intensity
	glm::vec4 Diffuse;
	glm::vec4 Specular;

	// ShadowData
	unsigned int ShadowMapId;
	glm::mat4 LightSpaceMatrix[SPLITS];
	glm::vec4 ShadowBoxSize;
	float Bias;
	float SlopeBias;
	float Softness;
	bool doSplits;
	float lambda = 0.4f;
	glm::vec4 splits;
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
	AmbientLight Ambient;
	DirectionalLight Directional;
	PointLight Points[PointLight::MAX_POINT_LIGHTS]; // See MAX_POINT_LIGHTS defined in glsl code.
};

struct ScenePostProcessing
{
	bool ToneMapping;
	float Exposure;

	bool doGammaCorrection;
	float Gamma;

	bool doTaa;
	bool writeVelocity;
	glm::vec2 jitter;
};

struct DrawParams
{
	bool doShadows;
};

struct GridParams
{
	glm::vec2 Min;
	glm::vec2 Max;
	float Step;
	glm::vec4 Color;
};

struct Environment
{
	// Skybox
	bool useSkyboxTexture;
	glm::vec3 SouthColor;
	glm::vec3 NorthColor;
	glm::vec3 EquatorColor;
	std::optional<OGLResources::OGLTextureCubemap> Skybox;

	// Diffuse IBL
	float intensity;
	std::optional<OGLResources::OGLTextureCubemap> IrradianceMap;

	// Specular IBL
	std::optional<OGLResources::OGLTexture2D> LUT;
	std::optional<OGLResources::OGLTextureCubemap> RadianceMap;
};

struct Texture
{
	unsigned char* data;
	int width;
	int height;
};

struct SceneParams
{
	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;
	glm::vec3 cameraPos;
	float cameraNear, cameraFar, cameraFovY;
	int viewportWidth, viewportHeight;
	glm::ivec2 mousePosition;
	SceneLights sceneLights;
	ScenePostProcessing postProcessing;
	DrawParams drawParams;
	Environment environment;
	GridParams grid;
	float pointWidth = 6.0f;
	float lineWidth = 1.8f;

	std::vector<OGLResources::OGLTexture2D> noiseTextures;
	std::optional<OGLResources::OGLTexture1D> poissonSamples;
};

constexpr const char* MaterialsFolder = "../../Assets/Materials";

struct Material
{
	glm::vec4 Albedo;
	float Roughness;
	float Metallic;

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

const Material MaterialsCollection::NullMaterial = Material{ glm::vec4(1, 0, 1, 1), 0.0, 0.0 };
const Material MaterialsCollection::ShinyRed = Material{ glm::vec4(1, 0, 0, 1), 0.2, 0.0 };
const Material MaterialsCollection::PlasticGreen = Material{ glm::vec4(0, 0.8, 0, 1), 0.3, 0.0 };
const Material MaterialsCollection::Copper = Material{ glm::vec4(0.8, 0.3, 0, 1),  0.2, 1.0 };
const Material MaterialsCollection::PureWhite = Material{ glm::vec4(0.9, 0.9, 0.9, 1),0.1, 0.0 };
const Material MaterialsCollection::MatteGray = Material{ glm::vec4(0.5, 0.5, 0.5, 1), 0.8 , 0.0 };
const Material MaterialsCollection::ClayShingles = Material{ glm::vec4(0.5, 0.5, 0.5, 1), 0.4, 0.0, "Shingles" };
const Material MaterialsCollection::WornFactoryFloor = Material{ glm::vec4(0.5, 0.5, 0.5, 1), 0.4, 0.0, "WornFactoryFloor" };
const Material MaterialsCollection::WoodPlanks = Material{ glm::vec4(0.5, 0.5, 0.5, 1), 0.4, 0.0,  "WoodPlanks" };
const Material MaterialsCollection::Cobblestone = Material{ glm::vec4(0.5, 0.5, 0.5, 1), 0.7, 0.0, "Cobblestone" };
const Material MaterialsCollection::BlackAndWhiteTiles = Material{ glm::vec4(0.5, 0.5, 0.5, 1), 0.5, 0.0,  "BlackAndWhiteTiles" };


#endif