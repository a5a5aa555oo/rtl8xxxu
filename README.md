## Linux Kernel Module "rtl8xxxu"

Driver for Realtek 802.11n USB wireless chips, which is backported from linux mainline
<details>
<summary>Click here to display a list of supported chips</summary>
<pre><code>
RTL8188CU/CUS/CTV
RTL8188EU/EUS/ETV
RTL8188FU/FTV
RTL8188GU | RTL8188RU
RTL8191CU | RTL8192CU 
RTL8192EU | RTL8192FU
RTL8723AU | RTL8723BU
</code></pre>
</details>

## Prerequisites

git, make, gcc, kernel-headers, dkms and mokutil (dkms and mokutil are optional.)

## Installation Guide

1. If your USB Wi-Fi adapter is based on a RTL8188GU or RTL8192FU chip and you see the adapter is in "Driver CDROM Mode" when running `lsusb`, you need to install `usb_modeswitch`, a tool that can switch your Wi-Fi adapter to Wi-Fi mode.

2. Create a clone of this repo in your local machine

   ```
   git clone https://github.com/a5a5aa555oo/rtl8xxxu
   ```

3. Change the working directory to `rtl8xxxu`

   ```
   cd rtl8xxxu
   ```

4. Build and install the module 

   * _via DKMS (Recommended especially Secure Boot is enabled in your system)_ 

   ```
   sudo dkms install $PWD
   ```

   * _via make_

   ```
   make clean modules && sudo make install
   ```

5. Install the firmware files necessary for the driver

   ```
   sudo make install_fw
   ```

6. Copy the configuration file `rtl8xxxu_git.conf` to /etc/modprobe.d/

   ```
   sudo cp rtl8xxxu_git.conf /etc/modprobe.d/
   ```

7. Enroll the MOK (Machine Owner Key). This is needed **ONLY IF** [Secure Boot](https://wiki.debian.org/SecureBoot) is enabled in your system. Please see [this guide](https://github.com/dell/dkms?tab=readme-ov-file#secure-boot) for details.

   ```
   sudo mokutil --import /var/lib/dkms/mok.pub
   ```

   For Ubuntu-based distro users, run this command instead.

   ```
   sudo mokutil --import /var/lib/shim-signed/mok/MOK.der
   ```

## Uninstallation Guide

For users who installed this driver via DKMS:

1. Check the version of rtl8xxxu driver installed in your system.

```
sudo dkms status rtl8xxxu
```

2. Remove the driver and its source code (Change the version accordingly)

```
sudo dkms remove rtl8xxxu/6.15 --all
```

```
sudo rm -rf /usr/src/rtl8xxxu-6.15
```

3. Delete the configuration file `rtl8xxxu_git.conf`

```
sudo rm -f /etc/modprobe.d/rtl8xxxu_git.conf
```

For users who installed this driver via make, run these commands in the rtl8xxxu source directory.

```
sudo make uninstall
```

```
sudo rm -f /etc/modprobe.d/rtl8xxxu_git.conf
```

## Note

Supported linux kernel version: 5.4.x ~ 6.17.x

Tested with RTL8192EU on the following linux distros and it works.

* Arch Linux  (kernel version: 6.12.12-1-lts / 6.6.62-1-lts)

* Debian 11.10 (kernel version: 5.10.0-33-amd64 / 6.1.0-0.deb11.21-amd64)

* Linux Mint 20.3 (kernel version: 5.4.0-216-generic / 5.15.0-139-generic)

Thanks to all the maintainers of this kernel module!

## Q&A

Q1. How to update the driver installed via `dkms`?

1. Check the version of the driver installed in your system.

```
sudo dkms status rtl8xxxu
```

2. Remove the driver and its source code (Change the version accordingly)

```
sudo dkms remove rtl8xxxu/6.15 --all
```

```
sudo rm -rf /usr/src/rtl8xxxu-6.15
```

3. Run this command in the rtl8xxxu source directory to pull the latest code

```
git pull
```

4. Build and install the driver from the latest code

```
sudo dkms install $PWD
```

Q2. How to update the driver installed via `make`?

1. Run this command in the rtl8xxxu source directory to pull the latest code

```
git pull
```

2. Rebuild and reinstall the driver from the latest code

```
make clean modules && sudo make install
```
