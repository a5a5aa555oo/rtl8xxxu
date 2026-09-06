// SPDX-License-Identifier: GPL-2.0-only
/*
 * RTL8XXXU mac80211 USB driver - 8188s/8191s/8192s specific subdriver
 *
 */

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "regs.h"
#include "rtl8xxxu.h"

/*
 * RTL8192SU predates the register and descriptor layout otherwise used by
 * rtl8xxxu's "gen1" devices.  Keep its definitions local to this file so the
 * common driver structures stay identical to the other chip-specific ports.
 */

#define RTL8192SU_FIRMWARE		"rtlwifi/rtl8192sufw.bin"
#define RTL8192SU_FIRMWARE_FALLBACK	"RTL8192SU/rtl8192sfw.bin"
#define RTL8192SU_FW_SIGNATURE		0x8192
#define RTL8192SU_FW_HEADER_SIZE		80
#define RTL8192SU_FW_PRIV_OFFSET		32
#define RTL8192SU_FW_PRIV_SIZE		48
#define RTL8192SU_FW_BLOCK_SIZE		0x3f00

#define RTL8192SU_REG_SYS_ISO_CTRL	0x0000
#define RTL8192SU_REG_SYS_FUNC		0x0002
#define RTL8192SU_REG_PMC_FSM		0x0004
#define RTL8192SU_REG_SYS_CLKR		0x0008
#define RTL8192SU_REG_EEPROM_CMD		0x000a
#define RTL8192SU_REG_AFE_MISC		0x0010
#define RTL8192SU_REG_SPS0_CTRL		0x0011
#define RTL8192SU_REG_SPS1_CTRL		0x0018
#define RTL8192SU_REG_RF_CTRL		0x001f
#define RTL8192SU_REG_LDOA15_CTRL	0x0020
#define RTL8192SU_REG_LDOV12_CTRL	0x0021
#define RTL8192SU_REG_AFE_XTAL_CTRL	0x0026
#define RTL8192SU_REG_AFE_PLL_CTRL	0x0028
#define RTL8192SU_REG_EFUSE_CTRL		0x0030
#define RTL8192SU_REG_EFUSE_TEST		0x0034

#define RTL8192SU_REG_CR		0x0040
#define RTL8192SU_REG_TXPAUSE		0x0042
#define RTL8192SU_REG_LBK_MODE		0x0043
#define RTL8192SU_REG_TCR		0x0044
#define RTL8192SU_REG_RCR		0x0048
#define RTL8192SU_REG_MSR		0x004c
#define RTL8192SU_REG_MACID		0x0050
#define RTL8192SU_REG_BSSID		0x0058

#define RTL8192SU_REG_SLOT_TIME		0x0089
#define RTL8192SU_REG_SIFS_CCK		0x008c
#define RTL8192SU_REG_SIFS_OFDM		0x008e
#define RTL8192SU_REG_ACK_TIMEOUT	0x0091
#define RTL8192SU_REG_BCN_INTERVAL	0x0094
#define RTL8192SU_REG_ATIMWND		0x0096
#define RTL8192SU_REG_MLT		0x009d

#define RTL8192SU_REG_LD_RQPN		0x00ab
#define RTL8192SU_REG_PBP		0x00b5
#define RTL8192SU_REG_RX_DRVINFO_SIZE	0x00b6
#define RTL8192SU_REG_RXDMA_CTRL		0x00bd
#define RTL8192SU_REG_RXDMA_AGG_PG_TH	0x00d9

#define RTL8192SU_REG_INIRTS_RATE	0x0180
#define RTL8192SU_REG_RRSR		0x0181
#define RTL8192SU_REG_ARFR0		0x0184
#define RTL8192SU_REG_AGGLEN_LMT_H	0x01a7
#define RTL8192SU_REG_AGGLEN_LMT_L	0x01a8
#define RTL8192SU_REG_DARFRC		0x01b0
#define RTL8192SU_REG_RARFRC		0x01b8
#define RTL8192SU_REG_EDCA_VO		0x01d0
#define RTL8192SU_REG_EDCA_VI		0x01d4
#define RTL8192SU_REG_EDCA_BE		0x01d8
#define RTL8192SU_REG_EDCA_BK		0x01dc
#define RTL8192SU_REG_ACM_CTRL		0x01e7
#define RTL8192SU_REG_SG_RATE		0x01f6

#define RTL8192SU_REG_BW_OPMODE		0x0203
#define RTL8192SU_REG_NAV_PROT_LEN	0x0234
#define RTL8192SU_REG_CFEND_TH		0x0236
#define RTL8192SU_REG_AMPDU_MIN_SPACE	0x0237
#define RTL8192SU_REG_TXOP_STALL_CTRL	0x0238

#define RTL8192SU_REG_WFM5		0x02c0
#define RTL8192SU_REG_RF_BB_CMD_ADDR	0x02c0
#define RTL8192SU_REG_RF_BB_CMD_DATA	0x02c4
#define RTL8192SU_REG_PHY_REG_RPT	0x02f3
#define RTL8192SU_REG_PHY_REG_DATA	0x02f4
#define RTL8192SU_REG_EFUSE_CLK		0x02f8
#define RTL8192SU_REG_LBUS_MON_ADDR	0x0364
#define RTL8192SU_REG_LBUS_ADDR_MASK	0x0368
#define RTL8192SU_REG_TP_POLL		0x0500

#define RTL8192SU_REG_USB_MAGIC		0xfe1c
#define RTL8192SU_REG_USB_HRPWM		0xfe58
#define RTL8192SU_REG_USB_DMA_AGG_TO	0xfe5b
#define RTL8192SU_REG_USB_AGG_TO		0xfe5c

#define RTL8192SU_CR_ENABLE		0x37fc
#define RTL8192SU_TCR_IMEM_DONE		BIT(0)
#define RTL8192SU_TCR_IMEM_CHKSUM	BIT(1)
#define RTL8192SU_TCR_EMEM_DONE		BIT(2)
#define RTL8192SU_TCR_EMEM_CHKSUM	BIT(3)
#define RTL8192SU_TCR_DMEM_DONE		BIT(4)
#define RTL8192SU_TCR_IMEM_READY		BIT(5)
#define RTL8192SU_TCR_BASE_CHANGED	BIT(6)
#define RTL8192SU_TCR_FW_READY		BIT(7)
#define RTL8192SU_TCR_LOAD_READY	0xff

#define RTL8192SU_RCR_AAP		BIT(0)
#define RTL8192SU_RCR_APM		BIT(1)
#define RTL8192SU_RCR_AM		BIT(2)
#define RTL8192SU_RCR_AB		BIT(3)
#define RTL8192SU_RCR_ACRC32		BIT(5)
#define RTL8192SU_RCR_APP_ICV		BIT(16)
#define RTL8192SU_RCR_APP_MIC		BIT(17)
#define RTL8192SU_RCR_ADF		BIT(18)
#define RTL8192SU_RCR_ACF		BIT(19)
#define RTL8192SU_RCR_AMF		BIT(20)
#define RTL8192SU_RCR_APWRMGT		BIT(22)
#define RTL8192SU_RCR_CBSSID		BIT(23)
#define RTL8192SU_RCR_APP_PHYST		(BIT(24) | BIT(25))
#define RTL8192SU_RCR_APPFCS		BIT(31)

#define RTL8192SU_MSR_NONE		0
#define RTL8192SU_MSR_STATION		2

#define RTL8192SU_RF_CHNLBW		0x18
#define RTL8192SU_RF_CHAN_MASK		0x3ff
#define RTL8192SU_RF_BW_MASK		GENMASK(11, 10)
#define RTL8192SU_RF_BW_20		BIT(10)

#define RTL8192SU_FW_BB_RESET_ENABLE	0xff00000d
#define RTL8192SU_FW_BB_RESET_DISABLE	0xff00000e
#define RTL8192SU_FW_CCA_CHK_ENABLE	0xff000011
#define RTL8192SU_FW_RA_INIT		0xfd000026
#define RTL8192SU_FW_RA_IOT_BG_COMB	0xfd000030
#define RTL8192SU_FW_RA_IOT_N_COMB	0xfd000031
#define RTL8192SU_FW_RA_REFRESH		0xfd0000a0
#define RTL8192SU_FW_RA_ACTIVE		0xfd0000a6
#define RTL8192SU_FW_RA_DISABLE_RSSI_MASK 0xfd0000ac
#define RTL8192SU_FW_RA_ENABLE_RSSI_MASK	0xfd0000ad
#define RTL8192SU_FW_RA_RESET		0xfd0000af

#define RTL8192SU_FW_RA_INIT_CTL		BIT(3)
#define RTL8192SU_FW_RA_BG_CTL		BIT(4)
#define RTL8192SU_FW_RA_N_CTL		BIT(5)
#define RTL8192SU_FW_DIG_ENABLE_CTL	BIT(0)
#define RTL8192SU_FW_HIGH_PWR_ENABLE_CTL	BIT(1)
#define RTL8192SU_FW_SS_CTL		BIT(2)

#define RTL8192SU_BB_FPGA0_RF_MODE	0x0800
#define RTL8192SU_BB_PHY_CCA		0x0803
#define RTL8192SU_BB_FPGA1_RF_MODE	0x0900
#define RTL8192SU_BB_FPGA0_ANALOG2	0x0884
#define RTL8192SU_BB_CCK0_SYSTEM	0x0a00
#define RTL8192SU_BB_CCK0_CCA		0x0a08
#define RTL8192SU_BB_OFDM1_LSTF		0x0d00
#define RTL8192SU_BB_OFDM0_XA_AGC	0x0c50
#define RTL8192SU_BB_OFDM0_XB_AGC	0x0c58

#define RTL8192SU_BB_TX_AGC_CCK		0x0e08
#define RTL8192SU_BB_TX_GAIN_STAGE	0x080c
#define RTL8192SU_BB_RF_INTERFACE_SW	0x0870
#define RTL8192SU_BB_RF_INTERFACE_OE_A	0x0860
#define RTL8192SU_BB_RF_INTERFACE_OE_B	0x0864
#define RTL8192SU_BB_HSSI_PARAMETER2_A	0x0824
#define RTL8192SU_BB_HSSI_PARAMETER2_B	0x082c

#define RTL8192SU_EFUSE_LEN		512
#define RTL8192SU_EFUSE_MAP_LEN		128
#define RTL8192SU_EFUSE_SECTIONS	16

#define RTL8192SU_TP_POLL_CMD		BIT(5)
#define RTL8192SU_RFENV			0x10
#define RTL8192SU_RFREG_MASK		0x000fffff

#define RTL8192SU_TXDW1_NON_QOS		BIT(16)
#define RTL8192SU_TXDW2_AGG_ENABLE	BIT(29)
#define RTL8192SU_TXDW2_AGG_BREAK	BIT(30)
#define RTL8192SU_TXDW4_RTS_FB_LIMIT_SHIFT	7
#define RTL8192SU_TXDW4_CTS_ENABLE	BIT(11)
#define RTL8192SU_TXDW4_RTS_ENABLE	BIT(12)
#define RTL8192SU_TXDW4_TX_HT		BIT(16)
#define RTL8192SU_TXDW4_TX_SHORT	BIT(17)
#define RTL8192SU_TXDW4_TX_BW		BIT(18)
#define RTL8192SU_TXDW4_TX_SC_SHIFT	19
#define RTL8192SU_TXDW4_RTS_HT		BIT(24)
#define RTL8192SU_TXDW4_RTS_SHORT	BIT(25)
#define RTL8192SU_TXDW4_RTS_BW		BIT(26)
#define RTL8192SU_TXDW4_RTS_SC_SHIFT	27
#define RTL8192SU_TXDW4_USER_RATE	BIT(31)
#define RTL8192SU_TXDW5_RATE_SHIFT	9
#define RTL8192SU_TXDW5_FB_LIMIT_SHIFT	16

MODULE_FIRMWARE(RTL8192SU_FIRMWARE);
MODULE_FIRMWARE(RTL8192SU_FIRMWARE_FALLBACK);

/* 8-byte alignment is required by the firmware ABI. */
struct rtl8192su_firmware_priv {
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
	u8 rsvd010;
	u8 rsvd011;

