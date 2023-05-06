#include "GizmosRenderer.h"
#include "GizmosShaderLib.h"
#include "GizmosUtils.h"
#include <glm/gtc/type_ptr.hpp>
namespace tao_gizmos
{
	
	 PointGizmo::PointGizmo(RenderContext& rc, const point_gizmo_descriptor& desc):
			_vbo{rc.CreateVertexBuffer()},
			_vao{rc.CreateVertexAttribArray()},
			_pointSize(desc.point_size),
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
			_symbolAtlasLinearFilter = desc.symbol_atlas_descriptor->symbol_filter_smooth;
			_symbolAtlas.emplace(rc.CreateTexture2D());
			_symbolAtlas.value().TexImage(
				0,
				GetSymbolAtlasFormat(desc.symbol_atlas_descriptor->symbol_atlas_data_format),
				desc.symbol_atlas_descriptor->symbol_atlas_width,
				desc.symbol_atlas_descriptor->symbol_atlas_height,
				desc.symbol_atlas_descriptor->symbol_atlas_data_format,
				desc.symbol_atlas_descriptor->symbol_atlas_data_type,
				desc.symbol_atlas_descriptor->symbol_atlas_data
			);

			int const symbolCount = desc.symbol_atlas_descriptor->symbol_atlas_mapping.size();
			_symbolAtlasTexCoordLUT.emplace(vector<vec4>(symbolCount));
			
