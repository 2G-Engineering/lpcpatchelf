# lpcpatchelf
Patches flash checksums for LPC microcontrollers into elf binaries.

If you're developing software for the NXP LPC microcontrollers using the gnu toolchain and OpenOCD debugger you're likely familiar with the following message:

> Warn : Verification will fail since checksum in image (0x00000000) to be written to flash is different from calculated vector checksum

This message doesn't prevent you from flashing the image because OpenOCD will calculate the correct checksum for you, but it prevents you from doing a verify step.

Or you're using the mbed platform for the LPC1768 microcontrollers with the external tool-chain and you're annoyed reading:

> *****
> ***** You must modify vector checksum value in *.bin and *.hex files.
> *****

This utility is the remedy. 

# Howto:

After compiling the target code simply run:

> lpcpatchelf -f mybinary.elf

on the linked executable. This will recalculate the flash checksum. You can then either directly flash the image using OpenOCD or convert it to a raw binary using objcopy.

This utility has been tested with the LPC17xx family of microcontrollers but will likely work with other LPC families as well.

# Building:

The code just has the libelf-dev package as a dependency.

Afterwards just run make and copy the binary into a folder of your choice.
