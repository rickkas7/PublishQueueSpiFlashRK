# PublishQueueSpiFlashRK

*Particle offline event queue storage on external SPI flash chips*

This library is designed to hold a circular buffer of events in a FIFO queue stored 
in SPI NOR flash memory. It can only be used with external SPI flash chips; it cannot be used with the 
built-in SPI flash on Particle Gen 3 and Gen 4 devices.

It works with SPI flash chips that are compatible with [SpiFlashRK](https://github.com/rickkas7/SpiFlashRK)
including most from Winbond, Macronix, and ISSI. It supports all sizes of devices, including those 
that require 4-byte addressing (larger than 16 Mbyte). It requires 4096 byte sectors, however.

It can use any portion, divided at a sector boundary, or the entire chip.

It limited to 16-bit sector numbers within the circular buffer, so the maximum size circular buffer is 
256 Mbyte but the chip can be larger than that.

A chip can contain multiple separate buffers if desired by instantiating multiple CircularBuffer SpiFlashRK
objects sharing a single SpiFlash object. You can also use other portions of the flash for other purposes as 
long as the other uses properly lock the SPI bus or access it using the shared SpiFlash object.

The main advantage of this library is that it does not require a file system, like LittleFS or SPIFFS.
It takes advantage of the natural circularity of the buffer to wear level across all sectors.
It works up to the maximum event size and can pack multiple events into a single sector for efficiency.

It also is highly reliable: All events are written to flash before attempting to send, so if the device
is reset the events will be safely stored. The format of the flash is designed to be resilient in the case
of unexpected resets, even while writing.





