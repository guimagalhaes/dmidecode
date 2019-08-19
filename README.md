# dmidecode
A demonstration on how to parse and decode SMBIOS tables

It uses the /dev/mem 'method' of reading the SMBIOS tables available at special memory address.

## Compile

`gcc dmidecode.c`

## Running

Run as root user since special permission is required to read /dev/mem.
