
######################################################################
##### Open sourced by IBEX, an electronic product design company #####
#####    based in South East England.  http://www.ibexuk.com     #####
######################################################################


This is a general use PIC24 comms UART handler.

The driver is full duplex bus so TX and RX can occur independantly

It automatically deals with variable packet length by including a length value in the packet.  It also automatically adds simple checksumming of the packet.
