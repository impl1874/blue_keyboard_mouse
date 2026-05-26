# Mouse Support Design — BlueKeyboard/BluKeyborg

## Status: Implemented

This document describes the mouse support implementation added to the BlueKeyboard/BluKeyborg system.

## Overview

Mouse support was added as a single composite BLE command (opcode 0xE1) carrying buttons + X/Y movement + wheel in one MTLS-protected frame. The implementation mirrors the existing keyboard architecture while keeping mouse simple and efficient.

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|------------|
| **Protocol** | Single opcode 0xE1, 4-byte payload | One round-trip for all mouse data |
| **Security** | MTLS-protected (like 0xD0) | Mouse intercept could be sensitive; fire-and-forget still applies |
| **USB HID** | ESP32 `USBHIDMouse` via `RawMouse` wrapper | Native TinyUSB support, no extra HID descriptor needed |
| **Payload** | `[buttons][dx][dy][wheel]` | Standard 4-byte HID mouse report format |

## Protocol Binary Format

```
Command frame (inside B3 MTLS wrapper):
0xE1 [0x04 0x00] [buttons][dx][dy][wheel]
 ^^ opcode        ^^ len=4 LE   ^^ 4 bytes
```

- `buttons`: bitmask — bit0=LEFT, bit1=RIGHT, bit2=MIDDLE
- `dx`, `dy`, `wheel`: signed int8 (-127 to +127)

**No response** — fire-and-forget, like 0xE0 keyboard raw.

## HID Report Structure

Standard HID Mouse Report (4 bytes):

| Byte | Field | Type | Range |
|------|-------|------|-------|
| 0 | Buttons | uint8 | bitmask |
| 1 | X movement | int8 | -127 to +127 |
| 2 | Y movement | int8 | -127 to +127 |
| 3 | Wheel | int8 | -127 to +127 |

## Files Created

### `blue_keyboard/RawMouse.h` (NEW)

Parallel to `RawKeyboard.h`:

```cpp
class RawMouse : public USBHIDMouse {
public:
    void move(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);
    inline void clickLeft()  { move(0x01, 0, 0, 0); }
    inline void clickRight() { move(0x02, 0, 0, 0); }
    inline void clickMiddle(){ move(0x04, 0, 0, 0); }
    inline void scroll(int8_t delta) { move(0, 0, 0, delta); }
};
```

## Files Modified

### Firmware

| File | Changes |
|------|---------|
| `blue_keyboard/commands.h` | Added `#include "RawMouse.h"`, `extern RawMouse Mouse`, opcode 0xE1 handler in `handle_mtls_ops()` |
| `blue_keyboard/blue_keyboard.ino` | Added `RawMouse Mouse;` global, `Mouse.begin();` in both display/no-display setup branches |

### Mobile Clients

| File | Changes |
|------|---------|
| `apps/android/BleHub.kt` | Added `sendRawMouseEvent()`, `clickMouseLeft()`, `clickMouseRight()`, `clickMouseMiddle()`, `scrollMouse()` |
| `apps/ios/BleHub.swift` | Added `sendRawMouseEvent()`, `clickMouseLeft()`, `clickMouseRight()`, `clickMouseMiddle()`, `scrollMouse()` |

### Linux Client

| File | Changes |
|------|---------|
| `apps/linux/src/ble_proto.h` | Added `send_mouse_event()` declaration |
| `apps/linux/src/ble_proto.cpp` | Added `send_mouse_event()` implementation |

## Firmware Opcode 0xE1 Handler

```cpp
// :: RAW_MOUSE_EVENT (0xE1)
if( op == 0xE1 ) {
    if( n < 4 ) {
        sendFrame(0xFF, (const uint8_t*)"bad len", 7);
        return true;
    }
    uint8_t buttons = p[0];
    int8_t  dx      = static_cast<int8_t>(p[1]);
    int8_t  dy      = static_cast<int8_t>(p[2]);
    int8_t  wheel   = static_cast<int8_t>(p[3]);
    Mouse.move(buttons, dx, dy, wheel);
    return true;  // fire-and-forget
}
```

## Client API (Android/Kotlin)

```kotlin
// Full mouse event
fun sendRawMouseEvent(
    buttons: Int,  // bit0=LEFT, bit1=RIGHT, bit2=MIDDLE
    dx: Int,       // -127 to +127
    dy: Int,       // -127 to +127
    wheel: Int,    // -127 to +127
    onResult: (Boolean, String?) -> Unit
)

// Convenience helpers
fun clickMouseLeft(onResult: (Boolean, String?) -> Unit)
fun clickMouseRight(onResult: (Boolean, String?) -> Unit)
fun clickMouseMiddle(onResult: (Boolean, String?) -> Unit)
fun scrollMouse(delta: Int, onResult: (Boolean, String?) -> Unit)
```

All methods reuse the existing `sendAppFrame(0xE1, payload)` infrastructure — no crypto changes needed.

## Out of Scope

- Mobile app UI for mouse control (sliders, touchpad)
- Absolute positioning (only relative movement)
- Multiple mouse button combinations beyond left/right/middle
- Consumer control media keys (already exists via `sendConsumerUsage()`)

## Verification

1. **Firmware build**: `cd blue_keyboard && arduino-cli compile` — verify no compile errors
2. **Firmware flash**: Flash to device, verify USB HID mouse enumerates
3. **Protocol test**: Use Linux CLI `send_mouse_event()` call
