// 3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include <thread>

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
using namespace tao_render_context;
using namespace tao_gizmos;
using namespace tao_input;
using namespace tao_pbr;


enum MouseInputAgentTag
{
    None           = 1u<<0,
    CameraRotate   = 1u<<1,
    CameraZoom     = 1u<<2
};

// The ugliest struct...
struct MouseInputModifier
{
    MouseInputModifier(bool leftMouseButton, bool middleMouseButton, bool rightMouseButton,
                       bool ctrlKey, bool leftShiftKey):
                       lmb{leftMouseButton},
                       mmb{middleMouseButton},
                       rmb{rightMouseButton},
                       ctrl{ctrlKey},
                       lshift{leftShiftKey}
    {
    }
    bool lmb;
    bool mmb;
    bool rmb;

    bool ctrl;
    bool lshift;
};


class MouseInputManager;

class MouseInputAgent
{
public:
    MouseInputAgent(MouseInputModifier inputModifier, MouseInputAgentTag tag):
    _tag{tag},
    _mod{inputModifier}
    {
    }

    virtual void MouseUp  (float, float, int, int)  = 0;
    virtual void MouseDown(float, float, int, int)  = 0;
    virtual void MouseMove(float, float, float, float, int, int)  = 0;
    virtual void Enable()  = 0;
    virtual void Disable() = 0;
    virtual void Mute()    = 0;
    virtual void UnMute()  = 0;

    MouseInputAgentTag  GetTag()      const{return _tag;}
    MouseInputModifier  GetModifier() const{return _mod;}

    void SetInputManager(MouseInputManager* manager)
    {
        _manager = manager;
    }

private:
    MouseInputAgentTag _tag = MouseInputAgentTag::None;
    MouseInputModifier _mod;
protected:
    MouseInputManager* _manager;
};

class MouseInputManager
{
public:

    MouseInputManager(RenderContext* renderContext) :
    _rc{renderContext},
    _mouseListeners{},
    _agents{},
    _muted{None}
    {
    }

    void PollMouseEvents()
    {
        int surfaceWidth, surfaceHeight;
        _rc->GetWindowSize(surfaceWidth, surfaceHeight);

        _rc->PollEvents();

        for(int i=0;i<_agents.size();i++)
        {
            if(_agents[i]->GetTag() & _muted) continue;

            float posX, posY, smoothPosX, smoothPosY;
            _mouseListeners[i]->Position(posX, posY, false, tao_input::mouse_origin::bottom_left);
            _mouseListeners[i]->Position(smoothPosX, smoothPosY, true, tao_input::mouse_origin::bottom_left);

            _agents[i]->MouseMove(posX, posY, smoothPosX, smoothPosY, surfaceWidth, surfaceHeight);
        }

    }

    void AddInputAgent(MouseInputAgent* agent)
    {
        agent->SetInputManager(this);

        _agents.push_back(agent);

        auto shouldListedPredicate = [this, agent]()
        {
            return
                    !(this->_rc->Mouse().IsKeyPressed(left_shift_key)      ^ agent->GetModifier().lshift) &&
                    !(this->_rc->Mouse().IsKeyPressed(left_control_key)    ^ agent->GetModifier().ctrl) &&
                    !(this->_rc->Mouse().IsPressed(left_mouse_button)    ^ agent->GetModifier().lmb) &&
                    !(this->_rc->Mouse().IsPressed(middle_mouse_button)  ^ agent->GetModifier().mmb) &&
                    !(this->_rc->Mouse().IsPressed(right_mouse_button)   ^ agent->GetModifier().rmb);
        };

        function<void()> enableFunc = bind(&MouseInputAgent::Enable, agent);
        function<void()> disableFunc = bind(&MouseInputAgent::Disable, agent);
        function<void(float, float, int, int)> mouseDownFunc = bind(&MouseInputAgent::MouseDown, agent,placeholders::_1,placeholders::_2,placeholders::_3,placeholders::_4);
        function<void(float, float, int, int)> mouseUpFunc = bind(&MouseInputAgent::MouseUp, agent,placeholders::_1,placeholders::_2,placeholders::_3,placeholders::_4);

        auto listenerPtr = make_shared<MouseInputListener>(shouldListedPredicate, enableFunc,disableFunc, mouseDownFunc, mouseUpFunc, 60.0f);

        _mouseListeners.push_back(listenerPtr);
        _rc->Mouse().AddListener(listenerPtr);
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
    vector<shared_ptr<MouseInputListener>>  _mouseListeners;
    vector<MouseInputAgent*>                _agents;
    MouseInputAgentTag                      _muted = None;
};

class CameraZoomAgent : public MouseInputAgent
{
public:
    CameraZoomAgent(float initialDistance, MouseInputModifier inputModifier):
            MouseInputAgent{inputModifier, CameraZoom},
            _shouldRefreshData{true},
            _latestMouseY{-1.0},
            _distance{initialDistance},
            _zoom{1.0f}
    {
        ZoomCamera(0.0f);
    }
    mat4 GetZoomMatrix()
    {
        return _zoom;
    }

