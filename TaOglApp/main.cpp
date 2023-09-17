// 3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include <thread>
#include <variant>

#include "GeometricPrimitives.h"
#include "RenderContext.h"
#include "GizmosRenderer.h"
#include "GizmosHelper.h"
#include "PbrRenderer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

using namespace std;
using namespace glm;
using namespace std::chrono;
using namespace tao_ogl_resources;
using namespace tao_math;
using namespace tao_render_context;
using namespace tao_gizmos_procedural;
using namespace tao_gizmos_sdf;
using namespace tao_gizmos;
//using namespace tao_input;
using namespace tao_pbr;


enum MouseInputAgentTag
{
    None           = 0x0,
    CameraRotate   = 1u<<1,
    CameraZoom     = 1u<<2
};

class MouseInputManager;

enum mouse_button
{
    left = GLFW_MOUSE_BUTTON_LEFT,
    middle = GLFW_MOUSE_BUTTON_MIDDLE,
    right = GLFW_MOUSE_BUTTON_RIGHT,
};
enum key
{
    left_shift = GLFW_KEY_LEFT_SHIFT
};

class MouseInputAgent
{
public:
    MouseInputAgent(MouseInputAgentTag tag):
    _tag{tag}
    {
    }

    virtual void MouseUp  (float, float, int, int)  = 0;
    virtual void MouseDown(float, float, int, int)  = 0;
    virtual void MouseMove(float, float, int, int)  = 0;
    virtual void Mute()    = 0;
    virtual void UnMute()  = 0;

    MouseInputAgentTag  GetTag()      const{return _tag;}

    void SetInputManager(MouseInputManager* manager)
    {
        _manager = manager;
    }

private:
    MouseInputAgentTag _tag = MouseInputAgentTag::None;

protected:
    MouseInputManager* _manager;
};

class MouseInputManager
{
public:

    MouseInputManager(RenderContext* renderContext) :
    _rc{renderContext},
    //_mouseListeners{},
    _agents{},
    _muted{None}
    {
    }

    void PollMouseEvents()
    {
        // prevent mouse clicks to "pass through" ImGui
        // panels and widgets.
        // rc->PollEvents calls agents mouse down callbacks.
        if(const auto& io = ImGui::GetIO(); io.WantCaptureMouse)
        {
            glfwPollEvents();
            return;
        }

        _rc->PollEvents();

        int surfaceWidth, surfaceHeight;
        double posX, posY;
        _rc->GetCursorPosition(&posX, &posY);
        _rc->GetWindowSize(&surfaceWidth, &surfaceHeight);

        // from top-left origin
        // to bottom-left
        posY = surfaceHeight-posY;

        posX = glm::clamp(posX, 0.0, static_cast<double>(surfaceWidth));
        posY = glm::clamp(posY, 0.0, static_cast<double>(surfaceHeight));

        for(int i=0;i<_agents.size();i++)
        {
            if(_agents[i]->GetTag() & _muted) continue;

            _agents[i]->MouseMove(
                    glm::min(static_cast<float>(surfaceWidth) , glm::max(0.0f, static_cast<float>(posX))),
                    glm::min(static_cast<float>(surfaceHeight), glm::max(0.0f, static_cast<float>(posY))),
                    surfaceWidth, surfaceHeight);
        }

        UpdateMouseButton(mouse_button::left  , posX, posY, surfaceWidth, surfaceHeight);
        UpdateMouseButton(mouse_button::middle, posX, posY, surfaceWidth, surfaceHeight);
        UpdateMouseButton(mouse_button::right , posX, posY, surfaceWidth, surfaceHeight);
    }

    double GetTime()
    {
        return _rc->GetTime();
    }
    bool IsKeyPressed(key k)
    {
        return _rc->IsKeyPressed(k);
    }
    bool IsMouseButtonPressed(mouse_button b)
    {
        return _rc->IsMouseButtonPressed(b);
    }

    void AddInputAgent(MouseInputAgent* agent)
    {
        agent->SetInputManager(this);

        _agents.push_back(agent);
    }

    void MuteAgents(MouseInputAgentTag tag)
    {
        for(int i=0;i<_agents.size();i++)
        {
            bool isMuted        = _agents[i]->GetTag() & _muted;
            bool willBeMuted    = _agents[i]->GetTag() & tag;

            if (!isMuted&&willBeMuted) _agents[i]->Mute();
        }

        _muted = static_cast<MouseInputAgentTag>(_muted|tag);
    }

    void UnMuteAgents(MouseInputAgentTag tag)
    {
        for(int i=0;i<_agents.size();i++)
        {
            bool isMuted          = _agents[i]->GetTag() & _muted;
            bool willBeUnMuted    = _agents[i]->GetTag() & tag;

            if (isMuted&&willBeUnMuted) _agents[i]->UnMute();
        }

        _muted = static_cast<MouseInputAgentTag>(_muted&(~tag));
    }

private:
    RenderContext*                          _rc;
    //vector<shared_ptr<MouseInputListener>>  _mouseListeners;
    vector<MouseInputAgent*>                _agents;
    MouseInputAgentTag                      _muted = None;
    map<mouse_button, bool> _buttons = {make_pair(mouse_button::left, false), make_pair(mouse_button::middle, false), make_pair(mouse_button::right, false)};

    void FireMouseUp(mouse_button button, float x, float y, int w, int h)
    {
        for(const auto& a : _agents)
        {
            if(a->GetTag() & _muted) continue;

            a->MouseUp(x, y, w, h);
        }
    }
    void FireMouseDown(mouse_button button, float x, float y, int w, int h)
    {
        for(const auto& a : _agents)
        {
            if(a->GetTag() & _muted) continue;

            a->MouseDown(x, y, w, h);
        }
    }

    void UpdateMouseButton(mouse_button button, float x, float y, int w, int h)
    {
        bool wasPressed = _buttons[button];
        bool isPressed = _rc->IsMouseButtonPressed(button);

        if(!(wasPressed^isPressed)) return; // nothing new happened

        if(!wasPressed && isPressed) // fire mouse down
        {
            // if `button` is pressed, fire mouse down for
            // all the other buttons (if any was down)
            for(int i=mouse_button::left; i<=mouse_button::right; i++)
            {
                mouse_button b = static_cast<mouse_button>(i);

                if(b==button) continue;

                if(_buttons[b])
                {
                    _buttons[b] = false;
                    FireMouseUp(b, x, y, w, h);
                }
            }

            _buttons[button] = true;
            FireMouseDown(button, x, y, w, h);
        }
        if(wasPressed && !isPressed) // fire mouse up
        {
            _buttons[button] = false;
            FireMouseUp(button, x, y, w, h);
        }
    }
};

class CameraZoomAgent : public MouseInputAgent
{
public:
    CameraZoomAgent(float initialDistance):
            MouseInputAgent{CameraZoom},
            _enabled{false},
            _distance{initialDistance},
            _zoom{1.0f}
    {
        ZoomCamera(0.0f);
    }
    mat4 GetZoomMatrix()
    {
        return _zoom;
    }

    void MouseUp(float x, float y, int w, int h) override
    {
        if(!_enabled) return;

        _enabled = false;
        _manager->UnMuteAgents(CameraRotate);
    }
    void MouseDown(float x, float y, int w, int h) override
    {
        if(!ShouldListen()) return;

        _enabled = true;
        _manager->MuteAgents(CameraRotate);
        _latestMouseY = y;
        _targetMouseY = y;
        _latestTimeMs = _manager->GetTime()*1e3f;
    }
    void MouseMove(float x, float y, int w, int h) override
    {
        // NOTE:
        // With smooth motion (DamperMotion())
        // we should continue moving the camera
        // even after mouse up

        if(_enabled)
        {
            _targetMouseY = y;
        }

        float deltaTimeMs = _manager->GetTime()*1e3f - _latestTimeMs;
        float smoothY = DamperMotion(_latestMouseY, _targetMouseY, _kDampHalfTimeMs, deltaTimeMs);

        float dy = (smoothY-_latestMouseY)/h;

        ZoomCamera(dy);

        _latestMouseY = smoothY;
        _latestTimeMs +=deltaTimeMs;
    }

    void Mute() override
    {
    }

    void UnMute() override
    {
        _targetMouseY = _latestMouseY;
    }

private:
    bool _enabled = false;
    float _latestMouseY;
    float _targetMouseY;
    float _latestTimeMs;
    float _distance;
    mat4  _zoom;

    static constexpr float _kDampHalfTimeMs = 50.0f;

    bool ShouldListen()
    {
        return
                MouseInputAgent::_manager->IsKeyPressed(left_shift) &&
                MouseInputAgent::_manager->IsMouseButtonPressed(middle);
    }

    void ZoomCamera(float dy)
    {
        const float kMinDst = 0.1;
        const float kMaxDst = 30.0;
        const float kSensitivity = 65.0f;

        float f = mix(0.05f, 1.0f, (_distance-kMinDst)/(kMaxDst-kMinDst));
        float newDst = _distance - dy * kSensitivity * f;
        newDst = glm::clamp(newDst, kMinDst, kMaxDst);
        _distance = newDst;

        _zoom = translate(mat4{1.0}, _distance * vec3{0.0, 0.0, -1.0});
    }
};

class CameraRotationAgent : public MouseInputAgent
{
public:
    CameraRotationAgent(vec3 initialEyePosition, vec3 initialTarget, vec3 initialUp):
    MouseInputAgent{CameraRotate},
    _enabled{false},
    _latestMouseX{0.0f},
    _latestMouseY{0.0f},
    _eyePosition{initialEyePosition},
    _target{initialTarget},
    _up{initialUp},
    _rotation{1.0f}
    {
        RotateCamera(0.0f, 0.0f);
    }
    mat4 GetRotationMatrix()
    {
        return _rotation;
    }

    void MouseUp(float x, float y, int w, int h) override
    {
        if(!_enabled) return;

        _enabled = false;
        _manager->UnMuteAgents(CameraZoom);
    }
    void MouseDown(float x, float y, int w, int h) override
    {
        if(!ShouldListen()) return;

        _enabled = true;
        _manager->MuteAgents(CameraZoom);

        _latestMouseX = x;
        _latestMouseY = y;
        _targetMouseX = x;
        _targetMouseY = y;
        _latestTimeMs = MouseInputAgent::_manager->GetTime()*1e3f;
    }
    void MouseMove(float x, float y, int w, int h) override
    {
        // NOTE:
        // With smooth motion (DamperMotion())
        // we should continue moving the camera
        // even after mouse up

        if(_enabled)
        {
            _targetMouseX = x;
            _targetMouseY = y;
        }

        float deltaTimeMs = MouseInputAgent::_manager->GetTime()*1e3f - _latestTimeMs;
        float smoothX = DamperMotion(_latestMouseX, _targetMouseX, _kDampHalfTimeMs, deltaTimeMs);
        float smoothY = DamperMotion(_latestMouseY, _targetMouseY, _kDampHalfTimeMs, deltaTimeMs);

        float dx = (smoothX-_latestMouseX)/w;
        float dy = (smoothY-_latestMouseY)/h;

        RotateCamera(dx, dy);

        _latestMouseX = smoothX;
        _latestMouseY = smoothY;
        _latestTimeMs +=deltaTimeMs;
    }

    void Mute() override
    {
    }

    void UnMute() override
    {
        _latestMouseX = _targetMouseX;
        _latestMouseY = _targetMouseY;
    }

private:
    bool  _enabled = false;
    float _latestMouseX;
    float _latestMouseY;
    float _targetMouseX;
    float _targetMouseY;
    float _latestTimeMs;
    vec3  _eyePosition;
    vec3  _target;
    vec3  _up;
    mat4  _rotation;

    static constexpr float _kDampHalfTimeMs = 50.0f;

    bool ShouldListen()
    {
        return
            MouseInputAgent::_manager->IsMouseButtonPressed(middle) &&
            !MouseInputAgent::_manager->IsKeyPressed(left_shift);
    }

    void RotateCamera(float dx, float dy)
    {
        mat4 rotZ = glm::rotate(mat4(1.0f), -2.f * pi<float>() * dx, _up);
        _eyePosition = rotZ * vec4(_eyePosition, 1.0);

        vec3 right = cross(_up, _target - _eyePosition);
        right = normalize(right);

        mat4 rotX = glm::rotate(mat4(1.0f), -pi<float>() * dy, right);
        vec3 newEyePos = rotX * vec4(_eyePosition, 1.0);

        // ugly: limit the rotation so that eyePos-eyeTarget is never
        // parallel to world Z.
        if (abs(dot(normalize(newEyePos), _up)) <= 0.98f)_eyePosition = newEyePos;

        _rotation = lookAt(_eyePosition, _target, _up);
    }
};

static vec4 Gradient(float t, vec4 start, vec4 mid, vec4 end)
{
	t = glm::clamp(t, 0.f, 1.f);

	vec3 rgb{};

	if (t < 0.5f)	rgb = t        * mid + (0.5f - t) * start;
	else			rgb = (t-0.5f) * end + (1.0f - t) * mid;
	

	return { rgb*2.f, 1.0f };
}

vector<LineGizmoVertex> Circle(float radius, unsigned int subdv)
{
	float da = pi<float>()*2.0f/subdv;

	vector<LineGizmoVertex> pts(subdv + 1);

	for (unsigned int i = 0; i <= subdv; i++)
	pts[i].Position(vec3
		{
			cos(i * da),
			sin(i * da),
			0.0f
		} *radius)
	.Color(glm::vec4{ 1.0f });

	return pts;
}

struct MyGizmoLayers
{
	RenderLayer opaqueLayer;
	RenderLayer transparentLayer;
};

struct MyGizmoLayersVC
{
	RenderLayer depthPrePassLayer;
	RenderLayer opaqueLayer;
	RenderLayer blendEqualLayer;
	RenderLayer blendGreaterLayer;
};


void CreateDashedPatternVC(unsigned int patternLen, unsigned int patternWidth, tao_gizmos_procedural::TextureDataRgbaUnsignedByte& tex)
{

    tex.FillWithFunction([patternLen, patternWidth](unsigned int x, unsigned int y)
     {
         vec2 sampl = vec2{ x, y }+vec2{0.5f};

         float d = patternLen / 4.0f;

         vec2 dashStart = vec2{ d , patternWidth / 2.0f };
         vec2 dashEnd = vec2{ patternLen - d , patternWidth / 2.0f };

         float val =
                 SdfSegment<float>{ dashStart, dashEnd }
                 .Inflate(patternWidth*0.3f)
                 .Evaluate(sampl);

         val = glm::clamp(val, 0.0f, 1.0f);

         return vec<4, unsigned char>{ 255, 255, 255, (1.0f - val) * 255 };
     });

}

