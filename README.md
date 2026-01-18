# Documentation

## Assignment 1 

### Communication between the two boards

This firmware is meant to run on two identical boards.
The boards are configured as **dev01** and **dev02** using `DEVICE_ADDR` (0/1).

Each board does the same things:

**Maintain a single shared state:** `ledCount` (0..6), meaning “how many LEDs are lit”.

**Local button press updates the state first**, then the board **broadcasts the full state** to the other board via the radio.

* BTN_INC increments `ledCount`
* BTN_DEC decrements `ledCount`
* Debounce is handled in software (30 ms) because the MCP inputs are pulled up.

**Receive updates from the other board** and apply them.

When a valid packet arrives, the receiver updates its own `ledCount` and drives LEDs accordingly.
Duplicate or out-of-order packets are ignored using a **sequence number** comparison with wrap-around safety.

#### Radio roles

Each board is configured with:

* one **writing pipe** aimed at the other board’s address
* one **reading pipe** listening on its own address

So it’s **bidirectional**, but what we do is:

* send update → switch to TX briefly → send → return to RX listening mode
* the boards are always listening for incoming updates in the background

#### Reliability strategy

We are making use of auto-retries and acknowledgements to improve delivery probability. On a local button press, it sends the full state once, and if it fails it retries once more after a short delay. On the receiver side, old/duplicate packets are filtered using `seq` ordering, so resends don’t cause flicker or reverse state.

---

### Package format

The radio payload is a fixed-size struct:

```cpp
struct Msg {
  uint8_t sender;
  uint8_t seq;
  uint8_t ledCount;
};
```

### Field meanings

**sender**

  * Identifies which device sent the message.
  * Used as a safety check so a device ignores its own packets.

**seq**

  * Both boards increase sequence number for every received message.
  * Used to filter duplicates/out-of-order messages.
  * Wrap-around is handled: the code treats a sequence as “newer” if `(s - last) < 128`.

**ledCount**

  * Number of LEDs that are turned on.
  * If a packet is lost, the next successful packet still contains the full state and resynchronizes both devices.

### Payload size

Payload size is fixed with `radio.setPayloadSize(sizeof(Msg))`, and its 24 bits.

---

## Code brief

### Helpers 

####  (`sendState(uint8_t newState)`)

Packages up the current state into a `Msg` and transmits it to the other board. Before sending, it:

* stamps the message with our `sender` id (`DEVICE_ADDR`)
* increments our `seq` counter (so the other side can deduplicate)
* puts the full `ledCount` value into the payload

It temporarily stops listening because the RF24 can’t be in RX and TX at the same time. After the send attempt it immediately goes back to listening, so we don’t miss incoming updates.

#### `updateLocalAndBroadcast(int8_t delta)`

This is the “button press effect” function. It applies `delta` (+1 or -1) to `ledCount`, clamps the result to the valid range (0..`NUM_LEDS`), and returns early if the value didn’t actually change (e.g., you try to decrement at 0).

**WE ACTUALLY NEED TO ONLY UPDATE WHEN WE GET ACK**
It updates the local LEDs first (so the board feels responsive), and only then broadcasts the full state. The payload is the full `ledCount`, not “+1/-1”, so even if packets are lost, the next successful packet will resynchronize the other board.

If the first send fails, it waits a few milliseconds and tries once more. That’s not a full resend queue, but it’s enough to smooth out occasional transient failures.

### Button presses and actual radio calls (`handleButtonsAndSend()`)

Polls the two button inputs from the MCP23S08 and implements debouncing for each button separately.

The buttons are pulled up, so “not pressed” reads HIGH and “pressed” reads LOW. We also debounce the button press here.

A button press is detected on the falling edge, when stable state goes from HIGH to LOW. When that happens, it calls `updateLocalAndBroadcast(1)` for INC or `updateLocalAndBroadcast(-1)` for DEC.

### Receiving new state `handleReceive()`

Processes any queued incoming packets. Each packet is read into a `Msg` struct and validated lightly.

It ignores:

* packets that claim they were sent by us (`m.sender == DEVICE_ADDR`)
* packets that are duplicates or older than what we already applied, based on `seq`

