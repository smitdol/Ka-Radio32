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
/*
parts from https://github.com/csdexter/RDSDecoder/blob/master/RDSDecoder.cpp
https://goughlui.com/2016/10/09/experiment-sydney-wfm-broadcast-rds-tmc-rt-acs-audio
usefull bibliography https://github.com/mmassaki/tcc-kzsh/tree/master/bibliography/iso%2014819
*/
 
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "rdsdecoder.h"
#include "constants.h"
#include "tmc_events.h"
#include <math.h>

void dout(const char* format, ...) {
    va_list argptr;
    va_start(argptr, format);
    printf(format, argptr); //  The Print stream is configured to the UART0 of the ESP32. ?
    va_end(argptr);
}

void LOG(const char* format, ...) {
    va_list argptr;
    va_start(argptr, format);
    printf(format, argptr); //  The Print stream is configured to the UART0 of the ESP32. ?
    va_end(argptr);
}

void rdsdecoder_reset() {

	memset(rdsdecoder_radiotext, ' ', sizeof(rdsdecoder_radiotext));
	memset(rdsdecoder_program_service_name, '.', sizeof(rdsdecoder_program_service_name));
	memset(rdsdecoder_af_string,' ', sizeof(rdsdecoder_af_string));
	memset(rdsdecoder_tmcprovider,0, sizeof(rdsdecoder_tmcprovider));
	for (int i=0; i < 6; i++ ){
		memset(message[i], ' ', sizeof(message[i]));
        newmessage[i]=false;
	}

	rdsdecoder_radiotext_AB_flag              = 0;
	rdsdecoder_traffic_program                = false;
	rdsdecoder_traffic_announcement           = false;
	rdsdecoder_music_speech                   = false;
	rdsdecoder_program_type                   = 0;
	rdsdecoder_pi_country_identification      = 0;
	rdsdecoder_pi_area_coverage               = 0;
	rdsdecoder_pi_program_reference_number    = 0;
	rdsdecoder_mono_stereo                    = false;
	rdsdecoder_artificial_head                = false;
	rdsdecoder_compressed                     = false;
	rdsdecoder_static_pty                     = false;
}

/* type 0 = PI
 * type 1 = PS
 * type 2 = PTY
 * type 3 = flagstring: TP, TA, MuSp, MoSt, AH, CMP, stPTY
 * type 4 = RadioText 
 * type 5 = ClockTime
 * type 6 = Alternative Frequencies */
void rdsdecoder_send_message(long msgtype, const char* msgtext) {
    strcpy(message[msgtype], msgtext);
    makePrintable(message[msgtype]);
    newmessage[msgtype]=true;
}


