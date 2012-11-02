/* USB MIDI Link
 * 
 * (C) 2010 by morecat_lab (original: MICOMOCO http://morecatlab.akiba.coocan.jp/morecat_lab/MOCO.html)
 * (C) 2012 Kaoru Shoji (this version: https://github.com/kshoji/USB-MIDI-Link)
 */

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "oddebug.h"

void parseUSBMidiMessage(uchar *, uchar);
uchar parseSerialMidiMessage(uchar);

/*
 PORT ASSIGN
 PD0: RxD (Serial MIDI)
 PD1: TxD (Serial MIDI)
 PD2: USB D+
 PD3: USB D-
 */

#define HW_CDC_BULK_OUT_SIZE     (8)
#define HW_CDC_BULK_IN_SIZE      (8)

#define	TRUE			(1)
#define	FALSE			(0)

// This deviceDescrMIDI[] is based on 
// http://www.usb.org/developers/devclass_docs/midi10.pdf
// Appendix B. Example: Simple MIDI Adapter (Informative)

// B.1 Device Descriptor
static PROGMEM char deviceDescrMIDI[] = { /* USB device descriptor */
18, /* sizeof(usbDescriptorDevice): length of descriptor in bytes */
USBDESCR_DEVICE, /* descriptor type */
0x10, 0x01, /* USB version supported */
0, /* device class: defined at interface level */
0, /* subclass */
0, /* protocol */
8, /* max packet size */
USB_CFG_VENDOR_ID, /* 2 bytes */
USB_CFG_DEVICE_ID, /* 2 bytes */
USB_CFG_DEVICE_VERSION, /* 2 bytes */
1, /* manufacturer string index */
2, /* product string index */
0, /* serial number string index */
1, /* number of configurations */
};

