// 3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include <thread>
#include <variant>
#include <regex>

#include "RenderContext.h"
#include "GizmosRenderer.h"
#include "GizmosHelper.h"
#include "PbrRenderer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include "TaOglAppConfig.h"
#include "TaoScene.h"

using namespace std;
using namespace glm;
using namespace std::chrono;
using namespace tao_ogl_resources;
using namespace tao_math;
using namespace tao_render_context;
using namespace tao_gizmos_procedural;
using namespace tao_gizmos_sdf;
using namespace tao_gizmos;
using namespace tao_gizmos_shader_graph;
using namespace tao_pbr;
using namespace tao_instrument;
using namespace tao_scene;

// ImGui helper functions straight from the docs
// ----------------------------------------------
void InitImGui(GLFWwindow *window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // IF using Docking Branch

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window,
                                 true);   // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();
}

void StartImGuiFrame(tao_scene::TaoScene &scene)
{
    // (Your code calls glfwPollEvents())
    // ...
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::ShowDemoWindow(); // Show demo window! :)

    ImGui::Begin("Controls");
    ImGui::Text("Rotate: RMB \nZoom  : RMB + left shift\nPan   : RMB + left ctrl");
    ImGui::Text("LMB to pick and manipulate lights");
    ImGui::End();

    if (ImGui::Button("Reload shaders"))
    {
        try
        {
            scene.GetPbrRenderer().ReloadShaders();
        }
        catch (std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    if (ImGui::BeginCombo("Environment", scene.GetEnvironmentName(scene.GetCurrentEnvironment()).c_str()))
    {
        for (int n = 0; n < scene.EnvironmentsCount(); n++)
        {
            bool is_selected = (n == scene.GetCurrentEnvironment());
            if (ImGui::Selectable(scene.GetEnvironmentName(n).c_str(), is_selected))
                scene.SetEnvironment(n);

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::RadioButton("Translate", scene.GetCurrentTmMode() == TransformManipulator::TmMode::Translate))
        scene.SetTmMode(TransformManipulator::TmMode::Translate);
    if (ImGui::RadioButton("Rotate", scene.GetCurrentTmMode() == TransformManipulator::TmMode::Rotate))
        scene.SetTmMode(TransformManipulator::TmMode::Rotate);
    if (ImGui::RadioButton("Scale", scene.GetCurrentTmMode() == TransformManipulator::TmMode::Scale))
        scene.SetTmMode(TransformManipulator::TmMode::Scale);


    ImGui::Begin("GPU perf");
    ImGui::Text(std::format("GPass(ms)    : {}", scene.GetPbrRenderer().PerfCounters.GPassTime).c_str());
    ImGui::Text(std::format("LightPass(ms): {}", scene.GetPbrRenderer().PerfCounters.LightPassTime).c_str());
    ImGui::End();

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

    tao_scene::TaoScene scene{tao_scene::TaoScene::tao_scene_config{}};

    scene.LoadGltf(format("{}/{}/scene.gltf", MODELS_DIR, "DamagedHelmet").c_str());

    scene.AddLight(RectLight{
        .transformation =
            translate(mat4{1.0}, vec3{2.0f, -1.0f, 0.0f}) *
            rotate(mat4{1.0f}, -0.25f * pi<float>(), vec3(0.0f, 0.0f, 1.0f)) *
            rotate(mat4{1.0f}, -0.7f * pi<float>(), vec3(0.0f, 1.0f, 0.0f)),
        .intensity = vec3(0.5, 0.1, 0.7) * 5.0f,
        .size = vec2(1.8f, 0.7f),
    });

    scene.AddLight(RectLight{
        .transformation =
            translate(mat4{1.0}, vec3{0.0f, 2.0f, 0.5f}) *
            rotate(mat4{1.0f}, -1.6f * pi<float>(), vec3(0.0f, 0.0f, 1.0f)) *
            rotate(mat4{1.0f}, -0.7f * pi<float>(), vec3(0.0f, 1.0f, 0.0f)),
        .intensity = vec3(0.8, 0.5, 0.1) * 5.0f,
        .size = vec2(0.7f, 2.0f),
    });

    scene.AddLight(DirectionalLight{
        .transformation =
            translate(mat4{1.0}, vec3{0.0f, 0.0f, 2.2f}) *
            rotate(mat4{1.0f}, 0.8f * pi<float>(), vec3{-1.0f, 1.0f, 0.0f}),
        .intensity = vec3(0.5f),
    });

    InitImGui(scene.GetRenderContext().GetWindow());

    while (!scene.GetRenderContext().ShouldClose())
    {
        scene.NewFrame();

        StartImGuiFrame(scene);

        EndImGuiFrame();

        scene.EndFrame();
    }

    ImGuiShutdown();
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
