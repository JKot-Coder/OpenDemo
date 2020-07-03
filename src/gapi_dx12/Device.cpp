#include "Device.hpp"

// TODO temporary
//#include "gapi/FencedRingBuffer.hpp"

#include "gapi/CommandList.hpp"

#include "gapi_dx12/CommandListCompiler.hpp"
#include "gapi_dx12/CommandListImpl.hpp"
#include "gapi_dx12/FenceImpl.hpp"

#include "gapi_dx12/d3dx12.h"

#include <chrono>
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
    namespace Render
    {
        namespace Device
        {
            namespace DX12
            {
                template <typename T>
                void ThrowIfFailed(T c) { std::ignore = c; };

                class DeviceImplementation
                {
                public:
                    DeviceImplementation();

                    GAPIStatus Init();
                    GAPIStatus Reset(const PresentOptions& presentOptions);
                    GAPIStatus Present();

                    GAPIStatus CompileCommandList(CommandList& commandList) const;
                    GAPIStatus SubmitCommandList(CommandList& commandList) const;

                    void WaitForGpu();

                private:
                    bool enableDebug_ = true;

                    std::thread::id creationThreadID_;
                    bool inited_ = false;

                    ComSharedPtr<ID3D12Debug1> debugController_;
                    ComSharedPtr<IDXGIFactory2> dxgiFactory_;
                    ComSharedPtr<IDXGIAdapter1> dxgiAdapter_;
                    ComSharedPtr<ID3D12Device> d3dDevice_;
                    ComSharedPtr<IDXGISwapChain3> swapChain_;

                    std::array<ComSharedPtr<ID3D12CommandQueue>, static_cast<size_t>(CommandQueueType::COUNT)> commandQueues_;
                    std::array<ComSharedPtr<ID3D12Resource>, MAX_BACK_BUFFER_COUNT> renderTargets_;

                    D3D_FEATURE_LEVEL d3dFeatureLevel_ = D3D_FEATURE_LEVEL_1_0_CORE;

                    uint32_t frameIndex_ = UNDEFINED_FRAME_INDEX;

                    uint32_t backBufferIndex_ = 0;
                    uint32_t backBufferCount_ = 0;

                    // TEMPORARY
                    ComSharedPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
                    std::unique_ptr<CommandListImpl> commandList_;
                    std::shared_ptr<FenceImpl> fence_;
                    std::array<uint64_t, GPU_FRAMES_BUFFERED> fenceValues_;
                    winrt::handle fenceEvent_;
                    uint32_t rtvDescriptorSize_;
                    // TEMPORARY END

                    CD3DX12_CPU_DESCRIPTOR_HANDLE getRenderTargetView(uint32_t backBufferIndex)
                    {
                        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
                            rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
                            static_cast<INT>(backBufferIndex), rtvDescriptorSize_);
                    }

                    ComSharedPtr<ID3D12CommandQueue> getCommandQueue(CommandQueueType commandQueueType)
                    {
                        return commandQueues_[static_cast<std::underlying_type<CommandQueueType>::type>(commandQueueType)];
                    }

                    GAPIStatus createDevice();

                    GAPIStatus handleDeviceLost();

                    void moveToNextFrame();
                };

                DeviceImplementation::DeviceImplementation()
                    : creationThreadID_(std::this_thread::get_id())
                {
                }

                GAPIStatus DeviceImplementation::Init()
                {
                    ASSERT_IS_CREATION_THREAD;
                    ASSERT(inited_ == false);

                    auto& commandQueue = getCommandQueue(CommandQueueType::GRAPHICS);

                    // TODO Take from parameters. Check by assert;
                    backBufferCount_ = 2;

                    GAPIStatus result = GAPIStatus::OK;

                    if (GAPIStatusU::Failure(result = createDevice()))
                    {
                        LOG_ERROR("Failed CreateDevice");
                        return result;
                    }

                    // Create the command queue.
                    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
                    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
                    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

                    {
                        ComSharedPtr<ID3D12CommandQueue> commandQueue;
                        if (GAPIStatusU::Failure(result = GAPIStatus(d3dDevice_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.put())))))
                        {
                            LOG_ERROR("Failure create CommandQueue with HRESULT of 0x%08X", result);
                            return result;
                        }
                        commandQueue->SetName(L"MainCommandQueue");

                        commandQueue.as(commandQueues_[static_cast<size_t>(CommandQueueType::GRAPHICS)]);
                    }

                    // Create descriptor heaps for render target views and depth stencil views.
                    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
                    rtvDescriptorHeapDesc.NumDescriptors = MAX_BACK_BUFFER_COUNT;
                    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

                    if (GAPIStatusU::Failure(result = GAPIStatus(d3dDevice_->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(rtvDescriptorHeap_.put())))))
                    {
                        LOG_ERROR("Failure create DescriptorHeap with HRESULT of 0x%08X", result);
                        return result;
                    }

                    rtvDescriptorHeap_->SetName(L"DescriptorHead");

                    rtvDescriptorSize_ = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                    /*
                    if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
                    {
                        D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
                        dsvDescriptorHeapDesc.NumDescriptors = 1;
                        dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

                        ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(m_dsvDescriptorHeap.ReleaseAndGetAddressOf())));

                        m_dsvDescriptorHeap->SetName(L"DeviceResources");
                    }*/

                    // Create a fence for tracking GPU execution progress.
                    fence_.reset(new FenceImpl());
                    if (GAPIStatusU::Failure(result = fence_->Init(d3dDevice_.get(), 1, "FrameSync")))
                    {
                        return result;
                    }

                    commandList_.reset(new CommandListImpl(D3D12_COMMAND_LIST_TYPE_DIRECT));
                    if (GAPIStatusU::Failure(commandList_->Init(d3dDevice_.get(), "Main")))
                    {
                        return result;
                    }

                    for (int i = 0; i < GPU_FRAMES_BUFFERED; i++)
                    {
                        fenceValues_[i] = 0;
                    }
                    fenceValues_[frameIndex_] = 1;
                    //_fence->SetName(L"DeviceResources");

                    fenceEvent_.attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
                    if (!bool { fenceEvent_ })
                    {
                        LOG_ERROR("Failure create fence Event");
                        return GAPIStatus::FAIL;
                    }

                    inited_ = GAPIStatusU::Success(result);
                    return result;
                }

                void DeviceImplementation::WaitForGpu()
                {
                    ASSERT_IS_DEVICE_INITED;
                }

                // These resources need to be recreated every time the window size is changed.
                GAPIStatus DeviceImplementation::Reset(const PresentOptions& presentOptions)
                {
                    ASSERT_IS_CREATION_THREAD;
                    ASSERT_IS_DEVICE_INITED;
                    ASSERT(presentOptions.windowHandle);
                    ASSERT(presentOptions.isStereo == false);

                    if (!backBufferCount_)
                        backBufferCount_ = presentOptions.bufferCount;

                    ASSERT_MSG(presentOptions.bufferCount == backBufferCount_, "Changing backbuffer count should work, but this is untested")

                    const HWND windowHandle = presentOptions.windowHandle;

                    // Wait until all previous GPU work is complete.
                    WaitForGpu();

                    // Release resources that are tied to the swap chain and update fence values.
                    for (int n = 0; n < backBufferCount_; n++)
                    {
                        renderTargets_[n] = nullptr;
                        // m_fenceValues[n] = m_fenceValues[m_frameIndex];
                    }

                    GAPIStatus result = GAPIStatus::OK;

                    // If the swap chain already exists, resize it, otherwise create one.
                    if (swapChain_)
                    {
                        DXGI_SWAP_CHAIN_DESC1 currentSwapChainDesc;
                        if (GAPIStatusU::Failure(result = GAPIStatus(swapChain_->GetDesc1(&currentSwapChainDesc))))
                        {
                            LOG_ERROR("Failure get swapChain Desc");
                            return result;
                        }

                        const auto& targetSwapChainDesc = D3DUtils::GetDXGISwapChainDesc1(presentOptions, DXGI_SWAP_EFFECT_FLIP_DISCARD);
                        const auto swapChainCompatable = D3DUtils::SwapChainDesc1MatchesForReset(currentSwapChainDesc, targetSwapChainDesc);

                        if (!swapChainCompatable)
                        {
                            LOG_ERROR("SwapChains incompatible");
                            return GAPIStatus::FAIL;
                        }

                        // If the swap chain already exists, resize it.
                        HRESULT hr = swapChain_->ResizeBuffers(
                            targetSwapChainDesc.BufferCount,
                            targetSwapChainDesc.Width,
                            targetSwapChainDesc.Height,
                            targetSwapChainDesc.Format,
                            targetSwapChainDesc.Flags);

                        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
                        {
                            LOG_ERROR("Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? d3dDevice_->GetDeviceRemovedReason() : hr))

                            // If the device was removed for any reason, a new device and swap chain will need to be created.
                            // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
                            // and correctly set up the new device.
                            return handleDeviceLost();
                        }
                        else if (GAPIStatusU::Failure(result = GAPIStatus(hr)))
                        {
                            LOG_ERROR("Failed ResizeBuffers")
                            return GAPIStatus(result);
                        }
                    }
                    else
                    {
                        const auto& graphicsCommandQueue = getCommandQueue(CommandQueueType::GRAPHICS);
                        const auto& targetSwapChainDesc = D3DUtils::GetDXGISwapChainDesc1(presentOptions, DXGI_SWAP_EFFECT_FLIP_DISCARD);

                        ComSharedPtr<IDXGISwapChain1> swapChain2;
                        // Create a swap chain for the window.
                        if (GAPIStatusU::Failure(result = GAPIStatus(dxgiFactory_->CreateSwapChainForHwnd(
                                                     graphicsCommandQueue.get(),
                                                     presentOptions.windowHandle,
                                                     &targetSwapChainDesc,
                                                     nullptr,
                                                     nullptr,
                                                     swapChain2.put()))))
                        {
                            LOG_ERROR("Failure CreateSwapChainForHwnd");
                            return result;
                        }

                        swapChain2.as(swapChain_);
                    }

                    // Update backbuffers.
                    DXGI_SWAP_CHAIN_DESC1 currentSwapChainDesc;
                    if (GAPIStatusU::Failure(result = GAPIStatus(swapChain_->GetDesc1(&currentSwapChainDesc))))
                    {
                        LOG_ERROR("Failure get swapChain Desc");
                        return result;
                    }

                    for (int index = 0; index < backBufferCount_; index++)
                    {
                        ThrowIfFailed(swapChain_->GetBuffer(index, IID_PPV_ARGS(renderTargets_[index].put())));
                        D3DUtils::SetAPIName(renderTargets_[index].get(), "BackBuffer", index);

                        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                        rtvDesc.Format = currentSwapChainDesc.Format;
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

                        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(
                            rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
                            static_cast<INT>(index), rtvDescriptorSize_);
                        d3dDevice_->CreateRenderTargetView(renderTargets_[index].get(), &rtvDesc, rtvDescriptor);
                    }

                    backBufferIndex_ = 0;

                    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
                    //   if (GAPIStatusU::Failure(dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER)))
                    // {
                    // }*/
                }

                GAPIStatus DeviceImplementation::Present()
                {
                    ASSERT_IS_CREATION_THREAD;
                    ASSERT_IS_DEVICE_INITED;

                    GAPIStatus result = GAPIStatus::OK;

                    const auto& commandQueue = getCommandQueue(CommandQueueType::GRAPHICS);

                    const UINT64 currentFenceValue = fenceValues_[frameIndex_];

                    {
                        // Transition the render target to the state that allows it to be presented to the display.
                        // D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_backBufferIndex].Get(), beforeState, D3D12_RESOURCE_STATE_PRESENT);
                        //commandList->ResourceBarrier(1, &barrier);
                    }

                    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[backBufferIndex_].get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    const auto& commandList = commandList_->GetCommandList();

                    commandList->ResourceBarrier(1, &barrier);
                    for (int i = 0; i < 100000; i++)
                    {
                        float color[4] = { std::rand() / static_cast<float>(RAND_MAX),
                            std::rand() / static_cast<float>(RAND_MAX),
                            std::rand() / static_cast<float>(RAND_MAX), 1 };

                        commandList->ClearRenderTargetView(getRenderTargetView(backBufferIndex_), color, 0, nullptr);
                    }
                    barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[backBufferIndex_].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
                    commandList->ResourceBarrier(1, &barrier);
                    // Send the command list off to the GPU for processing.
                    commandList->Close();

                    //  TODO Check correct fence work.
                    commandList_->Submit(commandQueue.get());

                    //HRESULT hr;
                    /*if (m_options & c_AllowTearing)
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
                    DXGI_PRESENT_PARAMETERS parameters
                        = {};
                    //  std::this_thread::sleep_for(10ms);
                    HRESULT hr = swapChain_->Present1(0, 0, &parameters);

                    // If the device was reset we must completely reinitialize the renderer.
                    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
                    {
                        Log::Print::Warning("Device Lost on Present: Reason code 0x%08X\n", static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? d3dDevice_->GetDeviceRemovedReason() : hr));

                        // Todo error check
                        handleDeviceLost();
                    }
                    else
                    {
                        if (GAPIStatusU::Failure(result = GAPIStatus(hr)))
                        {
                            LOG_ERROR("Fail on Present");
                            return result;
                        }

                        moveToNextFrame();

                        if (!dxgiFactory_->IsCurrent())
                        {
                            LOG_ERROR("Dxgi is not current");
                            return GAPIStatus::FAIL;

                            // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
                            //ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));
                        }
                    }

                    return result;
                }

                GAPIStatus DeviceImplementation::CompileCommandList(CommandList& commandList) const
                {
                    ASSERT_IS_CREATION_THREAD;
                    ASSERT_IS_DEVICE_INITED;
                    ASSERT(commandList.GetTargetSubmitFrame() != UNDEFINED_FRAME_INDEX)

                    CommandListCompilerContext compileContext(d3dDevice_.get(), &commandList);

                    return CommandListCompiler::Compile(compileContext);
                }

                GAPIStatus DeviceImplementation::SubmitCommandList(CommandList& commandList) const
                {
                    ASSERT_IS_CREATION_THREAD;
                    ASSERT_IS_DEVICE_INITED;

                    commandList.SetTargetSubmitFrame(UNDEFINED_FRAME_INDEX);

                    return GAPIStatus::OK;
                }

                GAPIStatus DeviceImplementation::handleDeviceLost()
                {
                    // Todo implement properly Device lost event processing
                    Log::Print::Fatal("Device was lost.");
                    return GAPIStatus::OK;
                }

                GAPIStatus DeviceImplementation::createDevice()
                {
                    ASSERT_IS_CREATION_THREAD;

                    GAPIStatus result = GAPIStatus::OK;

                    UINT dxgiFactoryFlags = 0;
                    // Enable the debug layer (requires the Graphics Tools "optional feature").
                    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
                    if (enableDebug_)
                    {
                        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController_.put()))))
                        {
                            debugController_->EnableDebugLayer();
                            debugController_->SetEnableGPUBasedValidation(true);
                            debugController_->SetEnableSynchronizedCommandQueueValidation(true);
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

                    if (GAPIStatusU::Failure(result = GAPIStatus(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(dxgiFactory_.put())))))
                    {
                        LOG_ERROR("Failure create DXGIFactory with HRESULT of 0x%08X", result);
                        return result;
                    }

                    D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;
                    if (GAPIStatusU::Failure(result = GAPIStatus(D3DUtils::GetAdapter(dxgiFactory_, minimumFeatureLevel, dxgiAdapter_))))
                    {
                        LOG_ERROR("Failure create Adapter with HRESULT of 0x%08X", result);
                        return result;
                    }

                    // Create the DX12 API device object.
                    if (GAPIStatusU::Failure(result = GAPIStatus(D3D12CreateDevice(dxgiAdapter_.get(), minimumFeatureLevel, IID_PPV_ARGS(d3dDevice_.put())))))
                    {
                        LOG_ERROR("Failure create Device with HRESULT of 0x%08X", result);
                        return result;
                    }

                    D3DUtils::SetAPIName(d3dDevice_.get(), "Main");
                    if (enableDebug_)
                    {
                        // Configure debug device (if active).
                        ComSharedPtr<ID3D12InfoQueue> d3dInfoQueue;

                        if (GAPIStatusU::Success(result = GAPIStatus(
                                                     d3dDevice_->QueryInterface(IID_PPV_ARGS(d3dInfoQueue.put())))))
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

                    return result;
                }

                void DeviceImplementation::moveToNextFrame()
                {
                    ASSERT_IS_CREATION_THREAD;
                    ASSERT_IS_DEVICE_INITED;

                    const auto& commandQueue = getCommandQueue(CommandQueueType::GRAPHICS);

                    // Schedule a Signal command in the queue.
                    const UINT64 currentFenceValue = fenceValues_[frameIndex_];
                    // ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

                    fence_->Signal(commandQueue.get(), currentFenceValue);

                    // Update the back buffer index.
                    backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
                    frameIndex_ = (frameIndex_++ % GPU_FRAMES_BUFFERED);

                    // If the next frame is not ready to be rendered yet, wait until it is ready.
                    if (fence_->GetGpuValue() < fenceValues_[frameIndex_])
                    {
                        fence_->SetEventOnCompletion(fenceValues_[frameIndex_], fenceEvent_.get());
                        //   ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent.Get()));
                        WaitForSingleObjectEx(fenceEvent_.get(), INFINITE, FALSE);
                    }

                    // Set the fence value for the next frame.
                    fenceValues_[frameIndex_] = currentFenceValue + 1;
                }

                Device::Device()
                    : _impl(new DeviceImplementation())
                {
                }

                Device::~Device() { }

                GAPIStatus Device::Init()
                {
                    return _impl->Init();
                }

                GAPIStatus Device::Reset(const PresentOptions& presentOptions)
                {
                    return _impl->Reset(presentOptions);
                }

                GAPIStatus Device::Present()
                {
                    return _impl->Present();
                }

                void Device::WaitForGpu()
                {
                    return _impl->WaitForGpu();
                }

                GAPIStatus Device::CompileCommandList(CommandList& commandList) const
                {
                    return _impl->CompileCommandList(commandList);
                }

                GAPIStatus Device::SubmitCommandList(CommandList& commandList) const
                {
                    return _impl->SubmitCommandList(commandList);
                }

                uint64_t Device::GetGpuFenceValue(Fence::ConstSharedPtrRef fence) const
                {
                    return 0;
                }

                GAPIStatus Device::InitResource(CommandList& commandList) const
                {
                    return GAPIStatus::OK;
                }
            }
        }
    }
}