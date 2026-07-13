#include "chasemodulator.h"
#include "lxcontrolservice.h"

#include <sequenceplayer.h>
#include <mathutils.h>
#include <cmath>

RTTI_BEGIN_CLASS(lx::ChaseModulator)
	RTTI_PROPERTY("Rate",		&lx::ChaseModulator::mRate,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PulseWidth",	&lx::ChaseModulator::mPulseWidth,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Glide",		&lx::ChaseModulator::mGlide,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// Wraps x into [0,1) -- std::fmod keeps the sign of x, which frac() must not.
	static double wrapFrac(double x)
	{
		return x - std::floor(x);
	}


	void ChaseModulator::generateCurve(nap::lxcontrolService& svc)
	{
		// Value is computed analytically in valueForSlot() from the player's own time; this dummy curve
		// exists only to pin mDuration/the sequence duration to one second, matching LfoModulator.
		mDuration = 1.0;
		using I = nap::math::ECurveInterp;
		std::vector<lx::Key> keys = { {0.0, 0.0f, I::Linear}, {1.0, 0.0f, I::Linear} };
		svc.authorFloatCurve(*mEditor, mTrackID, keys);
	}


	void ChaseModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mPlayer == nullptr)
			return;
		mPlayer->setPlaybackSpeed(mRate);
		mPlayer->setIsLooping(true);
		if (!mPlayer->getIsPlaying())
			mPlayer->setPlayerTime(0.0);
		mPlayer->setIsPlaying(true);
	}


	void ChaseModulator::onStop()
	{
		Modulator::onStop();
		if (mPlayer != nullptr)
			mPlayer->setIsPlaying(false);	// free-running: gate-off stops immediately
	}


	float ChaseModulator::valueForSlot(int slot) const
	{
		if (mPlayer == nullptr)
			return 0.0f;

		double t = mPlayer->getPlayerTime();
		double phase = wrapFrac(t - (double)slot / (double)mSlotCount);
		float pw = nap::math::clamp(mPulseWidth, 0.01f, 1.0f);

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
