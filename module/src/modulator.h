#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>
#include <string>

// Local Includes
#include "effectparameter.h"

namespace nap
{
	class SequencePlayer;
	class SequenceEditor;
	class lxcontrolService;
}

namespace lx
{
	enum class EModulatorBlend : int { Replace, Multiply, Add };

	/**
	 * Base class for a modulator: a shape that drives a 0..1 value over time and blends it into a target
	 * EffectParameter. The value comes from a real napsequence curve track (authored by generateCurve()
	 * via lxcontrolService::authorFloatCurve) that the stock SequencePlayerCurveAdapter samples off the
	 * SequencePlayer's own time into a sink ParameterFloat. Gate/trigger events map onto the player
	 * transport (setIsPlaying/setPlayerTime/setIsPaused/setIsLooping/setPlaybackSpeed) in onTrigger()/
	 * onStop()/update(). value() reads the sink back and maps it to [Min,Max]; Effect::update() blends it.
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

		/** @return the sink value mapped to [Min,Max]. Read on the main thread. */
		float value() const;

		/** @return this modulator's value for fixture slot `slot` (see Effect::mTargetMode). Base:
		 *  broadcasts the same value() to every slot; multi-fixture types (Chase, Noise) override. */
		virtual float valueForSlot(int slot) const	{ return value(); }
		/** Tells this modulator how many fixture slots it's driving (Effect::mFixtureCount, or 1 for
		 *  Single mode). Base: no-op; multi-fixture types cache it for use in valueForSlot(). */
		virtual void setSlotCount(int count)			{ }

		std::string							mName;						///< Property: 'Name'
		nap::ResourcePtr<EffectParameter>	mTarget;					///< Property: 'Target'
		int									mTargetComponent = -1;		///< Property: 'TargetComponent' -1 = all
		float								mMin = 0.0f;				///< Property: 'Min'
		float								mMax = 1.0f;				///< Property: 'Max'
		EModulatorBlend						mBlend = EModulatorBlend::Replace;	///< Property: 'Blend'

		// Runtime, wired by lxcontrolService::buildModulatorGraph (non-serialized; the objects are).
		nap::SequencePlayer*	mPlayer = nullptr;
		nap::ParameterFloat*	mSink = nullptr;
		nap::SequenceEditor*	mEditor = nullptr;
		std::string				mTrackID;
		double					mDuration = 1.0;	///< total curve duration in seconds (set by generateCurve)

	protected:
		bool	mReleased = false;	///< gate released (onStop seen); reset by onTrigger
	};
}
