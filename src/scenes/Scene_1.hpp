#pragma once

#include <vector>
#include <memory>

#include "rendering/SceneGraph.hpp"

namespace Rendering {
    class RenderContext;
    class Mesh;
    struct RenderElement;
}

namespace Scenes {

    class Scene_1 final : public Rendering::SceneGraph {
    public:
        virtual void Init() override;
        virtual void Terminate() override;

        virtual void Collect(Rendering::RenderContext& renderContext) override;
    private:
        std::shared_ptr<Rendering::Mesh> sphereMesh;
        std::vector<Rendering::RenderElement> renderElements;
    };
}