// B.2 Configuration Descriptor
static PROGMEM char configDescrMIDI[] = { /* USB configuration descriptor */
9, /* sizeof(usbDescrConfig): length of descriptor in bytes */
USBDESCR_CONFIG, /* descriptor type */
101, 0, /* total length of data returned (including inlined descriptors) */
2, /* number of interfaces in this configuration */
1, /* index of this configuration */
0, /* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
		(1 << 7) | USBATTR_SELFPOWER, /* attributes */
#else
		(1 << 7), /* attributes */
#endif
				USB_CFG_MAX_BUS_POWER / 2, /* max USB current in 2mA units */

// B.3 AudioControl Interface Descriptors
// The AudioControl interface describes the device structure (audio function topology) 
// and is used to manipulate the Audio Controls. This device has no audio function 
// incorporated. However, the AudioControl interface is mandatory and therefore both 
// the standard AC interface descriptor and the classspecific AC interface descriptor 
// must be present. The class-specific AC interface descriptor only contains the header 
// descriptor.

// B.3.1 Standard AC Interface Descriptor
// The AudioControl interface has no dedicated endpoints associated with it. It uses the 
// default pipe (endpoint 0) for all communication purposes. Class-specific AudioControl 
// Requests are sent using the default pipe. There is no Status Interrupt endpoint provided.
		/* descriptor follows inline: */
		9, /* sizeof(usbDescrInterface): length of descriptor in bytes */
		USBDESCR_INTERFACE, /* descriptor type */
		0, /* index of this interface */
		0, /* alternate setting for this interface */
		0, /* endpoints excl 0: number of endpoint descriptors to follow */
		1, /* */
		1, /* */
		0, /* */
		0, /* string index for interface */

// B.3.2 Class-specific AC Interface Descriptor
// The Class-specific AC interface descriptor is always headed by a Header descriptor 
// that contains general information about the AudioControl interface. It contains all 
// the pointers needed to describe the Audio Interface Collection, associated with the 
// described audio function. Only the Header descriptor is present in this device 
// because it does not contain any audio functionality as such.
		/* descriptor follows inline: */
		9, /* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
		36, /* descriptor type */
		1, /* header functional descriptor */
		0x0, 0x01, /* bcdADC */
		9, 0, /* wTotalLength */
		1, /* */
		1, /* */

// B.4 MIDIStreaming Interface Descriptors

// B.4.1 Standard MS Interface Descriptor
		/* descriptor follows inline: */
		9, /* length of descriptor in bytes */
		USBDESCR_INTERFACE, /* descriptor type */
		1, /* index of this interface */
		0, /* alternate setting for this interface */
		2, /* endpoints excl 0: number of endpoint descriptors to follow */
		1, /* AUDIO */
		3, /* MS */
		0, /* unused */
		0, /* string index for interface */

// B.4.2 Class-specific MS Interface Descriptor
		/* descriptor follows inline: */
		7, /* length of descriptor in bytes */
		36, /* descriptor type */
		1, /* header functional descriptor */
		0x0, 0x01, /* bcdADC */
		65, 0, /* wTotalLength */

// B.4.3 MIDI IN Jack Descriptor
		/* descriptor follows inline: */
		6, /* bLength */
		36, /* descriptor type */
		2, /* MIDI_IN_JACK desc subtype */
		1, /* EMBEDDED bJackType */
		1, /* bJackID */
		0, /* iJack */

		/* descriptor follows inline: */
		6, /* bLength */
		36, /* descriptor type */
		2, /* MIDI_IN_JACK desc subtype */
		2, /* EXTERNAL bJackType */
		2, /* bJackID */
		0, /* iJack */

//B.4.4 MIDI OUT Jack Descriptor
		/* descriptor follows inline: */
		9, /* length of descriptor in bytes */
		36, /* descriptor type */
		3, /* MIDI_OUT_JACK descriptor */
		1, /* EMBEDDED bJackType */
		3, /* bJackID */
		1, /* No of input pins */
		2, /* BaSourceID */
		1, /* BaSourcePin */
		0, /* iJack */

		/* descriptor follows inline: */
		9, /* bLength of descriptor in bytes */
		36, /* bDescriptorType */
		3, /* MIDI_OUT_JACK bDescriptorSubtype */
		2, /* EXTERNAL bJackType */
		4, /* bJackID */
		1, /* bNrInputPins */
		1, /* baSourceID (0) */
		1, /* baSourcePin (0) */
		0, /* iJack */

// B.5 Bulk OUT Endpoint Descriptors

//B.5.1 Standard Bulk OUT Endpoint Descriptor
		/* descriptor follows inline: */
		9, /* bLenght */
		USBDESCR_ENDPOINT, /* bDescriptorType = endpoint */
		0x1, /* bEndpointAddress OUT endpoint number 1 */
		3, /* bmAttributes: 2:Bulk, 3:Interrupt endpoint */
		8, 0, /* wMaxPacketSize */
		10, /* bInterval in ms */
		0, /* bRefresh */
		0, /* bSyncAddress */

// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
		/* descriptor follows inline: */
		5, /* bLength of descriptor in bytes */
		37, /* bDescriptorType */
		1, /* bDescriptorSubtype */
		1, /* bNumEmbMIDIJack  */
		1, /* baAssocJackID (0) */

//B.6 Bulk IN Endpoint Descriptors

//B.6.1 Standard Bulk IN Endpoint Descriptor
		/* descriptor follows inline: */
		9, /* bLenght */
		USBDESCR_ENDPOINT, /* bDescriptorType = endpoint */
		0x81, /* bEndpointAddress IN endpoint number 1 */
		3, /* bmAttributes: 2: Bulk, 3: Interrupt endpoint */
		8, 0, /* wMaxPacketSize */
		10, /* bInterval in ms */
		0, /* bRefresh */
		0, /* bSyncAddress */

// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
		/* descriptor follows inline: */
		5, /* bLength of descriptor in bytes */
		37, /* bDescriptorType */
		1, /* bDescriptorSubtype */
		1, /* bNumEmbMIDIJack (0) */
		3, /* baAssocJackID (0) */
};

uchar usbFunctionDescriptor(usbRequest_t * rq) {
	if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
		usbMsgPtr = (uchar *) deviceDescrMIDI;
		return sizeof(deviceDescrMIDI);
	} else { /* must be config descriptor */
		usbMsgPtr = (uchar *) configDescrMIDI;
		return sizeof(configDescrMIDI);
	}
}

