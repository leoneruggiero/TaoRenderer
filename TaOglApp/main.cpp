// 3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include <thread>

#include "GeometricPrimitives.h"
#include "RenderContext.h"
#include "GizmosRenderer.h"
#include "GizmosHelper.h"
#include "PbrRenderer.h"
#include "../../TestApp_OpenGL/TestApp_OpenGL/stb_image.h" // TODO: WTF??

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

using namespace std;
using namespace glm;
using namespace std::chrono;
using namespace tao_ogl_resources;
using namespace tao_render_context;
using namespace tao_gizmos;
using namespace tao_input;
using namespace tao_pbr;


static vec4 Gradient(float t, vec4 start, vec4 mid, vec4 end)
{
	t = glm::clamp(t, 0.f, 1.f);

	vec3 rgb{};

	if (t < 0.5f)	rgb = t        * mid + (0.5f - t) * start;
	else			rgb = (t-0.5f) * end + (1.0f - t) * mid;
	

	return { rgb*2.f, 1.0f };
}

struct mouse_input_data
{
public:
	float currMouseX = 0.0f;
	float currMouseY = 0.0f;
	float prevMouseX = 0.0f;
	float prevMouseY = 0.0f;
	bool  mouseDown = false;
};


void MouseInputUpdate(mouse_input_data& data, float newX, float newY, float& deltaX, float& deltaY)
{
	data.prevMouseX = data.currMouseX;
	data.prevMouseY = data.currMouseY;
	data.currMouseX = newX;
	data.currMouseY = newY;
	
	if (data.mouseDown)
	{
		data.prevMouseX = data.currMouseX;
		data.prevMouseY = data.currMouseY;
		data.mouseDown = false;
	}

	deltaX = data.currMouseX - data.prevMouseX;
	deltaY = data.currMouseY - data.prevMouseY;
}

vector<LineGizmoVertex> Circle(float radius, unsigned int subdv)
{
	float da = pi<float>()*2.0f/subdv;

	vector<LineGizmoVertex> pts(subdv + 1);

	for (unsigned int i = 0; i <= subdv; i++)
	pts[i].Position(vec3
		{
			cos(i * da),
			sin(i * da),
			0.0f
		} *radius)
	.Color(glm::vec4{ 1.0f });

	return pts;
}

struct MyGizmoLayers
{
	RenderLayer opaqueLayer;
	RenderLayer transparentLayer;
};

struct MyGizmoLayersVC
{
	RenderLayer depthPrePassLayer;
	RenderLayer opaqueLayer;
	RenderLayer blendEqualLayer;
	RenderLayer blendGreaterLayer;
};


std::function<void(long)> animation;
std::function<void(std::optional<gizmo_instance_id>)> onSelection;

void InitDynamicScene(GizmosRenderer& gr, const MyGizmoLayers& layers, std::function<void(long)>& animateFunc)
{
	/// Init Mesh Gizmo (cube)
	//////////////////////////////
	auto geom = tao_geometry::Mesh::Box(1, 1, 1);
	auto positions		= geom.GetPositions();
	auto normals		= geom.GetNormals();
	auto indices						= geom.GetIndices();

	// center to origin
	std::for_each(positions.begin(), positions.end(), [](vec3& v) {v -= vec3{ 0.5 }; });

	std::vector<MeshGizmoVertex> vertices(geom.NumVertices());
	for(int i=0; i<vertices.size(); i++)
	{
		vertices[i] = MeshGizmoVertex{}
			.Position(positions[i])
			.Normal(normals[i])
			.Color(vec4(1.0));
	}

	auto cubeId = gr.CreateMeshGizmo(mesh_gizmo_descriptor
	{
		.vertices = vertices,
		.triangles = &indices,
		.zoom_invariant = false,
		.usage_hint = gizmo_usage_hint::usage_dynamic
	}
	);

	gr.AssignGizmoToLayers(cubeId, { layers.opaqueLayer });

	/// Instancing on a grid
	//////////////////////////////
	const int count = 10;
	const float ext = 2.0f;

	std::vector<gizmo_instance_descriptor> instances(count*count);

	const float d = ext / count;
	for (int y = 0; y < count; y++)
	for (int x = 0; x < count; x++)
	{
		vec3 tr = vec3{ (x - count / 2) * d, (y - count / 2) * d , 0.0f};
		instances[y * count + x].color = vec4{ 1.0f };
		instances[y * count + x].transform = 
			glm::scale(glm::translate(glm::mat4{1.0f}, tr), vec3{ d*0.7f});
		instances[y * count + x].selectable = y>0;
	}

	auto instanceIDs = gr.InstanceMeshGizmo(cubeId, instances);

	animateFunc = [&gr, cubeId, d, ext, count, instanceIDs](float time)
	{
		std::vector<std::pair<gizmo_instance_id, gizmo_instance_descriptor>> instances(count * count);

		for (int y = 0; y < count; y++)
		for (int x = 0; x < count; x++)
		{
			vec3 tr = vec3{ (x - count / 2) * d, (y - count / 2) * d , 0.0f };

			float dst = glm::length(tr);
			float sin = glm::sin((dst/ext)*15.0f-time*0.0025f);
			sin = sin * 0.5f + 0.5f;
			float h = sin * (1.0f - (dst / ext));
			tr.z = h*0.45f;

			vec4 color = vec4{0.0f, 0.0f, 0.0f, 1.0f};//vec4{ 0.3f+h*0.7f, 0.0f, 0.8f + h * 0.2f, 1.0f };
			mat4 transform =
				glm::translate(mat4{ 1.0f }, tr) *
				//glm::rotate(mat4{ 1.0f }, glm::pi<float>() * time * 0.001f*(1.0f - dst/ext), vec3{ 1.0, 0.0, 0.0 })*
				glm::scale(mat4{ 1.0f }, vec3{ d * 0.7f });

			const int idx = y * count + x;
			instances[idx] = std::make_pair(instanceIDs[idx], gizmo_instance_descriptor{ transform, color });
		}

		gr.SetGizmoInstances(cubeId, instances);
	};

	onSelection = [cubeId, instanceIDs, instances, &gr ](std::optional<gizmo_instance_id> selected)
	{
		vector<pair<gizmo_instance_id, gizmo_instance_descriptor>> newInstances(instances.size());

		for(int i=0;i<instances.size();i++)
		{
			newInstances[i] = make_pair(instanceIDs[i], instances[i]);
			if(selected.has_value() && selected==instanceIDs[i])	
				newInstances[i].second.color = vec4{1.0f, 0.0f, 0.0f, 1.0f};
			
		}

		gr.SetGizmoInstances(cubeId, newInstances);
	};

}

