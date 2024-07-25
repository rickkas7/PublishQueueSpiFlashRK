# PublishQueueSpiFlashRK

*Particle offline event queue storage on external SPI flash chips*

**Typical uses case:** When you have a large number of events you want to store offline and you
have an external SPI NOR flash chip available in your design.

This library is designed to hold a circular buffer of events in a FIFO queue stored 
in SPI NOR flash memory. It can only be used with external SPI flash chips; it cannot be used with the 
built-in SPI flash on Particle Gen 3 and Gen 4 devices. It cannot be used with SD cards.

The main advantage of this library is that it does not require a file system, like LittleFS or SPIFFS.
It takes advantage of the natural circularity of the buffer to wear level across all sectors.
It works up to the maximum event size and can pack multiple events into a single sector for efficiency.

It also is highly reliable: All events are written to flash before attempting to send, so if the device
is reset the events will be safely stored. The format of the flash is designed to be resilient in the case
of unexpected resets, even while writing.

It limited to 16-bit sector numbers within the circular buffer, so the maximum size circular buffer is 
256 Mbyte but the chip can be larger than that.

## SPI flash

It works with SPI flash chips that are compatible with [SpiFlashRK](https://github.com/rickkas7/SpiFlashRK)
including most from Winbond, Macronix, and ISSI. It supports all sizes of devices, including those 
that require 4-byte addressing (larger than 16 Mbyte). It requires 4096 byte sectors, however.

It can use any portion, divided at a sector boundary, or the entire chip.

A chip can contain multiple separate buffers if desired by instantiating multiple CircularBuffer SpiFlashRK
objects sharing a single SpiFlash object. You can also use other portions of the flash for other purposes as 
long as the other uses properly lock the SPI bus or access it using the shared SpiFlash object.

It can also be used with most other vendors of SPI NOR flash chip by creating an adapter subclass, which
can be done from your code without modifying SpIFlashRK. Note that it cannot be used with NAND flash chips
that are commonly used with SD cards.

## Example

See the example 1-simple for using this library.

### Create a SpiFlash object for your chip

You will typically create a global object for your SPI flash chip by using one of the following 
lines, possibly with a different CS pin:

```cpp
// Pick a chip, port, and CS line
// SpiFlashISSI spiFlash(SPI, A2);
// SpiFlashWinbond spiFlash(SPI, A4);
SpiFlashMacronix spiFlash(SPI, A4);
// SpiFlashWinbond spiFlash(SPI1, D5);
```

### setup

From setup, you need to call `spiFlash.begin()` and initialize the `PublishQueueSpiFlash` object:

```cpp
spiFlash.begin();

PublishQueueSpiFlashRK::instance()
    .withSpiFlash(&spiFlash, 0, 100 * 4096)
    .setup();
```

Note the `withSpiFlash` call; you use this to specify the `SpiFlash` object for your chip, and also the start 
and end addresses to use for your chip. The parameters are:

- spiFlash The SpiFlashRK object for the SPI NOR flash chip.
- addrStart Address to start at (typically 0). Must be sector aligned (multiple of 4096 bytes).
- addrEnd Address to end at (not inclusive). Must be sector aligned (multiple of 4096 bytes).

### loop

From loop(), make sure you call this, or publishing will not occur:

```cpp
PublishQueueSpiFlashRK::instance().loop();
```

While the actual publish occurs in its own thread, the processing of the queue only occurs
from the loop thread when this is called.

### publish

Instead of using `Particle.publish()` you use code that looks like this:

```cpp
PublishQueueSpiFlashRK::instance().publish("testEvent", buf, WITH_ACK);
```

You can call this whether online or offline, and the event will be queued for sending later.
It does not block, other than if the SPI flash is currently in use.