#include "CopyCommandList.hpp"

#include "TestContextFixture.hpp"

#include <catch2/catch.hpp>

#include "ApprovalTests/ApprovalTests.hpp"

#include "gapi/CommandList.hpp"
#include "gapi/CommandQueue.hpp"
#include "gapi/MemoryAllocation.hpp"
#include "gapi/Texture.hpp"

#include "render/RenderContext.hpp"

#include "common/Math.hpp"

namespace OpenDemo
{
    namespace Tests
    {
        namespace
        {
            template <typename T>
            T texelZeroFill(Vector3u texel, uint32_t level)
            {
                return T(0);
            }

            template <typename T>
            T checkerboardPattern(Vector3u texel, uint32_t level);

            template <>
            Vector4 checkerboardPattern(Vector3u texel, uint32_t level)
            {
                if ((texel.x + texel.y + texel.z) / 8 + level & 1 == 0)
                {
                    return Vector4(0.5f, 0.5f, 0.5f, 0.5f);
                }

                std::array<Vector4, 8> colors = {
                    Vector4(0, 0, 1, 1),
                    Vector4(0, 1, 0, 1),
                    Vector4(1, 0, 0, 1),
                    Vector4(0, 1, 1, 1),
                    Vector4(1, 0, 1, 1),
                    Vector4(1, 1, 0, 1),
                    Vector4(1, 1, 1, 1),
                    Vector4(0.25, 0.25, 0.25, 1),
                };

                return colors[std::min(level, 7u)];
            }

            template <>
            uint32_t checkerboardPattern(Vector3u texel, uint32_t level)
            {
                const auto& value = checkerboardPattern<Vector4>(texel, level);
                // TODO olor class
                // RGBA format
                return static_cast<uint32_t>(value.x * 255.0f) << 24 |
                       static_cast<uint32_t>(value.y * 255.0f) << 16 |
                       static_cast<uint32_t>(value.z * 255.0f) << 8 |
                       static_cast<uint32_t>(value.w * 255.0f);
            }

            GAPI::Texture::SharedPtr CreateTestTexture(const GAPI::TextureDescription& description, const U8String& name, GAPI::GpuResourceCpuAccess cpuAcess = GAPI::GpuResourceCpuAccess::None, GAPI::GpuResourceBindFlags bindFlags = GAPI::GpuResourceBindFlags::None)
            {
                auto& renderContext = Render::RenderContext::Instance();

                auto texture = renderContext.CreateTexture(description, bindFlags, {}, cpuAcess, name);
                REQUIRE(texture);

                return texture;
            }

            template <typename T>
            void initTextureData(const GAPI::TextureDescription& description, const GAPI::IntermediateMemory::SharedPtr& data)
            {
                auto& renderContext = Render::RenderContext::Instance();
                const auto textureData = renderContext.AllocateIntermediateTextureData(description, GAPI::MemoryAllocationType::Upload);

                ASSERT((std::is_same<T, uint32_t>::value && description.format == GAPI::GpuResourceFormat::RGBA8Uint) ||
                       (std::is_same<T, Vector4>::value && description.format == GAPI::GpuResourceFormat::RGBA16Float) ||
                       (std::is_same<T, Vector4>::value && description.format == GAPI::GpuResourceFormat::RGBA32Float));

                const auto& subresourceFootprints = data->GetSubresourceFootprints();

                for (uint32_t index = 0; index < subresourceFootprints.size(); index++)
                {
                    const auto& subresourceFootprint = subresourceFootprints[index];

                    auto rowPointer = static_cast<uint8_t*>(subresourceFootprint.data);
                    for (uint32_t row = 0; row < subresourceFootprint.numRows; row++)
                    {
                        auto columnPointer = reinterpret_cast<T*>(rowPointer);
                        for (uint32_t column = 0; column < description.width; column++)
                        {
                            const auto texel = Vector3u(row, column, 0);

                            *columnPointer = checkerboardPattern<T>(texel, index);
                            columnPointer++;
                        }

                        rowPointer += subresourceFootprint.rowPitch;
                    }
                }
            }

