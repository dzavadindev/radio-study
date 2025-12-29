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
```

5) Which data rates does the radio support?

```
it supports 250kbps, 1Mbps and 2Mbps on air data
rates.

```

6) How many channels does the radio support?
• What is the relationship between channels and frequencies?

```
```

7) Which modulation technique does the radio use?
• Explain how this modulation technique works in your own words.

```
```

8) Power consumption in the idle mode states (power-down, standby-I and standby-II).
• Explain in your own words what the different states do.

```
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
```

11) What is Enhanced ShockBurst and what are the most important functions? Describe this in
your own words.


```
Enhanced ShockBurst is the nRF24L01+’s built-in packet-handling and reliability engine. It automatically formats and buffers packets, checks CRC on receive, and can acknowledge received packets and retransmit automatically if no ACK is received, so your microcontroller doesn’t have to implement these low-level link functions in software.
```

12) On what layer in the ISO/OSI model does this operate?


```
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
```

17) Add a table that describes the name and the function of every NRF24 pin. Include to which
pin of the Arduino Uno header the NRF24 pin is connected

```
```

18) The Getting started example has the TX and RX address configured to a default value. What
do you think would happen If a classroom full of students would start using the same
address?

```
```

19) Find in the datasheet what ‘ART’ stands for and explain this in your own words

```
ART= Auto Retransmission;
It is a function where the radio automatically re-sends a packet if it doesn’t receive an ACK from the receiver.It will retry up to a programmable limit, with a programmable delay between retries, and it stops early as soon as an ACK is received.

```

