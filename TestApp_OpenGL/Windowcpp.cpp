#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "Shader.h"
#include "Mesh.h"
#include "stb_image.h"
#include "Camera.h"
#include "Mesh.h"
#include "GeometryHelper.h"
#include "SceneUtils.h"
#include "FrameBuffer.h"
#include "Scene.h"

// CONSTANTS ======================================================
const char* glsl_version = "#version 130";

const unsigned int shadowMapResolution = 2048;
const int SSAO_MAX_SAMPLES = 64;
const int SSAO_BLUR_MAX_RADIUS = 16;


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_scroll_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

Camera camera = Camera( 5, 0, 0);

bool firstMouse = true;
float lastX, lastY, yaw, pitch, roll;
//unsigned int width, height;


// Debug ===========================================================
bool showGrid = false;
bool showLights = false;
bool showBoundingBox = false;
bool showAO = false;


// Camera ==========================================================
bool perspective = true;
float fov = 45.0f;

// Scene Parameters ==========================================================
SceneParams sceneParams = SceneParams();

// BBox ============================================================
using namespace Utils;


// ImGUI ============================================================
const char* ao_comboBox_items[] = {"SSAO", "HBAO"};
static const char* ao_comboBox_current_item = "SSAO";

AOType AOShaderFromItem(const char* item)
{
    if (!strcmp("SSAO", item))
        return AOType::SSAO;

    else if (!strcmp("HBAO", item))
        return AOType::HBAO;

    else return AOType::SSAO;
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
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Lights"))
        {
            if (ImGui::CollapsingHeader("Ambient", ImGuiTreeNodeFlags_None))
            {
                ImGui::ColorEdit4("Color", (float*)&(sceneParams.sceneLights.Ambient.Ambient));
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

            if (ImGui::CollapsingHeader("Directional", ImGuiTreeNodeFlags_None))
            {
                ImGui::DragFloat3("Direction", (float*)&(sceneParams.sceneLights.Directional.Direction), 0.05f, -1.0f, 1.0f);
                ImGui::ColorEdit4("Diffuse", (float*)&(sceneParams.sceneLights.Directional.Diffuse));
                ImGui::ColorEdit4("Specular", (float*)&(sceneParams.sceneLights.Directional.Specular));

                if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_None))
                {
                    ImGui::Checkbox("Enable", &sceneParams.drawParams.doShadows);
                    ImGui::DragFloat("Bias", &sceneParams.sceneLights.Directional.Bias, 0.001f, 0.0f, 0.05f);
                    ImGui::DragFloat("SlopeBias", &sceneParams.sceneLights.Directional.SlopeBias, 0.001f, 0.0f, 0.05f);
                    ImGui::DragFloat("Softness", &sceneParams.sceneLights.Directional.Softness, 0.00005f, 0.0f, 0.01f);
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

  
    
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));

    
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

void LoadScene_PCSStest(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
    Mesh pillar_0 = Mesh::Box(1, 1, 4);
    sceneMeshCollection.AddToCollection(
        std::make_shared<MeshRenderer>(glm::vec3(0, 0, 1), 0.0, glm::vec3(0, 0, 1), glm::vec3(1, 1, 1), pillar_0,
            (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
            (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed));

    Mesh pillar_1 = Mesh::Box(0.25, 0.25, 4.5);
    sceneMeshCollection.AddToCollection(
        std::make_shared<MeshRenderer>(glm::vec3(1.5, 0, 0), 0.0, glm::vec3(0, 0, 1), glm::vec3(1, 1, 1), pillar_1,
            (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
            (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed));

    Mesh pillar_2 = Mesh::Box(0.1, 0.1, 5);
    sceneMeshCollection.AddToCollection(
        std::make_shared<MeshRenderer>(glm::vec3(3.0, 0, 0), 0.0, glm::vec3(0, 0, 1), glm::vec3(1, 1, 1), pillar_2,
            (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
            (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed));


    /*Mesh coneMesh = Mesh::Cone(0.8, 1.6, 16);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(1, 2, 1), 1.5, glm::vec3(0, -1, 1), glm::vec3(1, 1, 1), &coneMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen));


    Mesh cylMesh = Mesh::Cylinder(0.6, 1.6, 16);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-1, -0.5, 1), 1.1, glm::vec3(0, 1, 1), glm::vec3(1, 1, 1), &cylMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::Copper));*/


    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-5, -5, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(10, 10, 0.25), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));


}

void LoadPlane(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection, float size)
{
    
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(glm::vec3(-size/2.0f, -size/2.0f, -size/20.0f), 0.0, glm::vec3(0, 0, 1), glm::vec3(size, size, size/20.0f), planeMesh,
        (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(),
        (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray));

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

void LoadScene_ALotOfMonkeys(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>>* shadersCollection)
{
    Mesh planeMesh = Mesh::Box(1, 1, 1);
    MeshRenderer plane =
        MeshRenderer(glm::vec3(-4, -4, -0.25), 0.0, glm::vec3(0, 0, 1), glm::vec3(8, 8, 0.25),
            planeMesh, shadersCollection->at("LIT_WITH_SHADOWS_SSAO").get(), shadersCollection->at("LIT_WITH_SSAO").get(), MaterialsCollection::MatteGray);

    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(plane)));

    FileReader reader = FileReader("Assets/Models/suzanne.obj");
    reader.Load();
    Mesh monkeyMesh = reader.Meshes()[0];
    MeshRenderer monkey1 =
        MeshRenderer(glm::vec3(-1.0, -1.0, 0.4), 0.9, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::ShinyRed);
    monkey1.Transform(glm::vec3(0, 0, 0), glm::pi<float>() / 4.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0));

    MeshRenderer monkey2 =
        MeshRenderer(glm::vec3(-1.0, 1.0, 0.4), 0.9, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PlasticGreen);
    monkey2.Transform(glm::vec3(-1.5, 1.5, 0), 3.0 * glm::pi<float>() / 4.0, glm::vec3(0, 0.0, -1.0), glm::vec3(1.0, 1.0, 1.0));

    MeshRenderer monkey3 =
        MeshRenderer(glm::vec3(0.0, 0.5, 1.0), 1.2, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::Copper);
    //monkey3.Transform(glm::vec3(0, 0, 0), 3.0 * glm::pi<float>() / 4.0, glm::vec3(0, 0.0, 1.0), glm::vec3(1.0, 1.0, 1.0));

    MeshRenderer monkey4 =
        MeshRenderer(glm::vec3(1.0, -1.0, 0.4), 0.9, glm::vec3(1, 0, 0), glm::vec3(1, 1, 1),
            monkeyMesh, (*shadersCollection).at("LIT_WITH_SHADOWS_SSAO").get(), (*shadersCollection).at("LIT_WITH_SSAO").get(), MaterialsCollection::PureWhite);
    monkey4.Transform(glm::vec3(0, 0, 0), glm::pi<float>() / 4.0, glm::vec3(0, 0.0, 1.0), glm::vec3(1.0, 1.0, 1.0));

    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey1)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey2)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey3)));
    sceneMeshCollection.AddToCollection(std::make_shared<MeshRenderer>(std::move(monkey4)));
}
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
            std::make_shared<MeshRenderer>(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(0.6, 0.6, 0.6),
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
            std::make_shared<MeshRenderer>(glm::vec3(0, 0, 0), glm::pi<float>() * 0, glm::vec3(1, 0, 0), glm::vec3(0.6, 0.6, 0.6),
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
   
    LoadSceneFromPath("./Assets/Models/TechnoDemon.fbx", sceneMeshCollection, shadersCollection);

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

    
    Material matBody = Material{ glm::vec4(1.0), glm::vec4(1.0), 64, "TechnoDemon_Body" };
    Material matArm = Material{ glm::vec4(1.0), glm::vec4(1.0), 2, "TechnoDemon_Arm" };
  
    sceneMeshCollection.at(5).get()->SetMaterial(matBody);
    sceneMeshCollection.at(6).get()->SetMaterial(matArm);
    sceneMeshCollection.at(3).get()->SetMaterial(matBody);
    sceneMeshCollection.at(4).get()->SetMaterial(matArm);

    // ----------------- //
    LoadPlane(sceneMeshCollection, shadersCollection, 5.0);
}

void SetupScene(SceneMeshCollection& sceneMeshCollection, std::map<std::string, std::shared_ptr<MeshShader>> *shadersCollection)
{
    //LoadPlane(sceneMeshCollection, shadersCollection, 10.0);
    //LoadScene_NormalMapping(sceneMeshCollection, shadersCollection);
    LoadScene_TechnoDemon(sceneMeshCollection, shadersCollection);
    //LoadSceneFromPath("./Assets/Models/suzanne.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::ClayShingles);
    //LoadSceneFromPath("./Assets/Models/Trex.obj", sceneMeshCollection, shadersCollection, Material{glm::vec4(1.0), glm::vec4(1.0), 64, "Trex"});
    //LoadSceneFromPath("./Assets/Models/Draenei.fbx", sceneMeshCollection, shadersCollection, Material{glm::vec4(1.0), glm::vec4(1.0), 64});
    //LoadSceneFromPath("./Assets/Models/TestPCSS.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::ShinyRed);
    //LoadSceneFromPath("./Assets/Models/Dragon.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::PlasticGreen);
    //LoadSceneFromPath("./Assets/Models/Knob.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::PlasticGreen);
    //LoadSceneFromPath("./Assets/Models/trees.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::MatteGray);
    //LoadSceneFromPath("./Assets/Models/OldBridge.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::PlasticGreen);
    //LoadSceneFromPath("./Assets/Models/Engine.obj", sceneMeshCollection, shadersCollection, MaterialsCollection::PlasticGreen);
    //LoadScene_ALotOfMonkeys(sceneMeshCollection, shadersCollection);
    //LoadScene_Primitives(sceneMeshCollection, shadersCollection);
    //LoadScene_PCSStest(sceneMeshCollection, shadersCollection);
    //LoadScene_Cadillac(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Dragon(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Nefertiti(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Bunny(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Jinx(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Engine(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_Knob(sceneMeshCollection, shadersCollection, sceneBoundingBox);
    //LoadScene_AoTest(sceneMeshCollection, shadersCollection);
    //LoadScene_Porsche(sceneMeshCollection, shadersCollection, sceneBoundingBox);

}

std::map<std::string, std::shared_ptr<MeshShader>> InitializeShaders()
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
void UpdateMatricesUBO(UniformBufferObject& ubo, const SceneParams& sceneParams)
{
    ubo.SetData<float>(34, NULL);
    ubo.SetSubData(0 , 16, glm::value_ptr(sceneParams.viewMatrix));
    ubo.SetSubData(16, 16, glm::value_ptr(sceneParams.projectionMatrix));
    ubo.SetSubData(32, 1, &sceneParams.cameraNear);
    ubo.SetSubData(33, 1, &sceneParams.cameraFar);
}

void UpdateLightsUBO(UniformBufferObject& ubo, SceneLights& lights, Camera& camera)
{
    ubo.SetData<float>(20, NULL);

    ubo.SetSubData(0, 4, glm::value_ptr(lights.Directional.Direction));
    ubo.SetSubData(4, 4, glm::value_ptr(lights.Directional.Diffuse));
    ubo.SetSubData(8, 4, glm::value_ptr(lights.Directional.Specular));

    ubo.SetSubData(12, 4, glm::value_ptr(lights.Ambient.Ambient));
    ubo.SetSubData(16, 4, glm::value_ptr(camera.Position));
}

void UpdateShadowsUBO(UniformBufferObject& ubo, SceneLights& lights)
{
    ubo.SetData<float>(20, NULL);

    ubo.SetSubData(0, 16, glm::value_ptr(lights.Directional.LightSpaceMatrix));
    ubo.SetSubData(16, 1, &lights.Directional.Bias);
    ubo.SetSubData(17, 1, &lights.Directional.SlopeBias);
    ubo.SetSubData(18, 1, &lights.Directional.Softness);
    ubo.SetSubData(19, 1, &lights.Directional.ShadowMapToWorld);
}

void UpdateAoUBO(UniformBufferObject& ubo, SceneLights& lights)
{
    ubo.SetData<float>(1, NULL);
    ubo.SetSubData(0, 1, &lights.Ambient.aoStrength);
}

void ShadowPass(
    SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection, 
    std::vector<UniformBufferObject>& sceneUniformBuffers, const MeshShader& shaderForShadows, const FrameBuffer& shadowFBO)
{
    glm::mat4
        viewShadow, projShadow;

    Utils::GetShadowMatrices(sceneParams.sceneLights.Directional.Position, sceneParams.sceneLights.Directional.Direction, sceneMeshCollection.GetSceneBBPoints(), viewShadow, projShadow);
    
    sceneParams.sceneLights.Directional.LightSpaceMatrix = projShadow * viewShadow;
    sceneParams.viewMatrix = viewShadow;
    sceneParams.projectionMatrix = projShadow;
    sceneParams.sceneLights.Directional.ShadowMapToWorld = (2.0f / projShadow[0][0]);
    UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);

    shadowFBO.Bind(true, true);

    glViewport(0, 0, shadowMapResolution, shadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    OGLUtils::CheckOGLErrors();

    for (int i = 0; i < sceneMeshCollection.size(); i++)
    {
        sceneMeshCollection.at(i).get()->DrawCustom(shaderForShadows);
    }

    sceneParams.sceneLights.Directional.ShadowMapId = shadowFBO.DepthTextureId();
    shadowFBO.UnBind();
    
}

void AmbienOcclusionPass(
    SceneParams& sceneParams, const SceneMeshCollection& sceneMeshCollection, std::vector<UniformBufferObject>& sceneUniformBuffers, 
    const MeshShader& shaderForDepthPass, const FrameBuffer& ssaoFBO, const PostProcessingUnit& postProcessingUnit)
{

    ssaoFBO.Bind(true, true);
    glDrawBuffer(GL_COLOR_ATTACHMENT0 + 1);
    glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    sceneParams.viewMatrix = camera.GetViewMatrix();
    Utils::GetTightNearFar(sceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix, sceneParams.cameraNear, sceneParams.cameraFar);

    sceneParams.projectionMatrix = glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight, sceneParams.cameraNear, sceneParams.cameraFar); //TODO: near is brutally hard coded in PCSS calculations (#define NEAR 0.1)

    UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);
    
    OGLUtils::CheckOGLErrors();

    for (int i = 0 ; i < sceneMeshCollection.size() ; i++)
    {
        sceneMeshCollection.at(i).get()->DrawCustom(shaderForDepthPass);
    }

    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    ssaoFBO.UnBind();

    postProcessingUnit.ComputeAO(AOShaderFromItem(ao_comboBox_current_item), ssaoFBO, sceneParams, sceneParams.viewportWidth, sceneParams.viewportHeight);

    // Blur 
    postProcessingUnit.BlurTexture(ssaoFBO, 0, sceneParams.sceneLights.Ambient.aoBlurAmount, sceneParams.viewportWidth, sceneParams.viewportHeight);

    sceneParams.sceneLights.Ambient.AoMapId = ssaoFBO.ColorTextureId();
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1000, 1000, "TestApp_OpenGL", NULL, NULL);

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

    sceneParams.viewportWidth = sceneParams.viewportHeight = 800;
    glViewport(0, 0, sceneParams.viewportWidth, sceneParams.viewportHeight);

    // Scene View and Projection matrices
    sceneParams.viewMatrix = glm::mat4(1);
    sceneParams.cameraNear = 0.1f, sceneParams.cameraFar = 50.0f;
    sceneParams.projectionMatrix = glm::perspective(glm::radians(45.0f), sceneParams.viewportHeight / (float)sceneParams.viewportWidth, sceneParams.cameraNear, sceneParams.cameraFar);

    // Shaders Collections 
    static std::map<std::string, std::shared_ptr<MeshShader>> Shaders = InitializeShaders();
    static std::map<std::string, std::shared_ptr<ShaderBase>> PostProcessingShaders = InitializePostProcessingShaders();

    // Setup Scene
    
    SceneMeshCollection sceneMeshCollection;

    SetupScene(sceneMeshCollection, &Shaders);

    camera.CameraOrigin = sceneMeshCollection.GetSceneBBCenter();
    camera.ProcessMouseMovement(0, 0); //TODO: camera.Update()

    // DEBUG
    Mesh lightArrow{ Mesh::Arrow(0.3, 1, 0.5, 0.5, 16) };
    MeshRenderer lightMesh =
        MeshRenderer(glm::vec3(0, 0, 0), 0, glm::vec3(0, 1, 1), glm::vec3(0.3, 0.3, 0.3),
            lightArrow, Shaders.at("UNLIT").get(), Shaders.at("UNLIT").get(), MaterialsCollection::PureWhite);

    LinesRenderer grid =
        LinesRenderer(glm::vec3(0, 0, 0), 0.0f, glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), Wire::Grid(glm::vec2(-5, -5), glm::vec2(5, 5), 1.0f),
            Shaders.at("UNLIT").get(), glm::vec4(0.5, 0.5, 0.5, 1.0));


    std::vector<glm::vec3> bboxLines = sceneMeshCollection.GetSceneBBLines();
    Wire bbWire = Wire(bboxLines);
    LinesRenderer bbRenderer =
        LinesRenderer(glm::vec3(0, 0, 0), 0.0f, glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), bbWire,
            Shaders.at("UNLIT").get(), glm::vec4(1.0, 0.0, 1.0, 1.0));

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
    sceneParams.sceneLights.Ambient.Ambient = glm::vec4(1, 1, 1, 0.6);
    sceneParams.sceneLights.Ambient.aoRadius = 0.2;
    sceneParams.sceneLights.Ambient.aoSamples = 16;
    sceneParams.sceneLights.Ambient.aoSteps = 16;
    sceneParams.sceneLights.Ambient.aoBlurAmount = 3;
    sceneParams.sceneLights.Directional.Direction = glm::vec3(0.9, 0.9, -0.9);
    sceneParams.sceneLights.Directional.Diffuse = glm::vec4(1.0, 1.0, 1.0, 0.75);
    sceneParams.sceneLights.Directional.Specular = glm::vec4(1.0, 1.0, 1.0, 0.75);

    // Shadow Map
    FrameBuffer shadowFBO = FrameBuffer(shadowMapResolution, shadowMapResolution, false, 0, true);

    // Noise Textures for PCSS 
    std::vector<OGLTexture2D> noiseTextures{};

    noiseTextures.push_back(OGLTexture2D(
        9,9,
        TextureInternalFormat::R_16f,
        GetUniformDistributedNoise(81, -1.0, 1.0).data(), GL_RED, GL_FLOAT,
        TextureFiltering::Nearest, TextureFiltering::Nearest,
        TextureWrap::Repeat, TextureWrap::Repeat));

    noiseTextures.push_back(OGLTexture2D(
        10, 10,
        TextureInternalFormat::R_16f,
        GetUniformDistributedNoise(100, -1.0, 1.0).data(), GL_RED, GL_FLOAT,
        TextureFiltering::Nearest, TextureFiltering::Nearest,
        TextureWrap::Repeat, TextureWrap::Repeat));

    noiseTextures.push_back(OGLTexture2D(
        11, 11,
        TextureInternalFormat::R_16f,
        GetUniformDistributedNoise(121, -1.0, 1.0).data(), GL_RED, GL_FLOAT,
        TextureFiltering::Nearest, TextureFiltering::Nearest,
        TextureWrap::Repeat, TextureWrap::Repeat));

    sceneParams.noiseTexId_0 = noiseTextures.at(0).ID();
    sceneParams.noiseTexId_1 = noiseTextures.at(1).ID();
    sceneParams.noiseTexId_2 = noiseTextures.at(2).ID();

    // AmbientOcclusion Map
    FrameBuffer ssaoFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true);

    // Tone-mapping and gamma correction
    FrameBuffer mainFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 1, true);
    sceneParams.postProcessing.ToneMapping = false;
    sceneParams.postProcessing.Exposure = 1.0f;
    sceneParams.postProcessing.GammaCorrection = false;

    // Post processing unit (ao, blur, bloom, ...)
    PostProcessingUnit postProcessingUnit(SSAO_BLUR_MAX_RADIUS, SSAO_MAX_SAMPLES);

    // Uniform buffer objects 
    std::vector<UniformBufferObject> sceneUniformBuffers{};
    for (int i = 0; i <= UBOBinding::AmbientOcclusion; i++)
        sceneUniformBuffers.push_back(std::move(UniformBufferObject(UniformBufferObject::UBOType::StaticDraw)));
    
    BindUBOs(sceneUniformBuffers);

    glEnable(GL_DEPTH_TEST);

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
            ShadowPass(sceneParams, sceneMeshCollection, sceneUniformBuffers, *Shaders.at("UNLIT").get(), shadowFBO);
        

        // AO PASS ////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        // Resize the ssao framebuffer if needed (window resized)
        if (ssaoFBO.Height() != sceneParams.viewportHeight || ssaoFBO.Width() != sceneParams.viewportWidth)
            ssaoFBO = FrameBuffer(sceneParams.viewportWidth, sceneParams.viewportHeight, true, 2, true);

        AmbienOcclusionPass(sceneParams, sceneMeshCollection, sceneUniformBuffers, *Shaders.at("VIEWNORMALS").get(), ssaoFBO, postProcessingUnit);


        // OPAQUE PASS /////////////////////////////////////////////////////////////////////////////////////////////////////

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

        sceneParams.viewMatrix = camera.GetViewMatrix();
        Utils::GetTightNearFar(sceneMeshCollection.GetSceneBBPoints(), sceneParams.viewMatrix, sceneParams.cameraNear, sceneParams.cameraFar);
        sceneParams.projectionMatrix = glm::perspective(glm::radians(fov), sceneParams.viewportWidth / (float)sceneParams.viewportHeight, sceneParams.cameraNear, sceneParams.cameraFar); 

        UpdateMatricesUBO(sceneUniformBuffers.at(UBOBinding::Matrices), sceneParams);
        UpdateLightsUBO(sceneUniformBuffers.at(UBOBinding::Lights), sceneParams.sceneLights, camera);
        UpdateShadowsUBO(sceneUniformBuffers.at(UBOBinding::Shadows), sceneParams.sceneLights);
        UpdateAoUBO(sceneUniformBuffers.at(UBOBinding::AmbientOcclusion), sceneParams.sceneLights);

        for (int i = 0; i < sceneMeshCollection.size(); i++)
        {
            sceneMeshCollection.at(i).get()->Draw(camera.Position, sceneParams);
        }
        
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


        // DEBUG ////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        if (showAO)
            ssaoFBO.CopyToOtherFbo(nullptr, true, 10, false, glm::vec2(0.0, 0.0), glm::vec2(sceneParams.viewportWidth, sceneParams.viewportHeight));

        if (showLights)
        {
            glm::quat orientation = glm::quatLookAt(-normalize(sceneParams.sceneLights.Directional.Direction), glm::vec3(0, 0, 1));
            lightMesh.SetTransformation(sceneParams.sceneLights.Directional.Position, glm::angle(orientation), glm::axis(orientation), glm::vec3(0.3, 0.3, 0.3));
            lightMesh.Draw(camera.Position, sceneParams);
        }
        if (showGrid)
            grid.Draw(camera.GetViewMatrix(), sceneParams.projectionMatrix, camera.Position, sceneParams.sceneLights);
        if (showBoundingBox)
            bbRenderer.Draw(camera.GetViewMatrix(), sceneParams.projectionMatrix, camera.Position, sceneParams.sceneLights);


        // WINDOW /////////////////////////////////////////////////////////////////////////////////////////////////////
        if (showWindow)
        {
            ShowImGUIWindow();

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
    Shaders.clear();
    PostProcessingShaders.clear();
    noiseTextures.clear();
    sceneMeshCollection.~SceneMeshCollection();
    lightMesh.~MeshRenderer();
    ssaoFBO.~FrameBuffer();
    shadowFBO.~FrameBuffer();
    mainFBO.~FrameBuffer();
    grid.~LinesRenderer();
    lightMesh.~MeshRenderer();
    bbRenderer.~LinesRenderer();
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
        camera.Reset();
    }
}



