#include "Application.hpp"

#include "common/Time.hpp"

#include "inputting/Input.hpp"

#include "resource_manager/ResourceManager.hpp"

#include "gapi_dx12/Device.hpp"

#include "rendering/Mesh.hpp"
#include "rendering/Primitives.hpp"
#include "rendering/Render.hpp"
#include "rendering/RenderPipeline.hpp"
#include "rendering/Shader.hpp"

#include "windowing/InputtingWindow.hpp"
#include "windowing/WindowSettings.hpp"
#include "windowing/Windowing.hpp"

#include "scenes/Scene_1.hpp"
#include "scenes/Scene_2.hpp"

namespace OpenDemo
{
    using namespace Common;

    void Application::OnWindowResize(const Windowing::Window& window_)
    {
        if (!_device)
            return;
        int width = window_.GetWidth();
        int height = window_.GetHeight();

        Render::Device::PresentOptions presentOptions;
        presentOptions.bufferCount = 2;
        presentOptions.isStereo = false;
        presentOptions.rect = RectU(0, 0, width, height);
        presentOptions.resourceFormat = Render::ResourceFormat::Unknown;
        presentOptions.windowHandle = _window->GetNativeHandle();

        _device->Reset(presentOptions);
    }

    void Application::Start()
    {
        init();
        // loadResouces();
        _device.reset(new Render::Device::DX12::Device());
        // TODO REMOVE IT
   
        _device->Init();
        Render::Device::PresentOptions presentOptions;
        presentOptions.bufferCount = 2;
        presentOptions.isStereo = false;
        presentOptions.rect = RectU(0, 0, 100, 100);
        presentOptions.resourceFormat = Render::ResourceFormat::Unknown;
        presentOptions.windowHandle = _window->GetNativeHandle();
        _device->Reset(presentOptions);

        //  const auto& input = Inputting::Instance();
        const auto& time = Time::Instance();
        //   const auto& render = Rendering::Instance();
        //  auto* renderPipeline = new Rendering::RenderPipeline(_window);
        //  renderPipeline->Init();

        time->Init();

        while (!_quit)
        {
            Windowing::Windowing::PoolEvents();
            //  renderPipeline->Collect(_scene);
            //    renderPipeline->Draw();

            //   _scene->Update();
            _device->Present();
            //    render->SwapBuffers();
            //  input->Update();

            time->Update();
        }

        //  delete renderPipeline;

        terminate();
    }

    void Application::OnQuit()
    {
        _quit = true;
    }

    void Application::init()
    {
        Windowing::WindowSettings settings;
        Windowing::WindowRect rect(Windowing::WindowRect::WINDOW_POSITION_CENTERED,
            Windowing::WindowRect::WINDOW_POSITION_CENTERED, 800, 600);

        settings.Title = "OpenDemo";
        settings.WindowRect = rect;

        Windowing::Windowing::Subscribe(this);
        _window = std::shared_ptr<Windowing::InputtingWindow>(new Windowing::InputtingWindow());
        _window->Init(settings, true);

        Inputting::Instance()->Init();
        Inputting::Instance()->SubscribeToWindow(_window);

        // auto& render = Rendering::Instance();
        // render->Init(_window);
    }

    void Application::terminate()
    {
        _scene->Terminate();

        _window.reset();
        _window = nullptr;

        Inputting::Instance()->Terminate();

        // Rendering::Instance()->Terminate();
        Windowing::Windowing::UnSubscribe(this);
    }

    void Application::loadResouces()
    {
        _scene = std::make_shared<Scenes::Scene_2>();
        _scene->Init();
    }
}