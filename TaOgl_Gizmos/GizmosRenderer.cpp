#include "GizmosRenderer.h"

#include <ranges>

#include "GizmosShaderLib.h"
#include "GizmosUtils.h"
#include "Instrumentation.h"
#include <glm/gtc/type_ptr.hpp>

namespace tao_gizmos
{
	using namespace tao_render_context;
	using namespace tao_ogl_resources;
	using namespace std;
	using namespace glm;

	ogl_texture_internal_format TextureInternalFormat(ogl_texture_format format)
	{
		switch (format)
		{
		case(tex_for_rgba):
		case(tex_for_brga):
			return tex_int_for_rgba8; break;

		case(tex_for_bgr):
		case(tex_for_rgb):
			return tex_int_for_rgb8; break;

		default:
			throw exception("Unsupported symbol atlas format");
		}
	}

	class BufferDataPacker
	{
		struct DataPtrWithFormat
		{
			unsigned int   elementSize;
			unsigned int   elementsCount;
			const unsigned char* dataPtr;
		};
		std::vector<DataPtrWithFormat> dataArrays;

	public:
		BufferDataPacker() :
			dataArrays(0)
		{
		}

		template<glm::length_t L, typename T>
		BufferDataPacker& AddDataArray(const std::vector<glm::vec<L, T>>& data)
		{
			DataPtrWithFormat dataPtr
			{
				.elementSize	= sizeof(T) * L,
				.elementsCount	= static_cast<unsigned int>(data.size()),
				.dataPtr		= reinterpret_cast<const unsigned char*>(data.data())
			};

			dataArrays.push_back(dataPtr);
			return *this;
		}

		template<glm::length_t C, glm::length_t R, typename T>
		BufferDataPacker& AddDataArray(const std::vector<glm::mat<C, R, T>>& data)
		{
			DataPtrWithFormat dataPtr
			{
				.elementSize	= sizeof(T) * C * R,
				.elementsCount	= static_cast<unsigned int>(data.size()),
				.dataPtr		= reinterpret_cast<const unsigned char*>(data.data())
			};

			dataArrays.push_back(dataPtr);
			return *this;
		}

		std::vector<unsigned char> InterleavedBuffer() const
		{
			// early exit if empty
			if (dataArrays.empty()) return std::vector<unsigned char>{};

			// check all the arrays to be
			// the same length
			const unsigned int elementCount = dataArrays[0].elementsCount;
			unsigned int cumulatedElementSize = 0;
			for(const auto& d : dataArrays)
			{
				if (d.elementsCount != elementCount) 
					throw std::exception("BufferDataPacker: Invalid input. Interleaving requies all the arrays to be the same length.");

				cumulatedElementSize +=  d.elementSize;
			}

			std::vector<unsigned char> interleaved(elementCount * cumulatedElementSize);

			for (int i = 0; i < elementCount; i++)
			{
				unsigned int accum = 0;
				for (int j = 0; j < dataArrays.size(); j++)
				{
					memcpy
					(
						interleaved.data() + cumulatedElementSize * i + accum,
						dataArrays[j].dataPtr + i * dataArrays[j].elementSize,
						dataArrays[j].elementSize
					);

					accum += dataArrays[j].elementSize;

				}
			}

			return interleaved;
		}
	};


	unsigned int ResizeBuffer(unsigned int currVertCapacity, unsigned int newVertCount)
	{
		// initialize vbo size to the required count
		if (currVertCapacity == 0)					return newVertCount;

		// never shrink the size (heuristic)
		else if (currVertCapacity >= newVertCount) return currVertCapacity;

		// currVertSize < newVertCount -> increment size by the 
		// smallest integer multiple of the current size (heuristic)
		else 										return currVertCapacity * ((newVertCount / currVertCapacity) + 1);
	}

