#include <stdio.h>
//#include <avr/pgmspace.h>
#include <Arduino.h>
#include <Wire.h>
#include "TEA5767.h"

TEA5767::TEA5767() {
	Wire.begin();
	HILO = 1;
}

TEA5767::TEA5767(double initial_freq) {
	Wire.begin();
	HILO = 1;
	set_frequency(initial_freq);
}

//calculate the optimial hi or lo injection mode for the freq is in hz
//return 1 if high is the best, or 0 for low injection
int TEA5767::hilo_optimal (unsigned long freq) {

	int signal_high = 0;
	int signal_low = 0;
	unsigned char buf[5];

	set_frequency (1, (double) (freq + 450000) / 1000000);
	delay (30);

	// Read the signal level
	if (read_status (buf) == 1) {
		signal_high = signal_level (buf);
	}

	set_frequency (0, (double) (freq - 450000) / 1000000);
	delay (30);

	if (read_status (buf) == 1) {
		signal_low = signal_level (buf);
	}

	return (signal_high < signal_low) ? 1 : 0;
}

void TEA5767::set_frequency (int hilo, double freq) {
	unsigned char buffer[5];
	unsigned div;
	int rc;

	memset (buffer, 0, 5);

	buffer[2] = 0;
	buffer[2] |= TEA5767_PORT1_HIGH;

	if (hilo == 1)
    	buffer[2] |= TEA5767_HIGH_LO_INJECT;

	buffer[3] = 0;

	if (ctrl_data.port2)
		buffer[3] |= TEA5767_PORT2_HIGH;

	if (ctrl_data.high_cut)
		buffer[3] |= TEA5767_HIGH_CUT_CTRL;

	if (ctrl_data.st_noise)
		buffer[3] |= TEA5767_ST_NOISE_CTL;

	if (ctrl_data.soft_mute)
		buffer[3] |= TEA5767_SOFT_MUTE;

	if (ctrl_data.japan_band)
		buffer[3] |= TEA5767_JAPAN_BAND;

	buffer[3] |= TEA5767_XTAL_32768;
	buffer[4] = 0;

	if (ctrl_data.deemph_75)
		buffer[4] |= TEA5767_DEEMPH_75;

	if (ctrl_data.pllref)
		buffer[4] |= TEA5767_PLLREF_ENABLE;

	if (hilo == 1)
		div = (4 * (freq * 1000 + 225)) / 32.768;
	else
		div = (4 * (freq * 1000 - 225)) / 32.768;

	buffer[0] = (div >> 8) & 0x3f;
	buffer[1] = div & 0xff;

	Wire.beginTransmission (0x60);

	for (int i = 0; i < 5; i++)
		Wire.write (buffer[i]);

	Wire.endTransmission ();
}

/* Freq should be specifyed at X M hz */
void TEA5767::set_frequency (double freq) {
	HILO = hilo_optimal ((unsigned long) (freq * 1000000));
	set_frequency (HILO, freq);
}

//read functions

int TEA5767::read_status (unsigned char *buf) {
	memset (buf, 0, 5);
	Wire.requestFrom (0x60, 5);    //reading TEA5767

	if (Wire.available ()) {
		for (int i = 0; i < 5; i++) {
			buf[i] = Wire.read ();
		}
      	return 1;
	} else {
    	return 0;
	}
}

int TEA5767::signal_level (unsigned char *buf) {
	int signal = ((buf[3] & TEA5767_ADC_LEVEL_MASK) >> 4);
	return signal;
}

int TEA5767::stereo (unsigned char *buf) {
	int stereo = (buf[2] & TEA5767_STEREO_MASK);
	return stereo ? 1 : 0;
}

//returns 1 if tuning completed or BL reached
int TEA5767::ready (unsigned char *buf) {
	return (buf[0] & 0x80) ? 1 : 0;
}

//returns 1 if band limit is reached during searching
int TEA5767::bl_reached (unsigned char *buf) {
	return (buf[0] & 0x40) ? 1 : 0;
}

//returns freq available in Hz
double TEA5767::frequency_available (unsigned char *buf) {
	double freq_available;
	if (HILO == 1)
    	freq_available = (((buf[0] & 0x3F) << 8) + buf[1]) * 32768 / 4 - 225000;
  	else
    	freq_available = (((buf[0] & 0x3F) << 8) + buf[1]) * 32768 / 4 + 225000;
  	return freq_available;
}

