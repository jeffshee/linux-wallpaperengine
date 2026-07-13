#include "wpe_embed.h"

#include "ExternalOpenGLDriver.h"

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Embed;

struct wpe_context {
    /** argv strings backing the ApplicationContext for its whole lifetime */
    std::vector<std::string> args {};
    std::vector<char*> argv {};

    std::unique_ptr<ApplicationContext> appContext = nullptr;
    std::unique_ptr<WallpaperApplication> app = nullptr;
    /** owned by the application once injected */
    ExternalOpenGLDriver* driver = nullptr;

    /** scene clock, advanced from host timestamps while not paused */
    double sceneTime = 0.0;
    double lastHostTime = 0.0;
    bool firstFrame = true;
    bool paused = false;

    /** raw pointer state in window coordinates (origin top-left) */
    double mouseX = 0.0;
    double mouseY = 0.0;
    bool mouseLeft = false;
    bool mouseRight = false;
    bool mouseEnabled = true;
};

static void setError (char** error_out, const char* message) {
    if (error_out != nullptr) {
	*error_out = strdup (message);
    }
}

wpe_context* wpe_context_create (
    const wpe_init_params* params, const wpe_get_proc_address_fn get_proc_address, void* userdata, char** error_out
) {
    if (params == nullptr || params->assets_dir == nullptr || params->background == nullptr
	|| get_proc_address == nullptr) {
	setError (error_out, "wpe_context_create: assets_dir, background and get_proc_address are required");
	return nullptr;
    }

    auto ctx = std::make_unique<wpe_context> ();

    ctx->args = { "wpe-embed", "--assets-dir", params->assets_dir };

    if (params->disable_mouse) {
	ctx->args.emplace_back ("--disable-mouse");
	ctx->mouseEnabled = false;
    }
    if (params->disable_parallax) {
	ctx->args.emplace_back ("--disable-parallax");
    }
    if (params->disable_audio) {
	ctx->args.emplace_back ("--silent");
    }
    if (params->disable_audio_processing) {
	ctx->args.emplace_back ("--no-audio-processing");
    }

    // the fullscreen detector is a no-op stub and pulse-based automute would
    // need a real one; the host decides when to pause instead
    ctx->args.emplace_back ("--noautomute");
    ctx->args.emplace_back ("--no-fullscreen-pause");

    ctx->args.emplace_back ("--volume");
    ctx->args.emplace_back (std::to_string (std::clamp (params->volume, 0, 128)));

    if (params->properties != nullptr) {
	for (const char* const* property = params->properties; *property != nullptr; property++) {
	    ctx->args.emplace_back ("--set-property");
	    ctx->args.emplace_back (*property);
	}
    }

    ctx->args.emplace_back (params->background);

    for (auto& arg : ctx->args) {
	ctx->argv.push_back (arg.data ());
    }

    try {
	ctx->appContext = std::make_unique<ApplicationContext> (static_cast<int> (ctx->argv.size ()), ctx->argv.data ());
	ctx->appContext->loadSettingsFromArgv ();

	ctx->app = std::make_unique<WallpaperApplication> (*ctx->appContext);

	auto driver = std::make_unique<ExternalOpenGLDriver> (
	    *ctx->appContext, *ctx->app, get_proc_address, userdata,
	    glm::ivec2 { std::max (params->width, 1), std::max (params->height, 1) }, params->vflip != 0
	);
	ctx->driver = driver.get ();

	ctx->app->setExternalComponents (
	    std::move (driver),
	    std::make_unique<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> (*ctx->appContext)
	);

	// creates the render context, wallpapers and their GL resources —
	// the host GL context must be current
	ctx->app->setup ();
    } catch (const std::exception& e) {
	setError (error_out, e.what ());
	return nullptr;
    }

    return ctx.release ();
}

void wpe_context_render (
    wpe_context* ctx, const unsigned int framebuffer, const int width, const int height, const double time_seconds
) {
    if (ctx->firstFrame) {
	ctx->lastHostTime = time_seconds;
	ctx->firstFrame = false;
    }

    if (!ctx->paused) {
	// clamp so host stalls (suspend, debugger) don't fast-forward the scene
	ctx->sceneTime += std::clamp (time_seconds - ctx->lastHostTime, 0.0, 1.0);
    }

    ctx->lastHostTime = time_seconds;

    ctx->driver->setFramebufferSize ({ std::max (width, 1), std::max (height, 1) });
    ctx->driver->setRenderTime (static_cast<float> (ctx->sceneTime));

    if (ctx->mouseEnabled) {
	// convert to OpenGL convention (origin bottom-left), like the window drivers do
	ctx->driver->getMouseInput ().setPosition ({ ctx->mouseX, static_cast<double> (height) - ctx->mouseY });
	ctx->driver->getMouseInput ().setButtons (ctx->mouseLeft, ctx->mouseRight);
    }

    ctx->app->setDestinationFramebuffer (framebuffer);
    ctx->app->render ();
}

void wpe_context_set_paused (wpe_context* ctx, const int paused) { ctx->paused = paused != 0; }

void wpe_context_set_volume (wpe_context* ctx, const int volume) {
    ctx->appContext->state.audio.volume = std::clamp (volume, 0, 128);
}

void wpe_context_set_audio_enabled (wpe_context* ctx, const int enabled) {
    ctx->appContext->state.audio.enabled = enabled != 0;
}

void wpe_context_set_mouse (
    wpe_context* ctx, const double x, const double y, const int left_pressed, const int right_pressed
) {
    ctx->mouseX = x;
    ctx->mouseY = y;
    ctx->mouseLeft = left_pressed != 0;
    ctx->mouseRight = right_pressed != 0;
}

void wpe_context_destroy (wpe_context* ctx) {
    // tears down wallpapers and their GL resources — the host GL context must
    // be current; the application owns and frees the injected driver
    delete ctx;
}
