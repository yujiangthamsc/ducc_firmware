/*
  ******************************************************************************
  Dual UART Charge/Communication Software

  Snap, Inc. 2017
  Yu Jiang Tham
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "application.h"
#include "stdarg.h"
#include "boost/circular_buffer.hpp"

PRODUCT_ID(PLATFORM_ID);
PRODUCT_VERSION(2);

/* Constants -----------------------------------------------------------------*/
const int BAUDRATE = 115200;
const int BLUELED = D7;
const int TX_BUFFER_SIZE = 1024;
const int RX_BUFFER_SIZE = 1024;

/* Globals Variables ---------------------------------------------------------*/
boost::circular_buffer<char> mTxFifo(TX_BUFFER_SIZE);
// char mRxBuffer[RX_BUFFER_SIZE];

/* Function prototypes -------------------------------------------------------*/
SYSTEM_MODE(MANUAL);
// SYSTEM_MODE(AUTOMATIC);

/* This function is called once at start up ----------------------------------*/
void setup()
{
  // Serial port setup
  Serial.begin(BAUDRATE);   // USB serial port to computer
  Serial1.begin(BAUDRATE);  // Serial to charge port

  // Ring Buffer setup

  // LED Setup
  pinMode(BLUELED, OUTPUT);
}

/* This function loops forever --------------------------------------------*/
void loop()
{
  //This will run in a loop
  // for (;;) {
  if (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    // Serial1.write(c);
    mTxFifo.push_back(c);
  }

  // if (Serial1.available()) {
  //   char c = Serial1.read();
  //   Serial1.write(c);
  //   Serial.write(c);
  // }

  if (mTxFifo.size() != 0) {
    char c = mTxFifo.pop_front();
    Serial1.write(c);
  }
  // }
}
