# DY-XXXX UART player for PIC

This library provides a C API for controlling DY-XXXX audio modules through
their UART protocol. It is currently intended for PIC microcontrollers using
Microchip XC8.

The implementation is hardware-independent at the UART boundary: the
application supplies callbacks for transmitting and, when needed, receiving
bytes. No dynamic allocation is used by the PIC port.

The library supports the UART protocol only. It does not support the
`ONE_Line` protocol.

## Supported modules

The protocol is used by several DY-XXXX and JQ8400-family modules. The exact
features and electrical limits depend on the board, so check the datasheet for
the module being used.

The following modules were listed as compatible with the original protocol
library. They are a compatibility reference, not a guarantee that every model
or every feature has been tested with this PIC port.

| Model    | Storage / capacity | SD support | Amplifier       | Supply |
| :------- | :----------------- | :--------- | :-------------- | :----- |
| DY-SV17F | 32 Mbit            | No         | 3-5 W (4/8 ohm) | 5 VDC  |
| DY-SV8F  | 64 Mbit            | No         | 3-5 W (4/8 ohm) | 5 VDC  |
| DY-HV20T | External storage   | Yes, up to 32 GB | 3-5 W (4/8 ohm) | 5 VDC |
| DY-HV8F  | 8 Mbit             | No         | 10 W (8 ohm), 20 W (4 ohm) | 6-35 VDC |
| DY-SV5W  | External storage   | Yes, up to 32 GB | 3-5 W (4/8 ohm) | 5 VDC |

The original library was tested mainly with a DY-SV17F and DY-SV5W. Verify
power, protocol compatibility and command behavior on the actual board.

## Wiring

If the board has DIP switches, set `CON3` to on and leave `CON1` and `CON2`
off. If the board has no DIP switches, connect a `10KOhm` resistor from each
`CON#` pin as follows:

| Module pin | Connect to | Via      |
| :--------- | :--------- | :------- |
| `CON1`     | `GND`      | `10KOhm` |
| `CON2`     | `GND`      | `10KOhm` |
| `CON3`     | `3.3V`     | `10KOhm` |

The board normally exposes the 3.3 V point needed for `CON3`. Further
connections are required:

| Module pin | Connect to | Via                         |
| :--------- | :--------- | :-------------------------- |
| `V?`       | `V+`       | Voltage depends on the board |
| `GND`      | `GND`      |                             |
| `IO0/TX`   | PIC RX     | `1KOhm` if using a 5 V board |
| `IO1/RX`   | PIC TX     | `1KOhm` if using a 5 V board |
| `SPK+`     | Speaker +  |                             |
| `SPK-`     | Speaker -  |                             |

Check the logic levels in the module datasheet. A 5 V UART signal may damage
a 3.3 V input; use the recommended series resistors or level conversion.

## Implementation layout

- `src/DYPlayer_PIC.h`: public C types, constants and function declarations.
- `src/DYPlayer_PIC.c`: PIC implementation.
- `CMakeLists.txt`: builds the static `dyplayer` library from the PIC source.

The `examples/` directory still contains legacy platform examples. They are
not part of the current PIC API documentation and have intentionally not been
updated here.

## Initialisation and UART callbacks

The write callback is required. The read callback may be `NULL` when the
application only uses commands that do not require a response.

```c
#include "DYPlayer_PIC.h"

static void uart_write(const uint8_t *buffer, uint8_t size)
{
    /* Send exactly size bytes through the PIC UART. */
}

static bool uart_read(uint8_t *buffer, uint8_t size)
{
    /* Read exactly size bytes before returning true on success. */
    return false;
}

dy_player_t player;

if (!DYPlayer_init(&player, uart_write, uart_read, 0)) {
    /* Invalid player or missing write callback. */
}
```

The callback structure is:

```c
typedef void (*dy_uart_write_fn_t)(const uint8_t *buffer, uint8_t size);
typedef bool (*dy_uart_read_fn_t)(uint8_t *buffer, uint8_t size);
```