If a packet passes those checks, it treats the packet’s `ledCount` as valid, clamps it to `NUM_LEDS` for safety, applies it locally via `applyLedCount()`.

This is basically the “other side changed state; mirror it here” path. The seq-based filter is what prevents flicker or reverting when retries/out-of-order deliveries happen.

### Normal test setup 
<img src="./images/setup.png" alt="Normal test setup" width="450">
In the normal close-range setup, most configurations performed very reliably when the channel was relatively “clean” (for example channel 100). At 2 Mbps with PA_LOW the link was consistently perfect (PER = 0) with essentially no retransmission effort (avgRetries ≈ 0). At 1 Mbps, the weakest retry setting (ARC = 1) produced small but repeatable loss, while slightly stronger retry budgets (typically ARC ≥ 3) removed the losses and kept PER at or near zero. A strong and consistent finding was that PA_HIGH often made performance dramatically worse (very high PER and high avgRetries), which suggests receiver saturation/desense or strong multipath effects at short distance rather than “more power = better.”

### Distance test
<img src="./images/5m_setup.png" alt="Normal test setup" width="450">
At ~5 m the same patterns remained, but the reduced link margin made weaknesses more visible. The 1 Mbps configuration with minimal retries degraded compared to the close-range setup, while moderate retry settings (ARC ≥ 3) generally restored PER back toward zero on clean channels. Channel choice still dominated: channels that overlap busy parts of the 2.4 GHz spectrum showed much higher avgRetries and, in some cases, clear packet loss. This indicates that increasing distance mainly amplifies the impact of interference and makes conservative retry settings more valuable for stability.

### Penetration test (close, with a box between)
<img src="./images/box_in_the_middle.png" alt="Normal test setup" width="450">
With a box between the radios, performance was generally closer to the normal close-range setup than to the 5 m test: the obstacle reduced signal quality, but not as strongly as increasing distance. Clean-channel configurations stayed robust, while interference-prone channels still produced high retransmissions and loss. The same PA behavior also persisted: PA_HIGH remained a poor choice and could severely reduce reliability even though an obstacle was present. Overall, the penetration test confirms that obstacles reduce margin, but channel overlap and power-related effects can still be the main drivers of performance.

### Conclusion

Across all three tests, the most reliable approach in this environment is using a clean channel, avoiding PA_HIGH, and using at least a modest retry budget (ARC ≥ 3) for 1 Mbps operation. Distance and obstacles mainly reduce margin, which makes the choice of channel and retry configuration more important, while excessive transmit power can be counterproductive.

## Assignment 2

### Packet Error Rate (Part 1)

#### Introduction

The goal of this work is to evaluate the reliability of an nRF24L01 radio link by measuring Packet Error Rate (PER) while systematically varying key PHY/MAC parameters:

* Transmit power (PA level)
* Frequency channel
* Data rate
* Auto Retransmit (ART) parameters (retransmit delay ARD and retransmit count ARC)

To do this, we have implemented a repeatable measurement setup using two STM32 Nucleo boards and the RF24 radio module. One board acts as the transmitter (TX) and drives the experiments; the other board acts as the receiver (RX) and follows the experiment configuration.

#### Problem statement

Wireless reliability of radios depends heavily on configuration and environment. For embedded use-cases that rely on the nRF24L01, it is important to understand how configuration choices affect delivery reliability.

Research questions:

* RQ1. How does data rate influence PER under the same physical conditions?
* RQ2. How does transmit power (PA level) influence PER and retry behavior?
* RQ3. How do ART settings (ARD, ARC) influence PER and the average number of retries?
* RQ4. How does frequency channel influence PER (e.g., due to interference in the 2.4 GHz band)?
* RQ5. Is there a configuration that provides the best tradeoff between reliability (low PER) and latency/throughput (low retries / lower delays)?

#### Methodology

This part is about testing link reliability under different possible configurations. Those configuration options are:

- Power Amplification (PA) - literal power of the signal
- Datarate - how much data can the communication support, bps
- Automatic Retry Delay - factor defining how much 250uS to wait between retires 
- Automatic Retry Count - the maximum amount of retries before giving up
- Channel - the communication channel inside the band

The main principle of work for the app is:

#### Measurement procedure

