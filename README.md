# kasy (early alpha)

This program translates your *midi keyboard* key inputs into *X11 keyboard* input. It's made just for
fun and educational purposes.

## build

You will need [libX11](https://gitlab.freedesktop.org/xorg/lib/libx11), [xorgproto](https://gitlab.freedesktop.org/xorg/proto) and [alsa-lib](https://github.com/alsa-project/alsa-lib) to be installed for **kasy** to compile.

After you checked all dependencies `cd` to project directory and run:

```console
$ make
```

to build the program, then just execute `./build/kasy --help` to see the help.
