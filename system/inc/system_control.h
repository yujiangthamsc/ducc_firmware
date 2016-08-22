/**
 ******************************************************************************
 * @file    system_control.h
 * @author  Andrey Tolstoy
 * @version V1.0.0
 * @date    12-June-2016
 * @brief   Header for system_contro.cpp module
 ******************************************************************************
  Copyright (c) 2013-2016 Particle Industries, Inc.  All rights reserved.

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

#ifndef SYSTEM_CONTROL_H_
#define SYSTEM_CONTROL_H_

#include <stddef.h>

#include "usb_hal.h"

typedef enum DataFormat { // TODO: Move to appropriate header
  DATA_FORMAT_INVALID = 0,
  DATA_FORMAT_BINARY = 10, // Generic binary format
  DATA_FORMAT_TEXT = 20, // Generic text format
  DATA_FORMAT_JSON = 30
} DataFormat;

#ifdef USB_VENDOR_REQUEST_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

typedef enum USBRequestType {
  USB_REQUEST_INVALID                     = 0x0000,
  USB_REQUEST_GET_DEVICE_ID               = 0x0001,
  USB_REQUEST_GET_SYSTEM_VERSION          = 0x0002,
  USB_REQUEST_RESET                       = 0x0003,
  USB_REQUEST_ENTER_DFU_MODE              = 0x0004,
  USB_REQUEST_ENTER_LISTENING_MODE        = 0x0005,
  USB_REQUEST_TEST                        = 0x0006, // For testing purposes
  USB_REQUEST_CONFIG_LOG                  = 0x0007
} USBRequestType;

typedef enum USBRequestResult {
  USB_REQUEST_RESULT_OK = 0,
  USB_REQUEST_RESULT_ERROR = 10
} USBRequestResult;

typedef struct USBRequest {
  size_t size; // Structure size
  int type; // Request type (as defined by USBRequestType enum)
  char* data; // Data buffer
  size_t request_size; // Request size
  size_t reply_size; // Reply size (set to maximum allowed size initially)
  int format; // Data format
} USBRequest;

// Callback invoked for USB requests that should be processed at application side
typedef bool(*usb_request_app_handler_type)(USBRequest* req, void* reserved);

// Sets application callback for USB requests
void system_set_usb_request_app_handler(usb_request_app_handler_type handler, void* reserved);

// Signals that processing of the USB request has finished
void system_set_usb_request_result(USBRequest* req, int result, void* reserved);

#ifdef __cplusplus
} // extern "C"

class SystemControlInterface {
public:
  SystemControlInterface();
  ~SystemControlInterface();

  static void setRequestResult(USBRequest* req, USBRequestResult result);

private:
  struct USBRequestData {
    USBRequest req; // Externally accessible part of the request data
    USBRequestResult result;
    bool active;
    volatile bool ready;

    USBRequestData();
    ~USBRequestData();
  };

  USBRequestData usbReq_;

  // Maximum size allowed for request and reply data
  static const size_t USB_REQUEST_BUFFER_SIZE = 512;

  uint8_t handleVendorRequest(HAL_USB_SetupRequest* req);
  uint8_t handleAsyncVendorRequest(HAL_USB_SetupRequest* req, DataFormat fmt = DATA_FORMAT_INVALID);
  uint8_t fetchAsyncVendorRequestResult(HAL_USB_SetupRequest* req, DataFormat fmt = DATA_FORMAT_INVALID);

  static uint8_t vendorRequestCallback(HAL_USB_SetupRequest* req, void* data); // Called by HAL
  static void asyncVendorRequestCallback(void* data); // Called by SystemThread

  static void setRequestResult(USBRequestData* req, USBRequestResult result);
};

#endif // __cplusplus

#endif // USB_VENDOR_REQUEST_ENABLE

#endif // SYSTEM_CONTROL_H_
