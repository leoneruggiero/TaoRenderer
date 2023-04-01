#pragma once
#include <iostream>
#include "Resources.h"
#include <GLFW/glfw3.h>

namespace render_context
{

    using namespace ogl_resources;

    class RenderContext
    {
    public:
        RenderContext(GLFWwindow* window)
        {
            glfwMakeContextCurrent(window);

            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                std::cout << "Failed to initialize GLAD" << std::endl;
                throw "";
            }

        }

        [[nodiscard]] std::shared_ptr<OglVertexShader>   CreateVertexShader();
        [[nodiscard]] std::shared_ptr<OglVertexShader>   CreateVertexShader(const char* source);

        [[nodiscard]] std::shared_ptr<OglFragmentShader> CreateFragmentShader();
        [[nodiscard]] std::shared_ptr<OglFragmentShader> CreateFragmentShader(const char* source);

        [[nodiscard]] std::shared_ptr<OglGeometryShader> CreateGeometryShader();
        [[nodiscard]] std::shared_ptr<OglGeometryShader> CreateGeometryShader(const char* source);

        [[nodiscard]] std::shared_ptr<OglShaderProgram> CreateShaderProgram();
        [[nodiscard]] std::shared_ptr<OglShaderProgram> CreateShaderProgram(const char* vertSource, const char* fragSource);
        [[nodiscard]] std::shared_ptr<OglShaderProgram> CreateShaderProgram(const char* vertSource, const char* geomSource, const char* fragSource);

    };
}

