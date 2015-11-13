#include "application.h"

#include "Serial3/Serial3.h"
#include "core_hal.h"

SYSTEM_MODE(MANUAL);

#define now() millis()
uint32_t lastFlash = now();
String com = "";
#define PASS_GREEN() RGB.color(0,255,0)
#define FAIL_RED() RGB.color(255,0,0)
#define FAIL_BLUE() RGB.color(0,0,255)
#define RGB_OFF() RGB.color(0,0,0)

int8_t testUbloxIsReset();
void   clearUbloxBuffer();
int8_t sendATcommand(const char* ATcommand, const char* expected_answer1, system_tick_t timeout);
int8_t testATOK(system_tick_t timeout);

void setup()
{
	RGB.control(true);

	STM32_Pin_Info* PIN_MAP2 = HAL_Pin_Map();
	// This pin tends to stay low when floating on the output of the buffer (PWR_UB)
	// It shouldn't hurt if it goes low temporarily on STM32 boot, but strange behavior
	// was noticed when it was left to do whatever it wanted so by adding a 100k pull up
	// all flakey behavior has ceased like the modem just stopped responding to AT commands.
	PIN_MAP2[PWR_UC].gpio_peripheral->BSRRL = PIN_MAP2[PWR_UC].gpio_pin;
	pinMode(PWR_UC, OUTPUT);
	// This pin tends to stay high when floating on the output of the buffer (RESET_UB),
	// but we need to ensure it gets set high before being set to an OUTPUT.
	// If this pin goes LOW, the modem will be reset and all configuration will be
	PIN_MAP2[RESET_UC].gpio_peripheral->BSRRL = PIN_MAP2[RESET_UC].gpio_pin;
	pinMode(RESET_UC, OUTPUT);
	//digitalWrite(RESET_UC, HIGH);
	//digitalWrite(PWR_UC, HIGH);
	pinMode(RTS_UC, OUTPUT);
	digitalWrite(RTS_UC, LOW); // VERY IMPORTANT FOR CORRECT OPERATION!!
	pinMode(LVLOE_UC, OUTPUT);
	digitalWrite(LVLOE_UC, LOW); // VERY IMPORTANT FOR CORRECT OPERATION!!

	Serial.begin(9600);
	Serial3.begin(9600);

	// TEST RGB LED
	FAIL_RED();
	delay(100);
	PASS_GREEN();
	delay(100);
	FAIL_BLUE();
	delay(100);
	RGB_OFF();
	delay(500);

	// With the new 100k pull up resistors added to RESET_UC and PWR_UC, the modem will
	// not power up unless specifically commanded to do so.  So let's power it up!
	int i = 0;
    while (i++ < 10) {
    	Serial.print("Modem::powerOn attempt ");
    	Serial.println(i);

        // SARA-U2/LISA-U2 50..80us
        HAL_GPIO_Write(PWR_UC, 0); HAL_Delay_Milliseconds(50);
        HAL_GPIO_Write(PWR_UC, 1); HAL_Delay_Milliseconds(10);

        // SARA-G35 >5ms, LISA-C2 > 150ms, LEON-G2 >5ms
        HAL_GPIO_Write(PWR_UC, 0); HAL_Delay_Milliseconds(150);
        HAL_GPIO_Write(PWR_UC, 1); HAL_Delay_Milliseconds(100);

        // purge any messages
        clearUbloxBuffer();

        // check interface
        if(!testATOK(1000)) FAIL_RED();
        else {
            PASS_GREEN();
            break;
        }
    }
    if (i >= 10) {
        Serial.println("No Reply from Modem");
    }
}

void loop()
{
	if ( Serial.available() ) {
		char c = Serial.read();
		com += c;
		Serial.write(c); //echo input
		if (c == '\r') {
			// Serial.print("Command: ");
			// Serial.println(com);
			Serial3.println(com);
			com = "";
		}
		// Serial3.write(c);
	}

	if (Serial3.available()) {
		char c = Serial3.read();
		Serial.write(c);
	}
}

void clearUbloxBuffer() {
  while (Serial3.available()) {
	Serial3.read();
  }
}

int8_t sendATcommand(const char* ATcommand, const char* expected_answer1, system_tick_t timeout) {
	uint8_t x = 0, rv = 0;
	char response[100];
	system_tick_t previous;

	memset(response, '\0', 100);
	delay(100);

	clearUbloxBuffer();

	Serial3.println(ATcommand);

	previous = now();
	do {
		if (Serial3.available() != 0) {
			response[x] = Serial3.read();
			x++;

			// check whether we've received the expected answer
			if (strstr(response, expected_answer1) != NULL) {
				rv = 1;
			}
		}
	} while ((rv == 0) && ((now() - previous) < timeout));

	//Serial.print("\e[0;35m");
	//Serial.println(response);
	return rv;
}

int8_t testATOK(system_tick_t timeout) {
	int8_t rv = sendATcommand("AT", "OK", timeout);
	return rv;
}
