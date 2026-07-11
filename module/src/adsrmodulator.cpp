#include "adsrmodulator.h"
#include "lxcontrolservice.h"

#include <sequenceplayer.h>
#include <algorithm>

RTTI_BEGIN_CLASS(lx::AdsrModulator)
	RTTI_PROPERTY("Attack",		&lx::AdsrModulator::mAttack,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Decay",		&lx::AdsrModulator::mDecay,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Sustain",	&lx::AdsrModulator::mSustain,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Release",	&lx::AdsrModulator::mRelease,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Loop",		&lx::AdsrModulator::mLoop,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// Minimum phase length so segments never collapse to zero duration (insertSegment needs t strictly increasing).
	static constexpr double sMinPhase = 0.001;

	void AdsrModulator::generateCurve(nap::lxcontrolService& svc)
	{
		const double a = std::max<double>(mAttack, sMinPhase);
		const double d = std::max<double>(mDecay, sMinPhase);
		const double r = std::max<double>(mRelease, sMinPhase);
		mSustainTime = a + d;
		mDuration = a + d + r;

		const std::vector<lx::Key> keys = {
			{ 0.0,          0.0f,             nap::math::ECurveInterp::Linear },
			{ a,            1.0f,             nap::math::ECurveInterp::Linear },
			{ a + d,        mSustain,         nap::math::ECurveInterp::Linear },
			{ a + d + r,    0.0f,             nap::math::ECurveInterp::Linear }
		};
		svc.authorFloatCurve(*mEditor, mTrackID, keys);
	}


	void AdsrModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mPlayer == nullptr)
			return;
		mPlayer->setPlayerTime(0.0);
		mPlayer->setIsPaused(false);
		mPlayer->setIsLooping(mLoop);
		mPlayer->setIsPlaying(true);
	}


	void AdsrModulator::update(double deltaTime)
	{
		if (mPlayer == nullptr)
			return;

		// Hold sustain: pause exactly at the decay->sustain boundary (non-loop only).
		if (!mReleased && !mLoop && mPlayer->getIsPlaying() && !mPlayer->getIsPaused() &&
			mPlayer->getPlayerTime() >= mSustainTime)
		{
			mPlayer->setIsPaused(true);
		}

		// Release finished (played out to the end): stop.
		if (mReleased && mPlayer->getIsPlaying() && mPlayer->getPlayerTime() >= mDuration)
			mPlayer->setIsPlaying(false);
	}


	void AdsrModulator::onStop()
	{
		Modulator::onStop();
		if (mPlayer == nullptr)
			return;
		// Release from the sustain point out to zero.
		mPlayer->setIsLooping(false);
		mPlayer->setPlayerTime(mSustainTime);
		mPlayer->setIsPaused(false);
		mPlayer->setIsPlaying(true);
	}


	bool AdsrModulator::isFinished() const
	{
		if (mPlayer == nullptr)
			return true;
		return mReleased && !mPlayer->getIsPlaying();
	}
}
