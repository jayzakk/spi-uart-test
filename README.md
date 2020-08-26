# spi-uart
 Fast SPI-UART for Arduino/Nano (ATmega328P) and special support for LGT8F MCU


 Very fast and optimized replacement for SPI.h using the USART as SPI.

 This code can keep up a constant SPI clock without interruptions at 1/2 system clock (16MHz MCU = 8MHz SPI, 32MHz MCU = 16MHz SPI) in its transfer(buffer[,buffer],count) functions.

 Special support for LGT8F MCUs, allows remapping RXD/TXD (MISO/MOSI) from pins D0/D1 to D5/D6 for "parallel" usage of serial interface, SPI and USART SPI on different pins.

 Please read SPI_UART.h

# Pins
 Set the pin mode before begin() for LGT8F mcu:
 - useAlternatePins=false: RXD =MISO=D0, TXD =MOSI=D1
 - useAlternatePins=true:  RXD*=MISO=D5, TXD*=MOSI=D6

 The CLOCK always is XCK=SCK=D4
 We do NOT have an SS line, as USART only support master mode.

 You can NOT use the Serial in parallel using SPI_UART, but you may switch between using Serial.begin+Serial.end and SPI_UART.begin/SPI_UART.end
