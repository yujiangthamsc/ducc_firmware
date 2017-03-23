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
const int BAUDRATE =            115200;

const int TX_BUFFER_SIZE =      1024;
const int RX_BUFFER_SIZE =      1024;

const int TX_CONTROL =          D0;
const int RX_CONTROL =          D1;
const int CHG_CONTROL =         D2;
const int CHG_LED =             D7;
const int UART_RX =             RX;

const int CHG_MODE_MS =         200;
const int CHG_TO_RX_MODE_MS =   250;
const int RX_MODE_MS =          430;
const int RX_TO_TX_MODE_MS =    1;
const int TX_MODE_MS =          150;
const int TX_TO_CHG_MODE_MS =   10;
const int RST_TO_CHG_MODE_MS =  341;

const char NULL_CHAR =          0x00;

enum CableMode {
  MODE_CHG,
  MODE_CHG_TO_RX,
  MODE_RX,
  MODE_RX_TO_TX,
  MODE_TX,
  MODE_TX_TO_CHG,
  MODE_RST_TO_CHG
};

/* Globals Variables ---------------------------------------------------------*/
boost::circular_buffer<char> mTxFifo(TX_BUFFER_SIZE);
boost::circular_buffer<char> mRxFifo(RX_BUFFER_SIZE);

CableMode mCurrentMode;
unsigned long long mStartCycleTime;
unsigned long long mCurrentTime;
unsigned long long mInterruptTime;

bool mConfiguring = false;
bool mTransmitting = false;
bool mReadyToSend = false;
bool mReadyToReceive = false;
int mRxBytesToSend = 0;

/* Function prototypes -------------------------------------------------------*/
SYSTEM_MODE(MANUAL);
void configureChgMode();

/* This function is called once at start up ----------------------------------*/
void setup()
{
  // Serial port setup
  Serial.begin(BAUDRATE);   // USB serial port to computer
  Serial1.begin(BAUDRATE);  // Serial to charge cable

  // Relay control pins
  pinMode(TX_CONTROL, OUTPUT);
  pinMode(RX_CONTROL, OUTPUT);
  pinMode(CHG_CONTROL, OUTPUT);

  // LED Setup
  pinMode(CHG_LED, OUTPUT);
  digitalWrite(CHG_LED, HIGH);

  // Start the cycle
  configureChgMode();
}

void configureChgMode() {
  mConfiguring = true;
  Serial1.end();
  digitalWrite(CHG_LED, HIGH);
  digitalWrite(TX_CONTROL, LOW);
  digitalWrite(RX_CONTROL, LOW);
  delay(1);
  digitalWrite(CHG_CONTROL, HIGH);
  delay(1);

  mCurrentMode = MODE_CHG;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureChgToRxMode() {
  mConfiguring = true;
  digitalWrite(CHG_LED, LOW);
  digitalWrite(CHG_CONTROL, LOW);
  digitalWrite(TX_CONTROL, LOW);
  digitalWrite(RX_CONTROL, HIGH);
  pinMode(UART_RX, INPUT_PULLUP);

  mCurrentMode = MODE_CHG_TO_RX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureRxMode() {
  mConfiguring = true;
  delay(6); // Capacitor discharge delay
  Serial1.begin(BAUDRATE);

  mCurrentMode = MODE_RX;
  mStartCycleTime = mCurrentTime;
  mRxBytesToSend = 0;
  mConfiguring = false;
}

void configureRxToTxMode() {
  mConfiguring = true;
  Serial1.end();
  digitalWrite(CHG_LED, LOW);
  digitalWrite(CHG_CONTROL, LOW);
  digitalWrite(RX_CONTROL, LOW);
  digitalWrite(TX_CONTROL, HIGH);

  mCurrentMode = MODE_RX_TO_TX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureTxMode() {
  mConfiguring = true;
  Serial1.begin(BAUDRATE);
  delay(60);  // Guard delay

  mCurrentMode = MODE_TX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
  mReadyToSend = true;
}

void configureTxToChgMode() {
  mConfiguring = true;
  mCurrentMode = MODE_TX_TO_CHG;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void resetToChgMode() {
  mConfiguring = true;
  Serial1.begin(BAUDRATE);
  delay(2);
  Serial1.end();
  digitalWrite(CHG_LED, LOW);
  digitalWrite(TX_CONTROL, LOW);
  digitalWrite(RX_CONTROL, LOW);
  digitalWrite(CHG_CONTROL, LOW);

  mCurrentMode = MODE_RST_TO_CHG;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}


/* Main run loop -------------------------------------------------------------*/
void loop()
{
  mCurrentTime = millis();

  while (Serial.available() > 0) {
    char txChar = Serial.read();
    Serial.write(txChar);
    mTxFifo.push_back(txChar);

    // Check for special characters
  }

  if (mConfiguring) return;

  switch (mCurrentMode) {
    case MODE_CHG:
    {
      if (mCurrentTime - mStartCycleTime > CHG_MODE_MS) {
        configureChgToRxMode();
        break;
      }

      if (mReadyToReceive) {
        while(mRxFifo.size() != 0) {
          char rxChar = mRxFifo.front();
          mRxFifo.pop_front();
          Serial.write(rxChar);
        }
      }
      break;
    }

    case MODE_CHG_TO_RX:
    {
      // Find out how long the charge disconnect interrupt time takes on Specs
      mInterruptTime = mCurrentTime - mStartCycleTime;
      if (mInterruptTime > CHG_TO_RX_MODE_MS) {
        // Timer timeout for pre-Rx mode; go back to charge mode after delay
        resetToChgMode();
        break;
      }
      if (digitalRead(UART_RX) == LOW) {
        configureRxMode();
        break;
      }
      break;
    }

    case MODE_RX:
    {
      if (mCurrentTime - mStartCycleTime + mInterruptTime > RX_MODE_MS) {
        configureRxToTxMode();
        break;
      }

      // Check if there's data available from the DUT and write it to the FIFO
      while (Serial1.available() > 0) {
        mTransmitting = true;
        char rxChar = Serial1.read();
        if (rxChar != NULL_CHAR) {
          mRxFifo.push_back(rxChar);
          if (mRxFifo.size() == RX_BUFFER_SIZE) {
            // Rx buffer is full, stop grabbing data from the Serial
            break;
          }
          mReadyToReceive = true;
        }
      }
      if (Serial1.available() == 0) {
        mTransmitting = false;
      }
      break;
    }

    case MODE_RX_TO_TX:
    {
      if (mCurrentTime - mStartCycleTime > RX_TO_TX_MODE_MS) {
        configureTxMode();
        break;
      }
      break;
    }

    case MODE_TX:
    {
      if (mCurrentTime - mStartCycleTime > TX_MODE_MS) {
        configureTxToChgMode();
        break;
      }
      if (mReadyToSend) {
        while(mTxFifo.size() != 0) {
          char txChar = mTxFifo.front();
          mTxFifo.pop_front();
          Serial1.write(txChar);
        }
        mReadyToSend = false;
      }
      break;
    }

    case MODE_TX_TO_CHG:
    {
      if (mCurrentTime - mStartCycleTime > TX_TO_CHG_MODE_MS) {
        configureChgMode();
        break;
      }
      break;
    }

    case MODE_RST_TO_CHG:
    {
      if (mCurrentTime - mStartCycleTime > RST_TO_CHG_MODE_MS) {
        configureChgMode();
        break;
      }
      break;
    }

    default:
    {
      Serial.printf("DUCC is in an invalid state.");
      break;
    }
  }
}
