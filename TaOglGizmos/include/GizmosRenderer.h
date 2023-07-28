#pragma once
#include "RenderContext.h"
#include <glm/glm.hpp>
#include <map>
#include <optional>
#include <glm/gtc/type_ptr.hpp>
#include <queue>

namespace tao_gizmos
{
	struct gizmo_id
	{
		friend class GizmosRenderer;
	public:
		bool operator==(const gizmo_id& other) const
		{
			return this->_key == other._key;
		}
	private:
		unsigned long long _key=0;
	};

	struct gizmo_instance_id
	{
		friend class GizmosRenderer;
	public:
		bool operator==(const gizmo_instance_id& other) const
		{
			return
				this->_gizmoKey		== other._gizmoKey		&&
				this->_instanceKey	== other._instanceKey	;
		}
	private:
		gizmo_id _gizmoKey;
		unsigned int _instanceKey=0;
	};

	struct RenderLayer
	{
		friend class GizmosRenderer;

	private:
		unsigned int _guid = 0;
		tao_ogl_resources::ogl_depth_state		depth_state;
		tao_ogl_resources::ogl_rasterizer_state rasterizer_state;
		tao_ogl_resources::ogl_blend_state		blend_state;

		explicit RenderLayer(unsigned int progressiveID)
		{
			if (progressiveID > 31) throw std::runtime_error{ "RenderLayer: Invalid GUID" };

			_guid = 1 << progressiveID;
		}
	};

	struct RenderPass
	{
		friend class GizmosRenderer;

	private:
		unsigned int _layersMask = 0;

		explicit RenderPass(unsigned int layerMask):_layersMask(layerMask)
		{

		}
	};

	struct gizmo_instance_descriptor
	{
		glm::mat4    transform	= glm::mat4{1.0f};
		glm::vec4    color		= glm::vec4{1.0f};
		bool		 visible	= true;
		bool		 selectable = true;
	};

	// TODO: virtual destructor and deleted methods
	class Gizmo
	{
		friend class GizmosRenderer;
		
	private:
		unsigned int _layerMask = 0xFFFFFFFF;

	protected:

		// instance data (transform, visibility, color, ...)
		// ---------------------------------------------------
		unsigned int							_instanceCount;
		std::vector<gizmo_instance_descriptor>	_instanceData;

		virtual void									SetInstanceData(const std::vector<gizmo_instance_descriptor>& instances)
		{
			_instanceCount = instances.size();
			_instanceData  = instances;
		};
		const std::vector<gizmo_instance_descriptor>&	GetInstanceData()const { return _instanceData; }

		// zoom invariance
		// ---------------------------------------------------
		bool  _isZoomInvariant = false;
		float _zoomInvariantScale;
	};

	struct symbol_atlas_descriptor
	{
		const void*                              data;
		tao_ogl_resources::ogl_texture_format    data_format;
		tao_ogl_resources::ogl_texture_data_type data_type;
		unsigned int                             width;
		unsigned int                             height;
		bool                                     filter_smooth;
	};

	struct PointGizmoVertex
	{
	private:
		/* Position  */ float pX, pY, pZ;
		/* Color     */ float cX, cY, cZ, cW;
		/* Tex Coord */ float sU, sV, eU, eV;

	public:
		PointGizmoVertex()
		{
			pX = pY = pZ = 0.0f;
			sU = sV = eU = eV = 0.0f;
			cX = cY = cZ = cW = 0.0f;
		}

		PointGizmoVertex& Position(glm::vec3 p)
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

		PointGizmoVertex& TexCoordStart(glm::vec2 c)
		{
			sU = c.x;
			sV = c.y;
			return *this;
		}

		glm::vec2 GetTexCoordStart() const
		{
			return glm::vec2{ sU, sV };
		}

		PointGizmoVertex& TexCoordEnd(glm::vec2 c)
		{
			eU = c.x;
			eV = c.y;
			return *this;
		}
		glm::vec2 GetTexCoordEnd() const
		{
			return glm::vec2{ eU, eV };
		}

		PointGizmoVertex& Color(glm::vec4 c)
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

	enum class gizmo_usage_hint
	{
		usage_static,
		usage_dynamic
	};

