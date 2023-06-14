#pragma once
#include "RenderContext.h"
#include <glm/glm.hpp>
#include <map>
#include <optional>
#include <glm/gtc/type_ptr.hpp>

namespace tao_gizmos
{
	template
		<typename Buff,
		Buff CreateFunc(tao_render_context::RenderContext&, tao_ogl_resources::ogl_buffer_usage),
		void ResizeFunc(Buff&, unsigned int, tao_ogl_resources::ogl_buffer_usage)>
		requires tao_ogl_resources::ogl_buffer<typename Buff::ogl_resource_type>
	class ResizableOglBuffer
	{
		
	private:
		unsigned int                        _capacity = 0;
		tao_ogl_resources::ogl_buffer_usage _usage;
		Buff								_oglBuffer;
		std::function<unsigned int(unsigned int, unsigned int)> _resizePolicy;
	public:
		// Const getters
		unsigned int Capacity()						const { return _capacity; }
		tao_ogl_resources::ogl_buffer_usage Usage() const { return _usage; }

		// Vbo getter (non const)
		Buff& OglBuffer() { return _oglBuffer; }

		ResizableOglBuffer(
			tao_render_context::RenderContext& rc,
			unsigned int capacity, 
			tao_ogl_resources::ogl_buffer_usage usage,
			const std::function<unsigned int(unsigned int, unsigned int)>& resizePolicy):

			_capacity{ capacity },
			_usage{ usage },
			_oglBuffer{ CreateFunc(rc, _usage) },
			_resizePolicy{resizePolicy}
		{
		}

		void Resize(unsigned int capacity)
		{
			const auto newCapacity = _resizePolicy(_capacity, capacity);

			if(_capacity!=newCapacity)
			{
				_capacity = newCapacity;
				ResizeFunc(_oglBuffer, _capacity, _usage);
			}
		}
	};

