#include "sv/app.hpp"
#include "sv/context.hpp"
#include "sv/renderer.hpp"

#include <ranges>

int
main(int argc, char** argv)
{
  auto views = std::views::iota(0, argc) |
               std::views::transform([args = argv](const auto i) {
                 return std::string_view(args[i]);
               }) |
               std::ranges::to<std::vector<std::string_view>>();

  using namespace sv;
  PresentMode mode{ PresentMode::FIFO };
  if (auto it = std::ranges::find(views, "mode");
      it != std::ranges::end(views)) {
    auto next = std::ranges::next(it);

    if (next != std::ranges::end(views)) {
      auto as_string = std::string{ *next };
      mode = as_string == "Mailbox" ? PresentMode::Mailbox : PresentMode::FIFO;
    }
  }

  auto maybe_app = App::create({
    .mode = mode,
  });
  if (!maybe_app)
    return 1;
  auto app = std::move(maybe_app.value());

  auto maybe_ctx = VulkanContext::create(app.get_window());
  if (!maybe_ctx)
    return 1;
  auto context = std::move(maybe_ctx.value());

  if (!app.attach_context(*context))
    return 1;

  Renderer renderer{ *context };

  while (!app.should_close()) {
    app.poll_events();

    auto af = app.acquire_frame();
    if (!af)
      continue;

    renderer.record(*af);

    app.submit_frame(*af);
  }

  app.detach_context();
  return 0;
}
