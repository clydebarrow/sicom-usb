/*
 * 	Sicom USB device protocol
 */

#define	VERS_MAJOR	1
#define	VERS_MINOR	03

#define	BOOL	bit
#define	TRUE	1
#define	FALSE	0

// USB endpoints

#define	USB_IN_EP	1		// USB IN endpoint
#define	USB_OUT_EP	2		// USB OUT endpoint

// command codes
#define MASTER_MSG     0x80
#define LOG_MASTER     0x80
#define NO_LOG_MASTER  0x7F
#define MAX_ADDRESS    0x20
#define BROADCAST      0xFA
#define CRC_ERR        0x01
#define MAX_MSG_LEN    62
#define MIN_MSG_LEN    5

// serial commands
#define BROADCAST_ADDRESS 0xFA
#define REQUEST_DATA_MSG 0x0A
#define ALL_CALL_MSG 0x31
#define REPLY_DATA_MSG 0xA0



// device flags
#define DEVICE_SEEN 0x01 // device has been seen
#define DEVICE_SENT 0x02 // data from this device has been sent to the host
