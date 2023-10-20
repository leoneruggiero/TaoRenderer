#pragma once

#include "Resources.h"
#include "RenderContext.h"
#include <vector>
#include <string>
#include <glm\glm.hpp>
#include <stdexcept>
#include <functional>
namespace tao_render_context
{
    class RenderContext;

    class WindowCompositor
    {
    public:

        enum blend_option
        {
            copy,
            alpha_blend,
        };

        struct location
        {
            int x;
            int y;
            int width;
            int height;
        };

        WindowCompositor(RenderContext& renderContext, int width, int height);

        [[nodiscard]] WindowCompositor& AddLayer(tao_ogl_resources::OglTexture2D& layerTex, location loc, blend_option blend);

        /*[[nodiscard]] tao_ogl_resources::OglTexture2D*/ void GetResult();

        void ClearLayers();

        void Resize(int newWidth, int newHeight);

    private:
        tao_render_context::RenderContext*              _renderContext;
        tao_ogl_resources::OglVertexBuffer              _vbo;
        tao_ogl_resources::OglVertexAttribArray         _vao;
        tao_ogl_resources::OglShaderProgram             _shader;
        tao_ogl_resources::OglSampler                   _linearSampler;
        //tao_ogl_resources::OglTexture2D                 _colorTexture;
        //tao_ogl_resources::OglFramebuffer<OglTexture2D> _framebuffer;

        ogl_blend_state         _blendStateBlendingOn;
        ogl_blend_state         _blendStateBlendingOff;
        ogl_depth_state         _depthState;
        ogl_rasterizer_state    _rasterizerState;

        struct blendOperation
        {
            tao_ogl_resources::OglTexture2D *texture;
            location                         location;
            blend_option                     operation;
        };

        std::vector<blendOperation> _operations;
    };

    class ShaderLoader
    {
    public:
        static std::string LoadShader(const char *shaderSourceFile, const char *shaderSourceDir, const char *shaderIncludeDir);
        static std::string DefineConditional(const std::string &shaderSource, const std::vector<std::string> &definitions);
        static std::string ReplaceSymbols(const std::string &shaderSource, const std::vector<std::pair<std::string, std::string>> &replacements);

    private:
        static constexpr const char *INCLUDE_DIRECTIVE = "//! #include";
        static constexpr const char *DEFINE_DIRECTIVE = "#define";
        static constexpr const char *EXC_PREAMBLE = "ShaderLoader: ";
    };

    class BufferDataPacker
    {
    public:
        BufferDataPacker() :
                _dataArrays(0)
        {
        }

        template<glm::length_t L, typename T>
        BufferDataPacker &AddDataArray(const std::vector<glm::vec<L, T>> &data)
        {
            DataPtrWithFormat dataPtr
                    {
                            .elementSize    = sizeof(T) * L,
                            .elementsCount    = static_cast<unsigned int>(data.size()),
                            .dataPtr        = reinterpret_cast<const unsigned char *>(data.data())
                    };

            _dataArrays.push_back(dataPtr);
            return *this;
        }

        template<typename T>
        BufferDataPacker &AddDataArray(const std::vector<T> &data)
        {
            DataPtrWithFormat dataPtr
                    {
                            .elementSize    = sizeof(T),
                            .elementsCount    = static_cast<unsigned int>(data.size()),
                            .dataPtr        = reinterpret_cast<const unsigned char *>(data.data())
                    };

            _dataArrays.push_back(dataPtr);
            return *this;
        }

        template<glm::length_t C, glm::length_t R, typename T>
        BufferDataPacker &AddDataArray(const std::vector<glm::mat<C, R, T>> &data)
        {
            DataPtrWithFormat dataPtr
                    {
                            .elementSize    = sizeof(T) * C * R,
                            .elementsCount    = static_cast<unsigned int>(data.size()),
                            .dataPtr        = reinterpret_cast<const unsigned char *>(data.data())
                    };

            _dataArrays.push_back(dataPtr);
            return *this;
        }

