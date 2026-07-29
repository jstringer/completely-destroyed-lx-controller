#include "patchparameter.h"

#include <mathutils.h>
#include <algorithm>

RTTI_BEGIN_CLASS(lx::PatchParameter)
	RTTI_PROPERTY("Name",	&lx::PatchParameter::mName,	nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Units",	&lx::PatchParameter::mUnits,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(lx::FloatParameter)
	RTTI_PROPERTY("Role",	&lx::FloatParameter::mRole,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Value",	&lx::FloatParameter::mValue,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(lx::ColorParameter)
	RTTI_PROPERTY("Red",	&lx::ColorParameter::mRed,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Green",	&lx::ColorParameter::mGreen,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Blue",	&lx::ColorParameter::mBlue,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(lx::ToggleParameter)
	RTTI_PROPERTY("Role",	&lx::ToggleParameter::mRole,	nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Value",	&lx::ToggleParameter::mValue,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	bool PatchParameter::init(nap::utility::ErrorState& errorState)
	{
		mCurrentValues.assign(getComponentCount(), 0.0f);
		resetToBase();
		return true;
	}


	bool PatchParameter::appliesToUnit(int unit) const
	{
		if (mUnits.empty())
			return true;
		return std::find(mUnits.begin(), mUnits.end(), unit) != mUnits.end();
	}


	float PatchParameter::getComponentValue(int voice, int c) const
	{
		int count = getComponentCount();
		if (voice < 0 || c < 0 || c >= count)
			return 0.0f;
		int idx = voice * count + c;
		return (idx < static_cast<int>(mCurrentValues.size())) ? mCurrentValues[idx] : getBaseValue(c);
	}


	void PatchParameter::setComponentValue(int voice, int c, float value)
	{
		int count = getComponentCount();
		if (voice < 0 || c < 0 || c >= count)
			return;
		int idx = voice * count + c;
		if (static_cast<int>(mCurrentValues.size()) <= idx)
			mCurrentValues.resize(idx + 1, 0.0f);
		mCurrentValues[idx] = nap::math::clamp(value, 0.0f, 1.0f);
	}


	void PatchParameter::resetToBase(int voices)
	{
		int count = getComponentCount();
		int total = std::max(1, voices) * count;
		if (static_cast<int>(mCurrentValues.size()) != total)
			mCurrentValues.assign(total, 0.0f);
		for (int s = 0; s < std::max(1, voices); ++s)
			for (int c = 0; c < count; ++c)
				mCurrentValues[s * count + c] = nap::math::clamp(getBaseValue(c), 0.0f, 1.0f);
	}


	EChannelRole ColorParameter::getComponentRole(int c) const
	{
		switch (c)
		{
		case 0:		return EChannelRole::Red;
		case 1:		return EChannelRole::Green;
		default:	return EChannelRole::Blue;
		}
	}


	float ColorParameter::getBaseValue(int c) const
	{
		switch (c)
		{
		case 0:		return mRed;
		case 1:		return mGreen;
		default:	return mBlue;
		}
	}
}