	/* --- long word 2 ---- */
	/* 0x00: normal, 0x03: MACLBK, 0x01: PHYLBK */
	u8 lbk_mode;
	/* 1: for MP use, 0: for normal
	 * driver (to be discussed) */
	u8 mp_mode;
	/* 0: off, 1: on, 2: auto */
	u8 rsvd020;
	u8 rsvd021;
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

/* 8-byte alignment is required by the firmware ABI. */
struct rtl8192su_firmware_header {
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
	__le32 reserved0;
	__le32 reserved1;
	__le32 reserved2;

	struct rtl8192su_firmware_priv fw_priv;
	u8 data[];
} __packed;

struct rtl8192su_efuse {
	__le16 id;
	__le16 hpon;
	__le16 clk;
	__le16 testr;
	__le16 vid;
	__le16 did;
	u8 usb_optional;
	u8 usb_phy_para1[5];
	u8 mac_addr[ETH_ALEN];
	u8 unknown0[56];
	u8 version;
	u8 channel_plan;
	u8 custom_id;
	u8 sub_custom_id;
	u8 board_type;
	u8 tx_pwr_cck[2][3];
	u8 tx_pwr_ht40_1t[2][3];
	u8 tx_pwr_ht40_2t[2][3];
	u8 pw_diff;
	u8 thermal_meter;
	u8 crystal_cap;
	u8 unknown1;
	u8 tssi[2];
	u8 unknown2;
	u8 tx_pwr_ht20_diff[3];
	u8 tx_pwr_ofdm_diff[2];
	u8 tx_pwr_edge[2][3];
	u8 tx_pwr_edge_chk;
	u8 regulatory;
	u8 rf_ind_power_diff;
	u8 tx_pwr_ofdm_diff_cont;
	u8 unknown3[3];
} __packed;

struct rtl8192su_reg32maskval {
	u16 reg;
	u32 mask;
	u32 val;
};

struct rtl8192su_rx_fwinfo {
	u8 gain_trsw[4];
	u8 pwdb_all;
	u8 cfosho[4];
	u8 cfotail[4];
	s8 rxevm[2];
	s8 rxsnr[4];
	u8 pdsnr[2];
	u8 csi_current[2];
	u8 csi_target[2];
	u8 sigevm;
	u8 max_ex_pwr;
	u8 flags;
} __packed;

struct rtl8192su_cck_phy {
	u8 adc_pwdb[4];
	u8 sq_rpt;
	u8 cck_agc_rpt;
} __packed;

static_assert(sizeof(struct rtl8192su_firmware_priv) ==
	      RTL8192SU_FW_PRIV_SIZE);
static_assert(sizeof(struct rtl8192su_firmware_header) ==
	      RTL8192SU_FW_HEADER_SIZE);
static_assert(sizeof(struct rtl8192su_efuse) == RTL8192SU_EFUSE_MAP_LEN);
static_assert(sizeof(struct rtl8192su_rx_fwinfo) == 28);


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


static const struct rtl8xxxu_reg32val rtl8192su_phy_2t2r_init_table[] = {
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

static const struct rtl8xxxu_reg32val rtl8192su_agc_table[] = {
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

static const struct rtl8xxxu_rfregval rtl8192su_radiob_init_table[] = {
	{0x000, 0x00030159},
	{0x001, 0x00001041},
	{0x002, 0x00011000},
	{0x005, 0x00080fc0},
	{0x007, 0x000fc803},
	{0x013, 0x00017cb0},
	{0x013, 0x00011cc0},
	{0x013, 0x0000dc60},
	{0x013, 0x00008c60},
	{0x013, 0x00004450},
	{0x013, 0x00000020},
	{0x0ff, 0xffffffff},
};

static const struct rtl8xxxu_rfregval rtl8192su_radiob_green_init_table[] = {
	{0x000, 0x00030159},
	{0x001, 0x00001041},
	{0x002, 0x00011000},
	{0x005, 0x00080fc0},
	{0x007, 0x000fc803},
	{0x013, 0x0000bef0},
	{0x013, 0x00007e90},
	{0x013, 0x00003e30},
	{0x0ff, 0xffffffff},
};

static const struct rtl8192su_reg32maskval
rtl8192su_phy_change_to_1t1r[] = {
	{0x844, 0xffffffff, 0x00010000},
	{0x804, 0x0000000f, 0x00000001},
	{0x824, 0x00f0000f, 0x00300004},
	{0x82c, 0x00f0000f, 0x00100002},
	{0x870, 0x04000000, 0x00000001},
	{0x864, 0x00000400, 0x00000000},
	{0x878, 0x000f000f, 0x00000002},
	{0xe74, 0x0f000000, 0x00000002},
	{0xe78, 0x0f000000, 0x00000002},
	{0xe7c, 0x0f000000, 0x00000002},
	{0xe80, 0x0f000000, 0x00000002},
	{0x90c, 0x000000ff, 0x00000011},
	{0xc04, 0x000000ff, 0x00000011},
	{0xd04, 0x0000000f, 0x00000001},
	{0x1f4, 0xffff0000, 0x00007777},
	{0x234, 0xf8000000, 0x0000000a},
	{0xffff, 0xffffffff, 0xffffffff},
};

static const struct rtl8192su_reg32maskval
rtl8192su_phy_change_to_1t2r[] = {
	{0x804, 0x0000000f, 0x00000003},
	{0x824, 0x00f0000f, 0x00300004},
	{0x82c, 0x00f0000f, 0x00300002},
	{0x870, 0x04000000, 0x00000001},
	{0x864, 0x00000400, 0x00000000},
	{0x878, 0x000f000f, 0x00000002},
	{0xe74, 0x0f000000, 0x00000002},
	{0xe78, 0x0f000000, 0x00000002},
	{0xe7c, 0x0f000000, 0x00000002},
	{0xe80, 0x0f000000, 0x00000002},
	{0x90c, 0x000000ff, 0x00000011},
	{0xc04, 0x000000ff, 0x00000033},
	{0xd04, 0x0000000f, 0x00000003},
	{0x1f4, 0xffff0000, 0x00007777},
	{0x234, 0xf8000000, 0x0000000a},
	{0xffff, 0xffffffff, 0xffffffff},
};

static const struct rtl8192su_reg32maskval
rtl8192su_phy_change_to_2t2r[] = {
	{0x804, 0x0000000f, 0x00000003},
	{0x824, 0x00f0000f, 0x00300004},
	{0x82c, 0x00f0000f, 0x00300004},
	{0x870, 0x04000000, 0x00000001},
	{0x864, 0x00000400, 0x00000001},
	{0x878, 0x000f000f, 0x00020002},
	{0xe74, 0x0f000000, 0x00000006},
	{0xe78, 0x0f000000, 0x00000006},
	{0xe7c, 0x0f000000, 0x00000006},
	{0xe80, 0x0f000000, 0x00000006},
	{0x90c, 0x000000ff, 0x00000033},
	{0xc04, 0x000000ff, 0x00000033},
	{0xd04, 0x0000000f, 0x00000003},
	{0x1f4, 0xffff0000, 0x0000ffff},
	{0x234, 0xf8000000, 0x00000013},
	{0xffff, 0xffffffff, 0xffffffff},
};

static const struct rtl8192su_reg32maskval rtl8192su_phy_pg_init[] = {
	{0xe00, 0xffffffff, 0x04060606},
	{0xe04, 0xffffffff, 0x00020204},
	{0xe08, 0x0000ff00, 0x00000000},
	{0xe10, 0xffffffff, 0x0408080a},
	{0xe14, 0xffffffff, 0x00020204},
	{0xe18, 0xffffffff, 0x0408080a},
	{0xe1c, 0xffffffff, 0x00020204},
	{0xe00, 0xffffffff, 0x00000000},
	{0xe04, 0xffffffff, 0x00000000},
	{0xe08, 0x0000ff00, 0x00000000},
	{0xe10, 0xffffffff, 0x00000000},
	{0xe14, 0xffffffff, 0x00000000},
	{0xe18, 0xffffffff, 0x00000000},
	{0xe1c, 0xffffffff, 0x00000000},
	{0xe00, 0xffffffff, 0x00000000},
	{0xe04, 0xffffffff, 0x00000000},
	{0xe08, 0x0000ff00, 0x00000000},
	{0xe10, 0xffffffff, 0x00000000},
	{0xe14, 0xffffffff, 0x00000000},
	{0xe18, 0xffffffff, 0x00000000},
	{0xe1c, 0xffffffff, 0x00000000},
	{0xe00, 0xffffffff, 0x00000000},
	{0xe04, 0xffffffff, 0x00000000},
	{0xe08, 0x0000ff00, 0x00000000},
	{0xe10, 0xffffffff, 0x00000000},
	{0xe14, 0xffffffff, 0x00000000},
	{0xe18, 0xffffffff, 0x00000000},
	{0xe1c, 0xffffffff, 0x00000000},
	{0xffff, 0xffffffff, 0xffffffff},
};

static bool rtl8192su_has_out_endpoint(struct rtl8xxxu_priv *priv, u8 ep)
{
	int i;

	for (i = 0; i < priv->nr_out_eps; i++)
		if (priv->out_ep[i] == ep)
			return true;

	return false;
}

static int rtl8192su_config_endpoints(struct rtl8xxxu_priv *priv)
{
	struct device *dev = &priv->udev->dev;
	u8 bk, vi, mgnt, beacon, high;
	u8 required[7];
	int i;

	switch (priv->nr_out_eps) {
	case 3:
		bk = 0x06;
		vi = 0x04;
		mgnt = 0x0d;
		beacon = 0x0d;
		high = 0x0d;
		break;
	case 5:
		bk = 0x07;
		vi = 0x05;
		mgnt = 0x0d;
		beacon = 0x0d;
		high = 0x0d;
		break;
	case 8:
		bk = 0x07;
		vi = 0x05;
		mgnt = 0x0c;
		beacon = 0x0a;
		high = 0x0b;
		break;
	default:
		dev_err(dev, "unsupported number of OUT endpoints: %d\n",
			priv->nr_out_eps);
		return -EINVAL;
	}

	required[0] = 0x04;
	required[1] = 0x06;
	required[2] = bk;
	required[3] = vi;
	required[4] = mgnt;
	required[5] = beacon;
	required[6] = high;
	for (i = 0; i < ARRAY_SIZE(required); i++) {
		if (!rtl8192su_has_out_endpoint(priv, required[i])) {
			dev_err(dev, "required OUT endpoint 0x%02x is missing\n",
				required[i]);
			return -ENODEV;
		}
	}

	memset(priv->pipe_out, 0, sizeof(priv->pipe_out));
	priv->pipe_out[TXDESC_QUEUE_BE] =
		usb_sndbulkpipe(priv->udev, 0x06);
	priv->pipe_out[TXDESC_QUEUE_BK] =
		usb_sndbulkpipe(priv->udev, bk);
	priv->pipe_out[TXDESC_QUEUE_VI] =
		usb_sndbulkpipe(priv->udev, vi);
	priv->pipe_out[TXDESC_QUEUE_VO] =
		usb_sndbulkpipe(priv->udev, 0x04);
	priv->pipe_out[TXDESC_QUEUE_MGNT] =
		usb_sndbulkpipe(priv->udev, mgnt);
	priv->pipe_out[TXDESC_QUEUE_BEACON] =
		usb_sndbulkpipe(priv->udev, beacon);
	priv->pipe_out[TXDESC_QUEUE_HIGH] =
		usb_sndbulkpipe(priv->udev, high);
	/* Firmware download packets are submitted through the VO endpoint. */
	priv->pipe_out[TXDESC_QUEUE_CMD] =
		priv->pipe_out[TXDESC_QUEUE_VO];
	priv->ep_tx_count = priv->nr_out_eps;

	return 0;
}

static int rtl8192su_identify_chip(struct rtl8xxxu_priv *priv)
{
	struct device *dev = &priv->udev->dev;
	u32 version;

	version = (rtl8xxxu_read32(priv, RTL8192SU_REG_PMC_FSM) >> 16) & 0x0f;
	if (version > 2) {
		dev_err(dev, "unsupported RTL8192S chip version 0x%x\n",
			version);
		return -ENODEV;
	}

	/*
	 * Board type in the efuse distinguishes 8188SU, 8191SU and 8192SU.
	 * Use the family value until the efuse has been decoded so core.c can
	 * dispatch the family-specific read and initialization paths.
	 */
	priv->rtl_chip = RTL8192S;
	priv->chip_cut = version;
	priv->rf_paths = 2;
	priv->rx_paths = 2;
	priv->tx_paths = 2;
	priv->usb_interrupts = 0;
	priv->has_wifi = 1;
	priv->has_bluetooth = 0;
	priv->has_gps = 0;
	/*
	 * RX aggregation remains disabled in hardware.  Reuse the existing
	 * large-buffer allocation path so an 8192SU URB can hold an A-MSDU.
	 */
	priv->rx_buf_aggregation = 1;
	priv->hw_feature.max_bw = 40;
	strscpy(priv->chip_vendor, "TSMC", sizeof(priv->chip_vendor));

	return rtl8192su_config_endpoints(priv);
}

static int rtl8192su_efuse_read_byte(struct rtl8xxxu_priv *priv,
				     u16 address, u8 *data)
{
	u8 val8;
	int i;

	rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_CTRL + 1,
			address & 0xff);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_EFUSE_CTRL + 2);
	val8 = (val8 & 0xfc) | ((address >> 8) & 0x03);
	rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_CTRL + 2, val8);
	rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_CTRL + 3, 0x72);

	for (i = 0; i < 100; i++) {
		if (rtl8xxxu_read8(priv, RTL8192SU_REG_EFUSE_CTRL + 3) &
		    BIT(7)) {
			*data = rtl8xxxu_read8(priv, RTL8192SU_REG_EFUSE_CTRL);
			return 0;
		}
		udelay(1);
	}

	*data = 0xff;
	return -ETIMEDOUT;
}

