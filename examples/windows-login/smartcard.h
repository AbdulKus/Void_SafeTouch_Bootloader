#ifndef SAFETOUCH_SMARTCARD_H
#define SAFETOUCH_SMARTCARD_H

#include <stdint.h>

int card_present(void);
void card_off(void);
/* Returns ST_CARD_ID_EMV or ST_CARD_ID_ATR, zero on failure. */
unsigned card_read_identity(uint8_t identity[16]);

#endif
