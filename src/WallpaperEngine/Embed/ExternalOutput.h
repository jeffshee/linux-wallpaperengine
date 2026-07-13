#pragma once

#include "WallpaperEngine/Render/Drivers/Output/Output.h"
#include "WallpaperEngine/Render/Drivers/Output/OutputViewport.h"

namespace WallpaperEngine::Embed {
using namespace WallpaperEngine::Render::Drivers;

/**
 * Viewport for host-managed framebuffers: the host binds the target before
 * rendering, so activating/presenting are no-ops.
 */
class ExternalOutputViewport final : public Output::OutputViewport {
public:
    ExternalOutputViewport (const glm::ivec4 viewport, std::string name) :
	OutputViewport (viewport, std::move (name)) { }

    void makeCurrent () override { }
    void swapOutput () override { }
};

/**
 * Single-viewport output whose size follows the host framebuffer
 * (reported through the driver) instead of a window system.
 */
class ExternalOutput final : public Output::Output {
public:
    ExternalOutput (ApplicationContext& context, VideoDriver& driver, bool vflip);
    ~ExternalOutput () override;

    void reset () override { }
    bool renderVFlip () const override { return this->m_vflip; }
    bool renderMultiple () const override { return false; }
    bool haveImageBuffer () const override { return false; }
    void* getImageBuffer () const override { return nullptr; }
    uint32_t getImageBufferSize () const override { return 0; }
    void updateRender () const override;

private:
    bool m_vflip;
};
} // namespace WallpaperEngine::Embed
