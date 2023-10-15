#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <random>
#include<stack>

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
#include "GeometryHelper.h"
#include "SceneUtils.h"
#include "FrameBuffer.h"
#include "Scene.h"


// Debugging using feedback from shaders.
// --------------------------------------
//#define GFX_SHADER_DEBUG_VIZ
#include "Mesh.h"

#ifdef GFX_SHADER_DEBUG_VIZ

    #define SSBO_CAPACITY 100 // max debug items count

    #define DEBUG_DIRECTIONAL_LIGHT     0
    #define DEBUG_POINT_LIGHT           1
    #define DEBUG_DIRECTIONAL_SPLITS    2
    #define DEBUG_TAA                   3

    #define GFX_SHADER_DEBUG_VIZ  DEBUG_DIRECTIONAL_LIGHT
    
#endif
// --------------------------------------

#if GFX_STOPWATCH
    #include "Diagnostics.h"
#endif


using namespace OGLResources;

// CONSTANTS ======================================================
const char* glsl_version = "#version 130";

const unsigned int directionalShadowMapResolution = 2048;
const unsigned int pointShadowMapResolution = 1024;
const int SSAO_MAX_SAMPLES = 64;
const int SSAO_BLUR_MAX_RADIUS = 16;
const int TAA_JITTER_SAMPLES = 4;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_scroll_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void keyboard_button_callback(GLFWwindow* window, int key, int scancode, int action, int mods);


Camera camera = Camera( 25, 1, 1);

bool firstMouse = true;
int lastKey = -1;
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
            
            bool freeFlyCam = camera.GetNavigationMode() == NavigationType::Freefly;
            if (ImGui::Checkbox("FreeFly", &freeFlyCam))
                camera.SetNavigationMode(freeFlyCam ? NavigationType::Freefly : NavigationType::Turntable);


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
                    ImGui::DragFloat("Softness", &sceneParams.sceneLights.Directional.Softness, 0.01f, 0.0f, 0.5f);
                    ImGui::Checkbox("Splits", &sceneParams.sceneLights.Directional.doSplits);
                    ImGui::DragFloat("Lambda", &sceneParams.sceneLights.Directional.lambda, 0.05f, 0.0f, 1.0f);
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
                        ImGui::DragFloat("Size", &sceneParams.sceneLights.Points[i].Size, 0.01f, 0.0f, 0.5f);
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
            
            ImGui::Checkbox("GammaCorrection", &sceneParams.postProcessing.doGammaCorrection);

            if (ImGui::CollapsingHeader("TAA", ImGuiTreeNodeFlags_None))
            {
                ImGui::Checkbox("Enabled", &sceneParams.postProcessing.doTaa);
                ImGui::Checkbox("Use Velocity", &sceneParams.postProcessing.writeVelocity);
            }
            

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
    Mesh boxMesh = Mesh::Box(1, 1, 6);
    sceneMeshCollection.AddToCollection(
        std::make_shared<MeshRenderer>(glm::vec3(0, 0, 0), 0.0, glm::vec3(0, 0, 1), glm::vec3(1, 1, 1), boxMesh,
            (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
            (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed));

    Mesh planeMesh = Mesh::Box(8, 8, .1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-4, -4, 0.0f), 0.2, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PureWhite));
    return;

    Mesh coneMesh = Mesh::Cone(0.8, 2.6, 16);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(2, 3, 1), 1.5, glm::vec3(0, -1, 1), glm::vec3(1, 1, 1), coneMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed));
    
    
    Mesh cylMesh = Mesh::Cylinder(0.6, 3.6, 16);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-3, -1.5, 1), 1.1, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), cylMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen));

    Mesh sphereMesh = Mesh::Sphere(1.0, 32);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-2, 1.5, 1.8),0.0, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), sphereMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PureWhite));

  
    
   

    
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
            mr.SetMaterial(std::make_shared <Material>(Material{glm::vec4(1.0, 1.0, 1.0, 1.0), (i+1) / 5.0f, (j+1) / 5.0f }));

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
            mr.SetMaterial(std::make_shared <Material>(Material{ glm::vec4(1.0, 1.0, 1.0, 1.0), (i + 1) / 5.0f, (j + 1) / 5.0f }));

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
                mr.SetMaterial(std::make_shared <Material>(Material{ glm::vec4(1.0, 1.0, 1.0, 1.0), (i + 1) / 3.0f, (j + 1) / 3.0f }));

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
    for (auto& s : GetR2Sequence(16/*, Domain2D(DomainType2D::Square, 2.0f)*/))
        squareSamplesPts.push_back(glm::vec3(s*2.0f, 0.01f));

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

   /* Wire squareSamplesWire_32 = Wire(
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
    squareSamples_64.SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));*/

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_8)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_16)));
    //sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_32)));
    //sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(squareSamples_64)));

    // -----------------

   
}


// PSSM from: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
// ------------------------------------------------------------------------------------------------------------------------------------------
void GetSplitAtIndex(int index, int numberOfSplits, float lambda, float cameraNear, float cameraFar, float* splitStart)
{
    *splitStart =

        lambda * (cameraNear * glm::pow(cameraFar / cameraNear, (float)index / numberOfSplits)) +       // Logarithmic
        (1-lambda) * (cameraNear + (cameraFar - cameraNear) * ((float)index / numberOfSplits));         // Linear

}

BoundingBox<glm::vec3> GetSplitAABB_LightSpace(int index, int numberOfSplits, float lambda, float cameraNear, float cameraFar, float cameraFoVy, float cameraAspect, glm::mat4 viewMatrix, glm::mat4 lightMatrix/*, std::vector<glm::vec3>* worldSpacePts*/)
{
    // world space points
    std::vector<glm::vec3> frustumPts(8);

    float splitStart = 0;
    float splitEnd = 0;

    GetSplitAtIndex(index,     numberOfSplits, lambda, cameraNear, cameraFar, &splitStart);
    GetSplitAtIndex(index + 1, numberOfSplits, lambda, cameraNear, cameraFar, &splitEnd);


    float y = splitStart * glm::tan(cameraFoVy / 2.0f);
    float x = y * cameraAspect;

    frustumPts[0] = glm::vec3(x, y, -splitStart);
    frustumPts[1] = glm::vec3(x, -y, -splitStart);
    frustumPts[2] = glm::vec3(-x, y, -splitStart);
    frustumPts[3] = glm::vec3(-x, -y, -splitStart);

    y = splitEnd * glm::tan(cameraFoVy / 2.0f);
    x = y * cameraAspect;

    frustumPts[4] = glm::vec3(x, y, -splitEnd);
    frustumPts[5] = glm::vec3(x, -y, -splitEnd);
    frustumPts[6] = glm::vec3(-x, y, -splitEnd);
    frustumPts[7] = glm::vec3(-x, -y, -splitEnd);

    glm::mat4 invViewMat = glm::inverse(viewMatrix);


    //*worldSpacePts = std::vector<glm::vec3>(8);
    //for (int i = 0; i < 8; i++)
    //    (*worldSpacePts)[i] = invViewMat * glm::vec4(frustumPts[i], 1.0f);

    for (int i = 0; i < 8; i++)
        frustumPts[i] = lightMatrix * invViewMat * glm::vec4(frustumPts[i], 1.0f);

    return BoundingBox(frustumPts);
}

glm::mat4 GetCropMatrix(BoundingBox<glm::vec3> splitAABB_LightSpace, float lightNear, float lightFar)
{
    // Light space is built as the "view space" for light
    // so positive Z faces away from the target.
    glm::vec3 boxMin = splitAABB_LightSpace.Min();
    glm::vec3 boxMax = splitAABB_LightSpace.Max();

    boxMax.z = -lightNear;
    boxMin.z = -lightFar;

    float scaleX, scaleY, scaleZ;
    float offsetX, offsetY, offsetZ;
    scaleX = 2.0f / (boxMax.x - boxMin.x);
    scaleY = 2.0f / (boxMax.y - boxMin.y);
    scaleZ = 2.0f / (boxMin.z - boxMax.z); // z away from target
    offsetX = -0.5f * (boxMax.x + boxMin.x) * scaleX;
    offsetY = -0.5f * (boxMax.y + boxMin.y) * scaleY;
    offsetZ = -0.5f * (boxMax.z + boxMin.z) * scaleZ;
    

    return glm::mat4
    (   scaleX,     0.0f,       0.0f,       0.0f,
        0.0f,       scaleY,     0.0f,       0.0f,
        0.0f,       0.0f,       scaleZ,     0.0f,
        offsetX,    offsetY,    offsetZ,    1.0f);
}

glm::mat4 GetCropMatrix(BoundingBox<glm::vec3> splitAABB_LightSpace)
{
    // Light space is built as the "view space" for light
    // so positive Z faces away from the target.
    glm::vec3 boxMin = splitAABB_LightSpace.Min();
    glm::vec3 boxMax = splitAABB_LightSpace.Max();


    float scaleX, scaleY, scaleZ;
    float offsetX, offsetY, offsetZ;
    scaleX = 2.0f / (boxMax.x - boxMin.x);
    scaleY = 2.0f / (boxMax.y - boxMin.y);
    scaleZ = 2.0f / (boxMin.z - boxMax.z); // z away from target
    offsetX = -0.5f * (boxMax.x + boxMin.x) * scaleX;
    offsetY = -0.5f * (boxMax.y + boxMin.y) * scaleY;
    offsetZ = -0.5f * (boxMax.z + boxMin.z) * scaleZ;


    return glm::mat4
    (scaleX, 0.0f, 0.0f, 0.0f,
        0.0f, scaleY, 0.0f, 0.0f,
        0.0f, 0.0f, scaleZ, 0.0f,
        offsetX, offsetY, offsetZ, 1.0f);
}

