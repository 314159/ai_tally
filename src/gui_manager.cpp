#include "gui_manager.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#if defined(__APPLE__)
#include "imgui_impl_metal.h"
#include <SDL_metal.h>
#else
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>
#endif
#include <iostream>

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#endif

namespace atem {

GuiManager::GuiManager()
{
    for (uint16_t i = 1; i <= 8; ++i) {
        tally_states_[i] = TallyState { i, false, false, {} };
    }
}

GuiManager::~GuiManager()
{
    stop();
#if defined(__APPLE__)
    // Release bridged Metal objects that we retained in init()
    if (metal_command_queue_) {
        CFBridgingRelease((CFTypeRef)metal_command_queue_);
        metal_command_queue_ = nullptr;
    }
    if (metal_device_) {
        CFBridgingRelease((CFTypeRef)metal_device_);
        metal_device_ = nullptr;
    }
#endif
#if defined(__APPLE__)
    ImGui_ImplMetal_Shutdown();
#else
    ImGui_ImplOpenGL3_Shutdown();
#endif
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

#if !defined(__APPLE__)
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
    }
#endif

    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool GuiManager::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "Error: " << SDL_GetError() << std::endl;
        return false;
    }

#if defined(__APPLE__)
    // Create a window suitable for Metal
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_METAL);
    window_ = SDL_CreateWindow("ATEM Tally Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 200, window_flags);
    std::cerr << "[GuiManager] window_ = " << window_ << std::endl;

    // Setup Metal
    // Create the Metal device and take ownership (retain)
    metal_device_ = (void*)CFBridgingRetain(MTLCreateSystemDefaultDevice());
    if (!metal_device_) {
        std::cerr << "Error: Could not create Metal view." << std::endl;
        return false;
    }
    std::cerr << "[GuiManager] metal_device_ = " << metal_device_ << std::endl;
    // Create a command queue and retain it as well
    metal_command_queue_ = (void*)CFBridgingRetain([(__bridge id<MTLDevice>)metal_device_ newCommandQueue]);
    std::cerr << "[GuiManager] metal_command_queue_ = " << metal_command_queue_ << std::endl;

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForMetal(window_);
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)metal_device_);

#else
    // --- OpenGL Path (for non-Apple platforms) ---
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window_ = SDL_CreateWindow("ATEM Tally Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 200, window_flags);
    gl_context_ = SDL_GL_CreateContext(window_);
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return false;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif

    running_ = true;
    return true;
}

void GuiManager::run_loop()
{
    while (running_) {
        process_events();
        render();
    }
}

void GuiManager::stop()
{
    running_ = false;
}

void GuiManager::update_tally_state(const TallyUpdate& update)
{
    std::ostringstream oss;
    oss << "[GuiManager] update_tally_state called on thread " << std::this_thread::get_id() << " for input " << update.input_id << std::endl;
    std::cout << oss.str();
    if (update.input_id > 0 && update.input_id <= 8) {
        std::lock_guard<std::mutex> lock(tally_states_mutex_);
        tally_states_[update.input_id].program = update.program;
        tally_states_[update.input_id].preview = update.preview;
    }
}

void GuiManager::process_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            stop();
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window_)) {
            stop();
        }
    }
}

void GuiManager::render()
{
    std::ostringstream _render_log;
    _render_log << "[GuiManager] render() start on thread " << std::this_thread::get_id() << std::endl;
    std::cout << _render_log.str();
#if defined(__APPLE__)
    @autoreleasepool {
        std::cerr << "[GuiManager] entered autoreleasepool" << std::endl;
        if (!window_) {
            std::cerr << "[GuiManager] render: window_ is null, skipping render." << std::endl;
            return;
        }

        CAMetalLayer* layer = static_cast<CAMetalLayer*>(SDL_Metal_GetLayer(window_));
        std::cerr << "[GuiManager] CAMetalLayer* = " << layer << std::endl;
        if (!layer) {
            std::cerr << "[GuiManager] render: CAMetalLayer is null, skipping render." << std::endl;
            return;
        }

        id<CAMetalDrawable> drawable = [layer nextDrawable];
        std::cerr << "[GuiManager] drawable = " << drawable << std::endl;
        if (!drawable) {
            std::cerr << "[GuiManager] render: drawable is null, skipping render." << std::endl;
            return;
        }

        MTLRenderPassDescriptor* render_pass_descriptor = [MTLRenderPassDescriptor new];
        render_pass_descriptor.colorAttachments[0].texture = drawable.texture;
        render_pass_descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        render_pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);
        ImGui_ImplMetal_NewFrame(render_pass_descriptor);
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Create a window that fills the whole viewport
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowBgAlpha(0.0f); // Transparent background

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

        // Use a temporary bool for ImGui, then update the atomic flag.
        bool gui_is_running = running_.load();
        ImGui::Begin("Tally Display", &gui_is_running, window_flags);
        if (!gui_is_running) {
            stop();
        }
        // Draw tally boxes
        float window_width = ImGui::GetContentRegionAvail().x;
        float box_size = (window_width - (7 * 10.0f)) / 8.0f; // 8 boxes with 10px spacing

        std::lock_guard<std::mutex> lock(tally_states_mutex_);
        for (uint16_t i = 1; i <= 8; ++i) {
            ImVec4 color(0.2f, 0.2f, 0.2f, 1.0f); // Off
            if (tally_states_[i].program) {
                color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Program (Red)
            } else if (tally_states_[i].preview) {
                color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Preview (Green)
            }

            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::Button(std::to_string(i).c_str(), ImVec2(box_size, box_size));
            ImGui::PopStyleColor(1);

            if (i < 8) {
                ImGui::SameLine();
            }
        }

        ImGui::End();

        // Rendering
        ImGui::Render();

#if defined(__APPLE__)
    std::cerr << "[GuiManager] metal_command_queue_ (before cmd buffer) = " << metal_command_queue_ << std::endl;
    id<MTLCommandBuffer> command_buffer = [(__bridge id<MTLCommandQueue>)metal_command_queue_ commandBuffer];
    std::cerr << "[GuiManager] command_buffer = " << command_buffer << std::endl;

    id<MTLRenderCommandEncoder> render_encoder = [command_buffer renderCommandEncoderWithDescriptor:render_pass_descriptor];
    std::cerr << "[GuiManager] render_encoder = " << render_encoder << std::endl;
        ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), command_buffer, render_encoder);
        [render_encoder endEncoding];

        [command_buffer presentDrawable:drawable];
        [command_buffer commit];
    }
#else
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
#endif
}

} // namespace atem