void InitGrid(GizmosRenderer& gr, const MyGizmoLayers& layers)
{
	constexpr float width			= 10.0f;
	constexpr float height			= 10.0f;
	constexpr unsigned int subdvX	= 51;
	constexpr unsigned int subdvY	= 51;
	constexpr vec4 colDark			= vec4{ 0.7, 0.7, 0.7, 0.4 };
	constexpr vec4 colLight			= vec4{ 0.4, 0.4, 0.4, 0.4 };

	
	vector<LineGizmoVertex> vertices{ (subdvX + subdvY) *2 };

	for (unsigned int i = 0; i < subdvX; i++)
	{
		const vec4 color = i == subdvX / 2
			? vec4(0.0, 1.0, 0.0, 1.0)
			: i % 5 == 0 ? colDark : colLight;

		vertices[i * 2 + 0] = 
			LineGizmoVertex{}
		.Position({ (-width / 2.0f) + (width / (subdvX - 1)) * i , -height / 2.0f , -0.001f })
			.Color(color);

		vertices[i * 2 + 1] =
			LineGizmoVertex{}
			.Position({ (-width / 2.0f) + (width / (subdvX - 1)) * i ,  height / 2.0f , -0.001f })
			.Color(color);
	}

	for (unsigned int j = 0; j < subdvY; j++)
	{
		const vec4 color = j == subdvY / 2
			? vec4(1.0, 0.0, 0.0, 1.0)
			: j % 5 == 0 ? colDark : colLight;

		vertices[(subdvX + j) * 2 + 0] =
			LineGizmoVertex{}
			.Position({ -width / 2.0f, (-height / 2.0f) + (height / (subdvY - 1)) * j, -0.001f })
			.Color(color);

		vertices[(subdvX + j) * 2 + 1] =
			LineGizmoVertex{}
			.Position({ width / 2.0f, (-height / 2.0f) + (height / (subdvY - 1)) * j , -0.001f })
			.Color(color);
	}
	auto key = gr.CreateLineGizmo(line_list_gizmo_descriptor{
		.vertices = vertices,
		.line_size = 1,
		.pattern_texture_descriptor = nullptr
	});

	// a single instance
	vector<gizmo_instance_descriptor> instances
	{
		gizmo_instance_descriptor
		{
			.transform	= glm::mat4{1.0f},
			.color		= vec4{1.0f}
		}
	};
	gr.InstanceLineGizmo(key, instances);

	gr.AssignGizmoToLayers(key, { layers.transparentLayer });
}
void InitWorldAxis(GizmosRenderer& gr, const MyGizmoLayers& layers)
{
	/// Create 3 dashed axis in X,Y and Z
	//////////////////////////////////////////

	constexpr unsigned int lineWidth  = 4;
	constexpr unsigned int patternLen = 20;

	// dashed pattern
	tao_gizmos_procedural::TextureDataRgbaUnsignedByte patternTex
	{
		patternLen,
		lineWidth,
		tao_gizmos_procedural::TexelData<4,unsigned char>({0,0,0,0})
	};

	patternTex.FillWithFunction([](unsigned x, unsigned y)
	{
		float h = lineWidth / 2;
		float v = tao_gizmos_sdf::SdfSegment(vec2(x,y), vec2(4, h), vec2(16, h));
			  v = tao_gizmos_sdf::SdfInflate(v, 3.0f);

		float a = glm::clamp(0.5f - v, 0.0f, 1.0f);
		return vec<4, unsigned char>{255, 255, 255, static_cast<unsigned char>(a * 255)};
	});

	pattern_texture_descriptor patternDesc
	{
		.data			= patternTex.DataPtr(),
		.data_format	= tex_for_rgba,
		.data_type		= tex_typ_unsigned_byte,
		.width			= patternLen,
		.height			= lineWidth,
		.pattern_length = patternLen
	};
	
}