// ------------------------------------------------------------------------------------------------------------------------------------------



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

void LoadScene_PSSM(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<WiresShader>>* shadersCollection)
{

    int numSplits = 4;

    float cAspect = 1.8f;
    float cFoV = glm::radians(45.0f);
    float cNear = 2.3f;
    float cFar = 18.6f;
    glm::vec3 cPos = glm::vec3(4.8f, 2.4f, 0.5f);
    glm::vec3 cDir = glm::normalize(glm::vec3(1, 0, 0));
    glm::vec3 cAxis = glm::normalize(glm::cross(glm::vec3(0, 0, 1), cDir));
    glm::vec3 cUp = glm::cross(cDir, cAxis);
    glm::mat4 cProj = glm::perspective(cFoV, cAspect, cNear, cFar);
    glm::mat4 cView = glm::lookAt(cPos, cPos + cDir, cUp);

    glm::vec3 lPos = glm::vec3(18, 18, 7);
    glm::vec3 lDir = glm::normalize(glm::vec3(0.1, -1, -0.35f));
    glm::vec3 lAxis = glm::normalize(glm::cross(glm::vec3(0, 0, 1), lDir));
    glm::vec3 lUp = glm::cross(lDir, lAxis);
    glm::mat4 lView = glm::lookAt(lPos, lPos + lDir, lUp);


    BoundingBox<glm::vec3> ndcCubeBbox = BoundingBox(std::vector<glm::vec3>
    {
            glm::vec3( 1,  1,  1),
            glm::vec3( 1,  1, -1),
            glm::vec3( 1, -1,  1),
            glm::vec3( 1, -1, -1),
            glm::vec3(-1,  1,  1),
            glm::vec3(-1,  1, -1),
            glm::vec3(-1, -1,  1),
            glm::vec3(-1, -1, -1),
    });

    Wire wireSplit0 = Wire(ndcCubeBbox.GetLines(), WireNature::LINES);
    Wire wireSplit1 = Wire(ndcCubeBbox.GetLines(), WireNature::LINES);
    Wire wireSplit2 = Wire(ndcCubeBbox.GetLines(), WireNature::LINES);
    Wire wireSplit3 = Wire(ndcCubeBbox.GetLines(), WireNature::LINES);


    BoundingBox<glm::vec3> bbSplit0 = GetSplitAABB_LightSpace(0, numSplits, 0.8f, cNear, cFar, cFoV, cAspect, cView, lView/*, &ptsSplit0*/);
    BoundingBox<glm::vec3> bbSplit1 = GetSplitAABB_LightSpace(1, numSplits, 0.8f, cNear, cFar, cFoV, cAspect, cView, lView/*, &ptsSplit1*/);
    BoundingBox<glm::vec3> bbSplit2 = GetSplitAABB_LightSpace(2, numSplits, 0.8f, cNear, cFar, cFoV, cAspect, cView, lView/*, &ptsSplit2*/);
    BoundingBox<glm::vec3> bbSplit3 = GetSplitAABB_LightSpace(3, numSplits, 0.8f, cNear, cFar, cFoV, cAspect, cView, lView/*, &ptsSplit3*/);

    // LIGHT
    // -----
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(
        lPos, 0.0f, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
        Mesh::Sphere(1.0f, 32), 
        nullptr,
        nullptr
        ));

   
    // SPLITS
    // ------
    WiresRenderer wrSplit0 = WiresRenderer(wireSplit0, shadersCollection->at("LINES").get());
    wrSplit0.SetTransformation(glm::inverse(GetCropMatrix(bbSplit0) * lView));
    wrSplit0.SetColor(glm::vec4(1, 0.8, 0, 1));
    WiresRenderer wrSplit1 = WiresRenderer(wireSplit1, shadersCollection->at("LINES").get());
    wrSplit1.SetTransformation(glm::inverse(GetCropMatrix(bbSplit1) * lView));
    wrSplit1.SetColor(glm::vec4(1, 0.6, 0, 1));
    WiresRenderer wrSplit2 = WiresRenderer(wireSplit2, shadersCollection->at("LINES").get());
    wrSplit2.SetTransformation(glm::inverse(GetCropMatrix(bbSplit2) * lView));
    wrSplit2.SetColor(glm::vec4(1, 0.4, 0, 1));
    WiresRenderer wrSplit3 = WiresRenderer(wireSplit3, shadersCollection->at("LINES").get());
    wrSplit3.SetTransformation(glm::inverse(GetCropMatrix(bbSplit3) * lView));
    wrSplit3.SetColor(glm::vec4(1, 0.2, 0, 1));

    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(wrSplit0)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(wrSplit1)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(wrSplit2)));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(wrSplit3)));

    // View FRUSTUM
    // ------------
    Wire cWire = Wire(GetFrustumEdges(cView, cProj), WireNature::LINES);
    WiresRenderer cWr = WiresRenderer(cWire, shadersCollection->at("LINES").get());
    cWr.SetColor(glm::vec4(0, 1, 0, 1));
    sceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(cWr)));
}


void LoadScene_PCSStest(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
    Mesh planeMesh = Mesh::Box(8, 8, 0.5);

    FileReader reader = FileReader("../../Assets/Models/Teapot.obj");
    reader.Load();
    Mesh teapotMesh = reader.Meshes()[0];

    MeshRenderer plane = MeshRenderer(planeMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get());
    plane.SetMaterial(std::make_shared<Material>( Material{ glm::vec4(1.0, 1.0, 1.0 ,1.0), 0.8, 0.0 } ));
    plane.Renderer::SetTransformation(glm::vec3(-4, -4, -0.25), 0, glm::vec3(1.0, 0, 0), glm::vec3(1, 1, 1));
    
    
    MeshRenderer
        teapot1 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot2 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot3 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot4 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get()),
        teapot5 = MeshRenderer(teapotMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get());

    teapot1.Renderer::Transform(glm::vec3(-1, 1, 2), 1.5, glm::vec3(1, -0.5, 0.6), glm::vec3(1, 1, 1));
    teapot2.Renderer::Transform(glm::vec3(0, 0.6, 4), 0.5, glm::vec3(0, -0.5, 0.6), glm::vec3(1, 1, 1));
    teapot3.Renderer::Transform(glm::vec3(0, -1.5, 1.6), 3.2, glm::vec3(1, -0, 0.6), glm::vec3(1, 1, 1));
    teapot4.Renderer::Transform(glm::vec3(-0.5, -0.6, 3.3), 1.4, glm::vec3(1, -0.5, 1.6), glm::vec3(1, 1, 1));
    teapot5.Renderer::Transform(glm::vec3(1.6, 0.8, 1.5), 0.6, glm::vec3(1,1.0,2.0), glm::vec3(1, 1, 1));

    teapot1.SetMaterial(std::make_shared<Material>( Material{ glm::vec4(1.0, 0.0, 0.0 ,1.0), 0.9, 0.0 } ));
    teapot2.SetMaterial(std::make_shared<Material>( Material{ glm::vec4(0.9, 0.5, 0.0 ,1.0), 0.1, 0.0 } ));
    teapot3.SetMaterial(std::make_shared<Material>( Material{ glm::vec4(0.8, 0.3, 0.5 ,1.0), 0.5, 0.0 } ));
    teapot4.SetMaterial(std::make_shared<Material>( Material{ glm::vec4(0.89, 0.92, 0.84 ,1.0), 0.5, 1.0 } ));
    teapot5.SetMaterial(std::make_shared<Material>( Material{ glm::vec4(0.4, 0.4, 0.9 ,1.0), 0.6, 0.0 } ));


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

void LoadSceneFromPath(const char* path, SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection, std::shared_ptr<Material> material)
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

    
    auto matBody = std::make_shared <Material>(Material{ glm::vec4(1.0),0.9, 0.0, "TechnoDemon_Body"});
    auto matArm  = std::make_shared <Material>(Material{ glm::vec4(1.0),0.2, 1.0, "TechnoDemon_Arm" });
  
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
    auto mat_0 = std::make_shared <Material>(Material{ glm::vec4(1.0),1.0, 0.0, "UtilityKnife" });
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
    auto mat_0 = std::make_shared <Material>(Material{ glm::vec4(1.0), 1.0, 0.0, "RadialEngine" });
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