void InitViewCube(tao_gizmos::GizmosRenderer& gizRdr, const MyGizmoLayersVC& layers)
{
	/// Settings
	///////////////////////////////////////////////////////////////////////
	float zoomInvariantScale = 0.08f;
	vec4 cubeColor = vec4{ 0.8, 0.8, 0.8, 0.0f };
	vec4 edgeColor = vec4{ 0.8, 0.8, 0.8, 1.0f };

	/// 3D Mesh
	///////////////////////////////////////////////////////////////////////
	auto box = tao_geometry::Mesh::Box(1, 1, 1);
	auto pos = box.GetPositions();
	auto nrm = box.GetNormals();
	auto tri = box.GetIndices();

	for (auto& p : pos) p -= vec3{ 0.5, 0.5, 0.5 }; 

	vector<MeshGizmoVertex> vertices{pos.size()};

	for(int i=0;i<vertices.size();i++)
	{
		vertices[i] = MeshGizmoVertex{}
		.Position(pos[i])
		.Normal(nrm[i])
		.Color(cubeColor);
	}

	auto key = gizRdr.CreateMeshGizmo(mesh_gizmo_descriptor
		{
				.vertices = vertices,
				.triangles = &tri,
				.zoom_invariant = false,
				.zoom_invariant_scale = zoomInvariantScale
		});

	gizRdr.InstanceMeshGizmo(key,
	{
			gizmo_instance_descriptor{glm::scale(glm::mat4{1.0f}, vec3{0.98f}) , glm::vec4{1.0f}}
		});

	gizRdr.AssignGizmoToLayers(key, { layers.blendEqualLayer, layers.depthPrePassLayer });

	/// Edges
	///////////////////////////////////////////////////////////////////////
	vector<vec3> edgePos{ 8 };
	edgePos[0] = vec3{ -0.5f,  -0.5f, -0.5f };
	edgePos[1] = vec3{  0.5f, -0.5f, -0.5f };
	edgePos[2] = vec3{  0.5f,  0.5f, -0.5f };
	edgePos[3] = vec3{  -0.5f, 0.5f, -0.5f };
	for (int i = 0; i < 4; i++) edgePos[i + 4] = edgePos[i] + vec3{ 0.0f, 0.0f, 1.0f };

	vector<LineGizmoVertex> edgeVertices;
	for (int i = 0; i < 4; i++)
	{
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[(i + 0)%4]).Color(edgeColor));
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[(i + 1)%4]).Color(edgeColor));
	}
	for (int i = 0; i < 4; i++)
	{
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[4+((i + 0) % 4)]).Color(edgeColor));
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[4+((i + 1) % 4)]).Color(edgeColor));
	}
	for (int i = 0; i < 4; i++)
	{
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[i + 0]).Color(edgeColor));
		edgeVertices.push_back(LineGizmoVertex{}.Position(edgePos[i + 4]).Color(edgeColor));
	}

	unsigned int edgeWidth = 3;
	auto edgesKey = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor
	{
		.vertices = edgeVertices,
		.line_size = edgeWidth,
		.zoom_invariant = false,
		.zoom_invariant_scale = zoomInvariantScale ,
		.pattern_texture_descriptor = nullptr
	});

	tao_gizmos_procedural::TextureDataRgbaUnsignedByte dashTex{ 16, 2, {0} };
	CreateDashedPatternVC(16, 2, dashTex);
	pattern_texture_descriptor dashPattern
	{
		.data = dashTex.DataPtr(),
		.data_format = tex_for_rgba,
		.data_type = tex_typ_unsigned_byte,
		.width = 16,
		.height = 2,
		.pattern_length = 16
	};

	auto dashedEdgesKey = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor
		{
			.vertices = edgeVertices,
			.line_size = 2,
			.zoom_invariant = false,
			.zoom_invariant_scale = zoomInvariantScale ,
			.pattern_texture_descriptor = &dashPattern
		});

	gizRdr.InstanceLineGizmo(edgesKey,
		{
			gizmo_instance_descriptor{glm::mat4{1.0f}, vec4{1.0f}}
		});
	gizRdr.InstanceLineGizmo(dashedEdgesKey,
		{
			gizmo_instance_descriptor{glm::mat4{1.0f}, vec4{0.6f}}
		});

	gizRdr.AssignGizmoToLayers(edgesKey, { layers.depthPrePassLayer, layers.opaqueLayer });
	gizRdr.AssignGizmoToLayers(dashedEdgesKey, { layers.blendGreaterLayer });
}

MyGizmoLayersVC InitGizmosLayersAndPassesVC(GizmosRenderer& gr)
{
	MyGizmoLayersVC myLayers
	{
		.depthPrePassLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = true,
				.depth_func = depth_func_less_equal,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			ogl_blend_state
			{
				.blend_enable = false,
				.color_mask = mask_none
			},
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			}
		) ,
		.opaqueLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = true,
				.depth_func = depth_func_less_equal,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			no_blend,
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = true
			}
		) ,
		.blendEqualLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = false,
				.depth_func = depth_func_equal,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			alpha_interpolate,
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			}
		) ,
		.blendGreaterLayer = gr.CreateRenderLayer(
		ogl_depth_state
			{
				.depth_test_enable = true,
				.depth_write_enable = false,
				.depth_func = depth_func_greater,
				.depth_range_near = 0.0,
				.depth_range_far = 1.0
			},
			alpha_interpolate,
			ogl_rasterizer_state
			{
				.culling_enable = false,
				.multisample_enable = true,
				.alpha_to_coverage_enable = false
			}
		)
	};

	gr.AddRenderPass
    ({
        gr.CreateRenderPass({ myLayers.depthPrePassLayer }),
        gr.CreateRenderPass({ myLayers.blendGreaterLayer}),
        gr.CreateRenderPass({ myLayers.opaqueLayer }),
        gr.CreateRenderPass({ myLayers.blendEqualLayer })
    });

	return myLayers;

}

glm::vec2 ClosestPointOnLine(const glm::vec2& pt, const glm::vec2& l0, const glm::vec2& l1)
{
    vec2 l01Nrm = glm::normalize(l1-l0);
    return  l0+dot(pt-l0, l01Nrm)*l01Nrm;
}

// from: https://paulbourke.net/geometry/pointlineplane/
glm::vec3 RayClosestPointOnLine(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& lp, const glm::vec3& ld)
{
    // Algorithm is ported from the C algorithm of
    // Paul Bourke at http://local.wasp.uwa.edu.au/~pbourke/geometry/lineline3d/

    const float kE = 1e-5;

    vec3 p1 = ro;
    vec3 p2 = ro+rd;
    vec3 p3 = lp;
    vec3 p4 = lp+ld;
    vec3 p13 = p1 - p3;
    vec3 p43 = p4 - p3;

    if (dot(p43, p43) < kE) {
        return ro;
    }
    vec3 p21 = p2 - p1;
    if (dot(p21, p21) < kE) {
        return ro;
    }

    float d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
    float d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
    float d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
    float d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
    float d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

    float denom = d2121 * d4343 - d4321 * d4321;
    if (abs(denom) < kE) {
        return ro;
    }
    float numer = d1343 * d4321 - d1321 * d4343;

    float mua = numer / denom;
    float mub = (d1343 + d4321 * (mua)) / d4343;

    return lp+mub*ld;
}

class GizmoPickAgent : public MouseInputAgent
{
public:
    GizmoPickAgent(GizmosRenderer* gizmosRenderer):
            MouseInputAgent{MouseInputAgentTag::None},
            _gr{gizmosRenderer},
            _clientSelectionCallbacks{},
            _clientHoverCallbacks{},
            _clientMouseMoveCallbacks{},
            _clientMouseUpCallbacks{},
            _clientMouseDownCallbacks{}
    {
        _selectionCallback = bind(&GizmoPickAgent::OnSelection, this, placeholders::_1);
        _hoverCallback     = bind(&GizmoPickAgent::OnHover, this, placeholders::_1);
    }

    void MouseUp(float x, float y, int w, int h) override
    {
        if(!_dragging) return;

        for(const auto* c : _clientMouseUpCallbacks )
        {
            if (c) (*c)(x, y, w, h);
        }
    }
    void MouseDown(float x, float y, int w, int h) override
    {
        if(!IsClickButtonDown()) return;

        _dragging = true;

        _gr->GetGizmoUnderCursor(x,y,_selectionCallback);

        for(const auto* c : _clientMouseDownCallbacks )
        {
            if (c) (*c)(x, y, w, h);
        }
    }
    void MouseMove(float x, float y, int w, int h) override
    {
        if(!IsAnyMouseButtonDown())
            _gr->GetGizmoUnderCursor(x,y,_hoverCallback);

        if(!_dragging) return;

        for(const auto* c : _clientMouseMoveCallbacks )
        {
            if (c) (*c)(x, y, w, h);
        }
    }

    void Mute() override{}
    void UnMute() override{}

    void SubscribeToMouseMove(const function<void(float, float, int, int)>* onMouseMove)
    {
        _clientMouseMoveCallbacks.push_back(onMouseMove);
    }

    void SubscribeToMouseDown(const function<void(float, float, int, int)>* onMouseDown)
    {
        _clientMouseDownCallbacks.push_back(onMouseDown);
    }

    void SubscribeToMouseUp(const function<void(float, float, int, int)>* onMouseUp)
    {
        _clientMouseUpCallbacks.push_back(onMouseUp);
    }

    void SubscribeToSelection(const function<void(optional<gizmo_instance_id>)>* onSelection)
    {
        _clientSelectionCallbacks.push_back(onSelection);
    }

    void SubscribeToHover(const function<void(optional<gizmo_instance_id>)>* onHover)
    {
        _clientHoverCallbacks.push_back(onHover);
    }

    /*void Lock_areYouSureAboutThis(const function<void(optional<gizmo_instance_id>)>* mh)
    {
        _clientExclusiveSelectionCallback = mh;
    }

    void Unlock_areYouSureAboutThis()
    {
        _clientExclusiveSelectionCallback = nullptr;
    }*/

    void MuteCameraInput()
    {
        _manager->MuteAgents(static_cast<MouseInputAgentTag>(CameraRotate|CameraZoom));
    }
    void UnMuteCameraInput()
    {
        _manager->UnMuteAgents(static_cast<MouseInputAgentTag>(CameraRotate|CameraZoom));
    }

private:
    GizmosRenderer* _gr = nullptr;
    //const function<void(optional<gizmo_instance_id>)>* _clientExclusiveSelectionCallback = nullptr;
    vector<const function<void(optional<gizmo_instance_id>)>*> _clientSelectionCallbacks;
    vector<const function<void(optional<gizmo_instance_id>)>*> _clientHoverCallbacks;
    vector<const function<void(float, float, int, int)>*> _clientMouseMoveCallbacks;
    vector<const function<void(float, float, int, int)>*> _clientMouseDownCallbacks;
    vector<const function<void(float, float, int, int)>*> _clientMouseUpCallbacks;
    function<void(optional<gizmo_instance_id>)>  _selectionCallback;
    function<void(optional<gizmo_instance_id>)>  _hoverCallback;

    bool _dragging = false;

    bool IsClickButtonDown() const
    {
        return MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::left);
    }

    bool IsAnyMouseButtonDown() const
    {
        return
            MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::left) ||
            MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::middle) ||
            MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::right);
    }


    void OnSelection(optional<gizmo_instance_id> selectedItem)
    {
        //if(_clientExclusiveSelectionCallback)
        //{
        //    (*_clientExclusiveSelectionCallback)(selectedItem);
        //    return;
        //}

        for(const auto* c : _clientSelectionCallbacks )
        {
            if (c) (*c)(selectedItem);
        }
    }

    void OnHover(optional<gizmo_instance_id> selectedItem)
    {
        for(const auto* c : _clientHoverCallbacks )
        {
            if (c) (*c)(selectedItem);
        }
    }
};


class TransformManipulator
{
public:
    TransformManipulator(GizmosRenderer& gr, GizmoPickAgent& inputAgent):
            _gr{&gr},
            _selectionCallback{bind(&TransformManipulator::OnSelection, this, placeholders::_1)},
            _hoverCallback{bind(&TransformManipulator::OnHover, this, placeholders::_1)},
            _mouseMoveCallback{bind(&TransformManipulator::OnMouseMove, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4)},
            _mouseUpCallback{bind(&TransformManipulator::OnMouseUp, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4)},
            _mouseDownCallback{bind(&TransformManipulator::OnMouseDown, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4)},
            _transformationChanged{nullptr},
            _enabled{false},
            _inputAgent{inputAgent}
    {

        _inputAgent.SubscribeToMouseMove(&_mouseMoveCallback);
        _inputAgent.SubscribeToMouseDown(&_mouseDownCallback);
        _inputAgent.SubscribeToMouseUp(&_mouseUpCallback);
        _inputAgent.SubscribeToSelection(&_selectionCallback);
        _inputAgent.SubscribeToHover(&_hoverCallback);

        auto renderPasses = CreateRenderLayersAndPasses();

        _tmTranslate = TmTranslate  {*_gr, renderPasses.opaqueLayer, renderPasses.blendLayer};
        _tmRotate    = TmRotate     {*_gr, renderPasses.opaqueLayer, renderPasses.blendLayer};
        _tmScale     = TmScale      {*_gr, renderPasses.opaqueLayer, renderPasses.blendLayer};

        _transformManipulators.insert(make_pair(TmMode::Translate, &_tmTranslate));
        _transformManipulators.insert(make_pair(TmMode::Rotate   , &_tmRotate));
        _transformManipulators.insert(make_pair(TmMode::Scale    , &_tmScale));
    }

    enum class TmMode
    {
        Translate,
        Rotate,
        Scale
    };

    void Enable(const mat4& initialTransformation, const function<void(const mat4&)>* transformChangedCallback)
    {
        _currentTransformation = initialTransformation;
        _transformationChanged = transformChangedCallback;
        _enabled = true;

        _transformManipulators[_currentMode]->Enable(initialTransformation);
    }

    void Disable()
    {
        _currentTransformation = nullopt;
        _transformationChanged = nullptr;
        _enabled = false;

        _transformManipulators[_currentMode]->Disable();
    }