/* BASIC TUNING: see page 21 of the standard */
void rdsdecoder_decode_type0(short unsigned int *group, bool B) {
	unsigned int af_code_1 = 0;
	unsigned int af_code_2 = 0;
	unsigned int  no_af    = 0;
	double af_1            = 0;
	double af_2            = 0;
	char flagstring[8]     = "0000000";
	
	rdsdecoder_traffic_program        = (group[1] >> 10) & 0x01;       // "TP"
	rdsdecoder_traffic_announcement   = (group[1] >>  4) & 0x01;       // "TA"
	rdsdecoder_music_speech           = (group[1] >>  3) & 0x01;       // "MuSp"

	bool decoder_control_bit      = (group[1] >> 2) & 0x01; // "DI"
	unsigned char segment_address =  group[1] & 0x03;       // "DI segment"

	rdsdecoder_program_service_name[segment_address * 2]     = (group[3] >> 8) & 0xff;
	rdsdecoder_program_service_name[segment_address * 2 + 1] =  group[3]       & 0xff;

	/* see page 41, table 9 of the standard */
	switch (segment_address) {
		case 0:
			rdsdecoder_mono_stereo=decoder_control_bit;
		break;
		case 1:
			rdsdecoder_artificial_head=decoder_control_bit;
		break;
		case 2:
			rdsdecoder_compressed=decoder_control_bit;
		break;
		case 3:
			rdsdecoder_static_pty=decoder_control_bit;
		break;
		default:
		break;
	}
	flagstring[0] = rdsdecoder_traffic_program        ? '1' : '0';
	flagstring[1] = rdsdecoder_traffic_announcement   ? '1' : '0';
	flagstring[2] = rdsdecoder_music_speech           ? '1' : '0';
	flagstring[3] = rdsdecoder_mono_stereo            ? '1' : '0';
	flagstring[4] = rdsdecoder_artificial_head        ? '1' : '0';
	flagstring[5] = rdsdecoder_compressed             ? '1' : '0';
	flagstring[6] = rdsdecoder_static_pty             ? '1' : '0';
	

	if(!B) { // type 0A contains alternative frequencies in group[2], 0B repeats PI code in group[2]
		af_code_1 = ((int)group[2] >> 8) & 0xff;
		af_code_2 = ((int)group[2])      & 0xff;
		af_1 = rdsdecoder_decode_af(af_code_1);
		af_2 = rdsdecoder_decode_af(af_code_2);

		if(af_1) {
			no_af += 1;
		}
		if(af_2) {
			no_af += 2;
		}

		char af1_string[10];
		char af2_string[10];
		/* only AF1 => no_af==1, only AF2 => no_af==2, both AF1 and AF2 => no_af==3 */
		if(no_af) {
			if(af_1 > 80e3) {
				snprintf(af1_string, 10, "%2.2fMHz", (af_1/1e3));
			} else if((af_1<2e3)&&(af_1>100)) {
				snprintf(af1_string, 10, "%ikHz", (int)af_1);
			}
			if(af_2 > 80e3) {
				snprintf(af2_string, 10, "%2.2fMHz", (af_2/1e3));
			} else if ((af_2 < 2e3) && (af_2 > 100)) {
				snprintf(af2_string, 10, "%ikHz", (int)af_2);
			}
		}
		if(no_af == 1) {
			strcpy(rdsdecoder_af_string, af1_string);
		} else if(no_af == 2) {
			strcpy(rdsdecoder_af_string, af2_string);
		} else if(no_af == 3) {
			snprintf(rdsdecoder_af_string, 25, "%s, %s" , af1_string , af2_string);
		}
	}

	LOG( "==> %.*s" , 8, rdsdecoder_program_service_name, 8);
	LOG( "<== - %s" , (rdsdecoder_traffic_program ? "TP" : "  "));
	LOG( " - %s " , (rdsdecoder_traffic_announcement ? "TA" : "  "));
	LOG( " - %s " , (rdsdecoder_music_speech ? "Music" : "Speech"));
	LOG( " - %s " , (rdsdecoder_mono_stereo ? "MONO" : "STEREO"));
	LOG( " - AF: %s\n" , rdsdecoder_af_string );

	rdsdecoder_send_message(1, rdsdecoder_program_service_name);
	rdsdecoder_send_message(3, flagstring);
	rdsdecoder_send_message(6, rdsdecoder_af_string);
	
}

double rdsdecoder_decode_af(unsigned int af_code) {
	static bool vhf_or_lfmf             = 0; // 0 = vhf, 1 = lf/mf
	double alt_frequency                = 0; // in kHz

	if((af_code == 0) ||                              // not to be used
		( af_code == 205) ||                      // filler code
		((af_code >= 206) && (af_code <= 223)) || // not assigned
		( af_code == 224) ||                      // No AF exists
		( af_code >= 251)) {                      // not assigned
			alt_frequency   = 0;
	}
	if((af_code >= 225) && (af_code <= 249)) {        // VHF frequencies follow
		alt_frequency   = 0;
		vhf_or_lfmf     = 1;
	}
	if(af_code == 250) {                              // an LF/MF frequency follows
		alt_frequency   = 0;
		vhf_or_lfmf     = 0;
	}

	if((af_code > 0) && (af_code < 205) && vhf_or_lfmf)
		alt_frequency = 100.0 * (af_code + 875);          // VHF (87.6-107.9MHz)
	else if((af_code > 0) && (af_code < 16) && !vhf_or_lfmf)
		alt_frequency = 153.0 + (af_code - 1) * 9;        // LF (153-279kHz)
	else if((af_code > 15) && (af_code < 136) && !vhf_or_lfmf)
		alt_frequency = 531.0 + (af_code - 16) * 9 + 531; // MF (531-1602kHz)

	return alt_frequency;
}

