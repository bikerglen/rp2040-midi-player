//---------------------------------------------------------------------------------------------
// includes
//

// C
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// pico
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/audio_i2s.h"

// bsp
#include "bsp/board_api.h"

// hardware
#include "hardware/clocks.h"
#include "hardware/i2c.h"

// tiny usb and midi
#include "tusb.h"

// sdcard and fatfs
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include "sd_card.h"


//---------------------------------------------------------------------------------------------
// defines
//

#define BUTTON_FIFO_DEPTH 8

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 128

// figure out number of iic-connected status leds from cmake command line
#if defined STATUS_LED_SINGLE
  #define HAS_STATUS_LEDS
  #define NUM_STATUS_LEDS 1
#elif defined STATUS_LED_TRIPLE
  #define HAS_STATUS_LEDS
  #define NUM_STATUS_LEDS 3
#endif

// figure out what iic pins the status leds are connected to
#ifdef HAS_STATUS_LEDS
  #ifdef ADAFRUIT_QTPY_RP2040
    #define C_I2C_NUMBER  1
    #define C_I2C_SDA_PIN 22
    #define C_I2C_SCL_PIN 23
  #else
    #define C_I2C_NUMBER  0
    #define C_I2C_SDA_PIN 16
    #define C_I2C_SCL_PIN 17
  #endif

  // TCA9536 address depends on part suffix, this is for the no suffix version
  // these are bits [7:1] of the byte, bit [0] is R/W bit.
  #define TCA9536_I2C_ADDRESS 0x41
#endif


//---------------------------------------------------------------------------------------------
// typedefs
//

typedef uint8_t midi_button_t;


//---------------------------------------------------------------------------------------------
// prototypes -- core 0
//

#ifdef HAS_STATUS_LEDS
void init_status_leds (void);
void update_status_leds (bool midi_device_connected, bool sdcard_mounted, bool file_opened);
#endif
bool repeating_timer_callback_200Hz (struct repeating_timer *t);
struct audio_buffer_pool *init_audio (void);


//---------------------------------------------------------------------------------------------
// globals
//

// LED Configuration
#ifdef PICO_DEFAULT_LED_PIN
bi_decl(bi_program_feature_group(0x1111, 0, "LED Configuration"));
bi_decl(bi_ptr_int32(0x1111, 0, LED_PIN, PICO_DEFAULT_LED_PIN));
#endif

// UART Configuration
// PICO: 0/0/1/115200
// QTPY: 1/20/5/115200
bi_decl(bi_program_feature_group(0x1112, 0, "UART Configuration"));
bi_decl(bi_ptr_int32(0x1112, 0, UART_NUMBER, PICO_DEFAULT_UART));
bi_decl(bi_ptr_int32(0x1112, 0, UART_TX_PIN, PICO_DEFAULT_UART_TX_PIN));
bi_decl(bi_ptr_int32(0x1112, 0, UART_RX_PIN, PICO_DEFAULT_UART_RX_PIN));
bi_decl(bi_ptr_int32(0x1112, 0, UART_BAUD,   115200));

// Status LED Configuration
#ifdef HAS_STATUS_LEDS
bi_decl(bi_program_feature_group(0x1114, 0, "IIC Status Led Configuration"));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_NUMBER,    C_I2C_NUMBER));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_SDA_PIN,   C_I2C_SDA_PIN));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_SCL_PIN,   C_I2C_SCL_PIN));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_LED_COUNT, NUM_STATUS_LEDS));
#endif

// 200 Hz flag from periodic timer interrupt to main loop
volatile bool flag200 = false;

// midi device index
static uint8_t dev_idx = TUSB_INDEX_INVALID_8;

// queue to move midi button presses from midi rx callback to main loop
queue_t button_fifo;


//---------------------------------------------------------------------------------------------
// main
//