void TEA5767::search_up (unsigned char *buf) {
  	unsigned div;
  	double freq_av;

  	freq_av = frequency_available (buf);

	div = (4 * (((freq_av + 98304) / 1000000) * 1000000 + 225000)) / 32768;

	buf[0] = (div >> 8) & 0x3f;
  	buf[1] = div & 0xff;

  	buf[0] |= TEA5767_SEARCH;

  	buf[2] = 0;

  	buf[2] |= TEA5767_SEARCH_UP;
  	buf[2] |= TEA5767_SRCH_MID_LVL;
  	buf[2] |= TEA5767_HIGH_LO_INJECT;

  	//buf[3] = 0x18;
  	buf[3] = 0;

  	if (ctrl_data.port2)
    	buf[3] |= TEA5767_PORT2_HIGH;

  	if (ctrl_data.high_cut)
    	buf[3] |= TEA5767_HIGH_CUT_CTRL;

  	if (ctrl_data.st_noise)
    	buf[3] |= TEA5767_ST_NOISE_CTL;

  	if (ctrl_data.soft_mute)
    	buf[3] |= TEA5767_SOFT_MUTE;

  	if (ctrl_data.japan_band)
    	buf[3] |= TEA5767_JAPAN_BAND;

  	buf[3] |= TEA5767_XTAL_32768;

  	buf[4] = 0;

  	if (ctrl_data.deemph_75)
    	buf[4] |= TEA5767_DEEMPH_75;

  	if (ctrl_data.pllref)
    	buf[4] |= TEA5767_PLLREF_ENABLE;

  	Wire.beginTransmission (0x60);

  	for (int i = 0; i < 5; i++)
    	Wire.write (buf[i]);

  	Wire.endTransmission ();
  	HILO = 1;
}

void TEA5767::search_down (unsigned char *buf)
{
  	unsigned div;
  	double freq_av;

  	freq_av = frequency_available (buf);

  	div = (4 * (((freq_av - 98304) / 1000000) * 1000000 + 225000)) / 32768;

	buf[0] = (div >> 8) & 0x3f;
  	buf[1] = div & 0xff;

  	buf[0] |= TEA5767_SEARCH;

  	buf[2] = 0;

  	buf[2] |= TEA5767_SRCH_MID_LVL;
  	buf[2] |= TEA5767_HIGH_LO_INJECT;

  	buf[3] = 0;

  	if (ctrl_data.port2)
    	buf[3] |= TEA5767_PORT2_HIGH;

  	if (ctrl_data.high_cut)
    	buf[3] |= TEA5767_HIGH_CUT_CTRL;

  	if (ctrl_data.st_noise)
    	buf[3] |= TEA5767_ST_NOISE_CTL;

  	if (ctrl_data.soft_mute)
    	buf[3] |= TEA5767_SOFT_MUTE;

  	if (ctrl_data.japan_band)
    	buf[3] |= TEA5767_JAPAN_BAND;

  	buf[3] |= TEA5767_XTAL_32768;

  	buf[4] = 0;

  	if (ctrl_data.deemph_75)
    	buf[4] |= TEA5767_DEEMPH_75;

  	if (ctrl_data.pllref)
    	buf[4] |= TEA5767_PLLREF_ENABLE;

  	Wire.beginTransmission (0x60);

  	for (int i = 0; i < 5; i++)
    	Wire.write (buf[i]);

  	Wire.endTransmission ();

  	HILO = 1;
}

//Returns 1 if search is finished, 0 if wrapped and new search initiated
//TODO - To prevent endless looping add a static variable to abort if it has searched for more than 2 loops
int TEA5767::process_search (unsigned char *buf, int search_dir)
{
  	if (ready (buf) == 1) {
      	if (bl_reached (buf) == 1) {
      		if (search_dir == TEA5767_SEARCH_DIR_UP) {
          		//wrap down
          		set_frequency (87.5);
          		read_status (buf);
          		search_up (buf);
          		return 0;
        	} else if (search_dir == TEA5767_SEARCH_DIR_DOWN) {
          		//wrap up
          		set_frequency (108);
          		read_status (buf);
          		search_down (buf);
          		return 0;
        	}
    	} else {
      		// search finished - round up the pll word and feed it back as recommended
      		double rounded_freq;
      		double freq_available = frequency_available (buf);
      		rounded_freq = floor (freq_available / 100000 + .5) / 10;
      		set_frequency (rounded_freq);
      		return 1;
    	}
    }
}

void TEA5767::init() {
  ctrl_data.port1 = 1;
  ctrl_data.port2 = 1;
  ctrl_data.high_cut = 1;
  ctrl_data.st_noise = 1;
  ctrl_data.soft_mute = 1;
  ctrl_data.deemph_75 = 0;
  ctrl_data.japan_band = 0;
  ctrl_data.pllref = 0;

//  unsigned long freq = 87500000;
//  set_frequency((float) freq / 1000000);

}
