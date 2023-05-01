//? #version 430 core

#ifdef GIZMOS_POINTS
    layout (std140) uniform blk_PerObjectData
    {
        uniform float   o_size;         // 4  byte
        uniform bool    o_snap;         // 4  byte
        uniform bool    o_hasTexture;   // 4 bytes
                                        // TOTAL => 12 byte
    };
#endif

#ifdef GIZMOS_LINES
    layout (std140) uniform blk_PerObjectData
    {
        uniform mat4    o_modelMat;     // 64  byte
        uniform float   o_size;         // 4   byte
                                        // TOTAL => 68 byte
    };
#endif

layout (std140) uniform blk_PerFrameData
{
    uniform mat4    f_viewMat;          // 64  byte
    uniform mat4    f_projMat;          // 64  byte
    uniform vec2    f_viewportSize;     // 8   byte
    uniform vec2    f_invViewportSize;  // 8   byte
    uniform vec2    f_nearFar;          // 4   byte
                                        // TOTAL => 148 byte
};
