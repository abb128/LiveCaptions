# Live Captions

Linux application that provides live captioning (currently English-only)

![Screenshot of the application](https://github.com/abb128/LiveCaptions/blob/main/screenshot.png?raw=true)

Features:
* Simple interface
* Caption desktop/mic audio locally, audio is never sent anywhere
* Does not rely on any proprietary services/libraries
* Adjust font, font size, and text casing
* Optional token-level confidence text fading

Currently only PipeWire is supported because it seemed to be the only consistent way to capture desktop audio.

This application is built using [aprilasr](https://github.com/abb128/april-asr), a new library for realtime speech recognition.

## Building with GNOME Builder
You can build this easily with GNOME Builder. Make sure to download the SDK if it asks you, and click the play button to start the build.

## Building from the terminal
First you must [download ONNXRuntime v1.13.1](https://github.com/microsoft/onnxruntime/releases/download/v1.13.1/onnxruntime-linux-x64-1.13.1.tgz), extract it somewhere, and set the environment variables to point to it:
```
$ export ONNX_ROOT=/path/to/onnxruntime-linux-x64-1.13.1/
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/onnxruntime-linux-x64-1.13.1/lib
```

Alternatively you should also be able to locally build and install ONNXRuntime, in which case that step shouldn't be necessary.

To set up a build, run these commands:
```
$ meson setup builddir
$ meson devenv -C builddir
```

Now you can build the application by running `ninja`.

Before being able to run the app, you must also download the model and export `APRIL_MODEL_PATH` to where the model is. For example:
```
$ wget https://april.sapples.net/aprilv0_en-us.april
$ export APRIL_MODEL_PATH=`pwd`/aprilv0_en-us.april
```

You should now be able to run the app with `src/livecaptions`
