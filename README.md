# OpenPico64
Nintendo 64 flash cart using a Raspberry Pi Pico / RP2040 based on [kbeckmann](https://twitter.com/kbeckmann) [PicoCart64](https://github.com/kbeckmann/PicoCart64)

## How to build an OpenPico64

### Getting the raw PCB

- Download the `openpico64.gerber.zip` file from [Releases](https://github.com/JPZV/OpenPico64/releases/latest).

- Go to a PCB manufacturing service such as [JLCPCB](https://jlcpcb.com), [OSH Park](https://oshpark.com/), [PCBWay](https://pcbway.com) or similar.

- This takes you to the PCB configuration utility where you can upload the `.zip` from above. After uploading, you should see a rendered version of both sides of the PCB. If you want to produce a cheap cart for testing you can leave all settings as they are, except for this one:

  - ⚠️ *PCB Thickness* needs to be set to *1.2mm*, otherwise the cart won't fit in your console

⚠️ Note that this will result in a PCB with 90 degrees edges - the edge connector should be sanded down to get a nice 45 degree V shape. If this is not done, it may wear down your cart connector on the N64. The proper PCB order should be ENIG-RoHS + Gold Fingers + 45°finger chamfered, however this will result in a significant cost increase.

### Sourcing the components

In order to ensambe it you will need the following parts along with the PCBs. Here we're assuming you want to assemble 5 boards, as that is the number of PCBs you'll be getting from many PCB manufacturing services as a minimum.

⚠️ Note that this project **IS NOT** compatible with the original/oem `Raspberry Pi Pico`. **YOU HAVE** to use the [WeAct clone](https://aliexpress.com/item/1005003708090298.html) because it has more exposed pins which we need. I repeat, **DON'T** use the Raspberry Branded board (the green one with a Raspberry at the bottom), it will likely fry your N64 at best. I do recommend you to buy the 8MB version or higher, as I don't know how much will grow this project.

| Qty | Name                  | Source                                                                           | 
|-----|-----------------------|----------------------------------------------------------------------------------|
| 5   | WeAct Raspberry Pi Pico | [AliExpress](https://aliexpress.com/item/1005003708090298.html)               |       |
| 5   | BSS84 MOSFET          | [750-BSS84-HF](https://www.mouser.com/ProductDetail/750-BSS84-HF)                 |       |
| 5   | 0603 100k resistor    | [71-CRCW0603100KJNEAC](https://www.mouser.com/ProductDetail/71-CRCW0603100KJNEAC) |       |
| 5   | ATTINY85-20PU    | [556-ATTINY85-20PU](https://www.mouser.com/ProductDetail/556-ATTINY85-20PU) |       |

### Preparing the CIC (ATTiny85)

You have to flash the ATTiny85 with [UltraCIC](https://github.com/ManCloud/UltraCIC-III) before assembling everything else. You can do it with the OpenPico64 board headers to an easy installation. Then, **remember** to join the JP2 jumper!  

### Assembling the board

If your WeAct Pico didn't come with the headers ready, you'll have to solder them beforehand. Then, put the Pico on the OpenPico64 board with the USB connector facing outwards and solder the headers.

Solder the MOSFET according to the pin markings.

The resistor is optional to solder, but was placed there for good measure. (There is a resistor on the Pico, so it is not strictly needed.)

### Flashing/Updating the firmware

Go to the [Releases](https://github.com/JPZV/OpenPico64/releases/latest) and download the latest .UF2 file. **Remove the OpenPico64 from your N64**, then press and hold the **BOOTSEL** button while connecting the Pico to your computer. Open your file explorer (Explorer on Windows, Fiddle on Mac, and file manager in Linux), navigate to the new Flash memory, copy and paste the .UF2, and wait until it closes.

⚠️ **DON'T** use your OpenPico64 in your N64 while it's connected to your PC or anything. The N64 works with 3.3V and your computer with 5V, so likely it'll fry your N64.

## License

The PCB is licensed under the following license: "CERN Open Hardware Licence Version 2 - Permissive" aka "CERN-OHL-P".

The software is licensed under BSD-2-Clause unless otherwise specified.