static void rtl8192su_efuse_power(struct rtl8xxxu_priv *priv, bool on)
{
	u8 val8;

	if (on) {
		val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_FUNC + 1);
		rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_FUNC + 1,
				val8 | BIT(5));

		val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1);
		rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1,
				val8 & ~BIT(0));

		val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_EFUSE_TEST + 3);
		rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_TEST + 3,
				val8 | BIT(7));
		rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_CLK, 0x03);
		rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_CTRL + 3, 0x72);
	} else {
		rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_CLK, 0x02);
		val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_EFUSE_TEST + 3);
		rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_TEST + 3,
				val8 & ~BIT(7));
	}
}

static int rtl8192su_read_efuse(struct rtl8xxxu_priv *priv)
{
	struct device *dev = &priv->udev->dev;
	u8 physical[RTL8192SU_EFUSE_LEN];
	u8 logical[RTL8192SU_EFUSE_MAP_LEN];
	u8 eeprom_cmd, header, word_en;
	unsigned int addr, section, word;
	int ret = 0;

	eeprom_cmd = rtl8xxxu_read8(priv, RTL8192SU_REG_EEPROM_CMD);
	if (eeprom_cmd & BIT(4)) {
		dev_err(dev, "93C46 EEPROM boot is not supported\n");
		return -EOPNOTSUPP;
	}
	if (!(eeprom_cmd & BIT(5))) {
		dev_err(dev, "efuse autoload failed\n");
		return -EIO;
	}

	memset(physical, 0xff, sizeof(physical));
	memset(logical, 0xff, sizeof(logical));
	memset(priv->efuse_wifi.raw, 0xff, sizeof(priv->efuse_wifi.raw));

	rtl8192su_efuse_power(priv, true);
	for (addr = 0; addr < ARRAY_SIZE(physical); addr++) {
		ret = rtl8192su_efuse_read_byte(priv, addr, &physical[addr]);
		if (ret) {
			dev_err(dev, "efuse read timed out at 0x%x\n", addr);
			goto out_power;
		}
	}

	addr = 0;
	while (addr < ARRAY_SIZE(physical)) {
		header = physical[addr++];
		if (header == 0xff)
			break;

		section = header >> 4;
		word_en = header & 0x0f;
		if (section >= RTL8192SU_EFUSE_SECTIONS) {
			dev_err(dev, "invalid efuse section %u\n", section);
			ret = -EINVAL;
			goto out_power;
		}

		for (word = 0; word < 4; word++) {
			unsigned int offset;

			if (word_en & BIT(word))
				continue;
			if (addr + 1 >= ARRAY_SIZE(physical)) {
				ret = -EINVAL;
				goto out_power;
			}

			offset = section * 8 + word * 2;
			logical[offset] = physical[addr++];
			logical[offset + 1] = physical[addr++];
		}
	}

	memcpy(priv->efuse_wifi.raw, logical, sizeof(logical));

out_power:
	rtl8192su_efuse_power(priv, false);
	return ret;
}

static int rtl8192su_parse_efuse(struct rtl8xxxu_priv *priv)
{
	const struct rtl8192su_efuse *efuse;
	struct device *dev = &priv->udev->dev;

	efuse = (const struct rtl8192su_efuse *)priv->efuse_wifi.raw;
	if (efuse->id != cpu_to_le16(0x8129)) {
		dev_err(dev, "invalid efuse ID 0x%04x\n",
			le16_to_cpu(efuse->id));
		return -EINVAL;
	}
	if (!is_valid_ether_addr(efuse->mac_addr)) {
		dev_err(dev, "invalid MAC address in efuse\n");
		return -EINVAL;
	}

	ether_addr_copy(priv->mac_addr, efuse->mac_addr);
	memcpy(priv->cck_tx_power_index_A, efuse->tx_pwr_cck[0], 3);
	memcpy(priv->cck_tx_power_index_B, efuse->tx_pwr_cck[1], 3);
	memcpy(priv->ht40_1s_tx_power_index_A,
	       efuse->tx_pwr_ht40_1t[0], 3);
	memcpy(priv->ht40_1s_tx_power_index_B,
	       efuse->tx_pwr_ht40_1t[1], 3);
	priv->default_crystal_cap = efuse->crystal_cap >> 4;

	switch (efuse->board_type) {
	case 0:
		strscpy(priv->chip_name, "8188SU", sizeof(priv->chip_name));
		priv->rtl_chip = RTL8188S;
		priv->rf_paths = 1;
		priv->rx_paths = 1;
		priv->tx_paths = 1;
		break;
	case 1:
		strscpy(priv->chip_name, "8191SU", sizeof(priv->chip_name));
		priv->rtl_chip = RTL8191S;
		priv->rf_paths = 2;
		priv->rx_paths = 2;
		priv->tx_paths = 1;
		break;
	case 2:
	case 3:
		strscpy(priv->chip_name, "8192SU", sizeof(priv->chip_name));
		priv->rtl_chip = RTL8192S;
		priv->rf_paths = 2;
		priv->rx_paths = 2;
		priv->tx_paths = 2;
		break;
	default:
		dev_err(dev, "unsupported efuse board type %u\n",
			efuse->board_type);
		return -EINVAL;
	}

	priv->hw->wiphy->available_antennas_tx = BIT(priv->tx_paths) - 1;
	priv->hw->wiphy->available_antennas_rx = BIT(priv->rx_paths) - 1;

	return 0;
}

static int rtl8192su_validate_firmware(struct rtl8xxxu_priv *priv,
				       const struct firmware *firmware,
				       const char *name)
{
	const struct rtl8192su_firmware_header *header;
	struct device *dev = &priv->udev->dev;
	size_t remaining;
	u32 imem_size, sram_size;

	if (firmware->size < RTL8192SU_FW_HEADER_SIZE) {
		dev_err(dev, "%s: firmware header is truncated\n", name);
		return -EINVAL;
	}

	header = (const struct rtl8192su_firmware_header *)firmware->data;
	if (le16_to_cpu(header->signature) != RTL8192SU_FW_SIGNATURE) {
		dev_err(dev, "%s: invalid firmware signature 0x%04x\n",
			name, le16_to_cpu(header->signature));
		return -EINVAL;
	}
	if (le32_to_cpu(header->fw_priv_size) != RTL8192SU_FW_PRIV_SIZE) {
		dev_err(dev, "%s: invalid firmware private-data size %u\n",
			name, le32_to_cpu(header->fw_priv_size));
		return -EINVAL;
	}

	imem_size = le32_to_cpu(header->img_imem_size);
	sram_size = le32_to_cpu(header->img_sram_size);
	if (!imem_size || !sram_size) {
		dev_err(dev, "%s: firmware has an empty code section\n", name);
		return -EINVAL;
	}

	remaining = firmware->size - RTL8192SU_FW_HEADER_SIZE;
	if (imem_size > remaining || sram_size > remaining - imem_size) {
		dev_err(dev, "%s: firmware image is truncated\n", name);
		return -EINVAL;
	}

	return 0;
}

static int rtl8192su_load_firmware(struct rtl8xxxu_priv *priv)
{
	const struct firmware *firmware;
	const char *name = RTL8192SU_FIRMWARE;
	struct device *dev = &priv->udev->dev;
	void *data;
	int ret;

	ret = request_firmware(&firmware, name, dev);
	if (ret) {
		name = RTL8192SU_FIRMWARE_FALLBACK;
		ret = request_firmware(&firmware, name, dev);
	}
	if (ret)
		return ret;

	ret = rtl8192su_validate_firmware(priv, firmware, name);
	if (ret)
		goto out_release;

	data = kmemdup(firmware->data, firmware->size, GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out_release;
	}

	priv->fw_data = data;
	priv->fw_size = firmware->size;
	dev_info(dev, "loaded softmac firmware %s\n", name);

out_release:
	release_firmware(firmware);
	return ret;
}

static u8 rtl8192su_firmware_ep_count(struct rtl8xxxu_priv *priv)
{
	switch (priv->nr_out_eps) {
	case 3:
		return 4;
	case 5:
		return 6;
	case 8:
		return 11;
	default:
		return 0;
	}
}

static u8 rtl8192su_firmware_rf_config(struct rtl8xxxu_priv *priv)
{
	const struct rtl8192su_efuse *efuse;

	efuse = (const struct rtl8192su_efuse *)priv->efuse_wifi.raw;
	switch (efuse->board_type) {
	case 0:
		return 0x11;
	case 1:
		return 0x12;
	case 2:
		return 0x22;
	case 3:
		return 0x92;
	default:
		return 0x22;
	}
}

static void rtl8192su_update_fw_priv(struct rtl8xxxu_priv *priv)
{
	struct rtl8192su_firmware_header *header;
	struct rtl8192su_firmware_priv *fw_priv;

	header = (struct rtl8192su_firmware_header *)priv->fw_data;
	fw_priv = &header->fw_priv;
	fw_priv->hci_sel = 0x02;
	fw_priv->usb_ep_num = rtl8192su_firmware_ep_count(priv);
	fw_priv->beacon_offload = 2;
	fw_priv->rf_config = rtl8192su_firmware_rf_config(priv);
}

