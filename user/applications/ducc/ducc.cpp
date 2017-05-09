/*
  ******************************************************************************
  Dual Usage Charge/Communication Software

  Snap, Inc. 2017
  Yu Jiang Tham
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "application.h"
#include "stdarg.h"
#include "RingBufCPP/RingBufCPP.h"

PRODUCT_ID(PLATFORM_ID);
PRODUCT_VERSION(2);

/* Constants -----------------------------------------------------------------*/
const int USB_BAUDRATE =        115200;
const int DUT_BAUDRATE =        57600;

const int TX_BUFFER_SIZE =      128;
const int RX_BUFFER_SIZE =      1024;

const int TX_CONTROL =          D0;
const int RX_CONTROL =          D1;
const int CHG_CONTROL =         D2;
const int CHG_LED =             D7;
const int UART_RX =             RX;

const int CAP_DISCHARGE_MS =    40;
const int TX_GUARD_DELAY_MS =   30;

const int CHG_MODE_MS =         300;
const int CHG_TO_RX_MODE_MS =   250;
const int RX_MODE_MS =          330 - CAP_DISCHARGE_MS;
const int RX_TO_TX_MODE_MS =    1;
const int TX_MODE_MS =          100;
const int TX_TO_CHG_MODE_MS =   10;
const int RST_TO_CHG_MODE_MS =  341;

const char NULL_CHAR =          0x00;
const char DEL_CHAR =           0x7F;
const char BKSP_CHAR =          0x08;

enum CableMode {
  MODE_CHG,
  MODE_CHG_TO_RX,
  MODE_RX,
  MODE_RX_TO_TX,
  MODE_TX,
  MODE_TX_TO_CHG,
  MODE_RST_TO_CHG
};

/* Global Variables ----------------------------------------------------------*/
// char mTxFifo[TX_BUFFER_SIZE];
// boost::circular_buffer<char> mRxFifo(RX_BUFFER_SIZE);
RingBufCPP<char, TX_BUFFER_SIZE> mTxFifo;
RingBufCPP<char, RX_BUFFER_SIZE> mRxFifo;

CableMode mCurrentMode;
unsigned long mStartCycleTime;
unsigned long mCurrentTime;
unsigned long mInterruptTime;

bool mConfiguring = false;
bool mTransmitting = false;
bool mReadyToSend = false;
bool mReadyToReceive = false;

/* Function prototypes -------------------------------------------------------*/
SYSTEM_MODE(MANUAL);
void configureChgMode();

/* This function is called once at start up ----------------------------------*/
void setup()
{
  // Disable WifFi since we don't use it
  WiFi.off();

  // Serial port setup
  Serial.begin(USB_BAUDRATE);   // USB serial port to computer
  Serial1.begin(DUT_BAUDRATE);  // Serial to charge cable

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
  delay(1);
  digitalWrite(RX_CONTROL, HIGH);
  pinMode(UART_RX, INPUT_PULLUP);

  mCurrentMode = MODE_CHG_TO_RX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureRxMode() {
  mConfiguring = true;
  delay(CAP_DISCHARGE_MS); // Capacitor discharge delay
  Serial1.begin(DUT_BAUDRATE);

  mCurrentMode = MODE_RX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureRxToTxMode() {
  mConfiguring = true;
  Serial1.end();
  digitalWrite(CHG_LED, LOW);
  digitalWrite(CHG_CONTROL, LOW);
  digitalWrite(RX_CONTROL, LOW);
  delay(1);
  digitalWrite(TX_CONTROL, HIGH);

  mCurrentMode = MODE_RX_TO_TX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureTxMode() {
  mConfiguring = true;
  Serial1.begin(DUT_BAUDRATE);
  delay(TX_GUARD_DELAY_MS);  // Guard delay

  mCurrentMode = MODE_TX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
  mReadyToSend = true;
}

void configureTxToChgMode() {
  mConfiguring = true;
  Serial1.end();
  mCurrentMode = MODE_TX_TO_CHG;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void resetToChgMode() {
  mConfiguring = true;
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
    // Add input to the FIFO and local output when user types a character
    char txChar = Serial.read();
    // Serial.write(txChar);
    mTxFifo.add(txChar);

    // // Check for special characters
    // if (txChar == DEL_CHAR || txChar == BKSP_CHAR) {
    //   Serial.write("\x1b[D\x1b[P"); // This is the escape sequence for DEL.
    // }
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
        while(!mRxFifo.isEmpty()) {
          char rxChar;
          mRxFifo.pull(&rxChar);
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
          // mRxFifo.add(rxChar);
          // if (mRxFifo.isFull()) {
          //   // Rx buffer is full, stop grabbing data from the Serial
          //   break;
          // }
          Serial.write(rxChar);
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
        while(!mTxFifo.isEmpty()) {
          char txChar;
          mTxFifo.pull(&txChar);
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
