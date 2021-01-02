#include "Device.hpp"

// TODO temporary
//#include "gapi/FencedRingBuffer.hpp"

#include "gapi/CommandList.hpp"
#include "gapi/CommandQueue.hpp"
#include "gapi/Fence.hpp"
#include "gapi/Frame.hpp"
#include "gapi/Object.hpp"
#include "gapi/SwapChain.hpp"
#include "gapi/Texture.hpp"

#include "gapi_dx12/CommandContextImpl.hpp"
#include "gapi_dx12/CommandListCompiler.hpp"
#include "gapi_dx12/CommandListImpl.hpp"
#include "gapi_dx12/CommandQueueImpl.hpp"
#include "gapi_dx12/D3DUtils.hpp"
#include "gapi_dx12/DescriptorHeap.hpp"
#include "gapi_dx12/DescriptorHeapSet.hpp"
#include "gapi_dx12/FenceImpl.hpp"
#include "gapi_dx12/ResourceCreator.hpp"
#include "gapi_dx12/ResourceImpl.hpp"
#include "gapi_dx12/SwapChainImpl.hpp"

#include <chrono>
#include <iterator>
#include <thread>

#ifdef ENABLE_ASSERTS
#define ASSERT_IS_CREATION_THREAD ASSERT(creationThreadID_ == std::this_thread::get_id())
#define ASSERT_IS_DEVICE_INITED ASSERT(inited_)
#else
#define ASSERT_IS_CREATION_THREAD
#define ASSERT_IS_DEVICE_INITED
#endif

