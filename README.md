# serial-inspector
Serial port terminal with a hexdump feature. Can be useful for embedded platform development and serial communication debugging.

## compile instructions

Compile the source code using meson (http://mesonbuild.com/) and ninja tools.
```
meson --buildtype=debugoptimized buildopt
cd buildopt
ninja
```
## usage
on general use -h option to see help.

```
  -c <cr|lf|crlf>  line end symbol(s) used to terminate data sent to serial device 
  -b BAUDRATE      baud rate
  -p PORT          serial port device
  -x               enable hex dump for received data payloads
```

Note: Installing meson on a Fedora linux:
```
dnf install meson
``` 

Example output:
```
[krystian:0:buildopt]$ sudo ./sio -ccrlf -x -p /dev/ttyUSB0
2018-02-17 23:11:52.644485: [6] (*) Staring serial port protocol debugger 1.0.1. 1 
READY
--[ payload: (    20 Bytes) ]-------------------------------------
       0    2    4    6    8    10   12   14      ASCII:
------------------------------------------------------------------
0000:  2e20 6765 6967 6572 5f6d 6574 6572 2031    . geiger_meter 1
0016:  5f30 0d0a                                  _0..            
------------------------------------------------------------------
. geiger_meter 1_0

```
