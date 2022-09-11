#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "Shader.h"
#include "stb_image.h"
#include "Camera.h"
#include "Mesh.h"
#include "GeometryHelper.h"
#include "SceneUtils.h"
#include "FrameBuffer.h"
#include "Scene.h"



#if GFX_STOPWATCH
    #include "Diagnostics.h"
#endif

using namespace OGLResources;

// CONSTANTS ======================================================
const char* glsl_version = "#version 130";

const unsigned int directionalShadowMapResolution = 2048;
const unsigned int pointShadowMapResolution = 512;
const int SSAO_MAX_SAMPLES = 64;
const int SSAO_BLUR_MAX_RADIUS = 16;


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_scroll_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void keyboard_button_callback(GLFWwindow* window, int key, int scancode, int action, int mods);


Camera camera = Camera( 5, 0, 0);

bool firstMouse = true;
float lastX, lastY, yaw, pitch, roll;
//unsigned int width, height;


// Debug ===========================================================
bool showGrid = false;
bool showLights = false;
bool showBoundingBox = false;
bool showAO = false;
bool showStats = true;

// Camera ==========================================================
bool perspective = true;
float fov = 45.0f;

// Scene Parameters ==========================================================
SceneParams sceneParams = SceneParams();

// BBox ============================================================
using namespace Utils;


// ImGUI ============================================================
const char* ao_comboBox_items[] = {"SSAO", "HBAO", "NONE"};
static const char* ao_comboBox_current_item = "HBAO";

AOType AOShaderFromItem(const char* item)
{
    if (!strcmp("SSAO", item))
        return AOType::SSAO;

    else if (!strcmp("HBAO", item))
        return AOType::HBAO;

    else return AOType::NONE;
}


void ShowStatsWindow()
{
#if GFX_STOPWATCH

    // Constants
    // ----------
    int lineWidth_font = 28;
    int barWidth_font = 10;

    ImGui::Begin("Stats", &showStats);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    long totTime = 0;

    for (const auto& q : Profiler::Instance().GetAllNamedQueries())
        totTime += (long)q.value;

    int cnt = 0;
    for (const auto& q : Profiler::Instance().GetAllNamedQueries())
    {
        std::string s = "";
        s.insert(s.begin(), std::max(0, 15 - (int)q.name.size()), ' '); // left padding
        s = s.append(q.name);

        ImGui::Text("%s (ms/frame): %.2f", s.c_str(), (long)q.value / 1000000.0f);
        
        // The bar is proportional to the elapsed time 
        float fac = (float)q.value / totTime;

        ImVec2 screenPos = ImGui::GetCursorScreenPos();
        ImVec2 barSize = ImVec2(ImGui::GetFontSize() * barWidth_font * fac, ImGui::GetFontSize());

        ImVec2 p0 = ImVec2(screenPos.x + ImGui::GetFontSize() * lineWidth_font - barSize.x, screenPos.y - barSize.y);
        ImVec2 p1 = ImVec2(p0.x + barSize.x, p0.y + barSize.y);

        draw_list->AddRectFilled(p0, p1,
            ImGui::GetColorU32(IM_COL32(
                PROFILER_COLORS_4RGB[(cnt % 4)*3 + 0],
                PROFILER_COLORS_4RGB[(cnt % 4)*3 + 1],
                PROFILER_COLORS_4RGB[(cnt % 4)*3 + 2],
                120))
        );

        cnt++;
    }

    ImGui::End();

#endif
}

void ShowImGUIWindow()
{
    /*ImGui::ShowDemoWindow(&showWindow);*/

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Camera"))
        {
            ImGui::Checkbox("Perspective", &perspective);
            ImGui::SliderFloat("FOV", &fov, 10.0f, 100.0f);
            
            if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_None))
            {
                ImGuiColorEditFlags f =
                    ImGuiColorEditFlags_DisplayHSV |
                    ImGuiColorEditFlags_PickerHueWheel |
                    ImGuiColorEditFlags_NoSidePreview;

                ImGui::Checkbox("Image Texture", &sceneParams.environment.useSkyboxTexture);
                ImGui::ColorPicker3("North", &sceneParams.environment.NorthColor[0], f);
                ImGui::ColorPicker3("Equator", &sceneParams.environment.EquatorColor[0], f);
                ImGui::ColorPicker3("South", &sceneParams.environment.SouthColor[0], f);
            }

            ImGui::EndTabItem();

            
        }
        if (ImGui::BeginTabItem("Lights"))
        {
            // Ambient Light
            // -------------
            if (ImGui::CollapsingHeader("Ambient", ImGuiTreeNodeFlags_None))
            {
                ImGui::DragFloat("Environment", (float*)&(sceneParams.environment.intensity), 0.1, 0.0, 1.0);
                if (ImGui::BeginCombo("AO Tecnique", ao_comboBox_current_item)) // The second parameter is the label previewed before opening the combo.
                {
                    for (int n = 0; n < IM_ARRAYSIZE(ao_comboBox_items); n++)
                    {
                        bool is_selected = (ao_comboBox_current_item == ao_comboBox_items[n]); // You can store your selection however you want, outside or inside your objects

                        if (ImGui::Selectable(ao_comboBox_items[n], is_selected))
                            ao_comboBox_current_item = ao_comboBox_items[n];

                        if (is_selected)
                            ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)

                    }
                    ImGui::EndCombo();
                }
                ImGui::DragFloat("AORadius", &sceneParams.sceneLights.Ambient.aoRadius, 0.001f, 0.001f, 1.0f);
                ImGui::DragInt("AOSamples", &sceneParams.sceneLights.Ambient.aoSamples, 1, 1, 64);
                ImGui::DragInt("AOSteps", &sceneParams.sceneLights.Ambient.aoSteps, 1, 1, 64);
                ImGui::DragInt("AOBlur", &sceneParams.sceneLights.Ambient.aoBlurAmount, 1, 0, SSAO_BLUR_MAX_RADIUS-1);
                ImGui::DragFloat("AOStrength", &sceneParams.sceneLights.Ambient.aoStrength, 0.1f, 0.0f, 5.0f);
            }

            // Directional Light
            // -----------------
            if (ImGui::CollapsingHeader("Directional", ImGuiTreeNodeFlags_None))
            {
                ImGui::DragFloat3("Direction", (float*)&(sceneParams.sceneLights.Directional.Direction), 0.05f, -1.0f, 1.0f);
                ImGui::ColorEdit3("Diffuse", (float*)&(sceneParams.sceneLights.Directional.Diffuse));
                ImGui::DragFloat("Intensity", (float*)&(sceneParams.sceneLights.Directional.Diffuse.a), 0.1, 0.0, 5.0);
                //ImGui::ColorEdit4("Specular", (float*)&(sceneParams.sceneLights.Directional.Specular));

                if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_None))
                {
                    ImGui::Checkbox("Enable", &sceneParams.drawParams.doShadows);
                    ImGui::DragFloat("Bias", &sceneParams.sceneLights.Directional.Bias, 0.001f, 0.0f, 0.05f);
                    ImGui::DragFloat("SlopeBias", &sceneParams.sceneLights.Directional.SlopeBias, 0.001f, 0.0f, 0.05f);
                    ImGui::DragFloat("Softness", &sceneParams.sceneLights.Directional.Softness, 0.00005f, 0.0f, 0.05f);
                }
            }

            // Point Lights
            // ------------
            for (int i = 0; i < PointLight::MAX_POINT_LIGHTS; i++)
            {
                if (ImGui::CollapsingHeader((std::string("Point") + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_None))
                {
                    ImGui::DragFloat3("Position", (float*)&(sceneParams.sceneLights.Points[i].Position), 0.1f);

                    if (ImGui::ColorEdit3("Color", (float*)&(sceneParams.sceneLights.Points[i].Color)) ||
                        ImGui::DragFloat("Intensity", (float*)&(sceneParams.sceneLights.Points[i].Color.a), 0.1, 0.0, 100.0))
                    {
                        sceneParams.sceneLights.Points[i].ComputeRadius();
                    }

                    if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_None))
                    {
                        ImGui::DragFloat("Bias", &sceneParams.sceneLights.Points[i].Bias, 0.001f, 0.0f, 0.05f);
                        ImGui::DragFloat("SlopeBias", &sceneParams.sceneLights.Points[i].SlopeBias, 0.001f, 0.0f, 0.05f);
                    }
                }
            }

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("PostProcessing"))
        {
            if (ImGui::CollapsingHeader("ToneMapping", ImGuiTreeNodeFlags_None))
            {
                ImGui::Checkbox("Enable", &sceneParams.postProcessing.ToneMapping);
                ImGui::DragFloat("Exposure", &sceneParams.postProcessing.Exposure, 0.25f, 0.0f, 5.0f);
            }
            
            ImGui::Checkbox("GammaCorrection", &sceneParams.postProcessing.GammaCorrection);

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debug"))
        {
            ImGui::Checkbox("Grid", &showGrid);
            ImGui::Checkbox("BoundingBox", &showBoundingBox);
            ImGui::Checkbox("Show Lights", &showLights);
            ImGui::Checkbox("AO Pass", &showAO);
            ImGui::Checkbox("Stats", &showStats);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void LoadScene_Primitives(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>> *shadersCollection)
{
    Mesh boxMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(
        std::make_shared<MeshRenderer>(glm::vec3(0, 0, 2), 0.0, glm::vec3(0, 0, 1), glm::vec3(1, 1, 1),boxMesh, 
            (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
            (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::Copper));
    
    
    Mesh coneMesh = Mesh::Cone(0.8, 1.6, 16);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(1, 2, 1), 1.5, glm::vec3(0, -1, 1), glm::vec3(1, 1, 1), coneMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed));
    
    
    Mesh cylMesh = Mesh::Cylinder(0.6, 1.6, 16);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-1, -0.5, 1), 1.1, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), cylMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen));

    Mesh sphereMesh = Mesh::Sphere(1.0, 32);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-1, -1.5, 1.2),0.0, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), sphereMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen));

  
    
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));

    
}

void LoadScene_PbrTestSpheres(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
     
    Mesh sphereMesh = Mesh::Sphere(1.0, 32);
    
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            MeshRenderer mr
            {
                sphereMesh,
                (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
                (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get()
            };

            glm::mat4 t = glm::mat4(1.0);
            t = glm::translate(t, glm::vec3(2.5 * i, 2.5 * j, 0.0));
            t = glm::translate(t, glm::vec3(-5.0, -5.0, 0.0));

            mr.SetTransformation(t);
            mr.SetMaterial(Material{ glm::vec4(1.0, 1.0, 1.0, 1.0), (i+1) / 5.0f, (j+1) / 5.0f });

            sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(mr)));
        }
    }
}

