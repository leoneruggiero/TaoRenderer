
#ifndef TAOGL_TAOSCENE_H
#define TAOGL_TAOSCENE_H

#include <iostream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <chrono>
#include <thread>
#include <variant>
#include <regex>
#include <filesystem>
#include <sys/stat.h>

#include "GeometricPrimitives.h"
#include "RenderContext.h"
#include "GizmosRenderer.h"
#include "GizmosHelper.h"
#include "PbrRenderer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "TaOglAppConfig.h"

namespace tao_scene {

    enum MouseInputAgentTag {
        None = 0x0,
        Camera = 1u << 1
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
        left_shift = GLFW_KEY_LEFT_SHIFT,
        left_ctrl = GLFW_KEY_LEFT_CONTROL
    };

    struct mouse_input_modifier {
        std::optional<mouse_button> button;
        std::optional<key> key;
    };

    class MouseInputAgent {
    public:
        explicit MouseInputAgent(MouseInputAgentTag tag) :
                _tag{tag} {
        }

        virtual void MouseUp(float, float, int, int) = 0;
        virtual void MouseDown(float, float, int, int) = 0;
        virtual void MouseMove(float, float, int, int) = 0;
        virtual void Mute() = 0;
        virtual void UnMute() = 0;

        MouseInputAgentTag GetTag() const { return _tag; }

        void SetInputManager(MouseInputManager *manager) {
            _manager = manager;
        }

    private:
        MouseInputAgentTag _tag = MouseInputAgentTag::None;

    protected:
        MouseInputManager *_manager{};
    };

    class MouseInputManager {
    public:

        explicit MouseInputManager(tao_render_context::RenderContext *renderContext) :
                _rc{renderContext},
                //_mouseListeners{},
                _agents{},
                _muted{None} {
        }

        void PollMouseEvents();
        double GetTime();
        bool IsKeyPressed(key k);
        bool IsMouseButtonPressed(mouse_button b);
        void AddInputAgent(MouseInputAgent *agent);
        void MuteAgents(MouseInputAgentTag tag);
        void UnMuteAgents(MouseInputAgentTag tag);

    private:
        tao_render_context::RenderContext *_rc;
        //vector<shared_ptr<MouseInputListener>>  _mouseListeners;
        std::vector<MouseInputAgent *> _agents;
        MouseInputAgentTag _muted = None;
        std::map<mouse_button, bool> _buttons = {std::make_pair(mouse_button::left, false),
                                                 std::make_pair(mouse_button::middle, false),
                                                 std::make_pair(mouse_button::right, false)};

        void FireMouseUp        (mouse_button button, float x, float y, int w, int h);
        void FireMouseDown      (mouse_button button, float x, float y, int w, int h);
        void UpdateMouseButton  (mouse_button button, float x, float y, int w, int h);
    };

    class CameraInputAgent : public MouseInputAgent {
    public:
        CameraInputAgent(glm::vec3 initialEyePosition, glm::vec3 initialTarget, glm::vec3 up,
                         mouse_input_modifier zoomInputModifier, mouse_input_modifier panInputModifier,
                         mouse_input_modifier rotateInputModifier);

        glm::mat4 GetViewMatrix();

        void MouseUp(float x, float y, int w, int h) override;
        void MouseDown(float x, float y, int w, int h) override;
        void MouseMove(float x, float y, int w, int h) override;
        void Mute() override;
        void UnMute() override;

    private:
        enum camera_mode {
            zoom,
            pan,
            rotate
        };
        struct smooth_mouse_data {
            float _latestMouseX = 0.0f;
            float _latestMouseY = 0.0f;
            float _targetMouseX = 0.0f;
            float _targetMouseY = 0.0f;
            float _latestTimeMs = 0.0f;
        };

        bool _enabled = false;
        camera_mode _mode = rotate;
        std::map<camera_mode, smooth_mouse_data> _mouseData;
        std::map<camera_mode, mouse_input_modifier> _inputModifiers;
        glm::vec3 _eyePosition;
        glm::vec3 _target;
        glm::vec3 _up;

        static constexpr float _kDampHalfTimeMs = 50.0f;

        void ResetMouseData(camera_mode mode);
        void InitMouseData(camera_mode mode, float targetX, float targetY, float timeMs);
        void UpdateMouseTarget(camera_mode mode, float targetX, float targetY);
        void UpdateMouse(camera_mode mode, float timeMs, float &smoothDx, float &smoothDy);
        bool ShouldListen(camera_mode mode);

