#include "adsrmodulator.h"

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
	float AdsrModulator::evaluate() const
	{
		if (mReleasing)
		{
			if (mRelease <= 0.0f || mReleaseElapsed >= mRelease)
				return 0.0f;
			return mReleaseFrom * (1.0f - static_cast<float>(mReleaseElapsed / mRelease));
		}

		if (!mHeld)
			return 0.0f;

		double t = mElapsed;
		if (t < mAttack)
			return mAttack > 0.0f ? static_cast<float>(t / mAttack) : 1.0f;

		double decay_t = t - mAttack;
		if (decay_t < mDecay)
			return mDecay > 0.0f ? nap::math::lerp(1.0f, mSustain, static_cast<float>(decay_t / mDecay)) : mSustain;

		return mSustain;
	}


	void AdsrModulator::onTrigger()
	{
		Modulator::onTrigger();
		mElapsed = 0.0;	// envelope restarts from attack
	}


	bool AdsrModulator::isFinished() const
	{
		if (mReleasing)
			return mReleaseElapsed >= mRelease;
		return !mHeld;
	}
}