    void SetMode(TmMode mode)
    {
        if(_enabled)
        {
            auto* currCallback = _transformationChanged;
            mat4 currTransform = _currentTransformation.value();

            if(_selectedItem.has_value())
                OnSelection(nullopt);

            if(_hoveredItem.has_value())
                OnHover(nullopt);

            Disable();
            _currentMode = mode;
            Enable(currTransform, currCallback);
        }
        else
            _currentMode = mode;
    }
    TmMode GetMode() const
    {
        return _currentMode;
    }

private:
    class Tm
    {
    public:
        bool IsEnabled() const {return _enabled;}
        virtual bool HasGizmo(gizmo_instance_id id) const = 0;
        virtual void HoverEnter(gizmo_instance_id hoveredItem) = 0 ;
        virtual void HoverExit(gizmo_instance_id hoveredItem) = 0 ;
        virtual void Enable(const mat4& transform){_enabled = true; _transformation = transform;}
        virtual void Disable(){_enabled = false;}
        virtual void DragEnter(gizmo_instance_id selectedItem) = 0 ;
        virtual void DragExit() = 0 ;
        virtual mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) = 0 ;
    protected:
        bool _enabled = false;
        mat4 _transformation = mat4{1.0f};

    };

    class TmTranslate : public Tm
    {

    public:
        TmTranslate(){} // Ugly...however I need a parameterless constructor
        TmTranslate(GizmosRenderer& gr,
                    RenderLayer& depthPrePassLayer, // To achieve a "ghost" entity effect, we need a depth pre-pass
                    RenderLayer& colorBlendLayer)   // and a depth less-equal color pass with blending enabled.
        {

            /// Geometry creation
            /////////////////////////////////////////////////////
            vector<MeshGizmoVertex> arrowMeshVertices{};
            vector<int> arrowMeshTriangles{};
            CreateArrowMesh(arrowMeshVertices, arrowMeshTriangles);

            /// Instantiating the gizmo
            ///////////////////////////////////////////////////////
            _gr = &gr;
            _gizmoId = _gr->CreateMeshGizmo(mesh_gizmo_descriptor
             {
                 .vertices = arrowMeshVertices,
                 .triangles = &arrowMeshTriangles,
                 .zoom_invariant = true,
                 .zoom_invariant_scale = 0.1f
             });

            mat4
            transformX = rotate(mat4{1.0}, 0.5f*pi<float>(), vec3{0.0, 1.0, 0.0}) ,
            transformY = rotate(mat4{1.0},-0.5f*pi<float>(), vec3{1.0, 0.0, 0.0}) ,
            transformZ = rotate(mat4{1.0}, 0.0f*pi<float>(), vec3{1.0, 0.0, 0.0}) ;

            vector<gizmo_instance_descriptor> instances =
            {
                    /* X Axis */ gizmo_instance_descriptor{ .transform = transformX, .color = kAxisColorX, .visible = false, .selectable = false},
                    /* Y Axis */ gizmo_instance_descriptor{ .transform = transformY, .color = kAxisColorY, .visible = false, .selectable = false},
                    /* Z Axis */ gizmo_instance_descriptor{ .transform = transformZ, .color = kAxisColorZ, .visible = false, .selectable = false}
            };

            auto allKeys = _gr->InstanceMeshGizmo(_gizmoId, instances);

            _axisData.insert(make_pair(
                TmAxis::AxisX,  TmAxisData
                            {
                                .id             = allKeys[0],
                                .transform      = transformX,
                                .color          = kAxisColorX,
                                .defaultColor   = kAxisColorX,
                                .highlightColor = kAxisColorHighlightX
                            }));

            _axisData.insert(make_pair(
                    TmAxis::AxisY,  TmAxisData
                            {
                                .id             = allKeys[1],
                                .transform      = transformY,
                                .color          = kAxisColorY,
                                .defaultColor   = kAxisColorY,
                                .highlightColor = kAxisColorHighlightY
                            }));

            _axisData.insert(make_pair(
                    TmAxis::AxisZ,  TmAxisData
                            {
                                .id             = allKeys[2],
                                .transform      = transformZ,
                                .color          = kAxisColorZ,
                                .defaultColor   = kAxisColorZ,
                                .highlightColor = kAxisColorHighlightZ
                            }));

            _gr->AssignGizmoToLayers(_gizmoId, {depthPrePassLayer, colorBlendLayer});

            // init as non-visible and non-selectable
            SetAxisProperties(AxisX, nullopt, nullopt, false, false);
            SetAxisProperties(AxisY, nullopt, nullopt, false, false);
            SetAxisProperties(AxisZ, nullopt, nullopt, false, false);
        }

        bool HasGizmo(gizmo_instance_id id) const override
        {
            for(const auto& a : _axisData)
            {
                if(a.second.id == id)
                    return true;
            }
            return false;
        }

        virtual void HoverEnter(gizmo_instance_id id) override
        {
            auto hoveredAxis = GetAxisFromId(id);

            SetAxisProperties(hoveredAxis, nullopt, _axisData[hoveredAxis].highlightColor, true, true);

        }
        virtual void HoverExit(gizmo_instance_id id) override
        {
            auto hoveredAxis = GetAxisFromId(id);

            vector<MeshGizmoVertex> newVerts;
            vector<int> newTris;
            CreateArrowMesh(newVerts, newTris);
            _gr->SetMeshGizmoVertices(_gizmoId, newVerts, &newTris);

            SetAxisProperties(hoveredAxis, nullopt, _axisData[hoveredAxis].defaultColor, true, true);
        }

        virtual void Enable(const mat4& transformation) override
        {
            Tm::Enable(transformation);

            SetAxisProperties(AxisX, nullopt, nullopt, true, true);
            SetAxisProperties(AxisY, nullopt, nullopt, true, true);
            SetAxisProperties(AxisZ, nullopt, nullopt, true, true);
        }
        virtual void Disable() override
        {
            Tm::Disable();
            SetAxisProperties(AxisX, nullopt, nullopt, false, false);
            SetAxisProperties(AxisY, nullopt, nullopt, false, false);
            SetAxisProperties(AxisZ, nullopt, nullopt, false, false);
        }
        virtual void DragEnter(gizmo_instance_id id) override
        {

            _selectedAxis = GetAxisFromId(id);

            // Hide the non-selected axis
            SetAxisProperties(AxisX, nullopt, nullopt, _selectedAxis.value() == AxisX, false);
            SetAxisProperties(AxisY, nullopt, nullopt, _selectedAxis.value() == AxisY, false);
            SetAxisProperties(AxisZ, nullopt, nullopt, _selectedAxis.value() == AxisZ, false);

        }
        virtual void DragExit() override
        {
            _selectedAxis = nullopt;

            SetAxisProperties(AxisX, nullopt, nullopt, true, true);
            SetAxisProperties(AxisY, nullopt, nullopt, true, true);
            SetAxisProperties(AxisZ, nullopt, nullopt, true, true);
        }
        virtual mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) override
        {
            vec3 localAxis;

            switch(_selectedAxis.value())
            {
                case(AxisX): localAxis = vec3{1.0, 0.0, 0.0}; break;
                case(AxisY): localAxis = vec3{0.0, 1.0, 0.0}; break;
                case(AxisZ): localAxis = vec3{0.0, 0.0, 1.0}; break;
            }

            vec3 transl = localAxis * TranslateOnAxis(x,y,prevX, prevY, w, h, localAxis);
            mat4 t = translate(mat4{1.0f}, transl);

            Tm::_transformation*=t;
            SetAxisProperties(_selectedAxis.value(), nullopt, nullopt, true, true);
            return t;
        }

    private:
        static constexpr float
                kCylRadius  = 0.04f,
                kCylLength  = 0.6f,
                kConeRadius = 0.12f,
                kConeLength = 0.4f;
        static constexpr int
                kArrowSubdv = 16;
        static constexpr vec4
                kAxisColorX = vec4{0.85, 0.1, 0.1, 1.0},
                kAxisColorY = vec4{0.1, 0.85, 0.1, 1.0},
                kAxisColorZ = vec4{0.1, 0.1, 0.85, 1.0},
                kAxisColorHighlightX = vec4{0.9, 0.3, 0.3, 1.0},
                kAxisColorHighlightY = vec4{0.3, 0.9, 0.3, 1.0},
                kAxisColorHighlightZ = vec4{0.3, 0.3, 0.9, 1.0};

        enum TmAxis
        {
            AxisX,
            AxisY,
            AxisZ
        };
        struct TmAxisData
        {
            gizmo_instance_id id;
            mat4 transform;
            vec4 color;
            vec4 defaultColor;
            vec4 highlightColor;
        };

        GizmosRenderer* _gr = nullptr;
        gizmo_id _gizmoId;
        map<TmAxis, TmAxisData> _axisData;
        optional<TmAxis> _selectedAxis = nullopt;

        void SetAxisProperties(TmAxis axis,  optional<mat4> transform, optional<vec4> color, bool visible, bool selectable)
        {
            TmAxisData& axisData = _axisData[axis];

            axisData.color = color.has_value()?color.value() : axisData.color;
            axisData.transform = transform.has_value()? transform.value() : axisData.transform;

            _gr->SetGizmoInstances(_gizmoId,
               {make_pair(axisData.id,
                   gizmo_instance_descriptor
                   {
                       .transform = Tm::_transformation * axisData.transform,
                       .color = axisData.color,
                       .visible = visible,
                       .selectable = selectable,
                   })
               });
        }

        float TranslateOnAxis(float x, float y, float prevX, float prevY, int w, int h, vec3 localAxis)
        {
            mat4 viewMatrix;
            mat4 projMatrix;
            vec2 nearFar;
            _gr->GetView(viewMatrix, projMatrix, nearFar);

            glm::mat4 viewProjMatrix = projMatrix * viewMatrix;

            // Find the world position of the
            // triad origin and axis
            vec3 worldAxis   = _transformation*vec4{localAxis, 0.0f};
            vec3 worldOrigin = _transformation*vec4{0.0f, 0.0f, 0.0f, 1.0f};


            // find the 2D closest point from the mouse
            // position to the projected axis
            vec4 l0 = viewProjMatrix * vec4{worldOrigin, 1.0};
            vec4 l1 = viewProjMatrix * vec4{worldOrigin + worldAxis, 1.0};
            vec2 l02d = l0;
            l02d /= l0.w;
            vec2 l12d = l1;
            l12d /= l1.w;

            vec2 closestPoint2D0 = ClosestPointOnLine(vec2((prevX / w) * 2.0 - 1.0, (prevY / h) * 2.0 - 1.0), l02d, l12d);
            vec2 closestPoint2D1 = ClosestPointOnLine(vec2((x / w) * 2.0 - 1.0, (y / h) * 2.0 - 1.0), l02d, l12d);


            // find the world position of the 2D point computed above
            // by projecting onto the 3D axis
            vec4 ndc0 = vec4{closestPoint2D0, 0.0, 1.0f};
            vec4 ndc1 = vec4{closestPoint2D1, 0.0, 1.0f};
            mat4 viewProjInverse = glm::inverse(viewProjMatrix);
            vec4 pw0 = viewProjInverse * ndc0;
            vec4 pw1 = viewProjInverse * ndc1;
            vec4 ow = glm::inverse(viewMatrix) * vec4{0.0, 0.0, 0.0, 1.0};

            vec3 rayDir0 = normalize(pw0 / pw0.w - ow);
            vec3 intersection0 = RayClosestPointOnLine(ow, rayDir0, worldOrigin, worldAxis);

            vec3 rayDir1 = normalize(pw1 / pw1.w - ow);
            vec3 intersection1 = RayClosestPointOnLine(ow, rayDir1, worldOrigin, worldAxis);

            float translation = dot(worldAxis, intersection1) - dot(worldAxis, intersection0);

            return translation;
        }

        TmAxis GetAxisFromId(gizmo_instance_id id)
        {
            for(const auto& a : _axisData)
            {
                if(a.second.id == id)
                    return a.first;
            }

            throw runtime_error("Invalid gizmo instance id. Call Tm::HasGizmo before calling this method.");
        }

        void CreateArrowMesh(vector<MeshGizmoVertex>& verts, vector<int>& tris)
        {
            auto arrowMesh = tao_geometry::Mesh::Arrow(kCylRadius, kCylLength, kConeRadius, kConeLength, kArrowSubdv);


            vector<vec3> arrowMeshPosition  = arrowMesh.GetPositions();
            vector<vec3> arrowMeshNormals   = arrowMesh.GetNormals();

            tris = arrowMesh.GetIndices();
            verts = vector<MeshGizmoVertex>(arrowMeshPosition.size());
            for(int i=0;i<verts.size(); i++)
            {
                verts[i]
                        .Position(arrowMeshPosition[i] + vec3(0.0, 0.0, 0.3))
                        .Normal(arrowMeshNormals[i])
                        .Color({1.0, 1.0, 1.0, 1.0});
            }
        }

    };

    class TmRotate : public Tm
    {

    public:
        TmRotate(){} // Ugly...however I need a parameterless constructor
        TmRotate(GizmosRenderer& gr, RenderLayer& opaqueLayer, RenderLayer& transparentLayer)
        {
            /// Constants
            ///////////////////////////////////////////////////
            static constexpr float
                    kRadius  = 1.00f;
            static constexpr int
                    kCircPtsCount = 30,
                    kLineSize     = 3,
                    kLineSizeForSelection = 12;
            static constexpr vec4
                    kAxisColorX = vec4{0.75, 0.1, 0.1, 1.0},
                    kAxisColorY = vec4{0.1, 0.75, 0.1, 1.0},
                    kAxisColorZ = vec4{0.1, 0.1, 0.75, 1.0};

            /// Geometry creation
            /////////////////////////////////////////////////////
            vector<LineGizmoVertex> circPts(kCircPtsCount);
            const float arc = 2.0f*pi<float>();
            const float da = arc/(kCircPtsCount-1);
            for(int i=0;i<kCircPtsCount; i++)
            {
                circPts[i] = LineGizmoVertex()
                        .Position(vec3{cos(i*da), sin(i*da), 0.0f})
                        .Color(vec4{1.0f});
            }

            /// Instantiating the gizmo
            ///////////////////////////////////////////////////////
            _gr = &gr;

            _gizmoId = _gr->CreateLineStripGizmo(line_strip_gizmo_descriptor
            {
                .vertices = circPts,
                .isLoop   = true,
                .line_size = kLineSize,
                .zoom_invariant = true,
                .zoom_invariant_scale = 0.1f,
                .pattern_texture_descriptor = nullptr,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
            });

            // Create a bigger gizmo that will be used for selection
            _gizmoIdForSelection = _gr->CreateLineStripGizmo(line_strip_gizmo_descriptor
             {
                 .vertices = circPts,
                 .isLoop   = true,
                 .line_size = kLineSizeForSelection,
                 .zoom_invariant = true,
                 .zoom_invariant_scale = 0.1f,
                 .pattern_texture_descriptor = nullptr,
                 .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
             });

            mat4
                    transformX = rotate(mat4{1.0}, 0.5f*pi<float>(), vec3{0.0, 1.0, 0.0}) * scale(mat4{1.0f}, vec3{1.00f}),
                    transformY = rotate(mat4{1.0},-0.5f*pi<float>(), vec3{1.0, 0.0, 0.0}) * scale(mat4{1.0f}, vec3{0.93f}),
                    transformZ = rotate(mat4{1.0}, 0.0f*pi<float>(), vec3{1.0, 0.0, 0.0}) * scale(mat4{1.0f}, vec3{0.84f});

            vector<gizmo_instance_descriptor> instances =
                    {
                            /* X Axis */ gizmo_instance_descriptor{ .transform = transformX, .color = kAxisColorX, .visible = false, .selectable = false},
                            /* Y Axis */ gizmo_instance_descriptor{ .transform = transformY, .color = kAxisColorY, .visible = false, .selectable = false},
                            /* Z Axis */ gizmo_instance_descriptor{ .transform = transformZ, .color = kAxisColorZ, .visible = false, .selectable = false}
                    };

            auto allKeys             = _gr->InstanceLineStripGizmo(_gizmoId             , instances);
            auto allKeysForSelection = _gr->InstanceLineStripGizmo(_gizmoIdForSelection , instances);

            _axisData.insert(make_pair(
                    TmAxis::AxisX,  TmAxisData
                            {
                                    .id             = allKeys[0],
                                    .idForSelection = allKeysForSelection[0],
                                    .transform      = transformX,
                                    .color          = kAxisColorX,
                                    .defaultColor   = kAxisColorX
                            }));

            _axisData.insert(make_pair(
                    TmAxis::AxisY,  TmAxisData
                            {
                                    .id             = allKeys[1],
                                    .idForSelection = allKeysForSelection[1],
                                    .transform      = transformY,
                                    .color          = kAxisColorY,
                                    .defaultColor   = kAxisColorY
                            }));

            _axisData.insert(make_pair(
                    TmAxis::AxisZ,  TmAxisData
                            {
                                    .id             = allKeys[2],
                                    .idForSelection = allKeysForSelection[2],
                                    .transform      = transformZ,
                                    .color          = kAxisColorZ,
                                    .defaultColor   = kAxisColorZ
                            }));

            _gr->AssignGizmoToLayers(_gizmoId, {opaqueLayer});
            _gr->AssignGizmoToLayers(_gizmoIdForSelection, {transparentLayer});

            SetInstancesProperties(false, false); // init as non-visible and non-selectable

            /// Mesh and Line gizmo for mouse interaction (see OnDrag())
            ////////////////////////////////////////////////////////////
            vector<MeshGizmoVertex> immediateMeshVerts
            {
                    MeshGizmoVertex{}.Position(vec3{0.0f}).Color(vec4{1.0f}),
                    MeshGizmoVertex{}.Position(vec3{0.0f}).Color(vec4{1.0f}),
            };
            _immediateMeshGizmoId = _gr->CreateMeshGizmo(mesh_gizmo_descriptor
            {
                    .vertices = immediateMeshVerts,
                    .zoom_invariant = true,
                    .zoom_invariant_scale = 0.1f,
                    .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
            });
            _immediateMeshGizmoInstanceId = _gr->InstanceMeshGizmo(_immediateMeshGizmoId,
           {
               gizmo_instance_descriptor
               {
                       .transform = mat4{1.0f}, // world space coords
                       .color = vec4{1.0f},
                       .visible = true,
                       .selectable = false
               }
           })[0];

            vector<LineGizmoVertex> immediateLineVerts
            {
                    LineGizmoVertex{}.Position(vec3{0.0f}).Color(vec4{1.0f}),
                    LineGizmoVertex{}.Position(vec3{0.0f}).Color(vec4{1.0f}),
            };

            _immediateLineGizmoId = _gr->CreateLineGizmo(line_list_gizmo_descriptor
             {
                 .vertices = immediateLineVerts,
                 .line_size = 2,
                 .zoom_invariant = true,
                 .zoom_invariant_scale = 0.1f,
                 .pattern_texture_descriptor = nullptr,
                 .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
             });
            _immediateLineGizmoInstanceId = _gr->InstanceLineGizmo(_immediateLineGizmoId,
               {
                   gizmo_instance_descriptor
                   {
                           .transform = mat4{1.0f}, // world space coords
                           .color = vec4{1.0f},
                           .visible = true,
                           .selectable = false
                   }
               })[0];

            _gr->AssignGizmoToLayers(_immediateMeshGizmoId, {transparentLayer});
            _gr->AssignGizmoToLayers(_immediateLineGizmoId, {transparentLayer});
        }

        bool _firstTouch = true;
        float _firstTouchAngle = 0.0f;
        float _cumulatedAngle = 0.0f;
        gizmo_id _immediateMeshGizmoId;
        gizmo_instance_id _immediateMeshGizmoInstanceId;
        gizmo_id _immediateLineGizmoId;
        gizmo_instance_id _immediateLineGizmoInstanceId;

        void HideImmediateArc()
        {
            _gr->SetGizmoInstances(_immediateMeshGizmoId,{
            make_pair(_immediateMeshGizmoInstanceId,
              gizmo_instance_descriptor
              {
                  .transform{mat4{1.0f}},
                  .color{vec4{1.0f}},
                  .visible = false,
                  .selectable = false
              }
            )});

            _gr->SetGizmoInstances(_immediateLineGizmoId,{
            make_pair(_immediateLineGizmoInstanceId,
              gizmo_instance_descriptor
              {
                      .transform{mat4{1.0f}},
                      .color{vec4{1.0f}},
                      .visible = false,
                      .selectable = false
              }
            )});
        }

        void DrawImmediateArc(const mat4& transformation, float radius, float startAngle, float endAngle, const glm::vec4& color, const glm::vec4& lineColor)
        {
            // Allow 3 complete revolutions at max, otherwise the vert count
            // could grow indefinitely. After the first turns, the visual
            // feedback is quite unnoticeable.
            float angle        = endAngle - startAngle;
            float angleClamped = glm::min(abs(angle), 6.0f*pi<float>()) * sign(angle);
            int subdivisions = 16 * (1+static_cast<int>(abs(angleClamped)/(2.0f*pi<float>())));

            vector<MeshGizmoVertex> verts((subdivisions+1) +1/*vertex at center*/);

            /// vertices
            verts[0].Position(vec3{0.0f}).Color(color);

            float da = angleClamped/subdivisions;
            for(int i=0;i<=subdivisions; i++)
            {
                float a = startAngle + i*da;

                verts[i+1]
                    .Position(vec3{radius * cos(a), radius * sin(a), 0.0})
                    .Normal(vec3{0.0f, 0.0f, 1.0f})
                    .Color(vec4{1.0f});
            }

            /// triangles
            vector<int> tris(subdivisions*3);
            for(int i=0;i<subdivisions; i++)
            {
                tris[i*3 + 0] = 0;
                tris[i*3 + 1] = i+2;
                tris[i*3 + 2] = i+1;
            }

            _gr->SetMeshGizmoVertices(_immediateMeshGizmoId, verts, &tris);
            _gr->SetGizmoInstances(_immediateMeshGizmoId,{
                make_pair(_immediateMeshGizmoInstanceId,
                    gizmo_instance_descriptor
                    {
                        .transform{transformation},
                        .color{color},
                        .visible = true,
                        .selectable = false
                    }
                )});

            vector<LineGizmoVertex> lineVerts
            {
                LineGizmoVertex().Position(vec3{radius * cos(startAngle), radius * sin(startAngle), 0.0}).Color(vec4{1.0f}),
                LineGizmoVertex().Position(vec3{0.0f}).Color(vec4{1.0f}),
                LineGizmoVertex().Position(vec3{0.0f}).Color(vec4{1.0f}),
                LineGizmoVertex().Position(vec3{radius * cos(startAngle + angle), radius * sin(startAngle + angle), 0.0}).Color(vec4{1.0f})
            };
            _gr->SetLineGizmoVertices(_immediateLineGizmoId, lineVerts);
            _gr->SetGizmoInstances(_immediateLineGizmoId,{
            make_pair(_immediateLineGizmoInstanceId,
              gizmo_instance_descriptor
                  {
                      .transform{transformation},
                      .color{lineColor},
                      .visible = true,
                      .selectable = false
                  }
            )});
        }

        bool HasGizmo(gizmo_instance_id id) const override
        {
            for(const auto& a : _axisData)
            {
                if(a.second.id == id || a.second.idForSelection == id)
                    return true;
            }
            return false;
        }

        virtual void HoverEnter(gizmo_instance_id id) override
        {
            TmAxis axis = GetAxisFromId(id);

            SetAxisProperties(axis, _axisData[axis].defaultColor * 2.5f);
        }
        virtual void HoverExit(gizmo_instance_id id) override
        {
            TmAxis axis = GetAxisFromId(id);

            SetAxisProperties(axis, nullopt);
        }

        virtual void Enable(const mat4& transformation) override
        {
            Tm::Enable(transformation);
            SetInstancesProperties(true, true);
        }
        virtual void Disable() override
        {
            Tm::Disable();
            SetInstancesProperties(false, false);
        }
        virtual void DragEnter(gizmo_instance_id id) override
        {
            _firstTouch = true;
            _cumulatedAngle = 0.0f;

            const vec4 kGhostColor{0.65f};

            _selectedAxis = GetAxisFromId(id);

            // Hide non selected axis
            SetInstancesProperties(AxisX, _selectedAxis.value() == AxisX, false);
            SetInstancesProperties(AxisY, _selectedAxis.value() == AxisY, false);
            SetInstancesProperties(AxisZ, _selectedAxis.value() == AxisZ, false);

        }
        virtual void DragExit() override
        {
            _selectedAxis = nullopt;

            // Show all the axis again
            SetInstancesProperties(true, true);
            HideImmediateArc();
        }
        virtual mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) override
        {
            // The rotation is computed as the angular difference
            // between the vectors gizmo origin->plane projection of prev mouse position and
            // gizmo origin->plane projection of current mouse position

            // find the ray-plane intersection of current and prev mouse pos
            vec4 localAxis{_selectedAxis.value()==AxisX,_selectedAxis.value()==AxisY,_selectedAxis.value()==AxisZ,0.0f};

            mat4 view, proj;
            vec2 unused;
            _gr->GetView(view, proj, unused);

            mat4 invViewProj = glm::inverse(proj * view);
            vec3 eye = glm::inverse(view) * vec4{0.0f, 0.0f, 0.0f, 1.0f};
            vec4 currMouseUnproj = invViewProj * vec4{(x/w)     * 2.0f-1.0f, (y/h)     * 2.0f-1.0f, -1.0f, 1.0f};
            vec4 prevMouseUnproj = invViewProj * vec4{(prevX/w) * 2.0f-1.0f, (prevY/h) * 2.0f-1.0f, -1.0f, 1.0f};
            currMouseUnproj/=currMouseUnproj.w;
            prevMouseUnproj/=prevMouseUnproj.w;
            vec3 eyeCurrDir = vec3{currMouseUnproj}-eye;
            vec3 eyePrevDir = vec3{prevMouseUnproj}-eye;

            Plane gizPl{Tm::_transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f}, Tm::_transformation * localAxis};
            Ray currRay{eye, eyeCurrDir};
            Ray prevRay{eye, eyePrevDir};

            vec3 currIntersection = RayPlaneIntersection(currRay, gizPl).value();
            vec3 prevIntersection = RayPlaneIntersection(prevRay, gizPl).value();

            // find the angular difference
            vec2 currProj = normalize(gizPl.ProjectPoint(currIntersection));
            vec2 prevProj = normalize(gizPl.ProjectPoint(prevIntersection));

            float da =
                    acos(glm::min(dot(currProj, prevProj), 1.0f)) *            // clamp to avoid NaN
                    sign(cross(vec3{prevProj, 0.0f}, vec3{currProj, 0.0f}).z); // CCW -> positive, CW -> negative

            mat4 t= rotate(mat4{1.0f}, da, vec3{localAxis});

            Tm::_transformation *= t;

            /// DEBUG
            ///////////////////////////////////////
            if(_firstTouch)
            {
                _firstTouchAngle = atan2(currProj.y, currProj.x);
                _firstTouch = false;
            }
            _cumulatedAngle += da;
            vec4 color      = _axisData[_selectedAxis.value()].color;
            vec4 lineColor  = _axisData[_selectedAxis.value()].color;
            color.a     = 0.55f;
            lineColor.a = 0.85f;
            DrawImmediateArc(gizPl.Transformation(), length(_axisData[_selectedAxis.value()].transform[0]), _firstTouchAngle, _firstTouchAngle + _cumulatedAngle, color, lineColor);
            ///////////////////////////////////////

            return t;
        }

    private:
        enum TmAxis
        {
            AxisX,
            AxisY,
            AxisZ
        };
        struct TmAxisData
        {
            gizmo_instance_id id;
            gizmo_instance_id idForSelection;
            mat4 transform;
            vec4 color;
            vec4 defaultColor;
        };

        GizmosRenderer* _gr = nullptr;
        gizmo_id _gizmoId;
        gizmo_id _gizmoIdForSelection;
        map<TmAxis, TmAxisData> _axisData;
        optional<TmAxis> _selectedAxis = nullopt;


        void SetInstancesProperties(TmAxis axis, bool visible, bool selectable)
        {
            _gr->SetGizmoInstances(_gizmoId, {
                make_pair(_axisData[axis].id,
                  gizmo_instance_descriptor
                  {
                      .transform = Tm::_transformation * _axisData[axis].transform,
                      .color = _axisData[axis].color,
                      .visible = visible,
                      .selectable = selectable,
                  })
            });

            _gr->SetGizmoInstances(_gizmoIdForSelection, {
                make_pair(_axisData[axis].idForSelection,
                  gizmo_instance_descriptor
                  {
                      .transform = Tm::_transformation * _axisData[axis].transform,
                      .color = vec4(0.0f), // always invisible, only needed for selection
                      .visible = visible,
                      .selectable = selectable,
                  })
            });
        }

        void SetInstancesProperties(bool visible, bool selectable)
        {
            SetInstancesProperties(AxisX, visible, selectable);
            SetInstancesProperties(AxisY, visible, selectable);
            SetInstancesProperties(AxisZ, visible, selectable);
        }

        void SetAxisProperties(TmAxis axis, optional<vec4> color)
        {
            TmAxisData& axisData = _axisData[axis];

            axisData.color = color.has_value() ? color.value() : axisData.defaultColor;

            _gr->SetGizmoInstances(_gizmoId,
               {make_pair(axisData.id,
                  gizmo_instance_descriptor
                  {
                          .transform = Tm::_transformation * axisData.transform,
                          .color = axisData.color,
                          .visible = true,
                          .selectable = true,
                  })
               });
        }


        TmAxis GetAxisFromId(gizmo_instance_id id)
        {
            for(const auto& a : _axisData)
            {
                if(a.second.id == id || a.second.idForSelection == id)
                    return a.first;
            }

            throw runtime_error("Invalid gizmo instance id. Call Tm::HasGizmo before calling this method.");
        }

    };

    class TmScale : public Tm
    {

    public:
        TmScale(){} // Ugly...however I need a parameterless constructor
        TmScale(GizmosRenderer& gr,
                    RenderLayer& opaqueLayer,
                    RenderLayer& blendLayer)
        {
            /// Geometry creation
            //////////////////////////////////////////////////////////////////////////////
            auto cubeMesh = tao_geometry::Mesh::Box(kCubeSize, kCubeSize, kCubeSize);

            vector<vec3> cubeMeshPositions = cubeMesh.GetPositions();
            vector<vec3> cubeMeshNormals   = cubeMesh.GetNormals();
            vector<int>  cubeMeshTriangles = cubeMesh.GetIndices();

            vector<MeshGizmoVertex> cubeMeshVertices(cubeMeshPositions.size());
            for(int i=0;i<cubeMeshVertices.size(); i++)
            {
                cubeMeshVertices[i]
                .Position(cubeMeshPositions[i] - vec3{kCubeSize*0.5f} + vec3(0.0, 0.0,  kLineStart + kLineLength))
                .Normal(cubeMeshNormals[i])
                .Color({1.0, 1.0, 1.0, 1.0});
            }

            /// Instancing the Gizmos (line and mesh)
            //////////////////////////////////////////////////////////////////////////////
            vector<LineGizmoVertex> lineVertices(
            {
                // start
                LineGizmoVertex{}
                        .Position({0.0, 0.0, kLineStart})
                        .Color(vec4{1.0f}),

                // end
                LineGizmoVertex{}
                        .Position({0.0, 0.0, kLineStart + kLineLength})
                        .Color(vec4{1.0f}),
            });

            _gr = &gr;
            auto cubeGizKey = _gr->CreateMeshGizmo(mesh_gizmo_descriptor
           {
               .vertices = cubeMeshVertices,
               .triangles = &cubeMeshTriangles,
               .zoom_invariant = true,
               .zoom_invariant_scale = 0.1f,
               .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
           });

            auto lineGizKey = _gr->CreateLineGizmo(line_list_gizmo_descriptor
           {
               .vertices = lineVertices,
               .line_size = kLineSize,
               .zoom_invariant = true,
               .zoom_invariant_scale = 0.1f,
               .pattern_texture_descriptor = nullptr,
               .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
           });

            // We are also creating a non zoom invariant line to be used when drawing
            // interactively during mouse drag.
            // This will be used to draw a line defined in 3D world coordinates,
            // see Drag() method.
            // ---------------------------------------------------------------------------
            auto immediateLineGizKey = _gr->CreateLineGizmo(line_list_gizmo_descriptor
            {
                .vertices = lineVertices,
                .line_size = kLineSize,
                .zoom_invariant = false,
                .pattern_texture_descriptor = nullptr,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
            });

            _immediateLineGizmoInstanceId = _gr->InstanceLineGizmo(immediateLineGizKey,
            {
                gizmo_instance_descriptor
                {
                    .transform = mat4{1.0f}, // world space coords
                    .color = vec4{1.0f},
                    .visible = false,
                    .selectable = false
                }
            })[0];
            // ---------------------------------------------------------------------------


            mat4
                    transformX = rotate(mat4{1.0}, 0.5f*pi<float>(), vec3{0.0, 1.0, 0.0}) ,
                    transformY = rotate(mat4{1.0},-0.5f*pi<float>(), vec3{1.0, 0.0, 0.0}) ,
                    transformZ = rotate(mat4{1.0}, 0.0f*pi<float>(), vec3{1.0, 0.0, 0.0}) ;

            vector<gizmo_instance_descriptor> instances =
            {
                /* X Axis */ gizmo_instance_descriptor{ .transform = transformX, .color = kAxisColorX, .visible = false, .selectable = false},
                /* Y Axis */ gizmo_instance_descriptor{ .transform = transformY, .color = kAxisColorY, .visible = false, .selectable = false},
                /* Z Axis */ gizmo_instance_descriptor{ .transform = transformZ, .color = kAxisColorZ, .visible = false, .selectable = false}
            };

            auto allCubeKeys     = _gr->InstanceMeshGizmo(cubeGizKey, instances);
            auto allLineKeys     = _gr->InstanceLineGizmo(lineGizKey, instances);

            _meshGizmoId = cubeGizKey;
            _lineGizmoId = lineGizKey;
            _immediateLineGizmoId = immediateLineGizKey;

            TmAxisData axisDataX
            {
                .meshInstanceId = allCubeKeys[0],
                .lineInstanceId = allLineKeys[0],
                .defaultTransform = instances  [0].transform,
                .meshTransform = instances  [0].transform,
                .lineTransform = instances  [0].transform,
                .color = instances  [0].color,
                .defaultColor = instances  [0].color,
            };
            _axisData.insert(make_pair(AxisX, axisDataX));

            TmAxisData axisDataY
            {
                .meshInstanceId = allCubeKeys[1],
                .lineInstanceId = allLineKeys[1],
                .defaultTransform = instances  [1].transform,
                .meshTransform = instances  [1].transform,
                .lineTransform = instances  [1].transform,
                .color = instances  [1].color,
                .defaultColor = instances  [1].color,
            };
            _axisData.insert(make_pair(AxisY, axisDataY));

            TmAxisData axisDataZ
            {
                .meshInstanceId = allCubeKeys[2],
                .lineInstanceId = allLineKeys[2],
                .defaultTransform = instances  [2].transform,
                .meshTransform = instances  [2].transform,
                .lineTransform = instances  [2].transform,
                .color = instances  [2].color,
                .defaultColor = instances  [2].color,
            };
            _axisData.insert(make_pair(AxisZ, axisDataZ));

            _gr->AssignGizmoToLayers(_meshGizmoId, {opaqueLayer});
            _gr->AssignGizmoToLayers(_lineGizmoId, {opaqueLayer});
            _gr->AssignGizmoToLayers(_immediateLineGizmoId, {opaqueLayer});

            // init as non-visible and non-selectable
            ResetAxisProperties(AxisX);
            ResetAxisProperties(AxisY);
            ResetAxisProperties(AxisZ);
        }

        bool HasGizmo(gizmo_instance_id id) const override
        {
            for(const auto& a : _axisData)
            {
                if( a.second.meshInstanceId == id ||
                    a.second.lineInstanceId == id)
                    return true;
            }
            return false;
        }

        virtual void HoverEnter(gizmo_instance_id id) override
        {
            auto hoveredAxis = GetAxisFromId(id);

            SetAxisProperties(hoveredAxis, nullopt, _axisData[hoveredAxis].defaultColor * 2.5f, true, true, true, false);

        }
        virtual void HoverExit(gizmo_instance_id id) override
        {
            auto hoveredAxis = GetAxisFromId(id);
            SetAxisProperties(hoveredAxis, _axisData[hoveredAxis].defaultTransform, _axisData[hoveredAxis].defaultColor, true, true, true, false);
        }

        virtual void Enable(const mat4& transformation) override
        {
            Tm::Enable(transformation);

            SetAxisProperties(AxisX, _axisData[AxisX].defaultTransform, _axisData[AxisX].defaultColor, true, true, true, false);
            SetAxisProperties(AxisY, _axisData[AxisY].defaultTransform, _axisData[AxisY].defaultColor, true, true, true, false);
            SetAxisProperties(AxisZ, _axisData[AxisZ].defaultTransform, _axisData[AxisZ].defaultColor, true, true, true, false);
        }
        virtual void Disable() override
        {
            ResetAxisProperties(AxisX);
            ResetAxisProperties(AxisY);
            ResetAxisProperties(AxisZ);

            Tm::Disable();
        }
        virtual void DragEnter(gizmo_instance_id id) override
        {
            const vec4 kGhostColor{0.65f};

            _selectedAxis = GetAxisFromId(id);

            // Hide the non - selected axis
            SetAxisProperties(AxisX, nullopt, nullopt, _selectedAxis.value() == AxisX, false, _selectedAxis.value() == AxisX, false);
            SetAxisProperties(AxisY, nullopt, nullopt, _selectedAxis.value() == AxisY, false, _selectedAxis.value() == AxisY, false);
            SetAxisProperties(AxisZ, nullopt, nullopt, _selectedAxis.value() == AxisZ, false, _selectedAxis.value() == AxisZ, false);

            // Hide the selected axis line (will be drawn with the "immediate mode line")
            SetAxisProperties(_selectedAxis.value(), nullopt, nullopt, false, false, false, false);

            SetImmediateLineVisibility(true);

        }
        virtual void DragExit() override
        {
            _selectedAxis = nullopt;

            SetAxisProperties(AxisX, _axisData[AxisX].defaultTransform, _axisData[AxisX].defaultColor, true, true, true, false);
            SetAxisProperties(AxisY, _axisData[AxisY].defaultTransform, _axisData[AxisY].defaultColor, true, true, true, false);
            SetAxisProperties(AxisZ, _axisData[AxisZ].defaultTransform, _axisData[AxisZ].defaultColor, true, true, true, false);

            SetImmediateLineVisibility(false);
        }
        virtual mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) override
        {
            vec3 localAxis;
            switch(_selectedAxis.value())
            {
                case(AxisX): localAxis = vec3{1.0, 0.0, 0.0}; break;
                case(AxisY): localAxis = vec3{0.0, 1.0, 0.0}; break;
                case(AxisZ): localAxis = vec3{0.0, 0.0, 1.0}; break;
            }

            vec3 prevMousePos;
            vec3 currMousePos;
            vec3 worldOrigin = Tm::_transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f};

            TranslateOnAxis(x,y,prevX, prevY, w, h, localAxis, currMousePos, prevMousePos);

            float sc  =  abs(dot(currMousePos-worldOrigin, localAxis)/dot(prevMousePos-worldOrigin, localAxis));

            // Only positive scale
            mat4 t = glm::scale(mat4{1.0f}, vec3{1.0f} + localAxis * (sc-1.0f));

            // curr and prev mouse pos are world space positions.
            // Here we need to find the translation (curr-pos) and
            // bring that in "axis/local" space.
            mat4 invTmTransf = glm::inverse(Tm::_transformation);
            mat4 tr   = translate(mat4{1.0f}, vec3{invTmTransf * vec4{currMousePos, 1.0f} - invTmTransf * vec4{prevMousePos, 1.0f}});

            SetAxisProperties(_selectedAxis.value(), tr * _axisData[_selectedAxis.value()].meshTransform, nullopt, true, true, false, false);

            mat4 zTr = _gr->GetZoomInvariantTransformation(_axisData[_selectedAxis.value()].meshInstanceId);
            vec3 start = Tm::_transformation * _axisData[_selectedAxis.value()].defaultTransform * zTr * vec4{0.0f, 0.0f, kLineStart, 1.0f};
            vec3 end   = Tm::_transformation * _axisData[_selectedAxis.value()].meshTransform    * zTr * vec4{0.0f, 0.0f, kLineStart + kLineLength, 1.0f};

            SetImmediateLinePosition(start, end, _axisData[_selectedAxis.value()].color);

            return t;
        }

    private:

        constexpr static float
                                kCubeSize    = 0.20f,
                                kLineLength  = 0.80f,
                                kLineStart   = 0.30f;
        constexpr static int
                                kLineSize = 3;
        constexpr static vec4
                                kAxisColorX = vec4{0.75, 0.1, 0.1, 1.0},
                                kAxisColorY = vec4{0.1, 0.75, 0.1, 1.0},
                                kAxisColorZ = vec4{0.1, 0.1, 0.75, 1.0};


        enum TmAxis
        {
            AxisX,
            AxisY,
            AxisZ
        };
        struct TmAxisData
        {
            gizmo_instance_id meshInstanceId;
            gizmo_instance_id lineInstanceId;
            mat4 defaultTransform;
            mat4 meshTransform;
            mat4 lineTransform;
            vec4 color;
            vec4 defaultColor;
        };

        GizmosRenderer* _gr = nullptr;
        gizmo_id _meshGizmoId;
        gizmo_id _lineGizmoId;
        map<TmAxis, TmAxisData> _axisData;
        optional<TmAxis> _selectedAxis = nullopt;

        gizmo_id _immediateLineGizmoId;
        gizmo_instance_id _immediateLineGizmoInstanceId;


        void SetImmediateLineVisibility(bool visible)
        {
            _gr->SetGizmoInstances(_immediateLineGizmoId,
            {
                make_pair(
                    _immediateLineGizmoInstanceId,
                    gizmo_instance_descriptor
                    {
                        .transform = mat4{1.0f},
                        .color = vec4{1.0f},
                        .visible = visible,
                        .selectable = false
                    })
            });
        }

        void SetImmediateLinePosition(const vec3& start, const vec3& end, const vec4& color)
        {
            _gr->SetLineGizmoVertices(_immediateLineGizmoId,
              {
                LineGizmoVertex{}.Position(start).Color( color),
                LineGizmoVertex{}.Position(end)  .Color( color),
              }
            );
        }

        void SetAxisProperties(
                TmAxis axis,
                optional<mat4> transformation,
                optional<vec4> color,
                optional<bool> meshVisible, optional<bool> meshSelectable,
                optional<bool> lineVisible, optional<bool> lineSelectable)
        {
            TmAxisData& axisData = _axisData[axis];

            axisData.meshTransform = transformation ? transformation.value() : axisData.meshTransform;
            axisData.lineTransform = transformation ? transformation.value() : axisData.lineTransform;
            axisData.color         = color          ? color.value()          : axisData.color;

            /// Mesh gizmo properties (cube)
            ///////////////////////////////////////
            _gr->SetGizmoInstances(_meshGizmoId,
               {make_pair(axisData.meshInstanceId,
                  gizmo_instance_descriptor
                  {
                      .transform    = Tm::_transformation * axisData.meshTransform,
                      .color        = axisData.color,
                      .visible      = meshVisible.has_value() ? meshVisible.value() : false,
                      .selectable   = meshSelectable.has_value() ? meshVisible.value() : false,
                  })
               });

            /// Line gizmo properties
            ///////////////////////////////////////
            _gr->SetGizmoInstances(_lineGizmoId,
               {make_pair(axisData.lineInstanceId,
                  gizmo_instance_descriptor
                  {
                      .transform = Tm::_transformation * axisData.lineTransform,
                      .color = axisData.color,
                      .visible      = lineVisible.has_value() ? lineVisible.value() : false,
                      .selectable   = lineSelectable.has_value() ? lineSelectable.value() : false,
                  })
               });
        }

        void ResetAxisProperties(TmAxis axis)
        {
            SetAxisProperties(axis, nullopt, nullopt, nullopt, nullopt, nullopt, nullopt);
        }

        void TranslateOnAxis(float x, float y, float prevX, float prevY, int w, int h, vec3 localAxis, vec3& currentPos, vec3& prevPos)
        {
            mat4 viewMatrix;
            mat4 projMatrix;
            vec2 nearFar;
            _gr->GetView(viewMatrix, projMatrix, nearFar);

            glm::mat4 viewProjMatrix = projMatrix * viewMatrix;

            // Find the world position of the
            // triad origin and axis
            vec3 worldAxis   = _transformation*vec4{localAxis, 0.0f};
            vec3 worldOrigin = _transformation*vec4{0.0f, 0.0f, 0.0f, 1.0f};


            // find the 2D closest point from the mouse
            // position to the projected axis
            vec4 l0 = viewProjMatrix * vec4{worldOrigin, 1.0};
            vec4 l1 = viewProjMatrix * vec4{worldOrigin + worldAxis, 1.0};
            vec2 l02d = l0;
            l02d /= l0.w;
            vec2 l12d = l1;
            l12d /= l1.w;

            vec2 closestPoint2D0 = ClosestPointOnLine(vec2((prevX / w) * 2.0 - 1.0, (prevY / h) * 2.0 - 1.0), l02d, l12d);
            vec2 closestPoint2D1 = ClosestPointOnLine(vec2((x / w) * 2.0 - 1.0, (y / h) * 2.0 - 1.0), l02d, l12d);


            // find the world position of the 2D point computed above
            // by projecting onto the 3D axis
            vec4 ndc0 = vec4{closestPoint2D0, 0.0, 1.0f};
            vec4 ndc1 = vec4{closestPoint2D1, 0.0, 1.0f};
            mat4 viewProjInverse = glm::inverse(viewProjMatrix);
            vec4 pw0 = viewProjInverse * ndc0;
            vec4 pw1 = viewProjInverse * ndc1;
            vec4 ow = glm::inverse(viewMatrix) * vec4{0.0, 0.0, 0.0, 1.0};

            vec3 rayDir0 = normalize(pw0 / pw0.w - ow);
            vec3 intersection0 = RayClosestPointOnLine(ow, rayDir0, worldOrigin, worldAxis);

            vec3 rayDir1 = normalize(pw1 / pw1.w - ow);
            vec3 intersection1 = RayClosestPointOnLine(ow, rayDir1, worldOrigin, worldAxis);

            currentPos  = intersection1;
            prevPos     = intersection0;

        }



        TmAxis GetAxisFromId(gizmo_instance_id id)
        {
            for(const auto& a : _axisData)
            {
                if(a.second.meshInstanceId == id || a.second.lineInstanceId == id)
                    return a.first;
            }

            throw runtime_error("Invalid gizmo instance id. Call Tm::HasGizmo before calling this method.");
        }
    };

    GizmosRenderer* _gr=nullptr;

    TmMode _currentMode = TmMode::Translate;
    map<TmMode, Tm*> _transformManipulators;

    TmTranslate _tmTranslate;
    TmRotate    _tmRotate;
    TmScale     _tmScale;

    mat4 _transformation = mat4{1.0};
    bool _enabled = false;
    const function<void(optional<gizmo_instance_id>)> _selectionCallback;
    const function<void(optional<gizmo_instance_id>)> _hoverCallback;
    const function<void(float, float, int, int)> _mouseMoveCallback;
    const function<void(float, float, int, int)> _mouseUpCallback;
    const function<void(float, float, int, int)> _mouseDownCallback;
    const function<void(const mat4&)>* _transformationChanged=nullptr;

    struct renderPasses
    {
        RenderLayer opaqueLayer;
        RenderPass opaquePass;

        RenderLayer blendLayer;
        RenderPass blendPass;
    };

    renderPasses CreateRenderLayersAndPasses()
    {
        ogl_depth_state dsPre
        {
            .depth_test_enable = true,
            .depth_write_enable = true,
            .depth_func = depth_func_less_equal,
            .depth_range_near = 0.0f,
            .depth_range_far = 1.0f
        };
        ogl_depth_state dsCol
        {
                .depth_test_enable = true,
                .depth_write_enable = false,
                .depth_func = depth_func_less_equal,
                .depth_range_near = 0.0f,
                .depth_range_far = 1.0f
        };
        ogl_rasterizer_state rs_atoc
        {
            .culling_enable=false,
            .front_face=front_face_ccw,
            .cull_mode=cull_mode_back,
            .polygon_mode=polygon_mode_fill,
            .multisample_enable=true,
            .alpha_to_coverage_enable=true,
        };
        ogl_rasterizer_state rs
        {
                .culling_enable=false,
                .front_face=front_face_ccw,
                .cull_mode=cull_mode_back,
                .polygon_mode=polygon_mode_fill,
                .multisample_enable=true,
                .alpha_to_coverage_enable=false,
        };
        ogl_blend_state bsPre
        {
            .blend_enable=false,
            .color_mask = mask_all
        };
        ogl_blend_state bsCol
        {
            .blend_enable=true,
            .blend_equation_rgb=ogl_blend_equation
            {
                    .blend_func=blend_func_add,
                    .blend_factor_src=blend_fac_src_alpha,
                    .blend_factor_dst=blend_fac_one_minus_src_alpha,
            },
            .blend_equation_alpha=ogl_blend_equation
            {
                    .blend_func=blend_func_add,
                    .blend_factor_src=blend_fac_one,
                    .blend_factor_dst=blend_fac_one_minus_src_alpha,
            },
            .color_mask = mask_all
        };

        auto opaqueLayer = _gr->CreateRenderLayer(dsPre, bsPre, rs_atoc);
        auto blendLayer  = _gr->CreateRenderLayer(dsCol, bsCol, rs);
        auto opaquePass  = _gr->CreateRenderPass({opaqueLayer});
        auto blendPass   = _gr->CreateRenderPass({blendLayer});

        _gr->AddRenderPass({opaquePass, blendPass});

        return renderPasses
        {
           .opaqueLayer = opaqueLayer,
           .opaquePass  = opaquePass,
           .blendLayer  = blendLayer,
           .blendPass   = blendPass,
        };
    }

    optional<gizmo_instance_id> _selectedItem = nullopt;
    optional<gizmo_instance_id> _hoveredItem = nullopt;

    void OnSelectionEnter(gizmo_instance_id selectedItem)
    {
        // Acquire control over mouse movement
        // and selection event.
        // This way other listener to those events
        // won't be updated as long as the user is dragging the TM
        _inputAgent.MuteCameraInput();
        //_inputAgent.Lock_areYouSureAboutThis(&_selectionCallback);

       for(const auto& tm : _transformManipulators)
       {
           if(tm.second->HasGizmo(selectedItem))
               tm.second->DragEnter(selectedItem);
       }

        ResetInputData();
    }

    void OnSelectionExit(gizmo_instance_id selectedItem)
    {
        _inputAgent.UnMuteCameraInput();
        //_inputAgent.Unlock_areYouSureAboutThis();

        for(const auto& tm : _transformManipulators)
        {
            if(tm.second->HasGizmo(selectedItem))
                tm.second->DragExit();
        }
    }

    void OnSelection(optional<gizmo_instance_id> newItem)
    {
        if( (newItem == nullopt && _selectedItem == nullopt) ||
            (newItem.has_value()&& _selectedItem.has_value() && newItem.value() == _selectedItem.value()))
            return;

        if(_selectedItem)
        {
            OnSelectionExit(_selectedItem.value());
            _selectedItem = nullopt;
        }

        if(newItem)
        {
            bool shouldBother = false;
            for(const auto& tm : _transformManipulators)
                shouldBother |= tm.second->HasGizmo(newItem.value());

            if(!shouldBother) return;

            _selectedItem = newItem;
            OnSelectionEnter(_selectedItem.value());
        }
    }

    void OnHoverEnter(gizmo_instance_id hoveredItem)
    {
        for(const auto& tm : _transformManipulators)
        {
            if(tm.second->HasGizmo(hoveredItem))
                tm.second->HoverEnter(hoveredItem);
        }
    }

    void OnHoverExit(gizmo_instance_id hoveredItem)
    {
        for(const auto& tm : _transformManipulators)
        {
            if(tm.second->HasGizmo(hoveredItem))
                tm.second->HoverExit(hoveredItem);
        }
    }

    void OnHover(optional<gizmo_instance_id> newItem)
    {
        if( (newItem == nullopt && _hoveredItem == nullopt) ||
            (newItem.has_value()&& _hoveredItem.has_value() && newItem.value() == _hoveredItem.value()))
            return;

        if(_hoveredItem)
        {
            OnHoverExit(_hoveredItem.value());
            _hoveredItem = nullopt;
        }

        if(newItem)
        {
            bool shouldBother = false;
            for(const auto& tm : _transformManipulators)
                shouldBother |= tm.second->HasGizmo(newItem.value());

            if(!shouldBother) return;

            _hoveredItem = newItem;
            OnHoverEnter(_hoveredItem.value());
        }
    }

    float _latestX=0.0f;
    float _latestY=0.0f;
    bool  _hasMouseData=false;
    void ResetInputData()
    {
        _latestX=0.0f;
        _latestY=0.0f;
        _hasMouseData=false;
    }
    void SetInputData(float x, float y)
    {
        _latestX=x;
        _latestY=y;
        _hasMouseData=true;
    }

    optional<mat4> _currentTransformation = nullopt;
    void OnDrag(float x, float y, float prevX, float prevY, int w, int h)
    {
        mat4 transform = _transformManipulators[_currentMode]->Drag(x, y, prevX, prevY, w, h);

        // update the transform manipulator transformation
        // after the user dragged the widget.
        switch(_currentMode)
        {
            case(TmMode::Translate):
            case(TmMode::Rotate):
                _currentTransformation = _currentTransformation.value() * transform;
            break;

            // Scaling transformation shouldn't influence TM's appearance
            // case(TmMode::Scale):
            //     break;

            // default: throw
        }

        // call client's callback
        if(_transformationChanged)(*_transformationChanged)(transform);
    }

    bool _dragging = false;
    void OnMouseDown(float x, float y, int w, int h)
    {
        _dragging = true;
    }
    void OnMouseUp(float x, float y, int w, int h)
    {
        if(_dragging) OnSelection(nullopt);
        
       _dragging = false;

    }
    void OnMouseMove(float x, float y, int w, int h)
    {
        if(_selectedItem && _dragging)
        {
            // Dragging
            if(!_hasMouseData)
                SetInputData(x,y);

            OnDrag(x, y, _latestX, _latestY, w, h);

            SetInputData(x,y);
        }
    }

    GizmoPickAgent& _inputAgent;
};


