/*
    This file is free software; you can redistribute it and/or modify
   it under the terms of either the GNU General Public License version 2
   or the GNU Lesser General Public License version 2.1, both as
   published by the Free Software Foundation.

   2020/08 jayzakk@gmail.com
*/

#ifndef _SPIUART_H_INCLUDED
#define _SPIUART_H_INCLUDED

/*

  MODE1 and MODE3 (CPAH=1) don't seem to work reliable at >1MHz SPI

*/


/*
    We want be be as in-place-replacement as possible.
    Because of this, we would need SPISettings in SPI.h to include SPIUARTClass as a friend class, so we can
    access the two private members spcr and spsr:

    | uint8_t spcr;
    | uint8_t spsr;
    | friend class SPIClass;
    | friend class SPIUARTClass;

    If you can change SPI.h, please remove the USART_SETTINGS_WITHOUT_FRIEND define!

    If this is not possible, we will access the private SPISettings members anonymously using pointer with 3 assumptions:
    - these are the 2 first variables in the class,
    - they are uint8_t (1 byte),
    - and exactly in this order.

*/

#define USART_SETTINGS_WITHOUT_FRIEND

#include <SPI.h>

// set the pin mode before begin() for LGT8F mcu:
// - useAlternatePins=false: RXD =MISO=D0, TXD =MOSI=D1
// - useAlternatePins=true:  RXD*=MISO=D5, TXD*=MOSI=D6
// The CLOCK always is XCK=SCK=D4
// We do NOT have an SS line, as USART only support master mode.
// You can NOT use the Serial in parallel using SPI_UART, but you may switch between using
// Serial.begin+Serial.end and SPI_UART.begin/SPI_UART.end

class SPIUARTClass {
  public:
#ifdef __LGT8F__
    static boolean useAlternatePins;
#endif

    // Initialize the SPI library
    static void begin();

    inline static void beginTransaction(SPISettings settings) {
      interruptSave = SREG;
      noInterrupts();

      uint8_t spcr;
      uint8_t spsr;

#ifdef USART_SETTINGS_WITHOUT_FRIEND
      uint8_t*p = (uint8_t*)&settings;
      spcr = *p++;
      spsr = *p;
#else
      spcr = settings.spcr;
      spsr = settings.spsr;
#endif

      // As I did not want to change SPISettings class, we have to convert the SPI registers to USART-SPI registers

      // SPR1,SPR0,SPR2X log scaler to lin scaler
      UBRR0 = clockDivTranslate[((spcr & SPI_CLOCK_MASK) << 1) | spsr & SPI_2XCLOCK_MASK];

      // remap control bits to USART:
      // SPCR  : Bit3 CPOL, Bit2: CPHA, Bit5: DORD
      // UCSR0C: Bit0 CPOL, Bit1: CPHA, Bit2: DORD
      UCSR0C = _BV(UMSEL01) | _BV(UMSEL00) | ((spcr & (_BV(DORD) | _BV(CPOL))) >> 3) | ((spcr & _BV(CPHA)) >> 1);
    }

    // Write to the USART SPI bus (MOSI pin) and also receive (MISO pin)
    inline static uint8_t transfer(uint8_t data) {
      uint8_t rcvd;
      //while ( !(UCSR0A & _BV(UDRE0)));
      UDR0 = data;
      while ( !(UCSR0A & _BV(RXC0) ));
      rcvd = UDR0;
      return rcvd;
    }

    inline static uint16_t transfer16(uint16_t data) {
      union {
        uint16_t val;
        struct {
          uint8_t lsb;
          uint8_t msb;
        };
      } in;

      in.val = data;
      if (!( UCSR0C & _BV(UDORD0))) {
        transfer(in.msb);
        transfer(in.lsb);
      } else {
        transfer(in.lsb);
        transfer(in.msb);
      }
      return in.val;
    }

    /*
       This here has a damn tight timing (especially at 1/2 sysclk SPI), and takes the 1 byte buffer into consideration.
       we'll push in data twice (one in serializer, one in buffer), and must be fast enough to catch the already
       available buffered result before it will be overwritten (in 7 SPI cycles: 32MHz CPU/16MHz SPI -> in 14 cpu cycles).
       due to this, we can manage a constant 16mbit/s (at LGT8F) or 8mbit/s (at 328p) clock
    */

    inline static void transfer(void *buf, size_t count) {
      if (count == 0) return;

      uint8_t sreg = SREG;
      noInterrupts(); // Protect from interruption

      uint8_t*p = buf;
      uint8_t*pret = buf;

      UDR0 = *p++;
      while (--count > 0) {
        while ( !(UCSR0A & _BV(RXC0) )) {
          if ((UCSR0A & _BV(UDRE0))) {
            UDR0 = *p++;
          }
        }
        *pret++ = UDR0;
      }

      while ( !(UCSR0A & _BV(RXC0) ));
      *pret = UDR0;

      SREG = sreg;
    }


    const static void SPIUARTClass::transfer(void * buf, void * retbuf, size_t count);


    inline static void endTransaction(void) {
      SREG = interruptSave;
    }

    static void end();

    // This function is deprecated.  New applications should use
    // beginTransaction() to configure SPI settings.
    inline static void setBitOrder(uint8_t bitOrder) {
      if (bitOrder == LSBFIRST) {
        UCSR0C |= _BV(UDORD0);
      } else {
        UCSR0C &= ~(_BV(UDORD0));
      }
    }

    // This function is deprecated.  New applications should use
    // beginTransaction() to configure SPI settings.
    inline static void setDataMode(uint8_t dataMode) {
      // SPRC  : Bit3 CPOL, Bit2: CPHA (dataMode)
      // UCSR0C: Bit0 CPOL, Bit1: CPHA
      uint8_t ucsr = UCSR0C & ~(_BV(UCPHA0) | _BV(UCPOL0));
      UCSR0C = ucsr | ((dataMode & _BV(CPHA)) >> 1) | ((dataMode & _BV(CPOL)) >> 3);
    }

    // This function is deprecated.  New applications should use
    // beginTransaction() to configure SPI settings.
    inline static void setClockDivider(uint8_t clockDiv) {
      // here, bit 0 is bit 2... yay... :-/
      UBRR0 = clockDivTranslate[((clockDiv & SPI_CLOCK_MASK) << 1) | ((clockDiv >> 2) & SPI_2XCLOCK_MASK)];
    }

    // These undocumented functions should not be used.
    // we do not use them, too...
    inline static void attachInterrupt() {
      UCSR0A |= _BV(UDRE0);
    }
    inline static void detachInterrupt() {
      UCSR0A &= ~_BV(UDRE0);
    }

  private:
    static uint8_t initialized;
    static uint8_t interruptSave;
    static uint8_t clockDivTranslate[8];
};

extern SPIUARTClass SPI_UART;

#endif
