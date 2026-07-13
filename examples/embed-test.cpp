/**
 * Standalone exerciser for the wpe_embed.h C API.
 *
 * Simulates an embedding host: creates its own (hidden) GL context via GLFW,
 * renders a scene into a private FBO through the embed API only, and dumps
 * frames as PNG for inspection.
 *
 * Usage: wpe-embed-test <assets-dir> <background-dir> <output-dir>
 */

#include "WallpaperEngine/Embed/wpe_embed.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;
constexpr int TOTAL_FRAMES = 120;
constexpr double FRAME_STEP = 1.0 / 60.0;

void* getProcAddress (void* userdata, const char* name) {
    return reinterpret_cast<void*> (glfwGetProcAddress (name));
}

bool savePng (const std::string& path, const std::vector<unsigned char>& rgba) {
    // with vflip=0 the engine writes rows top-down into the FBO (the
    // orientation GTK expects when sampling a GLArea framebuffer), so the
    // glReadPixels buffer is already in PNG row order
    return stbi_write_png (path.c_str (), WIDTH, HEIGHT, 4, rgba.data (), WIDTH * 4) != 0;
}
} // namespace

int main (int argc, char* argv []) {
    if (argc != 4) {
	fprintf (stderr, "usage: %s <assets-dir> <background-dir> <output-dir>\n", argv [0]);
	return 1;
    }

    if (glfwInit () == GLFW_FALSE) {
	fprintf (stderr, "glfwInit failed\n");
	return 1;
    }

    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow (64, 64, "wpe-embed-test", nullptr, nullptr);
    if (window == nullptr) {
	fprintf (stderr, "glfwCreateWindow failed\n");
	return 1;
    }

    glfwMakeContextCurrent (window);

    // resolve the few GL functions the host itself needs (FBO setup + readback)
    glewExperimental = GL_TRUE;
    glewInit ();

    // host-owned render target, standing in for GTK's GLArea FBO
    GLuint texture = 0, fbo = 0;
    glGenTextures (1, &texture);
    glBindTexture (GL_TEXTURE_2D, texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers (1, &fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	fprintf (stderr, "host FBO incomplete\n");
	return 1;
    }

    const wpe_init_params params = {
	.assets_dir = argv [1],
	.background = argv [2],
	.width = WIDTH,
	.height = HEIGHT,
	.vflip = 0,
	.disable_mouse = 0,
	.disable_parallax = 0,
	.disable_audio = 1,
	.disable_audio_processing = 1,
	.volume = 0,
	.properties = nullptr,
    };

    char* error = nullptr;
    wpe_context* ctx = wpe_context_create (&params, getProcAddress, nullptr, &error);

    if (ctx == nullptr) {
	fprintf (stderr, "wpe_context_create failed: %s\n", error != nullptr ? error : "(no message)");
	free (error);
	return 1;
    }

    printf ("context created, rendering %d frames at %dx%d\n", TOTAL_FRAMES, WIDTH, HEIGHT);

    const std::string outputDir = argv [3];
    std::vector<unsigned char> pixels (WIDTH * HEIGHT * 4);
    std::vector<unsigned char> firstFrame;

    for (int frame = 1; frame <= TOTAL_FRAMES; frame++) {
	// wiggle the pointer so pointer-reactive scenes get exercised too
	wpe_context_set_mouse (ctx, WIDTH / 2.0 + frame, HEIGHT / 2.0, 0, 0);
	wpe_context_render (ctx, fbo, WIDTH, HEIGHT, frame * FRAME_STEP);

	if (frame == 1 || frame == TOTAL_FRAMES / 2 || frame == TOTAL_FRAMES) {
	    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
	    glReadPixels (0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());

	    const std::string path = outputDir + "/frame-" + std::to_string (frame) + ".png";
	    if (!savePng (path, pixels)) {
		fprintf (stderr, "failed to write %s\n", path.c_str ());
		return 1;
	    }
	    printf ("wrote %s\n", path.c_str ());

	    if (frame == 1) {
		firstFrame = pixels;
	    }
	}
    }

    // sanity checks: the scene drew something, and it animates
    bool nonBlack = false;
    for (size_t i = 0; i < pixels.size (); i += 4) {
	if (pixels [i] != 0 || pixels [i + 1] != 0 || pixels [i + 2] != 0) {
	    nonBlack = true;
	    break;
	}
    }

    const bool animated = memcmp (firstFrame.data (), pixels.data (), pixels.size ()) != 0;

    printf ("non-black output: %s\n", nonBlack ? "yes" : "NO");
    printf ("frame 1 != frame %d (animated): %s\n", TOTAL_FRAMES, animated ? "yes" : "NO");

    wpe_context_destroy (ctx);
    glfwDestroyWindow (window);
    glfwTerminate ();

    return nonBlack ? 0 : 2;
}
