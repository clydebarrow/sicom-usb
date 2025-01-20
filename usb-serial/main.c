#include	<8051.h>
#include	"crc.h"
#include	<string.h>

#include	<stdio.h>
#include	<htusb.h>
#include	"sicom.h"



// The USB descriptors.

extern USB_CONST USB_descriptor_table	sicom_usb_table;

extern void serialSend(void);

static unsigned char xdata rx_buf[256];
static unsigned char xdata tx_buf[MAX_MSG_LEN];		// transmit data buffer

static unsigned char device_idx;


// buffer pointers

static unsigned char rx_inp, tx_inp, tx_outp;

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

#define TX_ENABLE P2_BITS.B1


#define	SET_TIMEOUT(x)	((timeout = (((unsigned long)(x)*T2RATE))/1000))
#define TIMEDOUT()  (timeout == 0)
#define KICK_DOG()      (PCA0CPH4 = 0)          // kick watchdog


#define	FLASH_RATE	3
#define	FLASH_PRELOAD	((T2RATE+1)/FLASH_RATE)

#define MESSAGE_RATE 128	// 32 main loops between messages.


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
		SET_TIMEOUT(5);
    	if (next != (unsigned char)sizeof(rx_buf)) {
    		rx_buf[rx_inp++] = c;
    		green_led_state = LED_ON;
    	} else {
    		rx_inp = 0;
    		green_led_state = LED_OFF;
    	}
    }
    if (TI0) {
        TI0 = FALSE;
        if (tx_inp == tx_outp) {
			tx_outp = 0;
        	tx_inp = 0;
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

static const unsigned char ALL_CALL[] = { 0xFA, 0x04, 0x31, 0xCF, 0xFF};

static void send_data(void) {
	unsigned char idx = 0;
	unsigned char len = rx_buf[1] + 1;
	if (rx_inp < MIN_MSG_LEN)
		return;
	if (len >= MAX_MSG_LEN) {
		rx_inp = 0;
		green_led_state = LED_OFF;
		return;
	}
	if (rx_inp < len)
		return;
    if(USB_status[USB_IN_EP] & TX_BUSY || calculateCRC16(rx_buf, len) != 0) {
		rx_inp = 0;
		green_led_state = LED_OFF;
		return;
	}
	do {
        USB_send_byte(USB_IN_EP, rx_buf[idx++]);
    } while(--len != 0);
	rx_inp = 0;
    USB_flushin(USB_IN_EP, 0);
    green_led_state = LED_OFF;;
}

static void start_uart()
{
	if (TX_ENABLE || !TIMEDOUT() || tx_inp == 0)
		return;
	TX_ENABLE = TRUE;
	rx_inp = 0;	// flush the receive buffer
	green_led_state = LED_OFF;
	tx_outp = 0;
	TI0 = TRUE;
}

static void
endpoint_out(void)
{
	unsigned char cnt;

	cnt = USB_read_packet(USB_OUT_EP, tx_buf, sizeof tx_buf);
	USB_flushin(USB_OUT_EP, TRUE);
	switch (tx_buf[0])
	{
	case LOG_MASTER:
		log_master = TRUE;
		break;

	case NO_LOG_MASTER:
		log_master = FALSE;
		break;

	default:
		break;
	}
	if (tx_buf[0] <= MAX_ADDRESS || tx_buf[0] == BROADCAST) {
		tx_outp = 0;
		tx_inp = cnt;
		start_uart();
	}
}

static void send_request() {
	if (TX_ENABLE)
		return;
	if (device_idx == 0) {
		memcpy(tx_buf, ALL_CALL, sizeof ALL_CALL);
	} else {
		tx_buf[0] = device_idx;
		tx_buf[1] = 0x04;
		tx_buf[2] = REQUEST_DATA_MSG;
		add_crc(tx_buf, 3);
	}
	tx_outp = 0;
	tx_inp = 5;
	device_idx++;
	device_idx &= 0x3;
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
	TI0 = FALSE;
	RI0 = FALSE;
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
    unsigned char req, cnt = 0;

	PORT_init();
	CLK_init();
	USB_init(&sicom_usb_table);
    green_led_state = LED_FLASH;
	red_led_state = LED_OFF;
	for(;;) {
		KICK_DOG();
		EA = 0;
		if(USB_status[0] & USB_SUSPEND) {
			// suspended - turn the LEDs off lest we draw too much current.
			red_led_state = LED_OFF;
			green_led_state = LED_OFF;
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
			endpoint_out();
			continue;
		}
		EA = 1;
        send_data();
		cnt++;
		if (cnt == 0)
			red_led_state = !red_led_state;
		//if ((cnt & (MESSAGE_RATE -1)) == 0)
			//send_request();
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
