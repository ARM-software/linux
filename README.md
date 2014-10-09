### 1.  Release details
*  Product Release Status
      - 1.0-Juno
*  Linux Kernel BSP
      - The Linux Kernel BSP contains the upstream version 3.15-rc8 of the 
        Linux kernel modified to support the Juno development board.
*  Functionality included
      - Linux kernel configures the Juno Compute System and a limited set
        of peripherals on the Juno development board.
*  New features
      - full native resolution on HDMI connectors.
      - USB 1.1 and USB 2.0 devices fully working
      - Initial support for HMP patchset for AArch64. Please note that the patch has not been fine-tuned for Juno yet.
      - Initial support for DVFS
      - Unified defconfig for both standard Linux filesystem and Android
      - Double buffering support when paired with Mali DDK code.
*  Limitations
      - No cpuidle support at the moment. Code is present but performance has not been tuned.
*  Issues resolved since last release
      - USB keyboard issues fixed
      - Drivers for DVFS and cpufreq
      - SCPI clock framework updated to follow the new MHU driver that uses the mailbox mechanisms
*  Test cases and results
      - This release does not contain any test cases or example code other
          than those already present in the upstream version of the kernel.
*  Other information
      - Please refer to the supplied documentation.

### 2.  Installation

This is only a short summary of the steps required in order to build
the Linux kernel and the Juno device tree bindings binaries. It
assumes that the GNU C cross-compiler used is already set
in the _$PATH_ environment variable and that the name prefix for the
toolchain executables is 'aarch64-linux-gnu-'. If this is different
in your setup, please adjust the command lines accordingly.

*   Ensure clean state of the source code deliverable

    ```
$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- mrproper
    ```

*   Initial configuration of the kernel source code

    ```
$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
    ```

*   Please note that if you plan to use the kernel with an Android software stack, the provided defconfig enables all the features needed for it.

*   Kernel compilation. This step needs to be repeated after changes in the kernel source code.

    ```
$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
    ```

*   Device Tree compilation. This step needs to be repeated after changes in the arch/arm64/boot/dts/juno.dts file.

    ```
$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs
    ```

*   Deployment. The kernel binary will be stored in _arch/arm64/boot/Image_ and
        the compiled device tree bindings in _arch/arm64/boot/dts/juno.dtb_. Copy
        these two files to the SOFTWARE directory on the mounted Juno board storage.

###3.  Deploying a root filesystem

* This BSP code has been tested with **_minimal_** version of the AArch64 OpenEmbedded
      filesystem from Linaro.
* Download the filesystem from here: [14.02 minimal release](http://releases.linaro.org/14.02/openembedded/aarch64/linaro-image-minimal-genericarmv8-20140223-649.rootfs.tar.gz)
* Format a partition on the USB mass storage as ext4 filesystem. If another 
      filesystem is preferred then support needs to be enabled in the kernel.
* Mount the USB mass storage on the computer and extract the rootfs as root
      onto the formatted partition (replace */media/usb_storage* with the correct
      path to the mounted directory)

    ```
$ sudo tar zxf linaro-image-minimal-genericarmv8-20140223-649.rootfs.tar.gz -C /media/usb_storage/
    ```
* Return to the directory where you have compiled the kernel and install the built modules onto the USB mass storage partition:

    ```
$ sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=/media/usb_storage modules_install
    ```
* Unmount the USB mass storage and place it onto the Juno board in one of the
      USB ports next to the HDMI connectors.

###4.  Tools

The Linux kernel BSP depends on the availability of a GNU C cross-compiler
    that supports ARM's v8 architecture. 

###5.  Support

Any feedback is to be routed through [support@arm.com](mailto:support@arm.com).

###6.  Test

This is just a test line added
