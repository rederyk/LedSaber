#!/bin/bash
#update simply proxy for ai

pio run -t clean && pio run && ./venv/bin/python ./update_saber.py -YY ./.pio/build/esp32cam/firmware.bin 7C:9E:BD:65:BE:2E