void LoadScene_PbrTestTeapots(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{

    FileReader reader = FileReader("../../Assets/Models/Teapot.obj");
    reader.Load();
    Mesh teapotMesh = reader.Meshes()[0];

    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            MeshRenderer mr
            {
                teapotMesh,
                (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
                (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get()
            };

            glm::mat4 t = glm::mat4(1.0);
            t = glm::translate(t, glm::vec3(2.0 * i, 2.0 * j, -0.1));
            t = glm::translate(t, glm::vec3(-4.0f, -4.0f, 0.0f));

            mr.SetTransformation(t);
            mr.SetMaterial(Material{ glm::vec4(1.0, 1.0, 1.0, 1.0), (i + 1) / 5.0f, (j + 1) / 5.0f });

            sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(mr)));
        }
    }
}

void LoadScene_PbrTestKnobs(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{

    FileReader reader = FileReader("../../Assets/Models/Knob.obj");
    reader.Load();
    std::vector<Mesh> meshes = reader.Meshes();

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int m = 0; m < meshes.size(); m++)
            {
                MeshRenderer mr
                {
                    meshes.at(m),
                    (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
                    (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get()
                };

                glm::mat4 t = glm::mat4(1.0);
                t = glm::translate(t, glm::vec3(4.0 * i, 4.0 * j, -0.1));
                t = glm::translate(t, glm::vec3(-3.0f, -3.0f, 0.0f));

                mr.SetTransformation(t);
                mr.SetMaterial(Material{ glm::vec4(1.0, 1.0, 1.0, 1.0), (i + 1) / 3.0f, (j + 1) / 3.0f });

                sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(mr)));
            }
        }
    }
}

void LoadScene_NormalMapping(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
    
    Mesh sphereMesh0 = Mesh::Sphere(1.0, 64);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(0.0, -1.5, 1.5), 0.0, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), sphereMesh0,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::Cobblestone));
    
    Mesh sphereMesh1 = Mesh::Sphere(1.0, 64);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-1.5, 0.0, 1.1), 0.0, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), sphereMesh1,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ClayShingles));

    Mesh sphereMesh2 = Mesh::Sphere(1.0, 64);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(1.5, 0.0, 1.1), 0.0, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), sphereMesh2,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::WornFactoryFloor));


    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-4, -4.75, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));


}

void LoadScene_Hilbert(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<WiresShader>>* shadersCollection)
{

    Wire hilbertWire = Wire(GetHilberCurve2D(4, 1.0f), WireNature::LINE_STRIP);
    Wire hilbertPointsWire = Wire(GetHilberCurve2D(4, 1.0f), WireNature::POINTS);

    WiresRenderer hilbert = WiresRenderer(hilbertWire, shadersCollection->at("LINE_STRIP").get());
    WiresRenderer hilbertPoints = WiresRenderer(hilbertPointsWire, shadersCollection->at("POINTS").get());

    hilbert.SetColor(glm::vec4(1.0, 0.2, 0.1, 1.0));
    hilbertPoints.SetColor(glm::vec4(0.8, 0.5, 0.0, 1.0));

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(hilbert)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(hilbertPoints)));
  

}

void LoadScene_PoissonDistribution(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<WiresShader>>* shadersCollection)
{
   
    Wire circleWire = Wire::Circle(32, 1.0f);
    Wire squareWire = Wire::Square(2.0f);
    
    // Circles
    // -----------------
    WiresRenderer circle_8 = WiresRenderer(circleWire, shadersCollection->at("LINE_STRIP").get());
    circle_8.SetColor(glm::vec4(0.0, 0.5, 0.5, 1.0));

    WiresRenderer circle_16 = WiresRenderer(circleWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&circle_16)->Translate(2.5f, 0.0f , 0.0f);
    circle_16.SetColor(glm::vec4(0.0, 0.5, 0.5, 1.0));

    WiresRenderer circle_32 = WiresRenderer(circleWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&circle_32)->Translate(5.0f, 0.0f, 0.0f);
    circle_32.SetColor(glm::vec4(0.0, 0.5, 0.5, 1.0));

    WiresRenderer circle_64 = WiresRenderer(circleWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&circle_64)->Translate(7.5f, 0.0f, 0.0f);
    circle_64.SetColor(glm::vec4(0.0, 0.5, 0.5, 1.0));

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circle_8)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circle_16)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circle_32)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circle_64)));

    std::vector<glm::vec3> circleSamplesPts{};
    for (auto& s : GetPoissonDiskSamples2D(64, Domain2D(DomainType2D::Disk, 1.0f)))
        circleSamplesPts.push_back(glm::vec3(s, 0.01f));

    Wire circleSamplesWire_8 = Wire(
        std::vector<glm::vec3>(circleSamplesPts.begin(), circleSamplesPts.begin() + 8),
        WireNature::POINTS);
    WiresRenderer circleSamples_8 = WiresRenderer(circleSamplesWire_8, shadersCollection->at("POINTS").get());
    circleSamples_8.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    Wire circleSamplesWire_16 = Wire(
        std::vector<glm::vec3>(circleSamplesPts.begin(), circleSamplesPts.begin() + 16),
        WireNature::POINTS);
    WiresRenderer circleSamples_16 = WiresRenderer(circleSamplesWire_16, shadersCollection->at("POINTS").get());
    ((Renderer*)&circleSamples_16)->Translate(2.5f, 0.0f, 0.0f);
    circleSamples_16.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    Wire circleSamplesWire_32 = Wire(
        std::vector<glm::vec3>(circleSamplesPts.begin(), circleSamplesPts.begin() + 32),
        WireNature::POINTS);
    WiresRenderer circleSamples_32 = WiresRenderer(circleSamplesWire_32, shadersCollection->at("POINTS").get());
    ((Renderer*)&circleSamples_32)->Translate(5.0f, 0.0f, 0.0f);
    circleSamples_32.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    Wire circleSamplesWire_64 = Wire(
        circleSamplesPts,
        WireNature::POINTS);
    WiresRenderer circleSamples_64 = WiresRenderer(circleSamplesWire_64, shadersCollection->at("POINTS").get());
    ((Renderer*)&circleSamples_64)->Translate(7.5f, 0.0f, 0.0f);
    circleSamples_64.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circleSamples_8)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circleSamples_16)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circleSamples_32)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(circleSamples_64)));

    // -----------------

    // Squares
    // -----------------
    WiresRenderer square_8 = WiresRenderer(squareWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&square_8)->Translate(-1.0f, 2.5f, 0.0f);
    square_8.SetColor(glm::vec4(0.9, 0.5, 0.0, 1.0));

    WiresRenderer square_16 = WiresRenderer(squareWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&square_16)->Translate(1.5f, 2.5f, 0.0f);
    square_16.SetColor(glm::vec4(0.9, 0.5, 0.0, 1.0));

    WiresRenderer square_32 = WiresRenderer(squareWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&square_32)->Translate(4.0f, 2.5f, 0.0f);
    square_32.SetColor(glm::vec4(0.9, 0.5, 0.0, 1.0));

    WiresRenderer square_64 = WiresRenderer(squareWire, shadersCollection->at("LINE_STRIP").get());
    ((Renderer*)&square_64)->Translate(6.5f, 2.5f, 0.0f);
    square_64.SetColor(glm::vec4(0.9, 0.5, 0.0, 1.0));

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(square_8)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(square_16)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(square_32)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(square_64)));

    std::vector<glm::vec3> squareSamplesPts{};
    for (auto& s : GetPoissonDiskSamples2D(64, Domain2D(DomainType2D::Square, 2.0f)))
        squareSamplesPts.push_back(glm::vec3(s, 0.01f));

    Wire squareSamplesWire_8 = Wire(
        std::vector<glm::vec3>(squareSamplesPts.begin(), squareSamplesPts.begin() + 8),
        WireNature::POINTS);
    WiresRenderer squareSamples_8 = WiresRenderer(squareSamplesWire_8, shadersCollection->at("POINTS").get());
    ((Renderer*)&squareSamples_8)->Translate(-1.0f, 2.5f, 0.0f);
    squareSamples_8.SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    Wire squareSamplesWire_16 = Wire(
        std::vector<glm::vec3>(squareSamplesPts.begin(), squareSamplesPts.begin() + 16),
        WireNature::POINTS);
    WiresRenderer squareSamples_16 = WiresRenderer(squareSamplesWire_16, shadersCollection->at("POINTS").get());
    ((Renderer*)&squareSamples_16)->Translate(1.5f, 2.5f, 0.0f);
    squareSamples_16.SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    Wire squareSamplesWire_32 = Wire(
        std::vector<glm::vec3>(squareSamplesPts.begin(), squareSamplesPts.begin() + 32),
        WireNature::POINTS);
    WiresRenderer squareSamples_32 = WiresRenderer(squareSamplesWire_32, shadersCollection->at("POINTS").get());
    ((Renderer*)&squareSamples_32)->Translate(4.0f, 2.5f, 0.0f);
    squareSamples_32.SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    Wire squareSamplesWire_64 = Wire(
        squareSamplesPts,
        WireNature::POINTS);
    WiresRenderer squareSamples_64 = WiresRenderer(squareSamplesWire_64, shadersCollection->at("POINTS").get());
    ((Renderer*)&squareSamples_64)->Translate(6.5f, 2.5f, 0.0f);
    squareSamples_64.SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_8)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_16)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_32)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_64)));

    // -----------------

   
}

