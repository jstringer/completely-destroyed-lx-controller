#include "effectparameter.h"

#include <mathutils.h>
#include <algorithm>

RTTI_BEGIN_CLASS(lx::EffectParameter)
	RTTI_PROPERTY("Name",	&lx::EffectParameter::mName,	nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Units",	&lx::EffectParameter::mUnits,	nap::rtti::EPropertyMetaData::Default)
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
	bool EffectParameter::init(nap::utility::ErrorState& errorState)
	{
		mCurrentValues.assign(getComponentCount(), 0.0f);
		resetToBase();
		return true;
	}


	bool EffectParameter::appliesToUnit(int unit) const
	{
		if (mUnits.empty())
			return true;
		return std::find(mUnits.begin(), mUnits.end(), unit) != mUnits.end();
	}


	float EffectParameter::getComponentValue(int c) const
	{
		return (c >= 0 && c < static_cast<int>(mCurrentValues.size())) ? mCurrentValues[c] : 0.0f;
	}


	void EffectParameter::setComponentValue(int c, float value)
	{
		if (c >= 0 && c < static_cast<int>(mCurrentValues.size()))
			mCurrentValues[c] = nap::math::clamp(value, 0.0f, 1.0f);
	}


	void EffectParameter::resetToBase()
	{
		int count = getComponentCount();
		if (static_cast<int>(mCurrentValues.size()) != count)
			mCurrentValues.assign(count, 0.0f);
		for (int c = 0; c < count; ++c)
			mCurrentValues[c] = nap::math::clamp(getBaseValue(c), 0.0f, 1.0f);
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
