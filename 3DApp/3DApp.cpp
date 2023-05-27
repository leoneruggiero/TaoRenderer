// 3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include "RenderContext.h"
#include "GizmosRenderer.h"
#include "GizmosHelper.h"
#include "../../TestApp_OpenGL/TestApp_OpenGL/stb_image.h"

using namespace std;
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

vector<vec3> Circle(float radius, unsigned int subdv)
{
	float da = pi<float>()*2.0f/subdv;

	vector<vec3> pts(subdv + 1);

	for (unsigned int i = 0; i <= subdv; i++)
	pts[i] = vec3
	{
		cos(i * da),
		sin(i * da),
		1.0f
	} * radius;

	return pts;
}

void InitGrid(GizmosRenderer& gr)
{
	constexpr float width			= 10.0f;
	constexpr float height			= 10.0f;
	constexpr unsigned int subdvX	= 11;
	constexpr unsigned int subdvY	= 11;
	constexpr vec4 gridColor		= vec4{ 0.8, 0.8, 0.8, 0.5 };

	gr.CreateLineGizmo(1, line_gizmo_descriptor{ 2, nullptr });

	vector<line_gizmo_instance> instances{ subdvX + subdvY };

	for (unsigned int i = 0; i < subdvX; i++)
	{
		instances[i] = line_gizmo_instance
		{
			.start	= vec3{(-width / 2.0f) + (width / (subdvX-1)) * i , -height / 2.0f , -0.001f},
			.end	= vec3{(-width / 2.0f) + (width / (subdvX-1)) * i ,  height / 2.0f , -0.001f},
			.color  = gridColor
		};
	}

	for (unsigned int j = 0; j < subdvY; j++)
	{
		instances[j + subdvX] = line_gizmo_instance
		{
			.start = vec3{-width / 2.0f, (-height / 2.0f) + (height / (subdvY-1)) * j , -0.001f},
			.end   = vec3{ width / 2.0f, (-height / 2.0f) + (height / (subdvY-1)) * j , -0.001f},
			.color = gridColor
		};
	}

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

	gr.CreateLineGizmo(0, line_gizmo_descriptor
	{
		.line_size = lineWidth,
		.pattern_texture_descriptor = &patternDesc
	});

	gr.InstanceLineGizmo(0,
{
		line_gizmo_instance{{0,0,0}, {5,0,0}, {1, 0, 0, 1}}, // X axis (red)
		line_gizmo_instance{{0,0,0}, {0,5,0}, {0, 1, 0, 1}}, // Y axis (green)
		line_gizmo_instance{{0,0,0}, {0,0,5}, {0, 0, 1, 1}}, // Z axis (blue)
	});
}
void InitGizmos(GizmosRenderer& gr)
{
	InitWorldAxis(gr);
	InitGrid	 (gr);
}

int main()
{
	try
	{
		int windowWidth  = 1920;
		int windowHeight = 1080;
		int fboWidth	 = 0;
		int fboHeight	 = 0;

		RenderContext rc{ windowWidth, windowHeight };
		rc.GetFramebufferSize(fboWidth, fboHeight);
		GizmosRenderer gizRdr{rc, fboWidth, fboHeight};

		mouse_input_data mouseRotateData;
		mouse_input_data mouseZoomData;
		bool enableRotate	= false;
		bool enableZoom		= false;
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
			[&mouseZoomData, &enableZoom ]()
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

		vec2 nearFar	= vec2(0.001f, 20.f);
		vec3 eyePos		= vec3(-2.5f, -2.5f, 2.f);
		vec3 eyeTrg		= vec3(0.f);
		vec3 up			= vec3(0.f, 0.f, 1.f);
		mat4 viewMatrix = glm::lookAt(eyePos, eyeTrg, up);
		mat4 projMatrix = glm::perspective(radians<float>(60), static_cast<float>(fboWidth) / fboHeight, nearFar.x, nearFar.y);

		float zoomDeltaX	= 0.0f;
		float zoomDeltaY	= 0.0f;
		float rotateDeltaX	= 0.0f;
		float rotateDeltaY	= 0.0f;

		time_point startTime = high_resolution_clock::now();

		InitGizmos(gizRdr);

		// Add some line strips
		vector<vec3> strip = Circle(1.0f, 4);

		// dashed pattern
		unsigned int lineWidth  = 16;
		unsigned int patternLen = 32;
		tao_gizmos_procedural::TextureDataRgbaUnsignedByte patternTex
		{
			patternLen,
			lineWidth,
			tao_gizmos_procedural::TexelData<4,unsigned char>({0,0,0,0})
		};

		patternTex.FillWithFunction([lineWidth, patternLen](unsigned x, unsigned y)
			{
				float h = lineWidth / 2;
				float k = patternLen / 10;

			    // dash
				float v0 = tao_gizmos_sdf::SdfSegment(vec2(x, y), vec2(6 * k, h), vec2(9 * k, h));
				v0 = tao_gizmos_sdf::SdfInflate(v0, h / 2.0f);

			    // dot
				float v1 = tao_gizmos_sdf::SdfCircle(vec2(x, y), h/2.0f).Translate(vec2(2.5 * k, h));

				float v = tao_gizmos_sdf::SdfAdd(v0, v1);
				float a = glm::clamp(0.5f - v, 0.0f, 1.0f); // `anti aliasing`
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
			.vertices = &strip,
			.isLoop = true,
			.line_size = lineWidth,
			.pattern_texture_descriptor = &patternDesc
		});

		gizRdr.InstanceLineStripGizmo(0,
			{
				line_strip_gizmo_instance{translate(mat4(1.0f), {0.0, 0.0, 0.0}), vec4(1.0, 0.0, 0.25, 1.0)},
				line_strip_gizmo_instance{translate(mat4(1.0f), {0.5, 0.0, 0.2}), vec4(1.0, 0.0, 0.25, 1.0)},
				line_strip_gizmo_instance{translate(mat4(1.0f), {1.0, 0.0, 0.4}), vec4(1.0, 0.0, 0.25, 1.0)},
				line_strip_gizmo_instance{translate(mat4(1.0f), {1.5, 0.0, 0.6}), vec4(1.0, 0.0, 0.25, 1.0)},
				line_strip_gizmo_instance{translate(mat4(1.0f), {2.0, 0.0, 0.8}), vec4(1.0, 0.0, 0.25, 1.0)},
				line_strip_gizmo_instance{translate(mat4(1.0f), {2.5, 0.0, 1.0}), vec4(1.0, 0.0, 0.25, 1.0)},
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
