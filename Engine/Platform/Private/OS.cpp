#include "OS.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <imgui_impl_sdl3.h>

#include "Assert.hpp"
#include "CommandListPool.hpp"
#include "FramePacer.hpp"
#include "FrameProfiler.hpp"
#include "MainLoop.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "RHIFrameScope.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"
#include "RuntimeConfig.hpp"
#include "SDLInputProvider.hpp"
#include "SDLWindow.hpp"

namespace Crowy
{
    // well under ImGui's KeyRepeatDelay (0.275s),
    // so no single hitch can reach it
    inline constexpr f32 UI_MAX_DELTA_TIME = 0.1f;
    // a --port-input run counts the UI's time as it counts its frames
    inline constexpr f32 UI_PORT_DELTA_TIME = 1.0f / 60.0f;

    // the port is not an SDL event, so this is how late a held loop answers it
    inline constexpr i32 HELD_WAIT_MS = 4;

    OS* OS::singleton = nullptr;

    namespace
    {
        // what a --port-input run keeps from ImGui and the provider: the
        // devices, and the focus and hover that would let ImGui read them
        bool isDeviceEvent(Uint32 type) noexcept {
            switch(type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_TEXT_EDITING:
            case SDL_EVENT_TEXT_INPUT:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_WHEEL:
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_MOUSE_ENTER:
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                return true;
            default:
                return false;
            }
        }

        WindowConfig windowOf(const RuntimeConfig& config) {
            auto window = config.window;
            // nothing the owner does on it takes their keyboard or mouse
            if(config.portInput)
                window.focusable = false;

            return window;
        }
    }

    class SDLInitializer final {
    public:
        SDLInitializer(const RuntimeConfig&);
        ~SDLInitializer() = default;
    };

    SDLInitializer::SDLInitializer(const RuntimeConfig& config) {
        if(!SDL_SetAppMetadata(
               config.name.c_str(),
               config.version.c_str(),
               config.identifier.c_str()
           )) {
            throw std::runtime_error(
                std::format("Couldn't set runtime metadata: {}", SDL_GetError())
            );
        }
    }

    class OS::Impl {
    private:
        SDLInitializer initializer;
        SDLWindow window;
        RHISwapchainRAII swapchain = nullptr;
        u32 width = 0, height = 0;

        SDLInputProvider inputProvider;

        FramePacer framePacer;
        CommandListPool cmdListPool;
        FrameProfiler profiler;

        bool imguiEnabled = false;
        std::optional<u64> exitAfterFrame;
        bool portInput = false;
        std::vector<InputEvent> pushedInput;

    public:
        Impl(const RuntimeConfig&, RHIDevice&);
        ~Impl() = default;

        void Run(MainLoop&, RHIDevice&);

        const InputProvider& GetInputProvider() const noexcept {
            return inputProvider;
        }
        InputStatus ReadInput() const noexcept {
            const auto& gate = inputProvider.Gate();

            return InputStatus{
                .relativeMouse = inputProvider.IsRelativeMouse(),
                .uiHasMouse = gate.mouse,
                .uiHasKeyboard = gate.keyboard
            };
        }
        void PushInput(const InputEvent& input) {
            CROWY_ASSERT(portInput, "pushed input outside a --port-input run");
            pushedInput.push_back(input);
        }

    private:
        // returns when the loop ends by itself; a throw leaves it mid-frame
        void RunLoop(MainLoop&, RHIDevice&);
        bool ProcessEvents(MainLoop&);
        bool PumpHeld(MainLoop&, RHIDevice&);
        void HandleWindowEvent(MainLoop&, const SDL_WindowEvent&);
        void ForwardInput(const SDL_Event&);
        void DeliverPushedInput();
        // a relative look leaves the UI's mouse where the look began
        bool IsLookMotion(const SDL_Event&) const noexcept;
        void ApplyRelativeMouse(bool wanted);

        void BeginFrame(RHIDevice&);
        void EndFrame(RHIDevice&);
    };

    OS::OS(const RuntimeConfig& config, RHIDevice& device)
        : impl(std::make_unique<Impl>(config, device)) {
        singleton = this;
    }

    OS::Impl::Impl(const RuntimeConfig& config, RHIDevice& device)
        : initializer(config),
          window(windowOf(config)),
          inputProvider(!config.portInput),
          framePacer(device),
          cmdListPool(device),
          profiler(config, device),
          exitAfterFrame(config.exitAfterFrame),
          portInput(config.portInput) {
        RHITextureCreateDesc backBufferDesc{
            .width = config.window.width,
            .height = config.window.height,
            .format = config.window.format
        };

        swapchain = device.CreateSwapchain(
            RHISwapchainCreateDesc{
                .sdlWindow = window.Handle(),
                .bufferDesc = backBufferDesc,
                .bufferCount = 3,
                .vsync = config.window.vsync
            },
            std::format("Swapchain for {}", config.window.title)
        );
        for(const auto& capture: config.captures)
            swapchain->RequestFrameDump(capture.path, capture.frame);
    }