class LightGizmos
{
public:

    LightGizmos(GizmosRenderer* renderer, GizmoPickAgent* inputAgent)
            :_renderer{renderer}, _inputAgent{inputAgent}
    {

        _selectionCallback = bind(&LightGizmos::OnSelection, this, placeholders::_1);
        _inputAgent->SubscribeToSelection(&_selectionCallback);

        /// Sphere light gizmo
        _sphereLightGizmoData._iconGizmoId  = CreateSphereLightIconGizmo(renderer);
        _sphereLightGizmoData._lightGizmoId = CreateSphereLightAreaGizmo(renderer);

        /// Directional light gizmo
        _directionalLightGizmoData._iconGizmoId  = CreateDirectionalLightIconGizmo(renderer);
        _directionalLightGizmoData._lightGizmoId = CreateDirectionalLightDirectionGizmo(renderer);

        auto layer = renderer->CreateRenderLayer(DEPTH_LESS_EQUAL_NO_WRITE, BLEND_ON_TOP, RASTERIZER_MS);

        renderer->AssignGizmoToLayers(_sphereLightGizmoData._lightGizmoId, {layer});
        renderer->AssignGizmoToLayers(_sphereLightGizmoData._iconGizmoId, {layer});
        renderer->AssignGizmoToLayers(_directionalLightGizmoData._lightGizmoId, {layer});
        renderer->AssignGizmoToLayers(_directionalLightGizmoData._iconGizmoId, {layer});

        renderer->AddRenderPass({renderer->CreateRenderPass({layer})});
    }


