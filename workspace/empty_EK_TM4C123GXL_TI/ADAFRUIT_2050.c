/*
 * ADAFRUIT_2050.c
 *
 *  Created on: 18 juli 2022
 *      Author: Oskar von Heideken
 */
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <xdc/std.h>
#include "ADAFRUIT_2050.h"
#include "board.h"
#include <ti/drivers/GPIO.h>

void sendLcdCommand(SPI_Handle spiHandle, char command, char* pData, uint32_t numData, uint32_t delayUs){
    SPI_Transaction transaction;
    transaction.count = 1; // 1 byte at a time for
    volatile char cmdBuf = command;
    transaction.rxBuf = (void *) NULL;
    transaction.txBuf = (void *) &cmdBuf;
    // If the command isn't 0xFF, send a command
    if(command != HX8357_NO_COMMAND){
        // Drive the D/C low for command.
        GPIO_write(GPIO_DC_PIN, 0);
        // Drive manual CS low:
        GPIO_write(GPIO_CS_PIN, 0);
        // Send command
        SPI_transfer(spiHandle, &transaction);
        // Drive the D/C high for end of command.
        GPIO_write(GPIO_DC_PIN, 1);
    }
    // Send data if any
    if(pData != NULL){
        // The data is maximum 1024 frames, hence if numData is larger that 1024, then we need to
        // break up the transfer. This is due to the DMA.
        if(numData < 1024){
            transaction.txBuf = (void *) pData;
            transaction.count = numData;
            if(!SPI_transfer(spiHandle, &transaction)){
                // TODO: catch error and handle it instead of looping forever.
                while(1);
            }
        }
        else {
            uint32_t dataLeft = numData;
            uint32_t dataWritten = 0;
            transaction.txBuf = (void *) pData;
            transaction.count = 1024;
            while(dataLeft > 0){
                if(!SPI_transfer(spiHandle, &transaction)){
                    // TODO: catch error and handle it instead of looping forever.
                    while(1);
                }
                dataWritten += transaction.count;
                dataLeft -= transaction.count;
                transaction.txBuf = (void *) &pData[dataWritten];
                if(dataLeft > 1024){
                    transaction.count = 1024;
                }
                else {
                    transaction.count = dataLeft;
                }
            }
        }
    }
    // Drive manual CS high:
    GPIO_write(GPIO_CS_PIN, 1);
    // Delay if needed.
    if(delayUs > 0){
        usleep(delayUs);
    }
}

// Same function as above but without touching the CS
void sendLcdCommandNoCS(SPI_Handle spiHandle, char command, char* pData, uint32_t numData, uint32_t delayUs){
    SPI_Transaction transaction;
    transaction.count = 1; // 1 byte at a time for
    transaction.rxBuf = (void *) NULL;
    transaction.txBuf = (void *) &command;
    // If the command isn't 0xFF, send a command
    if(command != HX8357_NO_COMMAND){
        // Drive the D/C low for command.
        GPIO_write(GPIO_DC_PIN, 0);
        // Send command
        SPI_transfer(spiHandle, &transaction);
        // Drive the D/C high for end of command.
        GPIO_write(GPIO_DC_PIN, 1);
    }
    // Send data if any
    if(pData != NULL){
        // The data is maximum 1024 frames, hence if numData is larger that 1024, then we need to
        // break up the transfer. This is due to the DMA.
        if(numData < 1024){
            transaction.txBuf = (void *) pData;
            transaction.count = numData;
            if(!SPI_transfer(spiHandle, &transaction)){
                // TODO: catch error and handle it instead of looping forever.
                while(1);
            }
        }
        else {
            uint32_t dataLeft = numData;
            uint32_t dataWritten = 0;
            transaction.txBuf = (void *) pData;
            transaction.count = 1024;
            while(dataLeft > 0){
                if(!SPI_transfer(spiHandle, &transaction)){
                    // TODO: catch error and handle it instead of looping forever.
                    while(1);
                }
                dataWritten += transaction.count;
                dataLeft -= transaction.count;
                transaction.txBuf = (void *) &pData[dataWritten];
                if(dataLeft > 1024){
                    transaction.count = 1024;
                }
                else {
                    transaction.count = dataLeft;
                }
            }
        }
    }
    // Delay if needed.
    if(delayUs > 0){
        usleep(delayUs);
    }
}