	// Resizable VBO
	/////////////////////////////////
	inline tao_ogl_resources::OglVertexBuffer CreateEmptyVbo(tao_render_context::RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
	{
		return rc.CreateVertexBuffer(nullptr, 0, usg);
	}
	inline void ResizeVbo(tao_ogl_resources::OglVertexBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
	{
		buff.SetData(newCapacity, nullptr, usg);
	}

	typedef ResizableOglBuffer<tao_ogl_resources::OglVertexBuffer, CreateEmptyVbo, ResizeVbo> ResizableVbo;

	// Resizable EBO
	/////////////////////////////////
	inline tao_ogl_resources::OglIndexBuffer  CreateEmptyEbo(tao_render_context::RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
	{
		auto ebo = rc.CreateIndexBuffer();
		ebo.SetData(0, nullptr, usg);
		return ebo;
	}
	inline void ResizeEbo(tao_ogl_resources::OglIndexBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
	{
		buff.SetData(newCapacity, nullptr, usg);
	}

	typedef ResizableOglBuffer<tao_ogl_resources::OglIndexBuffer, CreateEmptyEbo, ResizeEbo> ResizableEbo;

	// Resizable SSBO
	/////////////////////////////////
	inline tao_ogl_resources::OglShaderStorageBuffer  CreateEmptySsbo(tao_render_context::RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg)
	{
		auto ssbo = rc.CreateShaderStorageBuffer();
		ssbo.SetData(0, nullptr, usg);
		return ssbo;
	}
	inline void	ResizeSsbo(tao_ogl_resources::OglShaderStorageBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg)
	{
		buff.SetData(newCapacity, nullptr, usg);
	}

	typedef ResizableOglBuffer<tao_ogl_resources::OglShaderStorageBuffer, CreateEmptySsbo, ResizeSsbo> ResizableSsbo;

	struct point_gizmo_instance
	{
		glm::vec3    position;
		glm::vec4    color;
		unsigned int symbol_index;
	};

	struct symbol_mapping
	{
		glm::vec2 uv_min;
		glm::vec2 uv_max;
	};

	struct symbol_atlas_descriptor
	{
		const void*                              data;
		tao_ogl_resources::ogl_texture_format    data_format;
		tao_ogl_resources::ogl_texture_data_type data_type;
		unsigned int                             width;
		unsigned int                             height;
		std::vector<symbol_mapping>              mapping;
		bool                                     filter_smooth;
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
		
		// graphics data
		// -----------------------------
		std::vector<glm::vec3>						   _positionList;
		std::vector<glm::vec4>						   _colorList;

		ResizableVbo								   _vbo;
		tao_ogl_resources::OglVertexAttribArray        _vao;
		std::optional<tao_ogl_resources::OglTexture2D> _symbolAtlas;
		std::optional<std::vector<glm::vec4>>          _symbolAtlasTexCoordLUT;

		// settings
		// ------------------------------
		unsigned int _pointSize;
		bool		 _snap;
		bool		 _symbolAtlasLinearFilter;

		// this will be called by GizomsRenderer instances
		PointGizmo(tao_render_context::RenderContext& rc, const point_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<glm::vec3>& positions, const std::vector<glm::vec4>& colors, const std::optional<std::vector<unsigned int>>& symbolIndices);
	};

	struct LineGizmoVertex
	{
	private:
		/* Position */ float pX, pY, pZ;
		/* Color    */ float cX, cY, cZ, cW;
	public:
		LineGizmoVertex()
		{
			pX = pY = pZ = 0.0f;
			cX = cY = cZ = cW = 0.0f;
		}

		LineGizmoVertex& Position(glm::vec3 p)
		{
			pX = p.x;
			pY = p.y;
			pZ = p.z;
			return *this;
		}
		glm::vec3 GetPosition() const
		{
			return glm::vec3{ pX, pY, pZ };
		}

		LineGizmoVertex& Color(glm::vec4 c)
		{
			cX = c.x;
			cY = c.y;
			cZ = c.z;
			cW = c.w;
			return *this;
		}
		glm::vec4 GetColor() const
		{
			return glm::vec4{ cX, cY, cZ, cW };
		}
	};

	struct line_list_gizmo_instance
	{
		glm::mat4 transformation;
		glm::vec4 color;
	};

	struct pattern_texture_descriptor
	{
		const void*                              data;
		tao_ogl_resources::ogl_texture_format    data_format;
		tao_ogl_resources::ogl_texture_data_type data_type;
		unsigned int                             width;
		unsigned int                             height;
		unsigned int                             pattern_length;
	};

	struct line_list_gizmo_descriptor
	{
		std::vector<LineGizmoVertex>& vertices;
		unsigned int				  line_size					  = 1;
		bool						  zoom_invariant			  = false;
		float						  zoom_invariant_scale		  = 0.0f;
		pattern_texture_descriptor*   pattern_texture_descriptor  = nullptr;
	};

	class LineListGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _vertexCount;
		unsigned int _instanceCount = 0;
		
		// graphics data
		// -----------------------------
		std::vector<glm::vec3> _vertices;
		std::vector<glm::mat4> _transformList;

		ResizableVbo								   _vbo;
		ResizableSsbo								   _ssboInstanceColor;
		ResizableSsbo								   _ssboInstanceTransform;
		tao_ogl_resources::OglVertexAttribArray        _vao;
		std::optional<tao_ogl_resources::OglTexture2D> _patternTexture;

		// settings
		// -------------------------------
		unsigned int _lineSize;
		unsigned int _patternSize;

		// zoom invariance ---------------
		bool  _isZoomInvariant = false;
		float _zoomInvariantScale;

		// this will be called by GizomsRenderer instances
		LineListGizmo(tao_render_context::RenderContext& rc, const line_list_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<glm::mat4>& transformations, const std::vector<glm::vec4>& colors);
	};

	struct line_strip_gizmo_instance
	{
		glm::mat4			transformation;
		glm::vec4			color;
	};

	struct line_strip_gizmo_descriptor
	{
		std::vector<LineGizmoVertex>&	vertices;
		bool							isLoop						= false;
		unsigned int					line_size					= 1;
		bool							zoom_invariant				= false;
		float							zoom_invariant_scale		= 0.0f;
		pattern_texture_descriptor*		pattern_texture_descriptor	= nullptr;
	};

	class LineStripGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _instanceCount				= 0;
		
		// graphics data
		// -----------------------------
		std::vector<glm::vec3> _vertices;        
		std::vector<glm::mat4> _transformList;    // one transformation per instance.

		ResizableVbo									_vboVertices;     
		ResizableSsbo									_ssboInstanceTransform;
		ResizableSsbo									_ssboInstanceColor;
		tao_ogl_resources::OglVertexAttribArray			_vao;
		std::optional<tao_ogl_resources::OglTexture2D>  _patternTexture;

		// settings
		// -------------------------------
		unsigned int _lineSize;
		unsigned int _patternSize;

		// zoom invariance ---------------
		bool  _isZoomInvariant = false;
		float _zoomInvariantScale;

		// this will be called by GizomsRenderer instances
		LineStripGizmo(tao_render_context::RenderContext& rc, const line_strip_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<glm::mat4>& transforms, const std::vector<glm::vec4>& colors);
	};


	struct MeshGizmoVertex
	{
	private:
		/* Position */ float pX, pY, pZ;
		/* Normal   */ float nX, nY, nZ;
		/* Color    */ float cX, cY, cZ, cW;
		/* TexCoord */ float tX, tY;
	public:
		MeshGizmoVertex()
		{
			pX = pY = pZ		= 0.0f;
			nX = nY = nZ		= 0.0f;
			cX = cY = cZ = cW	= 0.0f;
			tX = tY				= 0.0f;
		}

		MeshGizmoVertex& Position(glm::vec3 p)
		{
			pX = p.x;
			pY = p.y;
			pZ = p.z;
			return *this;
		}
		glm::vec3 GetPosition() const
		{
			return glm::vec3{ pX, pY, pZ };
		}

		MeshGizmoVertex& Normal(glm::vec3 n)
		{
			nX = n.x;
			nY = n.y;
			nZ = n.z;
			return *this;
		}
		glm::vec3 GetNormal() const
		{
			return glm::vec3{ nX, nY, nZ };
		}

		MeshGizmoVertex& Color(glm::vec4 c)
		{
			cX = c.x;
			cY = c.y;
			cZ = c.z;
			cW = c.w;
			return *this;
		}
		glm::vec4 GetColor() const
		{
			return glm::vec4{ cX, cY, cZ, cW};
		}

		MeshGizmoVertex& TexCoord(glm::vec2 t)
		{
			tX = t.x;
			tY = t.y;
			return *this;
		}
		glm::vec2 GetTexCoord() const
		{
			return glm::vec2{ tX, tY };
		}
	};

	struct mesh_gizmo_instance
	{
		glm::mat4	transformation;
		glm::vec4	color;
	};

	struct mesh_gizmo_descriptor
	{
		std::vector<MeshGizmoVertex>&    vertices;
		std::vector<unsigned int>*		 triangles = nullptr;
		bool							 zoom_invariant       = false;
		float							 zoom_invariant_scale = 0.0f;
	};

	class MeshGizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _instanceCount				= 0;
		unsigned int _vertexCount				= 0;
		unsigned int _trisCount					= 0;

		// graphics data
		// --------------------------------------------------------
		std::vector<glm::vec3>	   _vertices;
		std::vector<unsigned int>  _triangles;
		// instance data ------------------------------------------
		std::vector<glm::mat4> _transformList;    // instance transformation
		std::vector<glm::vec4> _colorList;        // instance color

		ResizableVbo							    _vboVertices;
		tao_ogl_resources::OglVertexAttribArray		_vao;
		ResizableEbo								_ebo;
		ResizableSsbo								_ssboInstanceTransform;
		ResizableSsbo								_ssboInstanceColorAndNrmMat;

		// zoom invariance ---------------
		bool  _isZoomInvariant = false;
		float _zoomInvariantScale;

		// this will be called by GizomsRenderer instances
		MeshGizmo(tao_render_context::RenderContext& rc, const mesh_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<glm::mat4>& transforms, const std::vector<glm::vec4>& colors);
	};


	class GizmosRenderer
	{
	private:
		static constexpr const char*       SHADER_SRC_DIR = "C:/Users/Admin/Documents/LearnOpenGL/TestApp_OpenGL/TaOgl_Gizmos/Shaders"; //todo: this is obviously a joke
		static constexpr const char*       EXC_PREAMBLE   = "GizmosRenderer: ";
		int                                _windowWidth, _windowHeight;
		tao_render_context::RenderContext* _renderContext;

		tao_ogl_resources::OglShaderProgram _pointsShader;
		tao_ogl_resources::OglShaderProgram _linesShader;
		tao_ogl_resources::OglShaderProgram _lineStripShader;
		tao_ogl_resources::OglShaderProgram _meshShader;

		tao_ogl_resources::OglUniformBuffer _pointsObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _linesObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _lineStripObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _meshObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _frameDataUbo;

		ResizableSsbo _lenghSumSsbo; 

		tao_ogl_resources::OglSampler _nearestSampler;
		tao_ogl_resources::OglSampler _linearSampler;

		tao_ogl_resources::OglTexture2DMultisample _colorTex;
		tao_ogl_resources::OglTexture2DMultisample _depthTex;

		tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2DMultisample> _mainFramebuffer;

		std::map<unsigned int, PointGizmo>		_pointGizmos;
		std::map<unsigned int, LineListGizmo>		_lineGizmos;
		std::map<unsigned int, LineStripGizmo>  _lineStripGizmos;
		std::map<unsigned int, MeshGizmo>       _meshGizmos;

		void RenderPointGizmos();
		void RenderLineGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
		void RenderLineStripGizmos	(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
		void RenderMeshGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

	public:
		GizmosRenderer(tao_render_context::RenderContext& rc, int windowWidth, int windowHeight);

		// todo: projection matrix is not a parameter (near and far should be modified)
		[[nodiscard]] const tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2DMultisample>& Render(glm::mat4 viewMatrix, glm::mat4 projectionMatrix, glm::vec2 nearFar);

		///////////////////////////////////////
 		// Point Gizmos
		void CreatePointGizmo	(unsigned int pointGizmoIndex, const point_gizmo_descriptor& desc);
		void InstancePointGizmo	(unsigned int pointGizmoIndex, const std::vector<point_gizmo_instance>& instances);
		void DestroyPointGizmo	(unsigned int pointGizmoIndex);

		///////////////////////////////////////
		// Line Gizmos
		void CreateLineGizmo	(unsigned int lineGizmoIndex, const line_list_gizmo_descriptor& desc);
		void InstanceLineGizmo	(unsigned int lineGizmoIndex, const std::vector<line_list_gizmo_instance>& instances);
		void DestroyLineGizmo	(unsigned int lineGizmoIndex);

		///////////////////////////////////////
		// LineStrip Gizmos
		void CreateLineStripGizmo	(unsigned int lineStripGizmoIndex, const line_strip_gizmo_descriptor& desc);
		void InstanceLineStripGizmo	(unsigned int lineStripGizmoIndex, const std::vector<line_strip_gizmo_instance>& instances);
		void DestroyLineStripGizmo	(unsigned int lineStripGizmoIndex);

		///////////////////////////////////////
		// Mesh Gizmos
		void CreateMeshGizmo	(unsigned int meshGizmoIndex, const mesh_gizmo_descriptor& desc);
		void InstanceMeshGizmo	(unsigned int meshGizmoIndex, const std::vector<mesh_gizmo_instance>& instances);
		void DestroyMeshGizmo	(unsigned int meshGizmoIndex);
	};
}
