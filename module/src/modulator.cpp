#include "modulator.h"
#include "effectparameter.h"

#include <mathutils.h>

RTTI_BEGIN_ENUM(lx::EModulatorBlend)
	RTTI_ENUM_VALUE(lx::EModulatorBlend::Replace,	"Replace"),
	RTTI_ENUM_VALUE(lx::EModulatorBlend::Multiply,	"Multiply"),
	RTTI_ENUM_VALUE(lx::EModulatorBlend::Add,		"Add")
RTTI_END_ENUM

RTTI_BEGIN_ENUM(lx::EModulatorClock)
	RTTI_ENUM_VALUE(lx::EModulatorClock::Standard,		"Standard"),
	RTTI_ENUM_VALUE(lx::EModulatorClock::Independent,	"Independent")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Modulator)
	RTTI_PROPERTY("Name",			&lx::Modulator::mName,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Target",			&lx::Modulator::mTarget,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetComponent",	&lx::Modulator::mTargetComponent,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Min",			&lx::Modulator::mMin,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Max",			&lx::Modulator::mMax,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Blend",			&lx::Modulator::mBlend,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Clock",			&lx::Modulator::mClock,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ClockFrequency",	&lx::Modulator::mClockFrequency,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	bool Modulator::init(nap::utility::ErrorState& errorState)
	{
		return true;
	}


	void Modulator::onTrigger()
	{
		// Subtypes reset mElapsed if they need to (ADSR always; LFO only if Retrigger).
		mHeld = true;
		mReleasing = false;
	}


	void Modulator::onStop()
	{
		// Capture the value being released FROM while still "held", then flip to releasing.
		mReleaseFrom = evaluate();
		mReleaseElapsed = 0.0;
		mHeld = false;
		mReleasing = true;
	}


	void Modulator::advance(double deltaTime)
	{
		mElapsed += deltaTime;
		if (mReleasing)
			mReleaseElapsed += deltaTime;
	}


	float Modulator::value() const
	{
		float raw = mSink != nullptr ? mSink->mValue : 0.0f;
		return nap::math::lerp(mMin, mMax, raw);
	}
}
