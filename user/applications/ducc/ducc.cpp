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
// #include "boost/circular_buffer.hpp"
#include "RingBufCPP/RingBufCPP.h"

PRODUCT_ID(PLATFORM_ID);
PRODUCT_VERSION(2);

/* Constants -----------------------------------------------------------------*/
const int BAUDRATE =            115200;

const int TX_BUFFER_SIZE =      64;
const int RX_BUFFER_SIZE =      64;

const int TX_CONTROL =          D0;
const int RX_CONTROL =          D1;
const int CHG_CONTROL =         D2;
const int CHG_LED =             D7;

const int CHG_MODE_MS =         200;
const int CHG_TO_RX_MODE_MS =   250;
const int RX_MODE_MS =          430;
const int RX_TO_TX_MODE_MS =    1;
const int TX_MODE_MS =          150;
const int TX_TO_CHG_MODE_MS =   10;
const int RST_TO_CHG_MODE_MS =  341;

const char NULL_CHAR =          0x00;

typedef enum {
  MODE_CHG,
  MODE_CHG_TO_RX,
  MODE_RX,
  MODE_RX_TO_TX,
  MODE_TX,
  MODE_TX_TO_CHG,
  MODE_RST_TO_CHG
} CABLE_MODE_t;

/* Global Variables ----------------------------------------------------------*/
RingBufCPP<char, 64> mTxFifo;
RingBufCPP<char, 64> mRxFifo;
// boost::circular_buffer<char> mTxFifo(64);
// boost::circular_buffer<char> mRxFifo(64);
// mTxFifo->set_capacity(64);
// boost::circular_buffer<char> mRxFifo(RX_BUFFER_SIZE);
// boost::circular_buffer<char> mRxFifo;
// Fifo mTxFifo(TX_BUFFER_SIZE);
// Fifo mRxFifo(RX_BUFFER_SIZE);

/* Function prototypes -------------------------------------------------------*/
SYSTEM_MODE(MANUAL);

/* This function is called once at start up ----------------------------------*/
void setup()
{
  // Serial port setup
  Serial.begin(BAUDRATE);   // USB serial port to computer
  Serial1.begin(BAUDRATE);  // Serial to charge cable

  // Setup FIFO
  // mTxFifo.set_capacity(TX_BUFFER_SIZE);
  // mRxFifo.set_capacity(RX_BUFFER_SIZE);

  // Relay control pins
  pinMode(TX_CONTROL, OUTPUT);
  pinMode(RX_CONTROL, OUTPUT);
  pinMode(CHG_CONTROL, OUTPUT);

  // LED Setup
  pinMode(CHG_LED, OUTPUT);
  digitalWrite(CHG_LED, HIGH);
}

/* This function loops forever --------------------------------------------*/
void loop()
{
  //This will run in a loop
  if (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    // Serial1.write(c);
    mTxFifo.add(c);
  }

  if (Serial1.available()) {
    char c = Serial1.read();
    Serial1.write(c);
    // Serial.write(c);
    mRxFifo.add(c);
  }

  if (!mTxFifo.isEmpty()) {
    char c;
    mTxFifo.pull(&c);
    Serial1.write(c);
  }

  if (!mRxFifo.isEmpty()) {
    char c;
    mRxFifo.pull(&c);
    Serial.write(c);
  }
}
