// SPDX-License-Identifier: GPL-2.0-only
/*
 * RTL8XXXU mac80211 USB driver - 8188s/8191s/8192s specific subdriver
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/wireless.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <net/mac80211.h>
#include "rtl8xxxu.h"
#include "rtl8xxxu_regs.h"

/* 8-bytes alignment required */
struct rtl8192su_firmware_priv
{
	/* --- long word 0 ---- */
	/* 0x12: CE product, 0x92: IT product */
	u8 signature_0;
	/* 0x87: CE product, 0x81: IT product */
	u8 signature_1;
	/* 0x81: PCI-AP, 01:PCIe, 02: 92S-U,
	 * 0x82: USB-AP, 0x12: 72S-U, 03:SDIO */
	u8 hci_sel;
	/* the same value as register value  */
	u8 chip_version;
	/* customer  ID low byte */
	u8 customer_id_0;
	/* customer  ID high byte */
	u8 customer_id_1;
	/* 0x11:  1T1R, 0x12: 1T2R,
	 * 0x92: 1T2R turbo, 0x22: 2T2R */
	u8 rf_config;
	/* 4: 4EP, 6: 6EP, 11: 11EP */
	u8 usb_ep_num;

	/* --- long word 1 ---- */
	/* regulatory class bit map 0 */
	u8 regulatory_class_0;
	/* regulatory class bit map 1 */
	u8 regulatory_class_1;
	/* regulatory class bit map 2 */
	u8 regulatory_class_2;
	/* regulatory class bit map 3 */
	u8 regulatory_class_3;
	/* 0:SWSI, 1:HWSI, 2:HWPI */
	u8 rfintfs;
	u8 def_nettype;
	u8 turbo_mode;
	u8 low_power_mode;

	/* --- long word 2 ---- */
	/* 0x00: normal, 0x03: MACLBK, 0x01: PHYLBK */
	u8 lbk_mode;
	/* 1: for MP use, 0: for normal
	 * driver (to be discussed) */
	u8 mp_mode;
	/* 0: off, 1: on, 2: auto */
	u8 vcs_type;
	/* 0: none, 1: RTS/CTS, 2: CTS to self */
	u8 vcs_mode;
	u8 rsvd022;
	u8 rsvd023;
	u8 rsvd024;
	u8 rsvd025;

	/* --- long word 3 ---- */
	/* QoS enable */
	u8 qos_en;
	/* 40MHz BW enable */
	/* 4181 convert AMSDU to AMPDU, 0: disable */
	u8 bw_40mhz_en;
	u8 amsdu2ampdu_en;
	/* 11n AMPDU enable */
	u8 ampdu_en;
	/* FW offloads, 0: driver handles */
	u8 rate_control_offload;
	/* FW offloads, 0: driver handles */
	u8 aggregation_offload;
	u8 rsvd030;
	u8 rsvd031;

	/* --- long word 4 ---- */
	/* 1. FW offloads, 0: driver handles */
	u8 beacon_offload;
	/* 2. FW offloads, 0: driver handles */
	u8 mlme_offload;
	/* 3. FW offloads, 0: driver handles */
	u8 hwpc_offload;
	/* 4. FW offloads, 0: driver handles */
	u8 tcp_checksum_offload;
	/* 5. FW offloads, 0: driver handles */
	u8 tcp_offload;
	/* 6. FW offloads, 0: driver handles */
	u8 ps_control_offload;
	/* 7. FW offloads, 0: driver handles */
	u8 wwlan_offload;
	u8 rsvd040;

	/* --- long word 5 ---- */
	/* tcp tx packet length low byte */
	u8 tcp_tx_frame_len_l;
	/* tcp tx packet length high byte */
	u8 tcp_tx_frame_len_h;
	/* tcp rx packet length low byte */
	u8 tcp_rx_frame_len_l;
	/* tcp rx packet length high byte */
	u8 tcp_rx_frame_len_h;
	u8 rsvd050;
	u8 rsvd051;
	u8 rsvd052;
	u8 rsvd053;
} __packed;

#define R8712SU_FW_SIGNATURE (0x8712)
#define R8192SU_FW_SIGNATURE (0x8192)

/* 8-byte alinment required */
struct rtl8192su_firmware_header
{
	__le16 signature;

	/* 0x8000 ~ 0x8FFF for FPGA version,
	 * 0x0000 ~ 0x7FFF for ASIC version */
	__le16 version;

	/* define the size of boot loader */
	__le32 dmem_size;

	/* define the size of FW in IMEM */
	__le32 img_imem_size;
	/* define the size of FW in SRAM */
	__le32 img_sram_size;

	/* define the size of DMEM variable */
	__le32 fw_priv_size;
	__le16 efuse_addr;
	__le16 h2ccmd_resp_addr;

	__le32 svn_evision;
	__le32 release_time;

	struct rtl8192su_firmware_priv fw_priv;
	u8 data[];
} __packed;

static struct rtl8xxxu_power_base rtl8192s_power_base = {
	.reg_0e00 = 0x00000000,
	.reg_0e04 = 0x00000000,
	.reg_0e08 = 0x00000000,
	.reg_086c = 0x00000000,

	.reg_0e10 = 0x00000000,
	.reg_0e14 = 0x00000000,
	.reg_0e18 = 0x00000000,
	.reg_0e1c = 0x00000000,

	.reg_0830 = 0x00000000,
	.reg_0834 = 0x00000000,
	.reg_0838 = 0x00000000,
	.reg_086c_2 = 0x00000000,

	.reg_083c = 0x00000000,
	.reg_0848 = 0x00000000,
	.reg_084c = 0x00000000,
	.reg_0868 = 0x00000000,
};