void LoadScene_Spaceship(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{

    LoadSceneFromPath("../../Assets/Models/Spaceship.fbx", sceneMeshCollection, shadersCollection);

    // ___ Transformation ___ //
    // ---------------------- //
     glm::mat4 tr = glm::mat4(1.0);
     tr = glm::translate(tr, glm::vec3(0, 0,3.0f));
     tr = glm::scale(tr, glm::vec3(0.01));
     tr = glm::rotate(tr, glm::pi<float>() / 2.0f, glm::vec3(1.0, 0.0, 0.0));
     
     for (int i = 0; i < sceneMeshCollection.size(); i++)
         sceneMeshCollection.at(i).get()->Transform(tr);

    sceneMeshCollection.UpdateBBox();
    // ---------------------- //


    auto mat = std::make_shared <Material>(Material{ glm::vec4(1.0), 0.9f, 0.0, "Spaceship" });
   
    for (int i = 0; i < sceneMeshCollection.size(); i++)
        sceneMeshCollection.at(i)->SetMaterial(mat);
    

    // ----------------- //
    LoadPlane(sceneMeshCollection, shadersCollection, 15.0);
}

void SetupScene(
    SceneMeshCollection& sceneMeshCollection, 
    std::map<std::string, std::shared_ptr<MeshShader>> *meshShadersCollection,
    std::map<std::string, std::shared_ptr<WiresShader>>* wiresShadersCollection
)
{
    //LoadPlane(sceneMeshCollection, meshShadersCollection, 15.0, -0.0f);


    //LoadScene_PSSM(sceneMeshCollection, wiresShadersCollection);
    //LoadScene_CubicBezier(sceneMeshCollection, wiresShadersCollection);
    //LoadScene_Hilbert(sceneMeshCollection, wiresShadersCollection);
    //LoadScene_PoissonDistribution(sceneMeshCollection, wiresShadersCollection);
    //LoadScene_PbrTestSpheres(sceneMeshCollection, meshShadersCollection);
    //LoadScene_PbrTestTeapots(sceneMeshCollection, meshShadersCollection);
    //LoadScene_PbrTestKnobs(sceneMeshCollection, meshShadersCollection);
    //LoadSceneFromPath("../../Assets/Models/Teapot.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::ShinyRed);
	//LoadScene_NormalMapping(sceneMeshCollection, meshShadersCollection);
    //LoadScene_TechnoDemon(sceneMeshCollection, meshShadersCollection);
    //LoadScene_RadialEngine(sceneMeshCollection, meshShadersCollection);
    //LoadScene_UtilityKnife(sceneMeshCollection, meshShadersCollection);
    //LoadSceneFromPath("../../Assets/Models/aoTest.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
    //LoadSceneFromPath("../../Assets/Models/RadialEngine.fbx", sceneMeshCollection, meshShadersCollection);
    //LoadSceneFromPath("../../Assets/Models/House.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::MatteGray);
    //LoadSceneFromPath("../../Assets/Models/TestPCSS.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::ShinyRed);
     //LoadSceneFromPath("../../Assets/Models/Dragon.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
     //LoadSceneFromPath("../../Assets/Models/Sponza.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
    //LoadSceneFromPath("../../Assets/Models/Knob.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::PureWhite);
    //LoadSceneFromPath("../../Assets/Models/trees.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::MatteGray);
    //LoadSceneFromPath("../../Assets/Models/OldBridge.obj", sceneMeshCollection, meshShadersCollection, MaterialsCollection::MatteGray);
    //LoadSceneFromPath("./Assets/Models/Engine.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::PlasticGreen);
    //LoadScene_ALotOfMonkeys(sceneMeshCollection, meshShadersCollection);
    //LoadScene_Primitives(sceneMeshCollection, meshShadersCollection);
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
    LoadScene_Spaceship(sceneMeshCollection, meshShadersCollection);
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
#ifdef GFX_SHADER_DEBUG_VIZ
            "DEFS_DEBUG_VIZ",
#endif
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
#ifdef GFX_SHADER_DEBUG_VIZ
            "DEFS_DEBUG_VIZ",
#endif
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

int GetUboStorageSize(UBOBinding ubo)
{
    switch (ubo)
    {
    case PerFrameData:
        return sizeof(dataPerFrame);
        break;
    case ViewData:
        return sizeof(viewData);
        break;
    case PerObjectData:
        return sizeof(dataPerObject);
        break;
    case Shadows:
        return 0;
        break;
    case AmbientOcclusion:
        return 0;
        break;
    default:
        throw "Ayo...";
        break;
    }
}
void InitUBOs(std::vector<UniformBufferObject>& uboCollection)
{
    for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
    {
        UBOBinding bindingPoint = static_cast<UBOBinding>(i);
        uboCollection.at(i).SetBindingPoint(bindingPoint);
        uboCollection.at(i).SetData<char>(GetUboStorageSize(bindingPoint), NULL);
    }
}
void FreeUBOs(std::vector<UniformBufferObject>& uboCollection)
{
    uboCollection.clear();
}

void UpdateObjectData(UniformBufferObject& ubo, const glm::mat4 objectTransformation)
{
    ubo.SetSubData<float>(0, 16, glm::value_ptr(objectTransformation));
}
void UpdateObjectData(UniformBufferObject& ubo, const glm::mat4 objectTransformation, const glm::vec4 color)
{
    ubo.SetSubData<float>(0, 16, glm::value_ptr(objectTransformation));
    ubo.SetSubData<float>(32, 4, glm::value_ptr(color));
}

void UpdateViewData(UniformBufferObject& ubo,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    float cameraNear, float cameraFar)
{
    viewData data = viewData
    {
        viewMatrix,
        projectionMatrix,
        cameraNear,
        cameraFar
    };

    ubo.SetSubData(0, 1, &data);
}
void UpdateViewData(UniformBufferObject& ubo, const SceneParams& sceneParams)
{
    UpdateViewData(ubo,
        sceneParams.viewMatrix, sceneParams.projectionMatrix,
        sceneParams.cameraNear, sceneParams.cameraFar);
}


void UpdatePerFrameDataUBO(UniformBufferObject& ubo, const SceneParams& sceneParams)
{
   
    dirLightData dlData = dirLightData
    {
        glm::normalize(glm::vec4(sceneParams.sceneLights.Directional.Direction, 0.0)),
        sceneParams.sceneLights.Directional.Diffuse,
        sceneParams.sceneLights.Directional.Softness,
        sceneParams.sceneLights.Directional.Bias,
        (float)sceneParams.sceneLights.Directional.doSplits,
        0,
        sceneParams.sceneLights.Directional.splits
    };
    pointLightData plData0 = pointLightData
    {
        sceneParams.sceneLights.Points[0].Color,
        glm::vec4(sceneParams.sceneLights.Points[0].Position, 0.0),
        sceneParams.sceneLights.Points[0].Radius,
        sceneParams.sceneLights.Points[0].Size,
        1.0f / glm::pow(sceneParams.sceneLights.Points[0].Radius, 2.0f),
        sceneParams.sceneLights.Points[0].Bias
    };
    pointLightData plData1 = pointLightData
    {
        sceneParams.sceneLights.Points[1].Color,
        glm::vec4(sceneParams.sceneLights.Points[1].Position, 0.0),
        sceneParams.sceneLights.Points[1].Radius,
        sceneParams.sceneLights.Points[1].Size,
        1.0f / glm::sqrt(sceneParams.sceneLights.Points[1].Radius),
        sceneParams.sceneLights.Points[1].Bias
    };
    pointLightData plData2 = pointLightData
    {
        sceneParams.sceneLights.Points[2].Color,
        glm::vec4(sceneParams.sceneLights.Points[2].Position, 0.0),
        sceneParams.sceneLights.Points[2].Radius,
        sceneParams.sceneLights.Points[2].Size,
        1.0f / glm::sqrt(sceneParams.sceneLights.Points[2].Radius),
        sceneParams.sceneLights.Points[2].Bias
    };

    
    dataPerFrame data = dataPerFrame
    {
        sceneParams.sceneLights.Directional.LightSpaceMatrix[0],
        sceneParams.sceneLights.Directional.LightSpaceMatrix[1],
        sceneParams.sceneLights.Directional.LightSpaceMatrix[2],
        sceneParams.sceneLights.Directional.LightSpaceMatrix[3],

        glm::inverse(sceneParams.sceneLights.Directional.LightSpaceMatrix[0]),
        glm::inverse(sceneParams.sceneLights.Directional.LightSpaceMatrix[1]),
        glm::inverse(sceneParams.sceneLights.Directional.LightSpaceMatrix[2]),
        glm::inverse(sceneParams.sceneLights.Directional.LightSpaceMatrix[3]),

        dlData,
        plData0,plData1,plData2,

        glm::vec4(sceneParams.cameraPos, 0.0),
        sceneParams.sceneLights.Directional.ShadowBoxSize,
        sceneParams.postProcessing.doGammaCorrection,
        sceneParams.postProcessing.Gamma,
        sceneParams.environment.IrradianceMap.has_value(),
        sceneParams.environment.intensity,
        sceneParams.environment.RadianceMap.has_value(),
        sceneParams.environment.LUT.has_value(),
        sceneParams.environment.RadianceMap.has_value()
            ? sceneParams.environment.RadianceMap.value().MinLevel()
            : 0,
        sceneParams.environment.RadianceMap.has_value()
            ? sceneParams.environment.RadianceMap.value().MaxLevel()
            : 0,

        sceneParams.postProcessing.jitter,
        glm::vec2(sceneParams.viewportWidth, sceneParams.viewportHeight),
        sceneParams.postProcessing.doTaa
    };

    ubo.SetSubData(0, 1, &data);

}



void UpdateAoUBO(UniformBufferObject& ubo, SceneLights& lights)
{
    ubo.SetData<float>(1, NULL);
    ubo.SetSubData(0, 1, &lights.Ambient.aoStrength);
}


void DrawEnvironment(
    const Camera& camera, SceneParams& sceneParams, 
    const VertexAttribArray& unitCubeVAO,  
    std::vector<UniformBufferObject>& sceneUBOs, ForwardSkyboxShader& environmentShader)
{
   
#if GFX_STOPWATCH
    Profiler::Instance().StartNamedStopWatch("Environment");
#endif


    glm::mat4 prevView = sceneParams.viewMatrix;
    glm::mat4 prevProj = sceneParams.projectionMatrix;
    float prevNear = sceneParams.cameraNear;
    float prevFar = sceneParams.cameraFar;

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

    //TODO: Not here...
    environmentShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
    UpdateViewData(sceneUBOs.at(UBOBinding::ViewData), sceneParams.viewMatrix, proj, sceneParams.cameraNear, sceneParams.cameraFar);
    UpdateObjectData(sceneUBOs.at(UBOBinding::PerObjectData), transf);

    if (sceneParams.environment.useSkyboxTexture && sceneParams.environment.RadianceMap.has_value())
    {
        glActiveTexture(GL_TEXTURE0);
        sceneParams.environment.Skybox.value().Bind();
        environmentShader.SetEnvironmentMap(0);
    }
    environmentShader.SetDrawWithTexture(sceneParams.environment.useSkyboxTexture);

    OGLUtils::CheckOGLErrors();

    environmentShader.SetColors(
        sceneParams.environment.NorthColor,
        sceneParams.environment.EquatorColor,
        sceneParams.environment.SouthColor);

    OGLUtils::CheckOGLErrors();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);

    unitCubeVAO.Bind();
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    unitCubeVAO.UnBind();

    if (sceneParams.environment.useSkyboxTexture && sceneParams.environment.RadianceMap.has_value())
        sceneParams.environment.Skybox.value().UnBind();

    glDepthFunc(GL_LESS);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    // Reset view data
    sceneParams.viewMatrix = prevView;
    sceneParams.projectionMatrix = prevProj;
    sceneParams.cameraNear = prevNear;
    sceneParams.cameraFar = prevFar;

    UpdateViewData(sceneUBOs.at(UBOBinding::ViewData), sceneParams);


#if GFX_STOPWATCH
    Profiler::Instance().StopNamedStopWatch("Environment");
#endif
}

void ComputeDirectionalShadowMap(SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection,
    std::vector<UniformBufferObject>& sceneUniformBuffers, const ShaderBase& shaderForShadows, const FrameBuffer<OGLTexture2D>& shadowFBO)
{
    glm::mat4
        viewShadow, projShadow;

    float lightNear, lightFar;

    shadowFBO.Bind(true, true);

    glViewport(0, 0, directionalShadowMapResolution, directionalShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    OGLUtils::CheckOGLErrors();

    shaderForShadows.SetCurrent();


    Utils::GetDirectionalShadowMatrices(
        sceneParams.sceneLights.Directional.Position, sceneParams.sceneLights.Directional.Direction,
        sceneMeshCollection.GetSceneBBPoints(), viewShadow, projShadow, &lightNear, &lightFar);

    glm::mat4 prevViewMat = sceneParams.viewMatrix;
    glm::mat4 prevProjMat = sceneParams.projectionMatrix;
    float prevCameraNear    = sceneParams.cameraNear;
    float prevCameraFar     = sceneParams.cameraFar;

    if (!sceneParams.sceneLights.Directional.doSplits)
    {
        // Update view and proj matrices for the shadow map
        sceneParams.viewMatrix = viewShadow;
        sceneParams.projectionMatrix = projShadow;
        sceneParams.sceneLights.Directional.LightSpaceMatrix[0] = projShadow * viewShadow;
        UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams);

        for (int i = 0; i < sceneMeshCollection.size(); i++)
        {
            if (!sceneMeshCollection.at(i).get()->ShouldDrawForShadows())
                continue;
            UpdateObjectData(sceneUniformBuffers.at(UBOBinding::PerObjectData), sceneMeshCollection.at(i).get()->GetTransformation());
            sceneMeshCollection.at(i).get()->DrawCustom();
        }
    }
    // Use PSSM (4 splits on a single texture)
    else 
    {
       
        auto cameraZMinMax = BoundingBox<glm::vec3>::GetMinMax(sceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix);
       
        float sceneCameraZExt = cameraZMinMax.second.z - cameraZMinMax.first.z;

        sceneParams.cameraFar = glm::max(sceneParams.cameraFar, sceneParams.cameraNear + sceneCameraZExt);

        int splitResolution = directionalShadowMapResolution / 2;
        for (int split = 0; split < DirectionalLight::SPLITS; split++)
        {
            // This works only for the current hardcoded configuration
            // -> 4 splits in a single texture.
            glViewport((split % 2) * splitResolution, (split / 2) * splitResolution, splitResolution, splitResolution);

            // Draw each split with the corresponding crop matrix (projection)
            glm::mat4 splitProjMat = GetCropMatrix(
                GetSplitAABB_LightSpace(split, DirectionalLight::SPLITS, sceneParams.sceneLights.Directional.lambda,
                    sceneParams.cameraNear, sceneParams.cameraFar,
                    sceneParams.cameraFovY,
                    sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
                    prevViewMat, viewShadow),
                lightNear, lightFar
            );

            sceneParams.sceneLights.Directional.LightSpaceMatrix[split] = splitProjMat * viewShadow;

            // Update view and proj matrices for the shadow map
            sceneParams.viewMatrix = viewShadow;
            sceneParams.projectionMatrix = splitProjMat;
            UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams);


            for (int i = 0; i < sceneMeshCollection.size(); i++)
            {
                if (!sceneMeshCollection.at(i).get()->ShouldDrawForShadows())
                    continue;
                UpdateObjectData(sceneUniformBuffers.at(UBOBinding::PerObjectData), sceneMeshCollection.at(i).get()->GetTransformation());
                sceneMeshCollection.at(i).get()->DrawCustom();
            }

            // Store split z ranges
            GetSplitAtIndex(split+1, DirectionalLight::SPLITS, 
                sceneParams.sceneLights.Directional.lambda, sceneParams.cameraNear, sceneParams.cameraFar,
                &sceneParams.sceneLights.Directional.splits[split]);

        }
    }
    sceneParams.sceneLights.Directional.ShadowMapId = shadowFBO.DepthTextureId();
    shadowFBO.UnBind();

    sceneParams.sceneLights.Directional.ShadowBoxSize = glm::vec4(

        2.0f / projShadow[0][0], // r - l
        2.0f / projShadow[1][1], // t - b
        lightNear,
        lightFar
    );

    // Reset....wow
    sceneParams.viewMatrix = prevViewMat;
    sceneParams.projectionMatrix = prevProjMat;
    sceneParams.cameraNear = prevCameraNear;
    sceneParams.cameraFar = prevCameraFar;
    UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams);

    // Update data for the Light Pass
    UpdatePerFrameDataUBO(sceneUniformBuffers.at(UBOBinding::PerFrameData), sceneParams);

}

