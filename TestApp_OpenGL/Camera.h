#ifndef CAMERA_H
#define CAMERA_H

#include <iostream>
#include <iomanip>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <chrono>
#include "Timer.h"


// Default camera values
const float SENSITIVITY = 0.005f;
const float FLY_SPEED = 0.25f;
const float SCROLL_SENSITIVITY = 0.2f;
const int  ANIMATION_TIME_MS = 500;
const int  ANIMATION_INTERVAL_MS = 30;

enum class ViewType
{
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back
};


enum class NavigationType
{
    Turntable,
    Freefly
};

class Camera
{

private:

    // Lock user input during animations
    bool _locked = false;
    NavigationType _navigationMode;
    Timer _animationTimer;


    glm::vec3 localToWorld(glm::vec3 localDirection)
    {
        glm::mat3 locToW = 
            glm::mat3(localX, localY, localZ);

        return locToW * localDirection;
    }

    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        glm::mat4 rotMatrix = glm::mat4(1.0f);
        rotMatrix = glm::rotate(rotMatrix, Phi, glm::vec3(0, 0, 1));
        rotMatrix = glm::rotate(rotMatrix, /*(glm::pi<float>() / 2)*/ Theta, WorldX);

        localX = glm::vec3(rotMatrix * glm::vec4(WorldX, 1.0));
        localY = glm::vec3(rotMatrix * glm::vec4(WorldY, 1.0));
        localZ = glm::vec3(rotMatrix * glm::vec4(WorldZ, 1.0));
    }

    void updateCameraPosition()
    {
        Position = CameraOrigin + (R * localY);
    }

    glm::quat CurrentRotation()
    {
        glm::quat
            q1 = glm::angleAxis(Phi, WorldZ),
            q2 = glm::angleAxis(Theta, WorldX);

        return q1 * q2;
    }

    glm::quat TargetRotation(ViewType view)
    {
        float phi, theta;

        switch (view)
        {
        case ViewType::Top:
            phi = 0.0f;
            theta = glm::pi<float>() / 2.0f;
            break;
        case ViewType::Bottom:
            phi = 0.0f;
            theta = -glm::pi<float>() / 2.0f;
            break;
        case ViewType::Left:
            phi = -glm::pi<float>() / 2.0f;
            theta = 0.0f;
            break;
        case ViewType::Right:
            phi = glm::pi<float>() / 2.0f;
            theta = 0.0f;
            break;
        case ViewType::Front:
            phi = 0.0f;
            theta = 0.0f;
            break;
        case ViewType::Back:
            phi = glm::pi<float>();
            theta = 0.0f;
            break;
        }

        glm::quat
            q1 = glm::angleAxis(phi, WorldZ),
            q2 = glm::angleAxis(theta, WorldX);

        return q1 * q2;
    }

    
public:
    // static Utils
    glm::vec3 WorldOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 WorldX = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 WorldY = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 WorldZ = glm::vec3(0.0f, 0.0f, 1.0f);
    
    // camera Attributes
    glm::vec3 CameraOrigin;
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    
    // polar coordinates
    float Theta;
    float Phi;
    float R;
    
    // local axis
    glm::vec3 localX;
    glm::vec3 localY;
    glm::vec3 localZ;
    
    // camera options
    float MouseSensitivity;
    float MouseScrollSensitivity;
    float FlySpeed;


    // constructor with initial polar coordinates
    Camera(float r, float theta, float phi) : 
        MouseSensitivity(SENSITIVITY), MouseScrollSensitivity(SCROLL_SENSITIVITY), FlySpeed(FLY_SPEED),
        _animationTimer(), _navigationMode(NavigationType::Turntable)
    {
        CameraOrigin = WorldOrigin; // To be changed
        Theta = theta;
        Phi = phi;
        R = r;
        updateCameraVectors();
        updateCameraPosition();
    }

    void SetNavigationMode(NavigationType navigationMode)
    {
        _navigationMode = navigationMode;
    }

    NavigationType GetNavigationMode()
    {
        return _navigationMode;
    }

    // returns the view matrix calculated using Euler Angles and  LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position - localY, localZ);
    }

    void SetView(ViewType view)
    {
        if (_locked) 
            return;

        if (_navigationMode != NavigationType::Turntable)
            return;

        _locked = true;

        glm::quat
            from = CurrentRotation(),
            to = TargetRotation(view);

        auto startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        _animationTimer.setInterval([=](){

            auto currTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            float param = glm::clamp((currTime - startTime) / float(ANIMATION_TIME_MS), 0.0f, 1.0f);

            glm::vec3 euler = glm::eulerAngles(glm::slerp<float>(from, to, param));
            Phi = euler.z;
            Theta = euler.x;
           
            updateCameraVectors();
            updateCameraPosition();
        
        }, ANIMATION_INTERVAL_MS);

        _animationTimer.setTimeout([=]() {
            
            glm::vec3 euler = glm::eulerAngles(to);

            this->Phi = euler.z;
            this->Theta = euler.x;
        
            this->updateCameraVectors();
            this->updateCameraPosition();
        
            _locked = false;
            _animationTimer.stop();
        }, ANIMATION_TIME_MS);

    }


    void UpdateTurntableCamera(float xoffset, float yoffset)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Theta -= yoffset;
        Phi -= xoffset;


        updateCameraVectors();
        updateCameraPosition();
    }

    void UpdateFreeflyCamera(float xoffset, float yoffset)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Theta -= yoffset;
        Phi -= xoffset;


        updateCameraVectors();
       
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset)
    {
        if (_locked) return;

        switch (_navigationMode)
        {
        case(NavigationType::Turntable):
            UpdateTurntableCamera(xoffset, yoffset);
            break;

        case(NavigationType::Freefly):
            UpdateFreeflyCamera(xoffset, yoffset);
            break;

        default:
            throw("You need to implement this");

        }
        
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        if (_locked) return;
        switch (_navigationMode)
        {
        case NavigationType::Turntable:

            R -= (float)yoffset * MouseScrollSensitivity;
            if (R < 0.0f)
                R = 0.0f;
            updateCameraPosition();
            break;

        case NavigationType::Freefly:

            /* Empty */

            break;
        default:
            break;
        }
        
    }

    /// <summary>
    /// Moves the camera in the provided local direction.
    /// The direction is interpreted in the reference frame of the camera (localX,Y,Z). Axis minus Y faces towards the camera target.
    /// </summary>
    void MoveTowards(glm::vec3 localDirection)
    {
        if (_locked)
            return;
        if (_navigationMode != NavigationType::Freefly)
            return;

        

        Position += FlySpeed * localToWorld(glm::normalize(localDirection));
    }

    void Reset()
    {

        Theta = 0;
        Phi = 0;
    }



};
#endif