`DYPlayer_init()` stores the callbacks and option flags in `dy_player_t`. It
returns `false` when `player` or `uart_write_fn` is `NULL`. Response-reading
functions return their documented failure value when `uart_read` is `NULL`,
when the callback fails, or when the response checksum is invalid.

Available options can be combined with bitwise OR:

| Option              | Value  | Meaning |
| :------------------ | :----- | :------ |
| `OPTION_AUX_IN`     | `0x01` | The module has an AUX input. |
| `OPTION_ONLY_FLASH` | `0x02` | Reject commands targeting USB or SD. |

For example:

```c
uint8_t options = OPTION_AUX_IN | OPTION_ONLY_FLASH;
DYPlayer_init(&player, uart_write, uart_read, options);
```

## UART protocol

Commands use this frame format:

```text
AA [command] [length] [data...] [checksum]
```

- `AA` is the start byte (`0xAA`).
- `command` identifies the operation.
- `length` is the number of data bytes.
- `data` contains the command arguments.
- `checksum` is the 8-bit sum of every preceding frame byte.

For example, playing file number `1` sends command `0x07` with two data bytes:

```text
AA 07 02 00 01 checksum
```

The checksum is allowed to wrap at 8 bits. Responses are validated in the
same way before their values are returned to the application.

## Devices and return values

Storage device constants are plain C values of type `device_t`:

| Constant            | Value  | Meaning |
| :------------------ | :----- | :------ |
| `DEVICE_USB`        | `0x00` | USB storage. |
| `DEVICE_SD`         | `0x01` | SD card. |
| `DEVICE_FLASH`      | `0x02` | On-board Flash storage. |
| `DEVICE_FAIL`       | `0xFE` | UART or response failure. |
| `DEVICE_NO_DEVICE`  | `0xFF` | No storage device detected. |

Play state constants are values of type `play_state_t`:

| Constant             | Value | Meaning |
| :------------------- | :---- | :------ |
| `PLAY_STATE_FAIL`    | `-1`  | UART or checksum failure. |
| `PLAY_STATE_STOPPED` | `0`   | Playback stopped. |
| `PLAY_STATE_PLAYING` | `1`   | Playback active. |
| `PLAY_STATE_PAUSED`  | `2`   | Playback paused. |

Numeric response functions return `0` on failure. Consequently, callers that
need to distinguish a valid zero from an error should also check the UART or
perform an independent device-status check.

## Command reference

The command codes used by the PIC implementation are:

| Command                         | Code |
| :------------------------------ | :--- |
| Check play state                | `0x01` |
| Play                            | `0x02` |
| Pause                           | `0x03` |
| Stop                            | `0x04` |
| Previous music                 | `0x05` |
| Next music                     | `0x06` |
| Play specified music            | `0x07` |
| Play specified device and path  | `0x08` |
| Get playing device              | `0x0A` |
| Set playing device              | `0x0B` |
| Get sound count                | `0x0C` |
| Get playing sound              | `0x0D` |
| Previous directory              | `0x0E` or `0x0F` |
| Stop interlude                 | `0x10` |
| Get first sound in directory    | `0x11` |
| Get directory sound count      | `0x12` |
| Set volume                     | `0x13` |
| Increase volume                | `0x14` |
| Decrease volume                | `0x15` |
| Play specified interlude       | `0x16` |
| Play interlude by path         | `0x17` |
| Set cycle mode                 | `0x18` |
| Set cycle times                | `0x19` |
| Set equalizer                 | `0x1A` |
| Combination play               | `0x1B` |
| End combination play           | `0x1C` |
| Select specified sound         | `0x1F` |

## Playback and file naming

For normal playback, files are commonly named sequentially:

```text
00001.mp3
00002.mp3
00003.mp3
...
65535.mp3
```

`DYPlayer_playSpecified()` sends a numeric file index. The module may use the
order in which files were written to the storage device rather than the
numeric filename itself. File number `1` therefore does not necessarily mean
the file literally named `00001.mp3`.

For predictable ordering:

1. Empty or format the storage device.
2. Remove unrelated or temporary files.
3. Copy files in the intended playback order.