static const struct rtl8xxxu_reg8val rtl8192su_mac_init_table[] = {
{0x020,0x00000035},
{0x048,0x0000000e},
{0x049,0x000000f0},
{0x04a,0x00000077},
{0x04b,0x00000083},
{0x0b5,0x00000021},
{0x0dc,0x000000ff},
{0x0dd,0x000000ff},
{0x0de,0x000000ff},
{0x0df,0x000000ff},
{0x116,0x00000000},
{0x117,0x00000000},
{0x118,0x00000000},
{0x119,0x00000000},
{0x11a,0x00000000},
{0x11b,0x00000000},
{0x11c,0x00000000},
{0x11d,0x00000000},
{0x160,0x0000000b},
{0x161,0x0000000b},
{0x162,0x0000000b},
{0x163,0x0000000b},
{0x164,0x0000000b},
{0x165,0x0000000b},
{0x166,0x0000000b},
{0x167,0x0000000b},
{0x168,0x0000000b},
{0x169,0x0000000b},
{0x16a,0x0000000b},
{0x16b,0x0000000b},
{0x16c,0x0000000b},
{0x16d,0x0000000b},
{0x16e,0x0000000b},
{0x16f,0x0000000b},
{0x170,0x0000000b},
{0x171,0x0000000b},
{0x172,0x0000000b},
{0x173,0x0000000b},
{0x174,0x0000000b},
{0x175,0x0000000b},
{0x176,0x0000000b},
{0x177,0x0000000b},
{0x178,0x0000000b},
{0x179,0x0000000b},
{0x17a,0x0000000b},
{0x17b,0x0000000b},
{0x17c,0x0000000b},
{0x17d,0x0000000b},
{0x17e,0x0000000b},
{0x17f,0x0000000b},
{0x236,0x0000000c},
{0x503,0x00000022},
{0x560,0x00000009},
{0xFFFF, 0xFF},
};


const struct rtl8xxxu_reg32val rtl8192su_phy_2t2r_init_table[] = {
{0x01c,0x07000000},
{0x800,0x00040000},
{0x804,0x00008003},
{0x808,0x0000fc00},
{0x80c,0x0000000a},
{0x810,0x10005088},
{0x814,0x020c3d10},
{0x818,0x00200185},
{0x81c,0x00000000},
{0x820,0x01000000},
{0x824,0x00390004},
{0x828,0x01000000},
{0x82c,0x00390004},
{0x830,0x00000004},
{0x834,0x00690200},
{0x838,0x00000004},
{0x83c,0x00690200},
{0x840,0x00010000},
{0x844,0x00010000},
{0x848,0x00000000},
{0x84c,0x00000000},
{0x850,0x00000000},
{0x854,0x00000000},
{0x858,0x48484848},
{0x85c,0x65a965a9},
{0x860,0x0f7f0130},
{0x864,0x0f7f0130},
{0x868,0x0f7f0130},
{0x86c,0x0f7f0130},
{0x870,0x03000700},
{0x874,0x03000300},
{0x878,0x00020002},
{0x87c,0x004f0201},
{0x880,0xa8300ac1},
{0x884,0x00000058},
{0x888,0x00000008},
{0x88c,0x00000004},
{0x890,0x00000000},
{0x894,0xfffffffe},
{0x898,0x40302010},
{0x89c,0x00706050},
{0x8b0,0x00000000},
{0x8e0,0x00000000},
{0x8e4,0x00000000},
{0xe00,0x30333333},
{0xe04,0x2a2d2e2f},
{0xe08,0x00003232},
{0xe10,0x30333333},
{0xe14,0x2a2d2e2f},
{0xe18,0x30333333},
{0xe1c,0x2a2d2e2f},
{0xe30,0x01007c00},
{0xe34,0x01004800},
{0xe38,0x1000dc1f},
{0xe3c,0x10008c1f},
{0xe40,0x021400a0},
{0xe44,0x281600a0},
{0xe48,0xf8000001},
{0xe4c,0x00002910},
{0xe50,0x01007c00},
{0xe54,0x01004800},
{0xe58,0x1000dc1f},
{0xe5c,0x10008c1f},
{0xe60,0x021400a0},
{0xe64,0x281600a0},
{0xe6c,0x00002910},
{0xe70,0x31ed92fb},
{0xe74,0x361536fb},
{0xe78,0x361536fb},
{0xe7c,0x361536fb},
{0xe80,0x361536fb},
{0xe84,0x000d92fb},
{0xe88,0x000d92fb},
{0xe8c,0x31ed92fb},
{0xed0,0x31ed92fb},
{0xed4,0x31ed92fb},
{0xed8,0x000d92fb},
{0xedc,0x000d92fb},
{0xee0,0x000d92fb},
{0xee4,0x015e5448},
{0xee8,0x21555448},
{0x900,0x00000000},
{0x904,0x00000023},
{0x908,0x00000000},
{0x90c,0x01121313},
{0xa00,0x00d047c8},
{0xa04,0x80ff0008},
{0xa08,0x8ccd8300},
{0xa0c,0x2e62120f},
{0xa10,0x9500bb78},
{0xa14,0x11144028},
{0xa18,0x00881117},
{0xa1c,0x89140f00},
{0xa20,0x1a1b0000},
{0xa24,0x090e1317},
{0xa28,0x00000204},
{0xa2c,0x10d30000},
{0xc00,0x40071d40},
{0xc04,0x00a05633},
{0xc08,0x000000e4},
{0xc0c,0x6c6c6c6c},
{0xc10,0x08800000},
{0xc14,0x40000100},
{0xc18,0x08000000},
{0xc1c,0x40000100},
{0xc20,0x08000000},
{0xc24,0x40000100},
{0xc28,0x08000000},
{0xc2c,0x40000100},
{0xc30,0x6de9ac44},
{0xc34,0x469652cf},
{0xc38,0x49795994},
{0xc3c,0x0a979764},
{0xc40,0x1f7c403f},
{0xc44,0x000100b7},
{0xc48,0xec020000},
{0xc4c,0x007f037f},
{0xc50,0x69543420},
{0xc54,0x433c0094},
{0xc58,0x69543420},
{0xc5c,0x433c0094},
{0xc60,0x69543420},
{0xc64,0x433c0094},
{0xc68,0x69543420},
{0xc6c,0x433c0094},
{0xc70,0x2c7f000d},
{0xc74,0x0186175b},
{0xc78,0x0000001f},
{0xc7c,0x00b91612},
{0xc80,0x40000100},
{0xc84,0x20f60000},
{0xc88,0x20000080},
{0xc8c,0x20200000},
{0xc90,0x40000100},
{0xc94,0x00000000},
{0xc98,0x40000100},
{0xc9c,0x00000000},
{0xca0,0x00492492},
{0xca4,0x00000000},
{0xca8,0x00000000},
{0xcac,0x00000000},
{0xcb0,0x00000000},
{0xcb4,0x00000000},
{0xcb8,0x00000000},
{0xcbc,0x28000000},
{0xcc0,0x00000000},
{0xcc4,0x00000000},
{0xcc8,0x00000000},
{0xccc,0x00000000},
{0xcd0,0x00000000},
{0xcd4,0x00000000},
{0xcd8,0x64b22427},
{0xcdc,0x00766932},
{0xce0,0x00222222},
{0xce4,0x00000000},
{0xce8,0x37644302},
{0xcec,0x2f97d40c},
{0xd00,0x00000750},
{0xd04,0x00000403},
{0xd08,0x0000907f},
{0xd0c,0x00000001},
{0xd10,0xa0633333},
{0xd14,0x33333c63},
{0xd18,0x6a8f5b6b},
{0xd1c,0x00000000},
{0xd20,0x00000000},
{0xd24,0x00000000},
{0xd28,0x00000000},
{0xd2c,0xcc979975},
{0xd30,0x00000000},
{0xd34,0x00000000},
{0xd38,0x00000000},
{0xd3c,0x00027293},
{0xd40,0x00000000},
{0xd44,0x00000000},
{0xd48,0x00000000},
{0xd50,0x6437140a},
{0xd54,0x024dbd02},
{0xd58,0x00000000},
{0xd5c,0x30032064},
{0xd60,0x4653de68},
{0xd64,0x00518a3c},
{0xd68,0x00002101},
{0xf14,0x00000003},
{0xf4c,0x00000000},
{0xf00,0x00000300},
{0xFFFF, 0xFFFFFFFF},
};

