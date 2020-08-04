# epd-wm: Wayland window manager for IT8951 E-Paper displays.

A minimal Wayland window manager for (USB connected) IT8951 E-Paper
displays. The general idea is to bypass the Linux kernel and render
directly to IT8951 displays by hooking into the backend of the window
manager/compositor. It's based on
[Cage](https://hjdskes.nl/projects/cage) and uses
[wlroots](https://github.com/swaywm/wlroots) underneath.

## Displays

[Waveshare](https://www.waveshare.com/) sell a line of E-Paper
displays that are driven by an [IT8951
chip](http://www.ite.com.tw/en/product/view?mid=95).  I believe this
is a similar setup to E-Ink's [ICE driving
board](https://shopkits.eink.com/product/ice-driving-board/) Waveshare
market theirs for use with Raspberry Pi's and embedded development
boards using their SPI interface, but it also supports USB.  I've been
running and testing their [9.7 inch E-Paper
Display](https://www.waveshare.com/9.7inch-e-paper-hat.htm) which can
be pretty quick depending on the display mode. My hope is that using
USB lets us push large updates to the display faster than if we were
using a Raspberry Pi's GPIO pins (since USB is pretty high bandwidth).

## Status
This is a work-in-progress that ~~will crash before you can do anything
useful with it~~ runs but is relatively buggy.

I built this because I'd really like to get a normal, GUI-mode Emacs
running on my e-paper display. It's now at a stage where that
technically works, but is too slow to be usable (in my opinion). At
the moment, it works well for the standard, high latency applications
of e-ink. With the advantage of not having to bother writing e-ink
specific code.

Notes:

  - The `epd-wm` binary will boot up the given program, full screen on your display.
      - It currently updates at 2 Hz but I reckon we can manage closer to 10Hz when doing partial updates.
        - Keyboard inputs trigger partial updates (but not mouse movement).
      - Run `epd-wm --help` for usage information.
      - xeyes ✅
      - Xfce4 Terminal ✅. I use it as a basis to launch other programs.
      - Firefox ✅. Run `GDK_BACKEND=x11 firefox` from the xfce4-terminal.
      - GTK Emacs ✅. Run `GDK_BACKEND=x11 emacs` from the xfce4-terminal. Has issues with keyboard input (see "Tasks" below).
  - The `epd-demo` binary demonstrates the rendering modes available on the
    IT8951 controller. It also showcases the potential speed increases
    we could see with software improvements.

## Getting Started
TODO

## Developing
TODO

## Release
TODO

## Credits
Based on [Cage: A Wayland kiosk](https://github.com/Hjdskes/cage) for which Jente Hidskes is the copyright holder. Copyright notices have been preserved on individual files. Code in the `./hacks` directory has been copied from wlroots verbatim, and retains their copyright notices. I used the wlroots internals as a reference for large sections of the code inside `epd_output.c`.

## License
MIT licensed. For more information, see the [LICENSE](./LICENSE) file in this repo.
