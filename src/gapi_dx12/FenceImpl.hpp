#pragma once

#include "gapi/Fence.hpp"

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            class CommandQueueImpl;
            class ResourceReleaseContext;

            class FenceImpl final : public IFence
            {
            public:
                FenceImpl() = default;

                void ReleaseD3DObjects(ResourceReleaseContext& releaseContext);

                Result Init(const ComSharedPtr<ID3D12Device>& device, const U8String& name, uint64_t initialValue = 0);

                Result Signal(const std::shared_ptr<CommandQueue>& queue) override;
                Result Signal(CommandQueueImpl& queue);

                // TODO infinity
                Result SyncCPU(std::optional<uint64_t> value, uint32_t timeout = 0xFFFFFF) const override;
                Result SyncGPU(const std::shared_ptr<CommandQueue>& queue) const override;

                uint64_t GetGpuValue() const override
                {
                    ASSERT(D3DFence_)
                    return D3DFence_->GetCompletedValue();
                }
                uint64_t GetCpuValue() const override { return cpuValue_; }

                const ComSharedPtr<ID3D12Fence>& GetD3DObject() const { return D3DFence_; }

            private:
                HANDLE event_ = 0;
                ComSharedPtr<ID3D12Fence> D3DFence_ = nullptr;
                uint64_t cpuValue_ = 0;
            };
        }
    }
}