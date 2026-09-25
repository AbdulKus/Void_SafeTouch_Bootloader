#include <stdint.h>
#include "../../protocol/safetouch_protocol.h"
#include "hardware.h"
#include "sha256.h"
#include "smartcard.h"

#define USART1_BASE 0xFFFC4000u
#define US_CR       REG32(USART1_BASE + 0x00u)
#define US_MR       REG32(USART1_BASE + 0x04u)
#define US_IDR      REG32(USART1_BASE + 0x0Cu)
#define US_CSR      REG32(USART1_BASE + 0x14u)
#define US_RHR      REG32(USART1_BASE + 0x18u)
#define US_THR      REG32(USART1_BASE + 0x1Cu)
#define US_BRGR     REG32(USART1_BASE + 0x20u)
#define US_RTOR     REG32(USART1_BASE + 0x24u)
#define US_TTGR     REG32(USART1_BASE + 0x28u)
#define US_FIDI     REG32(USART1_BASE + 0x40u)
#define ATR_MAX 32u

int card_present(void) { return !(PIO_PDSR & CARD_DETECT); }
void card_off(void)
{
    US_CR=(1u<<5)|(1u<<7); PIO_PER=CARD_IO|CARD_CLOCK; PIO_OER=CARD_IO|CARD_CLOCK|CARD_RESET|CARD_POWER;
    PIO_CODR=CARD_IO|CARD_CLOCK|CARD_RESET|CARD_POWER;
}
static void interface_init(void)
{
    PMC_PCER=(1u<<7); PIO_ASR=CARD_IO|CARD_CLOCK; PIO_PDR=CARD_IO|CARD_CLOCK;
    US_CR=(1u<<2)|(1u<<3)|(1u<<5)|(1u<<7)|(1u<<8); US_IDR=0xFFFFFFFFu;
    US_MR=0x04u|(1u<<18)|(3u<<24); US_BRGR=12u; US_FIDI=372u; US_RTOR=0; US_TTGR=5u;
    US_CR=(1u<<4)|(1u<<6);
}
static int receive(uint8_t *value,uint32_t timeout)
{
    while(timeout--){uint32_t s=US_CSR;if(s&((1u<<5)|(1u<<6)|(1u<<7))){US_CR=(1u<<8);return 0;}if(s&1u){*value=(uint8_t)US_RHR;return 1;}}
    return 0;
}
static int transmit(uint8_t value,uint32_t timeout)
{
    while(timeout--){if(US_CSR&(1u<<1)){US_THR=value;return 1;}}return 0;
}
static int read_atr(uint8_t atr[ATR_MAX],unsigned *atr_length)
{
    unsigned index=0,historical,group=0;uint8_t y;int tck=0;*atr_length=0;if(!card_present())return 0;
    card_off();delay(20000);interface_init();PIO_SODR=CARD_POWER;delay(50000);PIO_SODR=CARD_RESET;
    if(!receive(&atr[index++],2000000u)||(atr[0]!=0x3B&&atr[0]!=0x3F)||!receive(&atr[index++],1000000u))goto fail;
    historical=atr[1]&15u;y=atr[1]>>4;
    do{uint8_t td=0;if((y&1u)&&(index>=ATR_MAX||!receive(&atr[index++],1000000u)))goto fail;
       if((y&2u)&&(index>=ATR_MAX||!receive(&atr[index++],1000000u)))goto fail;
       if((y&4u)&&(index>=ATR_MAX||!receive(&atr[index++],1000000u)))goto fail;
       if(y&8u){if(index>=ATR_MAX||!receive(&td,1000000u))goto fail;atr[index++]=td;if((td&15u)!=0)tck=1;y=td>>4;}else y=0;
       if(++group>7u)goto fail;}while(y);
    while(historical--){if(index>=ATR_MAX||!receive(&atr[index++],1000000u))goto fail;}
    if(tck&&(index>=ATR_MAX||!receive(&atr[index++],1000000u)))goto fail;
    *atr_length=index;
    return 1;
fail: card_off();return 0;
}

/* Minimal T=0 exchange. It supports the SELECT and READ RECORD commands used
 * below. Response chaining (SW1=61) is handled with GET RESPONSE. */
