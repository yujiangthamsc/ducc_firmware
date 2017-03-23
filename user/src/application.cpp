/*
 *******************************************************************************
 Dual UART Charge/Communication Software

 Snap, Inc. 2017
 Yu Jiang Tham
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "application.h"
#include "stdarg.h"

PRODUCT_ID(PLATFORM_ID);
PRODUCT_VERSION(3);

/* Constants -----------------------------------------------------------------*/
int BAUDRATE = 9600;
int BLUELED = D7;

/* Function prototypes -------------------------------------------------------*/
#if Wiring_WiFi
STARTUP(System.enable(SYSTEM_FLAG_WIFITESTER_OVER_SERIAL1));
#endif

SYSTEM_MODE(AUTOMATIC);

/* This function is called once at start up ----------------------------------*/
void setup()
{
    //
    Serial.begin(BAUDRATE);   // USB serial port to computer
    Serial1.begin(BAUDRATE);  // Serial to charge port

    // LED Setup
    pinMode(BLUELED, OUTPUT);
}

/* This function loops forever --------------------------------------------*/
void loop()
{
    //This will run in a loop
    Serial.println("Test");
    Serial1.println("Toast");
    digitalWrite(BLUELED, HIGH);
    delay(50);
    // digitalWrite(BLUELED, LOW);
    // delay(50);
}
