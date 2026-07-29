#pragma once

namespace lxstyleguide
{
	/**
	 * Draws the "Design Language" test-bed window: a live gallery of every lxtheme color, the four-hue
	 * state system, and every widget helper, rendered from fake data so it depends on nothing runtime.
	 * A permanent dev/preview bench for GUI work. Call from App::update() (after selectWindow), passing
	 * a persisted bool the window's close-box can flip. No-ops when *open is false.
	 */
	void draw(bool* open);
}
