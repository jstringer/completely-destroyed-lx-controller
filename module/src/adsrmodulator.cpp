#include "adsrmodulator.h"

#include <sequenceplayer.h>
#include <mathutils.h>

RTTI_BEGIN_CLASS(lx::AdsrModulator)
	RTTI_PROPERTY("Attack",		&lx::AdsrModulator::mAttack,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Decay",		&lx::AdsrModulator::mDecay,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Sustain",	&lx::AdsrModulator::mSustain,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Release",	&lx::AdsrModulator::mRelease,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Loop",		&lx::AdsrModulator::mLoop,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	float AdsrModulator::evaluate(double time)
	{
		if (mReleasing)
		{
			double rel = time - mReleaseStartTime;
			if (mRelease <= 0.0f || rel >= mRelease)
				return 0.0f;
			return mReleaseFrom * (1.0f - static_cast<float>(rel / mRelease));
		}

		if (!mHeld)
			return 0.0f;

		if (time < mAttack)
			return mAttack > 0.0f ? static_cast<float>(time / mAttack) : 1.0f;

		double decay_t = time - mAttack;
		if (decay_t < mDecay)
			return mDecay > 0.0f ? nap::math::lerp(1.0f, mSustain, static_cast<float>(decay_t / mDecay)) : mSustain;

		return mSustain;
	}


	void AdsrModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mPlayer != nullptr)
			mPlayer->setPlayerTime(0.0);
	}


	bool AdsrModulator::isFinished() const
	{
		if (mReleasing)
		{
			double now = mPlayer != nullptr ? mPlayer->getPlayerTime() : 0.0;
			return (now - mReleaseStartTime) >= mRelease;
		}
		return !mHeld;
	}
}
