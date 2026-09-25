#include <stdint.h>
#include "../../protocol/safetouch_protocol.h"
#include "hardware.h"
#include "lcd.h"
#include "sha256.h"
#include "smartcard.h"
#include "usb_hid.h"

#define WDT_MR REG32(0xFFFFFD44u)
#define MC_FMR REG32(0xFFFFFF60u)
#define MC_FCR REG32(0xFFFFFF64u)
#define MC_FSR REG32(0xFFFFFF68u)
#define MC_FRDY (1u<<0)
#define MC_LOCKE (1u<<2)
#define MC_PROGE (1u<<3)
#define MC_KEY (0x5Au<<24)
#define MC_FCMD_WP 0x01u
#define FLASH_BASE 0x00100000u
#define CONFIG_A 0x0010FF00u
#define CONFIG_B 0x0010FF80u
#define PAGE_SIZE 128u
#define CONFIG_MAGIC 0x31435453u /* STC1 */

#define CFG_MAGIC 0u
#define CFG_VERSION 4u
#define CFG_GENERATION 8u
#define CFG_SECRET 12u
#define CFG_DEVICE_ID 44u
#define CFG_CARD_ID 60u
#define CFG_V1_CARD_SOURCE 76u
#define CFG_V1_NAME_LENGTH 77u
#define CFG_V1_NAME 78u
#define CFG_BACKUP_CARD_ID 76u
#define CFG_CARD_SOURCE 92u
#define CFG_BACKUP_CARD_SOURCE 93u
#define CFG_NAME_LENGTH 94u
#define CFG_NAME 95u
#define CFG_CRC 124u

