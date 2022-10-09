#version 430 core

//! #include "../Include/UboDefs.glsl"

in VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
}fs_in;

uniform vec3 f_equator_color;
uniform vec3 f_south_color; 
uniform vec3 f_north_color;
uniform bool f_drawWithTexture;
uniform samplerCube t_EnvironmentMap;

#ifndef PI
#define PI 3.14159265358979323846
#endif
    
#ifndef PI_2
#define PI_2 1.57079632679
#endif

out vec4 FragColor;

void main()
{
    vec3 fpwN = normalize(fs_in.fragPosWorld);
    float f = asin(-fpwN.y) / PI_2;

    vec3 poleColor = f > 0.0 ? f_north_color : f_south_color;

    vec3 col = 
        f_drawWithTexture
            ? texture(t_EnvironmentMap, (fpwN * vec3(1.0, -1.0, -1.0))).rgb
            : mix(f_equator_color, poleColor, abs(f));
        
    if(f_doGamma && f_drawWithTexture)
        col = pow(col, vec3(f_gamma));
        
    FragColor = vec4(col, 1.0);
       
}