#include "noisemodulator.h"
#include "lxcontrolservice.h"

#include <sequenceplayer.h>
#include <mathutils.h>
#include <cmath>

RTTI_BEGIN_CLASS(lx::NoiseModulator)
	RTTI_PROPERTY("Rate",		&lx::NoiseModulator::mRate,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Smoothing",	&lx::NoiseModulator::mSmoothing,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// Deterministic pure hash of (slot, step) -> [0,1). Splitmix64-style avalanche; no persisted RNG
	// state needed since the same (slot, step) always maps to the same value.
	static float hash01(int slot, uint64_t step)
	{
		uint64_t x = (static_cast<uint64_t>(static_cast<uint32_t>(slot)) << 32) ^ step;
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


	void NoiseModulator::generateCurve(nap::lxcontrolService& svc)
	{
		// Value is computed analytically in valueForSlot() from mElapsed; this dummy curve exists only
		// to pin mDuration/the sequence duration, matching every other modulator's generateCurve().
		mDuration = 1.0;
		using I = nap::math::ECurveInterp;
		std::vector<lx::Key> keys = { {0.0, 0.0f, I::Linear}, {1.0, 0.0f, I::Linear} };
		svc.authorFloatCurve(*mEditor, mTrackID, keys);
	}


	void NoiseModulator::onTrigger()
	{
		Modulator::onTrigger();
		mElapsed = 0.0;
		if (mPlayer == nullptr)
			return;
		mPlayer->setIsLooping(true);
		mPlayer->setIsPlaying(true);
	}


	void NoiseModulator::onStop()
	{
		Modulator::onStop();
		if (mPlayer != nullptr)
			mPlayer->setIsPlaying(false);	// free-running: gate-off stops immediately
	}


	void NoiseModulator::update(double deltaTime)
	{
		if (mPlayer != nullptr && mPlayer->getIsPlaying())
			mElapsed += deltaTime;
	}


	float NoiseModulator::valueForSlot(int slot) const
	{
		double t = mElapsed * (double)std::max(mRate, 0.001f);
		double step_d = std::floor(t);
		uint64_t step = static_cast<uint64_t>(std::max(step_d, 0.0));
		float frac = static_cast<float>(t - step_d);

		float a = hash01(slot, step);
		if (mSmoothing <= 0.0f)
			return a;

		float b = hash01(slot, step + 1);
		return nap::math::lerp(a, b, smoothstep01(frac));
	}
}