## Path playback

The public API accepts a normal, null-terminated C string:

```c
DYPlayer_playSpecifiedDevicePath(
    &player,
    DEVICE_FLASH,
    "/SONGS/FILE.MP3"
);
```

The module protocol requires a converted path. The PIC implementation performs
this conversion while sending, without allocating a second path buffer:

```text
/SONGS/IN/A/PATH/00001.MP3
/SONGS*/IN*/A*/PATH*/00001*MP3
```

The initial `/` is preserved. Every later `/` is sent as `*/`, every `.` is
sent as `*`, and letters are converted to uppercase. The transformed path
length includes the inserted `*` bytes and must not exceed
`DY_MAX_PATH_LEN` (`36` by default). Oversized, empty or `NULL` paths are
rejected before transmission.

The module manual commonly limits directory and file components to eight
characters. Keep paths short and avoid unnecessary nesting. The public path
functions do not return an error status, so applications should validate path
lengths before calling them when the result matters.

When `OPTION_ONLY_FLASH` is enabled, path and interlude commands targeting
`DEVICE_USB` or `DEVICE_SD` are rejected. They are not silently redirected to
Flash.

## Combination play

Combination play creates a playlist from two-character filenames. For example,
use files named `01.mp3`, `A2.mp3` and `B7.mp3` in the special directory
required by the module firmware. Common directory names are `DY`, `XY` and
`ZH`; the correct name depends on the module.

The PIC API uses a contiguous array of two-byte entries:

```c
const char sounds[][2] = {
    {'0', '1'},
    {'A', '2'},
    {'B', '7'}
};

DYPlayer_combinationPlay(&player, sounds, 3);
```

The module receives the ASCII bytes `0`, `1`, `A`, `2`, `B` and `7`. A C string
terminator is neither required nor transmitted. The number of entries must not
exceed `127`.

## Directories, interludes and controls

The API also provides:

- `DYPlayer_setPlayingDevice()` to select USB, SD or Flash;
- `DYPlayer_previousDir()` with `PREVIOUS_DIR_FIRST_SOUND` or
  `PREVIOUS_DIR_LAST_SOUND`;
- `DYPlayer_getFirstInDir()` and `DYPlayer_getSoundCountDir()`;
- `DYPlayer_interludeSpecified()` and
  `DYPlayer_interludeSpecifiedDevicePath()`;
- `DYPlayer_stopInterlude()`;
- `DYPlayer_setVolume()`, `DYPlayer_volumeIncrease()` and
  `DYPlayer_volumeDecrease()`;
- `DYPlayer_setCycleMode()` and `DYPlayer_setCycleTimes()`;
- `DYPlayer_setEq()`;
- `DYPlayer_select()` and `DYPlayer_endCombinationPlay()`.

An interlude temporarily plays a sound over the current playback. The module
manual describes this as a single interlude level; a new interlude can replace
the active one. `DYPlayer_stopInterlude()` stops the interlude and resumes
normal playback when supported by the module.

Volume is normally in the range `0` to `30`, with a typical default of `20`.
The cycle-time command applies to the repeat modes.

## Enumerations

Equalizer values:

| Constant       | Value |
| :------------- | :---- |
| `EQ_NORMAL`    | `0x00` |
| `EQ_POP`       | `0x01` |
| `EQ_ROCK`      | `0x02` |
| `EQ_JAZZ`      | `0x03` |
| `EQ_CLASSIC`   | `0x04` |

Playback modes:

| Constant               | Value | Meaning |
| :--------------------- | :---- | :------ |
| `PLAY_MODE_REPEAT`     | `0x00` | Repeat all music. |
| `PLAY_MODE_REPEAT_ONE` | `0x01` | Repeat the current sound. |
| `PLAY_MODE_ONE_OFF`    | `0x02` | Play once and stop. |
| `PLAY_MODE_RANDOM`     | `0x03` | Play random sounds. |
| `PLAY_MODE_REPEAT_DIR` | `0x04` | Repeat the current directory. |
| `PLAY_MODE_RANDOM_DIR` | `0x05` | Play random sounds in the current directory. |
| `PLAY_MODE_SEQUENCE_DIR` | `0x06` | Play the current directory in sequence, then stop. |
| `PLAY_MODE_SEQUENCE`   | `0x07` | Play the device in sequence, then stop. |

