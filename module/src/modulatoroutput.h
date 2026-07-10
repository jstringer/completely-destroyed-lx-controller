#pragma once

// External Includes
#include <sequenceplayeroutput.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>
#include <rtti/factory.h>
#include <vector>

namespace nap { class SequenceService; }

namespace lx
{
	class Modulator;
	class ModulatorAdapter;

	/**
	 * Custom napsequence output for a procedural modulator. Holds the 0..1 sink ParameterFloat that
	 * the modulator's value is written into, and a back-pointer to the owning Modulator (whose
	 * evaluate() the adapter calls each tick). Its update() runs on the main thread (from the
	 * SequenceService) and flushes each adapter's thread-stored value into the sink parameter.
	 */
	class NAPAPI ModulatorOutput : public nap::SequencePlayerOutput
	{
		RTTI_ENABLE(nap::SequencePlayerOutput)
	public:
		ModulatorOutput(nap::SequenceService& service);

		nap::ResourcePtr<nap::ParameterFloat>	mParameter;			///< Property: 'Parameter' the 0..1 sink

		Modulator*		mModulator = nullptr;	///< Runtime back-pointer to the owning modulator (non-serialized)

		void registerAdapter(ModulatorAdapter* adapter);
		void removeAdapter(ModulatorAdapter* adapter);

	protected:
		void update(double deltaTime) override;

	private:
		std::vector<ModulatorAdapter*> mAdapters;
	};

	using ModulatorOutputObjectCreator = nap::rtti::ObjectCreator<ModulatorOutput, nap::SequenceService>;
}
