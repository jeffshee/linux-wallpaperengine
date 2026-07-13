#include "ExternalOutput.h"

#include "WallpaperEngine/Render/Drivers/VideoDriver.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"

using namespace WallpaperEngine::Embed;

ExternalOutput::ExternalOutput (ApplicationContext& context, VideoDriver& driver, const bool vflip) :
    Output (context, driver), m_vflip (vflip) {
    this->m_fullWidth = driver.getFramebufferSize ().x;
    this->m_fullHeight = driver.getFramebufferSize ().y;

    this->m_viewports[DEFAULT_WINDOW_NAME] = new ExternalOutputViewport {
	{ 0, 0, this->m_fullWidth, this->m_fullHeight }, DEFAULT_WINDOW_NAME
    };
}

ExternalOutput::~ExternalOutput () {
    for (const auto& [name, viewport] : this->m_viewports) {
	delete viewport;
    }
}

void ExternalOutput::updateRender () const {
    // follow host framebuffer resizes, same as GLFWWindowOutput does for
    // WM-initiated window resizes
    this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
    this->m_fullHeight = this->m_driver.getFramebufferSize ().y;

    this->m_viewports[DEFAULT_WINDOW_NAME]->viewport = { 0, 0, this->m_fullWidth, this->m_fullHeight };
}
