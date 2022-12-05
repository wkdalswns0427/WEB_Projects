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

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

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
void cdc_task(void);
void webserial_task(void);

void uart_send_confirm(void);
void uart_send_price(void);
void uart_make_crc(void);
void uart_task(void);



/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();
  uart_init(UART_ID, BAUD_RATE);

  // Set the TX and RX pins by using the function select on the GPIO
  // Set datasheet for more information on function select
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
  uart_set_fifo_enabled(UART_ID, false);

  while (1)
  {

    uart_task();
    tud_task(); // tinyusb device task
    cdc_task();
    webserial_task();
    led_blinking_task();
  }

  return 0;
}

// send characters to both CDC and WebUSB
// void echo_all(uint8_t buf[], uint32_t count)
// {
//   // echo to web serial
//   if ( web_serial_connected )
//   {
//     tud_vendor_write(buf, count);
//   }

//   // echo to cdc
//   if ( tud_cdc_connected() )
//   {
//     for(uint32_t i=0; i<count; i++)
//     {
//       tud_cdc_write_char(buf[i]);

//       if ( buf[i] == '\r' ) tud_cdc_write_char('\n');
//     }
//     tud_cdc_write_flush();
//   }
// }

void send_confirm(uint8_t buf[], uint32_t count)
{
  // echo to web serial
  uint8_t confirm[4] = {'6','6','6','6'};
  if ( web_serial_connected )
  {
    // tud_vendor_write(buf, count);
    tud_vendor_write(confirm, 4);
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

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

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

void webserial_task(void)
{
  if ( web_serial_connected )
  {
    if ( tud_vendor_available() )
    {
      uint8_t buf[64];
      uint32_t count = tud_vendor_read(buf, sizeof(buf));
      
      // echo back to both web serial and cdc
      send_confirm(buf, count); //reacts to webserial
    }
  }
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if ( tud_cdc_available() )
    {
      uint8_t buf[64];

      uint32_t count = tud_cdc_read(buf, sizeof(buf));

      // echo back to both web serial and cdc
      send_confirm(buf, count);
    }
  }
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

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
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

void uart_send_confirm(void)
{
    uint8_t conf_msg[4] = {0x06, 0x06, 0x06, 0x06};
    uart_write_blocking(UART_ID, conf_msg, 4);
};

void uart_send_price(void){
    // uint8_t buf[64];
    // uint8_t price = ws_get_price(buf);
    // unsigned short icrc = CalcCRC();
    // uint8_t CRC_L = icrc | 0xff;
    // uint8_t CRC_H = (icrc>>8) | 0xff;

    // dummy price data
    uint8_t price[12] = {0x02, 0x00, 0x08, 0xf8, 0x20, 0x02, 0x00, 0x10, 0x00, 0x03, 0x0D, 0x47};
    uart_write_blocking(UART_ID, price, sizeof(price));
}

void uart_task(){
  if(uart_is_readable(UART_ID) != 0){
    led_blinking_task();
    uint8_t initdata[8];
    uart_read_blocking(UART_ID, initdata, sizeof(initdata));
    // uart_write_blocking(UART_ID, initdata, 8);
    tud_vendor_write(initdata, sizeof(initdata));
    uart_send_confirm();
    sleep_ms(1000);
    uart_send_price();
    sleep_ms(2500);
    uart_send_confirm();
  }
  // else{
  //   const char not_in = "N";
  //   tud_vendor_write(not_in, sizeof(not_in));
  // }
}

//-----------------------------------------------------------------------------------------------------------------
// /* 
//  * The MIT License (MIT)
//  *
//  * Copyright (c) 2019 Ha Thach (tinyusb.org)
//  *
//  * Permission is hereby granted, free of charge, to any person obtaining a copy
//  * of this software and associated documentation files (the "Software"), to deal
//  * in the Software without restriction, including without limitation the rights
//  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  * copies of the Software, and to permit persons to whom the Software is
//  * furnished to do so, subject to the following conditions:
//  *
//  * The above copyright notice and this permission notice shall be included in
//  * all copies or substantial portions of the Software.
//  *
//  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  * THE SOFTWARE.
//  *
//  */

// /* This example demonstrates WebUSB as web serial with browser with WebUSB support (e.g Chrome).
//  * After enumerated successfully, browser will pop-up notification
//  * with URL to landing page, click on it to test
//  *  - Click "Connect" and select device, When connected the on-board LED will litted up.
//  *  - Any charters received from either webusb/Serial will be echo back to webusb and Serial
//  *
//  * Note:
//  * - The WebUSB landing page notification is currently disabled in Chrome
//  * on Windows due to Chromium issue 656702 (https://crbug.com/656702). You have to
//  * go to landing page (below) to test
//  *
//  * - On Windows 7 and prior: You need to use Zadig tool to manually bind the
//  * WebUSB interface with the WinUSB driver for Chrome to access. From windows 8 and 10, this
//  * is done automatically by firmware.
//  *
//  * - On Linux/macOS, udev permission may need to be updated by
//  *   - copying '/examples/device/99-tinyusb.rules' file to /etc/udev/rules.d/ then
//  *   - run 'sudo udevadm control --reload-rules && sudo udevadm trigger'
//  */

// #include <stdlib.h>
// #include <stdio.h>
// #include <string.h>

// #include "bsp/board.h"
// #include "tusb.h"
// #include "usb_descriptors.h"

// //--------------------------------------------------------------------+
// // MACRO CONSTANT TYPEDEF PROTYPES
// //--------------------------------------------------------------------+

// /* Blink pattern
//  * - 250 ms  : device not mounted
//  * - 1000 ms : device mounted
//  * - 2500 ms : device is suspended
//  */
// enum  {
//   BLINK_NOT_MOUNTED = 250,
//   BLINK_MOUNTED     = 1000,
//   BLINK_SUSPENDED   = 2500,

//   BLINK_ALWAYS_ON   = UINT32_MAX,
//   BLINK_ALWAYS_OFF  = 0
// };

// static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// #define URL  "example.tinyusb.org/webusb-serial/"

// const tusb_desc_webusb_url_t desc_url =
// {
//   .bLength         = 3 + sizeof(URL) - 1,
//   .bDescriptorType = 3, // WEBUSB URL type
//   .bScheme         = 1, // 0: http, 1: https
//   .url             = URL
// };

// static bool web_serial_connected = false;

// //------------- prototypes -------------//
// void led_blinking_task(void);
// void cdc_task(void);
// void webserial_task(void);

// /*------------- MAIN -------------*/
// int main(void)
// {
//   board_init();

//   tusb_init();

//   while (1)
//   {
//     tud_task(); // tinyusb device task
//     cdc_task();
//     webserial_task();
//     led_blinking_task();
//   }

//   return 0;
// }

// // send characters to both CDC and WebUSB
// void echo_all(uint8_t buf[], uint32_t count)
// {
//   // echo to web serial
//   if ( web_serial_connected )
//   {
//     tud_vendor_write(buf, count);
//   }

//   // echo to cdc
//   if ( tud_cdc_connected() )
//   {
//     for(uint32_t i=0; i<count; i++)
//     {
//       tud_cdc_write_char(buf[i]);

//       if ( buf[i] == '\r' ) tud_cdc_write_char('\n');
//     }
//     tud_cdc_write_flush();
//   }
// }

// //--------------------------------------------------------------------+
// // Device callbacks
// //--------------------------------------------------------------------+

// // Invoked when device is mounted
// void tud_mount_cb(void)
// {
//   blink_interval_ms = BLINK_MOUNTED;
// }

// // Invoked when device is unmounted
// void tud_umount_cb(void)
// {
//   blink_interval_ms = BLINK_NOT_MOUNTED;
// }

// // Invoked when usb bus is suspended
// // remote_wakeup_en : if host allow us  to perform remote wakeup
// // Within 7ms, device must draw an average of current less than 2.5 mA from bus
// void tud_suspend_cb(bool remote_wakeup_en)
// {
//   (void) remote_wakeup_en;
//   blink_interval_ms = BLINK_SUSPENDED;
// }

// // Invoked when usb bus is resumed
// void tud_resume_cb(void)
// {
//   blink_interval_ms = BLINK_MOUNTED;
// }

// //--------------------------------------------------------------------+
// // WebUSB use vendor class
// //--------------------------------------------------------------------+

// // Invoked when a control transfer occurred on an interface of this class
// // Driver response accordingly to the request and the transfer stage (setup/data/ack)
// // return false to stall control endpoint (e.g unsupported request)
// bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
// {
//   // nothing to with DATA & ACK stage
//   if (stage != CONTROL_STAGE_SETUP) return true;

//   switch (request->bmRequestType_bit.type)
//   {
//     case TUSB_REQ_TYPE_VENDOR:
//       switch (request->bRequest)
//       {
//         case VENDOR_REQUEST_WEBUSB:
//           // match vendor request in BOS descriptor
//           // Get landing page url
//           return tud_control_xfer(rhport, request, (void*) &desc_url, desc_url.bLength);

//         case VENDOR_REQUEST_MICROSOFT:
//           if ( request->wIndex == 7 )
//           {
//             // Get Microsoft OS 2.0 compatible descriptor
//             uint16_t total_len;
//             memcpy(&total_len, desc_ms_os_20+8, 2);

//             return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
//           }else
//           {
//             return false;
//           }

//         default: break;
//       }
//     break;

//     case TUSB_REQ_TYPE_CLASS:
//       if (request->bRequest == 0x22)
//       {
//         // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
//         web_serial_connected = (request->wValue != 0);

//         // Always lit LED if connected
//         if ( web_serial_connected )
//         {
//           board_led_write(true);
//           blink_interval_ms = BLINK_ALWAYS_ON;

//           tud_vendor_write_str("\r\nTinyUSB WebUSB device example\r\n");
//         }else
//         {
//           blink_interval_ms = BLINK_MOUNTED;
//         }

//         // response with status OK
//         return tud_control_status(rhport, request);
//       }
//     break;

//     default: break;
//   }

//   // stall unknown request
//   return false;
// }

// void webserial_task(void)
// {
//   if ( web_serial_connected )
//   {
//     if ( tud_vendor_available() )
//     {
//       uint8_t buf[64];
//       uint32_t count = tud_vendor_read(buf, sizeof(buf));

//       // echo back to both web serial and cdc
//       echo_all(buf, count);
//     }
//   }
// }


// //--------------------------------------------------------------------+
// // USB CDC
// //--------------------------------------------------------------------+
// void cdc_task(void)
// {
//   if ( tud_cdc_connected() )
//   {
//     // connected and there are data available
//     if ( tud_cdc_available() )
//     {
//       uint8_t buf[64];

//       uint32_t count = tud_cdc_read(buf, sizeof(buf));

//       // echo back to both web serial and cdc
//       echo_all(buf, count);
//     }
//   }
// }

// // Invoked when cdc when line state changed e.g connected/disconnected
// void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
// {
//   (void) itf;

//   // connected
//   if ( dtr && rts )
//   {
//     // print initial message when connected
//     tud_cdc_write_str("\r\nTinyUSB WebUSB device example\r\n");
//   }
// }

// // Invoked when CDC interface received data from host
// void tud_cdc_rx_cb(uint8_t itf)
// {
//   (void) itf;
// }

// //--------------------------------------------------------------------+
// // BLINKING TASK
// //--------------------------------------------------------------------+
// void led_blinking_task(void)
// {
//   static uint32_t start_ms = 0;
//   static bool led_state = false;

//   // Blink every interval ms
//   if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
//   start_ms += blink_interval_ms;

//   board_led_write(led_state);
//   led_state = 1 - led_state; // toggle
// }

// ---------------------------------------------------------------

// int main() {
//     // Set up our UART with a basic baud rate.
//     uart_init(UART_ID, 2400);

//     // Set the TX and RX pins by using the function select on the GPIO
//     // Set datasheet for more information on function select
//     gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
//     gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

//     // Actually, we want a different speed
//     // The call will return the actual baud rate selected, which will be as close as
//     // possible to that requested
//     int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

//     // Set UART flow control CTS/RTS, we don't want these, so turn them off
//     uart_set_hw_flow(UART_ID, false, false);

//     // Set our data format
//     uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

//     // Turn off FIFO's - we want to do this character by character
//     uart_set_fifo_enabled(UART_ID, false);

//     // Set up a RX interrupt
//     // We need to set up the handler first
//     // Select correct interrupt for the UART we are using
//     int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

//     // And set up and enable the interrupt handlers
//     irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
//     irq_set_enabled(UART_IRQ, true);

//     // Now enable the UART to send interrupts - RX only
//     uart_set_irq_enables(UART_ID, true, false);

//     // OK, all set up.
//     // Lets send a basic string out, and then run a loop and wait for RX interrupts
//     // The handler will count them, but also reflect the incoming data back with a slight change!
//     uart_puts(UART_ID, "\nHello, uart interrupts\n");

//     while (1)
//         tight_loop_contents();
// }