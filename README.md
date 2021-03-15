# Firmware for the Blixt Zero

## Introduction
Someone has to write this!

## How to build

### Build environment setup

The instructions provided through this section are based on Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) and empirical usage. 

We use ``conda`` environment/package management system to create a separate Python environment rather than fiddling around with operating system's native Python environment.


* Install required packages using operating system's package manager (the following command and package names are only for Debian/Ubuntu based Linux systems).
    ```console
    $ sudo apt install --no-install-recommends git ninja-build gperf \
        ccache dfu-util device-tree-compiler wget xz-utils file make \
        gcc gcc-multilib g++-multilib libsdl2-dev
    ```
* Installing conda and create a Python environment
    * Follow the instruction in [here](https://conda.io/projects/conda/en/latest/user-guide/install/index.html) to install ``conda``.
    * Create a Python environment named ``zephyr``.
    ```console
    $ conda create -n zephyr python=3
    $ source activate zephyr
    # Now we are in zephyr conda environment
    (zephyr) $ pip install pip --upgrade
    (zephyr) $ pip install cmake tk setuptools wheel pyyaml edt elftools pyelftools pykwalify west
    ```

* Installing ARM Toolchain
    * Download the latest SDK installer for ARM.
    ```console
    (zephyr) $ wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.11.4/zephyr-toolchain-arm-0.11.4-setup.run
    ```
    * Install the toolchain into ``~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4``
    ```console
    (zephyr) $ chmod +x zephyr-toolchain-arm-0.11.4-setup.run
    (zephyr) $ mkdir -p ~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
    (zephyr) $ ./zephyr-toolchain-arm-0.11.4-setup.run -- -d ~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
    ```

    * Setting up environment variables for the conda environment. 
    
        Assuming conda is installed into ``~/software/conda`` directory.
    ```console
    (zephyr) $ mkdir -p ~/software/conda/envs/zephyr/etc/conda/activate.d
    (zephyr) $ cat << EOL >> ~/software/conda/envs/zephyr/etc/conda/activate.d/env_vars.sh
    export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
    export ZEPHYR_SDK_INSTALL_DIR=~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
    EOF

    (zephyr) $ mkdir -p ~/software/conda/envs/zephyr/etc/conda/deactivate.d
    (zephyr) $ cat << EOL >> ~/software/conda/envs/zephyr/etc/conda/activate.d/env_vars.sh
    unset ZEPHYR_TOOLCHAIN_VARIANT
    unset ZEPHYR_SDK_INSTALL_DIR
    EOL
    ```

    Deactivate and reactivate conda environment for changes to take effect and verify toolchain environment variables are set properly.

    ```console
    (zephyr) $ source deactivate
    $ source activate zephyr
    $ echo $ZEPHYR_TOOLCHAIN_VARIANT
    zehpyr
    $ echo $ZEPHYR_SDK_INSTALL_DIR
    /home/xxxx/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
    ```

### Setting up source repository.
   
Clone this repository and update other modules using ``west``.

```console
(zephyr) $ git clone git@gitlab.com:blixt/circuit-breaker/bcb-firmware-zephyr.git
(zephyr) $ cd bcb-firmware-zephyr
(zephyr) $ west update
(zephyr) $ pip install -r scripts/requirements.txt
```

### The Zero Firmware
Zero uses a bootloader (mcuboot) that enables it to receive firmware updates over Ethernet.
The bootloader also verifies the cryptographic signature of the firmware.

#### First Time Setup
* First build mcuboot and flash - the boot loader
  
    ```console
    (zephyr) $ cd zephyr-os/bootloader/mcuboot/boot/zephyr
    (zephyr) $ west build
    # To upload, use the following west command
    (zephyr) $ west flash
    # to go back to the previous directory
    (zephyr) $ cd -
    ```
* Then build the firmware, sign it, and flash it.
  Please note, this example uses the standard key. For production deployment this key **must** be replaced.

    ```console
    (zephyr) $ cd apps/zero
    (zephyr) $ west build
    # sign the firmware, here we use the standard mcuboot key
    # replace this key with the production key 
    (zephyr) $ west sign -t imgtool -- --key ../../zephyr-os/bootloader/mcuboot/root-rsa-2048.pem
    # To upload, use the following west command
    (zephyr) $ west flash
    ```

* Install mcumgr
  mcumgr is the program used to upload new firmwares to the Zero. It is written in golang. 
  Follow the instructions at https://docs.zephyrproject.org/latest/guides/device_mgmt/index.html

#### Building and Uploading a new version
* This process is almost the same as building the initial firmware version. But instead of flashing it, 
we use mcumgr to upload it. In order to do the upload the firmware to a connected Zero, you need to know the IP.
This assumes mcumgr is in your path.


    ```console
    (zephyr) $ cd apps/zero
    (zephyr) $ west build
    # sign the firmware, here we use the standard mcuboot key
    (zephyr) $ west sign -t imgtool -- --key ../../zephyr-os/bootloader/mcuboot/root-rsa-2048.pem
    # replace this key with the production key 
    # To upload, use the following west command
    (zephyr) $ mcumgr --conntype udp --connstring=[<zeros-ip>]:1337 image upload build/zephyr/zephyr.signed.bin 
    ```

#### TODO
* Find out how to populated the firmware version field so that mcuboot and mcumgr can read it.

### Creating a new App
* As the first application, we build a blink application that will blink red and green LEDs of the breaker alternatively.

    ```console
    (zephyr) $ cd apps/blinky
    (zephyr) $ west build
    # To upload, use the following west command
    (zephyr) $ west flash
    ```

* Special note on ``cmake`` when writing applications

    As this repository is an out-of-tree Zephyr repository, you need to include ``zephy.cmake`` in your project's ``CMakeLists.txt`` file.
    Use the following ``cmake`` command to include ``zephy.cmake`` file.

    ```cmake
    cmake_minimum_required(VERSION 3.13.1)
    # Use the following line before using other cmake commands.
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/zephyr.cmake")
    ```   

### Notes on Renode

#### Setup the DHCP server
* Create a bridge
    ```bridge
    ip link add name renode_bridge type bridge
    ip addr add 192.168.1.1/24 brd + dev renode_bridge
    ip link set renode_bridge up
    ```

* Add tap0 to it
    ```Add tap0 to it
    ip link set tap0 master renode_bridge
    ip link set tap0 up
    ```

* Run dnsmasq as DHCP server
    ```Run dnsmasq
    dnsmasq --no-daemon --log-queries --no-hosts --no-resolv --leasefile-ro --interface=renode_bridge  -p0 --log-dhcp --dhcp-range=192.168.1.2,192.168.1.10
    ```


