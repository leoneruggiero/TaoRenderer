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
#include "../../TestApp_OpenGL/TestApp_OpenGL/stb_image.h"
#include "3DApp.h"

using namespace std;
using namespace glm;
using namespace std::chrono;
using namespace tao_ogl_resources;
using namespace tao_render_context;
using namespace tao_gizmos;
using namespace tao_input;


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

void InitGrid(GizmosRenderer& gr)
{
	constexpr float width			= 10.0f;
	constexpr float height			= 10.0f;
	constexpr unsigned int subdvX	= 51;
	constexpr unsigned int subdvY	= 51;
	constexpr vec4 colDark			= vec4{ 0.7, 0.7, 0.7, 1.0 };
	constexpr vec4 colLight			= vec4{ 0.4, 0.4, 0.4, 1.0 };

	
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
	gr.CreateLineGizmo(1, line_list_gizmo_descriptor{
		.vertices = vertices,
		.line_size = 1,
		.pattern_texture_descriptor = nullptr
	});

	// a single instance
	vector<line_list_gizmo_instance> instances
	{
	line_list_gizmo_instance
		{
		.transformation = glm::mat4{1.0f},
		.color = vec4{1.0f}
		}
	};
	gr.InstanceLineGizmo(1, instances);
}
void InitWorldAxis(GizmosRenderer& gr)
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

	//gr.CreateLineGizmo(0, line_list_gizmo_descriptor
	//{
	//	.vertices = 
	//	.line_size = lineWidth,
	//	.pattern_texture_descriptor = nullptr//&patternDesc
	//});
	//
	//gr.InstanceLineGizmo(0,
//{
	//	line_list_gizmo_instance{mat4{1.0f},vec4{1, 0, 0, 1}}, // X axis (red)
	//	line_list_gizmo_instance{mat4{1.0f},vec4{0, 1, 0, 1}}, // Y axis (green)
	//	line_list_gizmo_instance{mat4{1.0f},vec4{0, 0, 1, 1}}, // Z axis (blue)
	//});
}


void InitArrowTriad2D(tao_gizmos::GizmosRenderer& gizRdr)
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

	// X Axis
	///////////////////////////////////
	axisVertsX.push_back(LineGizmoVertex{}.Position({ 0.0f	 , 0.0f, 0.0f }).Color(whi));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen, 0.0f, 0.0f }).Color(red));


	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen, 0.0f,  arrowWidth * 0.5f }).Color(red));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen, 0.0f, -arrowWidth * 0.5f }).Color(red));

	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen			 ,0.0f, arrowWidth * 0.5f }).Color(red));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen + arrowDepth,0.0f			  , 0.0f }).Color(red));

	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen			 , 0.0f, -arrowWidth * 0.5f }).Color({ red }));
	axisVertsX.push_back(LineGizmoVertex{}.Position({ axisLen + arrowDepth, 0.0f, 0.0f }).Color({ red }));

	// Y Axis
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

	gizRdr.CreateLineGizmo(2, line_list_gizmo_descriptor
	{
		.vertices					= verts,
		.line_size					= 3,
		.zoom_invariant				= true,
		.zoom_invariant_scale		= 0.1f,
		.pattern_texture_descriptor = nullptr
	});

	gizRdr.InstanceLineGizmo(2,
{
		line_list_gizmo_instance{ translate(mat4{ 1.0f }, vec3{ 1.0,-1.0, 0.25 }), vec4(1.0f) },
		line_list_gizmo_instance{ translate(mat4{ 1.0f }, vec3{-1.0, 1.0, 0.25 }), vec4(1.0f) }
		});
}



void CreateArrowTriad(
	vector   <glm::vec3>& positions,
	vector   <glm::vec3>& normals,
	vector   <glm::vec4>& colors,
	vector<unsigned int>& triangles)
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
	triangles = vector<unsigned int>(x.NumIndices() + y.NumIndices() + z.NumIndices());

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
	auto yTri = y.GetIndices(); std::for_each(yTri.begin(), yTri.end(), [xVertCount](unsigned int& i) {i += xVertCount; });
	auto zTri = z.GetIndices(); std::for_each(zTri.begin(), zTri.end(), [xVertCount, yVertCount](unsigned int& i) {i += xVertCount + yVertCount; });

	std::copy(xTri.begin(), xTri.end(), triangles.begin());
	std::copy(yTri.begin(), yTri.end(), triangles.begin() + xTri.size());
	std::copy(zTri.begin(), zTri.end(), triangles.begin() + xTri.size() + yTri.size());

	// Color
	// ------------------------------------
	std::generate(colors.begin(), colors.begin() + xVertCount, []() { return glm::vec4(1.0, 0.0, 0.0, 1.0); });
	std::generate(colors.begin() + xVertCount, colors.begin() + xVertCount + yVertCount, []() { return glm::vec4(0.0, 1.0, 0.0, 1.0); });
	std::generate(colors.begin() + xVertCount + yVertCount, colors.end(), []() { return glm::vec4(0.0, 0.0, 1.0, 1.0); });

}

