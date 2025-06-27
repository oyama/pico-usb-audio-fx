# pico-usb-audio-fx

A super-small **tape-stop** insert effect that lives entirely on a Raspberry Pi Pico or Pico 2.  
The Pico enumerates as a USB Audio device—stereo in/out, 24-bit 48 kHz—and slows incoming audio to a halt, then smoothly spins it back up.  
Watch it in action here: [YouTube demo](https://www.youtube.com/watch?v=-kohc86NgPs).

## Features

* Class-compliant USB Audio (no drivers on macOS, Windows, or Linux)
* Single-button control with the Pico’s on-board BOOTSEL button  
  * press → slow-down / stop  
  * release → ramp back to normal speed
* Zero extra hardware—just a Pico and a USB cable

## Getting Started

To compile and install this project, a setup with the [pico-sdk](https://github.com/raspberrypi/pico-sdk) is necessary. Please refer to [Getting Started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) to prepare your toolchain.

### Build and Install

```bash
git clone https://github.com/oyama/pico-usb-audio-fx.git
cd pico-usb-audio-fx
mkdir build; cd build
PICO_SDK_PATH=/path/to/pico-sdk cmake ..
make
```

After successful compilation, the `pico-usb-audio-fx.uf2` binary will be produced. Simply drag and drop this file onto your Raspberry Pi Pico W while in BOOTSEL mode to install.

Create a simple send-return loop—either with your DAW’s routing plug-in (e.g., Logic Pro: _Utility > I/O_) or a loopback utility. Feed your host audio to _Pico Audio FX TapeStop_ IN, and monitor the effected signal coming back on _Pico Audio FX TapeStop_ OUT.

## License

This project is licensed under the 3-Clause BSD License. For details, see the [LICENSE](LICENSE.md) file.
