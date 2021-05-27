# Firmware for the Blixt Zero

This is the source code for the firmware running in the Blixt Zero - a miniature solid state circuit breaker.

<img src="https://blixt.tech/wp-content/uploads/2021/01/Slider_img_BLIXT-ZERO.png" align="right"
     alt="Blixt Zero" width="120" height="178">

The firmware provides the following features currently:

    * CoAP based protocol
    * Remote Open / Close
    * Voltage and current RMS
    * Board and MCU temperatures
    * OTA software update with mcumgr
    * Custom tripping behviour
    * Auto-recovery after inrush current


# User Guide

## OTA Update
* Install mcumgr
mcumgr is a Go application that can be used to manage remote devices.
To install, run
```
go get github.com/apache/mynewt-mcumgr-cli/mcumgr
```

* Download the latest firmware
Go to the [release page](https://github.com/blixttech/zero-firmware/releases) and download the desired firmware.
The files that are needed for the update are `zero-X.X.X.signed.bin`.

* Identify the IP of the Zero to upload the firmware to.
This can be done in the Zero Controller UI.

* Upload the firmware
Replace 
- `<PATH TO MCUMGR>` with the path to the mcumgr binary
- `ZERO-IP` with the IP of the Zero
- `<PATH TO FW>` with the path to the zero firmware
```
<PATH TO MCUMGR>/mcumgr --conntype udp --connstring=[ZERO-IP]:1337 image upload <PATH TO FW>/zero-0.5.2.signed.bin
```

* List the available firmware versions on the device and get the hash of the firmware in slot 1
```
<PATH TO MCUMGR>/mcumgr --conntype udp --connstring=[ZERO-IP]:1337 image list 
```
The output of the above command should be similar to the following:

```
        Images:
        image=0 slot=0
           version: 0.5.0
           bootable: true
           flags: active confirmed
           hash: 87c260f02259996a46090420cf877612895bdc023c5f362c030870a8a7ae11c3
        image=0 slot=1
           version: 0.5.2
           bootable: true
           flags:  
           hash: fb5134a109903a63860f1e776c2acb6f468554804858189289842e8042c14147
        Split status: N/A (0)
```

* Confirm the firmware to be used by specifying the hash from slot 1
```
<PATH TO MCUMGR>/mcumgr --conntype udp --connstring=[ZERO-IP]:1337 image confirm <HASH OF SLOT 1> 
```

* Reboot the device
```
<PATH TO MCUMGR>/mcumgr --conntype udp --connstring=[ZERO-IP]:1337 reset 
```


# Developer Guide

## Build environment setup

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

## Setting up source repository.
   
Clone this repository and update other modules using ``west``.

```console
(zephyr) $ git clone git@gitlab.com:blixt/circuit-breaker/bcb-firmware-zephyr.git
(zephyr) $ cd bcb-firmware-zephyr
(zephyr) $ west update
(zephyr) $ pip install -r scripts/requirements.txt
```

## The Zero Firmware
Zero uses a bootloader (mcuboot) that enables it to receive firmware updates over Ethernet.
The bootloader also verifies the cryptographic signature of the firmware.

### First Time Setup
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

### Building and Uploading a new version
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

## Creating a new basic firmware
***** This is only for the case you would like to replace the Zero firmware alltogether **
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

## Notes on Renode

### Setup the DHCP server
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


