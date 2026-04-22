## Sicom Interface Protocol

The Sicom bus is used by Simarine devices to communicate sensor data.

### Hardware interface

* Signal levels: RS485
* Cable: 4 wire
* Pinout:
  *  +12v (nominal)
  *  GND
  *  RS485-A
  *  RS485-B
* Connectors: RJ9

### Display connector

The Sicom display uses a 4 pin round DIN connector with the pinout:

1. A
2. 12V
3. GND
4. B

View looking at connector (male pins) on back of display, upright.

```
1    4
 2  3
```


### Signalling

* Baud rate: 115200
* Data bits: 9
* Stop bits: 1

### Messaging protocol

The bus protocol allows for one controller and multiple (up to 32) devices. Devices only ever transmit in response to a controller request.

Bit 8 of a data byte is set only for the first byte in a controller message.

### Serial Numbers

Bus devices each have a unique 32 bit serial number.

### Controller Messages

Each controller message has the following format:

* Address (1 byte) with bit 8 set
* Length (1 byte) - the number of bytes in the message excluding the address, but including the length and CRC
* Type (1 byte) - the message type
* Data (length-4 bytes)
* CRC (2 bytes)


The CRC is calculated with a CRC-16 with polynomial 0x1189, initial value 0, xor value 0 and no inversions. All bytes in the
message before the CRC are included in the calculation, and the CRC is appended in big-endian format. This means that a CRC calculated on the received message will be equal to 0 if the message is intact.

Multi-byte data is send in big-endian format.

### Device Messages

* Address (1 byte) with bit 8 set
* Length (1 byte) - the number of bytes in the message excluding the address, but including the length and CRC
* Device (1 byte) - the type of the device. See below for values
* Type (1 byte) - the message type
* Data (length-5 bytes)
* CRC (2 bytes)

### Addressing

Devices are assigned an address in the range 1-31 by the controller during the discovery protocol. All subsequent messages to or from the device use that address.

The address in a message is a single byte (1-based) representing the destination (for a controller message) or source (for a device message.)
 The 9th bit is set for all messages from the controller so a message addressed to device 1 will start with the address 0x101.
A response from device 1 will start with address byte 1.

There is a special address for broadcast enquiries equal to 0x1FA. This is used in the discovery protocol.


### Discovery Protocol

Devices on the bus are initially unconfigured, i.e. do not have an address assigned. The controller requests new devices to identify themselves with a command 31 "ALL-CALL" broadcast message:


```
1FA.04.31.CF.FF
```

Any devices on the bus that have not yet been configured will respond to this. There is presumably some algorithm implemented in devices to prevent simultaneous responses. The response will be of the form:

```
FA.09.<type>.C0.<serial-L>.<crc>
```

Where <type> is a single byte denoting the device type. Known values are:

* 0x02 - SCQ25T
* 0x03 - ST107
* 0x0E - SC301  
* 0x10 - SC303


The serial number is the 32 bit serial number of the device, in *little endian* order.

The master responds to this message with the an enrolment message:

```
1FA.09.20.<serial-B>.<address>.<crc>
```

Where the serial number is sent in big-endian format. The address is the 1-based address that will be used to communicate
with the device from now on.

The device acknowledges this with a message:

```
<address>.09.<type>.C1.<serial-L>.<crc>
```

On receipt this the master must immediately send a data request message (see below) which the device uses to confirm
that it has been enrolled successfully. Subsequently as long as the device is polled at least once per second it will
not respond to the ALL-CALL  message.

The ALL-CALL  message is repeated once per second to identify any further devices or new devices added to the bus.

### Data Request

The controller requests data from each device several times per second. The data request message is:

```
<address>.04.0A.<crc>
```

Where address is the device address with bit 8 set, e.g. 0x101 for a device with address 1.

The device responds with a message:


```
<address>.<length>.<type>.A0.<data>.<crc>
```

The contents of the data field is device-specific.


### SC301 Data

FF.FF.FF.FC Current .01A
04.E8
3C.7D
00.08
65.00
0C.9F U2 .001V
28.67
28.67


