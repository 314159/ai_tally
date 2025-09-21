#pragma once

#include "atem/tally_state.h"
#include <SDL.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

// Forward-declare Metal types for macOS
#if defined(__APPLE__)
typedef void* id; // C++ compatible forward declaration for Objective-C objects
#endif

namespace atem {

class GuiManager {
public:
    GuiManager();
    ~GuiManager();

    // Non-copyable, non-movable
    GuiManager(const GuiManager&) = delete;
    GuiManager& operator=(const GuiManager&) = delete;
    GuiManager(GuiManager&&) = delete;
    GuiManager& operator=(GuiManager&&) = delete;

    bool init();
    void run_loop();
    void stop();

    void update_tally_state(const TallyUpdate& update);

private:
    void process_events();
    void render();

    SDL_Window* window_ = nullptr;
#if defined(__APPLE__)
    void* metal_device_ = nullptr;
    void* metal_command_queue_ = nullptr;
#else
    SDL_GLContext gl_context_ = nullptr; // Keep for non-Apple platforms
#endif
    std::atomic<bool> running_ { false };

    std::mutex tally_states_mutex_;
    std::unordered_map<uint16_t, TallyState> tally_states_;
};

} // namespace atem