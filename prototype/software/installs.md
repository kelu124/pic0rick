sudo apt install cmake gcc-arm-none-eabi build-essential git

mkdir ~/pico
cd ~/pico
git clone -b master https://github.com/raspberrypi/pico-sdk.git
git clone -b master https://github.com/raspberrypi/pico-examples.git

cd pico-sdk
git submodule update --init



Set an environment variable so CMake can find it:

export PICO_SDK_PATH=~/pico/pico-sdk


(You can add that line to your ~/.bashrc or Windows environment variables.)