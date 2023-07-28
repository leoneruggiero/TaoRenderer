#include "GizmosRenderer.h"

#include <ranges>

#include "TaOglGizmosConfig.h"
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
			throw runtime_error("Unsupported symbol atlas format");
		}
	}

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
		_vertexCount			{ static_cast<unsigned int>(desc.vertices.size())},
		_vbo{rc, 0,
			desc.usage_hint==gizmo_usage_hint::usage_static
			? buf_usg_static_draw
			: buf_usg_dynamic_draw
			, ResizeBuffer },

		_ssboInstanceColor{rc, 0,
			desc.usage_hint == gizmo_usage_hint::usage_static
			? buf_usg_static_draw
			: buf_usg_dynamic_draw, ResizeBuffer},

		_ssboInstanceTransform{ rc, 0,
			desc.zoom_invariant || desc.usage_hint == gizmo_usage_hint::usage_dynamic		// Zoom invariance means we must update
			? buf_usg_dynamic_draw															// the transform list whenever the view/proj
			: buf_usg_static_draw, ResizeBuffer },										// matrix changes -> `dynamic` hint.
																								
		 _ssboInstanceVisibility	{ rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
			? buf_usg_dynamic_draw
			: buf_usg_static_draw, ResizeBuffer },

		_ssboInstanceSelectability { rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
			? buf_usg_dynamic_draw
			: buf_usg_static_draw, ResizeBuffer },

		_vao{rc.CreateVertexAttribArray()},
		_pointSize(desc.point_half_size),
		_snap(desc.snap_to_pixel)
	 {

		 Gizmo::_isZoomInvariant	= desc.zoom_invariant;
		 Gizmo::_zoomInvariantScale = desc.zoom_invariant_scale;
		 
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

	 void PointGizmo::SetInstanceData(const vector<gizmo_instance_descriptor>& instances)
	 {
		Gizmo::SetInstanceData(instances);

		std::vector<glm::mat4> transformations (_instanceCount);
		std::vector<glm::vec4> colors		   (_instanceCount);
		std::vector<unsigned int> visibility   (_instanceCount);
		std::vector<unsigned int> selectability(_instanceCount);

		for(int i=0;i<_instanceCount; i++)
		{
			transformations[i] = _instanceData[i].transform;
			colors	       [i] = _instanceData[i].color;
			visibility	   [i] = _instanceData[i].visible;
			selectability  [i] = _instanceData[i].selectable;
		}

		 // Set instance transformation data
		 unsigned int dataSize = _instanceCount * sizeof(float) * 16;
		 _ssboInstanceTransform.Resize(dataSize);
		 _ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transformations.data());

		 // Set instance color data
		 dataSize = _instanceCount * sizeof(float) * 4;
		 _ssboInstanceColor.Resize(dataSize);
		 _ssboInstanceColor.OglBuffer().SetSubData(0, dataSize, colors.data());

		 // Set visibility data
		 dataSize = _instanceCount * sizeof(unsigned int);
		 _ssboInstanceVisibility.Resize(dataSize);
		 _ssboInstanceVisibility.OglBuffer().SetSubData(0, dataSize, visibility.data());

		 // Set selectability data
		 dataSize = _instanceCount * sizeof(unsigned int);
		 _ssboInstanceSelectability.Resize(dataSize);
		 _ssboInstanceSelectability.OglBuffer().SetSubData(0, dataSize, selectability.data());
	 }

	 LineListGizmo::LineListGizmo(RenderContext& rc, const line_list_gizmo_descriptor& desc) :
		 _vertexCount			{ static_cast<unsigned int>(desc.vertices.size())},
		 _vbo					{ rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_static
			? buf_usg_static_draw
			: buf_usg_dynamic_draw, ResizeBuffer },

		 _ssboInstanceColor		{ rc, 0,
			desc.usage_hint == gizmo_usage_hint::usage_static
			? buf_usg_static_draw
			: buf_usg_dynamic_draw, ResizeBuffer },

		 _ssboInstanceTransform	{ rc, 0,													// Zoom invariance means we must update
			desc.zoom_invariant || desc.usage_hint == gizmo_usage_hint::usage_dynamic		// the transform list whenever the view/proj  
			? ogl_buffer_usage::buf_usg_dynamic_draw										// matrix changes -> `dynamic` hint.
		 	: ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },

		 _ssboInstanceVisibility	{ rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
			? buf_usg_dynamic_draw
			: buf_usg_static_draw, ResizeBuffer },

		_ssboInstanceSelectability { rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
			? buf_usg_dynamic_draw
			: buf_usg_static_draw, ResizeBuffer },

		 _vao					{ rc.CreateVertexAttribArray() },
		 _lineSize				{ desc.line_size    },
		 _patternSize			{ 0 }
	  {

		 Gizmo::_isZoomInvariant	= desc.zoom_invariant;
		 Gizmo::_zoomInvariantScale = desc.zoom_invariant_scale;
		 
		 // vbo layout for points:
		 // pos(vec3)-color(vec4)
		 //--------------------------------------------
		 const unsigned int vertexComponents = 7;
		 const unsigned int vertexSize = vertexComponents * sizeof(float);
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 0, 3, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(0));
		 _vao.SetVertexAttribPointer(_vbo.OglBuffer(), 1, 4, vao_typ_float, false, vertexSize, reinterpret_cast<void*>(3 * sizeof(float)));
		 _vao.EnableVertexAttrib(0);
		 _vao.EnableVertexAttrib(1);

		 unsigned int vertexCount = desc.vertices.size();
		 _vbo.Resize(vertexSize*vertexCount);
		 _vbo.OglBuffer().SetSubData(0, vertexSize* vertexCount, desc.vertices.data());

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

	 void LineListGizmo::SetInstanceData(const vector<gizmo_instance_descriptor>& instances)
	 {
		 Gizmo::SetInstanceData(instances);

		 std::vector<glm::mat4> transformations(_instanceCount);
		std::vector<glm::vec4> colors		   (_instanceCount);
		std::vector<unsigned int> visibility   (_instanceCount);

		for(int i=0;i<_instanceCount; i++)
		{
			transformations[i] = _instanceData[i].transform;
			colors	       [i] = _instanceData[i].color;
			visibility	   [i] = _instanceData[i].visible;
		}

		 // Set instance transformation data
		 unsigned int dataSize = _instanceCount * sizeof(float) * 16;
		 _ssboInstanceTransform.Resize(dataSize);
		 _ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transformations.data());

		 // Set instance color data
		 dataSize = _instanceCount * sizeof(float) * 4;
		 _ssboInstanceColor.Resize(dataSize);
		 _ssboInstanceColor.OglBuffer().SetSubData(0, dataSize, colors.data());

		// Set visibility data
		 dataSize = _instanceCount * sizeof(unsigned int);
		 _ssboInstanceVisibility.Resize(dataSize);
		 _ssboInstanceVisibility.OglBuffer().SetSubData(0, dataSize, visibility.data());
	 }

	 LineStripGizmo::LineStripGizmo(RenderContext& rc, const line_strip_gizmo_descriptor& desc) :
		 _vertexCount			{ static_cast<unsigned int>(desc.vertices.size())},
		 _vertices				(desc.vertices.size()), 
		 _vboVertices			{ rc, 0,
			desc.usage_hint == gizmo_usage_hint::usage_static
			? buf_usg_static_draw
			: buf_usg_dynamic_draw, ResizeBuffer },

		 _ssboInstanceColor		{ rc, 0,
			desc.usage_hint == gizmo_usage_hint::usage_static
			? buf_usg_static_draw
			: buf_usg_dynamic_draw, ResizeBuffer },

		 _ssboInstanceTransform{ rc, 0,
			desc.zoom_invariant || desc.usage_hint == gizmo_usage_hint::usage_dynamic
		 	? ogl_buffer_usage::buf_usg_dynamic_draw
			: ogl_buffer_usage::buf_usg_static_draw, ResizeBuffer },

		 _ssboInstanceVisibility	{ rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
			? buf_usg_dynamic_draw
			: buf_usg_static_draw, ResizeBuffer },

		 _ssboInstanceSelectability { rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
			? buf_usg_dynamic_draw
			: buf_usg_static_draw, ResizeBuffer },

		 _vao					{ rc.CreateVertexAttribArray() },
		 _lineSize				{ desc.line_size },
		 _patternSize			{ 0 }
	 {
		 Gizmo::_isZoomInvariant	= desc.zoom_invariant;
	 	 Gizmo::_zoomInvariantScale	= desc.zoom_invariant_scale;

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

	 void LineStripGizmo::SetInstanceData(const vector<gizmo_instance_descriptor>& instances)
	 {
		Gizmo::SetInstanceData(instances);

		std::vector<glm::mat4> transformations (_instanceCount);
		std::vector<glm::vec4> colors		   (_instanceCount);
		std::vector<unsigned int> visibility   (_instanceCount);
		std::vector<unsigned int> selectability(_instanceCount);

		for(int i=0;i<_instanceCount; i++)
		{
			transformations[i] = _instanceData[i].transform;
			colors	       [i] = _instanceData[i].color;
			visibility	   [i] = _instanceData[i].visible;
			selectability  [i] = _instanceData[i].selectable;
		}

		 // Set instance transformation data
		 unsigned int dataSize = _instanceCount * sizeof(float) * 16;
		 _ssboInstanceTransform.Resize(dataSize);
		 _ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transformations.data());

		 // Set instance color data
		 dataSize = _instanceCount * sizeof(float) * 4;
		 _ssboInstanceColor.Resize(dataSize);
		 _ssboInstanceColor.OglBuffer().SetSubData(0, dataSize, colors.data());

		 // Set visibility data
		 dataSize = _instanceCount * sizeof(unsigned int);
		 _ssboInstanceVisibility.Resize(dataSize);
		 _ssboInstanceVisibility.OglBuffer().SetSubData(0, dataSize, visibility.data());

		 // Set selectability data
		 dataSize = _instanceCount * sizeof(unsigned int);
		 _ssboInstanceSelectability.Resize(dataSize);
		 _ssboInstanceSelectability.OglBuffer().SetSubData(0, dataSize, selectability.data());
	 }

	 MeshGizmo::MeshGizmo(RenderContext& rc, const mesh_gizmo_descriptor& desc) :
		 _vertexCount	{ static_cast<unsigned int>(desc.vertices.size())},
		 _trisCount		{ static_cast<unsigned int>((*desc.triangles).size())},
		 _vertices		{  desc.vertices.size() },
		 _triangles		{  *desc.triangles },

		 _vboVertices				{ rc, 0,
			 desc.usage_hint == gizmo_usage_hint::usage_static
										? buf_usg_static_draw
										: buf_usg_dynamic_draw, ResizeBuffer },

		 _ebo						{ rc, 0,
			desc.usage_hint == gizmo_usage_hint::usage_static
										? buf_usg_static_draw
										: buf_usg_dynamic_draw, ResizeBuffer },

		 _ssboInstanceTransform		{ rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic || desc.zoom_invariant
										? buf_usg_dynamic_draw
										: buf_usg_static_draw, ResizeBuffer },

		 _ssboInstanceVisibility	{ rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
										? buf_usg_dynamic_draw
										: buf_usg_static_draw, ResizeBuffer },

		 _ssboInstanceSelectability { rc, 0,
		 	desc.usage_hint == gizmo_usage_hint::usage_dynamic 
										? buf_usg_dynamic_draw
										: buf_usg_static_draw, ResizeBuffer },

		 _ssboInstanceColorAndNrmMat{ rc, 0,
			desc.usage_hint == gizmo_usage_hint::usage_static
										? buf_usg_static_draw
										: buf_usg_dynamic_draw, ResizeBuffer },

		 _vao{ rc.CreateVertexAttribArray() }
	  {

		 Gizmo::_isZoomInvariant = desc.zoom_invariant;
		 Gizmo::_zoomInvariantScale = desc.zoom_invariant_scale;

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

         _vao.SetIndexBuffer(_ebo.OglBuffer());

	 }

	 void MeshGizmo::SetInstanceData(const vector<gizmo_instance_descriptor>& instances)
	 {
		 Gizmo::SetInstanceData(instances);

		 vector<mat4> transformations		(_instanceCount);
		 vector<vec4> colors				(_instanceCount);
		 vector<unsigned int> visiblity		(_instanceCount);
		 vector<unsigned int> selectability (_instanceCount);

		 for(int i=0;i<_instanceCount;i++)
		 {
			 transformations[i] = instances[i].transform;
			 colors			[i] = instances[i].color;
			 visiblity      [i] = instances[i].visible;
			 selectability  [i] = instances[i].selectable;
		 }

		// Instance data:
		// mat4 transform - mat4 normal matrix - vec4 color - uint visibility - uint selectability
		// [ ---buff0--- ]  [ ---------buff1------------- ]	 [ ----buff2---- ] [ -----buff3------ ] 
		//   per-frame					constant			      constant			 constant
		// (zoom invariance)
		//    

		// Transform
		unsigned int dataSize = _instanceCount * sizeof(float) * 16;
		_ssboInstanceTransform.Resize(dataSize);
		_ssboInstanceTransform.OglBuffer().SetSubData(0, dataSize, transformations.data());

		// Visibility
		dataSize = _instanceCount * sizeof(unsigned int);
		_ssboInstanceVisibility.Resize(dataSize);
		_ssboInstanceVisibility.OglBuffer().SetSubData(0, dataSize, visiblity.data());

		// Selectability
		dataSize = _instanceCount * sizeof(unsigned int);
		_ssboInstanceSelectability.Resize(dataSize);
		_ssboInstanceSelectability.OglBuffer().SetSubData(0, dataSize, selectability.data());

		// Normal matrix and Color
		vector<glm::mat4> normalMatrices{ _instanceCount };
		for (int i = 0; i < _instanceCount; i++) normalMatrices[i] = inverse(transpose(transformations[i]));

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

	 void GizmosRenderer::InitMainFramebuffer(int width, int height)
	{
		 _colorTex.TexImage(8, tex_int_for_rgba, width, height, false);
		 _depthTex.TexImage(8, tex_int_for_depth, width, height, false);

		 _mainFramebuffer.AttachTexture(fbo_attachment_color0, _colorTex, 0);
		 _mainFramebuffer.AttachTexture(fbo_attachment_depth, _depthTex, 0);
	}

	 void GizmosRenderer::InitSelectionFramebuffer(int width, int height)
	 {
		 _selectionColorTex.TexImage(0, tex_int_for_rgba, width, height, tex_for_rgba, tex_typ_byte, nullptr);
		 _selectionDepthTex.TexImage(0, tex_int_for_depth, width, height, tex_for_depth, tex_typ_unsigned_byte, nullptr);

		 _selectionFramebuffer.AttachTexture(fbo_attachment_color0, _selectionColorTex, 0);
		 _selectionFramebuffer.AttachTexture(fbo_attachment_depth, _selectionDepthTex, 0);

		 for(int i=0;i<_selectionPBOsCount;i++)
		 {
			_selectionPBOs.emplace_back(_renderContext->CreatePixelPackBuffer());

			// This works as long as we ask OGL to give us back results 4-byte aligned
			// ( or if we keep the default)
			_selectionPBOs[i].BufferStorage(/*width*height**/4, nullptr, ogl_buffer_flags::buffer_flags_map_read);
		 }
	 }

	 void GizmosRenderer::InitShaders()
	 {
		 // per-object data
		 _pointsShader		.SetUniformBlockBinding(POINTS_OBJ_DATA_BLOCK_NAME, POINTS_OBJ_DATA_BINDING);
		 _linesShader		.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINES_OBJ_DATA_BINDING);
		 _lineStripShader	.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINE_STRIP_OBJ_DATA_BINDING);
		 _meshShader		.SetUniformBlockBinding(MESH_OBJ_BLOCK_NAME, MESH_OBJ_DATA_BINDING);

		 // per-frame data
		 _pointsShader		.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _lineStripShader	.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _linesShader		.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _meshShader		.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);

		 // samplers
		 _pointsShader		.UseProgram(); _pointsShader	.SetUniform(POINTS_TEX_ATLAS_NAME, 0);
		 _linesShader		.UseProgram(); _linesShader		.SetUniform(LINE_PATTERN_TEX_NAME, 0);
		 _lineStripShader	.UseProgram(); _lineStripShader	.SetUniform(LINE_PATTERN_TEX_NAME, 0);
		 // todo: RESET
	 }

	 void GizmosRenderer::InitShadersForSelection()
	 {
		 // per-object data
		 _pointsShaderForSelection		.SetUniformBlockBinding(POINTS_OBJ_DATA_BLOCK_NAME, POINTS_OBJ_DATA_BINDING);
		 _linesShaderForSelection		.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINES_OBJ_DATA_BINDING);
		 _lineStripShaderForSelection	.SetUniformBlockBinding(LINES_OBJ_BLOCK_NAME, LINE_STRIP_OBJ_DATA_BINDING);
		 _meshShaderForSelection		.SetUniformBlockBinding(MESH_OBJ_BLOCK_NAME, MESH_OBJ_DATA_BINDING);

		 // per-frame data
		 _pointsShaderForSelection			.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _lineStripShaderForSelection		.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _linesShaderForSelection			.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);
		 _meshShaderForSelection			.SetUniformBlockBinding(FRAME_DATA_BLOCK_NAME, FRAME_DATA_BINDING);

		 // samplers
		 _pointsShaderForSelection		.UseProgram(); _pointsShaderForSelection	.SetUniform(POINTS_TEX_ATLAS_NAME, 0);
		 _linesShaderForSelection		.UseProgram(); _linesShaderForSelection		.SetUniform(LINE_PATTERN_TEX_NAME, 0);
		 _lineStripShaderForSelection	.UseProgram(); _lineStripShaderForSelection	.SetUniform(LINE_PATTERN_TEX_NAME, 0);
		 // todo: RESET
	 }


	 GizmosRenderer::GizmosRenderer(RenderContext& rc, int windowWidth, int windowHeight) :
		 _windowWidth  (windowWidth),
		 _windowHeight (windowHeight),
		 _renderContext(&rc),
		 _pointsShader				  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::points    , gizmos_shader_modifier::none, SHADER_SRC_DIR)),
		 _linesShader				  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lines	 , gizmos_shader_modifier::none, SHADER_SRC_DIR)),
		 _lineStripShader			  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lineStrip , gizmos_shader_modifier::none, SHADER_SRC_DIR)),
		 _meshShader				  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::mesh      , gizmos_shader_modifier::none, SHADER_SRC_DIR)),
		 _pointsShaderForSelection	  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::points	 , gizmos_shader_modifier::selection, SHADER_SRC_DIR)),
		 _linesShaderForSelection	  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lines	 , gizmos_shader_modifier::selection, SHADER_SRC_DIR)),
		 _lineStripShaderForSelection (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::lineStrip , gizmos_shader_modifier::selection, SHADER_SRC_DIR)),
		 _meshShaderForSelection	  (GizmosShaderLib::CreateShaderProgram(rc, gizmos_shader_type::mesh		 , gizmos_shader_modifier::selection, SHADER_SRC_DIR)),
		 _pointsObjDataUbo	 (rc.CreateUniformBuffer()),
		 _linesObjDataUbo	 (rc.CreateUniformBuffer()),
		 _lineStripObjDataUbo(rc.CreateUniformBuffer()),
		 _meshObjDataUbo     (rc.CreateUniformBuffer()),
		 _frameDataUbo		 (rc.CreateUniformBuffer()),
		 _lenghSumSsbo		 {rc, 0, buf_usg_dynamic_draw, ResizeBuffer },
		 _selectionColorSsbo {rc, 0, buf_usg_dynamic_draw, ResizeBuffer },
		  _selectionPBOs	 {},
		 _nearestSampler	 (rc.CreateSampler()),
		 _linearSampler		 (rc.CreateSampler()),
		 _colorTex			 (rc.CreateTexture2DMultisample()),
		 _depthTex			 (rc.CreateTexture2DMultisample()),
		 _mainFramebuffer	 (rc.CreateFramebuffer<OglTexture2DMultisample>()),
		 _selectionColorTex		(rc.CreateTexture2D()),
		 _selectionDepthTex		(rc.CreateTexture2D()),
		 _selectionFramebuffer	(rc.CreateFramebuffer<OglTexture2D>()),
		 _selectionRequests  {},
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

		 // Framebuffers
		 // -----------------------------------------
		 InitMainFramebuffer	 (_windowWidth, _windowHeight);
		 InitSelectionFramebuffer(_windowWidth, _windowHeight); // false color drawing for mouse picking

		 // Shaders
		 // -----------------------------------------
		 InitShaders();
		 InitShadersForSelection();

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
			 throw runtime_error(
				 string{}.append(exceptionPreamble)
				 .append("an element at index ")
				 .append(to_string(key))
				 .append(" already exist.").c_str());

	}

	 template<typename KeyType, typename ValType>
	 void CheckKeyPresent(const std::map<KeyType, ValType>& map, KeyType key, const char* exceptionPreamble)
	 {
		 if (!map.contains(key))
			 throw runtime_error(
				 string{}.append(exceptionPreamble)
				 .append("an element at index ")
				 .append(to_string(key))
				 .append(" doesn't exist.").c_str());
	 }

	 void CheckKeyValid(unsigned short key, const char* exceptionPreamble)
	{
		if(!key)
			throw runtime_error(
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
				throw std::runtime_error{ "Referencing non-existing render layer" };

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
		throw std::runtime_error{ "Invalid key" };
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

	std::vector<gizmo_instance_id> GizmosRenderer::InstancePointGizmo(gizmo_id key, const vector<gizmo_instance_descriptor>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_POINT);
		CheckKeyPresent(_pointGizmos, k, EXC_PREAMBLE);

		_pointGizmos.at(k).SetInstanceData(instances);

		// give back keys to the caller so that the individual
		// instances can be identified for future operations
		return CreateInstanceKeys(key, instances.size());
	}

	void GizmosRenderer::SetGizmoInstances(const gizmo_id key, const std::vector<std::pair<gizmo_instance_id, gizmo_instance_descriptor>>& instances)
	{
		Gizmo* gzm = nullptr;
		
		const auto kMesh		= DecodeGizmoKey(key._key, KEY_MASK_MESH);
		const auto kPoint		= DecodeGizmoKey(key._key, KEY_MASK_POINT);
		const auto kLineList	= DecodeGizmoKey(key._key, KEY_MASK_LINE);
		const auto kLineStrip	= DecodeGizmoKey(key._key, KEY_MASK_LINE_STRIP);

		// check the key is well-formed
		// only one of the Decode tries
		// should yield a valid result
		if(
			(kMesh>0)		+
			(kPoint>0)		+
			(kLineStrip>0)	+
			(kLineList>0)
							>1) throw std::runtime_error{"Invlid key."};

		if(kMesh)
		{
			CheckKeyPresent(_meshGizmos, kMesh, EXC_PREAMBLE);
			gzm = &_meshGizmos.at(kMesh);
		}
		else if(kPoint)
		{
			CheckKeyPresent(_pointGizmos, kPoint, EXC_PREAMBLE);
			gzm = &_pointGizmos.at(kPoint);
		}
		else if(kLineList)
		{
			CheckKeyPresent(_lineGizmos, kLineList, EXC_PREAMBLE);
			gzm = &_lineGizmos.at(kLineList);
		}
		else if(kLineStrip)
		{
			CheckKeyPresent(_lineStripGizmos, kLineStrip, EXC_PREAMBLE);
			gzm = &_lineStripGizmos.at(kLineStrip);
		}
		
		// 1. copy current data
		// 2. edit only the instance data identified by the keys in `instances`
		// 3. send all the data (old+new) to the GPU
		std::vector<gizmo_instance_descriptor> newInstData	= gzm->GetInstanceData();
		
		for (int i = 0; i < instances.size(); i++)
		{
			const unsigned int index = instances[i].first._instanceKey;

			if (index >= newInstData.size())
				throw std::runtime_error{ "Invalid gizmo instance id." };

			newInstData[index]	= instances[i].second;
		}

		gzm->SetInstanceData(newInstData);
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

	std::vector<gizmo_instance_id> GizmosRenderer::InstanceLineGizmo(const gizmo_id key, const vector<gizmo_instance_descriptor>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_LINE);
		CheckKeyPresent(_lineGizmos, k, EXC_PREAMBLE);

		_lineGizmos.at(k).SetInstanceData(instances);

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

	std::vector<gizmo_instance_id> GizmosRenderer::InstanceLineStripGizmo(const gizmo_id key, const vector<gizmo_instance_descriptor>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_LINE_STRIP);
		CheckKeyPresent(_lineStripGizmos, k, EXC_PREAMBLE);

		_lineStripGizmos.at(k).SetInstanceData(instances);

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

	std::vector<gizmo_instance_id> GizmosRenderer::InstanceMeshGizmo(const gizmo_id key, const vector<gizmo_instance_descriptor>& instances)
	{
		auto k = DecodeGizmoKey(key._key, KEY_MASK_MESH);
		CheckKeyPresent(_meshGizmos, k, EXC_PREAMBLE);

		_meshGizmos.at(k).SetInstanceData(instances);

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
		const vector<gizmo_instance_descriptor>& instanceData)
	{
		vector<glm::mat4> newTransformations{ instanceData.size() };
		float r = 1.0f;
		for (int i = 0; i < instanceData.size(); i++)
		{
			glm::mat4 tr = instanceData[i].transform;

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

	void GizmosRenderer::ProcessPointGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		for (auto& pGzm : _pointGizmos)
		if (pGzm.second._isZoomInvariant)
		{
			vector<mat4> newTransf =
				ComputeZoomInvarianceTransformations(
					pGzm.second._zoomInvariantScale,
					viewMatrix, projectionMatrix, pGzm.second._instanceData
				);
			pGzm.second._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 /*mat4*/ * sizeof(float), newTransf.data());
		}
	}

	void GizmosRenderer::RenderPointGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	{
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

			_pointsObjDataUbo.SetSubData(0, sizeof(points_obj_data_block), &objData);

			// bind static instance data SSBO (draw instanced)
			pGzm.second._ssboInstanceColor.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			pGzm.second._ssboInstanceTransform.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			pGzm.second._ssboInstanceVisibility.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);

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

	void GizmosRenderer::RenderPointGizmosForSelection(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc)
	{
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

			preDrawFunc(pGzm.second._instanceCount);

			_pointsObjDataUbo.SetSubData(0, sizeof(points_obj_data_block), &objData);

			// bind static instance data SSBO (draw instanced)
			pGzm.second._ssboInstanceColor			.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			pGzm.second._ssboInstanceTransform		.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			pGzm.second._ssboInstanceVisibility		.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);
			// bind instance selectability SSBO (draw instanced)
			pGzm.second._ssboInstanceSelectability	.OglBuffer().Bind(INSTANCE_DATA_SELECTABILITY_SSBO_BINDING);

			pGzm.second._vao.Bind();

			_renderContext->DrawArrays(pmt_type_points, 0, pGzm.second._instanceCount);
			
			if (hasTexture)
			{
				OglTexture2D::UnBindToTextureUnit(tex_unit_0);
				OglSampler::UnBindToTextureUnit(tex_unit_0);
			}
		}
	}

	void GizmosRenderer::ProcessLineListGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		for (auto& lGzm : _lineGizmos)
			if (lGzm.second._isZoomInvariant)
			{
				vector<mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						lGzm.second._zoomInvariantScale,
						viewMatrix, projectionMatrix, lGzm.second._instanceData
					);
				lGzm.second._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 /*mat4*/ * sizeof(float), newTransf.data());
			}
	}

	void GizmosRenderer::RenderLineGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	 {
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

			_linesObjDataUbo.SetSubData(0, sizeof(lines_obj_data_block), &objData);
			
			// bind static instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceColor		.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceTransform	.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			lGzm.second._ssboInstanceVisibility	.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);

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

	void GizmosRenderer::RenderLineGizmosForSelection(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc)
	 {
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

			preDrawFunc(lGzm.second._instanceCount);

			_linesObjDataUbo.SetSubData(0, sizeof(lines_obj_data_block), &objData);
			
			// bind static instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceColor			.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceTransform		.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			lGzm.second._ssboInstanceVisibility		.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);
			// bind instance selectability SSBO (draw instanced)
			lGzm.second._ssboInstanceSelectability	.OglBuffer().Bind(INSTANCE_DATA_SELECTABILITY_SSBO_BINDING);

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
		const vector<gizmo_instance_descriptor>& instances, 
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
			const mat4 mvp = projectionMatrix * viewMatrix * instances[i].transform;

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

	void GizmosRenderer::ProcessLineStripGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
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

		for (auto& pair : _lineStripGizmos)
		{
			LineStripGizmo& lGzm = pair.second;
			vector<gizmo_instance_descriptor>& realTransformList = lGzm._instanceData;

			if (lGzm._isZoomInvariant)
			{
				vector<glm::mat4> newTransf =
					ComputeZoomInvarianceTransformations(
						lGzm._zoomInvariantScale,
						viewMatrix, projectionMatrix, lGzm._instanceData
					);

				for(int i=0;i<realTransformList.size();i++)
					realTransformList[i].transform = newTransf[i];

				lGzm._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 * sizeof(float), newTransf.data());
			}

			// compute screen length sum for the line strip gizmo
			auto part = PefixSumLineStrip(
				lGzm._vertices,
				realTransformList,
				viewMatrix, projectionMatrix,
				_windowWidth, _windowHeight);

			std::copy(part.begin(), part.end(), ssboData.begin() + cnt);
			cnt += part.size();
		}

		_lenghSumSsbo.Resize(totalVerts * sizeof(float));
		_lenghSumSsbo.OglBuffer().SetSubData(0, totalVerts * sizeof(float), ssboData.data());
	}

	void GizmosRenderer::RenderLineStripGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	{
		_linesObjDataUbo.Bind(LINE_STRIP_OBJ_DATA_BINDING);

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
			_lenghSumSsbo.OglBuffer().BindRange(LINE_STRIP_SCREEN_LENGTH_SSBO_BINDING, vertDrawn * sizeof(float), vertCount * instanceCount * sizeof(float));
			// bind static instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceColor		.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceTransform	.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			lGzm.second._ssboInstanceVisibility	.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);
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

	void GizmosRenderer::RenderLineStripGizmosForSelection(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc)
	{
		_linesObjDataUbo.Bind(LINE_STRIP_OBJ_DATA_BINDING);

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

			preDrawFunc(lGzm.second._instanceCount);

			// Bind UBO
			_lineStripObjDataUbo.SetSubData(0, sizeof(line_strip_obj_data_block), &objData);
			// Bind SSBO for screen length sum
			_lenghSumSsbo.OglBuffer().BindRange(LINE_STRIP_SCREEN_LENGTH_SSBO_BINDING, vertDrawn * sizeof(float), vertCount * instanceCount * sizeof(float));
			// bind static instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceColor			.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			lGzm.second._ssboInstanceTransform		.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			lGzm.second._ssboInstanceVisibility		.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);
			// bind instance selectability SSBO (draw instanced)
			lGzm.second._ssboInstanceSelectability	.OglBuffer().Bind(INSTANCE_DATA_SELECTABILITY_SSBO_BINDING);
			// Bind VAO
			lGzm.second._vao.Bind();

			_renderContext->DrawArraysInstanced(pmt_type_line_strip_adjacency, 0, vertCount, instanceCount);

			vertDrawn += vertCount * instanceCount;
		}
	}

	void GizmosRenderer::ProcessMeshGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		for (auto& mGzm : _meshGizmos)
		if (mGzm.second._isZoomInvariant)
		{
			vector<glm::mat4> newTransf =
				ComputeZoomInvarianceTransformations(
					mGzm.second._zoomInvariantScale,
					viewMatrix, projectionMatrix, mGzm.second._instanceData
				);
			mGzm.second._ssboInstanceTransform.OglBuffer().SetSubData(0, newTransf.size() * 16 * sizeof(float), newTransf.data());
		}
	}

	void GizmosRenderer::RenderMeshGizmos(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const RenderPass& currentPass)
	{
		_meshObjDataUbo.Bind(MESH_OBJ_DATA_BINDING);

		for (auto& mGzm : _meshGizmos)
		{
			mesh_obj_data_block const objData
			{
				.has_texture = false,
			};

			_meshObjDataUbo.SetSubData(0, sizeof(mesh_obj_data_block), &objData);

			// bind VAO
			mGzm.second._vao.Bind();
			// bind static instance data SSBO (draw instanced)
			mGzm.second._ssboInstanceColorAndNrmMat	.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			mGzm.second._ssboInstanceTransform		.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility SSBO (draw instanced)
			mGzm.second._ssboInstanceVisibility		.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);
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

	void GizmosRenderer::RenderMeshGizmosForSelection(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const std::function<void(unsigned int)>& preDrawFunc)
	{
		_meshObjDataUbo.Bind(MESH_OBJ_DATA_BINDING);

		unsigned int drawnInstances = 0;
		for (auto& mGzm : _meshGizmos)
		{
			mesh_obj_data_block const objData
			{
				.has_texture = false,
			};

			_meshObjDataUbo.SetSubData(0, sizeof(mesh_obj_data_block), &objData);

			preDrawFunc(mGzm.second._instanceCount);

			// bind VAO
			mGzm.second._vao.Bind();
			// bind static instance data SSBO (draw instanced)
			mGzm.second._ssboInstanceColorAndNrmMat	.OglBuffer().Bind(INSTANCE_DATA_STATIC_SSBO_BINDING);
			// bind dynamic instance data SSBO (draw instanced)
			mGzm.second._ssboInstanceTransform		.OglBuffer().Bind(INSTANCE_DATA_DYNAMIC_SSBO_BINDING);
			// bind instance visibility  SSBO (draw instanced)
			mGzm.second._ssboInstanceVisibility		.OglBuffer().Bind(INSTANCE_DATA_VISIBILITY_SSBO_BINDING);
			// bind instance selectability  SSBO (draw instanced)
			mGzm.second._ssboInstanceSelectability	.OglBuffer().Bind(INSTANCE_DATA_SELECTABILITY_SSBO_BINDING);
			// bind EBO
			mGzm.second._ebo.OglBuffer().Bind();

			_renderContext->DrawElementsInstanced(pmt_type_triangles, mGzm.second._trisCount, idx_typ_unsigned_int, nullptr, mGzm.second._instanceCount);
		}
	}

	vec4 IndexToFalseColor(unsigned int index)
	{
		if(index>(1<<24)-1) throw std::runtime_error("index is bigger than 24 bits unsigned.");

		const unsigned char b = (index>>0 ) % 256;
		const unsigned char g = (index>>8 ) % 256;
		const unsigned char r = (index>>16) % 256;

		return vec4{r/255.0f, g/255.0f, b/255.0f, 1.0f};
	}

	unsigned int FalseColorToIndex(unsigned char r, unsigned char g, unsigned char b)
	{
		const unsigned int bi = b;
		const unsigned int gi = g;
		const unsigned int ri = r;
		return
			bi<<0  |
			gi<<8  |
			ri<<16 ;
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
			.view_matrix = viewMatrix,
			.projection_matrix = projectionMatrix,
			.view_position = glm::inverse(viewMatrix) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
			.view_direction = -viewMatrix[2],
			.viewport_size = glm::vec2(_windowWidth, _windowHeight),
			.viewport_size_inv = 1.0f / glm::vec2(_windowWidth, _windowHeight),
			.near_far = nearFar //todo 
		};

		_frameDataUbo.SetSubData(0, sizeof(frame_data_block), &frameData);
		_frameDataUbo.Bind(FRAME_DATA_BINDING);

		ProcessPointGizmos(viewMatrix, projectionMatrix);
		ProcessLineListGizmos(viewMatrix, projectionMatrix);
		ProcessLineStripGizmos(viewMatrix, projectionMatrix);
		ProcessMeshGizmos(viewMatrix, projectionMatrix);

		for (const RenderPass& pass : _renderPasses)
		{
			_meshShader		.UseProgram(); RenderMeshGizmos		(viewMatrix, projectionMatrix, pass);
			_linesShader	.UseProgram(); RenderLineGizmos		(viewMatrix, projectionMatrix, pass);
			_lineStripShader.UseProgram(); RenderLineStripGizmos(viewMatrix, projectionMatrix, pass);
			_pointsShader	.UseProgram(); RenderPointGizmos	(viewMatrix, projectionMatrix, pass);
		}

		// TODO: here???
		ProcessSelectionRequests();

		return _mainFramebuffer;
	}

	

	std::optional<gizmo_instance_id> GizmosRenderer::GetGizmoKeyFromFalseColors(
		const unsigned char* pixelDataAsRGBAUint, unsigned int stride, 
		int imageWidth, int imageHeight,
		unsigned int posX, unsigned int posY, 
		std::vector<std::pair<unsigned long long, glm::ivec2>> lut)
	{
		std::optional<gizmo_instance_id> selectedItem;

		if(posX>=imageWidth || posY>=imageHeight)
			return selectedItem;

		unsigned int index=FalseColorToIndex(
			pixelDataAsRGBAUint[/*posX*4+posY*stride+*/0],
			pixelDataAsRGBAUint[/*posX*4+posY*stride+*/1],
			pixelDataAsRGBAUint[/*posX*4+posY*stride+*/2]
		);

		if(index)
		{
			index-=1; // it was starting at 1

			for(const auto& t : lut)
			{
				if(t.second.x <= index && index < t.second.y)
				{
					selectedItem = gizmo_instance_id{};
					auto gizmoId = gizmo_id{};
					gizmoId._key =t.first;

					selectedItem->_gizmoKey = gizmoId;
					selectedItem->_instanceKey = index-t.second.x;
				}
			}
		}

		return selectedItem;
	}

	void GizmosRenderer::IssueSelectionRequest(unsigned int imageWidth, unsigned int imageHeight, unsigned posX, unsigned posY, std::vector<std::pair<unsigned long long, glm::ivec2>> lut)
	{
		// too many requests, issuing another one will mean
		// binding a PBO that is still waiting for results
		// to be written by OGL or read by us.
		if(_selectionRequests.size()>=_selectionPBOsCount)
			throw std::runtime_error("Too many selection requests.");

		_selectionPBOs[_latestSelectionPBOIdx].Bind();

		// TODO: PIXEL_PACK Alignment
		_renderContext->ReadPixels(posX, posY, 1, 1, read_pix_for_rgba, tex_typ_unsigned_byte, 0);
		
		_selectionRequests.push(SelectionRequest
		{
			._pbo = _selectionPBOs[_latestSelectionPBOIdx],
			._fence = _renderContext->CreateFence(),
			._imageWidth = imageWidth,
			._imageHeight = imageHeight,
			._posX = posX,
			._posY = posY,
			._gizmoKeyIndicesLUT = lut
		});

		_latestSelectionPBOIdx = (++_latestSelectionPBOIdx)%_selectionPBOsCount;
	}


	void GizmosRenderer::ProcessSelectionRequests()
	{
		if(_selectionRequests.empty()) return;

		bool ready=false;

		do // Async read from PBO
		{
			SelectionRequest& currentRequest =_selectionRequests.front();
			
			auto res = currentRequest._fence.ClientWaitSync(wait_sync_flags_none, 0);

			if(	res == wait_sync_res_already_signaled || 
				res == wait_sync_res_condition_satisfied)
				ready = true;
			else if (res == wait_sync_res_timeout_expired)
				ready = false;
			else 
				throw std::runtime_error("Unexpected OpenGl sync object state.");

			if(ready) // The fence for that PBO is signaled, we can read without stalling 
			{
				auto selectedItem = GetGizmoKeyFromFalseColors(
					static_cast<const unsigned char*>(currentRequest._pbo.MapBuffer(map_flags_read_only)), 
					currentRequest._imageWidth * 4,
					currentRequest._imageWidth,
					currentRequest._imageHeight,
					currentRequest._posX, currentRequest._posY, 
					currentRequest._gizmoKeyIndicesLUT
				);

				if(_selectionCallback) (*_selectionCallback)(selectedItem);

				currentRequest._pbo.UnmapBuffer();

				_selectionRequests.pop();
			}
		}
		while(ready && !_selectionRequests.empty());
	}

	void GizmosRenderer::SetSelectionCallback(const std::function<void(std::optional<gizmo_instance_id>)>& callback)
	{
		_selectionCallback = &callback;
	}

	void GizmosRenderer::GetGizmoUnderCursor(const unsigned cursorX, const unsigned cursorY, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& nearFar)
	{
		
		/// Draw for selection
		///--------------------------------------------------------------
		_renderContext->SetDepthState		(SELECTION_DEPTH_STATE);
		_renderContext->SetBlendState		(SELECTION_BLEND_STATE);
		_renderContext->SetRasterizerState	(SELECTION_RASTERIZER_STATE);

		_selectionFramebuffer.Bind(fbo_read_draw);
		_renderContext->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		_renderContext->ClearDepthStencil();

		vector<vec4> falseColors{};
		vector<pair<unsigned long long, ivec2>> lut;
		unsigned int k=0;
		
		/// Mesh false colors
		//////////////////////////////////////////////////////////////////
		for(const auto& val : _meshGizmos	)
		{
			lut.emplace_back(EncodeGizmoKey(val.first, KEY_MASK_MESH), ivec2{k, k+val.second._instanceCount});

			for(int i=0;i<val.second._instanceCount;i++)
				falseColors.push_back(IndexToFalseColor(++k));
		}
		
		/// Lines false colors
		//////////////////////////////////////////////////////////////////
		for(const auto& val : _lineGizmos)
		{
			lut.emplace_back(EncodeGizmoKey(val.first, KEY_MASK_LINE), ivec2{k, k+val.second._instanceCount});

			for(int i=0;i<val.second._instanceCount;i++)
				falseColors.push_back(IndexToFalseColor(++k));
		}
		
		/// Line Strip false colors
		//////////////////////////////////////////////////////////////////
		for(const auto& val : _lineStripGizmos)
		{
			lut.emplace_back(EncodeGizmoKey(val.first, KEY_MASK_LINE_STRIP), ivec2{k, k+val.second._instanceCount});

			for(int i=0;i<val.second._instanceCount;i++)
				falseColors.push_back(IndexToFalseColor(++k));
		}
		
		/// Points false colors
		//////////////////////////////////////////////////////////////////
		for(const auto& val : _pointGizmos)
		{
			lut.emplace_back(EncodeGizmoKey(val.first, KEY_MASK_POINT), ivec2{k, k+val.second._instanceCount});

			for(int i=0;i<val.second._instanceCount;i++)
				falseColors.push_back(IndexToFalseColor(++k));
		}
		
		unsigned int size = falseColors.size()*4*sizeof(float);

		_selectionColorSsbo.Resize(size);
		_selectionColorSsbo.OglBuffer().SetSubData(0, size, falseColors.data());

		unsigned int accum=0;
		auto& ssbo = _selectionColorSsbo;
		auto bindSsboRange = [&accum, &ssbo](unsigned int instancesToDraw)
		{
			ssbo.OglBuffer().BindRange(SELECTION_COLOR_SSBO_BINDING, accum*4*sizeof(float), instancesToDraw*4*sizeof(float));
			accum+=instancesToDraw;
		};

		_meshShaderForSelection.UseProgram();
		RenderMeshGizmosForSelection(viewMatrix, projectionMatrix, bindSsboRange);

		_linesShaderForSelection.UseProgram();
		RenderLineGizmosForSelection(viewMatrix, projectionMatrix, bindSsboRange);
		
		_lineStripShaderForSelection.UseProgram();
		RenderLineStripGizmosForSelection(viewMatrix, projectionMatrix, bindSsboRange);
		
		_pointsShaderForSelection.UseProgram();
		RenderPointGizmosForSelection(viewMatrix, projectionMatrix, bindSsboRange);

		IssueSelectionRequest(_windowWidth, _windowHeight, cursorX, cursorY, lut);

		OglFramebuffer<OglTexture2D>::UnBind(fbo_read_draw);
	}



}