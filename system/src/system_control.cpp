/**
 ******************************************************************************
 * @file    system_control.cpp
 * @author  Andrey Tolstoy
 * @version V1.0.0
 * @date    12-June-2016
 * @brief
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

#include "system_control.h"

#include "system_threading.h"
#include "deviceid_hal.h"
#include "spark_wiring.h"

#ifdef USB_VENDOR_REQUEST_ENABLE

namespace {

usb_request_app_handler usbRequestAppHandler = nullptr;

} // namespace

// ::*
void system_set_usb_request_app_handler(usb_request_app_handler handler, void* reserved) {
  usbRequestAppHandler = handler;
}

void system_set_usb_request_reply_ready(USBRequest* req, void* reserved) {
  SystemControlInterface::setReplyReady(req);
}

void system_set_usb_request_failed(USBRequest* req, void* reserved) {
  SystemControlInterface::setRequestFailed(req);
}

// SystemControlInterface::USBRequestData
SystemControlInterface::USBRequestData::USBRequestData() :
    handler(nullptr),
    active(false),
    failed(false),
    ready(false) {
  req.size = sizeof(USBRequest);
  req.type = USB_REQUEST_INVALID;
  req.data = (char*)malloc(USB_REQUEST_BUFFER_SIZE);
  req.request_size = 0;
  req.reply_size = 0;
  req.format = DATA_FORMAT_INVALID;
}

SystemControlInterface::USBRequestData::~USBRequestData() {
  free(req.data);
}

// SystemControlInterface
SystemControlInterface::SystemControlInterface() {
  HAL_USB_Set_Vendor_Request_Callback(&SystemControlInterface::vendorRequestCallback, static_cast<void*>(this));
}

SystemControlInterface::~SystemControlInterface() {
  HAL_USB_Set_Vendor_Request_Callback(nullptr, nullptr);
}

uint8_t SystemControlInterface::vendorRequestCallback(HAL_USB_SetupRequest* req, void* ptr) {
  SystemControlInterface* self = static_cast<SystemControlInterface*>(ptr);
  if (self)
    return self->handleVendorRequest(req);

  return 1;
}

uint8_t SystemControlInterface::handleVendorRequest(HAL_USB_SetupRequest* req) {
  /*
   * This callback should process vendor-specific SETUP requests from the host.
   * NOTE: This callback is called from an ISR.
   *
   * Each request contains the following fields:
   * - bmRequestType - request type bit mask. Since only vendor-specific device requests are forwarded
   *                   to this callback, the only bit that should be of intereset is
   *                   "Data Phase Transfer Direction", easily accessed as req->bmRequestTypeDirection:
   *                   0 - Host to Device
   *                   1 - Device to Host
   * - bRequest - 1 byte, request identifier. The only reserved request that will not be forwarded to this callback
   *              is 0xee, which is used for Microsoft-specific vendor requests.
   * - wIndex - 2 byte index, any value between 0x0000 and 0xffff.
   * - wValue - 2 byte value, any value between 0x0000 and 0xffff.
   * - wLength - each request might have an optional data stage of up to 0xffff (65535) bytes.
   *             Host -> Device requests contain data sent by the host to the device.
   *             Device -> Host requests request the device to send up to wLength bytes of data.
   *
   * This callback should return 0 if the request has been correctly handled or 1 otherwise.
   *
   * When handling Device->Host requests with data stage, this callback should fill req->data buffer with
   * up to wLength bytes of data if req->data != NULL (which should be the case when wLength <= 64)
   * or set req->data to point to some buffer which contains up to wLength bytes of data.
   * wLength may be safely modified to notify that there is less data in the buffer than requested by the host.
   *
   * [1] Host -> Device requests with data stage containing up to 64 bytes can be handled by
   * an internal buffer in HAL. For requests larger than 64 bytes, the vendor request callback
   * needs to provide a buffer of an appropriate size:
   *
   * req->data = buffer; // sizeof(buffer) >= req->wLength
   * return 0;
   *
   * The callback will be called again once the data stage completes
   * It will contain the same bmRequest, bRequest, wIndex, wValue and wLength fields.
   * req->data should contain wLength bytes received from the host.
   */

  /*
   * We are handling only bRequest = 0x50 ('P') requests.
   * The request type itself (enum ControlRequest) should be in wIndex field.
   */
  if (req->bRequest != 0x50)
    return 1;

  if (req->bmRequestTypeDirection == 0) {
    // Host -> Device
    switch (req->wIndex) {
      case USB_REQUEST_RESET: {
        // FIXME: We probably shouldn't reset from an ISR.
        // The host will probably get an error that control request has timed out since we
        // didn't respond to it.
        System.reset(req->wValue);
        break;
      }

      case USB_REQUEST_ENTER_DFU_MODE: {
        // FIXME: We probably shouldn't enter DFU mode from an ISR.
        // The host will probably get an error that control request has timed out since we
        // didn't respond to it.
        System.dfu(false);
        break;
      }

      case USB_REQUEST_ENTER_LISTENING_MODE: {
        // FIXME: We probably shouldn't enter listening mode from an ISR.
        // The host will probably get an error that control request has timed out since we
        // didn't respond to it.
        system_set_flag(SYSTEM_FLAG_STARTUP_SAFE_LISTEN_MODE, 1, nullptr);
        System.enterSafeMode();
        break;
      }

      case USB_REQUEST_SETUP_LOGGING: {
        return handleAsyncHostToDeviceRequest(req, appRequestHandler, DATA_FORMAT_JSON); // Forwarded to application module
      }

      default: {
        // Unknown request
        return 1;
      }
    }
  } else {
    // Device -> Host
    switch (req->wIndex) {
      case USB_REQUEST_GET_DEVICE_ID: {
        if (req->wLength == 0 || req->data == NULL) {
          // No data stage or requested > 64 bytes
          return 1;
        }
        if (req->wValue == 0x0001) {
          // Return as buffer
          if (req->wLength < 12)
            return 1;

          HAL_device_ID(req->data, req->wLength);
          req->wLength = 12;
        } else {
          // Return as string
          String id = System.deviceID();
          if (req->wLength < (id.length() + 1))
            return 1;
          strncpy((char*)req->data, id.c_str(), req->wLength);
          req->wLength = id.length() + 1;
        }
        break;
      }

      case USB_REQUEST_GET_SYSTEM_VERSION: {
        if (req->wLength == 0 || req->data == NULL) {
          // No data stage or requested > 64 bytes
          return 1;
        }

        strncpy((char*)req->data, __XSTRING(SYSTEM_VERSION_STRING), req->wLength);
        req->wLength = sizeof(__XSTRING(SYSTEM_VERSION_STRING)) + 1;
        break;
      }

      case USB_REQUEST_SETUP_LOGGING: {
        return handleAsyncDeviceToHostRequest(req, DATA_FORMAT_JSON);
      }

      default: {
        // Unknown request
        return 1;
      }
    }
  }

  return 0;
}