void LoadScene_CubicBezier(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<WiresShader>>* shadersCollection)
{

    // Curve 0
    // -------
    Curves::CubicBezier cb0 = Curves::CubicBezier(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(2.0f, 1.0f, 0.0f)
        );

    Wire cb0Wire = Wire(cb0.ToPoints(30), WireNature::LINE_STRIP);
    Wire cb0PointsWire = Wire(std::vector<glm::vec3>{cb0.P0(), cb0.P1(), cb0.P2(), cb0.P3()}, WireNature::POINTS);
    WiresRenderer cb0WR = WiresRenderer(cb0Wire, shadersCollection->at("LINE_STRIP").get());
    WiresRenderer cb0PointsWR = WiresRenderer(cb0PointsWire, shadersCollection->at("POINTS").get());
    cb0PointsWR.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    cb0WR.SetColor(glm::vec4(0.1f, 1.0f, 0.2f, 1.0f));
    
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb0WR)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb0PointsWR)));

    // Curve 1
    // -------
    Curves::CubicBezier cb1 = Curves::CubicBezier(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(0.5f, 0.5f, 0.0f)
    );

    Wire cb1Wire = Wire(cb1.ToPoints(30), WireNature::LINE_STRIP);
    Wire cb1PointsWire = Wire(std::vector<glm::vec3>{cb1.P0(), cb1.P1(), cb1.P2(), cb1.P3()}, WireNature::POINTS);
    WiresRenderer cb1WR = WiresRenderer(cb1Wire, shadersCollection->at("LINE_STRIP").get());
    WiresRenderer cb1PointsWR = WiresRenderer(cb1PointsWire, shadersCollection->at("POINTS").get());
    cb1PointsWR.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    cb1WR.SetColor(glm::vec4(0.1f, 1.0f, 0.2f, 1.0f));
    cb1PointsWR.Translate(glm::vec3(4.0f, 0.0f, 0.0f));
    cb1WR.Translate(glm::vec3(4.0f, 0.0f, 0.0f));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb1WR)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb1PointsWR)));

    // Curve 2
    // -------
    Curves::CubicBezier cb2 = Curves::CubicBezier(
        glm::vec3(0.0f, 0.3f, 0.0f),
        glm::vec3(1.0f,-1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(0.0f,-0.3f, 0.0f)
    );

    Wire cb2Wire = Wire(cb2.ToPoints(30), WireNature::LINE_STRIP);
    Wire cb2PointsWire = Wire(std::vector<glm::vec3>{cb2.P0(), cb2.P1(), cb2.P2(), cb2.P3()}, WireNature::POINTS);
    WiresRenderer cb2WR = WiresRenderer(cb2Wire, shadersCollection->at("LINE_STRIP").get());
    WiresRenderer cb2PointsWR = WiresRenderer(cb2PointsWire, shadersCollection->at("POINTS").get());
    cb2PointsWR.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    cb2WR.SetColor(glm::vec4(0.1f, 1.0f, 0.2f, 1.0f));
    cb2PointsWR.Translate(glm::vec3(0.0f, -4.0f, 0.0f));
    cb2WR.Translate(glm::vec3(0.0f, -4.0f, 0.0f));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb2WR)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb2PointsWR)));

    // Curve 3
    // -------
    Curves::CubicBezier cb3 = Curves::CubicBezier(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.5f, 0.0f),
        glm::vec3(1.0f, 1.5f, 0.0f)
    );

    Wire cb3Wire = Wire(cb3.ToPoints(30), WireNature::LINE_STRIP);
    Wire cb3PointsWire = Wire(std::vector<glm::vec3>{cb3.P0(), cb3.P1(), cb3.P2(), cb3.P3()}, WireNature::POINTS);
    WiresRenderer cb3WR = WiresRenderer(cb3Wire, shadersCollection->at("LINE_STRIP").get());
    WiresRenderer cb3PointsWR = WiresRenderer(cb3PointsWire, shadersCollection->at("POINTS").get());
    cb3PointsWR.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    cb3WR.SetColor(glm::vec4(0.1f, 1.0f, 0.2f, 1.0f));
    cb3PointsWR.Translate(glm::vec3(4.0f, -4.0f, 0.0f));
    cb3WR.Translate(glm::vec3(4.0f, -4.0f, 0.0f));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb3WR)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cb3PointsWR)));
}

void LoadScene_PCSStest(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
    Mesh planeMesh = Mesh::Box(8, 8, 0.5);

    FileReader reader = FileReader("../../Assets/Models/Teapot.obj");
    reader.Load();
    Mesh teapotMesh = reader.Meshes()[0];

    MeshRenderer plane = MeshRenderer(planeMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get());
    plane.SetMaterial(Material{ glm::vec4(1.0, 1.0, 1.0 ,1.0), 0.8, 0.0 });
    plane.Renderer::SetTransformation(glm::vec3(-4, -4, -0.5), 0, glm::vec3(1.0, 0, 0), glm::vec3(1, 1, 1));
    
    
    MeshRenderer
        teapot1 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot2 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot3 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot4 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot5 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get());

    teapot1.Renderer::Transform(glm::vec3(-1, 1, 1), 1.5, glm::vec3(1, -0.5, 0.6), glm::vec3(1, 1, 1));
    teapot2.Renderer::Transform(glm::vec3(0, 0.6, 3), 0.5, glm::vec3(0, -0.5, 0.6), glm::vec3(1, 1, 1));
    teapot3.Renderer::Transform(glm::vec3(0, -1.5, 0.6), 3.2, glm::vec3(1, -0, 0.6), glm::vec3(1, 1, 1));
    teapot4.Renderer::Transform(glm::vec3(-0.5, -0.6, 2.3), 1.4, glm::vec3(1, -0.5, 1.6), glm::vec3(1, 1, 1));
    teapot5.Renderer::Transform(glm::vec3(1.6, 0.8, 0.5), 0.6, glm::vec3(1,1.0,2.0), glm::vec3(1, 1, 1));

    teapot1.SetMaterial(Material{ glm::vec4(1.0, 0.0, 0.0 ,1.0), 0.9, 0.0 });
    teapot2.SetMaterial(Material{ glm::vec4(0.9, 0.5, 0.0 ,1.0), 0.1, 0.0 });
    teapot3.SetMaterial(Material{ glm::vec4(0.8, 0.3, 0.5 ,1.0), 0.5, 0.0 });
    teapot4.SetMaterial(Material{ glm::vec4(0.89, 0.92, 0.84 ,1.0), 0.5, 1.0 });
    teapot5.SetMaterial(Material{ glm::vec4(0.4, 0.4, 0.9 ,1.0), 0.6, 0.0 });


    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(plane)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(teapot1)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(teapot2)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(teapot3)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(teapot4)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(teapot5)));



}

void LoadPlane(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection, float size, float height)
{

    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-size / 2.0f, -size / 2.0f, -size / 20.0f + height), 0.0, glm::vec3(0, 0, 1), glm::vec3(size, size, size / 20.0f), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));

}
void LoadPlane(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection, float size)
{

    LoadPlane(sceneMeshCollection, shadersCollection, size, 0.0f);

}

void LoadScene_Monkeys(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
{
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    MeshRenderer plane =
        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
            planeMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray);

    sceneMeshCollection->push_back(std::move(plane));

    FileReader reader = FileReader("../../Assets/Models/suzanne.obj");
    reader.Load();
    Mesh monkeyMesh = reader.Meshes()[0];
    MeshRenderer monkey1 =
        MeshRenderer(glm::vec3(0, -1.0, 2.0), 1.3, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed);

    MeshRenderer monkey2 =
        MeshRenderer(glm::vec3(-2.0, 1.0, 0.8), 1.3, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen);

    MeshRenderer monkey3 =
        MeshRenderer(glm::vec3(2.0, 1.0, 0.8), 1.3, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::Copper);

    sceneMeshCollection->push_back(std::move(monkey1));
    sceneMeshCollection->push_back(std::move(monkey2));
    sceneMeshCollection->push_back(std::move(monkey3));

    for (int i = 0; i < sceneMeshCollection->size(); i++)
    {
        MeshRenderer* mr = &(sceneMeshCollection->data()[i]);
        sceneBoundingBox->Update(mr->GetTransformedPoints());
    }
}