void ComputePointShadowMap(int pointLightIndex, const SceneMeshCollection& sceneMeshCollection,
    std::vector<UniformBufferObject>& sceneUniformBuffers, ForwardOmniDirShadowShader& shaderForShadows, const FrameBuffer<OGLTextureCubemap>& shadowFBO)
{
    PointLight& light = sceneParams.sceneLights.Points[pointLightIndex];

    if (light.Color.w == 0.0f)
        return;

    std::vector<glm::mat4> shadowTransforms;
    
    float near;
    float far;

    Utils::GetPointShadowMatrices(light.Position, light.Radius, near, far, light.LightSpaceMatrix);

    shaderForShadows.SetCurrent();

    shaderForShadows.SetShadowMatrices(light.LightSpaceMatrix);
    shaderForShadows.SetLightIndex(pointLightIndex);

    shadowFBO.Bind(true, true);

    glViewport(0, 0, pointShadowMapResolution, pointShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    OGLUtils::CheckOGLErrors();

    shaderForShadows.SetCurrent();

    for (int i = 0; i < sceneMeshCollection.size(); i++)
    {
        if (!sceneMeshCollection.at(i).get()->ShouldDrawForShadows())
            continue;
        UpdateObjectData(sceneUniformBuffers.at(UBOBinding::PerObjectData), sceneMeshCollection.at(i).get()->GetTransformation());
        sceneMeshCollection.at(i).get()->DrawCustom();
    }

    light.ShadowMapId = shadowFBO.DepthTextureId();
    shadowFBO.UnBind();
}

void ShadowPass(
    SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection, std::vector<UniformBufferObject>& sceneUniformBuffers,
    ForwardUnlitShader& shadowShaderDirectional, ForwardOmniDirShadowShader& shadowShaderPoint , const FrameBuffer<OGLTexture2D>& directionalShadowFBO, const std::vector<FrameBuffer<OGLTextureCubemap>>& pointShadowFBOs)
{
#if GFX_STOPWATCH
    Profiler::Instance().StartNamedStopWatch("Shadows");
#endif

    shadowShaderDirectional.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
    shadowShaderPoint.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);

    ComputeDirectionalShadowMap(sceneParams, sceneMeshCollection, sceneUniformBuffers, shadowShaderDirectional, directionalShadowFBO);
    
    for(int i =0; i<PointLight::MAX_POINT_LIGHTS; i++)
        ComputePointShadowMap(i, sceneMeshCollection, sceneUniformBuffers, shadowShaderPoint, pointShadowFBOs[i]);

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

    UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams);
    
    OGLUtils::CheckOGLErrors();

    for (int i = 0 ; i < sceneMeshCollection.size() ; i++)
    {
        sceneMeshCollection.at(i).get()->DrawCustom();
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

#ifdef GFX_SHADER_DEBUG_VIZ

glm::mat4 GetTransformation(glm::uvec4 uData, glm::vec4 f4Data0, glm::vec4 f4Data1, glm::vec4 f4Data2)
{
    
    glm::mat4 tr = glm::mat4(1.0f);

    switch (uData.x)
    {
    case(0): // Cone
    {
        glm::vec3 start = glm::vec3(f4Data0.x, f4Data0.y, f4Data0.z);
        glm::vec3 direction = glm::vec3(f4Data1.x, f4Data1.y, f4Data1.z);
        float radius = f4Data1.w;

        glm::quat quat = glm::quat(glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(direction));
        tr = glm::translate(tr, start);
        tr = glm::rotate(tr, glm::angle(quat), glm::axis(quat));
        tr = glm::scale(tr, glm::vec3(radius, radius, glm::length(direction)));
        break;
    }
    case(1): // Sphere

        break;

    case(2): // Point

        break;

    case(3): // Line
    {
        glm::vec3 start = glm::vec3(f4Data0.x, f4Data0.y, f4Data0.z);
        glm::vec3 end = glm::vec3(f4Data1.x, f4Data1.y, f4Data1.z);
        
        glm::quat quat = glm::quat(glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(end - start));
        tr = glm::translate(tr, start);
        tr = glm::rotate(tr, glm::angle(quat), glm::axis(quat));
        tr = glm::scale(tr, glm::vec3(glm::length(end - start)));
    }
        break;

    default:
        throw "SHADER_DEBUG_VIZ: invalid enum.";
        return glm::mat4(1.0f);
        break;
    }

    return tr;
}

void DrawShaderDebugViz(
    OGLResources::ShaderStorageBufferObject& debugSsbo, 
    std::vector<UniformBufferObject>& sceneUBOs,
    MeshRenderer& debugConeRenderer,
    MeshRenderer& debugSphereRenderer,
    WiresRenderer& debugPointRenderer,
    WiresRenderer& debugLineRenderer,
    ForwardUnlitShader& meshShader,
    ForwardThickPointsShader& pointShader,
    ForwardThickLinesShader& lineshader)
{
    unsigned int viewport[4];
    unsigned int mouse[4];
    unsigned int counter[4];

    debugSsbo.ReadData(0, 4, &viewport[0]);
    debugSsbo.ReadData(4, 4, &mouse[0]);
    debugSsbo.ReadData(8, 4, &counter[0]);

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (counter[0] > 0)
    {
        // TODO: not here...
        meshShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
        lineshader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
        pointShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
        
        for (int i = 0; i < glm::min(SSBO_CAPACITY, static_cast<int>(counter[0])); i++)
        {
            glm::uvec4 uData;
            glm::vec4 
                f4Data0, 
                f4Data1, 
                f4Data2;

            debugSsbo.ReadData(12 + (i * 16)            , 4, glm::value_ptr(uData));
            debugSsbo.ReadData(12 + (i * 16) + 4        , 4, glm::value_ptr(f4Data0));
            debugSsbo.ReadData(12 + (i * 16) + 4 + 4    , 4, glm::value_ptr(f4Data1));
            debugSsbo.ReadData(12 + (i * 16) + 4 + 4 + 4, 4, glm::value_ptr(f4Data2));

            glm::mat4 transformation = GetTransformation(uData, f4Data0, f4Data1, f4Data2);

            switch (uData.x)
            {
            case(0): // Cone
                meshShader.SetCurrent();
                UpdateObjectData(sceneUBOs.at(UBOBinding::PerObjectData), transformation, f4Data2);
                debugConeRenderer.DrawCustom();
                break;

            case(1): // Sphere

                break;

            case(2): // Point

                break;

            case(3): // Line
                lineshader.SetCurrent();
                lineshader.SetScreenData(viewport[0], viewport[1]);
                lineshader.SetThickness(2.0f);
                UpdateObjectData(sceneUBOs.at(UBOBinding::PerObjectData), transformation, f4Data2);
                debugLineRenderer.DrawCustom();
                break;
            }

        
        }

    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

#endif

void DrawExtraSceneEntities(
    std::vector<OGLResources::UniformBufferObject>& sceneUniformBuffers,
    SceneMeshCollection& extraSceneMeshCollection,
    ForwardUnlitShader& meshShader,
    ForwardThickPointsShader& pointShader,
    ForwardThickLinesShader& lineShader,
    ForwardThickLineStripShader& lineStripShader)
{
    for (int i = 0; i < extraSceneMeshCollection.size(); i++)
    {
        Renderer* r = extraSceneMeshCollection.at(i).get();

        // Set transformation and color
        UpdateObjectData(
            sceneUniformBuffers.at(UBOBinding::PerObjectData),
            r->GetTransformation(), r->GetMaterial().Albedo);

        // It's a Mesh
        if (MeshRenderer* mr = dynamic_cast<MeshRenderer*>(r))
            meshShader.SetCurrent();

        // It's a wire (lines, points, linestrip)
        else if (WiresRenderer* wr = dynamic_cast<WiresRenderer*>(r))
        {
            switch (wr->GetNature())
            {
            case(WireNature::POINTS):       
                pointShader.SetCurrent();       
                pointShader.SetScreenData(sceneParams.viewportWidth, sceneParams.viewportHeight);   
                pointShader.SetThickness(sceneParams.pointWidth);       
                break;

            case(WireNature::LINES):        
                lineShader.SetCurrent();        
                lineShader.SetScreenData(sceneParams.viewportWidth, sceneParams.viewportHeight);
                lineShader.SetThickness(sceneParams.lineWidth);         
                break;

            case(WireNature::LINE_STRIP):   
                lineStripShader.SetCurrent();   
                lineStripShader.SetScreenData(sceneParams.viewportWidth, sceneParams.viewportHeight);
                lineStripShader.SetThickness(sceneParams.lineWidth);    
                break;

            default:
                throw"LOL...\n";
            }
        }

        OGLUtils::CheckOGLErrors();
        r->DrawCustom();
    }
}

void DrawExtraScene(
    std::vector<OGLResources::UniformBufferObject>& sceneUniformBuffers, 
    SceneMeshCollection& extraSceneMeshCollection,
    bool before,
    ForwardUnlitShader& meshShader,
    ForwardThickPointsShader& pointShader, 
    ForwardThickLinesShader& lineShader, 
    ForwardThickLineStripShader& lineStripShader)
{
    
    float extraN;
    float extraF;
    
    float newNear;
    float newFar;

    glm::mat4 prevView = sceneParams.viewMatrix;
    glm::mat4 prevProj = sceneParams.projectionMatrix;
    float prevNear = sceneParams.cameraNear;
    float prevFar = sceneParams.cameraFar;

    Utils::GetTightNearFar(
        extraSceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix, 0.1f, extraN, extraF
    );

    glm::mat4 gridProjAfterScene;

    // TODO: do at initializaion....they won't change
    meshShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
    pointShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
    lineShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
    lineStripShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);

    if (before)
    {
        newNear = sceneParams.cameraFar;
        newFar = glm::max(sceneParams.cameraFar, extraF);
        gridProjAfterScene =
            glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
                newNear, newFar);

        UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams.viewMatrix, gridProjAfterScene, newNear, newFar);


        glDepthMask(GL_FALSE);

        DrawExtraSceneEntities(sceneUniformBuffers, extraSceneMeshCollection, meshShader, pointShader, lineShader, lineStripShader);

        glDepthMask(GL_TRUE);
    }
    else
    {
        
        UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams.viewMatrix, sceneParams.projectionMatrix, sceneParams.cameraNear, sceneParams.cameraFar);

        glDepthMask(GL_FALSE);

        DrawExtraSceneEntities(sceneUniformBuffers, extraSceneMeshCollection, meshShader, pointShader, lineShader, lineStripShader);

        newNear = glm::min(sceneParams.cameraNear, glm::max(0.0f, extraN));
        newFar = sceneParams.cameraNear;

        gridProjAfterScene =
            glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
                newNear, newFar);

        UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams.viewMatrix, gridProjAfterScene, newNear, newFar);

        glDisable(GL_DEPTH_TEST);

        DrawExtraSceneEntities(sceneUniformBuffers, extraSceneMeshCollection, meshShader, pointShader, lineShader, lineStripShader);

        glEnable(GL_DEPTH_TEST);


        // Draw with depth test GREATER and blending
        glDepthFunc(GL_GEQUAL);
        glDepthRange(1.0f, 1.0f);
        glEnable(GL_BLEND);
        glBlendColor(1.0f, 1.0f, 1.0f, 0.1f);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);

        newNear = glm::min(sceneParams.cameraNear, glm::max(0.0f, extraN));
        newFar = glm::max(sceneParams.cameraFar, extraF);
        gridProjAfterScene =
            glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight,
                newNear, newFar);
        UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams.viewMatrix, gridProjAfterScene, newNear, newFar);

        DrawExtraSceneEntities(sceneUniformBuffers, extraSceneMeshCollection, meshShader, pointShader, lineShader, lineStripShader);

        glDisable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glDepthRange(0.0f, 1.0f);
        glDepthMask(GL_TRUE);
    }

    // Reset view data
    UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), prevView, prevProj, prevNear, prevFar);

    
}


