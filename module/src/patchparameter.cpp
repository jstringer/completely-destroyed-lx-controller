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
		return true;	// authored base only; nothing runtime to size or reset
	}


	bool PatchParameter::appliesToUnit(int unit) const
	{
		if (mUnits.empty())
			return true;
		return std::find(mUnits.begin(), mUnits.end(), unit) != mUnits.end();
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