    OS::~OS() {
        singleton = nullptr;
    }

    void OS::Run(MainLoop& mainLoop, RHIDevice& device) {
        impl->Run(mainLoop, device);
    }

    void OS::Impl::Run(MainLoop& mainLoop, RHIDevice& device) {
        mainLoop.OnInit(device, *swapchain);

        imguiEnabled = ImGui::GetCurrentContext() != nullptr;
        if(imguiEnabled) {
            ImGui_ImplSDL3_InitForOther(window.Handle());
            // a scripted run neither reads nor writes the owner's layout
            if(portInput)
                ImGui::GetIO().IniFilename = nullptr;
        }

        try {
            RunLoop(mainLoop, device);
        }
        catch(...) {
            // the app is torn down right behind this, and it frees what the
            // GPU may still read and the context the backend is attached to
            framePacer.WaitForIdle();
            if(imguiEnabled)
                ImGui_ImplSDL3_Shutdown();

            throw;
        }

        framePacer.WaitForIdle();
        profiler.CollectGPUTimes();

        if(imguiEnabled) {
            ImGui_ImplSDL3_Shutdown();
        }

        mainLoop.Finalize();

        profiler.WriteReport();
    }

    void OS::Impl::RunLoop(MainLoop& mainLoop, RHIDevice& device) {
        // set by a held iteration, which drops input on the way
        bool held = false;

        // the scopes below are blocks because the loop leaves from the
        // middle of two of them, and a section still has to close
        while(true) {
            mainLoop.NewFrame();

            if(!mainLoop.ShouldAdvance()) {
                if(!PumpHeld(mainLoop, device)) [[unlikely]]
                    return;
                held = true;
                continue;
            }
            // what the hold dropped included releases, so nothing may stay
            // down; a --port-input run's hold drops nothing, and its held
            // keys and buttons are the port's
            if(std::exchange(held, false) && imguiEnabled && !portInput) {
                auto& io = ImGui::GetIO();
                io.ClearInputKeys();
                io.ClearInputMouse();
            }

            const auto frame = framePacer.BeginFrame();
            profiler.BeginFrame(frame, framePacer.GetLastWaitTime());
            mainLoop.OnFrameBegin(frame);

            {
                FrameProfiler::Scope section(profiler, FrameSection::Events);

                if(!ProcessEvents(mainLoop)) [[unlikely]]
                    return;
                mainLoop.ProcessInput(inputProvider);
            }

            {
                FrameProfiler::Scope section(profiler, FrameSection::Update);

                if(!mainLoop.Update()) [[unlikely]]
                    return;
                ApplyRelativeMouse(mainLoop.WantsRelativeMouse());
            }

            {
                FrameProfiler::Scope section(profiler, FrameSection::Acquire);
                BeginFrame(device);
            }

            {
                FrameProfiler::Scope section(profiler, FrameSection::Record);

                if(imguiEnabled) {
                    ImGui_ImplSDL3_NewFrame();
                    // ImGui adds this to hold times, so a hitch past its
                    // KeyRepeatDelay auto-repeats and one click steps twice
                    auto& io = ImGui::GetIO();
                    io.DeltaTime =
                        portInput ? UI_PORT_DELTA_TIME
                                  : std::min(io.DeltaTime, UI_MAX_DELTA_TIME);

                    ImGui::NewFrame();
                }
                mainLoop.Render(cmdListPool, *swapchain);
            }

            {
                FrameProfiler::Scope section(profiler, FrameSection::Submit);
                EndFrame(device);
            }

            mainLoop.OnFrameEnd(profiler.EndFrame(cmdListPool.GetFrameStats()));
            if(profiler.ShouldStop()) [[unlikely]]
                return;
            if(exitAfterFrame.has_value() && frame >= *exitAfterFrame)
                [[unlikely]]
                return;
        }
    }

