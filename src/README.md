Environment installation:

Install the RP2040 PICO C SDK development environment.</br>
Clone the github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico repository somewhere convenient. </br>
Edit CMakeLists.txt to point to the cloned directory. </br>

Building a .elf version for the original RP2040 PICO board with a SWD debug probe:

mkdir build_pico</br>
cd build_pico</br>
cmake ..</br>
make</br>

To download the .elf version to the PICO using a SWD debug probe:

openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program soundboard.elf verify reset exit"

Building a .uf2 version for the Adafruit QT PY RP2040:

mkdir build_qtpybff</br>
cd build_qtpybff</br>
cmake .. -DPICO_BOARD=adafruit_qtpy_rp2040</br>
make</br>

An I2C-based status indicator can be added to either build by appending one of the following to the above cmake commands:

-DSTATUS_LED_CONFIG=single</br>
-DSTATUS_LED_CONFIG=triple</br>

To download the .uf2 version:

Press and hold boot while pressing and releasing reset. Copy the .uf2 file to the freshly mounted drive.

The connections to the Adafruit Audio BFF are the same as the connections to the PCM5100 DAC breakout board so the qtpybff build above will work for both the speaker and line out versions of the midi player.

picotool info -a soundboard.uf2 or .elf will list the UART and SDCARD pins for each build. I2S audio pins are default pins in the pico-extras i2s audio library for both builds.