void rdsdecoder_decode_type1(short unsigned int *group, bool B){
	int ecc    = 0;
	int paging = 0;
	char country_code           = (group[0] >> 12) & 0x0f;
	char radio_paging_codes     =  group[1]        & 0x1f;
	int variant_code            = (group[2] >> 12) & 0x7;
	unsigned int slow_labelling =  group[2]        & 0xfff;
	int day    = (int)((group[3] >> 11) & 0x1f);
	int hour   = (int)((group[3] >>  6) & 0x1f);
	int minute = (int) (group[3]        & 0x3f);

	if(radio_paging_codes) {
		LOG( "paging codes: %i " , ((int)radio_paging_codes));
	}
	if(day || hour || minute) {
		LOG( "program item: %id, %i, %i ", day , hour , minute);
	}

	if(!B){ // 1A contains labeling codes in group[2], 1B repeats PI code in group[2]
		switch(variant_code){
			case 0: // paging + ecc
				paging = (slow_labelling >> 8) & 0x0f;
				ecc    =  slow_labelling       & 0xff;
				if(paging) {
					LOG( "paging: %i " , paging );
				}
				if((ecc > 223) && (ecc < 229)) {
					LOG( "extended country code: %s\n",
						pi_country_codes[country_code-1][ecc-224]);
				} else {
					LOG( "invalid extended country code: %i\n" , ecc );
				}
				break;
			case 1: // TMC identification
				LOG( "TMC identification code received\n" );
				break;
			case 2: // Paging identification
				LOG( "Paging identification code received" );
				break;
			case 3: // language codes
				if(slow_labelling < 44) {
					LOG( "language: %s" , language_codes[slow_labelling] );
				} else {
					LOG( "language: invalid language code %s" , slow_labelling );
				}
				break;
			default:
				break;
		}
	}
}

void rdsdecoder_decode_type2(short unsigned int *group, bool B){
	unsigned char text_segment_address_code = group[1] & 0x0f;

	// when the A/B flag is toggled, flush your current radiotext
    bool abFlag = ((group[1] >> 4) & 0x01);
	if(rdsdecoder_radiotext_AB_flag != abFlag) {
		memset(rdsdecoder_radiotext, ' ', sizeof(rdsdecoder_radiotext));
        rdsdecoder_radiotext_AB_flag = abFlag;	
    }
	if(!B) {
		rdsdecoder_radiotext[text_segment_address_code * 4    ] = (group[2] >> 8) & 0xff;
		rdsdecoder_radiotext[text_segment_address_code * 4 + 1] =  group[2]       & 0xff;
		rdsdecoder_radiotext[text_segment_address_code * 4 + 2] = (group[3] >> 8) & 0xff;
		rdsdecoder_radiotext[text_segment_address_code * 4 + 3] =  group[3]       & 0xff;
	} else {
		rdsdecoder_radiotext[text_segment_address_code * 2    ] = (group[3] >> 8) & 0xff;
		rdsdecoder_radiotext[text_segment_address_code * 2 + 1] =  group[3]       & 0xff;
	}
	LOG( "Radio Text %s:%.*s" , (rdsdecoder_radiotext_AB_flag ? 'B' : 'A'),
		sizeof(rdsdecoder_radiotext), rdsdecoder_radiotext );
	rdsdecoder_send_message(4, rdsdecoder_radiotext);
}

