# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cd blue_keyboard_mouse/blue_keyboard
arduino-cli compile --fqbn esp32:esp32:esp32s3
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3
```

Requirements: ESP32-S3 board support, TFT_eSPI (LilyGO variant), NimBLE-Arduino, FastLED, Adafruit_SPIFlash.

## Project Overview

ESP32-S3 firmware for the Blue Keyboard / BluKeyborg USB HID dongle. Acts as a secure BLE → USB HID bridge: receives encrypted commands over BLE from a mobile app, decrypts them, and emits USB keyboard/mouse events on the host computer.

## Architecture

```
BLE (phone) ──► NimBLEServer ──► commands.h (dispatch)
                                          │
                                          ├─► handle_appkey_ops()  [A0/A2/A3] — pre-MTLS onboarding
                                          ├─► handle_mtls_ops()   [C0/C1/C4/D0/E0/E1] — MTLS-protected
                                          └─► mtls.cpp/h          [B0/B1/B2/B3] — session layer
                                                    │
                                          RawKeyboard/RawMouse ──► USB HID
```

## Key Files

| File | Role |
|------|------|
| `blue_keyboard.ino` | Main loop, setup, BLE callbacks, USB init |
| `commands.h` | Protocol dispatcher: all opcode handlers (A*, B*, C*, D*, E*). `dispatch_binary_frame()` is the main entry point |
| `mtls.cpp/h` | MTLS session: B0/B1/B2/B3 handling, key derivation (HKDF), AES-CTR + HMAC |
| `RawKeyboard.h` | USB HID keyboard wrapper (sends HID key reports via `sendReport()`) |
| `RawMouse.h` | USB HID mouse wrapper (sends HID mouse reports: buttons + x/y/wheel) |
| `layout_kb_profiles.h` | Keyboard layout enum and mapping to HID usage codes per locale |
| `kb_layouts/*` | Per-locale HID scan code tables (US, UK, DE, FR, etc. × WINLIN/MAC) |
| `setup_portal.cpp` | Wi-Fi AP setup portal for first-time provisioning |
| `settings.h` | NVS storage helpers, BLE name/layout/policy persistence |
| `usb_desc_override.c` | Custom USB device descriptor (VID/PID override for Espressif) |

## BLE Protocol Opcodes

**Pre-MTLS (APPKEY onboarding):**
| Opcode | Name | Direction |
|--------|------|-----------|
| `0xA0` | GET_APPKEY | App→Dongle | Request KDF params + challenge
| `0xA2` | APPKEY_CHALLENGE | Dongle→App | `[salt16][iters4][chal16]`
| `0xA3` | APPKEY_PROOF | App→Dongle | HMAC-SHA256(verif, "APPKEY"||chal)
| `0xA1` | APPKEY_RESPONSE | Dongle→App | Raw or wrapped AppKey

**MTLS Handshake:**
| Opcode | Name | Direction |
|--------|------|-----------|
| `0xB0` | SERVER_HELLO | Dongle→App | P-256 pubkey (65 bytes) + session ID
| `0xB1` | CLIENT_KEYX | App→Dongle | Ephemeral pub + HMAC
| `0xB2` | SERVER_FINISH | Dongle→App | HMAC confirmation
| `0xB3` | ENCRYPTED_RECORD | Both | Wrapped encrypted frame

**Application Commands (MTLS-protected):**
| Opcode | Name | Description |
|--------|------|-------------|
| `0xC0` | SET_LAYOUT | Set keyboard layout by name |
| `0xC1` | GET_INFO | Query `"LAYOUT=X; PROTO=1.6; FW=2.1.0"` |
| `0xC4` | RESET_TO_DEFAULT | Factory reset (clears AppKey) |
| `0xC8` | SET_RAW_FAST_MODE | Enable 0xE0 unencrypted fast path |
| `0xD0` | SEND_STRING | Type UTF-8 text (responds with MD5) |
| `0xE0` | RAW_KEY_TAP | Fast raw HID key (fire-and-forget, requires C8) |
| `0xE1` | RAW_MOUSE_EVENT | Mouse move/click/scroll (buttons + dx/dy/wheel) |

Binary frame: `[OP u8][LEN u16 LE][PAYLOAD...]`
Encrypted (B3): `[0xB3][LEN u16 LE][seq_be16][clen_be16][cipher][mac16]`

## Security Model

Host computer is **untrusted**. Two layers of protection:
1. BLE link encryption + bonding
2. Application MTLS: ECDHE P-256 + AES-CTR + HMAC-SHA256 + sequence counters

APPKEY retrieved via PBKDF2 challenge-response (never transmitted in clear). Dongle appears as standard USB HID keyboard — no drivers needed on host.

## Layout System

Keyboard layout correctness is handled **on the dongle** (not in the app):
- `kb_layouts/*` maps characters → HID scan codes + modifiers
- Separate variants: `WINLIN` (Windows/Linux) and `MAC`
- `layout_kb_profiles.h` defines the `KeyboardLayout` enum and `layoutName()`, `m_nKeyboardLayout`
- `sendUnicodeAware()` handles UTF-8 → HID conversion using the active layout

## Supported Hardware

- LilyGO ESP32-S3 T-Dongle (with/without display)
- LilyGO ESP32-S3 T-QT
- Waveshare ESP32-S3 1.47" Display
- Waveshare ESP32-S3 Zero
- Seeed Studio XIAO ESP32-S3

Board selection via `BLUKEY_BOARD` define in `pin_config.h`. Display support controlled by `NO_DISPLAY` flag.

## Recent Changes

- **Mouse support** (0xE1): `RawMouse.h` wraps `USBHIDMouse`, global `Mouse` instance in `blue_keyboard.ino`, handler in `commands.h`
- **Protocol version 1.6**: firmware version 2.1.0