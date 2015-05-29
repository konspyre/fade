/*
//
// V-USB keyboard (multimedia-only for now) using an ATTiny85
// GPL2 (V-USB constrained)
//
// Designed around the ATtiny85 with some code and documentation
// derived from Adafruit's IRKey and other places as
// noted.
//
// Hardware buttons present in this design:
// PB0 - #1
// PB3 - #2
// PB4 - #3
//
// The ATTiny85's internal pull-ups are connected to these pins.
//
*/

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include <util/delay.h>

#include "usbconfig.h"
#include "usbdrv/usbdrv.h"

#define KEY_VOL_UP   PB0 // PCB: Pin 1
#define KEY_VOL_MUTE PB3 // PCB: Pin 2
#define KEY_VOL_DOWN PB4 // PCB: Pin 3

/*
// Keycodes issued by this code:
// Reference: p.77 & p.78 of Hut1_12v2.pdf
// Mute: 0xE3
// Volume Up: 0xE9
// Volume Down: 0xEA
*/

#define KEYCODE_VOL_UP   0xE9
#define KEYCODE_VOL_MUTE 0xE3
#define KEYCODE_VOL_DOWN 0xEA

#define STATE_WAIT    0
#define STATE_SEND    1
#define STATE_RELEASE 2

/*
// Custom descriptor stored in flash memory
// See: http://vusb.wikidot.com/examples
// Descriptors can be found in HID1_11.pdf - "Device Class Definition
// for Human Interface Devices (HID)"
// Appendix B: Boot Interface Descriptors
// USB.org also provides a Windows only tool that can be used to generate
// descriptors:
// http://www.usb.org/developers/hidpage/
//
// Frank's tutorial on how to build these:
// http://eleccelerator.com/tutorial-about-usb-hid-report-descriptors/
//
// More documentation:
// http://www.usb.org/developers/hidpage/Hut1_12v2.pdf
//
*/

// Derived from the IRKey
const PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
  0x05, 0x0C,           // USAGE_PAGE (Consumer Devices)
  0x09, 0x01,           // USAGE (Consumer Control)
  0xA1, 0x01,           // COLLECTION (Application)
  0x85, 0x01,            //   REPORT_ID
  0x19, 0x00,           //   USAGE_MINIMUM (Unassigned)
  0x2A, 0x3C, 0x02,     //   USAGE_MAXIMUM
  0x15, 0x00,           //   LOGICAL_MINIMUM (0)
  0x26, 0x3C, 0x02,     //   LOGICAL_MAXIMUM
  0x95, 0x08,           //   REPORT_COUNT (1)
  0x75, 0x10,           //   REPORT_SIZE (16)
  0x81, 0x00,           //   INPUT (Data,Ary,Abs)
  0xC0,                 // END_COLLECTION
};

static uchar   reportBuffer[2];
static uchar   idleRate = 500 / 4;   // idleRate is defined in 4ms increments.
                                     // We need 500 ms to satisfy 7.2.4
                                     // HID1_11.pdf
static uint8_t protocolVersion = 0;  // 0 = boot protocol (7.2.6, HID1_11.pdf)

uchar lastKey = 0;

/*
// usbFunctionSetup: Handle incoming responses from the system.
// From: http://vusb.wikidot.com/driver-api (code styling derived from Adafruit IRKey)
*/

