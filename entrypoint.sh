#!/bin/bash

meson setup builddir --reconfigure
ninja -C builddir