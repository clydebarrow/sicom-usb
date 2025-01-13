;	Jump table to USB routines - interleave them with the interrupt vectors

	global	_USB_init
	global	_USB_detach
	global	_USB_control
	global	_USB_read_packet
	global	_USB_flushout
	global	_USB_flushin
	global	_USB_send_byte
	global	_USB_halt


_USB_init			equ	0x06
_USB_detach			equ	0x0E
_USB_control		equ	0x16
_USB_read_packet	equ	0x1E
_USB_flushout		equ	0x26
_USB_flushin		equ	0x2E
_USB_send_byte		equ	0x36
_USB_halt			equ	0x3E

	global			_USB_status

_USB_status			equ	0x430
