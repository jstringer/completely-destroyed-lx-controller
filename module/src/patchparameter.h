#pragma once

// External Includes
#include <nap/resource.h>
#include <vector>

// Local Includes
#include "channelrole.h"

namespace lx
{
	/**
	 * A logical patch parameter: purely the AUTHORED base value. One parameter can have several
	 * components (a colour has 3); each component has a semantic role.
	 *
	 * It holds no live/runtime value. Post-modulation values are computed on demand by
	 * Patch::evaluate(param, sourceIndex, sourceCount, component) -- a channel claim knows where it sits
	 * in the fired group's source list and asks for its own value each frame. That is what removed the
	 * per-voice buffer this class used to own, and with it the collision two activations of one shared
	 * patch used to have over that single buffer.
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

		std::string			mName;			///< Property: 'Name'
		std::vector<int>	mUnits;			///< Property: 'Units' empty = all units
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
