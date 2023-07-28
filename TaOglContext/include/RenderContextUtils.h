#pragma once

#include "OglUtils.h"
#include <vector>
#include <string>
#include <glm\glm.hpp>
#include <stdexcept>

namespace tao_render_context
{
    class ShaderLoader
    {
    public:
        static std::string
        LoadShader(const char *shaderSourceFile, const char *shaderSourceDir, const char *shaderIncludeDir);

        static void DefineConditional(std::string &shaderSource, const std::vector<std::string> &definitions);

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

        [[nodiscard]] std::vector<unsigned char> InterleavedBuffer() const
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

            std::vector<unsigned char> interleaved(elementCount * cumulatedElementSize);

            for (int i = 0; i < elementCount; i++)
            {
                unsigned int accum = 0;
                for (int j = 0; j < _dataArrays.size(); j++)
                {
                    memcpy(interleaved.data() + cumulatedElementSize * i + accum,
                           _dataArrays[j].dataPtr + i * _dataArrays[j].elementSize,
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

}