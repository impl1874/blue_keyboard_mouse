////////////////////////////////////////////////////////////////////
// helper to send raw HID mouse reports
////////////////////////////////////////////////////////////////////
#pragma once
#include "USBHIDMouse.h"
#include "USB.h"

extern USBHIDMouse Mouse;

// Thin wrapper for raw HID mouse reports.
// MouseReport is 4 bytes: buttons(1) + x(1) + y(1) + wheel(1)
// buttons: bit0=LEFT, bit1=RIGHT, bit2=MIDDLE
class RawMouse : public USBHIDMouse
{
public:
	// Send mouse movement/click/scroll.
	// buttons: bit0=LEFT, bit1=RIGHT, bit2=MIDDLE
	// dx, dy: signed movement (-127 to +127)
	// wheel: signed wheel delta (-127 to +127)
	void move(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
	{
		MouseReport rpt = {};
		rpt.buttons = buttons;
		rpt.x = dx;
		rpt.y = dy;
		rpt.wheel = wheel;
		this->sendReport(&rpt);
		delay(1);
	}

	// Convenience: left click
	inline void clickLeft()  { move(0x01, 0, 0, 0); }
	inline void clickRight() { move(0x02, 0, 0, 0); }
	inline void clickMiddle(){ move(0x04, 0, 0, 0); }

	// Scroll wheel only
	inline void scroll(int8_t delta) { move(0, 0, 0, delta); }
};