void CreateDashedPatternVC(unsigned int patternLen, unsigned int patternWidth, tao_gizmos_procedural::TextureDataRgbaUnsignedByte& tex)
{
	
	tex.FillWithFunction([patternLen, patternWidth](unsigned int x, unsigned int y)
	{
		vec2 sampl = vec2{ x, y }+vec2{0.5f};

		float d = patternLen / 4.0f;

		vec2 dashStart = vec2{ d , patternWidth / 2.0f };
		vec2 dashEnd = vec2{ patternLen - d , patternWidth / 2.0f };

		float val =
			tao_gizmos_sdf::SdfSegment<float>{ sampl, dashStart, dashEnd };
		val = tao_gizmos_sdf::SdfInflate(val, patternWidth*0.3f);
		val = glm::clamp(val, 0.0f, 1.0f);
		return vec<4, unsigned char>{ 255, 255, 255, (1.0f - val) * 255 };
	});
	
}

void InitViewCube(tao_gizmos::GizmosRenderer& gizRdr, const MyGizmoLayersVC& layers)
{
	/// Settings
	///////////////////////////////////////////////////////////////////////
	float zoomInvariantScale = 0.08f;
	vec4 cubeColor = vec4{ 0.8, 0.8, 0.8, 0.0f };
	vec4 edgeColor = vec4{ 0.8, 0.8, 0.8, 1.0f };

	/// 3D Mesh
	///////////////////////////////////////////////////////////////////////
	auto box = tao_geometry::Mesh::Box(1, 1, 1);
	auto pos = box.GetPositions();
	auto nrm = box.GetNormals();
	auto tri = box.GetIndices();

	for (auto& p : pos) p -= vec3{ 0.5, 0.5, 0.5 }; 

	vector<MeshGizmoVertex> vertices{pos.size()};

	for(int i=0;i<vertices.size();i++)
	{
		vertices[i] = MeshGizmoVertex{}
		.Position(pos[i])
		.Normal(nrm[i])
		.Color(cubeColor);
	}

	auto key = gizRdr.CreateMeshGizmo(mesh_gizmo_descriptor
		{
				.vertices = vertices,
				.triangles = &tri,
				.zoom_invariant = false,
				.zoom_invariant_scale = zoomInvariantScale
		});

	gizRdr.InstanceMeshGizmo(key,
	{
			gizmo_instance_descriptor{glm::scale(glm::mat4{1.0f}, vec3{0.98f}) , glm::vec4{1.0f}}
		});

	gizRdr.AssignGizmoToLayers(key, { layers.blendEqualLayer, layers.depthPrePassLayer });

	/// Edges
	///////////////////////////////////////////////////////////////////////
	vector<vec3> edgePos{ 8 };
	edgePos[0] = vec3{ -0.5f,  -0.5f, -0.5f };
	edgePos[1] = vec3{  0.5f, -0.5f, -0.5f };
	edgePos[2] = vec3{  0.5f,  0.5f, -0.5f };
	edgePos[3] = vec3{  -0.5f, 0.5f, -0.5f };
	for (int i = 0; i < 4; i++) edgePos[i + 4] = edgePos[i] + vec3{ 0.0f, 0.0f, 1.0f };

	vector<LineGizmoVertex> edgeVertices;
	for (int i = 0; i < 4; i++)
	{
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[(i + 0)%4]).Color(edgeColor));
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[(i + 1)%4]).Color(edgeColor));
	}
	for (int i = 0; i < 4; i++)
	{
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[4+((i + 0) % 4)]).Color(edgeColor));
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[4+((i + 1) % 4)]).Color(edgeColor));
	}
	for (int i = 0; i < 4; i++)
	{
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[i + 0]).Color(edgeColor));
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[i + 4]).Color(edgeColor));
	}

	unsigned int edgeWidth = 3;
	auto edgesKey = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor
	{
		.vertices = edgeVertices,
		.line_size = edgeWidth,
		.zoom_invariant = false,
		.zoom_invariant_scale = zoomInvariantScale ,
		.pattern_texture_descriptor = nullptr
	});

	tao_gizmos_procedural::TextureDataRgbaUnsignedByte dashTex{ 16, 2, {0} };
	CreateDashedPatternVC(16, 2, dashTex);
	pattern_texture_descriptor dashPattern
	{
		.data = dashTex.DataPtr(),
		.data_format = tex_for_rgba,
		.data_type = tex_typ_unsigned_byte,
		.width = 16,
		.height = 2,
		.pattern_length = 16
	};

	auto dashedEdgesKey = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor
		{
			.vertices = edgeVertices,
			.line_size = 2,
			.zoom_invariant = false,
			.zoom_invariant_scale = zoomInvariantScale ,
			.pattern_texture_descriptor = &dashPattern
		});

	gizRdr.InstanceLineGizmo(edgesKey,
		{
			gizmo_instance_descriptor{glm::mat4{1.0f}, vec4{1.0f}}
		});
	gizRdr.InstanceLineGizmo(dashedEdgesKey,
		{
			gizmo_instance_descriptor{glm::mat4{1.0f}, vec4{0.6f}}
		});

	gizRdr.AssignGizmoToLayers(edgesKey, { layers.depthPrePassLayer, layers.opaqueLayer });
	gizRdr.AssignGizmoToLayers(dashedEdgesKey, { layers.blendGreaterLayer });
}

