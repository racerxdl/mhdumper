# MH BOOTROM Dumper

**This requires a custom firmware written on the device**

Want to easy dump a bootrom from a MH190x device? Install the firmware under `firmware`:

```bash
pio run -t upload
```

And dump the bootrom from the device:

```bash
cd cmd
go run . /dev/ttyUSB0 --readmem 0x0 131072
```

## TODOs

* [ ] Add uploader to go code
* [ ] Add pre-built firmware to go code
* [ ] Add binary releases to github
* [ ] Change firmware to run from RAM (allowing it to be used for dumping flash memory)
