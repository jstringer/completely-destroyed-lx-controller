#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>

// Local Includes
#include "effectparameter.h"

namespace nap { class SequencePlayer; }

namespace lx
{
	enum class EModulatorBlend : int { Replace, Multiply, Add };
	enum class EModulatorClock : int { Standard, Independent };

	/**
	 * Base class for a modulator: owns (wired by lxcontrolService) a SequencePlayer + clock used as a
	 * timebase, a custom ModulatorOutput + a 0..1 sink ParameterFloat. Subtypes implement evaluate(),
	 * called from the adapter on the clock thread; the result is thread-hopped into the sink, and
	 * value() reads it back and maps it to [Min,Max]. Blended into the target EffectParameter by
	 * Effect::update().
	 */
	class NAPAPI Modulator : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		virtual bool init(nap::utility::ErrorState& errorState) override;

		virtual void onTrigger();
		virtual void onStop();
		/** @return the raw 0..1 modulator value at the given (player) time. Called on the clock thread. */
		virtual float evaluate(double time)		{ return 0.0f; }
		virtual bool isFinished() const			{ return !mHeld && !mReleasing; }

		/** @return the sink value mapped to [Min,Max]. Read on the main thread. */
		float value() const;

		std::string							mName;						///< Property: 'Name'
		nap::ResourcePtr<EffectParameter>	mTarget;					///< Property: 'Target'
		int									mTargetComponent = -1;		///< Property: 'TargetComponent' -1 = all
		float								mMin = 0.0f;				///< Property: 'Min'
		float								mMax = 1.0f;				///< Property: 'Max'
		EModulatorBlend						mBlend = EModulatorBlend::Replace;	///< Property: 'Blend'
		EModulatorClock						mClock = EModulatorClock::Standard;	///< Property: 'Clock'
		float								mClockFrequency = 1000.0f;	///< Property: 'ClockFrequency' (Independent clock, Hz)

		// Runtime, wired by lxcontrolService (non-serialized here; the objects themselves are serialized)
		nap::SequencePlayer*	mPlayer = nullptr;
		nap::ParameterFloat*	mSink = nullptr;

	protected:
		bool	mHeld = false;
		bool	mReleasing = false;
		double	mReleaseStartTime = 0.0;
		float	mReleaseFrom = 0.0f;
	};
}
