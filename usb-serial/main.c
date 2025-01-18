#include	<8051.h>

#include	<stdio.h>
#include	<htusb.h>
#include	"sicom.h"

// flag bits
#define MASTER_MSG     0x80
#define LOG_MASTER     0x80
#define NO_LOG_MASTER  0x7F
#define MAX_ADDRESS    0x20
#define BROADCAST      0xFA
#define CRC_ERR        0x01
#define MAX_MSG_LEN    64
#define MIN_MSG_LEN    5


// The USB descriptors.

extern USB_CONST USB_descriptor_table	sicom_usb_table;

extern void serialSend(void);

static unsigned char xdata rx_buf[256];
static unsigned char xdata tx_buf[MAX_MSG_LEN];		// transmit data buffer

// buffer pointers

static unsigned char rx_inp, rx_outp, tx_inp, tx_outp;

// if true, return master messages as well
static bit log_master;

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


// send data from the rx buffer to the in endpoint if there is a complete message

static void send_data(void) {
	unsigned char remaining = rx_inp - rx_outp, len;
	// skip over any non-address bytes
	while (remaining) {
		unsigned char c = rx_buf[rx_outp];
		if (c <= MAX_ADDRESS || c == BROADCAST)
			break;
		rx_outp++;
		remaining--;
	}
	if (remaining < MIN_MSG_LEN)
		return;
	len = rx_buf[(unsigned char)(rx_outp+1)] + 1;
	// check for a valid message length
	if (len > MAX_MSG_LEN) {
		rx_outp++;
		return;
	}
	// check for a complete message
	if (remaining < len)
		return;

    if(USB_status[USB_IN_EP] & TX_BUSY)
        return;     // fifo is full

	do {
        USB_send_byte(USB_IN_EP, rx_buf[rx_outp++]);
    } while(--len != 0);
    USB_flushin(USB_IN_EP, 0);
    green_led_state = LED_OFF;;
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
    if (RI0) {
    	unsigned char next = rx_inp + 1;
    	unsigned char c = SBUF0;
    	RI0 = FALSE;
    	if (next != rx_outp)
    	{
    		rx_buf[rx_inp++] = c;
    		green_led_state = LED_ON;
    		SET_TIMEOUT(5);
    	}
    }
    if (TI0) {
        TI0 = FALSE;
        if (tx_inp == tx_outp) {
			tx_outp = 0;
            TX_ENABLE = FALSE;
        } else
        {
	        if (tx_outp == 0)
	        	TB80 = TRUE;
	        else
	        	TB80 = FALSE;
        	SBUF0 = tx_buf[tx_outp++];
        }
    }
}

static void
Endpoint_1(void)
{
	unsigned char	cnt, i;

	// don't read if still sending last packet or receiving
	red_led_state = LED_ON;
    if (TX_ENABLE || !TIMEDOUT())
        return;
	red_led_state = LED_ON;
	tx_inp = USB_read_packet(USB_OUT_EP, tx_buf, sizeof tx_buf);
    USB_flushin(USB_OUT_EP, TRUE);
    switch (tx_buf[0]) {
        case LOG_MASTER:
            log_master = TRUE;
            break;

        case NO_LOG_MASTER:
            log_master = FALSE;
            break;

        default:
            if (tx_buf[0] <= MAX_ADDRESS || tx_buf[0] == BROADCAST) {
                tx_outp = 0;
                TX_ENABLE = TRUE;
                TI0 = TRUE;
            }
            break;
    }
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
	red_led_state = LED_OFF;
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