void SetRenderTarget(std::stack<FrameBuffer<OGLTexture2D>*>& stack, FrameBuffer<OGLTexture2D>* fbo, bool read, bool write)
{
    stack.push(fbo);
    
    if(fbo)
        fbo->Bind(read, write);

    else // Set default framebuffer
    {
        int mask = (read ? GL_READ_FRAMEBUFFER : 0) | (write ? GL_DRAW_FRAMEBUFFER : 0);
        glBindFramebuffer(mask, 0);
    }
}

void SetRenderTarget(std::stack<FrameBuffer<OGLTexture2D>*>& stack, FrameBuffer<OGLTexture2D>* fbo)
{
    SetRenderTarget(stack, fbo, true, true);
}

void ResetRenderTarget(std::stack<FrameBuffer<OGLTexture2D>*>& stack)
{
    stack.pop();
    if (!stack.empty())
        stack.top()->Bind();
    else
        FrameBuffer<OGLTexture2D>::UnBind();
}

std::vector<glm::vec2> GetTaaJitterVectors()
{
    auto samples =  GetR2Sequence(TAA_JITTER_SAMPLES);
    std::for_each(samples.begin(), samples.end(), [](glm::vec2 &v) {v -= glm::vec2(0.5); });

	//return samples;

    //return std::vector<glm::vec2>(TAA_JITTER_SAMPLES, glm::vec2(0.0f, 0.0f));
    return std::vector<glm::vec2>
    {
        glm::vec2(-0.25f, -0.25f),
        glm::vec2( 0.25f,  0.25f),
        glm::vec2( 0.25f, -0.25f),
        glm::vec2(-0.25f,  0.25f)
    };
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

    sceneParams.viewportWidth = 800;
    sceneParams.viewportHeight = 800;
    glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);

    // Scene View and Projection matrices
    sceneParams.viewMatrix = glm::mat4(1);
    sceneParams.cameraNear = 0.1f, sceneParams.cameraFar = 50.0f;
    sceneParams.cameraFovY = glm::radians(45.0f);
    sceneParams.projectionMatrix = glm::perspective(sceneParams.cameraFovY, sceneParams.viewportHeight / (float)sceneParams.viewportWidth, sceneParams.cameraNear, sceneParams.cameraFar);

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

    VertexBufferObject envCube_vbo = VertexBufferObject(Usage::StaticDraw);
    IndexBufferObject envCube_ebo = IndexBufferObject(Usage::StaticDraw);
    VertexAttribArray envCube_vao = VertexAttribArray();
    envCube_vbo.SetData(24, flatten3(vertices).data());
    envCube_ebo.SetData(36, indices.data());
    envCube_vao.SetAndEnableAttrib(envCube_vbo.ID(), 0, 3, false, 0, 0);
    envCube_vao.SetIndexBuffer(envCube_ebo.ID());

    ShaderBase environmentShader = ShaderBase(VertexSource_Environment::EXP_VERTEX, FragmentSource_Environment::EXP_FRAGMENT);
    
    // Initial skybox default params
    sceneParams.environment.NorthColor   = glm::vec3(198 / 255.0f, 233 / 255.0f, 238 / 255.0f);
    sceneParams.environment.EquatorColor = glm::vec3(136 / 255.0f, 153 / 255.0f, 186 / 255.0f);
    sceneParams.environment.SouthColor   = glm::vec3( 69 / 255.0f,  91 / 255.0f, 135 / 255.0f);


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

    SceneMeshCollection extraSceneMeshCollection;
    extraSceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(grid)));
    extraSceneMeshCollection.AddToCollection(std::make_shared<WiresRenderer>(std::move(bbRenderer)));


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
    sceneParams.sceneLights.Directional.Direction = glm::vec3(0.5, 0.5, -0.7);
    sceneParams.sceneLights.Directional.Diffuse = glm::vec4(1.0, 1.0, 1.0, 1.0);
    sceneParams.sceneLights.Directional.Specular = glm::vec4(1.0, 1.0, 1.0, 0.75);
    sceneParams.sceneLights.Directional.Bias = 0.002f;
    sceneParams.sceneLights.Directional.SlopeBias = 0.015f;
    sceneParams.sceneLights.Directional.Softness = 0.00f;
    
    //sceneParams.sceneLights.Points[0] = PointLight(glm::vec4(1.0, 1.0, 1.0, 15.0), glm::vec3( 0.5, 0.5, 4.0));
    //sceneParams.sceneLights.Points[1] = PointLight(glm::vec4(1.0, 0.0, 0.0, 12.0), glm::vec3(1.0, 1.0, 3.0));
    //sceneParams.sceneLights.Points[2] = PointLight(glm::vec4(0.0, 0.0, 1.0, 12.0), glm::vec3(-1.0, 1.0, 3.0));
   
    sceneParams.drawParams.doShadows = true;

    // Shadow Maps
    // ----------------------
    FrameBuffer directionalShadowFBO = FrameBuffer(directionalShadowMapResolution, directionalShadowMapResolution, false, 0, true,
        TextureInternalFormat::Depth_Component /* UNUSED */, TextureFiltering::Nearest, TextureFiltering::Nearest);

    std::vector<FrameBuffer<OGLTextureCubemap>> pointShadowFBOs = std::vector<FrameBuffer<OGLTextureCubemap>>{};
    for (int i = 0; i < 16; i++)
    {
        pointShadowFBOs.push_back(FrameBuffer<OGLTextureCubemap>(pointShadowMapResolution, pointShadowMapResolution, false, 0, true,
            TextureInternalFormat::Depth_Component /* UNUSED */, TextureFiltering::Nearest, TextureFiltering::Nearest));
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
    sceneParams.postProcessing.doGammaCorrection = true;
    sceneParams.postProcessing.Gamma = 2.2f;

    // TAA
    // ---
    TaaPassShader taaPassShader = TaaPassShader();
    VelocityPassShader velocityPassShader = VelocityPassShader();

    // Velocity buffer (swap each frame)
    // TODO: depth0 and depth1 are here since I wanted to have a policy for
    // rejecting samples that were occluded in the previous frame
    OGLTexture2D velocityColor  = OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_32f /* overkill ?? */,TextureFiltering::Nearest, TextureFiltering::Nearest);
    OGLTexture2D velocityDepth0 = OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, Depth_Component);
    OGLTexture2D velocityDepth1 = OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, Depth_Component);

    FrameBuffer<OGLTexture2D> velocityFBO[2] =
    {
        FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true,
            TextureInternalFormat::Rgba_32f, TextureFiltering::Nearest, TextureFiltering::Nearest),
        FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true,
            TextureInternalFormat::Rgba_32f, TextureFiltering::Nearest, TextureFiltering::Nearest)
    };

    // "Double-buffered" history (swap each frame)
    FrameBuffer<OGLTexture2D> taaHistoryFBO[2] =
    { 
        FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, false, 
            TextureInternalFormat::Rgba_16f, TextureFiltering::Linear, TextureFiltering::Linear),
        FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, false,
            TextureInternalFormat::Rgba_16f, TextureFiltering::Linear, TextureFiltering::Linear)
    };
    int taaBufferIndex = 0;
    int taaJitterIndex = 0;
    std::vector<glm::vec2> taaJitter = GetTaaJitterVectors();
    glm::mat4 taaPrevVP = glm::mat4(1.0f);
    sceneParams.postProcessing.doTaa = true;
    sceneParams.postProcessing.writeVelocity = true;
    sceneParams.postProcessing.jitter = taaJitter[0];

    // Post processing unit (ao, blur, bloom, ...)
    // -------------------------------------------
    PostProcessingUnit postProcessingUnit(SSAO_BLUR_MAX_RADIUS, SSAO_MAX_SAMPLES);

    // GBuffer for deferred shading
    std::vector<OGLTexture2D> gBufferTextures;
    
    gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
    gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
    gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
    gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
    gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));


    FrameBuffer gBuffer = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight,
        std::move(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Depth_Component)),
        std::move(gBufferTextures)
    );

    GPassShader GPassStandard = GPassShader();
    LightPassShader LightPassStandard = LightPassShader();
    ForwardUnlitShader ForwardUnlit = ForwardUnlitShader();
    ForwardOmniDirShadowShader ForwardPointLight = ForwardOmniDirShadowShader();
    ForwardThickPointsShader ForwardPoints = ForwardThickPointsShader();
    ForwardThickLinesShader ForwardLines = ForwardThickLinesShader();
    ForwardThickLineStripShader ForwardLineStrip = ForwardThickLineStripShader();
    ForwardSkyboxShader ForwardSkybox = ForwardSkyboxShader();

    // FBO stack
    std::stack sceneFBOs = std::stack<FrameBuffer<OGLTexture2D>*>();

    // Uniform buffer objects 
    // ----------------------
    std::vector<UniformBufferObject> sceneUniformBuffers{};
    for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
        sceneUniformBuffers.push_back(std::move(UniformBufferObject(OglBuffer::Usage::StaticDraw)));
    
    InitUBOs(sceneUniformBuffers);

