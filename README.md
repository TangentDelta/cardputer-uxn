# Cucumber - Cardputer Uxn

![Running the command `cat /test.txt` and seeing "Hello, World!" on a console. Running another command `cat /test.txt | upper` and seeing "HELLO, WORLD!" on a console.](/assets/images/cardputer_heol.jpg)

It's [Uxn](https://wiki.xxiivv.com/site/uxn.html) running on an [M5Stack Card Computer](https://docs.m5stack.com/en/core/Cardputer)!

This project is still in a very experimental (but ever so slightly more usable) state. My end goal is to be able to poke around at Uxn on the go with a little hackable handheld device, and do "self-hosted" development.

The shell is a bit rough around the edges, but should provide enough functionality to load Uxn .rom files off of the SD card's root and connect their console device inputs and outputs together with pipes (`|`). For example, `hello | upper` will pipe the console output from the "hello.rom" rom into the console input of the "upper.rom" rom, and display the output from that to the terminal.

## Building and Installing

This project is built using PlatformIO. You should be able to build and upload the project to a Card Computer by doing:

```
git clone https://github.com/TangentDelta/cardputer-uxn/
cd cardputer-uxn
pio run --target upload
```

The shell has no usability on its own and will need some .rom files on an SD card to do anything useful. There are a few Unix userland-style programs in the `Uxntal Programs` directory of this repo.

## Settings

When Cucumber starts up it attempts to open and read the settings from `/settings.ini` off of the SD card. At the moment the only settings stored in here are for the WiFi.

Example `settings.ini`:
```
[wifi]
ssid=my_cool_wifi_network
password=SuperSecretWiFiPassword
```

## WiFi!

New! Barely tested! Cucumber now has WiFi support, with a shiny new Uxn device to use it. Once you have your SSID and password configured in [settings.ini](#settings) (and rebooted the Cardputer), you can use the `Socket` device to establish a connection to a remote TCP socket.

```
        _______________    _______________
socket | 70 | vector*  |  | 78 | command* |
       |____|          |  |    |          |
       | 71 |          |  | 79 |          |
       |____|__________|  |____|__________|
       | 72 | success* |  | 7a |  length* |
       |____|          |  |    |          |
       | 73 |          |  | 7b |          |
       |____|__________|  |____|__________|
       | 74 |  status  |  | 7c |  read*   |
       |____|__________|  |    |          |
       | 75 |          |  | 7d |          |
       |____|__________|  |____|__________|
       | 76 |          |  | 7e |  write*  |
       |____|__________|  |    |          |
       | 77 |          |  | 7f |          |
       |____|__________|  |____|__________|
```

### Socket/command

The `Socket` device behaves nearly identically to the `File` device. Instead of using a null-terminated filename, it instead uses a null-terminated "command" string. Construct the command string in memory and write its address into the `Socket/command` port. The command string must start with the protocol to connect with (right now "TCP" is the only option), followed by a space, then the host to connect to (either an IP or a resolvable DNS name), a colon, and the port of the socket to connect to. Here are a couple examples:
```
TCP 192.168.0.115:80
TCP test.foo-bar.org:23
```
### Socket/status

The `Socket/status` contains the status of the socket connection.

|Value|Description|
|-----|-----------|
|`00`|Success/okay|
|`01`|Disconnected|
|`80`|Network configuration error|
|`81`|Error connecting to the socket|
|`82`|Error parsing the command|
|`83`|Error writing to the socket|

During the `reset` vector the port can be sampled to get the state of the host's network connection. A value of `01` indicates everything is ready to go. `80` indicates a network failure of some kind. `00` would indicate the host doesn't implement the `Socket` device.

### Socket/vector

When data is received over the open socket, `Socket/vector` is evaluated. Whatever data was reveived over the socket can be read out to a buffer using the `Socket/read` port.

### Socket/read

With the size of the buffer to read into written to `Socket/length`, writing the address of the buffer to `Socket/read` will try to read as many bytes as possible from the TCP buffer. The number of bytes successfully transfered is then presented via the `Socket/success` port.

### Socket/write

With the size of the buffer to send data from written to `Socket/length`, writing the address of the buffer to `Socket/write` will try to send as many bytes as possible over the TCP socket. The number of bytes successfully transfered is then presented via the `Socket/success` port.

## Status Indicator

Due to the fun added complexity of WiFi and the settings.ini file I needed a way to quickly check the status of these components. The quickest and least-obstructive way to implement this was to add a little status character to the upper-right corner of the terminal. For most cases the character printing is a one-shot event, but in the case of a WiFi conneciton error it'll be reprinted every 5 or so seconds.

The following is a breakdown of the status character:
|Color|Character|Meaning|
|-----|---------|-------|
|Green|`C`|Successfully connected to WiFi
|Yellow|`i`|Issue opening [settings.ini](#settings)|
|Red|`S`|Error with SD card|
|Red|`i`|Error parsing [settings.ini](#settings)|
|Red|`X`|Problem connecting to WiFi|

## Uxn Instance Sizing

Because the ESP32-S3 in the cardputer has limited RAM (only 512KB), allocating an entire 64K of memory +512  bytes of stack for every instance really limits the number of instances that can be spun up and piped together. To remedy this, I implemented an instance sizing feature similar to [D6](https://merveilles.town/@d6)'s [μxn](https://git.phial.org/d6/muxn/src/branch/main).

To keep everything simple and contained, the ROM's sizing parameters are stored in its extended [metadata](https://wiki.xxiivv.com/site/metadata.html) fields. The placeholder ID for the ROM capabilities is `0xf0`.
```
|100
@on-reset ( -> )
  ;meta #06 DEO2
  BRK

@meta 00 
  ( Body )
  ( Nothing here, so just null-terminate immediately )
  00
  ( Extended metadata )
  01 ( 1 field )
  f0 2000 ( ROM capabilities. 1024 bytes of memory, 16 bytes of stack, and no screen or expansion )
```

The value is a 16-bit word, which can be broken down into 4 4-bit nybbles:

- The most-significant nybble is the number of 256-byte pages to allocate for memory. Just like μxn it ranges from 1 (512 bytes (256 for zero-page and 256 for the program)) to 8 (the full 64K). In this example it's 2, which allocates 1024 bytes for memory. 0 is an invalid value and will result in a ROM loading error.
- The second most-significant nybble is the number of 16-byte chunks to allocate for the stacks. It ranges from 0 (16 bytes) to 4 (256 bytes). In this example it's 0, so I'm only allocating 16 bytes for the working and return stacks.
- The third most-significant nybble is the number of screen layers. 0 indicates no screen, 1 is only the background layer, etc. Since Cucumber doesn't implement the Screen device yet I have this set to 0 in this example.
- The least-significant nybble is the number of System/expansion banks. Just like with the Screen layers this is 0 since Cucumber doesn't support the expanion banks.

## Terminal Customization

The terminal's color palette and default colors can be changed in `src/terminal.h`. The `TERM_COLOR_*` defines are used to define the 16 colors in the palette (8 "normal", 8 "bright"). The default foreground and bakcground ANSI color indices can be set a little further down by the `_color_default_fg` and `_color_default_bg` member variables.