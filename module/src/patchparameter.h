#pragma once

// External Includes
#include <nap/resource.h>
#include <vector>

// Local Includes
#include "channelrole.h"

namespace lx
{
	/**
	 * A logical patch parameter: an authored base value (serialized) plus a live runtime output
	 * (mCurrentValues, non-serialized) that modulators blend into. One parameter can have several
	 * components (a color has 3). Each component has a semantic role and optional unit scoping.
	 *
	 * Deliberately does NOT wrap nap::Parameter (see PLAN.md §2): every component is 0..1 so there's
	 * no min/max win, and the GUI widgets are one-liners.
	 */
	class NAPAPI PatchParameter : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		virtual bool init(nap::utility::ErrorState& errorState) override;

		/** @return number of value components (1 for float/toggle, 3 for color). */
		virtual int getComponentCount() const			{ return 0; }
		/** @return the semantic role of component c. */
		virtual EChannelRole getComponentRole(int c) const	{ return EChannelRole::Generic; }
		/** @return the authored base value of component c (0..1). */
		virtual float getBaseValue(int c) const			{ return 0.0f; }

		/** @return true if this parameter targets the given fixture unit (empty Units = all units). */
		bool appliesToUnit(int unit) const;

		/** Voice-0 convenience wrappers (Single-mode patches; every existing caller keeps working). */
		float getComponentValue(int c) const					{ return getComponentValue(0, c); }
		void setComponentValue(int c, float value)				{ setComponentValue(0, c, value); }

		/** @return component c's current (post-modulation) value for fixture voice `voice`. */
		float getComponentValue(int voice, int c) const;
		/** Sets component c's current value for fixture voice `voice`, clamped 0..1. Grows storage as needed. */
		void setComponentValue(int voice, int c, float value);

		/** Copies each component's authored base value into every one of `voices` fixture voices
		 *  (called each frame before modulation; `voices` = 1 for Single-mode patches). */
		void resetToBase(int voices = 1);

		std::string			mName;			///< Property: 'Name'
		std::vector<int>	mUnits;			///< Property: 'Units' empty = all units

		/** Runtime post-modulation output, non-serialized. Flattened [voice * getComponentCount() + c]. */
		std::vector<float>	mCurrentValues;
	};


	/** Single-component float parameter with an explicit role. */
	class NAPAPI FloatParameter : public PatchParameter
	{
		RTTI_ENABLE(PatchParameter)
	public:
		int getComponentCount() const override			{ return 1; }
		EChannelRole getComponentRole(int) const override	{ return mRole; }
		float getBaseValue(int) const override			{ return mValue; }

		EChannelRole	mRole = EChannelRole::Generic;	///< Property: 'Role'
		float			mValue = 0.0f;					///< Property: 'Value'
	};


	/** Three-component color parameter (roles Red/Green/Blue). */
	class NAPAPI ColorParameter : public PatchParameter
	{
		RTTI_ENABLE(PatchParameter)
	public:
		int getComponentCount() const override { return 3; }
		EChannelRole getComponentRole(int c) const override;
		float getBaseValue(int c) const override;

		float mRed = 0.0f;		///< Property: 'Red'
		float mGreen = 0.0f;	///< Property: 'Green'
		float mBlue = 0.0f;		///< Property: 'Blue'
	};


	/** Single-component boolean parameter (0/1) with an explicit role. */
	class NAPAPI ToggleParameter : public PatchParameter
	{
		RTTI_ENABLE(PatchParameter)
	public:
		int getComponentCount() const override			{ return 1; }
		EChannelRole getComponentRole(int) const override	{ return mRole; }
		float getBaseValue(int) const override			{ return mValue ? 1.0f : 0.0f; }

		EChannelRole	mRole = EChannelRole::Generic;	///< Property: 'Role'
		bool			mValue = false;					///< Property: 'Value'
	};
}
