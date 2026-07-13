#pragma once

#include "ExternalMouseInput.h"
#include "ExternalOutput.h"
#include "wpe_embed.h"

#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

#include <memory>

namespace WallpaperEngine::Embed {
using namespace WallpaperEngine::Render::Drivers;

/**
 * Video driver for embedding: the host owns the GL context, frame clock and
 * destination framebuffer. The driver never creates a window, never swaps
 * buffers and never sleeps; dispatchEventQueue renders exactly one frame
 * into the application's destination framebuffer.
 *
 * The host must have its GL context current before construction and before
 * every dispatchEventQueue call.
 */
class ExternalOpenGLDriver final : public VideoDriver {
public:
    ExternalOpenGLDriver (
	ApplicationContext& context, WallpaperApplication& app, wpe_get_proc_address_fn getProcAddress,
	void* userdata, glm::ivec2 size, bool vflip
    );

    Output::Output& getOutput () override;
    float getRenderTime () const override;
    bool closeRequested () override;
    void resizeWindow (glm::ivec2 size) override;
    void resizeWindow (glm::ivec4 positionAndSize) override;
    void showWindow () override;
    void hideWindow () override;
    glm::ivec2 getFramebufferSize () const override;
    uint32_t getFrameCounter () const override;
    void* getProcAddress (const char* name) const override;
    void dispatchEventQueue () override;

    /** Host-driven state, fed once per frame before dispatchEventQueue. */
    void setFramebufferSize (glm::ivec2 size);
    void setRenderTime (float time);
    ExternalMouseInput& getMouseInput ();

private:
    wpe_get_proc_address_fn m_getProcAddress;
    void* m_userdata;
    ExternalMouseInput m_mouseInput {};
    std::unique_ptr<ExternalOutput> m_output = nullptr;
    glm::ivec2 m_framebufferSize;
    float m_renderTime = 0.0f;
    uint32_t m_frameCounter = 0;
};
} // namespace WallpaperEngine::Embed
