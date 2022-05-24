#pragma once

#include "common/Event.hpp"
#include "common/Math.hpp"
#include <any>

namespace RR::Platform
{
    class WindowSystem;

    namespace Input
    {
        enum class MouseButton : uint32_t;
        enum class ModifierFlag : uint32_t;
    }

    class Window : public std::enable_shared_from_this<Window>, private NonCopyable
    {
    public:
        using SharedPtr = std::shared_ptr<Window>;
        using SharedConstPtr = std::shared_ptr<const Window>;

        enum class Attribute : uint32_t
        {
            Cursor,
            Focused,
            Hovered,
            Maximized,
            Minimized,
            MousePassthrough,
            TaskbarIcon,
        };

        enum class Cursor : int32_t
        {
            Normal,
            Hidden,
            Disabled,
        };

        struct Description
        {
            U8String title = "";
            Vector2i size = { 0, 0 };

            bool autoIconify = true;
            bool centerCursor = true;
            bool decorated = true;
            bool floating = false;
            bool focused = true;
            bool focusOnShow = true;
            bool resizable = true;
            bool visible = true;
            bool mousePassthrough = false;
            bool taskbarIcon = true;
        };

    public:
        virtual ~Window() = default;

        virtual bool Init(const Description& description) = 0;

        virtual void ShowCursor(bool value) = 0;

        virtual Vector2i GetSize() const = 0;
        virtual Vector2i GetFramebufferSize() const = 0;
        virtual void SetSize(const Vector2i& size) const = 0;

        virtual Vector2i GetPosition() const = 0;
        virtual void SetPosition(const Vector2i& position) const = 0;

        virtual Vector2i GetMousePosition() const = 0;
        virtual void SetMousePosition(const Vector2i& position) const = 0;

        virtual void SetTitle(const U8String& title) const = 0;
        virtual void SetWindowAlpha(float alpha) const = 0;

        virtual int32_t GetWindowAttribute(Window::Attribute attribute) const = 0;
        virtual void SetWindowAttribute(Window::Attribute attribute, int32_t value) = 0;

        virtual void SetClipboardText(const U8String& text) const = 0;
        virtual U8String GetClipboardText() const = 0;

        virtual std::any GetNativeHandle() const = 0;
        virtual std::any GetNativeHandleRaw() const = 0;

        virtual void Focus() const = 0;
        virtual void Show() const = 0;

    public:
        Event<const Window&> OnClose;
        Event<const Window&, bool> OnFocus;
        Event<const Window&, Input::MouseButton, Input::ModifierFlag> OnMouseButtonPress;
        Event<const Window&, Input::MouseButton, Input::ModifierFlag> OnMouseButtonRelease;
        Event<const Window&, const Vector2i&> OnMouseMove;
        Event<const Window&, const Vector2i&> OnMouseWheel;
        Event<const Window&, const Vector2i&> OnMove;
        Event<const Window&, const Vector2i&> OnResize;
    };
}