int main (void)
{
  // local system variables
  struct repeating_timer timer_200Hz;
  uint8_t led_timer = 0;
  bool midi_connected = false;
  bool sdcard_mounted = false;
  bool file_opened = false;
  FATFS fs;
  FRESULT fr;
  FIL fil;
  uint8_t file_buffer[4*SAMPLES_PER_BUFFER];

  // sleep one second after power up / reset
  sleep_ms (1000);

  // initialize board
  board_init ();

  // initialize uart stdio
  stdio_uart_init_full (UART_INSTANCE(UART_NUMBER), UART_BAUD, UART_TX_PIN, UART_RX_PIN);

#ifdef PICO_DEFAULT_LED_PIN
  // initialize led to off
  gpio_init (LED_PIN);
  gpio_set_dir (LED_PIN, GPIO_OUT);
  gpio_put (LED_PIN, 0);
#endif

  // hello world
  printf ("\n\n%s v%s starting...\n\n", 
  PICO_PROGRAM_NAME, PICO_PROGRAM_VERSION_STRING);

  // initialize button queue
  queue_init (&button_fifo, sizeof (midi_button_t), BUTTON_FIFO_DEPTH);

#ifdef HAS_STATUS_LEDS
  init_status_leds ();
#endif 

  // set up 5 ms / 200 Hz repeating timer
  add_repeating_timer_ms (-5, repeating_timer_callback_200Hz, NULL, &timer_200Hz);

  // init tiny usb
  tusb_init ();

  // init audio
  struct audio_buffer_pool *ap = init_audio();

  while (true) {

    //----------------------------------------
    // tiny usb tasks
    //----------------------------------------

    tuh_task ();
    midi_connected = dev_idx != TUSB_INDEX_INVALID_8 && tuh_midi_mounted(dev_idx);


    //----------------------------------------
    // sd card tasks
    //----------------------------------------

    // attempt to mount sd card if not already mounted
    if (!sdcard_mounted) {
      f_unmount ("");
      sd_card_t *sd_card_p = sd_get_by_num(0);
      sd_card_p->state.m_Status |= STA_NOINIT | STA_NODISK;
      fr = f_mount (&fs, "", 1);
      if (fr == FR_OK) {
        printf ("SD card mounted.\n");
        sdcard_mounted = true;
      }
    }


    //----------------------------------------
    // audio tasks
    //----------------------------------------

    struct audio_buffer *buffer = take_audio_buffer(ap, false);
    if (buffer) {
      // default to silence
      memset (buffer->buffer->bytes, 0, 4*buffer->max_sample_count);

      // replace silence with samples if a file is open
      if (file_opened) {
        UINT length = 4*buffer->max_sample_count;
        fr = f_read (&fil, file_buffer, length, &length);
        if (fr == FR_OK) {
          if (length == 0) {
            f_close (&fil);
            file_opened = false;
          } else {
            int count = length >> 2;
            int16_t *samples = (int16_t *) buffer->buffer->bytes;
            for (int i = 0; i < count; i++) {
              samples[2*i+0] = file_buffer[4*i+0] | (file_buffer[4*i+1] << 8);
              samples[2*i+1] = file_buffer[4*i+2] | (file_buffer[4*i+3] << 8);
            }
          }
        } else {
          f_close (&fil);
          file_opened = false;
          sdcard_mounted = false;
        }
      }
      buffer->sample_count = buffer->max_sample_count;
      give_audio_buffer(ap, buffer);
    }


    //----------------------------------------
    // midi button tasks
    //----------------------------------------

    midi_button_t button;
    if (queue_try_remove (&button_fifo, &button)) {
      do {

        // check if sdcard is mounted
        if (!sdcard_mounted) {
          printf ("error: sdcard not mounted\n");
          break;
        }

        // if a file is already opened, close it
        if (file_opened) {
          f_close (&fil);
          file_opened = false;
        }

        // make file name
        char filename[20];
        sprintf (filename, "effect%02x.raw", button);

        // open file for reading
        fr = f_open (&fil, filename, FA_OPEN_EXISTING | FA_READ);
        if (fr != FR_OK) {
          printf ("error: file not opened: %s, reason: %d\n", filename, fr);
          sdcard_mounted = false;
          file_opened = false;
          break;
        }
        file_opened = true;

        printf ("opened file %s\n", filename);

      } while (0);
    }


    //----------------------------------------
    // 200 Hz tasks
    //----------------------------------------

    if (flag200) {
      flag200 = false;

#ifdef PICO_DEFAULT_LED_PIN
      // blihk led
      if (led_timer == 0) {
        // led on
        gpio_put (LED_PIN, 1);
      } else if (led_timer == 25) {
        // led off
        gpio_put (LED_PIN, 0);
      }
#endif

      // increment led timer counter, 1.0 second period
      if (++led_timer >= 200) {
        led_timer = 0;
      }

#ifdef HAS_STATUS_LEDS
      update_status_leds (midi_connected, sdcard_mounted, file_opened);
#endif 
    }
  }
}


//---------------------------------------------------------------------------------------------
// repeating timer callback
//

bool repeating_timer_callback_200Hz (__attribute__((unused))struct repeating_timer *t)
{
  flag200 = true;
  return true;
}


