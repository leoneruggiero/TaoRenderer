#include "GizmosShaderLib.h"
#include<iostream>
#include <sstream>
#include <string>
using namespace std;

int main()
{
	try
	{
		render_context::RenderContext rc{ 800, 800 };
		auto prog=gizmos_shader_library::GizmosShaderLib::CreateShaderProgram(
			rc,
			gizmos_shader_library::gizmos_shader_type::lineStrip,
			"C:\Users\Admin\Documents\LearnOpenGL\TestApp_OpenGL\TestApp_OpenGL\Shaders\Forward");

	}
	catch(const exception& e)
	{
		cout << e.what() << endl;
	}
}