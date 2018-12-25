#include "common/Stream.hpp"
#include "common/Utils.hpp"

#include "filesystem/FileSystem.hpp"

#include "rendering/Render.hpp"
#include "rendering/Shader.hpp"

#include "ResourceManager.hpp"

namespace ResourceManager {

    std::unique_ptr<ResourceManager> ResourceManager::instance = std::unique_ptr<ResourceManager>(new ResourceManager());

    const std::shared_ptr<Rendering::Shader> ResourceManager::LoadShader(const std::string& filename) {
        auto *filesystem = FileSystem::Instance().get();

        std::shared_ptr<Common::Stream> stream;
        try {
            stream = filesystem->Open(filename, FileSystem::Mode::READ);
        } catch(const std::exception &exception) {
            LOG("Error opening resource \"%s\" with error: %s", filename.c_str(), exception.what());
        }

        auto *render = Rendering::Instance().get();
        auto shader = render->CreateShader();
        shader->LinkSource(stream.get());

        return shader;
    }
}
