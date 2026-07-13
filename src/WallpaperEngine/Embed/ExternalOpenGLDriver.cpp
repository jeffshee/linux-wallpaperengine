#include "ExternalOpenGLDriver.h"

#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#include <GL/glew.h>

using namespace WallpaperEngine::Embed;

namespace {
/**
 * Saves/restores the GL state the engine is known to touch. The engine was
 * written assuming it owns the context; when embedded, the host's renderer
 * (e.g. GTK's) runs on the same context right after us and must not see our
 * leftovers — stale blend/scissor/program state can break or even hang the
 * host compositor's rendering on some drivers.
 */
class ScopedGLState {
public:
    ScopedGLState () {
	glGetIntegerv (GL_CURRENT_PROGRAM, &m_program);
	glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &m_vertexArray);
	glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &m_arrayBuffer);
	glGetIntegerv (GL_ACTIVE_TEXTURE, &m_activeTexture);
	glGetIntegerv (GL_TEXTURE_BINDING_2D, &m_texture2D);
	glGetIntegerv (GL_FRAMEBUFFER_BINDING, &m_framebuffer);
	glGetIntegerv (GL_VIEWPORT, m_viewport);
	glGetIntegerv (GL_BLEND_SRC_RGB, &m_blendSrcRgb);
	glGetIntegerv (GL_BLEND_DST_RGB, &m_blendDstRgb);
	glGetIntegerv (GL_BLEND_SRC_ALPHA, &m_blendSrcAlpha);
	glGetIntegerv (GL_BLEND_DST_ALPHA, &m_blendDstAlpha);
	glGetIntegerv (GL_BLEND_EQUATION_RGB, &m_blendEquationRgb);
	glGetIntegerv (GL_BLEND_EQUATION_ALPHA, &m_blendEquationAlpha);
	glGetBooleanv (GL_COLOR_WRITEMASK, m_colorMask);
	m_blend = glIsEnabled (GL_BLEND);
	m_cullFace = glIsEnabled (GL_CULL_FACE);
	m_depthTest = glIsEnabled (GL_DEPTH_TEST);
	m_scissorTest = glIsEnabled (GL_SCISSOR_TEST);
	glGetBooleanv (GL_DEPTH_WRITEMASK, &m_depthMask);
    }

    ~ScopedGLState () {
	glUseProgram (m_program);
	glBindVertexArray (m_vertexArray);
	glBindBuffer (GL_ARRAY_BUFFER, m_arrayBuffer);
	glActiveTexture (m_activeTexture);
	glBindTexture (GL_TEXTURE_2D, m_texture2D);
	glBindFramebuffer (GL_FRAMEBUFFER, m_framebuffer);
	glViewport (m_viewport [0], m_viewport [1], m_viewport [2], m_viewport [3]);
	glBlendFuncSeparate (m_blendSrcRgb, m_blendDstRgb, m_blendSrcAlpha, m_blendDstAlpha);
	glBlendEquationSeparate (m_blendEquationRgb, m_blendEquationAlpha);
	glColorMask (m_colorMask [0], m_colorMask [1], m_colorMask [2], m_colorMask [3]);
	setEnabled (GL_BLEND, m_blend);
	setEnabled (GL_CULL_FACE, m_cullFace);
	setEnabled (GL_DEPTH_TEST, m_depthTest);
	setEnabled (GL_SCISSOR_TEST, m_scissorTest);
	glDepthMask (m_depthMask);
    }

private:
    static void setEnabled (const GLenum capability, const GLboolean enabled) {
	if (enabled) {
	    glEnable (capability);
	} else {
	    glDisable (capability);
	}
    }

    GLint m_program = 0;
    GLint m_vertexArray = 0;
    GLint m_arrayBuffer = 0;
    GLint m_activeTexture = GL_TEXTURE0;
    GLint m_texture2D = 0;
    GLint m_framebuffer = 0;
    GLint m_viewport [4] = {};
    GLint m_blendSrcRgb = GL_ONE;
    GLint m_blendDstRgb = GL_ZERO;
    GLint m_blendSrcAlpha = GL_ONE;
    GLint m_blendDstAlpha = GL_ZERO;
    GLint m_blendEquationRgb = GL_FUNC_ADD;
    GLint m_blendEquationAlpha = GL_FUNC_ADD;
    GLboolean m_colorMask [4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    GLboolean m_blend = GL_FALSE;
    GLboolean m_cullFace = GL_FALSE;
    GLboolean m_depthTest = GL_FALSE;
    GLboolean m_scissorTest = GL_FALSE;
    GLboolean m_depthMask = GL_TRUE;
};
} // namespace

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
    // restore every piece of host GL state we touch once the frame is done
    const ScopedGLState state;

    const GLuint destination = this->getApp ().getDestinationFramebuffer ();

    glBindFramebuffer (GL_FRAMEBUFFER, destination);
    glDisable (GL_SCISSOR_TEST);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& [screen, viewport] : this->m_output->getViewports ()) {
	this->getApp ().update (viewport);
    }

    this->m_output->updateRender ();
    this->m_frameCounter++;
}

void ExternalOpenGLDriver::setFramebufferSize (const glm::ivec2 size) { this->m_framebufferSize = size; }

void ExternalOpenGLDriver::setRenderTime (const float time) { this->m_renderTime = time; }

ExternalMouseInput& ExternalOpenGLDriver::getMouseInput () { return this->m_mouseInput; }
