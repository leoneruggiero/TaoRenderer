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

namespace tao_scene {
    void MouseInputManager::PollMouseEvents() {
        // prevent mouse clicks to "pass through" ImGui
        // panels and widgets.
        // rc->PollEvents calls agents mouse down callbacks.
        if (ImGui::GetCurrentContext())
        if(const auto &io = ImGui::GetIO(); io.WantCaptureMouse)
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
        posY = surfaceHeight - posY;

        posX = glm::clamp(posX, 0.0, static_cast<double>(surfaceWidth));
        posY = glm::clamp(posY, 0.0, static_cast<double>(surfaceHeight));

        UpdateMouseButton(mouse_button::left, posX, posY, surfaceWidth, surfaceHeight);
        UpdateMouseButton(mouse_button::middle, posX, posY, surfaceWidth, surfaceHeight);
        UpdateMouseButton(mouse_button::right, posX, posY, surfaceWidth, surfaceHeight);

        for (int i = 0; i < _agents.size(); i++) {
            if (_agents[i]->GetTag() & _muted) continue;

            _agents[i]->MouseMove(
                    glm::min(static_cast<float>(surfaceWidth), glm::max(0.0f, static_cast<float>(posX))),
                    glm::min(static_cast<float>(surfaceHeight), glm::max(0.0f, static_cast<float>(posY))),
                    surfaceWidth, surfaceHeight);
        }
    }

    double MouseInputManager::GetTime() {
        return _rc->GetTime();
    }

    bool MouseInputManager::IsKeyPressed(key k) {
        return _rc->IsKeyPressed(k);
    }

    bool MouseInputManager::IsMouseButtonPressed(mouse_button b) {
        return _rc->IsMouseButtonPressed(b);
    }

    void MouseInputManager::AddInputAgent(MouseInputAgent *agent) {
        agent->SetInputManager(this);

        _agents.push_back(agent);
    }

    void MouseInputManager::MuteAgents(MouseInputAgentTag tag) {
        for (int i = 0; i < _agents.size(); i++) {
            bool isMuted = _agents[i]->GetTag() & _muted;
            bool willBeMuted = _agents[i]->GetTag() & tag;

            if (!isMuted && willBeMuted) _agents[i]->Mute();
        }

        _muted = static_cast<MouseInputAgentTag>(_muted | tag);
    }

    void MouseInputManager::UnMuteAgents(MouseInputAgentTag tag) {
        for (int i = 0; i < _agents.size(); i++) {
            bool isMuted = _agents[i]->GetTag() & _muted;
            bool willBeUnMuted = _agents[i]->GetTag() & tag;

            if (isMuted && willBeUnMuted) _agents[i]->UnMute();
        }

        _muted = static_cast<MouseInputAgentTag>(_muted & (~tag));
    }

    void MouseInputManager::FireMouseUp(mouse_button button, float x, float y, int w, int h) {
        for (const auto &a: _agents) {
            if (a->GetTag() & _muted) continue;

            a->MouseUp(x, y, w, h);
        }
    }

    void MouseInputManager::FireMouseDown(mouse_button button, float x, float y, int w, int h) {
        for (const auto &a: _agents) {
            if (a->GetTag() & _muted) continue;

            a->MouseDown(x, y, w, h);
        }
    }

    void
    MouseInputManager::UpdateMouseButton(mouse_button button, float x, float y, int w, int h) {
        bool wasPressed = _buttons[button];
        bool isPressed = _rc->IsMouseButtonPressed(button);

        if (!(wasPressed ^ isPressed)) return; // nothing new happened

        if (!wasPressed && isPressed) // fire mouse down
        {
            // if `button` is pressed, fire mouse down for
            // all the other buttons (if any was down)
            for (int i = mouse_button::left; i <= mouse_button::right; i++) {
                mouse_button b = static_cast<mouse_button>(i);

                if (b == button) continue;

                if (_buttons[b]) {
                    _buttons[b] = false;
                    FireMouseUp(b, x, y, w, h);
                }
            }

            _buttons[button] = true;
            FireMouseDown(button, x, y, w, h);
        }
        if (wasPressed && !isPressed) // fire mouse up
        {
            _buttons[button] = false;
            FireMouseUp(button, x, y, w, h);
        }
    }

    CameraInputAgent::CameraInputAgent(glm::vec3 initialEyePosition, glm::vec3 initialTarget, glm::vec3 up,
                                                  mouse_input_modifier zoomInputModifier,
                                                  mouse_input_modifier panInputModifier,
                                                  mouse_input_modifier rotateInputModifier) :

            MouseInputAgent{Camera},
            _enabled{false},
            _eyePosition{initialEyePosition},
            _target{initialTarget} {
        glm::vec3 dir = normalize(_target - _eyePosition);
        glm::vec3 right = cross(dir, normalize(up));
        _up = cross(right, dir);

        _mouseData.insert({zoom, smooth_mouse_data()});
        _mouseData.insert({rotate, smooth_mouse_data()});
        _mouseData.insert({pan, smooth_mouse_data()});

        _inputModifiers.insert({zoom, zoomInputModifier});
        _inputModifiers.insert({pan, panInputModifier});
        _inputModifiers.insert({rotate, rotateInputModifier});
    }

    glm::mat4 CameraInputAgent::GetViewMatrix() {
        return glm::lookAt(_eyePosition, _target, _up);
    }

    void CameraInputAgent::MouseUp(float x, float y, int w, int h) {
        if (!_enabled) return;

        _enabled = false;
    }

    void CameraInputAgent::MouseDown(float x, float y, int w, int h) {
        if (ShouldListen(zoom)) _mode = zoom;
        else if (ShouldListen(pan)) _mode = pan;
        else if (ShouldListen(rotate)) _mode = rotate;

        else return;

        _enabled = true;
        InitMouseData(_mode, x, y, MouseInputAgent::_manager->GetTime() * 1e3f);
    }

    void CameraInputAgent::MouseMove(float x, float y, int w, int h) {
        // NOTE:
        // With smooth motion (DamperMotion())
        // we should continue moving the camera
        // even after mouse up

        if (_enabled)
            UpdateMouseTarget(_mode, x, y);

        float smoothDx, smoothDy;
        UpdateMouse(_mode, MouseInputAgent::_manager->GetTime() * 1e3f, smoothDx, smoothDy);

        switch (_mode) {
            case (zoom)  :
                ZoomCamera(smoothDy / h);
                break;
            case (rotate):
                RotateCamera(smoothDx / w, smoothDy / h);
                break;
            case (pan)   :
                PanCamera(smoothDx / w, smoothDy / h);
                break;
        }
    }

    void CameraInputAgent::UnMute() {
        ResetMouseData(zoom);
        ResetMouseData(pan);
        ResetMouseData(rotate);
    }

    void CameraInputAgent::ResetMouseData(CameraInputAgent::camera_mode mode) {
        smooth_mouse_data &data = _mouseData[mode];
        data._targetMouseX = 0.0f;
        data._targetMouseY = 0.0f;
        data._latestMouseX = 0.0f;
        data._latestMouseY = 0.0f;
        data._latestTimeMs = 0.0f;
    }

    void
    CameraInputAgent::InitMouseData(CameraInputAgent::camera_mode mode, float targetX,
                                               float targetY,
                                               float timeMs) {
        smooth_mouse_data &data = _mouseData[mode];
        data._targetMouseX = targetX;
        data._targetMouseY = targetY;
        data._latestMouseX = targetX;
        data._latestMouseY = targetY;
        data._latestTimeMs = timeMs;
    }

    void CameraInputAgent::UpdateMouseTarget(CameraInputAgent::camera_mode mode, float targetX,
                                                        float targetY) {
        smooth_mouse_data &data = _mouseData[mode];
        data._targetMouseX = targetX;
        data._targetMouseY = targetY;
    }

    void
    CameraInputAgent::UpdateMouse(CameraInputAgent::camera_mode mode, float timeMs,
                                             float &smoothDx,
                                             float &smoothDy) {
        smooth_mouse_data &data = _mouseData[mode];

        float deltaTimeMs = timeMs - data._latestTimeMs;
        float smoothX = DamperMotion(data._latestMouseX, data._targetMouseX, _kDampHalfTimeMs, deltaTimeMs);
        float smoothY = DamperMotion(data._latestMouseY, data._targetMouseY, _kDampHalfTimeMs, deltaTimeMs);

        smoothDx = (smoothX - data._latestMouseX);
        smoothDy = (smoothY - data._latestMouseY);

        data._latestMouseX = smoothX;
        data._latestMouseY = smoothY;
        data._latestTimeMs += deltaTimeMs;
    }

    bool CameraInputAgent::ShouldListen(CameraInputAgent::camera_mode mode) {
        return
                (!_inputModifiers[mode].button.has_value() ||
                 MouseInputAgent::_manager->IsMouseButtonPressed(_inputModifiers[mode].button.value())) &&
                (!_inputModifiers[mode].key.has_value() ||
                 MouseInputAgent::_manager->IsKeyPressed(_inputModifiers[mode].key.value()));
    }