using namespace std::chrono_literals;

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            template <typename T>
            void ThrowIfFailed(T c) { std::ignore = c; };

            class DeviceImplementation
            {
            public:
                DeviceImplementation();

                Result Init(const Device::Description& description);
                Result Submit(const CommandList::SharedPtr& commandList);
                Result Present(const SwapChain::SharedPtr& swapChain);

                ID3D12Device* GetDevice() const
                {
                    return d3dDevice_.get();
                }

                Result WaitForGpu();

                Result InitResource(const Object::SharedPtr& resource) const;
                Result DeviceImplementation::InitResource(const Fence::SharedPtr& resource, uint64_t initialValue) const;

            private:
                Result createDevice();

                void moveToNextFrame();

            private:
                Device::Description description_ = {};

                bool inited_ = false;

                std::thread::id creationThreadID_;

                std::unique_ptr<ResourceCreatorContext> resourceCreatorContext_;

                ComSharedPtr<ID3D12Debug1> debugController_;
                ComSharedPtr<IDXGIFactory2> dxgiFactory_;
                ComSharedPtr<IDXGIAdapter1> dxgiAdapter_;
                ComSharedPtr<ID3D12Device> d3dDevice_;

                D3D_FEATURE_LEVEL d3dFeatureLevel_ = D3D_FEATURE_LEVEL_1_0_CORE;
                std::shared_ptr<CommandQueueImpl> graphicsCommandQueue_;
                std::unique_ptr<FenceImpl> gpuWaitFence_;
                std::shared_ptr<DescriptorHeapSet> descriptorHeapSet_;
            };

            DeviceImplementation::DeviceImplementation()
                : creationThreadID_(std::this_thread::get_id())
            {
            }

            Result DeviceImplementation::Init(const Device::Description& description)
            {
                ASSERT_IS_CREATION_THREAD;
                ASSERT(inited_ == false);

                ASSERT(description.gpuFramesBuffered <= MAX_BACK_BUFFER_COUNT);

                description_ = description;

                D3DCallMsg(createDevice(), "CreateDevice");

                descriptorHeapSet_ = std::make_shared<DescriptorHeapSet>();
                D3DCall(descriptorHeapSet_->Init(d3dDevice_));

                gpuWaitFence_ = std::make_unique<FenceImpl>();
                D3DCall(gpuWaitFence_->Init(d3dDevice_, "GpuWait"));

                graphicsCommandQueue_ = std::make_shared<CommandQueueImpl>(CommandQueueType::Graphics);
                D3DCall(graphicsCommandQueue_->Init(d3dDevice_, "Primary"));

                resourceCreatorContext_ = std::make_unique<ResourceCreatorContext>(
                    d3dDevice_,
                    dxgiFactory_,
                    graphicsCommandQueue_,
                    descriptorHeapSet_);

                inited_ = true;

                return Result::Ok;
            }

            Result DeviceImplementation::WaitForGpu()
            {
                ASSERT_IS_DEVICE_INITED;

                D3DCall(gpuWaitFence_->Signal(*graphicsCommandQueue_.get()));
                D3DCall(gpuWaitFence_->SyncCPU(std::nullopt));

                return Result::Ok;
            }

            Result DeviceImplementation::InitResource(const Object::SharedPtr& resource) const
            {
                ASSERT(resourceCreatorContext_);
                return ResourceCreator::InitResource(*resourceCreatorContext_.get(), resource);
            }

            Result DeviceImplementation::Submit(const CommandList::SharedPtr& commandList)
            {
                /* ASSERT_IS_CREATION_THREAD;
                ASSERT_IS_DEVICE_INITED;
                ASSERT(commandList)

                Log::Print::Info("submit\n");
                const auto& commandQueue = getCommandQueue(CommandQueueType::Graphics);

                ASSERT(dynamic_cast<CommandListImpl*>(commandList->GetInterface()));
                const auto commandListImpl = reinterpret_cast<const CommandListImpl*>(commandList->GetInterface());

                const auto D3DCommandList = commandListImpl->GetD3DObject();
                ASSERT(D3DCommandList)

                ID3D12CommandList* ppCommandLists[] = { D3DCommandList.get() };
                commandQueue->ExecuteCommandLists(std::size(ppCommandLists), ppCommandLists);
                */
                return Result::Ok;
            }

            Result DeviceImplementation::Present(const SwapChain::SharedPtr& swapChain)
            {
                ASSERT_IS_CREATION_THREAD;
                ASSERT_IS_DEVICE_INITED;
                ASSERT(swapChain);

                HRESULT hr;
                /*  if (m_options & c_AllowTearing)
                {
                    // Recommended to always use tearing if supported when using a sync interval of 0.
                    // Note this will fail if in true 'fullscreen' mode.
                    hr = m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
                }
                else
                {*/
                // The first argument instructs DXGI to block until VSync, putting the application
                // to sleep until the next VSync. This ensures we don't waste any cycles rendering
                // frames that will never be displayed to the screen.
                //  hr = swapChain->Present(0, 0);
                //  }

                // The first argument instructs DXGI to block until VSync, putting the application
                // to sleep until the next VSync. This ensures we don't waste any cycles rendering
                // frames that will never be displayed to the screen.
                DXGI_PRESENT_PARAMETERS parameters = {};

                ASSERT(dynamic_cast<SwapChainImpl*>(swapChain->GetInterface()));
                auto swapChainImpl = static_cast<SwapChainImpl*>(swapChain->GetInterface());
                Result result = swapChainImpl->Present(0);

                // If the device was reset we must completely reinitialize the renderer.
                if (result == Result::DeviceRemoved || result == Result::DeviceReset)
                {
                    result = (result == Result::DeviceRemoved) ? Result(d3dDevice_->GetDeviceRemovedReason()) : result;
                    Log::Print::Warning("Device Lost on Present. Error: %s\n", result.ToString());

                    // Todo error check
                    //handleDeviceLost();
                    ASSERT(false);

                    return Result::Fail;
                }
                else
                {
                    if (!dxgiFactory_->IsCurrent())
                    {
                        LOG_ERROR("Dxgi is not current");
                        return Result::Fail;

                        // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
                        //ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));
                    }
                }

                return Result::Ok;
            }

            Result DeviceImplementation::createDevice()
            {
                ASSERT_IS_CREATION_THREAD

                UINT dxgiFactoryFlags = 0;

                // Enable the debug layer (requires the Graphics Tools "optional feature").
                // NOTE: Enabling the debug layer after device creation will invalidate the active device.
                if (description_.debugMode == Device::DebugMode::Debug
                    || description_.debugMode == Device::DebugMode::Instrumented)
                {
                    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController_.put()))))
                    {
                        debugController_->EnableDebugLayer();
                        debugController_->SetEnableGPUBasedValidation(true);

                        if (description_.debugMode == Device::DebugMode::Debug)
                        {
                            debugController_->SetEnableSynchronizedCommandQueueValidation(true);
                        }
                    }
                    else
                    {
                        LOG_WARNING("WARNING: Direct3D Debug Device is not available\n");
                    }

                    ComSharedPtr<IDXGIInfoQueue> dxgiInfoQueue;
                    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.put()))))
                    {
                        dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

                        dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
                        dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
                    }
                }

                D3DCallMsg(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(dxgiFactory_.put())), "CreateDXGIFactory2");

                D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;
                D3DCallMsg(D3DUtils::GetAdapter(dxgiFactory_, minimumFeatureLevel, dxgiAdapter_), "GetAdapter");

                // Create the DX12 API device object.
                D3DCallMsg(D3D12CreateDevice(dxgiAdapter_.get(), minimumFeatureLevel, IID_PPV_ARGS(d3dDevice_.put())), "D3D12CreateDevice");
                D3DUtils::SetAPIName(d3dDevice_.get(), "Main");

                if (description_.debugMode == Device::DebugMode::Debug
                    || description_.debugMode == Device::DebugMode::Instrumented)
                {
                    // Configure debug device (if active).
                    ComSharedPtr<ID3D12InfoQueue> d3dInfoQueue;

                    Result result;
                    if (result = Result(d3dDevice_->QueryInterface(IID_PPV_ARGS(d3dInfoQueue.put()))))
                    {
                        d3dInfoQueue->ClearRetrievalFilter();
                        d3dInfoQueue->ClearStorageFilter();

                        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
                    }
                    else
                    {
                        LOG_ERROR("Unable to get ID3D12InfoQueue with HRESULT of 0x%08X", result);
                    }
                }

                // Determine maximum supported feature level for this device
                static const D3D_FEATURE_LEVEL s_featureLevels[] = {
                    D3D_FEATURE_LEVEL_12_1,
                    D3D_FEATURE_LEVEL_12_0,
                    D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_11_0,
                };

                D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels = {
                    _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
                };

                if (SUCCEEDED(d3dDevice_->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels))))
                {
                    d3dFeatureLevel_ = featLevels.MaxSupportedFeatureLevel;
                }
                else
                {
                    d3dFeatureLevel_ = minimumFeatureLevel;
                }

                return Result::Ok;
            }

            void DeviceImplementation::moveToNextFrame()
            {
                ASSERT_IS_CREATION_THREAD;
                ASSERT_IS_DEVICE_INITED;
            }

            Device::Device()
                : _impl(new DeviceImplementation())
            {
            }

            Device::~Device() { }

            Result Device::Init(const Description& description)
            {
                return _impl->Init(description);
            }

            Result Device::Present(const std::shared_ptr<SwapChain>& swapChain)
            {
                return _impl->Present(swapChain);
            }
            /*
            Result Device::Submit(const CommandList::SharedPtr& CommandList)
            {
                return _impl->Submit(CommandList);
            }*/

            Result Device::WaitForGpu()
            {
                return _impl->WaitForGpu();
            }

            Result Device::InitResource(const Object::SharedPtr& resource) const
            {
                return _impl->InitResource(resource);
            }

        }
    }
}