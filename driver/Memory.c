#include "NVP1204.h"
//=========================================================================================================================//
NTSTATUS DmaMemAllocPool(PDEVICE_OBJECT pDeviceObject)
{
	NTSTATUS			status	= STATUS_SUCCESS;
	PDEVICE_EXTENSION	pdx		= (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	BYTE				i, j=0;

	if(pdx->m_bBufferAllocate == TRUE)
	{
		DmaMemFreePool(pDeviceObject);
	}
	
	//  Encoder
	for(i=0; i<MAX_DECODER; i++)
	{
		pdx->m_pbyEncoderBuffer[i] = (BYTE *)(*pdx->m_pAdapterObject->DmaOperations->AllocateCommonBuffer)(pdx->m_pAdapterObject, MAX_ENCODER_SIZE * PAGESIZE, &pdx->m_EncoderPhysicalAddress[i], FALSE);
		if(!pdx->m_pbyEncoderBuffer[i])
		{
			KdPrint(("m_pbyEncoderBuffer mem Allocate Fail \n"));		
			for(j=0; j<i; j++)
				(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_ENCODER_SIZE * PAGESIZE, pdx->m_EncoderPhysicalAddress[j], pdx->m_pbyEncoderBuffer[j], FALSE);
			status = STATUS_INVALID_DEVICE_REQUEST;
			return status;
		}
		pdx->m_dwEncoderBuffer[i] = (DWORD)pdx->m_EncoderPhysicalAddress[i].LowPart;
	}
	
	//Video
	for(i=0; i<MAX_DECODER; i++)
	{
		pdx->m_pbyVideoBuffer[i] = (BYTE *)(*pdx->m_pAdapterObject->DmaOperations->AllocateCommonBuffer)(pdx->m_pAdapterObject, MAX_VIDEO_CAP_SIZE * PAGESIZE, &pdx->m_VideoPhysicalAddress[i], FALSE);
		if(!pdx->m_pbyVideoBuffer[i])
		{
			KdPrint(("m_pbyVideoBuffer[%d] mem Allocate Fail\n", i));
			
			for(j=0; j<i; j++)
			{
				(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_VIDEO_CAP_SIZE * PAGESIZE, pdx->m_VideoPhysicalAddress[j], pdx->m_pbyVideoBuffer[j], FALSE);
			}
			for(j=0; j<MAX_DECODER; j++)
				(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_ENCODER_SIZE * PAGESIZE, pdx->m_EncoderPhysicalAddress[j], pdx->m_pbyEncoderBuffer[j], FALSE);
				
			status = STATUS_INVALID_DEVICE_REQUEST;
			return status;
		}
		pdx->m_dwVideoBuffer[i] = (DWORD)pdx->m_VideoPhysicalAddress[i].LowPart;
		KdPrint(("Mem allocate::m_dwVideoBuffer[%d] = %x\n", i, pdx->m_dwVideoBuffer[i]));
	}

	//Audio
	pdx->m_pbyAudioBuffer = (BYTE *)(*pdx->m_pAdapterObject->DmaOperations->AllocateCommonBuffer)(pdx->m_pAdapterObject, MAX_AUDIO_CAP_SIZE * PAGESIZE, &pdx->m_AudioPhysicalAddress, FALSE);
	if(!pdx->m_pbyAudioBuffer)
	{
		KdPrint(("m_pbyAudioBuffer mem Allocate Fail \n"));	
		(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_AUDIO_CAP_SIZE * PAGESIZE, pdx->m_AudioPhysicalAddress, pdx->m_pbyAudioBuffer, FALSE);
		
		for(j=0; j<MAX_DECODER; j++)
		{
			(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_VIDEO_CAP_SIZE * PAGESIZE, pdx->m_VideoPhysicalAddress[j], pdx->m_pbyVideoBuffer[j], FALSE);
			(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_ENCODER_SIZE * PAGESIZE, pdx->m_EncoderPhysicalAddress[j], pdx->m_pbyEncoderBuffer[j], FALSE);	
		}
			
		status = STATUS_INVALID_DEVICE_REQUEST;
		return status;
	}
	pdx->m_dwAudioBuffer = (DWORD)pdx->m_AudioPhysicalAddress.LowPart;
	pdx->m_bBufferAllocate = TRUE;
	return status;
}
//=========================================================================================================================//
void DmaMemFreePool(PDEVICE_OBJECT pDeviceObject)
{
	PDEVICE_EXTENSION	pdx	= (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	int					i	= 0;
	
	if(pdx->m_bBufferAllocate == TRUE)
	{
		for(i=0; i<MAX_DECODER; i++)
		{
			(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_ENCODER_SIZE * PAGESIZE, pdx->m_EncoderPhysicalAddress[i], pdx->m_pbyEncoderBuffer[i], FALSE);
			(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_VIDEO_CAP_SIZE * PAGESIZE, pdx->m_VideoPhysicalAddress[i], pdx->m_pbyVideoBuffer[i], FALSE);
		}
		(*pdx->m_pAdapterObject->DmaOperations->FreeCommonBuffer)(pdx->m_pAdapterObject, MAX_AUDIO_CAP_SIZE * PAGESIZE, pdx->m_AudioPhysicalAddress, pdx->m_pbyAudioBuffer, FALSE);
		pdx->m_bBufferAllocate = FALSE;
	}
}
//=========================================================================================================================//
NTSTATUS MDLAllocateCapBuf(PDEVICE_OBJECT pDeviceObject)
{
	NTSTATUS			status	= STATUS_SUCCESS;
	PDEVICE_EXTENSION	pdx		= (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	BYTE				i, j;
	
	if(pdx->m_bMDLAllocate == TRUE)
		MDLFreeCapBuf(pDeviceObject);
	
	pdx->m_pMDLAudioBuffer = IoAllocateMdl(pdx->m_pbyAudioBuffer, MAX_AUDIO_CAP_SIZE*PAGESIZE, FALSE, FALSE, NULL);
	if(!pdx->m_pMDLAudioBuffer)
	{
		KdPrint(("IoAllocateMdl fail1\n"));

		MmUnmapLockedPages(pdx->m_pbyUserAudioBuffer, pdx->m_pMDLAudioBuffer);
		IoFreeMdl(pdx->m_pMDLAudioBuffer);

		status = STATUS_INVALID_DEVICE_REQUEST;
		return status;
	}
	MmBuildMdlForNonPagedPool(pdx->m_pMDLAudioBuffer);
	pdx->m_pbyUserAudioBuffer = (BYTE *)MmMapLockedPages(pdx->m_pMDLAudioBuffer, UserMode);

	for(i=0; i<MAX_DECODER; i++)
	{
		pdx->m_pMDLVideoBuffer[i] = IoAllocateMdl(pdx->m_pbyVideoBuffer[i], MAX_VIDEO_CAP_SIZE*PAGESIZE, FALSE, FALSE, NULL);
		if(!pdx->m_pMDLVideoBuffer[i])
		{
			KdPrint(("IoAllocateMdl fail1\n"));

			for(j=0; j<i; j++)
			{
				if(pdx->m_pbyUserVideoBuffer[j])
				{
					MmUnmapLockedPages(pdx->m_pbyUserVideoBuffer[j], pdx->m_pMDLVideoBuffer[j]);
					IoFreeMdl(pdx->m_pMDLVideoBuffer[j]);
				}
			}
			MmUnmapLockedPages(pdx->m_pbyUserAudioBuffer, pdx->m_pMDLAudioBuffer);
			IoFreeMdl(pdx->m_pMDLAudioBuffer);
			
			status = STATUS_INVALID_DEVICE_REQUEST;
			return status;
		}
		MmBuildMdlForNonPagedPool(pdx->m_pMDLVideoBuffer[i]);
		pdx->m_pbyUserVideoBuffer[i] = (BYTE *)MmMapLockedPages(pdx->m_pMDLVideoBuffer[i], UserMode);
	}
	for(i=0; i<MAX_DECODER; i++)
	{
		pdx->m_pMDLEncoderBuffer[i] = IoAllocateMdl(pdx->m_pbyEncoderBuffer[i], MAX_ENCODER_SIZE*PAGESIZE, FALSE, FALSE, NULL);
		if(!pdx->m_pMDLEncoderBuffer[i])
		{
			KdPrint(("IoAllocateMdl fail2\n"));
			
			for(j=0; j<i; j++)
			{	
				MmUnmapLockedPages(pdx->m_pbyUserEncoderBuffer[j], pdx->m_pMDLEncoderBuffer[j]);
				IoFreeMdl(pdx->m_pMDLEncoderBuffer[j]);
			}
			for(j=0; j<MAX_DECODER; j++)
			{
				if(pdx->m_pbyUserVideoBuffer[j])
				{
					MmUnmapLockedPages(pdx->m_pbyUserVideoBuffer[j], pdx->m_pMDLVideoBuffer[j]);
					IoFreeMdl(pdx->m_pMDLVideoBuffer[j]);
				}
			}
			MmUnmapLockedPages(pdx->m_pbyUserAudioBuffer, pdx->m_pMDLAudioBuffer);
			IoFreeMdl(pdx->m_pMDLAudioBuffer);
			status = STATUS_INVALID_DEVICE_REQUEST;
			return status;
		}
		MmBuildMdlForNonPagedPool(pdx->m_pMDLEncoderBuffer[i]);
		pdx->m_pbyUserEncoderBuffer[i] = (BYTE *)MmMapLockedPages(pdx->m_pMDLEncoderBuffer[i], UserMode);
	}

	pdx->m_bMDLAllocate = TRUE;
	return status;
}
//=========================================================================================================================//
void MDLFreeCapBuf(PDEVICE_OBJECT pDeviceObject)
{
	PDEVICE_EXTENSION	pdx = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	BYTE				j;

	if(pdx->m_bMDLAllocate == TRUE)
	{
		MmUnmapLockedPages(pdx->m_pbyUserAudioBuffer, pdx->m_pMDLAudioBuffer);
		IoFreeMdl(pdx->m_pMDLAudioBuffer);

		for(j=0; j<MAX_DECODER; j++)
		{
			if(pdx->m_pbyUserVideoBuffer[j])
			{
				MmUnmapLockedPages(pdx->m_pbyUserVideoBuffer[j], pdx->m_pMDLVideoBuffer[j]);
				IoFreeMdl(pdx->m_pMDLVideoBuffer[j]);
			}
			if(pdx->m_pbyUserEncoderBuffer[j])
			{
				MmUnmapLockedPages(pdx->m_pbyUserEncoderBuffer[j], pdx->m_pMDLEncoderBuffer[j]);
				IoFreeMdl(pdx->m_pMDLEncoderBuffer[j]);
			}
		}
		pdx->m_bMDLAllocate = FALSE;
	}
}
//=========================================================================================================================//
NTSTATUS KKGetPhysicalAddress(IN unsigned char *pR0Address, OUT PHYSICAL_ADDRESS *PhysAddress)
{
	NTSTATUS	status = STATUS_SUCCESS;

	KdPrint(("KKGetPhysicalAddress \n"));
	*PhysAddress = MmGetPhysicalAddress(pR0Address);
	return status;
}


