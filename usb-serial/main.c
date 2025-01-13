#include	<8051.h>

#include	<stdio.h>
#include	<htusb.h>
#include	"sicom.h"

// flag bits
#define MASTER_MSG  0x80
#define CRC_ERR     0x01
#define MAX_MSG_LEN 63
#define MIN_LEN_B   4
#define MAX_LEN_B   62


// The USB descriptors.

extern USB_CONST USB_descriptor_table	sicom_usb_table;

extern void serialSend(void);

static unsigned char xdata databuf[64];		// buffer for data received on OUT endpoint
static unsigned char xdata rx_buf[MAX_MSG_LEN+1];		// receive data buffer
static unsigned char xdata tx_buf[MAX_MSG_LEN+1];		// transmit data buffer

// buffer pointers

static unsigned char rx_inp, tx_inp, tx_outp;

// request buffer is also in databuf
// choose which pins the activity and power leds are on, and
// their polarity

#define	POWER_LED		(P0_BITS.B6)
#define	ACTIVITY_LED	(P0_BITS.B0)
#define	ACTIVE(x)		ACTIVITY_LED = !(x)
#define	GREEN(x)		POWER_LED = !(x)

                           
// LED states
typedef enum {
    LED_OFF,
    LED_ON,
    LED_FLASH,
}   ledState;


ledState red_led_state, green_led_state;

// Timer 2 reload period

#define	SYSCLK	24000000ul		// system clock freq
#define	T2PRE	12				// timer 2 prescaler
#define	T2RATE	(1000U)		    // timer 2 desired interrupt rate - 1kHz

#define	T2LOAD	(SYSCLK/T2PRE/(T2RATE-1))	// reload value for T2

static unsigned int	    timeout;		// timeout value in ticks
static unsigned char	error_code;		// output this error code on the activity led

#define TX_ENABLE P2_BITS.B1


#define	SET_TIMEOUT(x)	((timeout = (((unsigned long)(x)*T2RATE))/1000))
#define TIMEDOUT()  (timeout == 0)
#define KICK_DOG()      (PCA0CPH4 = 0)          // kick watchdog


#define	FLASH_RATE	3
#define	FLASH_PRELOAD	((T2RATE+1)/FLASH_RATE)


/**
 * Function called at intervals to drive LED state
 * The argument is true or false for high/low states
 */

