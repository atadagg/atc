#include "identity_textures.hpp"

#include <SDL3/SDL.h>

#include <map>
#include <utility>

namespace identity_textures {
namespace {

SDL_Renderer* g_renderer = nullptr;
uint64_t g_seed = 0;
bool g_seed_set = false;

// Keyed by (firm, size): a row wants a small mark and the detail panel a
// large one, and they are different rasterizations of the same mark.
std::map<std::pair<uint32_t, int>, SDL_Texture*> g_cache;

ImTextureID to_id(SDL_Texture* t) {
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(t));
}

void clear() {
    for (auto& [key, tex] : g_cache) SDL_DestroyTexture(tex);
    g_cache.clear();
}

}  // namespace

void init(SDL_Renderer* renderer) { g_renderer = renderer; }

ImTextureID mark_texture(uint64_t run_seed, uint32_t firm, const identity::Mark& m,
                         int size) {
    if (g_renderer == nullptr || size <= 0) return ImTextureID{};
    if (!g_seed_set || run_seed != g_seed) {
        clear();
        g_seed = run_seed;
        g_seed_set = true;
    }

    const auto key = std::make_pair(firm, size);
    if (const auto it = g_cache.find(key); it != g_cache.end()) {
        return to_id(it->second);
    }

    const Bitmap b = rasterize(m, size);
    SDL_Texture* tex = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, size, size);
    if (tex == nullptr) return ImTextureID{};
    SDL_UpdateTexture(tex, nullptr, b.px.data(), size * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    // Nearest: the marks are flat ordered fills, and smoothing a 16px emblem
    // is what makes a generated logo look like a smudge.
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    g_cache[key] = tex;
    return to_id(tex);
}

void shutdown() {
    clear();
    g_renderer = nullptr;
    g_seed_set = false;
}

}  // namespace identity_textures
