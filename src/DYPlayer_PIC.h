/**
 * Abstraction of basic features of the DY-SV17F mp3 player board, written for
 * Arduino, should work on other frameworks as well. Instead of DY-SV17F I will
 * from here on refer to it as the "module".
 *
 * There are some virtual methods that MUST be overridden (serialRead and
 * serialWrite) and one that you may override (begin)
 */
#ifndef DYPLAYER_PIC_H
#define DYPLAYER_PIC_H

#include <stdint.h>
#include <stdbool.h>

//#define DY_PATH_LEN 20
#define DY_MAX_PATH_LEN 36

#ifdef __cplusplus
namespace DY
{
#endif

/**
  * Options for the DYPlayer module.
  * These options can be combined using bitwise OR.
  * For example, to enable both AUX_IN and ONLY_FLASH options, you would use:
  * `options = OPTION_AUX_IN | OPTION_ONLY_FLASH;`
  *
  * OPTION_AUX_IN     - The canal AUX input is present on the module.
  * OPTION_ONLY_FLASH - Only the flash storage is available on the module.
  */
typedef uint8_t dy_option_t;
enum
{
    OPTION_NONE       = 0x00,
    OPTION_AUX_IN     = 0x01,
    OPTION_ONLY_FLASH = 0x02
};


/**
  * Storage devices reported by module and to choose from when selecting a
  * storage device.
  */
typedef uint8_t device_t;

enum
{
    DEVICE_USB       = 0x00,
    DEVICE_SD        = 0x01,
    DEVICE_FLASH     = 0x02,
    DEVICE_FAIL      = 0xfe,
    DEVICE_NO_DEVICE = 0xff
};

/**
  * The current module play state.
  */
typedef int8_t play_state_t;

enum
{
    PLAY_STATE_FAIL    = -1,
    PLAY_STATE_STOPPED = 0,
    PLAY_STATE_PLAYING = 1,
    PLAY_STATE_PAUSED  = 2
};

/**
  * Equalize settings.
  */
typedef uint8_t eq_t;

enum
{
    EQ_NORMAL = 0,
    EQ_POP,
    EQ_ROCK,
    EQ_JAZZ,
    EQ_CLASSIC
};

/**
  * Play modes are basically whatever you commonly find on a media player,
  * i.e.:
  * Repeat 1, Repeat all, Repeat list (dir), playlist (by dir), random play.
  *
  * The default is perhaps somewhat unexpected: `DY::PlayMode::OneOff`. Often
  * these modules will be used in toys or information displays where you can
  * press a button and hear a corresponding sound. To get default media player
  * behaviour, you should probably set `DY::PlayMode::Sequence` to just continue
  * playing the next song until all are played or skipped, then stop.
  */
typedef uint8_t play_mode_t;

enum
{
    PLAY_MODE_REPEAT = 0,
    PLAY_MODE_REPEAT_ONE,
    PLAY_MODE_ONE_OFF,
    PLAY_MODE_RANDOM,
    PLAY_MODE_REPEAT_DIR,
    PLAY_MODE_RANDOM_DIR,
    PLAY_MODE_SEQUENCE_DIR,
    PLAY_MODE_SEQUENCE
};


/**
  * The `DY::DYPlayer::previousDir()` method expects this type as its argument.
  * Imagine you would press a button on a media player that selects the
  * previous directory/playlist, do you expect it to play the first song of
  * that list, or the last one? Depending on what you find logical or on your
  * requirement, this enumeration allows you to choose what happens when you
  * go to the previous directory.
  */
  
  typedef uint8_t playDirSound_t;
  enum
  {
      PREVIOUS_DIR_FIRST_SOUND = 0,
      PREVIOUS_DIR_LAST_SOUND
  };  

  typedef void (*dy_uart_write_fn_t)(const uint8_t *buffer, uint8_t size);
  typedef bool (*dy_uart_read_fn_t)(uint8_t *buffer, uint8_t size);

