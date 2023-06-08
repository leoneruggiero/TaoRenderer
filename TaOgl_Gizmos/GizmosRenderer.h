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
		const void*				data;
		ogl_texture_format		data_format;
		ogl_texture_data_type	data_type;
		unsigned int			width;
		unsigned int			height;
		vector<symbol_mapping>  mapping;
		bool					filter_smooth;
	};

	struct point_gizmo_descriptor
	{
		unsigned int			 point_half_size		 = 1;
		bool					 snap_to_pixel			 = false;
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
		unsigned int _pointSize;
		bool		 _snap;
		bool		 _symbolAtlasLinearFilter;

		// this will be called by GizomsRenderer instances
		PointGizmo(RenderContext& rc, const point_gizmo_descriptor& desc);
		void SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors, const optional<vector<unsigned int>>& symbolIndices);
	};

	struct line_gizmo_instance
	{
		vec3			start;
		vec3			end;
		vec4			color;
	};

	struct pattern_texture_descriptor
	{
		const void*				data;
		ogl_texture_format		data_format;
		ogl_texture_data_type	data_type;
		unsigned int			width;
		unsigned int			height;
		unsigned int			pattern_length;
	};

	struct line_gizmo_descriptor
	{
		unsigned int				line_size					= 1;
		pattern_texture_descriptor* pattern_texture_descriptor  = nullptr;
	};

	class LineGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _instanceCount = 0;
		unsigned int _vboCapacity   = 0;

		// graphics data
		// -----------------------------
		vector<vec3> _positionList;
		vector<vec4> _colorList;

		OglVertexBuffer			_vbo;
		OglVertexAttribArray	_vao;
		optional<OglTexture2D>  _patternTexture;

		// settings
		// ------------------------------
		unsigned int _lineSize;
		unsigned int _patternSize;

		// this will be called by GizomsRenderer instances
		LineGizmo(RenderContext& rc, const line_gizmo_descriptor& desc);
		void SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors);
	};

	struct line_strip_gizmo_instance
	{
		mat4			transformation;
		vec4			color;
	};

	struct line_strip_gizmo_descriptor
	{
		vector<vec3>*				vertices;
		bool						isLoop;
		unsigned int				line_size = 1;
		pattern_texture_descriptor* pattern_texture_descriptor = nullptr;
	};

	class LineStripGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _instanceCount				= 0;
		unsigned int _vboVertCapacity			= 0;
		unsigned int _vboInstanceDataCapacity	= 0;

		// graphics data
		// -----------------------------
		vector<vec4> _vertices;        
		vector<mat4> _transformList;    // one transformation per instance.
		vector<vec4> _colorList;        // one color per instance. todo: color per vertex?

		OglVertexBuffer			_vboVertices;     
		OglVertexBuffer			_vboInstanceData; // draw instanced
		OglVertexAttribArray	_vao;
		optional<OglTexture2D>  _patternTexture;

		// settings
		// ------------------------------
		unsigned int _lineSize;
		unsigned int _patternSize;

		// this will be called by GizomsRenderer instances
		LineStripGizmo(RenderContext& rc, const line_strip_gizmo_descriptor& desc);
		void SetInstanceData(const vector<mat4>& transforms, const vector<vec4>& colors);
	};


	class MeshGizmoVertex
	{
	public:
		glm::vec3 Position;
		glm::vec3 Normal	 = glm::vec3(0.0f);
		glm::vec4 Color		 = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
		glm::vec2 TexCoord	 = glm::vec2(0.0f);

		MeshGizmoVertex(glm::vec3 position): Position{position}
		{
		}

		MeshGizmoVertex& AddNormal(glm::vec3 normal)
		{
			Normal = normal;
			return *this;
		}

		MeshGizmoVertex& AddColor(glm::vec4 color)
		{
			Color = color;
			return *this;
		}

		MeshGizmoVertex& AddTexCoord(glm::vec2 texCoord)
		{
			TexCoord = texCoord;
			return *this;
		}
	};

	struct mesh_gizmo_instance
	{
		mat4			transformation;
		vec4			color;
	};

	struct mesh_gizmo_descriptor
	{
		vector<MeshGizmoVertex>*    vertices;
		vector<unsigned int>*		triangles;
	};

	class MeshGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _instanceCount				= 0;
		unsigned int _vertexCount				= 0;
		unsigned int _trisCount					= 0;
		unsigned int _vboVertCapacity			= 0;
		unsigned int _eboCapacity				= 0;
		unsigned int _ssboInstanceDataCapacity	= 0;

		// graphics data
		// --------------------------------------------------------
		vector<vec3>		  _vertices;
		vector<unsigned int>  _triangles;
		// instance data ------------------------------------------
		vector<mat4> _transformList;    // instance transformation
		vector<vec4> _colorList;        // instance color

		OglVertexBuffer			_vboVertices;
		OglVertexAttribArray	_vao;
		OglIndexBuffer			_ebo;
		OglShaderStorageBuffer  _ssboInstanceData;

		// this will be called by GizomsRenderer instances
		MeshGizmo(RenderContext& rc, const mesh_gizmo_descriptor& desc);
		void SetInstanceData(const vector<mat4>& transforms, const vector<vec4>& colors);
	};


	class GizmosRenderer
	{
	private:
		static constexpr const char* SHADER_SRC_DIR = "C:/Users/Admin/Documents/LearnOpenGL/TestApp_OpenGL/TaOgl_Gizmos/Shaders"; //todo: this is obviously a joke
		static constexpr const char* EXC_PREAMBLE = "GizmosRenderer: ";
		int _windowWidth, _windowHeight;
		RenderContext* _renderContext;

		OglShaderProgram		_pointsShader;
		OglShaderProgram		_linesShader;
		OglShaderProgram		_lineStripShader;
		OglShaderProgram		_meshShader;

		OglUniformBuffer		_pointsObjDataUbo;
		OglUniformBuffer		_linesObjDataUbo;
		OglUniformBuffer		_lineStripObjDataUbo;
		OglUniformBuffer		_meshObjDataUbo;
		OglUniformBuffer		_frameDataUbo;

		OglShaderStorageBuffer  _lenghSumSsbo; 
		unsigned int			_lengthSumSsboCapacity = 0; //todo....should't the capacity be a field of SSBO

		OglSampler				_nearestSampler;
		OglSampler				_linearSampler;

		OglTexture2DMultisample					_colorTex;
		OglTexture2DMultisample					_depthTex;
		OglFramebuffer<OglTexture2DMultisample> _mainFramebuffer;

		map<unsigned int, PointGizmo>		_pointGizmos;
		map<unsigned int, LineGizmo>		_lineGizmos;
		map<unsigned int, LineStripGizmo>	_lineStripGizmos;
		map<unsigned int, MeshGizmo>		_meshGizmos;

		void RenderPointGizmos();
		void RenderLineGizmos();
		void RenderLineStripGizmos(const mat4& viewMatrix, const mat4& projMatrix);
		void RenderMeshGizmos();

	public:
		GizmosRenderer(RenderContext& rc, int windowWidth, int windowHeight);

		// todo: projection matrix is not a parameter (near and far should be modified)
		[[nodiscard]] const OglFramebuffer<OglTexture2DMultisample>& Render(glm::mat4 viewMatrix, glm::mat4 projectionMatrix, glm::vec2 nearFar);

		///////////////////////////////////////
 		// Point Gizmos
		void CreatePointGizmo	(unsigned int pointGizmoIndex, const point_gizmo_descriptor& desc);
		void InstancePointGizmo	(unsigned int pointGizmoIndex, const vector<point_gizmo_instance>& instances);
		void DestroyPointGizmo	(unsigned int pointGizmoIndex);

		///////////////////////////////////////
		// Line Gizmos
		void CreateLineGizmo	(unsigned int lineGizmoIndex, const line_gizmo_descriptor& desc);
		void InstanceLineGizmo	(unsigned int lineGizmoIndex, const vector<line_gizmo_instance>& instances);
		void DestroyLineGizmo	(unsigned int lineGizmoIndex);

		///////////////////////////////////////
		// LineStrip Gizmos
		void CreateLineStripGizmo	(unsigned int lineStripGizmoIndex, const line_strip_gizmo_descriptor& desc);
		void InstanceLineStripGizmo	(unsigned int lineStripGizmoIndex, const vector<line_strip_gizmo_instance>& instances);
		void DestroyLineStripGizmo	(unsigned int lineStripGizmoIndex);

		///////////////////////////////////////
		// Mesh Gizmos
		void CreateMeshGizmo	(unsigned int meshGizmoIndex, const mesh_gizmo_descriptor& desc);
		void InstanceMeshGizmo	(unsigned int meshGizmoIndex, const vector<mesh_gizmo_instance>& instances);
		void DestroyMeshGizmo	(unsigned int meshGizmoIndex);
	};
}