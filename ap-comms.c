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




//ALL COMMS ARE DONE BY LOADING A BUFFER AND JUST LETTING THESE FUNCTIONS GET ON WITH IT

#include "main.h"					//Global data type definitions (see https://github.com/ibexuk/C_Generic_Header_File )
#define AP_COMMS
#include "ap-comms.h"







//***********************************
//***********************************
//********** PROCESS COMMS **********
//***********************************
//***********************************
void comms_process (void)
{
	BYTE *p_buffer;
	WORD command;


	//----- HAS A PACKET BEEN RECEVEID ? -----
	if (comms_rx_no_of_bytes_to_rx != 0)
		return;


	//---------------------------
	//----- PACKET RECEIVED -----
	//---------------------------

	//Packet format:
	//	CommandH | CommandL | LengthH | LengthL | 0-# Data Bytes | Checksum H | Checksum L

	//comms_rx_byte		The size of the received packet including the checksum

	p_buffer = &comms_rx_buffer[4];						//Set pointer to start of data area

	command = (WORD)comms_rx_buffer[0] << 8;
	command |= (WORD)comms_rx_buffer[1];

	switch (command)
	{
	case CMD_GET_STATUS_REQUEST:
		//------------------------------
		//----- GET STATUS REQUEST -----
		//------------------------------

		//Deal with RX
		// = *p_buffer++;


		break;


	//<<<<< ADD HANDLING OF OTHER PACKETS HERE


	}


	//Enable RX again
	comms_rx_byte = 0;
	comms_rx_no_of_bytes_to_rx = 0xffff;
}




//*************************************
//*************************************
//********** COMMS TX PACKET **********
//*************************************
//*************************************
//Call with:
//	comms_tx_byte			Check this is zero before loading comms_tx_buffer (confirms last tx is complete)
//	comms_tx_buffer[]		The packet data to send with the command in byte 0:1.  The Length will automatically be added to bytes 2:3 by this function
//	packet_length			The number of data bytes excluding the checksum (which is automatically added by the tx function)
void comms_tx_packet (WORD packet_length)
{

	//Packet format:
	//	CommandH | CommandL | LengthH | LengthL | 0-# Data Bytes | Checksum H | Checksum L

	//Check last tx is complete
	if (comms_tx_byte)
		return;

	if (packet_length > COMMS_TX_BUFFER_LENGTH)
		return;

	//----- START TX -----
	comms_tx_no_of_bytes_to_tx = packet_length;		//no of bytes to tx (excluding checksum)

	//Set the length bytes
	comms_tx_buffer[2] = (BYTE)(packet_length >> 8);
	comms_tx_buffer[3] = (BYTE)(packet_length & 0x00ff);

	//comms_rx_no_of_bytes_to_rx = 0xfffe;		//If you want to reset rx
	//comms_rx_1ms_timeout_timer = ;

	comms_tx_byte = 0;

	comms_tx_chksum = (WORD)comms_tx_buffer[0];
	
	//COMMS_TX_REG = (WORD)rs485_tx_rx_buffer[comms_tx_byte++] | 0x0100;				//Send byte with bit 9 high
	COMMS_TX_REG = (WORD)comms_tx_buffer[comms_tx_byte++];
	COMMS_ENABLE_TX_IRQ = 1;
}







//*****************************************
//*****************************************
//********** TX BUFFER EMPTY IRQ **********
//*****************************************
//*****************************************
void __attribute__((__interrupt__,__auto_psv__)) _U2TXInterrupt(void)
{
	COMMS_TX_IRQ_FLAG = 0;				//Reset irq flag


	//if (comms_tx_byte == 0)		//Done in start tx function
	//{
	//	//----- SEND BYTE 0 -----
	//	comms_tx_chksum = (WORD)comms_tx_buffer[0];
	//	COMMS_TX_REG = (WORD)comms_tx_buffer[comms_tx_byte++];
	//}
	if (comms_tx_byte < comms_tx_no_of_bytes_to_tx)
	{
		//----- SEND NEXT DATA BYTE -----
		comms_tx_chksum += (WORD)comms_tx_buffer[comms_tx_byte];			//Add to checksum

		COMMS_TX_REG = (WORD)comms_tx_buffer[comms_tx_byte++];
	}
	else if (comms_tx_byte == comms_tx_no_of_bytes_to_tx)
	{
		//----- SEND CHECKSUM HIGH -----
		COMMS_TX_REG = (WORD)(comms_tx_chksum >> 8);
		comms_tx_byte++;
	}
	else if (comms_tx_byte == (comms_tx_no_of_bytes_to_tx + 1))
	{
		//----- SEND CHECKSUM LOW -----
		COMMS_TX_REG = (WORD)(comms_tx_chksum & 0x00ff);
		comms_tx_byte++;
	}
	else
	{
		//----- ALL SENT (TX SHIFT REGISTER EMPTY) -----
		COMMS_ENABLE_TX_IRQ = 0;						//disable tx irq

		comms_tx_byte = 0;					//Reset tx byte counter (indicates we're done)
	}
}






