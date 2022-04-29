#ifndef CAMERA_H
#define CAMERA_H

#include <iostream>
#include <iomanip>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>



// Default camera values
const float SENSITIVITY = 0.005f;
const float SCROLL_SENSITIVITY = 0.2f;


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
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


    // constructor with initial polar coordinates
    Camera(float r, float theta, float phi) : MouseSensitivity(SENSITIVITY), MouseScrollSensitivity(SCROLL_SENSITIVITY)
    {
        CameraOrigin = WorldOrigin; // To be changed
        Theta = theta;
        Phi = phi;
        R = r;
        localX = glm::vec3(1.0f, 0.0f, 0.0f);
        localY = glm::vec3(0.0f, -1.0f, 0.0f);
        localZ = glm::vec3(0.0f, 0.0f, 1.0f);
        updateCameraVectors();
        updateCameraPosition();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, CameraOrigin, localZ);
    }

  

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Theta -= yoffset;
        Phi += xoffset;
           
        //std::cout << glm::abs(yoffset) << std::endl;

        updateCameraVectors();
        updateCameraPosition();
        Reset();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        R -= (float)yoffset*MouseScrollSensitivity;
        if (R < 0.0f)
            R = 0.0f;
        updateCameraPosition();
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
       
        glm::mat4 rotMatrix = glm::mat4(1.0f);
        rotMatrix = glm::rotate(rotMatrix, -Phi, glm::vec3(0, 0, 1));
        rotMatrix = glm::rotate(rotMatrix, /*(glm::pi<float>() / 2)*/ - Theta, localX);

        localX = glm::vec3(rotMatrix * glm::vec4(localX, 1.0));
        localY = glm::vec3(rotMatrix * glm::vec4(localY, 1.0));
        localZ = glm::vec3(rotMatrix * glm::vec4(localZ, 1.0));
    }
private:
    void updateCameraPosition()
    {
       
        Position = CameraOrigin + (R * localY);

    }

public:
    void Reset()
    {
     
        Theta = 0;
        Phi = 0;
    }

};
#endif
