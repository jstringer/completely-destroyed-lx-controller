#include "modulator.h"
#include "patchparameter.h"

#include <sequenceplayer.h>
#include <sequence.h>
#include <sequencetrack.h>
#include <sequencetracksegmentcurve.h>
#include <mathutils.h>

RTTI_BEGIN_ENUM(lx::EModulatorBlend)
	RTTI_ENUM_VALUE(lx::EModulatorBlend::Replace,	"Replace"),
	RTTI_ENUM_VALUE(lx::EModulatorBlend::Multiply,	"Multiply"),
	RTTI_ENUM_VALUE(lx::EModulatorBlend::Add,		"Add")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Modulator)
	RTTI_PROPERTY("Name",			&lx::Modulator::mName,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Target",			&lx::Modulator::mTarget,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetComponent",	&lx::Modulator::mTargetComponent,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Min",			&lx::Modulator::mMin,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Max",			&lx::Modulator::mMax,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Blend",			&lx::Modulator::mBlend,				nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	bool Modulator::init(nap::utility::ErrorState& errorState)
	{
		return true;
	}


	void Modulator::onTrigger()
	{
		mReleased = false;
	}


	void Modulator::onStop()
	{
		mReleased = true;
	}


	bool Modulator::isFinished() const
	{
		// No player yet, or the transport has stopped -> not contributing.
		return mPlayer == nullptr || !mPlayer->getIsPlaying();
	}


	float Modulator::value() const
	{
		float raw = mSink != nullptr ? mSink->mValue : 0.0f;
		return nap::math::lerp(mMin, mMax, raw);
	}


	float Modulator::sampleShape(float phase01) const
	{
		if (mPlayer == nullptr || mDuration <= 0.0)
			return 0.0f;
		phase01 = nap::math::clamp(phase01, 0.0f, 1.0f);
		const double t = phase01 * mDuration;

		const nap::SequenceTrack* track = nullptr;
		for (const auto& tr : mPlayer->getSequenceConst().mTracks)
			if (tr->mID == mTrackID) { track = tr.get(); break; }
		if (track == nullptr || track->mSegments.empty())
			return 0.0f;

		const nap::SequenceTrackSegmentCurveFloat* last = nullptr;
		for (const auto& s : track->mSegments)
		{
			if (!s->get_type().is_derived_from(RTTI_OF(nap::SequenceTrackSegmentCurveFloat)))
				continue;
			const auto* seg = static_cast<const nap::SequenceTrackSegmentCurveFloat*>(s.get());
			last = seg;
			const double end = seg->mStartTime + seg->mDuration;
			if (t >= seg->mStartTime && t <= end)
			{
				const float local = seg->mDuration > 0.0 ? static_cast<float>((t - seg->mStartTime) / seg->mDuration) : 0.0f;
				return seg->getValue(nap::math::clamp(local, 0.0f, 1.0f));
			}
		}
		return last != nullptr ? last->getEndValue() : 0.0f;	// past the end: hold last value
	}


	float Modulator::playheadPhase() const
	{
		if (mPlayer == nullptr || mDuration <= 0.0 || !mPlayer->getIsPlaying())
			return -1.0f;
		return nap::math::clamp(static_cast<float>(mPlayer->getPlayerTime() / mDuration), 0.0f, 1.0f);
	}
}
