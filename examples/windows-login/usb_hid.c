#include <stdint.h>
#include "../../protocol/safetouch_protocol.h"
#include "hardware.h"
#include "usb_hid.h"

#define UDP_BASE       0xFFFB0000u
#define UDP_GLB_STAT   REG32(UDP_BASE + 0x04u)
#define UDP_FADDR      REG32(UDP_BASE + 0x08u)
#define UDP_IDR        REG32(UDP_BASE + 0x14u)
#define UDP_ISR        REG32(UDP_BASE + 0x1Cu)
#define UDP_ICR        REG32(UDP_BASE + 0x20u)
#define UDP_RST_EP     REG32(UDP_BASE + 0x28u)
#define UDP_CSR(ep)    REG32(UDP_BASE + 0x30u + ((ep) * 4u))
#define UDP_FDR(ep)    REG32(UDP_BASE + 0x50u + ((ep) * 4u))
#define UDP_TXVC       REG32(UDP_BASE + 0x74u)
#define UDP_TXCOMP (1u<<0)
#define UDP_RX_DATA_BK0 (1u<<1)
#define UDP_RXSETUP (1u<<2)
#define UDP_STALLSENT (1u<<3)
#define UDP_TXPKTRDY (1u<<4)
#define UDP_FORCESTALL (1u<<5)
#define UDP_RX_DATA_BK1 (1u<<6)
#define UDP_DIR (1u<<7)
#define UDP_EPTYPE_CTRL (0u<<8)
#define UDP_EPTYPE_INT_OUT (3u<<8)
#define UDP_EPTYPE_INT_IN (7u<<8)
#define UDP_EPEDS (1u<<15)
#define UDP_RXBYTECNT(v) (((v)>>16)&0x7FFu)
#define UDP_FEN (1u<<8)
#define UDP_FADDEN (1u<<0)
#define UDP_CONFG (1u<<1)
#define UDP_ENDBUSRES (1u<<12)
#define EP0_SIZE 8u

struct setup_packet { uint8_t type,request;uint16_t value,index,length; };
static const uint8_t device_descriptor[]={18,1,0,2,0,0,0,EP0_SIZE,
 (uint8_t)ST_USB_VID,(uint8_t)(ST_USB_VID>>8),(uint8_t)ST_USB_PID_LOGIN,(uint8_t)(ST_USB_PID_LOGIN>>8),0,1,1,2,3,1};
static const uint8_t report_descriptor[]={0x06,0,0xFF,0x09,1,0xA1,1,0x15,0,0x26,0xFF,0,0x75,8,
 0x95,ST_REPORT_SIZE,0x09,1,0x81,2,0x95,ST_REPORT_SIZE,0x09,1,0x91,2,0xC0};
static const uint8_t hid_descriptor[]={9,0x21,0x11,1,0,1,0x22,sizeof(report_descriptor),0};
static const uint8_t config_descriptor[]={9,2,41,0,1,1,0,0x80,25,9,4,0,0,2,3,0,0,4,
 9,0x21,0x11,1,0,1,0x22,sizeof(report_descriptor),0,7,5,0x81,3,ST_REPORT_SIZE,0,10,7,5,2,3,ST_REPORT_SIZE,0,10};
static const uint8_t string_language[]={4,3,9,4};
static const uint8_t string_manufacturer[]={10,3,'V',0,'o',0,'i',0,'d',0};
static const uint8_t string_product[]={30,3,'S',0,'a',0,'f',0,'e',0,'V',0,'o',0,'i',0,'d',0,' ',0,'L',0,'o',0,'g',0,'i',0,'n',0};
static const uint8_t string_serial[]={20,3,'S',0,'T',0,'L',0,'-',0,'0',0,'0',0,'0',0,'1',0};
static const uint8_t string_interface[]={36,3,'S',0,'a',0,'f',0,'e',0,'V',0,'o',0,'i',0,'d',0,' ',0,'A',0,'u',0,'t',0,'h',0,' ',0,'H',0,'I',0,'D',0};

