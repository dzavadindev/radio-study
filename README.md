# Documentation

## Assignment 1 

--- WIP ---

## Assignment 2

### Part 1 

This part is about testing link reliability under different possible configurations. Those configuration options are:

- Power Amplification (PA) - literal power of the signal
- Datarate - how much data can the communication support, bps
- Automatic Retry Delay - factor defining how much 250uS to wait between retires 
- Automatic Retry Count - the maximum amount of retries before giving up
- Channel - the communication channel inside the band

The main principle of work for the app is:

- TX selects a config (PA, Rate, ARD, ARC and Channel).
- Then it sends a packet containing that config information to the RX over the Control Channel (a channel and config combo that is static for both RX and TX).
- RX receives the config on the Control Channel and sends and auto-ACK.
- Only after TX has received an ACK from RX will it initiate the tests for the config selected.
- After END has been received by RX, it switches back to Control Channel to listen for more configuration packets.
- Repeat for a few different configurations

We have selected a few configurations, because testing all ~390,000 variants would not have been fun. Those configurations are:

- Channels: 10–20 (or 3 bands: low/mid/high)
- ARC: {0, 3, 7, 15}
- ARD: {0, 2, 5, 15} (or 250us, 750us, 1500us, 4000us of delay)
- PA: all 4
- Rate: all 3

That narrows it down to about 4000 variants to cover which is a bit more manageable.

### Part 2

--- WIP ---

# Questions

1) What is the maximum output power of the NRF24 module?
How does that compare to the output power of a 3G/4G/5G mobile phone?

```
officialy the max out put is 0 dBm ≈ 1 m Bu the transmitor suports when looking at the data sheet +4dBm + 4 mW
IT is alot lower than the output power of 3G 4G and 5G 
WMax TX setting:  
    - 3G = +24 dBm = 250 mW 
    - 4G = +23 dBm = 200 mW
    - 5G = +23 dBm = 200 mW

```

2) What is a frequency band?

```
 A specific range of frequencies in the electromagnetic frequency spectrum assigned to certain applications.
 For example the frequency band of NRF25L01+ is 2.4GHz.
```

3) Which frequency bands can be used by this radio?
• And which bands are used by WiFi and Bluetooth?
• So, where does the NRF24 overlap with these bands?

```
The nRF24L01+ is designed for operation in the world wide ISM frequency band at 2.400 - 2.525GHz. 5.2 General RF conditions page 15

Wi-Fi & Bluetooth bands: 2.4 GHz Wi-Fi,  uses 2412 MHz (channel 1) up to 2472 MHz (channel 13) GHz and also 5 GHz; newer Wi-Fi can also use 6 GHz where allowed.
Bluetooth channel center frequencies: 2402–2480 MHz

Overlap nRF24 - 2.4 GHz Wi-Fi: about 2401–2483.5 MHz basically the whole 2.4 GHz Wi-Fi band sits inside the nRF24’s 2.4 GHz ISM range.
Overlap nRF24 - Bluetooth: 2402–2480 MHz  Basically all of Bluetooth overlaps.
```

4) What is the wavelength in meters for the used NRF24 frequency band?

```
λ=v/f
λ= 3*10^8 m/s / 2.4 GHz ~ 0.12 m
```

5) Which data rates does the radio support?

```
it supports 250kbps, 1Mbps and 2Mbps on air data rates.

```

6) How many channels does the radio support?
• What is the relationship between channels and frequencies?

```
The greater hierarchy is ISM Bands -> Channels -> Frequency shifting. For us the ISM band is 2.4GHz, the channel spread is 1 MHz across 126 channels (2.4GHz-2.525) and lastly the FSK happens within the channel range (2.4GHz-2.401GHz)
```

7) Which modulation technique does the radio use?
• Explain how this modulation technique works in your own words.