static int rtl8192su_download_firmware_section(struct rtl8xxxu_priv *priv,
						const u8 *section,
						size_t length)
{
	struct rtl8xxxu_txdesc32 *tx_desc;
	struct device *dev = &priv->udev->dev;
	size_t block_len;
	u8 *buffer;
	int actual, ret = 0;

	buffer = kmalloc(sizeof(*tx_desc) + RTL8192SU_FW_BLOCK_SIZE,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	while (length) {
		block_len = min_t(size_t, length, RTL8192SU_FW_BLOCK_SIZE);
		tx_desc = (struct rtl8xxxu_txdesc32 *)buffer;
		memset(tx_desc, 0, sizeof(*tx_desc));
		tx_desc->pkt_size = cpu_to_le16(block_len);
		if (block_len == length)
			tx_desc->txdw0 = TXDESC_LINIP;
		memcpy(buffer + sizeof(*tx_desc), section, block_len);

		ret = usb_bulk_msg(priv->udev,
				   priv->pipe_out[TXDESC_QUEUE_VO],
				   buffer, sizeof(*tx_desc) + block_len,
				   &actual, 5000);
		if (ret)
			break;
		if (actual != sizeof(*tx_desc) + block_len) {
			dev_err(dev, "short firmware write: %d of %zu bytes\n",
				actual, sizeof(*tx_desc) + block_len);
			ret = -EIO;
			break;
		}

		section += block_len;
		length -= block_len;
	}

	if (!ret)
		rtl8xxxu_write8(priv, RTL8192SU_REG_TP_POLL,
				RTL8192SU_TP_POLL_CMD);
	kfree(buffer);
	return ret;
}

static int rtl8192su_poll_tcr(struct rtl8xxxu_priv *priv, u8 mask,
			      int count, unsigned int delay_ms)
{
	u8 value = 0;

	while (count--) {
		value = rtl8xxxu_read8(priv, RTL8192SU_REG_TCR);
		if ((value & mask) == mask)
			return 0;
		if (delay_ms)
			msleep(delay_ms);
		else
			udelay(5);
	}

	dev_err(&priv->udev->dev,
		"firmware state timeout: TCR=0x%02x, expected 0x%02x\n",
		value, mask);
	return -ETIMEDOUT;
}

static int rtl8192su_download_firmware(struct rtl8xxxu_priv *priv)
{
	struct rtl8192su_firmware_header *header;
	const u8 *imem, *emem;
	u32 imem_size, emem_size;
	u16 val16;
	u32 val32;
	u8 val8;
	int ret;

	header = (struct rtl8192su_firmware_header *)priv->fw_data;
	imem_size = le32_to_cpu(header->img_imem_size);
	emem_size = le32_to_cpu(header->img_sram_size);
	imem = header->data;
	emem = imem + imem_size;

	ret = rtl8192su_download_firmware_section(priv, imem, imem_size);
	if (ret)
		return ret;
	ret = rtl8192su_poll_tcr(priv,
				 RTL8192SU_TCR_IMEM_DONE |
				 RTL8192SU_TCR_IMEM_CHKSUM,
				 10, 20);
	if (ret)
		return ret;

	ret = rtl8192su_download_firmware_section(priv, emem, emem_size);
	if (ret)
		return ret;
	ret = rtl8192su_poll_tcr(priv,
				 RTL8192SU_TCR_EMEM_DONE |
				 RTL8192SU_TCR_EMEM_CHKSUM,
				 10, 20);
	if (ret)
		return ret;

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_CLKR);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_CLKR, val8 | BIT(2));
	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_FUNC);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_FUNC, val16 | BIT(10));
	ret = rtl8192su_poll_tcr(priv, RTL8192SU_TCR_IMEM_READY, 10, 20);
	if (ret)
		return ret;

	rtl8192su_update_fw_priv(priv);
	ret = rtl8192su_download_firmware_section(
		priv, (u8 *)header + RTL8192SU_FW_PRIV_OFFSET,
		RTL8192SU_FW_PRIV_SIZE);
	if (ret)
		return ret;
	ret = rtl8192su_poll_tcr(priv, RTL8192SU_TCR_DMEM_DONE, 10, 20);
	if (ret)
		return ret;
	ret = rtl8192su_poll_tcr(priv, RTL8192SU_TCR_LOAD_READY, 30, 100);
	if (ret)
		return ret;

	val32 = rtl8xxxu_read32(priv, RTL8192SU_REG_TCR);
	rtl8xxxu_write32(priv, RTL8192SU_REG_TCR,
			  val32 & ~BIT(19));
	val32 = rtl8xxxu_read32(priv, RTL8192SU_REG_RCR);
	val32 |= RTL8192SU_RCR_APPFCS | RTL8192SU_RCR_APP_ICV |
		 RTL8192SU_RCR_APP_MIC;
	rtl8xxxu_write32(priv, RTL8192SU_REG_RCR, val32);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LBK_MODE, 0);

	dev_info(&priv->udev->dev, "firmware version 0x%02x is ready\n",
		 le16_to_cpu(header->version) & 0xff);
	return 0;
}

static u8 rtl8192su_firmware_version(struct rtl8xxxu_priv *priv)
{
	const struct rtl8192su_firmware_header *header;

	header = (const struct rtl8192su_firmware_header *)priv->fw_data;
	return le16_to_cpu(header->version) & 0xff;
}

static int rtl8192su_set_sysclk(struct rtl8xxxu_priv *priv, u16 clock)
{
	u16 value;
	int i;

	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_CLKR, clock);
	mdelay(20);

	value = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_CLKR);
	if ((value & BIT(15)) != (clock & BIT(15)))
		return -EIO;

	if (!(clock & (BIT(14) | BIT(15)))) {
		for (i = 0; i < 10; i++) {
			msleep(20);
			value = rtl8xxxu_read16(priv,
						RTL8192SU_REG_SYS_CLKR);
			if (value & BIT(14))
				return 0;
		}
		return -ETIMEDOUT;
	}

	return 0;
}

static int rtl8192su_power_on(struct rtl8xxxu_priv *priv)
{
	struct device *dev = &priv->udev->dev;
	u16 val16;
	u8 val8;
	int i, ret;

	rtl8xxxu_write8(priv, RTL8192SU_REG_USB_HRPWM, 0);
	rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_TEST + 3, 0xb0);
	msleep(20);
	rtl8xxxu_write8(priv, RTL8192SU_REG_EFUSE_TEST + 3, 0x30);

	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_CLKR);
	if (val16 & BIT(15)) {
		val16 &= ~(BIT(14) | BIT(15));
		ret = rtl8192su_set_sysclk(priv, val16);
		if (ret)
			return ret;
	}

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_FUNC + 1);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_FUNC + 1, val8 & 0x73);
	mdelay(1);

	rtl8xxxu_write8(priv, RTL8192SU_REG_SPS0_CTRL + 1, 0x53);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SPS0_CTRL, 0x57);

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_AFE_MISC);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_MISC, val8 | BIT(0));
	mdelay(1);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_AFE_MISC);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_MISC,
			val8 | BIT(0) | BIT(1));
	mdelay(1);

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_LDOA15_CTRL);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LDOA15_CTRL, val8 | BIT(0));
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_LDOV12_CTRL);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LDOV12_CTRL, val8 | BIT(0));

	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_ISO_CTRL);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_ISO_CTRL,
			 val16 | BIT(11));
	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_FUNC);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_FUNC, val16 | BIT(13));

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1, val8 & 0x68);

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_AFE_XTAL_CTRL);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_XTAL_CTRL, val8 | BIT(0));
	mdelay(2);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_AFE_XTAL_CTRL + 1);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_XTAL_CTRL + 1,
			val8 & ~BIT(2));

	udelay(200);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_AFE_PLL_CTRL);
	val8 |= BIT(0) | BIT(4);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_PLL_CTRL, val8);
	udelay(500);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_PLL_CTRL, val8 | BIT(6));
	udelay(500);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_PLL_CTRL, val8);
	udelay(500);

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_ISO_CTRL);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_ISO_CTRL, val8 & 0xee);

	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_CLKR, 0);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_CLKR);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_CLKR, val8 | 0xa0);
	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_CLKR);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_CLKR,
			 val16 | BIT(12) | BIT(11));

	rtl8xxxu_write8(priv, RTL8192SU_REG_PMC_FSM, 0x02);
	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_FUNC);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_FUNC, val16 | BIT(11));
	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_FUNC);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_FUNC, val16 | BIT(15));

	val16 = rtl8xxxu_read16(priv, RTL8192SU_REG_SYS_CLKR);
	val16 |= BIT(15);
	val16 &= ~BIT(14);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SYS_CLKR, val16);

	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, RTL8192SU_CR_ENABLE);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_USB_AGG_TO);
	rtl8xxxu_write8(priv, RTL8192SU_REG_USB_AGG_TO, val8 | BIT(7));
	rtl8xxxu_write8(priv, RTL8192SU_REG_USB_MAGIC, BIT(7));

	for (i = 0; i < 20; i++) {
		val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_TCR);
		if ((val8 & (BIT(1) | BIT(3))) == (BIT(1) | BIT(3)))
			return 0;
		msleep(20);
	}

	dev_warn(dev, "TXDMA did not become ready; resetting it\n");
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_CR);
	rtl8xxxu_write8(priv, RTL8192SU_REG_CR, val8 & ~BIT(4));
	msleep(20);
	rtl8xxxu_write8(priv, RTL8192SU_REG_CR, val8 | BIT(4));

	return 0;
}

static void rtl8192su_power_off(struct rtl8xxxu_priv *priv)
{
	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, 0);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RF_CTRL, 0);
	msleep(20);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_CLKR + 1, 0x38);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_FUNC + 1, 0x70);
	rtl8xxxu_write8(priv, RTL8192SU_REG_PMC_FSM, 0x06);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_ISO_CTRL, 0xf9);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1, 0xe8);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_PLL_CTRL, 0);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LDOA15_CTRL, 0x54);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_FUNC + 1, 0x50);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LDOV12_CTRL, 0x24);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_MISC, 0x30);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SPS0_CTRL, 0x56);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SPS0_CTRL + 1, 0x43);
}

static void rtl8192su_mac_config_after_firmware(struct rtl8xxxu_priv *priv)
{
	u32 val32;
	u8 val8;
	int i;

	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, RTL8192SU_CR_ENABLE);
	val32 = rtl8xxxu_read32(priv, RTL8192SU_REG_TCR);
	rtl8xxxu_write32(priv, RTL8192SU_REG_TCR, val32 | BIT(23));

	rtl8xxxu_write16(priv, RTL8192SU_REG_SIFS_CCK, 0x0a0a);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SIFS_OFDM, 0x1010);
	rtl8xxxu_write8(priv, RTL8192SU_REG_ACK_TIMEOUT, 0x40);
	rtl8xxxu_write16(priv, RTL8192SU_REG_BCN_INTERVAL, 100);
	rtl8xxxu_write16(priv, RTL8192SU_REG_ATIMWND, 2);
	rtl8xxxu_write8(priv, RTL8192SU_REG_PBP, 0x22);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RXDMA_AGG_PG_TH, 1);

	rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR,
			priv->chip_cut == 0 ? 0xf0 : 0xff);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR + 1, 0x01);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR + 2, 0);
	if (priv->chip_cut == 0)
		for (i = 0; i < 8; i++)
			rtl8xxxu_write32(priv,
					 RTL8192SU_REG_ARFR0 + i * 4,
					 0x1f0ff0f0);

	rtl8xxxu_write8(priv, RTL8192SU_REG_AGGLEN_LMT_H, 0x0f);
	rtl8xxxu_write16(priv, RTL8192SU_REG_AGGLEN_LMT_L, 0x5221);
	rtl8xxxu_write16(priv, RTL8192SU_REG_AGGLEN_LMT_L + 2, 0xbbb5);
	rtl8xxxu_write16(priv, RTL8192SU_REG_AGGLEN_LMT_L + 4, 0xb551);
	rtl8xxxu_write16(priv, RTL8192SU_REG_AGGLEN_LMT_L + 6, 0xfffb);

	rtl8xxxu_write32(priv, RTL8192SU_REG_DARFRC, 0x01000000);
	rtl8xxxu_write32(priv, RTL8192SU_REG_DARFRC + 4, 0x07060504);
	rtl8xxxu_write32(priv, RTL8192SU_REG_RARFRC, 0x01000000);
	rtl8xxxu_write32(priv, RTL8192SU_REG_RARFRC + 4, 0x07060605);
	rtl8xxxu_write32(priv, RTL8192SU_REG_EDCA_BE, 0x0000a44f);
	rtl8xxxu_write16(priv, RTL8192SU_REG_SG_RATE, 0xffff);
	rtl8xxxu_write16(priv, RTL8192SU_REG_NAV_PROT_LEN, 0x0080);
	rtl8xxxu_write8(priv, RTL8192SU_REG_CFEND_TH, 0xff);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AMPDU_MIN_SPACE, 0x07);
	rtl8xxxu_write8(priv, RTL8192SU_REG_TXOP_STALL_CTRL, 0);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RX_DRVINFO_SIZE, 4);

	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_LD_RQPN);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LD_RQPN, val8 | 0xa0);
	rtl8xxxu_write8(priv, RTL8192SU_REG_USB_DMA_AGG_TO, 0x0a);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_USB_AGG_TO);
	rtl8xxxu_write8(priv, RTL8192SU_REG_USB_AGG_TO, val8 | BIT(7));
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SYS_ISO_CTRL + 1,
			val8 & 0xfe);
	val8 = rtl8xxxu_read8(priv, RTL8192SU_REG_LBUS_MON_ADDR);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LBUS_MON_ADDR,
			val8 & ~BIT(0));

	priv->regrcr = rtl8xxxu_read32(priv, RTL8192SU_REG_RCR);
}

static int rtl8192su_wait_bb_ready(struct rtl8xxxu_priv *priv)
{
	int i;

	for (i = 0; i < 100; i++)
		if (!(rtl8xxxu_read8(priv, RTL8192SU_REG_PHY_REG_RPT) &
		      BIT(0)))
			return 0;

	return -ETIMEDOUT;
}

