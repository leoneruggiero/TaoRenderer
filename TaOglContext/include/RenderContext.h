#pragma once

#define GFX_DEBUG
#ifdef  GFX_DEBUG
    #define GFX_DEBUG_OUTPUT_ENABLED
#endif

#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include "Resources.h"
//#include "RenderContextUtils.h"
#include "Input.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <functional>

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

        int _uniformBufferOffsetAlignment;

        static void SetInputOptions(GLFWwindow* window)
        {
            // TODO: do we really need it?
            //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            //
            //if (glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            //else                                throw exception("Raw mouse input is not available.");
        }

#ifdef GFX_DEBUG_OUTPUT_ENABLED

        std::ostream & _debugOutputStream = std::cout;

        void glDebugOutput(GLenum source,
                                    GLenum type,
                                    unsigned int id,
                                    GLenum severity,
                                    GLsizei length,
                                    const char *message,
                                    const void *userParam) const;

        void InitDebugOutput(GLFWwindow* window);

        static void ConfigureDebugOutput(ogl_debug_output_filter filter)
        {
            // TODO: configure level and severity
            glDebugMessageControl(filter.source,
                                  filter.type,
                                  filter.severity,
                                  0, nullptr, GL_TRUE);
        }
#endif

        void InitGlInfo();

    public:
        RenderContext(int windowWidth, int windowHeight, const char* windowName = "Unnamed Window") :
    	_windowWidth(windowWidth),
    	_windowHeight(windowHeight)
        {
            glfwInit();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CONTEXT_VER_MAJOR);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CONTEXT_VER_MINOR);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef GFX_DEBUG_OUTPUT_ENABLED
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
            /// Window initialization
            //////////////////////////////
            _glf_window = glfwCreateWindow(windowWidth, windowHeight, windowName, nullptr, nullptr);
            if (_glf_window == nullptr)
            {
                glfwTerminate();
                throw std::runtime_error("RenderContext initialization failed.");
            }

            glfwMakeContextCurrent(_glf_window);

            // disable v-sync
            glfwSwapInterval(0);

            /// Mouse input initialization
            ///////////////////////////////
            _mouseInput = make_unique<MouseInput>(_glf_window);
            SetInputOptions(_glf_window);


            /// GLAD loading
            //////////////////////////////
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
                throw std::runtime_error("RenderContext failed to load GL.");

            /// Ogl version check
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

                throw std::runtime_error(wrongVersionMessage.str().c_str());

            }

            /// Init useful Ogl info
            ///////////////////////////////
            InitGlInfo();

#ifdef GFX_DEBUG_OUTPUT_ENABLED
            /// Debug output
            ///////////////////////////////
            InitDebugOutput(_glf_window);
            ConfigureDebugOutput(ogl_debug_output_filter_severe);
#endif

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

        void SetViewport(GLint x, GLint y, GLsizei width, GLsizei height);

        void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, ogl_read_pixels_format format, ogl_texture_data_type type, void* data);

        void ReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, ogl_read_pixels_format format, ogl_texture_data_type type, GLsizei bufSize, void* data);

        void DrawArrays           (ogl_primitive_type mode, GLint first, GLsizei count);
        void DrawArraysInstanced  (ogl_primitive_type mode, GLint first, GLsizei count, GLsizei instanceCount);
        void DrawElements         (ogl_primitive_type mode, GLsizei count, ogl_indices_type type, const void* indices);
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
    	[[nodiscard]] OglVertexAttribArray CreateVertexAttribArray(const vector<pair<OglVertexBuffer&, vertex_buffer_layout_desc>> vertexDataSrc);

        [[nodiscard]] OglTexture2D  CreateTexture2D();

        [[nodiscard]] OglTexture2DMultisample  CreateTexture2DMultisample();

        [[nodiscard]] OglSampler CreateSampler();
        [[nodiscard]] OglSampler CreateSampler(ogl_sampler_params params);

        template<typename Tex> requires ogl_texture<typename Tex::ogl_resource_type>
        [[nodiscard]] OglFramebuffer<Tex> CreateFramebuffer();

        [[nodiscard]] OglUniformBuffer CreateUniformBuffer();

        [[nodiscard]] OglIndexBuffer CreateIndexBuffer();

        [[nodiscard]] OglShaderStorageBuffer CreateShaderStorageBuffer();

    	[[nodiscard]] OglPixelPackBuffer CreatePixelPackBuffer();

        [[nodiscard]] OglFence CreateFence();

        int UniformBufferOffsetAlignment() const {return _uniformBufferOffsetAlignment;};
    };
}