    /// Sphere Light Gizmo
    //////////////////////////////////////////////////////////////////////////////////////////////
    struct SphereLightGizmoProperties
    {
        mat4 transformation;
        float radius;
        vec4 color;
    };
    class SphereLightGizmo
    {
        friend class LightGizmos;

    public:
        SphereLightGizmo(LightGizmos& parent,int index)
                         : _parent{&parent}, _index{index}
        {

        }

        void SetProperties(const mat4& transform, float radius, const vec4& color)
        {
            _parent->SetSphereLightGizmoProperties(_index, transform, radius, color);
        }

    private:
        // todo: weak_ptr?
        LightGizmos* _parent;
        int _index;
    };

    weak_ptr<SphereLightGizmo> CreateSphereLightGizmo(SphereLight& light)
    {
        gizmo_instance_descriptor lightInstanceDescriptor = GetSphereLightInstanceDesc(light.transformation.matrix(), light.radius, vec4{1.0f});

        _sphereLightGizmoData._sphereLightsProperties.push_back(SphereLightGizmoProperties
        {
            .transformation = light.transformation.matrix(),
            .radius = light.radius,
            .color = vec4{1.0f}
        });

        _sphereLightGizmoData._iconInstances.push_back(lightInstanceDescriptor);
        _sphereLightGizmoData._lightInstances.push_back(lightInstanceDescriptor);

        _sphereLightGizmoData._iconInstanceIds  = _renderer->InstancePointGizmo(_sphereLightGizmoData._iconGizmoId, _sphereLightGizmoData._iconInstances);
        _sphereLightGizmoData._lightInstanceIds = _renderer->InstanceLineStripGizmo(_sphereLightGizmoData._lightGizmoId, _sphereLightGizmoData._lightInstances);

       _sphereLightGizmoData._sphereLights.push_back(make_shared<SphereLightGizmo>(*this, _sphereLightGizmoData._lightInstances.size()-1));

       return _sphereLightGizmoData._sphereLights.back();
    }