void InitArrowTriad2D(tao_gizmos::GizmosRenderer& gizRdr, const MyGizmoLayers& layers)
{
	constexpr float axisLen = 0.8f;
	constexpr float arrowWidth = 0.2f;
	constexpr float arrowDepth = 0.4f;
	constexpr glm::vec4 whi{ 0.8f , 0.8f, 0.8f, 1.0f };
	constexpr glm::vec4 red{ 0.7f , 0.2f, 0.2f, 1.0f };
	constexpr glm::vec4 gre{ 0.2f , 0.6f, 0.0f, 1.0f };
	constexpr glm::vec4 blu{ 0.2f , 0.0f, 0.6f, 1.0f };
	vector<LineGizmoVertex> verts{};
	vector<LineGizmoVertex> axisVertsX{};
	vector<LineGizmoVertex> axisVertsY{};
	vector<LineGizmoVertex> axisVertsZ{};

	/// X Axis
	///////////////////////////////////
	axisVertsX.push_back(LineGizmoVertex{}.Position({ 0.0f	 , 0.0f, 0.0f }).Color(whi));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen, 0.0f, 0.0f }).Color(red));


	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen, 0.0f,  arrowWidth * 0.5f }).Color(red));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen, 0.0f, -arrowWidth * 0.5f }).Color(red));

	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen			 ,0.0f, arrowWidth * 0.5f }).Color(red));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen + arrowDepth,0.0f			  , 0.0f }).Color(red));

	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen			 , 0.0f, -arrowWidth * 0.5f }).Color({ red }));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen + arrowDepth, 0.0f, 0.0f }).Color({ red }));

	/// Y Axis
	///////////////////////////////////
	glm::mat4 rot = glm::rotate(glm::mat4{ 1.0f }, glm::pi<float>() * 0.5f, glm::vec3{ 0.0f, 0.0f, 1.0f });
	for (int i = 0; i < axisVertsX.size(); i++)
		axisVertsY.push_back(LineGizmoVertex{}.Position(rot * glm::vec4{ axisVertsX[i].GetPosition(), 1.0f }).Color(i > 0 ? gre : whi));

	// Z Axis
	///////////////////////////////////
	rot = glm::rotate(glm::mat4{ 1.0f }, -glm::pi<float>() * 0.5f, glm::vec3{ 0.0f, 1.0f, 0.0f });
	for (int i = 0; i < axisVertsX.size(); i++)
		axisVertsZ.push_back(LineGizmoVertex{}.Position(rot * glm::vec4{ axisVertsX[i].GetPosition(), 1.0f }).Color(i > 0 ? blu : whi));

	// batch
	for (const auto& v : axisVertsX) verts.push_back(v);
	for (const auto& v : axisVertsY) verts.push_back(v);
	for (const auto& v : axisVertsZ) verts.push_back(v);

	auto key = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor
	{
		.vertices					= verts,
		.line_size					= 3,
		.zoom_invariant				= true,
		.zoom_invariant_scale		= 0.1f,
		.pattern_texture_descriptor = nullptr
	});

	gizRdr.InstanceLineGizmo(key,
{
		gizmo_instance_descriptor{ translate(mat4{ 1.0f }, vec3{ 1.0, -1.0, 0.25 }), vec4(1.0f) },
		gizmo_instance_descriptor{ translate(mat4{ 1.0f }, vec3{-1.0, 1.0, 0.25 }), vec4(1.0f) }
		});

	gizRdr.AssignGizmoToLayers(key, { layers.opaqueLayer });
}



