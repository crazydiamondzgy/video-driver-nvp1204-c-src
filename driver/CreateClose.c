#include "NVP1204.h"

extern LONG		g_dwDeviceCount;      // Device Number

//=========================================================================================================================//
NTSTATUS AL_DispatchCreate(IN PDEVICE_OBJECT pDO, IN PIRP pIrp)
{
	PDEVICE_EXTENSION	pdx;
	PIO_STACK_LOCATION	pIrpStack;
	NTSTATUS			Status;
	DWORD				i = 0, j, dwSystemTime;
	LARGE_INTEGER		liSystemTime, liLocalTime;
	
	pIrp->IoStatus.Information = 0;
	pdx = (PDEVICE_EXTENSION)pDO->DeviceExtension;    // Get local info struct
	KdPrint(("Video::AL_DispatchCreate::g_dwDeviceCount[%d]:pdx->m_byDeviceID[%d]:pdx->m_byMaxDecoder[%X]\n", g_dwDeviceCount, pdx->m_byDeviceID, pdx->m_byMaxDecoder));

	Status = IoAcquireRemoveLock(&pdx->m_RemoveLock, pIrp);
	if(!NT_SUCCESS(Status))
	{
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = Status;
		IoCompleteRequest (pIrp, IO_NO_INCREMENT);
		return Status;
	}
	
	if(!pdx->m_bStarted)
	{
		// We fail all the IRPs that arrive before the device is started.
		pIrp->IoStatus.Status = Status = STATUS_DEVICE_NOT_READY;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		IoReleaseRemoveLock(&pdx->m_RemoveLock, pIrp);
		return Status;
	}
	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

	pdx->m_pAudioEvent = 0;
	pdx->m_dwPhysDisplayAddress = 0;	

	AllDMADisable(pdx);

	Status = MDLAllocateCapBuf(pDO);

	pIrp->IoStatus.Status = Status;
	// Don't boost priority when returning since this took little time.
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	IoReleaseRemoveLock(&pdx->m_RemoveLock, pIrp);

	KeQuerySystemTime(&liSystemTime);
	ExSystemTimeToLocalTime(&liSystemTime, &liLocalTime);
	dwSystemTime = (DWORD)(liSystemTime.QuadPart / (10 * 1000));
	
	for(i=0; i<MAX_DECODER; i++)
	{
		pdx->m_byRecPathBrightness[i]	= (BYTE)0x00;
		pdx->m_byRecPathContrast[i]		= (BYTE)0x00;
		pdx->m_byRecPathColor[i]		= (BYTE)0x00;
		pdx->m_byRecPathHue[i]			= (BYTE)0x00;
	}	

	KdPrint(("liSystemTime.QuadPart = %I64X, liLocalTime.QuadPart = %I64X, wSystemTime = %X\n", liSystemTime.QuadPart, liLocalTime.QuadPart, dwSystemTime));
	return Status;
}

//=========================================================================================================================//
NTSTATUS AL_DispatchClose(IN PDEVICE_OBJECT pDO, IN PIRP pIrp)
{
	PDEVICE_EXTENSION		pdx;
	PIO_STACK_LOCATION		pIrpStack;
	NTSTATUS				Status;
	
	KdPrint(("Video::AL_DispatchClose\n"));
	pIrp->IoStatus.Information = 0;
	pdx = (PDEVICE_EXTENSION)pDO->DeviceExtension;    // Get local info struct
	Status = IoAcquireRemoveLock(&pdx->m_RemoveLock, pIrp);
	if(!NT_SUCCESS (Status)) 
	{
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = Status;
		IoCompleteRequest (pIrp, IO_NO_INCREMENT);
		return Status;
	}
	
	if(!pdx->m_bStarted) 
	{
		// We fail all the IRPs that arrive before the device is started.
		pIrp->IoStatus.Status = Status = STATUS_DEVICE_NOT_READY;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		IoReleaseRemoveLock(&pdx->m_RemoveLock, pIrp);       
		return Status;
	}
	
	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	
	AllDMADisable(pdx);
	MDLFreeCapBuf(pDO);
	
	// We're done with I/O request.  Record the Status of the I/O action.
	pIrp->IoStatus.Status = Status;
	
	// Don't boost priority when returning since this took little time.
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	IoReleaseRemoveLock(&pdx->m_RemoveLock, pIrp);
	return Status;
}

//=========================================================================================================================//
NTSTATUS AL_DispatchWrite(PDEVICE_OBJECT fdo, PIRP Irp)
{
	PDEVICE_EXTENSION		pdx = (PDEVICE_EXTENSION)fdo->DeviceExtension;	
	BYTE					*R0Address;
	PIO_STACK_LOCATION		stack = IoGetCurrentIrpStackLocation(Irp);

	R0Address = (BYTE *)MmGetMdlVirtualAddress(Irp->MdlAddress);
	if(R0Address != NULL)
		KKGetPhysicalAddress(R0Address, &pdx->m_PhysDisplayMemBase);

	pdx->m_dwPhysDisplayAddress = (DWORD)pdx->m_PhysDisplayMemBase.LowPart;
	KdPrint(("NVP1204 DispatchWrite::pdx->m_dwPhysDisplayAddress[%X], stack->Parameters.Write.Length = %X\n", pdx->m_dwPhysDisplayAddress, stack->Parameters.Write.Length));
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}

//=========================================================================================================================//
void AllDMADisable(PDEVICE_EXTENSION pdx)
{
	pdx->m_dwVideoDMAEnable = ALL_CAP_DMA_DISABLE;
	pdx->m_dwAudioDMAEnable = ALL_CAP_DMA_DISABLE;
	pdx->m_dwVideoIntEnable = ALL_CAP_INT_DISABLE;
	pdx->m_dwAudioIntEnable = ALL_CAP_INT_DISABLE;
	
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_DMA_ENABLE), pdx->m_dwVideoDMAEnable);
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_DMA_ENABLE), pdx->m_dwAudioDMAEnable);
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + VIDEO_INT_ENABLE), pdx->m_dwVideoIntEnable);
	WRITE_REGISTER_ULONG((DWORD*)(pdx->m_pbyMembase + AUDIO_INT_ENABLE), pdx->m_dwAudioIntEnable);
}
