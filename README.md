# Cardputer Uxn

It's [Uxn](https://wiki.xxiivv.com/site/uxn.html) running on an [M5Stack Card Computer](https://docs.m5stack.com/en/core/Cardputer)!

This project is still in a very experimental (but slightly more usable) state. My end goal is to be able to poke around at Uxn on the go with a little hackable handheld device, and do "self-hosted" development.

The shell is a bit rough around the edges, but should provide enough functionality to load Uxn .rom files off of the SD card's root and connect their console device inputs and outputs together with pipes (`|`). For example, `hello | upper` will pipe the console output from the "hello.rom" rom into the console input of the "upper.rom" rom, and display the output from that to the terminal.

This project is built using PlatformIO. You should be able to build and upload the project to a Card Computer by doing:

```
git clone https://github.com/TangentDelta/cardputer-uxn/
cd cardputer-uxn
pio run --target upload
```

The shell has no usability on its own and will need some .rom files on an SD card to do anything useful.