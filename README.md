# Cardputer Uxn

It's [Uxn](https://wiki.xxiivv.com/site/uxn.html) running on an [M5Stack Card Computer](https://docs.m5stack.com/en/core/Cardputer)!

Currently in a very experimental state, and just able to load `.rom` files off of an inserted SD card. My end goal is to be able to poke around at Uxn on the go with a little hackable handheld device.

The CLI interface is a placeholder at the moment. There's just enough there to load a ROM file from the root of the SD card by name (no extension). I would like to eventually have a "userland" built from small Uxn programs that can be invoked at the CLI, providing programs such as `cat`, `ls`, `grep`, `>`, etc.

To make the Uxn userland work I also need to implement a way to spin up light-weight Uxn cores (somewhat akin to d6's [Muxn](https://git.phial.org/d6/muxn) project), and pipe their stdio around.
- I've started working on this. The Uxn instances now allocate RAM and stack space form the heap upon creation, with some preliminary logic to support smaller instances.