void rdsdecoder_decode_type3(short unsigned int *group, bool B){
	if(B) { // Open data application
		dout("type 3B not implemented yet" );
		return;
	}

	int application_group = (group[1] >> 1) & 0xf;
	int group_type        =  group[1] & 0x1;
	int message           =  group[2];
	int aid               =  group[3]; //application identifier
	
	LOG( "aid group: %i %s" , application_group , (group_type ? 'B' : 'A'));
	if((application_group == 8) && (group_type == false)) { // 8A
		int variant_code = (message >> 14) & 0x3;
		if(variant_code == 0) {
			int ltn  = (message >> 6) & 0x3f; // location table number
			bool afi = (message >> 5) & 0x1;  // alternative freq. indicator
			bool M   = (message >> 4) & 0x1;  // mode of transmission
			bool I   = (message >> 3) & 0x1;  // international
			bool N   = (message >> 2) & 0x1;  // national
			bool R   = (message >> 1) & 0x1;  // regional
			bool U   =  message       & 0x1;  // urban
			LOG( "location table: %i - %s - %s - %s - %s - %s - %s - aid %i " , ltn,
				(afi ? "AFI-ON" : "AFI-OFF") 
				, (M   ? "enhanced mode" : "basic mode")
				, (I   ? "international " : "")
				, (N   ? "national " : "")
				, (R   ? "regional " : "")
				, (U   ? "urban" : "")
				, aid );

		} else if(variant_code==1) {
			int G   = (message >> 12) & 0x3;  // gap
			int sid = (message >>  6) & 0x3f; // service identifier
			int gap_no[4] = {3, 5, 8, 11};
			LOG( "gap: %i  groups, SID: %i" , gap_no[G] , sid );;
		}
	}
	LOG( "message: %s - aid %i" , message , aid );
	if (aid == 0xCD46 ) {
		//ALERT-C
	} else if (aid == 0x0D45) {
		//ALERT-C
	}
}

void rdsdecoder_decode_type4(short unsigned int *group, bool B){
	if(B) { // Open data application
		dout ( "type 4B not implemented yet" );
		return;
	}

	unsigned int hours   = ((group[2] & 0x1) << 4) | ((group[3] >> 12) & 0x0f);
	unsigned int minutes =  (group[3] >> 6) & 0x3f;
	double local_time_offset = .5 * (group[3] & 0x1f);

	if((group[3] >> 5) & 0x1) {
		local_time_offset *= -1;
	}
	double modified_julian_date = ((group[1] & 0x03) << 15) | ((group[2] >> 1) & 0x7fff);

	unsigned int year  = (int)((modified_julian_date - 15078.2) / 365.25);
	unsigned int month = (int)((modified_julian_date - 14956.1 - (int)(year * 365.25)) / 30.6001);
	unsigned int day   =        modified_julian_date - 14956 - (int)(year * 365.25) - (int)(month * 30.6001);
	bool K = ((month == 14) || (month == 15)) ? 1 : 0;
	year += K;
	month -= 1 + K * 12;

	char time[25];
	snprintf(time, sizeof(time), "%02i.%02i.%4i, %02i:%02i (%+.1fh)"
		, day , month , (1900 + year) , hours , minutes , local_time_offset);
	LOG( "Clocktime: " , time );

	rdsdecoder_send_message(5,time);
}

void rdsdecoder_decode_type5(short unsigned int *group, bool B){
	 //Transparent data channels or ODA
	 dout( "type 5 not implemented yet" );
}

void rdsdecoder_decode_type6(short unsigned int *group, bool B){
	//In-house applications or ODA
	dout ( "type 6 not implemented yet" );
}

void rdsdecoder_decode_type7(short unsigned int *group, bool B){
	//Radio Paging or ODA
	dout ( "type 7 not implemented yet" );
}

