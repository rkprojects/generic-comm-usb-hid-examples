#!/bin/bash

find . -name "*.c" -o -name "*.h" > cscope.files
find ../lpc_chip_43xx -name "*.c" -o -name "*.h" >> cscope.files
find ../lpc4357_xplorer_plusplus_board -name "*.c" -o -name "*.h" >> cscope.files
cscope -q -R -b -i cscope.files

