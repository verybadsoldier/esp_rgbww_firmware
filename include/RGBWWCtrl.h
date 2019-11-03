/**
 * @file
 * @author  Patrick Jahns http://github.com/patrickjahns
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 *
 */

#ifndef RGBWWCTRL_H_
#define RGBWWCTRL_H_

//default defines

#define CLEAR_PIN 16
#define DEFAULT_AP_IP "192.168.4.1"
#define DEFAULT_AP_SECURED false
#define DEFAULT_AP_PASSWORD "rgbwwctrl"
#define DEFAULT_AP_SSIDPREFIX "RGBWW"
#define DEFAULT_API_SECURED false
#define DEFAULT_API_PASSWORD "rgbwwctrl"
#define DEFAULT_CONNECTION_RETRIES 10
#define DEFAULT_OTA_URL "http://rgbww.dronezone.de/release/version.json"

// RGBWW related
#define DEFAULT_COLORTEMP_WW 2700
#define DEFAULT_COLORTEMP_CW 6000

#define PWM_FREQUENCY 339
#define RGBWW_USE_ESP_HWPWM

// Debugging
#define DEBUG_APP 1

//includes
#include <RGBWWLed/RGBWWLed.h>
#ifdef ARCH_ESP8266
#include <otaupdate.h>
#endif
#include <config.h>
#include <ledctrl.h>
#include <networking.h>
#include <webserver.h>
#include <mqtt.h>
#include <eventserver.h>
#include <jsonprocessor.h>
#include <application.h>
#include <stepsync.h>
#include <arduinojson.h>

#endif /* RGBWWCTRL_H_ */
