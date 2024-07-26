# z80pack on ESP32-2432S028R a.k.a. CYD (Cheap Yellow Display)

To build z80pack for this device you need to have the SDK for ESP32-based
devices installed and configured. The SDK manual has detailed instructions
how to install on all major PC platforms, it is available here:
[ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)

Then clone the GitHub repositories:

1. clone z80pack: git clone https://github.com/udo-munk/z80pack.git
2. checkout dev branch: cd z80pack; git checkout dev; cd ..
3. clone this: git clone https://github.com/sneakywumpus/cyd-80.git

To build the application:
```
cd cyd-80
idf.py build
```

Flash into the device with `idf.py flash`, and then prepare a MicroSD card.

In the root directory of the card create these directories:
```
CONF80
CODE80
DISKS80
```

Into the CODE80 directory copy all the .bin files from src-examples.
Into the DISKS80 directory copy the disk images from disks.