```
It uses GFSK or Gaussian Ferquency Shift Keying. FSK is basically just using multiple different frequencies withing the band to transmit encoded data. The encoding may look like: `fc − Δf` for 0 and `fc − Δf` for 1, where fc is the carrier frequency (channel) and delta f is the deviation factor. Its even possible to encode tuples and triplets of data within that channel, depending on the modulation scheme (3-FSK and 4-FSK). The one nRF24 uses is GFSK, which is a binary FSK implementation

"Gaussian" here means that the frequency at which the signals switch between the band frequencies is being smoothed by the Gaussian filter. This prevents sudden changes between frequencies at zero-time or infinite speed. Electrons don't move at infinite speed, even they are bound by physics, which means that forcing the electrons to move very hard, for a very short time, with extremely rapid changes in force will inavitebly cause fast oscillations. A pure square wave forces the electrons to move in many simultaneous rhythmic patterns.

```

8) Power consumption in the idle mode states (power-down, standby-I and standby-II).
• Explain in your own words what the different states do.

```
Page 8 and 14.
Consumes 26μA in Standby-I mode, 320μA in Standby-II mode and 900nA in power down mode.

Power down mode has all register values maintaines but the SPI powered on. That means its possible to change configs and upload/download data from. It allows the module to hover "half-awake" or "basically off" while still maintaining registers intact. Tradeoff is the slowest bootup time out of all 3 modes - 1.5ms

Standby-I is made to minimise average current consumption while maintaining short start up times. Besides everything that is active during "power down" (registers and SPI) the ocillator is also on during this mode. Usually used between packets when the radio is not transmitting or listening, or in low packet frequency communications. Slightly longer wake-up at 130μs

In standby-II mode extra clock buffers are active and more current is used compared to standby-I mode. But that does mean that the radio wakes up instantly, as there is no PPL initialization time. In this mode anything written into the TX FIFO buffer will immediatelly be transmitted.
```

9) Power consumption in the transmit state at the four different output levels.

```
     0dBm   output   power = 11.3 mA
    -6dBm   output   power = 9.0 mA
    -12dBm  output   power = 7.5 mA 
    -18dBm  output   power = 7.0 mA 
    see page 14 :  5.1 power consumption  

```

10) Power consumption in the receive mode state for the three different air data rates.

```
Page 14, Table 4
There are only three air data rates: 250 kbps, 1 Mbps and 2 Mbps.
At 250kbps is 12.6mA, at 1Mbps its 13.1mA and at 2Mbps its 13.5mA
```

11) What is Enhanced ShockBurst and what are the most important functions? Describe this in
your own words.


```
Enhanced ShockBurst is the nRF24L01+’s built-in packet-handling and reliability engine. It automatically formats and buffers packets, checks CRC on receive, and can acknowledge received packets and retransmit automatically if no ACK is received, so your microcontroller doesn’t have to implement these low-level link functions in software.
```

12) On what layer in the ISO/OSI model does this operate?


```
ShockBurst allows for automatic packet assembly and timing, automatic acknowledgement and retransmissions of packets. That would mean that it operates on the 2 OSI Level - Data Link, as it deals with handling the packet transmissions.
```

13) Look at the format of the radio packet (section 7.3 on page 28 of the datasheet) and describe
the function of the following fields:
a. Address
b. PID (Packet Identification)
c. No acknowledgement flag
d. CRC


```
a. Address – The receiver address used so the packet is detected and accepted by the correct receiver, avoiding accidental cross-talk with other nRF24L01+ systems.

b. PID (Packet Identification) – A 2-bit identifier used to tell whether a received packet is new or a retransmission, so the PRX doesn’t deliver the same payload twice.

c. No acknowledgement flag (NO_ACK) – When auto-ack is enabled, setting NO_ACK = 1 tells the receiver not to send an automatic ACK for that packet.

d. CRC – Mandatory error check 1 - 2 bytes calculated over the address + packet control field + payload; packets are rejected if the CRC fails.
```

14) Enhanced ShockBurst is able to automatically validate a packet. Describe how this process
works in your own words. Give an example of a packet that is broken and describe how the
radio is able to detect this.