void rdsdecoder_decode_type8(short unsigned int *group, bool B){
	if(B) { //ODA
		dout ( "type 8B not implemented yet" );
		return;
	}
	//Traffic Message Channel 
	bool T = (group[1] >> 4) & 0x1; // 0 = user message, 1 = tuning info
	bool F = (group[1] >> 3) & 0x1; // 0 = multi-group, 1 = single-group
	bool D = (group[2] > 15) & 0x1; // 1 = diversion recommended
	static unsigned long int free_format[4];
	static int no_groups = 0;
	bool encrypted = (group[1] & 0x1f) == 0x00;
	
	if (encrypted){
		LOG( "#encrypted TMC# ");
		bool encryptedAdministrationGroup = (group[2] & 0x3800) == 0x00;
		if (encryptedAdministrationGroup) {
			unsigned int encid =  group[2]        & 0x1f; // 5 bits
			unsigned int sid   = (group[2] >>  5) & 0x3f; // 6 bits
			unsigned int test  = (group[2] >> 11) & 0x03; // 2 bits; defined in 14819_6
			unsigned int ltnbe = (group[3] >> 10) & 0x3f; // location table before encryption
			unsigned int fuzzy =  group[3]        & 0x3ff; // 10 bits
			LOG("encid %i, sid %i, test %i, ltnbe %i, fuzzy %i", encid, sid, test, ltnbe, fuzzy);
		} else {
			//reserved for future use
		}
	} else if(T) { // tuning info
		LOG( "#tuning info# ");
		int variant = group[1] & 0xf;
		if((variant > 3) && (variant < 10)) {
			LOG( "variant: %i - %i %i" , variant , group[2] , group[3] );
			if ( variant == 4) {
				rdsdecoder_tmcprovider[0] = (group[2] >> 8) & 0xff;
				rdsdecoder_tmcprovider[1] =  group[2]       & 0xff;
				rdsdecoder_tmcprovider[2] = (group[3] >> 8) & 0xff;
				rdsdecoder_tmcprovider[3] =  group[3]       & 0xff;
			} else if (variant == 5) {
				rdsdecoder_tmcprovider[4] = (group[2] >> 8) & 0xff;
				rdsdecoder_tmcprovider[5] =  group[2]       & 0xff;
				rdsdecoder_tmcprovider[6] = (group[3] >> 8) & 0xff;
				rdsdecoder_tmcprovider[7] =  group[3]       & 0xff;
			}
			LOG( "tmc provider %s" , rdsdecoder_tmcprovider );
		} else {
			LOG( "invalid variant: %i" , variant );
		}

	} else if(F || D) { // T=0: User message; single-group or 1st of multi-group
		unsigned int dp_ci    =  group[1]        & 0x7;   // duration & persistence or continuity index
		bool sign             = (group[2] >> 14) & 0x1;   // event direction, 0 = +, 1 = -
		unsigned int extent   = (group[2] >> 11) & 0x7;   // number of segments affected
		unsigned int event    =  group[2]        & 0x7ff; // event code, defined in ISO 14819-2
		unsigned int location =  group[3];                // location code, defined in ISO 14819-3
		LOG( "#user msg# %s" , (D ? "diversion recommended, " : ""));
		if(F) {
			LOG( "single-grp, duration: %s" , tmc_duration[dp_ci][0]);
		} else {
			LOG( "multi-grp, continuity index: %i" , dp_ci);
		}
		int event_line = tmc_event_code_index[event][1];
		LOG( ", extent: %s%i segments, event:%s, location:" , (sign ? "-" : "") , extent + 1 ,
			 event , tmc_events[event_line][1]
			, location );

	} else { // 2nd or more of multi-group
		unsigned int ci = group[1] & 0x7;          // countinuity index
		bool sg = (group[2] >> 14) & 0x1;          // second group
		unsigned int gsi = (group[2] >> 12) & 0x3; // group sequence
		LOG( "#user msg# multi-grp, continuity index: %i %s, gsi:%i" , ci, (sg ? ", second group" : "") , gsi);
		LOG( ", free format: %i %i" , (group[2] & 0xfff) , group[3] );
		// it's not clear if gsi=N-2 when gs=true
		if(sg) {
			no_groups = gsi;
		}
		free_format[gsi] = ((group[2] & 0xfff) << 12) | group[3];
		if(gsi == 0) {
			rdsdecoder_decode_optional_content(no_groups, free_format);
		}
	}
}