    void CameraInputAgent::RotateCamera(float dx, float dy) {

        glm::mat4 tr = translate(glm::mat4{1.0f}, -_target);
        glm::mat4 trInv = glm::inverse(tr);
        glm::vec3 dir = glm::normalize(_target - _eyePosition);
        glm::vec3 right = glm::cross(dir, _up);
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::pi<float>() * dy, right);
        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), -2.f * glm::pi<float>() * dx, glm::vec3(0.0f, 0.0f, 1.0f));

        // ugly: limit the rotation so that eyePos-eyeTarget is never
        // parallel to world Z.
        if (abs(dot(rotX * glm::vec4{dir, 0.0f}, glm::vec4(0.0f, 0.0f, 1.0f, 0.0f))) >= 0.98f) {
            rotX = glm::mat4{1.0f};
        }

        // translate to origin -> rotate -> translate back
        glm::mat4 transformation = trInv * rotZ * rotX * tr;

        _eyePosition = transformation * glm::vec4(_eyePosition, 1.0);
        _up = transformation * glm::vec4{_up, 0.0f};
    }

    void CameraInputAgent::ZoomCamera(float dy) {
        const float kMinDst = 0.1;
        const float kMaxDst = 40.0;
        const float kSensitivity = 1.5f;

        float dist = glm::length(_eyePosition - _target);
        float newDst = glm::max(kMinDst, dist - dist * dy * kSensitivity);

        _eyePosition = _target + glm::normalize(_eyePosition - _target) * newDst;
    }

    void CameraInputAgent::PanCamera(float dx, float dy) {
        const float kSensitivity = 1.5f;
        const float kMinHeight = 0.1f;
        // --- Compute new eye position
        // motion is proportional to how close the eye is to the target
        // (ideally "what you are inspecting")
        float fac = kSensitivity * glm::length(_eyePosition - _target);
        glm::vec3 dir = glm::normalize(_target - _eyePosition);
        glm::vec3 right = glm::cross(dir, _up);
        glm::vec3 tr = right * fac * -dx + _up * fac * -dy;

        // avoid the camera go under the XY plane
        // (it breaks the next part "compute new eye target")
        if ((_eyePosition + tr).z > kMinHeight)
            _eyePosition += tr;

        // --- Compute new eye target
        // (just the intersection between the ray from eye to target
        // and the XY plane)
        Ray r{_eyePosition, dir};
        Plane xyPlane{vec3(0.0f), vec3(0.0f, 0.0f, 1.0f)};
        auto newTrg = RayPlaneIntersection(r, xyPlane);
        _target = newTrg.has_value() ? newTrg.value() : _target;
    }

    void CameraInputAgent::Mute(){
    }

    GizmoPickAgent::GizmoPickAgent(tao_gizmos::GizmosRenderer *gizmosRenderer) :
            MouseInputAgent{MouseInputAgentTag::None},
            _gr{gizmosRenderer},
            _clientSelectionCallbacks{},
            _clientHoverCallbacks{},
            _clientMouseMoveCallbacks{},
            _clientMouseUpCallbacks{},
            _clientMouseDownCallbacks{} {
        _selectionCallback = bind(&GizmoPickAgent::OnSelection, this, placeholders::_1);
        _hoverCallback = bind(&GizmoPickAgent::OnHover, this, placeholders::_1);
    }

    void GizmoPickAgent::MouseUp(float x, float y, int w, int h) {
        if (!_dragging) return;

        for (const auto *c: _clientMouseUpCallbacks) {
            if (c) (*c)(x, y, w, h);
        }
    }

    void GizmoPickAgent::MouseDown(float x, float y, int w, int h) {
        if (!IsClickButtonDown()) return;

        _dragging = true;

        _gr->GetGizmoUnderCursor(x, y, _selectionCallback);

        for (const auto *c: _clientMouseDownCallbacks) {
            if (c) (*c)(x, y, w, h);
        }
    }

    void GizmoPickAgent::MouseMove(float x, float y, int w, int h) {
        if (!IsAnyMouseButtonDown())
            _gr->GetGizmoUnderCursor(x, y, _hoverCallback);

        if (!_dragging) return;

        for (const auto *c: _clientMouseMoveCallbacks) {
            if (c) (*c)(x, y, w, h);
        }
    }

    void
    GizmoPickAgent::SubscribeToMouseMove(const std::function<void(float, float, int, int)> *onMouseMove) {
        _clientMouseMoveCallbacks.push_back(onMouseMove);
    }

    void
    GizmoPickAgent::SubscribeToMouseDown(const std::function<void(float, float, int, int)> *onMouseDown) {
        _clientMouseDownCallbacks.push_back(onMouseDown);
    }

    void GizmoPickAgent::SubscribeToMouseUp(const std::function<void(float, float, int, int)> *onMouseUp) {
        _clientMouseUpCallbacks.push_back(onMouseUp);
    }

    void GizmoPickAgent::SubscribeToSelection(
            const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)> *onSelection) {
        _clientSelectionCallbacks.push_back(onSelection);
    }

    void GizmoPickAgent::SubscribeToHover(
            const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)> *onHover) {
        _clientHoverCallbacks.push_back(onHover);
    }

    void GizmoPickAgent::MuteCameraInput() {
        _manager->MuteAgents(static_cast<MouseInputAgentTag>(Camera));
    }

    void GizmoPickAgent::UnMuteCameraInput() {
        _manager->UnMuteAgents(static_cast<MouseInputAgentTag>(Camera));
    }

    bool GizmoPickAgent::IsClickButtonDown() const {
        return MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::left);
    }

    bool GizmoPickAgent::IsAnyMouseButtonDown() const {
        return
                MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::left) ||
                MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::middle) ||
                MouseInputAgent::_manager->IsMouseButtonPressed(mouse_button::right);
    }

    void GizmoPickAgent::OnSelection(std::optional<tao_gizmos::gizmo_instance_id> selectedItem) {
        for (const auto *c: _clientSelectionCallbacks) {
            if (c) (*c)(selectedItem);
        }
    }

    void GizmoPickAgent::OnHover(std::optional<tao_gizmos::gizmo_instance_id> selectedItem) {
        for (const auto *c: _clientHoverCallbacks) {
            if (c) (*c)(selectedItem);
        }
    }

    TransformManipulator::TransformManipulator(GizmosRenderer &gr, GizmoPickAgent &inputAgent) :
            _gr{&gr},
            _selectionCallback{bind(&TransformManipulator::OnSelection, this, placeholders::_1)},
            _hoverCallback{bind(&TransformManipulator::OnHover, this, placeholders::_1)},
            _mouseMoveCallback{
                    bind(&TransformManipulator::OnMouseMove, this, placeholders::_1, placeholders::_2, placeholders::_3,
                         placeholders::_4)},
            _mouseUpCallback{
                    bind(&TransformManipulator::OnMouseUp, this, placeholders::_1, placeholders::_2, placeholders::_3,
                         placeholders::_4)},
            _mouseDownCallback{
                    bind(&TransformManipulator::OnMouseDown, this, placeholders::_1, placeholders::_2, placeholders::_3,
                         placeholders::_4)},
            _transformationChanged{nullptr},
            _enabled{false},
            _inputAgent{inputAgent} {

        _inputAgent.SubscribeToMouseMove(&_mouseMoveCallback);
        _inputAgent.SubscribeToMouseDown(&_mouseDownCallback);
        _inputAgent.SubscribeToMouseUp(&_mouseUpCallback);
        _inputAgent.SubscribeToSelection(&_selectionCallback);
        _inputAgent.SubscribeToHover(&_hoverCallback);

        auto renderPasses = CreateRenderLayersAndPasses();

        _tmTranslate = TmTranslate{*_gr, renderPasses.opaqueLayer, renderPasses.blendLayer};
        _tmRotate = TmRotate{*_gr, renderPasses.opaqueLayer, renderPasses.blendLayer};
        _tmScale = TmScale{*_gr, renderPasses.opaqueLayer, renderPasses.blendLayer};

        _transformManipulators.insert(make_pair(TmMode::Translate, &_tmTranslate));
        _transformManipulators.insert(make_pair(TmMode::Rotate, &_tmRotate));
        _transformManipulators.insert(make_pair(TmMode::Scale, &_tmScale));
    }

    void TransformManipulator::Enable(const mat4 &initialTransformation,
                                                 const std::function<void(
                                                         const glm::mat4 &)> *transformChangedCallback) {
        _currentTransformation = initialTransformation;
        _transformationChanged = transformChangedCallback;
        _enabled = true;

        _transformManipulators[_currentMode]->Enable(initialTransformation);
    }

    void TransformManipulator::Disable() {
        _currentTransformation = nullopt;
        _transformationChanged = nullptr;
        _enabled = false;

        _transformManipulators[_currentMode]->Disable();
    }

    void TransformManipulator::SetMode(TransformManipulator::TmMode mode) {
        if (_enabled) {
            auto *currCallback = _transformationChanged;
            mat4 currTransform = _currentTransformation.value();

            if (_selectedItem.has_value())
                OnSelection(nullopt);

            if (_hoveredItem.has_value())
                OnHover(nullopt);

            Disable();
            _currentMode = mode;
            Enable(currTransform, currCallback);
        } else
            _currentMode = mode;
    }

    void
    TransformManipulator::UpdateCamera(const mat4 &viewMatrix, const mat4 &projMatrix, const vec2 &nearFar) {
        _transformManipulators[_currentMode]->CameraChanged(viewMatrix, projMatrix, nearFar);
    }

    TransformManipulator::TmMode TransformManipulator::GetMode() const {
        return _currentMode;
    }

    TransformManipulator::renderPasses TransformManipulator::CreateRenderLayersAndPasses()
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
        auto blendLayer = _gr->CreateRenderLayer(dsCol, bsCol, rs);
        auto opaquePass = _gr->CreateRenderPass({opaqueLayer});
        auto blendPass = _gr->CreateRenderPass({blendLayer});

        _gr->AddRenderPass({opaquePass, blendPass});

        return renderPasses
                {
                        .opaqueLayer = opaqueLayer,
                        .opaquePass  = opaquePass,
                        .blendLayer  = blendLayer,
                        .blendPass   = blendPass,
                };
    }

    void TransformManipulator::OnSelectionEnter(gizmo_instance_id selectedItem) {
        // Acquire control over mouse movement
        // and selection event.
        // This way other listener to those events
        // won't be updated as long as the user is dragging the TM
        _inputAgent.MuteCameraInput();

        for (const auto &tm: _transformManipulators) {
            if (tm.second->HasGizmo(selectedItem))
                tm.second->DragEnter(selectedItem);
        }

        ResetInputData();
    }

    void TransformManipulator::OnSelectionExit(gizmo_instance_id selectedItem) {
        _inputAgent.UnMuteCameraInput();

        for (const auto &tm: _transformManipulators) {
            if (tm.second->HasGizmo(selectedItem))
                tm.second->DragExit();
        }
    }

    void TransformManipulator::OnSelection(optional<gizmo_instance_id> newItem) {
        if ((newItem == nullopt && _selectedItem == nullopt) ||
            (newItem.has_value() && _selectedItem.has_value() && newItem.value() == _selectedItem.value()))
            return;

        if (_selectedItem) {
            OnSelectionExit(_selectedItem.value());
            _selectedItem = nullopt;
        }

        if (newItem) {
            bool shouldBother = false;
            for (const auto &tm: _transformManipulators)
                shouldBother |= tm.second->HasGizmo(newItem.value());

            if (!shouldBother) return;

            _selectedItem = newItem;
            OnSelectionEnter(_selectedItem.value());
        }
    }

    void TransformManipulator::OnHoverEnter(gizmo_instance_id hoveredItem) {
        for (const auto &tm: _transformManipulators) {
            if (tm.second->HasGizmo(hoveredItem))
                tm.second->HoverEnter(hoveredItem);
        }
    }

    void TransformManipulator::OnHoverExit(gizmo_instance_id hoveredItem) {
        for (const auto &tm: _transformManipulators) {
            if (tm.second->HasGizmo(hoveredItem))
                tm.second->HoverExit(hoveredItem);
        }
    }

    void TransformManipulator::OnHover(optional<gizmo_instance_id> newItem) {
        if ((newItem == nullopt && _hoveredItem == nullopt) ||
            (newItem.has_value() && _hoveredItem.has_value() && newItem.value() == _hoveredItem.value()))
            return;

        if (_hoveredItem) {
            OnHoverExit(_hoveredItem.value());
            _hoveredItem = nullopt;
        }

        if (newItem) {
            bool shouldBother = false;
            for (const auto &tm: _transformManipulators)
                shouldBother |= tm.second->HasGizmo(newItem.value());

            if (!shouldBother) return;

            _hoveredItem = newItem;
            OnHoverEnter(_hoveredItem.value());
        }
    }

    void TransformManipulator::OnDrag(float x, float y, float prevX, float prevY, int w, int h) {
        mat4 transform = _transformManipulators[_currentMode]->Drag(x, y, prevX, prevY, w, h);

        // update the transform manipulator transformation
        // after the user dragged the widget.
        switch (_currentMode) {
            case (TmMode::Translate):
            case (TmMode::Rotate):
                _currentTransformation = _currentTransformation.value() * transform;
                break;

                // Scaling transformation shouldn't influence TM's appearance
                // case(TmMode::Scale):
                //     break;

                // default: throw
        }

        // call client's callback
        if (_transformationChanged)(*_transformationChanged)(transform);
    }

    void TransformManipulator::OnMouseDown(float x, float y, int w, int h) {
        _dragging = true;
    }

    void TransformManipulator::OnMouseUp(float x, float y, int w, int h) {
        if (_dragging) OnSelection(nullopt);

        _dragging = false;

    }

    void TransformManipulator::OnMouseMove(float x, float y, int w, int h) {
        if (_selectedItem && _dragging) {
            // Dragging
            if (!_hasMouseData)
                SetInputData(x, y);

            OnDrag(x, y, _latestX, _latestY, w, h);

            SetInputData(x, y);
        }
    }

    void TransformManipulator::ResetInputData() {
        _latestX = 0.0f;
        _latestY = 0.0f;
        _hasMouseData = false;
    }

    void TransformManipulator::SetInputData(float x, float y) {
        _latestX = x;
        _latestY = y;
        _hasMouseData = true;
    }

    TransformManipulator::TmTranslate::TmTranslate(GizmosRenderer &gr, RenderLayer &depthPrePassLayer,
                                                              RenderLayer &colorBlendLayer)   // and a depth less-equal color pass with blending enabled.
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
                transformX = rotate(mat4{1.0}, 0.5f * pi<float>(), vec3{0.0, 1.0, 0.0}),
                transformY = rotate(mat4{1.0}, -0.5f * pi<float>(), vec3{1.0, 0.0, 0.0}),
                transformZ = rotate(mat4{1.0}, 0.0f * pi<float>(), vec3{1.0, 0.0, 0.0});

        vector<gizmo_instance_descriptor> instances =
            {
                /* X Axis */
                gizmo_instance_descriptor{.transform = transformX, .color = kAxisColorX, .visible = false, .selectable = false},
                /* Y Axis */
                gizmo_instance_descriptor{.transform = transformY, .color = kAxisColorY, .visible = false, .selectable = false},
                /* Z Axis */
                gizmo_instance_descriptor{.transform = transformZ, .color = kAxisColorZ, .visible = false, .selectable = false}
            };

        auto allKeys = _gr->InstanceMeshGizmo(_gizmoId, instances);

        _axisData.insert(make_pair(
            TmAxis::AxisX, TmAxisData
                {
                    .id             = allKeys[0],
                    .transform      = transformX,
                    .color          = kAxisColorX,
                    .defaultColor   = kAxisColorX,
                    .highlightColor = kAxisColorHighlightX
                }));

        _axisData.insert(make_pair(
            TmAxis::AxisY, TmAxisData
                {
                    .id             = allKeys[1],
                    .transform      = transformY,
                    .color          = kAxisColorY,
                    .defaultColor   = kAxisColorY,
                    .highlightColor = kAxisColorHighlightY
                }));

        _axisData.insert(make_pair(
            TmAxis::AxisZ, TmAxisData
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

    bool TransformManipulator::TmTranslate::HasGizmo(tao_gizmos::gizmo_instance_id id) const {
        for (const auto &a: _axisData) {
            if (a.second.id == id)
                return true;
        }
        return false;
    }

    void TransformManipulator::TmTranslate::HoverEnter(tao_gizmos::gizmo_instance_id id) {
        auto hoveredAxis = GetAxisFromId(id);

        SetAxisProperties(hoveredAxis, nullopt, _axisData[hoveredAxis].highlightColor, true, true);

    }

    void TransformManipulator::TmTranslate::HoverExit(tao_gizmos::gizmo_instance_id id) {
        auto hoveredAxis = GetAxisFromId(id);

        vector<MeshGizmoVertex> newVerts;
        vector<int> newTris;
        CreateArrowMesh(newVerts, newTris);

        _gr->SetMeshGizmoVertices(_gizmoId, newVerts, &newTris);

        SetAxisProperties(hoveredAxis, nullopt, _axisData[hoveredAxis].defaultColor, true, true);
    }

    void TransformManipulator::TmTranslate::Enable(const glm::mat4 &transformation) {
        Tm::Enable(transformation);

        SetAxisProperties(AxisX, nullopt, nullopt, true, true);
        SetAxisProperties(AxisY, nullopt, nullopt, true, true);
        SetAxisProperties(AxisZ, nullopt, nullopt, true, true);
    }

    void TransformManipulator::TmTranslate::Disable() {
        Tm::Disable();
        SetAxisProperties(AxisX, nullopt, nullopt, false, false);
        SetAxisProperties(AxisY, nullopt, nullopt, false, false);
        SetAxisProperties(AxisZ, nullopt, nullopt, false, false);
    }

    void TransformManipulator::TmTranslate::DragEnter(tao_gizmos::gizmo_instance_id id) {

        _selectedAxis = GetAxisFromId(id);

        // Hide the non-selected axis
        SetAxisProperties(AxisX, nullopt, nullopt, _selectedAxis.value() == AxisX, false);
        SetAxisProperties(AxisY, nullopt, nullopt, _selectedAxis.value() == AxisY, false);
        SetAxisProperties(AxisZ, nullopt, nullopt, _selectedAxis.value() == AxisZ, false);

    }

    void TransformManipulator::TmTranslate::DragExit() {
        _selectedAxis = nullopt;

        SetAxisProperties(AxisX, nullopt, nullopt, true, true);
        SetAxisProperties(AxisY, nullopt, nullopt, true, true);
        SetAxisProperties(AxisZ, nullopt, nullopt, true, true);
    }

    glm::mat4
    TransformManipulator::TmTranslate::Drag(float x, float y, float prevX, float prevY, int w, int h) {
        vec3 localAxis;

        switch (_selectedAxis.value()) {
            case (AxisX):
                localAxis = vec3{1.0, 0.0, 0.0};
                break;
            case (AxisY):
                localAxis = vec3{0.0, 1.0, 0.0};
                break;
            case (AxisZ):
                localAxis = vec3{0.0, 0.0, 1.0};
                break;
        }

        vec3 transl = localAxis * TranslateOnAxis(x, y, prevX, prevY, w, h, localAxis);
        mat4 t = translate(mat4{1.0f}, transl);

        Tm::_transformation *= t;
        SetAxisProperties(_selectedAxis.value(), nullopt, nullopt, true, true);
        return t;
    }

    void TransformManipulator::TmTranslate::SetAxisProperties(
            TransformManipulator::TmTranslate::TmAxis axis, std::optional<glm::mat4> transform,
            std::optional<glm::vec4> color, bool visible, bool selectable) {
        TmAxisData &axisData = _axisData[axis];

        axisData.color = color.has_value() ? color.value() : axisData.color;
        axisData.transform = transform.has_value() ? transform.value() : axisData.transform;

        _gr->SetGizmoInstances(_gizmoId, {
                                   make_pair(axisData.id,
                                             gizmo_instance_descriptor{
                                                 .transform = Tm::_transformation * axisData.transform,
                                                 .color = axisData.color,
                                                 .visible = visible,
                                                 .selectable = selectable,
                                             })
                               }
        );
    }

    float
    TransformManipulator::TmTranslate::TranslateOnAxis(float x, float y, float prevX, float prevY, int w,
                                                                  int h,
                                                                  glm::vec3 localAxis) {
        mat4 viewMatrix;
        mat4 projMatrix;
        vec2 nearFar;
        _gr->GetView(viewMatrix, projMatrix, nearFar);

        glm::mat4 viewProjMatrix = projMatrix * viewMatrix;

        // Find the world position of the
        // triad origin and axis
        vec3 worldAxis = _transformation * vec4{localAxis, 0.0f};
        vec3 worldOrigin = _transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f};


        // find the 2D closest point from the mouse
        // position to the projected axis
        vec4 l0 = viewProjMatrix * vec4{worldOrigin, 1.0};
        vec4 l1 = viewProjMatrix * vec4{worldOrigin + worldAxis, 1.0};
        vec2 l02d = l0;
        l02d /= l0.w;
        vec2 l12d = l1;
        l12d /= l1.w;

        vec2 closestPoint2D0 = PointLineClosestPoint(vec2((prevX / w) * 2.0 - 1.0, (prevY / h) * 2.0 - 1.0), l02d, l12d);
        vec2 closestPoint2D1 = PointLineClosestPoint(vec2((x / w) * 2.0 - 1.0, (y / h) * 2.0 - 1.0), l02d, l12d);

        // find the world position of the 2D point computed above
        // by projecting onto the 3D axis
        vec4 ndc0 = vec4{closestPoint2D0, 0.0, 1.0f};
        vec4 ndc1 = vec4{closestPoint2D1, 0.0, 1.0f};
        mat4 viewProjInverse = glm::inverse(viewProjMatrix);
        vec4 pw0 = viewProjInverse * ndc0;
        vec4 pw1 = viewProjInverse * ndc1;
        vec4 ow = glm::inverse(viewMatrix) * vec4{0.0, 0.0, 0.0, 1.0};

        vec3 rayDir0 = normalize(pw0 / pw0.w - ow);
        vec3 intersection0 = RayLineClosestPoint(ow, rayDir0, worldOrigin, worldAxis);

        vec3 rayDir1 = normalize(pw1 / pw1.w - ow);
        vec3 intersection1 = RayLineClosestPoint(ow, rayDir1, worldOrigin, worldAxis);

        float translation = dot(worldAxis, intersection1) - dot(worldAxis, intersection0);

        return translation;
    }

    TransformManipulator::TmTranslate::TmAxis
    TransformManipulator::TmTranslate::GetAxisFromId(tao_gizmos::gizmo_instance_id id) {
        for (const auto &a: _axisData) {
            if (a.second.id == id)
                return a.first;
        }

        throw runtime_error("Invalid gizmo instance id. Call Tm::HasGizmo before calling this method.");
    }

    void TransformManipulator::TmTranslate::CreateArrowMesh(vector <tao_gizmos::MeshGizmoVertex> &verts,
                                                                       vector<int> &tris) {
        auto arrowMesh = tao_geometry::Mesh::Arrow(kCylRadius, kCylLength, kConeRadius, kConeLength, kArrowSubdv);


        vector<vec3> arrowMeshPosition = arrowMesh.GetPositions();
        vector<vec3> arrowMeshNormals = arrowMesh.GetNormals();

        tris = arrowMesh.GetIndices();
        verts = vector<MeshGizmoVertex>(arrowMeshPosition.size());
        for (int i = 0; i < verts.size(); i++) {
            verts[i]
                    .Position(arrowMeshPosition[i] + vec3(0.0, 0.0, 0.3))
                    .Normal(arrowMeshNormals[i])
                    .Color({1.0, 1.0, 1.0, 1.0});
        }
    }

    TransformManipulator::TmRotate::TmRotate(GizmosRenderer &gr, RenderLayer &opaqueLayer,
                                                        RenderLayer &transparentLayer) {
        /// Constants
        ///////////////////////////////////////////////////
        static constexpr float
                kRadius = 1.00f;
        static constexpr int
                kLineSize = 4,
                kLineSizeForSelection = 12;
        static constexpr glm::vec4
                kAxisColorX = glm::vec4{0.8, 0.1, 0.1, 1.0},
                kAxisColorY = glm::vec4{0.1, 0.8, 0.1, 1.0},
                kAxisColorZ = glm::vec4{0.1, 0.1, 0.8, 1.0};

        /// Geometry creation
        /////////////////////////////////////////////////////
        std::vector<tao_gizmos::LineGizmoVertex> circPts = GetCircleVertices(-0.5f * glm::pi<float>(),
                                                                             0.5f * glm::pi<float>());

        /// tao_gizmos::GizmosRendererthe gizmo
        ///////////////////////////////////////////////////////
        _gr = &gr;

        _gizmoId = _gr->CreateLineStripGizmo(line_strip_gizmo_descriptor{
            .vertices = circPts,
            .isLoop   = true,
            .line_size = kLineSize,
            .zoom_invariant = true,
            .zoom_invariant_scale = 0.1f,
            .pattern_texture_descriptor = nullptr,
            .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
        });

        // Create a bigger gizmo that will be used for selection
        _gizmoIdForSelection = _gr->CreateLineStripGizmo(line_strip_gizmo_descriptor{
            .vertices = circPts,
            .isLoop   = true,
            .line_size = kLineSizeForSelection,
            .zoom_invariant = true,
            .zoom_invariant_scale = 0.1f,
            .pattern_texture_descriptor = nullptr,
            .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
        });

        mat4
                transformX = GetDefaultAxisTransform(TmAxis::AxisX),
                transformY = GetDefaultAxisTransform(TmAxis::AxisY),
                transformZ = GetDefaultAxisTransform(TmAxis::AxisZ);

        vector<gizmo_instance_descriptor> instances =
                {
                        /* X Axis */
                        gizmo_instance_descriptor{.transform = transformX, .color = kAxisColorX, .visible = false, .selectable = false},
                        /* Y Axis */
                        gizmo_instance_descriptor{.transform = transformY, .color = kAxisColorY, .visible = false, .selectable = false},
                        /* Z Axis */
                        gizmo_instance_descriptor{.transform = transformZ, .color = kAxisColorZ, .visible = false, .selectable = false}
                };

        auto allKeys = _gr->InstanceLineStripGizmo(_gizmoId, instances);
        auto allKeysForSelection = _gr->InstanceLineStripGizmo(_gizmoIdForSelection, instances);

        _axisData.insert(make_pair(
            TmAxis::AxisX, TmAxisData
                {
                    .id             = allKeys[0],
                    .idForSelection = allKeysForSelection[0],
                    .transform      = transformX,
                    .color          = kAxisColorX,
                    .defaultColor   = kAxisColorX
                }));

        _axisData.insert(make_pair(
            TmAxis::AxisY, TmAxisData
                {
                    .id             = allKeys[1],
                    .idForSelection = allKeysForSelection[1],
                    .transform      = transformY,
                    .color          = kAxisColorY,
                    .defaultColor   = kAxisColorY
                }));

        _axisData.insert(make_pair(
            TmAxis::AxisZ, TmAxisData
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

        /// Mesh gizmo custom shader graph (flat color)
        float pow = 2.5f;
        auto kPow = SGConstVec(pow, pow, pow, pow);
        auto vertColor = make_shared<SGInVertColor>();
        auto color = SGPow(vertColor, kPow);
        auto customShader = SGOutColor{color};

        vector<MeshGizmoVertex> immediateMeshVerts
            {
                MeshGizmoVertex{}.Position(vec3{0.0f}).Color(vec4{1.0f}),
                MeshGizmoVertex{}.Position(vec3{0.0f}).Color(vec4{1.0f}),
            };
        _immediateMeshGizmoId = _gr->CreateMeshGizmo(
            mesh_gizmo_descriptor{
                .vertices = immediateMeshVerts,
                .zoom_invariant = true,
                .zoom_invariant_scale = 0.1f,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
            }, customShader);

        _immediateMeshGizmoInstanceId = _gr->InstanceMeshGizmo(_immediateMeshGizmoId,
                                                               {
                                                                   gizmo_instance_descriptor{
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

        _immediateLineGizmoId = _gr->CreateLineGizmo(
            line_list_gizmo_descriptor{
                .vertices = immediateLineVerts,
                .line_size = 2,
                .zoom_invariant = true,
                .zoom_invariant_scale = 0.1f,
                .pattern_texture_descriptor = nullptr,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
            });

        _immediateLineGizmoInstanceId = _gr->InstanceLineGizmo(_immediateLineGizmoId,
                                                               {
                                                                   gizmo_instance_descriptor{
                                                                       .transform = mat4{1.0f}, // world space coords
                                                                       .color = vec4{1.0f},
                                                                       .visible = true,
                                                                       .selectable = false
                                                                   }
                                                               })[0];

        _gr->AssignGizmoToLayers(_immediateMeshGizmoId, {transparentLayer});
        _gr->AssignGizmoToLayers(_immediateLineGizmoId, {transparentLayer});
    }

    void TransformManipulator::TmRotate::HideImmediateArc()
    {
        _gr->SetGizmoInstances(_immediateMeshGizmoId, {
            make_pair(_immediateMeshGizmoInstanceId,
                      gizmo_instance_descriptor
                          {
                              .transform{mat4{1.0f}},
                              .color{vec4{1.0f}},
                              .visible = false,
                              .selectable = false
                          }
            )
        });

        _gr->SetGizmoInstances(_immediateLineGizmoId, {
            make_pair(_immediateLineGizmoInstanceId,
                      gizmo_instance_descriptor
                          {
                              .transform{mat4{1.0f}},
                              .color{vec4{1.0f}},
                              .visible = false,
                              .selectable = false
                          }
            )
        });
    }

    void
    TransformManipulator::TmRotate::DrawImmediateArc(const mat4 &transformation, float radius,
                                                                float startAngle,
                                                                float endAngle, const vec4 &color,
                                                                const vec4 &lineColor) {
        // Allow 3 complete revolutions at max, otherwise the vert count
        // could grow indefinitely. After the first turns, the visual
        // feedback is quite unnoticeable.
        float angle = endAngle - startAngle;
        float angleClamped = glm::min(abs(angle), 6.0f * pi<float>()) * sign(angle);
        int subdivisions = 16 * (1 + static_cast<int>(abs(angleClamped) / (2.0f * pi<float>())));

        vector<MeshGizmoVertex> verts((subdivisions + 1) + 1/*vertex at center*/);

        /// vertices
        verts[0].Position(vec3{0.0f}).Color(color);

        float da = angleClamped / subdivisions;
        for (int i = 0; i <= subdivisions; i++) {
            float a = startAngle + i * da;

            verts[i + 1]
                    .Position(vec3{radius * cos(a), radius * sin(a), 0.0})
                    .Normal(vec3{0.0f, 0.0f, 1.0f})
                    .Color(vec4{1.0f});
        }

        /// triangles
        vector<int> tris(subdivisions * 3);
        for (int i = 0; i < subdivisions; i++) {
            tris[i * 3 + 0] = 0;
            tris[i * 3 + 1] = i + 2;
            tris[i * 3 + 2] = i + 1;
        }

        _gr->SetMeshGizmoVertices(_immediateMeshGizmoId, verts, &tris);
        _gr->SetGizmoInstances(_immediateMeshGizmoId, {
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
                        LineGizmoVertex().Position(vec3{radius * cos(startAngle), radius * sin(startAngle), 0.0}).Color(
                                vec4{1.0f}),
                        LineGizmoVertex().Position(vec3{0.0f}).Color(vec4{1.0f}),
                        LineGizmoVertex().Position(vec3{0.0f}).Color(vec4{1.0f}),
                        LineGizmoVertex().Position(
                                vec3{radius * cos(startAngle + angle), radius * sin(startAngle + angle), 0.0}).Color(
                                vec4{1.0f})
                };
        _gr->SetLineGizmoVertices(_immediateLineGizmoId, lineVerts);
        _gr->SetGizmoInstances(_immediateLineGizmoId, {
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

    bool TransformManipulator::TmRotate::HasGizmo(tao_gizmos::gizmo_instance_id id) const {
        for (const auto &a: _axisData) {
            if (a.second.id == id || a.second.idForSelection == id)
                return true;
        }
        return false;
    }

    void TransformManipulator::TmRotate::HoverEnter(tao_gizmos::gizmo_instance_id id) {
        TmAxis axis = GetAxisFromId(id);

        SetAxisProperties(axis, _axisData[axis].defaultColor * 2.5f);
    }

    void TransformManipulator::TmRotate::HoverExit(tao_gizmos::gizmo_instance_id id) {
        TmAxis axis = GetAxisFromId(id);

        SetAxisProperties(axis, nullopt);
    }

    void TransformManipulator::TmRotate::Enable(const mat4 &transformation) {
        Tm::Enable(transformation);
        SetInstancesProperties(true, true);
    }

    void TransformManipulator::TmRotate::Disable() {
        Tm::Disable();
        SetInstancesProperties(false, false);
    }

    void TransformManipulator::TmRotate::DragEnter(tao_gizmos::gizmo_instance_id id) {
        _firstTouch = true;
        _cumulatedAngle = 0.0f;

        const vec4 kGhostColor{0.65f};

        _selectedAxis = GetAxisFromId(id);

        // Switch from half circle to full circle gizmo
        _gr->SetLineStripGizmoVertices(_gizmoId, GetCircleVertices(0.0f, 2.0f * pi<float>()), true);
        _gr->SetLineStripGizmoVertices(_gizmoIdForSelection, GetCircleVertices(0.0f, 2.0f * pi<float>()), true);

        // Hide non selected axis
        SetInstancesProperties(AxisX, _selectedAxis.value() == AxisX, false);
        SetInstancesProperties(AxisY, _selectedAxis.value() == AxisY, false);
        SetInstancesProperties(AxisZ, _selectedAxis.value() == AxisZ, false);

    }

    void TransformManipulator::TmRotate::DragExit() {
        _selectedAxis = nullopt;

        // Switch from full to half circle gizmo
        _gr->SetLineStripGizmoVertices(_gizmoId, GetCircleVertices(-0.5f * pi<float>(), 0.5f * pi<float>()), false);
        _gr->SetLineStripGizmoVertices(_gizmoIdForSelection, GetCircleVertices(-0.5f * pi<float>(), 0.5f * pi<float>()),
                                       false);

        // Show all the axis again
        SetInstancesProperties(true, true);
        HideImmediateArc();
    }

    glm::mat4
    TransformManipulator::TmRotate::Drag(float x, float y, float prevX, float prevY, int w, int h) {
        // The rotation is computed as the angular difference
        // between the vectors gizmo origin->plane projection of prev mouse position and
        // gizmo origin->plane projection of current mouse position

        // find the ray-plane intersection of current and prev mouse pos
        vec4 localAxis{_selectedAxis.value() == AxisX, _selectedAxis.value() == AxisY, _selectedAxis.value() == AxisZ,
                       0.0f};

        mat4 view, proj;
        vec2 unused;
        _gr->GetView(view, proj, unused);

        mat4 invViewProj = glm::inverse(proj * view);
        vec3 eye = glm::inverse(view) * vec4{0.0f, 0.0f, 0.0f, 1.0f};
        vec4 currMouseUnproj = invViewProj * vec4{(x / w) * 2.0f - 1.0f, (y / h) * 2.0f - 1.0f, -1.0f, 1.0f};
        vec4 prevMouseUnproj = invViewProj * vec4{(prevX / w) * 2.0f - 1.0f, (prevY / h) * 2.0f - 1.0f, -1.0f, 1.0f};
        currMouseUnproj /= currMouseUnproj.w;
        prevMouseUnproj /= prevMouseUnproj.w;
        vec3 eyeCurrDir = vec3{currMouseUnproj} - eye;
        vec3 eyePrevDir = vec3{prevMouseUnproj} - eye;

        Plane gizPl{Tm::_transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f}, Tm::_transformation * localAxis};
        Ray currRay{eye, eyeCurrDir};
        Ray prevRay{eye, eyePrevDir};

        optional<vec3> currIntersection = RayPlaneIntersection(currRay, gizPl);
        optional<vec3> prevIntersection = RayPlaneIntersection(prevRay, gizPl);

        if (!currIntersection.has_value() || !prevIntersection.has_value())
            return mat4{1.0f};

        // find the angular difference
        vec2 currProj = normalize(gizPl.ProjectPoint(currIntersection.value()));
        vec2 prevProj = normalize(gizPl.ProjectPoint(prevIntersection.value()));

        float da =
                acos(glm::min(dot(currProj, prevProj), 1.0f)) *            // clamp to avoid NaN
                sign(cross(vec3{prevProj, 0.0f}, vec3{currProj, 0.0f}).z); // CCW -> positive, CW -> negative

        mat4 t = rotate(mat4{1.0f}, da, vec3{localAxis});

        Tm::_transformation *= t;

        // draw some visual feedback for mouse interaction
        if (_firstTouch) {
            _firstTouchAngle = atan2(currProj.y, currProj.x);
            _firstTouch = false;
        }
        _cumulatedAngle += da;
        vec4 color = _axisData[_selectedAxis.value()].color;
        vec4 lineColor = _axisData[_selectedAxis.value()].color;
        color.a = 0.55f;
        lineColor.a = 0.85f;

        // it's called immediate, but it isn't...bad naming? bug?
        DrawImmediateArc(gizPl.Transformation(), length(_axisData[_selectedAxis.value()].transform[0]),
                         _firstTouchAngle, _firstTouchAngle + _cumulatedAngle, color, lineColor);

        return t;
    }

    void TransformManipulator::TmRotate::CameraChanged(const mat4 &viewMatrix, const mat4 &, const vec2 &) {
        // Rotation handles are represented as half circles. This method
        // ensures that each half circle tries to face the view direction.

        // While dragging (_selectedAxis has value) handles are
        // full circles, no need to make them face the camera.
        if (!Tm::_enabled || _selectedAxis.has_value())
            return;

        vec4 axisX = Tm::_transformation * vec4{1.0f, 0.0f, 0.0f, 0.0f};
        vec4 axisY = Tm::_transformation * vec4{0.0f, 1.0f, 0.0f, 0.0f};
        vec4 axisZ = Tm::_transformation * vec4{0.0f, 0.0f, 1.0f, 0.0f};

        Plane gizmoPlaneZY{vec3{0.0f}, axisZ, axisY, axisX};
        Plane gizmoPlaneXZ{vec3{0.0f}, axisX, axisZ, axisY};
        Plane gizmoPlaneXY{vec3{0.0f}, axisX, axisY, axisZ};

        vec3 eyePos = vec3{glm::inverse(viewMatrix) * vec4{0.0f, 0.0f, 0.0f, 1.0f}};
        vec3 gizmoCenter = vec3{Tm::_transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f}};

        vec3 viewVec = normalize(eyePos - gizmoCenter);

        vec2 fwd2DX = normalize(gizmoPlaneZY.ProjectPoint(viewVec));
        vec2 fwd2DY = normalize(gizmoPlaneXZ.ProjectPoint(viewVec));
        vec2 fwd2DZ = normalize(gizmoPlaneXY.ProjectPoint(viewVec));

        float angleX = atan2(fwd2DX.y, fwd2DX.x);
        float angleY = atan2(fwd2DY.y, fwd2DY.x);
        float angleZ = atan2(fwd2DZ.y, fwd2DZ.x);

        _axisData[TmAxis::AxisX].transform =
                GetDefaultAxisTransform(TmAxis::AxisX) * rotate(mat4{1.0f}, angleX, vec3{0.0f, 0.0f, 1.0f});
        _axisData[TmAxis::AxisY].transform =
                GetDefaultAxisTransform(TmAxis::AxisY) * rotate(mat4{1.0f}, angleY, vec3{0.0f, 0.0f, 1.0f});
        _axisData[TmAxis::AxisZ].transform =
                GetDefaultAxisTransform(TmAxis::AxisZ) * rotate(mat4{1.0f}, angleZ, vec3{0.0f, 0.0f, 1.0f});

        SetInstancesProperties(true, true);
    }

    void TransformManipulator::TmRotate::SetInstancesProperties(
        TransformManipulator::TmRotate::TmAxis axis, bool visible, bool selectable)
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

    void TransformManipulator::TmRotate::SetInstancesProperties(bool visible, bool selectable) {
        SetInstancesProperties(AxisX, visible, selectable);
        SetInstancesProperties(AxisY, visible, selectable);
        SetInstancesProperties(AxisZ, visible, selectable);
    }

    void
    TransformManipulator::TmRotate::SetAxisProperties(TransformManipulator::TmRotate::TmAxis axis,
                                                                 optional <vec4> color) {
        TmAxisData &axisData = _axisData[axis];

        axisData.color = color.has_value() ? color.value() : axisData.defaultColor;

        _gr->SetGizmoInstances(_gizmoId,
                               {
                                   make_pair(axisData.id,
                                             gizmo_instance_descriptor
                                                 {
                                                     .transform = Tm::_transformation * axisData.transform,
                                                     .color = axisData.color,
                                                     .visible = true,
                                                     .selectable = true,
                                                 })
                               });
    }

    TransformManipulator::TmRotate::TmAxis
    TransformManipulator::TmRotate::GetAxisFromId(tao_gizmos::gizmo_instance_id id) {
        for (const auto &a: _axisData) {
            if (a.second.id == id || a.second.idForSelection == id)
                return a.first;
        }

        throw runtime_error("Invalid gizmo instance id. Call Tm::HasGizmo before calling this method.");
    }

    glm::mat4 TransformManipulator::TmRotate::GetDefaultAxisTransform(
            TransformManipulator::TmRotate::TmAxis axis) {
        switch (axis) {
            case (TmAxis::AxisX) :
                return rotate(mat4{1.0}, -0.5f * pi<float>(), vec3{0.0, 1.0, 0.0});
            case (TmAxis::AxisY) :
                return rotate(mat4{1.0}, 0.5f * pi<float>(), vec3{1.0, 0.0, 0.0});
            case (TmAxis::AxisZ) :
                return rotate(mat4{1.0}, 0.0f * pi<float>(), vec3{1.0, 0.0, 0.0});
            default:
                return mat4{1.0f};
        }
    }

    std::vector<tao_gizmos::LineGizmoVertex>
    TransformManipulator::TmRotate::GetCircleVertices(float startAngle, float endAngle) {
        const int kCircPtsCount = 30;

        vector<LineGizmoVertex> circPts(kCircPtsCount);
        const float da = (endAngle - startAngle) / (kCircPtsCount - 1);

        for (int i = 0; i < kCircPtsCount; i++) {
            circPts[i] = LineGizmoVertex()
                    .Position(vec3{cos(startAngle + i * da), sin(startAngle + i * da), 0.0f})
                    .Color(vec4{1.0f});
        }

        return circPts;
    }

    TransformManipulator::TmScale::TmScale(GizmosRenderer &gr, RenderLayer &opaqueLayer,
                                                      RenderLayer &blendLayer) {
        /// Geometry creation
        //////////////////////////////////////////////////////////////////////////////
        auto cubeMesh = tao_geometry::Mesh::Box(kCubeSize, kCubeSize, kCubeSize);

        vector<vec3> cubeMeshPositions = cubeMesh.GetPositions();
        vector<vec3> cubeMeshNormals = cubeMesh.GetNormals();
        vector<int> cubeMeshTriangles = cubeMesh.GetIndices();

        vector<MeshGizmoVertex> cubeMeshVertices(cubeMeshPositions.size());
        for (int i = 0; i < cubeMeshVertices.size(); i++) {
            cubeMeshVertices[i]
                    .Position(cubeMeshPositions[i] - vec3{kCubeSize * 0.5f} + vec3(0.0, 0.0, kLineStart + kLineLength))
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
        auto cubeGizKey = _gr->CreateMeshGizmo(mesh_gizmo_descriptor{
            .vertices = cubeMeshVertices,
            .triangles = &cubeMeshTriangles,
            .zoom_invariant = true,
            .zoom_invariant_scale = 0.1f,
            .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static
        });

        auto lineGizKey = _gr->CreateLineGizmo(line_list_gizmo_descriptor{
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
        auto immediateLineGizKey = _gr->CreateLineGizmo(line_list_gizmo_descriptor{
            .vertices = lineVertices,
            .line_size = kLineSize,
            .zoom_invariant = false,
            .pattern_texture_descriptor = nullptr,
            .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
        });

        _immediateLineGizmoInstanceId = _gr->InstanceLineGizmo(immediateLineGizKey,
                                                               {
                                                                   gizmo_instance_descriptor{
                                                                       .transform = mat4{1.0f}, // world space coords
                                                                       .color = vec4{1.0f},
                                                                       .visible = false,
                                                                       .selectable = false
                                                                   }
                                                               })[0];
        // ---------------------------------------------------------------------------


        mat4
            transformX = rotate(mat4{1.0}, 0.5f * pi<float>(), vec3{0.0, 1.0, 0.0}),
            transformY = rotate(mat4{1.0}, -0.5f * pi<float>(), vec3{1.0, 0.0, 0.0}),
            transformZ = rotate(mat4{1.0}, 0.0f * pi<float>(), vec3{1.0, 0.0, 0.0});

        vector<gizmo_instance_descriptor> instances =
            {
                /* X Axis */
                gizmo_instance_descriptor{.transform = transformX, .color = kAxisColorX, .visible = false, .selectable = false},
                /* Y Axis */
                gizmo_instance_descriptor{.transform = transformY, .color = kAxisColorY, .visible = false, .selectable = false},
                /* Z Axis */
                gizmo_instance_descriptor{.transform = transformZ, .color = kAxisColorZ, .visible = false, .selectable = false}
            };

        auto allCubeKeys = _gr->InstanceMeshGizmo(cubeGizKey, instances);
        auto allLineKeys = _gr->InstanceLineGizmo(lineGizKey, instances);

        _meshGizmoId = cubeGizKey;
        _lineGizmoId = lineGizKey;
        _immediateLineGizmoId = immediateLineGizKey;

        TmAxisData axisDataX
            {
                .meshInstanceId = allCubeKeys[0],
                .lineInstanceId = allLineKeys[0],
                .defaultTransform = instances[0].transform,
                .meshTransform = instances[0].transform,
                .lineTransform = instances[0].transform,
                .color = instances[0].color,
                .defaultColor = instances[0].color,
            };
        _axisData.insert(make_pair(AxisX, axisDataX));

        TmAxisData axisDataY
            {
                .meshInstanceId = allCubeKeys[1],
                .lineInstanceId = allLineKeys[1],
                .defaultTransform = instances[1].transform,
                .meshTransform = instances[1].transform,
                .lineTransform = instances[1].transform,
                .color = instances[1].color,
                .defaultColor = instances[1].color,
            };
        _axisData.insert(make_pair(AxisY, axisDataY));

        TmAxisData axisDataZ
            {
                .meshInstanceId = allCubeKeys[2],
                .lineInstanceId = allLineKeys[2],
                .defaultTransform = instances[2].transform,
                .meshTransform = instances[2].transform,
                .lineTransform = instances[2].transform,
                .color = instances[2].color,
                .defaultColor = instances[2].color,
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

    bool TransformManipulator::TmScale::HasGizmo(gizmo_instance_id id) const {
        for (const auto &a: _axisData) {
            if (a.second.meshInstanceId == id ||
                a.second.lineInstanceId == id)
                return true;
        }
        return false;
    }

    void TransformManipulator::TmScale::HoverEnter(gizmo_instance_id id) {
        auto hoveredAxis = GetAxisFromId(id);

        SetAxisProperties(hoveredAxis, nullopt, _axisData[hoveredAxis].defaultColor * 2.5f, true, true, true, false);

    }

    void TransformManipulator::TmScale::HoverExit(gizmo_instance_id id) {
        auto hoveredAxis = GetAxisFromId(id);
        SetAxisProperties(hoveredAxis, _axisData[hoveredAxis].defaultTransform, _axisData[hoveredAxis].defaultColor,
                          true, true, true, false);
    }

    void TransformManipulator::TmScale::Enable(const mat4 &transformation) {
        Tm::Enable(transformation);

        SetAxisProperties(AxisX, _axisData[AxisX].defaultTransform, _axisData[AxisX].defaultColor, true, true, true,
                          false);
        SetAxisProperties(AxisY, _axisData[AxisY].defaultTransform, _axisData[AxisY].defaultColor, true, true, true,
                          false);
        SetAxisProperties(AxisZ, _axisData[AxisZ].defaultTransform, _axisData[AxisZ].defaultColor, true, true, true,
                          false);
    }

    void TransformManipulator::TmScale::Disable() {
        ResetAxisProperties(AxisX);
        ResetAxisProperties(AxisY);
        ResetAxisProperties(AxisZ);

        Tm::Disable();
    }

    void TransformManipulator::TmScale::DragEnter(gizmo_instance_id id) {
        const vec4 kGhostColor{0.65f};

        _selectedAxis = GetAxisFromId(id);

        // Hide the non - selected axis
        SetAxisProperties(AxisX, nullopt, nullopt, _selectedAxis.value() == AxisX, false,
                          _selectedAxis.value() == AxisX, false);
        SetAxisProperties(AxisY, nullopt, nullopt, _selectedAxis.value() == AxisY, false,
                          _selectedAxis.value() == AxisY, false);
        SetAxisProperties(AxisZ, nullopt, nullopt, _selectedAxis.value() == AxisZ, false,
                          _selectedAxis.value() == AxisZ, false);

        // Hide the selected axis line (will be drawn with the "immediate mode line")
        SetAxisProperties(_selectedAxis.value(), nullopt, nullopt, false, false, false, false);

        SetImmediateLineVisibility(true);

    }

    void TransformManipulator::TmScale::DragExit() {
        _selectedAxis = nullopt;

        SetAxisProperties(AxisX, _axisData[AxisX].defaultTransform, _axisData[AxisX].defaultColor, true, true, true,
                          false);
        SetAxisProperties(AxisY, _axisData[AxisY].defaultTransform, _axisData[AxisY].defaultColor, true, true, true,
                          false);
        SetAxisProperties(AxisZ, _axisData[AxisZ].defaultTransform, _axisData[AxisZ].defaultColor, true, true, true,
                          false);

        SetImmediateLineVisibility(false);
    }

    mat4 TransformManipulator::TmScale::Drag(float x, float y, float prevX, float prevY, int w, int h) {
        vec3 localAxis;
        switch (_selectedAxis.value()) {
            case (AxisX):
                localAxis = vec3{1.0, 0.0, 0.0};
                break;
            case (AxisY):
                localAxis = vec3{0.0, 1.0, 0.0};
                break;
            case (AxisZ):
                localAxis = vec3{0.0, 0.0, 1.0};
                break;
        }

        vec3 prevMousePos;
        vec3 currMousePos;
        vec3 worldOrigin = Tm::_transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f};

        TranslateOnAxis(x, y, prevX, prevY, w, h, localAxis, currMousePos, prevMousePos);

        float sc = abs(dot(currMousePos - worldOrigin, localAxis) / dot(prevMousePos - worldOrigin, localAxis));

        // Only positive scale
        mat4 t = glm::scale(mat4{1.0f}, vec3{1.0f} + localAxis * (sc - 1.0f));

        // curr and prev mouse pos are world space positions.
        // Here we need to find the translation (curr-pos) and
        // bring that in "axis/local" space.
        mat4 invTmTransf = glm::inverse(Tm::_transformation);
        mat4 tr = translate(mat4{1.0f},
                            vec3{invTmTransf * vec4{currMousePos, 1.0f} - invTmTransf * vec4{prevMousePos, 1.0f}});

        SetAxisProperties(_selectedAxis.value(), tr * _axisData[_selectedAxis.value()].meshTransform, nullopt, true,
                          true, false, false);

        mat4 zTr = _gr->GetZoomInvariantTransformation(_axisData[_selectedAxis.value()].meshInstanceId);
        vec3 start = Tm::_transformation * _axisData[_selectedAxis.value()].defaultTransform * zTr *
                     vec4{0.0f, 0.0f, kLineStart, 1.0f};
        vec3 end = Tm::_transformation * _axisData[_selectedAxis.value()].meshTransform * zTr *
                   vec4{0.0f, 0.0f, kLineStart + kLineLength, 1.0f};

        SetImmediateLinePosition(start, end, _axisData[_selectedAxis.value()].color);

        return t;
    }

    void TransformManipulator::TmScale::SetImmediateLineVisibility(bool visible) {
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

    void TransformManipulator::TmScale::SetImmediateLinePosition(const vec3 &start, const vec3 &end,
                                                                            const vec4 &color) {
        _gr->SetLineGizmoVertices(_immediateLineGizmoId,
                                  {
                                      LineGizmoVertex{}.Position(start).Color(color),
                                      LineGizmoVertex{}.Position(end).Color(color),
                                  }
        );
    }

    void
    TransformManipulator::TmScale::SetAxisProperties(TransformManipulator::TmScale::TmAxis axis,
                                                                optional <mat4> transformation, optional <vec4> color,
                                                                optional<bool> meshVisible,
                                                                optional<bool> meshSelectable,
                                                                optional<bool> lineVisible,
                                                                optional<bool> lineSelectable) {
        TmAxisData &axisData = _axisData[axis];

        axisData.meshTransform = transformation ? transformation.value() : axisData.meshTransform;
        axisData.lineTransform = transformation ? transformation.value() : axisData.lineTransform;
        axisData.color = color ? color.value() : axisData.color;

        /// Mesh gizmo properties (cube)
        ///////////////////////////////////////
        _gr->SetGizmoInstances(_meshGizmoId,
                               {
                                   make_pair(axisData.meshInstanceId,
                                             gizmo_instance_descriptor{
                                                 .transform    = Tm::_transformation * axisData.meshTransform,
                                                 .color        = axisData.color,
                                                 .visible      = meshVisible.has_value() &&
                                                                 meshVisible.value(),
                                                 .selectable   = meshSelectable.has_value() &&
                                                                 meshVisible.value(),
                                             })
                               });

        /// Line gizmo properties
        ///////////////////////////////////////
        _gr->SetGizmoInstances(_lineGizmoId,
                               {
                                   make_pair(axisData.lineInstanceId,
                                             gizmo_instance_descriptor{
                                                 .transform     = Tm::_transformation * axisData.lineTransform,
                                                 .color         = axisData.color,
                                                 .visible       = lineVisible.has_value() &&
                                                                  lineVisible.value(),
                                                 .selectable    = lineSelectable.has_value() &&
                                                                  lineSelectable.value(),
                                             })
                               });
    }

    void
    TransformManipulator::TmScale::ResetAxisProperties(
            TransformManipulator::TmScale::TmAxis axis) {
        SetAxisProperties(axis, nullopt, nullopt, nullopt, nullopt, nullopt, nullopt);
    }

    void
    TransformManipulator::TmScale::TranslateOnAxis(float x, float y, float prevX, float prevY, int w, int h,
                                                              vec3 localAxis, vec3 &currentPos, vec3 &prevPos) {
        mat4 viewMatrix;
        mat4 projMatrix;
        vec2 nearFar;
        _gr->GetView(viewMatrix, projMatrix, nearFar);

        glm::mat4 viewProjMatrix = projMatrix * viewMatrix;

        // Find the world position of the
        // triad origin and axis
        vec3 worldAxis = _transformation * vec4{localAxis, 0.0f};
        vec3 worldOrigin = _transformation * vec4{0.0f, 0.0f, 0.0f, 1.0f};


        // find the 2D closest point from the mouse
        // position to the projected axis
        vec4 l0 = viewProjMatrix * vec4{worldOrigin, 1.0};
        vec4 l1 = viewProjMatrix * vec4{worldOrigin + worldAxis, 1.0};
        vec2 l02d = l0;
        l02d /= l0.w;
        vec2 l12d = l1;
        l12d /= l1.w;

        vec2 closestPoint2D0 = PointLineClosestPoint(vec2((prevX / w) * 2.0 - 1.0, (prevY / h) * 2.0 - 1.0), l02d, l12d);
        vec2 closestPoint2D1 = PointLineClosestPoint(vec2((x / w) * 2.0 - 1.0, (y / h) * 2.0 - 1.0), l02d, l12d);

        // find the world position of the 2D point computed above
        // by projecting onto the 3D axis
        vec4 ndc0 = vec4{closestPoint2D0, 0.0, 1.0f};
        vec4 ndc1 = vec4{closestPoint2D1, 0.0, 1.0f};
        mat4 viewProjInverse = glm::inverse(viewProjMatrix);
        vec4 pw0 = viewProjInverse * ndc0;
        vec4 pw1 = viewProjInverse * ndc1;
        vec4 ow = glm::inverse(viewMatrix) * vec4{0.0, 0.0, 0.0, 1.0};

        vec3 rayDir0 = normalize(pw0 / pw0.w - ow);
        vec3 intersection0 = RayLineClosestPoint(ow, rayDir0, worldOrigin, worldAxis);

        vec3 rayDir1 = normalize(pw1 / pw1.w - ow);
        vec3 intersection1 = RayLineClosestPoint(ow, rayDir1, worldOrigin, worldAxis);

        currentPos = intersection1;
        prevPos = intersection0;

    }

    TransformManipulator::TmScale::TmAxis
    TransformManipulator::TmScale::GetAxisFromId(gizmo_instance_id id) {
        for (const auto &a: _axisData) {
            if (a.second.meshInstanceId == id || a.second.lineInstanceId == id)
                return a.first;
        }

        throw runtime_error("Invalid gizmo instance id. Call Tm::HasGizmo before calling this method.");
    }

    LightGizmos::LightGizmos(GizmosRenderer *renderer, GizmoPickAgent *inputAgent)
            : _renderer{renderer}, _inputAgent{inputAgent} {

        _selectionCallback = bind(&LightGizmos::OnSelection, this, placeholders::_1);
        _inputAgent->SubscribeToSelection(&_selectionCallback);

        /// Sphere light gizmo
        _sphereLightGizmoData._iconGizmoId = CreateSphereLightIconGizmo(renderer);
        _sphereLightGizmoData._lightGizmoId = CreateSphereLightAreaGizmo(renderer);

        /// Directional light gizmo
        _directionalLightGizmoData._iconGizmoId = CreateDirectionalLightIconGizmo(renderer);
        _directionalLightGizmoData._lightGizmoId = CreateDirectionalLightDirectionGizmo(renderer);

        /// Rect light gizmo
        _rectLightGizmoData._iconGizmoId = CreateRectLightIconGizmo(renderer);
        _rectLightGizmoData._lightGizmoId = CreateRectLightDirectionGizmo(renderer);

        auto layer = renderer->CreateRenderLayer(DEPTH_LESS_EQUAL_NO_WRITE, BLEND_ON_TOP, RASTERIZER_MS);

        renderer->AssignGizmoToLayers(_sphereLightGizmoData._lightGizmoId, {layer});
        renderer->AssignGizmoToLayers(_sphereLightGizmoData._iconGizmoId, {layer});
        renderer->AssignGizmoToLayers(_directionalLightGizmoData._lightGizmoId, {layer});
        renderer->AssignGizmoToLayers(_directionalLightGizmoData._iconGizmoId, {layer});
        renderer->AssignGizmoToLayers(_rectLightGizmoData._iconGizmoId, {layer});
        renderer->AssignGizmoToLayers(_rectLightGizmoData._lightGizmoId, {layer});

        renderer->AddRenderPass({renderer->CreateRenderPass({layer})});
    }

    weak_ptr <LightGizmos::SphereLightGizmo> LightGizmos::CreateLightGizmo(const tao_pbr::SphereLight &light) {
        gizmo_instance_descriptor lightInstanceDescriptor = GetSphereLightInstanceDesc(light.transformation.matrix(),
                                                                                       light.radius, vec4{1.0f});

        _sphereLightGizmoData._sphereLightsProperties.push_back(SphereLightGizmoProperties{
            .transformation = light.transformation.matrix(),
            .radius = light.radius,
            .color = vec4{1.0f}
        });

        _sphereLightGizmoData._iconInstances.push_back(lightInstanceDescriptor);
        _sphereLightGizmoData._lightInstances.push_back(lightInstanceDescriptor);

        _sphereLightGizmoData._iconInstanceIds = _renderer->InstancePointGizmo(_sphereLightGizmoData._iconGizmoId,
                                                                               _sphereLightGizmoData._iconInstances);

        _sphereLightGizmoData._lightInstanceIds = _renderer->InstanceLineStripGizmo(_sphereLightGizmoData._lightGizmoId,
                                                                                    _sphereLightGizmoData._lightInstances);

        _sphereLightGizmoData._sphereLights.push_back(
                make_shared<SphereLightGizmo>(*this, _sphereLightGizmoData._lightInstances.size() - 1));

        return _sphereLightGizmoData._sphereLights.back();
    }

    void
    LightGizmos::SetSphereLightGizmoProperties(int index, const mat4 &transform, float radius,
                                                          const vec4 &color) {
        _sphereLightGizmoData._sphereLightsProperties[index].transformation = transform;
        _sphereLightGizmoData._sphereLightsProperties[index].radius = radius;
        _sphereLightGizmoData._sphereLightsProperties[index].color = color;

        gizmo_instance_descriptor lightInstanceDescriptor = GetSphereLightInstanceDesc(transform, radius, color);
        _sphereLightGizmoData._lightInstances[index] = lightInstanceDescriptor;
        _sphereLightGizmoData._iconInstances[index] = lightInstanceDescriptor;

        // TODO: batch update
        _renderer->SetGizmoInstances(_sphereLightGizmoData._iconGizmoId,
                                     {make_pair(_sphereLightGizmoData._iconInstanceIds[index],
                                                lightInstanceDescriptor)});
        _renderer->SetGizmoInstances(_sphereLightGizmoData._lightGizmoId,
                                     {make_pair(_sphereLightGizmoData._lightInstanceIds[index],
                                                lightInstanceDescriptor)});
    }

    void LightGizmos::UpdateView(const mat4 &viewMatrix) {
        vec3 eyePos = vec3{inverse(viewMatrix) * vec4{0.0, 0.0, 0.0, 1.0}};

        vector<pair<gizmo_instance_id, gizmo_instance_descriptor>> instances(
                _sphereLightGizmoData._lightInstances.size());

        for (int i = 0; i < instances.size(); i++) {
            float rad = _sphereLightGizmoData._sphereLightsProperties[i].radius;

            gizmo_instance_descriptor desc = _sphereLightGizmoData._lightInstances[i];
            vec3 lightPos = desc.transform[3];

            // Make a circle on the XY plane always face the camera
            vec3 up = normalize(cross(lightPos, eyePos));
            vec3 z = normalize(eyePos - lightPos);
            vec3 x = normalize(cross(z, up));
            vec3 y = cross(z, x);

            mat4 tr = mat4{vec4{x, 0.0}, vec4{y, 0.0}, vec4{z, 0.0}, vec4{lightPos, 1.0}};

            desc.transform = tr * scale(mat4{1.0f}, vec3{rad});

            instances[i] = make_pair(_sphereLightGizmoData._lightInstanceIds[i], desc);
        }

        _renderer->SetGizmoInstances(_sphereLightGizmoData._lightGizmoId, instances);
    }

    weak_ptr <LightGizmos::DirectionalLightGizmo> LightGizmos::CreateLightGizmo(const tao_pbr::DirectionalLight &light) {
        gizmo_instance_descriptor lightInstanceDescriptor = GetDirectionalLightInstanceDesc(
                light.transformation.matrix(), vec4{1.0f});

        _directionalLightGizmoData._directionalLightsProperties.push_back(DirectionalLightGizmoProperties{
            .transformation = light.transformation.matrix(),
            .color = vec4{1.0f}
        });

        _directionalLightGizmoData._iconInstances.push_back(lightInstanceDescriptor);
        _directionalLightGizmoData._lightInstances.push_back(lightInstanceDescriptor);

        _directionalLightGizmoData._iconInstanceIds = _renderer->InstancePointGizmo(
                _directionalLightGizmoData._iconGizmoId, _directionalLightGizmoData._iconInstances);
        _directionalLightGizmoData._lightInstanceIds = _renderer->InstanceLineGizmo(
                _directionalLightGizmoData._lightGizmoId, _directionalLightGizmoData._lightInstances);

        _directionalLightGizmoData._directionalLights.push_back(
                make_shared<DirectionalLightGizmo>(*this, _directionalLightGizmoData._lightInstances.size() - 1));

        return _directionalLightGizmoData._directionalLights.back();
    }

    void
    LightGizmos::SetDirectionalLightGizmoProperties(int index, const mat4 &transform, const vec4 &color) {
        _directionalLightGizmoData._directionalLightsProperties[index].transformation = transform;
        _directionalLightGizmoData._directionalLightsProperties[index].color = color;

        gizmo_instance_descriptor lightInstanceDescriptor = GetDirectionalLightInstanceDesc(transform, color);
        _directionalLightGizmoData._lightInstances[index] = lightInstanceDescriptor;
        _directionalLightGizmoData._iconInstances[index] = lightInstanceDescriptor;

        // TODO: batch update
        _renderer->SetGizmoInstances(_directionalLightGizmoData._iconGizmoId,
                                     {make_pair(_directionalLightGizmoData._iconInstanceIds[index],
                                                lightInstanceDescriptor)});
        _renderer->SetGizmoInstances(_directionalLightGizmoData._lightGizmoId,
                                     {make_pair(_directionalLightGizmoData._lightInstanceIds[index],
                                                lightInstanceDescriptor)});
    }

    weak_ptr <LightGizmos::RectLightGizmo> LightGizmos::CreateLightGizmo(const tao_pbr::RectLight &light) {
        gizmo_instance_descriptor lightInstanceDescriptor = GetRectLightInstanceDesc(light.transformation.matrix(),
                                                                                     light.size, vec4{1.0f});

        _rectLightGizmoData._rectLightsProperties.push_back(RectLightGizmoProperties{
            .transformation = light.transformation.matrix(),
            .size = light.size,
            .color = vec4{1.0f}
        });

        _rectLightGizmoData._iconInstances.push_back(lightInstanceDescriptor);
        _rectLightGizmoData._lightInstances.push_back(lightInstanceDescriptor);

        _rectLightGizmoData._iconInstanceIds = _renderer->InstancePointGizmo(_rectLightGizmoData._iconGizmoId,
                                                                             _rectLightGizmoData._iconInstances);
        _rectLightGizmoData._lightInstanceIds = _renderer->InstanceLineStripGizmo(_rectLightGizmoData._lightGizmoId,
                                                                                  _rectLightGizmoData._lightInstances);

        _rectLightGizmoData._rectLights.push_back(
                make_shared<RectLightGizmo>(*this, _rectLightGizmoData._lightInstances.size() - 1));

        return _rectLightGizmoData._rectLights.back();
    }

    void LightGizmos::SetRectLightGizmoProperties(int index, const mat4 &transform, const vec2 &size,
                                                             const vec4 &color) {
        _rectLightGizmoData._rectLightsProperties[index].transformation = transform;
        _rectLightGizmoData._rectLightsProperties[index].size = size;
        _rectLightGizmoData._rectLightsProperties[index].color = color;

        gizmo_instance_descriptor lightInstanceDescriptor = GetRectLightInstanceDesc(transform, size, color);
        _rectLightGizmoData._lightInstances[index] = lightInstanceDescriptor;
        _rectLightGizmoData._iconInstances[index] = lightInstanceDescriptor;

        // TODO: batch update
        _renderer->SetGizmoInstances(_rectLightGizmoData._iconGizmoId,
                                     {make_pair(_rectLightGizmoData._iconInstanceIds[index], lightInstanceDescriptor)});
        _renderer->SetGizmoInstances(_rectLightGizmoData._lightGizmoId,
                                     {make_pair(_rectLightGizmoData._lightInstanceIds[index],
                                                lightInstanceDescriptor)});
    }

    void LightGizmos::SubscribeToSelectionEvents(const function<void(LightGizmos::anyLightPtr)> &onSelectionEnter,
                                                            const function<void(LightGizmos::anyLightPtr)> &onSelectionExit) {
        _clientLightSelectedCallbacks.push_back(&onSelectionEnter);
        _clientLightDeselectedCallbacks.push_back(&onSelectionExit);
    }

    gizmo_instance_descriptor
    LightGizmos::GetSphereLightInstanceDesc(const mat4 &transform, float radius, const vec4 &color)
    {
        return
            gizmo_instance_descriptor{
                .transform = transform * scale(mat4{1.0f}, vec3{radius}),
                .color     = color,
                .visible   = true,
                .selectable= true
            };
    }

    gizmo_instance_descriptor
    LightGizmos::GetDirectionalLightInstanceDesc(const mat4 &transform, const vec4 &color)
    {
        return
            gizmo_instance_descriptor{
                .transform = transform,
                .color     = color,
                .visible   = true,
                .selectable= true
            };
    }

    gizmo_instance_descriptor
    LightGizmos::GetRectLightInstanceDesc(const mat4 &transform, const vec2 &size, const vec4 &color)
    {
        return
            gizmo_instance_descriptor{
                .transform = transform * glm::scale(mat4{1.0f}, vec3{size, 1.0f}),
                .color     = color,
                .visible   = true,
                .selectable= true
            };
    }

    gizmo_id LightGizmos::CreateSphereLightIconGizmo(GizmosRenderer *gr) {
        PointGizmoVertex v;
        v.Position(vec3{0.0f})
                .Color(vec4{1.0f})
                .TexCoordStart(vec2{0.0f})
                .TexCoordEnd(vec2{1.0f});

        std::vector<PointGizmoVertex> vertices = {v};

        /// Light Symbol SDF
        ///////////////////////////////////////////////////////////////////////////////////
        float h_2 = kGizmoSize * 0.50f;
        float h_3 = kGizmoSize * 0.33f;
        float h_4 = kGizmoSize * 0.25f;
        float h_5 = kGizmoSize * 0.20f;
        float h_6 = kGizmoSize * 0.16f;
        float h_7 = kGizmoSize * 0.14f;
        float h_9 = kGizmoSize * 0.11f;

        auto bodyLow =
            SdfTrapezoid{h_5, h_4, h_3}
                .Translate(glm::vec2{0.0, -0.5 * h_3});

        auto body =
            SdfCircle<float>{vec3{0.0f}, h_4}
                .Add(bodyLow)
                .Translate(glm::vec2{h_2, h_2});

        auto base0 =
            SdfSegment{vec2{-h_5 * 0.5, 0.0}, vec2{h_5 * 0.5, 0.0}}
                .Translate(vec2{0.0, -h_3 - 3.0f})
                .Inflate(1.0f);

        auto base =
            SdfCircle{vec2{0.0, 0.0}, 0.0f}
                .Translate(vec2{0.0, -h_3 - 3.0f})
                .Inflate(h_5 * 0.5f)
                .Translate(glm::vec2{h_2, h_2})
                .Subtract(body.Translate(vec2{0.0f, -3.0f}));

        auto contour = body
            .Shell(0.75f)
            .Add(base);

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kGizmoSize, kGizmoSize, {0, 0, 0, 0}};

        tex.FillWithFunction([&body, &contour](unsigned int x, unsigned int y) {
            float u = static_cast<float>(x);
            float v = static_cast<float>(y);
            vec2 uv{u, v};

            float valContour = glm::clamp(glm::mix(1.0f, 0.0f, contour.Evaluate(uv) * 2.0f), 0.0f, 1.0f);
            float valBody = glm::clamp(glm::mix(1.0f, 0.0f, body.Evaluate(uv) * 2.0f), 0.0f, 1.0f) * 0.3f;
            float val = glm::clamp((valBody + valContour), 0.0f, 1.0f);

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
                .point_half_size=kGizmoSize / 2,
                .snap_to_pixel = false,
                .vertices = vertices,
                .zoom_invariant = false,
                .symbol_atlas_descriptor = &texDesc,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
            };

        return gr->CreatePointGizmo(descriptor);
    }

    gizmo_id LightGizmos::CreateSphereLightAreaGizmo(GizmosRenderer *gr) {
        constexpr int kLineWidth = 2;
        constexpr float kOpacity = 0.6f;
        constexpr int kCircleSubd = 61;
        constexpr float kCircDa = pi<float>() * 2.0f / kCircleSubd;

        // Geometry - circle
        // ----------------------------------------------------------------------------------
        vector<LineGizmoVertex> vertices(kCircleSubd + 1);
        for (int i = 0; i <= kCircleSubd; i++) {
            LineGizmoVertex v{};
            v.Position({cos(kCircDa * i), sin(kCircDa * i), 0.0f})
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
    }

    gizmo_id LightGizmos::CreateDirectionalLightIconGizmo(GizmosRenderer *gr) {
        PointGizmoVertex v;
        v.Position(vec3{0.0f})
                .Color(vec4{1.0f})
                .TexCoordStart(vec2{0.0f})
                .TexCoordEnd(vec2{1.0f});

        std::vector<PointGizmoVertex> vertices = {v};

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kGizmoSize, kGizmoSize, {0, 0, 0, 0}};

        /// Light Symbol SDF
        ///////////////////////////////////////////////////////////////////////////////////
        auto sdfBody = SdfCircle{vec2{0.0f}, kGizmoSize * 0.25f};
        auto sdfBodyContour = sdfBody.Shell(0.75f);

        auto sdfRays =
                SdfSegment{vec2{kGizmoSize * 0.30f, 0.0f}, vec2{kGizmoSize * 0.45f, 0.0f}}
                        .RepeatRadial(8)
                        .Inflate(1.55f)
                        .Subtract(sdfBody.Inflate(5.0f));

        auto sdfContour =
                sdfRays
                        .Add(sdfBodyContour)
                        .Translate(vec2{kGizmoSize * 0.5f});

        tex.FillWithFunction([&sdfContour, &sdfBody](unsigned int x, unsigned int y) {
            float u = static_cast<float>(x);
            float v = static_cast<float>(y);
            vec2 uv{u, v};

            float valContour = sdfContour.Evaluate(uv);
            float valBody = sdfBody.Translate(vec2{kGizmoSize * 0.5f}).Evaluate(uv);

            valContour = glm::clamp(glm::mix(1.0f, 0.0f, valContour * 2.0f), 0.0f, 1.0f);
            valBody = glm::clamp(glm::mix(1.0f, 0.0f, valBody * 2.0f), 0.0f, 1.0f) * 0.3f;
            float val = glm::clamp((valBody + valContour), 0.0f, 1.0f);

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
                .point_half_size=kGizmoSize / 2,
                .snap_to_pixel = false,
                .vertices = vertices,
                .zoom_invariant = false,
                .symbol_atlas_descriptor = &texDesc,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
            };

        return gr->CreatePointGizmo(descriptor);
    }

    gizmo_id LightGizmos::CreateDirectionalLightDirectionGizmo(GizmosRenderer *gr) {
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
            SdfSegment{vec2{kPatternLen * 0.33333f, 0.0f}, vec2{kPatternLen * 0.66666f, 0.0f}}
                .Inflate(kLineWidth * 0.40f)
                .Translate(vec2{0.0f, kLineWidth * 0.5f});

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kPatternLen, kLineWidth, {0, 0, 0, 0}};
        tex.FillWithFunction([&sdfPattern](unsigned int x, unsigned int y)
                             {
                                 float u = static_cast<float>(x);
                                 float v = static_cast<float>(y);
                                 vec2 uv{u, v};

                                 float val = glm::clamp(glm::mix(1.0f, 0.0f, sdfPattern.Evaluate(uv) * 2.0f), 0.0f,
                                                        1.0f);

                                 return vec<4, unsigned char>{255, 255, 255, val * 255};
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
    }

    gizmo_id LightGizmos::CreateRectLightIconGizmo(GizmosRenderer *gr) {
        PointGizmoVertex v;
        v.Position(vec3{0.0f})
                .Color(vec4{1.0f})
                .TexCoordStart(vec2{0.0f})
                .TexCoordEnd(vec2{1.0f});

        std::vector<PointGizmoVertex> vertices = {v};

        tao_gizmos_procedural::TextureDataRgbaUnsignedByte tex{kGizmoSize, kGizmoSize, {255, 255, 255, 255}};

        /// Light Symbol SDF
        ///////////////////////////////////////////////////////////////////////////////////
        float h_2 = kGizmoSize * 0.5f;
        float h_5 = kGizmoSize * 0.2f;
        float h_4 = kGizmoSize * 0.25f;
        float h_8 = kGizmoSize * 0.125f;
        float h_10 = kGizmoSize * 0.1f;
        float h_20 = kGizmoSize * 0.05f;
        float rw = kGizmoSize * 0.8f;
        float rh = kGizmoSize * 0.6f;
        float radius = kGizmoSize * 0.1f;

        auto sdfBody =
            SdfRoundedRect{vec2{rw, rh}, vec4{radius, 0.0f, radius, 0.0f}}
                .Translate(vec2{0.0f, (rw - rh) * 0.5f});

        auto sdfMask =
            SdfRoundedRect{vec2{rw, rh}, vec4{radius, 0.0f, radius, 0.0f}}
                .Scale(vec2{0.8f})
                .Translate(vec2{0.0f, (rw - rh) * 0.5f});

        auto sdfBase =
            SdfRoundedRect{vec2{rw, rw}, vec4{radius}}
                .Subtract(sdfBody)
                .Subtract(sdfBody.Scale(vec2{2.0f, 1.0f}).Translate(vec2{0.0f, -h_10}));

        auto sdfLEDs =
            SdfRect{vec2(h_20)}
                .Translate(vec2{h_5 * 0.5f})
                .RepeatGrid(vec2{h_5})
                .Translate(vec2{h_5 * 0.5f})
                .Intersect(sdfMask);

        auto sdf =
            sdfBody
                .Shell(0.6f)
                .Add(sdfLEDs)
                .Add(sdfBase)
                .Translate(vec2{h_2});

        auto sdfBodyTr = sdfBody.Translate(vec2{h_2});

        tex.FillWithFunction([&sdf, &sdfBodyTr](unsigned int x, unsigned int y) {
            float u = static_cast<float>(x);
            float v = static_cast<float>(y);
            vec2 uv{u, v};

            float valContour = 1.0f - glm::clamp(sdf.Evaluate(uv), 0.0f, 1.0f);
            float valBody = (1.0f - glm::clamp(sdfBodyTr.Evaluate(uv), 0.0f, 1.0f)) * 0.3f;

            float val = glm::clamp(valContour + valBody, 0.0f, 1.0f);

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
                .point_half_size=kGizmoSize / 2,
                .snap_to_pixel = false,
                .vertices = vertices,
                .zoom_invariant = false,
                .symbol_atlas_descriptor = &texDesc,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
            };

        return gr->CreatePointGizmo(descriptor);
    }

    gizmo_id LightGizmos::CreateRectLightDirectionGizmo(GizmosRenderer *gr) {
        constexpr int kLineWidth = 2;
        constexpr float kOpacity = 0.6f;

        // Geometry - line strip
        // ----------------------------------------------------------------------------------
        vector<LineGizmoVertex> vertices
            ({
                 LineGizmoVertex{}.Position(vec3{-0.5f, -0.5f, 0.0f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity}),
                 LineGizmoVertex{}.Position(vec3{-0.5f, 0.5f, 0.0f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity}),
                 LineGizmoVertex{}.Position(vec3{0.5f, 0.5f, 0.0f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity}),
                 LineGizmoVertex{}.Position(vec3{0.5f, -0.5f, 0.0f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity}),
                 LineGizmoVertex{}.Position(vec3{-0.5f, -0.5f, 0.0f}).Color(vec4{1.0f, 1.0f, 1.0f, kOpacity})
             });

        line_strip_gizmo_descriptor descriptor
            {
                .vertices = vertices,
                .isLoop = true,
                .line_size = kLineWidth,
                .zoom_invariant = false,
                .pattern_texture_descriptor = nullptr,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_static,
            };

        return gr->CreateLineStripGizmo(descriptor);
    }

    optional<LightGizmos::lightIndex> LightGizmos::GetLightFromInstanceId(gizmo_instance_id id) {
        optional<lightIndex> res = nullopt;

        // sphere lights
        for (int i = 0; i < _sphereLightGizmoData._iconInstanceIds.size(); i++) {
            if (_sphereLightGizmoData._iconInstanceIds[i] == id) {
                res = lightIndex{.type = sphere, .index = i};
                break;
            }
        }

        // directional lights
        for (int i = 0; i < _directionalLightGizmoData._iconInstanceIds.size(); i++) {
            if (_directionalLightGizmoData._iconInstanceIds[i] == id) {
                res = lightIndex{.type = directional, .index = i};
                break;
            }
        }

        // rect lights
        for (int i = 0; i < _rectLightGizmoData._iconInstanceIds.size(); i++) {
            if (_rectLightGizmoData._iconInstanceIds[i] == id) {
                res = lightIndex{.type = rect, .index = i};
                break;
            }
        }

        return res;
    }

    LightGizmos::anyLightPtr LightGizmos::GetLightPtrFromIndex(LightGizmos::lightIndex index) {
        anyLightPtr l;

        if (index.type == sphere) l = _sphereLightGizmoData._sphereLights[index.index];
        else if (index.type == directional) l = _directionalLightGizmoData._directionalLights[index.index];
        else if (index.type == rect) l = _rectLightGizmoData._rectLights[index.index];

        return l;

    }

    gizmo_instance_id LightGizmos::GetInstanceIdFromIndex(LightGizmos::lightIndex index) {
        gizmo_instance_id id;

        if (index.type == sphere) id = _sphereLightGizmoData._iconInstanceIds[index.index];
        else if (index.type == directional) id = _directionalLightGizmoData._iconInstanceIds[index.index];
        else if (index.type == rect) id = _rectLightGizmoData._iconInstanceIds[index.index];

        return id;
    }

    void LightGizmos::OnSelection(std::optional<gizmo_instance_id> selectedItem) {
        if ((!selectedItem.has_value() && !_latestSelectedLight) ||
            (selectedItem.has_value() && _latestSelectedLight &&
             selectedItem == GetInstanceIdFromIndex(_latestSelectedLight.value())))
            return;

        if (_latestSelectedLight && !selectedItem) {
            auto l = GetLightPtrFromIndex(_latestSelectedLight.value());

            for (auto *cbk: _clientLightDeselectedCallbacks)
                if (cbk) (*cbk)(l);

            _latestSelectedLight = nullopt;
        }

        if (selectedItem) {
            optional<lightIndex> selectedLightIndex = GetLightFromInstanceId(selectedItem.value());

            if (selectedLightIndex.has_value()) {
                auto selectedLight = GetLightPtrFromIndex(selectedLightIndex.value());

                if (_latestSelectedLight.has_value()) {
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

    void
    LightGizmos::SphereLightGizmo::SetProperties(const mat4 &transform, float radius, const vec4 &color) {
        _parent->SetSphereLightGizmoProperties(_index, transform, radius, color);
    }

    void LightGizmos::DirectionalLightGizmo::SetProperties(const mat4 &transform, const vec4 &color) {
        _parent->SetDirectionalLightGizmoProperties(_index, transform, color);
    }

    void
    LightGizmos::RectLightGizmo::SetProperties(const mat4 &transform, const vec2 &size, const vec4 &color) {
        _parent->SetRectLightGizmoProperties(_index, transform, size, color);
    }

    GizmoGrid::GizmoGrid(GizmosRenderer &gr) :
            _gr{&gr} {
        /// Creating the Mesh gizmo (custom shader)
        vector<MeshGizmoVertex> dummyVerts{MeshGizmoVertex().Position(vec3(0.0f))};
        mesh_gizmo_descriptor descriptor
            {
                .vertices =dummyVerts,
                .triangles = nullptr,
                .zoom_invariant = false,
                .usage_hint = tao_gizmos::gizmo_usage_hint::usage_dynamic
            };

        auto customShader = CreateGridShader();
        _gizmoId = _gr->CreateMeshGizmo(descriptor, customShader);
        _instanceId = _gr->InstanceMeshGizmo(_gizmoId, {
            {
                .transform = mat4{1.0f},
                .color = vec4{1.0f},
                .visible = true,
                .selectable = false
            }
        })[0];

        /// Creating the rendering layer and pass
        const ogl_depth_state kOglDepthState
            {
                .depth_test_enable = true,
                .depth_write_enable = false,
                .depth_func = depth_func_less_equal,
                .depth_range_near = 0.0f,
                .depth_range_far = 1.0f,
            };
        const ogl_blend_state kOglBlendState
            {
                .blend_enable = true,
                .blend_equation_rgb = ogl_blend_equation{.blend_factor_src = blend_fac_src_alpha, .blend_factor_dst = blend_fac_one_minus_src_alpha},
                .blend_equation_alpha = ogl_blend_equation{.blend_factor_src = blend_fac_one, .blend_factor_dst = blend_fac_one_minus_src_alpha},
                .color_mask = mask_all
            };
        const ogl_rasterizer_state kOglRasterizerState
            {
                .culling_enable = false,
                .polygon_mode = polygon_mode_fill,
                .multisample_enable = false,
                .alpha_to_coverage_enable = false,
            };

        auto layer = _gr->CreateRenderLayer(kOglDepthState, kOglBlendState, kOglRasterizerState);
        _gr->AddRenderPass({_gr->CreateRenderPass({layer})});
        _gr->AssignGizmoToLayers(_gizmoId, {layer});
    }

    void GizmoGrid::UpdateView(const mat4 &viewMatrix, const mat4 &projMatrix, const vec2 &nearFar) {
        UpdateMeshGizmoVertices(viewMatrix, projMatrix, nearFar);
    }

    tao_gizmos_shader_graph::SGOutColorAndDepth GizmoGrid::CreateGridShader() {
        // from: https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/
        // with some minor modifications

        auto vertCol = make_shared<SGInVertColor>();
        auto vertNrm = make_shared<SGInVertNormal>();
        auto kWidthG = make_shared<SGFloatConst>("kWidthGrid", 0.5f);
        auto kWidthT = make_shared<SGFloatConst>("kWidthThick", 0.5f);
        auto kWidthA = make_shared<SGFloatConst>("kWidthAxis", 0.5f);
        auto kScale = make_shared<SGFloatConst>("KScale", 1.0f);
        auto kScaleT = make_shared<SGFloatConst>("KScaleT", 0.1f);
        auto kColor0 = make_shared<SGVec3Const>("kColor0", 0.5f, 0.5f, 0.5f);
        auto kColor1 = make_shared<SGVec3Const>("kColor1", 0.5f, 0.5f, 0.5f);
        auto kColorAxisX = make_shared<SGVec3Const>("kColorAxisX", 0.8f, 0.1f, 0.1f);
        auto kColorAxisY = make_shared<SGVec3Const>("kColorAxisY", 0.1f, 0.8f, 0.1f);

        // near projection is stored as vert Color
        // far  projection is stored as vert Normal
        auto nearPt = SGVec(SGSwizzleX(vertCol), SGSwizzleY(vertCol), SGSwizzleZ(vertCol));
        auto farPt = SGVec(SGSwizzleX(vertNrm), SGSwizzleY(vertNrm), SGSwizzleZ(vertNrm));

        auto t = SGSwizzleZ(nearPt) / (SGSwizzleZ(nearPt) - SGSwizzleZ(farPt));

        auto fragPosWrd = nearPt + (SGVec(t, t, t) * (farPt - nearPt));

        auto coord = SGVec(SGSwizzleX(fragPosWrd), SGSwizzleY(fragPosWrd)) * SGVec(kScale, kScale);
        auto derivative = SGFWidth(coord);
        auto fractPattern = SGAbs(SGFract(coord));
        auto grid = SGMin(SGConstVec(1.0f, 1.0f) - fractPattern, fractPattern) / derivative;

        // Drawing a thicker line for each 10 normal lines
        auto coordT = SGVec(SGSwizzleX(fragPosWrd), SGSwizzleY(fragPosWrd)) * SGVec(kScaleT, kScaleT);
        auto derivativeT = SGFWidth(coordT);
        auto fractPatternT = SGAbs(SGFract(coordT));
        auto gridT = SGMin(SGConstVec(1.0f, 1.0f) - fractPatternT, fractPatternT) / derivativeT;

        auto line = SGMin(SGSwizzleX(grid), SGSwizzleY(grid));
        auto lineT = SGMin(SGSwizzleX(gridT), SGSwizzleY(gridT));
        auto xAxis = SGAbs(SGSwizzleX(coord)) / SGSwizzleX(derivative);
        auto yAxis = SGAbs(SGSwizzleY(coord)) / SGSwizzleY(derivative);

        auto colorLineA =
                SGConstVec(0.2f) * (SGConstVec(1.0f) - SGClamp(line - kWidthG, SGConstVec(0.0f), SGConstVec(1.0f)));
        auto colorLine = SGMultiply(colorLineA, kColor0);
        auto colorLineTA =
                SGConstVec(0.3f) * (SGConstVec(1.0f) - SGClamp(lineT - kWidthT, SGConstVec(0.0f), SGConstVec(1.0f)));
        auto colorLineT = SGMultiply(colorLineTA, kColor1);
        auto colorXAxisA =
                SGConstVec(0.5f) * (SGConstVec(1.0f) - SGClamp(xAxis - kWidthA, SGConstVec(0.0f), SGConstVec(1.0f)));
        auto colorXAxis = SGMultiply(colorXAxisA, kColorAxisX);
        auto colorYAxisA =
                SGConstVec(0.5f) * (SGConstVec(1.0f) - SGClamp(yAxis - kWidthA, SGConstVec(0.0f), SGConstVec(1.0f)));
        auto colorYAxis = SGMultiply(colorYAxisA, kColorAxisY);

        // blend the colors together
        auto color = colorXAxis;
        auto colorA = colorXAxisA;

        color = color + SGMultiply(SGConstVec(1.0f) - colorA, colorYAxis);
        colorA = colorA + SGMultiply(SGConstVec(1.0f) - colorA, colorYAxisA);
        color = color + SGMultiply(SGConstVec(1.0f) - colorA, colorLineT);
        colorA = colorA + SGMultiply(SGConstVec(1.0f) - colorA, colorLineTA);
        color = color + SGMultiply(SGConstVec(1.0f) - colorA, colorLine);
        colorA = colorA + SGMultiply(SGConstVec(1.0f) - colorA, colorLineA);


        // Fade the grid based on dot(viewDir, grid normal)
        auto viewDirNrm = SGNorm(make_shared<SGInEyePosition>() - fragPosWrd);
        auto fade = SGConstVec(1.0f) -
                    ((SGConstVec(1.0f) - SGDot(viewDirNrm, SGConstVec(0.0f, 0.0f, 1.0f)) - SGConstVec(0.8f)) /
                     SGConstVec(0.2f));
        fade = SGClamp(fade, SGConstVec(0.0f), SGConstVec(1.0f));

        auto color4 = SGVec(SGSwizzleX(color) / colorA, SGSwizzleY(color) / colorA, SGSwizzleZ(color) / colorA,
                            colorA * fade);

        // manually compute depth as viewProj * world position
        auto ndc = (make_shared<SGInProjectionMatrix>() * make_shared<SGInViewMatrix>()) *
                   SGVec(SGSwizzleX(fragPosWrd), SGSwizzleY(fragPosWrd), SGSwizzleZ(fragPosWrd), SGConstVec(1.0f));

        auto outDepth = SGSwizzleZ(ndc) / SGSwizzleW(ndc);
        auto outCol = SGBranch(SGGreater(t, SGConstVec(0.0f)), color4, SGConstVec(0.0f, 0.0f, 0.0f, 0.0f));

        auto out = SGOutColorAndDepth(outCol, outDepth * SGConstVec(0.5f) + SGConstVec(0.5f));

        //ParseShaderGraphTGF(out, "c://Users/Admin/Downloads/Graph.tgf");
        return out;
    }

    void
    GizmoGrid::UpdateMeshGizmoVertices(const mat4 &viewMatrix, const mat4 &projMatrix, const vec2 &nearFar) {
        // see: https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/
        //
        // since custom vertex shaders are not supported we'll use
        // vertex color and normal to pass near and far projections

        // world space coords of a full screen quad on the
        // near camera plane.
        const int kCnt = 6;
        const vec2 kNdc[]
                {
                        vec2{-1.f, -1.f}, vec2{1.f, 1.f}, vec2{-1.f, 1.f},
                        vec2{-1.f, -1.f}, vec2{1.f, -1.f}, vec2{1.f, 1.f}
                };

        mat4 invViewProj = glm::inverse(projMatrix * viewMatrix);

        vector<MeshGizmoVertex> verts(kCnt);
        for (int i = 0; i < kCnt; i++) {
            vec4 pos = invViewProj * vec4{kNdc[i], 0.0f, 1.0f};
            pos /= pos.w;

            vec4 nearProj = invViewProj * vec4{kNdc[i], -1.0f, 1.0f};
            nearProj /= nearProj.w;

            vec4 farProj = invViewProj * vec4{kNdc[i], 1.0f, 1.0f};
            farProj /= farProj.w;

            verts[i]
                    .Position(pos)
                    .Color(nearProj)
                    .Normal(farProj);
        }

        _gr->SetMeshGizmoVertices(_gizmoId, verts, nullptr);
    }

    void TaoScene::LoadGltf(const char *path){

        LoadGltf(*_pbrRenderer, path);

    }

    void TaoScene::LoadHDRIs(PbrRenderer &renderer) {
        auto dirPath = std::filesystem::path{HDRI_DIR};

        struct stat sb;

        for(const auto& f : std::filesystem::directory_iterator{dirPath})
        {
            std::filesystem::path fullPath = f.path();
            std::string fileName = fullPath.filename().string();
            std::string fullPathStr = fullPath.string();

            if (stat(fullPathStr.c_str(), &sb) == 0 && !(sb.st_mode & S_IFDIR))
            {
                auto envKey = renderer.AddEnvironmentTexture(fullPathStr.c_str());
                _environmentLights.push_back({fileName, envKey});
                renderer.SetCurrentEnvironment(envKey);
                _currentEnvironment = _environmentLights.size()-1;
            }
        }
    }

    size_t TaoScene::EnvironmentsCount() const {
        return _environmentLights.size();
    }

    void TaoScene::SetEnvironment(size_t idx) {
        if(idx>=EnvironmentsCount())
            throw runtime_error("Invalid environment index.");

        _pbrRenderer->SetCurrentEnvironment(_environmentLights[idx].second);
        _currentEnvironment = idx;
    }

    const string& TaoScene::GetEnvironmentName(size_t idx) const{
        if(idx>=EnvironmentsCount())
            throw runtime_error("Invalid environment index.");

        return _environmentLights[idx].first;
    }

    size_t TaoScene::GetCurrentEnvironment() const {
        return _currentEnvironment;
    }

    glm::mat4 TaoScene::GetMat4(const aiMatrix4x4 &aiMat) {
        return mat4
            {
                aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
                aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
                aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
                aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
            };
    }

    void TaoScene::LoadAiNode(PbrRenderer &renderer, const aiScene *scene, const vector <GenKey<Mesh>> &pbrMeshes,
                              const vector <GenKey<PbrMaterial>> &pbrMaterials, const aiNode *node, const mat4 &accTransform,
                              const GenKey <PbrMaterial> &defaultMat) {
        mat4 currTransform = GetMat4(node->mTransformation);
        mat4 transform = accTransform * currTransform;

        // if node has meshes, create a new scene object for it
        if( node->mNumMeshes > 0)
        {
            for(int i=0;i<node->mNumMeshes; i++)
            {
                Transformation tr{transform};
                int meshIndex = node->mMeshes[i];
                int matIndex  = scene->mMeshes[meshIndex]->mMaterialIndex;

                renderer.AddMeshRenderer(tr, pbrMeshes[meshIndex], pbrMaterials[matIndex]);
            }
        }

        // continue for all child nodes
        for(int n=0;n<node->mNumChildren; n++)
        {
            LoadAiNode(renderer, scene, pbrMeshes, pbrMaterials, node->mChildren[n], transform, defaultMat);
        }
    }

    string TaoScene::GetTexName(const aiString &texName, const aiScene *scene) {
        aiString fileName = texName;
        if(texName.data[0]=='*') // embedded texture
            throw runtime_error("Embedded textures currently not supported.");

        return string{fileName.data};
    }

    Mesh TaoScene::LoadAiMesh(PbrRenderer &renderer, const aiMesh *mesh) {
        if(!mesh->HasPositions()||!mesh->HasNormals())
            throw runtime_error("Meshes without normals and/or positions are not supported.");

        vector<vec3> mPos(mesh->mNumVertices); // --- positions
        for(int i=0;i<mPos.size();i++) mPos[i] = vec3{mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};

        vector<int> mTri(mesh->mNumFaces * 3); // --- triangles
        for(int i=0;i < mesh->mNumFaces;i++)
        {
            const aiFace f = mesh->mFaces[i];

            if (f.mNumIndices != 3)
                throw runtime_error("Unsupported mesh face definition.");

            mTri[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
            mTri[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
            mTri[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
        }

        vector<vec3> mNrm(mesh->mNumVertices); // --- normals
        for(int i=0;i<mNrm.size();i++) mNrm[i] = vec3{mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};

        bool hasTex0 = mesh->HasTextureCoords(0);
        vector<vec2> mTex(mesh->mNumVertices); // --- texture coords (0)
        for(int i=0;i<mTex.size();i++)
        {
            mTex[i] = hasTex0
                      ? vec2{mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y}
                      : vec2{0.0f};
        }

        return Mesh{mPos, mNrm, mTex, mTri};
    }

    void TaoScene::LoadAiScene(PbrRenderer &renderer, const aiScene *scene, const string &rootDirName) {
        // --- Load meshes
        // maps 1:1 to scene.mMeshes
        vector<GenKey<Mesh>> myMeshes(scene->mNumMeshes);
        for(int i=0; i<scene->mNumMeshes; i++)
        {
            auto myMesh = LoadAiMesh(renderer, scene->mMeshes[i]);
            myMeshes[i] = renderer.AddMesh(myMesh);
        }

        // --- Load Textures
        map<string, GenKey<ImageTexture>> myTextures;

        // --- Load materials
        // maps 1:1 to scene.mMaterials
        vector<GenKey<PbrMaterial>> myMaterials(scene->mNumMaterials);
        for(int i=0; i<scene->mNumMaterials; i++)
        {
            auto myMat = LoadAiMaterial(renderer, myTextures, scene, scene->mMaterials[i], rootDirName);
            myMaterials[i] = renderer.AddMaterial(myMat);
        }

        // --- Load scene hierarchy
        GenKey<PbrMaterial> defaultMat = renderer.AddMaterial(PbrMaterial{0.5f, 0.0f, vec4{0.7f, 0.3f, 0.1f, 1.0f}});

        LoadAiNode(renderer, scene, myMeshes, myMaterials, scene->mRootNode, rotate(mat4{1.0f}, 0.5f*pi<float>(), vec3{1.0f, 0.0f, 0.0f}) /* from Y to Z up*/ , defaultMat);
    }

    void TaoScene::LoadGltf(PbrRenderer &renderer, const char *path) {
        // from:https://assimp-docs.readthedocs.io/en/latest/usage/use_the_lib.html
        // Create an instance of the Importer class
        Assimp::Importer importer;

        // And have it read the given file with some example postprocessing
        // Usually - if speed is not the most important aspect for you - you'll
        // probably to request more postprocessing than we do in this example.
        const aiScene* scene = importer.ReadFile( path,
                                                  aiProcess_FlipUVs |
                                                    aiProcess_Triangulate |
                                                    aiProcess_JoinIdenticalVertices |
                                                    aiProcess_SortByPType);

        // If the import failed, report it
        if (nullptr == scene) {
            throw runtime_error( importer.GetErrorString());
        }

        // Now we can access the file's contents.
        LoadAiScene( renderer, scene, std::filesystem::path{path}.parent_path().string());

        // We're done. Everything will be cleaned up by the importer destructor
    }

    PbrMaterial TaoScene::LoadAiMaterial(PbrRenderer &renderer, map<string, GenKey<ImageTexture>> &pbrTextures,
                                         const aiScene *scene, const aiMaterial *mat, const string &rootDirName) {
        aiColor3D diffuse;
        aiColor3D emission;
        float roughness;
        float metalness;

        mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
        mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission);
        mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        mat->Get(AI_MATKEY_METALLIC_FACTOR, metalness);

        // --- Load Textures
        bool hasDiffuseTex      = mat->GetTextureCount(aiTextureType_DIFFUSE);
        bool hasRoughnessTex    = mat->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS);
        bool hasMetalnessTex    = mat->GetTextureCount(aiTextureType_METALNESS);
        bool hasEmissionTex     = mat->GetTextureCount(aiTextureType_EMISSIVE);
        bool hasNormalTex       = mat->GetTextureCount(aiTextureType_NORMALS);
        bool hasOcclusionTex    = mat->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION);

        aiString diffuseTexName, roughnessTexName, metalnessTexName, emissionTexName, normalTexName, occlusionTexName;

        if(hasDiffuseTex)   { mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), diffuseTexName);                CreateTextureIfNeeded(renderer, pbrTextures, rootDirName, GetTexName(diffuseTexName, scene), true); }
        if(hasRoughnessTex) { mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE_ROUGHNESS, 0), roughnessTexName);    CreateTextureIfNeeded(renderer, pbrTextures, rootDirName, GetTexName(roughnessTexName, scene), false); }
        if(hasMetalnessTex) { mat->Get(AI_MATKEY_TEXTURE(aiTextureType_METALNESS, 0), metalnessTexName);            CreateTextureIfNeeded(renderer, pbrTextures, rootDirName, GetTexName(metalnessTexName, scene), false); }
        if(hasEmissionTex)  { mat->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), emissionTexName);              CreateTextureIfNeeded(renderer, pbrTextures, rootDirName, GetTexName(emissionTexName, scene), true); }
        if(hasNormalTex)    { mat->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), normalTexName);                 CreateTextureIfNeeded(renderer, pbrTextures, rootDirName, GetTexName(normalTexName, scene), false); }
        if(hasOcclusionTex) { mat->Get(AI_MATKEY_TEXTURE(aiTextureType_AMBIENT_OCCLUSION, 0), occlusionTexName);    CreateTextureIfNeeded(renderer, pbrTextures, rootDirName, GetTexName(occlusionTexName, scene), false); }

        pbr_material_descriptor descriptor
            {
                .diffuse        = vec3{diffuse.r, diffuse.g, diffuse.b},
                .diffuseTex     = hasDiffuseTex
                                  ? optional(pbrTextures.at(string(diffuseTexName.data)))
                                  : nullopt,
                .normalTex      = hasNormalTex
                                  ? optional(pbrTextures.at(string(normalTexName.data)))
                                  : nullopt,
                .roughness      = roughness,
                .roughnessTex   = hasRoughnessTex
                                  ? optional(pbrTextures.at(string(roughnessTexName.data)))
                                  : nullopt,
                .mergedMetalnessRoughness = true,
                .metalness      = metalness,
                .metalnessTex   = hasMetalnessTex
                                  ? optional(pbrTextures.at(string(metalnessTexName.data)))
                                  : nullopt,
                .emission       = vec3{emission.r, emission.g, emission.b},
                .emissionTex    = hasEmissionTex
                                  ? optional(pbrTextures.at(string(emissionTexName.data)))
                                  : nullopt,
                .occlusionTex   = hasOcclusionTex
                                  ? optional(pbrTextures.at(string(occlusionTexName.data)))
                                  : nullopt,
            };

        return PbrMaterial{descriptor};
    }

    void TaoScene::CreateTextureIfNeeded(PbrRenderer &renderer, map<string, GenKey<ImageTexture>> &pbrTextures,
                                         const string &rootDirName, const string &texName, bool srgb) {
        if(!pbrTextures.contains(texName))
        {
            ImageTexture i{std::format("{}/{}",rootDirName,texName), srgb};
            auto texKey = renderer.AddImageTexture(i);
            pbrTextures.insert({texName,  texKey});
        }
    }

    void TaoScene::InitCallbacks() {

        _resizeCallback = [this](auto && PH1, auto && PH2) { OnResize(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); };
        _lightSelectionEnterCallback = [this](auto && PH1) { OnLightSelectionEnter(std::forward<decltype(PH1)>(PH1)); };
        _lightSelectionExitCallback = [this](auto && PH1) { OnLightSelectionExit(std::forward<decltype(PH1)>(PH1)); };
        _tmUpdateCallback = [this](auto && PH1) { OnTmUpdate(std::forward<decltype(PH1)>(PH1)); };
    }

    void TaoScene::OnResize(int w, int h) {

        _renderContext->MakeCurrent();

        _renderContext->GetFramebufferSize(&_fboWidth, &_fboHeight);

        _windowCompositor->Resize(_fboWidth, _fboHeight);
        _gizmosRenderer  ->Resize(_fboWidth, _fboHeight);
        _pbrRenderer     ->Resize(_fboWidth, _fboHeight);
    }

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

        gizRdr.InstanceMeshGizmo(key, {
            gizmo_instance_descriptor{
                glm::scale(glm::mat4{1.0f}, vec3{0.98f}),
                glm::vec4{1.0f}}});

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
        auto edgesKey = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor{
            .vertices = edgeVertices,
            .line_size = edgeWidth,
            .zoom_invariant = false,
            .zoom_invariant_scale = zoomInvariantScale,
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

        auto dashedEdgesKey = gizRdr.CreateLineGizmo(line_list_gizmo_descriptor{
            .vertices = edgeVertices,
            .line_size = 2,
            .zoom_invariant = false,
            .zoom_invariant_scale = zoomInvariantScale,
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
                ),
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
                ),
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
                ),
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
                 gr.CreateRenderPass({myLayers.depthPrePassLayer}),
                 gr.CreateRenderPass({myLayers.blendGreaterLayer}),
                 gr.CreateRenderPass({myLayers.opaqueLayer}),
                 gr.CreateRenderPass({myLayers.blendEqualLayer})
             });

        return myLayers;

    }

    void TaoScene::InitGizmosVC() {
        _gizmosRendererVC = make_unique<GizmosRenderer>(*_renderContext, kFboWidthVC, kFboHeightVC );
        MyGizmoLayersVC layers = InitGizmosLayersAndPassesVC(*_gizmosRendererVC);
        InitViewCube(*_gizmosRendererVC, layers);
    }

    TaoScene::TaoScene(const TaoScene::tao_scene_config &config):
    _windowWidth{config.windowWidth},
    _windowHeight{config.windowHeight}
    {
        InitCallbacks();

        // --- Render Context
        _renderContext = make_unique<RenderContext>(static_cast<int>(_windowWidth), static_cast<int>(_windowHeight), "TaoApp");
        _renderContext ->GetFramebufferSize(&_fboWidth, &_fboHeight);
        _renderContext->MakeCurrent();
        _renderContext->SetResizeCallback(_resizeCallback);

        // --- Input manager
        _inputManager = make_unique<MouseInputManager>(_renderContext.get());

        // --- Camera input
        const vec3 kInitEyePos{-5.0f, -5.0f, 7.0f};
        const vec3 kInitEyeTgt{0.0f};
        const vec3 kInitEyeUp{0.0f, 0.0f, 1.0};
        _cameraInputAgent = make_unique<CameraInputAgent>
            (
                kInitEyePos, kInitEyeTgt, kInitEyeUp,
                mouse_input_modifier{.button = mouse_button::right, .key = left_shift}, // Zoom
                mouse_input_modifier{.button = mouse_button::right, .key = left_ctrl},   // Pan
                mouse_input_modifier{.button = mouse_button::right, .key = nullopt}  // Rotate
            );
        _inputManager->AddInputAgent(_cameraInputAgent.get());

        // --- Window compositor
        _windowCompositor = make_unique<WindowCompositor>(*_renderContext, _fboWidth, _fboHeight);

        // --- Gizmos renderer
        _gizmosRenderer = make_unique<GizmosRenderer>( *_renderContext, _fboWidth, _fboHeight );

        // --- Gizmos interaction
        _gizmoPickAgent= make_unique<GizmoPickAgent>(_gizmosRenderer.get());
        _inputManager->AddInputAgent(_gizmoPickAgent.get());

        // --- Transform manipulator gizmo
        _transformManipulator = make_unique<TransformManipulator>(*_gizmosRenderer,  *_gizmoPickAgent);

        // --- Grid gizmo
        _gridGizmo = make_unique<GizmoGrid>(*_gizmosRenderer);

        // --- Light gizmos
        _lightGizmo = make_unique<LightGizmos>(_gizmosRenderer.get(), _gizmoPickAgent.get());
        _lightGizmo->SubscribeToSelectionEvents(_lightSelectionEnterCallback, _lightSelectionExitCallback);

        // --- View Cube Gizmos Renderer
        InitGizmosVC();

        // --- Pbr Renderer
        _pbrRenderer = make_unique<PbrRenderer>(*_renderContext, _fboWidth, _fboHeight);

        LoadHDRIs(*_pbrRenderer);
    }

    void TaoScene::EndFrame() {

        _renderContext->SwapBuffers();
    }

    void TaoScene::NewFrame() {

        _renderContext->MakeCurrent();

        _inputManager->PollMouseEvents();

        // --- zoom and rotation
        _viewMatrix = _cameraInputAgent->GetViewMatrix();
        _projMatrix = glm::perspective(radians<float>(45), static_cast<float>(_fboWidth) / _fboHeight, _nearFar.x, _nearFar.y);

        // --- Pbr scene
        auto pbrOut = _pbrRenderer->Render(_viewMatrix, _projMatrix, _nearFar.x, _nearFar.y);

        // --- Update camera data for components that
        // --- requires it (billboarding)
        _lightGizmo          ->UpdateView(_viewMatrix);
        _gridGizmo           ->UpdateView(_viewMatrix, _projMatrix, _nearFar);
        _transformManipulator->UpdateCamera(_viewMatrix, _projMatrix, _nearFar);

        // --- Gizmo scene
        _gizmosRenderer->SetView(_viewMatrix, _projMatrix, _nearFar);
        _gizmosRenderer->SetDepthMask(*pbrOut._depthTexture);
        auto& gizOut = _gizmosRenderer->Render();

        // --- View cube gizmo
        mat4 viewMatrixVC = _viewMatrix;
        viewMatrixVC[3] = vec4{0.0, 0.0, -3.5, 1.0};
        mat4 projMatrixVC = glm::perspective(radians<float>(60), static_cast<float>(kFboWidthVC) / kFboHeightVC, 0.1f, 5.0f);
        _gizmosRendererVC->SetView(viewMatrixVC, projMatrixVC, vec2{0.1f, 3.0f});
        auto& vcGizOut = _gizmosRendererVC->Render();

        // --- Window compositing
        (*_windowCompositor)
        .AddLayer(*pbrOut._colorTexture , WindowCompositor::location{.x = 0, .y = 0, .width = _fboWidth, .height = _fboHeight}, WindowCompositor::blend_option::copy)
        .AddLayer(gizOut                , WindowCompositor::location{.x = 0, .y = 0, .width = _fboWidth, .height = _fboHeight}, WindowCompositor::blend_option::alpha_blend)
        .AddLayer(vcGizOut              , WindowCompositor::location{.x = _fboWidth - kFboWidthVC, .y = _fboHeight - kFboHeightVC, .width = kFboWidthVC, .height = kFboHeightVC}, WindowCompositor::blend_option::alpha_blend)
        .GetResult();

        _windowCompositor->ClearLayers();
    }

    template<typename LightType, typename LightGizmoType>
    void TaoScene::AddLight(const LightType& light, std::vector<MyLight<LightType, LightGizmoType>>& vec)
    {
        vec.push_back(
                MyLight<LightType, LightGizmoType>
                        {
                    .pbrLight = light,
                    .pbrLightKey = _pbrRenderer->AddLight(light),
                    .gizmoLightKey = _lightGizmo->CreateLightGizmo(light)
                        }
        );
    }

    void TaoScene::AddLight(const tao_pbr::DirectionalLight &light) {
        AddLight(light, _directionalLights);
    }
    void TaoScene::AddLight(const tao_pbr::SphereLight &light) {
        AddLight(light, _sphereLights);
    }
    void TaoScene::AddLight(const tao_pbr::RectLight &light) {
        AddLight(light, _rectLights);
    }

    void TaoScene::OnLightSelectionEnter(LightGizmos::anyLightPtr l) {

        if(holds_alternative<weak_ptr<LightGizmos::DirectionalLightGizmo>>(l))
            SelectLight(get<weak_ptr<LightGizmos::DirectionalLightGizmo>>(l), _directionalLights);

        else if(holds_alternative<weak_ptr<LightGizmos::SphereLightGizmo>>(l))
            SelectLight(get<weak_ptr<LightGizmos::SphereLightGizmo>>(l), _sphereLights);

        else if(holds_alternative<weak_ptr<LightGizmos::RectLightGizmo>>(l))
            SelectLight(get<weak_ptr<LightGizmos::RectLightGizmo>>(l), _rectLights);
    }

    void TaoScene::OnLightSelectionExit(LightGizmos::anyLightPtr) {

        _selectedLight = static_cast<MyDirectionalLight*>(nullptr);
        _transformManipulator->Disable();
    }

    void TaoScene::OnTmUpdate(const mat4 & t) {
        MyDirectionalLight* dirLight    = nullptr;
        MySphereLight*      sphereLight = nullptr;
        MyRectLight*        rectLight   = nullptr;

        if(holds_alternative<MySphereLight*>(_selectedLight))      sphereLight   = get<MySphereLight*>(_selectedLight);
        if(holds_alternative<MyDirectionalLight*>(_selectedLight)) dirLight      = get<MyDirectionalLight*>(_selectedLight);
        if(holds_alternative<MyRectLight*>(_selectedLight))        rectLight     = get<MyRectLight*>(_selectedLight);

        if(!dirLight && !sphereLight && !rectLight) return;

        // Use only position and rotation to update the
        // light's transformation.
        // Use scale to determine the radius.
        float scaleX = length(vec3{t[0]});
        float scaleY = length(vec3{t[1]});
        float scaleZ = length(vec3{t[2]});

        mat4 trNoScale = glm::scale(t, vec3{1.0/scaleX, 1.0/scaleY, 1.0/scaleZ});

        if(sphereLight)
        {
            sphereLight->pbrLight.transformation.Transform(trNoScale);
            sphereLight->pbrLight.radius *= scaleX * scaleY * scaleZ;

            _pbrRenderer->UpdateSphereLight(sphereLight->pbrLightKey, sphereLight->pbrLight);

            sphereLight->gizmoLightKey.lock()->SetProperties(sphereLight->pbrLight.transformation.matrix(),
                                                             sphereLight->pbrLight.radius, vec4{1.0f});
        }

        if(dirLight)
        {
            dirLight->pbrLight.transformation.Transform(trNoScale);
            _pbrRenderer->UpdateDirectionalLight(dirLight->pbrLightKey, dirLight->pbrLight);
            dirLight->gizmoLightKey.lock()->SetProperties(dirLight->pbrLight.transformation.matrix(), vec4{1.0f});
        }

        if(rectLight)
        {
            rectLight->pbrLight.transformation.Transform(trNoScale);
            rectLight->pbrLight.size *= vec2{scaleX, scaleY};

            _pbrRenderer->UpdateRectLight(rectLight->pbrLightKey, rectLight->pbrLight);
            rectLight->gizmoLightKey.lock()->SetProperties(rectLight->pbrLight.transformation.matrix(),
                                                           rectLight->pbrLight.size, vec4{1.0f});
        }
    }

    void TaoScene::SetTmMode(TransformManipulator::TmMode m) {
        _transformManipulator->SetMode(m);
    }

    TransformManipulator::TmMode TaoScene::GetCurrentTmMode() {
        return _transformManipulator->GetMode();
    }

    template<typename LightType, typename LightGizmoType>
    void TaoScene::SelectLight(weak_ptr<LightGizmoType> l, vector<MyLight<LightType, LightGizmoType>>& v)
    {

        for (auto &myL: v)
        {
            if (myL.gizmoLightKey.lock() == l.lock())
                _selectedLight = &myL;
        }

        const mat4& transform = get<MyLight<LightType, LightGizmoType>*>(_selectedLight)->pbrLight.transformation.matrix();

        _transformManipulator->Enable(transform, &_tmUpdateCallback);
    }
}