#pragma once

#include "gapi/DeviceInterface.hpp"
#include "windowing/Windowing.hpp"

namespace OpenDemo
{
    namespace Windowing
    {
        class InputtingWindow;
    }

    namespace Rendering
    {
        class Mesh;
        class SceneGraph;
    }

    class Application : public Windowing::IListener
    {
    public:
        Application() = default;

        void Start();
        virtual void OnQuit() override;

    private:
        bool _quit = false;

        std::shared_ptr<Windowing::InputtingWindow> _window;
        std::shared_ptr<Rendering::SceneGraph> _scene;
        std::shared_ptr<GAPI::SwapChain> swapChain_;

        void init();
        void terminate();

        void loadResouces();

        void OnWindowResize(const Windowing::Window& window) override;
    };
}