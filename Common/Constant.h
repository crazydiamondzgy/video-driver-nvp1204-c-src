/***********************************************************************/
/* Define PCI Related Constant
/***********************************************************************/
#ifndef min
#define	min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef max
#define	max(a,b) (((a)>(b))?(a):(b))
#endif
/***********************************************************************/
#define YBASE_ADD0_A 			0x100
#define YBASE_ADD0_A_U 			0x104
#define YBASE_ADD0_A_V 			0x108

#define YBASE_ADD0_B 			0x110
#define YBASE_ADD0_B_U 			0x114
#define YBASE_ADD0_B_V 			0x118

#define YBASE_ADD1_A 			0x120
#define YBASE_ADD1_A_U 			0x124
#define YBASE_ADD1_A_V 			0x128

#define YBASE_ADD1_B 			0x130
#define YBASE_ADD1_B_U 			0x134
#define YBASE_ADD1_B_V 			0x138

#define YBASE_ADD2_A 			0x140
#define YBASE_ADD2_A_U 			0x144
#define YBASE_ADD2_A_V 			0x148

#define YBASE_ADD2_B 			0x150
#define YBASE_ADD2_B_U 			0x154
#define YBASE_ADD2_B_V 			0x158

#define YBASE_ADD3_A 			0x160
#define YBASE_ADD3_A_U 			0x164
#define YBASE_ADD3_A_V 			0x168

#define YBASE_ADD3_B 			0x170
#define YBASE_ADD3_B_U 			0x174
#define YBASE_ADD3_B_V 			0x178

#define VIDEO0_FORMAT 			0x190
#define VIDEO1_FORMAT 			0x194
#define VIDEO2_FORMAT 			0x198
#define VIDEO3_FORMAT 			0x19C

#define VIDEO_DMA_OFFSET0		0x1B0
#define VIDEO_DMA_OFFSET1		0x1B4
#define VIDEO_DMA_OFFSET2		0x1B8
#define VIDEO_DMA_OFFSET3		0x1BC

#define VIDEO_DMA_SIZE0			0x1C0
#define VIDEO_DMA_SIZE1			0x1C4
#define VIDEO_DMA_SIZE2			0x1C8
#define VIDEO_DMA_SIZE3			0x1CC

#define VIDEO_DMA_ENABLE		0x1D0
#define VIDEO_INT_ENABLE		0x1D4
#define VIDEO_INT_STATUS		0x1D8

#define AUDIO_BASE_ADD0_A		0x200
#define AUDIO_BASE_ADD0_B		0x204
#define AUDIO_BASE_ADD1_A		0x208
#define AUDIO_BASE_ADD1_B		0x20C
#define AUDIO_DMA_SIZE0			0x210
#define AUDIO_DMA_ENABLE		0x220
#define AUDIO_INT_ENABLE		0x224
#define AUDIO_INT_STATUS		0x228

#define REG_I2C					0x240

#define ALL_INT_ENABLE			0x264

#define GPIO_DIR				0x270
#define GPIO_OUT				0x274
#define GPIO_IN					0x278

#define MUX_DIR					0x270
#define MUX_OUT					0x274
#define MUX_IN					0x278

#define VIDEO_HSCALE1			0x620
#define VIDEO_HSCALE2			0x624
#define VIDEO_HSCALE3			0x628
#define VIDEO_HSCALE4			0x62C

#define VIDEO_VSCALE1			0x630
#define VIDEO_VSCALE2			0x634
#define VIDEO_VSCALE3			0x638
#define VIDEO_VSCALE4			0x63C

#define AD_IRQ0					(0x3<<0)
#define AD_IRQ0_A				(1<<0)
#define AD_IRQ0_B				(1<<1)

#define ALL_CAP_INT_DISABLE		0x00000000
#define VCAP_INT_ENABLE			0x00000055

#define VCAP_INT_STATUS			0x00000055
#define ACAP_INT_STATUS			0x0000FFFF

#define ALL_CAP_DMA_DISABLE		0x00000000

/***********************************************************************/
/* I2C Control Constant
/***********************************************************************/
#define I2CR(xdid)			(xdid|0x01)
#define I2CW(xdid)			(xdid&0xFE)

#define I2C_MODE_WRITE		0
#define I2C_MODE_READ		1

#define GPIO_DIR_OUT		0x000000FF
#define GPIO_DIR_IN			0xFFFFFFFF

#define MUX_DIR_OUT			0xFFFFFF00
#define MUX_DIR_IN			0xFFFFFFFF
///////////////////////////////////////////////////////////////////////////////
// Device extension structure
enum DEVSTATE 
{
	STOPPED,								// device stopped
	WORKING,								// started and working
	PENDINGSTOP,							// stop pending
	PENDINGREMOVE,							// remove pending
	SURPRISEREMOVED,						// removed by surprise
	REMOVED,								// removed
};

//Host Interface
#define		RESET_DECODER						0x00000100
////////////////////////////////////////////////////////////////
#define		I2C_NVP1114_DEVICE_1			0x70
#define		I2C_NVP1114_DEVICE_2			0x72

#define		NVP1004_DEV_OFFSET				0x40

#define		NVP1204_BRIGHT					0x520
#define		NVP1204_CONTRAST				0x520
#define		NVP1204_HUE						0x520
#define		NVP1204_COLOR					0x524
