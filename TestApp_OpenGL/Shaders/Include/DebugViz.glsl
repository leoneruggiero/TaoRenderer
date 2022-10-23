//? #version 430 core

#define DEBUG_VIZ

#define DEBUG_VIZ_CONE      0
#define DEBUG_VIZ_SPHERE    1
#define DEBUG_VIZ_POINT     2
#define DEBUG_VIZ_LINE      3
            
#define DEBUG_DIRECTIONAL_LIGHT     0
#define DEBUG_POINT_LIGHT           1
#define DEBUG_DIRECTIONAL_SPLITS    2

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
        uDebug_VizItems[uDebug_cnt.x].u4Data.x  = DEBUG_VIZ_CONE;
        uDebug_VizItems[uDebug_cnt.x].f4Data0   = vec4(start, 0.0);
        uDebug_VizItems[uDebug_cnt.x].f4Data1   = vec4(direction, radius);
        uDebug_VizItems[uDebug_cnt.x].f4Data2   = color;

        atomicAdd(uDebug_cnt.x, 1);
    }     
}

void DViz_DrawLine(vec3 start, vec3 end, vec4 color)
{
    if(uDebug_cnt.x < uDebug_VizItems.length()-1)
    {
        uDebug_VizItems[uDebug_cnt.x].u4Data.x  = DEBUG_VIZ_LINE;
        uDebug_VizItems[uDebug_cnt.x].f4Data0   = vec4(start, 0.0);
        uDebug_VizItems[uDebug_cnt.x].f4Data1   = vec4(end, 0.0);
        uDebug_VizItems[uDebug_cnt.x].f4Data2   = color;

        atomicAdd(uDebug_cnt.x, 1);
    }      
}