//****************************************
//****************************************
//********** RX BUFFER FULL IRQ **********
//****************************************
//****************************************
void __attribute__((__interrupt__,__auto_psv__)) _U2RXInterrupt(void)
{
	static BYTE rx_data;
	static WORD rx_checksum;
	static WORD checksum_recevied;
	static WORD w_temp;
	static WORD rx_length_received;

	COMMS_RX_IRQ_FLAG = 0;				//Reset irq flag


	if (comms_rx_1ms_timeout_timer == 0)
		comms_rx_byte = 0;

	if ((COMMS_STATUS_REG_BITS.FERR == 0) && (COMMS_STATUS_REG_BITS.PERR == 0) && (COMMS_STATUS_REG_BITS.OERR == 0))
	{
		//--------------------
		//----- RX IS OK -----
		//--------------------
		comms_rx_1ms_timeout_timer = 50;			//<<<Force new packet reset if no data seen for # mS

		rx_data = COMMS_RX_REG;

		if (comms_rx_byte == 0)
			rx_checksum = 0;

		if (comms_rx_byte < comms_rx_no_of_bytes_to_rx)
		{
			//----- DATA BYTE -----
			if (comms_rx_byte < COMMS_RX_BUFFER_LENGTH)
				comms_rx_buffer[comms_rx_byte] = rx_data;

			rx_checksum += (WORD)rx_data;

			if (comms_rx_byte == 2)			//BYTES 2:3 ARE LENGTH
			{
				rx_length_received = (WORD)rx_data << 8;
			}
			else if (comms_rx_byte == 3)
			{
				rx_length_received |= (WORD)rx_data;
				comms_rx_no_of_bytes_to_rx = rx_length_received;
			}

			comms_rx_byte++;
		}
		else if (comms_rx_byte == comms_rx_no_of_bytes_to_rx)
		{
			//----- CHECKSUM HIGH -----
			checksum_recevied = (WORD)rx_data << 8;

			comms_rx_byte++;
		}
		else if (comms_rx_byte == (comms_rx_no_of_bytes_to_rx + 1))
		{
			//----- CHECKSUM LOW -----
			checksum_recevied |= (WORD)rx_data;

			comms_rx_byte++;

			if (rx_checksum == checksum_recevied)
			{
				//----- CHECKSUM IS GOOD -----
				comms_rx_no_of_bytes_to_rx = 0;				//Flag that we have rx to process
			}
			else
			{
				//CHECKSUM IS BAD
				comms_rx_byte = 0;
				comms_rx_no_of_bytes_to_rx = 0xffff;		//Reset to waiting for rx
			}
		}
	}
	else
	{
		//--------------------
		//----- RX ERROR -----
		//--------------------
		rx_data = COMMS_RX_REG;
		rx_data = COMMS_RX_REG;
		rx_data = COMMS_RX_REG;
		rx_data = COMMS_RX_REG;

		//RS485_STATUS_REG.OERR = 0;		//Can't red write UxSTA due to silicon bug
		w_temp = COMMS_STATUS_REG;
		w_temp &= 0xfffd;
		w_temp |= 0x2000;					//This bit gets cleared by a read in this device so need to set it again
		COMMS_STATUS_REG = w_temp;


		if (comms_rx_no_of_bytes_to_rx != 0)		//Don't reset if there is a received packet waiting to be processed
		{
			comms_rx_byte = 0;
			comms_rx_no_of_bytes_to_rx = 0xffff;		//Reset to waiting for rx
		}
	}
}







//**********************************
//**********************************
//********** RX ERROR IRQ **********
//**********************************
//**********************************
void __attribute__((__interrupt__,__auto_psv__)) _U2ErrInterrupt(void)
{
	static BYTE b_temp;
	static WORD w_temp;

	COMMS_ERROR_IRQ = 0;				//Reset irq flag


	if (COMMS_STATUS_REG_BITS.OERR)
	{
		//-------------------------
		//----- OVERRUN ERROR -----
		//-------------------------
		//Reset overrun bit to clear error and reset rx fifo so further bytes are received
		//RS485_STATUS_REG.OERR = 0;		//Can't red write UxSTA due to silicon bug
		w_temp = COMMS_STATUS_REG;
		w_temp &= 0xfffd;
		w_temp |= 0x2000;					//This bit gets cleared by a read in this device so need to set it again
		COMMS_STATUS_REG = w_temp;

		//N.B. If you run to cursor here you will get repeated overrun erros as this error will occur lots of times until we get a framing error and
		//start receiving properly.  Its made worse because the rx has a 4 deep fifo so it may take a few packets for the framing error to occur.  If
		//you worried about overrun errors occuring test by using a int16 counter and see if it continues incrementing after an initial load of errors.

		if (comms_rx_no_of_bytes_to_rx != 0)		//Don't reset if there is a received packet waiting to be processed
		{
			comms_rx_byte = 0;
			comms_rx_no_of_bytes_to_rx = 0xffff;		//Reset to waiting for rx
		}

	}
	//if (RS485_STATUS_REG.PERR)
	//{
	//	//------------------------
	//	//----- PARITY ERROR -----
	//	//------------------------
	//	//Parity error flag is read only (no need to reset)
	//	//Not relevant
	//}
	if (COMMS_STATUS_REG_BITS.FERR)
	{
		//-------------------------
		//----- FRAMING ERROR -----
		//-------------------------
		//Framing error flag is read only (no need to reset)
		b_temp = COMMS_RX_REG;				//Read rx byte - Do not do if using DMA as it screws it up
		COMMS_RX_IRQ_FLAG = 0;				//Reset receive irq flag - Do not do if using DMA as it screws it up

		if (comms_rx_no_of_bytes_to_rx != 0)		//Don't reset if there is a received packet waiting to be processed
		{
			comms_rx_byte = 0;
			comms_rx_no_of_bytes_to_rx = 0xffff;		//Reset to waiting for rx
		}

	}
}