static u32 rtl8192su_read_bbreg(struct rtl8xxxu_priv *priv, u16 reg)
{
	u32 value = 0xffffffff;

	mutex_lock(&priv->syson_indirect_access_mutex);
	if (rtl8192su_wait_bb_ready(priv))
		goto out;

	/* A read of the BB address starts the indirect read operation. */
	rtl8xxxu_read32(priv, reg);
	if (rtl8192su_wait_bb_ready(priv))
		goto out;
	value = rtl8xxxu_read32(priv, RTL8192SU_REG_PHY_REG_DATA);

out:
	mutex_unlock(&priv->syson_indirect_access_mutex);
	return value;
}

static u32 rtl8192su_read_bbreg_mask(struct rtl8xxxu_priv *priv, u16 reg,
				     u32 mask)
{
	u32 value;

	if (!mask)
		return 0;
	value = rtl8192su_read_bbreg(priv, reg);
	return (value & mask) >> __ffs(mask);
}

static void rtl8192su_write_bbreg_mask(struct rtl8xxxu_priv *priv, u16 reg,
				       u32 mask, u32 data)
{
	u32 value;

	if (!mask)
		return;

	mutex_lock(&priv->syson_indirect_access_mutex);
	if (mask != 0xffffffff) {
		if (rtl8192su_wait_bb_ready(priv))
			goto out;
		rtl8xxxu_read32(priv, reg);
		if (rtl8192su_wait_bb_ready(priv))
			goto out;
		value = rtl8xxxu_read32(priv, RTL8192SU_REG_PHY_REG_DATA);
		value &= ~mask;
		value |= (data << __ffs(mask)) & mask;
	} else {
		value = data;
	}

	if (!rtl8192su_wait_bb_ready(priv))
		rtl8xxxu_write32(priv, reg, value);

out:
	mutex_unlock(&priv->syson_indirect_access_mutex);
}

static int rtl8192su_wait_rf_ready(struct rtl8xxxu_priv *priv)
{
	int i;

	for (i = 0; i < 100; i++)
		if (!rtl8xxxu_read32(priv, RTL8192SU_REG_RF_BB_CMD_ADDR))
			return 0;

	return -ETIMEDOUT;
}

static u32 rtl8192su_read_rfreg(struct rtl8xxxu_priv *priv,
				enum rtl8xxxu_rfpath path, u8 reg)
{
	u32 value = 0xffffffff;

	mutex_lock(&priv->syson_indirect_access_mutex);
	if (rtl8192su_wait_rf_ready(priv))
		goto out;
	rtl8xxxu_write32(priv, RTL8192SU_REG_RF_BB_CMD_ADDR,
			  0xf0000002 | ((reg & 0x3f) << 8) |
			  ((u32)path << 16));
	if (rtl8192su_wait_rf_ready(priv))
		goto out;
	value = rtl8xxxu_read32(priv, RTL8192SU_REG_RF_BB_CMD_DATA);

out:
	mutex_unlock(&priv->syson_indirect_access_mutex);
	return value;
}

static int rtl8192su_write_rfreg(struct rtl8xxxu_priv *priv,
				 enum rtl8xxxu_rfpath path, u8 reg, u32 data)
{
	int ret;

	mutex_lock(&priv->syson_indirect_access_mutex);
	ret = rtl8192su_wait_rf_ready(priv);
	if (ret)
		goto out;
	rtl8xxxu_write32(priv, RTL8192SU_REG_RF_BB_CMD_DATA,
			  data & RTL8192SU_RFREG_MASK);
	rtl8xxxu_write32(priv, RTL8192SU_REG_RF_BB_CMD_ADDR,
			  0xf0000003 | ((reg & 0x3f) << 8) |
			  ((u32)path << 16));
	ret = rtl8192su_wait_rf_ready(priv);

out:
	mutex_unlock(&priv->syson_indirect_access_mutex);
	return ret;
}

static void rtl8192su_table_delay(u16 reg)
{
	switch (reg) {
	case 0xfe:
		mdelay(1000);
		break;
	case 0xfd:
		mdelay(5);
		break;
	case 0xfc:
		mdelay(1);
		break;
	case 0xfb:
		udelay(50);
		break;
	case 0xfa:
		udelay(5);
		break;
	case 0xf9:
		udelay(1);
		break;
	default:
		break;
	}
}

static void rtl8192su_init_mac(struct rtl8xxxu_priv *priv)
{
	const struct rtl8xxxu_reg8val *entry;

	for (entry = rtl8192su_mac_init_table; entry->reg != 0xffff; entry++)
		rtl8xxxu_write8(priv, entry->reg, entry->val);
}

static void rtl8192su_init_bb_table(struct rtl8xxxu_priv *priv,
				    const struct rtl8xxxu_reg32val *table)
{
	const struct rtl8xxxu_reg32val *entry;

	for (entry = table; entry->reg != 0xffff; entry++) {
		rtl8192su_table_delay(entry->reg);
		rtl8192su_write_bbreg_mask(priv, entry->reg, 0xffffffff,
					   entry->val);
	}
}

static void
rtl8192su_init_bb_mask_table(struct rtl8xxxu_priv *priv,
			     const struct rtl8192su_reg32maskval *table)
{
	const struct rtl8192su_reg32maskval *entry;

	for (entry = table; entry->reg != 0xffff; entry++) {
		rtl8192su_table_delay(entry->reg);
		rtl8192su_write_bbreg_mask(priv, entry->reg, entry->mask,
					   entry->val);
	}
}

static void rtl8192su_init_phy_bb(struct rtl8xxxu_priv *priv)
{
	const struct rtl8192su_efuse *efuse;
	const struct rtl8192su_reg32maskval *topology;

	efuse = (const struct rtl8192su_efuse *)priv->efuse_wifi.raw;
	rtl8192su_init_bb_table(priv, rtl8192su_phy_2t2r_init_table);

	switch (efuse->board_type) {
	case 0:
		topology = rtl8192su_phy_change_to_1t1r;
		break;
	case 1:
		topology = rtl8192su_phy_change_to_1t2r;
		break;
	default:
		topology = rtl8192su_phy_change_to_2t2r;
		break;
	}
	rtl8192su_init_bb_mask_table(priv, topology);
	rtl8192su_init_bb_mask_table(priv, rtl8192su_phy_pg_init);
	rtl8192su_init_bb_table(priv, rtl8192su_agc_table);
}

static int
rtl8192su_init_rf_table(struct rtl8xxxu_priv *priv,
			enum rtl8xxxu_rfpath path,
			const struct rtl8xxxu_rfregval *table)
{
	const struct rtl8xxxu_rfregval *entry;
	int ret;

	for (entry = table;
	     !(entry->reg == 0xff && entry->val == 0xffffffff);
	     entry++) {
		if (entry->reg >= 0xf9) {
			rtl8192su_table_delay(entry->reg);
			continue;
		}

		ret = rtl8192su_write_rfreg(priv, path, entry->reg,
					    entry->val);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtl8192su_init_rf_path(struct rtl8xxxu_priv *priv,
				  enum rtl8xxxu_rfpath path,
				  const struct rtl8xxxu_rfregval *table)
{
	u16 interface_oe, hssi_parameter2;
	u32 rfenv_mask, saved_rfenv;
	int ret;

	if (path == RF_A) {
		rfenv_mask = RTL8192SU_RFENV;
		interface_oe = RTL8192SU_BB_RF_INTERFACE_OE_A;
		hssi_parameter2 = RTL8192SU_BB_HSSI_PARAMETER2_A;
	} else {
		rfenv_mask = RTL8192SU_RFENV << 16;
		interface_oe = RTL8192SU_BB_RF_INTERFACE_OE_B;
		hssi_parameter2 = RTL8192SU_BB_HSSI_PARAMETER2_B;
	}

	saved_rfenv = rtl8192su_read_bbreg_mask(
		priv, RTL8192SU_BB_RF_INTERFACE_SW, rfenv_mask);
	rtl8192su_write_bbreg_mask(priv, interface_oe,
				   RTL8192SU_RFENV << 16, 1);
	rtl8192su_write_bbreg_mask(priv, interface_oe,
				   RTL8192SU_RFENV, 1);
	rtl8192su_write_bbreg_mask(priv, hssi_parameter2, 0x00000400, 0);
	rtl8192su_write_bbreg_mask(priv, hssi_parameter2, 0x00000800, 0);

	ret = rtl8192su_init_rf_table(priv, path, table);
	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_RF_INTERFACE_SW,
				   rfenv_mask, saved_rfenv);
	return ret;
}

static int rtl8192su_init_phy_rf(struct rtl8xxxu_priv *priv)
{
	const struct rtl8192su_efuse *efuse;
	const struct rtl8xxxu_rfregval *radio_b;
	int ret;

	efuse = (const struct rtl8192su_efuse *)priv->efuse_wifi.raw;
	rtl8xxxu_write8(priv, RTL8192SU_REG_AFE_XTAL_CTRL + 1, 0xdb);
	if (priv->chip_cut == 0)
		rtl8xxxu_write8(priv, RTL8192SU_REG_SPS1_CTRL + 3, 0x07);
	else
		rtl8xxxu_write8(priv, RTL8192SU_REG_RF_CTRL, 0x07);

	ret = rtl8192su_init_rf_path(priv, RF_A,
				     rtl8192su_radioa_1t_init_table);
	if (ret)
		return ret;

	if (priv->rf_paths > 1) {
		radio_b = efuse->board_type == 3 ?
			rtl8192su_radiob_green_init_table :
			rtl8192su_radiob_init_table;
		ret = rtl8192su_init_rf_path(priv, RF_B, radio_b);
		if (ret)
			return ret;
	}

	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_FPGA0_RF_MODE,
				   BIT(0), 1);
	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_FPGA0_RF_MODE,
				   BIT(1), 1);
	return 0;
}

static void rtl8192su_hw_configure(struct rtl8xxxu_priv *priv)
{
	u32 value;
	u8 min_space;

	value = rtl8xxxu_read32(priv, RTL8192SU_REG_INIRTS_RATE);
	value = (value & 0xff) | (0x00000fff << 8);
	rtl8xxxu_write32(priv, RTL8192SU_REG_INIRTS_RATE, value);
	rtl8xxxu_write8(priv, RTL8192SU_REG_BW_OPMODE, BIT(2));
	rtl8xxxu_write8(priv, RTL8192SU_REG_MLT, 0x8f);

	min_space = priv->tx_paths > 1 ? (0x13 << 3) : (0x0a << 3);
	rtl8xxxu_write8(priv, RTL8192SU_REG_AMPDU_MIN_SPACE, min_space);
}

void rtl8192su_set_ampdu_factor(struct rtl8xxxu_priv *priv, u8 ampdu_factor)
{
	u8 factor_level[] = {
		2, 4, 4, 7, 7, 13, 13, 13,
		2, 7, 7, 13, 13, 15, 15, 15,
		15, 0
	};
	u8 factor;
	int i;

	if (ampdu_factor > IEEE80211_HT_MAX_AMPDU_64K)
		return;

	factor = min_t(u8, 1 << (ampdu_factor + 2), 0x0f);
	for (i = 0; i < ARRAY_SIZE(factor_level) - 1; i++)
		factor_level[i] = min(factor_level[i], factor);

	for (i = 0; i < 8; i++)
		rtl8xxxu_write8(priv, RTL8192SU_REG_AGGLEN_LMT_L + i,
				factor_level[i * 2] |
				(factor_level[i * 2 + 1] << 4));
	rtl8xxxu_write8(priv, RTL8192SU_REG_AGGLEN_LMT_H,
			factor_level[16] | (factor_level[17] << 4));
}

static int rtl8192su_channel_group(int channel)
{
	if (channel <= 3)
		return 0;
	if (channel <= 8)
		return 1;
	return 2;
}

static int rtl8192su_add_signed_nibble(int power, u8 nibble)
{
	nibble &= 0x0f;
	if (nibble & BIT(3))
		power -= 16 - nibble;
	else
		power += nibble;

	return power;
}

static u32 rtl8192su_pg_offset(unsigned int group, unsigned int index)
{
	return rtl8192su_phy_pg_init[group * 7 + index].val;
}

