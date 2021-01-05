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
This is a work-in-progress that runs, is slow but just about usable. It's unpolished and sure to be filled with memory leaks.

I built this because I'd really like to get a normal, GUI-mode Emacs
running on my e-paper display. It's now at a stage where that
technically works, but typing latency is still a little high. At
the moment, it works well for the standard, high latency applications
of e-ink. With the advantage of not having to bother writing e-ink
specific code.

Notes:

  - The `epd-wm` binary will boot up the given program, full screen on
    your display.
      - It currently updates at 2 Hz but I reckon we can manage closer
        to 10Hz when doing partial updates.
        - Keyboard inputs trigger partial updates (but not mouse movement).
      - Run `epd-wm` for usage information.
      - xeyes ✅
      - Xfce4 Terminal ✅.
      - Firefox ✅.
      - GTK Emacs ✅.
      - Setting `GTK_THEME=HighContrast` makes things more legible.
  - The `epd-demo` binary demonstrates the rendering modes available on the
    IT8951 controller. It also showcases the potential speed increases
    we could see with software improvements.

## Getting Started
Unfortunately, I haven't been able to get the latest version of wlroots working on my Ubuntu 19.10 install. So this project is written and built against version 0.7.0-2 of wlroots (which was the version in the Ubuntu 19.10 repos at the time).

You'll need the packages listed in `ubuntu-requirements.txt` and the epd-wm binary (which for now can be built by following the instructions under "Developing" below).

Now:
  1. Open a new TTY outside of your existing Wayland/X11 session (I do this by pressing `Ctrl+Alt+F4` on my keyboard) and login.
  2. Identify the filesystem location of your IT8951 device:
    a. With the IT8951 *unplugged* run `ls /dev/sg*`. This will list all the SCSI devices connected which aren't you're display.
    b. With the IT8951 *plugged-in via USB* run `ls /dev/sg*` again.
    c. From these two outputs, you should be able to figure out the file system address of your display. Mine is usually `/dev/sg1`.
    d. Set the `EPD_WM_DEVICE` environment variable to this value, like `EPD_WM_DEVICE=/dev/sgN`.
  3. Run `epd-demo` to test the display. A successfull test run will flash thumbs-up emoji's all over the screen.
  4. Run `epd-wm` to get the usage information for the actual window manager:
  ```
    Usage: ./build/epd-wm [OPTIONS] [--] APPLICATION

    -d	 Don't draw client side decorations, when possible
    -r	 Rotate the output 90 degrees clockwise, specify up to three times
    -D	 Turn on damage tracking debugging
    -h	 Display this help message

    Use -- when you want to pass arguments to APPLICATION
  ```
    a. For example, to run `xfce4-terminal` inside a Wayland session on the display, I would enter: `epd-wm xfce4-terminal`.


Once it's running, epd-wm itself has few features. It fullscreens the currently active application, launching a new application from the parent will fullscreen the new one, exiting that child will give fullscreen focus back to the parent, exiting all programs will exit epd-wm. You can also exit epd-wm by pressing `Alt+F4` on your keyboard. And that's about it.

Logs from epd-wm will be output to your computers TTY, whilst the EPD displays your actual windows. It would be nice to automatically turn off the laptop/desktop display on launch - let me know if you have any good ideas on how to achieve this.

### Other setups (not Ubuntu 19.10 and wlroots 0.7.0)

I'm not wholly sure how this will work elsewhere. Feel free to experiment and give me a shout if you need some help getting it set up. I'd be keen to know if anyone gets this working on other setups.

I'm pretty sure this will break on some newer versions of wlroots. My aim is to track whatever version of wlroots is in the Ubuntu repos. I'll be updating my own computers 20.10 soon and will try to update epd-wm to match whatever new version of wlroots those repos have.


## Developing

To build this project you need Docker and Docker Compose setup on your computer. If you've got those setup and working you should be able to simply run `sudo docker-compose run build` (you might not need to `sudo`).

What does this do?
  1. Build the development Docker image (defined in `Dockerfile`):
    a. Download the Ubuntu 19.10 base Docker image
    b. Install Ubuntu dependencies (both those for development and running). These are defined in `ubuntu-requirements.txt` and `ubuntu-dev-requirements.txt`.
    c. Install the meson build system from PYPI. This is listed in `python-requirements.txt`.
  2. Start the Docker image and run the commands `meson build && ninja -C build` (defined in `docker-compose.yml`).

Hopefully you can get a Docker-less version of this up and running relatively easily using these files as a reference.


## Release
TODO

## Similar Projects
  - [Seagate/it8951](https://github.com/Seagate/it8951) - Utilities for working with IT8951 controllers (incl. dumping and flashing the firmware).
  - [~martijnbraam/it8951](https://git.sr.ht/~martijnbraam/it8951) - C library & utility for usb interface.
  - [julbouln/
tinydrm_it8951](https://github.com/julbouln/tinydrm_it8951) - tinydrm driver for spi/raspberry pi interface.
  - [GregDMeyer/IT8951](https://github.com/GregDMeyer/IT8951)  - Python library for spi/raspberry pi interface.
  - [joukos / PaperTTY](https://github.com/joukos/PaperTTY) - Python library with TTY and VNC for spi/raspberry pi interface.

## Credits
Based on [Cage: A Wayland kiosk](https://github.com/Hjdskes/cage) for
which Jente Hidskes is the copyright holder. Copyright notices have
been preserved on individual files. Code in the `./hacks` directory
has been copied from wlroots verbatim, and retains their copyright
notices. I used the wlroots internals as a reference for large
sections of the code inside `epd_output.c`.

## License
MIT licensed. For more information, see the [LICENSE](./LICENSE) file in this repo.
