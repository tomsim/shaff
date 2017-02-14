# SHAFF

LZ77-like lossless data compression format for retro computers and small devices
with simple public domain decoders written in Assembler and 
"copy-lefted" encoder for PC represented by single C-file shaff.c

**Format SHAFF0 - DONE**

**Format SHAFF1 - DONE**

**Format SHAFF2 - EXPERIMENTAL SUPPORT**

~~~~

SHAFF v1.2 (C) 2013,2017 A.A.Shabarshin <me@shaos.net>


Usage:
    shaff [options] filename

Encoding options:
    -0 to use SHAFF0 file format (by default)
    -1 to use SHAFF1 file format
    -2 to use SHAFF2 file format (experimental)
    -b to compress blocks into separate files
    -bN to compress only block N
    -bN-M to compress blocks N..M
    -lN to limit length of matches (default value is 4 for SHAFF0 and 2 for SHAFF1/2)
    -xHH to set prefix byte other than FF (applicable only to SHAFF0 file format)
    -e to set default table for English text (applicable only to SHAFF2 file format)

Decoding options:
    -d to decode compressed SHAFF file to file
    -c to decode compressed SHAFF file to screen
~~~~

Tested on:

* GCC v4.3.2 in 32-bit Debian Linux on PowerPC G4 (big endian)
* GCC v4.7.2 in 64-bit Debian Linux on Intel Xeon
* Borland C++ 5.5.1 for Win32 in WindowsXP on Intel Core Duo

Currently available decoders:

* plain C as part of shaff.c

(c) 2013,2017 A.A.Shabarshin me@shaos.net
