#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>
#include <string>

// Local Includes
#include "patchparameter.h"

namespace nap
{
	class SequencePlayer;
	class SequenceEditor;
	class lxcontrolService;
}

namespace lx
{
	/**
	 * How a modulator folds into the running value of its target, applied in patch order (see
	 * Patch::blend). Named for intent rather than arithmetic:
	 *  - Set    (was Replace)  : v = m      -- DISCARDS the base and every earlier modulator on this target
	 *  - Scale  (was Multiply)  : v = v * m
	 *  - Offset (was Add)       : v = v + m  -- clamped once at the end of the chain, not per step
	 */
	enum class EModulatorBlend : int { Set, Scale, Offset };

	/**
	 * Base class for a modulator: a shape that drives a 0..1 value over time and blends it into a target
	 * PatchParameter. The value comes from a real napsequence curve track (authored by generateCurve()
	 * via lxcontrolService::authorFloatCurve) that the stock SequencePlayerCurveAdapter samples off the
	 * SequencePlayer's own time into a sink ParameterFloat. Gate/trigger events map onto the player
	 * transport (setIsPlaying/setPlayerTime/setIsPaused/setIsLooping/setPlaybackSpeed) in onTrigger()/
	 * onStop()/update(). value() reads the sink back and maps it to [Min,Max]; Patch::update() blends it.
	 */
	class NAPAPI Modulator : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		virtual bool init(nap::utility::ErrorState& errorState) override;

		/** Authors this shape's curve into mTrackID (via svc.authorFloatCurve). Sets mDuration. */
		virtual void generateCurve(nap::lxcontrolService& svc) {}

		/** Gate on / (re)trigger: drives the player transport. Base clears the released flag. */
		virtual void onTrigger();
		/** Gate off: drives the player transport. Base sets the released flag. */
		virtual void onStop();
		/** Per-frame transport housekeeping on the main thread (sustain pause, one-shot end-stop). */
		virtual void update(double deltaTime) {}
		/** @return true once this modulator has stopped contributing (drives release-linger / claim reap). */
		virtual bool isFinished() const;

		/**
		 * @return this modulator's output at normalised position `pos01` for value component `component`,
		 * mapped into [Min,Max].
		 *
		 * NON-VIRTUAL on purpose: it is the one place the [Min,Max] range is applied, so a subclass cannot
		 * silently skip it. Override rawValue() instead. (Chase/Noise/Gradient each used to override
		 * value(); every one of them forgot the range, which is why the Min/Max sliders were inert for
		 * every Field modulator.)
		 *
		 * `component` is a component of the VALUE (0..2 of an RGB colour), used by fields that decorrelate
		 * per channel. It is NOT mTargetComponent, which selects which components get written -- the two
		 * are independent and easy to confuse.
		 */
		float value(float pos01, int component) const
		{
			const float raw = rawValue(pos01, component);
			return nap::math::lerp(mMin, mMax, raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw));
		}

		/** @return true when this modulator's source positions should be endpoint-EXCLUSIVE (i/n), so a
		 *  looping shape's seam does not land on source 0. Chase: true. Gradients/envelopes: false
		 *  (i/(n-1), so the last source actually reaches the end of the shape). See positionOf(). */
		virtual bool cyclicPositions() const			{ return false; }

		/** @return the authored curve's raw value (0..1) at normalized phase `phase01` across mDuration,
		 *  evaluated off the real napsequence curve (Bezier and all) — i.e. the static SHAPE, for previews. */
		float sampleShape(float phase01) const;
		/** @return the transport's current position as a phase 0..1 (loops for looping shapes), or -1 when
		 *  not playing — i.e. where the playhead marker sits on the shape preview. */
		float playheadPhase() const;

		std::string							mName;						///< Property: 'Name'
		nap::ResourcePtr<PatchParameter>	mTarget;					///< Property: 'Target' LEGACY single target; init() migrates it into mTargets
		std::vector<nap::ResourcePtr<PatchParameter>>	mTargets;		///< Property: 'Targets' every PatchParameter this modulator drives (mod-matrix)
		int									mTargetComponent = -1;		///< Property: 'TargetComponent' -1 = all
		float								mMin = 0.0f;				///< Property: 'Min'
		float								mMax = 1.0f;				///< Property: 'Max'
		/// Property: 'Blend'. Offset by default -- the only mode that composes. Defaulting to Set meant two
		/// freshly added modulators silently annihilated each other (the second discards the first).
		EModulatorBlend						mBlend = EModulatorBlend::Offset;
		/// Property: 'Collapsed' -- editor state, serialized on purpose so a folded card stays folded across
		/// restarts. A collapsed card still shows its plate/kind/targets/preview; only the editable params hide.
		bool								mCollapsed = false;

		// Runtime, wired by lxcontrolService::buildModulatorGraph (non-serialized; the objects are).
		nap::SequencePlayer*	mPlayer = nullptr;
		nap::ParameterFloat*	mSink = nullptr;
		nap::SequenceEditor*	mEditor = nullptr;
		std::string				mTrackID;
		double					mDuration = 1.0;	///< total curve duration in seconds (set by generateCurve)

	protected:
		/** @return the raw 0..1 shape value, BEFORE [Min,Max] mapping. Base: the sink the curve engine
		 *  writes, i.e. Curve modulators are position- and component-invariant. Field modulators override
		 *  this (never value()). */
		virtual float rawValue(float pos01, int component) const;

		bool	mReleased = false;	///< gate released (onStop seen); reset by onTrigger
	};


	/**
	 * A spatial modulator: computed analytically per frame from position + time, never sampled from an
	 * authored napsequence curve. That is exactly why its inputs CAN be modulated -- nothing has to be
	 * re-authored when one changes. Curve modulators (ADSR/AD/LFO/Step) bake their shape into a real
	 * sequence track, so their shape params stay authored: changing one calls generateCurve(), which
	 * rewrites track segments and cannot run per frame.
	 */
	class NAPAPI FieldModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		/** @return every modulatable input, in display order. Each is a PatchParameter so the existing
		 *  mod-matrix (Modulator::mTargets) can target it with no new target type at all. */
		virtual std::vector<PatchParameter*> inputs() = 0;

	protected:
		/** @return input `in`'s 0..1 value mapped into [lo,hi]. Reads the parameter's authored base -- pass 1
		 *  of Patch::update has already written any driven value into it. Falls back to `lo` if unwired. */
		float inputValue(const nap::ResourcePtr<FloatParameter>& in, float lo, float hi) const
		{
			if (in == nullptr)
				return lo;
			const float t = in->mValue < 0.0f ? 0.0f : (in->mValue > 1.0f ? 1.0f : in->mValue);
			return lo + (hi - lo) * t;
		}
	};


	/** @return the normalised position of source `index` of `count`, using the modulator's own convention
	 *  (see Modulator::cyclicPositions). count <= 1 collapses to 0 -- which is exactly the case that used
	 *  to be called "Single" spread mode. */
	inline float positionOf(const Modulator& m, int index, int count)
	{
		if (count <= 1)
			return 0.0f;
		return m.cyclicPositions() ? static_cast<float>(index) / count
		                           : static_cast<float>(index) / (count - 1);
	}
}
