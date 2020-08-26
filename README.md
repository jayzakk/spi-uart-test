# SPI USART for ATmega328p and clones
 Fast SPI-USART (SPI-UART) for Arduino/Nano (ATmega328p) and special support for LGT8F MCU


 Very fast replacement for SPI.h using the USART as SPI.

 This code can keep up a *constant SPI clock* without interruptions at fastest (half of system) clock (16MHz MCU = 8MHz SPI, 32MHz MCU = 16MHz SPI) in its `transfer(buffer[,buffer],count)` functions. The default SPI libraries are not capable of this, as the USART has a ONE byte buffer, the ATmega328p-SPI has none. With this single byte buffer you get much faster transfers using SPI_UART. Though the code was written with the LGT8F in mind, it now also works on the "slower" (more cpu cycles per instruction) 328p.

 Special support for LGT8F MCUs: Allows remapping RXD/TXD (MISO/MOSI) from pins D0/D1 to D5/D6 for "parallel" usage of serial interface, SPI and USART SPI on different pins. Playing with the big ones ^^ 

 Now, if all those libraries in the wild would ever think of not using a *hardcoded* "SPI.". There are even MCUs with 2 and more hardware SPI out there...

 Please read SPI_UART.h

 Remark: I used `SPI_UART` naming, despite it should be `SPI_USART`, to be able to use it with the nRF24 library.

 
# Pins
 Set the pin mode before begin() for LGT8F mcu:
 - `useAlternatePins=false`: RXD =MISO=D0, TXD =MOSI=D1
 - `useAlternatePins=true `: RXD*=MISO=D5, TXD*=MOSI=D6

 The CLOCK always is XCK=SCK=D4. We do NOT have an SS line, as USART only support master mode.

 You can NOT use the Serial in parallel using SPI_UART, but you may switch between using Serial.begin+Serial.end and SPI_UART.begin/SPI_UART.end

