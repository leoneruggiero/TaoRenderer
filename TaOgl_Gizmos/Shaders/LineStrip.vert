#version 430 core
#define GIZMOS_LINE_STRIP
//! #include "./Include/UboDefs.glsl"

layout(location = 0) in vec3  v_position;

layout(std430, binding = 0) buffer instanceBuff
{
    INST_DATA buff_inst_data[];
};

layout(std430, binding = 1) buffer buff
{
    float b_screenDst[];
};


out VS_OUT
{
    vec4  v_color;
    vec2  v_texCoord;
}
vs_out;

void main()
{
    gl_Position = f_projMat * f_viewMat * buff_inst_data[gl_InstanceID].i_transform * vec4(v_position, 1.0);
    vs_out.v_color = buff_inst_data[gl_InstanceID].i_color;
    vs_out.v_texCoord = vec2(b_screenDst[o_vertCount * gl_InstanceID + gl_VertexID]/o_patternSize, 0.0);
}