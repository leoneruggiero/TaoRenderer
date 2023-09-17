#pragma once

#include <algorithm>
#include <functional>
#include <cmath>
#include "GLFW/glfw3.h"

/*
namespace tao_input
{
    enum class mouse_origin
    {
	    bottom_left,
        top_left
    };
    
    enum mouse_button
    {
        left_mouse_button   = GLFW_MOUSE_BUTTON_LEFT,
        middle_mouse_button = GLFW_MOUSE_BUTTON_MIDDLE,
        right_mouse_button  = GLFW_MOUSE_BUTTON_RIGHT
    };

    enum modifier_key
    {
        left_shift_key   = GLFW_KEY_LEFT_SHIFT,
        left_control_key = GLFW_KEY_LEFT_CONTROL
    };

    // from https://theorangeduck.com/page/spring-roll-call
    static float Damper(float current, float goal, float halfTime, float timeStep)
    {
        return std::lerp(current, goal, 1.0f - powf(2, -timeStep / halfTime));
    }

    class MouseInputListener
    {
        friend class MouseInput;

    private:
        float _cpXRaw       = -1.0;
        float _cpYRaw       = -1.0;
        float _cpXSmooth    = -1.0;
        float _cpYSmooth    = -1.0;

        // to convert from pixel coords to normalized [0,1]
        int   _surfaceWidth  = 0;
        int   _surfaceHeight = 0;

        bool _enabled = false;
        float _halfTimeMilliseconds = 30.f;

        bool _lmbPressed = false;
        bool _mmbPressed = false;
        bool _rmbPressed = false;

    	std::function<bool()>   _shouldListen;
        std::function<void()>   _mouseEnable;
        std::function<void()>   _mouseDisable;
        std::function<void(float, float, int, int)>   _mouseDown;
        std::function<void(float, float, int, int)>   _mouseUp;
        void Reset()
        {
            _cpXRaw     = -1.0;
            _cpYRaw     = -1.0;
            _cpXSmooth  = -1.0;
            _cpYSmooth  = -1.0;
        }

        void UpdateDisabled(float deltaTimeMilliseconds, int surfaceWidth, int surfaceHeight)
        {
            if(_enabled)
            {
                _mouseDisable();
            }

            _enabled = false;
            UpdatePositions(_cpXRaw, _cpYRaw, deltaTimeMilliseconds, surfaceWidth, surfaceHeight);
        }

        void UpdateEnabled(float newPosX, float newPosY, float deltaTimeMilliseconds, int surfaceWidth, int surfaceHeight)
        {
            // It has not received new data the last time it was updated.
            if (!_enabled)
            {
                _mouseEnable();
                Reset();
            }

            _enabled = true;
            UpdatePositions(newPosX, newPosY, deltaTimeMilliseconds, surfaceWidth, surfaceHeight);
        }

        void UpdateButton(bool newVal, bool oldVal, float posX, float posY, int surfaceWidth, int surfaceHeight)
        {
            if(newVal!=oldVal)
            {
                if(newVal&&_enabled) _mouseDown(posX, posY, surfaceWidth, surfaceHeight);
                else                 _mouseUp  (posX, posY, surfaceWidth, surfaceHeight);
            }
        }

        void UpdateButtons(bool lmb, bool mmb, bool rmb, int surfaceWidth, int surfaceHeight)
        {
            float posX, posY;
            Position(posX, posY);

            UpdateButton(lmb, _lmbPressed, posX, posY, surfaceWidth, surfaceHeight);
            UpdateButton(mmb, _mmbPressed, posX, posY, surfaceWidth, surfaceHeight);
            UpdateButton(rmb, _rmbPressed, posX, posY, surfaceWidth, surfaceHeight);

            _lmbPressed = lmb;
            _mmbPressed = mmb;
            _rmbPressed = rmb;
        }

        void UpdatePositions(float rawPosX, float rawPosY, float deltaTimeMilliseconds, int surfaceWidth, int surfaceHeight)
        {
            _surfaceWidth = surfaceWidth;
            _surfaceHeight = surfaceHeight;

            if (_cpXSmooth < 0.f && _cpYSmooth < 0.f)
            {
                _cpXSmooth = rawPosX;
                _cpYSmooth = rawPosY;
            }
            else
            {
                _cpXSmooth = Damper(_cpXSmooth, rawPosX, _halfTimeMilliseconds, deltaTimeMilliseconds);
                _cpYSmooth = Damper(_cpYSmooth, rawPosY, _halfTimeMilliseconds, deltaTimeMilliseconds);
            }

            _cpXRaw = rawPosX;
            _cpYRaw = rawPosY;
        }

        void ApplyOrigin(float& xPos, float& yPos, mouse_origin origin) const
        {
            switch (origin)
            {
            case(mouse_origin::bottom_left): yPos = _surfaceHeight - yPos;   break;
            case(mouse_origin::top_left):                                    break;
            }
        }

    public:
        explicit
            MouseInputListener(
                const std::function<bool()>& shouldListenPredicate,
                const std::function<void()>& mouseEnableAction,
                const std::function<void()>& mouseDisableAction,
                const std::function<void(float, float, int, int)>& mouseDownAction,
                const std::function<void(float, float, int, int)>& mouseUpAction,
                float halfTimeMilliseconds = 30.f) :

                    _halfTimeMilliseconds(halfTimeMilliseconds),
                    _shouldListen   (shouldListenPredicate),
                    _mouseEnable     (mouseEnableAction),
                    _mouseDisable    (mouseDisableAction),
                    _mouseDown       (mouseDownAction),
                    _mouseUp         (mouseUpAction)
        {

        }

        MouseInputListener() = default;

        void Position(float& xPos, float& yPos, bool smooth = false, mouse_origin origin = mouse_origin::bottom_left) const
        {
            xPos = smooth ? _cpXSmooth : _cpXRaw;
            yPos = smooth ? _cpYSmooth : _cpYRaw;
            ApplyOrigin(xPos, yPos, origin);
        }

        void PositionNormalized(float& xPos, float& yPos, bool smooth = false, mouse_origin origin = mouse_origin::bottom_left) const
        {
           
            Position(xPos, yPos, smooth, origin);
            xPos = std::clamp(xPos / static_cast<float>(_surfaceWidth ), 0.f, 1.f);
            yPos = std::clamp(yPos / static_cast<float>(_surfaceHeight), 0.f, 1.f);
        }
    };

    class MouseInput
    {
    private:
        GLFWwindow* _window;
        
        std::vector<std::shared_ptr<MouseInputListener>> _mouseListeners;
        
        double _timems      = 0.0;

        void PollPosition(double& xPos, double& yPos) const
        {
            glfwGetCursorPos(_window, &xPos, &yPos);
        }

    public:

        MouseInput(GLFWwindow* const window) : _window(window)
        {
        }

        void AddListener(const std::shared_ptr<MouseInputListener>& listener)
        {
            _mouseListeners.push_back(listener);
        }

        bool IsPressed(mouse_button button) const
        {
            return glfwGetMouseButton(_window, button) == GLFW_PRESS;
        }

        bool IsKeyPressed(modifier_key key) const
        {
            return glfwGetKey(_window, key) == GLFW_PRESS;
        }

        void Poll()
        {
            double newXRaw, newYRaw;
            PollPosition(newXRaw, newYRaw);

            int newWinWidth, newWinHeight;
            glfwGetWindowSize(_window, &newWinWidth, &newWinHeight);

            double const newTime = glfwGetTime() * 1e3;
            float const deltaTimeMs   = newTime - _timems;
            _timems = newTime;
            
            for(auto& listener : _mouseListeners)
            {
                if (listener->_shouldListen())
                {
                    listener->UpdateEnabled(newXRaw, newYRaw, deltaTimeMs, newWinWidth, newWinHeight);
                }
                else
                    listener->UpdateDisabled(deltaTimeMs, newWinWidth, newWinHeight);

                listener->UpdateButtons(IsPressed(left_mouse_button), IsPressed(middle_mouse_button), IsPressed(right_mouse_button), newWinWidth, newWinHeight);
            }
        }
    };

  
}
*/
