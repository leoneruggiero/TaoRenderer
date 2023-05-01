#pragma once
#include "RenderContext.h"
#include <glm/glm.hpp>
#include <map>
#include <optional>
namespace tao_gizmos
{
	using namespace tao_render_context;
	using namespace tao_ogl_resources;
	using namespace glm;
	using namespace std;

	class LineGizmo
	{

	};
	class PolylineGizmo
	{

	};
	class MeshGizmo
	{
		
	};
	struct point_gizmo_instance
	{
		vec3			position;
		vec4			color;
		unsigned int	symbol_index;
	};
	struct symbol_mapping
	{
		vec2 uv_min;
		vec2 uv_max;
	};
	struct symbol_atlas_descriptor
	{
		const void*				symbol_atlas_data;
		ogl_texture_format		symbol_atlas_data_format;
		ogl_texture_data_type	symbol_atlas_data_type;
		int						symbol_atlas_width;
		int						symbol_atlas_height;
		vector<symbol_mapping>  symbol_atlas_mapping;
		bool					symbol_filter_smooth;
	};
	struct point_gizmo_descriptor
	{
		float					 point_size				 = 1.0f;
		bool					 pixel_snapping			 = false;
		symbol_atlas_descriptor* symbol_atlas_descriptor = nullptr;
	};
	class PointGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _instanceCount	= 0;
		unsigned int _vboCapacity	= 0;

		// graphics data
		// -----------------------------
		vector<vec3> _positionList;
		vector<vec4> _colorList;

		OglVertexBuffer			_vbo;
		OglVertexAttribArray	_vao;
		optional<OglTexture2D>  _symbolAtlas;
		optional<vector<vec4>>  _symbolAtlasTexCoordLUT;
		// settings
		// ------------------------------
		float _pointSize;
		bool  _snap;

		// static utils
		// ------------------------------
		static ogl_texture_internal_format GetSymbolAtlasFormat(ogl_texture_format format);
		static vector<vec4> GetSymbolAtlasTexCoordLUT(symbol_atlas_descriptor desc);

		// this will be called by GizomsRenderer instances
		PointGizmo(RenderContext& rc, const point_gizmo_descriptor& desc);
		void SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors, const optional<vector<unsigned int>>&);
	};

	class GizmosRenderer
	{
	private:
		static constexpr const char* SHADER_SRC_DIR = "C:/Users/Admin/Documents/LearnOpenGL/TestApp_OpenGL/TaOgl_Gizmos/Shaders"; //todo: this is obviously a joke
		static constexpr const char* EXC_PREAMBLE = "GizmosRenderer: ";
		int _windowWidth, _windowHeight;
		RenderContext* _renderContext;

		OglShaderProgram	_pointsShader;
		OglShaderProgram	_linesShader;
		OglShaderProgram	_lineStripShader;

		OglUniformBuffer    _pointsObjDataUbo;
		OglUniformBuffer    _linesObjDataUbo;
		OglUniformBuffer    _frameDataUbo;

		OglTexture2D		_colorTex;
		OglTexture2D		_deptheTex;
		OglFramebufferTex2D _mainFramebuffer;

		OglSampler			_nearestSampler;
		OglSampler			_linearSampler;

		map<unsigned int, PointGizmo> _pointGizmos;
	public:
		GizmosRenderer(RenderContext& rc, int windowWidth, int windowHeight);

		// todo: projection matrix is not a parameter (near and far should be modified)
		void Render(glm::mat4 viewMatrix, glm::mat4 projectionMatrix);

		void CreatePointGizmo(unsigned int pointGizmoIndex, const point_gizmo_descriptor& desc);
		void DestroyPointGizmo(unsigned int pointGizmoIndex);
		void InstancePointGizmo(unsigned int pointGizmoIndex, const vector<point_gizmo_instance>& instances);
	};
}