const struct rtl8xxxu_reg32val rtl8192su_agc_table[] = {
{0xc78,0x7f000001},
{0xc78,0x7f010001},
{0xc78,0x7e020001},
{0xc78,0x7d030001},
{0xc78,0x7c040001},
{0xc78,0x7b050001},
{0xc78,0x7a060001},
{0xc78,0x79070001},
{0xc78,0x78080001},
{0xc78,0x77090001},
{0xc78,0x760a0001},
{0xc78,0x750b0001},
{0xc78,0x740c0001},
{0xc78,0x730d0001},
{0xc78,0x720e0001},
{0xc78,0x710f0001},
{0xc78,0x70100001},
{0xc78,0x6f110001},
{0xc78,0x6f120001},
{0xc78,0x6e130001},
{0xc78,0x6d140001},
{0xc78,0x6d150001},
{0xc78,0x6c160001},
{0xc78,0x6b170001},
{0xc78,0x6a180001},
{0xc78,0x6a190001},
{0xc78,0x691a0001},
{0xc78,0x681b0001},
{0xc78,0x671c0001},
{0xc78,0x661d0001},
{0xc78,0x651e0001},
{0xc78,0x641f0001},
{0xc78,0x63200001},
{0xc78,0x4c210001},
{0xc78,0x4b220001},
{0xc78,0x4a230001},
{0xc78,0x49240001},
{0xc78,0x48250001},
{0xc78,0x47260001},
{0xc78,0x46270001},
{0xc78,0x45280001},
{0xc78,0x44290001},
{0xc78,0x2c2a0001},
{0xc78,0x2b2b0001},
{0xc78,0x2a2c0001},
{0xc78,0x292d0001},
{0xc78,0x282e0001},
{0xc78,0x272f0001},
{0xc78,0x26300001},
{0xc78,0x25310001},
{0xc78,0x24320001},
{0xc78,0x23330001},
{0xc78,0x22340001},
{0xc78,0x09350001},
{0xc78,0x08360001},
{0xc78,0x07370001},
{0xc78,0x06380001},
{0xc78,0x05390001},
{0xc78,0x043a0001},
{0xc78,0x033b0001},
{0xc78,0x023c0001},
{0xc78,0x013d0001},
{0xc78,0x003e0001},
{0xc78,0x003f0001},
{0xc78,0x7f400001},
{0xc78,0x7f410001},
{0xc78,0x7e420001},
{0xc78,0x7d430001},
{0xc78,0x7c440001},
{0xc78,0x7b450001},
{0xc78,0x7a460001},
{0xc78,0x79470001},
{0xc78,0x78480001},
{0xc78,0x77490001},
{0xc78,0x764a0001},
{0xc78,0x754b0001},
{0xc78,0x744c0001},
{0xc78,0x734d0001},
{0xc78,0x724e0001},
{0xc78,0x714f0001},
{0xc78,0x70500001},
{0xc78,0x6f510001},
{0xc78,0x6f520001},
{0xc78,0x6e530001},
{0xc78,0x6d540001},
{0xc78,0x6d550001},
{0xc78,0x6c560001},
{0xc78,0x6b570001},
{0xc78,0x6a580001},
{0xc78,0x6a590001},
{0xc78,0x695a0001},
{0xc78,0x685b0001},
{0xc78,0x675c0001},
{0xc78,0x665d0001},
{0xc78,0x655e0001},
{0xc78,0x645f0001},
{0xc78,0x63600001},
{0xc78,0x4c610001},
{0xc78,0x4b620001},
{0xc78,0x4a630001},
{0xc78,0x49640001},
{0xc78,0x48650001},
{0xc78,0x47660001},
{0xc78,0x46670001},
{0xc78,0x45680001},
{0xc78,0x44690001},
{0xc78,0x2c6a0001},
{0xc78,0x2b6b0001},
{0xc78,0x2a6c0001},
{0xc78,0x296d0001},
{0xc78,0x286e0001},
{0xc78,0x276f0001},
{0xc78,0x26700001},
{0xc78,0x25710001},
{0xc78,0x24720001},
{0xc78,0x23730001},
{0xc78,0x22740001},
{0xc78,0x09750001},
{0xc78,0x08760001},
{0xc78,0x07770001},
{0xc78,0x06780001},
{0xc78,0x05790001},
{0xc78,0x047a0001},
{0xc78,0x037b0001},
{0xc78,0x027c0001},
{0xc78,0x017d0001},
{0xc78,0x007e0001},
{0xc78,0x007f0001},
{0xc78,0x3000001e},
{0xc78,0x3001001e},
{0xc78,0x3002001e},
{0xc78,0x3003001e},
{0xc78,0x3004001e},
{0xc78,0x3405001e},
{0xc78,0x3806001e},
{0xc78,0x3e07001e},
{0xc78,0x3e08001e},
{0xc78,0x4409001e},
{0xc78,0x460a001e},
{0xc78,0x480b001e},
{0xc78,0x480c001e},
{0xc78,0x4e0d001e},
{0xc78,0x560e001e},
{0xc78,0x5a0f001e},
{0xc78,0x5e10001e},
{0xc78,0x6211001e},
{0xc78,0x6c12001e},
{0xc78,0x7213001e},
{0xc78,0x7214001e},
{0xc78,0x7215001e},
{0xc78,0x7216001e},
{0xc78,0x7217001e},
{0xc78,0x7218001e},
{0xc78,0x7219001e},
{0xc78,0x721a001e},
{0xc78,0x721b001e},
{0xc78,0x721c001e},
{0xc78,0x721d001e},
{0xc78,0x721e001e},
{0xc78,0x721f001e},
{0xFFFF,0xFFFFFFFF}
};