	struct point_gizmo_descriptor
	{
		unsigned int					point_half_size = 1;
		bool							snap_to_pixel = false;
		std::vector<PointGizmoVertex>&	vertices;
		unsigned int					line_size = 1;
		bool							zoom_invariant = false;
		float							zoom_invariant_scale = 0.0f;
		symbol_atlas_descriptor*		symbol_atlas_descriptor = nullptr;
		gizmo_usage_hint				usage_hint = gizmo_usage_hint::usage_static;
	};

	class PointGizmo : public Gizmo
	{
		friend class GizmosRenderer;
	private:
			
		// graphics data -----------------
		unsigned int _vertexCount = 0;
		tao_render_context::ResizableVbo			   _vbo;
        tao_render_context::ResizableSsbo			   _ssboInstanceColor;
        tao_render_context::ResizableSsbo			   _ssboInstanceTransform;
        tao_render_context::ResizableSsbo			   _ssboInstanceVisibility;
        tao_render_context::ResizableSsbo			   _ssboInstanceSelectability;
		tao_ogl_resources::OglVertexAttribArray        _vao;
		std::optional<tao_ogl_resources::OglTexture2D> _symbolAtlas;
		
		// settings ----------------------
		unsigned int _pointSize;
		bool		 _snap;
		bool		 _symbolAtlasLinearFilter;

		// this will be called by GizomsRenderer instances
		PointGizmo(tao_render_context::RenderContext& rc, const point_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<gizmo_instance_descriptor>& instances) override;
		
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
		gizmo_usage_hint			  usage_hint = gizmo_usage_hint::usage_static;
	};

	class LineListGizmo : public Gizmo
	{
		friend class GizmosRenderer;
	private:
		
		// graphics data
		// -----------------------------
		unsigned int _vertexCount = 0;
		tao_render_context::ResizableVbo			   _vbo;
		tao_render_context::ResizableSsbo			   _ssboInstanceColor;
		tao_render_context::ResizableSsbo			   _ssboInstanceTransform;
		tao_render_context::ResizableSsbo			   _ssboInstanceVisibility;
		tao_render_context::ResizableSsbo			   _ssboInstanceSelectability;
		tao_ogl_resources::OglVertexAttribArray        _vao;
		std::optional<tao_ogl_resources::OglTexture2D> _patternTexture;

		// settings
		// -------------------------------
		unsigned int _lineSize;
		unsigned int _patternSize;

		// this will be called by GizomsRenderer instances
		LineListGizmo(tao_render_context::RenderContext& rc, const line_list_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<gizmo_instance_descriptor>& instances) override;

	};

	struct line_strip_gizmo_descriptor
	{
		std::vector<LineGizmoVertex>&	vertices;
		bool							isLoop						= false;
		unsigned int					line_size					= 1;
		bool							zoom_invariant				= false;
		float							zoom_invariant_scale		= 0.0f;
		pattern_texture_descriptor*		pattern_texture_descriptor	= nullptr;
		gizmo_usage_hint				usage_hint = gizmo_usage_hint::usage_static;
	};

	class LineStripGizmo : public Gizmo
	{
		friend class GizmosRenderer;
	private:
		// graphics data
		// -----------------------------
		unsigned int		   _vertexCount = 0;
		std::vector<glm::vec3> _vertices;        

		tao_render_context::ResizableVbo				_vboVertices;
		tao_render_context::ResizableSsbo				_ssboInstanceColor;
		tao_render_context::ResizableSsbo				_ssboInstanceTransform;
		tao_render_context::ResizableSsbo			    _ssboInstanceVisibility;
		tao_render_context::ResizableSsbo			    _ssboInstanceSelectability;
		tao_ogl_resources::OglVertexAttribArray			_vao;
		std::optional<tao_ogl_resources::OglTexture2D>  _patternTexture;

		// settings
		// -------------------------------
		unsigned int _lineSize;
		unsigned int _patternSize;

		// this will be called by GizomsRenderer instances
		LineStripGizmo(tao_render_context::RenderContext& rc, const line_strip_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<gizmo_instance_descriptor>& instances) override;
		
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
	
	struct mesh_gizmo_descriptor
	{
		std::vector<MeshGizmoVertex>&    vertices;
		std::vector<int>*		         triangles = nullptr;
		bool							 zoom_invariant       = false;
		float							 zoom_invariant_scale = 0.0f;
		gizmo_usage_hint				 usage_hint = gizmo_usage_hint::usage_static;
	};

	class MeshGizmo : public Gizmo
	{
		friend class GizmosRenderer;
	private:
		unsigned int _vertexCount				= 0;
		unsigned int _trisCount					= 0;
		
