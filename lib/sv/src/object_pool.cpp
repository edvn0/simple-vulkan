#pragma once

#include "sv/object_pool.hpp"

#include "sv/context.hpp"
#include "sv/object_handle.hpp"

namespace sv::destruction {
auto
context_destroy(IContext* ctx, TextureHandle handle) -> void
{
  if (ctx)
    ctx->destroy(handle);
}
auto
context_destroy(IContext* ctx, BufferHandle handle) -> void
{
  if (ctx)
    ctx->destroy(handle);
}
auto
context_destroy(IContext* ctx, GraphicsPipelineHandle handle) -> void
{
  if (ctx)
    ctx->destroy(handle);
}
auto
context_destroy(IContext* ctx, ComputePipelineHandle handle) -> void
{
  if (ctx)
    ctx->destroy(handle);
}
auto
context_destroy(IContext* ctx, ShaderModuleHandle handle) -> void
{
  if (ctx)
    ctx->destroy(handle);
}
auto
context_destroy(IContext* ctx, SamplerHandle handle) -> void
{
  if (ctx)
    ctx->destroy(handle);
}
}