			for (int i = 0; i < symbolCount; i++)
				(*_symbolAtlasTexCoordLUT)[i] = vec4(
					desc.symbol_atlas_descriptor->symbol_atlas_mapping[i].uv_min,
					desc.symbol_atlas_descriptor->symbol_atlas_mapping[i].uv_max);
		}
	 }

	 ogl_texture_internal_format PointGizmo::GetSymbolAtlasFormat(ogl_texture_format format)
	 {
		 switch(format)
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

	 void PointGizmo::SetInstanceData(const vector<vec3>& positions, const vector<vec4>& colors, const optional<vector<unsigned int>>& symbolIndices)
	 {
		 constexpr int vertexSize = sizeof(float) * 11;
		 const unsigned int posLen = positions.size();

		 if (posLen != colors.size() || (symbolIndices && symbolIndices->size()!= colors.size()))
			 throw exception{ "Instance data must match in count." };
		 
		 _instanceCount = posLen;
		 _colorList = colors;

		 vector<float> data(posLen * 11 , 0.0f);
		 for(int i=0, j=0;i<posLen;i++)
		 {
			 data[j++] = positions[i].x; data[j++] = positions[i].y; data[j++] = positions[i].z;
			 data[j++] =    colors[i].x; data[j++] =    colors[i].y; data[j++] =    colors[i].z; data[j++] = colors[i].z;

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
	 
	 GizmosRenderer::GizmosRenderer(RenderContext& rc, int windowWidth, int windowHeight) :
		 _windowWidth  (windowWidth),
		 _windowHeight (windowHeight),
		 _renderContext(&rc),
		 _pointsShader		(GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::points, SHADER_SRC_DIR)),
		 _linesShader		(GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lines, SHADER_SRC_DIR)),
		 _lineStripShader	(GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lineStrip, SHADER_SRC_DIR)),
		 _pointsObjDataUbo	(rc.CreateUniformBuffer()),
		 _linesObjDataUbo	(rc.CreateUniformBuffer()),
		 _frameDataUbo		(rc.CreateUniformBuffer()),
		 _nearestSampler	(rc.CreateSampler()),
		 _linearSampler		(rc.CreateSampler()),
		 _colorTex			(rc.CreateTexture2DMultisample()),
		 _depthTex			(rc.CreateTexture2DMultisample()),
		 _mainFramebuffer	(rc.CreateFramebuffer<OglTexture2DMultisample>()),
		 _pointGizmos		{}
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
		 _pointsShader.SetUniformBlockBinding(POINTS_OBJ_DATA_BLOCK_NAME, POINTS_OBJ_DATA_BINDING);
		 _lineStripShader.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINES_OBJ_DATA_BINDING);
		 _linesShader.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINES_OBJ_DATA_BINDING);

		 // per-frame data
		 _pointsShader.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _lineStripShader.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _linesShader.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);

		 // uniform buffers
		 // ----------------------------
		 _pointsObjDataUbo.SetData(sizeof(points_obj_data_block), nullptr, buf_usg_static_draw);
		 _linesObjDataUbo.SetData(sizeof(lines_obj_data_block), nullptr, buf_usg_static_draw);
		 _frameDataUbo.SetData(sizeof(frame_data_block), nullptr, buf_usg_static_draw);

		 _pointsObjDataUbo.Bind(POINTS_OBJ_DATA_BINDING);
		 _linesObjDataUbo.Bind(LINES_OBJ_DATA_BINDING);
		 _frameDataUbo.Bind(FRAME_DATA_BINDING);

		// samplers
		// ----------------------------
		 _nearestSampler.SetParams(ogl_sampler_params{ .filter_params{
				 .min_filter = sampler_min_filter_nearest,
				 .mag_filter = sampler_mag_filter_nearest
		 }});

		 _linearSampler.SetParams(ogl_sampler_params{ .filter_params{
				  .min_filter = sampler_min_filter_linear,
				  .mag_filter = sampler_mag_filter_linear
		 }});
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

	const OglFramebuffer<OglTexture2DMultisample>& GizmosRenderer::Render(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
	{
		// todo isCurrent()
		// rc.MakeCurrent();

		// todo: how to show results? maybe return a texture or let the user access the drawn surface.
		_mainFramebuffer.Bind(fbo_read_draw);

		_renderContext->ClearColor(0.5f, 0.5f, 0.5f);
		_renderContext->ClearDepthStencil();
		_renderContext->SetDepthState(ogl_depth_state
		{
			.depth_test_enable			= true,
			.depth_func					= depth_func_less,
			.depth_range_near			= 0.0,
			.depth_range_far			= 1.0
		});
		_renderContext->SetRasterizerState(ogl_rasterizer_state
		{
			.multisample_enable			= true,
			.alpha_to_coverage_enable	= true
		});

		_renderContext->SetBlendState(no_blend);

		frame_data_block const frameData
		{
			.view_matrix		= viewMatrix,
			.projection_matrix	= projectionMatrix,
			.viewport_size		= glm::vec2(_windowWidth, _windowHeight),
			.viewport_size_inv	= 1.0f / glm::vec2(_windowWidth, _windowHeight),
			.near_far			= glm::vec2(0.1f, 100.0f) //todo 
		};

		_frameDataUbo.SetSubData(0, sizeof(frame_data_block), &frameData);

		_pointsShader.UseProgram();

		for(auto& pGzm : _pointGizmos)
		{
			const bool hasTexture = pGzm.second._symbolAtlas.has_value();
			if (hasTexture)
			{
				pGzm.second._symbolAtlas->BindToTextureUnit(tex_unit_0);

				(pGzm.second._symbolAtlasLinearFilter
					? _linearSampler
					: _nearestSampler)
					                     .BindToTextureUnit(tex_unit_0);

				_pointsShader.SetUniform  (POINTS_TEX_ATLAS_NAME, 0);
			}
			points_obj_data_block const objData
			{
				.size		  = pGzm.second._pointSize,
				.snap		  = pGzm.second._snap,
				.has_texture  = hasTexture
			};
			_pointsObjDataUbo.SetSubData(0, sizeof(points_obj_data_block), &objData);

			pGzm.second._vao.Bind();
			_renderContext->DrawArrays(pmt_type_points, 0, pGzm.second._instanceCount);

			if(hasTexture)
			{
				OglTexture2D::UnBindToTextureUnit(tex_unit_0);
				OglSampler::  UnBindToTextureUnit(tex_unit_0);
			}
		}

		OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);

		return _mainFramebuffer;
	}

}
