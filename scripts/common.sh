# Gemeinsame Werte fuer setup.sh und flash.sh.

SKETCH_DIR="firmware/claude_deck"

# Nicht optional: PSRAM=opi (der S3R8 hat OCTAL-PSRAM; mit "enabled" = QSPI haengt
# das Board in einer Boot-Schleife), FlashSize=16M, CDCOnBoot=cdc, USBMode=hwcdc.
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio,CPUFreq=240"

ARDUINO_LIBS="$HOME/Documents/Arduino/libraries"

# Ersten angeschlossenen ESP32 finden.
detect_port() {
  for p in /dev/cu.usbmodem* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART*; do
    [ -e "$p" ] && { echo "$p"; return 0; }
  done
  return 1
}
