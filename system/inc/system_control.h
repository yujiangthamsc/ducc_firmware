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
  // Perhaps could be combined into a signle request and a mode could be supplied in wValue?
  USB_REQUEST_ENTER_DFU_MODE              = 0x0004,
  USB_REQUEST_ENTER_LISTENING_MODE        = 0x0005,
  //
  USB_REQUEST_SETUP_LOGGING               = 0x0006
} USBRequestType;

typedef struct USBRequest {
  size_t size; // Structure size
  int type; // Request type
  char* data; // Data buffer
  size_t request_size; // Request size
  size_t reply_size; // Reply size (set to maximum size initially)
  int format; // Data format
} USBRequest;

// Callback invoked for USB requests that should be processed at application side
typedef bool(*usb_request_app_handler)(USBRequest* req, void* reserved);

// Sets application callback for USB requests
void system_set_usb_request_app_handler(usb_request_app_handler handler, void* reserved);

// Signals that processing of the USB request has finished successfully
void system_set_usb_request_reply_ready(USBRequest* req, void* reserved);

// Signals that processing of the USB request has finished with an error
void system_set_usb_request_failed(USBRequest* req, void* reserved);

#ifdef __cplusplus
} // extern "C"
#endif

class SystemControlInterface {
public:
  SystemControlInterface();
  ~SystemControlInterface();

  static void setReplyReady(USBRequest* req);
  static void setRequestFailed(USBRequest* req);

private:
  // Handler function for asynchronous USB requests
  typedef bool(*USBRequestHandler)(USBRequest*);

  struct USBRequestData {
    USBRequest req; // Public part of the USB request data
    USBRequestHandler handler;
    bool active, failed;
    volatile bool ready;

    USBRequestData();
    ~USBRequestData();
  };

  USBRequestData usbReq_; // Current request data

  // Maximum size allowed for request and reply data
  static const size_t USB_REQUEST_BUFFER_SIZE = 512;

  uint8_t handleVendorRequest(HAL_USB_SetupRequest* req);
  uint8_t handleAsyncHostToDeviceRequest(HAL_USB_SetupRequest* req, USBRequestHandler handler, DataFormat fmt = DATA_FORMAT_INVALID);
  uint8_t handleAsyncDeviceToHostRequest(HAL_USB_SetupRequest* req, DataFormat fmt = DATA_FORMAT_INVALID);

  static uint8_t vendorRequestCallback(HAL_USB_SetupRequest* req, void* ptr); // Called by HAL

  static void asyncRequestHandler(void* data); // Called by SystemThread
  static bool appRequestHandler(USBRequest* req);

  static void setReplyReady(USBRequestData* req);
  static void setRequestFailed(USBRequestData* req);
};

// SystemControlInterface
inline void SystemControlInterface::setReplyReady(USBRequest* req) {
  setReplyReady((USBRequestData*)req);
}

inline void SystemControlInterface::setRequestFailed(USBRequest* req) {
  setRequestFailed((USBRequestData*)req);
}

inline void SystemControlInterface::setReplyReady(USBRequestData* req) {
  req->ready = true;
}

inline void SystemControlInterface::setRequestFailed(USBRequestData* req) {
  req->failed = true;
  req->ready = true;
}

#endif // USB_VENDOR_REQUEST_ENABLE

#endif // SYSTEM_CONTROL_H_
