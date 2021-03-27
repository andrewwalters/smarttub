## ESP/Arduino control of IQ2020-based hot tubs

Hardware design detail and docs to come. Code is in very rough shape currently but at least shows proof of concept.

Directories:

- EspServer - ESP8266 webserver serving very simple HTML 1.0 style page that allows for basic control of hot tub. Also fetches internet time and automatically adjusts temperature according to a hardcoded schedule.
- NanoTubAt - Arduino sketch running on an ATMEGA168 that handles low-level I2C communication with the hot tub. Talks to ESP8266 over serial port and uses simple AT-style command set.
- common - Common defines between EspServer and NanoTubAt
- smarttub-app - Beginnings of a more fully-featured React app to control hot tub.