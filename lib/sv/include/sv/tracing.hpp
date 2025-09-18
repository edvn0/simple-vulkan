#if defined(HAS_TRACY_TRACING)
#include "tracy/TracyVulkan.hpp"
#define PROFILER_GPU_ZONE(name, context, cmd, color)                           \
  TracyVkZoneC(context->tracing_impl->vulkan_context, cmd, name, color);
#else
#define PROFILER_GPU_ZONE(name, context, cmd, color)
#endif // HAS_TRACY_TRACING

namespace sv {

}