// CDC --> terminal // vendor --> javascript console
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define UART_ID uart1
#define BAUD_RATE 115200

#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,

  BLINK_ALWAYS_ON   = UINT32_MAX,
  BLINK_ALWAYS_OFF  = 0
};

uint8_t prbuf[64];
uint32_t cnt;

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

#define URL  "example.tinyusb.org/webusb-serial/"

const tusb_desc_webusb_url_t desc_url =
{
  .bLength         = 3 + sizeof(URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 1, // 0: http, 1: https
  .url             = URL
};

static bool web_serial_connected = false;

//------------- prototypes -------------//
void led_blinking_task(void);
void webserial_task(void);
void SPD_500(void);

void uart_send_confirm(void);
void uart_send_price(void);
void uart_make_crc(void);
void uart_task(void);



/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();
  // uart_init(UART_ID, BAUD_RATE);

  // gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  // gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  // uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
  // uart_set_fifo_enabled(UART_ID, false);

  while (1)
  {
    SPD_500();
    tud_task();
    led_blinking_task();

  }
  return 0;
}

// send characters to both CDC and WebUSB
void echo_all(uint8_t buf[], uint32_t count)
{
  // echo to web serial
  if ( web_serial_connected )
  {
    tud_vendor_write(buf, count);
  }

  // echo to cdc
  if ( tud_cdc_connected() )
  {
    for(uint32_t i=0; i<count; i++)
    {
      tud_cdc_write_char(buf[i]);

      if ( buf[i] == '\r' ) tud_cdc_write_char('\n');
    }
    tud_cdc_write_flush();
  }
}

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case VENDOR_REQUEST_WEBUSB:
          // match vendor request in BOS descriptor
          // Get landing page url
          return tud_control_xfer(rhport, request, (void*) &desc_url, desc_url.bLength);

        case VENDOR_REQUEST_MICROSOFT:
          if ( request->wIndex == 7 )
          {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
          }else
          {
            return false;
          }

        default: break;
      }
    break;

    case TUSB_REQ_TYPE_CLASS:
      if (request->bRequest == 0x22)
      {
        // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
        web_serial_connected = (request->wValue != 0);

        // Always lit LED if connected
        if ( web_serial_connected )
        {
          board_led_write(true);
          blink_interval_ms = BLINK_ALWAYS_ON;

          tud_vendor_write_str("\r\nTinyUSB WebUSB WUAAAAA\r\n");
        }else
        {
          blink_interval_ms = BLINK_MOUNTED;
        }

        // response with status OK
        return tud_control_status(rhport, request);
      }
    break;

    default: break;
  }

  // stall unknown request
  return false;
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;

  // connected
  if ( dtr && rts )
  {
    // print initial message when connected
    tud_cdc_write_str("\r\nTinyUSB WebUSB WUAAAAA\r\n");
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

void SPD_500(void)
{
  if ( web_serial_connected )
  {
    if ( tud_vendor_available() )
    {
      cnt = tud_vendor_read(prbuf, sizeof(prbuf));
      tud_vendor_write_str("refresh prbuf");
      echo_all(prbuf, cnt);
    }
  }
  uart_task();
}

// send confirm message 6 6 6 6
void uart_send_confirm(void)
{
  uint8_t conf_msg[4] = {0x06, 0x06, 0x06, 0x06};
  uart_write_blocking(UART_ID, conf_msg, 4);
};


void uart_send_price(void)
{
  uart_write_blocking(UART_ID, prbuf, cnt); 
}

void uart_task(){
  uart_init(UART_ID, BAUD_RATE);

  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
  uart_set_fifo_enabled(UART_ID, false);

  if(uart_is_readable_within_us(UART_ID,1000) != 0){
    led_blinking_task();
    
    uint8_t initdata[8];
    tud_vendor_write(initdata, sizeof(initdata));
    uart_read_blocking(UART_ID, initdata, sizeof(initdata));
    tud_vendor_write(initdata, sizeof(initdata));

    uart_send_confirm();
    sleep_ms(1000);

    tud_vendor_write(prbuf, cnt);

    uart_send_price();
    sleep_ms(2000);

    uart_send_confirm();
  }
  uart_deinit(UART_ID);
}