- TX selects a config (PA, Rate, ARD, ARC and Channel).
- Then it sends a packet containing that config information to the RX over the Control Channel (a channel and config combo that is static for both RX and TX).
- RX receives the config on the Control Channel and sends and auto-ACK.
- Only after TX has received an ACK from RX will it initiate the tests for the config selected.
- After END has been received by RX, it switches back to Control Channel to listen for more configuration packets.
- Repeat for a few different configurations

We have selected a few configurations, because testing all ~390,000 variants would not have been fun. We have a baseline configuration we are going to modify to test for different configuration values. That default config is:

- PA - Low
- Datarate - 1Mbsp
- ARD - 5
- ARC - 5
- Channel - 76

From there, we are going to vary a single value inside of the config (e.g., check all 3 PA types, check multiple different channels, check all 3 datarates etc.) and record the PER results. During the experiments both the devices will stay within one meter of one another with a clear line of sight.

#### Results

#### Conclusion

### Throughput (Part 2)

#### Introduction
The goal of this measurement is to determine the actual end-to-end throughput of the radio module in a realistic application scenario, and compare it with the configured RF data rate (250 kbps, 1 Mbps, and 2 Mbps). We transmit a fixed amount of data (10,000 × 32-bit numbers = 320,000 bits) from a transmitter to a receiver, verify that all numbers are received, and measure the total transfer time.

#### Problem statement
Even if a radio is configured for a certain physical-layer data rate (e.g., 1 Mbps), the application throughput is often much lower due to protocol overhead, acknowledgements, waiting time, and retransmissions.

**Research question:**  
How does the configured RF data rate (250 kbps / 1 Mbps / 2 Mbps) affect the achieved application throughput when transmitting 10,000 32-bit values reliably?

### Methodology 
We measured application-level throughput by reliably transmitting a fixed dataset from a transmitter (TX) to a receiver (RX), while keeping the test conditions the same so the experiment can be reproduced.
- **Total payload:** 10,000 × 32 bits = 320,000 bits (40,000 bytes)  
- **Timing window:** start when TX begins sending, stop when RX has received the final expected value  
- **Throughput calculation:** `Throughput (bps) = 320,000 bits / time (s)`

**Physical setup **
TX and RX stayed in the same environment for each test. We dit not change the Distance and antenna orientation were kept the same for all data rates.
- Note: **2.4 GHz interference** (Wi-Fi/Bluetooth) can affect retries and therefore throughput.

**Variable under test**  
In this experiment, the only parameter that was changed between runs was the RF data rate. The tests were repeated for **250 kbps**, **1 Mbps**, and **2 Mbps**, while keeping all other settings and the physical setup the same.

#### Measurement procedure
For each RF data rate (250 kbps, 1 Mbps, 2 Mbps):
1. Set `RF_RATE` to the desired value in both TX and RX sketches.
2. Upload the RX sketch to the RX board and open Serial Monitor at **115200 baud**.
3. Upload the TX sketch to the TX board and open Serial Monitor at **115200 baud**.
4. Reset both boards (or power cycle) to start a clean run.
5. Let the TX complete the full 10,000 transmissions.
6. Record the TX output:
   - “Time (s)” and “Achieved throughput (kbps)”
7. Repeat for the next RF data rate while keeping all other conditions the same.

#### Results
The achieved throughput is much lower than the configured RF data rate in all tests, and the relationship is not linear.

| Configured RF rate | Time (s)   | Achieved throughput (bps) | Achieved throughput (kbps) |
|---:|---:|---:|---:|
| 250 kbps | 102.616 | 3,118.42  | 3.12  |
| 1 Mbps   | 138.257 | 2,314.53  | 2.31  |
| 2 Mbps   | 5.742   | 55,729.71 | 55.73 |

#### Conclusion
- Throughput is orders of magnitude lower than the configured RF rate, indicating the transfer is dominated by overhead and waiting rather than raw bit rate.
- The **2 Mbps** configuration performs much better (≈55.7 kbps) than 250 kbps and 1 Mbps.
- The **1 Mbps** test is even slower than 250 kbps, which suggests the system likely spent significant time on retries/timeouts/ACK waiting, or that fixed software delays dominated that run.

# Questions answers

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

