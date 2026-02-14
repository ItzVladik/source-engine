#!/bin/sh
git submodule init && git submodule update

if [ ! -e "android-ndk-r10e" ]; then
	wget https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip -o /dev/null
	unzip -q android-ndk-r10e-linux-x86_64.zip
fi

export ANDROID_NDK_HOME=$PWD/android-ndk-r10e/

if [ ! -e "clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04" ]; then
	wget https://github.com/llvm/llvm-project/releases/download/llvmorg-11.1.0/clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz -o /dev/null
	tar -xf clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
fi

export PATH="$PWD/clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04/bin:$PATH"

if [ ! -e "srceng-mod-launcher" ]; then
	git clone https://github.com/ItzVladik/srceng-mod-launcher
fi

set -e

python3 waf configure -T release --build-game=entropyzero2 --prefix=srceng-mod-launcher/android --disable-warns --android=aarch64,host,21 --target=../aarch64 &&
python3 waf install --target=client,server,gamepadui,game_shader_dx9,GameUI

set +e

export ICON=icons/ez2.png
export PACKAGE=entropyzero2
export APP_NAME="Entropy: Zero 2"

cd srceng-mod-launcher

if [ ! -e "android-sdk" ]; then
	git clone https://github.com/ItzVladik/android-sdk
fi

export ANDROID_SDK_HOME=$PWD/android-sdk


git pull
chmod +x waf android/scripts/script.sh
./android/scripts/script.sh
set -e
python3 waf configure -T release --javac-source-target=8 &&
python3 waf build