#ifdef GFX_SHADER_DEBUG_VIZ

    ShaderStorageBufferObject debugSsbo = ShaderStorageBufferObject(Usage::DynamicRead);
    debugSsbo.SetData<float>(12 + 16* SSBO_CAPACITY, NULL);
    debugSsbo.SetBindingPoint(SSBOBinding::Debug);

    Mesh debugConeMesh   = Mesh::Cone(1.0f, 1.0f, 16);
    Mesh debugSphereMesh = Mesh::Sphere(1.0f, 16);
    Wire debugPointWire  = Wire(std::vector<glm::vec3>{glm::vec3(0.0f, 0.0f, 0.0f)}, WireNature::POINTS);
    Wire debugLineWire   = Wire(std::vector<glm::vec3>{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)}, WireNature::LINES);

    MeshRenderer debugConeRenderer   = MeshRenderer(debugConeMesh  , MeshShaders.at("UNLIT").get(), MeshShaders.at("UNLIT").get());
    MeshRenderer debugSphereRenderer = MeshRenderer(debugSphereMesh, MeshShaders.at("UNLIT").get(), MeshShaders.at("UNLIT").get());
    WiresRenderer debugPointRenderer = WiresRenderer(debugPointWire, WiresShaders.at("POINTS").get());
    WiresRenderer debugLineRenderer  = WiresRenderer(debugLineWire, WiresShaders.at("LINES").get());
    
#endif


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


#ifdef GFX_SHADER_DEBUG_VIZ
        
        unsigned int w = (unsigned int)sceneParams.viewportWidth;
        unsigned int h = (unsigned int)sceneParams.viewportHeight;
        float clearVal = 0.0f;
        debugSsbo.ClearData(TextureInternalFormat::R_32f, GL_FLOAT, &clearVal);
        debugSsbo.SetSubData(0, 4,  glm::value_ptr(glm::uvec4(w, h, GFX_SHADER_DEBUG_VIZ, 0)));
        debugSsbo.SetSubData(4, 8,  glm::value_ptr(glm::uvec4((unsigned int)lastX, h - (unsigned int)lastY, 0, 0)));
        debugSsbo.SetSubData(8, 12, glm::value_ptr(glm::uvec4(0, 0, 0, 0)));

