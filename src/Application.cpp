#include <memory>

#include "resource_manager/ResourceManager.hpp"
#include "rendering/Render.hpp"
#include "rendering/Primitives.hpp"
#include "rendering/Shader.hpp"
#include "rendering/Mesh.hpp"

#include "windowing/WindowSettings.hpp"
#include "windowing/Windowing.hpp"
#include "windowing/Window.hpp"

#include "Application.hpp"

#include <chrono>

using namespace Common;

int osGetTime() {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

void Application::Start() {
    init();
    loadResouces();

    const auto& render = Rendering::Instance();

    while(!quit) {
        Windowing::Windowing::PoolEvents();
        render->Update();

        float rotation = osGetTime() / 1000000.0f;

        render->SetClearColor(vec4(0.25, 0.25, 0.25, 0));
        render->Clear(true, true);

        float r = 0.4 + PI;
        vec3 eyePos = vec3(sin(r), 0, cos(r) ) * 7;
       // eyePos = vec3(0, 0, -8);
        vec3 targetPos = vec3(0, 0, 0);

        vec2 windowSize = window->GetSize();

        mat4 proj(mat4::PROJ_ZERO_POS, 90, windowSize.x / windowSize.y, 1, 100);
        mat4 viewInv(eyePos, targetPos, vec3(0, -1, 0));
        mat4 view = viewInv.inverseOrtho();
        mat4 viewProj = proj * view;

        mat4 model;
        model.identity();
        //model.translate(vec3(0, 1.0, 0.0));


       // model.rotateX(rotation);
        vec4 material = vec4(sin(rotation), 0, 0, 0);

        shader->Bind();
        shader->SetParam(Rendering::Shader::VIEW_PROJECTION_MATRIX, viewProj, 1);
        shader->SetParam(Rendering::Shader::MODEL_MATRIX, model, 1);
        shader->SetParam(Rendering::Shader::CAMERA_POSITION, eyePos, 1);
        shader->SetParam(Rendering::Shader::MATERIAL, material, 1);

        sphereMesh->Draw();

        render->SwapBuffers();
    }

    terminate();
}

void Application::Quit() {
    quit = true;
}

void Application::init() {
    Windowing::WindowSettings settings;
    Windowing::WindowRect rect(0, 0, 800, 600);

    settings.Title = "OpenDemo";
    settings.WindowRect = rect;

    Windowing::Windowing::Subscribe(this);
    window = Windowing::Windowing::CreateWindow(settings);

    auto& render = Rendering::Instance();
    render->Init(window);

}

void Application::terminate() {
    window.reset();
    window = nullptr;

    Rendering::Instance()->Terminate();
    Windowing::Windowing::UnSubscribe(this);
}

void Application::loadResouces() {
   auto *resourceManager = ResourceManager::Instance().get();
   shader = resourceManager->LoadShader("resources/test.shader");

   sphereMesh = Rendering::Primitives::GetSphereMesh(23);
}