USB_PUBLIC uchar usbFunctionSetup(uchar data[8]) {
  usbRequest_t *rq = (void *)data;

  // Reject if not class request
  if((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_CLASS) {
      return 0;
  }

  switch(rq->bRequest) {

    case USBRQ_HID_GET_REPORT:
      usbMsgPtr = &reportBuffer;
      return sizeof(reportBuffer);

    case USBRQ_HID_GET_IDLE:
      usbMsgPtr = &idleRate;
      return 1;

    case USBRQ_HID_SET_IDLE:
      idleRate = rq->wValue.bytes[1];
      return 0;

    case USBRQ_HID_GET_PROTOCOL:
      usbMsgPtr = &protocolVersion;
      return 1;

    case USBRQ_HID_SET_PROTOCOL:
      protocolVersion = rq->wValue.bytes[1];
      return 0;

    default:
      return 0;

  }
}

int main() {
    uchar   bounce_timer = 0, state = STATE_WAIT, calibrationValue;

    // Set pins to input
    DDRB &= ~(1 << KEY_VOL_UP);
    DDRB &= ~(1 << KEY_VOL_MUTE);
    DDRB &= ~(1 << KEY_VOL_DOWN);

    // Enable built-in pull-ups on input lines
    PORTB |= (1 << KEY_VOL_UP);
    PORTB |= (1 << KEY_VOL_MUTE);
    PORTB |= (1 << KEY_VOL_DOWN);

    /*
    // Begin re-enumeration in case of a sudden device CPU reset.
    // Reason: The host doesn't know if we crashed or not, and can end up talking
    // to an endpoint that we are no longer on.
    */
    wdt_disable();

    calibrationValue = eeprom_read_byte(0); /* calibration value from last time */
    if(calibrationValue != 0xFF){
        OSCCAL = calibrationValue;
    }

    usbDeviceDisconnect();
    _delay_ms(250);
    usbDeviceConnect();
    usbInit();

    // End re-enumeration (main code path follows)

    while(1) {
      usbPoll();

      // Listen for button pushes
      if(!(PINB & (1<<KEY_VOL_UP))) {
        if(state == STATE_WAIT && bounce_timer == 255) {
            state = STATE_SEND;
            lastKey = KEYCODE_VOL_UP;
            bounce_timer = 0;
        }
      }

      if(!(PINB & (1<<KEY_VOL_MUTE))) {
        if(state == STATE_WAIT && bounce_timer == 255) {
            state = STATE_SEND;
            lastKey = KEYCODE_VOL_MUTE;
            bounce_timer = 0;
        }
      }

      if(!(PINB & (1<<KEY_VOL_DOWN))) {
        if(state == STATE_WAIT && bounce_timer == 255) {
            state = STATE_SEND;
            lastKey = KEYCODE_VOL_DOWN;
            bounce_timer = 0;
        }
      }

      if(bounce_timer < 255) {
        bounce_timer++;
      }

      if(usbInterruptIsReady() && state != STATE_WAIT) {

        switch(state) {
          case STATE_SEND:
            reportBuffer[0] = 0x01;    // Report ID
            reportBuffer[1] = lastKey; // Keycode to send
            state = STATE_RELEASE;
            break;

          case STATE_RELEASE:
            reportBuffer[0] = 0x01;
            reportBuffer[1] = 0;
            state = STATE_WAIT;
            break;

          default:
            state = STATE_WAIT;
        }

        usbSetInterrupt(reportBuffer,sizeof(reportBuffer));
        // Wipe buffer
        reportBuffer[0] = reportBuffer[1] = 0;

      }
  }

  return 0;

}

/*
// Clock tuning on USB reset, dependent on provided frame
// From: http://vusb.wikidot.com/examples
*/
void calibrateOscillator() {
  uchar       step = 128;
  uchar       trialValue = 0, optimumValue;
  int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
  /* do a binary search: */
  do {
    OSCCAL = trialValue + step;
    x = usbMeasureFrameLength();    /* proportional to current real frequency */
    if(x < targetValue) {             /* frequency still too low */
      trialValue += step;
    }
    step >>= 1;
  } while(step > 0);
  /* We have a precision of +/- 1 for optimum OSCCAL here */
  /* now do a neighborhood search for optimum value */
  optimumValue = trialValue;
  optimumDev = x; /* this is certainly far away from optimum */
  for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
    x = usbMeasureFrameLength() - targetValue;
    if(x < 0)
    x = -x;
    if(x < optimumDev){
      optimumDev = x;
      optimumValue = OSCCAL;
    }
  }

  OSCCAL = optimumValue;

}

void usbEventResetReady(void)
{
  cli();  // usbMeasureFrameLength() counts CPU cycles, so disable interrupts.
  calibrateOscillator();
  sei();
  eeprom_write_byte(0, OSCCAL);   // store the calibrated value in EEPROM
}
