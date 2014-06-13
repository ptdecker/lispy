#A simple bash shell script for compiling a c program

#Assumes libedit has been installed
#    sudo apt-get install libedit-dev

#!/bin/bash          
echo Compiling...
cc -std=c99 -Wall -ggdb $1.c -ledit -o $1