        void RotateCamera(float dx, float dy);
        void ZoomCamera(float dy);
        void PanCamera(float dx, float dy);
    };

    class GizmoPickAgent : public MouseInputAgent
    {
    public:
        explicit GizmoPickAgent(tao_gizmos::GizmosRenderer* gizmosRenderer);

        void MouseUp(float x, float y, int w, int h) override;
        void MouseDown(float x, float y, int w, int h) override;
        void MouseMove(float x, float y, int w, int h) override;
        void Mute() override{}
        void UnMute() override{}

        void SubscribeToMouseMove(const std::function<void(float, float, int, int)>* onMouseMove);
        void SubscribeToMouseDown(const std::function<void(float, float, int, int)>* onMouseDown);
        void SubscribeToMouseUp(const std::function<void(float, float, int, int)>* onMouseUp);
        void SubscribeToSelection(const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)>* onSelection);
        void SubscribeToHover(const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)>* onHover);

        void MuteCameraInput();
        void UnMuteCameraInput();

    private:
        tao_gizmos::GizmosRenderer* _gr = nullptr;
        bool _dragging = false;
        std::vector<const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)>*> _clientSelectionCallbacks;
        std::vector<const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)>*> _clientHoverCallbacks;
        std::vector<const std::function<void(float, float, int, int)>*> _clientMouseMoveCallbacks;
        std::vector<const std::function<void(float, float, int, int)>*> _clientMouseDownCallbacks;
        std::vector<const std::function<void(float, float, int, int)>*> _clientMouseUpCallbacks;
        std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)>  _selectionCallback;
        std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)>  _hoverCallback;

        bool IsClickButtonDown() const;
        bool IsAnyMouseButtonDown() const;
        void OnSelection(std::optional<tao_gizmos::gizmo_instance_id> selectedItem);
        void OnHover(std::optional<tao_gizmos::gizmo_instance_id> selectedItem);
    };

    class TransformManipulator
    {
    public:
        TransformManipulator(tao_gizmos::GizmosRenderer& gr, GizmoPickAgent& inputAgent);

        enum class TmMode
        {
            Translate,
            Rotate,
            Scale
        };

        void Enable(const glm::mat4& initialTransformation, const std::function<void(const glm::mat4&)>* transformChangedCallback);
        void Disable();
        void SetMode(TmMode mode);
        TmMode GetMode() const;
        void UpdateCamera(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec2& nearFar);

    private:
        class Tm
        {
        public:
            bool IsEnabled() const {return _enabled;}
            virtual bool HasGizmo(tao_gizmos::gizmo_instance_id id) const = 0;
            virtual void HoverEnter(tao_gizmos::gizmo_instance_id hoveredItem) = 0 ;
            virtual void HoverExit(tao_gizmos::gizmo_instance_id hoveredItem) = 0 ;
            virtual void Enable(const glm::mat4& transform){_enabled = true; _transformation = transform;}
            virtual void Disable(){_enabled = false;}
            virtual void DragEnter(tao_gizmos::gizmo_instance_id selectedItem) = 0 ;
            virtual void DragExit() = 0 ;
            virtual glm::mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) = 0 ;
            virtual void CameraChanged(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec2& nearFar) = 0;

        protected:
            bool _enabled = false;
            glm::mat4 _transformation = glm::mat4{1.0f};

        };

        class TmTranslate : public Tm
        {

        public:
            TmTranslate(){} // Ugly...however I need a parameterless constructor
            TmTranslate(tao_gizmos::GizmosRenderer& gr,
                        tao_gizmos::RenderLayer& depthPrePassLayer, // To achieve a "ghost" entity effect, we need a depth pre-pass
                        tao_gizmos::RenderLayer& colorBlendLayer);

            bool HasGizmo(tao_gizmos::gizmo_instance_id id) const override;

            void HoverEnter(tao_gizmos::gizmo_instance_id id) override;
            void HoverExit(tao_gizmos::gizmo_instance_id id) override;
            void Enable(const glm::mat4& transformation) override;
            void Disable() override;
            void DragEnter(tao_gizmos::gizmo_instance_id id) override;
            void DragExit() override;
            glm::mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) override;
            void CameraChanged(const glm::mat4&, const glm::mat4&, const glm::vec2&) override{}

        private:
            static constexpr float
                    kCylRadius  = 0.04f,
                    kCylLength  = 0.6f,
                    kConeRadius = 0.12f,
                    kConeLength = 0.4f;
            static constexpr int
                    kArrowSubdv = 16;
            static constexpr glm::vec4
                    kAxisColorX          = glm::vec4{0.85, 0.1, 0.1, 1.0},
                    kAxisColorY          = glm::vec4{0.1, 0.85, 0.1, 1.0},
                    kAxisColorZ          = glm::vec4{0.1, 0.1, 0.85, 1.0},
                    kAxisColorHighlightX = glm::vec4{0.9, 0.3, 0.3, 1.0},
                    kAxisColorHighlightY = glm::vec4{0.3, 0.9, 0.3, 1.0},
                    kAxisColorHighlightZ = glm::vec4{0.3, 0.3, 0.9, 1.0};

            enum TmAxis
            {
                AxisX,
                AxisY,
                AxisZ
            };
            struct TmAxisData
            {
                tao_gizmos::gizmo_instance_id id;
                glm::mat4 transform;
                glm::vec4 color;
                glm::vec4 defaultColor;
                glm::vec4 highlightColor;
            };

            tao_gizmos::GizmosRenderer* _gr = nullptr;
            tao_gizmos::gizmo_id _gizmoId;
            std::map<TmAxis, TmAxisData> _axisData;
            std::optional<TmAxis> _selectedAxis = std::nullopt;

            void SetAxisProperties(TmAxis axis,  std::optional<glm::mat4> transform, std::optional<glm::vec4> color, bool visible, bool selectable);
            float TranslateOnAxis(float x, float y, float prevX, float prevY, int w, int h, glm::vec3 localAxis);
            TmAxis GetAxisFromId(tao_gizmos::gizmo_instance_id id);
            void CreateArrowMesh(std::vector<tao_gizmos::MeshGizmoVertex>& verts, std::vector<int>& tris);

        };

        class TmRotate : public Tm
        {

        public:
            TmRotate(){} // Ugly...however I need a parameterless constructor
            TmRotate(tao_gizmos::GizmosRenderer& gr, tao_gizmos::RenderLayer& opaqueLayer, tao_gizmos::RenderLayer& transparentLayer);

            bool _firstTouch = true;
            float _firstTouchAngle = 0.0f;
            float _cumulatedAngle = 0.0f;
            tao_gizmos::gizmo_id _immediateMeshGizmoId;
            tao_gizmos::gizmo_instance_id _immediateMeshGizmoInstanceId;
            tao_gizmos::gizmo_id _immediateLineGizmoId;
            tao_gizmos::gizmo_instance_id _immediateLineGizmoInstanceId;

            void HideImmediateArc();
            void DrawImmediateArc(const glm::mat4& transformation, float radius, float startAngle, float endAngle, const glm::vec4& color, const glm::vec4& lineColor);

            bool HasGizmo(tao_gizmos::gizmo_instance_id id) const override;

            void HoverEnter(tao_gizmos::gizmo_instance_id id) override;
            void HoverExit(tao_gizmos::gizmo_instance_id id) override;
            void Enable(const glm::mat4& transformation) override;
            void Disable() override;
            void DragEnter(tao_gizmos::gizmo_instance_id id) override;
            void DragExit() override;
            glm::mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) override;
            void CameraChanged(const glm::mat4& viewMatrix, const glm::mat4&, const glm::vec2&) override;

        private:
            enum TmAxis
            {
                AxisX,
                AxisY,
                AxisZ
            };
            struct TmAxisData
            {
                tao_gizmos::gizmo_instance_id id;
                tao_gizmos::gizmo_instance_id idForSelection;
                glm::mat4 transform;
                glm::vec4 color;
                glm::vec4 defaultColor;
            };

            tao_gizmos::GizmosRenderer* _gr = nullptr;
            tao_gizmos::gizmo_id _gizmoId;
            tao_gizmos::gizmo_id _gizmoIdForSelection;
            std::map<TmAxis, TmAxisData> _axisData;
            std::optional<TmAxis> _selectedAxis = std::nullopt;

            void SetInstancesProperties(TmAxis axis, bool visible, bool selectable);
            void SetInstancesProperties(bool visible, bool selectable);
            void SetAxisProperties(TmAxis axis, std::optional<glm::vec4> color);
            TmAxis GetAxisFromId(tao_gizmos::gizmo_instance_id id);
            static glm::mat4 GetDefaultAxisTransform(TmAxis axis);
            std::vector<tao_gizmos::LineGizmoVertex> GetCircleVertices(float startAngle, float endAngle);
        };

        class TmScale : public Tm
        {

        public:
            TmScale(){} // Ugly...however I need a parameterless constructor
            TmScale(tao_gizmos::GizmosRenderer& gr,
                    tao_gizmos::RenderLayer& opaqueLayer,
                    tao_gizmos::RenderLayer& blendLayer);

            bool HasGizmo(tao_gizmos::gizmo_instance_id id) const override;

            void HoverEnter(tao_gizmos::gizmo_instance_id id) override;
            void HoverExit(tao_gizmos::gizmo_instance_id id) override;
            void Enable(const glm::mat4& transformation) override;
            void Disable() override;
            void DragEnter(tao_gizmos::gizmo_instance_id id) override;
            void DragExit() override;
            glm::mat4 Drag(float x, float y, float prevX, float prevY, int w, int h) override;
            void CameraChanged(const glm::mat4&, const glm::mat4&, const glm::vec2&) override{}

        private:

            constexpr static float
                    kCubeSize    = 0.20f,
                    kLineLength  = 0.80f,
                    kLineStart   = 0.30f;
            constexpr static int
                    kLineSize = 3;
            constexpr static glm::vec4
                    kAxisColorX = glm::vec4{0.75, 0.1, 0.1, 1.0},
                    kAxisColorY = glm::vec4{0.1, 0.75, 0.1, 1.0},
                    kAxisColorZ = glm::vec4{0.1, 0.1, 0.75, 1.0};

            enum TmAxis
            {
                AxisX,
                AxisY,
                AxisZ
            };
            struct TmAxisData
            {
                tao_gizmos::gizmo_instance_id meshInstanceId;
                tao_gizmos::gizmo_instance_id lineInstanceId;
                glm::mat4 defaultTransform;
                glm::mat4 meshTransform;
                glm::mat4 lineTransform;
                glm::vec4 color;
                glm::vec4 defaultColor;
            };

            tao_gizmos::GizmosRenderer* _gr = nullptr;
            tao_gizmos::gizmo_id _meshGizmoId;
            tao_gizmos::gizmo_id _lineGizmoId;
            std::map<TmAxis, TmAxisData> _axisData;
            std::optional<TmAxis> _selectedAxis = std::nullopt;

            tao_gizmos::gizmo_id _immediateLineGizmoId;
            tao_gizmos::gizmo_instance_id _immediateLineGizmoInstanceId;

            void SetImmediateLineVisibility(bool visible);
            void SetImmediateLinePosition(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
            void SetAxisProperties(
                    TmAxis axis,
                    std::optional<glm::mat4> transformation,
                    std::optional<glm::vec4> color,
                    std::optional<bool> meshVisible, std::optional<bool> meshSelectable,
                    std::optional<bool> lineVisible, std::optional<bool> lineSelectable);

            void ResetAxisProperties(TmAxis axis);
            void TranslateOnAxis(float x, float y, float prevX, float prevY, int w, int h, glm::vec3 localAxis, glm::vec3& currentPos, glm::vec3& prevPos);
            TmAxis GetAxisFromId(tao_gizmos::gizmo_instance_id id);
        };

        tao_gizmos::GizmosRenderer* _gr=nullptr;

        TmMode _currentMode = TmMode::Translate;
        std::map<TmMode, Tm*> _transformManipulators{};

        TmTranslate _tmTranslate;
        TmRotate    _tmRotate;
        TmScale     _tmScale;

        glm::mat4 _transformation = glm::mat4{1.0};
        bool _enabled = false;
        const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)> _selectionCallback{};
        const std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)> _hoverCallback{};
        const std::function<void(float, float, int, int)> _mouseMoveCallback{};
        const std::function<void(float, float, int, int)> _mouseUpCallback{};
        const std::function<void(float, float, int, int)> _mouseDownCallback{};
        const std::function<void(const glm::mat4&)>* _transformationChanged=nullptr;

        struct renderPasses
        {
            tao_gizmos::RenderLayer opaqueLayer;
            tao_gizmos::RenderPass opaquePass;

            tao_gizmos::RenderLayer blendLayer;
            tao_gizmos::RenderPass blendPass;
        };

        renderPasses CreateRenderLayersAndPasses();

        std::optional<tao_gizmos::gizmo_instance_id> _selectedItem = std::nullopt;
        std::optional<tao_gizmos::gizmo_instance_id> _hoveredItem = std::nullopt;

        float _latestX=0.0f;
        float _latestY=0.0f;
        bool  _hasMouseData=false;

        std::optional<glm::mat4> _currentTransformation = std::nullopt;
        bool _dragging = false;

        void OnSelectionEnter(tao_gizmos::gizmo_instance_id selectedItem);
        void OnSelectionExit(tao_gizmos::gizmo_instance_id selectedItem);
        void OnSelection(std::optional<tao_gizmos::gizmo_instance_id> newItem);
        void OnHoverEnter(tao_gizmos::gizmo_instance_id hoveredItem);
        void OnHoverExit(tao_gizmos::gizmo_instance_id hoveredItem);
        void OnHover(std::optional<tao_gizmos::gizmo_instance_id> newItem);

        void ResetInputData();
        void SetInputData(float x, float y);

        void OnDrag(float x, float y, float prevX, float prevY, int w, int h);
        void OnMouseDown(float x, float y, int w, int h);
        void OnMouseUp(float x, float y, int w, int h);
        void OnMouseMove(float x, float y, int w, int h);

        GizmoPickAgent& _inputAgent;
    };

    class LightGizmos
    {
    public:

        LightGizmos(tao_gizmos::GizmosRenderer* renderer, GizmoPickAgent* inputAgent);
        
        /// Sphere Light Gizmo
        //////////////////////////////////////////////////////////////////////////////////////////////
        struct SphereLightGizmoProperties
        {
            glm::mat4 transformation;
            float radius;
            glm::vec4 color;
        };
        class SphereLightGizmo
        {
            friend class LightGizmos;

        public:
            SphereLightGizmo(LightGizmos& parent,int index)
                    : _parent{&parent}, _index{index}
            {}

            void SetProperties(const glm::mat4& transform, float radius, const glm::vec4& color);

        private:
            // todo: weak_ptr?
            LightGizmos* _parent;
            int _index;
        };

        std::weak_ptr<SphereLightGizmo> CreateLightGizmo(const tao_pbr::SphereLight& light);

        void SetSphereLightGizmoProperties(int index, const glm::mat4& transform, float radius, const glm::vec4& color);

        void UpdateView(const glm::mat4& viewMatrix);

        /// Directional Light Gizmo
        //////////////////////////////////////////////////////////////////////////////////////////////
        struct DirectionalLightGizmoProperties
        {
            glm::mat4 transformation;
            glm::vec4 color;
        };
        class DirectionalLightGizmo
        {
            friend class LightGizmos;

        public:
            DirectionalLightGizmo(LightGizmos& parent,int index)
                    : _parent{&parent}, _index{index}
            {}

            void SetProperties(const glm::mat4& transform, const glm::vec4& color);

        private:
            // todo: weak_ptr?
            LightGizmos* _parent;
            int _index;
        };

        std::weak_ptr<DirectionalLightGizmo> CreateLightGizmo(const tao_pbr::DirectionalLight& light);

        void SetDirectionalLightGizmoProperties(int index, const glm::mat4& transform, const glm::vec4& color);

        /// Rect Light Gizmo
        //////////////////////////////////////////////////////////////////////////////////////////////
        struct RectLightGizmoProperties
        {
            glm::mat4 transformation;
            glm::vec2 size;
            glm::vec4 color;
        };
        class RectLightGizmo
        {
            friend class LightGizmos;

        public:
            RectLightGizmo(LightGizmos& parent,int index)
                    : _parent{&parent}, _index{index}
            {}

            void SetProperties(const glm::mat4& transform, const glm::vec2& size, const glm::vec4& color);

        private:
            // todo: weak_ptr?
            LightGizmos* _parent;
            int _index;
        };

        std::weak_ptr<RectLightGizmo> CreateLightGizmo(const tao_pbr::RectLight& light);
        void SetRectLightGizmoProperties(int index, const glm::mat4& transform,const glm::vec2& size, const glm::vec4& color);

        typedef std::variant<std::weak_ptr<SphereLightGizmo>, std::weak_ptr<DirectionalLightGizmo>, std::weak_ptr<RectLightGizmo>> anyLightPtr;
        void SubscribeToSelectionEvents(const std::function<void(anyLightPtr)>& onSelectionEnter, const std::function<void(anyLightPtr)>& onSelectionExit);

    private:
        tao_gizmos::GizmosRenderer* _renderer;
        GizmoPickAgent* _inputAgent;

        struct SphereLightGizmoData
        {
            tao_gizmos::gizmo_id _iconGizmoId;
            tao_gizmos::gizmo_id _lightGizmoId;

            // TODO: those should be GenKeyVector<>
            std::vector<tao_gizmos::gizmo_instance_id>           _iconInstanceIds;
            std::vector<tao_gizmos::gizmo_instance_descriptor>   _iconInstances;
            std::vector<tao_gizmos::gizmo_instance_id>           _lightInstanceIds;
            std::vector<tao_gizmos::gizmo_instance_descriptor>   _lightInstances;
            std::vector<SphereLightGizmoProperties>              _sphereLightsProperties;
            std::vector<std::shared_ptr<SphereLightGizmo>>       _sphereLights;
        };

        SphereLightGizmoData _sphereLightGizmoData;

        struct DirectionalLightGizmoData
        {
            tao_gizmos::gizmo_id _iconGizmoId;
            tao_gizmos::gizmo_id _lightGizmoId;

            // TODO: those should be GenKeyVector<>
            std::vector<tao_gizmos::gizmo_instance_id>               _iconInstanceIds;
            std::vector<tao_gizmos::gizmo_instance_descriptor>       _iconInstances;
            std::vector<tao_gizmos::gizmo_instance_id>               _lightInstanceIds;
            std::vector<tao_gizmos::gizmo_instance_descriptor>       _lightInstances;
            std::vector<DirectionalLightGizmoProperties>             _directionalLightsProperties;
            std::vector<std::shared_ptr<DirectionalLightGizmo>>      _directionalLights;
        };

        DirectionalLightGizmoData _directionalLightGizmoData;

        struct RectLightGizmoData
        {
            tao_gizmos::gizmo_id _iconGizmoId;
            tao_gizmos::gizmo_id _lightGizmoId;

            // TODO: those should be GenKeyVector<>
            std::vector<tao_gizmos::gizmo_instance_id>               _iconInstanceIds;
            std::vector<tao_gizmos::gizmo_instance_descriptor>       _iconInstances;
            std::vector<tao_gizmos::gizmo_instance_id>               _lightInstanceIds;
            std::vector<tao_gizmos::gizmo_instance_descriptor>       _lightInstances;
            std::vector<RectLightGizmoProperties>                    _rectLightsProperties;
            std::vector<std::shared_ptr<RectLightGizmo>>             _rectLights;
        };

        RectLightGizmoData _rectLightGizmoData;


        std::function<void(std::optional<tao_gizmos::gizmo_instance_id>)> _selectionCallback;
        std::vector<const std::function<void(anyLightPtr)>*> _clientLightSelectedCallbacks;
        std::vector<const std::function<void(anyLightPtr)>*> _clientLightDeselectedCallbacks;


        tao_gizmos::gizmo_instance_descriptor GetSphereLightInstanceDesc(const glm::mat4& transform, float radius, const glm::vec4& color);
        tao_gizmos::gizmo_instance_descriptor GetDirectionalLightInstanceDesc(const glm::mat4& transform, const glm::vec4& color);
        tao_gizmos::gizmo_instance_descriptor GetRectLightInstanceDesc(const glm::mat4& transform, const glm::vec2& size, const glm::vec4& color);

        static constexpr tao_ogl_resources::ogl_depth_state DEPTH_LESS_EQUAL_NO_WRITE
            {
                .depth_test_enable        = true,
                .depth_write_enable        = false,
                .depth_func                = tao_ogl_resources::depth_func_less_equal,
                .depth_range_near        = 0.0,
                .depth_range_far        = 1.0,
            };
        static constexpr tao_ogl_resources::ogl_blend_state BLEND_ON_TOP
            {
                .blend_enable = true,
                .blend_equation_rgb     =
                    {
                        .blend_func = tao_ogl_resources::blend_func_add,
                        .blend_factor_src = tao_ogl_resources::blend_fac_src_alpha,
                        .blend_factor_dst = tao_ogl_resources::blend_fac_one_minus_src_alpha
                    },
                .blend_equation_alpha   =
                    {
                        .blend_func = tao_ogl_resources::blend_func_add,
                        .blend_factor_src = tao_ogl_resources::blend_fac_one,
                        .blend_factor_dst = tao_ogl_resources::blend_fac_one_minus_src_alpha
                    },
                .color_mask = tao_ogl_resources::mask_all
            };
        static constexpr tao_ogl_resources::ogl_rasterizer_state RASTERIZER_MS
            {
                .culling_enable             = false,
                .front_face                 = tao_ogl_resources::front_face_ccw,
                .cull_mode                  = tao_ogl_resources::cull_mode_back,
                .polygon_mode               = tao_ogl_resources::polygon_mode_fill,
                .multisample_enable         = true,
                .alpha_to_coverage_enable   = false
            };

        static constexpr int kGizmoSize = 46;
        
        static tao_gizmos::gizmo_id CreateSphereLightIconGizmo(tao_gizmos::GizmosRenderer* gr);
        static tao_gizmos::gizmo_id CreateSphereLightAreaGizmo(tao_gizmos::GizmosRenderer* gr);;
        static tao_gizmos::gizmo_id CreateDirectionalLightIconGizmo(tao_gizmos::GizmosRenderer* gr);
        static tao_gizmos::gizmo_id CreateDirectionalLightDirectionGizmo(tao_gizmos::GizmosRenderer* gr);;
        static tao_gizmos::gizmo_id CreateRectLightIconGizmo(tao_gizmos::GizmosRenderer* gr);
        static tao_gizmos::gizmo_id CreateRectLightDirectionGizmo(tao_gizmos::GizmosRenderer* gr);;

        enum lightType
        {
            directional,
            sphere,
            rect
        };
        struct lightIndex
        {
            lightType type;
            int index;
        };

        std::optional<lightIndex> GetLightFromInstanceId(tao_gizmos::gizmo_instance_id id);
        anyLightPtr GetLightPtrFromIndex(lightIndex index);
        tao_gizmos::gizmo_instance_id GetInstanceIdFromIndex(lightIndex index);

        std::optional<lightIndex> _latestSelectedLight = std::nullopt;
        void OnSelection(std::optional<tao_gizmos::gizmo_instance_id> selectedItem);
    };

    class GizmoGrid
    {
    public:
        GizmoGrid(tao_gizmos::GizmosRenderer& gr);

        void UpdateView(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec2& nearFar);

    private:
        tao_gizmos::GizmosRenderer* _gr;
        tao_gizmos::gizmo_id _gizmoId;
        tao_gizmos::gizmo_instance_id _instanceId;

        tao_gizmos_shader_graph::SGOutColorAndDepth CreateGridShader();

        void UpdateMeshGizmoVertices(const glm::mat4& viewMatrix, const glm::mat4& projMatrix, const glm::vec2& nearFar);

    };

    class TaoScene {
    public:
        struct tao_scene_config {
            unsigned int windowWidth = 1920;
            unsigned int windowHeight = 1080;
        };

        explicit TaoScene(const tao_scene_config &config);

        tao_render_context::RenderContext&  GetRenderContext()  {return *_renderContext; }
        tao_gizmos::GizmosRenderer&         GetGizmosRenderer() {return *_gizmosRenderer; }
        tao_pbr::PbrRenderer&               GetPbrRenderer()    {return *_pbrRenderer; }

        void SetTmMode(TransformManipulator::TmMode);
        TransformManipulator::TmMode GetCurrentTmMode();

        void NewFrame();
        void EndFrame();

        void LoadGltf(const char* path);

        size_t EnvironmentsCount() const;
        void SetEnvironment(size_t idx);
        const std::string& GetEnvironmentName(size_t idx) const;
        size_t GetCurrentEnvironment() const;

        // TODO: --- cannot return void....need to refer to the
        // TODO: --- light later (for ex "delete")
        void AddLight(const tao_pbr::DirectionalLight& light);
        void AddLight(const tao_pbr::SphereLight& light);
        void AddLight(const tao_pbr::RectLight& light);

    private:
        unsigned int _windowWidth  = 0;
        unsigned int _windowHeight = 0;
        int _fboWidth       = 0;
        int _fboHeight      = 0;
        const int kFboWidthVC  = 256;
        const int kFboHeightVC = 256;

        std::unique_ptr<tao_render_context::RenderContext> _renderContext;
        std::unique_ptr<tao_gizmos::GizmosRenderer> _gizmosRenderer;
        std::unique_ptr<tao_gizmos::GizmosRenderer> _gizmosRendererVC;
        std::unique_ptr<tao_pbr::PbrRenderer> _pbrRenderer;
        std::unique_ptr<MouseInputManager> _inputManager;
        std::unique_ptr<CameraInputAgent> _cameraInputAgent;
        std::unique_ptr<GizmoPickAgent> _gizmoPickAgent;
        std::unique_ptr<TransformManipulator> _transformManipulator;
        std::unique_ptr<GizmoGrid> _gridGizmo;
        std::unique_ptr<LightGizmos> _lightGizmo;
        std::unique_ptr<tao_render_context::WindowCompositor> _windowCompositor;

        glm::mat4 _viewMatrix;
        glm::mat4 _projMatrix;
        glm::vec2 _nearFar=glm::vec2{0.1f, 100.0f};

        std::vector<std::pair<std::string, tao_pbr::GenKey<tao_pbr::EnvironmentLight>>> _environmentLights;
        size_t _currentEnvironment;

        std::function<void(int, int)> _resizeCallback;
        std::function<void(LightGizmos::anyLightPtr)> _lightSelectionEnterCallback;
        std::function<void(LightGizmos::anyLightPtr)> _lightSelectionExitCallback;
        std::function<void(const glm::mat4&)> _tmUpdateCallback;

        template<typename LightType, typename LightGizmoType>
        struct MyLight
        {
            LightType pbrLight;
            tao_pbr::GenKey<LightType> pbrLightKey;
            std::weak_ptr<LightGizmoType> gizmoLightKey;
        };
        typedef MyLight<tao_pbr::SphereLight, LightGizmos::SphereLightGizmo>            MySphereLight;
        typedef MyLight<tao_pbr::DirectionalLight, LightGizmos::DirectionalLightGizmo>  MyDirectionalLight;
        typedef MyLight<tao_pbr::RectLight, LightGizmos::RectLightGizmo>                MyRectLight;


        std::vector<MyDirectionalLight> _directionalLights;
        std::vector<MySphereLight>      _sphereLights;
        std::vector<MyRectLight>        _rectLights;

        std::variant<MyDirectionalLight*, MySphereLight*, MyRectLight*> _selectedLight = static_cast<MyDirectionalLight*>(nullptr);

        template<typename LightType, typename LightGizmoType>
        void AddLight(const LightType& light, std::vector<MyLight<LightType, LightGizmoType>>& vec);

        void LoadHDRIs(tao_pbr::PbrRenderer& renderer);

        // Gltf Import -------------------------------------------------------------------------------------------------------
        static glm::mat4 GetMat4(const aiMatrix4x4 &aiMat);

        static void LoadAiNode(tao_pbr::PbrRenderer &renderer, const aiScene *scene,
                               const std::vector<tao_pbr::GenKey<tao_pbr::Mesh>> &pbrMeshes,
                               const std::vector<tao_pbr::GenKey<tao_pbr::PbrMaterial>> &pbrMaterials,
                               const aiNode *node, const glm::mat4 &accTransform,
                               const tao_pbr::GenKey<tao_pbr::PbrMaterial> &defaultMat);

        static void CreateTextureIfNeeded(tao_pbr::PbrRenderer &renderer,
                                          std::map<std::string, tao_pbr::GenKey<tao_pbr::ImageTexture>> &pbrTextures,
                                          const std::string &rootDirName, const std::string &texName, bool srgb);

        static std::string GetTexName(const aiString &texName, const aiScene *scene);

        static tao_pbr::PbrMaterial LoadAiMaterial(tao_pbr::PbrRenderer &renderer,
                                                   std::map<std::string, tao_pbr::GenKey<tao_pbr::ImageTexture>> &pbrTextures,
                                                   const aiScene *scene, const aiMaterial *mat,
                                                   const std::string &rootDirName);

        static tao_pbr::Mesh LoadAiMesh(tao_pbr::PbrRenderer &renderer, const aiMesh *mesh);

        static void LoadAiScene(tao_pbr::PbrRenderer &renderer, const aiScene *scene, const std::string &rootDirName);

        static void LoadGltf(tao_pbr::PbrRenderer &renderer, const char *path);
        // ---------------------------------------------------------------------------------------------------------------------

        void OnResize(int, int);

        void OnLightSelectionEnter(LightGizmos::anyLightPtr);

        void OnLightSelectionExit(LightGizmos::anyLightPtr);

        void OnTmUpdate(const glm::mat4&);

        template<typename LightType, typename LightGizmoType>
        void SelectLight(std::weak_ptr<LightGizmoType> l, std::vector<MyLight<LightType, LightGizmoType>>& v);

        void InitCallbacks();

        void InitGizmosVC();
    };

}

#endif //TAOGL_TAOSCENE_H