//---------------------------------------------------------------------------------------------
// tiny usb callbacks
//

// mount
void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t* mount_cb_data)
{
  printf ("MIDI Device Index = %u, MIDI device address = %u, %u IN cables, %u OUT cables\n",
    idx, mount_cb_data->daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);

  if (dev_idx == TUSB_INDEX_INVALID_8) {
    dev_idx = idx;
  } else {
    printf ("A different USB MIDI Device is already connected.\n");
    printf ("Only one device at a time is supported in this program.\n");
    printf ("The new device is disabled.\n");
  }
}

// unmount
void tuh_midi_umount_cb(uint8_t idx)
{
  if (idx == dev_idx) {
    dev_idx = TUSB_INDEX_INVALID_8;
    printf("MIDI Device Index = %u is unmounted\n", idx);
  } else {
    printf("Unused MIDI Device Index  %u is unmounted\n", idx);
  }
}

// receive
void tuh_midi_rx_cb(uint8_t idx, uint32_t num_bytes)
{
  if (dev_idx == idx) {
    if (num_bytes != 0) {
      uint8_t cable_num;
      uint8_t buffer[48];
      while (1) {
        uint32_t bytes_read = tuh_midi_stream_read(dev_idx, &cable_num, buffer, sizeof(buffer));
        if (bytes_read == 0)
          return;
        printf("MIDI RX #%u:", cable_num);
        for (uint32_t jdx = 0; jdx < bytes_read; jdx++) {
          printf("%02x ", buffer[jdx]);
        }
        printf("\n");
        if ((buffer[0] == 0xb1) && (buffer[1] <= 0x0f) && (buffer[2] == 0x7f)) {
          midi_button_t button = buffer[1];
          if (!queue_try_add (&button_fifo, &button)) {
            printf ("dropped button %d\n", button);
          }
        } else if ((buffer[0] == 0x92) && (buffer[2] == 0x7f)) {
          midi_button_t button = (buffer[1] & 0x3) + (0x30 - (buffer[1] & 0xFC));
          if (button < 16) {
            if (!queue_try_add (&button_fifo, &button)) {
              printf ("dropped button %d\n", button);
            }
          }
        }
      }
    }
  }
}

// transmit
void tuh_midi_tx_cb(uint8_t idx, uint32_t num_bytes)
{
    (void)idx;
    (void)num_bytes;
}


//---------------------------------------------------------------------------------------------
// init audio
//

struct audio_buffer_pool *init_audio (void) {

    static audio_format_t audio_format = {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = 48000,
        .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 4
    };

    struct audio_buffer_pool *producer_pool = 
        audio_new_producer_pool(&producer_format, 4, SAMPLES_PER_BUFFER);

    bool __unused ok;
    const struct audio_format *output_format;

    struct audio_i2s_config config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel = 0,
        .pio_sm = 0,
    };

    output_format = audio_i2s_setup (&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect (producer_pool);
    assert (ok);
    audio_i2s_set_enabled (true);

    return producer_pool;
}


//---------------------------------------------------------------------------------------------
// iic-connected status leds
//

#ifdef HAS_STATUS_LEDS

void init_status_leds (void)
{
  uint8_t data[2];

  // configure rp2040 i2c hardware
  i2c_init (I2C_INSTANCE(I2C_NUMBER), 100 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  // set all four tca9536 pins to outputs by writing config reg 0x03 to 0x00
  data[0] = 0x03;
  data[1] = 0x00;
  i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);

  // set all four tca9536 outputs high by writing output port reg 0x01 to 0x0F (all leds off)
  data[0] = 0x01;
  data[1] = 0x0F;
  i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);
}


void update_status_leds (bool midi_device_connected, bool sdcard_mounted, bool file_opened)
{
  uint8_t state;
  uint8_t data[2];

  // consolidate individual inputs into a state variable
  state = (midi_device_connected ? 4 : 0) |
          (sdcard_mounted        ? 2 : 0) |
          (file_opened           ? 1 : 0);

#ifdef STATUS_LED_TRIPLE

  // three status leds, map inputs one-to-one to leds
  // left: midi connected, middle: sd card mounted, right: file playing
  data[0] = 0x01;
  data[1] = 8 | (7 & ~state);
  i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);

#endif

#ifdef STATUS_LED_SINGLE

  // single status led, on if midi_device_connected and sdcard_mounted
  data[0] = 0x01;
  data[1] = 0xe | (((state & 6) == 6) ? 0 : 1);
  i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);

#endif
}

#endif
