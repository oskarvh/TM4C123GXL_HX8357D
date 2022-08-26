# TM4C123GXL_HX8357D
This repo contains driver files for the TivaC launchpad (TM4C123GXL) to be used with the Adafruit 2050 screen (HX8357D driver), with GRLIB integration. 

You'll need a TM4C123GXL launchpad and an Adafruit 2050 TFT screen. Note that only the screen part is implemented, not the touch driver. 

The pin connections are as follows(For the SPI version):

Launcpad -> Adafruit 2050

VBUS -> Vin

GND -> GND

PA2 -> CLK

PA5 -> MISO (no function, does not need to be connected)

PA4 -> MOSI

PA6 -> CS

PA7 -> D/C

PB0 -> RST

PB5 -> Lite


There are several different tests available initiated by defining the following preprocessor defines:

BLACKOUT_SCREEN - blacks out the screen to begin with. This should always be run to begin the screen at black. 

PIXELDRAW_TEST - blacks out the screen pixel by pixel. 

LINEDRAWH_TEST - GRLIB test, drawing horizontal lines. 

LINEDRAWV_TEST - GRLIB test, drawing vertical lines. 

DRAW_RECTANGLE_TEST - Makes a rectangle swoosh around the screen à la DVD screen saver. 

TEXT_TEST - Displays some text on the screen. 

UART_SCREEN_TEST - Displays UART output, baud = 115200, 8 bits, 1 stop bit, no parity. Text works, along with backspace. 


TI-RTOS is POSIX enabled as well. 

Dependencies: TI-RTOS 2.16.0.08 (for Tiva) and XDCTools 3.32.0.06_core. Built on the "Empty" project. 