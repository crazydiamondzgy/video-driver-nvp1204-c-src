
#include "NVP1204.h"
#include "initguid.h"

// DEFINE_GUID(GUID_BUS_INTERFACE_STANDARD,     0x496B8280L, 0x6F25, 0x11D0, 0xBE, 0xAF, 0x08, 0x00, 0x2B, 0xE2, 0x09, 0x2F );
// DEFINE_GUID(GUID_PCI_BUS_INTERFACE_STANDARD, 0x496B8281L, 0x6F25, 0x11D0, 0xBE, 0xAF, 0x08, 0x00, 0x2B, 0xE2, 0x09, 0x2F );

extern LONG g_dwDeviceCount;
//===================================================================================//
NTSTATUS AL_DispatchPnp(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PIO_STACK_LOCATION      irpStack;
	NTSTATUS                status = STATUS_SUCCESS;
	KEVENT                  event;        
	UNICODE_STRING          win32DeviceName;
	PDEVICE_EXTENSION       pdx;
	WCHAR					win32NameBuf[128];

	pdx = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	
	status = IoAcquireRemoveLock(&pdx->m_RemoveLock, Irp);
	if(!NT_SUCCESS (status))
	{
		Irp->IoStatus.Status = status;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		return status;
	}
	
	KdPrint(("%s\n",PnPMinorFunctionString(irpStack->MinorFunction)));	
	switch (irpStack->MinorFunction) 
	{
	case IRP_MN_START_DEVICE:
		IoCopyCurrentIrpStackLocationToNext(Irp);
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE) AL_CompletionRoutine, &event, TRUE, TRUE, TRUE);
		status = IoCallDriver(pdx->m_pLowerDeviceObject, Irp);
		if (STATUS_PENDING == status) 
			KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		
		if (NT_SUCCESS(status) && NT_SUCCESS(Irp->IoStatus.Status)) 
		{
			status = AL_StartDevice(DeviceObject, Irp);
			if(NT_SUCCESS(status))
			{
				// As we are successfully now back from our start device
				// we can do work.
				pdx->m_bStarted = TRUE;
				pdx->m_bRemoved = FALSE;
			}
		}
		
		// We must now complete the IRP, since we stopped it in the
		// completion routine with STATUS_MORE_PROCESSING_REQUIRED.
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		break;
		
	case IRP_MN_QUERY_INTERFACE:
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(pdx->m_pLowerDeviceObject, Irp);
		break;

	case IRP_MN_QUERY_STOP_DEVICE:
		// Fail the query stop to prevent the system from taking away hardware 
		// resources. If you do support this you must have a queue to hold
		// incoming requests between stop and subsequent start with new set of
		// resources.
		Irp->IoStatus.Status = status = STATUS_UNSUCCESSFUL;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		break;
		
	case IRP_MN_QUERY_REMOVE_DEVICE:
		// The device can be removed without disrupting the machine. 
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(pdx->m_pLowerDeviceObject, Irp);
		break;
		
	case IRP_MN_SURPRISE_REMOVAL:		
		// The device has been unexpectedly removed from the machine 
		// and is no longer available for I/O. Stop all access to the device.
		// Release any resources associated with the device, but leave the 
		// device object attached to the device stack until the PnP Manager 
		// sends a subsequent IRP_MN_REMOVE_DEVICE request. 
		// You should fail any outstanding I/O to the device. You will
		// not get a remove until all the handles open to the device have been closed.
		pdx->m_bRemoved = TRUE;
		pdx->m_bStarted = FALSE;
		AL_StopDevice(DeviceObject, Irp);
		
		if(pdx->m_bPortWasMapped)
			pdx->m_bPortWasMapped = FALSE;
		
		memset(win32NameBuf, 0, 128*2);
		_snwprintf(win32NameBuf, 128, DOS_DEVICE_NAME, pdx->m_byDeviceID);
		RtlInitUnicodeString(&pdx->m_win32DeviceName, win32NameBuf);
		KdPrint(("IRP_MN_SURPRISE_REMOVAL::win32NameBuf  = %s\n",win32NameBuf));

		IoDeleteSymbolicLink(&pdx->m_win32DeviceName);
		IoSkipCurrentIrpStackLocation(Irp);
		Irp->IoStatus.Status = STATUS_SUCCESS;
		status = IoCallDriver(pdx->m_pLowerDeviceObject, Irp);
		break;       
		
	case IRP_MN_REMOVE_DEVICE:
		// Relinquish all resources here.
		// Detach and delete the device object so that
		// your driver can be unloaded. You get remove
		// either after query_remove or surprise_remove.
		if(!pdx->m_bRemoved)
		{
			pdx->m_bRemoved = TRUE;
			pdx->m_bStarted = FALSE;
			AL_StopDevice(DeviceObject, Irp);
			if(pdx->m_bPortWasMapped)
				pdx->m_bPortWasMapped = FALSE;
						
			memset(win32NameBuf, 0, 128*2);
			_snwprintf(win32NameBuf, 128, DOS_DEVICE_NAME, pdx->m_byDeviceID);		
			RtlInitUnicodeString(&pdx->m_win32DeviceName, win32NameBuf);
			KdPrint(("IRP_MN_REMOVE_DEVICE::win32NameBuf  = %s\n", win32NameBuf));
			IoDeleteSymbolicLink(&pdx->m_win32DeviceName);
		}        
		// Wait for all outstanding requests to complete
		KdPrint(("IRP_MN_REMOVE_DEVICE::Waiting for outstanding requests\n"));
		IoReleaseRemoveLockAndWait(&pdx->m_RemoveLock, Irp);
		
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(pdx->m_pLowerDeviceObject, Irp);
		
		IoDetachDevice(pdx->m_pLowerDeviceObject); 
		IoDeleteDevice(DeviceObject);	
		g_dwDeviceCount = -1;
		return status;
		
	case IRP_MN_STOP_DEVICE:
		// Since you failed query stop, you will not get this request.
	case IRP_MN_CANCEL_REMOVE_DEVICE: 
		// No action required in this case. Just pass it down.
	case IRP_MN_CANCEL_STOP_DEVICE: 
		//No action required in this case.
		Irp->IoStatus.Status = STATUS_SUCCESS;
	default:
		// Please see PnP documentation for use of these IRPs.
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(pdx->m_pLowerDeviceObject, Irp);
		break;
    }
    IoReleaseRemoveLock(&pdx->m_RemoveLock, Irp);       
    return status;
}
//===================================================================================//
NTSTATUS AL_StartDevice(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{	
	PCM_PARTIAL_RESOURCE_DESCRIPTOR resource, resourceTrans;
	PCM_PARTIAL_RESOURCE_LIST		partialResourceList, partialResourceListTranslated;
	
	PIO_STACK_LOCATION			stack;
	NTSTATUS					status = STATUS_SUCCESS;
	PHYSICAL_ADDRESS			Membase;
	KIRQL						irql, ulirql, InterruptRequestLevel;
	KINTERRUPT_MODE				mode, ulmode;
	KAFFINITY					affinity, ulaffinity;
	DEVICE_DESCRIPTION			dd;
	PDEVICE_EXTENSION			pdx;

	DWORD						dwMapRegisters, dwData1, dwData2, vector, ulvector;			// maximum # mapping registers
	BYTE						byRet, i;
	BOOL						gotinterrupt = FALSE;	
	
	pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;	
	stack = IoGetCurrentIrpStackLocation (Irp);

//	KdPrint(("Start Device\n"));
	if(pdx->m_bRemoved) 
	{
		// Some kind of surprise removal arrived. We will fail the IRP
		// The dispatch routine that called us will take care of 
		// completing the IRP.
		return STATUS_DELETE_PENDING;
	}
	// Do whatever initialization needed when starting the device: 
	// gather information about it,  update the registry, etc.
	if((stack->Parameters.StartDevice.AllocatedResources == NULL) && (stack->Parameters.StartDevice.AllocatedResourcesTranslated == NULL)) 
	{		
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	// Parameters.StartDevice.AllocatedResources points to a 
	// CM_RESOURCE_LIST describing the hardware resources that 
	// the PnP Manager assigned to the device. This list contains 
	// the resources in raw form. Use the raw resources to program 
	// the device.
	partialResourceList = &stack->Parameters.StartDevice.AllocatedResources->List[0].PartialResourceList;
	resource = &partialResourceList->PartialDescriptors[0];

	// Parameters.StartDevice.AllocatedResourcesTranslated points 
	// to a CM_RESOURCE_LIST describing the hardware resources that 
	// the PnP Manager assigned to the device. This list contains 
	// the resources in translated form. Use the translated resources 
	// to connect the interrupt vector, map I/O space, and map memory.
	partialResourceListTranslated = &stack->Parameters.StartDevice.AllocatedResourcesTranslated->List[0].PartialResourceList;
	resourceTrans = &partialResourceListTranslated->PartialDescriptors[0];
	
//	KdPrint(("Start Device :: resource complete\n"));
	for(i=0; i<partialResourceList->Count; i++, resource++, resourceTrans++) 
	{		
		switch (resourceTrans->Type) 
		{
		case CmResourceTypePort:
			//KdPrint(("resourceTrans:: Resource Translated Port: (%x) Length: (%d)\n", resourceTrans->u.Port.Start.LowPart,resourceTrans->u.Port.Length));			
			break;

		case CmResourceTypeMemory:
			Membase = resourceTrans->u.Memory.Start;
			pdx->m_pbyMembase = (PVOID)MmMapIoSpace(resourceTrans->u.Memory.Start, resourceTrans->u.Memory.Length, MmNonCached);
			pdx->m_dwMemsize = resourceTrans->u.Memory.Length;
			pdx->m_bPortWasMapped = TRUE;
			//KdPrint(("Resource Translated Memory: (%x) Length: (%d)\n", resourceTrans->u.Memory.Start.LowPart, resourceTrans->u.Memory.Length));			
			break;
			
		case CmResourceTypeInterrupt:
			//KdPrint(("Resource Interrupt Vector: (%x) Level: (%d)\n", resource->u.Interrupt.Vector, resource->u.Interrupt.Level));			
			ulirql			= (KIRQL) resourceTrans->u.Interrupt.Level;
			ulvector		= resourceTrans->u.Interrupt.Vector;
			ulaffinity		= resourceTrans->u.Interrupt.Affinity;
			ulmode			= (resourceTrans->Flags == CM_RESOURCE_INTERRUPT_LATCHED) ? Latched : LevelSensitive;
			pdx->m_bIrqshare = (resourceTrans->ShareDisposition == CmResourceShareShared) ? 1 : 0;
			gotinterrupt	= TRUE;
			mode			= ulmode;
			irql			= ulirql;
			vector			= ulvector;
			affinity		= ulaffinity;
			break;

		default:
			//KdPrint(("Unhandled resource type (0x%x)\n", resource->Type));
			break;			
		}
	}

	if(!(TRUE && gotinterrupt && pdx->m_bPortWasMapped))
	{
		KdPrint(("Didn't get expected I/O resources\n"));
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}
	
	if(!pdx->m_pbyMembase)
	{
		KdPrint(("Unable to map memory block %I64X, length %X\n", pdx->m_pbyMembase, pdx->m_dwMemsize));
		return STATUS_INSUFFICIENT_RESOURCES;
	}			
	KdPrint(("map memory block %I64X, length %X\n", pdx->m_pbyMembase, pdx->m_dwMemsize));
//	KdPrint(("Start Device :: m_pbyMembase complete\n"));

	RtlZeroMemory(&dd, sizeof(dd));
	dd.Version				= DEVICE_DESCRIPTION_VERSION;
	dd.InterfaceType		= PCIBus;
	dd.MaximumLength		= 0x1000;
	dd.Dma32BitAddresses	= TRUE;
	dd.Master				= TRUE;
	
	MemberVariableInit(pdx);
//	KdPrint(("Start Device :: MemberVariableInit complete\n"));

	pdx->m_pAdapterObject = IoGetDmaAdapter(pdx->m_pPdo, &dd, &dwMapRegisters);
	status = IoConnectInterrupt(&pdx->m_pInterruptObject, (PKSERVICE_ROUTINE)OnInterrupt, 
								(PVOID)pdx, NULL, vector, irql, irql, mode, pdx->m_bIrqshare, affinity, FALSE);
	
	if(!NT_SUCCESS(status))
	{
		KdPrint(("IoConnectInterrupt failed - %X\n", status));
		if(pdx->m_pbyMembase)
		{
			MmUnmapIoSpace(pdx->m_pbyMembase, pdx->m_dwMemsize);
			pdx->m_pbyMembase = NULL;
		}
		if(pdx->m_pAdapterObject)
		{
			(*pdx->m_pAdapterObject->DmaOperations->PutDmaAdapter)(pdx->m_pAdapterObject);
			pdx->m_pAdapterObject = NULL;
		}
		return status;
	}
//	KdPrint(("Start Device :: IoConnectInterrupt complete\n"));

	status = DmaMemAllocPool(DeviceObject);
//	KdPrint(("Start Device :: DmaMemAllocPool complete\n"));
	
	if(!NT_SUCCESS(status))
	{
		KdPrint(("StartDevice::DmaMemAllocPool m_byDeviceID[%d] Fail !!!!\n", pdx->m_byDeviceID));
		if(pdx->m_pbyMembase)
		{
			MmUnmapIoSpace(pdx->m_pbyMembase, pdx->m_dwMemsize);
			pdx->m_pbyMembase = NULL;
		}
		if(pdx->m_pAdapterObject)
		{
			(*pdx->m_pAdapterObject->DmaOperations->PutDmaAdapter)(pdx->m_pAdapterObject);
			pdx->m_pAdapterObject = NULL;
		}
		return status;
	}
	
	pdx->m_byNVP1114_ID[0] = I2C_NVP1114_DEVICE_1;
	pdx->m_byNVP1114_ID[1] = I2C_NVP1114_DEVICE_2;
	
	AllDMADisable(pdx);
	SetLocalRegister(pdx);

	I2C_Init(pdx);
//	Decoder_Init(pdx);
	
	return status;	
}
//===================================================================================//
NTSTATUS AL_StopDevice(IN PDEVICE_OBJECT pDeviceObject,IN PIRP Irp)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PDEVICE_EXTENSION	pdx = (PDEVICE_EXTENSION) pDeviceObject->DeviceExtension;
	
	KdPrint(("StopDevice\n"));	
	if(pdx->m_pInterruptObject)
	{
		IoDisconnectInterrupt(pdx->m_pInterruptObject);
		pdx->m_pInterruptObject = NULL;
	}

	DmaMemFreePool(pDeviceObject);
	if(pdx->m_pbyMembase)
	{
		MmUnmapIoSpace(pdx->m_pbyMembase, pdx->m_dwMemsize);
		pdx->m_pbyMembase = NULL;
	}
	
	if(pdx->m_pAdapterObject)
	{
		(*pdx->m_pAdapterObject->DmaOperations->PutDmaAdapter)(pdx->m_pAdapterObject);
		pdx->m_pAdapterObject = NULL;
	}	
	return status;
}
//===================================================================================//
void MemberVariableInit(PDEVICE_EXTENSION pdx)
{	
	BYTE	i;

	pdx->m_byVideoFormat = (BYTE)VIDEO_FORMAT_NTSC;
	pdx->m_byDataFormat = (BYTE)DEFAULT_DATA_FORMAT;
	
	memset(pdx->m_wImageSize, IMAGEWIDTH_R704, sizeof(WORD)*MAX_VIDEO_CHANNEL);
	pdx->m_wImageHeight = IMAGEHEIGHT_R480;
	
	memset(pdx->m_byCaptureSize, (BYTE)IMAGE_360X240, sizeof(BYTE)*MAX_VIDEO_CHANNEL);

	pdx->m_pInterruptObject = NULL;
	pdx->m_bBufferAllocate	= FALSE;	
	
	memset(pdx->m_pVideoEvent, 0x00, sizeof(PKEVENT)*MAX_DECODER);
	for(i=0; i<MAX_DECODER; i++)
		memset(pdx->m_pbyChannelTable[i], 0xFF, sizeof(BYTE)*MAX_CHANNEL_CHANGEABLE);

	memset(pdx->m_byOldVideoQCount, 0x00, sizeof(BYTE)*MAX_DECODER);
	memset(pdx->m_byCurVideoQCount, 0x00, sizeof(BYTE)*MAX_DECODER);
}
//===================================================================================//
// 0 for read 1 for write
// NTSTATUS ReadWriteConfigSpace(IN PDEVICE_OBJECT DeviceObject, IN ULONG ReadOrWrite, IN PVOID Buffer, IN ULONG Offset, IN ULONG Length)
// {
// 	IO_STATUS_BLOCK		ioStatusBlock;
// 	PIO_STACK_LOCATION	irpStack;
// 	PDEVICE_OBJECT		targetObject;
// 	KEVENT				event;
// 	NTSTATUS			status;
// 	PIRP				irp;
// 	
// 	PAGED_CODE();
// 	KeInitializeEvent(&event, NotificationEvent, FALSE);
// 	targetObject = IoGetAttachedDeviceReference(DeviceObject);
// 	irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, targetObject, NULL, 0, NULL, &event, &ioStatusBlock);
// 	if(irp == NULL) 
// 	{
// 		status = STATUS_INSUFFICIENT_RESOURCES;
// 		goto End;
// 	}
// 	
// 	irpStack = IoGetNextIrpStackLocation(irp);
// 	if(ReadOrWrite == 0) {
// 		irpStack->MinorFunction = IRP_MN_READ_CONFIG;
// 	}else {
// 		irpStack->MinorFunction = IRP_MN_WRITE_CONFIG;
// 	}
// 
// 	irpStack->Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
// 	irpStack->Parameters.ReadWriteConfig.Buffer = Buffer;
// 	irpStack->Parameters.ReadWriteConfig.Offset = Offset;
// 	irpStack->Parameters.ReadWriteConfig.Length = Length;
// 	
// 	// Initialize the status to error in case the bus driver does not set it correctly.
// 	irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
// 	status = IoCallDriver(targetObject, irp);
// 	
// 	if(status == STATUS_PENDING) 
// 	{
// 		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
// 		status = ioStatusBlock.Status;
// 	}
// 	
// End:
// 	// Done with reference
// 	ObDereferenceObject(targetObject);
// 	return status;
// }

//=========================================================================================================================//
// NTSTATUS GetPCIBusInterfaceStandard(IN PDEVICE_OBJECT DeviceObject, OUT PBUS_INTERFACE_STANDARD	BusInterfaceStandard)
// {
// 	IO_STATUS_BLOCK		ioStatusBlock;
// 	PIO_STACK_LOCATION	irpStack;
// 	PDEVICE_OBJECT		targetObject;
// 	
// 	KEVENT				event;
// 	NTSTATUS			status;
// 	PIRP				irp;
// 		
// 	KdPrint(("GetPciBusInterfaceStandard entered.\n"));	
// 	KeInitializeEvent(&event, NotificationEvent, FALSE);	
// 	targetObject = IoGetAttachedDeviceReference(DeviceObject);
// 	irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, targetObject, NULL, 0, NULL, &event, &ioStatusBlock);
// 	if(irp == NULL) 
// 	{
// 		status = STATUS_INSUFFICIENT_RESOURCES;
// 		goto End;
// 	}
// 	
// 	irpStack = IoGetNextIrpStackLocation(irp);
// 	irpStack->MinorFunction = IRP_MN_QUERY_INTERFACE;
// 	irpStack->Parameters.QueryInterface.InterfaceType =  (LPGUID)&GUID_BUS_INTERFACE_STANDARD;
// 	irpStack->Parameters.QueryInterface.Size = sizeof(BUS_INTERFACE_STANDARD);
// 	irpStack->Parameters.QueryInterface.Version = 1;
// 	irpStack->Parameters.QueryInterface.Interface = (PINTERFACE)BusInterfaceStandard;
// 	irpStack->Parameters.QueryInterface.InterfaceSpecificData = NULL;
// 	
// 	// Initialize the status to error in case the bus driver does not  set it correctly.
// 	irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
// 	status = IoCallDriver(targetObject, irp);
// 	if (status == STATUS_PENDING) 
// 	{		
// 		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
// 		status = ioStatusBlock.Status;
// 	}
// 	
// End:
// 	// Done with reference
// 	ObDereferenceObject(targetObject);
// 	return status;	
// }