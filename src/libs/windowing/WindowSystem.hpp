#pragma once

#include "windowing/Window.hpp"

#include "common/Singleton.hpp"

namespace RR
{
    namespace Windowing
    {
#ifdef OS_WINDOWS
        constexpr wchar_t WINDOW_CLASS_NAME[] = L"RedRevenWndClass";
#endif

        class WindowSystem final : public Singleton<WindowSystem>
        {
        public:
            WindowSystem();
            ~WindowSystem();

            void Init();

            std::shared_ptr<Window> Create(Window::ICallbacks* callbacks, const Window::Description& description) const;
            void PoolEvents() const;

        private:
            bool isInited_ = false;
        };
    }
}