#include "ComputeCommandList.hpp"

#include "TestContextFixture.hpp"

#include "ApprovalIntegration/ImageApprover.hpp"
#include "ApprovalTests/ApprovalTests.hpp"
#include <catch2/catch.hpp>

#include "gapi/Buffer.hpp"
#include "gapi/CommandList.hpp"
#include "gapi/CommandQueue.hpp"
#include "gapi/Texture.hpp"

#include "render/DeviceContext.hpp"

#include "common/OnScopeExit.hpp"

namespace RR
{
    namespace Tests
    {
        TEST_CASE_METHOD(TestContextFixture, "ComputeCommmandList", "[CommandList][ComputeCommandList]")
        {
            auto commandList = deviceContext.CreateCopyCommandList(u8"ComputeCommmandList");
            REQUIRE(commandList != nullptr);

            SECTION("Close")
            {
                commandList->Close();
            }
        }

        TEST_CASE_METHOD(TestContextFixture, "ClearBufferUAV", "[CommandList][ComputeCommandList][ClearUAV]")
        {
            auto commandList = deviceContext.CreateComputeCommandList(u8"ComputeCommmandList");
            REQUIRE(commandList != nullptr);

            auto queue = deviceContext.CreteCommandQueue(GAPI::CommandQueueType::Compute, "ComputeQueue");
            REQUIRE(queue != nullptr);

         /* SECTION("[Buffer::RawBuffer] UAV clear ClearUnorderedAccessViewUint")
            {
                const auto sourceData = "1234567890qwertyasdfg";
                const auto source = createBufferFromString(sourceData, "Source", GAPI::GpuResourceBindFlags::ShaderResource | GAPI::GpuResourceBindFlags::UnorderedAccess);
                const auto readbackData = deviceContext.AllocateIntermediateResourceData(source->GetDescription(), GAPI::MemoryAllocationType::Readback);

                const auto uav = source->GetUAV(GAPI::GpuResourceFormat::R32Uint);

                commandList->ClearUnorderedAccessViewUint(uav, Vector4u(0x61626365, 13, 8, 64));
                commandList->ReadbackGpuResource(source, readbackData);
                commandList->Close();

                submitAndWait(queue, commandList);

                const auto dataPointer = static_cast<uint8_t*>(readbackData->GetAllocation()->Map());
                ON_SCOPE_EXIT(
                    {
                        readbackData->GetAllocation()->Unmap();
                    });

                const auto testData = "ecbaecbaecbaecbaecbag";
                const auto& footprint = readbackData->GetSubresourceFootprintAt(0);
                REQUIRE(memcmp(dataPointer, testData, footprint.rowSizeInBytes) == 0);
            }

            SECTION(fmt::format("[Buffer::RawBuffer] Partical ClearUnorderedAccessViewFloat"))
            {
                const auto sourceData = "1234567890qwertyasdfg";
                const auto source = createBufferFromString(sourceData, "Source", GAPI::GpuResourceBindFlags::ShaderResource | GAPI::GpuResourceBindFlags::UnorderedAccess);
                const auto readbackData = deviceContext.AllocateIntermediateResourceData(source->GetDescription(), GAPI::MemoryAllocationType::Readback);

                const auto uav = source->GetUAV(GAPI::GpuResourceFormat::R32Uint, 4, 1);

                commandList->ClearUnorderedAccessViewFloat(uav, Vector4(0x66686984, 13, 8, 64));
                commandList->ReadbackGpuResource(source, readbackData);
                commandList->Close();

                submitAndWait(queue, commandList);

                const auto dataPointer = static_cast<uint8_t*>(readbackData->GetAllocation()->Map());
                ON_SCOPE_EXIT(
                    {
                        readbackData->GetAllocation()->Unmap();
                    });

                const auto testData = "1234567890qwerty�ihfg";
                const auto& footprint = readbackData->GetSubresourceFootprintAt(0);
                REQUIRE(memcmp(dataPointer, testData, footprint.rowSizeInBytes) == 0);
            }*/

            SECTION("[Texture2D::RGBA8] Partical ClearUnorderedAccessViewUint")
            {
                /*auto description = createTextureDescription(GAPI::GpuResourceDimension::Texture2D, 128, GAPI::GpuResourceFormat::RGBA8Uint, GAPI::GpuResourceUsage::Default);

                auto source = deviceContext.CreateTexture(description, createTestColorData(description), "Source");
                const auto readbackData = deviceContext.AllocateIntermediateResourceData(source->GetDescription(), GAPI::MemoryAllocationType::Readback);

                const auto uav = source->GetUAV(0);

                commandList->UpdateGpuResource(source, sourceData);
                commandList->ClearUnorderedAccessViewUint(uav, Vector4u(0x66686984, 13, 8, 64));
                commandList->ReadbackGpuResource(source, readbackData);
                commandList->Close();

                submitAndWait(queue, commandList);

                const auto dataPointer = static_cast<uint8_t*>(readbackData->GetAllocation()->Map());
                ON_SCOPE_EXIT(
                    {
                        readbackData->GetAllocation()->Unmap();
                    });

                const auto& footprint = readbackData->GetSubresourceFootprintAt(0);*/ 
                // REQUIRE(memcmp(dataPointer, testData, footprint.rowSizeInBytes) == 0);
            }
        }
    }
}