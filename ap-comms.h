/*
IBEX UK LTD http://www.ibexuk.com
Electronic Product Design Specialists
RELEASED SOFTWARE

The MIT License (MIT)

Copyright (c) 2013, IBEX UK Ltd, http://ibexuk.com

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//Project Name:		PIC24 Full duplex UART Driver



//This is a general use PIC24 comms UART handler.
//The driver is full duplex bus so TX and RX can occur independantly
//It automatically deals with variable packet length by including a length value in the packet.  It also automatically adds simple checksumming of the packet.


//Packet format:
//	CommandH | CommandL | LengthH | LengthL | 0-# Data Bytes | Checksum H | Checksum L


/*
##### ADD THIS TO YOUR APPLICATION STARTUP #####

//UART SETUP FOR PIC24HJ
	//----- SETUP UART 2 -----
	//Used for: 
	U2BRG =	(INSTRUCTION_CLOCK_FREQUENCY / (16 * (57600 + 1)));		//<<< Set the desired baud rate
	U2MODE = 0b1000000000000000;	//8 bit, no parity, BRGH = low (requried due to silicon bug),
	U2STA = 0b0010010100010000;		//TX IRQ when all data sent, RX IRQ on each byte
	//SILICON BUG - DO NOT USE READ MODIFY WRITE INSTRUCTIONS ON UxSTA
	//SILICON BUG - UTXISEL0 bit (UxSTA<13>) is always read as zero regardless of the value written to it
	//U2MODEbits.UARTEN = 1;		//Enable UART
	//U2STAbits.UTXEN = 1;			//Enable tx (do after setting UARTEN)


	//USART 2 TX IRQ
	_U2TXIP = 3;		//Set irq priority (0-7, 7=highest, 0=disabled)
	_U2TXIF = 0;		//Reset irq
	//_U2TXIE = 0;		//Enable irq

	//USART 2 RX IRQ
	_U2RXIP = 5;		//Set irq priority (0-7, 7=highest, 0=disabled)
	_U2RXIF = 0;		//Reset irq
	_U2RXIE = 1;		//Enable irq

	//USART 2 ERROR IRQ
	_U2EIP = 6;			//Set irq priority (0-7, 7=highest, 0=disabled)
	_U2EIF = 0;			//Reset irq
	_U2EIE = 1;			//Enable irq
*/

/*
#### ADD TO MAIN LOOP ####
	//----- PROCESS COMMS -----
	comms_process();
*/


/*
##### ADD THIS TO YOUR HEARTBEAT INTERRUPT SERVICE FUNCTION #####
	//----- HERE EVERY 1 mSec -----
	if (comms_rx_1ms_timeout_timer)
		comms_rx_1ms_timeout_timer--;

*/



/* TRANSMIT A PACKET EXAMPLE
	if (comms_tx_byte == 0)
	{
		p_buffer = &comms_tx_buffer[0];
		*p_buffer++ = CMD_GET_STATUS_REQUEST >> 8;			//Command
		*p_buffer++ = CMD_GET_STATUS_REQUEST & 0x00ff;
		*p_buffer++ = 0;									//Length
		*p_buffer++ = 0;
		*p_buffer++ = 'H';									//Data
		*p_buffer++ = 'e';
		*p_buffer++ = 'l';
		*p_buffer++ = 'l';
		*p_buffer++ = '0';

		comms_tx_packet(p_buffer - &comms_tx_buffer[0]);
	}
*/


//********************************
//********************************
//********** DO DEFINES **********
//********************************
//********************************
#ifndef AP_COMMS_INIT		//Do only once the first time this file is used
#define	AP_COMMS_INIT


#define	COMMS_TX_BUFFER_LENGTH			200				//<<< Set required length
#define	COMMS_RX_BUFFER_LENGTH			200				//<<< Set required length

#define	COMMS_ENABLE_TX_IRQ				_U2TXIE

#define	COMMS_TX_REG					U2TXREG
#define	COMMS_RX_REG					U2RXREG
#define	COMMS_STATUS_REG_BITS			U2STAbits		//<<< This code has special workaround for not being able to read/write U2STA - search for it and check for your ap
#define	COMMS_STATUS_REG				U2STA
#define	COMMS_TX_IRQ_FLAG				_U2TXIF
#define	COMMS_RX_IRQ_FLAG				_U2RXIF
														//### ALSO SET THE UART NUMBER FOR THE TWO __interrupt__ FUNCTIONS ###



//COMMANDS
#define	CMD_GET_STATUS_REQUEST				0x1001
#define	CMD_GET_STATUS_RESPONSE				0x1002


#endif



//*******************************
//*******************************
//********** FUNCTIONS **********
//*******************************
//*******************************
#ifdef AP_COMMS
//-----------------------------------
//----- INTERNAL ONLY FUNCTIONS -----
//-----------------------------------


//-----------------------------------------
//----- INTERNAL & EXTERNAL FUNCTIONS -----
//-----------------------------------------
//(Also defined below as extern)
void comms_process (void);
void comms_tx_packet (WORD packet_length);


#else
//------------------------------
//----- EXTERNAL FUNCTIONS -----
//------------------------------
extern void comms_process (void);
void comms_tx_packet (WORD packet_length);


#endif




//****************************
//****************************
//********** MEMORY **********
//****************************
//****************************
#ifdef AP_COMMS
//--------------------------------------------
//----- INTERNAL ONLY MEMORY DEFINITIONS -----
//--------------------------------------------
WORD comms_rx_byte;
WORD comms_rx_no_of_bytes_to_rx = 0xffff;
BYTE comms_rx_process_packet = 0;
WORD comms_tx_chksum;

//--------------------------------------------------
//----- INTERNAL & EXTERNAL MEMORY DEFINITIONS -----
//--------------------------------------------------
//(Also defined below as extern)
WORD comms_tx_byte;
WORD comms_tx_no_of_bytes_to_tx;
BYTE comms_tx_buffer[COMMS_TX_BUFFER_LENGTH];
BYTE comms_rx_buffer[COMMS_RX_BUFFER_LENGTH];
volatile WORD comms_rx_1ms_timeout_timer = 0;



#else
//---------------------------------------
//----- EXTERNAL MEMORY DEFINITIONS -----
//---------------------------------------
extern WORD comms_tx_byte;
extern WORD comms_tx_no_of_bytes_to_tx;
extern BYTE comms_tx_buffer[COMMS_TX_BUFFER_LENGTH];
extern BYTE comms_rx_buffer[COMMS_RX_BUFFER_LENGTH];
extern volatile WORD comms_rx_1ms_timeout_timer;


#endif






