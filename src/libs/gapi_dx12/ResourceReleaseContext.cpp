#include "ResourceReleaseContext.hpp"

#include "gapi_dx12/FenceImpl.hpp"
#include "gapi_dx12/third_party/d3d12_memory_allocator/D3D12MemAlloc.h"

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            ResourceReleaseContext::~ResourceReleaseContext()
            {
                Threading::ReadWriteGuard lock(mutex_);
                ASSERT(queue_.size() == 0);
            }

            void ResourceReleaseContext::Init()
            {
                fence_ = std::make_unique<FenceImpl>();
                fence_->Init("ResourceRelease");
            }

            void ResourceReleaseContext::deferredD3DResourceRelease(const ComSharedPtr<IUnknown>& resource, D3D12MA::Allocation* allocation)
            {
                Threading::ReadWriteGuard lock(mutex_);

                ASSERT(fence_);
                ASSERT(resource);

                queue_.push({ fence_->GetCpuValue(), resource, allocation });
            }

            void ResourceReleaseContext::ExecuteDeferredDeletions(const std::shared_ptr<CommandQueueImpl>& queue)
            {
                Threading::ReadWriteGuard lock(mutex_);

                ASSERT(fence_);
                ASSERT(queue);

                const auto gpuFenceValue = fence_->GetGpuValue();
                while (queue_.size() && queue_.front().cpuFrameIndex < gpuFenceValue)
                {
                    auto& allocation = queue_.front().allocation;
                    
                    if (allocation)
                        allocation->Release();

                    allocation = nullptr;

                    queue_.pop();
                }

                fence_->Signal(*queue.get());
            }
        }
    }
}