static u32 rtl8192su_pack_tx_power(u32 offset, int base, int ant_diff)
{
	u32 value = 0;
	int lower = 0;
	int upper = 0x3f;
	int i;

	/*
	 * The BB applies the signed path-B difference after the path-A
	 * per-rate value.  Bound path A so that path B cannot underflow or
	 * overflow the six-bit TX-power index.
	 */
	if (ant_diff < 0)
		lower = -ant_diff;
	else
		upper -= ant_diff;

	for (i = 0; i < 4; i++) {
		int power = base + ((offset >> (i * 8)) & 0x7f);

		value |= clamp(power, lower, upper) << (i * 8);
	}

	return value;
}

static void rtl8192su_set_tx_power(struct rtl8xxxu_priv *priv, int channel,
				   bool ht40)
{
	static const u16 power_regs[] = {
		0x0e00, 0x0e04, 0x0e10, 0x0e14, 0x0e18, 0x0e1c
	};
	const struct rtl8192su_efuse *efuse;
	int base_a, base_b, legacy_base;
	int final_a, final_b;
	int group, regulatory, ant_diff;
	u8 legacy_diff, ht20_diff_a, ht20_diff_b;
	u8 edge_limit;
	unsigned int index;

	if (channel < 1 || channel > 14)
		return;

	efuse = (const struct rtl8192su_efuse *)priv->efuse_wifi.raw;
	group = rtl8192su_channel_group(channel);

	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_TX_AGC_CCK,
				   0x0000ff00,
				   min_t(u8, efuse->tx_pwr_cck[0][group],
					 0x3f));

	if (priv->tx_paths > 1) {
		base_a = efuse->tx_pwr_ht40_2t[0][group];
		base_b = efuse->tx_pwr_ht40_2t[1][group];
	} else {
		base_a = efuse->tx_pwr_ht40_1t[0][group];
		base_b = efuse->tx_pwr_ht40_1t[1][group];
	}

	if (efuse->version >= 2) {
		if (group == 0)
			legacy_diff = efuse->tx_pwr_ofdm_diff[0] & 0x0f;
		else if (group == 1)
			legacy_diff = efuse->tx_pwr_ofdm_diff_cont & 0x0f;
		else
			legacy_diff = efuse->tx_pwr_ofdm_diff[1] & 0x0f;
	} else {
		legacy_diff = efuse->pw_diff & 0x0f;
	}
	legacy_base = base_a + legacy_diff;

	if (!ht40 && efuse->version >= 2) {
		ht20_diff_a = efuse->tx_pwr_ht20_diff[group] & 0x0f;
		ht20_diff_b = efuse->tx_pwr_ht20_diff[group] >> 4;
		base_a = rtl8192su_add_signed_nibble(base_a, ht20_diff_a);
		base_b = rtl8192su_add_signed_nibble(base_b, ht20_diff_b);
	}

	if (efuse->version >= 4)
		regulatory = efuse->regulatory & 0x07;
	else if (efuse->version >= 2)
		regulatory = efuse->regulatory & 0x01;
	else
		regulatory = 0;

	final_a = base_a;
	final_b = base_b;
	if (regulatory == 3) {
		u8 edge_a = efuse->tx_pwr_edge[0][group];
		u8 edge_b = efuse->tx_pwr_edge[1][group];

		edge_a = ht40 ? edge_a >> 4 : edge_a & 0x0f;
		edge_b = ht40 ? edge_b >> 4 : edge_b & 0x0f;
		final_a += edge_a;
		final_b += edge_b;
	}
	ant_diff = priv->tx_paths > 1 ?
		clamp_t(int, final_b - final_a, -8, 7) : 0;

	for (index = 0; index < ARRAY_SIZE(power_regs); index++) {
		u32 offset;
		int base = index < 2 ? legacy_base : base_a;

		switch (regulatory) {
		case 0:
			offset = rtl8192su_pg_offset(0, index);
			break;
		case 1:
			offset = ht40 ? 0 :
				rtl8192su_pg_offset(group + 1, index);
			break;
		case 2:
			offset = 0;
			break;
		case 3: {
			u32 source = rtl8192su_pg_offset(0, index);
			u32 limited = 0;
			int byte;

			edge_limit = efuse->tx_pwr_edge[0][group];
			edge_limit = ht40 ? edge_limit >> 4 :
				edge_limit & 0x0f;
			for (byte = 0; byte < 4; byte++)
				limited |= min_t(u8,
					(source >> (byte * 8)) & 0x7f,
					edge_limit) << (byte * 8);
			offset = limited;
			break;
		}
		default:
			offset = rtl8192su_pg_offset(0, index);
			break;
		}

		rtl8192su_write_bbreg_mask(
			priv, power_regs[index], 0x7f7f7f7f,
			rtl8192su_pack_tx_power(offset, base, ant_diff));
	}

	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_TX_GAIN_STAGE,
				   0x00000fff, ant_diff & 0x0f);
}

static void rtl8192su_config_channel(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	int primary, center, path;
	u32 rf_value;
	u8 prime_sc = 0;
	u8 value;

	primary = hw->conf.chandef.chan->hw_value;
	center = primary;

	if (!conf_is_ht40(&hw->conf)) {
		value = rtl8xxxu_read8(priv, RTL8192SU_REG_BW_OPMODE);
		rtl8xxxu_write8(priv, RTL8192SU_REG_BW_OPMODE,
				value | BIT(2));
		rtl8192su_write_bbreg_mask(priv,
					   RTL8192SU_BB_FPGA0_RF_MODE,
					   BIT(0), 0);
		rtl8192su_write_bbreg_mask(priv,
					   RTL8192SU_BB_FPGA1_RF_MODE,
					   BIT(0), 0);
		if (priv->chip_cut >= 1)
			rtl8xxxu_write8(priv,
					RTL8192SU_BB_FPGA0_ANALOG2, 0x58);
	} else {
		value = rtl8xxxu_read8(priv, RTL8192SU_REG_BW_OPMODE);
		rtl8xxxu_write8(priv, RTL8192SU_REG_BW_OPMODE,
				value & ~BIT(2));

		if (conf_is_ht40_minus(&hw->conf)) {
			prime_sc = 2;
			center = primary - 2;
		} else {
			prime_sc = 1;
			center = primary + 2;
		}

		rtl8192su_write_bbreg_mask(priv,
					   RTL8192SU_BB_FPGA0_RF_MODE,
					   BIT(0), 1);
		rtl8192su_write_bbreg_mask(priv,
					   RTL8192SU_BB_FPGA1_RF_MODE,
					   BIT(0), 1);
		rtl8192su_write_bbreg_mask(priv,
					   RTL8192SU_BB_CCK0_SYSTEM,
					   BIT(4), prime_sc >> 1);
		rtl8192su_write_bbreg_mask(priv,
					   RTL8192SU_BB_OFDM1_LSTF,
					   0x00000c00, prime_sc);
		if (priv->chip_cut >= 1)
			rtl8xxxu_write8(priv,
					RTL8192SU_BB_FPGA0_ANALOG2, 0x18);
	}

	value = rtl8xxxu_read8(priv, RTL8192SU_REG_RRSR + 2);
	value &= ~GENMASK(6, 5);
	value |= prime_sc << 5;
	rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR + 2, value);

	for (path = RF_A; path < priv->rf_paths; path++) {
		rf_value = rtl8192su_read_rfreg(priv, path,
					       RTL8192SU_RF_CHNLBW);
		if (rf_value == 0xffffffff)
			continue;
		rf_value &= ~RTL8192SU_RF_CHAN_MASK;
		rf_value |= center & RTL8192SU_RF_CHAN_MASK;
		if (path == RF_A) {
			rf_value &= ~RTL8192SU_RF_BW_MASK;
			if (!conf_is_ht40(&hw->conf))
				rf_value |= RTL8192SU_RF_BW_20;
		}
		rtl8192su_write_rfreg(priv, path, RTL8192SU_RF_CHNLBW,
				      rf_value);
	}
}

static int rtl8192su_wait_fw_command(struct rtl8xxxu_priv *priv)
{
	u32 command = 0;
	int i;

	for (i = 0; i < 100; i++) {
		command = rtl8xxxu_read32(priv, RTL8192SU_REG_WFM5);
		if (!command)
			return 0;
		msleep(20);
	}

	dev_warn(&priv->udev->dev,
		 "firmware command 0x%08x timed out\n", command);
	return -ETIMEDOUT;
}

static int rtl8192su_send_fw_command(struct rtl8xxxu_priv *priv, u32 command)
{
	int ret;

	/*
	 * WFM5 shares address 0x2c0 with the indirect RF command register.
	 * Serialize legacy firmware commands with indirect RF accesses.
	 */
	mutex_lock(&priv->syson_indirect_access_mutex);
	ret = rtl8192su_wait_fw_command(priv);
	if (ret)
		goto out;
	rtl8xxxu_write32(priv, RTL8192SU_REG_WFM5, command);
	ret = rtl8192su_wait_fw_command(priv);
out:
	mutex_unlock(&priv->syson_indirect_access_mutex);
	return ret;
}

static void rtl8192su_modern_ra_command(struct rtl8xxxu_priv *priv,
					u16 command)
{
	u16 command_map;
	u32 parameter;

	mutex_lock(&priv->h2c_mutex);
	command_map = rtl8xxxu_read16(priv,
				      RTL8192SU_REG_LBUS_MON_ADDR);
	command_map &= ~(RTL8192SU_FW_RA_INIT_CTL |
			 RTL8192SU_FW_RA_BG_CTL |
			 RTL8192SU_FW_RA_N_CTL);

	if (command == RTL8192SU_FW_RA_INIT_CTL) {
		command_map |= RTL8192SU_FW_RA_INIT_CTL;
		rtl8xxxu_write16(priv, RTL8192SU_REG_LBUS_MON_ADDR,
				 command_map);
	} else {
		parameter = rtl8xxxu_read32(priv,
					    RTL8192SU_REG_LBUS_ADDR_MASK);
		parameter &= 0xffff0000;
		command_map |= command == RTL8192SU_FW_RA_N_CTL ?
			       RTL8192SU_FW_RA_N_CTL :
			       RTL8192SU_FW_RA_BG_CTL;

		rtl8xxxu_write32(priv, RTL8192SU_REG_LBUS_ADDR_MASK,
				 parameter);
		rtl8xxxu_write16(priv, RTL8192SU_REG_LBUS_MON_ADDR,
				 command_map);
	}

	/*
	 * FW_CMD_IO_CLR() in both soft-MAC references only waits and clears
	 * the driver's cached copy.  The firmware consumes the hardware bit.
	 */
	udelay(1000);
	mutex_unlock(&priv->h2c_mutex);
}

static void rtl8192su_update_dm_map(struct rtl8xxxu_priv *priv,
				    u16 clear, u16 set)
{
	u16 command_map;

	mutex_lock(&priv->h2c_mutex);
	command_map = rtl8xxxu_read16(priv, RTL8192SU_REG_LBUS_MON_ADDR);
	command_map &= ~clear;
	command_map |= set;
	rtl8xxxu_write16(priv, RTL8192SU_REG_LBUS_MON_ADDR, command_map);
	mutex_unlock(&priv->h2c_mutex);
}

static void rtl8192su_init_rate_adaptation(struct rtl8xxxu_priv *priv)
{
	u8 version = rtl8192su_firmware_version(priv);

	if (version >= 0x35) {
		rtl8192su_modern_ra_command(priv,
					    RTL8192SU_FW_RA_INIT_CTL);
	} else if (version == 0x34) {
		rtl8192su_send_fw_command(priv, RTL8192SU_FW_RA_INIT);
	} else {
		rtl8192su_send_fw_command(priv, RTL8192SU_FW_RA_RESET);
		rtl8192su_send_fw_command(priv, RTL8192SU_FW_RA_ACTIVE);
		rtl8192su_send_fw_command(priv, RTL8192SU_FW_RA_REFRESH);
	}
}

static void rtl8192su_refresh_rate_adaptation(struct rtl8xxxu_priv *priv,
					       bool n_mode)
{
	u8 version = rtl8192su_firmware_version(priv);

	if (version >= 0x35) {
		rtl8192su_modern_ra_command(
			priv, n_mode ? RTL8192SU_FW_RA_N_CTL :
			RTL8192SU_FW_RA_BG_CTL);
	} else if (version == 0x34) {
		rtl8192su_send_fw_command(
			priv, n_mode ? RTL8192SU_FW_RA_IOT_N_COMB :
			RTL8192SU_FW_RA_IOT_BG_COMB);
	} else {
		rtl8192su_send_fw_command(priv, RTL8192SU_FW_RA_REFRESH);
		rtl8192su_send_fw_command(
			priv, n_mode ? RTL8192SU_FW_RA_ENABLE_RSSI_MASK :
			RTL8192SU_FW_RA_DISABLE_RSSI_MASK);
	}
}

