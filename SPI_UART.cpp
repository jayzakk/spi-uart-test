/*
   This file is free software; you can redistribute it and/or modify
   it under the terms of either the GNU General Public License version 2
   or the GNU Lesser General Public License version 2.1, both as
   published by the Free Software Foundation.

   2020/08 jayzakk@gmail.com
*/

#include "SPI_UART.h"

SPIUARTClass SPI_UART;

uint8_t SPIUARTClass::initialized = 0;
uint8_t SPIUARTClass::interruptSave;
// SPI divider to USART divider mapping (/2 /4 /8 /16 /32 /64 /64 /128)
// and take SPI2X inverted bit into consideration
uint8_t SPIUARTClass::clockDivTranslate[8] = { 1, 0, 7, 3, 31, 15, 63, 31 };
#ifdef __LGT8F__
static boolean SPIUARTClass::useAlternatePins;
#endif

void SPIUARTClass::begin()
{
  uint8_t sreg = SREG;
  noInterrupts(); // Protect from a scheduler and prevent transactionBegin
  if (!initialized) {
#ifdef __LGT8F__
    if (useAlternatePins) {
      fastioMode(D5, INPUT_PULLUP);
      fastioMode(D6, OUTPUT);
      PMX0 |= 1 << WCE;
      PMX0 |= _BV(TXD6) | _BV(RXD5);
    } else
#endif
    {
      fastioMode(D0, INPUT_PULLUP);
      fastioMode(D1, OUTPUT);
      PMX0 |= 1 << WCE;
      PMX0 &= ~(_BV(TXD6) | _BV(RXD5) );
    }

    fastioMode(D4, OUTPUT); // XCK

    UBRR0 = 0;
    UCSR0C |= _BV(UMSEL01) | _BV(UMSEL00);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);

    // default SPI speed: 4MHz (as far as possible, or f_cpu/2 if not)
#if F_CPU>=16000000
    UBRR0 = ((F_CPU / 4000000 / 2) - 1);
#endif
  }
  initialized++; // reference count
  SREG = sreg;
}

void SPIUARTClass::end() {
  uint8_t sreg = SREG;
  noInterrupts(); // Protect from a scheduler and prevent transactionBegin
  // Decrease the reference counter
  if (initialized)
    initialized--;
  // If there are no more references disable SPI
  if (!initialized) {
    UCSR0B &= ~(_BV(RXEN0) | _BV(TXEN0) );
    UCSR0C &= ~(_BV(UMSEL01) | _BV(UMSEL00) );
#ifdef __LGT8F__
    if (useAlternatePins) {
      PMX0 |= 1 << WCE;
      PMX0 &= ~(_BV(TXD6) | _BV(RXD5) );
    }
#endif
  }
  SREG = sreg;
}

/*
   This here has a damn tight timing (especially at 1/2 sysclk SPI), and takes the 1 byte buffer into consideration.
   we'll push in data twice (one in serializer, one in buffer), and must be fast enough to catch the already
   available buffered result before it will be overwritten (in 7 SPI cycles: 32MHz CPU/16MHz SPI -> in 14 cpu cycles).
   due to this, we can manage a constant 16mbit/s clock
*/
const static void SPIUARTClass::transfer(void * buf, void * retbuf, size_t count) {
  if (count == 0) return;

  uint8_t sreg = SREG;
  noInterrupts(); // Protect from interruption

  size_t writecount = count;
  uint8_t *p = (uint8_t *)buf;
  uint8_t *pret = (uint8_t *)retbuf;

  UDR0 = p ? *p++ : 0;
  writecount--;

  if (buf && !retbuf) {
    // optimized version: we only need to SEND
    while (writecount-- > 0) {
      if (UCSR0A & _BV(RXC0)) {
        uint8_t in = UDR0;
        count--;
      }
      while (!(UCSR0A & _BV(UDRE0)));
      UDR0 = *p++;
    }
    while (count-- > 0) {
      while ( !(UCSR0A & _BV(RXC0) ));
      uint8_t in = UDR0;
    }
  }
  else if (!buf && retbuf) {
    // optimized version: we only need to RECEIVE
    while (writecount-- > 0) {
      if (UCSR0A & _BV(RXC0)) {
        *pret++ = UDR0;
        count--;
      }
      while (!(UCSR0A & _BV(UDRE0)));
      UDR0 = 0;
    }
    while (count-- > 0) {
      while ( !(UCSR0A & _BV(RXC0) ));
      *pret++ = UDR0;
    }
  }
  else  {
    // this last part does write+read at the same time
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
  }

  SREG = sreg;
}
