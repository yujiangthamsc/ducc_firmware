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
// Baud rates
const int USB_BAUDRATE =        115200;
const int DUT_BAUDRATE =        57600;

// Buffer sizes
const int TX_BUFFER_SIZE =      128;
const int RX_BUFFER_SIZE =      1024;

// Control pins
const int TX_CONTROL =          D0;
const int RX_CONTROL =          D1;
const int CHG_CONTROL =         D2;
const int CHG_LED =             D7;
const int UART_RX =             RX;

// Pins for timing debug
const int SCOPE_TX_MODE =       D4;
const int SCOPE_RX_MODE =       D5;
const int SCOPE_CHG_MODE =      D6;

// Internal delays
const int RX_GUARD_DELAY_MS =   5;
const int TX_GUARD_DELAY_MS =   20;

// Mode timing
const int CHG_MODE_MS =         300;
const int CHG_TO_RX_MODE_MS =   250;  // Max timeout time to wait for pull-down
const int RX_MODE_MS =          250 - RX_GUARD_DELAY_MS;
const int RX_TO_TX_MODE_MS =    1;
const int TX_MODE_MS =          90;
const int TX_TO_CHG_MODE_MS =   30;
const int RST_TO_CHG_MODE_MS =  100;

// Characters
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

  // Timing debug pins
  pinMode(SCOPE_TX_MODE, OUTPUT);
  pinMode(SCOPE_RX_MODE, OUTPUT);
  pinMode(SCOPE_CHG_MODE, OUTPUT);

  // LED Setup
  pinMode(CHG_LED, OUTPUT);
  digitalWrite(CHG_LED, LOW);
  RGB.control(true);

  // Start the cycle
  configureChgMode();
}

void configureChgMode() {
  mConfiguring = true;
  digitalWrite(SCOPE_CHG_MODE, HIGH);

  Serial1.end();
  RGB.color(255,0,0);
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
  digitalWrite(SCOPE_CHG_MODE, LOW);

  digitalWrite(CHG_CONTROL, LOW);
  digitalWrite(TX_CONTROL, LOW);
  // delay(1);
  digitalWrite(RX_CONTROL, HIGH);
  pinMode(UART_RX, INPUT_PULLUP);

  mCurrentMode = MODE_CHG_TO_RX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureRxMode() {
  mConfiguring = true;
  // digitalWrite(SCOPE_RX_MODE, HIGH);

  RGB.color(0,255,0);
  delay(RX_GUARD_DELAY_MS); // Capacitor discharge delay
  digitalWrite(SCOPE_RX_MODE, HIGH);
  Serial1.begin(DUT_BAUDRATE);

  mCurrentMode = MODE_RX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void configureRxToTxMode() {
  mConfiguring = true;
  digitalWrite(SCOPE_RX_MODE, LOW);

  // Serial1.end();
  RGB.color(255,255,255);
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
  // digitalWrite(SCOPE_TX_MODE, HIGH);

  RGB.color(0,128,255);
  // Serial1.begin(DUT_BAUDRATE);
  delay(TX_GUARD_DELAY_MS);  // Guard delay
  digitalWrite(SCOPE_TX_MODE, HIGH);

  mCurrentMode = MODE_TX;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
  mReadyToSend = true;
}

void configureTxToChgMode() {
  mConfiguring = true;
  digitalWrite(SCOPE_TX_MODE, LOW);

  Serial1.end();
  digitalWrite(TX_CONTROL, LOW);  // All relays low for Specs security feature
  mCurrentMode = MODE_TX_TO_CHG;
  mStartCycleTime = mCurrentTime;
  mConfiguring = false;
}

void resetToChgMode() {
  mConfiguring = true;
  Serial1.end();
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

  // while (Serial1.available() > 0) {
  //   char rxChar = Serial1.read();
  //   Serial.write(rxChar);
  // }

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

      // Use UART_RX low to synchronize time between the DUCC and Specs
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
        // if (rxChar != NULL_CHAR) {
          mRxFifo.add(rxChar);
          digitalWrite(CHG_LED, HIGH);
          if (mRxFifo.isFull()) {
            // Rx buffer is full, stop grabbing data from the Serial
            break;
          }
          // Serial.write(rxChar);
          mReadyToReceive = true;
        // }
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
