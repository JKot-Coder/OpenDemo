#include "FenceImpl.hpp"

namespace OpenDemo
{
    namespace Render
    {
        namespace Device
        {
            namespace DX12
            {

                GAPIStatus FenceImpl::Init(ID3D12Device* device, uint64_t initialValue)
                {
                    ASSERT(device)
                    ASSERT(_fence.get() == nullptr)

                    GAPIStatus result = GAPIStatus::OK;

                    if (GAPIStatusU::Failure(result = GAPIStatus(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.put())))))
                    {
                        LOG_ERROR("Failure create CreateFence with HRESULT of 0x%08X", result);
                        return result;
                    }

                    return result;
                }

                GAPIStatus FenceImpl::Signal(ID3D12CommandQueue* commandQueue, uint64_t value) const
                {
                    ASSERT(_fence.get())

                    GAPIStatus result = GAPIStatus(commandQueue->Signal(_fence.get(), value));

                    if (GAPIStatusU::Failure(result))
                        LOG_ERROR("Failure signal fence with HRESULT of 0x%08X", result);

                    return result;
                }

                GAPIStatus FenceImpl::SetEventOnCompletion(uint64_t value, HANDLE event) const
                {
                    ASSERT(_fence.get())

                    GAPIStatus result = GAPIStatus(_fence->SetEventOnCompletion(value, event));

                    if (GAPIStatusU::Failure(result))
                        LOG_ERROR("Failure SetEventOnCompletion fence with HRESULT of 0x%08X", result);

                    return result;
                }

                uint64_t FenceImpl::GetGpuValue() const
                {
                    ASSERT(_fence.get())
                    return _fence->GetCompletedValue();
                }
            }
        }
    }
}