    bool OS::Impl::ProcessEvents(MainLoop& mainLoop) {
        bool keepRunning = true;

        InputGate gate;
        if(imguiEnabled) {
            const auto& io = ImGui::GetIO();
            // a relative look has the mouse, whatever the UI was hovering
            gate.mouse =
                io.WantCaptureMouse && !inputProvider.IsRelativeMouse();
            gate.keyboard = io.WantCaptureKeyboard;
        }
        inputProvider.NewFrame(gate);

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            // a --port-input run hears only the port
            const bool unheard = portInput && isDeviceEvent(event.type);
            if(imguiEnabled && !unheard && !IsLookMotion(event))
                ImGui_ImplSDL3_ProcessEvent(&event);

            if(SDL_EVENT_QUIT == event.type) [[unlikely]] {
                keepRunning = false;
                break;
            }

            if(!unheard)
                ForwardInput(event);

            if(!keepRunning) [[unlikely]]
                break;

            if(SDL_EVENT_WINDOW_FIRST <= event.type &&
               event.type <= SDL_EVENT_WINDOW_LAST)
                HandleWindowEvent(mainLoop, event.window);
        }
        if(keepRunning)
            DeliverPushedInput();

        return keepRunning;
    }

    bool OS::Impl::PumpHeld(MainLoop& mainLoop, RHIDevice& device) {
        // no frame's pool is open between frames, and a resize allocates
        const auto scope = device.CreateFrameScope();
        // a held loop gives the cursor back; a --port-input run holds none,
        // and its look goes on across the steps that hold it between frames
        if(!portInput)
            ApplyRelativeMouse(false);

        SDL_Event event;
        for(bool have = SDL_WaitEventTimeout(&event, HELD_WAIT_MS); have;
            have = SDL_PollEvent(&event)) {
            if(SDL_EVENT_QUIT == event.type) [[unlikely]]
                return false;

            // so a key let go during the hold is not held after it; the next
            // frame's NewFrame drops the edges and the motion
            if(!(portInput && isDeviceEvent(event.type)))
                ForwardInput(event);

            if(SDL_EVENT_WINDOW_FIRST <= event.type &&
               event.type <= SDL_EVENT_WINDOW_LAST)
                HandleWindowEvent(mainLoop, event.window);
        }

        return true;
    }

    void OS::Impl::HandleWindowEvent(
        MainLoop& mainLoop,
        const SDL_WindowEvent& event
    ) {
        if(event.type == SDL_EVENT_WINDOW_RESIZED) {
            int w = event.data1;
            int h = event.data2;

            framePacer.WaitForIdle();

            swapchain->Resize(w, h);
            mainLoop.OnResize(w, h);
        }
        if(event.type == SDL_EVENT_WINDOW_FOCUS_LOST && !portInput)
            inputProvider.OnFocusLost(window.Handle());
    }

    void OS::Impl::ForwardInput(const SDL_Event& event) {
        switch(event.type) {
        case SDL_EVENT_KEY_DOWN:
            [[fallthrough]];
        case SDL_EVENT_KEY_UP:
            inputProvider.OnPlatformEvent(event.key);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            inputProvider.OnPlatformEvent(event.motion);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            [[fallthrough]];
        case SDL_EVENT_MOUSE_BUTTON_UP:
            inputProvider.OnPlatformEvent(event.button);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            inputProvider.OnPlatformEvent(event.wheel);
            break;
        }
    }

    void OS::Impl::ApplyRelativeMouse(bool wanted) {
        auto sdlWindow = window.Handle();

        // SDL turns it back on for a window that regains focus, so only a
        // focused window holds it; a --port-input run's needs no window
        const bool focused = portInput || SDL_GetKeyboardFocus() == sdlWindow;
        inputProvider.SetRelativeMouse(sdlWindow, wanted && focused);
    }

    void OS::Impl::DeliverPushedInput() {
        const auto windowID = SDL_GetWindowID(window.Handle());

        // one at a time: each starts from where the one before left the mouse
        for(const auto& input: pushedInput) {
            const auto event = inputProvider.Synthesize(input, windowID);
            if(imguiEnabled && !IsLookMotion(event))
                ImGui_ImplSDL3_ProcessEvent(&event);
            ForwardInput(event);
        }
        pushedInput.clear();
    }

    bool OS::Impl::IsLookMotion(const SDL_Event& event) const noexcept {
        return event.type == SDL_EVENT_MOUSE_MOTION &&
               inputProvider.IsRelativeMouse();
    }

    void OS::Impl::BeginFrame(RHIDevice& device) {
        if(swapchain != nullptr) [[unlikely]]
            swapchain->AcquireNextImage();

        cmdListPool.BeginFrame(framePacer.CurrentFrame());
    }

    void OS::Impl::EndFrame(RHIDevice& device) {
        auto cmdLists = cmdListPool.ExtractRecorded();

        framePacer.EndFrame(cmdLists, *swapchain.get());
    }

    const InputProvider& OS::GetInputProvider() const noexcept {
        return impl->GetInputProvider();
    }

    InputStatus OS::ReadInput() const noexcept {
        return impl->ReadInput();
    }

    void OS::PushInput(const InputEvent& input) {
        impl->PushInput(input);
    }
}