void rdsdecoder_decode_optional_content(int no_groups, unsigned long int *free_format){
	int label          = 0;
	int content        = 0;
	int content_length = 0;
	int ff_pointer     = 0;
	
	for (int i = no_groups; i == 0; i--){
		ff_pointer = 12 + 16;
		while(ff_pointer > 0){
			ff_pointer -= 4;
			label = (free_format[i] && (0xf << ff_pointer));
			content_length = optional_content_lengths[label];
			ff_pointer -= content_length;
			content = (free_format[i] && ((int)(pow(2, content_length) - 1) << ff_pointer));
			LOG( "TMC optional content (%s):%s" , label_descriptions[label], content );
		}
	}
}

void rdsdecoder_decode_type9(short unsigned int *group, bool B){
	// Emergency warning systems or ODA
	dout ( "type 9 not implemented yet" );
}

void rdsdecoder_decode_type10(short unsigned int *group, bool B){
	// Programme Type Name (Group type 10A) and Open data (Group type 10B)
	dout ( "type 10 not implemented yet" );
}

void rdsdecoder_decode_type11(short unsigned int *group, bool B){
	//Open Data Application
	dout ( "type 11 not implemented yet" );
}

void rdsdecoder_decode_type12(short unsigned int *group, bool B){
	//Open Data Application
	dout ( "type 12 not implemented yet" );
}

void rdsdecoder_decode_type13(short unsigned int *group, bool B){
	//Enhanced Radio Paging or ODA
	dout ( "type 13 not implemented yet" );
}

void rdsdecoder_decode_type14(short unsigned int *group, bool B){
	//Enhanced Other Networks information
	
	bool tp_on               = (group[1] >> 4) & 0x01;
	char variant_code        = group[1] & 0x0f;
	unsigned int information = group[2];
	unsigned int pi_on       = group[3];
	
	char pty_on = 0;
	bool ta_on = 0;
	static char ps_on[8] = {' ',' ',' ',' ',' ',' ',' ',' '};
	double af_1 = 0;
	double af_2 = 0;
	
	if (!B){
		switch (variant_code){
			case 0: // PS(ON)
			case 1: // PS(ON)
			case 2: // PS(ON)
			case 3: // PS(ON)
				ps_on[variant_code * 2    ] = (information >> 8) & 0xff;
				ps_on[variant_code * 2 + 1] =  information       & 0xff;
				LOG( "PS(ON): ==> %.*s <==" , 8, ps_on) ;
			break;
			case 4: // AF
				af_1 = 100.0 * (((information >> 8) & 0xff) + 875);
				af_2 = 100.0 * ((information & 0xff) + 875);
				LOG( "AF:%3.2fMHz %3.2fMHz" , (af_1/1000) , (af_2/1000));
			break;
			case 5: // mapped frequencies
			case 6: // mapped frequencies
			case 7: // mapped frequencies
			case 8: // mapped frequencies
				af_1 = 100.0 * (((information >> 8) & 0xff) + 875);
				af_2 = 100.0 * ((information & 0xff) + 875);
				LOG( "TN:%3.2fMHz - ON:%3.2fMHz" , (af_1/1000) , (af_2/1000));
			break;
			case 9: // mapped frequencies (AM)
				af_1 = 100.0 * (((information >> 8) & 0xff) + 875);
				af_2 = 9.0 * ((information & 0xff) - 16) + 531;
				LOG( "TN:%3.2fMHz - ON:%ikHz" , (af_1/1000) , (int)(af_2));
			break;
			case 10: // unallocated
			break;
			case 11: // unallocated
			break;
			case 12: // linkage information
				LOG( "Linkage information: %x%x"
					, ((information >> 8) & 0xff) , (information & 0xff));
			break;
			case 13: // PTY(ON), TA(ON)
				ta_on = information & 0x01;
				pty_on = (information >> 11) & 0x1f;
				LOG( "PTY(ON): %s%s" , pty_table[(int)(pty_on)][rdsdecoder_pty_locale], (ta_on ?" - TA": ""));
			break;
			case 14: // PIN(ON)
				LOG( "PIN(ON):%x%x", ((information >> 8) & 0xff) , (information & 0xff));
			break;
			case 15: // Reserved for broadcasters use
			break;
			default:
				dout ( "invalid variant code: %i" , variant_code);
			break;
		}
	}
	if (pi_on){
		LOG( " PI(ON): %i %s\n" , pi_on, (tp_on ? "-TP-" : ""));
	}
}

