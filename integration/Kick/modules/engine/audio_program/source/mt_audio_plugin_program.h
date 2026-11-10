/*
 * Copyright (c) 2026 FlowSphereAI
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
// DRAFT for the follow-up PR (not compiled yet): the data side of a Plugin
// program (MTProgramType::Plugin), mirroring MTDrumProgram. It stores its
// settings in the project's ValueTree, creates the audio object, and wires up
// routing, sends, inserts and metering like the other program types.
//
// audio_program can't include the objects module (objects depends on
// audio_program), so the synth engine registers a creator function at
// startup instead (see objects/drumsynth/drumSynthProgram.cpp).

#pragma once

namespace mt
{
    class MTPluginProgram : public MTIProgram
    {
    public:
        using SynthCreator = std::function<std::unique_ptr<ISynthObject>( AudIO::Design&, const juce::String&, MTIProgram* )>;

        // Called once at startup by the synth engine's module.
        static void setSynthCreator( SynthCreator Creator ) { creator() = std::move( Creator ); }

        explicit MTPluginProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM = nullptr );
        ~MTPluginProgram();

        // Metering....
        void               enableOutputMetering( bool Enable ) override { if( synth ) synth->enableOutputMetering( Enable ); }
        const MTMeterData& readOutMeter() const override
        {
            if( synth ) return synth->readOutMeter();
            return MTIProgram::readOutMeter(); // the base class's dummy meter
        }

        // IMidiMsgProcessor
        void noteOn( int Note, int Velocity ) override { if( synth ) synth->noteOn( Note, Velocity ); };
        void noteOff( int Note ) override { if( synth ) synth->noteOff( Note ); };
        void noteOff( std::vector<int>& Notes ) override { if( synth ) synth->noteOff( Notes ); };
        void noteOffAll() override { if( synth ) synth->noteOffAll(); };

        ISynthObject* getSynth() { return synth; }

    private:
        static SynthCreator& creator()
        {
            static SynthCreator c;
            return c;
        }

        AudIO::DawEngine& engine;
        ISynthObject*     synth{ nullptr };
    };

} // namespace MT