static uint8_t ep0_buffer[64],request_report[64],reply_report[64];
static const uint8_t *ep0_data;static uint16_t ep0_length,ep0_offset;static uint8_t ep0_zlp,ep0_active;
static uint8_t pending_address,address_pending,pending_configuration,configuration_pending,configured;
static volatile uint8_t request_ready,reply_pending;
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static void csr_set(unsigned ep,uint32_t bits){uint32_t v=UDP_CSR(ep);v|=0x4Fu;v&=~0x30u;v|=bits;UDP_CSR(ep)=v;while((UDP_CSR(ep)&bits)!=bits){}}
static void csr_clear(unsigned ep,uint32_t bits){uint32_t v=UDP_CSR(ep);v|=0x4Fu;v&=~0x30u;v&=~bits;UDP_CSR(ep)=v;while(UDP_CSR(ep)&bits){}}
static void ep0_next(void){uint16_t left=ep0_length-ep0_offset,amount=left>EP0_SIZE?EP0_SIZE:left;for(uint16_t i=0;i<amount;++i)UDP_FDR(0)=ep0_data[ep0_offset++];csr_set(0,UDP_TXPKTRDY);}
static void ep0_send(const uint8_t *data,uint16_t length,uint16_t requested){if(length>requested)length=requested;ep0_data=data;ep0_length=length;ep0_offset=0;ep0_zlp=(length<requested&&(length%EP0_SIZE)==0);ep0_active=1;ep0_next();}
static void ep0_ack(void){ep0_active=0;csr_set(0,UDP_TXPKTRDY);}static void ep0_stall(void){csr_set(0,UDP_FORCESTALL);}
static void handle_setup(const uint8_t raw[8])
{
    struct setup_packet r={raw[0],raw[1],le16(raw+2),le16(raw+4),le16(raw+6)};
    if((r.type&0x80u)&&r.request==6){uint8_t type=(uint8_t)(r.value>>8),index=(uint8_t)r.value;
        if(type==1)ep0_send(device_descriptor,sizeof(device_descriptor),r.length);else if(type==2)ep0_send(config_descriptor,sizeof(config_descriptor),r.length);
        else if(type==0x21)ep0_send(hid_descriptor,sizeof(hid_descriptor),r.length);else if(type==0x22)ep0_send(report_descriptor,sizeof(report_descriptor),r.length);
        else if(type==3&&index==0)ep0_send(string_language,sizeof(string_language),r.length);else if(type==3&&index==1)ep0_send(string_manufacturer,sizeof(string_manufacturer),r.length);
        else if(type==3&&index==2)ep0_send(string_product,sizeof(string_product),r.length);else if(type==3&&index==3)ep0_send(string_serial,sizeof(string_serial),r.length);
        else if(type==3&&index==4)ep0_send(string_interface,sizeof(string_interface),r.length);
        else ep0_stall();
        return;
    }
    if((r.type&0x60u)==0){if(r.request==0){ep0_buffer[0]=ep0_buffer[1]=0;ep0_send(ep0_buffer,2,r.length);}else if(r.request==5){pending_address=(uint8_t)(r.value&0x7Fu);address_pending=1;ep0_ack();}
        else if(r.request==8){ep0_buffer[0]=configured?1:0;ep0_send(ep0_buffer,1,r.length);}else if(r.request==9){pending_configuration=(uint8_t)r.value;configuration_pending=1;ep0_ack();}
        else if(r.request==10){ep0_buffer[0]=0;ep0_send(ep0_buffer,1,r.length);}
        else if(r.request==11||r.request==1||r.request==3)ep0_ack();
        else ep0_stall();
        return;
    }
    if((r.type&0x60u)==0x20u){if((r.type&0x80u)&&r.request==1){for(unsigned i=0;i<64;++i)ep0_buffer[i]=0;ep0_buffer[0]='S';ep0_buffer[1]='T';ep0_buffer[2]=ST_PROTOCOL_MAJOR;ep0_send(ep0_buffer,64,r.length);}
        else if((r.type&0x80u)&&r.request==2){ep0_buffer[0]=0;ep0_send(ep0_buffer,1,r.length);}
        else if(!(r.type&0x80u)&&(r.request==9||r.request==10||r.request==11))ep0_ack();
        else ep0_stall();
        return;
    }
    ep0_stall();
}
static void reset_endpoints(void){UDP_RST_EP=7;UDP_RST_EP=0;UDP_FADDR=UDP_FEN;UDP_GLB_STAT=0;UDP_CSR(0)=UDP_EPEDS|UDP_EPTYPE_CTRL;while(!(UDP_CSR(0)&UDP_EPEDS)){}ep0_active=address_pending=configuration_pending=configured=0;}
static void finish_ack(void){if(address_pending){UDP_FADDR=UDP_FEN|pending_address;UDP_GLB_STAT=pending_address?UDP_FADDEN:0;address_pending=0;}if(configuration_pending){configured=pending_configuration?1:0;if(configured){UDP_GLB_STAT=UDP_FADDEN|UDP_CONFG;UDP_CSR(1)=UDP_EPEDS|UDP_EPTYPE_INT_IN;while(!(UDP_CSR(1)&UDP_EPEDS)){}UDP_CSR(2)=UDP_EPEDS|UDP_EPTYPE_INT_OUT;while(!(UDP_CSR(2)&UDP_EPEDS)){}}else UDP_GLB_STAT=UDP_FADDEN;configuration_pending=0;}}
static void poll_ep0(void)
{
    uint32_t csr=UDP_CSR(0);if(csr&UDP_RXSETUP){uint8_t raw[8]={0};unsigned count=UDP_RXBYTECNT(csr);for(unsigned i=0;i<count;++i){uint8_t b=(uint8_t)UDP_FDR(0);if(i<8)raw[i]=b;}
        if(count==8&&(raw[0]&0x80u))csr_set(0,UDP_DIR);else csr_clear(0,UDP_DIR);csr_clear(0,UDP_RXSETUP);if(count==8)handle_setup(raw);else ep0_stall();return;}
    if(csr&UDP_TXCOMP){csr_clear(0,UDP_TXCOMP);if(ep0_active){if(ep0_offset<ep0_length)ep0_next();else if(ep0_zlp){ep0_zlp=0;csr_set(0,UDP_TXPKTRDY);}else ep0_active=0;}else finish_ack();}
    csr=UDP_CSR(0);if(csr&UDP_RX_DATA_BK0)csr_clear(0,UDP_RX_DATA_BK0);if(csr&UDP_STALLSENT)csr_clear(0,UDP_STALLSENT);
}
static void poll_data(void)
{
    uint32_t csr=UDP_CSR(2),bank=csr&UDP_RX_DATA_BK0?UDP_RX_DATA_BK0:(csr&UDP_RX_DATA_BK1?UDP_RX_DATA_BK1:0);
    if(bank){unsigned count=UDP_RXBYTECNT(csr);uint8_t temporary[64];for(unsigned i=0;i<count;++i){uint8_t b=(uint8_t)UDP_FDR(2);if(i<64)temporary[i]=b;}csr_clear(2,bank);
        if(count==64&&!request_ready){for(unsigned i=0;i<64;++i)request_report[i]=temporary[i];request_ready=1;}}
    if(UDP_CSR(1)&UDP_TXCOMP)csr_clear(1,UDP_TXCOMP);
    if(reply_pending&&!(UDP_CSR(1)&UDP_TXPKTRDY)){for(unsigned i=0;i<64;++i)UDP_FDR(1)=reply_report[i];csr_set(1,UDP_TXPKTRDY);reply_pending=0;}
}
void usb_hid_init(void)
{
    PMC_PCER=(1u<<2)|(1u<<11);PMC_SCER=(1u<<7);PIO_PER=USB_PULLUP;PIO_OER=USB_PULLUP;PIO_SODR=USB_PULLUP;delay(200000);
    UDP_IDR=0xFFFFFFFFu;UDP_ICR=0xFFFFFFFFu;UDP_TXVC=0;reset_endpoints();PIO_CODR=USB_PULLUP;
}
void usb_hid_poll(void){uint32_t isr=UDP_ISR;if(isr&UDP_ENDBUSRES){UDP_ICR=UDP_ENDBUSRES;reset_endpoints();}poll_ep0();if(configured)poll_data();}
int usb_hid_take_request(uint8_t report[64]){if(!request_ready)return 0;for(unsigned i=0;i<64;++i)report[i]=request_report[i];request_ready=0;return 1;}
int usb_hid_send_reply(const uint8_t report[64]){if(reply_pending)return 0;for(unsigned i=0;i<64;++i)reply_report[i]=report[i];reply_pending=1;return 1;}
