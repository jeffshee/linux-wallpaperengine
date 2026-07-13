#include "ExternalOpenGLDriver.h"

#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#include <GL/glew.h>

using namespace WallpaperEngine::Embed;

ExternalOpenGLDriver::ExternalOpenGLDriver (
    ApplicationContext& context, WallpaperApplication& app, const wpe_get_proc_address_fn getProcAddress,
    void* userdata, const glm::ivec2 size, const bool vflip
) :
    VideoDriver (app, m_mouseInput), m_getProcAddress (getProcAddress), m_userdata (userdata),
    m_framebufferSize (size) {
    // the host context is expected to be current here
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit ();

    // GLEW_ERROR_NO_GLX_DISPLAY is expected on EGL-only setups (e.g. Wayland);
    // core GL entry points are still resolved through GLVND in that case
#ifdef GLEW_ERROR_NO_GLX_DISPLAY
    if (result != GLEW_OK && result != GLEW_ERROR_NO_GLX_DISPLAY) {
#else
    if (result != GLEW_OK) {
#endif
	sLog.exception ("Failed to initialize GLEW: ", glewGetErrorString (result));
    }

    this->m_output = std::make_unique<ExternalOutput> (context, *this, vflip);
}

Output::Output& ExternalOpenGLDriver::getOutput () { return *this->m_output; }

float ExternalOpenGLDriver::getRenderTime () const { return this->m_renderTime; }

bool ExternalOpenGLDriver::closeRequested () { return false; }

void ExternalOpenGLDriver::resizeWindow (glm::ivec2 size) { }
void ExternalOpenGLDriver::resizeWindow (glm::ivec4 positionAndSize) { }
void ExternalOpenGLDriver::showWindow () { }
void ExternalOpenGLDriver::hideWindow () { }

glm::ivec2 ExternalOpenGLDriver::getFramebufferSize () const { return this->m_framebufferSize; }

uint32_t ExternalOpenGLDriver::getFrameCounter () const { return this->m_frameCounter; }

void* ExternalOpenGLDriver::getProcAddress (const char* name) const {
    return this->m_getProcAddress (this->m_userdata, name);
}

void ExternalOpenGLDriver::dispatchEventQueue () {
    const GLuint destination = this->getApp ().getDestinationFramebuffer ();

    glBindFramebuffer (GL_FRAMEBUFFER, destination);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);
    }

    this->m_output->updateRender ();
    this->m_frameCounter++;

    // leave the destination framebuffer bound for the host
    glBindFramebuffer (GL_FRAMEBUFFER, destination);
}

void ExternalOpenGLDriver::setFramebufferSize (const glm::ivec2 size) { this->m_framebufferSize = size; }

void ExternalOpenGLDriver::setRenderTime (const float time) { this->m_renderTime = time; }

ExternalMouseInput& ExternalOpenGLDriver::getMouseInput () { return this->m_mouseInput; }
