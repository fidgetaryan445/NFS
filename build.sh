#!/bin/bash
gcc mkfs.c -o mkfs
./mkfs -f hello_world.fs
gcc try_code.c -o try 
clear
./try hello_world.fs