// Function to set the address window. Note that sendLcdCommand cannot be used, as it toggles the DC & CS pins
// CS must be set outside of this function, while DC is set inside this function.
void setAddressWindow(SPI_Handle spiHandle, uint16_t y, uint16_t x, uint16_t height, uint32_t width){
    char colAddr[4];
    colAddr[0] = (x & 0xFF00)>>8;
    colAddr[1] = (x & 0xFF);
    colAddr[2] = ((x + width - 1) & 0xFF00)>>8;
    colAddr[3] = ((x + width - 1) & 0x00FF);
    char rowAddr[4];
    rowAddr[0] = (y & 0xFF00)>>8;
    rowAddr[1] = (y & 0xFF);
    rowAddr[2] = ((y + height - 1) & 0xFF00)>>8;
    rowAddr[3] = ((y + height - 1) & 0x00FF);
    // Set the columns
    sendLcdCommand(spiHandle, HX8357_CASET, (char*)&colAddr, 4, 0);
    // Set rows
    sendLcdCommand(spiHandle, HX8357_PASET, (char*)&rowAddr, 4, 0);
    // Sent RAMWR:
    //sendLcdCommand(spiHandle, HX8357_RAMWR, NULL, 0, 0);
}

// Initialize display function.
// The init function needs to be called after SPI is initialized
void HX8357_init(SPI_Handle masterSpi){
    char pCmdBuf[35] = {0};
   // send soft reset, then wait 10 ms
   sendLcdCommand(masterSpi, HX8357_SWRESET, NULL, 0, 10000);

   // Send SETEXTC followed by data given by datasheet, then wait 300 ms
   pCmdBuf[0] = 0xFF;
   pCmdBuf[1] = 0x83;
   pCmdBuf[2] = 0x57;
   sendLcdCommand(masterSpi, HX8357D_SETC, pCmdBuf, 3, 300000);

   // Send SETRGB command, followed by 4 parameters
   pCmdBuf[0] = 0x80; // Enable SDO, EPF (colormapping) = 00, MPU interface for RAM access, internal oscillator
   pCmdBuf[1] = 0x00; // rising edge of DOTCLK, active low HSYNC, active low VSYNC, active high enable pin
   pCmdBuf[2] = 0x06; // Horizontal blanking period default
   pCmdBuf[3] = 0x06; // Vertical blanking period default
   sendLcdCommand(masterSpi, HX8357_SETRGB, pCmdBuf, 4, 0);

   // Send SETCOM command, set to -1.52V.
   // NOTE: This doesn't really make any sense. Datasheet and adafruits docs doesn't correspond here.
   pCmdBuf[0] = 0x25; // Should be 0x2C according to datasheet, but 0.25 according to adafruit.
   sendLcdCommand(masterSpi, HX8357D_SETCOM, pCmdBuf, 1, 0);

   // Send SETOSC command, setting to 75 Hz, idle 60Hz:
   pCmdBuf[0] = 0x68; // This is again settings that differ between adafruit comments and datasheet. According to adafruit it's 70/55 Hz
   sendLcdCommand(masterSpi, HX8357_SETOSC, pCmdBuf, 1, 0);

   // Send SETPANEL command, BGR with gate direction swapped
   pCmdBuf[0] = 0x05; //0x05; //  If we want normally black panel, set bit 1 = 1, see page 224 in datasheet.
   sendLcdCommand(masterSpi, HX8357_SETPANEL, pCmdBuf, 1, 0);

   // Send SETPWR1 command
   pCmdBuf[0] = 0x00; // Not deep standby
   pCmdBuf[1] = 0x15; // BT
   pCmdBuf[2] = 0x1C; // VSPR
   pCmdBuf[3] = 0x1C; // VSNR
   pCmdBuf[4] = 0x83; // AP
   pCmdBuf[5] = 0xAA; // FS
   sendLcdCommand(masterSpi, HX8357_SETPWR1, pCmdBuf, 6, 0);

   // Send SETSTBA command
   pCmdBuf[0] = 0x50; // OPON normal
   pCmdBuf[1] = 0x50; // OPON idle
   pCmdBuf[2] = 0x01; // STBA
   pCmdBuf[3] = 0x3C; // STBA
   pCmdBuf[4] = 0x1E; // STBA
   pCmdBuf[5] = 0x08; // GEN
   sendLcdCommand(masterSpi, HX8357D_SETSTBA, pCmdBuf, 6, 0);

   // Send SETCYC command
   pCmdBuf[0] = 0x02; // NW 0x02
   pCmdBuf[1] = 0x40; // RTN
   pCmdBuf[2] = 0x00; // DIV
   pCmdBuf[3] = 0x2A; // DUM
   pCmdBuf[4] = 0x2A; // DUM
   pCmdBuf[5] = 0x0D; // GDON
   pCmdBuf[6] = 0x78; // GDOFF
   sendLcdCommand(masterSpi, HX8357D_SETCYC, pCmdBuf, 7, 0);

   // Send SETGAMMA command
   pCmdBuf[0] = 0x02;
   pCmdBuf[1] = 0x0A;
   pCmdBuf[2] = 0x11;
   pCmdBuf[3] = 0x1d;
   pCmdBuf[4] = 0x23;
   pCmdBuf[5] = 0x35;
   pCmdBuf[6] = 0x41;
   pCmdBuf[7] = 0x4b;
   pCmdBuf[8] = 0x4b;
   pCmdBuf[9] = 0x42;
   pCmdBuf[10] = 0x3A;
   pCmdBuf[11] = 0x27;
   pCmdBuf[12] = 0x1B;
   pCmdBuf[13] = 0x08;
   pCmdBuf[14] = 0x09;
   pCmdBuf[15] = 0x03;
   pCmdBuf[16] = 0x02;
   pCmdBuf[17] = 0x0A;
   pCmdBuf[18] = 0x11;
   pCmdBuf[19] = 0x1d;
   pCmdBuf[20] = 0x23;
   pCmdBuf[21] = 0x35;
   pCmdBuf[22] = 0x41;
   pCmdBuf[23] = 0x4b;
   pCmdBuf[24] = 0x4b;
   pCmdBuf[25] = 0x42;
   pCmdBuf[26] = 0x3A;
   pCmdBuf[27] = 0x27;
   pCmdBuf[28] = 0x1B;
   pCmdBuf[29] = 0x08;
   pCmdBuf[30] = 0x09;
   pCmdBuf[31] = 0x03;
   pCmdBuf[32] = 0x00;
   pCmdBuf[33] = 0x01;
   sendLcdCommand(masterSpi, HX8357D_SETGAMMA, pCmdBuf, 34, 0);

   // send COLMOD command:
   pCmdBuf[0] = 0x55; // 16 bit per pixel
   sendLcdCommand(masterSpi, HX8357_COLMOD, pCmdBuf, 1, 0);

   // Send MADCTL command, see page 61 and 157
   uint8_t MV, MX, MY;
   MX = 0;
   MY = 1;
   MV = 1;
   pCmdBuf[0] = MY<<7 | MX <<6 | MV << 5;//0xC0; // [MV, MX, MY] =
   sendLcdCommand(masterSpi, HX8357_MADCTL, pCmdBuf, 1, 0);

   // Send TEON command
   pCmdBuf[0] = 0x00; //TW off
   sendLcdCommand(masterSpi, HX8357_TEON, pCmdBuf, 1, 0);

   // Send TEARLINE command:
   pCmdBuf[0] = 0x00;
   pCmdBuf[1] = 0x02;
   sendLcdCommand(masterSpi, HX8357_TEARLINE, pCmdBuf, 2, 0);

   // Send SPLOUT command, wait for 150 ms
   sendLcdCommand(masterSpi, HX8357_SLPOUT, NULL, 0, 150000);

   // Send DISPON, turning on screen. Then delay 50 ms.
   sendLcdCommand(masterSpi, HX8357_DISPON, NULL, 0, 50000);
}

