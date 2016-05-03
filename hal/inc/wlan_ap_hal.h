/**
 ******************************************************************************
  Copyright (c) 2016 Particle Industries, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */
#pragma once

#include "wlan_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Indicate if the application wants the AP mode active.
 * If the device is in listening mode, that takes precedence - the system will reconfigure the AP to the application specifications when
 * listening mode exits.
 */
int wlan_ap_manage_state(uint8_t enabled, void* reserved);

/**
 * Sets or clears the credentials.
 * When result is not null, the credentials stored in result are set.
 * When result is null, the AP credentials are cleared.
 */
int wlan_ap_set_credentials(WLanCredentials* result, void* reserved);

/**
 * Retrieves the AP credentials.
 * @param result  The struct where the credentials are stored..
 * @return 0 on success
 */
int wlan_ap_get_credentials(WiFiAccessPoint* result, void* reserved);

/**
 * Determines if AP credentials have been set.
 * @return 0 on success (credentials are set.)
 */
int wlan_ap_has_credentials(void* reserved);

/**
 * Begin AP listening mode.
 */
int wlan_ap_listen(void* reserved);


#ifdef __cplusplus
}
#endif

