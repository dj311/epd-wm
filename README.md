# epd-wm: Wayland window manager for IT8951 E-Paper displays.

This is a work-in-progress and doesn't do any of what's described in
the paragraph below. Consider it the project goal, rather than a
description of current functionality.

*Ambition:* A Wayland window manager for E-Paper displays. It exploits
the priviledged position of the window manager in Wayland to bypass
most of the Linux graphics stack and render directly to IT8951
displays. It's based on [Cage](https://hjdskes.nl/projects/cage) and
uses [wlroots](https://github.com/swaywm/wlroots) underneath.

*Reality:* This project builds the `cage` and `epd` binaries
separately. The former is the normal `cage` window manager. The latter
is a demo application and library for displaying images on the EPD. It
expects the display to be connected via USB (and available as a SCSI
generic device at `/dev/sg1`). The `epd` library is an in-progress C
port of some Python code I wrote to control the display. I'm trying to
do this in a way which will make it easy to make a `WLR_backend`
facade over the library in `epd.c`. The `epd` binary displays a demo
image on the display and that's about it.


## Displays

[Waveshare](https://www.waveshare.com/) sell a line of E-Paper
displays that are driven by an 
[IT8951 chip](http://www.ite.com.tw/en/product/view?mid=95). 
I believe this is a similar setup to E-Ink's 
[ICE driving board](https://shopkits.eink.com/product/ice-driving-board/)
Waveshare market theirs for use with Raspberry Pi's and embedded
development boards using their SPI interface, but it also supports USB.
I've been running and testing their
[9.7 inch E-Paper Display](https://www.waveshare.com/9.7inch-e-paper-hat.htm)
which can be pretty quick depending on the display mode. My hope is
that using USB lets us push large updates to the display faster than if
we were using a Raspberry Pi's GPIO pins (since USB is pretty high
bandwidth).

## Project
Status:

  - I've got super fast 8 bit drawing going (when I set
    `wait_display_ready` to 0) but it is a bit buggy near the
    borders. It would be good to find out why since it is considerably
    faster than with `wait_display_ready=1`.
  - ~~The main issue is the same as with Python: every 10-15
    `LD_IMG_AREA` calls hang for a few seconds at a time.~~ 
    This is now fixed, see commit [7d362f5f](https://github.com/dj311/epd-wm/commit/7d362f5f686b1d6541910843c60f21bc284532e2).
  - Nothing is hooked into Cage/wl-roots yet.

TODO:

  - [ ] Investigate source of issues at image border when `wait_display_ready=0`.
    - I've tried offsetting the locations we write to so that we start
      with x=10 and y=10 (we never hit the actual borders). This
      doesn't help: we get the same kinds of issues at the beginning.
  - [ ] Does the `FAST_WRITE_MEM` operation avoid hanging like
        `LD_IMG_AREA`? What are it's performance characteristics in
        general? I reckon getting new buffers/diffs on to the it8951
        chip might end up being a considerable source of latency, so
        optimising this might be worth it. (Also: I don't have any
        control over the EPD rendering other than setting the
        `EPD_UPD_*` method, so this is the best I can do).
    - I've spent some time trying to implement `FAST_WRITE_MEM` but I'm 
      getting errors back from the controller. Presumably I've got the 
      format slightly off.
  - [x] Is the hanging caused by my Linux box? Is ther a priority or
        method I should be making these SCSI calls with that won't
        block?
    - This is now fixed, see commit [7d362f5f](https://github.com/dj311/epd-wm/commit/7d362f5f686b1d6541910843c60f21bc284532e2).
      

Plans:

  - It would be nice if the UI part of the window manager: deciding
    what to do with user inputs, laying out windows etc could be done
    in an embedded Python programme. The C code would handle
    rendering, forwarding events to Python, and rendering the desktop
    as requested by Python.

## Getting Started
TODO

## Developing
TODO

## Release
TODO

## Credits
Based on [Cage: A Wayland kiosk](https://github.com/Hjdskes/cage) for which Jente Hidskes owns the copyright. Copyright notices have been preserved on individual files.

## License
MIT licensed. For more information, see the [LICENSE](./LICENSE) file in this repo.