void rdsdecoder_decode_type15(unsigned short *group, bool B){
	// Fast basic tuning and switching information
	dout ( "type 15 not implemented yet" );
}

void rdsdecoder_parse(unsigned short* group) {
	// TODO: verify offset chars are one of: "ABCD", "ABcD", "EEEE" (in US)

	unsigned int group_type = (unsigned int)((group[1] >> 12) & 0xf);
	bool ab = (group[1] >> 11 ) & 0x1;

	LOG( "%02i%c ", group_type , (ab ? 'B' :'A'));
	LOG( "(%s)" , rds_group_acronyms[group_type]);

	rdsdecoder_program_identification = group[0];     // "PI"
	rdsdecoder_program_type = (group[1] >> 5) & 0x1f; // "PTY"
	int pi_country_identification = (rdsdecoder_program_identification >> 12) & 0xf;
	rdsdecoder_pi_area_coverage = (rdsdecoder_program_identification >> 8) & 0xf;
	rdsdecoder_pi_program_reference_number = rdsdecoder_program_identification & 0xff;
	char pistring[10];
	snprintf(pistring, sizeof(pistring), "%04X", rdsdecoder_program_identification);
	rdsdecoder_send_message(0, pistring);
	rdsdecoder_send_message(2, pty_table[rdsdecoder_program_type][rdsdecoder_pty_locale]);

	LOG( " - PI: %s - PTY: %s (country: %s/%s/%s/%s/%s, area: %s, program: %s)\n", pistring, pty_table[rdsdecoder_program_type][rdsdecoder_pty_locale],
		pi_country_codes[pi_country_identification - 1][0],
		pi_country_codes[pi_country_identification - 1][1],
		pi_country_codes[pi_country_identification - 1][2],
		pi_country_codes[pi_country_identification - 1][3],
		pi_country_codes[pi_country_identification - 1][4],
		coverage_area_codes[rdsdecoder_pi_area_coverage],
		(int)rdsdecoder_pi_program_reference_number);

	switch (group_type) {
		case 0:
			rdsdecoder_decode_type0(group, ab);
			break;
		case 1:
			rdsdecoder_decode_type1(group, ab);
			break;
		case 2:
			rdsdecoder_decode_type2(group, ab);
			break;
		case 3:
			rdsdecoder_decode_type3(group, ab);
			break;
		case 4:
			rdsdecoder_decode_type4(group, ab);
			break;
		case 5:
			rdsdecoder_decode_type5(group, ab);
			break;
		case 6:
			rdsdecoder_decode_type6(group, ab);
			break;
		case 7:
			rdsdecoder_decode_type7(group, ab);
			break;
		case 8:
			rdsdecoder_decode_type8(group, ab);
			break;
		case 9:
			rdsdecoder_decode_type9(group, ab);
			break;
		case 10:
			rdsdecoder_decode_type10(group, ab);
			break;
		case 11:
			rdsdecoder_decode_type11(group, ab);
			break;
		case 12:
			rdsdecoder_decode_type12(group, ab);
			break;
		case 13:
			rdsdecoder_decode_type13(group, ab);
			break;
		case 14:
			rdsdecoder_decode_type14(group, ab);
			break;
		case 15:
			rdsdecoder_decode_type15(group, ab);
			break;
	}

	dout ( " %04X %04X %04X %04X\n" , group[0], group[1], group[2], group[3]);
}