    void SetSphereLightGizmoProperties(int index, const mat4& transform, float radius, const vec4& color)
    {
        _sphereLightGizmoData._sphereLightsProperties[index].transformation = transform;
        _sphereLightGizmoData._sphereLightsProperties[index].radius = radius;
        _sphereLightGizmoData._sphereLightsProperties[index].color = color;

        gizmo_instance_descriptor lightInstanceDescriptor = GetSphereLightInstanceDesc(transform, radius, color);
        _sphereLightGizmoData._lightInstances[index] = lightInstanceDescriptor;
        _sphereLightGizmoData._iconInstances[index]  = lightInstanceDescriptor;

        // TODO: batch update
        _renderer->SetGizmoInstances(_sphereLightGizmoData._iconGizmoId, {make_pair(_sphereLightGizmoData._iconInstanceIds[index], lightInstanceDescriptor)});
        _renderer->SetGizmoInstances(_sphereLightGizmoData._lightGizmoId, {make_pair(_sphereLightGizmoData._lightInstanceIds[index], lightInstanceDescriptor)});
    }

    void UpdateView(const mat4& viewMatrix)
    {
        vec3 eyePos = vec3{inverse(viewMatrix)*vec4{0.0, 0.0 ,0.0, 1.0}};

        vector<pair<gizmo_instance_id, gizmo_instance_descriptor>> instances(_sphereLightGizmoData._lightInstances.size());

        for(int i=0;i<instances.size();i++)
        {
            float rad = _sphereLightGizmoData._sphereLightsProperties[i].radius;

           gizmo_instance_descriptor desc = _sphereLightGizmoData._lightInstances[i];
           vec3 lightPos = desc.transform[3];

           // Make a circle on the XY plane always face the camera
           vec3 up = normalize(cross(lightPos, eyePos));
           vec3 z = normalize(eyePos-lightPos);
           vec3 x = normalize(cross(z, up));
           vec3 y = cross(z, x);

           mat4 tr = mat4{vec4{x, 0.0}, vec4{y, 0.0}, vec4{z, 0.0}, vec4{lightPos, 1.0}};

           desc.transform = tr * scale(mat4{1.0f}, vec3{rad});

           instances[i] = make_pair(_sphereLightGizmoData._lightInstanceIds[i], desc);
        }

        _renderer->SetGizmoInstances(_sphereLightGizmoData._lightGizmoId, instances);
    }

    /// Directional Light Gizmo
    //////////////////////////////////////////////////////////////////////////////////////////////
    struct DirectionalLightGizmoProperties
    {
        mat4 transformation;
        vec4 color;
    };
    class DirectionalLightGizmo
    {
        friend class LightGizmos;

    public:
        DirectionalLightGizmo(LightGizmos& parent,int index)
                : _parent{&parent}, _index{index}
        {

        }

        void SetProperties(const mat4& transform, const vec4& color)
        {
            _parent->SetDirectionalLightGizmoProperties(_index, transform, color);
        }

    private:
        // todo: weak_ptr?
        LightGizmos* _parent;
        int _index;
    };

    weak_ptr<DirectionalLightGizmo> CreateDirectionalLightGizmo(DirectionalLight& light)
    {
        gizmo_instance_descriptor lightInstanceDescriptor = GetDirectionalLightInstanceDesc(light.transformation.matrix(), vec4{1.0f});

        _directionalLightGizmoData._directionalLightsProperties.push_back(DirectionalLightGizmoProperties
                                                                {
                                                                        .transformation = light.transformation.matrix(),
                                                                        .color = vec4{1.0f}
                                                                });

        _directionalLightGizmoData._iconInstances.push_back(lightInstanceDescriptor);
        _directionalLightGizmoData._lightInstances.push_back(lightInstanceDescriptor);

        _directionalLightGizmoData._iconInstanceIds  = _renderer->InstancePointGizmo(_directionalLightGizmoData._iconGizmoId, _directionalLightGizmoData._iconInstances);
        _directionalLightGizmoData._lightInstanceIds = _renderer->InstanceLineGizmo(_directionalLightGizmoData._lightGizmoId, _directionalLightGizmoData._lightInstances);

        _directionalLightGizmoData._directionalLights.push_back(make_shared<DirectionalLightGizmo>(*this, _directionalLightGizmoData._lightInstances.size()-1));

        return _directionalLightGizmoData._directionalLights.back();
    }

    void SetDirectionalLightGizmoProperties(int index, const mat4& transform, const vec4& color)
    {
        _directionalLightGizmoData._directionalLightsProperties[index].transformation = transform;
        _directionalLightGizmoData._directionalLightsProperties[index].color = color;

        gizmo_instance_descriptor lightInstanceDescriptor = GetDirectionalLightInstanceDesc(transform, color);
        _directionalLightGizmoData._lightInstances[index] = lightInstanceDescriptor;
        _directionalLightGizmoData._iconInstances[index]  = lightInstanceDescriptor;

        // TODO: batch update
        _renderer->SetGizmoInstances(_directionalLightGizmoData._iconGizmoId, {make_pair(_directionalLightGizmoData._iconInstanceIds[index], lightInstanceDescriptor)});
        _renderer->SetGizmoInstances(_directionalLightGizmoData._lightGizmoId, {make_pair(_directionalLightGizmoData._lightInstanceIds[index], lightInstanceDescriptor)});
    }

    typedef std::variant<weak_ptr<SphereLightGizmo>, weak_ptr<DirectionalLightGizmo>> anyLightPtr;

    void SubscribeToSelectionEvents(const function<void(anyLightPtr)>& onSelectionEnter, const function<void(anyLightPtr)>& onSelectionExit)
    {
        _clientLightSelectedCallbacks  .push_back(&onSelectionEnter);
        _clientLightDeselectedCallbacks.push_back(&onSelectionExit);
    }


private:
    GizmosRenderer* _renderer;
    GizmoPickAgent* _inputAgent;

    struct SphereLightGizmoData
    {
        gizmo_id _iconGizmoId;
        gizmo_id _lightGizmoId;

        // TODO: those should be GenKeyVector<>
        vector<gizmo_instance_id>           _iconInstanceIds;
        vector<gizmo_instance_descriptor>   _iconInstances;
        vector<gizmo_instance_id>           _lightInstanceIds;
        vector<gizmo_instance_descriptor>   _lightInstances;
        vector<SphereLightGizmoProperties>  _sphereLightsProperties;
        vector<shared_ptr<SphereLightGizmo>> _sphereLights;
    };