//void LoadScene_ALotOfMonkeys(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            planeMesh, shadersCollection->at("LIT_WITH_SHADOWS_SSAO").get(), shadersCollection->at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray);
//
//    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(plane)));
//
//    FileReader reader = FileReader("Assets/Models/suzanne.obj");
//    reader.Load();
//    Mesh monkeyMesh = reader.Meshes()[0];
//    MeshRenderer monkey1 =
//        MeshRenderer(glm::vec3(-1.0, -1.0, 0.4), 0.9, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed);
//    monkey1.Transform(glm::vec3(0, 0, 0), glm::pi<float>() / 4.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0));
//
//    MeshRenderer monkey2 =
//        MeshRenderer(glm::vec3(-1.0, 1.0, 0.4), 0.9, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen);
//    monkey2.Transform(glm::vec3(-1.5, 1.5, 0), 3.0 * glm::pi<float>() / 4.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0));
//
//    MeshRenderer monkey3 =
//        MeshRenderer(glm::vec3(0.0, 0.5, 1.0), 1.2, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::Copper);
//    //monkey3.Transform(glm::vec3(0, 0, 0), 3.0 * glm::pi<float>() / 4.0, glm::vec3(0, 0.0, 1.0), glm::vec3(1.0, 1.0, 1.0));
//
//    MeshRenderer monkey4 =
//        MeshRenderer(glm::vec3(1.0, -1.0, 0.4), 0.9, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PureWhite);
//    monkey4.Transform(glm::vec3(0, 0, 0), glm::pi<float>() / 4.0, glm::vec3(0, 0.0, 1.0), glm::vec3(1.0, 1.0, 1.0));
//
//    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey1)));
//    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey2)));
//    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey3)));
//    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey4)));
//}
//
//void LoadScene_Cadillac(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//
//    FileReader reader = FileReader("./Assets/Models/Cadillac.obj");
//    reader.Load();
//    Mesh cadillacMesh0 = reader.Meshes()[0];
//    MeshRenderer cadillac0 =
//        MeshRenderer(glm::vec3(-1.0, -1.0, 0.4), glm::pi<float>()*0.5, glm::vec3(1, 0, 0), glm::vec3(1,1,1),
//            &cadillacMesh0, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::ShinyRed);
//    cadillac0.Transform(glm::vec3(1, 0.5, -0.5),0.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0), true);
//
//    Mesh cadillacMesh1 = reader.Meshes()[1];
//    MeshRenderer cadillac1 =
//        MeshRenderer(glm::vec3(-1.0, -1.0, 0.4), glm::pi<float>() * 0.5, glm::vec3(1, 0, 0), glm::vec3(1,1,1),
//            &cadillacMesh1, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::ShinyRed);
//    cadillac1.Transform(glm::vec3(1, 0.5, -0.5), 0.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0), true);
//
//    Mesh cadillacMesh2 = reader.Meshes()[2];
//    MeshRenderer cadillac2 =
//        MeshRenderer(glm::vec3(-1.0, -1.0, 0.4), glm::pi<float>() * 0.5, glm::vec3(1, 0, 0), glm::vec3(1,1,1),
//            &cadillacMesh2, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::ShinyRed);
//    cadillac2.Transform(glm::vec3(1, 0.5, -0.5), 0.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0), true);
//
//    
//    sceneMeshCollection->push_back(cadillac0);
//    sceneMeshCollection->push_back(cadillac1);
//    sceneMeshCollection->push_back(cadillac2);
//
//
//    for (int i = 0; i < sceneMeshCollection->size(); i++)
//    {
//        MeshRenderer* mr = &(sceneMeshCollection->data()[i]);
//        sceneBoundingBox->Update(mr->GetTransformedPoints());
//    }
//}
//
//void LoadScene_Dragon(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//
//    FileReader reader = FileReader("./Assets/Models/Dragon.obj");
//    reader.Load();
//    Mesh dragonMesh = reader.Meshes()[0];
//    MeshRenderer dragon =
//        MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0.0, glm::vec3(0, 1, 0), glm::vec3(1, 1, 1),
//            &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//    
//    sceneMeshCollection->push_back(dragon);
//
//
//
//    for (int i = 0; i < sceneMeshCollection->size(); i++)
//    {
//        MeshRenderer* mr = &(sceneMeshCollection->data()[i]);
//        sceneBoundingBox->Update(mr->GetTransformedPoints());
//    }
//}
//
//void LoadScene_Nefertiti(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//
//    FileReader reader = FileReader("./Assets/Models/Nefertiti.obj");
//    reader.Load();
//    Mesh dragonMesh = reader.Meshes()[0];
//    MeshRenderer dragon =
//        MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0.5, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//            &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(dragon);
//
//
//
//    for (int i = 0; i < sceneMeshCollection->size(); i++)
//    {
//        MeshRenderer* mr = &(sceneMeshCollection->data()[i]);
//        sceneBoundingBox->Update(mr->GetTransformedPoints());
//    }
//}
//
//void LoadScene_Knob(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//    sceneBoundingBox->Update((&plane)->GetTransformedPoints());
//
//    FileReader reader = FileReader("./Assets/Models/Knob.obj");
//    reader.Load();
//    for (int i = 0; i < reader.Meshes().size(); i++)
//    {
//        Mesh dragonMesh = reader.Meshes()[i];
//        MeshRenderer dragon =
//            MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0.5f, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//                &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//        sceneMeshCollection->push_back(dragon);
//        sceneBoundingBox->Update((&dragon)->GetTransformedPoints());
//    }
//}
//
//void LoadScene_Bunny(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//
//    FileReader reader = FileReader("./Assets/Models/Bunny.obj");
//    reader.Load();
//    Mesh dragonMesh = reader.Meshes()[0];
//    MeshRenderer dragon =
//        MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//            &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(dragon);
//
//
//
//    for (int i = 0; i < sceneMeshCollection->size(); i++)
//    {
//        MeshRenderer* mr = &(sceneMeshCollection->data()[i]);
//        sceneBoundingBox->Update(mr->GetTransformedPoints());
//    }
//}
//
//void LoadScene_Jinx(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//    sceneBoundingBox->Update((&plane)->GetTransformedPoints());
//
//    FileReader reader = FileReader("./Assets/Models/Jinx.obj");
//    reader.Load();
//    for (int i = 0; i < reader.Meshes().size(); i++)
//    {
//        Mesh dragonMesh = reader.Meshes()[i];
//        MeshRenderer dragon =
//            MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
//                &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//        sceneMeshCollection->push_back(dragon);
//        sceneBoundingBox->Update((&dragon)->GetTransformedPoints());
//    }
//   
//}
//
//void LoadScene_Engine(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//    sceneBoundingBox->Update((&plane)->GetTransformedPoints());
//
//    FileReader reader = FileReader("./Assets/Models/Engine.obj");
//    reader.Load();
//    for (int i = 0; i < reader.Meshes().size(); i++)
//    {
//        Mesh dragonMesh = reader.Meshes()[i];
//        MeshRenderer dragon =
//            MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(0.6, 0.6, 0.6),
//                &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::PureWhite);
//
//        sceneMeshCollection->push_back(dragon);
//        sceneBoundingBox->Update((&dragon)->GetTransformedPoints());
//    }
//
//}
//
void LoadScene_AoTest(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
   
    FileReader reader = FileReader("./Assets/Models/aoTest.obj");
    reader.Load();
    Mesh dragonMesh = reader.Meshes()[0];
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>
    (glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
        dragonMesh, shadersCollection->at("LIT_WITH_SHADOWS_SSAO").get(), shadersCollection->at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));

}

void LoadSceneFromPath(const char* path, SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection, const Material& material)
{
    FileReader reader = FileReader(path);
    reader.Load();
    for (int i = 0; i < reader.Meshes().size(); i++)
    {
        Mesh mesh = reader.Meshes()[i];
        sceneMeshCollection.AddToCollection(
            std::make_shared<MeshRenderer>(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(1.0, 1.0, 1.0),
               mesh, shadersCollection->at("LIT_WITH_SHADOWS_SSAO").get(), shadersCollection->at("LIT_WITH_SSAO").get(), material));
        
        sceneMeshCollection.at(sceneMeshCollection.size()-1).get()->SetTransformation(reader.Transformations().at(i));
    }
}

void LoadSceneFromPath(const char* path, SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
    FileReader reader = FileReader(path);
    reader.Load();
    for (int i = 0; i < reader.Meshes().size(); i++)
    {
        Mesh mesh = reader.Meshes()[i];
        sceneMeshCollection.AddToCollection(
            std::make_shared<MeshRenderer>(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(1.0, 1.0, 1.0),
                mesh, shadersCollection->at("LIT_WITH_SHADOWS_SSAO").get(), shadersCollection->at("LIT_WITH_SSAO").get()));

        sceneMeshCollection.at(sceneMeshCollection.size() - 1).get()->SetTransformation(reader.Transformations().at(i));
    }
}

//
//void LoadScene_Porsche(std::vector<MeshRenderer>* sceneMeshCollection, std::map<std::string, ShaderBase>* shadersCollection, BoundingBox<glm::vec3>* sceneBoundingBox)
//{
//    Mesh planeMesh = Mesh::Box(1, 1, 1);
//    MeshRenderer plane =
//        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
//            &planeMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::MatteGray);
//
//    sceneMeshCollection->push_back(plane);
//    sceneBoundingBox->Update((&plane)->GetTransformedPoints());
//
//    FileReader reader = FileReader("./Assets/Models/Porsche911.obj");
//    reader.Load();
//    for (int i = 0; i < reader.Meshes().size(); i++)
//    {
//        Mesh dragonMesh = reader.Meshes()[i];
//        MeshRenderer dragon =
//            MeshRenderer(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(0.6, 0.6, 0.6),
//                &dragonMesh, &(*shadersCollection)["LIT_WITH_SHADOWS_SSAO"], &(*shadersCollection)["LIT_WITH_SSAO"], MaterialsCollection::PureWhite);
//
//        sceneMeshCollection->push_back(dragon);
//        sceneBoundingBox->Update((&dragon)->GetTransformedPoints());
//    }
//
//}

void LoadScene_TechnoDemon(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
   
    LoadSceneFromPath("../../Assets/Models/TechnoDemon.fbx", sceneMeshCollection, shadersCollection);

    // ___ Transformation ___ //
    // ---------------------- //
    glm::mat4 tr = glm::mat4(1.0);
    tr = glm::translate(tr, glm::vec3(0, 0, -0.06));
    tr = glm::scale(tr, glm::vec3(0.01));
    tr = glm::rotate(tr, glm::pi<float>() / 2.0f, glm::vec3(1.0, 0.0, 0.0));

    for (int i = 0; i < sceneMeshCollection.size(); i++)
        sceneMeshCollection.at(i).get()->Transform(tr);

    sceneMeshCollection.UpdateBBox();
    // ---------------------- //

    
    Material matBody = Material{ glm::vec4(1.0),0.9, 0.0, "TechnoDemon_Body" };
    Material matArm = Material{ glm::vec4(1.0),0.2, 1.0, "TechnoDemon_Arm" };
  
    sceneMeshCollection.at(5).get()->SetMaterial(matBody);
    sceneMeshCollection.at(6).get()->SetMaterial(matArm);
    sceneMeshCollection.at(3).get()->SetMaterial(matBody);
    sceneMeshCollection.at(4).get()->SetMaterial(matArm);

    // ----------------- //
    LoadPlane(sceneMeshCollection, shadersCollection, 5.0);
}

void LoadScene_UtilityKnife(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{

    LoadSceneFromPath("../../Assets/Models/UtilityKnife.fbx", sceneMeshCollection, shadersCollection);

    // ___ Transformation ___ //
    // ---------------------- //
    glm::mat4 tr = glm::mat4(1.0);
    tr = glm::scale(tr, glm::vec3(0.15f));
    tr = glm::rotate(tr, glm::pi<float>() / 4.0f, glm::vec3(1.0, 0.0, 0.0));
    tr = glm::rotate(tr, glm::pi<float>() / 8.0f, glm::vec3(0.0, 0.0, -1.0));
    
    for (int i = 0; i < sceneMeshCollection.size(); i++)
        sceneMeshCollection.at(i).get()->SetTransformation(tr);

    sceneMeshCollection.UpdateBBox();


    // ___ Material ___ //
    // ---------------- //
    Material mat_0 = Material{ glm::vec4(1.0),1.0, 0.0, "UtilityKnife" };
    for (int i = 0; i < sceneMeshCollection.size(); i++)
        sceneMeshCollection.at(i).get()->SetMaterial(mat_0);
   
    // Plane
    // --------------
    float size = 8.0f;
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-size / 2.0f, -size / 2.0f, -2.5f), 0.0, glm::vec3(0, 0, 1), glm::vec3(size, size, size / 20.0f), planeMesh,
    (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
    (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));
}

void LoadScene_RadialEngine(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{

    LoadSceneFromPath("../../Assets/Models/RadialEngine.fbx", sceneMeshCollection, shadersCollection);

    // ___ Transformation ___ //
    // ---------------------- //
    glm::mat4 tr = glm::mat4(1.0);
    tr = glm::translate(tr, glm::vec3(0, 0, -1.0));
    tr = glm::scale(tr, glm::vec3(0.015f));
    tr = glm::rotate(tr, glm::pi<float>() / 2.0f, glm::vec3(1.0, 0.0, 0.0));

    for (int i = 0; i < sceneMeshCollection.size(); i++)
        sceneMeshCollection.at(i).get()->SetTransformation(tr);

    sceneMeshCollection.UpdateBBox();


    // ___ Material ___ //
    // ---------------- //
    Material mat_0 = Material{ glm::vec4(1.0),1.0, 0.0, "RadialEngine" };
    for (int i = 0; i < sceneMeshCollection.size(); i++)
        sceneMeshCollection.at(i).get()->SetMaterial(mat_0);

    // Plane
    // --------------
    float size = 8.0f;
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-size / 2.0f, -size / 2.0f, -2.5f), 0.0, glm::vec3(0, 0, 1), glm::vec3(size, size, size / 20.0f), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));
}

