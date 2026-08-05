#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <mathutils.h>
#include <vector>

// Local Includes
#include "patchparameter.h"
#include "modulator.h"

namespace lx
{
	/**
	 * A named bundle of PatchParameters and Modulators. Has no fixture knowledge and holds no per-source
	 * state: trigger()/stop() forward to modulators, update() only does per-frame transport housekeeping,
	 * and VALUES are computed on demand by evaluate() / evaluateAt().
	 *
	 * There is deliberately no spread mode and no fixture count. How far an effect spreads is a property
	 * of the FixtureGroup a Control is routed to, not of the patch: a channel claim records where it sits
	 * in that group's source list and asks evaluate() for its own value each frame. A one-source spread is
	 * just position 0 of a set of one -- the case that used to be called "Single".
	 */
	class NAPAPI Patch : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		void trigger();
		void stop();
		void update(double deltaTime);
		bool isFinished() const;

		/** @return true if `p` is a modulatable input of one of this patch's Field modulators (rather than a
		 *  source parameter). Such a parameter is written by pass 1 of update() and must never be blended
		 *  into a source value. */
		bool isFieldInput(const PatchParameter* p) const;

		/**
		 * @return `param`'s component `component` for source `sourceIndex` of `sourceCount`: the authored
		 * base with every modulator targeting `param` blended in, each evaluated at ITS OWN position (a
		 * Chase wants sourceIndex/sourceCount, a gradient wants sourceIndex/(sourceCount-1) -- see
		 * positionOf / Modulator::cyclicPositions). Stateless; this is what a channel claim calls.
		 */
		float evaluate(const PatchParameter& param, int sourceIndex, int sourceCount, int component) const
		{
			return blend(param, component, [&](const Modulator& m) { return positionOf(m, sourceIndex, sourceCount); });
		}

		/**
		 * @return the same blend with every modulator evaluated at the continuous position `pos01`. For
		 * previews (lxtheme::FieldStrip), which draw the field itself and so have no source count.
		 */
		float evaluateAt(const PatchParameter& param, float pos01, int component) const
		{
			return blend(param, component, [pos01](const Modulator&) { return pos01; });
		}

		std::string								mName;			///< Property: 'Name'
		std::vector<nap::ResourcePtr<PatchParameter>>	mParameters;	///< Property: 'Parameters'
		std::vector<nap::ResourcePtr<Modulator>>		mModulators;	///< Property: 'Modulators'
		// Editor fold state, serialized so the patch opens the way you left it. The readouts (CURRENT's field
		// strips) stay visible when folded; only the editable rows hide.
		bool									mCurrentCollapsed = false;	///< Property: 'CurrentCollapsed'
		bool									mSourceCollapsed = false;	///< Property: 'SourceCollapsed'
		bool									mModulationCollapsed = false;	///< Property: 'ModulationCollapsed'

	private:
		/** Shared blend: authored base, then every modulator that targets `param`, positioned by `posOf`.
		 *  Templated rather than taking a std::function because evaluate() runs once per claimed channel
		 *  per frame and must not allocate. */
		template<typename PosFn>
		float blend(const PatchParameter& param, int component, PosFn posOf) const
		{
			float v = nap::math::clamp(param.getBaseValue(component), 0.0f, 1.0f);
			for (auto& modulator : mModulators)
			{
				bool targets = false;
				for (auto& t : modulator->mTargets)
					if (t.get() == &param) { targets = true; break; }
				if (!targets)
					continue;

				// A modulator that drives a Field's INPUT contributes to that input, never to a source
				// value; pass 1 of update() has already applied it.
				if (isFieldInput(&param))
					continue;

				// mTargetComponent selects WHICH components this modulator writes (-1 = all). Distinct from
				// the `component` argument, which is the component being asked for.
				if (modulator->mTargetComponent >= 0 && modulator->mTargetComponent != component)
					continue;

				const float m = modulator->value(posOf(*modulator), component);
				switch (modulator->mBlend)
				{
				case EModulatorBlend::Set:		v = m;			break;
				case EModulatorBlend::Scale:	v = v * m;		break;
				case EModulatorBlend::Offset:	v = v + m;		break;
				}
				// Deliberately NOT clamped per step. Clamping mid-chain made Offset saturate early and made
				// Offset-then-Scale lossy in a way that depended on ordering (base .6 +.7 -> clamp 1.0 -> x.5
				// = .5, where the composed value is .65). Every term is already <= 1, so one clamp at the end
				// is enough and the chain composes predictably.
			}
			return nap::math::clamp(v, 0.0f, 1.0f);
		}
	};
}