    SphereLightGizmoData _sphereLightGizmoData;

    struct DirectionalLightGizmoData
    {
        gizmo_id _iconGizmoId;
        gizmo_id _lightGizmoId;

        // TODO: those should be GenKeyVector<>
        vector<gizmo_instance_id>               _iconInstanceIds;
        vector<gizmo_instance_descriptor>       _iconInstances;
        vector<gizmo_instance_id>               _lightInstanceIds;
        vector<gizmo_instance_descriptor>       _lightInstances;
        vector<DirectionalLightGizmoProperties> _directionalLightsProperties;
        vector<shared_ptr<DirectionalLightGizmo>> _directionalLights;
    };

    DirectionalLightGizmoData _directionalLightGizmoData;


    function<void(optional<gizmo_instance_id>)> _selectionCallback;
    vector<const function<void(anyLightPtr)>*> _clientLightSelectedCallbacks;
    vector<const function<void(anyLightPtr)>*> _clientLightDeselectedCallbacks;


    gizmo_instance_descriptor GetSphereLightInstanceDesc(const mat4& transform, float radius, const vec4& color)
    {
        return
            gizmo_instance_descriptor
            {
                .transform = transform * scale(mat4{1.0f}, vec3{radius}),
                .color     = color,
                .visible   = true,
                .selectable= true
            };
    }
    gizmo_instance_descriptor GetDirectionalLightInstanceDesc(const mat4& transform, const vec4& color)
    {
        return
            gizmo_instance_descriptor
            {
                    .transform = transform,
                    .color     = color,
                    .visible   = true,
                    .selectable= true
            };
    }


    static constexpr ogl_depth_state DEPTH_LESS_EQUAL_NO_WRITE
    {
        .depth_test_enable		= true,
        .depth_write_enable		= false,
        .depth_func	            = depth_func_less_equal,
        .depth_range_near		= 0.0,
        .depth_range_far		= 1.0,
    };
    static constexpr ogl_blend_state BLEND_ON_TOP
    {
        .blend_enable = true,
        .blend_equation_rgb     = {.blend_func = blend_func_add, .blend_factor_src = blend_fac_src_alpha, .blend_factor_dst = blend_fac_one_minus_src_alpha},
        .blend_equation_alpha   = {.blend_func = blend_func_add, .blend_factor_src = blend_fac_one,       .blend_factor_dst = blend_fac_one_minus_src_alpha},
        .color_mask = mask_all
    };
    static constexpr ogl_rasterizer_state RASTERIZER_MS
    {
        .culling_enable             = false,
        .front_face                 = front_face_ccw,
        .cull_mode                  = cull_mode_back,
        .polygon_mode               = polygon_mode_fill,
        .multisample_enable         = true,
        .alpha_to_coverage_enable   = false
    };

    static constexpr int kGizmoSize = 46;
    static gizmo_id CreateSphereLightIconGizmo(GizmosRenderer* gr)
    {
        PointGizmoVertex v;
        v.Position(vec3{0.0f})
                .Color(vec4{1.0f})
                .TexCoordStart(vec2{0.0f})
                .TexCoordEnd(vec2{1.0f});

        std::vector<PointGizmoVertex> vertices={v};

        /// Light Symbol SDF
        ///////////////////////////////////////////////////////////////////////////////////
        float h_2 = kGizmoSize*0.50f;
        float h_3 = kGizmoSize*0.33f;
        float h_4 = kGizmoSize*0.25f;
        float h_5 = kGizmoSize*0.20f;
        float h_6 = kGizmoSize*0.16f;
        float h_7 = kGizmoSize*0.14f;
        float h_9 = kGizmoSize*0.11f;

        auto bodyLow =
                SdfTrapezoid{h_6, h_2, h_3}
                        .Translate(glm::vec2{0.0, -0.5*h_3});

        auto body =
                SdfCircle<float>{vec3{0.0f}, h_4}
                    .Add(bodyLow)
                    .Translate(glm::vec2{h_2, h_2});

        auto base0 = SdfSegment{vec2{-h_6*0.5,0.0}, vec2{h_6*0.5, 0.0}}.Translate(vec2{0.0, -h_3-0.66*h_6});
        auto base1 = SdfSegment{vec2{-h_9*0.5,0.0}, vec2{h_9*0.5, 0.0}}.Translate(vec2{0.0, -h_3-0.99*h_6});

        auto base =
                base0
                .Add(base1)
                .Inflate(1.0f)
                .Translate(glm::vec2{h_2, h_2});

        auto contour = body
                .Shell(0.7f)
                .Add(base);

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kGizmoSize, kGizmoSize, {0, 0, 0, 0}};

        tex.FillWithFunction([&body, &contour](unsigned int x, unsigned int y)
         {
            float u=static_cast<float>(x);
            float v=static_cast<float>(y);
            vec2  uv{u,v};

            float valContour = glm::clamp(glm::mix(1.0f, 0.0f, contour.Evaluate(uv)*2.0f), 0.0f, 1.0f);
            float valBody    = glm::clamp(glm::mix(1.0f, 0.0f, body.Evaluate(uv)*2.0f), 0.0f, 1.0f) * 0.3f;
            float val  = glm::clamp((valBody + valContour), 0.0f, 1.0f);

            return vec<4, unsigned char>{255, 255, 255, val*255};
         });

        symbol_atlas_descriptor texDesc
        {
            .data=tex.DataPtr(),
            .data_format = tex_for_rgba,
            .data_type = tex_typ_unsigned_byte,
            .width = kGizmoSize,
            .height = kGizmoSize,
            .filter_smooth = true,
        };
        point_gizmo_descriptor descriptor
        {
             .point_half_size=kGizmoSize/2,
             .snap_to_pixel = false,
             .vertices = vertices,
             .zoom_invariant = false,
             .symbol_atlas_descriptor = &texDesc,
             .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
        };

        return gr->CreatePointGizmo(descriptor);
    }

    static gizmo_id CreateSphereLightAreaGizmo(GizmosRenderer* gr)
    {
        constexpr int kLineWidth = 2;
        constexpr float kOpacity = 0.6f;
        constexpr int kCircleSubd = 64;
        constexpr float kCircDa = pi<float>()*2.0f/kCircleSubd;

        // Geometry - circle
        // ----------------------------------------------------------------------------------
        vector<LineGizmoVertex> vertices(kCircleSubd+1);
        for(int i=0;i<=kCircleSubd;i++)
        {
            LineGizmoVertex v{};
            v.Position({cos(kCircDa*i), sin(kCircDa*i), 0.0f})
                    .Color(vec4{1.0f, 1.0f, 1.0f, kOpacity});

            vertices[i] = v;
        }

        line_strip_gizmo_descriptor descriptor
        {
            .vertices = vertices,
            .isLoop = true,
            .line_size = kLineWidth,
            .zoom_invariant = false,
            .pattern_texture_descriptor = nullptr,
            .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic,
        };

        return gr->CreateLineStripGizmo(descriptor);
    };

    static gizmo_id CreateDirectionalLightIconGizmo(GizmosRenderer* gr)
    {
        PointGizmoVertex v;
        v.Position(vec3{0.0f})
                .Color(vec4{1.0f})
                .TexCoordStart(vec2{0.0f})
                .TexCoordEnd(vec2{1.0f});

        std::vector<PointGizmoVertex> vertices={v};

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kGizmoSize, kGizmoSize, {0, 0, 0, 0}};

        /// Light Symbol SDF
        ///////////////////////////////////////////////////////////////////////////////////
        auto sdfBody = SdfCircle{vec2{0.0f}, kGizmoSize*0.25f};
        auto sdfBodyContour = sdfBody.Shell(0.75f);

        auto sdfRays =
                SdfSegment{vec2{kGizmoSize*0.30f, 0.0f}, vec2{kGizmoSize*0.45f, 0.0f}}
                        .RepeatRadial(8)
                        .Inflate(1.55f)
                        .Subtract(sdfBody.Inflate(5.0f));

        auto sdfContour =
                sdfRays
                        .Add(sdfBodyContour)
                        .Translate(vec2{kGizmoSize*0.5f});

        tex.FillWithFunction([&sdfContour, &sdfBody](unsigned int x, unsigned int y)
         {
             float u=static_cast<float>(x);
             float v=static_cast<float>(y);
             vec2  uv{u,v};

             float valContour = sdfContour.Evaluate(uv);
             float valBody    = sdfBody.Translate(vec2{kGizmoSize*0.5f}).Evaluate(uv);

             valContour = glm::clamp(glm::mix(1.0f, 0.0f, valContour*2.0f), 0.0f, 1.0f);
             valBody    = glm::clamp(glm::mix(1.0f, 0.0f, valBody*2.0f), 0.0f, 1.0f) * 0.3f;
             float val  = glm::clamp((valBody + valContour), 0.0f, 1.0f);

             return vec<4, unsigned char>{255, 255, 255, val * 255};
         });

        symbol_atlas_descriptor texDesc
        {
                .data=tex.DataPtr(),
                .data_format = tex_for_rgba,
                .data_type = tex_typ_unsigned_byte,
                .width = kGizmoSize,
                .height = kGizmoSize,
                .filter_smooth = true,
        };
        point_gizmo_descriptor descriptor
        {
                .point_half_size=kGizmoSize/2,
                .snap_to_pixel = false,
                .vertices = vertices,
                .zoom_invariant = false,
                .symbol_atlas_descriptor = &texDesc,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
        };

        return gr->CreatePointGizmo(descriptor);
    }

    static gizmo_id CreateDirectionalLightDirectionGizmo(GizmosRenderer* gr)
    {
        constexpr int kLineWidth = 4;
        constexpr float kOpacity = 0.6f;

        // Geometry - line
        // ----------------------------------------------------------------------------------
        vector<LineGizmoVertex> vertices
        ({
                 LineGizmoVertex{}.Position(vec3{0.0f, 0.0f, 0.2f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity}),
                 LineGizmoVertex{}.Position(vec3{0.0f, 0.0f, 1.0f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity})
         });

        /// Dashed line pattern texture
        ///////////////////////////////////////////////////////////////////////////////////////////////
        const int kPatternLen = 16;

        auto sdfPattern =
                SdfSegment{vec2{kPatternLen*0.33333f, 0.0f}, vec2{kPatternLen*0.66666f, 0.0f}}
                .Inflate(kLineWidth*0.40f)
                .Translate(vec2{0.0f, kLineWidth*0.5f});

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kPatternLen, kLineWidth, {0, 0, 0, 0}};
        tex.FillWithFunction([&sdfPattern](unsigned int x, unsigned int y)
         {
             float u=static_cast<float>(x);
             float v=static_cast<float>(y);
             vec2  uv{u,v};

             float val = glm::clamp(glm::mix(1.0f, 0.0f, sdfPattern.Evaluate(uv)*2.0f), 0.0f, 1.0f);

             return vec<4, unsigned char>{255, 255, 255, val*255};
         });

        pattern_texture_descriptor texDesc
        {
            .data = tex.DataPtr(),
            .data_format = tex_for_rgba,
            .data_type = tex_typ_unsigned_byte,
            .width = kPatternLen,
            .height = kLineWidth,
            .pattern_length = kPatternLen,
        };

        line_list_gizmo_descriptor descriptor
        {
            .vertices = vertices,
            .line_size = kLineWidth,
            .zoom_invariant = true,
            .zoom_invariant_scale = 0.30f,
            .pattern_texture_descriptor = &texDesc,
            .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
        };

        return gr->CreateLineGizmo(descriptor);
    };


    enum lightType
    {
        directional,
        sphere
    };
    struct lightIndex
    {
        lightType type;
        int index;
    };

    optional<lightIndex> GetLightFromInstanceId(gizmo_instance_id id)
    {
        optional<lightIndex> res = nullopt;

        for(int i=0;i<_sphereLightGizmoData._iconInstanceIds.size(); i++)
        {
            if(_sphereLightGizmoData._iconInstanceIds[i] ==  id)
            {
                res=lightIndex{.type = sphere, .index = i};
                break;
            }
        }
        for(int i=0;i<_directionalLightGizmoData._iconInstanceIds.size(); i++)
        {
            if(_directionalLightGizmoData._iconInstanceIds[i] ==  id)
            {
                res=lightIndex{.type = directional, .index = i};
                break;
            }
        }

        return res;
    }

    anyLightPtr GetLightPtrFromIndex(lightIndex index)
    {
        anyLightPtr l;
        if(index.type == sphere) l = _sphereLightGizmoData     ._sphereLights     [index.index];
        else                     l = _directionalLightGizmoData._directionalLights[index.index];
        return l;

    }

    gizmo_instance_id GetInstanceIdFromIndex(lightIndex index)
    {
        gizmo_instance_id id;
        if(index.type == sphere) id = _sphereLightGizmoData     ._iconInstanceIds[index.index];
        else                     id = _directionalLightGizmoData._iconInstanceIds[index.index];
        return id;
    }

    std::optional<lightIndex> _latestSelectedLight = nullopt;
    void OnSelection(std::optional<gizmo_instance_id> selectedItem)
    {
        if((!selectedItem.has_value() && !_latestSelectedLight) ||
            (selectedItem.has_value() && _latestSelectedLight && selectedItem == GetInstanceIdFromIndex(_latestSelectedLight.value())))
            return;

        if(_latestSelectedLight && !selectedItem)
        {
            auto l = GetLightPtrFromIndex(_latestSelectedLight.value());

            for(auto* cbk : _clientLightDeselectedCallbacks)
                if(cbk) (*cbk)(l);

            _latestSelectedLight = nullopt;
        }

        if(selectedItem)
        {
            optional<lightIndex> selectedLightIndex = GetLightFromInstanceId(selectedItem.value());

            if(selectedLightIndex.has_value())
            {
                auto selectedLight = GetLightPtrFromIndex(selectedLightIndex.value());

                if(_latestSelectedLight.has_value())
                {
                    for (auto *cbk: _clientLightDeselectedCallbacks)
                        if (cbk) (*cbk)(selectedLight);

                    _latestSelectedLight = nullopt;
                }

                for (auto *cbk: _clientLightSelectedCallbacks)
                    if (cbk) (*cbk)(selectedLight);

                _latestSelectedLight = selectedLightIndex;
            }
        }
    }

};



void InitGizmosVC(GizmosRenderer& gr)
{
	MyGizmoLayersVC layers = InitGizmosLayersAndPassesVC(gr);
	InitViewCube(gr, layers);
	//InitArrowTriad3DVC(gr, layers.opaqueLayer);
}

// ImGui helper functions straight from the docs
// ----------------------------------------------
void InitImGui(GLFWwindow* window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // IF using Docking Branch

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);   // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();
}

