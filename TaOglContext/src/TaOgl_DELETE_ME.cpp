// TaOgl.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include "RenderContext.h"

using namespace render_context;
using namespace std;


struct MeshRenderer
{
	vector<shared_ptr<OglVertexBuffer>> vertexBuffers;
	shared_ptr<OglVertexAttribArray> vertexAttribArray;
};

/**
 * \brief Creates a MeshRenderer.
 * \param rc The render context.
 * \param positions The vertices positions.
 * \param normals The vertices normals.
 * \param textureCoordinates The vertices texture coordinates.
 * \param colors The vertices colors.
 * \return The MeshRenderer containing the provided vertex attributes.
 */
MeshRenderer CreateMeshRenderer(
	RenderContext& rc,
	const vector<float>& positions,
	const vector<float>& normals,
	const vector<float>& textureCoordinates,
	const vector<float>& colors)
{
	int vCount = positions.size() / 3;

	// interleaved attribs: PPP0-NNN0-PPP1-NNN1-...
	vector<float> posNrm(positions.size()* 2);

	for (int i = 0, j = 0; i < vCount * 3;/**/)
	{
		posNrm[i + j] = positions[i];i++;
		posNrm[i + j] = positions[i];i++;
		posNrm[i + j] = positions[i];i++;
		posNrm[i + j] = normals[j];j++;
		posNrm[i + j] = normals[j];j++;
		posNrm[i + j] = normals[j];j++;
	}

	// creating VBOs ////////////////////////
	auto posNrmVbo = rc.CreateVertexBuffer(posNrm.data(), sizeof(float) * 6 * vCount);				// VBO 1 -> Positions and Normals
	auto texCoorVbo = rc.CreateVertexBuffer(textureCoordinates.data(), sizeof(float) * 2 * vCount);	// VBO 2 -> TextureCoordinates
	auto colorsVbo = rc.CreateVertexBuffer(colors.data(), sizeof(float) * 4 * vCount);				// VBO 3 -> Color 

	// creating VAO  ////////////////////////
	vertex_attribute_desc vboDsc_posNrm[] = {
		vertex_attribute_desc{
			.index = 0,
			.element_count = 3,
			.offset = 0,
			.stride = sizeof(float) * 6,
			.element_type = vao_typ_float
		},
		vertex_attribute_desc{
			.index = 1,
			.element_count = 3,
			.offset = sizeof(float) * 3,
			.stride = sizeof(float) * 6,
			.element_type = vao_typ_float
		}
	};
	vertex_buffer_layout_desc vboLayout_posNrm{ .attrib_count = 2, .attrib_descriptors = vboDsc_posNrm };

	vertex_attribute_desc  vboDsc_tex{
		.index = 2,
		.element_count = 2,
		.offset = 0,
		.stride = 0,
		.element_type = vao_typ_float
	};
	vertex_buffer_layout_desc vboLayout_tex{ .attrib_count = 1, .attrib_descriptors = &vboDsc_tex };

	vertex_attribute_desc  vboDsc_cols{
		.index = 3,
		.element_count = 4,
		.offset = 0,
		.stride = 0,
		.element_type = vao_typ_float
	};
	vertex_buffer_layout_desc vboLayout_cols{ .attrib_count = 1, .attrib_descriptors = &vboDsc_cols };

	auto vao = rc.CreateVertexAttribArray({
		{ posNrmVbo,vboLayout_posNrm},
		{texCoorVbo,vboLayout_tex },
		{ colorsVbo, vboLayout_cols }
		});

	return MeshRenderer{
		.vertexBuffers = {posNrmVbo, texCoorVbo, colorsVbo},
		.vertexAttribArray = vao
	};
}

void GetVertexData(vector<float>& positions, vector<float>& normals, vector<float>& texCoords, vector<float>& colors)
{
	positions=
	{
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f
	};
	normals=
	{
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f
	};
	texCoords =
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
	colors =
	{
		1.0f, 0.3f, 0.1f, 1.0f,
		0.2f, 1.0f, 0.2f, 1.0f,
		0.2f, 0.2f, 1.0f, 1.0f
	};
}

shared_ptr<OglShaderProgram> GetShader(RenderContext& rc)
{
	// Vertex shader
	////////////////////////////////////////////
	const char* vs =
		R"(
	        #version 330 core

			layout(location = 0) in vec3 pos;
			layout(location = 1) in vec3 nrm;
			layout(location = 2) in vec2 tex;
			layout(location = 3) in vec4 col;
			    
			struct VS_OUT
			{
				vec3 nrm;
				vec2 tex;
				vec4 col;
			};

			out VS_OUT vs_out;

	        uniform mat4 m_model;             
			uniform mat4 m_view;
			uniform mat4 m_proj;
	        void main()
	        {
	            gl_Position = m_proj*m_view*m_model * vec4(pos, 1.0);

				vs_out.nrm = nrm;
				vs_out.tex = tex;
				vs_out.col = col;
	        }
    )";


	// Fragment shader
	////////////////////////////////////////////
	const char* fs =
		R"(
			#version 330 core

			struct VS_OUT
			{
				vec3 nrm;
				vec2 tex;
				vec4 col;
			};

			in VS_OUT vs_out;

			out vec4 out_fragColor;

			void main()
			{
				out_fragColor = vs_out.col;
			}
)";


	// Shader program
	////////////////////////////////////////////
	return rc.CreateShaderProgram(vs, fs);
}

void GetCameraMatrices(vector<float>& viewMat, vector<float>& projMat, float winW, float winH)
{

	viewMat =
	{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f,-1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
	};

	float scaleW = 5.0f;
	float scaleH = scaleW * (winH / winW);

	projMat = 
	{
		1.0f/scaleW, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f/scaleH, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

void GetModelMatrix(vector<float>& modelMat)
{
	modelMat =
	{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
	};
}

int main()
{
	try 
	{
		int windowW = 1000;
		int windowH = 600;
		RenderContext renderContext{ windowW, windowH, "MyWindow" };
		
		vector<float> vPos, vNrm, vTex, vCol, modelMat, projMat, viewMat;

		GetVertexData(vPos, vNrm, vTex, vCol);
		GetCameraMatrices(viewMat, projMat, windowW, windowH);
		GetModelMatrix(modelMat);

		auto mesh = CreateMeshRenderer(renderContext, vPos, vNrm, vTex, vCol);
		auto shader = GetShader(renderContext);

		while (!renderContext.ShouldClose())
		{
			renderContext.ClearColor(0.2f, 0.1f, 0.2f);
			renderContext.ClearDepthStencil();

			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);

			shader->UseProgram();
			shader->SetUniformMatrix4("m_model", modelMat.data());
			shader->SetUniformMatrix4("m_view", viewMat.data());
			shader->SetUniformMatrix4("m_proj", projMat.data());

			mesh.vertexAttribArray->Bind();
			glDrawArrays(GL_TRIANGLES, 0, 3);

			renderContext.PollEvents();
			renderContext.SwapBuffers();
		}

		return 0;
	}
	catch (exception& ex)
	{
		cout << ex.what() << std::endl;
		return -1;
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
