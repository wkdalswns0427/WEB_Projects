# WEB_Projects
Web based HW communication

### TIS-PAGE
node 16.20.1
```
node app.js
```
- fetch data from mongodb database (api in this case)
- display as card on webpage

Web based hardware control project
target device : SPD-500 card terminal, Raspberry Pi Pico, Arduino Leonardo

### WEB Serial API
- support chrome based web browsers on Windows
- does **NOT** support chrome for Android

### WEB USB API
- support chrome based web browsers in any OS excluding MAC
- terminal established with Leonardo
- currently working on Pico and micropython

### androd-chrome-dev
- android chrome **does not** support webSerial
- it supports webUSB so I intend to bridge serial signal and usb signal via raspberry pi pico board
- make c compiler env, find web-serial example in raspberrypi-pico-SDK folder, replace main with mine