static int t0_once(const uint8_t header[5],const uint8_t *output,unsigned output_length,
                   uint8_t *input,unsigned input_max,unsigned *input_length,uint16_t *status)
{
    unsigned sent=0,received=0;uint8_t value;for(unsigned i=0;i<5;++i)if(!transmit(header[i],500000u))return 0;
    for(;;){if(!receive(&value,1000000u))return 0;if(value==0x60)continue;
        uint8_t inverse_instruction=(uint8_t)~header[1];
        if(value==header[1]||value==inverse_instruction){
            if(output_length){unsigned count=value==header[1]?output_length-sent:1u;while(count--&&sent<output_length)if(!transmit(output[sent++],500000u))return 0;}
            else {unsigned expected=header[4]?header[4]:256u;unsigned count=value==header[1]?expected-received:1u;
                  while(count--&&received<expected){if(!receive(&value,1000000u))return 0;if(received<input_max)input[received]=value;received++;}}
            continue;
        }
        if((value&0xF0u)==0x60u||(value&0xF0u)==0x90u){uint8_t sw2;if(!receive(&sw2,1000000u))return 0;*status=((uint16_t)value<<8)|sw2;*input_length=received>input_max?input_max:received;return 1;}
    }
}
static int apdu(const uint8_t *command,unsigned command_length,uint8_t *response,unsigned maximum,unsigned *length)
{
    uint8_t header[5];uint16_t status;unsigned got=0;for(unsigned i=0;i<5;++i)header[i]=command[i];
    const uint8_t *data=command_length>5?command+5:0;unsigned data_length=command_length>5?command_length-5:0;
    if(!t0_once(header,data,data_length,response,maximum,&got,&status))return 0;
    if((status>>8)==0x61u){uint8_t get_response[5]={0x00,0xC0,0x00,0x00,(uint8_t)status};
        if(!t0_once(get_response,0,0,response,maximum,&got,&status))return 0;}
    *length=got;return status==0x9000u;
}

static int tlv_find(const uint8_t *data,unsigned length,uint32_t wanted,const uint8_t **value,unsigned *value_length)
{
    unsigned offset=0;while(offset+2u<=length){uint32_t tag=data[offset++];int constructed=(tag&0x20u)!=0;if((tag&31u)==31u){if(offset>=length)break;tag=(tag<<8)|data[offset++];}
        unsigned size=data[offset++];if(size&0x80u){unsigned bytes=size&0x7Fu;size=0;if(!bytes||bytes>2u||offset+bytes>length)break;while(bytes--)size=(size<<8)|data[offset++];}
        if(offset+size>length)break;
        if(tag==wanted){*value=data+offset;*value_length=size;return 1;}
        if(constructed&&tlv_find(data+offset,size,wanted,value,value_length))return 1;
        offset+=size;
    }
    return 0;
}

unsigned card_read_identity(uint8_t identity[16])
{
    static const uint8_t select_ppse[]={0x00,0xA4,0x04,0x00,0x0E,'2','P','A','Y','.','S','Y','S','.','D','D','F','0','1'};
    uint8_t atr[ATR_MAX],buffer[256],aid[32],digest[32];unsigned atr_length,response_length,aid_length=0;struct sha256_context hash;
    if(!read_atr(atr,&atr_length))return ST_CARD_ID_NONE;
    sha256_init(&hash);sha256_update(&hash,"SafeTouch card v1",17);sha256_update(&hash,atr,atr_length);
    if(apdu(select_ppse,sizeof(select_ppse),buffer,sizeof(buffer),&response_length)){
        const uint8_t *found;if(tlv_find(buffer,response_length,0x4Fu,&found,&aid_length)&&aid_length<=sizeof(aid)){
            for(unsigned i=0;i<aid_length;++i)aid[i]=found[i];
            uint8_t select[37]={0,0xA4,0x04,0};
            select[4]=(uint8_t)aid_length;
            for(unsigned i=0;i<aid_length;++i)select[5+i]=aid[i];
            if(apdu(select,5u+aid_length,buffer,sizeof(buffer),&response_length)){
                sha256_update(&hash,aid,aid_length);
                for(unsigned sfi=1;sfi<=10;++sfi)for(unsigned record=1;record<=4;++record){
                    uint8_t read_record[5]={0x00,0xB2,(uint8_t)record,(uint8_t)((sfi<<3)|4u),0};
                    if(apdu(read_record,5,buffer,sizeof(buffer),&response_length)){
                        const uint8_t *pan;unsigned pan_length;
                        if((tlv_find(buffer,response_length,0x5Au,&pan,&pan_length)||tlv_find(buffer,response_length,0x57u,&pan,&pan_length))&&pan_length){
                            sha256_update(&hash,pan,pan_length);sha256_final(&hash,digest);for(unsigned i=0;i<16;++i)identity[i]=digest[i];return ST_CARD_ID_EMV;
                        }
                    }
                }
            }
        }
    }
    sha256_final(&hash,digest);for(unsigned i=0;i<16;++i)identity[i]=digest[i];return ST_CARD_ID_ATR;
}
