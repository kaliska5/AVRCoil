/*
 * uart.c
 *
 *  Created on: 01-11-2014
 *      Author: piotr
 */
#include <avr/io.h>

void Usart_init(unsigned int ubrr)
{
   UBRR1H = (unsigned char)(ubrr>>8);
   UBRR1L = (unsigned char)ubrr;
   UCSR1B = (1<<RXEN)|(1<<TXEN);
   UCSR1A = (0<<U2X0);
   UCSR1C = (1<<USBS)|(3<<UCSZ0);
}

void USART_Transmit (unsigned char data)
{
while (! (UCSR1A & (1 << UDRE)));
UDR1 = data;
}

