#include "TestContextFixture.hpp"

#include "render/DeviceContext.hpp"

#include "Application.hpp"

namespace OpenDemo
{
    namespace Tests
    {
        void TestContextFixture::submitAndWait(const std::shared_ptr<GAPI::CommandQueue>& commandQueue, const std::shared_ptr<GAPI::CommandList>& commandList)
        {
            auto& renderContext = Render::DeviceContext::Instance();

            renderContext.Submit(commandQueue, commandList);
            renderContext.WaitForGpu(commandQueue);
            renderContext.MoveToNextFrame(commandQueue);
        }
    }
}