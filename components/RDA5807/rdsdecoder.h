/*
 * Copyright (C) 2014 Bastian Bloessl <bloessl@ccs-labs.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef INCLUDED_RDSDECODER_H
#define INCLUDED_RDSDECODER_H

void dout(const char* fmt, ...);
void log(const char* fmt, ...);

void rdsdecoder_parse(unsigned int* group);

void rdsdecoder_reset();
void rdsdecoder_send_message(long, std::string&);
double rdsdecoder_decode_af(unsigned int);
void rdsdecoder_decode_optional_content(int, unsigned long int *);

void rdsdecoder_decode_type0( unsigned int* group, bool B);
void rdsdecoder_decode_type1( unsigned int* group, bool B);
void rdsdecoder_decode_type2( unsigned int* group, bool B);
void rdsdecoder_decode_type3( unsigned int* group, bool B);
void rdsdecoder_decode_type4( unsigned int* group, bool B);
void rdsdecoder_decode_type5( unsigned int* group, bool B);
void rdsdecoder_decode_type6( unsigned int* group, bool B);
void rdsdecoder_decode_type7( unsigned int* group, bool B);
void rdsdecoder_decode_type8( unsigned int* group, bool B);
void rdsdecoder_decode_type9( unsigned int* group, bool B);
void rdsdecoder_decode_type10(unsigned int* group, bool B);
void rdsdecoder_decode_type11(unsigned int* group, bool B);
void rdsdecoder_decode_type12(unsigned int* group, bool B);
void rdsdecoder_decode_type13(unsigned int* group, bool B);
void rdsdecoder_decode_type14(unsigned int* group, bool B);
void rdsdecoder_decode_type15(unsigned int* group, bool B);

unsigned int   rdsdecoder_program_identification;
unsigned char  rdsdecoder_program_type;
unsigned char  rdsdecoder_pi_country_identification;
unsigned char  rdsdecoder_pi_area_coverage;
unsigned char  rdsdecoder_pi_program_reference_number;
char           rdsdecoder_radiotext[65];
char           rdsdecoder_program_service_name[9];
bool           rdsdecoder_radiotext_AB_flag;
bool           rdsdecoder_traffic_program;
bool           rdsdecoder_traffic_announcement;
bool           rdsdecoder_music_speech;
bool           rdsdecoder_mono_stereo;
bool           rdsdecoder_artificial_head;
bool           rdsdecoder_compressed;
bool           rdsdecoder_static_pty;
bool           rdsdecoder_debug;
bool           rdsdecoder_log;
unsigned char  rdsdecoder_pty_locale;
char           rdsdecoder_af_string[25];
char           message[7][65];

#endif /* INCLUDED_RDSDECODER_H */

