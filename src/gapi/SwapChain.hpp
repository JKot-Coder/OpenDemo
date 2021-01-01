#pragma once

#include "gapi/ForwardDeclarations.hpp"
#include "gapi/Limits.hpp"
#include "gapi/Object.hpp"
#include "gapi/Result.hpp"

#include "common/NativeWindowHandle.hpp"

namespace OpenDemo
{
    namespace GAPI
    {
        struct SwapChainDescription
        {
            NativeWindowHandle windowHandle;

            uint32_t width;
            uint32_t height;
            uint32_t bufferCount;

            ResourceFormat resourceFormat;
            bool isStereo;

        public:
            SwapChainDescription() {};
            SwapChainDescription(NativeWindowHandle windowHandle, uint32_t width, uint32_t height, uint32_t bufferCount, ResourceFormat resourceFormat, bool isStereo = false)
                : windowHandle(windowHandle),
                  width(width),
                  height(height),
                  bufferCount(bufferCount),
                  resourceFormat(resourceFormat),
                  isStereo(isStereo)
            {
            }
        };

        class SwapChainInterface
        {
        public:
            virtual Result InitBackBufferTexture(uint32_t backBufferIndex, const std::shared_ptr<Texture>& resource) = 0;

            virtual Result Reset(const SwapChainDescription& description, const std::array<std::shared_ptr<Texture>, MAX_BACK_BUFFER_COUNT>& backBuffers) = 0;
        };

        class SwapChain final : public InterfaceWrapObject<SwapChainInterface>
        {
        public:
            using SharedPtr = std::shared_ptr<SwapChain>;
            using SharedConstPtr = std::shared_ptr<const SwapChain>;

            std::shared_ptr<Texture> GetTexture(uint32_t backBufferIndex);

            const SwapChainDescription& GetDescription() const { return description_; }

        private:
            static SharedPtr Create(const SwapChainDescription& description, const U8String& name)
            {
                return SharedPtr(new SwapChain(description, name));
            }

            SwapChain(const SwapChainDescription& description, const U8String& name);

            Result Reset(const SwapChainDescription& description);

            inline Result InitBackBufferTexture(uint32_t backBufferIndex, const std::shared_ptr<Texture>& resource) { return GetInterface()->InitBackBufferTexture(backBufferIndex, resource); }

        private:
            SwapChainDescription description_;
            std::array<std::shared_ptr<Texture>, MAX_BACK_BUFFER_COUNT> backBuffers_;

            friend class Render::RenderContext;
        };
    }
}