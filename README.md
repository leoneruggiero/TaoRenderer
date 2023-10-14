# Tao Renderer
![Screenshot 2023-10-14 181635](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/8e63b128-2edb-429d-8bd3-d8de6a5c23a2)

**Tao** is an OpenGL renderer meant to be a playground to experiment with modern OpenGL concepts, computer-graphics techniques and learn a bit of C++. 
The renderer is made of different components:
* **RenderContext**: creates a window, an OpenGL context, and provides abstractions for OpenGL.
* **PbrRenderer**: draws a 3D scene using physically-based rendering techniques.
* **GizmosRenderer**: draws all the useful _extras_ in a 3D application, such as debug visualization, lines, overlays, ...

The different components can communicate since they share the same underlying OpenGL abstraction provided by RenderContex.

## Pbr Renderer Features
### Pbr materials
![Screenshot 2023-10-14 184257](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/efc992a1-0b08-408a-bced-5eb8f33ac794)
**Cook-Torrance** has been chosen as the specular BRDF of choice for this renderer since it is considered the standard in a number of articles and technical publications.<br>
**Lambertian** diffuse has been used for the diffuse term.

### Deferred Shading

### Environment IBL

![IBLDemo](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/7841c4ee-90c0-4aa9-a875-d3993cf7becd)

