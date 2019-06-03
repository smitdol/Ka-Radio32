python $HOME/esp/esp-idf-v3.3-beta3/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset --after hard_reset write_flash -u --flash_mode dio --flash_freq 40m --flash_size detect \
0xd000 $HOME/esp/Ka-Radio32/build/ota_data_initial.bin \
0x1000 $HOME/esp/Ka-Radio32/build/bootloader/bootloader.bin \
0x10000 $HOME/esp/Ka-Radio32/build/KaRadio32.bin \
0x1D0000 $HOME/esp/Ka-Radio32/build/KaRadio32.bin \
0x8000 $HOME/esp/Ka-Radio32/build/partitions.bin

