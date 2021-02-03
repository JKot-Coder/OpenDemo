#include "CommandListImpl.hpp"

#include "d3dx12.h"

#include "gapi/GpuResource.hpp"
#include "gapi/GpuResourceViews.hpp"
#include "gapi/MemoryAllocation.hpp"

#include "gapi_dx12/DeviceContext.hpp"
#include "gapi_dx12/FenceImpl.hpp"
#include "gapi_dx12/GpuMemoryHeap.hpp"
#include "gapi_dx12/ResourceImpl.hpp"
#include "gapi_dx12/ResourceReleaseContext.hpp"
#include "gapi_dx12/ResourceViewsImpl.hpp"

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            namespace
            {
                void CheckIsCopyAllowed(const std::shared_ptr<Texture>& sourceTexture, const std::shared_ptr<Texture>& destTexture)
                {
                    ASSERT(sourceTexture);
                    ASSERT(destTexture);

                    // Allow copy (gpu->gpu || cpuWrite->gpu)
                    ASSERT(sourceTexture->GetCpuAccess() == GpuResourceCpuAccess::Write ||
                           sourceTexture->GetCpuAccess() == GpuResourceCpuAccess::None);
                    ASSERT(destTexture->GetCpuAccess() == GpuResourceCpuAccess::None);
                }
            }

            void CommandListImpl::CommandAllocatorsPool::createAllocator(
                const U8String& name,
                const uint32_t index,
                ComSharedPtr<ID3D12CommandAllocator>& allocator) const
            {
                ASSERT(!allocator);

                D3DCall(DeviceContext::GetDevice()->CreateCommandAllocator(type_, IID_PPV_ARGS(allocator.put())));

                D3DUtils::SetAPIName(allocator.get(), name, index);
            }

            void CommandListImpl::CommandAllocatorsPool::Init(
                D3D12_COMMAND_LIST_TYPE type,
                const U8String& name)
            {
                type_ = type;
                fence_ = std::make_unique<FenceImpl>();
                fence_->Init(name);

                for (uint32_t index = 0; index < allocators_.size(); index++)
                {
                    auto& allocatorData = allocators_[index];
                    createAllocator(name, index, allocatorData.allocator);
                    allocatorData.cpuFenceValue = 0;
                }
            }

            void CommandListImpl::CommandAllocatorsPool::ReleaseD3DObjects()
            {
                for (auto& allocatorData : allocators_)
                    DeviceContext::GetResourceReleaseContext()->DeferredD3DResourceRelease(allocatorData.allocator);
            }

            const ComSharedPtr<ID3D12CommandAllocator>& CommandListImpl::CommandAllocatorsPool::GetNextAllocator()
            {
                auto& data = allocators_[ringBufferIndex_];

                auto gpu = fence_->GetGpuValue();

                ASSERT(data.cpuFenceValue < fence_->GetGpuValue());
                data.cpuFenceValue = fence_->GetCpuValue();
                data.allocator->Reset();

                return data.allocator;
            }

            void CommandListImpl::CommandAllocatorsPool::ResetAfterSubmit(CommandQueueImpl& commandQueue)
            {
                ringBufferIndex_ = (++ringBufferIndex_ % AllocatorsCount);
                return fence_->Signal(commandQueue);
            }

            CommandListImpl::CommandListImpl(const CommandListType commandListType)
            {
                switch (commandListType)
                {
                case CommandListType::Graphics:
                    type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
                    break;
                case CommandListType::Compute:
                    type_ = D3D12_COMMAND_LIST_TYPE_COMPUTE;
                    break;
                case CommandListType::Copy:
                    type_ = D3D12_COMMAND_LIST_TYPE_COPY;
                    break;
                default:
                    ASSERT_MSG(false, "Unsuported command list type");
                }
            }

            void CommandListImpl::ReleaseD3DObjects()
            {
                DeviceContext::GetResourceReleaseContext()->DeferredD3DResourceRelease(D3DCommandList_);
                commandAllocatorsPool_.ReleaseD3DObjects();
            }

            void CommandListImpl::Init(const U8String& name)
            {
                ASSERT(!D3DCommandList_);

                commandAllocatorsPool_.Init(type_, name);
                const auto allocator = commandAllocatorsPool_.GetNextAllocator();

                D3DCall(DeviceContext::GetDevice()->CreateCommandList(0, type_, allocator.get(), nullptr, IID_PPV_ARGS(D3DCommandList_.put())));

                D3DUtils::SetAPIName(D3DCommandList_.get(), name);
            }

            void CommandListImpl::ResetAfterSubmit(CommandQueueImpl& commandQueue)
            {
                ASSERT(D3DCommandList_);

                commandAllocatorsPool_.ResetAfterSubmit(commandQueue);
                const auto& allocator = commandAllocatorsPool_.GetNextAllocator();
                D3DCall(D3DCommandList_->Reset(allocator.get(), nullptr));
            }

            void CommandListImpl::CopyBuffer(const std::shared_ptr<Buffer>& sourceBuffer, const std::shared_ptr<Buffer>& destBuffer)
            {
            }

            void CommandListImpl::CopyBufferRegion(const std::shared_ptr<Buffer>& sourceBuffer, uint32_t sourceOffset,
                                                   const std::shared_ptr<Buffer>& destBuffer, uint32_t destOffset, uint32_t numBytes)
            {
            }

            void CommandListImpl::CopyTexture(const std::shared_ptr<Texture>& sourceTexture, const std::shared_ptr<Texture>& destTexture)
            {
                CheckIsCopyAllowed(sourceTexture, destTexture);

                const auto sourceImpl = sourceTexture->GetPrivateImpl<ResourceImpl>();
                ASSERT(sourceImpl);

                const auto destImpl = destTexture->GetPrivateImpl<ResourceImpl>();
                ASSERT(destImpl);

                const auto& sourceDesc = sourceTexture->GetDescription();
                const auto& destDesc = destTexture->GetDescription();
                // Actually we can  copy textures with different format with restrictions. So reconsider this assert
                ASSERT(sourceDesc == destDesc);

                /* // TODO ????????????
                D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(sourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
                D3DCommandList_->ResourceBarrier(1, &barrier);

                barrier = CD3DX12_RESOURCE_BARRIER::Transition(destImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                D3DCommandList_->ResourceBarrier(1, &barrier);*/

                D3DCommandList_->CopyResource(destImpl->GetD3DObject().get(), sourceImpl->GetD3DObject().get());

                D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(destImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
                D3DCommandList_->ResourceBarrier(1, &barrier);
            }

            void CommandListImpl::CopyTextureSubresource(const std::shared_ptr<Texture>& sourceTexture, uint32_t sourceSubresourceIdx,
                                                         const std::shared_ptr<Texture>& destTexture, uint32_t destSubresourceIdx)
            {
                CheckIsCopyAllowed(sourceTexture, destTexture);

                const auto sourceImpl = sourceTexture->GetPrivateImpl<ResourceImpl>();
                ASSERT(sourceImpl);

                const auto destImpl = destTexture->GetPrivateImpl<ResourceImpl>();
                ASSERT(destImpl);

#ifdef ENABLE_ASSERTS
                const auto& sourceDesc = sourceTexture->GetDescription();
                const auto& destDesc = destTexture->GetDescription();
                ASSERT(sourceSubresourceIdx < sourceDesc.GetNumSubresources());
                ASSERT(destSubresourceIdx < destDesc.GetNumSubresources());
#endif

                CD3DX12_TEXTURE_COPY_LOCATION dst(sourceImpl->GetD3DObject().get(), sourceSubresourceIdx);
                CD3DX12_TEXTURE_COPY_LOCATION src(destImpl->GetD3DObject().get(), destSubresourceIdx);
                D3DCommandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            }

            void CommandListImpl::CopyTextureSubresourceRegion(const std::shared_ptr<Texture>& sourceTexture, uint32_t sourceSubresourceIdx, const Box3u& sourceBox,
                                                               const std::shared_ptr<Texture>& destTexture, uint32_t destSubresourceIdx, const Vector3u& destPoint)
            {
                CheckIsCopyAllowed(sourceTexture, destTexture);

                const auto sourceImpl = sourceTexture->GetPrivateImpl<ResourceImpl>();
                ASSERT(sourceImpl);

                const auto destImpl = destTexture->GetPrivateImpl<ResourceImpl>();
                ASSERT(destImpl);

#ifdef ENABLE_ASSERTS
                const auto& sourceDesc = sourceTexture->GetDescription();
                const auto& destDesc = destTexture->GetDescription();
                ASSERT(sourceSubresourceIdx < sourceDesc.GetNumSubresources());
                ASSERT(destSubresourceIdx < destDesc.GetNumSubresources());
#endif

                D3D12_BOX box = {
                    sourceBox.left,
                    sourceBox.top,
                    sourceBox.front,
                    sourceBox.left + sourceBox.width,
                    sourceBox.top + sourceBox.height,
                    sourceBox.front + sourceBox.depth,
                };

                CD3DX12_TEXTURE_COPY_LOCATION dst(sourceImpl->GetD3DObject().get(), sourceSubresourceIdx);
                CD3DX12_TEXTURE_COPY_LOCATION src(destImpl->GetD3DObject().get(), destSubresourceIdx);
                D3DCommandList_->CopyTextureRegion(&dst, destPoint.x, destPoint.y, destPoint.z, &src, &box);
            }

            void CommandListImpl::copyImmediate(const std::shared_ptr<Texture>& texture, const std::shared_ptr<IntermediateMemory>& textureData, bool readback)
            {
                ASSERT(texture);
                ASSERT(textureData);
                ASSERT(texture->GetDescription().GetNumSubresources() <= textureData->GetFirstSubresource() + textureData->GetNumSubresources());

                const auto allocation = textureData->GetAllocation();
                ASSERT(allocation);

                const auto& device = DeviceContext::GetDevice();

#ifdef ENABLE_ASSERTS
                auto desc = resourceImpl->GetD3DObject()->GetDesc();

                UINT64 itermediateSize;
                device->GetCopyableFootprints(&desc, textureData->GetFirstSubresource(), textureData->GetNumSubresources(), 0, nullptr, nullptr, nullptr, &itermediateSize);
                ASSERT(allocation->GetSize() == itermediateSize);
#endif

                ASSERT((allocation->GetMemoryType() == MemoryAllocationType::Upload && readback == false) ||
                       (allocation->GetMemoryType() == MemoryAllocationType::Readback && readback == true));

                const auto firstResource = textureData->GetFirstSubresource();
                for (uint32_t index = 0; index < textureData->GetNumSubresources(); index++)
                {
                    const auto subresourceIndex = index + firstResource;
                    const auto& footprint = textureData->GetSubresourceFootprints()[index];

                    const auto allocationImpl = allocation->GetPrivateImpl<GpuMemoryHeap::Allocation>();
                    size_t intermediateDataOffset = allocationImpl->offset;

                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
                    UINT numRows;
                    UINT64 itermediateSize;
                    device->GetCopyableFootprints(&desc, subresourceIndex, 1, intermediateDataOffset, &layout, &numRows, nullptr, nullptr);

                    ASSERT(footprint.depthPitch == layout.Footprint.RowPitch * numRows);
                    ASSERT(footprint.rowPitch == layout.Footprint.RowPitch);

                    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                    D3DCommandList_->ResourceBarrier(1, &barrier);

                    CD3DX12_TEXTURE_COPY_LOCATION dst(resourceImpl->GetD3DObject().get(), subresourceIndex);
                    CD3DX12_TEXTURE_COPY_LOCATION src(allocationImpl->resource.get(), layout);
                    D3DCommandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

                    barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
                    D3DCommandList_->ResourceBarrier(1, &barrier);
                    break;
                }
            }

            void CommandListImpl::UpdateTexture(const std::shared_ptr<Texture>& texture, const std::shared_ptr<IntermediateMemory>& textureData)
            {
                ASSERT(texture);
                ASSERT(textureData);
                ASSERT(texture->GetDescription().GetNumSubresources() <= textureData->GetFirstSubresource() + textureData->GetNumSubresources());

                const auto resourceImpl = texture->GetPrivateImpl<ResourceImpl>();
                ASSERT(resourceImpl);

                const auto allocation = textureData->GetAllocation();
                ASSERT(allocation);

                const auto& device = DeviceContext::GetDevice();

#ifdef ENABLE_ASSERTS
                auto desc = resourceImpl->GetD3DObject()->GetDesc();

                UINT64 itermediateSize;
                device->GetCopyableFootprints(&desc, textureData->GetFirstSubresource(), textureData->GetNumSubresources(), 0, nullptr, nullptr, nullptr, &itermediateSize);
                ASSERT(allocation->GetSize() == itermediateSize);
#endif

                const auto firstResource = textureData->GetFirstSubresource();
                for (uint32_t index = 0; index < textureData->GetNumSubresources(); index++)
                {
                    const auto subresourceIndex = index + firstResource;
                    const auto& footprint = textureData->GetSubresourceFootprints()[index];

                    switch (allocation->GetMemoryType())
                    {
                    case MemoryAllocationType::Upload:
                    {
                        const auto allocationImpl = allocation->GetPrivateImpl<GpuMemoryHeap::Allocation>();
                        size_t intermediateDataOffset = allocationImpl->offset;

                        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
                        UINT numRows;
                        UINT64 itermediateSize;
                        device->GetCopyableFootprints(&desc, subresourceIndex, 1, intermediateDataOffset, &layout, &numRows, nullptr, nullptr);

                        ASSERT(footprint.depthPitch == layout.Footprint.RowPitch * numRows);
                        ASSERT(footprint.rowPitch == layout.Footprint.RowPitch);

                        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                        D3DCommandList_->ResourceBarrier(1, &barrier);

                        CD3DX12_TEXTURE_COPY_LOCATION dst(resourceImpl->GetD3DObject().get(), subresourceIndex);
                        CD3DX12_TEXTURE_COPY_LOCATION src(allocationImpl->resource.get(), layout);
                        D3DCommandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

                        barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
                        D3DCommandList_->ResourceBarrier(1, &barrier);
                        break;
                    }
                    default:
                        LOG_FATAL("Unsupported memory type");
                    }
                }
            }

            void CommandListImpl::ReadbackTexture(const std::shared_ptr<Texture>& texture, const std::shared_ptr<IntermediateMemory>& textureData)
            {
                ASSERT(texture);
                ASSERT(textureData);
                ASSERT(texture->GetDescription().GetNumSubresources() <= textureData->GetFirstSubresource() + textureData->GetNumSubresources());

                const auto resourceImpl = texture->GetPrivateImpl<ResourceImpl>();
                ASSERT(resourceImpl);

                const auto allocation = textureData->GetAllocation();
                ASSERT(allocation);

                const auto& device = DeviceContext::GetDevice();
                auto desc = resourceImpl->GetD3DObject()->GetDesc();

                UINT64 itermediateSize;
                device->GetCopyableFootprints(&desc, textureData->GetFirstSubresource(), textureData->GetNumSubresources(), 0, nullptr, nullptr, nullptr, &itermediateSize);
                ASSERT(allocation->GetSize() == itermediateSize);

                // TODO Default state // Barries for subresources?
                D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
                D3DCommandList_->ResourceBarrier(1, &barrier);

                const auto firstResource = textureData->GetFirstSubresource();
                for (uint32_t index = 0; index < textureData->GetNumSubresources(); index++)
                {
                    const auto subresourceIndex = index + firstResource;
                    const auto& footprint = textureData->GetSubresourceFootprints()[index];

                    switch (allocation->GetMemoryType())
                    {
                    case MemoryAllocationType::Readback:
                    {
                        const auto allocationImpl = allocation->GetPrivateImpl<GpuMemoryHeap::Allocation>();
                        size_t intermediateDataOffset = allocationImpl->offset;

                        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
                        UINT numRows;
                        UINT64 itermediateSize;
                        device->GetCopyableFootprints(&desc, subresourceIndex, 1, intermediateDataOffset, &layout, &numRows, nullptr, nullptr);

                        ASSERT(footprint.depthPitch == layout.Footprint.RowPitch * numRows);
                        ASSERT(footprint.rowPitch == layout.Footprint.RowPitch);

                        CD3DX12_TEXTURE_COPY_LOCATION dst(allocationImpl->resource.get(), layout);
                        CD3DX12_TEXTURE_COPY_LOCATION src(resourceImpl->GetD3DObject().get(), subresourceIndex);

                        D3DCommandList_.get()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

                        //barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
                        // D3DCommandList_->ResourceBarrier(1, &barrier);
                        break;
                    }
                    default:
                        LOG_FATAL("Unsupported memory type");
                    }
                }
            }

            void CommandListImpl::ClearRenderTargetView(const RenderTargetView::SharedPtr& renderTargetView, const Vector4& color)
            {
                ASSERT(renderTargetView);
                ASSERT(D3DCommandList_);

                const auto allocation = renderTargetView->GetPrivateImpl<DescriptorHeap::Allocation>();
                ASSERT(allocation);

                const auto& resource = renderTargetView->GetGpuResource().lock();
                ASSERT(resource);
                ASSERT(resource->GetGpuResourceType() == GpuResource::Type::Texture);

                const auto resourceImpl = resource->GetPrivateImpl<ResourceImpl>();
                ASSERT(resourceImpl);

                D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
                D3DCommandList_->ResourceBarrier(1, &barrier);

                D3DCommandList_->ClearRenderTargetView(allocation->GetCPUHandle(), &color.x, 0, nullptr);

                barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceImpl->GetD3DObject().get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
                D3DCommandList_->ResourceBarrier(1, &barrier);
            }

            void CommandListImpl::Close()
            {
                D3DCall(D3DCommandList_->Close());
            }
        };
    }
}