To address environment lights, the split-sum approximation as described by Karis in his 2013 Siggraph presentation notes [[1]](#1) has been used.<br>
Loading an environment for a 3D scene using **Tao** is as simple as providing the renderer with a valid path to a .hdr image which contains an equirectangular projected environment capture.
The renderer takes care of processing the image, without the need to use specialized software.

### Area Lights
In addition to directional lights, **Tao** features spherical and rectangular lights:
* Sphere lights are rendered by using the _Representative point_ method as described by Karis [[1]](#1).
* Rectangular lights are achieved using a technique known as _Linearly transformed cosine_ (Article[[2]](#2), [Implementation](https://github.com/selfshadow/ltc_code/tree/master#webgl-demos)).

### Contact Hardening Soft Shadows
![PCSS](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/a65aa6d0-853b-4bd0-892c-42270b1b067d)

CHSS for directional and area light implementation is based on the concepts presented in the article "Percentage-Closer Soft Shadows"[[3]](#3). 

## Gizmos Renderer Features
![GizmoDemo](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/48a36730-dd48-4079-8a03-4b461706fb3b)

**GizmosRenderer** is a flexible component that can be used to draw meshes, lines, line-strips and points which has been developed with overlay-drawing and debug visualization in mind.<br>

The GIF shows all the different elements drawn using this component:
* The XY-plane grid
* The small view-oriented cube on the top-right corner
* The light icons
* The handles used to manipulate a light's transformation (the arrow triad for example).  

### Zoom Invariance
![ZoomInvariance](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/e91d6ecc-acd0-4942-b89f-01e9d2387950)

Gizmos can optionally be drawn in the 3D scene so that they always cover the same area on the screen. 

### Constant Screen-Lenght Texture Mapping
![ConstantScreenLength](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/428af1d6-b9ae-4838-989d-563ade5e7450)

For lines and line-strip gizmos, an option is available that maps texture coordinates so they match the primitive screen length.
This allows for effects similar to the ones achieved with [glLineStipple](https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glLineStipple.xml) while being a much more powerful tool.

### Mouse Pick

Gizmos selection/picking has been achieved with an off-screen drawing where entity IDs have been color encoded.<br>
By using a stack of [Pixel Buffer Objects](https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object) and [Fences](https://www.khronos.org/opengl/wiki/Sync_Object), reading the result of the off-screen drawing doesn't stall the pipeline.


### Configurable Pipeline

**Gizmos** are assigned to render layers. A render layer contains a set of pipeline state configurations (depth-stencil/rasterizer/blend). 
These layers can be configured at will by the user, allowing for a variety of effects. The view-aligned cube on the top-right corner of the screen, for example,  which is shown in some of the GIFs and images, is drawn by using multiple layers (to mask visible and hidden edges).

### Shader Graph

To allow for custom shaders, a _shader-graph-like_ mechanism has been developed to write and pass shaders to **GizmosRenderer**.
The idea is to let the user write some normal-looking C++ code while building a graph under the hood. The graph is then converted to GLSL code, pasted onto an existing template, and compiled as an OpenGL shader.
This approach is an alternative to the simpler "pass the shader to the renderer as a string".<br>

Some key advantages of this approach
* **Shader details are hidden:** for example which inputs/outputs are available to a shader stage and what are their names
* **Decoupling of logic and implementation:** the same graph could be turned into HLSL code for example
* **Type checking:** the IDE will highlight C++ errors which would be, in turn, GLSL errors
* **Easily convertible to a visual shader editor:** similar to Blender's node editor and Unity's shader graph

The infinite grid on the XY plane is drawn using a fragment shader generated using this shader-graph mechanism. 

The following is an example of a shader written using this method (_this shader is meant to be just an example, not sure if it actually accomplishes anything useful_):<br>

**C++ code**

```cpp
auto kHighlight   = SGConstVec(0.8f, 0.2f, 0.2f, 1.0f);
auto kMinOpacity  = SGConstVec(0.3f);
auto kMaxOpacity  = SGConstVec(0.8f);

auto vertCol = SGInputVertexColor();
auto vertPos = SGInputVertexPosition();
auto vertNrm = SGInputVertexNormal();
auto eyePos  = SGInputEyePosition();

auto NoV = SGDot(SGNorm(eyePos - vertPos), vertNrm);

auto opacity = SGMix(kMinOpacity, kMaxOpacity, NoV);
auto color   = SGMix(vertCol, kHighlight, NoV);

auto out = SGOutColor(SGVec(SGSwizzleX(color), SGSwizzleY(color), SGSwizzleZ(color), opacity));
```

**Graph** (exported as .tgf and visualized using [yEd](https://www.yworks.com/products/yed))

![shaderGraph](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/a614a43b-d054-4119-b622-534eb6a0f33b)


**Auto-generated GLSL code**

```cpp
/* The following code was auto-generated */ 
void main()
{
  float sgVar_1 = dot( normalize( ( ( f_viewPos.xyz ) - ( fs_in.v_position ) ) ), fs_in.v_normal );
  vec4 sgVar_0 = mix( fs_in.v_color, const vec4(0.800000,0.200000,0.200000,1.000000), sgVar_1 );
  FragColor = vec4( ( (sgVar_0).x ), ( (sgVar_0).y ), ( (sgVar_0).z ), ( mix( const float(0.300000), const float(0.800000), sgVar_1 ) ) );
}
```

### Procedural Textures

All the icons for the different light gizmos, as well as the line patterns, have been generated programmatically by using a number of useful utilities provided by **GizmosRenderer**.<br>

Most notably, it provides a mechanism to create complex [SDFs](https://iquilezles.org/articles/distfunctions2d/) by intuitively chaining different operations, such as transformations, domain repetition, and more.<br>

The following code section shows what this SDF-manipulation API looks like from the user's perspective.

**Example: Rect light gizmo icon** (upscaled)

<img src="https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/f9113d30-ec26-4c54-aa99-596b876b98a1" width="256" height="256"/>

```cpp
auto sdfBody =
    SdfRoundedRect{vec2{rw, rh}, vec4{radius, 0.0f, radius, 0.0f}}
        .Translate(vec2{0.0f, (rw - rh) * 0.5f});

auto sdfMask =
    SdfRoundedRect{vec2{rw, rh}, vec4{radius, 0.0f, radius, 0.0f}}
        .Scale(vec2{0.8f})
        .Translate(vec2{0.0f, (rw - rh) * 0.5f});

auto sdfBase =
    SdfRoundedRect{vec2{rw, rw}, vec4{radius}}
        .Subtract(sdfBody)
        .Subtract(sdfBody
                      .Scale(vec2{2.0f, 1.0f})
                      .Translate(vec2{0.0f, -h_10}));

auto sdfLEDs =
    SdfRect{vec2(h_20)}
        .Translate(vec2{h_5 * 0.5f})
        .RepeatGrid(vec2{h_5})
        .Translate(vec2{h_5 * 0.5f})
        .Intersect(sdfMask);
```



# References
<a id="1">[1]</a> [Real Shading in Unreal Engine 4](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)<br>
<a id="2">[2]</a> [Real-Time Polygonal-Light Shading with Linearly Transformed Cosines](https://eheitzresearch.wordpress.com/415-2/)<br>
<a id="3">[3]</a> [Percentage-closer Soft Shadows](https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf)<br>


# 3D Models and HDRIs
* [Damaged Helmet](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/DamagedHelmet)
* [Apollo and Daphne](https://skfb.ly/oE866)
* [Viper Steam Carbine](https://skfb.ly/oxyAE)
* [Brown Photostudio 05](https://polyhaven.com/a/brown_photostudio_05)
* [Pine Attic](https://polyhaven.com/a/pine_attic)
* [Limpopo Golf Course](https://polyhaven.com/a/limpopo_golf_course)
* [Drackenstein Quarry](https://polyhaven.com/a/drackenstein_quarry)




