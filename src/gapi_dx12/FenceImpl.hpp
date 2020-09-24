#pragma once

namespace OpenDemo
{
    namespace Render
    {
        namespace DX12
        {
            class FenceImpl final
            {
            public:
                // _commandList->SetName(L"CommandList");
                FenceImpl() = default;
                GAPIStatus Init(ID3D12Device* device, uint64_t initialValue, const U8String& name);

                GAPIStatus Signal(ID3D12CommandQueue* commandQueue, uint64_t value);
                GAPIStatus SetEventOnCompletion(uint64_t value, HANDLE event) const;

                uint64_t GetGpuValue() const;
                uint64_t GetCpuValue() const { return _cpu_value; }

            private:
                ComSharedPtr<ID3D12Fence> _fence;
                uint64_t _cpu_value;
            };
        }
    }
}