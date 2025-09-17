#include "sv/common.hpp"

#include "sv/context.hpp"

namespace sv {

auto
set_name(const IContext& context,
         const std::uint64_t object,
         const VkObjectType type,
         const std::string_view name) -> void
{
  VkDebugUtilsObjectNameInfoEXT name_info{};
  name_info.objectHandle = object;
  name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  name_info.objectType = type;
  name_info.pObjectName = name.data();
  if (auto ctx = dynamic_cast<const VulkanContext*>(&context)) {
    ctx->dispatch<VKB_MEMBER(vkSetDebugUtilsObjectNameEXT)>(ctx->get_device(),
                                                            &name_info);
  }
}

}