            bool isResourceDataEqual(const GAPI::IntermediateMemory::SharedPtr& lhs,
                                     const GAPI::IntermediateMemory::SharedPtr& rhs)
            {
                ASSERT(lhs->GetNumSubresources() == rhs->GetNumSubresources());

                const auto numSubresources = lhs->GetNumSubresources();
                for (uint32_t index = 0; index < numSubresources; index++)
                {
                    const auto& lfootprint = lhs->GetSubresourceFootprintAt(index);
                    const auto& rfootprint = rhs->GetSubresourceFootprintAt(index);

                    ASSERT(lfootprint.rowSizeInBytes == rfootprint.rowSizeInBytes);
                    ASSERT(lfootprint.rowPitch == rfootprint.rowPitch);
                    ASSERT(lfootprint.depthPitch == rfootprint.depthPitch);
                    ASSERT(lfootprint.numRows == rfootprint.numRows);

                    auto lrowPointer = static_cast<uint8_t*>(lfootprint.data);
                    auto rrowPointer = static_cast<uint8_t*>(rfootprint.data);

                    for (uint32_t row = 0; row < lfootprint.numRows; row++)
                    {
                        if (memcmp(lrowPointer, rrowPointer, lfootprint.rowSizeInBytes) != 0)
                            return false;

                        lrowPointer += lfootprint.rowPitch;
                        rrowPointer += rfootprint.rowPitch;
                    }
                }

                return true;
            }
        }

        TEST_CASE_METHOD(TestContextFixture, "CopyCommmanList", "[CommandList][CopyCommmanList]")
        {
            auto& renderContext = Render::RenderContext::Instance();

            auto commandList = renderContext.CreateCopyCommandList(u8"CopyCommandList");
            REQUIRE(commandList != nullptr);

            auto copyQueue = renderContext.CreteCommandQueue(GAPI::CommandQueueType::Copy, "CopyQueue");
            REQUIRE(copyQueue != nullptr);

            SECTION("Close")
            {
                commandList->Close();
            }

            SECTION("CopyTexture_RGBA8")
            {
                const auto& description = GAPI::TextureDescription::Create2D(128, 128, GAPI::GpuResourceFormat::RGBA8Uint);

                const auto sourceData = renderContext.AllocateIntermediateTextureData(description, GAPI::MemoryAllocationType::Upload);
                const auto readbackData = renderContext.AllocateIntermediateTextureData(description, GAPI::MemoryAllocationType::Readback);

                initTextureData<uint32_t>(description, sourceData);

                auto source = renderContext.CreateTexture(description, GAPI::GpuResourceBindFlags::None, {}, GAPI::GpuResourceCpuAccess::None, "Source");
                auto dest = renderContext.CreateTexture(description, GAPI::GpuResourceBindFlags::None, {}, GAPI::GpuResourceCpuAccess::None, "Dest");

                commandList->UpdateTexture(source, sourceData);
                commandList->CopyTexture(source, dest);
                commandList->ReadbackTexture(dest, readbackData);

                commandList->Close();

                submitAndWait(copyQueue, commandList);
                REQUIRE(isResourceDataEqual(sourceData, readbackData));
            }

            SECTION("CopyTexture_Float")
            {
                const auto& description = GAPI::TextureDescription::Create2D(128, 128, GAPI::GpuResourceFormat::RGBA16Float);

                const auto sourceData = renderContext.AllocateIntermediateTextureData(description, GAPI::MemoryAllocationType::Upload);
                const auto readbackData = renderContext.AllocateIntermediateTextureData(description, GAPI::MemoryAllocationType::Readback);

                initTextureData<Vector4>(description, sourceData);

                auto source = renderContext.CreateTexture(description, GAPI::GpuResourceBindFlags::None, {}, GAPI::GpuResourceCpuAccess::None, "Source");
                auto dest = renderContext.CreateTexture(description, GAPI::GpuResourceBindFlags::None, {}, GAPI::GpuResourceCpuAccess::None, "Dest");

                commandList->UpdateTexture(source, sourceData);
                commandList->CopyTexture(source, dest);
                commandList->ReadbackTexture(dest, readbackData);

                commandList->Close();

                submitAndWait(copyQueue, commandList);
                REQUIRE(isResourceDataEqual(sourceData, readbackData));
            }
        }

        TEST_CASE("HelloApprovals")
        {
            //     ApprovalTests::Approvals::verify("Hello Approvals!");
        }
    }
}