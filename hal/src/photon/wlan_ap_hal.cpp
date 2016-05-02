
#include "wlan_ap_hal.h"


/**
 * Indicate if the application wants the AP mode active.
 * If the device is in listening mode, that takes precedence - the system will reconfigure the AP to the application specifications when
 * listening mode exits.
 */
int wlan_ap_manage_state(uint8_t enabled, void* reserved)
{
	//ap_requested = enabled;
	//ap_configure();
	return 0;
}

/**
 * Gets, sets or clears the credentials.
 * When update is false, then the credentials are read into the result parameter (if it is not null.)
 * When update is true, the credentials stored in result are set. If result is null, then the AP credentials are cleared.
 */
int wlan_ap_manage_credentials(WLanCredentials* result, uint8_t update, void* reserved)
{
	return 0;
}

/**
 * Begin AP listening mode.
 */
int wlan_ap_listen(void* reserved)
{
	return 0;
}