#endif


        

        sceneParams.viewMatrix = camera.GetViewMatrix();
        sceneParams.cameraPos = camera.Position;
        Utils::GetTightNearFar(sceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix, 0.001f, sceneParams.cameraNear, sceneParams.cameraFar);
        sceneParams.projectionMatrix = glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight, sceneParams.cameraNear, sceneParams.cameraFar);

        UpdateViewData(sceneUniformBuffers.at(UBOBinding::ViewData), sceneParams);



        // SHADOW PASS ////////////////////////////////////////////////////////////////////////////////////////////////////
        
        sceneParams.sceneLights.Directional.Position = 
            sceneMeshCollection.GetSceneBBCenter() - (glm::normalize(sceneParams.sceneLights.Directional.Direction) * sceneMeshCollection.GetSceneBBSize() * 0.5f);

        
        
        UpdatePerFrameDataUBO(sceneUniformBuffers.at(UBOBinding::PerFrameData), sceneParams);

       
        if (sceneParams.drawParams.doShadows)
            ShadowPass(sceneParams, sceneMeshCollection, sceneUniformBuffers, ForwardUnlit, ForwardPointLight, directionalShadowFBO, pointShadowFBOs);
        

        // AO PASS ////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        // Resize the ssao framebuffer if needed (window resized)
        //if (ssaoFBO.Height() != sceneParams.viewportHeight || ssaoFBO.Width() != sceneParams.viewportWidth)
        //    ssaoFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true, TextureInternalFormat::Rgb_16f);

        //AmbienOcclusionPass(sceneParams, sceneMeshCollection, sceneUniformBuffers, *MeshShaders.at("VIEWNORMALS").get(), ssaoFBO, postProcessingUnit);


        // Tonemapping and Gamma correction
        // --------------------------------
        if (sceneParams.postProcessing.ToneMapping || sceneParams.postProcessing.doGammaCorrection)
        {
            // Resize the main framebuffer if needed (window resized).
            if (mainFBO.Height() != sceneParams.viewportHeight || mainFBO.Width() != sceneParams.viewportWidth)
                mainFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, true);

            SetRenderTarget(sceneFBOs, &mainFBO);
        }

        glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef GFX_SHADER_DEBUG_VIZ
        //ssaoFBO.CopyToOtherFbo(sceneFBOs, &mainFBO, false, 0, true, glm::ivec2(0, 0), glm::ivec2(sceneParams.viewportWidth, sceneParams.viewportHeight));
       
#endif

        // ENVIRONMENT //////////////////////////////////////////////////////////////////////////////////////////////////////
        DrawEnvironment(camera, sceneParams, envCube_vao, sceneUniformBuffers, ForwardSkybox);


        // OPAQUE PASS /////////////////////////////////////////////////////////////////////////////////////////////////////

#if GFX_STOPWATCH
        Profiler::Instance().StartNamedStopWatch("Scene");
#endif
        
        // Draw grid before scene
        if (showGrid)
        {
            DrawExtraScene(
                sceneUniformBuffers, extraSceneMeshCollection, true,
                ForwardUnlit,
                ForwardPoints,
                ForwardLines,
                ForwardLineStrip);
        }

#ifdef GFX_SHADER_DEBUG_VIZ
        glDepthFunc(GL_LEQUAL);
#endif

       

        //////////////////////

        if (gBuffer.Height() != sceneParams.viewportHeight || gBuffer.Width() != sceneParams.viewportWidth)
        {
            gBufferTextures.clear();
            gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
            gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
            gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
            gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
            gBufferTextures.push_back(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Rgba_16f));
            gBuffer = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight,
                std::move(OGLTexture2D(sceneParams.viewportWidth, sceneParams.viewportHeight, TextureInternalFormat::Depth_Component)),
                std::move(gBufferTextures)
            );
        }
        SetRenderTarget(sceneFBOs, &gBuffer);

        GPassStandard.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
        GPassStandard.SetSamplers(TextureBinding::Normals, TextureBinding::Albedo, TextureBinding::Roughness, TextureBinding::Metallic, -1, TextureBinding::Emission);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (int i = 0; i < sceneMeshCollection.size(); i++)
        {
            if (MeshRenderer* mr = dynamic_cast<MeshRenderer*>(sceneMeshCollection.at(i).get()))
                mr->DrawDeferred(&GPassStandard, sceneUniformBuffers);
        }
        ResetRenderTarget(sceneFBOs);

        gBuffer.CopyToOtherFbo(sceneFBOs, sceneFBOs.empty() ? nullptr : sceneFBOs.top(), false, 0, true,
            glm::ivec2(0, 0), glm::ivec2(sceneParams.viewportWidth, sceneParams.viewportHeight));

        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, gBuffer.ColorTextureId(0));
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, gBuffer.ColorTextureId(1));
        glActiveTexture(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, gBuffer.ColorTextureId(2));
        glActiveTexture(GL_TEXTURE0 + 3);
        glBindTexture(GL_TEXTURE_2D, gBuffer.ColorTextureId(3));
        glActiveTexture(GL_TEXTURE0 + 4);
        glBindTexture(GL_TEXTURE_2D, gBuffer.ColorTextureId(4));

        OGLUtils::CheckOGLErrors();
        LightPassStandard.SetCurrent();


        // Noise textures for PCSS calculations
        // ------------------------------------
        glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap0);
        sceneParams.noiseTextures.at(0).Bind();
        
        glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap1);
        sceneParams.noiseTextures.at(1).Bind();
        
        glActiveTexture(GL_TEXTURE0 + TextureBinding::NoiseMap2);
        sceneParams.noiseTextures.at(2).Bind();
        
        glActiveTexture(GL_TEXTURE0 + TextureBinding::PoissonSamples);
        sceneParams.poissonSamples.value().Bind();
        
        // TODO bind texture once for all
        if (sceneParams.sceneLights.Directional.ShadowMapId)
        {
            // Directional light shadow map
            // ----------------------------
            glActiveTexture(GL_TEXTURE0 + TextureBinding::ShadowMapDirectional);
            glBindTexture(GL_TEXTURE_2D, sceneParams.sceneLights.Directional.ShadowMapId);
            
            OGLUtils::CheckOGLErrors();
        }

        for (int i = 0; i < PointLight::MAX_POINT_LIGHTS; i++)
        {
            if (sceneParams.sceneLights.Points[i].ShadowMapId)
            {
                // Point light shadow map
                // ----------------------------
                glActiveTexture(GL_TEXTURE0 + TextureBinding::ShadowMapPoint0 + i);
                OGLUtils::CheckOGLErrors();
                glBindTexture(GL_TEXTURE_CUBE_MAP, sceneParams.sceneLights.Points[i].ShadowMapId);
                OGLUtils::CheckOGLErrors();
                
            }
        }

        if (sceneParams.environment.IrradianceMap.has_value())
        {
            glActiveTexture(GL_TEXTURE0 + TextureBinding::IrradianceMap);
            sceneParams.environment.IrradianceMap.value().Bind();
            
        }
        
        if (sceneParams.environment.RadianceMap.has_value() && sceneParams.environment.LUT.has_value())
        {
         
            glActiveTexture(GL_TEXTURE0 + TextureBinding::RadianceMap);
            sceneParams.environment.RadianceMap.value().Bind();
           
            glActiveTexture(GL_TEXTURE0 + TextureBinding::BrdfLut);
            sceneParams.environment.LUT.value().Bind();
            
        }

        LightPassStandard.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
        LightPassStandard.SetSamplers(
            0, 1, 2, 3, 4,
            TextureBinding::PoissonSamples, TextureBinding::NoiseMap0, TextureBinding::NoiseMap1, TextureBinding::NoiseMap2,
            TextureBinding::BrdfLut, TextureBinding::RadianceMap, TextureBinding::IrradianceMap,
            TextureBinding::ShadowMapDirectional, TextureBinding::ShadowMapPoint0, TextureBinding::ShadowMapPoint1, TextureBinding::ShadowMapPoint2
        );

        OGLUtils::CheckOGLErrors();
        postProcessingUnit.DrawQuad();
        
        //////////////////////


        // TAA Pass
        // --------
        if (sceneParams.postProcessing.doTaa && 
            (sceneParams.postProcessing.ToneMapping || sceneParams.postProcessing.doGammaCorrection))
        {
            // Copy current color 
            taaHistoryFBO[taaBufferIndex]
                .CopyFromOtherFbo(&mainFBO, true, 0, false, glm::vec2(0.0, 0.0), glm::vec2(sceneParams.viewportWidth, sceneParams.viewportHeight));

            // TODO: CopyTo and From always mess the current fbo
            sceneFBOs.top()->Bind();

            // Resize the history framebuffer if needed (window resized).
            if (taaHistoryFBO[0].Height() != sceneParams.viewportHeight || taaHistoryFBO[0].Width() != sceneParams.viewportWidth)
            {
                taaHistoryFBO[0] = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, false, TextureInternalFormat::Rgba_16f, TextureFiltering::Linear, TextureFiltering::Linear);
                taaHistoryFBO[1] = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, false, TextureInternalFormat::Rgba_16f, TextureFiltering::Linear, TextureFiltering::Linear);
                velocityFBO  [0] = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, true,  TextureInternalFormat::Rgba_32f, TextureFiltering::Nearest, TextureFiltering::Nearest);
                velocityFBO  [1] = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, true,  TextureInternalFormat::Rgba_32f, TextureFiltering::Nearest, TextureFiltering::Nearest);
                //init
            }


            // Velocity pass
            // -------------
            velocityPassShader.SetCurrent();
            velocityPassShader.SetUniforms(
                taaPrevVP,
                glm::mat4(1.0f),
                sceneParams.projectionMatrix * sceneParams.viewMatrix,
                glm::mat4(1.0f));
            velocityPassShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);

            SetRenderTarget(sceneFBOs, &velocityFBO[taaBufferIndex]);
            {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                if (sceneParams.postProcessing.writeVelocity)
                {
                    for (int i = 0; i < sceneMeshCollection.size(); i++)
                    {
                        if (MeshRenderer* mr = dynamic_cast<MeshRenderer*>(sceneMeshCollection.at(i).get()))
                        {
                            UpdateObjectData(sceneUniformBuffers.at(UBOBinding::PerObjectData), mr->GetTransformation());
                            mr->DrawCustom();
                        }
                    }
                }
            }ResetRenderTarget(sceneFBOs);

            // -------------


            taaPassShader.SetCurrent();
            taaPassShader.SetUniformBlocks(UBOBinding::PerObjectData, UBOBinding::ViewData, UBOBinding::PerFrameData);
            
            glActiveTexture(GL_TEXTURE0 + TextureBinding::Albedo);      // Current Color
            taaHistoryFBO[taaBufferIndex].BindColorAttachment();
            glActiveTexture(GL_TEXTURE0 + TextureBinding::Normals);     // History Color
            taaHistoryFBO[(taaBufferIndex+1)%2].BindColorAttachment(); 
            glActiveTexture(GL_TEXTURE0 + TextureBinding::Roughness);   // Velocity buffer
            velocityFBO[taaBufferIndex].BindColorAttachment();
            glActiveTexture(GL_TEXTURE0 + TextureBinding::Metallic);    // Depth Buffer
            velocityFBO[taaBufferIndex].BindDepthAttachment();
            glActiveTexture(GL_TEXTURE0 + TextureBinding::AoMap);       // Prev Depth Buffer
            velocityFBO[(taaBufferIndex + 1) % 2].BindDepthAttachment();

            // Just 0, 1, 2, 3....TODO: Not this way
            taaPassShader.SetSamplers(
                TextureBinding::Albedo,
                TextureBinding::Normals,
                TextureBinding::Roughness,
                TextureBinding::Metallic,
                TextureBinding::AoMap
            );

            OGLUtils::CheckOGLErrors();
            postProcessingUnit.DrawQuad();

            taaHistoryFBO[taaBufferIndex]           .UnBindColorAttachment();
            taaHistoryFBO[(taaBufferIndex + 1) % 2] .UnBindColorAttachment();
            velocityFBO[taaBufferIndex]             .UnBindColorAttachment();
            velocityFBO[taaBufferIndex]             .UnBindDepthAttachment();
            velocityFBO[(taaBufferIndex + 1) % 2]   .UnBindDepthAttachment();
            mainFBO                                 .UnBindColorAttachment();

            taaHistoryFBO[taaBufferIndex]
                .CopyFromOtherFbo(&mainFBO, true, 0, false, glm::vec2(0.0, 0.0), glm::vec2(sceneParams.viewportWidth, sceneParams.viewportHeight));
            // TODO: CopyTo and From always mess the current fbo
            sceneFBOs.top()->Bind();

            // Swap the taa history buffers (current becomes old)
            taaBufferIndex = (taaBufferIndex + 1) % 2;

            // Prepare a new offset value for sub-pixel jittering
            taaJitterIndex = (taaJitterIndex + 1) % TAA_JITTER_SAMPLES;
            sceneParams.postProcessing.jitter =taaJitter[taaJitterIndex];

            taaPrevVP = sceneParams.projectionMatrix * sceneParams.viewMatrix;
        }

        UpdatePerFrameDataUBO(sceneUniformBuffers.at(UBOBinding::PerFrameData), sceneParams);
        // Forward for Wires
        for (int i = 0; i < sceneMeshCollection.size(); i++)
        {
            if (WiresRenderer* wr = dynamic_cast<WiresRenderer*>(sceneMeshCollection.at(i).get()))
                wr->Draw(camera.Position, sceneParams);
        }