```
There are multiple parts of a packet validation.

- The first is preamble - a byte long set of alternating 1 and 0. It's used to syncronise the demodulator with the transmit frequency. This gets rid of noise RF being picked up by the receiver.
- Next one is addressing. Packets that have an address different from the receiving device primary RX address are autodiscarded.
- Next is the PCF or Packet Control Filed. This contains the lenght of the subsequent payload, the packet unique ID, used for deduplication, and the NO_ACK flag. This one is important for validating broken payloads, as this covers 3 scenarios: payload too short/long, packet already received and an expected acknowledgement was not received. 
- Last thing is the CRC, which is computed from the address, PCF and the payload itself. The closest thing to compare to is hashing. One of the simpliest CRC methods is an XOR checksum. If the CRC computed on the receiver side doesn't match the one received in the TX payload - packet is malformed.

So, imagine a situation.
We are transfeting this:

[ Address: 0xE7 0xE7 0xE7 0xE7 0xE7 ][ PCF: length = 2 ][ Payload: 0x34 0x12 ][ CRC: 0xA9 0x3F ]

Then during the transfer we encounter a interfering frequency - not a payload byte is flipped.

[ Address: 0xE7 0xE7 0xE7 0xE7 0xE7 ][ PCF: length = 2 ][ Payload: 0x24 0x12 ][ CRC: 0xA9 0x3F ]

So all the checks described before will pass until we get to the CRC. Because of a flipped bit the CRC is mismatched now and the packet is discarded.

```
15) What conditions define a good address (see section 7.3.2 of the datasheet)? Give an example
of a good 5-byte address that adheres to these conditions. Write the address in hexadecimal
and binary format.

```
A “good” address is one that doesn’t look like noise or the preamble: avoid addresses where the bit level only shifts once and avoid addresses that are basically a continuation of the preamble’s hi–low toggling Example of a good 5-byte address: 0xE7D3F03577
```

16) Give an example of a bad 5-byte address and explain why this is not a good address. Write
the address in hexadecimal and binary format.

```
[0x00 0xFF 0xFF 0xFF 0xFF] is considered a bad address. Its considered such because the level only shifts ones, which means the address contains many duplicated hex values, which may be interpreted by the receiver hardware as plain noise.
```

17) Add a table that describes the name and the function of every NRF24 pin. Include to which
pin of the Arduino Uno header the NRF24 pin is connected


| nRF24 pin| Function| Arduino Uno header pin (typical)|
| -|- | -|
| **GND** | Ground reference | **GND**|
| **VCC** | Power input (**3.3V only**)| **3.3V**|
| **CE**  | Chip Enable (controls RX/TX mode)| **D9** *(any free digital pin works)*|
| **CSN (CS)** | SPI Chip Select (active LOW)| **D10** *(common choice)*|
| **SCK** | SPI clock| **D13** *(SCK)* |
| **MOSI**| SPI Master Out → Slave In| **D11** *(MOSI)*|
| **MISO**| SPI Master In ← Slave Out| **D12** *(MISO)*|
| **IRQ**| Interrupt output (active LOW, optional) | **D2**  |



18) The Getting started example has the TX and RX address configured to a default value. What
do you think would happen If a classroom full of students would start using the same
address?

```
If all devices use the same address, every receiver will accept every packet and attempt to acknowledge it. This will lead to acknowledgement mess, where a single message is acknowledged more than once, or the acknowledgements cause collisions between each other. This will force the transmitter to retry, and cause the same thing to happen again. The channel will become a complete mess within seconds.
```

19) Find in the datasheet what ‘ART’ stands for and explain this in your own words

```
ART= Auto Retransmission;
It is a function where the radio automatically re-sends a packet if it doesn’t receive an ACK from the receiver.It will retry up to a programmable limit, with a programmable delay between retries, and it stops early as soon as an ACK is received.

```

