/*
 * Copyright (c) 2017 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "wlan_ioctl.h"

#include "logging.h"

#include "wwd_sdpcm.h"
#include "wwd_buffer_interface.h"

#include <algorithm>

LOG_SOURCE_CATEGORY("hal.wlan.ioctl");

bool particle::getIovar(const char* name, void* data, size_t size) {
    const wwd_interface_t iface = WWD_STA_INTERFACE; // TODO: WWD_AP_INTERFACE?
    wiced_buffer_t sendBuf; // Variable name and additional parameters
    void* d = wwd_sdpcm_get_iovar_buffer(&sendBuf, size, name);
    if (!d) {
        LOG_DEBUG(ERROR, "Unable to allocate iovar buffer (size: %u, name: %s)", (unsigned)size, name);
        return false;
    }
    wiced_buffer_t respBuf; // Variable data
    const wwd_result_t ret = wwd_sdpcm_send_iovar(SDPCM_GET, sendBuf, &respBuf, iface);
    if (ret != WWD_SUCCESS) {
        LOG_DEBUG(ERROR, "Unable to get iovar value (code: %d, name: %s)", (int)ret, name);
        return false;
    }
    d = host_buffer_get_current_piece_data_pointer(respBuf); // Response data
    const size_t n = std::min(size, (size_t)host_buffer_get_current_piece_size(respBuf) /* Response size */);
    memcpy(data, d, n);
    host_buffer_release(respBuf, WWD_NETWORK_RX);
    return true;
}

bool particle::setIovar(const char* name, const void* data, size_t size) {
    const wwd_interface_t iface = WWD_STA_INTERFACE; // TODO: WWD_AP_INTERFACE?
    wiced_buffer_t sendBuf; // Variable name and data
    void* const d = wwd_sdpcm_get_iovar_buffer(&sendBuf, size, name);
    if (!d) {
        LOG_DEBUG(ERROR, "Unable to allocate iovar buffer (size: %u, name: %s)", (unsigned)size, name);
        return false;
    }
    memcpy(d, data, size);
    const wwd_result_t ret = wwd_sdpcm_send_iovar(SDPCM_SET, sendBuf, nullptr, iface);
    if (ret != WWD_SUCCESS) {
        LOG_DEBUG(ERROR, "Unable to set iovar value (code: %d, name: %s)", (int)ret, name);
        return false;
    }
    return true;
}

bool particle::getIoctl(uint32_t cmd, void* data, size_t size) {
    const wwd_interface_t iface = WWD_STA_INTERFACE; // TODO: WWD_AP_INTERFACE?
    wiced_buffer_t sendBuf;
    void* d = wwd_sdpcm_get_ioctl_buffer(&sendBuf, size);
    if (!d) {
        LOG_DEBUG(ERROR, "Unable to allocate ioctl buffer (size: %u, command: %u)", (unsigned)size, (unsigned)cmd);
        return false;
    }
    wiced_buffer_t respBuf;
    const wwd_result_t ret = wwd_sdpcm_send_ioctl(SDPCM_GET, cmd, sendBuf, &respBuf, iface);
    if (ret != WWD_SUCCESS) {
        LOG_DEBUG(ERROR, "Unable to send ioctl command (code: %d, command: %u)", (int)ret, (unsigned)cmd);
        return false;
    }
    d = host_buffer_get_current_piece_data_pointer(respBuf); // Response data
    const size_t n = std::min(size, (size_t)host_buffer_get_current_piece_size(respBuf) /* Response size */);
    memcpy(data, d, n);
    host_buffer_release(respBuf, WWD_NETWORK_RX);
    return true;
}