static const struct rtl8xxxu_rfregval rtl8192su_radioa_1t_init_table[] = {
{0x000,0x00030159},
{0x001,0x00030250},
{0x002,0x00010000},
{0x010,0x0008000f},
{0x011,0x000231fc},
{0x010,0x000c000f},
{0x011,0x0003f9f8},
{0x010,0x0002000f},
{0x011,0x00020101},
{0x014,0x0001093e},
{0x014,0x0009093e},
{0x015,0x000198f4},
{0x017,0x000f6500},
{0x01a,0x00013056},
{0x01b,0x00060000},
{0x01c,0x00000300},
{0x01e,0x00031059},
{0x021,0x00054000},
{0x022,0x0000083c},
{0x023,0x00001558},
{0x024,0x00000060},
{0x025,0x00022583},
{0x026,0x0000f200},
{0x027,0x000eacf1},
{0x028,0x0009bd54},
{0x029,0x00004582},
{0x02a,0x00000001},
{0x02b,0x00021334},
{0x02a,0x00000000},
{0x02b,0x0000000a},
{0x02a,0x00000001},
{0x02b,0x00000808},
{0x02b,0x00053333},
{0x02c,0x0000000c},
{0x02a,0x00000002},
{0x02b,0x00000808},
{0x02b,0x0005b333},
{0x02c,0x0000000d},
{0x02a,0x00000003},
{0x02b,0x00000808},
{0x02b,0x00063333},
{0x02c,0x0000000d},
{0x02a,0x00000004},
{0x02b,0x00000808},
{0x02b,0x0006b333},
{0x02c,0x0000000d},
{0x02a,0x00000005},
{0x02b,0x00000709},
{0x02b,0x00053333},
{0x02c,0x0000000d},
{0x02a,0x00000006},
{0x02b,0x00000709},
{0x02b,0x0005b333},
{0x02c,0x0000000d},
{0x02a,0x00000007},
{0x02b,0x00000709},
{0x02b,0x00063333},
{0x02c,0x0000000d},
{0x02a,0x00000008},
{0x02b,0x00000709},
{0x02b,0x0006b333},
{0x02c,0x0000000d},
{0x02a,0x00000009},
{0x02b,0x0000060a},
{0x02b,0x00053333},
{0x02c,0x0000000d},
{0x02a,0x0000000a},
{0x02b,0x0000060a},
{0x02b,0x0005b333},
{0x02c,0x0000000d},
{0x02a,0x0000000b},
{0x02b,0x0000060a},
{0x02b,0x00063333},
{0x02c,0x0000000d},
{0x02a,0x0000000c},
{0x02b,0x0000060a},
{0x02b,0x0006b333},
{0x02c,0x0000000d},
{0x02a,0x0000000d},
{0x02b,0x0000050b},
{0x02b,0x00053333},
{0x02c,0x0000000d},
{0x02a,0x0000000e},
{0x02b,0x0000050b},
{0x02b,0x00066623},
{0x02c,0x0000001a},
{0x02a,0x000e4000},
{0x030,0x00020000},
{0x031,0x000b9631},
{0x032,0x0000130d},
{0x033,0x00000187},
{0x013,0x00019e6c},
{0x013,0x00015e94},
{0x000,0x00010159},
{0x018,0x0000f401},
{0x0fe,0x00000000},
{0x01e,0x0003105b},
{0x0fe,0x00000000},
{0x000,0x00030159},
{0x010,0x0004000f},
{0x011,0x000203f9},
{0x0ff,0xffffffff}
};

