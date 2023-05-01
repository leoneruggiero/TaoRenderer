#pragma once

#include <algorithm>
#include <functional>
#include <GLFW/glfw3.h>

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

        bool _newDataIn = false;

        float _halfTimeMilliseconds = 30.f;

    	std::function<bool()> _shouldListen;
        std::function<void()>   _mouseDown;
        std::function<void()>   _mouseUp;
        void Reset()
        {
            _cpXRaw     = -1.0;
            _cpYRaw     = -1.0;
            _cpXSmooth  = -1.0;
            _cpYSmooth  = -1.0;
        }

        void Update(float deltaTimeMilliseconds, int surfaceWidth, int surfaceHeight)
        {
            if(_newDataIn)
            {
                _mouseUp();
            }

            _newDataIn = false;
            UpdatePositions(_cpXRaw, _cpYRaw, deltaTimeMilliseconds, surfaceWidth, surfaceHeight);
        }

        void UpdateWithNewData(float newPosX, float newPosY, float deltaTimeMilliseconds, int surfaceWidth, int surfaceHeight)
        {
            // It has not received new data the last time it was updated.
            if (!_newDataIn) 
            {
                _mouseDown();
                Reset();
            }

            _newDataIn = true;
            UpdatePositions(newPosX, newPosY, deltaTimeMilliseconds, surfaceWidth, surfaceHeight);
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
                const std::function<void()>& mouseDownAction,
                const std::function<void()>& mouseUpAction,
                float halfTimeMilliseconds = 30.f) :

            _halfTimeMilliseconds(halfTimeMilliseconds),
            _shouldListen   (shouldListenPredicate),
            _mouseDown      (mouseDownAction),
            _mouseUp        (mouseUpAction)
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
        
        std::vector<MouseInputListener*> _mouseListeners;
        
        double _timems      = 0.0;

        void PollPosition(double& xPos, double& yPos) const
        {
            glfwGetCursorPos(_window, &xPos, &yPos);
        }

    public:

        MouseInput(GLFWwindow* const window) : _window(window)
        {
        }

        void AddListener(MouseInputListener& listener)
        {
            _mouseListeners.push_back(&listener);
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
                    listener->UpdateWithNewData(newXRaw, newYRaw, deltaTimeMs, newWinWidth, newWinHeight);
                else
                    listener->Update(deltaTimeMs, newWinWidth, newWinHeight);
            }
        }
    };

  
}
