Environment installation:

Install the RP2040 PICO C SDK development environment.
git clone the github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico repository somewhere convenient. 
edit CMakeLists.txt to point to the cloned directory. 

Building a .elf version for the original RP2040 PICO board with a SWD debug probe:

mkdir build_pico
cd build_pico
cmake ..
make

To download the .elf version to the PICO using a SWD debug probe:

openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program soundboard.elf verify reset exit"

Buidling a .uf2 version for the Adafruit QT PY RP2040:

mkdir build_qtpybff
cd build_qtpybff
cmake .. -DPICO_BOARD=adafruit_qtpy_rp2040
make

To download the .uf2 version:

Press and hold boot while pressing and releasing reset. Copy the .uf2 file to the freshly mounted drive.

The connections to the Adafruit Audio BFF are the same as the connections to the PCM5100 DAC breakout board so the qtpybff build above will work for both the speaker and line out versions of the midi player.