void CreateArrowTriad(
	vector   <glm::vec3>& positions,
	vector   <glm::vec3>& normals,
	vector   <glm::vec4>& colors,
	vector<int>& triangles)
{
	tao_geometry::Mesh x = tao_geometry::Mesh::Arrow(0.05f, 0.5f, 0.13f, 0.5f, 32);
	tao_geometry::Mesh y = tao_geometry::Mesh::Arrow(0.05f, 0.5f, 0.13f, 0.5f, 32);
	tao_geometry::Mesh z = tao_geometry::Mesh::Arrow(0.05f, 0.5f, 0.13f, 0.5f, 32);

	glm::mat3 trX = glm::rotate(glm::mat4(1.0f), glm::pi<float>() * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat3 trY = glm::rotate(glm::mat4(1.0f), glm::pi<float>() * 0.5f, glm::vec3(-1.0f, 0.0f, 0.0f));
	glm::mat3 trZ = glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));

	positions = vector   <glm::vec3>(x.NumVertices() + y.NumVertices() + z.NumVertices());
	normals = vector   <glm::vec3>(x.NumVertices() + y.NumVertices() + z.NumVertices());
	colors = vector   <glm::vec4>(x.NumVertices() + y.NumVertices() + z.NumVertices());
	triangles = vector<int>(x.NumIndices() + y.NumIndices() + z.NumIndices());

	// Positions
	// ------------------------------------
	auto xPos = x.GetPositions(); std::for_each(xPos.begin(), xPos.end(), [&trX](glm::vec3& pos) {pos = trX * pos; });
	auto yPos = y.GetPositions(); std::for_each(yPos.begin(), yPos.end(), [&trY](glm::vec3& pos) {pos = trY * pos; });
	auto zPos = z.GetPositions(); std::for_each(zPos.begin(), zPos.end(), [&trZ](glm::vec3& pos) {pos = trZ * pos; });

	std::copy(xPos.begin(), xPos.end(), positions.begin());
	std::copy(yPos.begin(), yPos.end(), positions.begin() + xPos.size());
	std::copy(zPos.begin(), zPos.end(), positions.begin() + xPos.size() + yPos.size());

	// Normals
	// ------------------------------------
	auto xNrm = x.GetNormals(); std::for_each(xNrm.begin(), xNrm.end(), [&trX](glm::vec3& nrm) {nrm = trX * nrm; }); // rotation only,
	auto yNrm = y.GetNormals(); std::for_each(yNrm.begin(), yNrm.end(), [&trY](glm::vec3& nrm) {nrm = trY * nrm; }); // should be safe to 
	auto zNrm = z.GetNormals(); std::for_each(zNrm.begin(), zNrm.end(), [&trZ](glm::vec3& nrm) {nrm = trZ * nrm; }); // transform the normal

	std::copy(xNrm.begin(), xNrm.end(), normals.begin());
	std::copy(yNrm.begin(), yNrm.end(), normals.begin() + xNrm.size());
	std::copy(zNrm.begin(), zNrm.end(), normals.begin() + xNrm.size() + yNrm.size());

	// Triangles
	// ------------------------------------
	unsigned int xVertCount = xPos.size();
	unsigned int yVertCount = yPos.size();

	// offset triangles (merge meshes)
	auto xTri = x.GetIndices();
	auto yTri = y.GetIndices(); std::for_each(yTri.begin(), yTri.end(), [xVertCount](int& i) {i += xVertCount; });
	auto zTri = z.GetIndices(); std::for_each(zTri.begin(), zTri.end(), [xVertCount, yVertCount](int& i) {i += xVertCount + yVertCount; });

	std::copy(xTri.begin(), xTri.end(), triangles.begin());
	std::copy(yTri.begin(), yTri.end(), triangles.begin() + xTri.size());
	std::copy(zTri.begin(), zTri.end(), triangles.begin() + xTri.size() + yTri.size());

	// Color
	// ------------------------------------
	std::generate(colors.begin(), colors.begin() + xVertCount, []() { return glm::vec4(1.0, 0.0, 0.0, 1.0); });
	std::generate(colors.begin() + xVertCount, colors.begin() + xVertCount + yVertCount, []() { return glm::vec4(0.0, 1.0, 0.0, 1.0); });
	std::generate(colors.begin() + xVertCount + yVertCount, colors.end(), []() { return glm::vec4(0.0, 0.0, 1.0, 1.0); });

}

void InitArrowTriad3D(tao_gizmos::GizmosRenderer& gizRdr, const tao_gizmos::RenderLayer& layer)
{
	vector<glm::vec3> cubeMeshPos;
	vector<glm::vec3> cubeMeshNrm;
	vector<glm::vec2> cubeMeshTex;
	vector<glm::vec4> cubeMeshCol;
	vector<int> cubeMeshTris;

	CreateArrowTriad(cubeMeshPos, cubeMeshNrm, cubeMeshCol, cubeMeshTris);

	vector<MeshGizmoVertex> cubeMeshVertices{ cubeMeshPos.size() };

	for (int i = 0; i < cubeMeshVertices.size(); i++)
	{
		cubeMeshVertices[i] =
			MeshGizmoVertex{}
			.Position(cubeMeshPos[i])
			.Normal(cubeMeshNrm[i])
			.Color(cubeMeshCol[i]);
	}

	auto key = gizRdr.CreateMeshGizmo(mesh_gizmo_descriptor
		{
			.vertices = cubeMeshVertices,
			.triangles = &cubeMeshTris,
			.zoom_invariant = true,
			.zoom_invariant_scale = 0.1f
		});

	vector<gizmo_instance_descriptor> instances =
		{
			gizmo_instance_descriptor{ translate(mat4{ 1.0f }, vec3{-1.0,-1.0, 0.25 }), vec4(1.0f) },
			gizmo_instance_descriptor{ translate(mat4{ 1.0f }, vec3{ 1.0, 1.0, 0.25 }), vec4(1.0f) },
		};

	auto keys= gizRdr.InstanceMeshGizmo(key, instances);


	gizRdr.AssignGizmoToLayers(key, { layer });
}

