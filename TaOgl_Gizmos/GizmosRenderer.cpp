#include "GizmosRenderer.h"
#include "GizmosShaderLib.h"
#include "GizmosUtils.h"
#include <glm/gtc/type_ptr.hpp>

namespace tao_gizmos
{
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

	unsigned int ResizeVbo(unsigned int currVertCapacity, unsigned int newVertCount)
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
			_vbo{rc.CreateVertexBuffer()},
			_vao{rc.CreateVertexAttribArray()},
			_pointSize(desc.point_half_size),
			_snap(desc.snap_to_pixel)
	 {
		 // vbo layout for points:
		 // pos(vec3)-color(vec4)-tex(vec4) interleaved
		 //--------------------------------------------
		 int vertexSize = 11 * sizeof(float);
		 _vao.SetVertexAttribPointer(_vbo, 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0));
		 _vao.SetVertexAttribPointer(_vbo, 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3*sizeof(float)));
		 _vao.SetVertexAttribPointer(_vbo, 2, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(7*sizeof(float)));
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

		 const unsigned int vboVertCount = ResizeVbo(_vboCapacity, _instanceCount);

		 if (vboVertCount > _vboCapacity) // init buffer if necessary
		 {
			 _vbo.SetData(vboVertCount * vertexSize, nullptr, buf_usg_static_draw);
			 _vboCapacity = vboVertCount;
		 }

		 _vbo.SetSubData(0, _instanceCount * vertexSize, data.data());
	 }

	 LineGizmo::LineGizmo(RenderContext& rc, const line_gizmo_descriptor& desc) :
		 _vbo{ rc.CreateVertexBuffer()      },
		 _vao{ rc.CreateVertexAttribArray() },
		 _lineSize		{ desc.line_size    },
		 _patternSize	{ 0 }
	 {
		 // vbo layout for points:
		 // pos(vec3)-color(vec4)
		 //--------------------------------------------
		 int vertexSize = 7 * sizeof(float);
		 _vao.SetVertexAttribPointer(_vbo, 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0));
		 _vao.SetVertexAttribPointer(_vbo, 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);

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

	 void LineGizmo::SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors)
	 {
		 constexpr int vertexSize = sizeof(float) * 7; // position(3) + color(4)
		 const unsigned int posLen = positions.size();

		 if (posLen != colors.size())
			 throw exception{ "Instance data must match in count." };

		 _instanceCount = posLen;
		 _colorList = colors;

		 vector<float> data(posLen * 7, 0.0f);
		 for (int i = 0, j = 0; i < posLen; i++)
		 {
			 data[j++] = positions[i].x; data[j++] = positions[i].y; data[j++] = positions[i].z;
			 data[j++] =	colors[i].x; data[j++] =	colors[i].y; data[j++] =	colors[i].z; data[j++] = colors[i].w;
		 }

		 const unsigned int vboVertCount = ResizeVbo(_vboCapacity, _instanceCount);

		 if (vboVertCount > _vboCapacity) // init buffer if necessary
		 {
			 _vbo.SetData(vboVertCount * vertexSize, nullptr, buf_usg_static_draw);
			 _vboCapacity = vboVertCount;
		 }

		 _vbo.SetSubData(0, _instanceCount * vertexSize, data.data());
	 }

	 LineStripGizmo::LineStripGizmo(RenderContext& rc, const line_strip_gizmo_descriptor& desc) :
		 _vertices		 { desc.vertices->size()}, 
		 _vboVertices	 { rc.CreateVertexBuffer() },
		 _vboInstanceData{ rc.CreateVertexBuffer() },
		 _vao			 { rc.CreateVertexAttribArray() },
		 _lineSize		 { desc.line_size },
		 _patternSize	 { 0 }
	 {
		 // vbo layout for linestrip:
		 // vbo 0: pos(vec3) +  padding(float) because std140
	 	 // vbo 1: transform(mat4) - color(vec4) INSTANCED
		 //--------------------------------------------
		 constexpr int vertexSize   = 4        * sizeof(float);
		 constexpr int instDataSize = (16 + 4) * sizeof(float);
		 _vao.SetVertexAttribPointer(_vboVertices    , 0, 4, vao_typ_float, false, vertexSize  , reinterpret_cast<void*>(0 * sizeof(float)));
		 _vao.SetVertexAttribPointer(_vboInstanceData, 1, 4, vao_typ_float, false, instDataSize, reinterpret_cast<void*>(0 * sizeof(float)), 1);
		 _vao.SetVertexAttribPointer(_vboInstanceData, 2, 4, vao_typ_float, false, instDataSize, reinterpret_cast<void*>(4 * sizeof(float)), 1);
		 _vao.SetVertexAttribPointer(_vboInstanceData, 3, 4, vao_typ_float, false, instDataSize, reinterpret_cast<void*>(8 * sizeof(float)), 1);
		 _vao.SetVertexAttribPointer(_vboInstanceData, 4, 4, vao_typ_float, false, instDataSize, reinterpret_cast<void*>(12* sizeof(float)), 1);
		 _vao.SetVertexAttribPointer(_vboInstanceData, 5, 4, vao_typ_float, false, instDataSize, reinterpret_cast<void*>(16* sizeof(float)), 1);
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);
		 _vao.EnableVertexAttrib(2);
		 _vao.EnableVertexAttrib(3);
		 _vao.EnableVertexAttrib(4);
		 _vao.EnableVertexAttrib(5);

		 // allocate memory and fill vbo
		 // --------------------------------------------

		 // from vec3 to vec4 (needed to read from compute shader) 
		 for (int i = 0; i < desc.vertices->size(); i++) _vertices[i] = vec4((*desc.vertices)[i], 1.0f);

		 vec4 adjStart, adjEnd;
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

		 const unsigned int vboVertCount = ResizeVbo(_vboVertCapacity, _vertices.size());
		 if (vboVertCount > _vboVertCapacity) // init buffer if necessary
		 {
			 _vboVertices.SetData(vboVertCount * vertexSize, nullptr, buf_usg_static_draw);
			 _vboVertCapacity = vboVertCount;
		 }
		 _vboVertices.SetSubData(0, _vertices.size()* vertexSize, _vertices.data());

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

		 const unsigned int dataCmpCnt = (16 + 4);
		 const unsigned int instanceDataSize = dataCmpCnt * sizeof(float);
		 const unsigned int vboInstanceDataCount = ResizeVbo(_vboInstanceDataCapacity, _instanceCount);

		 vector<float> data(dataCmpCnt * vboInstanceDataCount);
		 for (int i = 0; i < vboInstanceDataCount; i++)
		 {
			 for (int j = 0; j < 4 ; j++) data[i * dataCmpCnt     + j] = *(value_ptr(colors    [i]) + j); //todo
			 for (int j = 0; j < 16; j++) data[i * dataCmpCnt + 4 + j] = *(value_ptr(transforms[i]) + j); //todo
		 }

		 if (vboInstanceDataCount > _vboInstanceDataCapacity) // init buffer if necessary
		 {
			 _vboInstanceData.SetData(vboInstanceDataCount * instanceDataSize, nullptr, buf_usg_static_draw);
			 _vboInstanceDataCapacity = vboInstanceDataCount;
		 }
		 _vboInstanceData.SetSubData(0, data.size()*sizeof(float), data.data());
	 }

	 GizmosRenderer::GizmosRenderer(RenderContext& rc, int windowWidth, int windowHeight) :
		 _windowWidth  (windowWidth),
		 _windowHeight (windowHeight),
		 _renderContext(&rc),
		 _pointsShader		 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::points	  , SHADER_SRC_DIR)),
		 _linesShader		 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lines	  , SHADER_SRC_DIR)),
		 _lineStripShader	 (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lineStrip, SHADER_SRC_DIR)),
		 _pointsObjDataUbo	 (rc.CreateUniformBuffer()),
		 _linesObjDataUbo	 (rc.CreateUniformBuffer()),
		 _lineStripObjDataUbo(rc.CreateUniformBuffer()),
		 _frameDataUbo		 (rc.CreateUniformBuffer()),
		 _lenghSumSsbo		 (rc.CreateShaderStorageBuffer()),
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
		 _linesShader	 .SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINES_OBJ_DATA_BINDING);
		 _lineStripShader.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINE_STRIP_OBJ_DATA_BINDING);

		 // per-frame data
		 _pointsShader	 .SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _lineStripShader.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _linesShader	 .SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);

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
		 _frameDataUbo		  .SetData(sizeof(frame_data_block)			 , nullptr, buf_usg_static_draw);

		 _pointsObjDataUbo	  .Bind(POINTS_OBJ_DATA_BINDING);
		 _linesObjDataUbo	  .Bind(LINES_OBJ_DATA_BINDING);
		 _lineStripObjDataUbo .Bind(LINE_STRIP_OBJ_DATA_BINDING);
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

	void GizmosRenderer::CreatePointGizmo(unsigned int pointGizmoIndex, const point_gizmo_descriptor& desc)
	 {
		if (_pointGizmos.contains(pointGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("a point gizmo at index ")
				.append(to_string(pointGizmoIndex))
				.append(" already exist.").c_str());

		_pointGizmos.insert(make_pair(pointGizmoIndex, PointGizmo{*_renderContext, desc}));
	 }

	void GizmosRenderer::DestroyPointGizmo(unsigned int pointGizmoIndex)
	{
		if (!_pointGizmos.contains(pointGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("a point gizmo at index ")
				.append(to_string(pointGizmoIndex))
				.append(" doesn't exist.").c_str());

		_pointGizmos.erase(pointGizmoIndex);
	}

	void GizmosRenderer::InstancePointGizmo(unsigned pointGizmoIndex, const vector<point_gizmo_instance>& instances)
	{
		if (!_pointGizmos.contains(pointGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("cannot instance point gizmo at index ")
				.append(to_string(pointGizmoIndex))
				.append(". It doesn't exist.").c_str());

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

	void GizmosRenderer::CreateLineGizmo(unsigned int lineGizmoIndex, const line_gizmo_descriptor& desc)
	{
		if (_lineGizmos.contains(lineGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("a line gizmo at index ")
				.append(to_string(lineGizmoIndex))
				.append(" already exist.").c_str());

		_lineGizmos.insert(make_pair(lineGizmoIndex, LineGizmo{ *_renderContext, desc }));
	}

	void GizmosRenderer::DestroyLineGizmo(unsigned int lineGizmoIndex)
	{
		if (!_lineGizmos.contains(lineGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("a line gizmo at index ")
				.append(to_string(lineGizmoIndex))
				.append(" doesn't exist.").c_str());

		_lineGizmos.erase(lineGizmoIndex);
	}

	void GizmosRenderer::InstanceLineGizmo(unsigned lineGizmoIndex, const vector<line_gizmo_instance>& instances)
	{
		if (!_lineGizmos.contains(lineGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("cannot instance line gizmo at index ")
				.append(to_string(lineGizmoIndex))
				.append(". It doesn't exist.").c_str());

		vector<vec3>			positions(instances.size() * 2);
		vector<vec4>			colors   (instances.size() * 2);

		auto iter = instances.begin();
		for (int i = 0; i < instances.size()*2; /**/)
		{
			positions[i  ] = iter->start;
			colors	 [i++] = iter->color;

			positions[i  ] = iter->end;
			colors	 [i++] = iter->color;
			
			++iter;
		}

		_lineGizmos.at(lineGizmoIndex).SetInstanceData(positions, colors);
	}

	void GizmosRenderer::CreateLineStripGizmo(unsigned int lineStripGizmoIndex, const line_strip_gizmo_descriptor& desc)
	{
		if (_lineStripGizmos.contains(lineStripGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("a line strip gizmo at index ")
				.append(to_string(lineStripGizmoIndex))
				.append(" already exist.").c_str());

		_lineStripGizmos.insert(make_pair(lineStripGizmoIndex, LineStripGizmo{ *_renderContext, desc }));
	}

	void GizmosRenderer::DestroyLineStripGizmo(unsigned int lineStripGizmoIndex)
	{
		if (!_lineStripGizmos.contains(lineStripGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("a line strip gizmo at index ")
				.append(to_string(lineStripGizmoIndex))
				.append(" doesn't exist.").c_str());

		_lineStripGizmos.erase(lineStripGizmoIndex);
	}

	void GizmosRenderer::InstanceLineStripGizmo(unsigned lineStripGizmoIndex, const vector<line_strip_gizmo_instance>& instances)
	{
		if (!_lineStripGizmos.contains(lineStripGizmoIndex))
			throw exception(
				string{}.append(EXC_PREAMBLE)
				.append("cannot instance line strip gizmo at index ")
				.append(to_string(lineStripGizmoIndex))
				.append(". It doesn't exist.").c_str());


		std::vector<mat4> transforms(instances.size());
		std::vector<vec4> colors    (instances.size());
		for (int i = 0; i < instances.size(); i++)
		{
			transforms[i] = instances[i].transformation;
			colors    [i] = instances[i].color;
		}

		_lineStripGizmos.at(lineStripGizmoIndex).SetInstanceData(transforms, colors);
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

			lGzm.second._vao.Bind();

			_renderContext->DrawArrays(pmt_type_lines, 0, lGzm.second._instanceCount);
		}
	 }

	void GizmosRenderer::RenderLineStripGizmos(glm::mat4 vpMatrix)
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

		if (_lengthSumSsboCapacity < totalVerts)
			_lenghSumSsbo.SetData(totalVerts*sizeof(float), nullptr, buf_usg_dynamic_draw);

		float near = 5;//todo

		vector<float> ssboData(totalVerts);
		vec4 prevPtSS;
		unsigned int i = 0;
		for (const auto& lGzm : _lineStripGizmos)
		for (const auto& tr : lGzm.second._transformList)
		{
			float dstAccum = 0;
			bool  restart = true;
			for (const auto& pt : lGzm.second._vertices)
			{
				const mat4 mvp = vpMatrix * tr;
				const vec4 ptSS = mvp * pt;

				//if(ptSS.w)
				
				if (!restart)
				{
					vec2 d =
						(vec2{	   ptSS.x,	   ptSS.y } /     ptSS.w) -
						(vec2{ prevPtSS.x, prevPtSS.y } / prevPtSS.w);

					d = d * 0.5f;
					d *= vec2{ _windowWidth, _windowHeight };

					dstAccum += length(d);
					ssboData[i++] = dstAccum;
				}
				else
					ssboData[i++] = 0.0f; // first iteration

				prevPtSS = ptSS;
				restart  = false;
			}
		}

		_lenghSumSsbo.SetSubData(0, totalVerts*sizeof(float), ssboData.data());

		unsigned int vertDrawn = 0;
		for (auto& lGzm : _lineStripGizmos)
		{

			const unsigned int vertCount	 = lGzm.second._vertices.size();
			const unsigned int instanceCount = lGzm.second._instanceCount;

			_lenghSumSsbo.BindRange(LINE_STRIP_SSBO_BINDING, vertDrawn * sizeof(float), vertCount * instanceCount * sizeof(float));

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

			_lineStripObjDataUbo.SetSubData(0, sizeof(line_strip_obj_data_block), &objData);

			lGzm.second._vao.Bind();

			_renderContext->DrawArraysInstanced(pmt_type_line_strip_adjacency, 0, vertCount, instanceCount);

			vertDrawn += vertCount * instanceCount;
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
			.viewport_size		=        glm::vec2(_windowWidth, _windowHeight),
			.viewport_size_inv	= 1.0f / glm::vec2(_windowWidth, _windowHeight),
			.near_far			= nearFar //todo 
		};

		_frameDataUbo.SetSubData(0, sizeof(frame_data_block), &frameData);

		RenderPointGizmos();

		RenderLineGizmos();

		RenderLineStripGizmos(projectionMatrix*viewMatrix);

		OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

		return _mainFramebuffer;
	}

}
