/**
 * Abstraction of basic features of the DY-SV17F mp3 player board, written for
 * Arduino, should work on other frameworks as well. Instead of DY-SV17F I will
 * from here on refer to it as the "module".
 *
 * There are some virtual methods that MUST be overridden (serialRead and
 * serialWrite) and one that you may override (begin)
 */
//#include <ctype.h>
#include <string.h>
#include <ctype.h>
#include "DYPlayer_PIC.h"

#define DY_UART_BYTE_TIMEOUT_MS  80 

#ifdef __cplusplus
namespace DY
{
#endif

//  private API

  /**
    * Calculate the sum of all bytes in a buffer as a simple "CRC".
    * @param data pointer to bytes to calculate the CRC for.
    * @param size of buffer.
    * @return Checksum of the buffer.
    */
  static uint8_t  checksum(const uint8_t *data, uint8_t size)
  {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < size; i++)
    {
      sum = sum + data[i];
    }
    return sum;
  }

  /**
    * Validate data buffer with CRC byte (last byte should be the CRC byte).
    * @param data pointer to bytes to calculate the CRC for.
    * @param size of data.
    * @return boolean indicating CRC is correct (true) or incorrect (false).
    */
  static bool  validateCrc(uint8_t *data, uint8_t size)
  {
    if (data == NULL || size == 0) return false;

    uint8_t crc = data[size - 1];
    return checksum(data, size - 1) == crc;
  }

  /**
    * Write a buffer to the UART interface of the player.
    * @param player pointer to the player instance.
    * @param buffer pointer to the bytes to write.
    * @param size number of bytes to write.
    * @return void.
  */
  static void uart_write_buffer(dy_player_t *player, const uint8_t *buffer, uint8_t size)
  {
      for (uint8_t i = 0; i < size; i++)
      {
          player->uart_write(buffer[i]);
      }
  }
/**
  * Function to flush any pending response from the UART interface.
  * @param player pointer to the player instance.
  * @return void.
*/
  static void flushResponse(dy_player_t *player)
{
    if (player == NULL || player->uart_read == NULL) return;

    uint8_t discarded;

    while (player->uart_read(&discarded, 0)) {
        // Ignore les octets non réclamés.
    }
}
  
  /**
    * Send a command to the module, adds a CRC to the passed buffer.
    * @param dy_player pointer to the player instance.
    * @param command The command to send to the module.
    * @param data pointer to bytes to send to the module (can be NULL if no data).
    * @param size of data (0 if no data).
    */
  static bool sendCommand(dy_player_t *player,uint8_t command, const uint8_t *data, uint8_t size)
  {
    if (player == NULL || player->uart_write == NULL) return false;
    if (data == NULL && size > 0) size=0;

    // On supprime d'éventuelles data en attente dans la pile de reception du PIC
    flushResponse(player);

    uint8_t buffer[3];
    buffer[0] = 0xAA;
    buffer[1] = command;
    buffer[2] = size;
    uart_write_buffer(player, buffer, sizeof(buffer));

    uint8_t crc[1] = {0};
    crc[0] = checksum(buffer, sizeof(buffer));
    if (data != NULL && size > 0) {
      uart_write_buffer(player,data, size);
      crc[0] += checksum(data, size);
    }

    uart_write_buffer(player, &crc[0], 1);

    return true;
  }


    
  /**
    * Send command with converted paths to  weird format required by the
    * modules.
    *
    * - Any dot in a path should become a star (`*`)
    * - Path ending slashes should be have a star prefix, except root.
    *
    * E.g.: /SONGS1/FILE1.MP3 should become: /SONGS1﹡/FILE1*MP3
    * NOTE: This comment uses a unicode * look-a-alike (﹡) because ﹡/ end the
    * comment.
    * @param player pointer to the player instance.
    * @param command The command to send.
    * @param device A [DY::Device member](#typedef-enum-class-dydevice_t),
    *               e.g  `DY::Device::Flash` or `DY::Device::Sd`.
    * @param path of the file (asbsolute).
    */
  static bool byPathCommand(dy_player_t *player, uint8_t command, device_t device, const char *path)
  {
    if (player == NULL || player->uart_write == NULL || path == NULL) return false;

    size_t original_len = strlen(path);

    if (original_len == 0 || original_len > UINT8_MAX) return false;
    
    uint8_t size = (uint8_t)original_len;
    size_t transformed_len = original_len;

    for (uint8_t i = 1; i < size; i++) {
      if (path[i] == '/') {
        transformed_len++;
      }
    }

    if (transformed_len > DY_MAX_PATH_LEN) return false;

    // On supprime d'éventuelles data en attente dans la pile de reception du PIC
    flushResponse(player);

    uint8_t header[5];
    uint8_t transformed_size = (uint8_t)transformed_len;
    header[0] = 0xAA;
    header[1] = command;
    header[2] = (uint8_t)(transformed_size + 1u); // + device byte
    header[3] = (uint8_t)device;
    header[4] = (uint8_t)toupper((unsigned char)path[0]);

    uint8_t crc = checksum(header, sizeof(header));

    uart_write_buffer(player, header, sizeof(header));

    for (uint8_t i = 1; i < size; i++) {
      char c = path[i];

      if (c == '.') {
        uint8_t byte = '*';
        crc += byte;
        uart_write_buffer(player, &byte, 1);
      } else if (c == '/') {
        uint8_t bytes[2] = { '*', '/' };
        crc += bytes[0] + bytes[1];
        uart_write_buffer(player, bytes, sizeof(bytes));
      } else {
        uint8_t byte = (uint8_t)toupper((unsigned char)c);
        crc += byte;
        uart_write_buffer(player, &byte, 1);
      }
    }

    uart_write_buffer(player, &crc, 1);
    return true;
  }
