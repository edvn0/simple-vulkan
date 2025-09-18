#include "GLFW/glfw3.h"
#include "sv/app.hpp"
#include "sv/context.hpp"
#include "sv/renderer.hpp"

#include <ranges>

namespace {
auto
parse_mode(const std::string_view val)
{
  if (val == "fifo" || val == "FIFO")
    return sv::PresentMode::FIFO;
  if (val == "mailbox" || val == "MAILBOX" || val == "mm")
    return sv::PresentMode::Mailbox;

  return sv::PresentMode::Mailbox;
}
}

auto
run(std::span<const std::string_view> args)
{
  using namespace sv;
  PresentMode mode{ PresentMode::FIFO };
  if (auto it = std::ranges::find(args, "mode"); it != std::ranges::end(args)) {
    auto next = std::ranges::next(it);

    if (next != std::ranges::end(args)) {
      mode = parse_mode(*next);
    }
  }

  auto maybe_app = App::create({
    .mode = mode,
  });
  if (!maybe_app)
    return 1;
  auto app = std::move(maybe_app.value());

  auto maybe_ctx = VulkanContext::create(app.get_window(),
                                         {
                                           .abort_on_validation_error = false,
                                         });
  if (!maybe_ctx)
    return 1;
  auto context = std::move(maybe_ctx.value());

  if (!app.attach_context(*context))
    return 1;

  Renderer renderer{ *context };

  while (!app.should_close()) {
    context->recreate_swapchain(app.get_window().width,
                                app.get_window().height);
    app.poll_events();

    auto& cmd = context->acquire_command_buffer();

    renderer.record(cmd, context->get_current_swapchain_texture());
    context->submit(cmd, context->get_current_swapchain_texture());
  }

  app.detach_context();
  return 0;
}

int
main(int argc, char** argv)
{
  auto views = std::views::iota(0, argc) |
               std::views::transform([args = argv](const auto i) {
                 return std::string_view(args[i]);
               }) |
               std::ranges::to<std::vector<std::string_view>>();
  auto done = run(views);

  return done;
}
