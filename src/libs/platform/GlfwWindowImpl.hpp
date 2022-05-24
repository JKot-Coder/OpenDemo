#pragma once

#include "platform/Window.hpp"

#ifdef OS_WINDOWS
#include <Windows.h>
#endif

#include <GLFW/glfw3.h>

namespace RR
{
    namespace Platform
    {
        class GlfwWindowSystem;

        class GlfwWindowImpl final : public Window
        {
        public:
            ~GlfwWindowImpl() override;

            bool Init(const Window::Description& description) override;

            void ShowCursor(bool value) override;

            Vector2i GetSize() const override;
            Vector2i GetFramebufferSize() const override;
            void SetSize(const Vector2i& size) const override;

            Vector2i GetPosition() const override;
            void SetPosition(const Vector2i& position) const override;

            Vector2i GetMousePosition() const override;
            void SetMousePosition(const Vector2i& position) const override;

            void SetTitle(const U8String& title) const override;
            void SetWindowAlpha(float alpha) const override;

            int32_t GetWindowAttribute(Window::Attribute attribute) const override;
            void SetWindowAttribute(Window::Attribute attribute, int32_t value) override;

            void SetClipboardText(const U8String& text) const override;
            U8String GetClipboardText() const override;

            std::any GetNativeHandle() const override;
            std::any GetNativeHandleRaw() const override;

            void Focus() const override;
            void Show() const override;

        private:
            GlfwWindowImpl() = default;

            static void windowCloseCallback(GLFWwindow* glfwWindow);
            static void windowFocusCallback(GLFWwindow* glfwWindow, int focused);
            static void mouseMoveCallback(GLFWwindow* glfwWindow, double x, double y);
            static void mouseButtonCallback(GLFWwindow* glfwWindow, int button, int action, int mods);
            static void windowPosCallback(GLFWwindow* glfwWindow, int x, int y);
            static void windowResizeCallback(GLFWwindow* glfwWindow, int width, int height);
            static void windowUpdateCallback(GLFWwindow* glfwWindow);

            void setWindowMousePassthrough(bool enabled);
            void setTaskbarIcon(bool enabled);

        private:
            GLFWwindow* window_;
#ifdef OS_WINDOWS
            HBRUSH bgBrush_;
#endif
            bool mousePassthrough_ = false;
            bool taskbarIcon_ = false;

        private:
            friend class Platform::WindowSystem;
        };
    }
}