Previous-directory behavior:

| Constant                    | Value | Meaning |
| :-------------------------- | :---- | :------ |
| `PREVIOUS_DIR_FIRST_SOUND`  | `0x00` | Start at the first sound. |
| `PREVIOUS_DIR_LAST_SOUND`   | `0x01` | Start at the last sound. |

## Public API

All declarations are in `src/DYPlayer_PIC.h`.

```c
bool DYPlayer_init(dy_player_t *player,
                   dy_uart_write_fn_t uart_write_fn,
                   dy_uart_read_fn_t uart_read_fn,
                   uint8_t options);

play_state_t DYPlayer_getPlayState(dy_player_t *player);
void DYPlayer_play(dy_player_t *player);
void DYPlayer_pause(dy_player_t *player);
void DYPlayer_stop(dy_player_t *player);
void DYPlayer_previous(dy_player_t *player);
void DYPlayer_next(dy_player_t *player);
void DYPlayer_playSpecified(dy_player_t *player, uint16_t number);
void DYPlayer_playSpecifiedDevicePath(dy_player_t *player,
                                      device_t device,
                                      const char *path);

device_t DYPlayer_getPlayingDevice(dy_player_t *player);
void DYPlayer_setPlayingDevice(dy_player_t *player, device_t device);
uint16_t DYPlayer_getSoundCount(dy_player_t *player);
uint16_t DYPlayer_getPlayingSound(dy_player_t *player);
void DYPlayer_previousDir(dy_player_t *player, playDirSound_t song);
uint16_t DYPlayer_getFirstInDir(dy_player_t *player);
uint16_t DYPlayer_getSoundCountDir(dy_player_t *player);

void DYPlayer_setVolume(dy_player_t *player, uint8_t volume);
void DYPlayer_volumeIncrease(dy_player_t *player);
void DYPlayer_volumeDecrease(dy_player_t *player);
void DYPlayer_interludeSpecified(dy_player_t *player,
                                 device_t device,
                                 uint16_t number);
void DYPlayer_interludeSpecifiedDevicePath(dy_player_t *player,
                                           device_t device,
                                           const char *path);
void DYPlayer_stopInterlude(dy_player_t *player);
void DYPlayer_setCycleMode(dy_player_t *player, play_mode_t mode);
void DYPlayer_setCycleTimes(dy_player_t *player, uint16_t cycles);
void DYPlayer_setEq(dy_player_t *player, eq_t eq);
void DYPlayer_select(dy_player_t *player, uint16_t number);
void DYPlayer_combinationPlay(dy_player_t *player,
                              const char (*sounds)[2],
                              uint8_t size);
void DYPlayer_endCombinationPlay(dy_player_t *player);
```

## Troubleshooting

### No communication

- Check module power and ground.
- Check that PIC TX connects to module RX and PIC RX connects to module TX.
- Check UART voltage levels and series resistors.
- Check the `CON#` configuration or DIP switches.
- Confirm that the callbacks use the correct hardware UART.
- Ensure `uart_write` transmits every requested byte in order.
- Ensure `uart_read` waits for the complete response and returns `true` only
  when all requested bytes were received.
- Disconnect any programmer or other UART device that shares the module UART.

### No sound

- Check the speaker connection and volume.
- Verify that the storage device contains compatible files.
- Start with `DYPlayer_playSpecified()` before testing path playback.
- Remember that numeric playback follows storage write order, not necessarily
  the visible filename number.
- Check path spelling, path length and the transformed path format.
- For combination play, check the two-character filenames and the special
  directory name required by the module.

### Response functions return failure

- Confirm that `uart_read` is not `NULL`.
- Confirm that the callback reads exactly the requested number of bytes.
- Check that the module response is not being consumed by another UART user.
- Check the response checksum and UART baud/configuration.
