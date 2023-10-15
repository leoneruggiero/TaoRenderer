#ifndef OBJECT_H
#define OBJECT_H

#include <glad/glad.h>
#include "ShaderCode.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SceneUtils.h"
class Object
{
public:
    ShaderCode shader;
    Material material;
private:
    unsigned int _vao;
    unsigned int _vbo;

    unsigned int _tex_diffuse;
    unsigned int _tex_specular;

    unsigned int _vertCount;
    unsigned int _trisCount;
    glm::mat4 _modelMatrix;





public:
    Object(glm::vec3 position, float rotation, glm::vec3 rotationAxis,glm::vec3 scale,  float* vertexData, int vertexCount,  ShaderCode shader, Material mat)
        :
        shader(shader), _vertCount(vertexCount), _trisCount ((int)(vertexCount/3)), material(mat)
    {
        glm::mat4 model = glm::mat4(1.0f);
        
        model = glm::translate(model, position);

        model = glm::rotate(model, rotation, rotationAxis);

        model = glm::scale(model, scale);

        _modelMatrix = model;

        glGenVertexArrays(1, &_vao);
     
        glGenBuffers(1, &_vbo);

        glBindVertexArray(_vao);

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);

        glBufferData(GL_ARRAY_BUFFER, (vertexCount * 8)* sizeof(float), vertexData, GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Tex coords
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
       
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Textures
        glGenTextures(1, &_tex_diffuse);
        glGenTextures(1, &_tex_specular);

        glBindTexture(GL_TEXTURE_2D, _tex_diffuse);
        glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB,material.Diffuse.width, material.Diffuse.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, material.Diffuse.data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, _tex_specular);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, material.Specular.width, material.Specular.height, 0, GL_RGB, GL_UNSIGNED_BYTE, material.Specular.data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

public:
    void Draw(glm::mat4 view, glm::mat4 proj, glm::vec3 eye, SceneLights lights)
    {

        shader.use();

        // ModelViewProjection matrices + NormalMatrix
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(_modelMatrix));

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(_modelMatrix))));

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "proj"), 1, GL_FALSE, glm::value_ptr(proj));

        glUniform3fv(glGetUniformLocation(shader.ID, "eyeWorldPos"), 1, glm::value_ptr(eye));


        // material settings
       
        // Diffuse->0
        glActiveTexture(GL_TEXTURE0);

        glBindTexture(GL_TEXTURE_2D, _tex_diffuse);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glUniform1i(glGetUniformLocation(shader.ID, "material.Diffuse"), 0);

        // Specular->1
        glActiveTexture(GL_TEXTURE1);

        glBindTexture(GL_TEXTURE_2D, _tex_specular);

        glUniform1i(glGetUniformLocation(shader.ID, "material.Specular"), 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glUniform1f(glGetUniformLocation(shader.ID, "material.Shininess"), material.Shininess);


        // light settings

        glUniform4fv(glGetUniformLocation(shader.ID, "lights.Ambient"), 1, glm::value_ptr(lights.Ambient));

        glUniform3fv(glGetUniformLocation(shader.ID, "lights.Directional.Direction"), 1, glm::value_ptr(lights.Directional.Direction));
        glUniform4fv(glGetUniformLocation(shader.ID, "lights.Directional.Diffuse"), 1, glm::value_ptr(lights.Directional.Diffuse));
        glUniform4fv(glGetUniformLocation(shader.ID, "lights.Directional.Specular"), 1, glm::value_ptr(lights.Directional.Specular));

        for (int  i = 0; i < 4; i++)
        {
            // point
            glUniform3fv(glGetUniformLocation(shader.ID, ("lights.Point" + std::to_string(i) + ".Direction").c_str()), 1, glm::value_ptr(lights.Point[i].Position));
            glUniform4fv(glGetUniformLocation(shader.ID, ("lights.Point" + std::to_string(i) + ".Diffuse").c_str()), 1, glm::value_ptr(lights.Point[i].Diffuse));
            glUniform4fv(glGetUniformLocation(shader.ID, ("lights.Point" + std::to_string(i) + ".Specular").c_str()), 1, glm::value_ptr(lights.Point[i].Specular));
            glUniform1f(glGetUniformLocation(shader.ID, ("lights.Point" + std::to_string(i) + ".Constant").c_str()), lights.Point[i].Constant);
            glUniform1f(glGetUniformLocation(shader.ID, ("lights.Point" + std::to_string(i) + ".Linear").c_str()), lights.Point[i].Linear);
            glUniform1f(glGetUniformLocation(shader.ID, ("lights.Point" + std::to_string(i) + ".Quadratic").c_str()),lights.Point[i].Quadratic);

            // spot
            glUniform3fv(glGetUniformLocation(shader.ID, ("lights.Spot" + std::to_string(i) + ".Direction").c_str()), 1, glm::value_ptr(lights.Spot[i].Position));
            glUniform4fv(glGetUniformLocation(shader.ID, ("lights.Spot" + std::to_string(i) + ".Diffuse").c_str()), 1, glm::value_ptr(lights.Spot[i].Diffuse));
            glUniform4fv(glGetUniformLocation(shader.ID, ("lights.Spot" + std::to_string(i) + ".Specular").c_str()), 1, glm::value_ptr(lights.Spot[i].Specular));
            glUniform1f(glGetUniformLocation(shader.ID, ("lights.Spot" + std::to_string(i) + ".InnerRadius").c_str()), lights.Spot[i].InnerRadius);
            glUniform1f(glGetUniformLocation(shader.ID, ("lights.Spot" + std::to_string(i) + ".OuterRadius").c_str()), lights.Spot[i].OuterRadius);
       

        }

 
        glBindVertexArray(_vao);

        glDrawArrays(GL_TRIANGLES, 0, _trisCount*3);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindVertexArray(0);

        

        GLenum error=0;

        while ((error = glGetError()) != GL_NO_ERROR)
        {
            ;
        }

    }
};
#endif