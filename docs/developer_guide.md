# Developer Guide

## Build environment setup

The instructions provided through this section are based on Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) and empirical usage. 

We use [``conda``](https://docs.conda.io/en/latest/index.html) environment/package management system to create a separate Python environment rather than fiddling around with operating system's native Python environment.

* Install required packages using operating system's package manager (the following command and package names are only for Debian/Ubuntu based Linux systems). Note that we will install some packages in [Setting up source repository](#setting-up-source-repository) section to get most up-to-date packages.
    ```console
    sudo apt install git wget xz-utils file make gcc g++ golang-go
    ```

* Installing conda and create a Python environment
    * Follow the instruction in [here](https://docs.conda.io/en/latest/miniconda.html) to install ``miniconda``, a minimal version of ``conda``. After the installation, you may use the following command to disable auto activation of the base environment if needed.
        ```bash
        conda config --set auto_activate_base false
        ```
    * Create a Python environment named ``zephyr``.
        ```bash
        conda create -n zephyr python=3
        conda activate zephyr
        # Now we are in zephyr conda environment. (zephyr) should be in the prompt
        pip install pip --upgrade
        ```
    * We will install required python packages in the [Setting up source repository](#setting-up-source-repository) section.

* Setting up ARM toolchain via Zephyr SDK.
    * Download the minimum SDK installer.
        ```bash
        wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.0/zephyr-sdk-0.16.0_linux-x86_64_minimal.tar.xz
        ```
    * Extract the installer into ``~/software/zephyr-sdk`` directory.
        ```bash
        mkdir -p ~/software/zephyr-sdk
        tar xvf zephyr-sdk-0.16.0_linux-x86_64_minimal.tar.xz -C ~/software/zephyr-sdk
        ```

    * Install toolchain and host tools
        ```bash
        cd ~/software/zephyr-sdk/zephyr-sdk-0.16.0
        ./setup.sh -t arm-zephyr-eabi -h
        ```

    * Setting up environment variables for the conda environment. 
    
        Assuming conda is installed into ``~/software/conda`` directory.
        ```bash
        mkdir -p ~/software/conda/envs/zephyr/etc/conda/activate.d

        cat << 'EOL' > ~/software/conda/envs/zephyr/etc/conda/activate.d/env_vars.sh
        export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
        export ZEPHYR_SDK_INSTALL_DIR=~/software/zephyr-sdk/zephyr-sdk-0.16.0
        export PATH_SAVED=$(echo $PATH)
        export PATH=$ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin:$PATH
        export PATH=$ZEPHYR_SDK_INSTALL_DIR/sysroots/x86_64-pokysdk-linux/usr/bin:$PATH
        EOL

        mkdir -p ~/software/conda/envs/zephyr/etc/conda/deactivate.d

        cat << 'EOL' > ~/software/conda/envs/zephyr/etc/conda/deactivate.d/env_vars.sh
        unset ZEPHYR_TOOLCHAIN_VARIANT
        unset ZEPHYR_SDK_INSTALL_DIR
        export PATH=$(echo $PATH_SAVED)
        unset PATH_SAVED
        EOL
        ```

    * Deactivate and reactivate conda environment for changes to take effect and verify toolchain environment variables are set properly.

        ```bash
        conda deactivate
        conda activate zephyr
        echo $ZEPHYR_TOOLCHAIN_VARIANT
        # The output should be zephyr
        echo $ZEPHYR_SDK_INSTALL_DIR
        # The output should be like /home/xxxx/software/zephyr-sdk/zephyr-sdk-0.16.0/arm-zephyr-eabi
        ```

## Setting up source repository.

*Note that the following commands should be used in the activated `conda` environment.*

* Clone this repository and change the current directory.
    ```bash
    git clone https://github.com/blixttech/zero-firmware.git
    cd zero-firmware
    ```

* Install packages using `conda`.
    ```bash
    conda install --file scripts/conda-requirements.txt
    ```

* Install required python packages with `pip`.
    ```bash
    pip install -r scripts/python-requirements.txt
    ```

* Update zephyr modules with `west`.
    ```bash
    west update
    ```

## Compiling & flashing

### Compiling

```bash
make APP=<application> BOARD=<board>
```

#### Supported applications & boards:
| **Application** | **Boards** |
|---|---|
| bootloader | zero, frdm_k64f |
| zero | zero |

### Flashing

```bash
make APP=<application> BOARD=<board> flash
```
