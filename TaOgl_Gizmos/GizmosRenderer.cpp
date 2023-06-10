#include "GizmosRenderer.h"
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
			_vao{rc.CreateVertexAttribArray()},
			_pointSize(desc.point_half_size),
			_snap(desc.snap_to_pixel)
	 {
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

			int const symbolCount = desc.symbol_atlas_descriptor->mapping.size();
			_symbolAtlasTexCoordLUT.emplace(vector<vec4>(symbolCount));
			
			for (int i = 0; i < symbolCount; i++)
				(*_symbolAtlasTexCoordLUT)[i] = vec4(
					desc.symbol_atlas_descriptor->mapping[i].uv_min,
					desc.symbol_atlas_descriptor->mapping[i].uv_max);
		}
	 }

	 void PointGizmo::SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors, const optional<vector<unsigned int>>& symbolIndices)
	 {
		 constexpr int vertexSize = sizeof(float) * 11; // position(3) + color(4) + texCoord(4)
		 const unsigned int posLen = positions.size();

		 if (posLen != colors.size() || (symbolIndices && symbolIndices->size()!= colors.size()))
			 throw exception{ "Instance data must match in count." };
		 
		 _instanceCount = posLen;
		 _colorList = colors;

		 vector<float> data(posLen * 11 , 0.0f);
		 for(int i=0, j=0;i<posLen;i++)
		 {
			 data[j++] = positions[i].x; data[j++] = positions[i].y; data[j++] = positions[i].z;
			 data[j++] =    colors[i].x; data[j++] =    colors[i].y; data[j++] =    colors[i].z; data[j++] = colors[i].w;

			 if (symbolIndices&&_symbolAtlasTexCoordLUT)
			 {
				 if (_symbolAtlasTexCoordLUT->size() <= (*symbolIndices)[i]) throw exception("Invalid symbol index.");

				 const vec4 uv = _symbolAtlasTexCoordLUT->at((*symbolIndices)[i]);
				 data[j++] = uv.x; data[j++] = uv.y; data[j++] = uv.z; data[j++] = uv.w;
			 }
			 else j += 4;
		 }

		 _vbo.Resize(_instanceCount * vertexSize);
		 _vbo.OglBuffer().SetSubData(0, _instanceCount * vertexSize, data.data());
	 }

	 LineListGizmo::LineListGizmo(RenderContext& rc, const line_list_gizmo_descriptor& desc) :
	     _vertices   (desc.vertices->size()),
		 _vbo { rc, 0, ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _ssbo{ rc, 0, ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },
		 _vao { rc.CreateVertexAttribArray() },
		 _lineSize		{ desc.line_size    },
		 _patternSize	{ 0 }
	 {
		 _vertexCount = desc.vertices->size();
		 for (int i = 0; i < _vertexCount; i++) _vertices[i] = (*desc.vertices)[i].Position;

		 // vbo layout for points:
		 // pos(vec3)-color(vec4)
		 //--------------------------------------------
		 const unsigned int vertexComponents = 7;
		 const unsigned int vertexSize = vertexComponents * sizeof(float);
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0));
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);

		 const unsigned int vertexCount = desc.vertices->size();
		 vector<float> vertexData(vertexCount* vertexComponents);
		 for(int i=0; i<vertexCount;i++)
		 {
			 for (int j = 0; j < 3; j++) vertexData[i * vertexComponents +	   j] = (*desc.vertices)[i].Position[j];
			 for (int j = 0; j < 4; j++) vertexData[i * vertexComponents + 3 + j] = (*desc.vertices)[i].Color   [j];
		 }

		_vbo.Resize(vertexSize* vertexCount);
		_vbo.OglBuffer().SetSubData(0, vertexData.size()*sizeof(float), vertexData.data());

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
		 _colorList = colors;

		 
		 BufferDataPacker pack{};
		 auto data = pack
			 .AddDataArray(transformations)
			 .AddDataArray(colors)
			 .InterleavedBuffer();

		 _ssbo.Resize(data.size());
		 _ssbo.OglBuffer().SetSubData(0, data.size(), data.data());
	 }

	 LineStripGizmo::LineStripGizmo(RenderContext& rc, const line_strip_gizmo_descriptor& desc) :
		 _vertices		 { *desc.vertices }, 
		 _vboVertices	 { rc, 0, buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceData{ rc, 0, buf_usg_static_draw, ResizeBuffer },
		 _vao			 { rc.CreateVertexAttribArray() },
		 _lineSize		 { desc.line_size },
		 _patternSize	 { 0 }
	 {
		 // vbo layout for linestrip:
		 // vbo 0: pos(vec3)
		 //--------------------------------------------
		 constexpr int vertexSize   = 3 * sizeof(float);
		 _vao.SetVertexAttribPointer(_vboVertices.OglBuffer(), 0, 4, vao_typ_float, false, vertexSize  , reinterpret_cast<void*>(0 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);

		 // allocate memory and fill vbo
		 // --------------------------------------------
		 vec3 adjStart, adjEnd;
		 if(desc.isLoop)
		 {
			 adjStart	= *(_vertices.  end() -2);
			 adjEnd		= *(_vertices.begin() +1);
		 }
		 else
		 {
			 adjStart	= 2.0f * (* _vertices.begin())  - *(_vertices.begin() + 1);
			 adjEnd		= 2.0f * (*(_vertices.end()-1)) - *(_vertices  .end() - 2);
		 }
		 // adjacency data
		 _vertices.insert(_vertices.begin(), adjStart);
		 _vertices.push_back(adjEnd);

		 _vboVertices.Resize(_vertices.size()* vertexSize);
		 _vboVertices.OglBuffer().SetSubData(0, _vertices.size()* vertexSize, _vertices.data());

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
		 _colorList     = colors;

		 BufferDataPacker pack{};
		 auto data = pack
			 .AddDataArray(transforms)
			 .AddDataArray(colors)
			 .InterleavedBuffer();
		 _ssboInstanceData.Resize(data.size());
		 _ssboInstanceData.OglBuffer().SetSubData(0, data.size(), data.data());
	 }

	 // Always centered at the origin in model space!!!
	 void ComputeBoundingSphere(const vector<glm::vec3>& points, glm::vec3& sphereCenter, float& sphereRadius)
	 {
		 sphereCenter = glm::vec3(0.0f); // wow...

		// find the most distant vertex 
		// from the center (origin).
		sphereRadius = 0.0f;
		for(const auto& p:points ) sphereRadius = glm::max(glm::length(p), sphereRadius);
	 }

	 MeshGizmo::MeshGizmo(RenderContext& rc, const mesh_gizmo_descriptor& desc) :
		 _vertices			{ desc.vertices->size() },
		 _triangles			{ *desc.triangles },
		 _vboVertices		{ rc, 0,  buf_usg_static_draw, ResizeBuffer},
		 _vao				{ rc.CreateVertexAttribArray() },
		 _ebo				{ rc, 0,  buf_usg_static_draw, ResizeBuffer },
		 _ssboInstanceData	{ rc, 0,  desc.zoom_invariant ? buf_usg_dynamic_draw : buf_usg_static_draw, ResizeBuffer },
	     _isZoomInvariant	{desc.zoom_invariant},
		_zoomInvariantScale	{desc.zoom_invariant_scale}
	    
	 {

		 // store vertex positions
		 for (int i=0;i<desc.vertices->size(); i++)
		 {
			 _vertices[i] = (*desc.vertices)[i].Position;
		 }
		 _vertexCount = desc.vertices->size();
		 _trisCount   = desc.triangles->size();

		 // Compute a bounding sphere centered at the origin 
		 // in model space to achieve zoom invariance.
		 if (_isZoomInvariant)
			 ComputeBoundingSphere(_vertices, _bSphereCenter, _bSphereRadius);

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

		 // from vector<MeshGizmoVertex> to vector<float)
		 vector<float> vertexData(vertexComponents * desc.vertices->size());
		 for(int i=0;i<desc.vertices->size(); i++)
		 {
			 // Position
			 vertexData[i * vertexComponents + 0] = (*desc.vertices)[i].Position.x;
			 vertexData[i * vertexComponents + 1] = (*desc.vertices)[i].Position.y;
			 vertexData[i * vertexComponents + 2] = (*desc.vertices)[i].Position.z;
			 // Normal		
			 vertexData[i * vertexComponents + 3] = (*desc.vertices)[i].Normal.x;
			 vertexData[i * vertexComponents + 4] = (*desc.vertices)[i].Normal.y;
			 vertexData[i * vertexComponents + 5] = (*desc.vertices)[i].Normal.z;
			 // Color		
			 vertexData[i * vertexComponents + 6] = (*desc.vertices)[i].Color.x;
			 vertexData[i * vertexComponents + 7] = (*desc.vertices)[i].Color.y;
			 vertexData[i * vertexComponents + 8] = (*desc.vertices)[i].Color.z;
			 vertexData[i * vertexComponents + 9] = (*desc.vertices)[i].Color.w;
			 // TexCoord	
			 vertexData[i * vertexComponents +10] = (*desc.vertices)[i].TexCoord.x;
			 vertexData[i * vertexComponents +11] = (*desc.vertices)[i].TexCoord.y;

		 }

		 _vboVertices.Resize(_vertices.size()* vertexSize);
		 _vboVertices.OglBuffer().SetSubData(0, _vertices.size()* vertexSize, vertexData.data());


		// allocate memory and fill EBO
		// --------------------------------------------
		 _ebo.Resize(_triangles.size() * sizeof(int));
		 _ebo.OglBuffer().SetSubData(0, _triangles.size() * sizeof(int) , _triangles.data());

		// Bind SSBO
		 _ssboInstanceData.OglBuffer().Bind(INSTANCE_DATA_SSBO_BINDING);
	 }


	 void MeshGizmo::SetInstanceData(const vector<mat4>& transforms, const vector<vec4>& colors)
	 {
		 if (transforms.size() != colors.size())throw exception{ "Instance data must match in count." };

		 _instanceCount = transforms.size();
		 _transformList = transforms;
		 _colorList     = colors;

		 // Instance data:
		 // mat4 transform - mat4 normal matrix - vec4 color
		 const unsigned int dataCmpCnt = (16 + 16 + 4);
		 const unsigned int instanceDataSize = dataCmpCnt * sizeof(float);
		 
		 // fill SSBO for instance data (transformation + color)
		 vector<glm::mat4> normalMatrices{ _instanceCount };
		for( int i=0;i>_transformList.size();i++) normalMatrices[i] = inverse(transpose(_transformList[i]));

		BufferDataPacker pack{};
		auto data = pack
			.AddDataArray(transforms)
			.AddDataArray(normalMatrices)
			.AddDataArray(colors)
			.InterleavedBuffer();

		 _ssboInstanceData.Resize(data.size());
		 _ssboInstanceData.OglBuffer().SetSubData(0, data.size(), data.data());
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
		 _lineGizmos		 {}
	 {
		 // main fbo
		 // ----------------------------
		 _colorTex.TexImage(8, tex_int_for_rgba , _windowWidth, _windowHeight, false);
		 _depthTex.TexImage(8, tex_int_for_depth, _windowWidth, _windowHeight, false);

		 _mainFramebuffer.AttachTexture(fbo_attachment_color0, _colorTex, 0);
		 _mainFramebuffer.AttachTexture(fbo_attachment_depth,  _depthTex, 0);

		 // shaders ubo bindings
		 // ----------------------------
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
		 // ----------------------------
		 _pointsObjDataUbo	  .SetData(sizeof(points_obj_data_block)	 , nullptr, buf_usg_static_draw);
		 _linesObjDataUbo	  .SetData(sizeof(lines_obj_data_block)		 , nullptr, buf_usg_static_draw);
		 _lineStripObjDataUbo .SetData(sizeof(line_strip_obj_data_block), nullptr, buf_usg_static_draw);
		 _meshObjDataUbo      .SetData(sizeof(mesh_obj_data_block)      , nullptr, buf_usg_static_draw);
		 _frameDataUbo		  .SetData(sizeof(frame_data_block)			 , nullptr, buf_usg_static_draw);

		 _pointsObjDataUbo	  .Bind(POINTS_OBJ_DATA_BINDING);
		 _linesObjDataUbo	  .Bind(LINES_OBJ_DATA_BINDING);
		 _lineStripObjDataUbo .Bind(LINE_STRIP_OBJ_DATA_BINDING);
		 _meshObjDataUbo      .Bind(MESH_OBJ_DATA_BINDING);
		 _frameDataUbo		  .Bind(FRAME_DATA_BINDING);

		// samplers
		// ----------------------------
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

	void GizmosRenderer::CreatePointGizmo(unsigned int pointGizmoIndex, const point_gizmo_descriptor& desc)
	 {
		CheckKeyUniqueness(_pointGizmos, pointGizmoIndex, EXC_PREAMBLE);

		_pointGizmos.insert(make_pair(pointGizmoIndex, PointGizmo{ *_renderContext, desc }));
	 }

	void GizmosRenderer::DestroyPointGizmo(unsigned int pointGizmoIndex)
	{
		CheckKeyPresent(_pointGizmos, pointGizmoIndex, EXC_PREAMBLE);

		_pointGizmos.erase(pointGizmoIndex);
	}

	void GizmosRenderer::InstancePointGizmo(unsigned pointGizmoIndex, const vector<point_gizmo_instance>& instances)
	{
		CheckKeyPresent(_pointGizmos, pointGizmoIndex, EXC_PREAMBLE);

		vector<vec3>			positions	( instances.size() );
		vector<vec4>			colors		( instances.size() );
		vector<unsigned int>	indices		( instances.size() );

		auto iter = instances.begin();
		for(int i = 0; i < instances.size(); i++)
		{
			positions[i]	= iter->position;
			colors[i]		= iter->color;
			indices[i]		= iter->symbol_index;
			++iter;
		}

		_pointGizmos.at(pointGizmoIndex).SetInstanceData(positions, colors, indices);
	}

	void GizmosRenderer::CreateLineGizmo(unsigned int lineGizmoIndex, const line_list_gizmo_descriptor& desc)
	{
		CheckKeyUniqueness(_lineGizmos, lineGizmoIndex, EXC_PREAMBLE);

		_lineGizmos.insert(make_pair(lineGizmoIndex, LineListGizmo{ *_renderContext, desc }));
	}

	void GizmosRenderer::DestroyLineGizmo(unsigned int lineGizmoIndex)
	{
		CheckKeyPresent(_lineGizmos, lineGizmoIndex, EXC_PREAMBLE);

		_lineGizmos.erase(lineGizmoIndex);
	}

	void GizmosRenderer::InstanceLineGizmo(unsigned lineGizmoIndex, const vector<line_list_gizmo_instance>& instances)
	{
		CheckKeyPresent(_lineGizmos, lineGizmoIndex, EXC_PREAMBLE);

		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors(instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i]	= instances[i].transformation;
			colors[i]		= instances[i].color;
		}

		_lineGizmos.at(lineGizmoIndex).SetInstanceData(transforms, colors);
	}

	void GizmosRenderer::CreateLineStripGizmo(unsigned int lineStripGizmoIndex, const line_strip_gizmo_descriptor& desc)
	{
		CheckKeyUniqueness(_lineStripGizmos, lineStripGizmoIndex, EXC_PREAMBLE);

		_lineStripGizmos.insert(make_pair(lineStripGizmoIndex, LineStripGizmo{ *_renderContext, desc }));
	}

	void GizmosRenderer::DestroyLineStripGizmo(unsigned int lineStripGizmoIndex)
	{
		CheckKeyPresent(_lineStripGizmos, lineStripGizmoIndex, EXC_PREAMBLE);

		_lineStripGizmos.erase(lineStripGizmoIndex);
	}

	void GizmosRenderer::InstanceLineStripGizmo(unsigned lineStripGizmoIndex, const vector<line_strip_gizmo_instance>& instances)
	{
		CheckKeyPresent(_lineStripGizmos, lineStripGizmoIndex, EXC_PREAMBLE);

		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors    (instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i] = instances[i].transformation;
			colors    [i] = instances[i].color;
		}

		_lineStripGizmos.at(lineStripGizmoIndex).SetInstanceData(transforms, colors);
	}

	void GizmosRenderer::CreateMeshGizmo(unsigned int meshGizmoIndex, const mesh_gizmo_descriptor& desc)
	{
		CheckKeyUniqueness(_meshGizmos, meshGizmoIndex, EXC_PREAMBLE);

		_meshGizmos.insert(make_pair(meshGizmoIndex, MeshGizmo{ *_renderContext, desc }));
	}

	void GizmosRenderer::DestroyMeshGizmo(unsigned int meshGizmoIndex)
	{
		CheckKeyPresent(_meshGizmos, meshGizmoIndex, EXC_PREAMBLE);

		_meshGizmos.erase(meshGizmoIndex);
	}

	void GizmosRenderer::InstanceMeshGizmo(unsigned meshGizmoIndex, const vector<mesh_gizmo_instance>& instances)
	{
		CheckKeyPresent(_meshGizmos, meshGizmoIndex, EXC_PREAMBLE);

		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors(instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i] = instances[i].transformation;
			colors	  [i] = instances[i].color;
		}

		_meshGizmos.at(meshGizmoIndex).SetInstanceData(transforms, colors);
	}

	void GizmosRenderer::RenderPointGizmos()
	 {
		_renderContext->SetDepthState(ogl_depth_state
		{
			.depth_test_enable = true,
			.depth_func = depth_func_less,
			.depth_range_near = 0.0,
			.depth_range_far = 1.0
		});
		_renderContext->SetRasterizerState(ogl_rasterizer_state
		{
			.multisample_enable = true,
			.alpha_to_coverage_enable = true
		});
		_renderContext->SetBlendState(no_blend);

		_pointsShader.UseProgram();

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

			_pointsObjDataUbo.SetSubData(0, sizeof(points_obj_data_block), &objData);

			pGzm.second._vao.Bind();

			_renderContext->DrawArrays(pmt_type_points, 0, pGzm.second._instanceCount);

			if (hasTexture)
			{
				OglTexture2D::UnBindToTextureUnit(tex_unit_0);
				OglSampler  ::UnBindToTextureUnit(tex_unit_0);
			}
		}
	 }

	void GizmosRenderer::RenderLineGizmos()
	 {
		_renderContext->SetDepthState(ogl_depth_state
		{
			.depth_test_enable	= true,
			.depth_func			= depth_func_less,
			.depth_range_near	= 0.0,
			.depth_range_far	= 1.0
		});
		_renderContext->SetRasterizerState(ogl_rasterizer_state
		{
			.multisample_enable			= true,
			.alpha_to_coverage_enable	= true
		});

		_renderContext->SetBlendState(no_blend);

		_linesShader.UseProgram();

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

			_linesObjDataUbo.SetSubData(0, sizeof(lines_obj_data_block), &objData);
			lGzm.second._ssbo.OglBuffer().Bind(INSTANCE_DATA_SSBO_BINDING);
			lGzm.second._vao.Bind();

			_renderContext->DrawArraysInstanced(pmt_type_lines, 0, lGzm.second._vertexCount, lGzm.second._instanceCount);
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
	
	void GizmosRenderer::RenderLineStripGizmos(const glm::mat4& viewMatrix, const glm::mat4& projMatrix)
	{
		_renderContext->SetDepthState(ogl_depth_state
		{
			.depth_test_enable = true,
			.depth_func = depth_func_less,
			.depth_range_near = 0.0,
			.depth_range_far = 1.0
		});

		_renderContext->SetRasterizerState(ogl_rasterizer_state
		{
			.multisample_enable = true,
			.alpha_to_coverage_enable = true
		});

		_renderContext->SetBlendState(no_blend);

		_lineStripShader.UseProgram();

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

		for (unsigned int i = 0; i < _lineStripGizmos.size(); i++)
		{
			// compute screen length sum for the line strip gizmo
			auto part = PefixSumLineStrip(
				_lineStripGizmos.at(i)._vertices,
				_lineStripGizmos.at(i)._transformList,
				viewMatrix, projMatrix,
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
			// Bind SSBO for Instance data (transform + color)
			lGzm.second._ssboInstanceData.OglBuffer().Bind(INSTANCE_DATA_SSBO_BINDING);
			// Bind VAO
			lGzm.second._vao.Bind();

			_renderContext->DrawArraysInstanced(pmt_type_line_strip_adjacency, 0, vertCount, instanceCount);

			vertDrawn += vertCount * instanceCount;
		}
	}

	float ComputeZoomInvarianceScale(
		const glm::vec3& boundingSphereCenter, 
		float boundingSphereRadius, 
		float screenRadius, 
		const glm::mat4& viewMatrix, 
		const glm::mat4& projectionMatrix)
	{
		const glm::mat4 proj	= projectionMatrix;
		const glm::mat4 projInv = inverse(proj);

		float w=  -(viewMatrix * glm::vec4(boundingSphereCenter, 1.0f)).z;
		glm::vec4 pt{ screenRadius, 0.0f, -1.0f, w};
		pt.x *= w;
		pt.y *= w;
		pt.z *= w;
		pt = projInv * pt;

		return pt.x / boundingSphereRadius;
	}

	vector<glm::mat4> ComputeZoomInvarianceTransformations(
		const glm::vec3& boundingSphereCenter,
		float boundingSphereRadius,
		float screenRadius,
		const glm::mat4& viewMatrix,
		const glm::mat4& projectionMatrix,
		const vector<glm::mat4>& transformations)
	{
		vector<glm::mat4> newTransformations{ transformations.size() };
		float r = boundingSphereRadius;
		for(int i=0;i<transformations.size();i++)
		{
			glm::mat4 tr = transformations[i];

			glm::vec4 o { boundingSphereCenter, 1.0f };
			glm::vec4 rx = o + glm::vec4{ r, 0.0f, 0.0f, 0.0f };
			glm::vec4 ry = o + glm::vec4{ 0.0f, r, 0.0f, 0.0f };
			glm::vec4 rz = o + glm::vec4{ 0.0f, 0.0f, r, 0.0f };

			o  = tr * o;
			rx = tr * rx;
			ry = tr * ry;
			rz = tr * rz;

			float maxR =glm::max(length(o - rx), glm::max(length(o - ry), length(o - rz)));

			float newScale = ComputeZoomInvarianceScale(glm::vec3(o), maxR, screenRadius, viewMatrix, projectionMatrix);

			newTransformations[i] = tr * glm::scale(glm::mat4(1.0f), glm::vec3(newScale));
		}

		return newTransformations;
	}

	void GizmosRenderer::RenderMeshGizmos(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
	{
		_renderContext->SetDepthState(ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_func = depth_func_less,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			});
		_renderContext->SetRasterizerState(ogl_rasterizer_state
			{
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			});

		_renderContext->SetBlendState(no_blend);

		_meshShader.UseProgram();

		for (auto& mGzm : _meshGizmos)
		{
			mesh_obj_data_block const objData
			{
				.has_texture = false,
			};

			_meshObjDataUbo.SetSubData(0, sizeof(mesh_obj_data_block), &objData);

			if(mGzm.second._isZoomInvariant)
			{
				vector<glm::mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						mGzm.second._bSphereCenter,
						mGzm.second._bSphereRadius,
						mGzm.second._zoomInvariantScale,
						viewMatrix, projectionMatrix, mGzm.second._transformList
					); 

				// fill SSBO for instance data (transformation + color)
				// TODO: since with _zoomInvariant we need to transfer transform data
				// TODO: to the GPU each frame, we should separate instance color data (static)
				// TODO: from instance transform data (dynamic).
				vector<float> data(36 * mGzm.second._instanceCount);
				for (int i = 0; i < mGzm.second._instanceCount; i++)
				{
					glm::mat3x3 nrmMat = newTransf[i];
					nrmMat = inverse(transpose(nrmMat));
					glm::mat4 nrmMatPad{ nrmMat };

					for (int j = 0; j < 16; j++) data[i * 36 +		j]	= *(value_ptr(newTransf[i]) + j);					//todo
					for (int j = 0; j < 16; j++) data[i * 36 + 16 + j]	= *(value_ptr(nrmMatPad) + j);						//todo
					for (int j = 0; j < 4 ; j++) data[i * 36 + 32 + j]	= *(value_ptr(mGzm.second._colorList[i]) + j);		//todo
				}

				mGzm.second._ssboInstanceData.OglBuffer().SetSubData(0, data.size() * sizeof(float), data.data());
			}

			// bind VAO
			mGzm.second._vao.Bind();
			// bind SSBO (draw instanced)
			mGzm.second._ssboInstanceData.OglBuffer().Bind(INSTANCE_DATA_SSBO_BINDING);
			// bind EBO
			mGzm.second._ebo.OglBuffer().Bind();

			_renderContext->DrawElementsInstanced(pmt_type_triangles, mGzm.second._trisCount, idx_typ_unsigned_int, nullptr, mGzm.second._instanceCount);
		}
	}

	const OglFramebuffer<OglTexture2DMultisample>& GizmosRenderer::Render(glm::mat4 viewMatrix, glm::mat4 projectionMatrix, glm::vec2 nearFar)
	{
		// todo isCurrent()
		// rc.MakeCurrent();

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

		RenderMeshGizmos(viewMatrix, projectionMatrix);

		RenderPointGizmos();

		RenderLineGizmos();

		RenderLineStripGizmos(viewMatrix, projectionMatrix);

		OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

		return _mainFramebuffer;
	}

}
