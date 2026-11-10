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
// DRAFT for the follow-up PR (not compiled yet): the Analog '83 kit as a
// KICK audio object, so a Plugin program (MTProgramType::Plugin) can play it.
// Mirrors DrumProgram: an ISynthObject that KICK routes, meters and runs
// inserts on. It has no ISynthVoice voices (those play sample slices); the
// kit renders everything in doProcess().

#pragma once

namespace mt
{
    class DrumSynthProgram : public ISynthObject
    {
    public:
        DrumSynthProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* Program );
        ~DrumSynthProgram();

        void setProgram( MTIProgram* Program ) override;

        // Notes from KICK's sequencer and live pads. These can arrive from
        // several threads; Kit::trigger() is safe from any of them.
        void noteOn( int Note, int Velocity ) override;
        void noteOff( int ) override {} // one-shot drums: nothing to stop
        void noteOff( std::vector<int>& ) override {}
        void noteOffAll() override {}

        // For the Plugin program's edit page (parameters, presets, preview).
        a83::Kit&       getKit() { return *kit; }
        const a83::Kit& getKit() const { return *kit; }

    private:
        void initialize( uint32_t samplerate, uint32_t blocksize ) override;
        void doProcess( juce::AudioBuffer<float>& buffer ) override;

        std::unique_ptr<a83::Kit> kit;
    };
};