void SetupScene(
    SceneMeshCollection& sceneMeshCollection, 
    std::map<std::string, std::shared_ptr<MeshShader>> *meshShadersCollection,
    std::map<std::string, std::shared_ptr<WiresShader>>* wiresShadersCollection
)
{

    //LoadScene_CubicBezier(sceneMeshCollection, wiresShadersCollection);
    //LoadScene_Hilbert(sceneMeshCollection, wiresShadersCollection);
    //LoadScene_PoissonDistribution(sceneMeshCollection, wiresShadersCollection);
    //LoadPlane(sceneMeshCollection, meshShadersCollection, 15.0, 0.0f);
    LoadScene_PbrTestSpheres(sceneMeshCollection, meshShadersCollection);
    //LoadScene_PbrTestTeapots(sceneMeshCollection, meshShadersCollection);
    //LoadScene_PbrTestKnobs(sceneMeshCollection, meshShadersCollection);
    //LoadSceneFromPath("../../Assets/Models/Teapot.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::ShinyRed);
    //LoadScene_NormalMapping(sceneMeshCollection, meshShadersCollection);
    //LoadScene_TechnoDemon(sceneMeshCollection, meshShadersCollection);
    //LoadScene_RadialEngine(sceneMeshCollection, meshShadersCollection);
    //LoadScene_UtilityKnife(sceneMeshCollection, meshShadersCollection);
    //LoadSceneFromPath("../../Assets/Models/aoTest.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
    //LoadSceneFromPath("../../Assets/Models/RadialEngine.fbx", sceneMeshCollection, meshShadersCollection);
    //LoadSceneFromPath("./Assets/Models/Draenei.fbx", sceneMeshCollection, shadersCollection, Material{glm::vec4(1.0), glm::vec4(1.0), 64});
    //LoadSceneFromPath("../../Assets/Models/TestPCSS.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::ShinyRed);
     //LoadSceneFromPath("../../Assets/Models/Dragon.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
     //LoadSceneFromPath("../../Assets/Models/Sponza.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
    //LoadSceneFromPath("../../Assets/Models/Knob.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
    //LoadSceneFromPath("../../Assets/Models/trees.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::MatteGray);
    //LoadSceneFromPath("../../Assets/Models/OldBridge.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::MatteGray);
    //LoadSceneFromPath("./Assets/Models/Engine.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::PlasticGreen);
    //LoadScene_ALotOfMonkeys(sceneMeshCollection, meshShadersCollection);
    //LoadScene_Primitives(sceneMeshCollection, shadersCollection);
    //LoadScene_PCSStest(sceneMeshCollection, meshShadersCollection);
    //LoadScene_Cadillac(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Dragon(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Nefertiti(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Bunny(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Jinx(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Engine(sceneMeshCollection, meshShadersCollection, sceneBoundingBox);
    //LoadScene_Knob(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_AoTest(sceneMeshCollection, shadersCollection);
    //LoadScene_Porsche(sceneMeshCollection, shadersCollection, sceneBoundingBox);

}

std::map<std::string, std::shared_ptr<WiresShader>> InitializeWiresShaders()
{
    std::shared_ptr<WiresShader> points{ std::make_shared<WiresShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_MATERIAL",
            "CALC_UNLIT_MAT"
            }),
            WireNature::POINTS
    ) };

    std::shared_ptr<WiresShader> lines{ std::make_shared<WiresShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_MATERIAL",
            "CALC_UNLIT_MAT"
            }),
            WireNature::LINES
    ) };

    std::shared_ptr<WiresShader> lineStrip{ std::make_shared<WiresShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_MATERIAL",
            "CALC_UNLIT_MAT"
            }),
            WireNature::LINE_STRIP
    ) };

    return std::map<std::string, std::shared_ptr<WiresShader>>
    {
        { "POINTS",     points },
        { "LINE_STRIP", lineStrip },
        { "LINES",      lines }
    };
}

std::map<std::string, std::shared_ptr<MeshShader>> InitializeMeshShaders()
{
    std::shared_ptr<MeshShader> basicLit{ std::make_shared<MeshShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_LIGHTS",
            "DEFS_MATERIAL",
            "CALC_LIT_MAT"
            }
    )) };

    std::shared_ptr<MeshShader> basicLit_withShadows{ std::make_shared<MeshShader>(
        std::vector<std::string>(
            {
            "DEFS_SHADOWS",
            "CALC_SHADOWS"
            }
            ),
        std::vector<std::string>(
            {
            "DEFS_LIGHTS",
            "DEFS_MATERIAL",
            "DEFS_SHADOWS",
            "CALC_LIT_MAT",
            "CALC_SHADOWS",
            }
    )) };

    std::shared_ptr<MeshShader> basicLit_withShadows_ssao{ std::make_shared<MeshShader>(
        std::vector<std::string>(
            {
            "DEFS_SHADOWS",
            "CALC_SHADOWS",
            }
            ),
        std::vector<std::string>(
            {
            "DEFS_LIGHTS",
            "DEFS_MATERIAL",
            "DEFS_SHADOWS",
            "DEFS_SSAO",
            "CALC_LIT_MAT",
            "CALC_SHADOWS",
            "CALC_SSAO",
            }
    ) )};

    std::shared_ptr<MeshShader> basicLit_withSsao{ std::make_shared<MeshShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_LIGHTS",
            "DEFS_MATERIAL",
            "DEFS_SSAO",
            "CALC_LIT_MAT",
            "CALC_SSAO",
            }
    )) };

    std::shared_ptr<MeshShader> basicUnlit{ std::make_shared<MeshShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_MATERIAL",
            "CALC_UNLIT_MAT"
            }
    )) };

    std::shared_ptr<MeshShader> viewNormals{ std::make_shared<MeshShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_NORMALS",
            "CALC_NORMALS"
            }
    )) };


    return std::map<std::string, std::shared_ptr<MeshShader>>
    {

        { "LIT",                    basicLit                   },
        { "LIT_WITH_SSAO",          basicLit_withSsao          },
        { "UNLIT",                  basicUnlit                 },
        { "LIT_WITH_SHADOWS",       basicLit_withShadows       },
        { "LIT_WITH_SHADOWS_SSAO",  basicLit_withShadows_ssao  },
        { "VIEWNORMALS",            viewNormals                }

    };
}

std::map<std::string, std::shared_ptr<ShaderBase>> InitializePostProcessingShaders()
{
    std::shared_ptr<PostProcessingShader> hbaoShader{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_SSAO",
            "CALC_HBAO"
            }
    )) };


    std::shared_ptr<PostProcessingShader> ssaoShader{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_SSAO",
            "CALC_SSAO"
            }
    )) };

    std::shared_ptr<PostProcessingShader> ssaoViewPos{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_SSAO",
            "CALC_POSITIONS"
            }
    )) };

    std::shared_ptr<PostProcessingShader> blur{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_BLUR",
            "CALC_BLUR"
            }
    )) };
    std::shared_ptr<PostProcessingShader> gaussianBlur{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_GAUSSIAN_BLUR",
            "CALC_GAUSSIAN_BLUR"
            }
    )) };
    std::shared_ptr<PostProcessingShader> toneMappingAndGammaCorrection{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_TONE_MAPPING_AND_GAMMA_CORRECTION",
            "CALC_TONE_MAPPING_AND_GAMMA_CORRECTION"
            }
    )) };

    // Source: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
    std::shared_ptr<PostProcessingShader> integrateBRDF{ std::make_shared<PostProcessingShader>(
        std::vector<std::string>({/* NO VERTEX_SHADER EXPANSIONS */ }),
        std::vector<std::string>(
            {
            "DEFS_BRDF_LUT",
            "CALC_BRDF_LUT"
            }
    )) };


    return std::map<std::string, std::shared_ptr<ShaderBase>>
    {

       { "SSAO",                                    ssaoShader                      },
       { "HBAO",                                    hbaoShader                      },
       { "SSAO_VIEWPOS",                            ssaoViewPos                     },
       { "BLUR",                                    blur                            },
       { "GAUSSIAN_BLUR",                           gaussianBlur                    },
       { "TONE_MAPPING_AND_GAMMA_CORRECTION",       toneMappingAndGammaCorrection   },
    };
}


void BindUBOs(std::vector<UniformBufferObject>& uboCollection)
{
    for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
        uboCollection.at(i).BindingPoint(static_cast<UBOBinding>(i));
}
void FreeUBOs(std::vector<UniformBufferObject>& uboCollection)
{
    uboCollection.clear();
}

void UpdateMatricesUBO(UniformBufferObject& ubo, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float near, float far)
{
    ubo.SetData<float>(34, NULL);
    ubo.SetSubData(0, 16, glm::value_ptr(viewMatrix));
    ubo.SetSubData(16, 16, glm::value_ptr(projectionMatrix));
    ubo.SetSubData(32, 1, &near);
    ubo.SetSubData(33, 1, &far);
}

void UpdateMatricesUBO(UniformBufferObject& ubo, const SceneParams& sceneParams)
{
    UpdateMatricesUBO(ubo, sceneParams.viewMatrix, sceneParams.projectionMatrix, sceneParams.cameraNear, sceneParams.cameraFar);
}

void UpdateLightsUBO(UniformBufferObject& ubo, SceneLights& lights, Camera& camera)
{
    ubo.SetData<float>(56, NULL);

    // Directional Light
    ubo.SetSubData(0, 4, glm::value_ptr(lights.Directional.Direction));
    ubo.SetSubData(4, 4, glm::value_ptr(lights.Directional.Diffuse));
    ubo.SetSubData(8, 4, glm::value_ptr(lights.Directional.Specular));

    // Ambient light
    ubo.SetSubData(12, 4, glm::value_ptr(lights.Ambient.Ambient));

    // Point Lights (See MAX_POINT_LIGHTS in glsl code)
    for (int i = 0; i < PointLight::MAX_POINT_LIGHTS; i++)
    {
        ubo.SetSubData(16 + i * 12, 4, glm::value_ptr(lights.Points[i].Color));
        ubo.SetSubData(20 + i * 12, 3, glm::value_ptr(lights.Points[i].Position));
        ubo.SetSubData(23 + i * 12, 1, &lights.Points[i].Radius);

        float invSqrRadius = 1.0f / (lights.Points[i].Radius * lights.Points[i].Radius);
        ubo.SetSubData(24 + i * 12, 1, &invSqrRadius);
    }

    // Eye position
    ubo.SetSubData(52, 4, glm::value_ptr(camera.Position));
}