/**
  * @brief Get response from the player.
  * @param player pointer to the player instance.
  * @param buffer buffer to store the response.
  * @param size size of the buffer.
  * @return true if the response is valid, false otherwise.
  */
  static bool readResponse( dy_player_t *player, uint8_t command_from, uint8_t *buffer, uint8_t size)
  {
      if (player == NULL ||
          player->uart_read == NULL ||
          command_from == 0 ||
          buffer == NULL ||
          size == 0) {
          return false;
      }
      do {
        // On cherche le début de la trame (0xAA)
        do {
            if (!player->uart_read(&buffer[0], DY_UART_BYTE_TIMEOUT_MS)) {
                return false;
            }
        } while (buffer[0] != 0xAA);
        // On contrôle si la reponse reçue correspond à la commande appelante
        if (!player->uart_read(&buffer[1], DY_UART_BYTE_TIMEOUT_MS)) {
            return false;
        }       
      } while (buffer[1] != command_from);


      for (uint8_t i = 2; i < size; i++) {
          if (!player->uart_read(&buffer[i], DY_UART_BYTE_TIMEOUT_MS)) {
              return false;
          }
      }

      return validateCrc(buffer, size);
  }

/** Public API */


bool DYPlayer_init(dy_player_t *player, dy_uart_write_byte_fn_t uart_write_byte_fn, dy_uart_read_byte_fn_t uart_read_byte_fn, uint8_t options)
{
    if (player == NULL || uart_write_byte_fn == NULL ) return false; //|| uart_read_byte_fn == NULL

    player->uart_write = uart_write_byte_fn;
    player->uart_read = uart_read_byte_fn;
    player->options = options;
    return true;
}

  play_state_t DYPlayer_getPlayState(dy_player_t *player)
  {
    if (player == NULL || player->uart_read == NULL)
        return PLAY_STATE_FAIL;


    sendCommand(player, 0x01, NULL, 0);
    uint8_t buffer[5];
    if (readResponse(player, 0x01, buffer, 5))
    {
      return (play_state_t)buffer[3];
    }
    return PLAY_STATE_FAIL;
  }

  void DYPlayer_play(dy_player_t *player)
  {
    sendCommand(player, 0x02, NULL, 0);
  }

  void DYPlayer_pause(dy_player_t *player)
  {
    sendCommand(player, 0x03, NULL, 0);
  }

  void DYPlayer_stop(dy_player_t *player)
  {
    sendCommand(player, 0x04, NULL, 0);
  }

  void DYPlayer_previous(dy_player_t *player)
  {
    sendCommand(player, 0x05, NULL, 0);
  }

  void DYPlayer_next(dy_player_t *player)
  {
    sendCommand(player, 0x06, NULL, 0);
  }

  void DYPlayer_playSpecified(dy_player_t *player, uint16_t number)
  {
    uint8_t data[2] = {
        (uint8_t)(number >> 8),
        (uint8_t)(number & 0xff)
    };

    sendCommand(player, 0x07, data, 2);
  }
  
  void DYPlayer_playSpecifiedDevicePath(dy_player_t *player, device_t device,const char *path)
  {
    if (player == NULL) return;
    if (player->options & OPTION_ONLY_FLASH && device != DEVICE_FLASH) return;

    byPathCommand(player, 0x08, device, path);
  }

  device_t DYPlayer_getPlayingDevice(dy_player_t *player)
  {
    if (!sendCommand(player, 0x0a, NULL, 0))
        return DEVICE_FAIL;

    uint8_t buffer[5];
    if (readResponse(player, 0x0a, buffer, 5))
    {
      return (device_t)buffer[3];
    }
    return DEVICE_FAIL;
  }

  void DYPlayer_setPlayingDevice(dy_player_t *player, device_t device)
  {
    if (player == NULL) return;
    if (player->options & OPTION_ONLY_FLASH) return;

    uint8_t data[1] = {(uint8_t)device};
    sendCommand(player, 0x0b, data, 1);  }

  uint16_t DYPlayer_getSoundCount(dy_player_t *player)
  {
    if (!sendCommand(player, 0x0c, NULL, 0))
        return 0;
    
    uint8_t buffer[6];
    if (readResponse(player, 0x0c, buffer, 6))
    {
      return (uint16_t)(((uint16_t)buffer[3] << 8) | (uint16_t)buffer[4]);
    }
    return 0;
  }

  uint16_t DYPlayer_getPlayingSound(dy_player_t *player)
  {
    if (!sendCommand(player, 0x0d, NULL, 0))
        return 0;

    uint8_t buffer[6];
    if (readResponse(player, 0x0d, buffer, 6))
    {
      return (uint16_t)(((uint16_t)buffer[3] << 8) | (uint16_t)buffer[4]);
    }
    return 0;
  }

  void DYPlayer_previousDir(dy_player_t *player, playDirSound_t song)
  {
    if (song == PREVIOUS_DIR_LAST_SOUND)
    {
      sendCommand(player, 0x0e, NULL, 0);
    }
    else
    {
      sendCommand(player, 0x0f, NULL, 0);
    }
  }

  uint16_t DYPlayer_getFirstInDir(dy_player_t *player)
  {
    if (!sendCommand(player, 0x11, NULL, 0))
        return 0;
      
    uint8_t buffer[6];
    if (readResponse(player, 0x11, buffer, 6))
    {
      return (uint16_t)(((uint16_t)buffer[3] << 8) | (uint16_t)buffer[4]);
    }
    return 0;
  }

  uint16_t DYPlayer_getSoundCountDir(dy_player_t *player)
  {
    if (!sendCommand(player, 0x12, NULL, 0))
        return 0;

    uint8_t buffer[6];
    if (readResponse(player, 0x12, buffer, 6))
    {
      return (uint16_t)(((uint16_t)buffer[3] << 8) | (uint16_t)buffer[4]);
    }
    return 0;
  }

  void DYPlayer_setVolume(dy_player_t *player, uint8_t volume)
  {
    uint8_t data[1] = {volume};
    sendCommand(player, 0x13, data, 1);
  }

  void DYPlayer_volumeIncrease(dy_player_t *player)
  {
    sendCommand(player, 0x14, NULL, 0);
  }

  void DYPlayer_volumeDecrease(dy_player_t *player)
  {
    sendCommand(player, 0x15, NULL, 0);
  }

  void DYPlayer_interludeSpecified(dy_player_t *player, device_t device, uint16_t number)
  {
    if (player == NULL) return;
    if (player->options & OPTION_ONLY_FLASH && device != DEVICE_FLASH) return;

    uint8_t data[3] = {0};
    data[0] = (uint8_t)device;
    data[1] = number >> 8;
    data[2] = number & 0xff;
    sendCommand(player, 0x16, data, 3);
  }

  void DYPlayer_interludeSpecifiedDevicePath(dy_player_t *player, device_t device, const char *path)
  {
    if (player == NULL) return;
    if (player->options & OPTION_ONLY_FLASH && device != DEVICE_FLASH) return;

    byPathCommand(player, 0x17, device, path);
  }

  void DYPlayer_stopInterlude(dy_player_t *player)
  {
    sendCommand(player, 0x10, NULL, 0);
  }

  void DYPlayer_setCycleMode(dy_player_t *player, play_mode_t mode)
  {
    uint8_t data[1] = {mode};
    sendCommand(player, 0x18, data, 1);
  }

  void DYPlayer_setCycleTimes(dy_player_t *player, uint16_t cycles)
  {
    uint8_t data[2] = {0};
    data[0] = cycles >> 8;
    data[1] = cycles & 0xff;
    sendCommand(player, 0x19, data, 2);
  }

  void DYPlayer_setEq(dy_player_t *player, eq_t eq)
  {
    uint8_t data[1] = { (uint8_t)eq };
    sendCommand(player, 0x1a, data, 1);
  }

  void DYPlayer_select(dy_player_t *player, uint16_t number)
  {
    uint8_t data[2] = {0};
    data[0] = number >> 8;
    data[1] = number & 0xff;
    sendCommand(player, 0x1f, data, 2);
  }

 void DYPlayer_combinationPlay(dy_player_t *player,const char (*sounds)[2], uint8_t size)
  {
    if (sounds == NULL || size == 0 || size > 127) return;
    if (player == NULL || player->uart_write == NULL) return;

    // This part of the command can be easily determined already.
    uint8_t header[3] = {0xaa, 0x1b, (uint8_t)(size*2)};

    // Depends on the length, checksum is a sum so we can add the other values
    // later.
    uint8_t crc = checksum(header, 3);
    uart_write_buffer(player, header, 3);

    // Send each pair of chars containing the file name and add the values of
    // each char to the crc.
    for (uint8_t i = 0; i < size; i++)
    {
      const uint8_t *sound = (const uint8_t *)sounds[i];
      crc += checksum(sound, 2);
      uart_write_buffer(player, sound, 2);
    }
    // Lastly, write the crc value.
    player->uart_write(crc);
  }

  void DYPlayer_endCombinationPlay(dy_player_t *player)
  {
    sendCommand(player, 0x1c, NULL, 0);
  }

#ifdef __cplusplus
}
#endif
