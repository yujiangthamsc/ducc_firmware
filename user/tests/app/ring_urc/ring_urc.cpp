/**
 ******************************************************************************
  Copyright (c) 2015 Particle Industries, Inc.  All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

// Compile from firmware/modules $ with
// make clean all PLATFORM_ID=10 -s TEST=app/ring_urc COMPILE_LTO=n DEBUG_BUILD=y program-dfu
//
// System will sleep approximately 1 minute after initially connecting to the cloud.
// Force a URC by grabbing the antenna and reducing signal strength to wake up the system.

#include "application.h"

// ALL_LEVEL, TRACE_LEVEL, DEBUG_LEVEL, WARN_LEVEL, ERROR_LEVEL, PANIC_LEVEL, NO_LOG_LEVEL
SerialDebugOutput debugOutput(9600, ALL_LEVEL);

uint32_t startTime = 0;
#define now millis()

SYSTEM_MODE(SEMI_AUTOMATIC);

void setup()
{
  delay(4000);

	DEBUG_D("Hello from the Electron! Boot time is: %-10.3f\r\n ms", millis()*0.001);

	Particle.connect(); // blocking call to connect
  startTime = now;
}

void loop()
{
	if (now - startTime > 60000UL) {
    DEBUG_D("Going to sleep now! Time: %-10.3f\r\n", millis()*0.001);
    delay(100);
    System.sleep(RI_UC, FALLING);
    // System consumes about 18mA while STM32 in stop mode and modem still powered on but idle.
    delay(100);
    DEBUG_D("Awake again! Time: %-10.3f\r\n", millis()*0.001);
    startTime = now;
  }
}