uint8_t SystemControlInterface::handleAsyncHostToDeviceRequest(HAL_USB_SetupRequest* req, USBRequestHandler handler, DataFormat fmt) {
  SPARK_ASSERT(req->bmRequestTypeDirection == 0); // Host to device
  if (usbReq_.active && !usbReq_.ready) {
    return 1; // // There is an active request already
  }
  if (req->wLength > 0) {
    if (!usbReq_.req.data) {
      return 1; // Initialization error
    } else if (req->wLength > USB_REQUEST_BUFFER_SIZE) {
      return 1; // Too large request
    } else if (req->wLength <= 64) {
      if (!req->data) {
        return 1; // Invalid request info
      }
      memcpy(usbReq_.req.data, req->data, req->wLength);
    } else if (!req->data) {
      req->data = (uint8_t*)usbReq_.req.data; // Provide buffer for request data
      return 0; // OK
    }
  }
  // Schedule request for further processing
  if (!SystemThread.invokeAsyncFromISR(asyncRequestHandler, &usbReq_)) {
    return 1;
  }
  usbReq_.req.type = (USBRequestType)req->wIndex;
  usbReq_.req.request_size = req->wLength;
  usbReq_.req.reply_size = USB_REQUEST_BUFFER_SIZE;
  usbReq_.req.format = fmt;
  usbReq_.handler = handler;
  usbReq_.active = true;
  usbReq_.failed = false;
  usbReq_.ready = false;
  return 0;
}

uint8_t SystemControlInterface::handleAsyncDeviceToHostRequest(HAL_USB_SetupRequest* req, DataFormat fmt) {
  SPARK_ASSERT(req->bmRequestTypeDirection == 1); // Device to host
  if (!usbReq_.ready) {
    return 1; // No reply data available
  }
  if (usbReq_.req.type != (USBRequestType)req->wIndex || usbReq_.req.format != fmt) {
    return 1; // Unexpected request type or format
  }
  if (usbReq_.failed) {
    return 1; // Request has failed (TODO: Reply to host with some result code?)
  }
  if (req->wLength > 0) {
    if (!usbReq_.req.data) {
      return 1; // Initialization error
    } else if (req->wLength < usbReq_.req.reply_size) {
      return 1; // Too large reply
    } else if (req->wLength <= 64) {
      if (!req->data) {
        return 1; // Invalid request info
      }
      memcpy(req->data, usbReq_.req.data, usbReq_.req.reply_size);
    } else if (!req->data) {
      req->data = (uint8_t*)usbReq_.req.data; // Provide buffer with reply data
      req->wLength = usbReq_.req.reply_size;
    }
  }
  usbReq_.active = false;
  usbReq_.ready = false;
  return 0;
}

void SystemControlInterface::asyncRequestHandler(void* data) {
  USBRequestData* req = static_cast<USBRequestData*>(data);
  USBRequestHandler func = req->handler;
  if (!func(&req->req)) {
    setRequestFailed(req);
  }
}

bool SystemControlInterface::appRequestHandler(USBRequest* req) {
  if (!usbRequestAppHandler) {
    return false;
  }
  return usbRequestAppHandler(req, nullptr);
}

#endif // USB_VENDOR_REQUEST_ENABLE
