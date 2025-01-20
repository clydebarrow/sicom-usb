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
#define SET_DEVICE_MSG  0x7E
#define MAX_ADDRESS    0x20
#define BROADCAST      0xFA
#define CRC_ERR        0x01
#define MAX_MSG_LEN    62
#define MIN_MSG_LEN    5

// device structure
#define DEV_ADDRESS 0x00
#define DEV_TYPE 0x01
#define DEV_SERIAL 0x02
#define DEV_LEN 0x08

typedef struct {
    unsigned char address;
    unsigned char type;
    unsigned char serial[4];
} device_t;

// serial commands
#define BROADCAST_ADDRESS 0xFA
#define REQUEST_DATA_MSG 0x0A
#define REPLY_DATA_MSG 0xA0
#define ALL_CALL_MSG 0x31
#define ENTROL_MSG 0x20
#define DEVICE_HELLO 0xC0
#define DEVICE_ACK 0xC1