	 PointGizmo::PointGizmo(RenderContext& rc, const point_gizmo_descriptor& desc):
		_vbo{rc, 0, buf_usg_static_draw, ResizeBuffer },
		_ssboInstanceColor{rc, 0, buf_usg_static_draw, ResizeBuffer},
		_ssboInstanceTransform{ rc, 0, desc.zoom_invariant
														? buf_usg_dynamic_draw
														: buf_usg_static_draw, ResizeBuffer },
		_vao{rc.CreateVertexAttribArray()},
		_pointSize(desc.point_half_size),
		_snap(desc.snap_to_pixel),
		_isZoomInvariant{ desc.zoom_invariant },
		_zoomInvariantScale{ desc.zoom_invariant_scale }
	 {

		 _positionList = vector<vec3>{ desc.vertices.size() };
		 for (int i = 0; i < _positionList.size(); i++)_positionList[i] = desc.vertices[i].GetPosition();

		 // vbo layout for points:
		 // pos(vec3)-color(vec4)-tex(vec4) interleaved
		 //--------------------------------------------
		 int vertexSize = 11 * sizeof(float);
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0));
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3*sizeof(float)));
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 2, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(7*sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);
		 _vao.EnableVertexAttrib(2);

		 _vbo.Resize(vertexSize*desc.vertices.size());
		 _vbo.OglBuffer().SetSubData(0, vertexSize* desc.vertices.size(), desc.vertices.data());


		// texture atlas
		// --------------------------------------------
		if(desc.symbol_atlas_descriptor)
		{
			_symbolAtlasLinearFilter = desc.symbol_atlas_descriptor->filter_smooth;
			_symbolAtlas.emplace(rc.CreateTexture2D());
			_symbolAtlas.value().TexImage(
				0,
				TextureInternalFormat(desc.symbol_atlas_descriptor->data_format),
				desc.symbol_atlas_descriptor->width,
				desc.symbol_atlas_descriptor->height,
				desc.symbol_atlas_descriptor->data_format,
				desc.symbol_atlas_descriptor->data_type,
				desc.symbol_atlas_descriptor->data
			);
		}
	 }

	 void PointGizmo::SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors)
	 {
		 constexpr int vertexSize = sizeof(float) * 11; // position(3) + color(4) + texCoord(4)
		 const unsigned int posLen = positions.size();

		 if (posLen != colors.size())
			 throw exception{ "Instance data must match in count." };
		 
		 _instanceCount = posLen;

		 _transformList = vector<mat4>{ positions.size() };
		 for (int i = 0; i < _positionList.size(); i++)_positionList[i] = positions[i];

		 // Set instance transformation data
		 unsigned int dataSize = _transformList.size() * sizeof(float) * 16;
		 _ssboInstanceTransform.Resize(dataSize);
		 _ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, _transformList.data());

		 // Set instance color data
		 dataSize = colors.size() * sizeof(float) * 4;
		 _ssboInstanceColor.Resize(dataSize);
		 _ssboInstanceColor.OglBuffer().SetSubData(0, dataSize, colors.data());
	 }

	 LineListGizmo::LineListGizmo(RenderContext& rc, const line_list_gizmo_descriptor& desc) :
	     _vertices				(desc.vertices.size()),
		 _vbo					{ rc, 0, ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceColor		{ rc, 0, ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceTransform	{ rc, 0, 
		 						desc.zoom_invariant ? ogl_buffer_usage::buf_usg_dynamic_draw
		 											: ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _vao					{ rc.CreateVertexAttribArray() },
		 _lineSize				{ desc.line_size    },
		 _patternSize			{ 0 },
		 _isZoomInvariant		{ desc.zoom_invariant },
		 _zoomInvariantScale	{ desc.zoom_invariant_scale }
	 {
		 _vertexCount = desc.vertices.size();
		 for (int i = 0; i < _vertexCount; i++) _vertices[i] = desc.vertices[i].GetPosition();

		 // vbo layout for points:
		 // pos(vec3)-color(vec4)
		 //--------------------------------------------
		 const unsigned int vertexComponents = 7;
		 const unsigned int vertexSize = vertexComponents * sizeof(float);
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0));
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);

		 _vbo.Resize(vertexSize*_vertexCount);
		 _vbo.OglBuffer().SetSubData(0, vertexSize* _vertexCount, desc.vertices.data());

		 // pattern texture
		 // --------------------------------------------
		 if (desc.pattern_texture_descriptor)
		 {
			 _patternSize = desc.pattern_texture_descriptor->pattern_length;
			 _patternTexture.emplace(rc.CreateTexture2D());
			 _patternTexture.value().TexImage(
				 0,
				 TextureInternalFormat(desc.pattern_texture_descriptor->data_format),
				 desc.pattern_texture_descriptor->width,
				 desc.pattern_texture_descriptor->height,
				 desc.pattern_texture_descriptor->data_format,
				 desc.pattern_texture_descriptor->data_type,
				 desc.pattern_texture_descriptor->data
			 );
		 }
	 }

	 void LineListGizmo::SetInstanceData(const vector<mat4>& transformations, const vector<vec4>& colors)
	 {

		 if (transformations.size() != colors.size())
			 throw exception{ "Instance data must match in count." };

		 _instanceCount = transformations.size();
		 _transformList = transformations;

		 // Set instance transformation data
		 unsigned int dataSize = _transformList.size() * sizeof(float) * 16;
		 _ssboInstanceTransform.Resize(dataSize);
		 _ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transformations.data());

		 // Set instance color data
		 dataSize = colors.size() * sizeof(float) * 4;
		 _ssboInstanceColor.Resize(dataSize);
		 _ssboInstanceColor.OglBuffer().SetSubData(0, dataSize, colors.data());
	 }

	 LineStripGizmo::LineStripGizmo(RenderContext& rc, const line_strip_gizmo_descriptor& desc) :
		 _vertices				(desc.vertices.size()), 
		 _vboVertices			{ rc, 0, buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceTransform	{ rc, 0,
								desc.zoom_invariant ? ogl_buffer_usage::buf_usg_dynamic_draw
													: ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceColor		{ rc, 0, ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _vao					{ rc.CreateVertexAttribArray() },
		 _lineSize				{ desc.line_size },
		 _patternSize			{ 0 },
		 _isZoomInvariant		{ desc.zoom_invariant },
		 _zoomInvariantScale	{ desc.zoom_invariant_scale }
	 {
		 // vbo layout for linestrip:
		 // vbo 0: pos(vec3)-color(vec4)
		 //--------------------------------------------
		 constexpr int vertexSize   = 7 * sizeof(float);
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0 * sizeof(float)));
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);

		 // allocate memory and fill vbo
		 // --------------------------------------------

		 // adjacency data for start and end point
		 LineGizmoVertex adjStart, adjEnd;
		 if(desc.isLoop)
		 {
			 adjStart	= *(desc.vertices.  end() -2);
			 adjEnd		= *(desc.vertices.begin() +1);
		 }
		 else
		 {
			 vec3 start	= 2.0f * (desc.vertices.begin())->GetPosition() - (desc.vertices.begin() + 1)->GetPosition();
			 vec3 end	= 2.0f * (desc.vertices.end()-1)->GetPosition() - (desc.vertices  .end() - 2)->GetPosition();

			 adjStart = LineGizmoVertex{}
		 	.Position(start)
		 	.Color(desc.vertices.begin()->GetColor());

			 adjEnd  = LineGizmoVertex{}
		 	.Position(end)
		 	.Color((desc.vertices.end()-1)->GetColor());
		 }

		// copy and store position data
		// needed to compute per-vertex screen length
		 for (int i = 0; i < _vertices.size(); i++) 
			 _vertices[i] = desc.vertices[i].GetPosition();

		 _vertices.insert(_vertices.begin(), adjStart.GetPosition());
		 _vertices.push_back(adjEnd.GetPosition());

		 vector<LineGizmoVertex> myVertsAdj = desc.vertices;
		 myVertsAdj.insert(myVertsAdj.begin(), adjStart);
		 myVertsAdj.push_back(adjEnd);

		 _vboVertices.Resize(myVertsAdj.size()* vertexSize);
		 _vboVertices.OglBuffer().SetSubData(0, myVertsAdj.size()* vertexSize, myVertsAdj.data());

		 int s = sizeof(LineGizmoVertex);

		 // pattern texture
		 // --------------------------------------------
		 if (desc.pattern_texture_descriptor)
		 {
			 _patternSize = desc.pattern_texture_descriptor->pattern_length;
			 _patternTexture.emplace(rc.CreateTexture2D());
			 _patternTexture.value().TexImage(
				 0,
				 TextureInternalFormat(desc.pattern_texture_descriptor->data_format),
				 desc.pattern_texture_descriptor->width,
				 desc.pattern_texture_descriptor->height,
				 desc.pattern_texture_descriptor->data_format,
				 desc.pattern_texture_descriptor->data_type,
				 desc.pattern_texture_descriptor->data
			 );
		 }
	 }

	 void LineStripGizmo::SetInstanceData(const vector<mat4>& transforms, const vector<vec4>& colors)
	 {
		 if (transforms.size() != colors.size())throw exception{ "Instance data must match in count." };

		 _instanceCount = transforms.size();
		 _transformList = transforms;
		
		 // Set instance transformation data
		 unsigned int dataSize = _transformList.size() * sizeof(float) * 16;
		 _ssboInstanceTransform.Resize(dataSize);
		 _ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transforms.data());
		 
		 // Set instance color data
		 dataSize = _transformList.size() * sizeof(float) * 4;
		 _ssboInstanceColor.Resize(dataSize);
		 _ssboInstanceColor.OglBuffer().SetSubData(0, dataSize, colors.data());
	 }

	 MeshGizmo::MeshGizmo(RenderContext& rc, const mesh_gizmo_descriptor& desc) :
		 _vertices					{ desc.vertices.size() },
		 _triangles					{ *desc.triangles },
		 _vboVertices				{ rc, 0,  buf_usg_static_draw, ResizeBuffer},
		 _vao						{ rc.CreateVertexAttribArray() },
		 _ebo						{ rc, 0,  buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceTransform		{ rc, 0,
									desc.zoom_invariant ? ogl_buffer_usage::buf_usg_dynamic_draw
														: ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceColorAndNrmMat{ rc, 0, ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
	     _isZoomInvariant			{desc.zoom_invariant},
		 _zoomInvariantScale        {desc.zoom_invariant_scale}
	    
	 {

		 // store vertex positions
		 for (int i=0;i<desc.vertices.size(); i++)
		 {
			 _vertices[i] = desc.vertices[i].GetPosition();
		 }
		 _vertexCount = desc.vertices.size();
		 _trisCount   = desc.triangles->size();

		 // VAO layout for mesh:
		 // vbo 0: Position (vec3) - Normal (vec3) - Color (vec4) - TexCoord (vec2)
		 //--------------------------------------------
		 constexpr int vertexComponents = 12;
		 constexpr int vertexSize = vertexComponents * sizeof(float);
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0  * sizeof(float)));
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 1, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3  * sizeof(float)));
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 2, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(6  * sizeof(float)));
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 3, 2, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(10 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);
		 _vao.EnableVertexAttrib(2);
		 _vao.EnableVertexAttrib(3);

		 // allocate memory and fill VBO
		 // --------------------------------------------
		 _vboVertices.Resize(_vertices.size()* vertexSize);
		 _vboVertices.OglBuffer().SetSubData(0, _vertices.size()* vertexSize, desc.vertices.data());


		// allocate memory and fill EBO
		// --------------------------------------------
		 _ebo.Resize(_triangles.size() * sizeof(int));
		 _ebo.OglBuffer().SetSubData(0, _triangles.size() * sizeof(int) , _triangles.data());

	 }

	 void MeshGizmo::SetInstanceData(const vector<mat4>& transforms, const vector<vec4>& colors)
	 {
		 if (transforms.size() != colors.size())throw exception{ "Instance data must match in count." };

		 _instanceCount = transforms.size();
		 _transformList = transforms;
		 _colorList     = colors;

		// Instance data:
		// mat4 transform - mat4 normal matrix - vec4 color
		// [ ----------- ]  [ --------------------------- ]
		//   per-frame					constant
		// (zoom invariance)
		//

		// Transform
		unsigned int dataSize = _transformList.size() * sizeof(float) * 16;
		_ssboInstanceTransform.Resize(dataSize);
		_ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transforms.data());

		// Normal matrix and Color
		vector<glm::mat4> normalMatrices{ _instanceCount };
		for (int i = 0; i < _transformList.size(); i++) normalMatrices[i] = inverse(transpose(_transformList[i]));
		BufferDataPacker pack{};
		const auto data = pack
			.AddDataArray(normalMatrices)
			.AddDataArray(colors)
			.InterleavedBuffer();
		_ssboInstanceColorAndNrmMat.Resize(data.size());
		_ssboInstanceColorAndNrmMat.OglBuffer().SetSubData(0, data.size(), data.data());
	 }

	 RenderLayer GizmosRenderer::DefaultLayer()
	 {
		 RenderLayer r{ 1 };
		 r.depth_state		= DEFAULT_DEPTH_STATE;
		 r.blend_state		= DEFAULT_BLEND_STATE;
		 r.rasterizer_state = DEFAULT_RASTERIZER_STATE;

		 return r;
	 }

	 GizmosRenderer::GizmosRenderer(RenderContext& rc, int windowWidth, int windowHeight) :
		 _windowWidth  (windowWidth),
		 _windowHeight (windowHeight),
		 _renderContext(&rc),
		 _pointsShader		 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::points   , SHADER_SRC_DIR)),
		 _linesShader		 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lines	   , SHADER_SRC_DIR)),
		 _lineStripShader	 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lineStrip, SHADER_SRC_DIR)),
		 _meshShader		 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::mesh     , SHADER_SRC_DIR)),
		 _pointsObjDataUbo	 (rc.CreateUniformBuffer()),
		 _linesObjDataUbo	 (rc.CreateUniformBuffer()),
		 _lineStripObjDataUbo(rc.CreateUniformBuffer()),
		 _meshObjDataUbo     (rc.CreateUniformBuffer()),
		 _frameDataUbo		 (rc.CreateUniformBuffer()),
		 _lenghSumSsbo		 {rc, 0, buf_usg_dynamic_draw, ResizeBuffer},
		 _nearestSampler	 (rc.CreateSampler()),
		 _linearSampler		 (rc.CreateSampler()),
		 _colorTex			 (rc.CreateTexture2DMultisample()),
		 _depthTex			 (rc.CreateTexture2DMultisample()),
		 _mainFramebuffer	 (rc.CreateFramebuffer<OglTexture2DMultisample>()),
		 _pointGizmos		 {},
		 _lineGizmos		 {},
		 _lineStripGizmos	 {},
		 _meshGizmos		 {},
		 _renderPasses		 {},
		 _renderLayers		 {}
	 {
		 // Init default render pass and layer
		 // -----------------------------------------
		 RenderLayer defaultLayer = DefaultLayer();
		 _renderLayers.insert(std::make_pair(defaultLayer._guid, defaultLayer));

		 RenderPass defaultPass{ 0 };
		 defaultPass._layersMask |= defaultLayer._guid;
		 _renderPasses.push_back(defaultPass);

		 // main fbo
		 // -----------------------------------------
		 _colorTex.TexImage(8, tex_int_for_rgba , _windowWidth, _windowHeight, false);
		 _depthTex.TexImage(8, tex_int_for_depth, _windowWidth, _windowHeight, false);

		 _mainFramebuffer.AttachTexture(fbo_attachment_color0, _colorTex, 0);
		 _mainFramebuffer.AttachTexture(fbo_attachment_depth,  _depthTex, 0);

		 // shaders ubo bindings
		 // -----------------------------------------
		 // per-object data
		 _pointsShader	 .SetUniformBlockBinding(POINTS_OBJ_DATA_BLOCK_NAME, POINTS_OBJ_DATA_BINDING);
		 _linesShader	 .SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME	   , LINES_OBJ_DATA_BINDING);
		 _lineStripShader.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME	   , LINE_STRIP_OBJ_DATA_BINDING);
		 _meshShader     .SetUniformBlockBinding(MESH_OBJ_BLOCK_NAME	   , MESH_OBJ_DATA_BINDING);

		 // per-frame data
		 _pointsShader	 .SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _lineStripShader.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _linesShader	 .SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _meshShader     .SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);

		 // samplers
		 _pointsShader	 .UseProgram(); _pointsShader	.SetUniform(POINTS_TEX_ATLAS_NAME, 0);
		 _linesShader	 .UseProgram(); _linesShader	.SetUniform(LINE_PATTERN_TEX_NAME, 0);
		 _lineStripShader.UseProgram(); _lineStripShader.SetUniform(LINE_PATTERN_TEX_NAME, 0);
		 // todo: RESET

		 // uniform buffers
		 // -----------------------------------------
		 _pointsObjDataUbo	  .SetData(sizeof(points_obj_data_block)	 , nullptr, buf_usg_static_draw);
		 _linesObjDataUbo	  .SetData(sizeof(lines_obj_data_block)		 , nullptr, buf_usg_static_draw);
		 _lineStripObjDataUbo .SetData(sizeof(line_strip_obj_data_block), nullptr, buf_usg_static_draw);
		 _meshObjDataUbo      .SetData(sizeof(mesh_obj_data_block)      , nullptr, buf_usg_static_draw);
		 _frameDataUbo		  .SetData(sizeof(frame_data_block)			 , nullptr, buf_usg_static_draw);

		// samplers
		// -----------------------------------------
		 _nearestSampler.SetParams(ogl_sampler_params{
			.filter_params{
				 .min_filter = sampler_min_filter_nearest,
				 .mag_filter = sampler_mag_filter_nearest
			},
			.wrap_params{
				 .wrap_s = sampler_wrap_clamp_to_edge,
				 .wrap_t = sampler_wrap_clamp_to_edge,
				 .wrap_r = sampler_wrap_clamp_to_edge
			}
		 });

		 _linearSampler.SetParams(ogl_sampler_params{
			.filter_params{
				  .min_filter = sampler_min_filter_linear,
				  .mag_filter = sampler_mag_filter_linear
			},
			.wrap_params{
				 .wrap_s = sampler_wrap_clamp_to_edge,
				 .wrap_t = sampler_wrap_clamp_to_edge,
				 .wrap_r = sampler_wrap_clamp_to_edge
			}
		});
	 }

	template<typename KeyType, typename ValType>
	void CheckKeyUniqueness(const std::map<KeyType, ValType>& map, KeyType key, const char* exceptionPreamble)
	{
		 if (map.contains(key))
			 throw exception(
				 string{}.append(exceptionPreamble)
				 .append("an element at index ")
				 .append(to_string(key))
				 .append(" already exist.").c_str());

	}

	 template<typename KeyType, typename ValType>
	 void CheckKeyPresent(const std::map<KeyType, ValType>& map, KeyType key, const char* exceptionPreamble)
	 {
		 if (!map.contains(key))
			 throw exception(
				 string{}.append(exceptionPreamble)
				 .append("an element at index ")
				 .append(to_string(key))
				 .append(" doesn't exist.").c_str());
	 }

	 void CheckKeyValid(unsigned short key, const char* exceptionPreamble)
	{
		if(!key)
			throw exception(
				string{}.append(exceptionPreamble)
				.append("invalid key.")
				.c_str());
	}

	unsigned long long  GizmosRenderer::EncodeGizmoKey(unsigned short key, unsigned long long mask)
	{
		unsigned long long longKey = 0;
		unsigned long long k	   = key;
		longKey |= (k << 48);
		longKey |= (k << 32);
		longKey |= (k << 16);
		longKey |= (k << 00);

		return longKey & mask;
	}

	unsigned short GizmosRenderer::DecodeGizmoKey(unsigned long long key, unsigned long long mask)
	{
		unsigned long long longKey = 0;
		unsigned long long k = key & mask;
		longKey =
			(k << 48) | (k >> 16) |
			(k << 32) | (k >> 32) |
			(k << 16) | (k >> 48) ;

		return static_cast<unsigned short>(longKey);
	}

	std::vector<gizmo_instance_id> GizmosRenderer::CreateInstanceKeys(const gizmo_id gizmoKey, unsigned instanceCount)
	{
		// Instance ID:
		// - gizmo key
		// - a progressive id for the instance

		vector<gizmo_instance_id> keys(instanceCount);
		for (int i = 0; i < instanceCount; i++)
		{
			keys[i] = gizmo_instance_id{};
			keys[i]._gizmoKey = gizmoKey;
			keys[i]._instanceKey = i;
		}

		return keys;
	}

	RenderLayer GizmosRenderer::CreateRenderLayer(
		const tao_ogl_resources::ogl_depth_state& depthState,
		const tao_ogl_resources::ogl_blend_state& blendState,
		const tao_ogl_resources::ogl_rasterizer_state& rasterizerState
	)
	{
		RenderLayer l{ _latestRenderLayerGuid++ };
		l.blend_state = blendState;
		l.rasterizer_state = rasterizerState;
		l.depth_state = depthState;

		_renderLayers.insert(make_pair(l._guid, l));

		return l;
	}

	unsigned int GizmosRenderer::MakeLayerMask(std::initializer_list<RenderLayer> layers)
	{
		unsigned int mask = 0;
		for (const RenderLayer& l : layers)
		{
			if (!_renderLayers.contains(l._guid))
				throw std::exception{ "Referencing non-existing render layer" };

			mask |= l._guid;
		}

		return mask;
	}

	RenderPass GizmosRenderer::CreateRenderPass(std::initializer_list<RenderLayer> layers)
	{
		RenderPass r{ MakeLayerMask(layers) };

		return r;
	}

	void GizmosRenderer::SetRenderPasses(std::initializer_list<RenderPass> passes)
	{
		_renderPasses = passes;
	}


	void GizmosRenderer::AssignGizmoToLayers(gizmo_id key, std::initializer_list<RenderLayer> layers)
	{
		// TODO: this thing is really ugly

		const unsigned int layerMask = MakeLayerMask(layers);

		// Try with points...
		auto k = DecodeGizmoKey(key._key, KEY_MASK_POINT);
		if (_pointGizmos.contains(k))
		{
			_pointGizmos.at(k)._layerMask = layerMask;
			return;
		}
		// Try with lines...
		k = DecodeGizmoKey(key._key, KEY_MASK_LINE);
		if (_lineGizmos.contains(k))
		{
			_lineGizmos.at(k)._layerMask = layerMask;
			return;
		}
		// Try with line strips...
		k = DecodeGizmoKey(key._key, KEY_MASK_LINE_STRIP);
		if (_lineStripGizmos.contains(k))
		{
			_lineStripGizmos.at(k)._layerMask = layerMask;
			return;
		}
		// Try with meshes...
		k = DecodeGizmoKey(key._key, KEY_MASK_MESH);
		if (_meshGizmos.contains(k))
		{
			_meshGizmos.at(k)._layerMask = layerMask;
			return;
		}
		// Ok it doesn't exist...throw
		throw std::exception{ "Invalid key" };
	}

	gizmo_id GizmosRenderer::CreatePointGizmo(const point_gizmo_descriptor& desc)
	 {
		const auto longKey = EncodeGizmoKey(++_latestPointKey, KEY_MASK_POINT);
		const auto key	 = DecodeGizmoKey(longKey, KEY_MASK_POINT);

		CheckKeyUniqueness(_pointGizmos, key, EXC_PREAMBLE);

		_pointGizmos.insert(make_pair(key, PointGizmo{ *_renderContext, desc }));

		gizmo_id r{};
		r._key = longKey;

		return r;
	 }

	void GizmosRenderer::DestroyPointGizmo(gizmo_id key)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_POINT);
		CheckKeyPresent(_pointGizmos, k, EXC_PREAMBLE);

		_pointGizmos.erase(k);
	}

	std::vector<gizmo_instance_id> GizmosRenderer::InstancePointGizmo(gizmo_id key, const vector<point_gizmo_instance>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_POINT);
		CheckKeyPresent(_pointGizmos, k, EXC_PREAMBLE);

		vector<vec3>			positions	( instances.size() );
		vector<vec4>			colors		( instances.size() );
		
		auto iter = instances.begin();
		for(int i = 0; i < instances.size(); i++)
		{
			positions[i]	= iter->position;
			colors[i]		= iter->color;
			++iter;
		}

		_pointGizmos.at(k).SetInstanceData(positions, colors);

		// give back keys to the caller so that the individual
		// instances can be identified for future operations
		return CreateInstanceKeys(key, instances.size());
	}

	gizmo_id GizmosRenderer::CreateLineGizmo(const line_list_gizmo_descriptor& desc)
	{
		const auto longKey = EncodeGizmoKey(++_latestLineKey, KEY_MASK_LINE);
		const auto key = DecodeGizmoKey(longKey, KEY_MASK_LINE);

		CheckKeyUniqueness(_lineGizmos, key, EXC_PREAMBLE);

		_lineGizmos.insert(make_pair(key, LineListGizmo{ *_renderContext, desc }));

		gizmo_id r{};
		r._key = longKey;
		return r;
	}

	void GizmosRenderer::DestroyLineGizmo(gizmo_id key)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_LINE);
		CheckKeyPresent(_lineGizmos, k, EXC_PREAMBLE);

		_lineGizmos.erase(k);
	}

	std::vector<gizmo_instance_id> GizmosRenderer::InstanceLineGizmo(const gizmo_id key, const vector<line_list_gizmo_instance>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_LINE);
		CheckKeyPresent(_lineGizmos, k, EXC_PREAMBLE);

		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors(instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i]	= instances[i].transformation;
			colors[i]		= instances[i].color;
		}

		_lineGizmos.at(k).SetInstanceData(transforms, colors);

		// give back keys to the caller so that the individual
		// instances can be identified for future operations
		return CreateInstanceKeys(key, instances.size());
	}

	gizmo_id GizmosRenderer::CreateLineStripGizmo(const line_strip_gizmo_descriptor& desc)
	{
		const auto longKey = EncodeGizmoKey(++_latestLineStripKey, KEY_MASK_LINE_STRIP);
		const auto key = DecodeGizmoKey(longKey, KEY_MASK_LINE_STRIP);

		CheckKeyUniqueness(_lineStripGizmos, key, EXC_PREAMBLE);

		_lineStripGizmos.insert(make_pair(key, LineStripGizmo{ *_renderContext, desc }));

		gizmo_id r{};
		r._key = longKey;
		return r;
	}

	void GizmosRenderer::DestroyLineStripGizmo(gizmo_id key)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_LINE_STRIP);
		CheckKeyPresent(_lineStripGizmos, k, EXC_PREAMBLE);

		_lineStripGizmos.erase(k);
	}

	std::vector<gizmo_instance_id> GizmosRenderer::InstanceLineStripGizmo(const gizmo_id key, const vector<line_strip_gizmo_instance>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_LINE_STRIP);
		CheckKeyPresent(_lineStripGizmos, k, EXC_PREAMBLE);

		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors    (instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i] = instances[i].transformation;
			colors    [i] = instances[i].color;
		}

		_lineStripGizmos.at(k).SetInstanceData(transforms, colors);

		// give back keys to the caller so that the individual
		// instances can be identified for future operations
		return CreateInstanceKeys(key, instances.size());
	}

	gizmo_id GizmosRenderer::CreateMeshGizmo(const mesh_gizmo_descriptor& desc)
	{
		const auto longKey = EncodeGizmoKey(++_latestMeshKey, KEY_MASK_MESH);
		const auto key = DecodeGizmoKey(longKey, KEY_MASK_MESH);
		
		CheckKeyUniqueness(_meshGizmos, key, EXC_PREAMBLE);

		_meshGizmos.insert(make_pair(key, MeshGizmo{ *_renderContext, desc }));

		gizmo_id r{};
		r._key = longKey;
		return r;
	}

	void GizmosRenderer::DestroyMeshGizmo(gizmo_id key)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_MESH);
		CheckKeyPresent(_meshGizmos, k, EXC_PREAMBLE);

		_meshGizmos.erase(k);
	}

	std::vector<gizmo_instance_id> GizmosRenderer::InstanceMeshGizmo(const gizmo_id key, const vector<mesh_gizmo_instance>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_MESH);
		CheckKeyPresent(_meshGizmos, k, EXC_PREAMBLE);

		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors(instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i] = instances[i].transformation;
			colors	  [i] = instances[i].color;
		}

		_meshGizmos.at(k).SetInstanceData(transforms, colors);

		// give back keys to the caller so that the individual
		// instances can be identified for future operations
		return CreateInstanceKeys(key, instances.size());
	}

	float ComputeZoomInvarianceScale(
		const glm::vec3& boundingSphereCenter,
		float boundingSphereRadius,
		float screenRadius,
		const glm::mat4& viewMatrix,
		const glm::mat4& projectionMatrix)
	{
		const glm::mat4 proj = projectionMatrix;
		const glm::mat4 projInv = inverse(proj);

		float w = -(viewMatrix * glm::vec4(boundingSphereCenter, 1.0f)).z;
		glm::vec4 pt{ screenRadius, 0.0f, -1.0f, w };
		pt.x *= w;
		pt.y *= w;
		pt.z *= w;
		pt = projInv * pt;

		return pt.x / boundingSphereRadius;
	}

	vector<glm::mat4> ComputeZoomInvarianceTransformations(
		const float zoomInvariantScale,
		const glm::mat4& viewMatrix,
		const glm::mat4& projectionMatrix,
		const vector<glm::mat4>& transformations)
	{
		vector<glm::mat4> newTransformations{ transformations.size() };
		float r = 1.0f;
		for (int i = 0; i < transformations.size(); i++)
		{
			glm::mat4 tr = transformations[i];

			glm::vec4 o{0.0f, 0.0f, 0.0f, 1.0f };
			glm::vec4 rx = o + glm::vec4{ r, 0.0f, 0.0f, 0.0f };
			glm::vec4 ry = o + glm::vec4{ 0.0f, r, 0.0f, 0.0f };
			glm::vec4 rz = o + glm::vec4{ 0.0f, 0.0f, r, 0.0f };

			o = tr * o;
			rx = tr * rx;
			ry = tr * ry;
			rz = tr * rz;

			float maxR = glm::max(length(o - rx), glm::max(length(o - ry), length(o - rz)));

			float newScale = ComputeZoomInvarianceScale(glm::vec3(o), maxR, zoomInvariantScale, viewMatrix, projectionMatrix);

			newTransformations[i] = tr * glm::scale(glm::mat4(1.0f), glm::vec3(newScale));
		}

		return newTransformations;
	}

	bool GizmosRenderer::ShouldRenderGizmo(const Gizmo& gzm, const RenderPass& currentPass, const RenderLayer& currentLayer)
	{
		return gzm._layerMask & currentLayer._guid & currentPass._layersMask;
	}

	void GizmosRenderer::RenderPointGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	{
		_pointsShader.UseProgram();
		_pointsObjDataUbo.Bind(POINTS_OBJ_DATA_BINDING);

		for (auto& pGzm : _pointGizmos)
		{
			const bool hasTexture = pGzm.second._symbolAtlas.has_value();
			if (hasTexture)
			{
				pGzm.second._symbolAtlas->BindToTextureUnit(tex_unit_0);

				(pGzm.second._symbolAtlasLinearFilter
					? _linearSampler
					: _nearestSampler)
					.BindToTextureUnit(tex_unit_0);
			}
			points_obj_data_block const objData
			{
				.size = pGzm.second._pointSize,
				.snap = pGzm.second._snap,
				.has_texture = hasTexture
			};

			// TODO: compute and set data outside this func!!! (multiple passes)
			if (pGzm.second._isZoomInvariant)
			{
				vector<glm::mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						pGzm.second._zoomInvariantScale,
						viewMatrix, projectionMatrix, pGzm.second._transformList
					);
				pGzm.second._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 * sizeof(float), newTransf.data());
			}

			_pointsObjDataUbo.SetSubData(0, sizeof(points_obj_data_block), &objData);

			// bind static instance data SSBO (draw instanced)
			pGzm.second._ssboInstanceColor.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			pGzm.second._ssboInstanceTransform.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);

			pGzm.second._vao.Bind();

			// TODO: early exit if there's no layer to render for the current pass
			for(const auto& val : _renderLayers | views::values)
			{
				if(ShouldRenderGizmo(pGzm.second, currentPass, val))
				{
					_renderContext->SetDepthState		(val.depth_state);
					_renderContext->SetRasterizerState	(val.rasterizer_state);
					_renderContext->SetBlendState		(val.blend_state);

					_renderContext->DrawArrays(pmt_type_points, 0, pGzm.second._instanceCount);
				}
			}

			if (hasTexture)
			{
				OglTexture2D::UnBindToTextureUnit(tex_unit_0);
				OglSampler::UnBindToTextureUnit(tex_unit_0);
			}
		}
	}

	void GizmosRenderer::RenderLineGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	 {
		_linesShader.UseProgram();
		_linesObjDataUbo.Bind(LINES_OBJ_DATA_BINDING);

		for (auto& lGzm : _lineGizmos)
		{
			bool hasTexture = lGzm.second._patternTexture.has_value();
			if (hasTexture)
			{
				lGzm.second._patternTexture->BindToTextureUnit(tex_unit_0);

				_linearSampler.BindToTextureUnit(tex_unit_0);
			}
			lines_obj_data_block const objData
			{
				.size			= lGzm.second._lineSize,
				.has_texture    = hasTexture,
				.pattern_size	= lGzm.second._patternSize
			};

			// TODO - multiple passes waste computation
			if (lGzm.second._isZoomInvariant)
			{
				vector<glm::mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						lGzm.second._zoomInvariantScale,
						viewMatrix, projectionMatrix, lGzm.second._transformList
					);
				lGzm.second._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 * sizeof(float), newTransf.data());
			}

			_linesObjDataUbo.SetSubData(0, sizeof(lines_obj_data_block), &objData);
			
			// bind static instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceColor		.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceTransform	.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);

			lGzm.second._vao.Bind();

			// TODO: early exit if there's no layer to render for the current pass
			for (const auto& val : _renderLayers | views::values)
			{
				if (ShouldRenderGizmo(lGzm.second, currentPass, val))
				{
					_renderContext->SetDepthState		(val.depth_state);
					_renderContext->SetRasterizerState	(val.rasterizer_state);
					_renderContext->SetBlendState		(val.blend_state);

					_renderContext->DrawArraysInstanced(pmt_type_lines, 0, lGzm.second._vertexCount, lGzm.second._instanceCount);
				}
			}
		}
	 }

	enum class clipSegmentResult
	{
		no_clip_necessary,
		clip_success,
		cant_clip_both_in_front
	};

	clipSegmentResult ClipSegmentAgainstNear(
		const vec4& startClipSpace, const vec4& endClipSpace,
			  vec4& newStart	  ,		  vec4& newEnd,
		float near)
	{
		// a point on the near plane 
	    // has clip coord .z = -near
		float clipPlane = -near;
		float startToNear = clipPlane - startClipSpace.z;
		float endToNear   = clipPlane - endClipSpace.z;

		// early exit conditions:
		// * both points are inside the frustum    -> no clip necessary
		// * both points are behind the near plane -> can't clip
		if (startToNear > 0 && endToNear > 0) return clipSegmentResult::cant_clip_both_in_front;
		if (startToNear<= 0 && endToNear<= 0) return clipSegmentResult::no_clip_necessary;

		// slope of xy with respect to z
		vec3 mClipSpace = (endClipSpace - startClipSpace);
		mClipSpace.x /= mClipSpace.z;
		mClipSpace.y /= mClipSpace.z;

		newStart = startClipSpace;
		newEnd   = endClipSpace;

		// if one point is behind the near plane
		// we clip it to near
		if (endToNear > 0.0f)
		{
			newEnd.x = startClipSpace.x + mClipSpace.x * startToNear;
			newEnd.y = startClipSpace.y + mClipSpace.y * startToNear;

			newEnd.z =  clipPlane;
			newEnd.w = -clipPlane; // 1 for ortho
		}

		if (startToNear > 0.0f)
		{
			newStart.x = endClipSpace.x + mClipSpace.x * endToNear;
			newStart.y = endClipSpace.y + mClipSpace.y * endToNear;

			newStart.z =  clipPlane;
			newStart.w = -clipPlane; // 1 for ortho
		}
		
		return clipSegmentResult::clip_success;
	}

	vector<float> PefixSumLineStrip(
		const vector<glm::vec3>& verts,
		const vector<glm::mat4>& instances, 
		const glm::mat4& viewMatrix, 
		const glm::mat4& projectionMatrix,
		const unsigned int screenWidth, 
		const unsigned int screenHeight)
	{
		vec4 prevPtCS;
		const unsigned int vertCount = verts.size();
		vector<float> sum(vertCount * instances.size());

		float near = projectionMatrix[3][2] / (projectionMatrix[2][2] - 1.0f);

		for (int i = 0; i < instances.size(); i++) // for each instance (mat4 transformation)
		{
			float dstAccum = 0;
			bool  restart = true;
			const mat4 mvp = projectionMatrix * viewMatrix * instances[i];

			for (int v = 0; v < verts.size(); v++) // for each vertex in the strip
			{
				const vec4 vert = vec4(verts[v], 1.0f);
				const vec4 ptCS = mvp * vert;

				if (!restart)
				{
					vec4 ptCSc{}, prevPtCSc{};

					switch (ClipSegmentAgainstNear(prevPtCS, ptCS, prevPtCSc, ptCSc, near))
					{
					case(clipSegmentResult::clip_success):
					{
						vec2 d =
							(vec2{ ptCSc.x,	   ptCSc.y } / ptCSc.w) -
							(vec2{ prevPtCSc.x, prevPtCSc.y } / prevPtCSc.w);
						d = d * 0.5f;
						d *= vec2{ screenWidth, screenHeight };

						dstAccum += length(d);
						break;
					}
					case(clipSegmentResult::no_clip_necessary):
					{
						vec2 d =
							(vec2{ ptCS.x,	   ptCS.y } / ptCS.w) -
							(vec2{ prevPtCS.x, prevPtCS.y } / prevPtCS.w);
						d = d * 0.5f;
						d *= vec2{ screenWidth, screenHeight };

						dstAccum += length(d);
						break;
					}
					case(clipSegmentResult::cant_clip_both_in_front):
						dstAccum += 0.0f;
						break;
					}

					sum[i*vertCount + v] = dstAccum;
				}
				else
					sum[i * vertCount + v] = 0.0f; // first iteration

				prevPtCS = ptCS;
				restart = false;
			}
		}

		return sum;
	}
	
	void GizmosRenderer::RenderLineStripGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	{
		_lineStripShader.UseProgram();
		_linesObjDataUbo.Bind(LINE_STRIP_OBJ_DATA_BINDING);

		/////////////////////////////////////////////////////
		// Line Strip Preprocessing:					   //
		// computing screen length for each line segment   //
		// to draw stippled lines.						   //
		/////////////////////////////////////////////////////
		
		// total number of vertices considering
		// all the instances for each linestrip gizmo.
		unsigned int totalVerts = 0;
		for (const auto& lGzm : _lineStripGizmos) 
			totalVerts += lGzm.second._instanceCount * lGzm.second._vertices.size();

		vector<float> ssboData(totalVerts);
		unsigned int cnt = 0;

		// TODO: multiple passes waste computation 
		for (auto& pair : _lineStripGizmos)
		{
			LineStripGizmo& lGzm = pair.second;
			vector<glm::mat4>& realTransformList = lGzm._transformList;

			if (lGzm._isZoomInvariant)
			{
				vector<glm::mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						lGzm._zoomInvariantScale,
						viewMatrix, projectionMatrix, lGzm._transformList
					);

				realTransformList = newTransf;

				lGzm._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 * sizeof(float), newTransf.data());
			}

			// compute screen length sum for the line strip gizmo
			auto part = PefixSumLineStrip(
				lGzm._vertices,
				realTransformList,
				viewMatrix, projectionMatrix,
				_windowWidth, _windowHeight);

			std::copy(part.begin(), part.end(), ssboData.begin()+cnt);
			cnt += part.size();
		}

		_lenghSumSsbo.Resize(totalVerts * sizeof(float));
		_lenghSumSsbo.OglBuffer().SetSubData(0, totalVerts*sizeof(float), ssboData.data());

		unsigned int vertDrawn = 0;
		for (auto& lGzm : _lineStripGizmos)
		{
			if (lGzm.second._instanceCount == 0)continue;

			const unsigned int vertCount	 = lGzm.second._vertices.size();
			const unsigned int instanceCount = lGzm.second._instanceCount;

			bool hasTexture = lGzm.second._patternTexture.has_value();
			if (hasTexture)
			{
				lGzm.second._patternTexture->BindToTextureUnit(tex_unit_0);
			
				_linearSampler.BindToTextureUnit(tex_unit_0);
			}
			line_strip_obj_data_block const objData
			{
				.size			= lGzm.second._lineSize,
				.has_texture	= hasTexture,
				.pattern_size	= lGzm.second._patternSize,
				.vert_count     = vertCount
			};

			// Bind UBO
			_lineStripObjDataUbo.SetSubData(0, sizeof(line_strip_obj_data_block), &objData);
			// Bind SSBO for screen length sum
			_lenghSumSsbo.OglBuffer().BindRange(LINE_STRIP_SSBO_BINDING_SCREEN_LENGTH, vertDrawn * sizeof(float), vertCount * instanceCount * sizeof(float));
			// bind static instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceColor.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceTransform.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// Bind VAO
			lGzm.second._vao.Bind();

			// TODO: early exit if there's no layer to render for the current pass
			for (const auto& val : _renderLayers | views::values)
			{
				if (ShouldRenderGizmo(lGzm.second, currentPass, val))
				{
					_renderContext->SetDepthState		(val.depth_state);
					_renderContext->SetRasterizerState	(val.rasterizer_state);
					_renderContext->SetBlendState		(val.blend_state);

					_renderContext->DrawArraysInstanced(pmt_type_line_strip_adjacency, 0, vertCount, instanceCount);
				}
			}

			vertDrawn += vertCount * instanceCount;
		}
	}

	void GizmosRenderer::RenderMeshGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	{
		_meshShader.UseProgram();
		_meshObjDataUbo.Bind(MESH_OBJ_DATA_BINDING);

		for (auto& mGzm : _meshGizmos)
		{
			mesh_obj_data_block const objData
			{
				.has_texture = false,
			};

			_meshObjDataUbo.SetSubData(0, sizeof(mesh_obj_data_block), &objData);
			
			// TODO: multiple passes waste computation
			if(mGzm.second._isZoomInvariant)
			{
				vector<glm::mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						mGzm.second._zoomInvariantScale,
						viewMatrix, projectionMatrix, mGzm.second._transformList
					); 
				mGzm.second._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 * sizeof(float), newTransf.data());
			}

			// bind VAO
			mGzm.second._vao.Bind();
			// bind static instance data SSBO (draw instanced)
			mGzm.second._ssboInstanceColorAndNrmMat	.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			mGzm.second._ssboInstanceTransform		.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind EBO
			mGzm.second._ebo.OglBuffer().Bind();

			// TODO: early exit if there's no layer to render for the current pass
			for (const auto& val : _renderLayers | views::values)
			{
				if (ShouldRenderGizmo(mGzm.second, currentPass, val))
				{
					_renderContext->SetDepthState		(val.depth_state);
					_renderContext->SetRasterizerState	(val.rasterizer_state);
					_renderContext->SetBlendState		(val.blend_state);

					_renderContext->DrawElementsInstanced(pmt_type_triangles, mGzm.second._trisCount, idx_typ_unsigned_int, nullptr, mGzm.second._instanceCount);
				}
			}
		}
	}

	const OglFramebuffer<OglTexture2DMultisample>& GizmosRenderer::Render(
		const glm::mat4& viewMatrix, 
		const glm::mat4& projectionMatrix, 
		const glm::vec2& nearFar)
	{
		// todo isCurrent()
		// rc.MakeCurrent();

		_renderContext->SetViewport(0, 0, _windowWidth, _windowHeight);

		// Set the default states so that clear operations
		// are not disabled by what happend previously
		_renderContext->SetDepthState		(DEFAULT_DEPTH_STATE);
		_renderContext->SetBlendState		(DEFAULT_BLEND_STATE);
		_renderContext->SetRasterizerState	(DEFAULT_RASTERIZER_STATE);

		// todo: how to show results? maybe return a texture or let the user access the drawn surface.
		_mainFramebuffer.Bind(fbo_read_draw);

		_renderContext->ClearColor(0.2f, 0.2f, 0.2f);
		_renderContext->ClearDepthStencil();

		frame_data_block const frameData
		{
			.view_matrix		= viewMatrix,
			.projection_matrix	= projectionMatrix,
			.view_position		= glm::inverse(viewMatrix) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
			.view_direction		= -viewMatrix[2],
			.viewport_size		=        glm::vec2(_windowWidth, _windowHeight),
			.viewport_size_inv	= 1.0f / glm::vec2(_windowWidth, _windowHeight),
			.near_far			= nearFar //todo 
		};

		_frameDataUbo.SetSubData(0, sizeof(frame_data_block), &frameData);
		_frameDataUbo.Bind(FRAME_DATA_BINDING);

		for (const RenderPass& pass : _renderPasses)
		{
			RenderMeshGizmos		(viewMatrix, projectionMatrix, pass);
			RenderLineGizmos		(viewMatrix, projectionMatrix, pass);
			RenderLineStripGizmos	(viewMatrix, projectionMatrix, pass);
			RenderPointGizmos		(viewMatrix, projectionMatrix, pass);
		}

		OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

		return _mainFramebuffer;
	}

}
