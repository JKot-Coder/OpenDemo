#pragma once

#include "gapi/Resource.hpp"
#include <optional>

namespace OpenDemo
{
    namespace GAPI
    {
        class IFence
        {
        public:
            virtual ~IFence() {};

            virtual void Signal(const std::shared_ptr<CommandQueue>& queue) = 0;

            virtual void SyncCPU(std::optional<uint64_t> value, uint32_t timeout) const = 0;
            virtual void SyncGPU(const std::shared_ptr<CommandQueue>& queue) const = 0;

            virtual uint64_t GetGpuValue() const = 0;
            virtual uint64_t GetCpuValue() const = 0;
        };

        class Fence final : public Resource<IFence>
        {
        public:
            using SharedPtr = std::shared_ptr<Fence>;
            using SharedConstPtr = std::shared_ptr<const Fence>;

            inline void Signal(const std::shared_ptr<CommandQueue>& queue) { return GetPrivateImpl()->Signal(queue); }
            inline void SyncCPU(std::optional<uint64_t> value, uint32_t timeout) const { return GetPrivateImpl()->SyncCPU(value, timeout); }
            inline void SyncGPU(const std::shared_ptr<CommandQueue>& queue) const { return GetPrivateImpl()->SyncGPU(queue); }

            inline uint64_t GetGpuValue() const { return GetPrivateImpl()->GetGpuValue(); }
            inline uint64_t GetCpuValue() const { return GetPrivateImpl()->GetCpuValue(); }

        private:
            template <class Deleter>
            static SharedPtr Create(const U8String& name, Deleter)
            {
                return SharedPtr(new Fence(name), Deleter());
            }

            Fence(const U8String& name)
                : Resource(Object::Type::Fence, name)
            {
            }

        private:
            friend class Render::RenderContext;
        };
    }
}