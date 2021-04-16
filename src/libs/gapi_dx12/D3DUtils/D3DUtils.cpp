#include "D3DUtils.hpp"

#include "gapi/Buffer.hpp"
#include "gapi/Device.hpp"
#include "gapi/SwapChain.hpp"
#include "gapi/Texture.hpp"

#include <comdef.h>

namespace OpenDemo
{
    namespace GAPI
    {
        namespace DX12
        {
            namespace D3DUtils
            {
                // Copy from _com_error::ErrorMessage(), with english locale
                inline const TCHAR* ErrorMessage(HRESULT hr) throw()
                {
                    TCHAR* pszMsg;
                    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL,
                                  hr,
                                  MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                                  (LPTSTR)&pszMsg,
                                  0,
                                  NULL);
                    if (pszMsg != NULL)
                    {
#ifdef UNICODE
                        size_t const nLen = wcslen(pszMsg);
#else
                        size_t const nLen = strlen(m_pszMsg);
#endif
                        if (nLen > 1 && pszMsg[nLen - 1] == '\n')
                        {
                            pszMsg[nLen - 1] = 0;
                            if (pszMsg[nLen - 2] == '\r')
                            {
                                pszMsg[nLen - 2] = 0;
                            }
                        }
                    }
                    else
                    {
                        pszMsg = (LPTSTR)LocalAlloc(0, 32 * sizeof(TCHAR));
                        if (pszMsg != NULL)
                        {
                            WORD wCode = _com_error::HRESULTToWCode(hr);
                            if (wCode != 0)
                            {
                                _COM_PRINTF_S_1(pszMsg, 32, TEXT("IDispatch error #%d"), (int)wCode);
                            }
                            else
                            {
                                _COM_PRINTF_S_1(pszMsg, 32, TEXT("Unknown error 0x%0lX"), hr);
                            }
                        }
                    }
                    return pszMsg;
                }

                U8String HResultToString(HRESULT hr)
                {
                    if (SUCCEEDED(hr))
                        return "";

                    const auto pMessage = ErrorMessage(hr);
                    const auto& messageString = StringConversions::WStringToUTF8(pMessage);
                    LocalFree((HLOCAL)pMessage);

                    return messageString;
                }

                struct GpuResourceFormatConversion
                {
                    GpuResourceFormat from;
                    ::DXGI_FORMAT to;
                };