static void flashLed(unsigned char hl) {
    switch(green_led_state) {

        case LED_ON:
            GREEN(TRUE);
            break;

        case LED_FLASH:
            GREEN(hl);
            break;

        case LED_OFF:
            GREEN(FALSE);
            break;
    }
    switch(red_led_state) {

        case LED_ON:
            ACTIVE(TRUE);
            break;

        case LED_FLASH:
            ACTIVE(hl);
            break;

        case LED_OFF:
            ACTIVE(FALSE);
            break;
    }
}
static USB_CONST unsigned short crc_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5,
    0xe97e, 0xf8f7, 0x0919, 0x1890, 0x2a0b, 0x3b82, 0x4f3d, 0x5eb4, 0x6c2f, 0x7da6, 0x8551, 0x94d8, 0xa643, 0xb7ca,
    0xc375, 0xd2fc, 0xe067, 0xf1ee, 0x1232, 0x03bb, 0x3120, 0x20a9, 0x5416, 0x459f, 0x7704, 0x668d, 0x9e7a, 0x8ff3,
    0xbd68, 0xace1, 0xd85e, 0xc9d7, 0xfb4c, 0xeac5, 0x1b2b, 0x0aa2, 0x3839, 0x29b0, 0x5d0f, 0x4c86, 0x7e1d, 0x6f94,
    0x9763, 0x86ea, 0xb471, 0xa5f8, 0xd147, 0xc0ce, 0xf255, 0xe3dc, 0x2464, 0x35ed, 0x0776, 0x16ff, 0x6240, 0x73c9,
    0x4152, 0x50db, 0xa82c, 0xb9a5, 0x8b3e, 0x9ab7, 0xee08, 0xff81, 0xcd1a, 0xdc93, 0x2d7d, 0x3cf4, 0x0e6f, 0x1fe6,
    0x6b59, 0x7ad0, 0x484b, 0x59c2, 0xa135, 0xb0bc, 0x8227, 0x93ae, 0xe711, 0xf698, 0xc403, 0xd58a, 0x3656, 0x27df,
    0x1544, 0x04cd, 0x7072, 0x61fb, 0x5360, 0x42e9, 0xba1e, 0xab97, 0x990c, 0x8885, 0xfc3a, 0xedb3, 0xdf28, 0xcea1,
    0x3f4f, 0x2ec6, 0x1c5d, 0x0dd4, 0x796b, 0x68e2, 0x5a79, 0x4bf0, 0xb307, 0xa28e, 0x9015, 0x819c, 0xf523, 0xe4aa,
    0xd631, 0xc7b8, 0x48c8, 0x5941, 0x6bda, 0x7a53, 0x0eec, 0x1f65, 0x2dfe, 0x3c77, 0xc480, 0xd509, 0xe792, 0xf61b,
    0x82a4, 0x932d, 0xa1b6, 0xb03f, 0x41d1, 0x5058, 0x62c3, 0x734a, 0x07f5, 0x167c, 0x24e7, 0x356e, 0xcd99, 0xdc10,
    0xee8b, 0xff02, 0x8bbd, 0x9a34, 0xa8af, 0xb926, 0x5afa, 0x4b73, 0x79e8, 0x6861, 0x1cde, 0x0d57, 0x3fcc, 0x2e45,
    0xd6b2, 0xc73b, 0xf5a0, 0xe429, 0x9096, 0x811f, 0xb384, 0xa20d, 0x53e3, 0x426a, 0x70f1, 0x6178, 0x15c7, 0x044e,
    0x36d5, 0x275c, 0xdfab, 0xce22, 0xfcb9, 0xed30, 0x998f, 0x8806, 0xba9d, 0xab14, 0x6cac, 0x7d25, 0x4fbe, 0x5e37,
    0x2a88, 0x3b01, 0x099a, 0x1813, 0xe0e4, 0xf16d, 0xc3f6, 0xd27f, 0xa6c0, 0xb749, 0x85d2, 0x945b, 0x65b5, 0x743c,
    0x46a7, 0x572e, 0x2391, 0x3218, 0x0083, 0x110a, 0xe9fd, 0xf874, 0xcaef, 0xdb66, 0xafd9, 0xbe50, 0x8ccb, 0x9d42,
    0x7e9e, 0x6f17, 0x5d8c, 0x4c05, 0x38ba, 0x2933, 0x1ba8, 0x0a21, 0xf2d6, 0xe35f, 0xd1c4, 0xc04d, 0xb4f2, 0xa57b,
    0x97e0, 0x8669, 0x7787, 0x660e, 0x5495, 0x451c, 0x31a3, 0x202a, 0x12b1, 0x0338, 0xfbcf, 0xea46, 0xd8dd, 0xc954,
    0xbdeb, 0xac62, 0x9ef9, 0x8f70};

unsigned short calculateCRC16(unsigned char length) {
	unsigned short crc = 0x0000;  // Initial value
	unsigned char i;

	for (i = 0 ; i != length ; i++) {
		unsigned char byte = rx_buf[i + 1];
		unsigned char tbl_idx = ((crc >> 8) ^ byte);
		crc = (crc_table[tbl_idx] ^ (crc << 8));
	}
	return crc;
}


// send data from the rx buffer to the in endpoint if there is a complete message

static void send_data(void) {
    char c;
    char rx_outp = 0;
	char len = rx_buf[2];
    // is the message complete?
    if (rx_inp < MIN_LEN_B || rx_inp < len + 2)
        return;
	if(USB_status[USB_IN_EP] & TX_BUSY)
		return;     // fifo is full
	if (calculateCRC16(len+1) != 0)
		rx_buf[0] |= CRC_ERR;
    while (rx_outp != len + 2) {
        USB_send_byte(USB_IN_EP, rx_buf[rx_outp++]);
    }
    USB_flushin(USB_IN_EP, 0);
    green_led_state = LED_OFF;
}


static void interrupt
t2_isr(void) @ TIMER2
{
	static unsigned char		debouncer;
    static unsigned int         flcounter;

	TF2H = 0;					// clear interrupt flag
	if(timeout != 0)
        timeout--;

    switch(++flcounter) {
        case FLASH_PRELOAD:
            flcounter = 0;
            flashLed(TRUE);
            break;

        case FLASH_PRELOAD/2:
            flashLed(FALSE);
            break;
    }
}

