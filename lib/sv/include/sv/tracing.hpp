#if defined(TRACY_ENABLE)
#include "tracy/TracyVulkan.hpp"
#define PROFILER_GPU_ZONE(name, context, cmd, color)                           \
  TracyVkZoneC(context->tracing_impl->vulkan_context, cmd, name, color);
#else
#define PROFILER_GPU_ZONE(name, context, cmd, color)
#endif // TRACY_ENABLE

namespace sv {

}