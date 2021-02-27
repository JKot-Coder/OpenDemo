#pragma once

#include "gapi/GpuResource.hpp"

namespace OpenDemo
{
    namespace GAPI
    {
        enum class TextureDimension : uint32_t
        {
            Unknown,

            Texture1D,
            Texture2D,
            Texture2DMS,
            Texture3D,
            TextureCube
        };

        struct TextureDescription
        {
            static constexpr uint32_t MaxPossible = 0xFFFFFF;

            static TextureDescription Create1D(uint32_t width, GpuResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = MaxPossible)
            {
                return TextureDescription(TextureDimension::Texture1D, width, 1, 1, format, 1, arraySize, mipLevels);
            }

            static TextureDescription Create2D(uint32_t width, uint32_t height, GpuResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = MaxPossible)
            {
                return TextureDescription(TextureDimension::Texture2D, width, height, 1, format, 1, arraySize, mipLevels);
            }

            static TextureDescription Create2DMS(uint32_t width, uint32_t height, GpuResourceFormat format, uint32_t sampleCount, uint32_t arraySize = 1)
            {
                return TextureDescription(TextureDimension::Texture2DMS, width, height, 1, format, sampleCount, arraySize, 1);
            }

            static TextureDescription Create3D(uint32_t width, uint32_t height, uint32_t depth, GpuResourceFormat format, uint32_t mipLevels = MaxPossible)
            {
                return TextureDescription(TextureDimension::Texture3D, width, height, depth, format, 1, 1, mipLevels);
            }

            static TextureDescription CreateCube(uint32_t width, uint32_t height, GpuResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = MaxPossible)
            {
                return TextureDescription(TextureDimension::TextureCube, width, height, 1, format, 1, arraySize, mipLevels);
            }

            uint32_t GetNumSubresources() const
            {
                constexpr uint32_t planeSlices = 1;
                const uint32_t numSets = (dimension == TextureDimension::TextureCube ? 6 : 1);
                return planeSlices * numSets * arraySize * mipLevels;
            }

            uint32_t GetMaxMipLevel() const
            {
                const uint32_t maxDimension = std::max(width, std::max(height, depth));
                return 1 + static_cast<uint32_t>(log2(static_cast<float>(maxDimension)));
            }

            inline friend bool operator==(const TextureDescription& lhs, const TextureDescription& rhs)
            {
                return lhs.format == rhs.format &&
                       lhs.dimension == rhs.dimension &&
                       lhs.width == rhs.width &&
                       lhs.height == rhs.height &&
                       lhs.depth == rhs.depth &&
                       lhs.mipLevels == rhs.mipLevels &&
                       lhs.sampleCount == rhs.sampleCount &&
                       lhs.arraySize == rhs.arraySize;
            }
            inline friend bool operator!=(const TextureDescription& lhs, const TextureDescription& rhs) { return !(lhs == rhs); }

            GpuResourceFormat format;
            TextureDimension dimension = TextureDimension::Unknown;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t depth = 0;
            uint32_t mipLevels = 0;
            uint32_t sampleCount = 0;
            uint32_t arraySize = 0;

        private:
            TextureDescription(TextureDimension dimension, uint32_t width, uint32_t height, uint32_t depth, GpuResourceFormat format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels)
                : dimension(dimension),
                  width(width),
                  height(height),
                  depth(depth),
                  format(format),
                  sampleCount(sampleCount),
                  arraySize(arraySize),
                  // Limit/Calc maximum mip count
                  mipLevels(std::min(GetMaxMipLevel(), mipLevels))
            {
            }
        };

        /*
        struct TextureSubresourceData
        {
            TextureSubresourceData(void* data, size_t size, size_t rowPitch, size_t depthPitch)
                : data(data), size(size), rowPitch(rowPitch), depthPitch(depthPitch) { }

            void* data;
            size_t size;
            size_t rowPitch;
            size_t depthPitch;
        };*/

        class IntermediateMemory : public std::enable_shared_from_this<IntermediateMemory>
        {
        public:
            using SharedPtr = std::shared_ptr<IntermediateMemory>;
            using SharedConstPtr = std::shared_ptr<const IntermediateMemory>;

            struct SubresourceFootprint
            {
                SubresourceFootprint() = default;
                SubresourceFootprint(size_t offset, uint32_t numRows, uint32_t rowSizeInBytes, size_t rowPitch, size_t depthPitch)
                    : offset(offset), numRows(numRows), rowSizeInBytes(rowSizeInBytes), rowPitch(rowPitch), depthPitch(depthPitch) { }

                bool isComplatable(const SubresourceFootprint& other) const
                {
                    return (numRows == other.numRows) &&
                           (rowSizeInBytes == other.rowSizeInBytes);
                }

                size_t offset;
                uint32_t numRows;
                size_t rowSizeInBytes;
                size_t rowPitch;
                size_t depthPitch;
            };

            IntermediateMemory(const std::shared_ptr<MemoryAllocation>& allocation, const std::vector<SubresourceFootprint>& subresourceFootprints, uint32_t firstSubresource)
                : allocation_(allocation), subresourceFootprints_(subresourceFootprints), firstSubresource_(firstSubresource)
            {
                ASSERT(allocation);
                ASSERT(GetNumSubresources() > 0);
            };

            inline std::shared_ptr<MemoryAllocation> GetAllocation() const { return allocation_; }
            inline uint32_t GetFirstSubresource() const { return firstSubresource_; }
            inline size_t GetNumSubresources() const { return subresourceFootprints_.size(); }
            inline const SubresourceFootprint& GetSubresourceFootprintAt(uint32_t index) const { return subresourceFootprints_[index]; }
            inline const std::vector<SubresourceFootprint>& GetSubresourceFootprints() const { return subresourceFootprints_; }

            void CopyDataFrom(const GAPI::IntermediateMemory::SharedPtr& source);

        private:
            std::shared_ptr<MemoryAllocation> allocation_;
            std::vector<SubresourceFootprint> subresourceFootprints_;
            uint32_t firstSubresource_;
        };

        class Texture final : public GpuResource
        {
        public:
            using SharedPtr = std::shared_ptr<Texture>;
            using SharedConstPtr = std::shared_ptr<const Texture>;

            static constexpr uint32_t MaxPossible = 0xFFFFFF;

        public:
            std::shared_ptr<ShaderResourceView> GetSRV(uint32_t mipLevel = 0, uint32_t mipCount = MaxPossible, uint32_t firstArraySlice = 0, uint32_t numArraySlices = MaxPossible);
            std::shared_ptr<RenderTargetView> GetRTV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t numArraySlices = MaxPossible);
            std::shared_ptr<DepthStencilView> GetDSV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t numArraySlices = MaxPossible);
            std::shared_ptr<UnorderedAccessView> GetUAV(uint32_t mipLevel, uint32_t firstArraySlice = 0, uint32_t numArraySlices = MaxPossible);

            const TextureDescription& GetDescription() const { return description_; }

        private:
            template <class Deleter>
            static SharedPtr Create(
                const TextureDescription& description,
                GpuResourceBindFlags bindFlags,
                GpuResourceCpuAccess cpuAccess,
                const U8String& name,
                Deleter)
            {
                return SharedPtr(new Texture(description, bindFlags, cpuAccess, name), Deleter());
            }

            Texture(const TextureDescription& description, GpuResourceBindFlags bindFlags, GpuResourceCpuAccess cpuAccess, const U8String& name);

        private:
            TextureDescription description_;

            friend class Render::RenderContext;
        };
    }
}