  typedef struct
  {
    dy_uart_read_fn_t uart_read;
    dy_uart_write_fn_t uart_write;
    uint8_t options;
  } dy_player_t;

/**
  * Initialize the DYPlayer module.
  * @param player pointer to the `dy_player_t` structure.
  * @param uart_write_fn function pointer for UART write.
  * @param uart_read_fn function pointer for UART read. Can be NULL if not used.
  * @param opt_aux_in_used indicates if the AUX input is used.
  * @param opt_only_flash indicates if only the flash storage is available.
  * @return boolean indicating if the initialization was successful (true) or not (false).
  */
bool DYPlayer_init(dy_player_t *player, dy_uart_write_fn_t uart_write_fn, dy_uart_read_fn_t uart_read_fn, uint8_t options);

/**
  * Check the current play state can, be called at any time.
  * @param player pointer to the `dy_player_t` structure.
  * @return Play status: A [`DY::PlayState`](#typedef-enum-class-dyplay_state_t),
  *         e.g DY::PlayMode::Stopped, DY::PlayMode::Playing, etc.
  */
play_state_t DYPlayer_getPlayState(dy_player_t *player);

/**
  * Play the currently selected file from the start.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_play(dy_player_t *player);

/**
  * Set the play state to paused.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_pause(dy_player_t *player);

/**
  * Set the play state to stopped.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_stop(dy_player_t *player);

/**
  * Play the previous file.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_previous(dy_player_t *player);

/**
  * Play the next file.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_next(dy_player_t *player);

/**
  * Play a sound file by number, number sent as 2 bytes.
  * @param player pointer to the `dy_player_t` structure.
  * @param number of the file, e.g. `1` for `00001.mp3`.
  */
void DYPlayer_playSpecified(dy_player_t *player, uint16_t number);

/**
  * Play a sound file by device and path.
  * Path may consist of up to 2 nested directories of 8 bytes long and a
  * file name of 8 bytes long excluding the extension of 4 bytes long.
  * If your directory names are shorter you can use more nesting. Use no
  * more than 36 bytes for your paths. If you require more, check the
  * readme, chapter: Memory use.
  * @param player pointer to the `dy_player_t` structure.
  * @param device A [`DY::Device member`](#typedef-enum-class-dydevice_t),
  *               e.g  `DY::Device::Flash` or `DY::Device::Sd`.
  * @param path pointer to the path of the file (asbsolute).
  */
void DYPlayer_playSpecifiedDevicePath(dy_player_t *player, device_t device,const char *path);

/**
  * Get the storage device that is currently used for playing sound files.
  *
  * @param player pointer to the `dy_player_t` structure.
  * @return a [`DY::Device` member](#typedef-enum-class-dydevice_t),
  *         e.g  `DY::Device::Flash` or `DY::Device::Sd`.
  */
device_t DYPlayer_getPlayingDevice(dy_player_t *player);

/**
  * Set the device number the module should use.
  * Tries to set the device but no guarantee is given, use `getDevice()`
  * to check the actual current storage device.
  * @param player pointer to the `dy_player_t` structure.
  * @param device A [`DY::Device` member](#typedef-enum-class-dydevice_t),
  *               e.g  `DY::Device::Flash` or `DY::Device::Sd`.
  */
void DYPlayer_setPlayingDevice(dy_player_t *player, device_t device);

/**
  * Get the amount of sound files on the current storage device.
  * @param player pointer to the `dy_player_t` structure.
  * @return number of sound files.
  */
uint16_t DYPlayer_getSoundCount(dy_player_t *player);

/**
  * Get the currently playing file by number.
  * @param player pointer to the `dy_player_t` structure.
  * @return number of the file currently playing.
  */
uint16_t DYPlayer_getPlayingSound(dy_player_t *player);

/**
  * Select previous directory and start playing the first or last song.
  * @param player pointer to the `dy_player_t` structure.
  * @param song Play `DY::PreviousDir::FirstSound` or
  *             DY::PreviousDir::LastSound
  */
void DYPlayer_previousDir(dy_player_t *player, playDirSound_t song);

/**
  * Get number of the first song in the currently selected directory.
  * @param player pointer to the `dy_player_t` structure.
  * @return number of the first song in the currently selected directory.
  */
uint16_t DYPlayer_getFirstInDir(dy_player_t *player);

/**
  * Get the amount of sound files in the currently selected directory.
  * NOTE: Excluding files in sub directories.
  * @param player pointer to the `dy_player_t` structure.
  * @return number of sound files in currently selected directory.
  */
uint16_t DYPlayer_getSoundCountDir(dy_player_t *player);

/**
  * Set the playback volume between 0 and 30.
  * Default volume if not set: 20.
  * @param player pointer to the `dy_player_t` structure.
  * @param volume to set (0-30)
  */
void DYPlayer_setVolume(dy_player_t *player, uint8_t volume);

/**
  * Increase the volume.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_volumeIncrease(dy_player_t *player);

/**
  * Decrease the volume.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_volumeDecrease(dy_player_t *player);

/**
  * Play an interlude file by device and number, number sent as 2 bytes.
  * Note from the manual: "Music interlude" only has level 1. Continuous
  * interlude will cover the previous interlude (the interlude will be
  * played immediately). When the interlude is finished, it will return to
  * the first interlude breakpoint and continue to play.
  * @param player pointer to the `dy_player_t` structure.
  * @param device A [`DY::Device member`](#typedef-enum-class-dydevice_t),
  *               e.g  `DY::Device::Flash` or `DY::Device::Sd`.
  * @param number of the file, e.g. `1` for `00001.mp3`.
  */
void DYPlayer_interludeSpecified(dy_player_t *player, device_t device, uint16_t number);

/**
  * Play an interlude by device and path.
  * Note from the manual: "Music interlude" only has level 1. Continuous
  * interlude will cover the previous interlude (the interlude will be
  * played immediately). When the interlude is finished, it will return to
  * the first interlude breakpoint and continue to play.
  *
  * Path may consist of up to 2 nested directories of 8 bytes long and a
  * file name of 8 bytes long excluding the extension of 4 bytes long.
  * If your directory names are shorter you can use more nesting. Use no
  * more than 36 bytes for your paths. If you require more, check the
  * readme, chapter: Memory use.
  * @param player pointer to the `dy_player_t` structure.
  * @param device A [`DY::Device member`](#typedef-enum-class-dydevice_t),
  *               e.g  `DY::Device::Flash` or `DY::Device::Sd`.
  * @param path pointer to the path of the file (asbsolute).
  */
void DYPlayer_interludeSpecifiedDevicePath(dy_player_t *player, device_t device,const char *path);

/**
  * Stop the interlude and continue playing.
  * Will also stop the current sound from playing if interlude is not
  * active.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_stopInterlude(dy_player_t *player);

/**
  * Sets the cycle mode.
  * See [`DY::play_state_t`](#typedef-enum-class-dyplay_state_t) for modes
  * and meaning.
  * @param player pointer to the `dy_player_t` structure.
  * @param mode The cycle mode to set.
  */
void DYPlayer_setCycleMode(dy_player_t *player, play_mode_t mode);

/**
  * Set how many cycles to play when in cycle modes 0, 1 or 4 (repeat
  * modes).
  * @param player pointer to the `dy_player_t` structure.
  * @param cycles The cycle count for repeat modes.
  */
void DYPlayer_setCycleTimes(dy_player_t *player, uint16_t cycles);

/**
  * Set the equalizer setting.
  * See [`DY::eq_t`](#typedef-enum-class-dyeq_t) for settings.
  * @param player pointer to the `dy_player_t` structure.
  * @param eq The equalizer setting.
  */
void DYPlayer_setEq(dy_player_t *player, eq_t eq);

/**
  * Select a sound file without playing it.
  * @param player pointer to the `dy_player_t` structure.
  * @param number of the file, e.g. `1` for `00001.mp3`.
  */
void DYPlayer_select(dy_player_t *player, uint16_t number);

/**
  * Combination play allows you to make a playlist of multiple sound files.
  *
  * You could use this to combine numbers e.g.: "fourthy-two" where you
  * have samples for "fourthy" and "two".
  *
  * This feature has a particularly curious parameters, you have to
  * specify the sound files by name, they have to be named by 2 numbers
  * and an extension, e.g.: `01.mp3` and specified by `01`. You should
  * pass them as an array pointer. You need to put the files into a
  * directory that can be called `DY`, `ZH or `XY`, you will have to check
  * the manual that came with your module, or try all of them. There may
  * well be more combinations! Also see
  * [Loading sound files](#loading-sound-files).
  *
  * E.g.
  * ```c
  * const char sounds[][2] = {
  *   {'0', '1'},
  *   {'0', '2'},
  *   {'A', '7'}
  * };
  * DYPlayer_combinationPlay(&player, sounds, 3);
  * ```
  * Note: number of songs transmitted cannot exceed 127
  * @param player pointer to the `dy_player_t` structure.
  * @param sounds An array of char[2] containing the names of sounds to
  *        play in order.
  * @param size The length of the passed array.
  */
void DYPlayer_combinationPlay(dy_player_t *player,const char (*sounds)[2], uint8_t size);

/**
  * End combination play.
  * @param player pointer to the `dy_player_t` structure.
  */
void DYPlayer_endCombinationPlay(dy_player_t *player);


#ifdef __cplusplus
  };}
#endif

#endif // DYPLAYER_PIC_H
