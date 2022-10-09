// From: https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
   
#version 430 core

//! #include "../Include/UboDefs.glsl"

in vec4 FragPos;

uniform int lightIndex;

void main()
{
    // get distance between fragment and light source
    float lightDistance = length(FragPos.xyz - f_pointLight[lightIndex].Position.xyz);
    
    // map to [0;1] range by dividing by far plane distance
    lightDistance = lightDistance / f_pointLight[lightIndex].Radius;
    
    // write this as modified depth
    gl_FragDepth = lightDistance;
}  
       