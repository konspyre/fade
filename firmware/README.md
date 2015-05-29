# davekey
### ATTiny85 based V-USB Media Keyboard

(***And development breakout for V-USB***)

This board has pre-defined pin breakouts for Volume Up, Down and Mute.

It is meant for use with a Cherry MX keyswitch breakboard board but can be connected to
a proper carrier board for use with other parts.

## Direct Fabrication

This board is available for direct fabrication through OSHPark.

## Bill Of Materials

## Firmware information

This firmware uses V-USB. Testing VID/PID used by this project is provided by
pid.codes under their private test pid.

http://pid.codes/1209/0001/

    VID - 0x1209
    PID - 0x0001

This is subject to change on actual public release.

## Fuses

These must be programmed, otherwise your board
will not enumerate with the host.

    avrdude -c usbtiny -p attiny85 -U lfuse:w:0xe1:m -U hfuse:w:0xdd:m

Run through: http://www.engbedded.com/fusecalc/
gives the following features:

    PLL clock
    BOD VCC 2.7v
    SPI enabled

## License

V-USB GPL. See LICENSE file.
Board files are CC0.
