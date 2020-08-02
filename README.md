# epd-wm: Wayland window manager for IT8951 E-Paper displays.

A minimal Wayland window manager for (USB connected) IT8951 E-Paper displays. The
general idea is to bypass most of the Linux graphics stack and render
directly to IT8951 displays by hooking into a window manager. It's
based on [Cage](https://hjdskes.nl/projects/cage) and uses 
[wlroots](https://github.com/swaywm/wlroots) underneath.

This is a work-in-progress that will crash before you can do anything
useful with it.


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

  - The `epd-demo` binary demonstrates the rendering modes available on the
    IT8951 controller.
  - The `epd-wm` binary will start up a Wayland session and render some
    amount of frames before crashing. 
      - It currently updates at 2 Hz but once the bugs are evened out I 
      reckon we can manage closer to 10Hz when doing partial updates.
      - Run `epd-wm --help` for usage information.
      - Xfce4 Terminal runs for a little while, and so does Firefox's
      profile manager. They also respond to user inputs.
      - GTK Emacs runs but shows an all grey screen. Logging from
      XWayland indicates that keyboard inputs arne't working.

Tasks:

  - [ ] Investigate why the wm crashes all the time.
     - I've narrowed the cause of the crash down to a call to `wlr_renderer_read_pixels`. 
     - I'm running an old version of wlroots (0.7.0) since that is what comes from the 
       Ubuntu repositories on my machine. I believe that version has 
       [https://github.com/swaywm/wlroots/pull/1809](a bug in  `wlr_renderer_read_pixels`). 
     - My first step is going to be to upgrade the version of wlroots this targets, and 
       see if it fixes the issue. This requires manually packaging a new version of wlroots,
       which I've been avoiding...
  - [ ] Because of the time it takes to transfer and render data on this screen, it might be
       worth doing manual, but more precise damage tracking. For example, from my experience
       it seems that Emacs marks the whole surface as damaged on all outputs. Relying on that
       reporting for refreshes would result in a pretty horrible experience.
  - [ ] What are the performance characteristics of `FAST_WRITE_MEM`?
        I reckon getting new buffers/diffs on to the it8951
        chip might end up being a considerable source of latency, so
        optimising this might be worth it. (Also: I don't have any
        control over the EPD rendering other than setting the
        `EPD_UPD_*` method, so this is the best I can do).
    - I've spent some time trying to implement `FAST_WRITE_MEM` but I'm 
      getting errors back from the controller. Presumably I've got the 
      format slightly off.
  - [x] Investigate source of issues at image border when `wait_display_ready=0`.
    - I've tried offsetting the locations we write to so that we start
      with x=10 and y=10 (we never hit the actual borders). This
      doesn't help: we get the same kinds of issues at the beginning.
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
Based on [Cage: A Wayland kiosk](https://github.com/Hjdskes/cage) for which Jente Hidskes is the copyright holder. Copyright notices have been preserved on individual files. Code in the `./hacks` directory has been copied from wlroots verbatim, and retains their copyright notices. I used the wlroots internals as a reference for large sections of the code inside `epd_output.c`.

## License
MIT licensed. For more information, see the [LICENSE](./LICENSE) file in this repo.
