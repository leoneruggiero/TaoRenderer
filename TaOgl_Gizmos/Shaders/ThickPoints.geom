#version 430 core
#define GIZMOS_POINTS
//! #include "./Include/UboDefs.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in VS_OUT
{
    vec4  v_color;
    vec4  v_texCoord; // rect, .xy = bl, .zw = tr 
}
vs_out[];

out GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}gs_out;

void main() {

    float f = o_size;
    f *= gl_in[0].gl_Position.w;        

    gs_out.v_color     = vs_out[0].v_color;

    // Top - Right
    gl_Position = gl_in[0].gl_Position + f * vec4(f_invViewportSize, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.zw; 
    EmitVertex();

    // Bottom - Right
    gl_Position = gl_in[0].gl_Position + f * vec4(f_invViewportSize.x, - f_invViewportSize.y, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.zy; 
    EmitVertex();

    // Top - Left
    gl_Position = gl_in[0].gl_Position + f * vec4(-f_invViewportSize.x, f_invViewportSize.y, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.xw; 
    EmitVertex();

    // Bottom - Left          
    gl_Position = gl_in[0].gl_Position + f * vec4(-f_invViewportSize, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.xy; 
    EmitVertex();

    EndPrimitive();
}