/* The EFC page buffer must be populated with aligned 32-bit writes. */
static uint8_t config_page[PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t pending_secret[32],pending_name[16],nonce[32],current_card[16];
static uint8_t auth_proof[16],wrap_key[32];
static unsigned current_card_source;
static uint8_t state,last_result,pending_enrollment,allow_atr;
static uint8_t displayed_card_present=2;
static uint8_t green_was_up=1,red_was_up=1;
static uint8_t buttons_armed=1;

static uint32_t read_u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write_u32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t crc32(const uint8_t *p,unsigned length){uint32_t c=0xFFFFFFFFu;while(length--){c^=*p++;for(unsigned b=0;b<8;++b)c=(c>>1)^(0xEDB88320u&(0u-(c&1u)));}return ~c;}
static int bytes_equal(const uint8_t *a,const uint8_t *b,unsigned length){uint8_t difference=0;while(length--)difference|=*a++^*b++;return difference==0;}
static void wipe(void *memory,unsigned length){volatile uint8_t *p=(volatile uint8_t *)memory;while(length--)*p++=0;}

static int valid_config(const uint8_t *page)
{
    uint32_t version=read_u32(page+CFG_VERSION);
    unsigned name_length=version==1u?page[CFG_V1_NAME_LENGTH]:page[CFG_NAME_LENGTH];
    return read_u32(page+CFG_MAGIC)==CONFIG_MAGIC&&(version==1u||version==2u)&&
           name_length<=ST_DISPLAY_NAME_SIZE&&crc32(page,CFG_CRC)==read_u32(page+CFG_CRC);
}
static const uint8_t *flash_config(void)
{
    const uint8_t *a=(const uint8_t *)CONFIG_A,*b=(const uint8_t *)CONFIG_B;int av=valid_config(a),bv=valid_config(b);
    if(av&&bv)
        return read_u32(a+CFG_GENERATION)>=read_u32(b+CFG_GENERATION)?a:b;
    return av?a:(bv?b:0);
}
static unsigned config_card_source(const uint8_t *cfg){return read_u32(cfg+CFG_VERSION)==1u?cfg[CFG_V1_CARD_SOURCE]:cfg[CFG_CARD_SOURCE];}
static unsigned config_name_length(const uint8_t *cfg){return read_u32(cfg+CFG_VERSION)==1u?cfg[CFG_V1_NAME_LENGTH]:cfg[CFG_NAME_LENGTH];}
static const uint8_t *config_name(const uint8_t *cfg){return cfg+(read_u32(cfg+CFG_VERSION)==1u?CFG_V1_NAME:CFG_NAME);}
static int config_has_backup(const uint8_t *cfg){return read_u32(cfg+CFG_VERSION)>=2u&&cfg[CFG_BACKUP_CARD_SOURCE]!=ST_CARD_ID_NONE;}
__attribute__((section(".ramfunc"),noinline))
static uint32_t program_page(uint32_t address,const uint8_t *bytes)
{
    volatile uint32_t *destination=(volatile uint32_t *)address;const uint32_t *source=(const uint32_t *)bytes;
    uint32_t page=(address-FLASH_BASE)/PAGE_SIZE,status;while(!(MC_FSR&MC_FRDY)){}
    for(unsigned i=0;i<PAGE_SIZE/4u;++i)destination[i]=source[i];MC_FCR=MC_KEY|(page<<8)|MC_FCMD_WP;
    do{status=MC_FSR;}while(!(status&MC_FRDY));return status&(MC_LOCKE|MC_PROGE);
}
static int save_config(void)
{
    const uint8_t *old=flash_config();uint32_t generation=old?read_u32(old+CFG_GENERATION)+1u:1u;
    uint32_t target=old==(const uint8_t *)CONFIG_A?CONFIG_B:CONFIG_A;for(unsigned i=0;i<PAGE_SIZE;++i)config_page[i]=0xFF;
    write_u32(config_page+CFG_MAGIC,CONFIG_MAGIC);write_u32(config_page+CFG_VERSION,2);write_u32(config_page+CFG_GENERATION,generation);
    for(unsigned i=0;i<32;++i)config_page[CFG_SECRET+i]=pending_secret[i];
    uint8_t digest[32],material[64];for(unsigned i=0;i<32;++i)material[i]=pending_secret[i];
    const char label[]=ST_LABEL_DEVICE_ID;for(unsigned i=0;i<sizeof(label)-1;++i)material[32+i]=(uint8_t)label[i];sha256(material,32u+sizeof(label)-1u,digest);
    for(unsigned i=0;i<16;++i)config_page[CFG_DEVICE_ID+i]=digest[i];
    for(unsigned i=0;i<16;++i)config_page[CFG_CARD_ID+i]=current_card[i];
    config_page[CFG_CARD_SOURCE]=(uint8_t)current_card_source;config_page[CFG_BACKUP_CARD_SOURCE]=ST_CARD_ID_NONE;unsigned length=0;while(length<16&&pending_name[length])++length;config_page[CFG_NAME_LENGTH]=(uint8_t)length;
    for(unsigned i=0;i<16;++i)config_page[CFG_NAME+i]=pending_name[i];
    write_u32(config_page+CFG_CRC,crc32(config_page,CFG_CRC));
    int ok=program_page(target,config_page)==0&&valid_config((const uint8_t *)target);wipe(digest,sizeof(digest));wipe(material,sizeof(material));wipe(config_page,sizeof(config_page));return ok;
}
static int save_backup_config(void)
{
    const uint8_t *old=flash_config();if(!old)return 0;uint32_t generation=read_u32(old+CFG_GENERATION)+1u;
    uint32_t target=old==(const uint8_t *)CONFIG_A?CONFIG_B:CONFIG_A;for(unsigned i=0;i<PAGE_SIZE;++i)config_page[i]=0xFF;
    write_u32(config_page+CFG_MAGIC,CONFIG_MAGIC);write_u32(config_page+CFG_VERSION,2);write_u32(config_page+CFG_GENERATION,generation);
    for(unsigned i=0;i<32;++i)config_page[CFG_SECRET+i]=old[CFG_SECRET+i];
    for(unsigned i=0;i<16;++i){config_page[CFG_DEVICE_ID+i]=old[CFG_DEVICE_ID+i];config_page[CFG_CARD_ID+i]=old[CFG_CARD_ID+i];config_page[CFG_BACKUP_CARD_ID+i]=current_card[i];}
    config_page[CFG_CARD_SOURCE]=(uint8_t)config_card_source(old);config_page[CFG_BACKUP_CARD_SOURCE]=(uint8_t)current_card_source;
    unsigned length=config_name_length(old);config_page[CFG_NAME_LENGTH]=(uint8_t)length;const uint8_t *name=config_name(old);for(unsigned i=0;i<16;++i)config_page[CFG_NAME+i]=i<length?name[i]:0;
    write_u32(config_page+CFG_CRC,crc32(config_page,CFG_CRC));int ok=program_page(target,config_page)==0&&valid_config((const uint8_t *)target);wipe(config_page,sizeof(config_page));return ok;
}

void clocks_init(void)
{
    WDT_MR=0x00008000u;MC_FMR=0x00490100u;PMC_MOR=0x00000601u;while(!(PMC_SR&(1u<<0))){}
    PMC_PLLR=0x10481C0Eu;while(!(PMC_SR&(1u<<2))){}PMC_MCKR=7u;while(!(PMC_SR&(1u<<3))){}
}
static void derive_auth_response(void)
{
    const uint8_t *cfg=flash_config();uint8_t auth_key[32],full[32],message[96];const char auth_label[]=ST_LABEL_AUTH_KEY,wrap_label[]=ST_LABEL_WRAP_KEY,proof_label[]=ST_LABEL_AUTH_PROOF;
    hmac_sha256(cfg+CFG_SECRET,32,auth_label,sizeof(auth_label)-1,auth_key);hmac_sha256(cfg+CFG_SECRET,32,wrap_label,sizeof(wrap_label)-1,wrap_key);
    unsigned used=0;for(unsigned i=0;i<sizeof(proof_label)-1;++i)message[used++]=(uint8_t)proof_label[i];for(unsigned i=0;i<32;++i)message[used++]=nonce[i];
    for(unsigned i=0;i<16;++i)message[used++]=cfg[CFG_CARD_ID+i];
    for(unsigned i=0;i<16;++i)message[used++]=cfg[CFG_DEVICE_ID+i];
    hmac_sha256(auth_key,32,message,used,full);for(unsigned i=0;i<16;++i)auth_proof[i]=full[i];wipe(auth_key,32);wipe(full,32);wipe(message,sizeof(message));
}
static void update_button_lights(void)
{
    unsigned on=0;
    if(state==ST_STATE_PRESS_GREEN)on=LED_GREEN|LED_RED;
    else if(state==ST_STATE_ACCESS_DENIED||state==ST_STATE_CANCELED||state==ST_STATE_ERROR)on=LED_RED;
    else if(card_present())on=LED_GREEN;
    else on=LED_RED;
    PIO_SODR=LED_GREEN|LED_RED;
    PIO_CODR=on;
}
static void render(void)
{
    const uint8_t *cfg=flash_config();char name[17];const uint8_t *stored_name=cfg?config_name(cfg):0;for(unsigned i=0;i<16;++i)name[i]=stored_name?(char)stored_name[i]:0;name[16]=0;
    displayed_card_present=(uint8_t)card_present();
    update_button_lights();
    if(state==ST_STATE_UNENROLLED)lcd_screen("SAFEVOID","NOT REGISTERED","RUN SETUP");
    else if(state==ST_STATE_IDLE)lcd_screen("WINDOWS LOGIN",name,displayed_card_present?"CARD INSERTED":"INSERT CARD");
    else if(state==ST_STATE_INSERT_CARD)lcd_screen(pending_enrollment==2?"ADD BACKUP":"WINDOWS LOGIN",name,pending_enrollment==2?"INSERT CARD":"INSERT CARD");
    else if(state==ST_STATE_READING_CARD)lcd_screen(pending_enrollment==2?"ADD BACKUP":"WINDOWS LOGIN",name,"READING CARD");
    else if(state==ST_STATE_PRESS_GREEN)lcd_screen(pending_enrollment==2?"ADD BACKUP":"WINDOWS LOGIN",name,pending_enrollment==2?"CONFIRM ADD?":"CONFIRM? YES/NO");
    else if(state==ST_STATE_AUTH_OK)lcd_screen("WINDOWS LOGIN",name,"SIGNING IN");
    else if(state==ST_STATE_ENROLLED)lcd_screen("SAFEVOID",name,"REGISTERED");
    else if(state==ST_STATE_CANCELED)lcd_screen("WINDOWS LOGIN",name,"CANCELED");
    else if(state==ST_STATE_ACCESS_DENIED)lcd_screen("ACCESS DENIED",name,"REMOVE CARD");
    else lcd_screen("WINDOWS LOGIN",name,"DEVICE ERROR");
}
static void start_card_flow(int enrollment)
{
    pending_enrollment=(uint8_t)enrollment;
    buttons_armed=(PIO_PDSR&(BUTTON_GREEN|BUTTON_RED))==(BUTTON_GREEN|BUTTON_RED);
    state=ST_STATE_INSERT_CARD;last_result=ST_RESULT_OK;render();
}
static void process_request(const uint8_t request[64])
{
    uint8_t reply[64];for(unsigned i=0;i<64;++i)reply[i]=0;reply[0]=request[0];reply[1]=ST_RESULT_OK;reply[2]=state;
    const uint8_t *cfg=flash_config();
    if(request[0]==ST_CMD_INFO){reply[3]=cfg?(uint8_t)config_card_source(cfg):0;if(cfg){const uint8_t *name=config_name(cfg);for(unsigned i=0;i<16;++i)reply[ST_INFO_DEVICE_ID_OFFSET+i]=cfg[CFG_DEVICE_ID+i];for(unsigned i=0;i<16;++i)reply[ST_INFO_CARD_ID_OFFSET+i]=cfg[CFG_CARD_ID+i];for(unsigned i=0;i<16;++i)reply[ST_INFO_NAME_OFFSET+i]=name[i];if(config_has_backup(cfg))reply[ST_INFO_FLAGS_OFFSET]|=ST_INFO_HAS_BACKUP;}reply[ST_INFO_VERSION_OFFSET]=ST_PROTOCOL_MAJOR;reply[ST_INFO_VERSION_OFFSET+1]=ST_PROTOCOL_MINOR;}
    else if(request[0]==ST_CMD_ENROLL_BEGIN){
        if(cfg&&(PIO_PDSR&(BUTTON_GREEN|BUTTON_RED))!=0){reply[1]=ST_RESULT_ALREADY_ENROLLED;}
        else if(state==ST_STATE_READING_CARD||state==ST_STATE_PRESS_GREEN){reply[1]=ST_RESULT_BUSY;}
        else {for(unsigned i=0;i<32;++i)pending_secret[i]=request[ST_ENROLL_SECRET_OFFSET+i];for(unsigned i=0;i<16;++i)pending_name[i]=request[ST_ENROLL_NAME_OFFSET+i];allow_atr=request[ST_ENROLL_FLAGS_OFFSET]&ST_ENROLL_ALLOW_ATR;start_card_flow(1);}
    } else if(request[0]==ST_CMD_ADD_CARD_BEGIN){if(!cfg)reply[1]=ST_RESULT_NOT_ENROLLED;else if((PIO_PDSR&(BUTTON_GREEN|BUTTON_RED))!=0)reply[1]=ST_RESULT_ALREADY_ENROLLED;else if(state==ST_STATE_READING_CARD||state==ST_STATE_PRESS_GREEN)reply[1]=ST_RESULT_BUSY;else{allow_atr=request[ST_ENROLL_FLAGS_OFFSET]&ST_ENROLL_ALLOW_ATR;start_card_flow(2);}}
    else if(request[0]==ST_CMD_AUTH_BEGIN){if(!cfg)reply[1]=ST_RESULT_NOT_ENROLLED;else {for(unsigned i=0;i<32;++i)nonce[i]=request[ST_AUTH_NONCE_OFFSET+i];start_card_flow(0);}}
    else if(request[0]==ST_CMD_CANCEL){state=cfg?ST_STATE_IDLE:ST_STATE_UNENROLLED;pending_enrollment=0;card_off();wipe(nonce,32);wipe(wrap_key,32);render();}
    else if(request[0]==ST_CMD_STATUS){reply[1]=last_result;reply[2]=state;reply[3]=(uint8_t)current_card_source;if(state==ST_STATE_AUTH_OK){for(unsigned i=0;i<16;++i)reply[ST_AUTH_PROOF_OFFSET+i]=auth_proof[i];for(unsigned i=0;i<32;++i)reply[ST_AUTH_WRAP_KEY_OFFSET+i]=wrap_key[i];}}
    else reply[1]=ST_RESULT_BAD_COMMAND;
    usb_hid_send_reply(reply);
    if(request[0]==ST_CMD_STATUS&&state==ST_STATE_AUTH_OK){wipe(auth_proof,16);wipe(wrap_key,32);wipe(nonce,32);state=ST_STATE_IDLE;render();}
}
static void poll_card_flow(void)
{
    int present=card_present();
    if((state==ST_STATE_PRESS_GREEN||state==ST_STATE_ACCESS_DENIED)&&!present){card_off();current_card_source=ST_CARD_ID_NONE;state=ST_STATE_INSERT_CARD;render();}
    else if(state==ST_STATE_IDLE&&(uint8_t)present!=displayed_card_present)render();
    if(state==ST_STATE_INSERT_CARD&&card_present()){state=ST_STATE_READING_CARD;render();current_card_source=card_read_identity(current_card);
        if(!current_card_source){state=ST_STATE_ERROR;last_result=ST_RESULT_BAD_STATE;render();return;}
        if(pending_enrollment&&current_card_source==ST_CARD_ID_ATR&&!allow_atr){state=ST_STATE_ERROR;last_result=ST_RESULT_CARD_WEAK_ID;render();return;}
        const uint8_t *cfg=flash_config();if(pending_enrollment==2&&bytes_equal(current_card,cfg+CFG_CARD_ID,16)){state=ST_STATE_ERROR;last_result=ST_RESULT_DUPLICATE_CARD;render();return;}
        if(!pending_enrollment&&!bytes_equal(current_card,cfg+CFG_CARD_ID,16)&&!(config_has_backup(cfg)&&bytes_equal(current_card,cfg+CFG_BACKUP_CARD_ID,16))){state=ST_STATE_ACCESS_DENIED;last_result=ST_RESULT_OK;render();return;}
        state=ST_STATE_PRESS_GREEN;render();}
    int green_up=(PIO_PDSR&BUTTON_GREEN)!=0,red_up=(PIO_PDSR&BUTTON_RED)!=0;
    if(!buttons_armed){
        green_was_up=(uint8_t)green_up;red_was_up=(uint8_t)red_up;
        if(green_up&&red_up)buttons_armed=1;
        return;
    }
    if(state==ST_STATE_PRESS_GREEN&&green_was_up&&!green_up){if(pending_enrollment){int ok=pending_enrollment==2?save_backup_config():save_config();if(ok){state=ST_STATE_ENROLLED;last_result=ST_RESULT_OK;}else{state=ST_STATE_ERROR;last_result=ST_RESULT_FLASH;}wipe(pending_secret,32);wipe(pending_name,16);pending_enrollment=0;}
        else {derive_auth_response();state=ST_STATE_AUTH_OK;}render();}
    if((state==ST_STATE_PRESS_GREEN||state==ST_STATE_INSERT_CARD)&&red_was_up&&!red_up){state=ST_STATE_CANCELED;pending_enrollment=0;last_result=ST_RESULT_OK;wipe(pending_secret,32);wipe(nonce,32);card_off();render();}
    green_was_up=(uint8_t)green_up;red_was_up=(uint8_t)red_up;
}
int main(void)
{
    clocks_init();PMC_PCER=(1u<<2);PIO_PER=BUTTON_GREEN|BUTTON_RED|LED_GREEN|LED_RED|CARD_DETECT|CARD_RESET|CARD_POWER;
    PIO_ODR=BUTTON_GREEN|BUTTON_RED|CARD_DETECT;PIO_OER=LED_GREEN|LED_RED|CARD_RESET|CARD_POWER;PIO_PUER=BUTTON_GREEN|BUTTON_RED|CARD_DETECT;PIO_SODR=LED_GREEN|LED_RED;PIO_CODR=CARD_RESET|CARD_POWER;
    lcd_init();state=flash_config()?ST_STATE_IDLE:ST_STATE_UNENROLLED;render();usb_hid_init();
    for(;;){uint8_t request[64];usb_hid_poll();if(usb_hid_take_request(request))process_request(request);poll_card_flow();}
}