//makeprintable from https://github.com/csdexter/RDSDecoder
#define pgm_read_byte(x) (char)(*x)

const char RDS2LCD_S[] = "\xE1\xE0\xE9\xE8\xED\xEE\xF3\xF2\xFA\xF9\xD1"
                                 "\xC7S\xDF\xA1J\xE2\xE4\xEA\xEB\xEE\xEF\xF4"
                                 "\xF6\xFB\xFC\xF1\xE7sgij\xAA\x90\xA9%Gen\xF6"
                                 "\x93""E\xA3$\x1B\x18\x1A\x19\xBA\xB9\xB2\xB3"
                                 "\xB1In\xFC\xB5\xBF\xF7\xB0\xBC\xBD\xBE\xA7"
                                 "\xC1\xC0\xC9\xC8\xCD\xCE\xD3\xD2\xDA\xD9RCSZ"
                                 "\xD0L\xC2\xC4\xCA\xCB\xCE\xCF\xD4\xD6\xDB\xDC"
                                 "rcsz\xF0l\xC3\xC5\xC6Oy\xDD\xD5""0\xDEGRCSZT"
								 "\xF0\xE3\xE5\xE6ow\xF5""0\xFEgrcszt";
								 
void makePrintable(char* str) {
	for(int i = 0; i < strlen(str); i++) {
        if(str[i] == 0x0D) {
			// CR ends the string, according to RDS 6.1.5.3
            str[i] = '\0';
            break;
        }
        // 0x80: a-acute, a-grave, e-acute, e-grave, i-acute, i-grave, o-acute,
        //       o-grave, u-acute, u-grave, N-tilde, C-cedilla, S-cedilla,
        //       scharfes-es, spanish-exclamation, dutch-IJ, a-circ, a-umlaut,
        //       e-circ, e-umlaut, i-circ, i-umlaut, o-circ, o-umlaut, u-circ,
        //       u-umlaut, n-tilde, c-cedilla, s-cedilla, g-breve,
        //       turkish-i-nodot, dutch-ij, a-superscript, alpha, (c), permille,
        //       G-breve, e-caron, n-caron, o-dprime, pi, EUR, GBP, USD, 
        //       arrow-left, arrow-up, arrow-right, arrow-down, o-superscript,
        //       1-superscript, 2-superscript, 3-superscript, +/-,
        //       turkish-I-dot, n-acute, u-dprime, miu, spanish-question,
        //       division, degree, 1/4, 1/2, 3/4, paragraph,
        //       A,E,I,O,U{acute,grave}, R,C,S,Z{caron}, D-line, L-dot,
        //       A,E,I,O,U{circ,umlaut}, r,c,s,z{caron}, d-line, l-dot,
        //       A-tilde, A-circle, AE, OE, y-circ, Y-acute, O-tilde, O-slash,
        //       Thorn, NG, R,C,S,Z{acute}, T-bar, th, a-tilde, a-circle, ae,
        //       oe, w-circ, o-tilde, o-slash, thorn, ng, r,c,s,z{acute}, t-bar
        if(str[i] == 0x0A || str[i] == 0x0B || str[i] == 0x1F)
            //LF, VT and US are allowed as a control characters. The first with
            //the same meaning as on UNIX, second as end-of-headline indicator
            continue;
        //Any other control character is an undetected error on the receiving
        //side (because the manufacturers of the RDS decoder chip were too cheap
        //to properly implement the ECC in the standard).
        if(str[i] < 32) str[i] = '?';
        else if(str[i] == 0x24) str[i] = '\xA4';
        else if(str[i] == 0x5E) str[i] = '-';
        else if(str[i] == 0x60) str[i] = '\xA0';
        else if(str[i] == 0x7E) str[i] = '_';
        else if(str[i] >= 0x80)
            str[i] = (char)pgm_read_byte(&RDS2LCD_S[str[i] - 0x80]);
	}
}
