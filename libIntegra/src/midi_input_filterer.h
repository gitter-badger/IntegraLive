/* libIntegra modular audio framework
 *
 * Copyright (C) 2007 Birmingham City University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, 
 * USA.
 */


#ifndef INTEGRA_MIDI_INPUT_FILTERER_H
#define INTEGRA_MIDI_INPUT_FILTERER_H

#include "api/common_typedefs.h"
#include "threaded_queue.h"
#include "midi_engine.h"

using namespace integra_api;



namespace integra_internal
{
	class CMidiInputFilterer
	{
		public:

			CMidiInputFilterer();
			~CMidiInputFilterer();

			/*
			 walks the input arrays, marking as 'filtered' any poly_pressure, cc, channel pressure and pitchbend messages 
			 are replaced by subsequent messages in the same input array
			*/
			void filter_input( midi_input_buffer_array &input_buffers );

		private:

			/* 
			 These could just be local variables, but it's more efficient not to allocate them 
			 each time filter_input is called 
			*/
			bool *m_got_poly_pressure;
			bool *m_got_control_change;
			bool *m_got_channel_pressure;
			bool *m_got_pitchbend;

			static const int number_of_midi_channels = 16;
			static const int number_of_notes = 128;
	};
}



#endif /* INTEGRA_MIDI_INPUT_FILTERER_H */
