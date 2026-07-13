#pragma once

#include "WallpaperEngine/Input/MouseInput.h"

namespace WallpaperEngine::Embed {
/**
 * Mouse input fed by the embedding host instead of polled from a window system.
 * The host provides coordinates already converted to OpenGL convention
 * (origin bottom-left); update() is a no-op.
 */
class ExternalMouseInput final : public Input::MouseInput {
public:
    void update () override { }

    [[nodiscard]] glm::dvec2 position () const override { return this->m_position; }
    [[nodiscard]] Input::MouseClickStatus leftClick () const override { return this->m_leftClick; }
    [[nodiscard]] Input::MouseClickStatus rightClick () const override { return this->m_rightClick; }

    void setPosition (const glm::dvec2 position) { this->m_position = position; }

    void setButtons (const bool left, const bool right) {
	this->m_leftClick = left ? Input::MouseClickStatus::Clicked : Input::MouseClickStatus::Released;
	this->m_rightClick = right ? Input::MouseClickStatus::Clicked : Input::MouseClickStatus::Released;
    }

private:
    glm::dvec2 m_position = { 0, 0 };
    Input::MouseClickStatus m_leftClick = Input::MouseClickStatus::Released;
    Input::MouseClickStatus m_rightClick = Input::MouseClickStatus::Released;
};
} // namespace WallpaperEngine::Embed
