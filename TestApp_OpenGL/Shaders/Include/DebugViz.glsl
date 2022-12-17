//? #version 430 core

#define DEBUG_VIZ

#define DEBUG_VIZ_CONE      0
#define DEBUG_VIZ_SPHERE    1
#define DEBUG_VIZ_POINT     2
#define DEBUG_VIZ_LINE      3
            
#define DEBUG_DIRECTIONAL_LIGHT     0
#define DEBUG_POINT_LIGHT           1
#define DEBUG_DIRECTIONAL_SPLITS    2
#define DEBUG_TAA                   3

struct debugVizItem
{
    uvec4 u4Data;
    vec4  f4Data0;
    vec4  f4Data1;
    vec4  f4Data2;
};
        
layout (std140) buffer buff_debugFeedback
{
    uvec4 uDebug_viewport;
    uvec4 uDebug_mouse;
    uvec4 uDebug_cnt;
    debugVizItem uDebug_VizItems[];
};

void DViz_DrawCone(vec3 start, vec3 direction, float radius, vec4 color)
{
    if(uDebug_cnt.x < uDebug_VizItems.length()-1)
    {
        uint i = atomicAdd(uDebug_cnt.x, 1);

        uDebug_VizItems[i].u4Data.x  = DEBUG_VIZ_CONE;
        uDebug_VizItems[i].f4Data0   = vec4(start, 0.0);
        uDebug_VizItems[i].f4Data1   = vec4(direction, radius);
        uDebug_VizItems[i].f4Data2   = color;
 
    }     
}

// Line defined by start and end points in world space.
void DViz_DrawLine(vec3 start, vec3 end, vec4 color)
{
    if(uDebug_cnt.x < uDebug_VizItems.length()-1)
    {
        uint i = atomicAdd(uDebug_cnt.x, 1);

        uDebug_VizItems[i].u4Data.x  = DEBUG_VIZ_LINE;
        uDebug_VizItems[i].f4Data0   = vec4(start, 0.0);
        uDebug_VizItems[i].f4Data1   = vec4(end, 0.0);
        uDebug_VizItems[i].f4Data2   = color;

        
    }      
}

// Line defined by start and end points in screen space.
void DViz_DrawLineSS(vec2 start, vec2 end, vec4 color, mat4 view, mat4 proj)
{
    // See  https://www.derschmale.com/2014/09/28/unprojections-explained/
    mat4 invProj = inverse(proj);
    mat4 invView = inverse(view);

    vec4 s4 = invView * invProj * vec4(start*2-1, 0.0, 1.0);
    vec4 e4 = invView * invProj * vec4(end  *2-1, 0.0, 1.0);

    DViz_DrawLine(s4.xyz/s4.w, e4.xyz/e4.w, color);
}