		// graphics data
		// --------------------------------------------------------
		std::vector<glm::vec3>	   _vertices;
		std::vector<int>  _triangles;
		

		tao_render_context::ResizableVbo		    _vboVertices;
		tao_render_context::ResizableEbo			_ebo;
		tao_render_context::ResizableSsbo			_ssboInstanceTransform;
		tao_render_context::ResizableSsbo			_ssboInstanceVisibility;
		tao_render_context::ResizableSsbo			_ssboInstanceSelectability;
		tao_render_context::ResizableSsbo			_ssboInstanceColorAndNrmMat;
		tao_ogl_resources::OglVertexAttribArray		_vao;

		// this will be called by GizomsRenderer instances
		MeshGizmo(tao_render_context::RenderContext& rc, const mesh_gizmo_descriptor& desc);
		void SetInstanceData(const std::vector<gizmo_instance_descriptor>& instances) override;

	};

	constexpr tao_ogl_resources::ogl_depth_state GetDefaultDepthState()
	{
		return tao_ogl_resources::ogl_depth_state{
		.depth_test_enable = true,
		.depth_write_enable = true,
		.depth_func = tao_ogl_resources::depth_func_less_equal,
		.depth_range_near = 0.0,
		.depth_range_far = 1.0
		};
	}
	constexpr tao_ogl_resources::ogl_blend_state GetDefaultBlendState()
	{
		return tao_ogl_resources::ogl_blend_state{
		.blend_enable = false,
		};
	}
	constexpr tao_ogl_resources::ogl_rasterizer_state GetDefaultRasterizerState()
	{
		return tao_ogl_resources::ogl_rasterizer_state{
		.culling_enable = false,
		.polygon_mode = tao_ogl_resources::polygon_mode_fill,
		.multisample_enable = true,
		.alpha_to_coverage_enable = true
		};
	}
	constexpr tao_ogl_resources::ogl_depth_state GetDepthStateForSelection()
	{
		return tao_ogl_resources::ogl_depth_state{
		.depth_test_enable = true,
		.depth_write_enable = true,
		.depth_func = tao_ogl_resources::depth_func_less_equal,
		.depth_range_near = 0.0,
		.depth_range_far = 1.0
		};
	}
	constexpr tao_ogl_resources::ogl_blend_state GetBlendStateForSelection()
	{
		return tao_ogl_resources::ogl_blend_state{
		.blend_enable = false,
		};
	}
	constexpr tao_ogl_resources::ogl_rasterizer_state GetRasterizerStateForSelection()
	{
		return tao_ogl_resources::ogl_rasterizer_state{
		.culling_enable = false,
		.polygon_mode = tao_ogl_resources::polygon_mode_fill,
		.multisample_enable = true,
		.alpha_to_coverage_enable = false
		};
	}
	
	struct SelectionRequest
	{
		tao_ogl_resources::OglPixelPackBuffer& _pbo;
		tao_ogl_resources::OglFence _fence;
		unsigned int _imageWidth;
		unsigned int _imageHeight;
		unsigned int _posX;
		unsigned int _posY;
		std::vector<std::pair<unsigned long long, glm::ivec2>> _gizmoKeyIndicesLUT;
	};

	class GizmosRenderer
	{
	private:
		static constexpr const char*       EXC_PREAMBLE   = "GizmosRenderer: ";
		int                                _windowWidth, _windowHeight;
		tao_render_context::RenderContext* _renderContext;

		tao_ogl_resources::OglShaderProgram _pointsShader;
		tao_ogl_resources::OglShaderProgram _linesShader;
		tao_ogl_resources::OglShaderProgram _lineStripShader;
		tao_ogl_resources::OglShaderProgram _meshShader;
		void InitShaders();

		tao_ogl_resources::OglShaderProgram _pointsShaderForSelection;
		tao_ogl_resources::OglShaderProgram _linesShaderForSelection;
		tao_ogl_resources::OglShaderProgram _lineStripShaderForSelection;
		tao_ogl_resources::OglShaderProgram _meshShaderForSelection;
		void InitShadersForSelection();

		tao_ogl_resources::OglUniformBuffer _pointsObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _linesObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _lineStripObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _meshObjDataUbo;
		tao_ogl_resources::OglUniformBuffer _frameDataUbo;

		tao_render_context::ResizableSsbo _lenghSumSsbo;
		tao_render_context::ResizableSsbo _selectionColorSsbo;

