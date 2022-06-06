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

Over-the-air (OTA) updates to the Zero firmware is done via an external tool, [mcumgr](https://github.com/apache/mynewt-mcumgr).

* Install mcumgr.
    ```console
    go install github.com/apache/mynewt-mcumgr-cli/mcumgr@latest
    export PATH=$PATH:$HOME/go/bin
    ```

* Download the desired firmware from the [release page](https://github.com/blixttech/zero-firmware/releases) or compile the firmware as described in the [Developer Guide](#developer-guide).
    The file that is needed for the update is in `zero-X.X.X.signed.bin` format such that `X.X.X` indicates the version number.

* Identify the IP address of the Zero device that needs the firmware update.
    This can be done in the Zero Controller UI.

* Uploading the firmware. Replace the following fields as needed.
    - `<ZERO-IP>` with the IP of the Zero device.
    - `<PATH TO FW>` with the path to the downloaded firmware file.
    ```console
    mcumgr --conntype udp --connstring=[<ZERO-IP>]:1337 image upload <PATH TO FW>
    ```

* List the available firmware versions on the device to get the hash of the newly uploaded firmware.
    ```console
    mcumgr --conntype udp --connstring=[<ZERO-IP>]:1337 image list 
    ```
    The output of the above command should be similar to the following:

    ```
        images:
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
        split status: n/a (0)
    ```

    The current firmware image has `active confirmed` in its flags. Therefore, copy the hash of the other firmware image.

* Confirm the firmware image to be used by specifying the hash.
  Replace the `<IMAGE HASH>` with the hash copied from the previous step.
    ```console
    mcumgr --conntype udp --connstring=[<ZERO-IP>]:1337 image confirm <IMAGE HASH> 
    ```

* Reboot the device
    ```console
    mcumgr --conntype udp --connstring=[<ZERO-IP>]:1337 reset 
    ```


# Developer Guide

## Build environment setup

The instructions provided through this section are based on Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) and empirical usage. 

We use ``conda`` environment/package management system to create a separate Python environment rather than fiddling around with operating system's native Python environment.


* Install required packages using operating system's package manager (the following command and package names are only for Debian/Ubuntu based Linux systems).
    ```console
    sudo apt install --no-install-recommends git ninja-build gperf \
        ccache dfu-util device-tree-compiler wget xz-utils file make \
        gcc gcc-multilib g++-multilib libsdl2-dev golang-go
    ```
* Installing conda and create a Python environment
    * Follow the instruction in [here](https://conda.io/projects/conda/en/latest/user-guide/install/index.html) to install ``conda``.
    * Create a Python environment named ``zephyr``.
    * We will install required python packages in the [Setting up source repository](#setting-up-source-repository) section.
    ```console
    conda create -n zephyr python=3
    source activate zephyr
    # Now we are in zephyr conda environment. (zephyr) should be in the prompt
    pip install pip --upgrade
    ```

* Setting up ARM Toolchain
    * Download the latest SDK installer for ARM.
        ```console
        wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.11.4/zephyr-toolchain-arm-0.11.4-setup.run
        ```
    * Install the toolchain into ``~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4``
        ```console
        chmod +x zephyr-toolchain-arm-0.11.4-setup.run
        mkdir -p ~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
        ./zephyr-toolchain-arm-0.11.4-setup.run -- -d ~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
        ```

    * Setting up environment variables for the conda environment. 
    
        Assuming conda is installed into ``~/software/conda`` directory.
        ```console
        mkdir -p ~/software/conda/envs/zephyr/etc/conda/activate.d
        cat << EOL >> ~/software/conda/envs/zephyr/etc/conda/activate.d/env_vars.sh
        export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
        export ZEPHYR_SDK_INSTALL_DIR=~/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
        EOF

        mkdir -p ~/software/conda/envs/zephyr/etc/conda/deactivate.d
        cat << EOL >> ~/software/conda/envs/zephyr/etc/conda/activate.d/env_vars.sh
        unset ZEPHYR_TOOLCHAIN_VARIANT
        unset ZEPHYR_SDK_INSTALL_DIR
        EOL
        ```

    * Deactivate and reactivate conda environment for changes to take effect and verify toolchain environment variables are set properly.

        ```console
        source deactivate
        source activate zephyr
        echo $ZEPHYR_TOOLCHAIN_VARIANT
        # THe output should be zehpyr
        echo $ZEPHYR_SDK_INSTALL_DIR
        # The output should be like /home/xxxx/software/zephyr-sdk/arm/zephyr-sdk-0.11.4
        ```

## Setting up source repository.
Clone this repository and update other modules using ``west``.
*Note that the following commands should be used in the activated conda environment.*

```console
git clone git@github.com:blixttech/zero-firmware.git
cd zero-firmware
pip install -r python-requirements.txt
west update
```

## Compiling and Flashing Zero Firmware
Zero firmware uses [MCUboot](https://www.mcuboot.com/) which is a secure bootloader supported by Zephyr RTOS in the device firmware upgrading process.
Therefore, MCUboot has to be flashed first prior to flashing the Zero firmware.
*Note that the following commands should be used in the activated conda environment.*

### Flashing MCUboot
In the root directory for the repository with activated conda environment

```console
make mcuboot
make mcuboot-flash
```

### Flashing Zero Firmware
In the root directory for the repository with activated conda environment.

```console
make zero
make zero-flash
```

### Updating Zero Firmware via OTA
Use the following command to gather the firmware images into `<repository root>/build` directory.

```console
make binaries
```

Refer to the [OTA Update](#ota-update) guide.

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


