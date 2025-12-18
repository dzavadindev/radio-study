1) What is the maximum output power of the NRF24 module?
How does that compare to the output power of a 3G/4G/5G mobile phone?

```
+4 dBm??
Max TX setting: 0 dBm ≈ 1 mW
+24 dBm = 250 mW 3G 
+23 dBm = 200 mW.4g
+23 dBm = 200 mW.5G

```

2) What is a frequency band?

```
```

3) Which frequency bands can be used by this radio?
• And which bands are used by WiFi and Bluetooth?
• So, where does the NRF24 overlap with these bands?

```
```

4) What is the wavelength in meters for the used NRF24 frequency band?

```
λ=v/f
λ= 3*10^8 m/s / 2.4 GHz ~ 0.12 m
```

5) Which data rates does the radio support?

```
```

6) How many channels does the radio support?
• What is the relationship between channels and frequencies?

```
```

7) Which modulation technique does the radio use?
• Explain how this modulation technique works in your own words.

```
It uses GFSK or Gaussian Ferquency Shift Keying. FSK is basically just using multiple different frequencies withing the band to transmit encoded data.

The greater hierarchy is ISM Bands -> Channels -> Frequency shifting. For us the ISM band is 2.4GHz, the channel spread is 1 MHz across 126 channels (2.4GHz-2.525) and lastly the FSK happens within the channel range (2.4GHz-2.401GHz). In that case, the encoding may look like: 2.400GHz for 0 and 2.490GHz for 1, or even encode tuples and triplets of data within that channel. (i.e. 00 -> 2.400GHz, 01 -> 2.410GHz, 10 -> 2.420GHz, 11 -> 2.430GHz)

"Gaussian" here means that the frequency at which the signals switch between the band frequencies is being smoothed by the Gaussian filter. This prevents sudden changes between frequencies at zero-time or infinite speed. Electrons don't move at infinite speed, even they are bound by physics, which means that forcing the electrons to move very hard, for a very short time, with extremely rapid changes in force will inavitebly cause fast oscillations. A pure square wave forces the electrons to move in many simultaneous rhythmic patterns.

```

8) Power consumption in the idle mode states (power-down, standby-I and standby-II).
• Explain in your own words what the different states do.

```

```

9) Power consumption in the transmit state at the four different output levels.

```
```

10) Power consumption in the receive mode state for the three different air data rates.

```
```

11) What is Enhanced ShockBurst and what are the most important functions? Describe this in
your own words.


```
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
```