		tao_ogl_resources::OglSampler _nearestSampler;
		tao_ogl_resources::OglSampler _linearSampler;

		/// Main Framebuffer and Textures
		////////////////////////////////////////////////
		tao_ogl_resources::OglTexture2DMultisample										_colorTex;
		tao_ogl_resources::OglTexture2DMultisample										_depthTex;
		tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2DMultisample>	_mainFramebuffer;
		void InitMainFramebuffer(int width, int height);

		///  Selection Framebuffer and Textures
		/// (false color drawing)
		////////////////////////////////////////////////
		tao_ogl_resources::OglTexture2D										_selectionColorTex;
		tao_ogl_resources::OglTexture2D										_selectionDepthTex;
		tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2D>	_selectionFramebuffer;
		void InitSelectionFramebuffer(int width, int height);

		unsigned int										_latestSelectionPBOIdx = 0;
		static constexpr unsigned int						_selectionPBOsCount = 3;
		std::vector<tao_ogl_resources::OglPixelPackBuffer>	_selectionPBOs;


		std::queue<SelectionRequest> _selectionRequests;

		std::map<unsigned short, PointGizmo>		_pointGizmos;
		std::map<unsigned short, LineListGizmo>		_lineGizmos;
		std::map<unsigned short, LineStripGizmo>	_lineStripGizmos;
		std::map<unsigned short, MeshGizmo>			_meshGizmos;

		unsigned short _latestPointKey		=0;
		unsigned short _latestLineKey		=0;
		unsigned short _latestLineStripKey	=0;
		unsigned short _latestMeshKey		=0;

		static constexpr unsigned long long KEY_MASK_POINT		= 0x0000'0000'0000'FFFF;
		static constexpr unsigned long long KEY_MASK_LINE		= 0x0000'0000'FFFF'0000;
		static constexpr unsigned long long KEY_MASK_LINE_STRIP	= 0x0000'FFFF'0000'0000;
		static constexpr unsigned long long KEY_MASK_MESH		= 0xFFFF'0000'0000'0000;

		[[nodiscard]]static unsigned long long  EncodeGizmoKey	(unsigned short		key, unsigned long long mask);
		[[nodiscard]]static unsigned short		DecodeGizmoKey	(unsigned long long key, unsigned long long mask);

		[[nodiscard]] static std::vector<gizmo_instance_id> CreateInstanceKeys(gizmo_id gizmoKey, unsigned int instanceCount);

		unsigned int _latestRenderLayerGuid = 2; // 1 is reserved for the `Default` layer
		
		std::vector<RenderPass>				_renderPasses;
		std::map<unsigned int, RenderLayer> _renderLayers;

		[[nodiscard]] unsigned int MakeLayerMask(std::initializer_list<RenderLayer> layers);

		static constexpr tao_ogl_resources::ogl_depth_state			DEFAULT_DEPTH_STATE			= GetDefaultDepthState();
		static constexpr tao_ogl_resources::ogl_blend_state			DEFAULT_BLEND_STATE			= GetDefaultBlendState();
		static constexpr tao_ogl_resources::ogl_rasterizer_state	DEFAULT_RASTERIZER_STATE	= GetDefaultRasterizerState();
		static constexpr tao_ogl_resources::ogl_depth_state			SELECTION_DEPTH_STATE		= GetDepthStateForSelection();
		static constexpr tao_ogl_resources::ogl_blend_state			SELECTION_BLEND_STATE		= GetBlendStateForSelection();
		static constexpr tao_ogl_resources::ogl_rasterizer_state	SELECTION_RASTERIZER_STATE	= GetRasterizerStateForSelection();

		[[nodiscard]] static RenderLayer DefaultLayer();

		[[nodiscard]]static bool ShouldRenderGizmo(const Gizmo& gzm, const RenderPass& currentPass, const RenderLayer& currentLayer);

		// Preprocessing, such as computing transformations
		// to achieve zoom invariance or computing screen
		// length for line strips (to apply a pattern)
		// ------------------------------------------------
		void ProcessPointGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
		void ProcessLineListGizmos	(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
		void ProcessLineStripGizmos	(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
		void ProcessMeshGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

		void RenderPointGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass);
		void RenderLineGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass);
		void RenderLineStripGizmos	(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass);
		void RenderMeshGizmos		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass);