#if GFX_STOPWATCH
        Profiler::Instance().StopNamedStopWatch("Scene");
#endif

        // DEBUG ////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if (showLights)
        {
            glm::quat orientation = glm::quatLookAt(-normalize(sceneParams.sceneLights.Directional.Direction), glm::vec3(0, 0, 1));
            lightMesh.Renderer::SetTransformation(sceneParams.sceneLights.Directional.Position, glm::angle(orientation), glm::axis(orientation), glm::vec3(0.3, 0.3, 0.3));
            //lightMesh.Draw(camera.Position, sceneParams);
        }
        if (showGrid)
        {
            DrawExtraScene(
                sceneUniformBuffers, extraSceneMeshCollection, false,
                ForwardUnlit,
                ForwardPoints,
                ForwardLines,
                ForwardLineStrip);
        }
        //if (showBoundingBox)
            //bbRenderer.Draw(camera.Position, sceneParams);


        if (sceneParams.postProcessing.ToneMapping || sceneParams.postProcessing.doGammaCorrection)
        {
            ResetRenderTarget(sceneFBOs);

            // Problems when resizind the window:
            // really don't know why this is needed since DepthTesting is disabled 
            // when drawing the quad to apply the post effects...
            // In RenderDoc snapshots everything seems fine but it's not what you get.
            glClear(GL_DEPTH_BUFFER_BIT);

            postProcessingUnit.ApplyToneMappingAndGammaCorrection(mainFBO, 0,
                sceneParams.postProcessing.ToneMapping, sceneParams.postProcessing.Exposure,
                sceneParams.postProcessing.doGammaCorrection, sceneParams.postProcessing.Gamma);

        }

        if (showAO)
            ssaoFBO.CopyToOtherFbo(nullptr, true, 10, false, glm::vec2(0.0, 0.0), glm::vec2(sceneParams.viewportWidth, sceneParams.viewportHeight));



#ifdef GFX_SHADER_DEBUG_VIZ
        DrawShaderDebugViz(
            debugSsbo,
            sceneUniformBuffers,
            debugConeRenderer, 
            debugSphereRenderer,
            debugPointRenderer,
            debugLineRenderer,
            ForwardUnlit,
            ForwardPoints,
            ForwardLines);

#endif

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
    gBuffer.~FrameBuffer();
    GPassStandard.~GPassShader();
    LightPassStandard.~LightPassShader();
    ForwardUnlit.~ForwardUnlitShader();
    ForwardPointLight.~ForwardOmniDirShadowShader();
    ForwardPoints.~ForwardThickPointsShader();
    ForwardLines.~ForwardThickLinesShader();
    ForwardLineStrip.~ForwardThickLineStripShader();
    ForwardSkybox.~ForwardSkyboxShader();
    MeshShaders.clear();
    WiresShaders.clear();
    PostProcessingShaders.clear();
    PointLightShadowMapShader.~ShaderBase();
    taaPassShader.~TaaPassShader();
    velocityPassShader.~VelocityPassShader();
    sceneParams.noiseTextures.~vector();
    sceneParams.poissonSamples.~optional();
    sceneMeshCollection.~SceneMeshCollection();
    extraSceneMeshCollection.~SceneMeshCollection();
    lightMesh.~MeshRenderer();
    ssaoFBO.~FrameBuffer();
    directionalShadowFBO.~FrameBuffer();
    pointShadowFBOs.clear();
    mainFBO.~FrameBuffer();
    taaHistoryFBO[0].~FrameBuffer();
    taaHistoryFBO[1].~FrameBuffer();
    velocityFBO[0].~FrameBuffer();
    velocityFBO[1].~FrameBuffer();
    lightMesh.~MeshRenderer();
    bbRenderer.~WiresRenderer();
    environmentShader.~ShaderBase();
    envCube_ebo.~IndexBufferObject();
    envCube_vbo.~VertexBufferObject();
    envCube_vao.~VertexAttribArray();
#ifdef GFX_SHADER_DEBUG_VIZ
    debugSsbo.~ShaderStorageBufferObject();
    debugConeRenderer.~MeshRenderer();
    debugSphereRenderer.~MeshRenderer();
    debugLineRenderer.~WiresRenderer();
    debugPointRenderer.~WiresRenderer();
#endif
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

    /* Freefly camera controls*/
    if (lastKey == GLFW_KEY_W) { camera.MoveTowards(glm::vec3(0, -1, 0)); }
    if (lastKey == GLFW_KEY_A) { camera.MoveTowards(glm::vec3(1, 0, 0)); }
    if (lastKey == GLFW_KEY_S) { camera.MoveTowards(glm::vec3(0, 1, 0)); }
    if (lastKey == GLFW_KEY_D) { camera.MoveTowards(glm::vec3(-1, 0, 0)); }

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
        switch (key)
        {

            /* Set view with rotation animation*/
            case GLFW_KEY_KP_1: camera.SetView(ViewType::Top);      break;
            case GLFW_KEY_KP_3: camera.SetView(ViewType::Right);      break;
            case GLFW_KEY_KP_7: camera.SetView(ViewType::Front);      break;
            case GLFW_KEY_KP_9: camera.SetView(ViewType::Back);      break;

        }

        lastKey = key;
    }
    if (action == GLFW_RELEASE)
    {
        if (key == lastKey)   
            lastKey = -1;      
    }
}