static int rtl8192su_identify_chip(struct rtl8xxxu_priv *priv)
{
	struct device *dev = &priv->udev->dev;
	u32 chip_version;
	int ret = 0;

	priv->usb_interrupts = 0;
	priv->has_wifi = 1;

	chip_version = (rtl8xxxu_read32(priv, REG_APS_FSMCO) >> 15) & 0x1F;
	if (chip_version == 0x3)
	{
		priv->chip_cut = 2;
	}
	else
	{
		chip_version = (chip_version >> 1) + 1;
		switch (chip_version)
		{
		case 1:
			priv->chip_cut = 0;
			break;
		case 3:
			priv->chip_cut = 2;
			break;
		case 2:
		default:
			priv->chip_cut = 1;
			break;
		}
	}

	// val32 = rtl8xxxu_read32(priv, REG_GPIO_OUTSTS);
	// priv->rom_rev = u32_get_bits(val32, GPIO_RF_RL_ID);

	rtl8xxxu_config_endpoints_sie(priv);

	/*
	 * Fallback for devices that do not provide REG_NORMAL_SIE_EP_TX
	 */
	if (!priv->ep_tx_count)
		ret = rtl8xxxu_config_endpoints_no_sie(priv);

out:
	return ret;
}

static int rtl8192su_parse_efuse(struct rtl8xxxu_priv *priv)
{
	struct rtl8192su_efuse *efuse = &priv->efuse_wifi.efuse8192su;
	int i;

	if (efuse->id != cpu_to_le16(0x8129))
		return -EINVAL;

	ether_addr_copy(priv->mac_addr, efuse->mac_addr);
	pr_info("%s: MAC Address from efuse: %pM\n", __func__, priv->mac_addr);

	memcpy(priv->cck_tx_power_index_A,
		   efuse->cck_tx_power_index_A,
		   sizeof(efuse->cck_tx_power_index_A));
	memcpy(priv->cck_tx_power_index_B,
		   efuse->cck_tx_power_index_B,
		   sizeof(efuse->cck_tx_power_index_B));

	memcpy(priv->ht40_1s_tx_power_index_A,
		   efuse->ht40_1s_tx_power_index_A,
		   sizeof(efuse->ht40_1s_tx_power_index_A));
	memcpy(priv->ht40_1s_tx_power_index_B,
		   efuse->ht40_1s_tx_power_index_B,
		   sizeof(efuse->ht40_1s_tx_power_index_B));
	for (i = 0; i < 3; i++)
	{
		priv->ht40_2s_tx_power_index_diff[i].a =
			efuse->ht40_2s_tx_power_index_diff_A[i];
		priv->ht40_2s_tx_power_index_diff[i].b =
			efuse->ht40_2s_tx_power_index_diff_B[i];
	}

	memcpy(priv->ht20_tx_power_index_diff,
		   efuse->ht20_tx_power_index_diff,
		   sizeof(efuse->ht20_tx_power_index_diff));
	memcpy(priv->ofdm_tx_power_index_diff,
		   efuse->ofdm_tx_power_index_diff,
		   sizeof(efuse->ofdm_tx_power_index_diff));

	/*memcpy(priv->ht40_max_power_offset,
		   efuse->ht40_max_power_offset,
		   sizeof(efuse->ht40_max_power_offset));
	memcpy(priv->ht20_max_power_offset,
		   efuse->ht20_max_power_offset,
		   sizeof(efuse->ht20_max_power_offset));*/
	dev_info(&priv->udev->dev, "efuse->board_type: %d\n", efuse->board_type);
	switch (efuse->board_type)
	{
	case 0:
		strscpy(priv->chip_name, "8188SU", sizeof(priv->chip_name));
		priv->rf_paths = 1;
		priv->rx_paths = 1;
		priv->tx_paths = 1;
		priv->rtl_chip = RTL8188S;
		break;
	case 1:
		strscpy(priv->chip_name, "8191SU", sizeof(priv->chip_name));
		priv->rf_paths = 2;
		priv->rx_paths = 2;
		priv->tx_paths = 1;
		priv->rtl_chip = RTL8191S;
		break;
	case 2:
	case 3:
		strscpy(priv->chip_name, "8192SU", sizeof(priv->chip_name));
		priv->rf_paths = 2;
		priv->rx_paths = 2;
		priv->tx_paths = 2;
		priv->rtl_chip = RTL8192S;
		break;
	default:
	}
	priv->power_base = &rtl8192s_power_base;

	return 0;
}

static int rtl8192su_load_firmware(struct rtl8xxxu_priv *priv)
{
	return rtl8xxxu_load_firmware(priv, "rtlwifi/rtl8192sufw.bin");
}

#define RTL8192SU_BLOCK_SIZE 0xC000

int rtl8192su_download_firmware_section(struct rtl8xxxu_priv *priv, u8 *section, size_t len)
{
	int ret = 0;
	void *tx = kzalloc(sizeof(struct rtl8xxxu_txdesc32) + RTL8192SU_BLOCK_SIZE, GFP_KERNEL);
	struct rtl8xxxu_txdesc32 *txdesc = tx;
	size_t block_len;
	if (!tx)
		return -ENOMEM;
	do
	{
		block_len = min(len, (size_t)RTL8192SU_BLOCK_SIZE);
		txdesc->pkt_size = cpu_to_le16(block_len);
		if (len <= RTL8192SU_BLOCK_SIZE)
			txdesc->txdw0 |= TXDESC_LINIP;
		memcpy(tx + sizeof(struct rtl8xxxu_txdesc32), section, block_len);
		ret = usb_bulk_msg(priv->udev, usb_sndbulkpipe(priv->udev, 0x04), tx, sizeof(struct rtl8xxxu_txdesc32) + block_len, NULL, 5000);
		pr_info("len: %zu, block_len: %zu, ret: %d\n", len, block_len, ret);
		if (ret)
			break;
		section += block_len;
		len -= block_len;
	} while (len > 0);
err:
	kfree(tx);
	return ret;
}