uchar usbFunctionSetup(uchar data[8]) {
	return USB_NO_MSG;
}

uchar usbFunctionRead(uchar *data, uchar len) {
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;
	data[6] = 0;
	return 7;
//	return len;
}

uchar usbFunctionWrite(uchar * data, uchar len) {
	return 1;
}

#define    RX_SIZE        (HW_CDC_BULK_IN_SIZE)
static uchar utxrdy = FALSE; /* USB Packet ready in utx_buf */
static uchar rx_buf[RX_SIZE]; /* tempory buffer */
static uchar utx_buf[RX_SIZE]; /* BULK_IN buffer */

#define    TX_SIZE        (HW_CDC_BULK_OUT_SIZE<<2)
#define    TX_MASK        (TX_SIZE-1)
static uchar uwptr = 0, irptr = 0;
static uchar tx_buf[TX_SIZE];

void usbFunctionWriteOut(uchar *data, uchar len) {
	parseUSBMidiMessage(data, len);
}

void parseUSBMidiMessage(uchar *data, uchar len) {
	uchar i;

	for (i = 0; i < 4; i++) {
		tx_buf[uwptr++] = *(data + i);
		uwptr &= TX_MASK;
	}

	if (len > 4) {
		parseUSBMidiMessage(data + 4, len - 4);
	}
}

// put 4 bytes, then set return value to TRUE
uchar parseSerialMidiMessage(uchar RxByte) {
	static uchar received = 0;
	rx_buf[received] = RxByte;

	if (received == 3) {
		memcpy(utx_buf, rx_buf, 4);
		received = 0;
		return TRUE;
	}
	received++;

	return FALSE;
}

#ifdef ARDUINO
#define PORTD_DDR (0x00)
#else
#define PORTD_DDR (0b00100010)
#endif

static void hardwareInit(void) {
	uchar i, j;

	/* activate pull-ups except on USB lines */
	USB_CFG_IOPORT = (uchar) ~((1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT) | PORTD_DDR);

	/* all pins input except USB (-> USB reset) */
#ifdef USB_CFG_PULLUP_IOPORT
	/* use usbDeviceConnect()/usbDeviceDisconnect() if available */
	USBDDR = 0 | PORTD_DDR; /* we do RESET by deactivating pullup */
	usbDeviceDisconnect();
#else
	USBDDR = (1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT) | PORTD_DDR;
	/* for output (LED, TxD) */
#endif

	j = 0;
	while (--j) { /* USB Reset by device only required on Watchdog Reset */
		i = 0;
		while (--i)
			; /* delay >10ms for USB reset */
	}
#ifdef USB_CFG_PULLUP_IOPORT
	usbDeviceConnect();
#else
	USBDDR = PORTD_DDR; /*  remove USB reset condition */
#endif

	/* set baud rate */

	// UBRR = F_CPU / (16 * baud rate) - 1
	UBRRH = 0;
	UBRRL = 7; /* 125000baud at 16MHz clock */

	UCSRB = (1 << RXEN) | (1 << TXEN);

	// debug LED
	DDRB = 0x01;
}

int main(void) {
	wdt_enable(WDTO_1S);
	odDebugInit();
	hardwareInit();
	usbInit();

	sei();
	for (;;) {
		wdt_reset();
		usbPoll();
		/*    send to Serial MIDI line    */
		if ((UCSRA & (1 << UDRE)) && uwptr != irptr) {
			UDR = tx_buf[irptr++];
			irptr &= TX_MASK;
		}
#if USB_CFG_HAVE_FLOWCONTROL == 1
		if( usbAllRequestsAreDisabled() &&
				((uwptr-irptr)&TX_MASK)<(TX_SIZE-HW_CDC_BULK_OUT_SIZE) ) {
			usbEnableAllRequests();
		}
#endif

		/*    receive from Serial MIDI line    */
		if (UCSRA & (1 << RXC)) {
			utxrdy |= parseSerialMidiMessage(UDR);
		}
		/* send packets to USB MIDI  */
		if (usbInterruptIsReady() && utxrdy) {
			usbSetInterrupt(utx_buf, 4);
			utxrdy = FALSE;
		}
	}
}
