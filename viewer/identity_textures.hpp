#pragma once

// The graphics half of firm identity: Bitmap in, ImGui texture out. Linked
// into atc_viewer only -- viewer_core stays free of SDL, which is what keeps
// the view model testable without a window.

#include <cstdint>

#include "imgui.h"
#include "model_identity.hpp"

struct SDL_Renderer;

namespace identity_textures {

// Called once, with the renderer the ImGui backend was initialised on.
void init(SDL_Renderer* renderer);

// The mark for (run seed, firm) at `size` pixels, uploaded once and kept.
// Identity never changes for a given seed and index, so the cache is
// fill-once; it is cleared only when the run seed changes.
ImTextureID mark_texture(uint64_t run_seed, uint32_t firm,
                         const identity::Mark& m, int size);

void shutdown();

}  // namespace identity_textures
