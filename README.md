# Tao Renderer
![TaoRendererCover](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/c0aa429e-233f-4130-8e50-b468a4e5d98d)

**Tao** is an OpenGL renderer designed as a playground for experimenting with modern OpenGL concepts, computer graphics techniques, and learning a bit of C++ along the way. The renderer comprises several key components:
* **RenderContext:** This component creates a window, an OpenGL context, and offers abstractions for OpenGL.
* **PbrRenderer:** It is responsible for rendering 3D scenes using physically-based rendering techniques.
* **GizmosRenderer:** This component handles the rendering of various useful extras in a 3D application, including debug visualizations and overlays.

The different components can communicate since they share the same underlying OpenGL abstraction provided by **RenderContex**.

## Pbr Renderer Features
### Pbr materials
![TaoRendererPbrMaterials](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/f5c07302-ea08-42db-b49f-d5b87cd35085)

**Cook-Torrance** has been chosen as the specular BRDF of choice for this renderer. It is considered the standard in a number of articles and technical publications.
For the diffuse term, the renderer employs the **Lambertian** model.

### Deferred Shading

### Environment IBL

![IBLDemo](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/4aa29199-2e47-423b-b425-f030482a3434)

To address environment image-based lighting, the split-sum approximation as described by Karis in his 2013 Siggraph presentation notes [[1]](#1) has been used.<br>
Loading an environment for a 3D scene using **Tao** is as simple as providing the renderer with a valid path to a .hdr image which contains an equirectangular projected environment capture.
The renderer takes care of processing the image, without the need to use specialized software.

### Area Lights
In addition to directional lights, **Tao** features spherical and rectangular lights:
* **Sphere lights** are rendered by using the _Representative point_ method, as described by Karis [[1]](#1).
* **Rectangular lights** are implemented using a technique known as _Linearly transformed cosine_ (Article[[2]](#2), [Implementation](https://github.com/selfshadow/ltc_code/tree/master#webgl-demos)).

### Contact Hardening Soft Shadows
![PCSS](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/9b1e5259-67bd-4033-8416-abc7c71d8cee)

CHSS for directional and area light implementation is based on the concepts presented in the article "Percentage-Closer Soft Shadows"[[3]](#3). 

## Gizmos Renderer Features
![GizmoDemo](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/3b514f33-9859-439d-b63b-fd26689d7b17)

**GizmosRenderer** is a versatile component designed for drawing meshes, lines, line-strips, and points. It has been developed with a focus on overlay drawing and facilitating debug visualization.<br>

The GIF shows all the different elements drawn using this component:
* The XY-plane grid
* The small view-oriented cube on the top-right corner
* The light icons
* The handles used to manipulate a light's transformation (the arrow triad for example).  

### Zoom Invariance
![ZoomInvariance](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/3708a8cf-55e5-4e6c-aec4-9f810c2030d1)

Gizmos can be drawn in the 3D scene with the option to maintain a consistent screen space coverage regardless of zoom level.

### Constant Screen-Lenght Texture Mapping
![ConstantScreenLength](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/3fdb2f8e-eae9-49d5-9155-4215b540da15)

For lines and line-strip gizmos, there is an option available that maps texture coordinates to match the primitive's screen length. This feature enables effects similar to those achieved with [glLineStipple](https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/glLineStipple.xml) while offering greater versatility and control.

### Mouse Pick

Gizmos selection and picking are accomplished through an off-screen drawing where entity IDs are encoded using color. This approach leverages [Pixel Buffer Objects](https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object) and [Fences](https://www.khronos.org/opengl/wiki/Sync_Object) to ensure that reading the results of the off-screen drawing operation does not stall the pipeline.


### Configurable Pipeline

**Gizmos** are associated with specific render layers. Each render layer encompasses a collection of pipeline state configurations, such as depth-stencil, rasterizer, and blend settings. Users have the freedom to configure these layers as needed, enabling a wide range of visual effects. For instance, the view-aligned cube featured in several GIFs and images on the top-right corner of the screen is drawn using multiple layers to manage visible and hidden edges.

### Shader Graph

To enable the creation of custom shaders, a _shader-graph-like_ mechanism has been developed to write and pass shaders to **GizmosRenderer**.
The idea is to let the user write some normal-looking C++ code while building a graph under the hood. The graph is then converted to GLSL code, pasted onto an existing template, and compiled as an OpenGL shader.
This approach offers an alternative to the simpler method of passing a shader as a string to the renderer.<br>

Some key advantages of this approach
* **Shader details are hidden:** for example which inputs/outputs are available to a shader stage and what are their names
* **Decoupling of logic and implementation:** the same graph could be turned into GLSL or HLSL code for example
* **Error checking:** GLSL errors corresponds to C++ errors
* **Easily convertible to a visual shader editor:** this approach sets the stage for creating a visual shader editor similar to Blender's node editor and Unity's shader graph

The infinite grid on the XY plane is drawn using a fragment shader generated using this shader-graph mechanism. 

Below is an illustrative example of a shader created using this method (_please note that this shader serves as an example and may not have a practical purpose_):

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

![shaderGraph](https://github.com/leoneruggiero/TaoRenderer/assets/55357743/c3949e53-99b3-43f7-8ba5-33d7e1005bce)

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

All the icons for the different light gizmos, as well as the line patterns,have been generated programmatically using a range of convenient utilities provided by **GizmosRenderer**.<br>

Notably, it provides a mechanism for creating complex [SDFs](https://iquilezles.org/articles/distfunctions2d/) by intuitively chaining different operations including transformations and domain repetitions.<br>

The following code section demonstrates how this SDF manipulation API appears from the user's perspective:

**Example: Rect light gizmo icon** (upscaled)

<img src="https://github.com/leoneruggiero/TaoRenderer/assets/55357743/19d45e8a-955f-4c91-ba06-17339bcb5772" width="256" height="256"/>

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