    void MouseUp(float x, float y, int w, int h) override{}
    void MouseDown(float x, float y, int w, int h) override{}
    void MouseMove(float x, float y, float smoothX, float smoothY, int w, int h) override
    {
        if(_shouldRefreshData)
        {
            _latestMouseY = smoothY;

            _shouldRefreshData=false;
        }

        float dy = (smoothY-_latestMouseY)/h;

        ZoomCamera(dy);

        _latestMouseY = smoothY;
    }

    void Enable() override
    {
        _shouldRefreshData = true;
        _manager->MuteAgents(CameraRotate);
    }

    void Disable() override
    {
        _manager->UnMuteAgents(CameraRotate);
    }

    void Mute() override
    {
    }

    void UnMute() override
    {
        _shouldRefreshData = true;
    }

private:
    bool  _shouldRefreshData;
    float _latestMouseY;
    float _distance;
    mat4  _zoom;

    void ZoomCamera(float dy)
    {
        const float kMinDst = 0.1;
        const float kMaxDst = 30.0;
        const float kSensitivity = 25.0f;

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
    CameraRotationAgent(vec3 initialEyePosition, vec3 initialTarget, vec3 initialUp, MouseInputModifier inputModifier):
    MouseInputAgent{inputModifier, CameraRotate},
    _shouldRefreshData{true},
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

    void MouseUp(float x, float y, int w, int h) override{}
    void MouseDown(float x, float y, int w, int h) override{}
    void MouseMove(float x, float y, float smoothX, float smoothY, int w, int h) override
    {
        if(_shouldRefreshData)
        {
            _latestMouseX = smoothX;
            _latestMouseY = smoothY;

            _shouldRefreshData=false;
        }

        float dx = (smoothX-_latestMouseX)/w;
        float dy = (smoothY-_latestMouseY)/h;

        RotateCamera(dx, dy);

        _latestMouseX = smoothX;
        _latestMouseY = smoothY;
    }

    void Enable() override
    {
        _shouldRefreshData = true;
        _manager->MuteAgents(CameraZoom);
    }

    void Disable() override
    {
        _manager->UnMuteAgents(CameraZoom);
    }

    void Mute() override
    {
    }

    void UnMute() override
    {
        _shouldRefreshData = true;
    }

private:
    bool  _shouldRefreshData;
    float _latestMouseX;
    float _latestMouseY;
    vec3  _eyePosition;
    vec3  _target;
    vec3  _up;
    mat4  _rotation;

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
			tao_gizmos_sdf::SdfSegment<float>{ sampl, dashStart, dashEnd };
		val = tao_gizmos_sdf::SdfInflate(val, patternWidth*0.3f);
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

	gr.SetRenderPasses({
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

class GizmoManager
{
public:
    GizmoManager(GizmosRenderer* gr)
                    :   _gr{gr},
                        _gizmoLayers{InitGizmosLayersAndPasses()},
                        _transformManipulator(*gr, _gizmoLayers.opaqueLayer),
                        _lightGizmos(gr, _gizmoLayers.opaqueLayer)
    {
        InitGrid();
    }

    /*void OnMouseMove(float x, float y, int windowWidth, int windowHeight, const glm::mat4& viewMatrix, const glm::mat4& projMatrix)
    {
        if(_dragging)
        {
            mat4 transl=mat4{1.0};
            const float kInc=0.01f;
            if(_latestClickedItem==_transformManipulator.AxisId(TransformManipulator::TmAxis::AxisX))
            {
                // Translate along X
                glm::mat4 viewProjMatrix = projMatrix*viewMatrix;

                // find the 2D closest point from the mouse
                // position to the projected axis
                vec4 l0 = viewProjMatrix*vec4{0.0, 0.0, 0.0, 1.0};
                vec4 l1 = viewProjMatrix*vec4{1.0, 0.0, 0.0, 1.0};
                vec2 l02d = l0;
                l02d/=l0.w;

                vec2 l12d = l1;
                l12d/=l1.w;

                vec2 closestPoint2D = ClosestPointOnLine(vec2((x/windowWidth)*2.0-1.0,(y/windowHeight)*2.0-1.0), l02d, l12d);

                vec4 ndc = vec4{closestPoint2D, 0.0, 1.0f};
                vec4 pw  = glm::inverse(viewProjMatrix)*ndc;
                vec4 ow  = glm::inverse(viewMatrix)*vec4{0.0, 0.0, 0.0, 1.0};
                vec3 rayO = ow;
                vec3 rayDir = normalize(pw/pw.w-ow);
                vec3 intersection = RayClosestPointOnLine(rayO, rayDir, vec3{0.0}, vec3{1.0, 0.0, 0.0});
                transl = glm::translate(mat4{1.0}, intersection);
            }
            else if(_latestClickedItem==_transformManipulator.AxisId(TransformManipulator::TmAxis::AxisY))
            {
                // Translate along Y
                transl = glm::translate(mat4{1.0}, vec3{0.0, kInc*y, 0.0f});
            }
            else if(_latestClickedItem==_transformManipulator.AxisId(TransformManipulator::TmAxis::AxisZ))
            {
                // Translate along Z
                transl = glm::translate(mat4{1.0}, vec3{0.0f, 0.0f, 0.0f});
            }

            _transformManipulator.SetTransformation(transl);
        }
    }*/

    class LightGizmos
    {
    public:
        LightGizmos(GizmosRenderer* renderer, const RenderLayer& layer)
        :_renderer{renderer}
        {
            PointGizmoVertex v;
            v.Position(vec3{0.0f})
            .Color(vec4{1.0f})
            .TexCoordStart(vec2{0.0f})
            .TexCoordEnd(vec2{1.0f});

            std::vector<PointGizmoVertex> vertices={v};

            _gizmoKey = renderer->CreatePointGizmo(point_gizmo_descriptor{8, false, vertices,false, 1.0f, nullptr, tao_gizmos::gizmo_usage_hint::usage_static});

            renderer->AssignGizmoToLayers(_gizmoKey, {layer});
        }

        void AddLightGizmo(SphereLight* light)
        {
            _lights.push_back(light);
            gizmo_instance_descriptor desc
            {
                .transform = light->transformation.matrix(),
                .color     = vec4{light->intensity, 1.0f},
                .visible   = true,
                .selectable= true
            };
            _instances.push_back(desc);

            _instanceKeys = _renderer->InstancePointGizmo(_gizmoKey, _instances);
        }

        void UpdateLightGizmo(SphereLight* light)
        {
            for(int i=0;i<_lights.size();i++)
            {
                // Lights,instances and instanceKeys should
                // correspond 1 to 1 (same index refer to the same element).
                if(light==_lights[i])
                {
                    gizmo_instance_descriptor desc
                    {
                            .transform = light->transformation.matrix(),
                            .color     = vec4{light->intensity, 1.0f},
                            .visible   = true,
                            .selectable= true
                    };
                    _instances[i] = desc;
                    _renderer->SetGizmoInstances(_gizmoKey, {make_pair(_instanceKeys[i], _instances[i])});

                    break;
                }
            }
        }

        optional<SphereLight*> GetLightFromID(gizmo_instance_id id)
        {
            for(int i=0;i<_instanceKeys.size();i++)
            {
                if(id==_instanceKeys[i]) return _lights[i];
            }

            return nullopt;
        }

    private:
        GizmosRenderer* _renderer;
        gizmo_id        _gizmoKey;
        std::vector<SphereLight*>               _lights;
        std::vector<gizmo_instance_descriptor>  _instances;
        std::vector<gizmo_instance_id>          _instanceKeys;
    };

    class TransformManipulator
    {
    public:
        TransformManipulator(GizmosRenderer& gr, RenderLayer& layer)
        :_gr{&gr}
        {
            const float
                    kCylRadius  = 0.05f,
                    kCylLength  = 0.6f,
                    kConeRadius = 0.12f,
                    kConeLength = 0.4f;
            const int   kArrowSubdv = 16;

            const vec4
                    kAxisColorX = vec4{0.9, 0.1, 0.0, 1.0},
                    kAxisColorY = vec4{0.1, 0.9, 0.0, 1.0},
                    kAxisColorZ = vec4{0.1, 0.1, 0.9, 1.0};

            auto arrowMesh = tao_geometry::Mesh::Arrow(kCylRadius, kCylLength, kConeRadius, kConeLength, kArrowSubdv);

            vector<glm::vec3> arrowMeshPosition  = arrowMesh.GetPositions();
            vector<glm::vec3> arrowMeshNormals   = arrowMesh.GetNormals();
            vector<int>       arrowMeshTriangles = arrowMesh.GetIndices();
            vector<MeshGizmoVertex> arrowMeshVertices(arrowMeshPosition.size());
            for(int i=0;i<arrowMeshVertices.size(); i++)
            {
                arrowMeshVertices[i]
                        .Position(arrowMeshPosition[i])
                        .Normal(arrowMeshNormals[i])
                        .Color({1.0, 1.0, 1.0, 1.0});
            }

            auto gizKey = gr.CreateMeshGizmo(mesh_gizmo_descriptor
                                                       {
                                                               .vertices = arrowMeshVertices,
                                                               .triangles = &arrowMeshTriangles,
                                                               .zoom_invariant = true,
                                                               .zoom_invariant_scale = 0.1f
                                                       });

            mat4
                    transformX = rotate(mat4{1.0}, 0.5f*pi<float>(), vec3{0.0, 1.0, 0.0}),
                    transformY = rotate(mat4{1.0},-0.5f*pi<float>(), vec3{1.0, 0.0, 0.0}),
                    transformZ = rotate(mat4{1.0}, 0.0f*pi<float>(), vec3{1.0, 0.0, 0.0});

            vector<gizmo_instance_descriptor> instances =
                    {
                            /* X Axis */ gizmo_instance_descriptor{ transformX, kAxisColorX},
                            /* Y Axis */ gizmo_instance_descriptor{ transformY, kAxisColorY},
                            /* Z Axis */ gizmo_instance_descriptor{ transformZ, kAxisColorZ}
                    };

            auto allKeys = gr.InstanceMeshGizmo(gizKey, instances);

            gr.AssignGizmoToLayers(gizKey, { layer });

            _tmKey = gizKey;
            _tmAxisX.id = allKeys[0];
            _tmAxisX.transform = transformX;
            _tmAxisX.defaultColor = kAxisColorX;
            _tmAxisX.color = kAxisColorX;

            _tmAxisY.id = allKeys[1];
            _tmAxisY.transform = transformY;
            _tmAxisY.defaultColor = kAxisColorY;
            _tmAxisY.color = kAxisColorY;

            _tmAxisZ.id = allKeys[2];
            _tmAxisZ.transform = transformZ;
            _tmAxisZ.defaultColor = kAxisColorZ;
            _tmAxisZ.color = kAxisColorZ;
        }

        enum class TmAxis
        {
            AxisX,
            AxisY,
            AxisZ,
        };

        void SetAxisColor(TmAxis axis, vec4 color)
        {
            SetResetAxisColor(axis, color, true);
        }

        void ResetAxisColor(TmAxis axis)
        {
            SetResetAxisColor(axis, nullopt, false);
        }

        void SetTransformation(const mat4& transformation)
        {
            SetTransform(transformation);
        }

        gizmo_instance_id AxisId(TmAxis axis)
        {
            switch(axis)
            {
                case(TmAxis::AxisX): return _tmAxisX.id; break;
                case(TmAxis::AxisY): return _tmAxisY.id; break;
                case(TmAxis::AxisZ): return _tmAxisZ.id; break;
            }
        }

    private:
        struct TmAxisData
        {
            gizmo_instance_id id;
            mat4 transform;
            vec4 color;
            vec4 defaultColor;
        };
        GizmosRenderer* _gr=nullptr;

        gizmo_id    _tmKey;
        TmAxisData _tmAxisX;
        TmAxisData _tmAxisY;
        TmAxisData _tmAxisZ;

        mat4 _transformation=mat4{1.0};

        void SetResetAxisColor(TmAxis axis, optional<vec4> color, bool set)
        {
            TmAxisData* axisData;
            switch(axis)
            {
                case(TmAxis::AxisX): axisData = &_tmAxisX; break;
                case(TmAxis::AxisY): axisData = &_tmAxisY; break;
                case(TmAxis::AxisZ): axisData = &_tmAxisZ; break;
            }
            if(!set)        axisData->color = axisData->defaultColor;
            else if(color)  axisData->color = color.value();
            _gr->SetGizmoInstances(_tmKey,{make_pair(axisData->id, gizmo_instance_descriptor{_transformation*axisData->transform,axisData->color})});
        }

        void SetTransform(const mat4& transform)
        {
            _transformation = transform;
            _gr->SetGizmoInstances(_tmKey, {
                    make_pair(_tmAxisX.id,gizmo_instance_descriptor{_transformation * _tmAxisX.transform, _tmAxisX.color}),
                    make_pair(_tmAxisY.id,gizmo_instance_descriptor{_transformation * _tmAxisY.transform, _tmAxisY.color}),
                    make_pair(_tmAxisZ.id,gizmo_instance_descriptor{_transformation * _tmAxisZ.transform, _tmAxisZ.color})
            });
        }
    };

    LightGizmos& GetLightGizmos(){return _lightGizmos;}

    void IssueMousePickRequest(int x, int y, const function<void(optional<gizmo_instance_id>)>& callback)
    {
        _gr->GetGizmoUnderCursor(x, y, _latestViewMatrix, _latestProjMatrix, _latestNearFar, callback);
    }

    OglTexture2D& Render(const mat4& viewMatrix, const mat4& projMatrix, const vec2& nearFar, OglTexture2D* depthBuffer)
    {
        _latestViewMatrix = viewMatrix;
        _latestProjMatrix = projMatrix;
        _latestNearFar    = nearFar;

        return _gr->Render(viewMatrix, projMatrix, nearFar, depthBuffer);
    }

private:
    typedef function<void(optional<gizmo_instance_id>)> selection_callback;
    GizmosRenderer* _gr;
    MyGizmoLayers   _gizmoLayers;
    TransformManipulator _transformManipulator;
    LightGizmos _lightGizmos;
    mat4 _latestViewMatrix;
    mat4 _latestProjMatrix;
    vec2 _latestNearFar;

    RenderLayer InitTransparentGizmoLayer()
    {
        auto blendLayer = _gr->CreateRenderLayer(
    ogl_depth_state
            {
                .depth_test_enable = true,
                .depth_write_enable = false,
                .depth_func = depth_func_less_equal,
                .depth_range_near = 0.0,
                .depth_range_far = 1.0
            },
    ogl_blend_state
            {
                .blend_enable = true,
                .blend_equation_rgb = ogl_blend_equation{blend_func_add, blend_fac_src_alpha, blend_fac_one_minus_src_alpha},
                .blend_equation_alpha = ogl_blend_equation{blend_func_add, blend_fac_one, blend_fac_one }
            },
    ogl_rasterizer_state
            {
                .culling_enable = false ,
                .polygon_mode = polygon_mode_fill,
                .multisample_enable = true,
                .alpha_to_coverage_enable = false
            }
        );

        return blendLayer;
    }

    MyGizmoLayers InitGizmosLayersAndPasses()
    {
        auto opaqueLayer		= InitOpaqueGizmoLayer();
        auto transparentLayer	= InitTransparentGizmoLayer();
        auto opaquePass			= _gr->CreateRenderPass({ opaqueLayer });
        auto transparentPass	= _gr->CreateRenderPass({ transparentLayer });

        _gr->SetRenderPasses({ opaquePass, transparentPass });

        MyGizmoLayers layers
        {
            .opaqueLayer = opaqueLayer,
            .transparentLayer = transparentLayer
        };

        return layers;
    }

    RenderLayer InitOpaqueGizmoLayer()
    {
        auto opaqueLayer = _gr->CreateRenderLayer(
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
                },
        ogl_rasterizer_state
                {
                    .culling_enable = false ,
                    .polygon_mode = polygon_mode_fill,
                    .multisample_enable = true,
                    .alpha_to_coverage_enable = true
                }
        );

        return opaqueLayer;
    }

    void InitGrid()
    {
        constexpr float width			= 10.0f;
        constexpr float height			= 10.0f;
        constexpr unsigned int subdvX	= 51;
        constexpr unsigned int subdvY	= 51;
        constexpr vec4 colDark			= vec4{ 0.7, 0.7, 0.7, 0.4 };
        constexpr vec4 colLight			= vec4{ 0.4, 0.4, 0.4, 0.4 };


        vector<LineGizmoVertex> vertices{ (subdvX + subdvY) *2 };

        for (unsigned int i = 0; i < subdvX; i++)
        {
            const vec4 color = i == subdvX / 2
                               ? vec4(0.0, 1.0, 0.0, 1.0)
                               : i % 5 == 0 ? colDark : colLight;

            vertices[i * 2 + 0] =
                    LineGizmoVertex{}
                            .Position({ (-width / 2.0f) + (width / (subdvX - 1)) * i , -height / 2.0f , -0.001f })
                            .Color(color);

            vertices[i * 2 + 1] =
                    LineGizmoVertex{}
                            .Position({ (-width / 2.0f) + (width / (subdvX - 1)) * i ,  height / 2.0f , -0.001f })
                            .Color(color);
        }

        for (unsigned int j = 0; j < subdvY; j++)
        {
            const vec4 color = j == subdvY / 2
                               ? vec4(1.0, 0.0, 0.0, 1.0)
                               : j % 5 == 0 ? colDark : colLight;

            vertices[(subdvX + j) * 2 + 0] =
                    LineGizmoVertex{}
                            .Position({ -width / 2.0f, (-height / 2.0f) + (height / (subdvY - 1)) * j, -0.001f })
                            .Color(color);

            vertices[(subdvX + j) * 2 + 1] =
                    LineGizmoVertex{}
                            .Position({ width / 2.0f, (-height / 2.0f) + (height / (subdvY - 1)) * j , -0.001f })
                            .Color(color);
        }
        auto key = _gr->CreateLineGizmo(line_list_gizmo_descriptor{
                .vertices = vertices,
                .line_size = 1,
                .pattern_texture_descriptor = nullptr
        });

        // a single instance
        vector<gizmo_instance_descriptor> instances
                {
                        gizmo_instance_descriptor
                                {
                                        .transform	= glm::mat4{1.0f},
                                        .color		= vec4{1.0f}
                                }
                };
        _gr->InstanceLineGizmo(key, instances);

        _gr->AssignGizmoToLayers(key, { _gizmoLayers.transparentLayer });
    }
};

class GizmoPickAgent:public MouseInputAgent
{
public:
    GizmoPickAgent(GizmoManager* manager):
            MouseInputAgent{MouseInputModifier{false, false, false, false, false}, None},
            _gizmoManager{manager}
    {
        _selectionCallback = bind(&GizmoPickAgent::Selection, this, placeholders::_1);
    }


    void MouseUp(float d, float d1, int i, int i1) override
    {

    }

    void MouseDown(float x, float y, int i, int i1) override
    {
        _gizmoManager->IssueMousePickRequest(x, y, _selectionCallback);
    }

    void MouseMove(float x, float y, float d2, float d3, int i, int i1) override
    {

    }

    void Enable() override
    {

    }

    void Disable() override
    {

    }

    void Mute() override
    {

    }

    void UnMute() override
    {

    }
private:
    GizmoManager* _gizmoManager;
    function<void(optional<gizmo_instance_id>)> _selectionCallback;
    optional<SphereLight*> _latestSelectedLight;
    void Selection(optional<gizmo_instance_id> selectedItem)
    {
        if(_latestSelectedLight)
        {
            _latestSelectedLight.value()->intensity = vec3{1.0, 1.0, 1.0};
            _gizmoManager->GetLightGizmos().UpdateLightGizmo(_latestSelectedLight.value());
        }


        // nothing has been picked
        if(!selectedItem)
        {
            return;
        }

        optional<SphereLight*> selectedLight = _gizmoManager->GetLightGizmos().GetLightFromID(selectedItem.value());
        if(selectedLight)
        {
            selectedLight.value()->intensity = vec3{1.0, 0.0, 1.0};
            _gizmoManager->GetLightGizmos().UpdateLightGizmo(selectedLight.value());
        }

        _latestSelectedLight = selectedLight;

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

void StartImGuiFrame(PbrRenderer& pbrRenderer /* TODO */)
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
		rc.GetFramebufferSize(fboWidth, fboHeight);

        // Input manager
        MouseInputManager inputManager{&rc};

        // Camera input
        const MouseInputModifier rotMod {true, false, false, false, false};
        const MouseInputModifier zoomMod{true, false, false, false, true};
        CameraRotationAgent cameraRotation{eyePos, eyeTrg, up, rotMod};
        CameraZoomAgent     cameraZoom    {10.0f, zoomMod};
        inputManager.AddInputAgent(&cameraRotation);
        inputManager.AddInputAgent(&cameraZoom);

        // ImGui
        InitImGui(rc.GetWindow());

        // Window compositor
        WindowCompositor compositor{rc, fboWidth, fboHeight};

        // GizmosRenderer
		GizmosRenderer gizRdr{ rc, fboWidth, fboHeight };
        GizmoManager gizmoManager(&gizRdr);

        // Gizmos interaction
        GizmoPickAgent gizmoPick{&gizmoManager};
        inputManager.AddInputAgent(&gizmoPick);


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
            rc.GetFramebufferSize(fboWidth, fboHeight);

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
            .transformation = glm::rotate(glm::mat4(1.0), 2.3f, vec3{1.0, 1.0, 0.0}),
            .intensity = vec3(0.35)
        };

        auto dirLightKey = pbrRdr.AddDirectionalLight(dirLight0);

        SphereLight sphereLight0
        {
            .transformation = glm::translate(glm::mat4(1.0), {1.0, 0.5, 3.0}),
            .intensity = vec3(8.0),
            .radius = 0.6
        };
        auto sphereLight = pbrRdr.AddSphereLight(sphereLight0);

        /*auto rectLight = pbrRdr.AddRectLight(
        RectLight
        {
            .transformation =
                    glm::translate(glm::mat4(1.0), {-3.0, 0.0, 1.6})*
                    glm::rotate(glm::mat4(1.0), 2.2f, vec3{0.0, 1.0, 0.0}),
            .intensity = vec3(7.0),
            .size = vec2{2.0, 3.0}
        });*/

        gizmoManager.GetLightGizmos().AddLightGizmo(&sphereLight0);


        // ----------------------------------------------------------------------------

        const int kHover = 1;
        const int kClick = 2;


		time_point startTime = high_resolution_clock::now();

		while (!rc.ShouldClose())
		{
            inputManager.PollMouseEvents();

            StartImGuiFrame(pbrRdr);

            // zoom and rotation
			// ----------------------------------------------------------------------
			viewMatrix = cameraZoom.GetZoomMatrix()*cameraRotation.GetRotationMatrix();


            projMatrix = glm::perspective(radians<float>(60), static_cast<float>(fboWidth) / fboHeight, nearFar.x, nearFar.y);
			// ----------------------------------------------------------------------

			time_point timeNow = high_resolution_clock::now();
			auto delta = duration_cast<milliseconds>(timeNow - startTime).count();
			//animation(delta);

            auto pbrOut = pbrRdr.Render(viewMatrix, projMatrix, nearFar.x, nearFar.y);


            auto& gizOut = gizmoManager.Render(viewMatrix, projMatrix, nearFar, pbrOut._depthTexture);

            //gizRdr.GetGizmoUnderCursor(mouseX, mouseY, viewMatrix, projMatrix, nearFar, GizmosRenderer::SelectionEventArgs{kHover});

			mat4 viewMatrixVC = glm::lookAt(normalize(eyePos - eyeTrg)*3.5f, vec3{ 0.0f }, vec3{0.0, 0.0, 1.0});
			mat4 projMatrixVC = glm::perspective(radians<float>(60), static_cast<float>(fboWidthVC) / fboHeightVC, 0.1f, 5.0f);

			auto& vcGizOut = gizRdrVC.Render(viewMatrixVC, projMatrixVC, vec2{0.1f, 3.0f}, nullptr);


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
