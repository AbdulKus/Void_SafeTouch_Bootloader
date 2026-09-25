#ifndef SAFETOUCH_PROTOCOL_H
#define SAFETOUCH_PROTOCOL_H

#include <stdint.h>

#define ST_USB_VID              0x1209u
#define ST_USB_PID_LOGIN        0xB008u
#define ST_REPORT_SIZE          64u
#define ST_PROTOCOL_MAJOR       1u
#define ST_PROTOCOL_MINOR       1u

#define ST_DEVICE_SECRET_SIZE   32u
#define ST_NONCE_SIZE           32u
#define ST_KEY_SIZE             32u
#define ST_PROOF_SIZE           16u
#define ST_DEVICE_ID_SIZE       16u
#define ST_CARD_ID_SIZE         16u
#define ST_DISPLAY_NAME_SIZE    16u

enum st_command {
    ST_CMD_INFO         = 0x01,
    ST_CMD_ENROLL_BEGIN = 0x10,
    ST_CMD_STATUS       = 0x11,
    ST_CMD_CANCEL       = 0x12,
    ST_CMD_ADD_CARD_BEGIN = 0x13,
    ST_CMD_AUTH_BEGIN   = 0x20
};

enum st_result {
    ST_RESULT_OK             = 0,
    ST_RESULT_BAD_COMMAND    = 1,
    ST_RESULT_BAD_LENGTH     = 2,
    ST_RESULT_BAD_STATE      = 3,
    ST_RESULT_ALREADY_ENROLLED = 4,
    ST_RESULT_NOT_ENROLLED   = 5,
    ST_RESULT_FLASH          = 6,
    ST_RESULT_CARD_WEAK_ID   = 7,
    ST_RESULT_BUSY           = 8,
    ST_RESULT_DUPLICATE_CARD = 9
};

enum st_state {
    ST_STATE_UNENROLLED   = 0,
    ST_STATE_IDLE         = 1,
    ST_STATE_INSERT_CARD  = 2,
    ST_STATE_READING_CARD = 3,
    ST_STATE_PRESS_GREEN  = 4,
    ST_STATE_AUTH_OK      = 5,
    ST_STATE_ACCESS_DENIED = 6,
    ST_STATE_CANCELED     = 7,
    ST_STATE_ERROR        = 8,
    ST_STATE_ENROLLED     = 9
};

enum st_card_id_source {
    ST_CARD_ID_NONE = 0,
    ST_CARD_ID_ATR = 1,
    ST_CARD_ID_EMV = 2
};

/* Request layouts. Every request and reply is exactly 64 bytes. */
#define ST_ENROLL_FLAGS_OFFSET       2u
#define ST_ENROLL_SECRET_OFFSET      4u
#define ST_ENROLL_NAME_OFFSET        36u
#define ST_ENROLL_NAME_LENGTH_OFFSET 3u
#define ST_ENROLL_ALLOW_ATR          0x01u

#define ST_AUTH_NONCE_OFFSET         4u

/* Common reply: command, result, state, flags, followed by command data. */
#define ST_REPLY_RESULT_OFFSET       1u
#define ST_REPLY_STATE_OFFSET        2u
#define ST_REPLY_FLAGS_OFFSET        3u
#define ST_REPLY_DATA_OFFSET         4u

/* INFO: device id at 4, card id at 20, name at 36. */
#define ST_INFO_DEVICE_ID_OFFSET     4u
#define ST_INFO_CARD_ID_OFFSET       20u
#define ST_INFO_NAME_OFFSET          36u
#define ST_INFO_VERSION_OFFSET       52u
#define ST_INFO_FLAGS_OFFSET         54u
#define ST_INFO_HAS_BACKUP           0x01u

/* Successful STATUS after authentication: proof at 4 and wrapping key at 20. */
#define ST_AUTH_PROOF_OFFSET         4u
#define ST_AUTH_WRAP_KEY_OFFSET      20u

#define ST_LABEL_AUTH_KEY   "SafeTouch auth key v1"
#define ST_LABEL_WRAP_KEY   "SafeTouch wrap key v1"
#define ST_LABEL_DEVICE_ID  "SafeTouch device id v1"
#define ST_LABEL_AUTH_PROOF "SafeTouch auth proof v1"

#endif
