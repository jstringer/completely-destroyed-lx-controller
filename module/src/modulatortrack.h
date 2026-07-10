#pragma once

// External Includes
#include <sequencetrack.h>

namespace lx
{
	/**
	 * A bare napsequence track that carries no segments. It exists only so the SequencePlayer's
	 * adapter factory (keyed by track type) fires for the modulator's ModulatorOutput; the value is
	 * computed procedurally in ModulatorAdapter::tick(), never read from segments.
	 */
	class NAPAPI ModulatorTrack : public nap::SequenceTrack
	{
		RTTI_ENABLE(nap::SequenceTrack)
	};
}
