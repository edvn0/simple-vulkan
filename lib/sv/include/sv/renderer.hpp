#pragma once

#include <vulkan/vulkan.h>

namespace sv {

struct IContext;
struct AcquiredFrame;

class Renderer
{
private:
  IContext* context{ nullptr };

public:
  Renderer(IContext&);

  auto record(const AcquiredFrame& af, VkCommandBuffer cmd) -> void;
};

}