        [[nodiscard]] std::vector<unsigned char> InterleavedBuffer(unsigned int alignment  = 1) const
        {
            // early exit if empty
            if (_dataArrays.empty()) return std::vector<unsigned char>{};

            // check all the arrays to be
            // the same length
            const unsigned int elementCount = _dataArrays[0].elementsCount;
            unsigned int cumulatedElementSize = 0;
            for (const auto &d: _dataArrays) {
                if (d.elementsCount != elementCount)
                    throw std::runtime_error(
                            "BufferDataPacker: Invalid input. Interleaving requires all the arrays to be the same length.");

                cumulatedElementSize += d.elementSize;
            }

            if(alignment>1)
            {
                cumulatedElementSize = (cumulatedElementSize/alignment)*alignment + ((cumulatedElementSize % alignment) ? alignment : 0);
            }

            std::vector<unsigned char> interleaved(elementCount * cumulatedElementSize);

            for (int i = 0; i < elementCount; i++)
            {
                unsigned int accum = 0;
                for (int j = 0; j < _dataArrays.size(); j++)
                {
                    memcpy(interleaved.data() + (cumulatedElementSize * i + accum),
                           _dataArrays[j].dataPtr + (i * _dataArrays[j].elementSize),
                           _dataArrays[j].elementSize
                           );
                    accum += _dataArrays[j].elementSize;
                }
            }

            return interleaved;
        }

    private:
        struct DataPtrWithFormat
        {
            unsigned int elementSize;
            unsigned int elementsCount;
            const unsigned char *dataPtr;
        };

        std::vector<DataPtrWithFormat> _dataArrays;

    };

    template
            <typename Buff,
                    Buff CreateFunc(tao_render_context::RenderContext&, tao_ogl_resources::ogl_buffer_usage),
                    void ResizeFunc(Buff&, unsigned int, tao_ogl_resources::ogl_buffer_usage)>
    requires tao_ogl_resources::ogl_buffer<typename Buff::ogl_resource_type>
    class ResizableOglBuffer
    {

    private:
        unsigned int                        _capacity = 0;
        tao_ogl_resources::ogl_buffer_usage _usage;
        Buff								_oglBuffer;
        std::function<unsigned int(unsigned int, unsigned int)> _resizePolicy;
    public:
        // Const getters
        unsigned int Capacity()						const { return _capacity; }
        tao_ogl_resources::ogl_buffer_usage Usage() const { return _usage; }

        // Vbo getter (non const)
        Buff& OglBuffer() { return _oglBuffer; }

        ResizableOglBuffer(
                tao_render_context::RenderContext& rc,
                unsigned int capacity,
                tao_ogl_resources::ogl_buffer_usage usage,
                const std::function<unsigned int(unsigned int, unsigned int)>& resizePolicy):

                _capacity{ capacity },
                _usage{ usage },
                _oglBuffer{ CreateFunc(rc, _usage) },
                _resizePolicy{resizePolicy}
        {
        }

        bool Resize(unsigned int capacity)
        {
            const auto newCapacity = _resizePolicy(_capacity, capacity);

            bool resized = false;

            if(_capacity!=newCapacity)
            {
                _capacity = newCapacity;
                ResizeFunc(_oglBuffer, _capacity, _usage);
                resized = true;
            }

            return resized;
        }
    };

    unsigned int ResizeBufferPolicy(unsigned int currVertCapacity, unsigned int newVertCount);

    // Resizable VBO
    /////////////////////////////////
    tao_ogl_resources::OglVertexBuffer CreateEmptyVbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg);
    void ResizeVbo(tao_ogl_resources::OglVertexBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg);

    typedef ResizableOglBuffer<tao_ogl_resources::OglVertexBuffer, CreateEmptyVbo, ResizeVbo> ResizableVbo;

    // Resizable EBO
    /////////////////////////////////
    tao_ogl_resources::OglIndexBuffer  CreateEmptyEbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg);
    void ResizeEbo(tao_ogl_resources::OglIndexBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg);

    typedef ResizableOglBuffer<tao_ogl_resources::OglIndexBuffer, CreateEmptyEbo, ResizeEbo> ResizableEbo;

    // Resizable SSBO
    /////////////////////////////////
    tao_ogl_resources::OglShaderStorageBuffer  CreateEmptySsbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg);
    void	ResizeSsbo(tao_ogl_resources::OglShaderStorageBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg);

    typedef ResizableOglBuffer<tao_ogl_resources::OglShaderStorageBuffer, CreateEmptySsbo, ResizeSsbo> ResizableSsbo;

    // Resizable UBO
    /////////////////////////////////
    tao_ogl_resources::OglUniformBuffer  CreateEmptyUbo(RenderContext& rc, tao_ogl_resources::ogl_buffer_usage usg);
    void	ResizeUbo(tao_ogl_resources::OglUniformBuffer& buff, unsigned int newCapacity, tao_ogl_resources::ogl_buffer_usage usg);

    typedef ResizableOglBuffer<tao_ogl_resources::OglUniformBuffer, CreateEmptyUbo, ResizeUbo> ResizableUbo;


}