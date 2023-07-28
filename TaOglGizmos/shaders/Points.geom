#version 430 core
#define GIZMOS_POINTS
//! #include "UboDefs.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in VS_OUT
{
    vec4  v_color;
    vec4  v_texCoord; // rect, .xy = bl, .zw = tr 
    bool  v_visible;
}
gs_in[];

out GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}gs_out;

void main() {

    if(!gs_in[0].v_visible) return;
    
    vec2 f = vec2(o_size);
    f = 2.0* f * f_invViewportSize.xy * gl_in[0].gl_Position.w;        
    
    vec4 c = gl_in[0].gl_Position;
    if(o_snap) // snap point center to nearest pixel
    {
        c.xy = round((c.xy/c.w*0.5+0.5)*f_viewportSize.xy);
        c.xy*= f_invViewportSize.xy;
        c.xy = c.xy*2-1;
        c.xy*= c.w;
    }

    // Top - Right
    gl_Position = c + vec4(f.xy, 0.0, 0.0);
    gs_out.v_texCoord  = gs_in[0].v_texCoord.zw; 
    gs_out.v_color     = gs_in[0].v_color;
    EmitVertex();

    // Bottom - Right
    gl_Position = c + vec4(f.x, -f.y, 0.0, 0.0);
    gs_out.v_texCoord  = gs_in[0].v_texCoord.zy; 
    gs_out.v_color     = gs_in[0].v_color;
    EmitVertex();

    // Top - Left
    gl_Position = c + vec4(-f.x, f.y, 0.0, 0.0);
    gs_out.v_texCoord  = gs_in[0].v_texCoord.xw; 
    gs_out.v_color     = gs_in[0].v_color;
    EmitVertex();

    // Bottom - Left          
    gl_Position = c + vec4(-f.xy, 0.0, 0.0);
    gs_out.v_texCoord  = gs_in[0].v_texCoord.xy; 
    gs_out.v_color     = gs_in[0].v_color;
    EmitVertex();

    EndPrimitive();
}