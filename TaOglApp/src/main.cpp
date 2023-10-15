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

vector<LineGizmoVertex> GetCircleVertices(float startAngle, float endAngle)
{
    const int kCircPtsCount = 37;

    vector<LineGizmoVertex> circPts(kCircPtsCount);
    const float da = (endAngle - startAngle) / (kCircPtsCount - 1);

    for (int i = 0; i < kCircPtsCount; i++)
    {
        circPts[i] = LineGizmoVertex()
            .Position(vec3{cos(startAngle + i * da), sin(startAngle + i * da), 0.0f})
            .Color(vec4{1.0f});
    }

    return circPts;
}

void CreateDashedPatternTest0(unsigned int patternLen, unsigned int patternWidth,
                              tao_gizmos_procedural::TextureDataRgbaUnsignedByte &tex)
{

    tex.FillWithFunction([patternLen, patternWidth](unsigned int x, unsigned int y)
                         {
                             vec2 sampl = vec2{x, y} + vec2{0.5f};

                             float d = patternLen / 4.0f;

                             vec2 dashStart = vec2{d, patternWidth / 2.0f};
                             vec2 dashEnd = vec2{patternLen - d, patternWidth / 2.0f};

                             SdfTrapezoid tip{patternWidth * 0.95f, 1e-6f, d};

                             float val =
                                 SdfSegment<float>{dashStart, dashEnd}
                                     .Inflate(patternWidth * 0.1f)
                                     .Add(tip.Rotate(-0.5f * pi<float>()).Translate(dashEnd))
                                     .Evaluate(sampl);

                             val = glm::clamp(val, 0.0f, 1.0f);

                             return vec<4, unsigned char>{255, 255, 255, (1.0f - val) * 255};
                         }
    );

}

void CreateDashedPatternTest1(unsigned int patternLen, unsigned int patternWidth,
                              tao_gizmos_procedural::TextureDataRgbaUnsignedByte &tex)
{

    tex.FillWithFunction([patternLen, patternWidth](unsigned int x, unsigned int y)
                         {
                             vec2 sampl = vec2{x, y} + vec2{0.5f};

                             float d = patternLen / 4.0f;

                             vec2 dashStart = vec2{d, patternWidth / 2.0f};
                             vec2 dashEnd = vec2{patternLen - d, patternWidth / 2.0f};

                             float val =
                                 SdfSegment<float>{dashStart, dashEnd}
                                     .Inflate(patternWidth * 0.3f)
                                     .Evaluate(sampl);

                             val = glm::clamp(val, 0.0f, 1.0f);

                             return vec<4, unsigned char>{255, 255, 255, (1.0f - val) * 255};
                         });

}

void CreateGizmoTest(GizmosRenderer &gr)
{
    const int kWidth0 = 16;
    const int kLength0 = 64;

    const int kWidth1 = 6;
    const int kLength1 = 32;

    auto layer = gr.CreateRenderLayer(

        ogl_depth_state{
            .depth_test_enable = true,
            .depth_write_enable = true,
            .depth_func = depth_func_less_equal,
            .depth_range_near =0.0f,
            .depth_range_far = 1.0f
        },
        ogl_blend_state{
            .blend_enable = false,
            .color_mask = mask_all
        },
        ogl_rasterizer_state{
            .culling_enable = false,
            .polygon_mode = polygon_mode_fill,
            .multisample_enable = true,
            .alpha_to_coverage_enable = true,
        }
    );
    auto pass = gr.CreateRenderPass({layer});
    gr.AddRenderPass({pass});

    auto verts = GetCircleVertices(0.0f, 1.3f * pi<float>());

    TextureDataRgbaUnsignedByte tex0{kLength0, kWidth0, TexelData<4, unsigned char>{255, 0, 0, 255}};
    TextureDataRgbaUnsignedByte tex1{kLength1, kWidth1, TexelData<4, unsigned char>{255, 0, 0, 255}};
    CreateDashedPatternTest0(kLength0, kWidth0, tex0);
    CreateDashedPatternTest1(kLength1, kWidth1, tex1);

    pattern_texture_descriptor texDesc0{
        .data = tex0.DataPtr(),
        .data_format = tex_for_rgba,
        .data_type = tex_typ_unsigned_byte,
        .width = kLength0,
        .height = kWidth0,
        .pattern_length = kLength0
    };

    pattern_texture_descriptor texDesc1{
        .data = tex1.DataPtr(),
        .data_format = tex_for_rgba,
        .data_type = tex_typ_unsigned_byte,
        .width = kLength1,
        .height = kWidth1,
        .pattern_length = kLength1
    };

    auto gizmo0 = gr.CreateLineStripGizmo(line_strip_gizmo_descriptor{
        .vertices = verts,
        .isLoop = false,
        .line_size = kWidth0,
        .zoom_invariant = false,
        .pattern_texture_descriptor = &texDesc0,
        .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
    });

    auto gizmo1 = gr.CreateLineStripGizmo(line_strip_gizmo_descriptor{
        .vertices = verts,
        .isLoop = false,
        .line_size = kWidth1,
        .zoom_invariant = false,
        .pattern_texture_descriptor = &texDesc1,
        .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
    });

    auto gizmo0Instance = gr.InstanceLineStripGizmo(gizmo0, {
        gizmo_instance_descriptor{
            .transform = mat4{1.0f} * translate(mat4{1.0f}, vec3{-0.5f, 0.5f, 0.0f}) * rotate(mat4{1.0f}, 0.5f * pi<float>(), vec3(1.0f, 0.0f, 0.0)),
            .color = vec4{1.0f, 0.5f, 0.0f, 1.0f},
            .visible = true,
            .selectable = false
        },
        //gizmo_instance_descriptor{
        //    .transform = mat4{1.0f} * translate(mat4{1.0f}, vec3{-0.5f, 0.5f, 0.0f}),
        //    .color = vec4{1.0f, 0.5f, 0.0f, 1.0f},
        //    .visible = true,
        //    .selectable = false
        //}
        });

    auto gizmo1Instance = gr.InstanceLineStripGizmo(gizmo1, {
        //gizmo_instance_descriptor{
        //    .transform = mat4{1.0f} * translate(mat4{1.0f}, vec3{-0.5f, 0.5f, 0.0f}),
        //    .color = vec4{0.3f, 0.1f, 0.9f, 1.0f},
        //    .visible = true,
        //    .selectable = false
        //},
        gizmo_instance_descriptor{
            .transform = mat4{1.0f} * translate(mat4{1.0f}, vec3{0.5f, -0.5f, 0.0f})* rotate(mat4{1.0f}, 0.5f * pi<float>(), vec3(1.0f, 0.0f, 0.0)),
            .color = vec4{0.3f, 0.1f, 0.9f, 1.0f},
            .visible = true,
            .selectable = false
        }});


    gr.AssignGizmoToLayers(gizmo0, {layer});
    gr.AssignGizmoToLayers(gizmo1, {layer});

}

int main()
{

    tao_scene::TaoScene scene{tao_scene::TaoScene::tao_scene_config{}};

    //CreateGizmoTest(scene.GetGizmosRenderer());

    scene.LoadGltf(format("{}/{}/scene.gltf", MODELS_DIR, "ApolloAndDaphne").c_str());

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
