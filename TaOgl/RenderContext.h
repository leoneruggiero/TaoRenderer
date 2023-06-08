#pragma once
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include "Resources.h"
#include "RenderContextUtils.h"
#include "Input.h"
#include <GLFW/glfw3.h>


namespace tao_render_context
{
    using namespace tao_ogl_resources;
    using namespace tao_input;
    using namespace  std;

    class RenderContext
    {

    private:
        static constexpr int CONTEXT_VER_MAJOR = 4;
        static constexpr int CONTEXT_VER_MINOR = 5;

        int _windowWidth;
        int _windowHeight;
        GLFWwindow* _glf_window;
        unique_ptr<MouseInput>  _mouseInput;

        static void SetInputOptions(GLFWwindow* window)
        {
            // TODO: do we really need it?
            //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            //
            //if (glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            //else                                throw exception("Raw mouse input is not available.");
        }

    public:
        RenderContext(int windowWidth, int windowHeight, const char* windowName = "Unnamed Window") :
    	_windowWidth(windowWidth),
    	_windowHeight(windowHeight)
        {
            glfwInit();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CONTEXT_VER_MAJOR);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CONTEXT_VER_MINOR);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            // Window initialization
            //////////////////////////////
            _glf_window = glfwCreateWindow(windowWidth, windowHeight, windowName, nullptr, nullptr);
            if (_glf_window == nullptr)
            {
                glfwTerminate();
                throw std::exception("RenderContext initialization failed.");
            }

            // Mouse input initialization
            ///////////////////////////////
            _mouseInput = make_unique<MouseInput>(_glf_window);
            SetInputOptions(_glf_window);

            glfwMakeContextCurrent(_glf_window);

            // GLAD loading
            //////////////////////////////
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
                throw std::exception("RenderContext failed to load GL.");

            // Ogl version check
        	//////////////////////////////
            const char* versionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            // version string is MAJ.MIN.(release, optional)
            const int contextVerMaj = versionString[0] - '0';
            const int contextVerMin = versionString[2] - '0';
            if (contextVerMaj < CONTEXT_VER_MAJOR || contextVerMin < CONTEXT_VER_MINOR)
            {
                ostringstream wrongVersionMessage{};
                wrongVersionMessage <<
                    "RenderContext initialization failed: minimum OpenGL version required is "<< CONTEXT_VER_MAJOR << "." << CONTEXT_VER_MINOR <<
                    " while current version is "<< contextVerMaj << "." << contextVerMin;

                throw std::exception(wrongVersionMessage.str().c_str());

            }
        }
        ~RenderContext()
        {
	        glfwTerminate();
        }

        void GetFramebufferSize(int& width, int& height)  const  { glfwGetFramebufferSize(_glf_window, &width, &height); }
        void MakeCurrent()                                       { glfwMakeContextCurrent(_glf_window); }
        bool ShouldClose()                                const  { return glfwWindowShouldClose(_glf_window) != 0; }
        void PollEvents()                                 const  { glfwPollEvents(); Mouse().Poll(); }
        MouseInput& Mouse()                               const  { return *_mouseInput; }
        void SwapBuffers()                                const  { glfwSwapBuffers(_glf_window); }

        void ClearColor(float red, float green, float blue, float alpha = 1.0f);
        void ClearDepth(float value = 1.0f);
        void ClearStencil(int value = 0);
        void ClearDepthStencil(double depth_val = 1.0f, int stencil_val = 0);

        void SetDepthState(ogl_depth_state state);

        void SetBlendState(ogl_blend_state state);

        void SetRasterizerState(ogl_rasterizer_state state);

        void DrawArrays           (ogl_primitive_type mode, GLint first, GLsizei count);
        void DrawArraysInstanced  (ogl_primitive_type mode, GLint first, GLsizei count, GLsizei instanceCount);
        void DrawElementsInstanced(ogl_primitive_type mode, GLsizei count, ogl_indices_type type, const void* offset, GLsizei instanceCount);

        [[nodiscard]] OglVertexShader   CreateVertexShader();
        [[nodiscard]] OglVertexShader   CreateVertexShader(const char* source);

        [[nodiscard]] OglFragmentShader CreateFragmentShader();
        [[nodiscard]] OglFragmentShader CreateFragmentShader(const char* source);

        [[nodiscard]] OglGeometryShader CreateGeometryShader();
        [[nodiscard]] OglGeometryShader CreateGeometryShader(const char* source);

        [[nodiscard]] OglShaderProgram CreateShaderProgram();
        [[nodiscard]] OglShaderProgram CreateShaderProgram(const char* vertSource, const char* fragSource);
        [[nodiscard]] OglShaderProgram CreateShaderProgram(const char* vertSource, const char* geomSource, const char* fragSource);

        [[nodiscard]] OglVertexBuffer CreateVertexBuffer();
        [[nodiscard]] OglVertexBuffer CreateVertexBuffer(const void* data, int size, ogl_buffer_usage usage = ogl_buffer_usage::buf_usg_static_draw);

        [[nodiscard]] OglVertexAttribArray CreateVertexAttribArray();
    	[[nodiscard]] OglVertexAttribArray CreateVertexAttribArray(const vector<pair<shared_ptr<OglVertexBuffer>, vertex_buffer_layout_desc>> vertexDataSrc);

        [[nodiscard]] OglTexture2D  CreateTexture2D();

        [[nodiscard]] OglTexture2DMultisample  CreateTexture2DMultisample();

        [[nodiscard]] OglSampler CreateSampler();
        [[nodiscard]] OglSampler CreateSampler(ogl_sampler_params params);

        template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
        [[nodiscard]] OglFramebuffer<Tex> CreateFramebuffer();

        [[nodiscard]] OglUniformBuffer CreateUniformBuffer();

        [[nodiscard]] OglIndexBuffer CreateIndexBuffer();

        [[nodiscard]] OglShaderStorageBuffer CreateShaderStorageBuffer();
    };
}

