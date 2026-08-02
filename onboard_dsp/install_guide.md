


```
git clone --branch 2.3.0 https://github.com/raspberrypi/pico-sdk.git ~/.pico-sdk/sdk/2.3.0
cd ~/.pico-sdk/sdk/2.3.0
git submodule update --init
```


If you'd rather do it from CMake, use the flag the error suggests — it tells CMake to download and build the correct picotool itself:

bash
rm -rf build/
cmake -B build -DPICOTOOL_FORCE_FETCH_FROM_GIT=1

Or build picotool 2.3.0 by hand and point the SDK at it:

bash
git clone --branch 2.3.0 https://github.com/raspberrypi/picotool.git ~/picotool-src
cd ~/picotool-src && mkdir build && cd build
cmake -DPICO_SDK_PATH=~/.pico-sdk/sdk/2.3.0 ..
make