void InitArrowTriad3DVC(tao_gizmos::GizmosRenderer& gizRdr, const tao_gizmos::RenderLayer& layer)
{
	vector<glm::vec3> cubeMeshPos;
	vector<glm::vec3> cubeMeshNrm;
	vector<glm::vec2> cubeMeshTex;
	vector<glm::vec4> cubeMeshCol;
	vector<int> cubeMeshTris;

	CreateArrowTriad(cubeMeshPos, cubeMeshNrm, cubeMeshCol, cubeMeshTris);

	vector<MeshGizmoVertex> cubeMeshVertices{ cubeMeshPos.size() };

	for (int i = 0; i < cubeMeshVertices.size(); i++)
	{
		cubeMeshVertices[i] =
			MeshGizmoVertex{}
			.Position(cubeMeshPos[i])
			.Normal(cubeMeshNrm[i])
			.Color(cubeMeshCol[i]);
	}

	auto key = gizRdr.CreateMeshGizmo(mesh_gizmo_descriptor
		{
			.vertices = cubeMeshVertices,
			.triangles = &cubeMeshTris,
			.zoom_invariant = false,
			.zoom_invariant_scale = 0.1f
		});

	gizRdr.InstanceMeshGizmo(key,
		{
			gizmo_instance_descriptor{ scale(mat4{ 1.0f }, vec3{1.5f}), vec4(1.0f) },
		});

	gizRdr.AssignGizmoToLayers(key, { layer });
}

RenderLayer InitOpaqueGizmoLayer(GizmosRenderer& gr)
{
	auto opaqueLayer = gr.CreateRenderLayer(
		ogl_depth_state
		{
			.depth_test_enable = true,
			.depth_write_enable = true,
			.depth_func = depth_func_less_equal,
			.depth_range_near = 0.0,
			.depth_range_far = 1.0
		},
		ogl_blend_state
		{
			.blend_enable = false,
		},
		ogl_rasterizer_state
		{
			.culling_enable = false ,
			.polygon_mode = polygon_mode_fill,
			.multisample_enable = true,
			.alpha_to_coverage_enable = true
		}
	);

	return opaqueLayer;
}
RenderLayer InitTransparentGizmoLayer(GizmosRenderer& gr)
{
	auto blendLayer = gr.CreateRenderLayer(
		ogl_depth_state
		{
			.depth_test_enable = true,
			.depth_write_enable = false,
			.depth_func = depth_func_less_equal,
			.depth_range_near = 0.0,
			.depth_range_far = 1.0
		},
		ogl_blend_state
		{
			.blend_enable = true,
			.blend_equation_rgb = ogl_blend_equation{blend_func_add, blend_fac_src_alpha, blend_fac_one_minus_src_alpha},
			.blend_equation_alpha = ogl_blend_equation{blend_func_add, blend_fac_one, blend_fac_one }
		},
		ogl_rasterizer_state
		{
			.culling_enable = false ,
			.polygon_mode = polygon_mode_fill,
			.multisample_enable = true,
			.alpha_to_coverage_enable = false
		}
	);

	return blendLayer;
}

MyGizmoLayers InitGizmosLayersAndPasses(GizmosRenderer& gr)
{
	auto opaqueLayer		= InitOpaqueGizmoLayer(gr);
	auto transparentLayer	= InitTransparentGizmoLayer(gr);
	auto opaquePass			= gr.CreateRenderPass({ opaqueLayer });
	auto transparentPass	= gr.CreateRenderPass({ transparentLayer });

	gr.SetRenderPasses({ opaquePass, transparentPass });

	MyGizmoLayers layers
	{
		.opaqueLayer = opaqueLayer,
		.transparentLayer = transparentLayer
	};

	return layers;
}

MyGizmoLayersVC InitGizmosLayersAndPassesVC(GizmosRenderer& gr)
{
	MyGizmoLayersVC myLayers
	{
		.depthPrePassLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = true,
				.depth_func = depth_func_less_equal,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			ogl_blend_state
			{
				.blend_enable = false,
				.color_mask = mask_none
			},
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			}
		) ,
		.opaqueLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = true,
				.depth_func = depth_func_less_equal,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			no_blend,
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = true
			}
		) ,
		.blendEqualLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = false,
				.depth_func = depth_func_equal,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			alpha_interpolate,
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			}
		) ,
		.blendGreaterLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = false,
				.depth_func = depth_func_greater,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			alpha_interpolate,
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			}
		)
	};

	gr.SetRenderPasses({
			gr.CreateRenderPass({ myLayers.depthPrePassLayer }),
			gr.CreateRenderPass({ myLayers.blendGreaterLayer}),
			gr.CreateRenderPass({ myLayers.opaqueLayer }),
			gr.CreateRenderPass({ myLayers.blendEqualLayer })
		});

	return myLayers;

}

void InitGizmos(GizmosRenderer& gr)
{
	MyGizmoLayers layers = InitGizmosLayersAndPasses(gr);

	//InitWorldAxis		(gr, layers);
	InitGrid			(gr, layers);
	InitArrowTriad3D	(gr, layers.opaqueLayer);
	InitArrowTriad2D	(gr, layers);
	InitDynamicScene	(gr, layers, animation);
}

void InitGizmosVC(GizmosRenderer& gr)
{
	MyGizmoLayersVC layers = InitGizmosLayersAndPassesVC(gr);
	InitViewCube(gr, layers);
	//InitArrowTriad3DVC(gr, layers.opaqueLayer);
}

