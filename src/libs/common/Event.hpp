#pragma once

#include <functional>

namespace RR
{
    namespace Common
    {

        template <typename Signature>
        class Event;

        template <typename ReturnType, typename... Args>
        class Event<ReturnType(Args...)>
        {
        private:
            struct Delegate
            {
            public:
                ReturnType operator()(Args... args) const
                {
                    return (*callback_)(target_, args...);
                }

                bool operator==(const Delegate& other) const
                {
                    return (callback_ == other.callback_) && (target_ == other.target_);
                }

                bool operator!=(const Delegate& other) const
                {
                    return !(*this == other);
                }

            public:
                template <auto Callback>
                static Delegate Create()
                {
                    Delegate delegate;
                    delegate.target_ = nullptr;
                    delegate.callback_ = +[](void*, Args... args) -> ReturnType
                    {
                        return Callback(args...);
                    };

                    return delegate;
                }

                template <class Class, auto Callback>
                static Delegate Create(Class* target)
                {
                    Delegate delegate;
                    delegate.target_ = static_cast<void*>(target);
                    delegate.callback_ = +[](void* target, Args... args) -> ReturnType
                    {
                        return (static_cast<Class*>(target)->*Callback)(args...);
                    };

                    return delegate;
                }

            private:
                Delegate() = default;

            private:
                using StubFunction = ReturnType (*)(void*, Args...);

                void* target_;
                StubFunction callback_;
            };

        public:
            Event(std::size_t initialSize = 8U) { delegates_.reserve(initialSize); }

            template <auto Callback>
            void Register()
            {
                const auto delegate = Delegate::Create<Callback>();

                ASSERT_MSG(!isRegistered(delegate), "Callback already registered");
                ASSERT_MSG(!protect_, "Callback registration is not allowed during dispatching");

                delegates_.push_back(delegate);
            }

            template <class Class, auto Callback>
            void Register(Class* target)
            {
                const auto delegate = Delegate::Create<Class, Callback>(target);

                ASSERT_MSG(!isRegistered(delegate), "Callback already registered");
                ASSERT_MSG(!protect_, "Callback registration is not allowed during dispatching");

                delegates_.push_back(delegate);
            }

            template <auto Callback>
            void Unregister()
            {
                ASSERT_MSG(!protect_, "Callback unregistration is not allowed during dispatching");

                unregister(Delegate::template Create<Callback>());
            }

            template <class Class, auto Callback>
            void Unregister(Class* target)
            {
                ASSERT_MSG(!protect_, "Callback unregistration is not allowed during dispatching");

                unregister(Delegate::template Create<Class, Callback>(target));
            }

            template <auto Callback>
            bool IsRegistered() const
            {
                return isRegistered(Delegate.template Create<Callback>());
            }

            template <class Class, auto Callback>
            bool IsRegistered() const
            {
                return isRegistered(Delegate.template Create<Class, Callback>());
            }

            void Dispatch(Args... args) const
            {
                protect_ = true;

                for (const auto& delegate : delegates_)
                    delegate(args...);

                protect_ = false;
            }

        private:
            inline bool isRegistered(const Delegate& delegate) const
            {
                return std::find(delegates_.begin(), delegates_.end(), delegate) != delegates_.end();
            }

            inline void unregister(const Delegate& delegate)
            {
                delegates_.erase(
                    std::remove(delegates_.begin(), delegates_.end(), delegate),
                    delegates_.end());
            }

        private:
            std::vector<Delegate> delegates_;
            mutable bool protect_ = false;
        };
    }
}
