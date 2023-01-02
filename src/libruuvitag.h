#ifndef _LIBRUUVITAG_H_
#define _LIBRUUVITAG_H_

#include <stdint.h>

// BUF sizes including trailing null
#define LIBRUUVITAG_MAC_BUF_SIZE (18)
#define LIBRUUVITAG_BLUEZ_DEV_OBJECT_PATH  (128) /* Something like /org/bluez/hci0 */


typedef struct
{
  char s_mac_tag_beacon[LIBRUUVITAG_MAC_BUF_SIZE];
  char s_mac_via_receiver[LIBRUUVITAG_MAC_BUF_SIZE];

} libruuvitag_beacon_data_struct;

uint8_t u8LibRuuviTagInit(char* s_listen_on);

uint8_t u8LibRuuviTagDeinit(void);

#endif // #define _LIBRUUVITAG_H_
