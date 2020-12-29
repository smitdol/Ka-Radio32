python $HOME/projects/esp/esp-idf/components/esptool_py/esptool/esptool.py \
--chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset \
--after hard_reset write_flash -u --flash_mode dio --flash_freq 40m --flash_size detect \
0x1000 $HOME/projects/esp/Ka-Radio32/binaries/bootloader.bin \
0x10000 $HOME/projects/esp/Ka-Radio32/build/KaRadio32.bin \
0x8000 $HOME/projects/esp/Ka-Radio32/binaries/partitions.bin
#0x1D0000 $HOME/projects/esp/Ka-Radio32/build/KaRadio32.bin \
#0xd000 $HOME/projects/esp/Ka-Radio32-master/build/ota_data_initial.bin \

