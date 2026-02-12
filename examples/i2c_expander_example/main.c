//---------------------------------------------------------------------------------------------
// includes
//

// C
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// pico
#include "pico/stdlib.h"
#include "pico/binary_info.h"

// hardware
#include "hardware/i2c.h"


//---------------------------------------------------------------------------------------------
// defines
//

#define C_I2C_NUMBER  0
#define C_I2C_SDA_PIN 16
#define C_I2C_SCL_PIN 17

// TCA9536 address depends on part suffix, this is for the no suffix version
// these are bits [7:1] of the byte, bit [0] is R/W bit.
#define TCA9536_I2C_ADDRESS 0x41


//---------------------------------------------------------------------------------------------
// typedefs
//


//---------------------------------------------------------------------------------------------
// prototypes -- core 0
//


//---------------------------------------------------------------------------------------------
// globals
//

// UART Configuration
// PICO: 0/0/1/115200
// QTPY: 1/20/5/115200
bi_decl(bi_program_feature_group(0x1112, 0, "UART Configuration"));
bi_decl(bi_ptr_int32(0x1112, 0, UART_NUMBER, PICO_DEFAULT_UART));
bi_decl(bi_ptr_int32(0x1112, 0, UART_TX_PIN, PICO_DEFAULT_UART_TX_PIN));
bi_decl(bi_ptr_int32(0x1112, 0, UART_RX_PIN, PICO_DEFAULT_UART_RX_PIN));
bi_decl(bi_ptr_int32(0x1112, 0, UART_BAUD,   115200));

// Status LED Configuration
bi_decl(bi_program_feature_group(0x1114, 0, "I2C Configuration"));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_NUMBER,    C_I2C_NUMBER));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_SDA_PIN,   C_I2C_SDA_PIN));
bi_decl(bi_ptr_int32(0x1114, 0, I2C_SCL_PIN,   C_I2C_SCL_PIN));


//---------------------------------------------------------------------------------------------
// main
//

int main (void)
{
  uint8_t data[2];

  // initialize uart stdio
  stdio_uart_init_full (UART_INSTANCE(UART_NUMBER), UART_BAUD, UART_TX_PIN, UART_RX_PIN);

  // hello world
  printf ("Hello, world!\n");

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

  while (true) {
    // left led on
    data[0] = 0x01;
    data[1] = 0x0B;
    i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);
    sleep_ms (1000);

    // middle led on
    data[0] = 0x01;
    data[1] = 0x0d;
    i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);
    sleep_ms (1000);

    // right led on
    data[0] = 0x01;
    data[0] = 0x01;
    data[1] = 0x0e;
    i2c_write_blocking (I2C_INSTANCE(I2C_NUMBER), TCA9536_I2C_ADDRESS, data, 2, false);
    sleep_ms (1000);
  }
}