                // clang-format off
            static GpuResourceFormatConversion formatsConversion[] = {
                { GpuResourceFormat::Unknown,           DXGI_FORMAT_UNKNOWN },
                { GpuResourceFormat::RGBA32Float,       DXGI_FORMAT_R32G32B32A32_FLOAT },
                { GpuResourceFormat::RGBA32Uint,        DXGI_FORMAT_R32G32B32A32_UINT },
                { GpuResourceFormat::RGBA32Sint,        DXGI_FORMAT_R32G32B32A32_SINT },
                { GpuResourceFormat::RGB32Float,        DXGI_FORMAT_R32G32B32_FLOAT },
                { GpuResourceFormat::RGB32Uint,         DXGI_FORMAT_R32G32B32_UINT },
                { GpuResourceFormat::RGB32Sint,         DXGI_FORMAT_R32G32B32_SINT },
                { GpuResourceFormat::RGBA16Float,       DXGI_FORMAT_R16G16B16A16_FLOAT },
                { GpuResourceFormat::RGBA16Unorm,       DXGI_FORMAT_R16G16B16A16_UNORM },
                { GpuResourceFormat::RGBA16Uint,        DXGI_FORMAT_R16G16B16A16_UINT },
                { GpuResourceFormat::RGBA16Snorm,       DXGI_FORMAT_R16G16B16A16_SNORM },
                { GpuResourceFormat::RGBA16Sint,        DXGI_FORMAT_R16G16B16A16_SINT },
                { GpuResourceFormat::RG32Float,         DXGI_FORMAT_R32G32_FLOAT },
                { GpuResourceFormat::RG32Uint,          DXGI_FORMAT_R32G32_UINT },
                { GpuResourceFormat::RG32Sint,          DXGI_FORMAT_R32G32_SINT },

                { GpuResourceFormat::RGB10A2Unorm,      DXGI_FORMAT_R10G10B10A2_UNORM },
                { GpuResourceFormat::RGB10A2Uint,       DXGI_FORMAT_R10G10B10A2_UINT },
                { GpuResourceFormat::R11G11B10Float,    DXGI_FORMAT_R11G11B10_FLOAT },
                { GpuResourceFormat::RGBA8Unorm,        DXGI_FORMAT_R8G8B8A8_UNORM },
                { GpuResourceFormat::RGBA8UnormSrgb,    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
                { GpuResourceFormat::RGBA8Uint,         DXGI_FORMAT_R8G8B8A8_UINT },
                { GpuResourceFormat::RGBA8Snorm,        DXGI_FORMAT_R8G8B8A8_SNORM },
                { GpuResourceFormat::RGBA8Sint,         DXGI_FORMAT_R8G8B8A8_SINT },
                { GpuResourceFormat::RG16Float,         DXGI_FORMAT_R16G16_FLOAT },
                { GpuResourceFormat::RG16Unorm,         DXGI_FORMAT_R16G16_UNORM },
                { GpuResourceFormat::RG16Uint,          DXGI_FORMAT_R16G16_UINT },
                { GpuResourceFormat::RG16Snorm,         DXGI_FORMAT_R16G16_SNORM },
                { GpuResourceFormat::RG16Sint,          DXGI_FORMAT_R16G16_SINT },

                { GpuResourceFormat::R32Float,          DXGI_FORMAT_R32_FLOAT },
                { GpuResourceFormat::R32Uint,           DXGI_FORMAT_R32_UINT },
                { GpuResourceFormat::R32Sint,           DXGI_FORMAT_R32_SINT },

                { GpuResourceFormat::RG8Unorm,          DXGI_FORMAT_R8G8_UNORM },
                { GpuResourceFormat::RG8Uint,           DXGI_FORMAT_R8G8_UINT },
                { GpuResourceFormat::RG8Snorm,          DXGI_FORMAT_R8G8_SNORM },
                { GpuResourceFormat::RG8Sint,           DXGI_FORMAT_R8G8_SINT },

                { GpuResourceFormat::R16Float,          DXGI_FORMAT_R16_FLOAT },
                { GpuResourceFormat::R16Unorm,          DXGI_FORMAT_R16_UNORM },
                { GpuResourceFormat::R16Uint,           DXGI_FORMAT_R16_UINT },
                { GpuResourceFormat::R16Snorm,          DXGI_FORMAT_R16_SNORM },
                { GpuResourceFormat::R16Sint,           DXGI_FORMAT_R16_SINT },
                { GpuResourceFormat::R8Unorm,           DXGI_FORMAT_R8_UNORM },
                { GpuResourceFormat::R8Uint,            DXGI_FORMAT_R8_UINT },
                { GpuResourceFormat::R8Snorm,           DXGI_FORMAT_R8_SNORM },
                { GpuResourceFormat::R8Sint,            DXGI_FORMAT_R8_SINT },
                { GpuResourceFormat::A8Unorm,           DXGI_FORMAT_A8_UNORM },

                { GpuResourceFormat::D32FloatS8X24Uint, DXGI_FORMAT_D32_FLOAT_S8X24_UINT },
                { GpuResourceFormat::D32Float,          DXGI_FORMAT_D32_FLOAT },
                { GpuResourceFormat::D24UnormS8Uint,    DXGI_FORMAT_D24_UNORM_S8_UINT },
                { GpuResourceFormat::D16Unorm,          DXGI_FORMAT_D16_UNORM },

                { GpuResourceFormat::R32FloatX8X24,     DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS },
                { GpuResourceFormat::X32G8Uint,         DXGI_FORMAT_X32_TYPELESS_G8X24_UINT },
                { GpuResourceFormat::R24UnormX8,        DXGI_FORMAT_R24_UNORM_X8_TYPELESS },
                { GpuResourceFormat::X24G8Uint,         DXGI_FORMAT_X24_TYPELESS_G8_UINT },

                { GpuResourceFormat::BC1Unorm,          DXGI_FORMAT_BC1_UNORM },
                { GpuResourceFormat::BC1UnormSrgb,      DXGI_FORMAT_BC1_UNORM_SRGB },
                { GpuResourceFormat::BC2Unorm,          DXGI_FORMAT_BC2_UNORM },
                { GpuResourceFormat::BC2UnormSrgb,      DXGI_FORMAT_BC2_UNORM_SRGB },
                { GpuResourceFormat::BC3Unorm,          DXGI_FORMAT_BC3_UNORM },
                { GpuResourceFormat::BC3UnormSrgb,      DXGI_FORMAT_BC3_UNORM_SRGB },
                { GpuResourceFormat::BC4Unorm,          DXGI_FORMAT_BC4_UNORM },
                { GpuResourceFormat::BC4Snorm,          DXGI_FORMAT_BC4_SNORM },
                { GpuResourceFormat::BC5Unorm,          DXGI_FORMAT_BC5_UNORM },
                { GpuResourceFormat::BC5Snorm,          DXGI_FORMAT_BC5_SNORM },
                { GpuResourceFormat::BC6HU16,           DXGI_FORMAT_BC6H_UF16 },
                { GpuResourceFormat::BC6HS16,           DXGI_FORMAT_BC6H_SF16 },
                { GpuResourceFormat::BC7Unorm,          DXGI_FORMAT_BC7_UNORM },
                { GpuResourceFormat::BC7UnormSrgb,      DXGI_FORMAT_BC7_UNORM_SRGB },

                { GpuResourceFormat::RGB16Float,        DXGI_FORMAT_UNKNOWN },
                { GpuResourceFormat::RGB16Unorm,        DXGI_FORMAT_UNKNOWN },
                { GpuResourceFormat::RGB16Uint,         DXGI_FORMAT_UNKNOWN },
                { GpuResourceFormat::RGB16Snorm,        DXGI_FORMAT_UNKNOWN },
                { GpuResourceFormat::RGB16Sint,         DXGI_FORMAT_UNKNOWN },

                { GpuResourceFormat::RGB5A1Unorm,       DXGI_FORMAT_B5G5R5A1_UNORM },
                { GpuResourceFormat::RGB9E5Float,       DXGI_FORMAT_R9G9B9E5_SHAREDEXP },

                { GpuResourceFormat::BGRA8Unorm,        DXGI_FORMAT_B8G8R8A8_UNORM },
                { GpuResourceFormat::BGRA8UnormSrgb,    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
                { GpuResourceFormat::BGRX8Unorm,        DXGI_FORMAT_B8G8R8X8_UNORM },
                { GpuResourceFormat::BGRX8UnormSrgb,    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB },

                { GpuResourceFormat::R5G6B5Unorm,       DXGI_FORMAT_B5G6R5_UNORM },
            }; // clang-format on

                static_assert(std::is_same<std::underlying_type<GpuResourceFormat>::type, uint32_t>::value);
                static_assert(std::size(formatsConversion) == static_cast<uint32_t>(GpuResourceFormat::Count));

                ::DXGI_FORMAT GetDxgiResourceFormat(GpuResourceFormat format)
                {
                    ASSERT(formatsConversion[static_cast<uint32_t>(format)].from == format);
                    ASSERT(format == GpuResourceFormat::Unknown ||
                           formatsConversion[static_cast<uint32_t>(format)].to != DXGI_FORMAT_UNKNOWN);

                    return formatsConversion[static_cast<uint32_t>(format)].to;
                }

                DXGI_FORMAT GetDxgiTypelessFormat(GpuResourceFormat format)
                {
                    switch (format)
                    {
                    case GpuResourceFormat::D16Unorm:
                        return DXGI_FORMAT_R16_TYPELESS;
                    case GpuResourceFormat::D32FloatS8X24Uint:
                        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                    case GpuResourceFormat::D24UnormS8Uint:
                        return DXGI_FORMAT_R24G8_TYPELESS;
                    case GpuResourceFormat::D32Float:
                        return DXGI_FORMAT_R32_TYPELESS;
                    default:
                        ASSERT(!GpuResourceFormatInfo::IsDepth(format));
                        return GetDxgiResourceFormat(format);
                    }
                }

                DXGI_FORMAT SRGBToLinear(DXGI_FORMAT format)
                {
                    switch (format)
                    {
                    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                        return DXGI_FORMAT_R8G8B8A8_UNORM;
                    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                        return DXGI_FORMAT_B8G8R8A8_UNORM;
                    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
                        return DXGI_FORMAT_B8G8R8X8_UNORM;
                    default:
                        return format;
                    }
                }

                D3D12_RESOURCE_FLAGS GetResourceFlags(GpuResourceBindFlags flags)
                {
                    D3D12_RESOURCE_FLAGS d3d = D3D12_RESOURCE_FLAG_NONE;

                    d3d |= IsSet(flags, GpuResourceBindFlags::UnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
                    // Flags cannot have D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE set without D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                    d3d |= !IsSet(flags, GpuResourceBindFlags::ShaderResource) && IsSet(flags, GpuResourceBindFlags::DepthStencil) ? D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE : D3D12_RESOURCE_FLAG_NONE;
                    d3d |= IsSet(flags, GpuResourceBindFlags::DepthStencil) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;
                    d3d |= IsSet(flags, GpuResourceBindFlags::RenderTarget) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;

                    return d3d;
                }

                D3D12_RESOURCE_DESC GetResourceDesc(const GpuResourceDescription& resourceDesc)
                {
                    DXGI_FORMAT format = GetDxgiResourceFormat(resourceDesc.GetFormat());

                    if (GpuResourceFormatInfo::IsDepth(resourceDesc.GetFormat()) && IsAny(resourceDesc.GetBindFlags(), GpuResourceBindFlags::ShaderResource | GpuResourceBindFlags::UnorderedAccess))
                        format = GetDxgiTypelessFormat(resourceDesc.GetFormat());

                    D3D12_RESOURCE_DESC desc;
                    switch (resourceDesc.GetDimension())
                    {
                    case GpuResourceDimension::Buffer:
                    {
                        const auto blockSize = (resourceDesc.GetFormat() == GpuResourceFormat::Unknown) ? 1 : GpuResourceFormatInfo::GetBlockSize(resourceDesc.GetFormat());
                        desc = CD3DX12_RESOURCE_DESC::Buffer(resourceDesc.GetWidth() * blockSize);
                    }
                    break;
                    case GpuResourceDimension::Texture1D:
                        desc = CD3DX12_RESOURCE_DESC::Tex1D(format, resourceDesc.GetWidth(), resourceDesc.GetArraySize(), resourceDesc.GetMipCount());
                        break;
                    case GpuResourceDimension::Texture2D:
                    case GpuResourceDimension::Texture2DMS:
                        desc = CD3DX12_RESOURCE_DESC::Tex2D(format, resourceDesc.GetWidth(), resourceDesc.GetHeight(), resourceDesc.GetArraySize(), resourceDesc.GetMipCount(), resourceDesc.GetSampleCount());
                        break;
                    case GpuResourceDimension::Texture3D:
                        desc = CD3DX12_RESOURCE_DESC::Tex3D(format, resourceDesc.GetWidth(), resourceDesc.GetHeight(), resourceDesc.GetDepth(), resourceDesc.GetMipCount());
                        break;
                    case GpuResourceDimension::TextureCube:
                        desc = CD3DX12_RESOURCE_DESC::Tex2D(format, resourceDesc.GetWidth(), resourceDesc.GetHeight(), resourceDesc.GetArraySize() * 6, resourceDesc.GetMipCount());
                        break;
                    default:
                        LOG_FATAL("Unsupported texture dimension");
                    }

                    desc.Flags = GetResourceFlags(resourceDesc.GetBindFlags());
                    return desc;
                }

                /*
                * TODO BUFFER SUPPORT
                D3D12_RESOURCE_DESC GetResourceDesc(const BufferDescription& resourceDesc)
                {
                    D3D12_RESOURCE_DESC desc;

                    ASSERT_MSG(false, "Fix bindflags")
                    desc = CD3DX12_RESOURCE_DESC::Buffer(resourceDesc.size);
                    desc.Flags = TypeConversions::GetResourceFlags(GpuResourceBindFlags::None);
                    desc.Format = TypeConversions::GetGpuResourceFormat(resourceDesc.format);
                    return desc;
                }*/

                bool SwapChainDesc1MatchesForReset(const DXGI_SWAP_CHAIN_DESC1& left, const DXGI_SWAP_CHAIN_DESC1& right)
                {
                    return (left.Stereo == right.Stereo &&
                            left.SampleDesc.Count == right.SampleDesc.Count &&
                            left.SampleDesc.Quality == right.SampleDesc.Quality &&
                            left.BufferUsage == right.BufferUsage &&
                            left.SwapEffect == right.SwapEffect &&
                            left.Flags == right.Flags);
                }

                DXGI_SWAP_CHAIN_DESC1 GetDxgiSwapChainDesc1(const SwapChainDescription& description, DXGI_SWAP_EFFECT swapEffect)
                {
                    ASSERT(description.width >= 0);
                    ASSERT(description.height >= 0);
                    ASSERT(description.bufferCount > 0 && description.bufferCount <= MAX_BACK_BUFFER_COUNT);

                    DXGI_SWAP_CHAIN_DESC1 output;
                    output.Width = description.width;
                    output.Height = description.height;
                    output.Format = GetDxgiResourceFormat(description.gpuResourceFormat);
                    output.Stereo = (description.isStereo) ? TRUE : FALSE;
                    output.SampleDesc = { 1, 0 };
                    output.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    output.BufferCount = description.bufferCount;
                    output.Scaling = DXGI_SCALING_STRETCH;
                    output.SwapEffect = swapEffect;
                    output.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                    output.Flags = 0;
                    return output;
                }

                // Todo replace to void
                HRESULT GetAdapter(const ComSharedPtr<IDXGIFactory1>& dxgiFactory, D3D_FEATURE_LEVEL minimumFeatureLevel, ComSharedPtr<IDXGIAdapter1>& adapter)
                {
                    HRESULT result = E_FAIL;

                    for (uint32_t adapterIndex = 0;; ++adapterIndex)
                    {
                        if (FAILED(result = dxgiFactory->EnumAdapters1(adapterIndex, adapter.put())))
                            break;

                        DXGI_ADAPTER_DESC1 desc;
                        if (FAILED(result = adapter->GetDesc1(&desc)))
                            break;

                        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                        {
                            // Don't select the software adapter.
                            continue;
                        }

                        // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
                        if (SUCCEEDED(result = D3D12CreateDevice(adapter.get(), minimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
                        {
                            Log::Print::Info("Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, StringConversions::WStringToUTF8(desc.Description));
                            break;
                        }
                    }

                    return result;
                }
            }
        }
    }
}