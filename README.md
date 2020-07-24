# epd-wm: Wayland window manager for IT8951 E-Paper displays.

This is a work-in-progress and doesn't do any of what's described in the paragraph below. Consider it the project goal, rather than a description of current functionality.

*Ambition:* A Wayland window manager for E-Paper displays. It exploits the priviledged position of the window manager in Wayland to bypass most of the Linux graphics stack and render directly to IT8951 displays. It's based on [Cage](https://hjdskes.nl/projects/cage) and uses [wlroots](https://github.com/swaywm/wlroots) underneath.

*Reality:* This project builds the `cage` and `epd` binaries separately. The former is the normal `cage` window manager. The latter is a demo application and library for displaying images on the EPD. It expects the display to be connected via USB. The `epd` library is an in-progress C port of some Python code I wrote to control the display. I'm trying to do this in a way which will make it easy to make a `WLR_backend` facade over the library in `epd.c`. The `epd` binary displays a demo image on the display and that's about it.


## Displays

[Waveshare](https://www.waveshare.com/) sell a line of E-Paper displays which use the IT8951 controller. These can be controlled over USB in addition to the usual SPI for Raspberry Pi's etc. I've been running and testing their [9.7 inch E-Paper Display](https://www.waveshare.com/9.7inch-e-paper-hat.htm) which can be pretty quick depending on the display mode.

## Plans
  - It would be nice if the UI part of the window manager: deciding what to do with user inputs, laying out windows etc could be done in an embedded Python programme. The C code would handle rendering, forwarding events to Python, and rendering the desktop as requested by Python.

## Getting Started
TODO

## Developing
TODO

## Release
TODO

## License
See the [LICENSE](./LICENSE) file in this repo.
