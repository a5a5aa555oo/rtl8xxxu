### Linux Kernel Module "rtl8xxxu"

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

### How To Use

1. If your usb wifi adapter is based on a RTL8188GU or RTL8192FU chip, you may need to

   use the command `usb_modeswitch` or `eject` to switch it to "Wifi Mode" first

2. Install gcc, make, linux-headers, dkms and other packages required to build this module

3. Build and install the module 

   * _In a traditional way_

     `make clean modules && sudo make install`

   * _In a DKMS way **(Recommended)**_

     `sudo dkms install $PWD`

   * _For Arch-based Linux distro users_

     Install the [rtl8xxxu-dkms-git](https://aur.archlinux.org/packages/rtl8xxxu-dkms-git) package from AUR.

4. Install firmware for RTL8188EU/RTL8188FU/RTL8188GU/RTL8192EU/RTL8192FU chips (Optional)

   `sudo make install_fw`

5. Load the module

   `sudo modprobe rtl8xxxu_git`

### Note

Supported linux kernel version: 5.5.x ~ 6.11.x

Tested on the following linux distros and it works fine.

* Arch Linux  (kernel version: 6.6.39-1-lts)

* Debian 11.10 (kernel version: 5.10.0-30-amd64 / 6.1.0-0.deb11.21-amd64)

* Linux Mint 20.3 (kernel version: 5.15.0-113-generic)

Thanks to all the maintainers of this kernel module!

### WARNING

No warranty, use at your own risk. :warning:
