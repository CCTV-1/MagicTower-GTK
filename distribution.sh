#!/bin/bash 
set -e

if [ $OSTYPE != msys ]; then
    exit 1;
fi

if [ $# != 1 ];then
    exit 1;
fi

if [ ! -d ./build ]; then
    mkdir build
fi

if [ ! -d ./build/lib ]; then
    mkdir ./build/lib
fi

if [ -d /mingw64/lib/gstreamer-1.0 ]; then
    cp -r /mingw64/lib/gstreamer-1.0 ./build/lib
fi

if [ ! -d ./build/share ]; then
    mkdir ./build/share
fi

if [ -d /mingw64/lib/gdk-pixbuf-2.0 ]; then
    cp -r /mingw64/lib/gdk-pixbuf-2.0 ./build/lib
fi

if [ -d /mingw64/share/glib-2.0 ]; then
    cp -r /mingw64/share/glib-2.0 ./build/share
fi

if [ ! -d ./build/share/icons ]; then
    mkdir ./build/share/icons
fi

if [ -d /mingw64/share/icons/Adwaita ]; then
    cp -r /mingw64/share/icons/Adwaita ./build/share/icons
fi

if [ ! -d ./build/share/themes ]; then
    mkdir ./build/share/themes
fi

if [ -d /mingw64/share/themes/Default ]; then
    cp -r /mingw64/share/themes/Default ./build/share/themes
fi

load_info=$(strace $1);
load_info=$(grep -e "loaded .:\\\\MSYS2\\\\" <<< $load_info)
load_info=$(sort <<< $load_info)
load_info=$(awk '{print $5}' <<< $load_info)
load_info=$(sed 's/\\/\//g' <<< $load_info)
xargs -i cp {} ./build/bin/ <<< $load_info