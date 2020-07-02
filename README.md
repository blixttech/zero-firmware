# Firmware for the Blixt Circuit Breaker (BCB)

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
```

### Your first application for the BCB
* As the first application, we build a blink application that will blink red and green LEDs of the breaker alternatively.

    ```console
    (zephyr) $ cd apps/blinky
    (zephyr) $ west build
    # To upload, use the following west command
    (zephyr) $ west flash
    ```

* Special note on ``cmake`` when writing applications

    As this repository is an out-of-tree Zephyr repository, you need to include ``zephy.cmake`` in your project's ``CMakeLists.txt`` file.
    Use the following ``cmake`` command to include `zephy.cmake`` file.

    ```cmake
    cmake_minimum_required(VERSION 3.13.1)
    # Use the following line before using other cmake commands.
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/zephyr.cmake")
    ```   