static void interrupt
UART_isr(void) @ SERIAL
{
    unsigned char c, remaining;
	while (RI0) {
        c = SBUF0;
        if (rx_inp == sizeof(rx_buf))
            rx_inp = 0;
        if (RB80) {   // start of master message, clear buffer
            rx_inp = 1;
            rx_buf[0] = MASTER_MSG;
        } else if (rx_inp == 0 && c <= 0x20) {
            rx_inp++;
            rx_buf[0] = 0;
        }
        if (rx_inp != 0) {
            rx_buf[rx_inp++] = c;
        }
        RI0 = FALSE;
        // check for valid length
        if (rx_inp == 3 && (c < MIN_LEN_B || c > MAX_LEN_B))
            rx_inp = 0;
        if (rx_inp > MIN_LEN_B)
            green_led_state = LED_ON;
        break;
	}
    while (TI0) {
        TI0 = FALSE;
        if (tx_inp == tx_outp) {
			tx_outp = 0;
            red_led_state = LED_OFF;
            TX_ENABLE = FALSE;
            break;
        }
		if (tx_outp == 0)
			TB80 = TRUE;
		else
			TB80 = FALSE;
        SBUF0 = tx_buf[tx_outp++];
    }
}

static void
Endpoint_1(void)
{
	unsigned char	cnt, i;

	// don't read if still sending last packet
	if (TX_ENABLE)
		return;
	tx_inp = USB_read_packet(USB_OUT_EP, tx_buf, sizeof tx_buf);
    USB_flushin(USB_OUT_EP, TRUE);
	red_led_state = LED_ON;
	tx_outp = 0;
	TX_ENABLE = TRUE;
	TI0 = TRUE;
}


static void
CLK_init(void)
{
    // Timer 1 is baud rate

    TH1 = 152;              // 115200 at 24MHz
    TMOD = 0B00100000;      // mode 2 timer
    CKCON = 0B1000;         // sysclk input to timer 1
    TR1 = 1;                // enable timer

    SCON0 = 0x90;           // UART settings - rx enabled, 9 bit
	ES0 = 1;				// enable UART interrupts

	// setup timer 2 as a periodic interrupt
	TMR2RLL = -T2LOAD & 0xFF;
	TMR2RLH = -T2LOAD/256;
	TR2 = 1;				// enable the timer - note first period will be extended.
	ET2 = 1;				// enable timer2 interrupts.
	PCA0CN = 0x40;			// enable PCA
	PCA0CPH0 = 0x80;
	PCA0CPL4 = 0xFF;		// maximum watchdog timeout available - about 30ms
	PCA0MD = 0b00001000;    // enable and lock watchdog timer, pca clocked by system/4
	EA = 1;					// enable global interrupts
}


static void
PORT_init(void)
{
	// init IO ports

	P0MDOUT = 0b01010001;	// bits 6, 4 and 0 are push-pull output
	P0SKIP =  0b11001011;	// skip all in P0
	P1MDOUT = 0b00000000;	// all input except 1
	P1SKIP =  0b11111111;	// skip all except 1
	P2MDOUT = 0b11100010;	// all input except bits 1, 5, 6 and 7
	P2SKIP =  0b11111111;	// P2.0 is CEX1
	XBR0 =    0x01;         // enable UART
    XBR1 =	  0x40;			// enable crossbar - enable pullups
	P0 = 	  0b11111110;	// turn on power led, others off
    P2 =      0b11111101;   // set all outputs high in P2
}


void
main(void)
{
    unsigned char req;

	PORT_init();
	CLK_init();
	USB_init(&sicom_usb_table);
    GREEN(1);
    ACTIVE(1);
    // let settle for a few seconds
    green_led_state = LED_FLASH;
	for(;;) {
		KICK_DOG();
		EA = 0;
		if(USB_status[0] & USB_SUSPEND) {
			// suspended - turn the LEDs off lest we draw too much current.
			ACTIVE(FALSE);
			GREEN(FALSE);
		}
		if(USB_status[0] & (RX_READY|USB_SUSPEND)) {
			EA = 1;
			req = USB_control();
			if (req == 0x7F) {
				red_led_state = LED_FLASH;
                green_led_state = LED_OFF;
				SET_TIMEOUT(1000);
				while(!TIMEDOUT())
					continue;
				USB_detach();
				SET_TIMEOUT(1000);
				while(!TIMEDOUT())
					continue;
				RSTSRC |= 0x10;         // software reset
			}
		}
		if(USB_status[USB_OUT_EP] & RX_READY) {
			EA = 1;
			Endpoint_1();
			continue;
		}
        send_data();
		EA = 1;
		PCON |= 1;		// sleep
	}
}

#asm	
	psect   text,class=CODE
	global  powerup,start1

powerup:
	; The powerup routine is needed to disable the Watchdog timer
	; before the user code, or else it may time out before main is
	; reached.
	anl     217,#-65
	jmp     start1
#endasm
