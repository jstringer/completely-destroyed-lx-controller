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
	// Deterministic pure hash of (voice, step, seed) -> [0,1). Splitmix64-style avalanche; no persisted RNG
	// state needed since the same (voice, step, seed) always maps to the same value. `seed` differentiates
	// otherwise-identical (voice, step) streams across independent NoiseModulator instances.
	static float hash01(int voice, uint64_t step, int seed)
	{
		uint64_t x = (static_cast<uint64_t>(static_cast<uint32_t>(voice)) << 32) ^ step;
		x ^= static_cast<uint64_t>(static_cast<uint32_t>(seed)) * 0x9E3779B97F4A7C15ULL;
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
		// Value is computed analytically in value(pos01) from mElapsed; this dummy curve exists only
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


	float NoiseModulator::value(float pos01, int component) const
	{
		double t = mElapsed * (double)std::max(mRate, 0.001f);
		double step_d = std::floor(t);
		uint64_t step = static_cast<uint64_t>(std::max(step_d, 0.0));
		float frac = static_cast<float>(t - step_d);

		// The position bucket stands in for the old voice index; `component` replaces the per-instance
		// seed. Buckets are independent here (spatially incoherent) -- Density/coherence is Task 9.
		const int bucket = static_cast<int>(pos01 * 1024.0f);

		float a = hash01(bucket, step, component);
		if (mSmoothing <= 0.0f)
			return a;

		float b = hash01(bucket, step + 1, component);
		return nap::math::lerp(a, b, smoothstep01(frac));
	}
}
