#!/bin/bash

set -e

TIMESTAMP=`date +"%Y-%m-%d_%H-%M"`

cd ..

zip -r "matrix-advancedclock-$TIMESTAMP.zip" . -x *~ *.zip *.o *.lss *.elf */*.o */*~

