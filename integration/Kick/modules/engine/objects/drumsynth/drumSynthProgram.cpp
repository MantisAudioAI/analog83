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
// DRAFT for the follow-up PR (not compiled yet). See drumSynthProgram.h.

namespace mt
{
    DrumSynthProgram::DrumSynthProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* Program )
        : ISynthObject( D, 0 ) // no sample-slice voices; the kit is the voice engine
        , kit( std::make_unique<a83::Kit>() )
    {
        setName( Name.toStdString() );
        setProgram( Program );
    }

    DrumSynthProgram::~DrumSynthProgram()
    {
        setBypass( true );
    }

    void DrumSynthProgram::setProgram( MTIProgram* Program )
    {
        ISynthObject::setProgram( Program ); // required by ISynthObject
        setBypass( Program == nullptr );
    }

    void DrumSynthProgram::noteOn( int Note, int Velocity )
    {
        if( !CommonUtils::isValidMidiNote( Note ) ) return;
        // Multi mode: GM drum map (36 kick, 42/44 closed hat, 46 open hat).
        // Single mode: one voice played chromatically.
        kit->trigger( Note, juce::jlimit( 0.0f, 1.0f, (float)Velocity / 127.0f ) );
    }

    void DrumSynthProgram::initialize( uint32_t samplerate, uint32_t /*blocksize*/ )
    {
        kit->prepare( (double)samplerate ); // allocates; runs from prepareToPlay, not the audio callback
    }

    void DrumSynthProgram::doProcess( juce::AudioBuffer<float>& buffer )
    {
        // ISynthObject has already cleared the buffer; the kit ADDS into it.
        // ISynthObject then runs inserts, the program's gain and pan, and metering.
        if( buffer.getNumChannels() < 2 ) return; // programs are stereo buses
        kit->process( buffer.getWritePointer( 0 ), buffer.getWritePointer( 1 ), buffer.getNumSamples() );
    }

    // Lets MTPluginProgram (in audio_program, which can't include this module)
    // create the kit. Runs once at startup.
    [[maybe_unused]] static const bool drumSynthRegistered = []() {
        MTPluginProgram::setSynthCreator( []( AudIO::Design& D, const juce::String& Name, MTIProgram* Program ) {
            return std::unique_ptr<ISynthObject>( std::make_unique<DrumSynthProgram>( D, Name, Program ) );
        } );
        return true;
    }();
};
