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

#ifndef WLAN_IOCTL_H
#define WLAN_IOCTL_H

#include <cstddef>

#include "wwd_wlioctl.h"

namespace particle {

// Helper functions for WICED's IOCTL API

bool getIovar(const char* name, void* data, size_t size);
bool setIovar(const char* name, const void* data, size_t size);

template<typename T>
inline bool getIovar(const char* name, T* value) {
    return getIovar(name, value, sizeof(T));
}

template<typename T>
inline bool setIovar(const char* name, const T& value) {
    return setIovar(name, &value, sizeof(T));
}

bool getIoctl(uint32_t cmd, void* data, size_t size);

} // namespace particle

#endif // WLAN_IOCTL_H
