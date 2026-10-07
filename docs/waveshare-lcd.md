# Waveshare ESP32-C5-Touch-LCD-2.8 backend

This branch is bringing C5VRX directly to the board's 2.8-inch ST7789 rather
than requiring an external composite monitor.

## Milestone 1: display transport

Enable `CONFIG_C5VRX_WAVESHARE_LCD` to compile the board/display backend.

Verified against Waveshare's ESP-IDF 5.5.4 BSP:

- LCD SCLK: GPIO6
- LCD MOSI: GPIO7
- LCD D/C: GPIO9
- LCD CS: GPIO10
- ST7789 SPI clock in Waveshare's BSP: 80 MHz
- LCD reset: CH32V003 expander pin 1
- Backlight: CH32V003 PWM
- Expander I2C: GPIO0 SDA / GPIO1 SCL

The C5VRX backend exposes the portrait glass as 320x240 landscape and provides
RGB565 line/rectangle/fill primitives.

## Important architecture constraint

The existing C5VRX realtime path attaches the ESP32-C5 BitScrambler to PARLIO
TX and loops the 32 KiB raw IQ ring into the resistor DAC. The LCD path cannot
simply create a second simultaneous BitScrambler pipeline.

The next milestone therefore replaces the DAC-output stage in LCD mode with a
digital demodulator path:

    MODEM_DIAG 40 MS/s
      -> PARLIO RX / 32 KiB IQ ring
      -> completed RX DMA descriptors
      -> FM/CVBS demodulation
      -> H/V sync and active-video extraction
      -> 320-pixel grayscale RGB565 scanlines
      -> ST7789 SPI DMA

The first video milestone is intentionally monochrome. NTSC/PAL chroma decoding
comes after stable sync, geometry, frame pacing and RF/display coexistence are
proven.

## Why this is opt-in

The original XIAO resistor-DAC build is the project's known-good reference.
Keeping the LCD backend behind Kconfig lets the new Waveshare target evolve
without changing that path while the digital demodulator is validated.
