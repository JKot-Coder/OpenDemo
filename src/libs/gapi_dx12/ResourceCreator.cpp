#include "ResourceCreator.hpp"

#include "gapi_dx12/CommandListImpl.hpp"
#include "gapi_dx12/CommandQueueImpl.hpp"
#include "gapi_dx12/DescriptorHeapSet.hpp"
#include "gapi_dx12/DeviceContext.hpp"
#include "gapi_dx12/FenceImpl.hpp"
#include "gapi_dx12/ResourceImpl.hpp"
#include "gapi_dx12/ResourceViewsImpl.hpp"
#include "gapi_dx12/SwapChainImpl.hpp"

#include "gapi/CommandList.hpp"
#include "gapi/CommandQueue.hpp"
#include "gapi/Fence.hpp"
#include "gapi/GpuResource.hpp"
#include "gapi/GpuResourceViews.hpp"
#include "gapi/Object.hpp"
#include "gapi/SwapChain.hpp"
#include "gapi/Texture.hpp"

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            namespace
            {
                template <typename DescType>
                DescType getViewDimension(GpuResourceDimension dimension, bool isTextureArray);

                template <>
                D3D12_RTV_DIMENSION getViewDimension(GpuResourceDimension dimension, bool isTextureArray)
                {
                    switch (dimension)
                    {
                    case GpuResourceDimension::Texture1D:
                        return (isTextureArray) ? D3D12_RTV_DIMENSION_TEXTURE1DARRAY : D3D12_RTV_DIMENSION_TEXTURE1D;
                    case GpuResourceDimension::Texture2D:
                        return (isTextureArray) ? D3D12_RTV_DIMENSION_TEXTURE2DARRAY : D3D12_RTV_DIMENSION_TEXTURE2D;
                    case GpuResourceDimension::Texture3D:
                        ASSERT(isTextureArray == false);
                        return D3D12_RTV_DIMENSION_TEXTURE3D;
                    case GpuResourceDimension::Texture2DMS:
                        return (isTextureArray) ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DMS;
                    case GpuResourceDimension::TextureCube:
                        return D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    default:
                        ASSERT_MSG(false, "Wrong texture dimension");
                        return D3D12_RTV_DIMENSION_UNKNOWN;
                    }
                }

                template <>
                D3D12_DSV_DIMENSION getViewDimension(GpuResourceDimension dimension, bool isTextureArray)
                {
                    switch (dimension)
                    {
                    case GpuResourceDimension::Texture1D:
                        return (isTextureArray) ? D3D12_DSV_DIMENSION_TEXTURE1DARRAY : D3D12_DSV_DIMENSION_TEXTURE1D;
                    case GpuResourceDimension::Texture2D:
                        return (isTextureArray) ? D3D12_DSV_DIMENSION_TEXTURE2DARRAY : D3D12_DSV_DIMENSION_TEXTURE2D;
                    case GpuResourceDimension::Texture2DMS:
                        return (isTextureArray) ? D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D12_DSV_DIMENSION_TEXTURE2DMS;
                    case GpuResourceDimension::TextureCube:
                        return D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                    default:
                        ASSERT_MSG(false, "Wrong texture dimension")
                        return D3D12_DSV_DIMENSION_UNKNOWN;
                    }
                }

                template <typename DescType>
                DescType CreateDsvRtvUavDescCommon(const GpuResourceDescription& GpuResourceDescription, const GpuResourceViewDescription& description)
                {
                    DescType result = {};
                    result.ViewDimension = getViewDimension<decltype(result.ViewDimension)>(GpuResourceDescription.GetDimension(), GpuResourceDescription.GetArraySize() > 1);

                    const uint32_t arrayMultiplier = (GpuResourceDescription.GetDimension() == GpuResourceDimension::TextureCube) ? 6 : 1;
                    ASSERT((description.texture.firstArraySlice + description.texture.arraySliceCount) * arrayMultiplier <= GpuResourceDescription.GetArraySize())

                    switch (GpuResourceDescription.GetDimension())
                    {
                    case GpuResourceDimension::Texture1D:
                        if (description.texture.arraySliceCount > 1)
                        {
                            result.Texture1DArray.ArraySize = description.texture.arraySliceCount;
                            result.Texture1DArray.FirstArraySlice = description.texture.firstArraySlice;
                            result.Texture1DArray.MipSlice = description.texture.mipLevel;
                        }
                        else
                        {
                            result.Texture1D.MipSlice = description.texture.firstArraySlice;
                        }
                        break;
                    case GpuResourceDimension::Texture2D:
                    case GpuResourceDimension::TextureCube:
                        if (description.texture.firstArraySlice * arrayMultiplier > 1)
                        {
                            result.Texture2DArray.ArraySize = description.texture.arraySliceCount * arrayMultiplier;
                            result.Texture2DArray.FirstArraySlice = description.texture.firstArraySlice * arrayMultiplier;
                            result.Texture2DArray.MipSlice = description.texture.mipLevel;
                        }
                        else
                        {
                            result.Texture2D.MipSlice = description.texture.mipLevel;
                        }
                        break;
                    case GpuResourceDimension::Texture2DMS:
                        //ASSERT(std::is_same<DescType, D3D12_DEPTH_STENCIL_VIEW_DESC>::value || std::is_same<DescType, D3D12_RENDER_TARGET_VIEW_DESC>::value)
                        break;
                    default:
                        LOG_FATAL("Unsupported resource view type");
                    }
                    result.Format = D3DUtils::GetDxgiResourceFormat(GpuResourceDescription.GetFormat());

                    return result;
                }

                template <typename DescType>
                DescType CreateDsvRtvDesc(const GpuResourceDescription& GpuResourceDescription, const GpuResourceViewDescription& description)
                {
                    static_assert(std::is_same<DescType, D3D12_DEPTH_STENCIL_VIEW_DESC>::value || std::is_same<DescType, D3D12_RENDER_TARGET_VIEW_DESC>::value);

                    DescType result = CreateDsvRtvUavDescCommon<DescType>(GpuResourceDescription, description);

                    if (GpuResourceDescription.GetDimension() == GpuResourceDimension::Texture2DMS)
                    {
                        if (GpuResourceDescription.GetArraySize() > 1)
                        {
                            result.Texture2DMSArray.ArraySize = description.texture.firstArraySlice;
                            result.Texture2DMSArray.FirstArraySlice = description.texture.arraySliceCount;
                        }
                    }

                    return result;
                }

                D3D12_DEPTH_STENCIL_VIEW_DESC CreateDsvDesc(const GpuResource::SharedPtr& resource, const GpuResourceViewDescription& description)
                {
                    ASSERT(resource->IsTexture())
                    const auto& texture = resource->GetTyped<Texture>();
                    const auto& GpuResourceDescription = texture->GetDescription();

                    return CreateDsvRtvDesc<D3D12_DEPTH_STENCIL_VIEW_DESC>(GpuResourceDescription, description);
                }

                D3D12_RENDER_TARGET_VIEW_DESC CreateRtvDesc(const GpuResource::SharedPtr& resource, const GpuResourceViewDescription& description)
                {
                    ASSERT(resource->IsTexture())
                    const auto& texture = resource->GetTyped<Texture>();
                    const auto& GpuResourceDescription = texture->GetDescription();

                    return CreateDsvRtvDesc<D3D12_RENDER_TARGET_VIEW_DESC>(GpuResourceDescription, description);
                }
            }

            void ResourceCreator::InitSwapChain(SwapChain& resource)
            {
                auto impl = std::make_unique<SwapChainImpl>();
                impl->Init(DeviceContext::GetDevice(), DeviceContext::GetDxgiFactory(), DeviceContext::GetGraphicsCommandQueue()->GetD3DObject(), resource.GetDescription(), resource.GetName());

                resource.SetPrivateImpl(impl.release());
            }

            void ResourceCreator::InitFence(Fence& resource)
            {
                auto impl = std::make_unique<FenceImpl>();
                impl->Init(resource.GetName());

                resource.SetPrivateImpl(impl.release());
            }

            void ResourceCreator::InitCommandQueue(CommandQueue& resource)
            {
                std::unique_ptr<CommandQueueImpl> impl;

                if (resource.GetCommandQueueType() == CommandQueueType::Graphics)
                {
                    static bool alreadyInited = false;
                    ASSERT(!alreadyInited); // Only one graphics command queue are alloved.
                    alreadyInited = true;

                    // Graphics command queue already initialized internally in device,
                    // so make copy to prevent d3d object leaking.
                    impl.reset(new CommandQueueImpl(*DeviceContext::GetGraphicsCommandQueue()));
                }
                else
                {
                    impl.reset(new CommandQueueImpl(resource.GetCommandQueueType()));
                    impl->Init(resource.GetName());
                }

                resource.SetPrivateImpl(impl.release());
            }

            void ResourceCreator::InitCommandList(CommandList& resource)
            {
                auto impl = std::make_unique<CommandListImpl>(resource.GetCommandListType());
                impl->Init(resource.GetName());

                resource.SetPrivateImpl(static_cast<ICommandList*>(impl.release()));
            }

            void ResourceCreator::InitGpuResourceView(GpuResourceView& object)
            {
                const auto& resourceSharedPtr = object.GetGpuResource().lock();
                ASSERT(resourceSharedPtr);

                const auto resourcePrivateImpl = resourceSharedPtr->GetPrivateImpl<ResourceImpl>();
                ASSERT(resourcePrivateImpl);

                const auto& d3dObject = resourcePrivateImpl->GetD3DObject();
                ASSERT(d3dObject);

                auto allocation = std::make_unique<DescriptorHeap::Allocation>();

                switch (object.GetViewType())
                {
                case GpuResourceView::ViewType::RenderTargetView:
                {
                    const auto& descriptorHeap = DeviceContext::GetDesciptorHeapSet()->GetRtvDescriptorHeap();
                    ASSERT(descriptorHeap);

                    descriptorHeap->Allocate(*allocation);

                    D3D12_RENDER_TARGET_VIEW_DESC desc = CreateRtvDesc(resourceSharedPtr, object.GetDescription());
                    DeviceContext::GetDevice()->CreateRenderTargetView(d3dObject.get(), &desc, allocation->GetCPUHandle());
                }
                break;
                    /*     case ResourceView::ViewType::RShaderResourceView:
                    break;
                case ResourceView::ViewType::RDepthStencilView:
                    break;*/
                case GpuResourceView::ViewType::UnorderedAccessView:
                    //const auto& descriptorHeap = DeviceContext::GetDesciptorHeapSet()->GetRtvDescriptorHeap();
                    LOG_FATAL("Unsupported resource view type");

                    break;
                default:
                    LOG_FATAL("Unsupported resource view type");
                }

                object.SetPrivateImpl(allocation.release());
            }
        }
    }
}