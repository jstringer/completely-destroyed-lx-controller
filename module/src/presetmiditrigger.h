#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <midievent.h>

// Local Includes
#include "effectlayer.h"
#include "miditriggercomponent.h"

namespace nap
{
	/**
	 * One row of a Preset's own note->effect trigger list: while the owning Preset is the
	 * currently-active one, MIDI note-on number 'Number' performs 'Action' on 'EffectLayer'.
	 * Scoped to a single Preset - the same note number can be reused by a different Preset to
	 * drive a completely different effect/action, since only the currently-active preset's own
	 * trigger list is ever consulted (see lxcontrolService::onMidiEvent).
	 *
	 * Runtime-authored only: created/edited/deleted live from the Presets tab via
	 * lxcontrolService::addPresetNoteMapping()/updatePresetNoteMapping()/removePresetNoteMapping(),
	 * never hand-declared in objects.json.
	 */
	class NAPAPI PresetMidiTrigger : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		virtual bool init(utility::ErrorState& errorState) override;

		MidiValue					mNumber = 0;							///< Property: 'Number' MIDI note-on number
		ResourcePtr<EffectLayer>	mEffectLayer;							///< Property: 'EffectLayer'
		EMidiTriggerAction			mAction = EMidiTriggerAction::Toggle;	///< Property: 'Action'
	};
}