static void rtl8192su_get_fwpriv(struct rtl8xxxu_priv *priv, struct rtl8192su_firmware_priv *fw_priv) {
	//memset(fw_priv, 0, sizeof(*fw_priv));

	//fw_priv->hci_sel = 0x12;
	fw_priv->usb_ep_num = priv->nr_out_eps + 1; /* IN + INTERRUPT*/
	//fw_priv->bw_40mhz_en = 1;
	switch (priv->efuse_wifi.efuse8192su.board_type) {
	case 0:
		fw_priv->rf_config = 0x11;
		break;
	case 1:
		fw_priv->rf_config = 0x12;
		break;
	case 2:
		fw_priv->rf_config = 0x22;
		break;
	case 3:
		fw_priv->rf_config = 0x92;
		break;
	}
/*
	fw_priv->mp_mode = 0;
	fw_priv->vcs_type = 2;
	fw_priv->vcs_mode = 1;

	fw_priv->turbo_mode = 1;
	fw_priv->low_power_mode = 0;
	fw_priv->rsvd024 = 1;*/

	// Dump fw_priv
	pr_info("fw_priv->hci_sel: 0x%02x\n", fw_priv->hci_sel);

}

int rtl8192su_download_firmware(struct rtl8xxxu_priv *priv)
{
	int i, ret;
	void *imem, *sram;
	u32 val32;
	u16 val16;
	u8 val8;
	struct rtl8192su_firmware_header *fw_data = priv->fw_data_8192su;

	imem = fw_data->data;
	sram = fw_data->data + fw_data->img_imem_size;
	ret = rtl8192su_download_firmware_section(priv, imem, fw_data->img_imem_size);
	if (ret)
		return ret;
	for (i = 0; i < 20; i++)
	{
		val16 = rtl8xxxu_read16(priv, REG_GPIO_PIN_CTRL);
		pr_info("TCR: 0x%04x\n", val16);
		if (val16 & BIT(0))
			break;
		msleep(1);
	}
	rtl8192su_download_firmware_section(priv, sram, fw_data->img_sram_size);
	for (i = 0; i < 20; i++)
	{
		val16 = rtl8xxxu_read16(priv, REG_GPIO_PIN_CTRL);
		pr_info("TCR: 0x%04x\n", val16);
		if (val16 & BIT(2))
			break;
		msleep(100);
	}
	val8 = rtl8xxxu_read8(priv, REG_SYS_CLKR);
	rtl8xxxu_write8(priv, REG_SYS_CLKR, val8 | BIT(2));
	val8 = rtl8xxxu_read8(priv, REG_SYS_FUNC + 1);
	rtl8xxxu_write8(priv, REG_SYS_FUNC + 1, val8 | BIT(2));
	rtl8192su_get_fwpriv(priv, &fw_data->fw_priv);
	rtl8192su_download_firmware_section(priv, (u8*)&fw_data->fw_priv, sizeof(fw_data->fw_priv));
	for (i = 0; i < 20; i++)
	{
		val16 = rtl8xxxu_read16(priv, REG_GPIO_PIN_CTRL);
		pr_info("TCR: 0x%04x\n", val16);
		if (val16 & BIT(4))
			break;
		msleep(100);
	}

	rtl8xxxu_write32(priv, 0x40, 0x27fc);
	rtl8xxxu_write32(priv, 0x43, 0);
	val8 = rtl8xxxu_read8(priv, 0xFE5C);
	val8 |= BIT(7);
	rtl8xxxu_write8(priv, 0xFE5C, val8);

	return 0;
	val32 = rtl8xxxu_read32(priv, 0x44);
	val32 &= ~BIT(19);
	rtl8xxxu_write32(priv, 0x44, val32);

	val32 = rtl8xxxu_read32(priv, 0x48);
	val32 |= BIT(25) | BIT(16) | BIT(17);
	rtl8xxxu_write32(priv, 0x48, val32);

	/* Set to normal mode. */
	rtl8xxxu_write8(priv, 0x43, 0);

	/* (re-)start all queues */
	val16 = rtl8xxxu_read16(priv, 0x42);
	val16 &= ~(0b11111111);
	rtl8xxxu_write16(priv, 0x42, val16);

	/* Setting TX/RX page size to 128 byte */
	val8 = rtl8xxxu_read8(priv, 0xB5);
	val8 |= BIT(0);
	rtl8xxxu_write8(priv, 0xB5, val8);
	//priv->rx_alignment = 128;

	/* enable aggregation */
	val8 = rtl8xxxu_read8(priv, 0xBD);
	val8 |= BIT(7);
	rtl8xxxu_write8(priv, 0xBD, val8);

	/* 48 pages * 128 Byte / Page = 6kb */
	rtl8xxxu_write8(priv, 0xD9, 48);

	/* 1.7 ms / "0x04" */
	rtl8xxxu_write8(priv, REG_USB_DMA_AGG_TO, 0x04);

	/* Fix the RX FIFO issue (USB Error) */
	val8 = rtl8xxxu_read8(priv, REG_USB_AGG_TIMEOUT);
	val8 |= BIT(7);
	rtl8xxxu_write8(priv, REG_USB_AGG_TIMEOUT, val8);
	
	/* The following config GPIO function */
	rtl8xxxu_write8(priv, 0x2F1, (BIT(3) | 0));
	val8 = rtl8xxxu_read8(priv, 0x2EE);

	/* config GPIO4 to input */
	val8 &= ~BIT(4);
	rtl8xxxu_write8(priv, 0x2EE, val8);

	return 0;
}

