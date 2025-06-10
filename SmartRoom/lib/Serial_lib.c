#include <stdio.h>

#include <avr/io.h>

#include "Serial_lib.h"

void serial_init()
{
	UCSRB = (1 << RXEN) | (1 << TXEN);
	UCSRC |= (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1);
	UBRRL = (int)((F_CPU / (SERIAL_BAUD_RATE * 16)) - 1);
	
#if SERIAL_INTERRUPT == 1
	UCSRB |= (1<<RXCIE);
#endif
}

void serial_send_char(char c)
{
	UCSRA = 1 << TXC; // clear TXC bit
	UDR = c;
	while ((UCSRA & (1 << TXC)) == 0);
}

void serial_send_string(char *s)
{
	while (*s != '\0')
	{
		serial_send_char(*s);
		s++;
	}
}

#if SERIAL_INTERRUPT == 0 //if we don't use receive_interrupt, we can use receive functions:

char serial_receive_char(void) //receives a char
{
	char c;
	while ((UCSRA & (1 << RXC)) == 0); // wait for a char - WARNING this will wait forever if nothing is received
	c = UDR;
	UDR = c;
	return c;
}

// source: https://stackoverflow.com/a/21591379/9691976
uint32_t serial_receive_string(char *stringArr, uint8_t size) //receives a string and returns the SIZE of the received string
{
	uint8_t i = 0;
	
	if (size == 0)
		return 0;			            // return 0 if no space
	
	while (i < size - 1) {              // check space is available (including additional null char at end)
		uint8_t c = serial_receive_char();
		if (c == '\r' || c == '\n' || c == '\0')
			break;						// break on NULL character
		stringArr[i] = c;               // write into the supplied buffer
		i++;
	}
	
	stringArr[i] = 0;					// ensure string is null terminated
	
	return i + 1;                       // return number of characters written
}
#endif
