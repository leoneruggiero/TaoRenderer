// 3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include "RenderContext.h"
#include "GizmosRenderer.h"


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

int main()
{
	try
	{
		int width = 1920;
		int height = 1080;
		RenderContext rc{width, height};
		GizmosRenderer gizRdr{rc, width, height};

		mouse_input_data mouseRotateData;
		mouse_input_data mouseZoomData;
		bool enableRotate = false;
		bool enableZoom = false;
		MouseInputListener mouseRotate{
			[&rc]()
			{
				return
					rc.Mouse().IsPressed(middle_mouse_button) &&
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
			120.0f
		};
		MouseInputListener mouseZoom{
			[&rc]()
			{
				return
					rc.Mouse().IsPressed(middle_mouse_button) &&
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
			120.0f
		};
		rc.Mouse().AddListener(mouseRotate);
		rc.Mouse().AddListener(mouseZoom);

		vec3 eyePos = vec3(0.f, -5.f, 5.f);
		vec3 eyeTrg = vec3(0.f);
		vec3 up = vec3(0.f, 0.f, 1.f);
		mat4 viewMatrix = glm::lookAt(eyePos, eyeTrg, up);
		mat4 projMatrix = glm::perspective(radians<float>(60), static_cast<float>(width) / height, 0.01f, 20.f);

		float zoomDeltaX = 0.0f;
		float zoomDeltaY = 0.0f;
		float rotateDeltaX = 0.0f;
		float rotateDeltaY = 0.0f;

		time_point startTime = high_resolution_clock::now();

		constexpr int xSub		= 50;
		constexpr int ySub		= 50;
		constexpr float scale	= 6.0f;
		constexpr float speed	= 0.005f;
		constexpr float sinFreq = 3.0f;
		constexpr float sinAmpl = 0.25f;
		vector<point_gizmo_instance> ptsInstances{ xSub * ySub };

		float image[] =
		{
			1.0f, 0.0f, 0.0f, 1.0f,
			0.0f, 1.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f
		};

		symbol_atlas_descriptor desc =
		{
			.symbol_atlas_data			= &image,
			.symbol_atlas_data_format	= tex_for_rgba,
			.symbol_atlas_data_type		= tex_typ_float,
			.symbol_atlas_width			= 2,
			.symbol_atlas_height		= 2,
			.symbol_atlas_mapping		= vector{symbol_mapping{vec2(0.0f, 0.0f), vec2(1.0f, 1.0f)}},
			.symbol_filter_smooth		= false
		};

		gizRdr.CreatePointGizmo(1, point_gizmo_descriptor
			{
				.point_size = 25.0f,
				.pixel_snapping = false,
				.symbol_atlas_descriptor = nullptr // &desc
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

			// update time in milliseconds
			float offset =
				duration<double, std::milli>(speed * (high_resolution_clock::now() - startTime)).count();

			for (int i = 0; i < xSub; i++)
				for (int j = 0; j < ySub; j++)
				{
					const float xPos = static_cast<float>(i) / xSub * scale - scale * 0.5f;
					const float yPos = static_cast<float>(j) / ySub * scale - scale * 0.5f;

					const float dst =  sqrt(pow(xPos, 2.f) + pow(yPos, 2.f));
					const float func = sinAmpl * sin(sinFreq * dst - offset);
					const vec4  color = Gradient(func / sinAmpl * 0.5f + 0.5f,
						vec4(0.1f, 0.f, 0.3f, 1.f),
						vec4(0.2f, 0.5f, 0.5f, 1.f),
						vec4(1.f, 0.f, 0.6f, 1.0f)
					);
					ptsInstances[i * ySub + j] = point_gizmo_instance
					{
						.position = vec3(xPos, yPos, func),
						.color = color,
						.symbol_index = 0
					};
				}

			gizRdr.InstancePointGizmo(1, ptsInstances);
			gizRdr.Render(viewMatrix, projMatrix);

#if DEBUG_MOUSE
			float mmbX, mmbY, mmbXsmooth, mmbYsmooth;
			float lmbX, lmbY, lmbXsmooth, lmbYsmooth;

			mouseMMB.PositionNormalized(mmbX, mmbY);
			mouseMMB.PositionNormalized(mmbXsmooth, mmbYsmooth, true);
			mouseRMB.PositionNormalized(lmbX, lmbY);
			mouseRMB.PositionNormalized(lmbXsmooth, lmbYsmooth, true);

			gizRdr.InstancePointGizmo(1,
				{
				point_gizmo_instance
				{
					.position = vec3(mmbX, mmbY, 0.0f) * 2.f - 1.f,
					.color = vec4(0.f, 0.f, 1.f, 1.f)
				},
				point_gizmo_instance
				{
					.position = vec3(mmbXsmooth, mmbYsmooth, 0.0f) * 2.f - 1.f,
					.color = vec4(1.f, 0.f, 0.f, 1.f)
				},
				point_gizmo_instance
				{
					.position = vec3(lmbX, lmbY, 0.0f) * 2.f - 1.f,
					.color = vec4(1.f, 0.f, 0.f, 1.f)
				} ,
				point_gizmo_instance
				{
					.position = vec3(lmbXsmooth, lmbYsmooth, 0.0f) * 2.f - 1.f,
					.color = vec4(0.f, 1.f, 0.f, 1.f)
				}
				});

			gizRdr.Render(mat4(1.0), mat4(1.0));
#endif

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
