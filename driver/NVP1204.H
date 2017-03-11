#include	<ntddk.h>
#include	<stdio.h>
#include	"..\\common\\ioctl.h"        // Get IOCTL interface definitions
#include	"..\\common\\Constant.h"
#include	"..\\Common\\DefineBase.h"

typedef unsigned long		DWORD;
typedef unsigned short		WORD;
typedef unsigned char		BYTE;
typedef unsigned int		UINT;
typedef BOOLEAN				BOOL;

typedef volatile unsigned long ULONG_REG, *PULONG_REG;

#include "..\\Common\\DefineStruct.h"

#if     !defined(__NVP1204__)
#define __NVP1204_H__

#define NT_DEVICE_NAME	L"\\Device\\NVP1204%d"
#define DOS_DEVICE_NAME L"\\DosDevices\\NVP1204%d"
			
// driver local data structure specific to each device object
typedef struct _DEVICE_EXTENSION 
{
	PDEVICE_OBJECT		m_pLowerDeviceObject;		// The top of the stack
	PDEVICE_OBJECT		m_pPdo;						// the PDO

	IO_REMOVE_LOCK		m_RemoveLock;				// removal control locking structure	
	UNICODE_STRING		m_win32DeviceName;
	
	PDMA_ADAPTER		m_pAdapterObject;			// DMA adapter object
	PKINTERRUPT			m_pInterruptObject;			// interrupt object
	KDPC				m_DpcObjectVideo;
	KDPC				m_DpcObjectAudio;

	PKEVENT				m_pVideoEvent[MAX_DECODER];
	PKEVENT				m_pAudioEvent;

	PMDL				m_pMDLVideoBuffer[MAX_DECODER];
	PMDL				m_pMDLAudioBuffer;
	
	VCAP_STATUS_INFO	*m_pVCAPStatus[MAX_DECODER][MAX_VIDEO_QUEUE];
	ACAP_STATUS_INFO	*m_pACAPStatus[MAX_AUDIO_QUEUE];
	
	LARGE_INTEGER		m_liFrequency;
	LARGE_INTEGER		m_liStartCounter;
	LARGE_INTEGER		m_liStartLocalTime;
	LARGE_INTEGER		m_liIntCounter;
	LARGE_INTEGER		m_liIntLocalTime;
	LARGE_INTEGER		m_liDiffCounter;
	
	PHYSICAL_ADDRESS	m_PhysDisplayMemBase;
	PHYSICAL_ADDRESS	m_VideoPhysicalAddress[MAX_DECODER];
	PHYSICAL_ADDRESS	m_AudioPhysicalAddress;
	PHYSICAL_ADDRESS	m_EncoderPhysicalAddress[MAX_DECODER];

	//***********************
	DWORD		m_dwMemsize;				// size of mapped memory area
	DWORD		m_dwPhysDisplayAddress;
	DWORD		m_dwVideoBuffer[MAX_DECODER];
	DWORD		m_dwAudioBuffer;
	DWORD		m_dwYOffSetSize;
	DWORD		m_dwUVOffSetSize;
	DWORD		m_dwOneQueueSize;
	DWORD		m_dwVideoDMAEnable;
	DWORD		m_dwAudioDMAEnable;
	DWORD		m_dwVideoIntEnable;
	DWORD		m_dwAudioIntEnable;
	DWORD		m_dwCapVideoIntStatus;
	DWORD		m_dwCapAudioIntStatus;

	DWORD		m_dwVideoIntMask[MAX_DECODER];
	DWORD		m_dwVideoDMAMask[MAX_DECODER];
	DWORD		m_dwVideoFormat[MAX_DECODER];
	DWORD		dwLowDateTime[MAX_VIDEO_CHANNEL];

	BOOL		m_bOddSended[MAX_VIDEO_CHANNEL];
	BOOL		m_bEvenSended[MAX_VIDEO_CHANNEL];
	BOOL		m_bInvalidcurChannel[MAX_DECODER];

	BYTE		m_byevencount[MAX_VIDEO_CHANNEL];
	BYTE		m_byoddcount[MAX_VIDEO_CHANNEL];

	BYTE		m_byDeviceID;				// DeviceID.
	BYTE		m_byMaxChannel;
	BYTE		m_byMaxDecoder;
	BYTE		*m_pbyMembase;				// mapped memory base address
	BYTE		*m_pVCapCurQCnt[MAX_DECODER];
	BYTE		*m_pACapCurQCnt;
	BYTE		*m_pbyVideoBuffer[MAX_DECODER];
	BYTE		*m_pbyUserVideoBuffer[MAX_DECODER];
	BYTE		*m_pbyAudioBuffer;
	BYTE		*m_pbyUserAudioBuffer;
	BYTE		m_byNVP1114_ID[2];
	BYTE		*m_pbyUserEncoderBuffer[MAX_DECODER];
	BYTE		m_byField_HWidth;

	BYTE		m_byCaptureSize[MAX_VIDEO_CHANNEL];
	BYTE		m_byVideoFormat;
	BYTE		m_byDataFormat;
	BYTE		m_byVideoVScale;
	BYTE		m_pbyChannelTable[MAX_DECODER][MAX_CHANNEL_CHANGEABLE];	

	BYTE		m_byCapField[MAX_VIDEO_CHANNEL];

	BYTE		m_byFrame_Count[MAX_VIDEO_CHANNEL];
	BYTE		m_byNextVideoQCount[MAX_DECODER];
	BYTE		m_byOldVideoQCount[MAX_DECODER];
	BYTE		m_byCurVideoQCount[MAX_DECODER];
	BYTE		m_byCurAudioQueueCount;

	BYTE		m_byNextChannelCnt[MAX_DECODER];
	BYTE		m_byCurChannelCnt[MAX_DECODER];
	BYTE		m_byBrightness[MAX_VIDEO_CHANNEL];	
	BYTE		m_byContrast[MAX_VIDEO_CHANNEL];
	BYTE		m_byColor[MAX_VIDEO_CHANNEL];
	BYTE		m_byHue[MAX_VIDEO_CHANNEL];

	BYTE		m_byRecPathBrightness[MAX_DECODER];	
	BYTE		m_byRecPathContrast[MAX_DECODER];
	BYTE		m_byRecPathColor[MAX_DECODER];
	BYTE		m_byRecPathHue[MAX_DECODER];		
	BYTE		m_byISRChannel[MAX_DECODER];
	BYTE		m_bVideoBW[MAX_VIDEO_CHANNEL];
	BYTE		m_bDecoderBW[MAX_DECODER];
	BYTE		m_byRealTime;
	BYTE		m_byOldISRChannel[MAX_DECODER];
	
	BOOL		m_bReal_Time[MAX_DECODER];
	BOOL		m_bIrqshare;
	BOOL		m_bStarted;
	BOOL		m_bRemoved;
	BOOL		m_bPortWasMapped;
	BOOL		m_bBufferAllocate;
	BOOL		m_bMDLAllocate;
	BOOL		m_bVCapStarted;
	BOOL		m_bACapStarted;
	BOOL		m_bSampleFrequency;
	BOOL		m_bSync[MAX_VIDEO_CHANNEL];
	BOOL		m_bVCapIntDone[MAX_DECODER];
	BOOL		m_bACapIntDone;	
	BOOL		m_bIntRet;
	BOOL		m_bCHSwitch[MAX_DECODER];
	BOOL		m_bQuSwitchSend[MAX_DECODER];	
	BOOL		m_bfield[MAX_VIDEO_CHANNEL];
	BOOL		m_bOdd[MAX_VIDEO_CHANNEL];
	BOOL		m_bEven[MAX_VIDEO_CHANNEL];
	BOOL		m_bLoss[MAX_VIDEO_CHANNEL];	
	
	BOOL		m_bVideoDMA[MAX_DECODER];
	BOOL		m_bAudioDMA;

	WORD		m_wImageSize[MAX_VIDEO_CHANNEL];
	WORD		m_wImageHeight;
	PMDL		m_pMDLEncoderBuffer[MAX_DECODER];
	DWORD		m_dwEncoderBuffer[MAX_DECODER];
	BYTE		*m_pbyEncoderBuffer[MAX_DECODER];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define arraysize(p) (sizeof(p)/sizeof((p)[0]))

#if DBG
#define DebugPrint(_x_) DbgPrint ("NVP1204:"); DbgPrint _x_;
#define TRAP() DbgBreakPoint()
#else
#define DebugPrint(_x_)
#define TRAP()
#endif

//-----------------------------------------------------------------------
NTSTATUS	DriverEntry(IN  PDRIVER_OBJECT DriverObject, IN  PUNICODE_STRING RegistryPath);
NTSTATUS	AL_DispatchCreate(IN PDEVICE_OBJECT pDO, IN PIRP pIrp);
NTSTATUS	AL_DispatchClose(IN PDEVICE_OBJECT pDO, IN PIRP pIrp);
NTSTATUS	AL_DispatchWrite(IN PDEVICE_OBJECT pDO, IN PIRP pIrp);
NTSTATUS	AL_DispatchControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
NTSTATUS	AL_AddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS	AL_DispatchPnp(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS	AL_StartDevice(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS	AL_StopDevice(IN PDEVICE_OBJECT pDeviceObject, IN PIRP Irp);
NTSTATUS	AL_DispatchPower(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS	AL_CompletionRoutine(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context);
NTSTATUS	MDLAllocateCapBuf(PDEVICE_OBJECT pDeviceObject);
NTSTATUS	DmaMemAllocPool(PDEVICE_OBJECT pDeviceObject);
NTSTATUS	AddDevice(IN PDRIVER_OBJECT pDriverObject, IN PDEVICE_OBJECT pdo);
NTSTATUS	DispatchPnp(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
NTSTATUS	DispatchPower(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
NTSTATUS	CompleteRequest(IN PIRP pIrp, IN NTSTATUS status, IN ULONG_PTR info);
NTSTATUS	KKGetPhysicalAddress(IN unsigned char *pR0Address, OUT PHYSICAL_ADDRESS *PhysAddress);

VOID		AL_Unload(IN PDRIVER_OBJECT DriverObject);
VOID		DpcForIsr_Video(PKDPC Dpc, PDEVICE_OBJECT pDeviceObject, PIRP junk, PDEVICE_EXTENSION pdx);
VOID		DpcForIsr_Audio(PKDPC Dpc, PDEVICE_OBJECT pDeviceObject, PIRP junk, PDEVICE_EXTENSION pdx);
VOID		DriverUnload(IN PDRIVER_OBJECT pDriverObject);

BOOL		OnInterrupt(PKINTERRUPT pInterruptObject, PDEVICE_EXTENSION pdx);

void		MDLFreeCapBuf(PDEVICE_OBJECT pDeviceObject);
void		DmaMemFreePool(PDEVICE_OBJECT pDeviceObject);
void		MemberVariableInit(PDEVICE_EXTENSION pdx);
void		RealTimeMode(PDEVICE_EXTENSION pdx, BYTE bydecoder);
void		NonRealTimeMode(PDEVICE_EXTENSION pdx);
void		SetCameraColor(PDEVICE_EXTENSION pdx, BYTE bydecoder, BYTE bychannel);
//-----------------------------------------------------------------------
void		InterruptMaskControl(PDEVICE_EXTENSION pdx, DWORD InterruptCtrlCode);
void		ImageCaptureControl(PDEVICE_EXTENSION pdx, BYTE CapCtrlCode, BYTE DmaCtrlCode);
void		InterruptStatusClear(PDEVICE_EXTENSION pdx);
void		SetLocalRegister(PDEVICE_EXTENSION pdx);
void		SetBrightness(PDEVICE_EXTENSION pdx, BYTE byChannel);
void		SetContrast(PDEVICE_EXTENSION pdx, BYTE byChannel);
void		SetColor(PDEVICE_EXTENSION pdx, BYTE byChannel);
void		SetHue(PDEVICE_EXTENSION pdx, BYTE byChannel);
void		GetCurrentSystemTime(DWORD* pdwHiTime, DWORD* pdwLowTime);
void		DelayPer100nanoseconds(PDEVICE_EXTENSION pdx, DWORD DelayTime);
void		DelayMicroSecond(int	mSec);
BOOL 		LocalTimeInit(PDEVICE_EXTENSION pdx);

BOOL		OnInterrupt(PKINTERRUPT pInterruptObject, PDEVICE_EXTENSION pdx);

PCHAR		PnPMinorFunctionString(BYTE MinorFunction);
//-----------------------------------------------------------------------
BOOL		I2C_Init(PDEVICE_EXTENSION pdx);
BOOL		I2C_Read(PDEVICE_EXTENSION pdx, BYTE bPort, BYTE bDevAddr, BYTE bSubAddr, BYTE *bData);
BOOL		I2C_Write(PDEVICE_EXTENSION pdx, BYTE bPort, BYTE bDevAddr, BYTE bSubAddr, BYTE bData);

BOOL		I2C_P0_Read(PDEVICE_EXTENSION pdx, BYTE bDevAddr, BYTE bSubAddr, BYTE *bData);
BOOL		I2C_P0_Write(PDEVICE_EXTENSION pdx, BYTE bDevAddr, BYTE bSubAddr, BYTE bData);

void		I2C_P0_Start(PDEVICE_EXTENSION pdx);
void		I2C_P0_Stop(PDEVICE_EXTENSION pdx);
void		I2C_P0_SetSDAMode(PDEVICE_EXTENSION pdx, DWORD dwMode);
void		I2C_P0_SetSDA(PDEVICE_EXTENSION pdx, DWORD dwSig);
BYTE		I2C_P0_GetSDA(PDEVICE_EXTENSION pdx);
void		I2C_P0_SetSCL(PDEVICE_EXTENSION pdx, DWORD dwSig);
void		I2C_P0_TransmitByte(PDEVICE_EXTENSION pdx, BYTE bData);
BOOL		I2C_P0_ReadAck(PDEVICE_EXTENSION pdx);
BYTE		I2C_P0_ReceiveByte(PDEVICE_EXTENSION pdx);
void		I2C_P0_SendOutAck(PDEVICE_EXTENSION pdx, DWORD dwSig);

BOOL		I2C_P1_Read(PDEVICE_EXTENSION pdx, BYTE bDevAddr, BYTE bSubAddr, BYTE *bData);
BOOL		I2C_P1_Write(PDEVICE_EXTENSION pdx, BYTE bDevAddr, BYTE bSubAddr, BYTE bData);

void		I2C_P1_Start(PDEVICE_EXTENSION pdx);
void		I2C_P1_Stop(PDEVICE_EXTENSION pdx);
void		I2C_P1_SetSDAMode(PDEVICE_EXTENSION pdx, DWORD dwMode);
void		I2C_P1_SetSDA(PDEVICE_EXTENSION pdx, DWORD dwSig);
BYTE		I2C_P1_GetSDA(PDEVICE_EXTENSION pdx);
void		I2C_P1_SetSCL(PDEVICE_EXTENSION pdx, DWORD dwSig);
void		I2C_P1_TransmitByte(PDEVICE_EXTENSION pdx, BYTE bData);
BOOL		I2C_P1_ReadAck(PDEVICE_EXTENSION pdx);
BYTE		I2C_P1_ReceiveByte(PDEVICE_EXTENSION pdx);
void		I2C_P1_SendOutAck(PDEVICE_EXTENSION pdx, DWORD dwSig);

DWORD		GetTick();
DWORD		GetTimeInterval(DWORD dwPrevTime, DWORD dwCurrTime);
BOOL		GetDecoderSyncData(PDEVICE_EXTENSION pdx, BYTE byChannel, BYTE *pbyRet);
BOOL		ColorControl(PDEVICE_EXTENSION pdx, BYTE byDecoder);
void		Decoder_Init(PDEVICE_EXTENSION pdx);
void		Chip_Reset(PDEVICE_EXTENSION pdx, DWORD dwReset);
void		SetCameraChange(PDEVICE_EXTENSION pdx, BYTE byDecoder);
void		SetWidthSelect(PDEVICE_EXTENSION pdx, BYTE byDecoder, BYTE byChannel);
void		AudioDMAStart(PDEVICE_EXTENSION pdx, BYTE byDecoder);
void		AudioDMAStop(PDEVICE_EXTENSION pdx, BYTE byDecoder);
void		VideoDMAStart(PDEVICE_EXTENSION pdx, BYTE byDecoder);
void		VideoDMAStop(PDEVICE_EXTENSION pdx, BYTE byDecoder);
void		AllDMADisable(PDEVICE_EXTENSION pdx);

#endif


