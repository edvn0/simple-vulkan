// simple-vulkan.cpp : Defines the entry point for the application.
//

#include "simple-vulkan.h"

#include "sv/app.hpp"
#include "sv/renderer.hpp"
#include "sv/context.hpp"

using namespace std;

int main() {
    using namespace sv;
    auto maybe_app = App::create({});
    if (!maybe_app) return 1;
    auto app = std::move(maybe_app.value());

    auto maybe_ctx = VulkanContext::create(app.get_window());
    if (!maybe_ctx) return 1;
    auto context = std::move(maybe_ctx.value());

    if (!app.attach_context(*context)) return 1;

    Renderer renderer{ *context };

    while (!app.should_close()) {
        app.poll_events();

        auto af = app.acquire_frame();
        if (!af) continue;

        auto cmd = app.command_buffer_for_frame();
        renderer.record(*af, cmd);

        app.submit_frame(*af);
    }

    app.detach_context();
    return 0;
}
