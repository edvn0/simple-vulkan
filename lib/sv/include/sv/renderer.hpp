#pragma once

#include "object_handle.hpp"
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
  Holder<ShaderModuleHandle> basic_shader;

public:
  Renderer(IContext&);
  auto record(ICommandBuffer&, TextureHandle) -> void;
};

}