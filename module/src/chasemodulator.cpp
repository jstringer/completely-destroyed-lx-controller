#include "chasemodulator.h"
#include "lxcontrolservice.h"

#include <mathutils.h>
#include <cmath>

RTTI_BEGIN_CLASS(lx::ChaseModulator)
	RTTI_PROPERTY("RateInput",			&lx::ChaseModulator::mRateInput,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PulseWidthInput",	&lx::ChaseModulator::mPulseWidthInput,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Glide",				&lx::ChaseModulator::mGlide,			nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// Wraps x into [0,1) -- std::fmod keeps the sign of x, which frac() must not.
	static double wrapFrac(double x)
	{
		return x - std::floor(x);
	}


	float ChaseModulator::rawValue(float pos01, int /*component*/) const
	{
		// Own clock: elapsed x rate replaces the player's playback-speed-driven time. A driven Rate is read
		// fresh here every frame rather than pushed into a stateful transport.
		const double t = mElapsed * static_cast<double>(inputValue(mRateInput, 0.05f, 8.0f));
		double phase = wrapFrac(t - static_cast<double>(pos01));
		float pw = nap::math::clamp(inputValue(mPulseWidthInput, 0.01f, 1.0f), 0.01f, 1.0f);

		if (!mGlide)
			return phase < pw ? 1.0f : 0.0f;

		// Soft-edged: ramp up over the first `edge` fraction, hold, ramp down over the last `edge`.
		double edge = std::min(0.15 * pw, 0.05);
		if (phase < edge)
			return (float)(phase / edge);
		if (phase < pw - edge)
			return 1.0f;
		if (phase < pw)
			return (float)((pw - phase) / edge);
		return 0.0f;
	}
}
