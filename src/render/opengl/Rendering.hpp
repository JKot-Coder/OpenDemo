#pragma once

#include "render/Rendering.hpp"

typedef void *SDL_GLContext;

namespace Render {
namespace OpenGL {

    class Rendering : public Render::Rendering {
    public:
        Rendering();

        virtual void Init(const std::shared_ptr<Windowing::Window>& window) override;
        virtual void Terminate() override;

        virtual std::shared_ptr<Render::Shader> CreateShader() override;
    private:
        SDL_GLContext context;
    };

}
}