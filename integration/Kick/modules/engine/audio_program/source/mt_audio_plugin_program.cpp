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
// DRAFT for the follow-up PR (not compiled yet). See mt_audio_plugin_program.h.

#pragma once

#include "mt_audio_plugin_program.h"

namespace mt
{
    MTPluginProgram::MTPluginProgram( AudIO::DawEngine& Engine, juce::ValueTree& VT, juce::UndoManager* UM )
        : MTIProgram( VT, UM ), engine( Engine )
    {
        if( !creator() ) return; // no plugin engine compiled in

        auto pProg = creator()( Engine, getName(), this );
        if( pProg )
        {
            synth = pProg.get();
            Engine.createObject( std::move( pProg ) );
            synth->initAudioRouting();

            auto outBusId = engine.getBusManager()->getBusId( getOutput().toStdString() );
            synth->connectOutputTo( outBusId );
            synth->setGainsForSend( getSendGains() );

            onNameChanged = [&]() {
                synth->setName( getName().toStdString() );
            };

            onOutChanged = [&]() {
                auto outBusId = engine.getBusManager()->getBusId( getOutput().toStdString() );
                synth->connectOutputTo( outBusId );
            };

            onSendGainChanged = [&]( int Channel, float Value ) {
                synth->setGainForSend( (uint32_t)Channel, Value );
            };

            // Inserts...
            auto vt = VT.getOrCreateChildWithName( IDs::INSERTS, nullptr );
            MTIInsertsInterface::setup( vt, UM, synth );
        }
    }

    MTPluginProgram::~MTPluginProgram()
    {
        if( synth )
        {
            engine.removeObject( synth->getId() );
        }
    }
} // namespace mt
