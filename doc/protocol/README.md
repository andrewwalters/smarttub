## Transaction format traces

These are captures of I2C traffic on the main board and best guesses at the meanings of certain bits. These were done without the remote (which is at I2C address 0x18).

Some conclusions prior to determining the remote protocol:

- Temperature control appears to be handled entirely in the control panel as there is no discernable communication of temperature up/down button presses.

- Without emulating the remote control protocol, another possible way to control the hot tub would be to splice into the 10-conductor cable going between the main board and control panel so that I2C traffic could be intercepted and modified.

  - There appears to be a signal on that cable that tells the main board when the water is at temperature or not. The control panel also reports set and actual temperature.

  - There are probably additional signal(s) that carry the analog thermistor reading.

  - Knowing the external remote control protocol, emulating it seems much easier.

See `transaction_format.txt` for format. Even though we're emulating the remote control, some of the information is useful to get detailed status, such as whether Jets 1, Jets 2, and/or Blower are on instead of just a generic 'Jets' status. `cp_signals.txt` describes what I saw when connecting a logic analyzer to the 10-conductor cable, in particular, which pins the I2C signals are on (though they are more easily accessible through the on-board header helpfully labeled "I2C Interface".