// GRLIB functions
// These functions will be linked to the tDisplay struct
// as a translation layer/API to the display itself.
// What the GRLIB needs are: write one pixel, write multiple pixels,
// fill rectangle, draw horisonal and vertical lines, flush (=abort any currently ongoing operations)
// and a way to translate from 24-bit RBG colorspace to whatever format is used by the display.
// The API prototypes are found in https://www.ti.com/lit/an/spma055/spma055.pdf


// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// lX is the X coordinate of the pixel.
// lY is the Y coordinate of the pixel.
// ulValue is the color of the pixel (already translated)
// Description:
// This function sets the given pixel to a particular color. The coordinates of the pixel are assumed
// to be within the extents of the display.
// Returns:
// None.
void PixelDraw(void *pvDisplayData, int32_t i32X, int32_t i32Y,
uint32_t ui32ulValue){
    tDisplayData *pDisplayData = (tDisplayData *)pvDisplayData;
    SPI_Handle spiHandle = pDisplayData->spiHandle;
    // Set the address window to 1 pixel
    setAddressWindow(spiHandle, i32Y, i32X, 1, 1);
    // As the 32 bit value is another bit format than what is accepted by the
    // screen, we need to rotate the bits. Done here since it's where the
    // issue is, otherwise we could rotate it in the colorTranslate function,
    // but that's a bit of a hack...
    char buf[2];
    buf[0] = ui32ulValue>>8;
    buf[1] = (ui32ulValue&0xFF);
    // Send the color to that address range. All colors are 2 bytes.
    sendLcdCommand(spiHandle, HX8357_RAMWR, buf/*(char*)&ui32ulValue*/, 2, 0);
}

// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// lX is the X coordinate of the first pixel.
// lY is the Y coordinate of the first pixel.
// lX0 is sub-pixel offset within the pixel data, which is valid for 1 or 4 bit per pixel formats.
// lCount is the number of pixels to draw.
// lBPP is the number of bits per pixel; must be 1, 4, or 8.
// pucData is a pointer to the pixel data. For 1 and 4 bit per pixel formats, the most significant
// bit(s) represent the left-most pixel.
// pucPalette is a pointer to the palette used to draw the pixels.
// Description:
// This function draws a horizontal sequence of pixels on the screen, using the supplied palette.
// For 1 bit per pixel format, the palette contains pre-translated colors; for 4 and 8 bit per pixel
// formats, the palette contains 24-bit RGB values that must be translated before being written to
// the display.
// Returns:
// None.
void PixelDrawMultiple(void *pvDisplayData, int32_t i32X, int32_t i32Y,
int32_t i32X0, int32_t i32Count, int32_t i32BPP,
const uint8_t *pui8Data,
const uint8_t *pui8Palette){

}

// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// lX1 is the X coordinate of the start of the line.
// lX2 is the X coordinate of the end of the line.
// lY is the Y coordinate of the line.
// ulValue is the color of the line. (already translated)
// Description:
// This function draws a horizontal line on the display. The coordinates of the line are assumed
// to be within the extents of the display.
// Returns:
// None.
void LineDrawH(void *pvDisplayData, int32_t i32X1, int32_t i32X2,
int32_t i32Y, uint32_t ui32ulValue){
    tDisplayData *pDisplayData = (tDisplayData *)pvDisplayData;
    SPI_Handle spiHandle = pDisplayData->spiHandle;
    // Set the address window to 1 pixel
    setAddressWindow(spiHandle, i32Y, i32X1, 1, i32X2-i32X1);
    // Send the color to that address range. All colors are 2 bytes.
    int i;
    GPIO_write(GPIO_CS_PIN, 0);

    // As the 32 bit value is another bit format than what is accepted by the
    // screen, we need to rotate the bits. Done here since it's where the
    // issue is, otherwise we could rotate it in the colorTranslate function,
    // but that's a bit of a hack...
    char buf[2];
    buf[0] = ui32ulValue>>8;
    buf[1] = (ui32ulValue&0xFF);

    // Send the command:
    sendLcdCommandNoCS(spiHandle, HX8357_RAMWR, NULL, 0, 0);

    for(i = i32X1 ; i <= i32X2 ; i++){
        sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, buf/*(char*)&ui32ulValue*/, 2, 0);
    }
    GPIO_write(GPIO_CS_PIN, 1);
}
// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// lX is the X coordinate of the line.
// lY1 is the Y coordinate of the start of the line.
// lY2 is the Y coordinate of the end of the line.
// ulValue is the color of the line. (already translated)
// Description:
// This function draws a vertical line on the display. The coordinates of the line are assumed to
// be within the extents of the display.
// Returns:
// None
void LineDrawV(void *pvDisplayData, int32_t i32X, int32_t i32Y1,
int32_t i32Y2, uint32_t ui32ulValue){
    tDisplayData *pDisplayData = (tDisplayData *)pvDisplayData;
    SPI_Handle spiHandle = pDisplayData->spiHandle;
    // Set the address window to 1 pixel
    setAddressWindow(spiHandle, i32Y1, i32X, i32Y2-i32Y1, 1);
    // Send the color to that address range. All colors are 2 bytes.
    int i;
    GPIO_write(GPIO_CS_PIN, 0);
    // As the 32 bit value is another bit format than what is accepted by the
    // screen, we need to rotate the bits. Done here since it's where the
    // issue is, otherwise we could rotate it in the colorTranslate function,
    // but that's a bit of a hack...
    char buf[2];
    buf[0] = ui32ulValue>>8;
    buf[1] = (ui32ulValue&0xFF);

    // Send the command:
    sendLcdCommandNoCS(spiHandle, HX8357_RAMWR, NULL, 0, 0);
    for(i = i32Y1 ; i <= i32Y2 ; i++){
        sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, buf/*(char*)&ui32ulValue*/, 2, 0);
    }
    GPIO_write(GPIO_CS_PIN, 1);
}

// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// pRect is a pointer to the structure describing the rectangle.
// ulValue is the color of the rectangle.
// Description:
// This function fills a rectangle on the display. The coordinates of the rectangle are assumed to
// be within the extents of the display, and the rectangle specification is fully inclusive (in other
// words, both sXMin and sXMax are drawn, along with sYMin and sYMax).
// Returns:
// None.

// DEBUG VARIABLES:
uint32_t numPixels = 0;
uint32_t numWritten = 0;
void RectFill(void *pvDisplayData, const tRectangle *psRect,
uint32_t ui32ulValue){
    // Get the SPI handle
    tDisplayData *pDisplayData = (tDisplayData *)pvDisplayData;
    SPI_Handle spiHandle = pDisplayData->spiHandle;

    // Try to allocate a temporary screen buffer, because it's a lot faster than writing
    // on a per-pixel basis.
    char *pScreenBuf;

    // Try to allocate all rows, which is num(x_pixels)*num(y_pixels)
    int n = (psRect->i16YMax) - (psRect->i16YMin);
    int numPixelsInARow = (psRect->i16XMax) - (psRect->i16XMin)+1;

    // One pixel is 16 bits, so allocate the number of pixels in one row * number of rows * 2
    pScreenBuf = (char*)malloc(numPixelsInARow*n*2);

    // If this does not work, then reduce the number of rows
    while((pScreenBuf == NULL) && (n > 1)){
        n = n>>1; // Divide by two to quickly go through this loop
        pScreenBuf = (char*)malloc(numPixelsInARow*n*2);
    }

    // Check if pScreenBuf wasn't allocated, in which case just print 1 pixel at a time
    // This happens if malloc couldn't allocate even one row.
    if(pScreenBuf == NULL){
        n = 0;
    }
    else{
        // If buffer was allocated, then copy the color to all entries.
        // One pixel is 16 bits, so the size is num(bits)*2
        // As the 32 bit value is another bit format than what is accepted by the
        // screen, we need to rotate the bits. Done here since it's where the
        // issue is, otherwise we could rotate it in the colorTranslate function,
        // but that's a bit of a hack...

        memset(pScreenBuf, (uint16_t)((ui32ulValue >> 8) | ((ui32ulValue&0xFF) << 8)), numPixelsInARow*n*2);
    }

    // Set the address window to match the rectangle.
    setAddressWindow(spiHandle, psRect->i16YMin,
                     psRect->i16XMin,
                     psRect->i16YMax-psRect->i16YMin
                     , psRect->i16XMax-psRect->i16XMin);
    numPixels = (psRect->i16YMax-psRect->i16YMin)*(psRect->i16XMax-psRect->i16XMin);
    // Send the color to that address range. All colors are 2 bytes.
    int x,y;

    // Set the CS pin low, as we want to loop several commands in the same CS period.
    GPIO_write(GPIO_CS_PIN, 0);

    // Send the command to write to screen buffer.
    sendLcdCommandNoCS(spiHandle, HX8357_RAMWR, NULL, 0, 0);


    if(n > 0){
        // Loop through the rows and write the entire buffer at once,
        // looping through the buffer several times if needed.
        y = psRect->i16YMin;
        while(y < psRect->i16YMax){
            sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, pScreenBuf, 2*n*numPixelsInARow, 0);
            y += n;
            numWritten += n*numPixelsInARow;
        }
        // If the exact number of rows haven't been written, then that means that
        // y%n != 0, which means that we need to write a shorter burst.
        if(y != psRect->i16YMax){
            y -= n;
            // Write the remainder of the pixels
            sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, pScreenBuf, 2*numPixelsInARow*(psRect->i16YMax-y), 0);
        }
        //y -= n;
        /*
        for(y = psRect->i16YMin ; y <= psRect->i16YMax-n ; y += n){
            sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, pScreenBuf, 2*n*numPixelsInARow, 0);
        }
        */
        // Check if there are rows left not sent due to y_size % n != 0:
        /*
        if(y < psRect->i16YMax){
            // Write the remainder of the pixels
            sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, pScreenBuf, 2*(n*numPixelsInARow-(psRect->i16YMax-y)), 0);
            numWritten += (n*numPixelsInARow-(psRect->i16YMax-y));
        }
        */
    }
    else{
        // Send the command for every pixel
        for(x = psRect->i16XMin ; x <= psRect->i16XMax ; x++){
            for(y = psRect->i16YMin ; y <= psRect->i16YMax ; y++){
                sendLcdCommandNoCS(spiHandle, HX8357_NO_COMMAND, (char*)&ui32ulValue, 2, 0);
            }
        }
    }
    GPIO_write(GPIO_CS_PIN, 1);
    // Finally, free the allocated memory
    free(pScreenBuf);
}

// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// ulValue is the 24-bit RGB color. The least-significant byte is the blue channel, the next byte is
// the green channel, and the third byte is the red channel.
// Description:
// This function translates a 24-bit RGB color into a value that can be written into the display’s
// frame buffer in order to reproduce that color, or the closest possible approximation of that color.
// Returns:
// Returns the display-driver specific color.
uint32_t ColorTranslate(void *pvDisplayData, uint32_t ui32ulValue){
    // The input format is: bits [23..16] = R, [15..8] = G, [7..0] = B
    // And the output format is RGB565, meaning [15..11] = R, [10..5] = G, [4..0] = B

    // There are several ways to go about this, one is to simply only take the most
    // significant bits, but with the risk of losing fidelity.
    // A better way would involve some dithering, but given that the
    // application here aims to show text, not images, the contrast would be high
    // to begin with. So shifting it is.

    // This gives:
    // Blue = output[4..0] = (input>>3) & 0x1F
    // Green = output[10..5] = (input>>5) & 0x7E0
    // Red = output[15..11] = (input>>7) & 0xF800
    // RGB order below.
    return ((ui32ulValue>>7) & 0xF800) | ((ui32ulValue>>5) & 0x7E0) | ((ui32ulValue>>3) & 0x1F);
}

// Parameters:
// pvDisplayData is a pointer to the driver-specific data for this display driver.
// Description:
// This function flushes any cached drawing operations to the display. This is useful when a local
// frame buffer is used for drawing operations, and the flush would copy the local frame buffer to
// the display. If there are no cached operations possible for a display driver, this function can be
// empty.
// Returns:
// None.
void Flush(void *pvDisplayData){
    // No such flush operation exist, hence, this is left blank.
}