static int rtl8192su_power_on(struct rtl8xxxu_priv *priv)
{
	u8 val8;
	u16 val16;
	int retry;
	/* Initialization for power on sequence,
	 * E-Fuse leakage prevention sequence
	 */
	rtl8xxxu_write8(priv, REG_EFUSE_TEST + 3, 0xb0);
	msleep(20);
	rtl8xxxu_write8(priv, REG_EFUSE_TEST + 3, 0x30);
	/* Set control path switch to HW control and reset Digital Core,
	 * CPU Core and MAC I/O to solve FW download fail when system
	 * from resume sate.
	 */
	val8 = rtl8xxxu_read8(priv, REG_SYS_CLKR + 1);
	if (val8 & 0x80)
	{
		val8 &= 0x3f;
		rtl8xxxu_write8(priv, REG_SYS_CLKR + 1, val8);
	}
	val8 = rtl8xxxu_read8(priv, REG_SYS_FUNC + 1);
	val8 &= 0x73;
	rtl8xxxu_write8(priv, REG_SYS_FUNC + 1, val8);
	msleep(20);
	/* Revised POS, */
	/* Enable AFE Macro Block's Bandgap and Enable AFE Macro
	 * Block's Mbias
	 */
	rtl8xxxu_write8(priv, REG_SPS0_CTRL + 1, 0x53);
	rtl8xxxu_write8(priv, REG_SPS0_CTRL, 0x57);
	val8 = rtl8xxxu_read8(priv, REG_AFE_MISC);
	/*Bandgap*/
	//rtl8xxxu_write8(priv, REG_AFE_MISC, (val8 | AFE_MISC_BGEN));
	//rtl8xxxu_write8(priv, REG_AFE_MISC, (val8 | AFE_MISC_BGEN | AFE_MISC_MBEN | AFE_MISC_I32_EN));
	rtl8xxxu_write8(priv, REG_AFE_MISC, (val8 | BIT(0)));
	rtl8xxxu_write8(priv, REG_AFE_MISC, (val8 | BIT(0) | BIT(1) | BIT(3)));
	/* Enable PLL Power (LDOA15V) */
	val8 = rtl8xxxu_read8(priv, REG_LDOA15_CTRL);
	rtl8xxxu_write8(priv, REG_LDOA15_CTRL, (val8 | LDOA15_ENABLE));
	/* Enable LDOV12D block */
	val8 = rtl8xxxu_read8(priv, REG_LDOV12D_CTRL);
	rtl8xxxu_write8(priv, REG_LDOV12D_CTRL, (val8 | LDOV12D_ENABLE));
	val8 = rtl8xxxu_read8(priv, REG_SYS_ISO_CTRL + 1);
	rtl8xxxu_write8(priv, REG_SYS_ISO_CTRL + 1, (val8 | 0x08));
	/* Engineer Packet CP test Enable */
	val8 = rtl8xxxu_read8(priv, REG_SYS_FUNC + 1);
	rtl8xxxu_write8(priv, REG_SYS_FUNC + 1, (val8 | 0x20));
	/* Support 64k IMEM */
	val8 = rtl8xxxu_read8(priv, REG_SYS_ISO_CTRL + 1);
	rtl8xxxu_write8(priv, REG_SYS_ISO_CTRL + 1, (val8 & 0x68));
	/* Enable AFE clock */
	val8 = rtl8xxxu_read8(priv, 0x26 + 1);
	rtl8xxxu_write8(priv, 0x26 + 1, (val8 & 0xfb));
	/* Enable AFE PLL Macro Block */
	val8 = rtl8xxxu_read8(priv, REG_AFE_PLL_CTRL);
	rtl8xxxu_write8(priv, REG_AFE_PLL_CTRL, (val8 | AFE_PLL_ENABLE | AFE_PLL_WDOGB));
	/* Some sample will download fw failure. The clock will be
	 * stable with 500 us delay after reset the PLL
	 * TODO: When usleep is added to kernel, change next 3
	 * udelay(500) to usleep(500)
	 */
	udelay(500);
	rtl8xxxu_write8(priv, REG_AFE_PLL_CTRL, (val8 | 0x51));
	udelay(500);
	rtl8xxxu_write8(priv, REG_AFE_PLL_CTRL, (val8 | 0x11));
	udelay(500);
	/* Attach AFE PLL to MACTOP/BB/PCIe Digital */
	val8 = rtl8xxxu_read8(priv, REG_SYS_ISO_CTRL);
	rtl8xxxu_write8(priv, REG_SYS_ISO_CTRL, (val8 & 0xEE));
	/* Switch to 40M clock */
	rtl8xxxu_write8(priv, REG_SYS_CLKR, 0x00);
	/* CPU Clock and 80M Clock SSC Disable to overcome FW download
	 * fail timing issue.
	 */
	val8 = rtl8xxxu_read8(priv, REG_SYS_CLKR);
	rtl8xxxu_write8(priv, REG_SYS_CLKR, (val8 | SYS_CLK_LOADER_ENABLE | SYS_CLK_80M_SSC_DISABLE));
	/* Enable MAC clock */
	val8 = rtl8xxxu_read8(priv, REG_SYS_CLKR + 1);
	rtl8xxxu_write8(priv, REG_SYS_CLKR + 1, (val8 | 0x18));
	/* Revised POS, */
	rtl8xxxu_write8(priv, REG_APS_FSMCO, APS_FSMCO_PFM_ALDN);
	/* Enable Core digital and enable IOREG R/W */
	val8 = rtl8xxxu_read8(priv, REG_SYS_FUNC + 1);
	rtl8xxxu_write8(priv, REG_SYS_FUNC + 1, (val8 | 0x08));
	/* Enable REG_EN */
	val8 = rtl8xxxu_read8(priv, REG_SYS_FUNC + 1);
	rtl8xxxu_write8(priv, REG_SYS_FUNC + 1, (val8 | 0x80));
	/* Switch the control path to FW */
	val8 = rtl8xxxu_read8(priv, REG_SYS_CLKR + 1);
	rtl8xxxu_write8(priv, REG_SYS_CLKR + 1, (val8 | 0x80) & 0xBF);
	rtl8xxxu_write8(priv, 0x40, 0xFC);
	rtl8xxxu_write8(priv, 0x40 + 1, 0x37);
	/* Fix the RX FIFO issue(usb error), 970410 */
	val8 = rtl8xxxu_read8(priv, REG_USB_AGG_TIMEOUT);
	rtl8xxxu_write8(priv, REG_USB_AGG_TIMEOUT, (val8 | BIT(7)));
	/* For power save, used this in the bit file after 970621 */
	val8 = rtl8xxxu_read8(priv, REG_SYS_CLKR);
	rtl8xxxu_write8(priv, REG_SYS_CLKR, val8 & (~BIT(2)));
	/* Revised for 8051 ROM code wrong operation. */
	rtl8xxxu_write8(priv, 0x1025fe1c, 0x80);
	/* To make sure that TxDMA can ready to download FW.
	 * We should reset TxDMA if IMEM RPT was not ready.
	 */
	retry = 20;
	do
	{
		val8 = rtl8xxxu_read8(priv, 0x44);
		if ((val8 & (BIT(1) | BIT(3))) == (BIT(1) | BIT(3)))
			break;
		udelay(5);			/* PlatformStallExecution(5); */
	} while (retry--); /* Delay 1ms */

	if (retry <= 0)
	{
		val8 = rtl8xxxu_read8(priv, 0x40);
		rtl8xxxu_write8(priv, 0x40, val8 & (~BIT(4)));
		udelay(2); /* PlatformStallExecution(2); */
		/* Reset TxDMA */
		rtl8xxxu_write8(priv, 0x40, val8 | BIT(4));
	}
	return 0;
}

