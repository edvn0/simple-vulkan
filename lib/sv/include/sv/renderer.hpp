#pragma once

#include "sv/context.hpp"
#include "sv/object_holder.hpp"

#include <vulkan/vulkan.h>

namespace sv {

struct IContext;
struct AcquiredFrame;

class Renderer
{
private:
  IContext* context{ nullptr };

  Holder<GraphicsPipelineHandle> basic;

public:
  Renderer(IContext&);
  auto record(const AcquiredFrame&, VkCommandBuffer) -> void;
};

}