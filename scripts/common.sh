# Gemeinsame Werte fuer setup.sh und flash.sh.

SKETCH_DIR="firmware/claude_dashboard"

# Board-Optionen, die bei diesem Geraet nicht optional sind:
#   PSRAM=opi       ESP32-S3R8 = 8 MB OCTAL-PSRAM. Mit "enabled" (= QSPI)
#                   meldet der Bootloader "PSRAM chip is not connected" und
#                   das Board haengt in einer Boot-Schleife, weil der
#                   Framebuffer nicht alloziert werden kann.
#   FlashSize=16M   das Board hat 16 MB, Default waeren 4 MB
#   CDCOnBoot=cdc   sonst kommt ueber den nativen USB-Port kein Serial an
#   USBMode=hwcdc   nativer USB-Serial/JTAG des S3
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio,CPUFreq=240"

ARDUINO_LIBS="$HOME/Documents/Arduino/libraries"

# Ersten angeschlossenen ESP32 finden.
detect_port() {
  for p in /dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
    [ -e "$p" ] && { echo "$p"; return 0; }
  done
  return 1
}