static int rtl8192su_init_phy_rf(struct rtl8xxxu_priv *priv)
{
	const struct rtl8xxxu_rfregval *rftable;
	int ret, i;
	u32 val32;

	rftable = rtl8192su_radioa_1t_init_table;

	val32 = rtl8xxxu_read32(priv, REG_FPGA0_RF_MODE);
	val32 |= FPGA_RF_MODE_CCK | FPGA_RF_MODE_OFDM;
	rtl8xxxu_write32(priv, REG_FPGA0_RF_MODE, val32);

	for(i = 0; ;i++) {
		u8 reg = rftable[i].reg;
		u32 val = rftable[i].val;
		
		if (reg == 0xFF && val == 0xFFFFFFFF)
			break;
		
		ret = rtl8xxxu_write32(priv, REG_RF_BB_CMD_DATA, val);
		if (ret < 0)
			break;
		ret = rtl8xxxu_write32(priv, REG_RF_BB_CMD_ADDR, 0xF0000003|
						(((u32)reg)<<8)| //RF_Offset= 0x00~0x3F
						(0<<16));  //RF_Path = 0(A) or 1(B)
		if (ret < 0)
			break;

	}

	//ret = rtl8xxxu_init_phy_rf(priv, rftable, RF_A);
	return 0;
}

void rtl8192su_phy_iq_calibrate(struct rtl8xxxu_priv *priv) {

}
void rtl8192su_phy_lc_calibrate(struct rtl8xxxu_priv *priv) {

}
struct rtl8xxxu_fileops rtl8192su_fops = {
	.identify_chip = rtl8192su_identify_chip,
	.parse_efuse = rtl8192su_parse_efuse,
	.load_firmware = rtl8192su_load_firmware,
	.power_on = rtl8192su_power_on,
	.power_off = rtl8xxxu_power_off,
	.read_efuse = rtl8xxxu_read_efuse,
	.reset_8051 = rtl8xxxu_reset_8051,
	.llt_init = rtl8xxxu_init_llt_table,
	.init_phy_bb = rtl8xxxu_gen1_init_phy_bb,
	.init_phy_rf = rtl8192su_init_phy_rf,
	.phy_lc_calibrate = rtl8192su_phy_lc_calibrate,
	.phy_iq_calibrate = rtl8192su_phy_iq_calibrate,
	.config_channel = rtl8xxxu_gen1_config_channel,
	.parse_rx_desc = rtl8xxxu_parse_rxdesc16,
	.parse_phystats = rtl8723au_rx_parse_phystats,
	.init_aggregation = rtl8xxxu_gen1_init_aggregation,
	.enable_rf = rtl8xxxu_gen1_enable_rf,
	.disable_rf = rtl8xxxu_gen1_disable_rf,
	.usb_quirks = rtl8xxxu_gen1_usb_quirks,
	.set_tx_power = rtl8xxxu_gen1_set_tx_power,
	.update_rate_mask = rtl8xxxu_update_rate_mask,
	.report_connect = rtl8xxxu_gen1_report_connect,
	.report_rssi = rtl8xxxu_gen1_report_rssi,
	.fill_txdesc = rtl8xxxu_fill_txdesc_v1,
	.cck_rssi = rtl8723a_cck_rssi,
	.writeN_block_size = 128,
	.rx_agg_buf_size = 16000,
	.tx_desc_size = sizeof(struct rtl8xxxu_txdesc32),
	.rx_desc_size = sizeof(struct rtl8xxxu_rxdesc16),
	.adda_1t_init = 0x0b1b25a0,
	.adda_1t_path_on = 0x0bdb25a0,
	.adda_2t_path_on_a = 0x04db25a4,
	.adda_2t_path_on_b = 0x0b1b25a4,
	.trxff_boundary = 0x27ff,
	.pbp_rx = PBP_PAGE_SIZE_128,
	.pbp_tx = PBP_PAGE_SIZE_128,
	.mactable = rtl8192su_mac_init_table,
	.total_page_num = TX_TOTAL_PAGE_NUM,
	.page_num_hi = TX_PAGE_NUM_HI_PQ,
	.page_num_lo = TX_PAGE_NUM_LO_PQ,
	.page_num_norm = TX_PAGE_NUM_NORM_PQ,
};
