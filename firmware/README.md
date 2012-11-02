USB MIDI Link
-------------

Connects two computers with USB MIDI protocol.
This project is based on [MICO-MOCO project][micomoco].
[micomoco]: http://morecatlab.akiba.coocan.jp/morecat_lab/MOCO.html

Hardware and circuit
--------------------
Please see [circuit directory][circuit] for details.
[circuit]: https://github.com/kshoji/USB-MIDI-Link/circuit

Compile
-------
My environments:

- CrossPack-AVR-20100115
- Eclipse CDT
- AVR Eclipse Plugin (2.3.4.20100807PRD)
 - The latest version of the plugin doesn't work with my environment.

Select MCU Type: ATtiny2313, and Clock Frequency 16000000.
Compile with Release target(with Size optimization).

FUSE Setting
------------
FUSEH = 0xcf
FUSEL = 0xef

```
 avrdude -P /dev/tty.usbserial-XXXXXXX -c arduino -p t2313 -b 19200 -U lfuse:w:0xef:m -U hfuse:w:0xcf:m -U efuse:w:0xff:m
```

License
-------
[GNU General Public License Version 2 (GPL)][GPL2]
[GPL2]: http://www.gnu.org/licenses/gpl-2.0.html