static void rtl8192su_update_rate_mask(struct rtl8xxxu_priv *priv,
				       u32 ramask, u8 rateid, int sgi,
				       int txbw_40mhz, u8 macid)
{
	u32 mask;
	u8 highest_mcs, short_gi_rate;
	bool n_mode;

	(void)rateid;
	(void)macid;

	n_mode = !!(ramask & 0xfffff000);
	if (n_mode) {
		if (priv->tx_paths > 1)
			mask = txbw_40mhz ? 0x0f0ff015 : 0x0f0ff005;
		else
			mask = txbw_40mhz ? 0x000ff015 : 0x000ff005;
		ramask &= mask;
	} else {
		ramask &= 0x00000fff;
	}

	if (priv->chip_cut == 0)
		ramask &= 0x0ffffff0;
	else
		ramask &= 0x0fffffff;

	if (n_mode && sgi) {
		ramask |= BIT(28);
		highest_mcs = fls((ramask & 0x0ffff000) >> 12) - 1;
		short_gi_rate = highest_mcs;
		short_gi_rate |= highest_mcs << 4;
		rtl8xxxu_write8(priv, RTL8192SU_REG_SG_RATE,
				short_gi_rate);
	}

	rtl8xxxu_write32(priv, RTL8192SU_REG_ARFR0, ramask);
	rtl8192su_refresh_rate_adaptation(priv, n_mode);
}

static void rtl8192su_write_mac_address(struct rtl8xxxu_priv *priv,
					u16 reg, const u8 *address)
{
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		rtl8xxxu_write8(priv, reg + i, address[i]);
}

int rtl8192su_init_device(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	int ret;

	ret = rtl8192su_power_on(priv);
	if (ret)
		return ret;

	ret = rtl8192su_download_firmware(priv);
	if (ret)
		goto error_power_off;

	rtl8192su_mac_config_after_firmware(priv);
	rtl8192su_init_mac(priv);
	priv->regrcr = rtl8xxxu_read32(priv, RTL8192SU_REG_RCR);
	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, RTL8192SU_CR_ENABLE);

	rtl8192su_init_phy_bb(priv);
	ret = rtl8192su_init_phy_rf(priv);
	if (ret)
		goto error_power_off;

	rtl8192su_hw_configure(priv);
	rtl8192su_set_tx_power(priv, 1, false);
	rtl8192su_write_mac_address(priv, RTL8192SU_REG_MACID,
				    priv->mac_addr);
	rtl8192su_init_rate_adaptation(priv);

	rtl8xxxu_write32(priv, RTL8192SU_REG_EDCA_BE, 0x005e4322);
	rtl8xxxu_write32(priv, RTL8192SU_REG_EDCA_BK, 0x005e4322);
	rtl8xxxu_write32(priv, RTL8192SU_REG_EDCA_VI, 0x005e4322);
	rtl8xxxu_write32(priv, RTL8192SU_REG_EDCA_VO, 0x005e4322);
	rtl8192su_send_fw_command(priv, RTL8192SU_FW_CCA_CHK_ENABLE);
	return 0;

error_power_off:
	rtl8192su_power_off(priv);
	return ret;
}

int rtl8192su_add_interface(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif)
{
	struct rtl8xxxu_vif *rtlvif = (struct rtl8xxxu_vif *)vif->drv_priv;
	struct rtl8xxxu_priv *priv = hw->priv;

	if (vif->type != NL80211_IFTYPE_STATION)
		return -EOPNOTSUPP;
	if (priv->vifs[0])
		return -EOPNOTSUPP;

	priv->vifs[0] = vif;
	rtlvif->port_num = 0;
	rtlvif->hw_key_idx = 0xff;
	ether_addr_copy(priv->mac_addr, vif->addr);
	rtl8192su_write_mac_address(priv, RTL8192SU_REG_MACID, vif->addr);
	rtl8xxxu_write8(priv, RTL8192SU_REG_MSR, RTL8192SU_MSR_NONE);
	return 0;
}

static void rtl8192su_set_media_status(struct rtl8xxxu_priv *priv, u8 status)
{
	u32 tcr;
	u8 msr;

	msr = rtl8xxxu_read8(priv, RTL8192SU_REG_MSR);
	msr &= ~0x03;
	msr |= status;
	rtl8xxxu_write8(priv, RTL8192SU_REG_MSR, msr);

	tcr = rtl8xxxu_read32(priv, RTL8192SU_REG_TCR);
	rtl8xxxu_write32(priv, RTL8192SU_REG_TCR, tcr & ~BIT(8));
	rtl8xxxu_write32(priv, RTL8192SU_REG_TCR, tcr | BIT(8));
}

static void rtl8192su_set_basic_rates(struct rtl8xxxu_priv *priv,
				      u32 basic_rates)
{
	u16 rates;
	u8 highest;

	rates = basic_rates & (priv->chip_cut == 0 ? 0x0150 : 0x015f);
	rates |= BIT(0);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR, rates & 0xff);
	rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR + 1, rates >> 8);
	highest = fls(rates) - 1;
	rtl8xxxu_write8(priv, RTL8192SU_REG_INIRTS_RATE, highest);
}

void rtl8192su_bss_info_changed(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
				struct ieee80211_bss_conf *bss_conf,
				u64 changed)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	struct ieee80211_sta *sta;
	u32 ramask, rcr;
	int sgi, txbw_40mhz;
	u8 value;

	if (changed & BSS_CHANGED_BSSID)
		rtl8192su_write_mac_address(priv, RTL8192SU_REG_BSSID,
					    bss_conf->bssid);

	if (changed & BSS_CHANGED_BASIC_RATES)
		rtl8192su_set_basic_rates(priv, bss_conf->basic_rates);

	if (changed & BSS_CHANGED_BEACON_INT)
		rtl8xxxu_write16(priv, RTL8192SU_REG_BCN_INTERVAL,
				 bss_conf->beacon_int);

	if (changed & BSS_CHANGED_ERP_PREAMBLE) {
		value = rtl8xxxu_read8(priv, RTL8192SU_REG_RRSR + 2);
		if (bss_conf->use_short_preamble)
			value |= BIT(7);
		else
			value &= ~BIT(7);
		rtl8xxxu_write8(priv, RTL8192SU_REG_RRSR + 2, value);
	}

	if (changed & BSS_CHANGED_ERP_SLOT)
		rtl8xxxu_write8(priv, RTL8192SU_REG_SLOT_TIME,
				bss_conf->use_short_slot ? 9 : 20);

	if (!(changed & BSS_CHANGED_ASSOC))
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	if (!vif->cfg.assoc) {
#else
	if (!bss_conf->assoc) {
#endif
		rtl8192su_set_media_status(priv, RTL8192SU_MSR_NONE);
		rcr = priv->regrcr & ~RTL8192SU_RCR_CBSSID;
		rtl8xxxu_write32(priv, RTL8192SU_REG_RCR, rcr);
		priv->regrcr = rcr;
		return;
	}

	rtl8192su_set_media_status(priv, RTL8192SU_MSR_STATION);
	rcr = priv->regrcr | RTL8192SU_RCR_CBSSID;
	rtl8xxxu_write32(priv, RTL8192SU_REG_RCR, rcr);
	priv->regrcr = rcr;
	rtl8192su_update_dm_map(priv, 0,
				RTL8192SU_FW_DIG_ENABLE_CTL |
				RTL8192SU_FW_SS_CTL);

	/*
	 * The rate mask is copied while the station is protected by RCU;
	 * no station pointer is retained after the read-side critical section.
	 */
	ramask = BIT(0);
	sgi = 0;
	txbw_40mhz = conf_is_ht40(&hw->conf);
	rcu_read_lock();
	sta = ieee80211_find_sta(vif, bss_conf->bssid);
	if (sta) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
		ramask = (sta->deflink.supp_rates[NL80211_BAND_2GHZ] &
			  0x0fff) |
			 ((u32)sta->deflink.ht_cap.mcs.rx_mask[0] << 12) |
			 ((u32)sta->deflink.ht_cap.mcs.rx_mask[1] << 20);
		sgi = !!(sta->deflink.ht_cap.cap &
			 (txbw_40mhz ? IEEE80211_HT_CAP_SGI_40 :
					  IEEE80211_HT_CAP_SGI_20));
#else
		ramask = (sta->supp_rates[NL80211_BAND_2GHZ] & 0x0fff) |
			 ((u32)sta->ht_cap.mcs.rx_mask[0] << 12) |
			 ((u32)sta->ht_cap.mcs.rx_mask[1] << 20);
		sgi = !!(sta->ht_cap.cap &
			 (txbw_40mhz ? IEEE80211_HT_CAP_SGI_40 :
					  IEEE80211_HT_CAP_SGI_20));
#endif
	}
	rcu_read_unlock();
	rtl8xxxu_update_ra_report(&priv->ra_report, fls(ramask) - 1, sgi,
				  txbw_40mhz ? RATE_INFO_BW_40 :
				  RATE_INFO_BW_20);
	rtl8192su_update_rate_mask(priv, ramask, 0, sgi,
				    txbw_40mhz, 0);
}

int rtl8192su_conf_tx(struct ieee80211_hw *hw, u16 queue,
		      const struct ieee80211_tx_queue_params *param)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	u16 reg;
	u8 acm_bit;
	u8 acm;
	u32 value;

	value = param->aifs | (fls(param->cw_min) << 8) |
		(fls(param->cw_max) << 12) | ((u32)param->txop << 16);

	switch (queue) {
	case IEEE80211_AC_VO:
		reg = RTL8192SU_REG_EDCA_VO;
		acm_bit = BIT(3);
		break;
	case IEEE80211_AC_VI:
		reg = RTL8192SU_REG_EDCA_VI;
		acm_bit = BIT(2);
		break;
	case IEEE80211_AC_BE:
		reg = RTL8192SU_REG_EDCA_BE;
		acm_bit = BIT(1);
		break;
	case IEEE80211_AC_BK:
		reg = RTL8192SU_REG_EDCA_BK;
		acm_bit = 0;
		break;
	default:
		return -EINVAL;
	}

	rtl8xxxu_write32(priv, reg, value);
	acm = rtl8xxxu_read8(priv, RTL8192SU_REG_ACM_CTRL);
	if (param->acm) {
		acm |= BIT(0) | acm_bit;
	} else {
		acm &= ~acm_bit;
		if (!(acm & (BIT(1) | BIT(2) | BIT(3))))
			acm &= ~BIT(0);
	}
	rtl8xxxu_write8(priv, RTL8192SU_REG_ACM_CTRL, acm);
	return 0;
}

void rtl8192su_configure_filter(struct ieee80211_hw *hw,
				unsigned int *total_flags)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	u32 rcr = priv->regrcr;

	if (*total_flags & FIF_ALLMULTI)
		rcr |= RTL8192SU_RCR_AM;
	else
		rcr &= ~RTL8192SU_RCR_AM;
	rcr |= RTL8192SU_RCR_AB;

	if (*total_flags & FIF_FCSFAIL)
		rcr |= RTL8192SU_RCR_ACRC32;
	else
		rcr &= ~RTL8192SU_RCR_ACRC32;

	if (*total_flags & FIF_BCN_PRBRESP_PROMISC)
		rcr &= ~RTL8192SU_RCR_CBSSID;
	else
		rcr |= RTL8192SU_RCR_CBSSID;

	if (*total_flags & FIF_CONTROL)
		rcr |= RTL8192SU_RCR_ACF;
	else
		rcr &= ~RTL8192SU_RCR_ACF;

	if (*total_flags & FIF_OTHER_BSS)
		rcr |= RTL8192SU_RCR_AAP;
	else
		rcr &= ~RTL8192SU_RCR_AAP;

	if (*total_flags & FIF_PSPOLL)
		rcr |= RTL8192SU_RCR_APWRMGT;
	else
		rcr &= ~RTL8192SU_RCR_APWRMGT;

	rtl8xxxu_write32(priv, RTL8192SU_REG_RCR, rcr);
	priv->regrcr = rcr;
	*total_flags &= FIF_ALLMULTI | FIF_FCSFAIL |
			FIF_BCN_PRBRESP_PROMISC | FIF_CONTROL |
			FIF_OTHER_BSS | FIF_PSPOLL | FIF_PROBE_REQ;
}