void UpdateShadowsUBO(UniformBufferObject& ubo, SceneLights& lights)
{
    ubo.SetData<float>(46, NULL);

    ubo.SetSubData(0, 16, glm::value_ptr(lights.Directional.LightSpaceMatrix));
    ubo.SetSubData(16, 1, &lights.Directional.Bias);
    ubo.SetSubData(17, 1, &lights.Directional.SlopeBias);
    ubo.SetSubData(20, 1, &lights.Points[0].Bias);
    ubo.SetSubData(24, 1, &lights.Points[1].Bias);
    ubo.SetSubData(28, 1, &lights.Points[2].Bias);
    ubo.SetSubData(32, 1, &lights.Points[0].SlopeBias);
    ubo.SetSubData(36, 1, &lights.Points[1].SlopeBias);
    ubo.SetSubData(40, 1, &lights.Points[2].SlopeBias);
    ubo.SetSubData(44, 1, &lights.Directional.Softness);
    ubo.SetSubData(45, 1, &lights.Directional.ShadowMapToWorld);
}

void UpdateAoUBO(UniformBufferObject& ubo, SceneLights& lights)
{
    ubo.SetData<float>(1, NULL);
    ubo.SetSubData(0, 1, &lights.Ambient.aoStrength);
}


void DrawEnvironment(
    const Camera& camera, const SceneParams& sceneParams, const VertexAttribArray& unitCubeVAO,  const ShaderBase& environmentShader)
{
   
#if GFX_STOPWATCH
    Profiler::Instance().StartNamedStopWatch("Environment");
#endif


    OGLUtils::CheckOGLErrors();
    glm::mat4 transf = glm::mat4(1.0);

    transf[0] =  sceneParams.viewMatrix[0];
    transf[1] =  -sceneParams.viewMatrix[2];
    transf[2] =  sceneParams.viewMatrix[1];
    
    glm::mat4 proj = glm::perspective(
        glm::radians(90.0f),
        sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
        sceneParams.cameraNear, sceneParams.cameraFar);

    OGLUtils::CheckOGLErrors();

    environmentShader.SetCurrent();


    OGLUtils::CheckOGLErrors();


    glUniformMatrix4fv(environmentShader.UniformLocation("u_model"), 1, GL_FALSE, glm::value_ptr(transf));
    glUniformMatrix4fv(environmentShader.UniformLocation("u_projection"), 1, GL_FALSE, glm::value_ptr(proj));

    if (sceneParams.environment.useSkyboxTexture && sceneParams.environment.RadianceMap.has_value())
    {
        glActiveTexture(GL_TEXTURE0);
        sceneParams.environment.Skybox.value().Bind();
        glUniform1i(environmentShader.UniformLocation("EnvironmentMap"), 0);
        glUniform1i(environmentShader.UniformLocation("u_hasEnvironmentMap"), true);
    }
    else
        glUniform1i(environmentShader.UniformLocation("u_hasEnvironmentMap"), false);


    OGLUtils::CheckOGLErrors();


    // Gamma correction
    glUniform1ui(environmentShader.UniformLocation("u_doGammaCorrection"), sceneParams.postProcessing.GammaCorrection);
    glUniform1f(environmentShader.UniformLocation("u_gamma"), 2.2f);

    //  Equator and Poles colors to be interpolated
    glUniform3fv(environmentShader.UniformLocation("u_equator_color"), 1,  glm::value_ptr(sceneParams.environment.EquatorColor));
    glUniform3fv(environmentShader.UniformLocation("u_north_color"), 1, glm::value_ptr(sceneParams.environment.NorthColor));
    glUniform3fv(environmentShader.UniformLocation("u_south_color"), 1, glm::value_ptr(sceneParams.environment.SouthColor));

    OGLUtils::CheckOGLErrors();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);

    unitCubeVAO.Bind();
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    unitCubeVAO.UnBind();

    if (sceneParams.environment.useSkyboxTexture)
        sceneParams.environment.Skybox.value().UnBind();

    glDepthFunc(GL_LESS);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

#if GFX_STOPWATCH
    Profiler::Instance().StopNamedStopWatch("Environment");
#endif
}


void ComputeDirectionalShadowMap(SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection,
    std::vector<UniformBufferObject>& sceneUniformBuffers, const ShaderBase& shaderForShadows, const FrameBuffer<OGLTexture2D>& shadowFBO)
{
    glm::mat4
        viewShadow, projShadow;

    Utils::GetDirectionalShadowMatrices(sceneParams.sceneLights.Directional.Position, sceneParams.sceneLights.Directional.Direction, sceneMeshCollection.GetSceneBBPoints(), viewShadow, projShadow);

    sceneParams.sceneLights.Directional.LightSpaceMatrix = projShadow * viewShadow;
    sceneParams.viewMatrix = viewShadow;
    sceneParams.projectionMatrix = projShadow;
    sceneParams.sceneLights.Directional.ShadowMapToWorld = (2.0f / projShadow[0][0]);
    UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);

    shadowFBO.Bind(true, true);

    glViewport(0, 0, directionalShadowMapResolution, directionalShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    OGLUtils::CheckOGLErrors();

    for (int i = 0; i < sceneMeshCollection.size(); i++)
    {
        if (sceneMeshCollection.at(i).get()->ShouldDrawForShadows())
            sceneMeshCollection.at(i).get()->DrawCustom(&shaderForShadows);
    }

    sceneParams.sceneLights.Directional.ShadowMapId = shadowFBO.DepthTextureId();
    shadowFBO.UnBind();
}

void ComputePointShadowMap(PointLight& light, const SceneMeshCollection& sceneMeshCollection,
    std::vector<UniformBufferObject>& sceneUniformBuffers, const ShaderBase& shaderForShadows, const FrameBuffer<OGLTextureCubemap>& shadowFBO)
{
    if (light.Color.w == 0.0f)
        return;

    std::vector<glm::mat4> shadowTransforms;
    
    float near;
    float far;

    Utils::GetPointShadowMatrices(light.Position, light.Radius, near, far,light.LightSpaceMatrix);

    shaderForShadows.SetCurrent();

    glUniform1f(shaderForShadows.UniformLocation("far"), far);
    glUniform3fv(shaderForShadows.UniformLocation("lightPos"), 1, glm::value_ptr(light.Position));
    glUniformMatrix4fv(shaderForShadows.UniformLocation("shadowTransforms[0]"), 1, false, glm::value_ptr(light.LightSpaceMatrix[0]));
    glUniformMatrix4fv(shaderForShadows.UniformLocation("shadowTransforms[1]"), 1, false, glm::value_ptr(light.LightSpaceMatrix[1]));
    glUniformMatrix4fv(shaderForShadows.UniformLocation("shadowTransforms[2]"), 1, false, glm::value_ptr(light.LightSpaceMatrix[2]));
    glUniformMatrix4fv(shaderForShadows.UniformLocation("shadowTransforms[3]"), 1, false, glm::value_ptr(light.LightSpaceMatrix[3]));
    glUniformMatrix4fv(shaderForShadows.UniformLocation("shadowTransforms[4]"), 1, false, glm::value_ptr(light.LightSpaceMatrix[4]));
    glUniformMatrix4fv(shaderForShadows.UniformLocation("shadowTransforms[5]"), 1, false, glm::value_ptr(light.LightSpaceMatrix[5]));

    shadowFBO.Bind(true, true);

    glViewport(0, 0, pointShadowMapResolution, pointShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    OGLUtils::CheckOGLErrors();

    for (int i = 0; i < sceneMeshCollection.size(); i++)
    {
        if (sceneMeshCollection.at(i).get()->ShouldDrawForShadows())
            sceneMeshCollection.at(i).get()->DrawCustom((ShaderBase*)&shaderForShadows);
    }

    light.ShadowMapId = shadowFBO.DepthTextureId();
    shadowFBO.UnBind();
}

void ShadowPass(
    SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection, std::vector<UniformBufferObject>& sceneUniformBuffers,
    const MeshShader& shadowShaderDirectional, const ShaderBase& shadowShaderPoint , const FrameBuffer<OGLTexture2D>& directionalShadowFBO, const std::vector<FrameBuffer<OGLTextureCubemap>>& pointShadowFBOs)
{
#if GFX_STOPWATCH
    Profiler::Instance().StartNamedStopWatch("Shadows");
#endif

    ComputeDirectionalShadowMap(sceneParams, sceneMeshCollection, sceneUniformBuffers, shadowShaderDirectional, directionalShadowFBO);
    
    for(int i =0; i<PointLight::MAX_POINT_LIGHTS; i++)
        ComputePointShadowMap(sceneParams.sceneLights.Points[i], sceneMeshCollection, sceneUniformBuffers, shadowShaderPoint, pointShadowFBOs[i]);

#if GFX_STOPWATCH
    Profiler::Instance().StopNamedStopWatch("Shadows");
#endif
}

void AmbienOcclusionPass(
    SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection, std::vector<UniformBufferObject>& sceneUniformBuffers, 
    const MeshShader& shaderForDepthPass, const FrameBuffer<OGLTexture2D>& ssaoFBO, const PostProcessingUnit& postProcessingUnit)
{

#if GFX_STOPWATCH 
    Profiler::Instance().StartNamedStopWatch("AO");
#endif

    ssaoFBO.Bind(true, true);
    glDrawBuffer(GL_COLOR_ATTACHMENT0 + 1);
    glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    sceneParams.viewMatrix = camera.GetViewMatrix();
    Utils::GetTightNearFar(sceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix, 0.001f, sceneParams.cameraNear, sceneParams.cameraFar);

    sceneParams.projectionMatrix = glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight, sceneParams.cameraNear, sceneParams.cameraFar); //TODO: near is brutally hard coded in PCSS calculations (#define NEAR 0.1)

    UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);
    
    OGLUtils::CheckOGLErrors();

    for (int i = 0 ; i < sceneMeshCollection.size() ; i++)
    {
        sceneMeshCollection.at(i).get()->DrawCustom((ShaderBase *)&shaderForDepthPass);
    }

    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    ssaoFBO.UnBind();

    postProcessingUnit.ComputeAO(AOShaderFromItem(ao_comboBox_current_item), ssaoFBO, sceneParams, sceneParams.viewportWidth, sceneParams.viewportHeight);

    // Blur 
    postProcessingUnit.BlurTexture(ssaoFBO, 0, sceneParams.sceneLights.Ambient.aoBlurAmount, sceneParams.viewportWidth, sceneParams.viewportHeight);

    sceneParams.sceneLights.Ambient.AoMapId = ssaoFBO.ColorTextureId();

