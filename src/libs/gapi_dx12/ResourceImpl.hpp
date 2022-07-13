#pragma once

#include "gapi/GpuResource.hpp"

namespace D3D12MA
{
    class Allocation;
}

namespace RR
{
    namespace GAPI::DX12
    {
        class ResourceImpl final : public IGpuResource
        {
        public:
            ResourceImpl() = default;
            ~ResourceImpl();

            void Init(const std::shared_ptr<GpuResource>& resource);
            void Init(const GpuResourceDescription& resourceDesc, const U8String& name);
            void Init(const ComSharedPtr<ID3D12Resource>& resource, D3D12MA::Allocation* allocation, const U8String& name);

            std::any GetRawHandle() const override { return D3DResource_.get(); }
            D3D12_RESOURCE_STATES GetDefaultResourceState() const { return defaultState_; }

            std::vector<CpuResourceData::SubresourceFootprint> GetSubresourceFootprints(const GpuResourceDescription& desc) const override;

            void* Map() override;
            void Unmap() override;

            const ComSharedPtr<ID3D12Resource>& GetD3DObject() const { return D3DResource_; }

        public:
            static GpuResourceFootprint GetFootprint(const GpuResourceDescription& resourceDesc);

        private:
            ComSharedPtr<ID3D12Resource> D3DResource_;
            D3D12MA::Allocation* allocation_;
            D3D12_RESOURCE_STATES defaultState_;
        };
    }
}