void rtl8192su_sw_scan_start(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;

	rtl8192su_update_dm_map(priv,
				RTL8192SU_FW_DIG_ENABLE_CTL |
				RTL8192SU_FW_HIGH_PWR_ENABLE_CTL |
				RTL8192SU_FW_SS_CTL, 0);
	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_OFDM0_XA_AGC,
				   0x000000ff, 0x17);
	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_OFDM0_XB_AGC,
				   0x000000ff, 0x17);
	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_CCK0_CCA,
				   0x00ff0000, 0x40);
}

void rtl8192su_sw_scan_complete(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	int channel;

	rtl8192su_write_bbreg_mask(priv, RTL8192SU_BB_CCK0_CCA,
				   0x00ff0000, 0xcd);
	rtl8192su_update_dm_map(priv, 0,
				RTL8192SU_FW_DIG_ENABLE_CTL |
				RTL8192SU_FW_HIGH_PWR_ENABLE_CTL |
				RTL8192SU_FW_SS_CTL);

	if (!hw->conf.chandef.chan)
		return;
	channel = hw->conf.chandef.chan->hw_value;
	rtl8192su_set_tx_power(priv, channel, conf_is_ht40(&hw->conf));
}

int rtl8192su_start(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	u32 rcr;

	rtl8xxxu_write8(priv, RTL8192SU_REG_TXPAUSE, 0);
	rcr = priv->regrcr | RTL8192SU_RCR_APM | RTL8192SU_RCR_AB |
	      RTL8192SU_RCR_ADF | RTL8192SU_RCR_AMF |
	      RTL8192SU_RCR_APP_PHYST;
	rtl8xxxu_write32(priv, RTL8192SU_REG_RCR, rcr);
	priv->regrcr = rcr;
	return 0;
}

void rtl8192su_stop(struct ieee80211_hw *hw)
{
	struct rtl8xxxu_priv *priv = hw->priv;

	rtl8xxxu_write8(priv, RTL8192SU_REG_TXPAUSE, 0xff);
	rtl8xxxu_write32(priv, RTL8192SU_REG_RCR, 0);
}

static void
rtl8192su_rx_parse_phystats(struct rtl8xxxu_priv *priv,
			    struct ieee80211_rx_status *rx_status,
			    struct rtl8723au_phy_stats *phy_stats,
			    u32 rxmcs, struct ieee80211_hdr *hdr,
			    bool crc_icv_err)
{
	const struct rtl8192su_rx_fwinfo *fwinfo;
	const struct rtl8192su_cck_phy *cck;
	int signal;

	(void)priv;
	(void)hdr;
	(void)crc_icv_err;

	fwinfo = (const struct rtl8192su_rx_fwinfo *)phy_stats;
	if (rxmcs <= DESC_RATE_11M) {
		u8 report;

		cck = (const struct rtl8192su_cck_phy *)phy_stats;
		report = cck->cck_agc_rpt >> 6;
		switch (report) {
		case 3:
			signal = -40 - (cck->cck_agc_rpt & 0x3e);
			break;
		case 2:
			signal = -20 - (cck->cck_agc_rpt & 0x3e);
			break;
		case 1:
			signal = -2 - (cck->cck_agc_rpt & 0x3e);
			break;
		default:
			signal = 14 - (cck->cck_agc_rpt & 0x3e);
			break;
		}
	} else {
		signal = ((fwinfo->pwdb_all >> 1) & 0x7f) - 110;
	}

	rx_status->signal = signal + 10;
}

static void
rtl8192su_fill_txdesc(struct ieee80211_hw *hw, struct ieee80211_hdr *hdr,
		      struct ieee80211_tx_info *tx_info,
		      struct rtl8xxxu_txdesc32 *tx_desc, bool sgi,
		      bool short_preamble, bool ampdu_enable,
		      u32 rts_rate, u8 macid)
{
	struct rtl8xxxu_priv *priv = hw->priv;
	__le16 frame_control = hdr->frame_control;
	u32 qsel, txdw1, txdw2, txdw3, txdw4, txdw5;
	u16 packet_size, sequence;
	u8 rate = DESC_RATE_1M;
	u8 tid = 0;
	bool first, last, qos, use_driver_rate;

	packet_size = le16_to_cpu(tx_desc->pkt_size);
	qsel = (le32_to_cpu(tx_desc->txdw1) & TXDESC_QUEUE_MASK) >>
		TXDESC_QUEUE_SHIFT;
	if (ieee80211_is_beacon(frame_control)) {
		qsel = TXDESC_QUEUE_BEACON;
	} else if (ieee80211_is_mgmt(frame_control)) {
		qsel = TXDESC_QUEUE_MGNT;
	} else {
		/*
		 * RTL8192S numbers BK/VI/VO as 1/4/6.  rtl8xxxu's common
		 * descriptor constants use the newer 2/5/7 numbering.
		 */
		switch (qsel) {
		case TXDESC_QUEUE_BK:
			qsel = 1;
			break;
		case TXDESC_QUEUE_VI:
			qsel = 4;
			break;
		case TXDESC_QUEUE_VO:
			qsel = 6;
			break;
		}
	}

	qos = ieee80211_is_data_qos(frame_control);
	if (qos)
		tid = *ieee80211_get_qos_ctl(hdr) &
			IEEE80211_QOS_CTL_TID_MASK;

	first = !(le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_FRAG);
	last = !ieee80211_has_morefrags(frame_control);
	if (ieee80211_is_nullfunc(frame_control) ||
	    ieee80211_is_ctl(frame_control))
		first = last = true;

	use_driver_rate = !ieee80211_is_data(frame_control) ||
		ieee80211_is_nullfunc(frame_control) ||
		is_multicast_ether_addr(ieee80211_get_DA(hdr));
	if (priv->chip_cut == 0 && use_driver_rate)
		rate = DESC_RATE_12M;

	memset(tx_desc, 0, sizeof(*tx_desc));
	tx_desc->pkt_size = cpu_to_le16(packet_size);
	tx_desc->pkt_offset = sizeof(*tx_desc);
	tx_desc->txdw0 = TXDESC_OWN;
	if (first)
		tx_desc->txdw0 |= TXDESC_FIRST_SEGMENT;
	if (last)
		tx_desc->txdw0 |= TXDESC_LAST_SEGMENT;

	txdw1 = (macid & 0x1f) | (qsel << 8);
	if (!qos)
		txdw1 |= RTL8192SU_TXDW1_NON_QOS;
	if (tx_info->control.hw_key) {
		switch (tx_info->control.hw_key->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			txdw1 |= 1 << 22;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			txdw1 |= 2 << 22;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			txdw1 |= 3 << 22;
			break;
		default:
			break;
		}
	}

	txdw2 = (macid & 0x1f) << 24;
	if (ampdu_enable && qos &&
	    test_bit(tid, priv->tid_tx_operational))
		txdw2 |= RTL8192SU_TXDW2_AGG_ENABLE;
	else
		txdw2 |= RTL8192SU_TXDW2_AGG_BREAK;

	sequence = IEEE80211_SEQ_TO_SN(le16_to_cpu(hdr->seq_ctrl));
	txdw3 = (u32)sequence << 16;

	txdw4 = (rts_rate & 0x3f) |
		(0xf << RTL8192SU_TXDW4_RTS_FB_LIMIT_SHIFT);
	if (tx_info->control.use_rts)
		txdw4 |= RTL8192SU_TXDW4_RTS_ENABLE;
	else if (tx_info->control.use_cts_prot)
		txdw4 |= RTL8192SU_TXDW4_CTS_ENABLE;

	if (ampdu_enable)
		txdw4 |= RTL8192SU_TXDW4_TX_HT;
	if ((sgi && ampdu_enable) ||
	    (short_preamble && use_driver_rate))
		txdw4 |= RTL8192SU_TXDW4_TX_SHORT;
	if (qos && ampdu_enable && conf_is_ht40(&hw->conf))
		txdw4 |= RTL8192SU_TXDW4_TX_BW;

	if (conf_is_ht40(&hw->conf)) {
		u8 prime_sc = conf_is_ht40_minus(&hw->conf) ? 2 : 1;

		if (!(txdw4 & RTL8192SU_TXDW4_TX_BW))
			txdw4 |= (u32)prime_sc <<
				 RTL8192SU_TXDW4_TX_SC_SHIFT;
		txdw4 |= (u32)prime_sc <<
			 RTL8192SU_TXDW4_RTS_SC_SHIFT;
	}
	if (use_driver_rate)
		txdw4 |= RTL8192SU_TXDW4_USER_RATE;

	txdw5 = 0x1f << RTL8192SU_TXDW5_FB_LIMIT_SHIFT;
	if (use_driver_rate)
		txdw5 |= (u32)rate << RTL8192SU_TXDW5_RATE_SHIFT;

	tx_desc->txdw1 = cpu_to_le32(txdw1);
	tx_desc->txdw2 = cpu_to_le32(txdw2);
	tx_desc->txdw3 = cpu_to_le32(txdw3);
	tx_desc->txdw4 = cpu_to_le32(txdw4);
	tx_desc->txdw5 = cpu_to_le32(txdw5);
	/* On RTL8192S, the low half of DWORD 7 is the buffer size. */
	tx_desc->csum = cpu_to_le16(packet_size);
}

static void rtl8192su_enable_rf(struct rtl8xxxu_priv *priv)
{
	rtl8192su_send_fw_command(priv, RTL8192SU_FW_BB_RESET_ENABLE);
	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, 0x37fc);
	rtl8xxxu_write8(priv, RTL8192SU_BB_PHY_CCA, 0x03);
	rtl8xxxu_write8(priv, RTL8192SU_REG_TXPAUSE, 0x00);
	rtl8xxxu_write8(priv, RTL8192SU_REG_SPS1_CTRL, 0x64);
}

static void rtl8192su_disable_rf(struct rtl8xxxu_priv *priv)
{
	u8 value;

	rtl8192su_send_fw_command(priv, RTL8192SU_FW_BB_RESET_DISABLE);

	value = rtl8xxxu_read8(priv, RTL8192SU_REG_LDOV12_CTRL);
	rtl8xxxu_write8(priv, RTL8192SU_REG_LDOV12_CTRL, value | BIT(0));
	rtl8xxxu_write8(priv, RTL8192SU_REG_SPS1_CTRL, 0x00);
	rtl8xxxu_write8(priv, RTL8192SU_REG_TXPAUSE, 0xff);

	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, 0x77fc);
	rtl8xxxu_write8(priv, RTL8192SU_BB_PHY_CCA, 0x00);
	udelay(100);
	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, 0x37fc);
	udelay(10);
	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, 0x77fc);
	udelay(10);
	rtl8xxxu_write16(priv, RTL8192SU_REG_CR, 0x57fc);
}

struct rtl8xxxu_fileops rtl8192su_fops = {
	.identify_chip = rtl8192su_identify_chip,
	.read_efuse = rtl8192su_read_efuse,
	.parse_efuse = rtl8192su_parse_efuse,
	.load_firmware = rtl8192su_load_firmware,
	.power_on = rtl8192su_power_on,
	.power_off = rtl8192su_power_off,
	.init_phy_bb = rtl8192su_init_phy_bb,
	.init_phy_rf = rtl8192su_init_phy_rf,
	.config_channel = rtl8192su_config_channel,
	.parse_rx_desc = rtl8xxxu_parse_rxdesc16,
	.parse_phystats = rtl8192su_rx_parse_phystats,
	.enable_rf = rtl8192su_enable_rf,
	.disable_rf = rtl8192su_disable_rf,
	.set_tx_power = rtl8192su_set_tx_power,
	.update_rate_mask = rtl8192su_update_rate_mask,
	.fill_txdesc = rtl8192su_fill_txdesc,
	.writeN_block_size = 128,
	.rx_agg_buf_size = 9100,
	.tx_desc_size = sizeof(struct rtl8xxxu_txdesc32),
	.rx_desc_size = sizeof(struct rtl8xxxu_rxdesc16),
	.mactable = rtl8192su_mac_init_table,
};