// ImGui helper functions straight from the docs
// ----------------------------------------------
void InitImGui(GLFWwindow* window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // IF using Docking Branch

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);   // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();
}

void StartImGuiFrame(PbrRenderer& pbrRenderer /* TODO */)
{
    // (Your code calls glfwPollEvents())
    // ...
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::ShowDemoWindow(); // Show demo window! :)

    if(ImGui::Button("Reload shaders"))
    {
        // TODO: a lot of todos....
        try
        {
            pbrRenderer.ReloadShaders();
        }
        catch(std::exception& e)
        {
            std::cout<<e.what()<<std::endl;
        }
    }
}

void EndImGuiFrame()
{
    // Rendering
    // (Your code clears your framebuffer, renders your other stuff etc.)
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // (Your code calls glfwSwapBuffers() etc.)
}

void ImGuiShutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
// ----------------------------------------------

int main()
{
	try
	{
		int windowWidth = 1920;
		int windowHeight = 1080;
		int fboWidth = 0;
		int fboHeight = 0;

		RenderContext rc{ windowWidth, windowHeight };
		rc.GetFramebufferSize(fboWidth, fboHeight);

        InitImGui(rc.GetWindow());

        // GizmosRenderer
		GizmosRenderer gizRdr{ rc, fboWidth, fboHeight };

        // View Cube Gizmos Renderer
		int fboWidthVC  = 256;
		int fboHeightVC = 256;
		GizmosRenderer gizRdrVC{ rc, fboWidthVC, fboHeightVC };

        // Pbr Renderer
        PbrRenderer pbrRdr{rc, fboWidth, fboHeight};

        // Handle window resize
        rc.SetResizeCallback
        ([&rc, &gizRdr, &gizRdrVC, &pbrRdr, &fboWidth, &fboHeight](int w, int h)
        {
            rc.GetFramebufferSize(fboWidth, fboHeight);

            gizRdr.Resize(fboWidth, fboHeight);
            pbrRdr.Resize(fboWidth, fboHeight);
        });

        // *** TEST ***
        // -------------------
        auto sphere = tao_geometry::Mesh::Sphere(1.0f, 32);
        tao_pbr::Mesh sphereMesh{
            sphere.GetPositions(),
            sphere.GetNormals(),
            sphere.GetTextureCoordinates(),
            sphere.GetIndices()
        };
        auto cube = tao_geometry::Mesh::Box(2.0f, 2.0f, 2.0f);
        tao_pbr::Mesh cubeMesh{
                cube.GetPositions(),
                cube.GetNormals(),
                cube.GetTextureCoordinates(),
                cube.GetIndices()
        };

        auto sphereMeshKey = pbrRdr.AddMesh(sphereMesh);
        auto cubeMeshKey = pbrRdr.AddMesh(cubeMesh);
        auto blueMat = pbrRdr.AddMaterial(PbrMaterial{0.6f, 0.0f, glm::vec3(0.0f, 0.3f, 1.0f)});
        auto redMat = pbrRdr.AddMaterial(PbrMaterial{0.6f, 0.0f, glm::vec3(1.0f, 0.3f, 0.0f)});
        auto orangeMat = pbrRdr.AddMaterial(PbrMaterial{0.6f, 0.0f, glm::vec3(0.9f, 0.6f, 0.0f)});

        MeshRenderer mr0 = MeshRenderer(glm::mat4(1.0f), sphereMeshKey, blueMat);
        MeshRenderer mr1 = MeshRenderer(glm::mat4(1.0f), cubeMeshKey, redMat);
        MeshRenderer mr2 = MeshRenderer(glm::mat4(1.0f), sphereMeshKey, redMat);
        MeshRenderer mr3 = MeshRenderer(glm::mat4(1.0f), cubeMeshKey, blueMat);
        MeshRenderer mr4 = MeshRenderer(glm::mat4(1.0f), sphereMeshKey, orangeMat);

        mr0.transformation.Translate({-1.0f, 0.0f, 2.0f});
        mr1.transformation.Translate({0.2f, -1.0f, 0.5f});
        mr2.transformation.Translate({3.2f, 2.2f, 1.3f});
        mr3.transformation.Translate({2.0f, -0.5f, 0.0f});
        mr4.transformation.Translate({0.0f, -1.0f, -3.0f});

        auto meshRdrKey0 = pbrRdr.AddMeshRenderer(mr0);
        auto meshRdrKey1 = pbrRdr.AddMeshRenderer(mr1);
        auto meshRdrKey2 = pbrRdr.AddMeshRenderer(mr2);
        auto meshRdrKey3 = pbrRdr.AddMeshRenderer(mr3);
        auto meshRdrKey4 = pbrRdr.AddMeshRenderer(mr4);
        // -------------------

		mouse_input_data mouseRotateData;
		mouse_input_data mouseZoomData;
		bool enableRotate = false;
		bool enableZoom = false;
		MouseInputListener mouseRotate
		{
			[&rc]()
			{
				return
					rc.Mouse().IsPressed(left_mouse_button) &&
					!rc.Mouse().IsKeyPressed(left_shift_key);
			},
			[&mouseRotateData, &enableRotate]()
			{
				mouseRotateData.mouseDown = true;
				enableRotate = true;
			},
			[]()
			{
			},
			60.0f
		};
		MouseInputListener mouseZoom
		{
			[&rc]()
			{
				return
					rc.Mouse().IsPressed(left_mouse_button) &&
					rc.Mouse().IsKeyPressed(left_shift_key);
			},
			[&mouseZoomData, &enableZoom]()
			{
				mouseZoomData.mouseDown = true;
				enableZoom = true;
			},
			[]()
			{
			},
			60.0f
		};
		MouseInputListener mouseRawPosition
		{
			[](){return true;},
			[](){},
			[](){}
		};
		rc.Mouse().AddListener(mouseRotate);
		rc.Mouse().AddListener(mouseZoom);
		rc.Mouse().AddListener(mouseRawPosition);

		vec2 nearFar = vec2(0.5f, 50.f);
		vec3 eyePos = vec3(-2.5f, -2.5f, 2.f);
		vec3 eyeTrg = vec3(0.f);
		vec3 up = vec3(0.f, 0.f, 1.f);

		float zoomDeltaX = 0.0f;
		float zoomDeltaY = 0.0f;
		float rotateDeltaX = 0.0f;
		float rotateDeltaY = 0.0f;

		time_point startTime = high_resolution_clock::now();

		InitGizmos(gizRdr);
		InitGizmosVC(gizRdrVC);

		gizRdr.SetSelectionCallback(onSelection);


		while (!rc.ShouldClose())
		{
            rc.PollEvents();

            StartImGuiFrame(pbrRdr);

            // zoom and rotation
			// ----------------------------------------------------------------------
			float newX = 0.0f;
			float newY = 0.0f;

			mouseRotate.PositionNormalized(newX, newY, true);
			MouseInputUpdate(mouseRotateData, newX, newY, rotateDeltaX, rotateDeltaY);

			mouseZoom.PositionNormalized(newX, newY, true);
			MouseInputUpdate(mouseZoomData, newX, newY, zoomDeltaX, zoomDeltaY);

			// rotation
			if (enableRotate)
			{
				mat4 rotZ = glm::rotate(mat4(1.0f), -2.f * pi<float>() * rotateDeltaX, up);
				eyePos = rotZ * vec4(eyePos, 1.0);

				vec3 right = cross(up, eyeTrg - eyePos);
					 right = normalize(right);

				mat4 rotX = glm::rotate(mat4(1.0f), -pi<float>() * rotateDeltaY, right);
				vec3 newEyePos = rotX * vec4(eyePos, 1.0);

				// ugly: limit the rotation so that eyePos-eyeTarget is never
				// parallel to world Z.
				if (abs(dot(normalize(newEyePos), up)) <= 0.98f)eyePos = newEyePos;
			}

			// zoom
			if (enableZoom)
			{
				float eyeDst = length(eyePos - eyeTrg) - zoomDeltaY * 20.0f;
					  eyeDst = glm::clamp(eyeDst, 1.0f, 20.0f);
					  eyePos = eyeTrg + eyeDst * normalize(eyePos - eyeTrg);
			}

            mat4 viewMatrix = glm::lookAt(eyePos, eyeTrg, up);
            mat4 projMatrix = glm::perspective(radians<float>(60), static_cast<float>(fboWidth) / fboHeight, nearFar.x, nearFar.y);
			// ----------------------------------------------------------------------

			time_point timeNow = high_resolution_clock::now();
			auto delta = duration_cast<milliseconds>(timeNow - startTime).count();
			//animation(delta);

            pbrRdr.Render(viewMatrix, projMatrix, nearFar.x, nearFar.y)
                .CopyTo(nullptr, fboWidth, fboHeight, fbo_copy_mask_color_bit);

			float mouseX, mouseY;
			mouseRawPosition.Position(mouseX, mouseY);

            //gizRdr.Render(viewMatrix, projMatrix, nearFar).CopyTo(nullptr, fboWidth, fboHeight, fbo_copy_mask_color_bit);
            //gizRdr.GetGizmoUnderCursor(mouseX, mouseY, viewMatrix, projMatrix, nearFar);

			mat4 viewMatrixVC = glm::lookAt(normalize(eyePos - eyeTrg)*3.5f, vec3{ 0.0f }, vec3{0.0, 0.0, 1.0});
			mat4 projMatrixVC = glm::perspective(radians<float>(60), static_cast<float>(fboWidthVC) / fboHeightVC, 0.1f, 5.0f);

            // View Cube drawing seems to conflict with ImGui,
            // so I had to place it here (ImGui is hidden by the VC however)
            EndImGuiFrame();

			gizRdrVC.Render(viewMatrixVC, projMatrixVC, vec2{0.1f, 3.0f})
				.CopyTo(
					nullptr,
					0, 0,
					fboWidthVC, fboHeightVC,
					fboWidth - fboWidthVC, fboHeight - fboHeightVC,
					fboWidth, fboHeight,
					fbo_copy_mask_color_bit, fbo_copy_filter_nearest
				);


			rc.SwapBuffers();
		}

        ImGuiShutdown();
	}
	catch (const exception& e)
	{
		cout << e.what() << endl;
	}

}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