void InitArrowTriad3D(tao_gizmos::GizmosRenderer& gizRdr)
{
	vector<glm::vec3> cubeMeshPos;
	vector<glm::vec3> cubeMeshNrm;
	vector<glm::vec2> cubeMeshTex;
	vector<glm::vec4> cubeMeshCol;
	vector<unsigned int> cubeMeshTris;

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

	gizRdr.CreateMeshGizmo(0, mesh_gizmo_descriptor
		{
			.vertices = cubeMeshVertices,
			.triangles = &cubeMeshTris,
			.zoom_invariant = true,
			.zoom_invariant_scale = 0.1f
		});

	gizRdr.InstanceMeshGizmo(0,
		{
			mesh_gizmo_instance{ translate(mat4{ 1.0f }, vec3{-1.0,-1.0, 0.25 }), vec4(1.0f) },
			mesh_gizmo_instance{ translate(mat4{ 1.0f }, vec3{ 1.0, 1.0, 0.25 }), vec4(1.0f) },
		});
}

void InitGizmos(GizmosRenderer& gr)
{
	InitWorldAxis		(gr);
	InitGrid			(gr);
	InitArrowTriad3D	(gr);
	InitArrowTriad2D	(gr);
}

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
		GizmosRenderer gizRdr{ rc, fboWidth, fboHeight };

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

		rc.Mouse().AddListener(mouseRotate);
		rc.Mouse().AddListener(mouseZoom);

		vec2 nearFar = vec2(0.5f, 50.f);
		vec3 eyePos = vec3(-2.5f, -2.5f, 2.f);
		vec3 eyeTrg = vec3(0.f);
		vec3 up = vec3(0.f, 0.f, 1.f);
		mat4 viewMatrix = glm::lookAt(eyePos, eyeTrg, up);
		mat4 projMatrix = glm::perspective(radians<float>(60), static_cast<float>(fboWidth) / fboHeight, nearFar.x, nearFar.y);

		float zoomDeltaX = 0.0f;
		float zoomDeltaY = 0.0f;
		float rotateDeltaX = 0.0f;
		float rotateDeltaY = 0.0f;

		time_point startTime = high_resolution_clock::now();

		InitGizmos(gizRdr);

		// Add some line strips
		vector<LineGizmoVertex> strip = Circle(1.0f, 64);
		for (int i = 0; i < strip.size(); i++)
		{
			strip[i].Color(vec4{ static_cast<float>(i) / strip.size(), 0.0f, 0.7f, 1.0f });
		}


		// dashed pattern
		unsigned int lineWidth  = 6;
		unsigned int patternLen = 32;
		tao_gizmos_procedural::TextureDataRgbaUnsignedByte patternTex
		{
			patternLen,
			lineWidth,
			tao_gizmos_procedural::TexelData<4,unsigned char>({0,0,0,0})
		};

		patternTex.FillWithFunction([lineWidth, patternLen](unsigned x, unsigned y)
		{
			float h = lineWidth  / 2.0f;
			float k = patternLen / 10.0f;

			// dash
			float v0 = tao_gizmos_sdf::SdfSegment(vec2(x, y), vec2(3 * k, h), vec2(7 * k, h));
			v0 = tao_gizmos_sdf::SdfInflate(v0, h);
			
			float a = glm::clamp(0.5f - v0, 0.0f, 1.0f); // `anti aliasing`
			return vec<4, unsigned char>{255, 255, 255, static_cast<unsigned char>(a * 255)};
		});

		pattern_texture_descriptor patternDesc
		{
			.data = patternTex.DataPtr(),
			.data_format = tex_for_rgba,
			.data_type = tex_typ_unsigned_byte,
			.width = patternLen,
			.height = lineWidth,
			.pattern_length = patternLen
		};

		gizRdr.CreateLineStripGizmo(0, line_strip_gizmo_descriptor
			{
				.vertices = strip,
				.isLoop = true,
				.line_size = lineWidth,
				.pattern_texture_descriptor = &patternDesc
			});

		gizRdr.InstanceLineStripGizmo(0,
			{
				line_strip_gizmo_instance{rotate(translate(mat4{1}, vec3{2.0, 2.0, 2.0}),0.0f              , vec3(1, 0, 0)), vec4(0.95, 0.85, 0.9, 1.0)},
				line_strip_gizmo_instance{rotate(translate(mat4{1}, vec3{2.0, 2.0, 2.0}),0.5f * pi<float>(), vec3(1, 0, 0)), vec4(0.95, 0.85, 0.9, 1.0)},
				line_strip_gizmo_instance{rotate(translate(mat4{1}, vec3{2.0, 2.0, 2.0}),0.5f * pi<float>(), vec3(0, 1, 0)), vec4(0.95, 0.85, 0.9, 1.0)},
			});



		while (!rc.ShouldClose())
		{
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

			viewMatrix = glm::lookAt(eyePos, eyeTrg, up);
			// ----------------------------------------------------------------------

			
			gizRdr.Render(viewMatrix, projMatrix, nearFar).CopyTo(nullptr, fboWidth, fboHeight, fbo_copy_mask_color_bit);

			rc.SwapBuffers();
			rc.PollEvents();

		}
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
