avr-gcc -mmcu=atmega8 main.c -o main.elf -Os
avr-objcopy main.elf -O ihex main.hex