void StartImGuiFrame(PbrRenderer& pbrRenderer, TransformManipulator& tm/* TODO */)
{
    // (Your code calls glfwPollEvents())
    // ...
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::ShowDemoWindow(); // Show demo window! :)

    if(ImGui::Button("Reload shaders"))
    {
        // TODO: a lot of todos....
        try
        {
            pbrRenderer.ReloadShaders();
        }
        catch(std::exception& e)
        {
            std::cout<<e.what()<<std::endl;
        }
    }

    if(ImGui::RadioButton("Translate", tm.GetMode() == TransformManipulator::TmMode::Translate)) tm.SetMode(TransformManipulator::TmMode::Translate);
    if(ImGui::RadioButton("Rotate"   , tm.GetMode() == TransformManipulator::TmMode::Rotate))    tm.SetMode(TransformManipulator::TmMode::Rotate);
    if(ImGui::RadioButton("Scale"    , tm.GetMode() == TransformManipulator::TmMode::Scale))     tm.SetMode(TransformManipulator::TmMode::Scale);


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

struct MySphereLight
{
    SphereLight light;
    GenKey<SphereLight> pbrLightKey;
    weak_ptr<LightGizmos::SphereLightGizmo> gizmoLightKey;
};
struct MyDirectionalLight
{
    DirectionalLight light;
    GenKey<DirectionalLight> pbrLightKey;
    weak_ptr<LightGizmos::DirectionalLightGizmo> gizmoLightKey;
};
int main()
{

	try
	{
		int windowWidth = 1920;
		int windowHeight = 1080;
		int fboWidth = 0;
		int fboHeight = 0;

        vec2 nearFar = vec2(0.5f, 50.f);
        vec3 eyePos = vec3(-2.5f, -2.5f, 2.f);
        vec3 eyeTrg = vec3(0.f);
        vec3 up = vec3(0.f, 0.f, 1.f);
        mat4 viewMatrix;
        mat4 projMatrix;

        /// Initialization
        //////////////////////////////////////////

        // Render Context
		RenderContext rc{ windowWidth, windowHeight };
		rc.GetFramebufferSize(&fboWidth, &fboHeight);

        // Input manager
        MouseInputManager inputManager{&rc};

        // Camera input
        CameraRotationAgent cameraRotation{eyePos, eyeTrg, up};
        CameraZoomAgent     cameraZoom    {10.0f};
        inputManager.AddInputAgent(&cameraRotation);
        inputManager.AddInputAgent(&cameraZoom);

        // ImGui
        InitImGui(rc.GetWindow());

        // Window compositor
        WindowCompositor compositor{rc, fboWidth, fboHeight};

        // Gizmos renderer
		GizmosRenderer gizRdr{ rc, fboWidth, fboHeight };

        // Gizmos interaction
        GizmoPickAgent gizmoPickAgent{&gizRdr};
        inputManager.AddInputAgent(&gizmoPickAgent);

        // Transform manipulator gizmo
        TransformManipulator tmGizmo(gizRdr,  gizmoPickAgent);

        // Light gizmos
        LightGizmos lightGizmos{&gizRdr, &gizmoPickAgent};

        // View Cube Gizmos Renderer
		int fboWidthVC  = 256;
		int fboHeightVC = 256;
		GizmosRenderer gizRdrVC{ rc, fboWidthVC, fboHeightVC };
        InitGizmosVC(gizRdrVC);

        // Pbr Renderer
        PbrRenderer pbrRdr{rc, fboWidth, fboHeight};

        // Handle window resize
        rc.SetResizeCallback
        ([&rc, &compositor, &gizRdr, &gizRdrVC, &pbrRdr, &fboWidth, &fboHeight](int w, int h)
        {
            rc.GetFramebufferSize(&fboWidth, &fboHeight);

            compositor  .Resize(fboWidth, fboHeight);
            gizRdr      .Resize(fboWidth, fboHeight);
            pbrRdr      .Resize(fboWidth, fboHeight);
        });

        // *** TEST ***
        // ----------------------------------------------------------------------------
        auto sphere = tao_geometry::Mesh::Sphere(0.3f, 32);
        tao_pbr::Mesh sphereMesh{
            sphere.GetPositions(),
            sphere.GetNormals(),
            sphere.GetTextureCoordinates(),
            sphere.GetIndices()
        };
        auto cube = tao_geometry::Mesh::Box(10.0f, 10.0f, 1.0f);
        tao_pbr::Mesh cubeMesh{
                cube.GetPositions(),
                cube.GetNormals(),
                cube.GetTextureCoordinates(),
                cube.GetIndices()
        };

        auto sphereMeshKey = pbrRdr.AddMesh(sphereMesh);
        auto cubeMeshKey = pbrRdr.AddMesh(cubeMesh);

        glm::vec3 dColor = vec3{0.9, 0.25, 0.0};
        glm::vec3 mColor = vec3{1.0};

        // dielectrics
        // -------------------------------------------------------------------------------------------------------------
        auto matD0 = pbrRdr.AddMaterial(PbrMaterial{0.05f, 0.0f, dColor});
        auto matD1 = pbrRdr.AddMaterial(PbrMaterial{0.3f, 0.0f, dColor});
        auto matD2 = pbrRdr.AddMaterial(PbrMaterial{0.5f, 0.0f, dColor});
        auto matD3 = pbrRdr.AddMaterial(PbrMaterial{0.8f, 0.0f, dColor});

        GenKey<MeshRenderer> mr0 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), {-1.5, -1.0, 0.0}), sphereMeshKey, matD0);
        GenKey<MeshRenderer> mr1 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), {-0.5, -1.0, 0.5}), sphereMeshKey, matD1);
        GenKey<MeshRenderer> mr2 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), { 0.5, -1.0, 1.0}), sphereMeshKey, matD2);
        GenKey<MeshRenderer> mr3 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), { 1.5, -1.0, 1.5}), sphereMeshKey, matD3);

        // metals
        // -------------------------------------------------------------------------------------------------------------
        auto matM0 = pbrRdr.AddMaterial(PbrMaterial{0.05f, 1.0f, mColor});
        auto matM1 = pbrRdr.AddMaterial(PbrMaterial{0.3f, 1.0f, mColor});
        auto matM2 = pbrRdr.AddMaterial(PbrMaterial{0.5f, 1.0f, mColor});
        auto matM3 = pbrRdr.AddMaterial(PbrMaterial{0.8f, 1.0f, mColor});

        GenKey<MeshRenderer> mr4 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), {-1.5, 1.0, 0.0}), sphereMeshKey, matM0);
        GenKey<MeshRenderer> mr5 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), {-0.5, 1.0, 0.5}), sphereMeshKey, matM1);
        GenKey<MeshRenderer> mr6 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), { 0.5, 1.0, 1.0}), sphereMeshKey, matM2);
        GenKey<MeshRenderer> mr7 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), { 1.5, 1.0, 1.5}), sphereMeshKey, matM3);

        GenKey<MeshRenderer> mr8 = pbrRdr.AddMeshRenderer(glm::translate(glm::mat4(1.0f), { -5.0, -5.0, -1.2}), cubeMeshKey, matD0);


        auto env = EnvironmentLight("C:/Users/Admin/Downloads/brown_photostudio_05_1k.hdr");
        auto envKey = pbrRdr.AddEnvironmentTexture(env);
        pbrRdr.SetCurrentEnvironment(envKey);

        DirectionalLight dirLight0
        {
            .transformation = glm::translate(mat4{1.0f}, vec3{-1.0f, -1.0f, 3.5f}) * glm::rotate(glm::mat4(1.0), 2.3f, vec3{1.0, 1.0, 0.0}),
            .intensity = vec3(0.1)
        };

        auto dirLightKey = pbrRdr.AddDirectionalLight(dirLight0);

        SphereLight sphereLight0
        {
            .transformation = glm::translate(glm::mat4(1.0), {1.0, 0.5, 3.0}),
            .intensity = vec3(15.0),
            .radius = 0.5
        };
        auto sphereLight0Key = pbrRdr.AddSphereLight(sphereLight0);

        SphereLight sphereLight1
        {
                .transformation = glm::translate(glm::mat4(1.0), {2.0, 1.5, 3.0}),
                .intensity = vec3(12.0),
                .radius = 0.7
        };
        auto sphereLight1Key = pbrRdr.AddSphereLight(sphereLight1);

        SphereLight sphereLight2
        {
                .transformation = glm::translate(glm::mat4(1.0), {0.0, -0.5, 2.0}) * glm::rotate(glm::mat4{1.0f}, 1.4f, glm::vec3{1.0f}),
                .intensity = vec3(5.0),
                .radius = 0.25
        };
        auto sphereLight2Key = pbrRdr.AddSphereLight(sphereLight2);

        // light gizmos
        auto directionalLightGizmo0Key = lightGizmos.CreateDirectionalLightGizmo(dirLight0);

        auto sphereLightGizmo0Key = lightGizmos.CreateSphereLightGizmo(sphereLight0);
        auto sphereLightGizmo1Key = lightGizmos.CreateSphereLightGizmo(sphereLight1);
        auto sphereLightGizmo2Key = lightGizmos.CreateSphereLightGizmo(sphereLight2);

        vector<MySphereLight> mySphereLights
        ({
            MySphereLight{.light = sphereLight0, .pbrLightKey = sphereLight0Key, .gizmoLightKey = sphereLightGizmo0Key},
            MySphereLight{.light = sphereLight1, .pbrLightKey = sphereLight1Key, .gizmoLightKey = sphereLightGizmo1Key},
            MySphereLight{.light = sphereLight2, .pbrLightKey = sphereLight2Key, .gizmoLightKey = sphereLightGizmo2Key},
         });

        vector<MyDirectionalLight> myDirectionalLights
        ({
                 MyDirectionalLight{.light = dirLight0, .pbrLightKey = dirLightKey, .gizmoLightKey = directionalLightGizmo0Key}
         });

        variant<MyDirectionalLight*, MySphereLight*> selectedLight;

        /*auto rectLight = pbrRdr.AddRectLight(
        RectLight
        {
            .transformation =
                    glm::translate(glm::mat4(1.0), {-3.0, 0.0, 1.6})*
                    glm::rotate(glm::mat4(1.0), 2.2f, vec3{0.0, 1.0, 0.0}),
            .intensity = vec3(7.0),
            .size = vec2{2.0, 3.0}
        });*/



        function<void(const mat4&)> transformLightCallback_DEBUG =
        [&selectedLight, &pbrRdr](const mat4& tr)
        {
            MyDirectionalLight* dirLight    = nullptr;
            MySphereLight*      sphereLight = nullptr;

            if(holds_alternative<MySphereLight*>(selectedLight))      sphereLight   = get<MySphereLight*>(selectedLight);
            if(holds_alternative<MyDirectionalLight*>(selectedLight)) dirLight      = get<MyDirectionalLight*>(selectedLight);

            if(!dirLight && !sphereLight) return;

            // Use only position and rotation to update the
            // light's transformation.
            // Use scale to determine the radius.
            float scaleX = length(vec3{tr[0]});
            float scaleY = length(vec3{tr[1]});
            float scaleZ = length(vec3{tr[2]});

            mat4 trNoScale = glm::scale(tr, vec3{1.0/scaleX, 1.0/scaleY, 1.0/scaleZ});

            if(sphereLight)
            {
                sphereLight->light.transformation.Transform(trNoScale);
                sphereLight->light.radius *= scaleX * scaleY * scaleZ;

                pbrRdr.UpdateSphereLight(sphereLight->pbrLightKey, sphereLight->light);

                sphereLight->gizmoLightKey.lock()->SetProperties(sphereLight->light.transformation.matrix(),
                                                                 sphereLight->light.radius, vec4{1.0f});
            }

            if(dirLight)
            {
                dirLight->light.transformation.Transform(trNoScale);
                pbrRdr.UpdateDirectionalLight(dirLight->pbrLightKey, dirLight->light);
                dirLight->gizmoLightKey.lock()->SetProperties(dirLight->light.transformation.matrix(), vec4{1.0f});
            }
        };

        function<void(LightGizmos::anyLightPtr l)> lightSelectionEnterCallback_DEBUG =
        [&transformLightCallback_DEBUG, &tmGizmo, &mySphereLights, &myDirectionalLights, &selectedLight](LightGizmos::anyLightPtr l)
        {
            // a sphere light is selected
            if(holds_alternative<weak_ptr<LightGizmos::SphereLightGizmo>>(l))
            {
                weak_ptr<LightGizmos::SphereLightGizmo> sl = get<weak_ptr<LightGizmos::SphereLightGizmo>>(l);
                for (auto &myL: mySphereLights)
                {
                    if (myL.gizmoLightKey.lock() == sl.lock())
                        selectedLight = &myL;
                }

                tmGizmo.Enable(get<MySphereLight*>(selectedLight)->light.transformation.matrix(), &transformLightCallback_DEBUG);
            }

            // a directional light is selected
            if(holds_alternative<weak_ptr<LightGizmos::DirectionalLightGizmo>>(l))
            {
                weak_ptr<LightGizmos::DirectionalLightGizmo> dl = get<weak_ptr<LightGizmos::DirectionalLightGizmo>>(l);
                for (auto &myL: myDirectionalLights)
                {
                    if (myL.gizmoLightKey.lock() == dl.lock())
                        selectedLight = &myL;
                }

                tmGizmo.Enable(get<MyDirectionalLight*>(selectedLight)->light.transformation.matrix(), &transformLightCallback_DEBUG);
            }
        };

        function<void(LightGizmos::anyLightPtr l)> lightSelectionExitCallback_DEBUG =
        [&transformLightCallback_DEBUG, &tmGizmo, &selectedLight](LightGizmos::anyLightPtr l)
        {
            tmGizmo.Disable();
        };

        lightGizmos.SubscribeToSelectionEvents(lightSelectionEnterCallback_DEBUG, lightSelectionExitCallback_DEBUG);

        // ----------------------------------------------------------------------------

        const int kHover = 1;
        const int kClick = 2;


		time_point startTime = high_resolution_clock::now();

		while (!rc.ShouldClose())
		{
            inputManager.PollMouseEvents();

            StartImGuiFrame(pbrRdr, tmGizmo);

            // zoom and rotation
			// ----------------------------------------------------------------------
			viewMatrix = cameraZoom.GetZoomMatrix()*cameraRotation.GetRotationMatrix();
            projMatrix = glm::perspective(radians<float>(60), static_cast<float>(fboWidth) / fboHeight, nearFar.x, nearFar.y);
			// ----------------------------------------------------------------------

			time_point timeNow = high_resolution_clock::now();
			auto delta = duration_cast<milliseconds>(timeNow - startTime).count();
			//animation(delta);

            auto pbrOut = pbrRdr.Render(viewMatrix, projMatrix, nearFar.x, nearFar.y);


            lightGizmos.UpdateView(viewMatrix);

            gizRdr.SetView(viewMatrix, projMatrix, nearFar);
            gizRdr.SetDepthMask(*pbrOut._depthTexture);
            auto& gizOut = gizRdr.Render();


			mat4 viewMatrixVC = viewMatrix;
            viewMatrixVC[3] = vec4{0.0, 0.0, -3.5, 1.0};
			mat4 projMatrixVC = glm::perspective(radians<float>(60), static_cast<float>(fboWidthVC) / fboHeightVC, 0.1f, 5.0f);

            gizRdrVC.SetView(viewMatrixVC, projMatrixVC, vec2{0.1f, 3.0f});
			auto& vcGizOut = gizRdrVC.Render();


            // composite windows
            // ------------------
            compositor
            .AddLayer(*pbrOut._colorTexture , WindowCompositor::location{.x = 0, .y = 0, .width = fboWidth, .height = fboHeight}, WindowCompositor::blend_option::copy)
            .AddLayer(gizOut                , WindowCompositor::location{.x = 0, .y = 0, .width = fboWidth, .height = fboHeight}, WindowCompositor::blend_option::alpha_blend)
            .AddLayer(vcGizOut              , WindowCompositor::location{.x = fboWidth - fboWidthVC, .y = fboHeight - fboHeightVC, .width = fboWidthVC, .height = fboHeightVC}, WindowCompositor::blend_option::alpha_blend)
            .GetResult();

            compositor.ClearLayers();

            // View Cube drawing seems to conflict with ImGui,
            // so I had to place it here (ImGui is hidden by the VC however)
            EndImGuiFrame();

			rc.SwapBuffers();
		}

        ImGuiShutdown();
	}
	catch (const exception& e)
	{
		cout << e.what() << endl;
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
