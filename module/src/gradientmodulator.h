#pragma once

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * A colour ramp across the source list: Start -> End, offset by Phase and repeated every Period of the
	 * position axis. Purely analytic (no sequence track is sampled), so every input is modulatable -- an AD
	 * on Phase offsets the ramp along the rig like a chase; an LFO on Period breathes its scale.
	 *
	 * Endpoints are ColorParameters rather than raw floats specifically so they can be driven too.
	 * Repeats mirror instead of hard-cutting, so a Period below 1 never produces a seam.
	 */
	class NAPAPI GradientModulator : public FieldModulator
	{
		RTTI_ENABLE(FieldModulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;
		float rawValue(float pos01, int component) const override;
		std::vector<PatchParameter*> inputs() override;

		nap::ResourcePtr<ColorParameter>	mStartInput;	///< Property: 'StartInput' ramp start colour
		nap::ResourcePtr<ColorParameter>	mEndInput;		///< Property: 'EndInput' ramp end colour
		nap::ResourcePtr<FloatParameter>	mPhaseInput;	///< Property: 'PhaseInput' 0..1 of one period
		nap::ResourcePtr<FloatParameter>	mPeriodInput;	///< Property: 'PeriodInput' 0..1 -> [0.1, 4] spans

	private:
		// Self-accumulated, like NoiseModulator: the player exists only to gate trigger/stop.
		double	mElapsed = 0.0;
	};
}
