# CardKB

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

Connect an M5Stack CardKB to the Wio Tracker L1 Grove I2C port before boot. Zen
detects it once at startup at address `0x5F`; **Settings › Keyboard › CardKB**
shows `Found` or `Missing`.

CardKB is polled every 30 ms only while the display is awake. Polling stops
after three failed reads. Wake the display with the Tracker Back button before
typing. Restart after connecting or reconnecting the keyboard.

| Key | Action |
| --- | ------ |
| Arrows | Navigate |
| Enter | Select or operate the highlighted key |
| Fn+Enter | Submit text |
| Esc | Back or cancel |
| Backspace | Delete previous character |
| Tab | Complete a word or open the current context menu |
| Fn+M | Open the message emoji picker |

Printable characters enter text directly. The compact editor is selected
automatically while CardKB is connected; Tracker controls remain available.

CardKB shares the Grove I2C bus with sensors. Other devices need distinct
addresses and a suitable hub or splitter.
