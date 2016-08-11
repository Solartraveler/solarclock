#!/bin/bash

set -e

cd ..

make

cd linux-gui

make

cd ../testcases

make

echo "Running test cases..."

./runtestcases
