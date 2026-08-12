#include "noisemodulator.h"
#include "lxcontrolservice.h"

#include <mathutils.h>
#include <cmath>

RTTI_BEGIN_CLASS(lx::NoiseModulator)
	RTTI_PROPERTY("RateInput",		&lx::NoiseModulator::mRateInput,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("DensityInput",	&lx::NoiseModulator::mDensityInput,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("SmoothingInput",	&lx::NoiseModulator::mSmoothingInput,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// Deterministic pure hash of (cell, step, component) -> [0,1). Splitmix64-style avalanche; no persisted
	// RNG state needed since the same inputs always map to the same value. `component` differentiates the
	// R/G/B streams of one colour field -- the job the old per-instance Seed property did by hand.
	static float hash01(int cell, uint64_t step, int component)
	{
		uint64_t x = (static_cast<uint64_t>(static_cast<uint32_t>(cell)) << 32) ^ step;
		x ^= static_cast<uint64_t>(static_cast<uint32_t>(component)) * 0x9E3779B97F4A7C15ULL;
		x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
		x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
		x ^= x >> 33;
		return static_cast<float>(x >> 11) * (1.0f / static_cast<float>(1ULL << 53));
	}


	static float smoothstep01(float t)
	{
		t = nap::math::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}


	float NoiseModulator::rawValue(float pos01, int component) const
	{
		const float rate    = inputValue(mRateInput, 0.05f, 8.0f);
		const float density = inputValue(mDensityInput, 1.0f, 24.0f);
		const float smooth  = inputValue(mSmoothingInput, 0.0f, 1.0f);

		// Bilinear value noise over (space, time). Interpolating on the SPACE axis is what gives coherence:
		// the old hash was deliberately incoherent per voice, which is a different look from the Perlin-ish
		// field this is for. Density is the spatial frequency, so high Density recovers the old look.
		const double tt = mElapsed * static_cast<double>(std::max(rate, 0.001f));
		const double xx = static_cast<double>(pos01) * static_cast<double>(density);

		const int ix = static_cast<int>(std::floor(xx));
		const double it_d = std::floor(tt);
		const uint64_t it = static_cast<uint64_t>(std::max(it_d, 0.0));
		const float fx = smoothstep01(static_cast<float>(xx - std::floor(xx)));
		const float ft = smoothstep01(static_cast<float>(tt - it_d));

		const float a = hash01(ix,     it,     component);
		const float b = hash01(ix + 1, it,     component);
		const float top = nap::math::lerp(a, b, fx);
		if (smooth <= 0.0f)
			return top;		// hard sample-and-hold in time (the old Smoothing 0 behaviour)

		const float c = hash01(ix,     it + 1, component);
		const float d = hash01(ix + 1, it + 1, component);
		const float bot = nap::math::lerp(c, d, fx);
		return nap::math::lerp(top, bot, ft * smooth);
	}
}