#if GFX_STOPWATCH 
   Profiler::Instance().StopNamedStopWatch("AO");
#endif
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "TestApp_OpenGL", NULL, NULL);

    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }


    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, keyboard_button_callback);

    sceneParams.viewportWidth = 200;
    sceneParams.viewportHeight = 800;
    glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);

    // Scene View and Projection matrices
    sceneParams.viewMatrix = glm::mat4(1);
    sceneParams.cameraNear = 0.1f, sceneParams.cameraFar = 50.0f;
    sceneParams.projectionMatrix = glm::perspective(glm::radians(45.0f), sceneParams.viewportHeight / (float)sceneParams.viewportWidth, sceneParams.cameraNear, sceneParams.cameraFar);

    // Shaders Collections 
    static std::map<std::string, std::shared_ptr<MeshShader>> MeshShaders = InitializeMeshShaders();
    static std::map<std::string, std::shared_ptr<WiresShader>> WiresShaders = InitializeWiresShaders();
    static std::map<std::string, std::shared_ptr<ShaderBase>> PostProcessingShaders = InitializePostProcessingShaders();
    static ShaderBase PointLightShadowMapShader = ShaderBase(
        VertexSource_Shadows::EXP_VERTEX,
        GeometrySource_Shadows::EXP_GEOMETRY,
        FragmentSource_Shadows::EXP_FRAGMENT);

    // Environment
    std::string currPath = std::filesystem::current_path().string();

    sceneParams.environment.Skybox = 
        OGLTextureCubemap("../../Assets/Environments/Outdoor/Skybox", TextureFiltering::Linear, TextureFiltering::Linear);

    sceneParams.environment.IrradianceMap =
        OGLTextureCubemap("../../Assets/Environments/Outdoor/IrradianceMap", TextureFiltering::Linear, TextureFiltering::Linear);

    sceneParams.environment.RadianceMap =
        OGLTextureCubemap("../../Assets/Environments/Outdoor/Radiance", TextureFiltering::Linear_Mip_Linear, TextureFiltering::Linear);

    float l = 2.0;
    std::vector<glm::vec3> vertices =
        std::vector<glm::vec3>{

        glm::vec3(-l, -l, -l),
        glm::vec3( l, -l, -l),
        glm::vec3( l, -l,  l),
        glm::vec3(-l, -l,  l),

        glm::vec3(-l,  l, -l),
        glm::vec3( l,  l, -l),
        glm::vec3( l,  l,  l),
        glm::vec3(-l,  l,  l),
    };
   
    std::vector<int> indices =
        std::vector<int>{

        0, 1, 2,
        0, 2, 3,

        1, 6, 2,
        1, 5, 6, 

        0, 3, 7, 
        0, 7, 4, 

        3, 2, 6, 
        3, 6, 7, 

        5, 1, 0, 
        5, 0, 4,

        5, 7, 6, 
        5, 4, 7,
    };

    VertexBufferObject envCube_vbo = VertexBufferObject(VertexBufferObject::VBOType::StaticDraw);
    IndexBufferObject envCube_ebo = IndexBufferObject(IndexBufferObject::EBOType::StaticDraw);
    VertexAttribArray envCube_vao = VertexAttribArray();
    envCube_vbo.SetData(24, flatten3(vertices).data());
    envCube_ebo.SetData(36, indices.data());
    envCube_vao.SetAndEnableAttrib(envCube_vbo.ID(), 0, 3, false, 0, 0);
    envCube_vao.SetIndexBuffer(envCube_ebo.ID());

    ShaderBase environmentShader = ShaderBase(VertexSource_Environment::EXP_VERTEX, FragmentSource_Environment::EXP_FRAGMENT);
    
    // Initial skybox default params
    sceneParams.environment.NorthColor = glm::vec3(0.8, 0.8, 0.8);
    sceneParams.environment.EquatorColor = glm::vec3(0.5, 0.5, 0.5);
    sceneParams.environment.SouthColor = glm::vec3(0.2, 0.2, 0.2);


    // Setup Scene
    SceneMeshCollection sceneMeshCollection;
    SetupScene(sceneMeshCollection, &MeshShaders, &WiresShaders);

    camera.CameraOrigin = sceneMeshCollection.GetSceneBBCenter();
    camera.ProcessMouseMovement(0, 0); //TODO: camera.Update()

    // DEBUG
    Mesh lightArrow{ Mesh::Arrow(0.3, 1, 0.5, 0.5, 16) };
    MeshRenderer lightMesh =
        MeshRenderer(glm::vec3(0, 0, 0), 0, glm::vec3(0, 1, 1), glm::vec3(0.3, 0.3, 0.3),
            lightArrow, MeshShaders.at("UNLIT").get(), MeshShaders.at("UNLIT").get(), MaterialsCollection::PureWhite);

    sceneParams.grid.Min = glm::vec2(-10.f, -10.0f);
    sceneParams.grid.Max = glm::vec2(10.f, 10.0f);
    sceneParams.grid.Color = glm::vec4(0.5f, 0.5f, 0.5f, 0.6f);
    sceneParams.grid.Step = 1.0f;

    WiresRenderer grid =
        WiresRenderer(glm::vec3(0, 0, 0), 0.0f, glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), 
            Wire::Grid(sceneParams.grid.Min, sceneParams.grid.Max, (int)sceneParams.grid.Step),
            WiresShaders.at("LINES").get(), sceneParams.grid.Color);


    std::vector<glm::vec3> bboxLines = sceneMeshCollection.GetSceneBBLines();
    Wire bbWire = Wire(bboxLines, WireNature::LINES);
    WiresRenderer bbRenderer =
        WiresRenderer(glm::vec3(0, 0, 0), 0.0f, glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), bbWire,
            WiresShaders.at("LINES").get(), glm::vec4(1.0, 0.0, 1.0, 1.0));

    bool showWindow = true;

    //Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    //Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGui::StyleColorsClassic();

    //Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Lights
    // ---------------------
    sceneParams.sceneLights.Ambient.Ambient = glm::vec4(1, 1, 1, 0.6);
    sceneParams.environment.intensity = 0.2f;
    sceneParams.sceneLights.Ambient.aoStrength = 1.0;
    sceneParams.sceneLights.Ambient.aoRadius = 0.5;
    sceneParams.sceneLights.Ambient.aoSamples = 8;
    sceneParams.sceneLights.Ambient.aoSteps = 8;
    sceneParams.sceneLights.Ambient.aoBlurAmount = 3;
    sceneParams.sceneLights.Directional.Direction = glm::vec3(0.9, 0.9, -0.9);
    sceneParams.sceneLights.Directional.Diffuse = glm::vec4(1.0, 1.0, 1.0, 0.75);
    sceneParams.sceneLights.Directional.Specular = glm::vec4(1.0, 1.0, 1.0, 0.75);
    sceneParams.sceneLights.Directional.Bias = 0.002f;
    sceneParams.sceneLights.Directional.SlopeBias = 0.015f;
    sceneParams.sceneLights.Directional.Softness = 0.01f;
    
    sceneParams.sceneLights.Points[0] = PointLight(glm::vec4(1.0, 0.9, 0.9, 15.0), glm::vec3( 2.0, 0.0, 5.0));
    //sceneParams.sceneLights.Points[1] = PointLight(glm::vec4(1.0, 0.0, 0.0, 12.0), glm::vec3(1.0, 1.0, 3.0));
    //sceneParams.sceneLights.Points[2] = PointLight(glm::vec4(0.0, 0.0, 1.0, 12.0), glm::vec3(-1.0, 1.0, 3.0));
   
    sceneParams.drawParams.doShadows = true;

    // Shadow Maps
    // ----------------------
    FrameBuffer directionalShadowFBO = FrameBuffer(directionalShadowMapResolution, directionalShadowMapResolution, false, 0, true);
    std::vector<FrameBuffer<OGLTextureCubemap>> pointShadowFBOs = std::vector<FrameBuffer<OGLTextureCubemap>>{};
    for (int i = 0; i < 16; i++)
    {
        pointShadowFBOs.push_back(FrameBuffer<OGLTextureCubemap>(pointShadowMapResolution, pointShadowMapResolution, false, 0, true));
    }

    // Noise Textures for PCSS 
    // ----------------------
    sceneParams.noiseTextures.push_back(OGLTexture2D(
            9, 9,
            TextureInternalFormat::R_16f,
            GetUniformDistributedSamples1D(81, -1.0, 1.0).data(), GL_RED, GL_FLOAT,
            TextureFiltering::Nearest, TextureFiltering::Nearest,
            TextureWrap::Repeat, TextureWrap::Repeat));
    
    sceneParams.noiseTextures.push_back(OGLTexture2D(
        10, 10,
        TextureInternalFormat::R_16f,
        GetUniformDistributedSamples1D(100, -1.0, 1.0).data(), GL_RED, GL_FLOAT,
        TextureFiltering::Nearest, TextureFiltering::Nearest,
        TextureWrap::Repeat, TextureWrap::Repeat));
    
    sceneParams.noiseTextures.push_back(OGLTexture2D(
        11, 11,
        TextureInternalFormat::R_16f,
        GetUniformDistributedSamples1D(121, -1.0, 1.0).data(), GL_RED, GL_FLOAT,
        TextureFiltering::Nearest, TextureFiltering::Nearest,
        TextureWrap::Repeat, TextureWrap::Repeat));

    std::vector<float> poissonSamples = 
        flatten2(GetPoissonDiskSamples2D(64, Domain2D{ DomainType2D::Disk, 1.0f }));
    sceneParams.poissonSamples = 
        OGLTexture1D(64, TextureInternalFormat::Rg_16f, poissonSamples.data());

    // AmbientOcclusion Map
    // --------------------
    FrameBuffer ssaoFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true, TextureInternalFormat::Rgb_16f);

    // Tone-mapping and gamma correction
    // ---------------------------------
    FrameBuffer mainFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, true);
    sceneParams.postProcessing.ToneMapping = false;
    sceneParams.postProcessing.Exposure = 1.0f;
    sceneParams.postProcessing.GammaCorrection = true;

    // Post processing unit (ao, blur, bloom, ...)
    // -------------------------------------------
    PostProcessingUnit postProcessingUnit(SSAO_BLUR_MAX_RADIUS, SSAO_MAX_SAMPLES);

    // Uniform buffer objects 
    // ----------------------
    std::vector<UniformBufferObject> sceneUniformBuffers{};
    for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
        sceneUniformBuffers.push_back(std::move(UniformBufferObject(UniformBufferObject::UBOType::StaticDraw)));
    
    BindUBOs(sceneUniformBuffers);

    // This should (does it?) be computed at each startup, since it should (does it?) 
    // be kept in sync with the PBR shaders
    postProcessingUnit.IntegrateBRDFintoLUT(sceneParams);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);


    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        glfwMakeContextCurrent(window);

        // Input procesing
        processInput(window);

        glfwPollEvents();

        if (showWindow)
        {
            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        glfwGetFramebufferSize(window, &sceneParams.viewportWidth, &sceneParams.viewportHeight);

        // SHADOW PASS ////////////////////////////////////////////////////////////////////////////////////////////////////
        
        sceneParams.sceneLights.Directional.Position = 
            sceneMeshCollection.GetSceneBBCenter() - (glm::normalize(sceneParams.sceneLights.Directional.Direction) * sceneMeshCollection.GetSceneBBSize() * 0.5f);

        if (sceneParams.drawParams.doShadows)
            ShadowPass(sceneParams, sceneMeshCollection, sceneUniformBuffers, *MeshShaders.at("UNLIT").get(), PointLightShadowMapShader, directionalShadowFBO, pointShadowFBOs);
        

        // AO PASS ////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        // Resize the ssao framebuffer if needed (window resized)
        if (ssaoFBO.Height() != sceneParams.viewportHeight || ssaoFBO.Width() != sceneParams.viewportWidth)
            ssaoFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true, TextureInternalFormat::Rgb_16f);

        AmbienOcclusionPass(sceneParams, sceneMeshCollection, sceneUniformBuffers, *MeshShaders.at("VIEWNORMALS").get(), ssaoFBO, postProcessingUnit);


        // Offscreen rendering to enable hdr and gamma correction.
        if (sceneParams.postProcessing.ToneMapping || sceneParams.postProcessing.GammaCorrection)
        {
            // Resize the main framebuffer if needed (window resized).
            if (mainFBO.Height() != sceneParams.viewportHeight || mainFBO.Width() != sceneParams.viewportWidth)
                mainFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, true);

            mainFBO.Bind();
        }
        glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ENVIRONMENT //////////////////////////////////////////////////////////////////////////////////////////////////////
        DrawEnvironment(camera, sceneParams, envCube_vao, environmentShader);


        // OPAQUE PASS /////////////////////////////////////////////////////////////////////////////////////////////////////

