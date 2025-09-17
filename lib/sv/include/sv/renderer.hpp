#pragma once

#include "sv/abstract_context.hpp"
#include "sv/object_holder.hpp"

#include <vulkan/vulkan.h>

namespace sv {

struct IContext;

class Renderer
{
private:
  IContext* context{ nullptr };

  Holder<GraphicsPipelineHandle> basic;

public:
  Renderer(IContext&);
   auto record(ICommandBuffer&, TextureHandle)->void;
};

}