		void RenderPointGizmosForSelection		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc);
		void RenderLineGizmosForSelection		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc);
		void RenderLineStripGizmosForSelection	(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc);
		void RenderMeshGizmosForSelection		(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc);

		[[nodiscard]]std::optional<gizmo_instance_id> GetGizmoKeyFromFalseColors(
			const unsigned char* pixelDataAsRGBAUint, unsigned int stride, 
			int imageWidth, int imageHeight,
			unsigned int posX, unsigned int posY, 
			std::vector<std::pair<unsigned long long, glm::ivec2>> lut
		);


		const std::function<void(std::optional<gizmo_instance_id>)>* _selectionCallback;

		void IssueSelectionRequest(
			unsigned int imageWidth, unsigned int imageHeight, unsigned int posX, unsigned int posY, 
			std::vector<std::pair<unsigned long long, glm::ivec2>> lut
		);

		void ProcessSelectionRequests();

	public:
		GizmosRenderer(tao_render_context::RenderContext& rc, int windowWidth, int windowHeight);

		// todo: projection matrix is not a parameter (near and far should be modified)
		[[nodiscard]] const tao_ogl_resources::OglFramebuffer<tao_ogl_resources::OglTexture2DMultisample>& Render(
			const glm::mat4& viewMatrix, 
			const glm::mat4& projectionMatrix,
			const glm::vec2& nearFar
		);

		void SetSelectionCallback(const std::function<void(std::optional<gizmo_instance_id>)>& callback);

		void GetGizmoUnderCursor(
			const unsigned int cursorX,
			const unsigned int cursorY,
			const glm::mat4& viewMatrix, 
			const glm::mat4& projectionMatrix,
			const glm::vec2& nearFar
		);

		///////////////////////////////////////
 		// Point Gizmos
		[[nodiscard]] gizmo_id							CreatePointGizmo	   (const point_gizmo_descriptor& desc);
		// TODO: InstanceGizmo
		[[nodiscard]] std::vector<gizmo_instance_id>	InstancePointGizmo	   (const gizmo_id key, const std::vector<gizmo_instance_descriptor>& instances);
		void											DestroyPointGizmo	   (const gizmo_id key);

		///////////////////////////////////////
		// Line Gizmos
		[[nodiscard]] gizmo_id							CreateLineGizmo			  (const line_list_gizmo_descriptor& desc);
		// TODO: InstanceGizmo
		[[nodiscard]] std::vector<gizmo_instance_id>	InstanceLineGizmo		  (const gizmo_id key, const std::vector<gizmo_instance_descriptor>& instances);
		void											DestroyLineGizmo		  (const gizmo_id key);

		///////////////////////////////////////
		// LineStrip Gizmos
		[[nodiscard]] gizmo_id							CreateLineStripGizmo	   (const line_strip_gizmo_descriptor& desc);
		// TODO: InstanceGizmo
		[[nodiscard]] std::vector<gizmo_instance_id>	InstanceLineStripGizmo	   (const gizmo_id key, const std::vector<gizmo_instance_descriptor>& instances);
		void											DestroyLineStripGizmo	   (const gizmo_id key);

		///////////////////////////////////////
		// Mesh Gizmos
		[[nodiscard]] gizmo_id							CreateMeshGizmo		  (const mesh_gizmo_descriptor& desc);
		// TODO: InstanceGizmo
		[[nodiscard]] std::vector<gizmo_instance_id>	InstanceMeshGizmo	  (const gizmo_id key, const std::vector<gizmo_instance_descriptor>& instances);
		void											DestroyMeshGizmo	  (const gizmo_id key);

		void											SetGizmoInstances (const gizmo_id key, const std::vector<std::pair<gizmo_instance_id, gizmo_instance_descriptor>>& instances);

		///////////////////////////////////////
		// Render Passes and Layers
		[[nodiscard]] RenderLayer	CreateRenderLayer(
			const tao_ogl_resources::ogl_depth_state&		depthState,
			const tao_ogl_resources::ogl_blend_state&		blendState,
			const tao_ogl_resources::ogl_rasterizer_state&	rasterizerState
		);
		[[nodiscard]] RenderPass	CreateRenderPass(std::initializer_list<RenderLayer> layers);
		void						SetRenderPasses(std::initializer_list<RenderPass> passes);
		void						AssignGizmoToLayers(gizmo_id key, std::initializer_list<RenderLayer> layers);
	};
}