#if GFX_STOPWATCH
        Profiler::Instance().StartNamedStopWatch("Scene");
#endif
        sceneParams.viewMatrix = camera.GetViewMatrix();
        Utils::GetTightNearFar(sceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix, 0.001f, sceneParams.cameraNear, sceneParams.cameraFar);
        sceneParams.projectionMatrix = glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight, sceneParams.cameraNear, sceneParams.cameraFar); 

        UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);
        UpdateLightsUBO(sceneUniformBuffers.at(UBOBinding::Lights), sceneParams.sceneLights, camera);
        UpdateShadowsUBO(sceneUniformBuffers.at(UBOBinding::Shadows), sceneParams.sceneLights);
        UpdateAoUBO(sceneUniformBuffers.at(UBOBinding::AmbientOcclusion), sceneParams.sceneLights);

        // Draw grid before scene
        if (showGrid)
        {
            glDepthMask(GL_FALSE);
            float gridNear;
            float gridFar;
            std::vector<glm::vec3> gridPoints =
                std::vector<glm::vec3>
            {
                glm::vec3(sceneParams.grid.Min.x, sceneParams.grid.Min.y, 0.0f),
                    glm::vec3(sceneParams.grid.Min.x, sceneParams.grid.Max.y, 0.0f),
                    glm::vec3(sceneParams.grid.Max.x, sceneParams.grid.Max.y, 0.0f),
                    glm::vec3(sceneParams.grid.Max.x, sceneParams.grid.Min.y, 0.0f)

            };

            Utils::GetTightNearFar(
                gridPoints, sceneParams.viewMatrix, 0.1f, gridNear, gridFar
            );

            glm::mat4 gridProjAfterScene =
                glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
                    sceneParams.cameraFar, glm::max(sceneParams.cameraFar, gridFar));

            UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams.viewMatrix, gridProjAfterScene, gridNear, gridFar);

            grid.Draw(camera.Position, sceneParams);

            UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);
            
            glDepthMask(GL_TRUE);
        }


        for (int i = 0; i < sceneMeshCollection.size(); i++)
        {
            sceneMeshCollection.at(i).get()->Draw(camera.Position, sceneParams);
        }
        
#if GFX_STOPWATCH
        Profiler::Instance().StopNamedStopWatch("Scene");
#endif

        // DEBUG ////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if (showLights)
        {
            glm::quat orientation = glm::quatLookAt(-normalize(sceneParams.sceneLights.Directional.Direction), glm::vec3(0, 0, 1));
            lightMesh.Renderer::SetTransformation(sceneParams.sceneLights.Directional.Position, glm::angle(orientation), glm::axis(orientation), glm::vec3(0.3, 0.3, 0.3));
            lightMesh.Draw(camera.Position, sceneParams);
        }
        if (showGrid)
        {
            grid.Draw(camera.Position, sceneParams);

            glDisable(GL_DEPTH_TEST);

            float gridNear;
            float gridFar;
            std::vector<glm::vec3> gridPoints =
                std::vector<glm::vec3>
            {
                glm::vec3(sceneParams.grid.Min.x, sceneParams.grid.Min.y, 0.0f),
                    glm::vec3(sceneParams.grid.Min.x, sceneParams.grid.Max.y, 0.0f),
                    glm::vec3(sceneParams.grid.Max.x, sceneParams.grid.Max.y, 0.0f),
                    glm::vec3(sceneParams.grid.Max.x, sceneParams.grid.Min.y, 0.0f)

            };

            Utils::GetTightNearFar(
                gridPoints, sceneParams.viewMatrix, 0.1f, gridNear, gridFar
            );

            glm::mat4 gridProjAfterScene =
                glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
                    glm::max(0.0f, gridNear), sceneParams.cameraNear);

            UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams.viewMatrix, gridProjAfterScene, gridNear, gridFar);

            grid.Draw(camera.Position, sceneParams);

            UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);

            glEnable(GL_DEPTH_TEST);
        }
        if (showBoundingBox)
            bbRenderer.Draw(camera.Position, sceneParams);


        if (sceneParams.postProcessing.ToneMapping || sceneParams.postProcessing.GammaCorrection)
        {
            mainFBO.UnBind();

            // Problems when resizind the window:
            // really don't know why this is needed since DepthTesting is disabled 
            // when drawing the quad to apply the post effects...
            // In RenderDoc snapshots everything seems fine but it's not what you get.
            glClear(GL_DEPTH_BUFFER_BIT);

            postProcessingUnit.ApplyToneMappingAndGammaCorrection(mainFBO, 0,
                sceneParams.postProcessing.ToneMapping, sceneParams.postProcessing.Exposure,
                sceneParams.postProcessing.GammaCorrection, 2.2f);

        }

        if (showAO)
            ssaoFBO.CopyToOtherFbo(nullptr, true, 10, false, glm::vec2(0.0, 0.0), glm::vec2(sceneParams.viewportWidth, sceneParams.viewportHeight));

        // WINDOW /////////////////////////////////////////////////////////////////////////////////////////////////////
        if (showWindow)
        {
            ShowImGUIWindow();
            if (showStats)
                ShowStatsWindow();
        }

        // Rendering
        if (showWindow)
        {
            ImGui::Render();
            int display_w, display_h;
            //glfwGetFramebufferSize(window, &display_w, &display_h);
            //glViewport(0, 0, display_w, display_h);
            //glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        // Front and back buffers swapping
        glfwSwapBuffers(window);
    }

    // TODO: Why do I have to call clear() EXPLICITLY????
    // using a scope InitContext{ ... std::map<..., Shader> ...) }
    // apparently doesn't work ... ???

    // BIG TODO HERE: How to manage those resources????
    // explicitly call the destructor here is not good?
    MeshShaders.clear();
    WiresShaders.clear();
    PostProcessingShaders.clear();
    PointLightShadowMapShader.~ShaderBase();
    sceneParams.noiseTextures.~vector();
    sceneParams.poissonSamples.~optional();
    sceneMeshCollection.~SceneMeshCollection();
    lightMesh.~MeshRenderer();
    ssaoFBO.~FrameBuffer();
    directionalShadowFBO.~FrameBuffer();
    pointShadowFBOs.clear();
    mainFBO.~FrameBuffer();
    grid.~WiresRenderer();
    lightMesh.~MeshRenderer();
    bbRenderer.~WiresRenderer();
    environmentShader.~ShaderBase();
    envCube_ebo.~IndexBufferObject();
    envCube_vbo.~VertexBufferObject();
    envCube_vao.~VertexAttribArray();
    sceneParams.environment.~Environment();
    //gaussianKernelValuesTexture.~OGLTexture2D();
    postProcessingUnit.~PostProcessingUnit();
    FreeUBOs(sceneUniformBuffers);

    glfwTerminate();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

}

void ResizeResources(int width, int height)
{
    //ssaoFBO.FreeUnmanagedResources();
    //ssaoFBO = FrameBuffer(width, height, true, true);
}

void framebuffer_size_callback(GLFWwindow* window, int newWidth, int newHeight)
{
    sceneParams.viewportWidth = newWidth;
    sceneParams.viewportHeight = newHeight;
    ResizeResources(sceneParams.viewportWidth, sceneParams.viewportHeight);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    sceneParams.mousePosition = glm::ivec2((int)xpos, sceneParams.viewportHeight - (int)ypos);

    if (!firstMouse)
    {
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;
        camera.ProcessMouseMovement(xoffset, yoffset);
    }

}
void mouse_scroll_callback(GLFWwindow* window, double xpos, double ypos)
{
    
    camera.ProcessMouseScroll(ypos);

}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        firstMouse = true;
    }
}

void keyboard_button_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        ViewType vt;

        switch (key)
        {
        case GLFW_KEY_KP_1: vt = ViewType::Top; break;
        case GLFW_KEY_KP_3: vt = ViewType::Right; break;
        case GLFW_KEY_KP_7: vt = ViewType::Front; break;
        case GLFW_KEY_KP_9: vt = ViewType::Back; break;
        default:
            vt = ViewType::Top;
            break;
        }

        camera.SetView(vt);
    }
}




