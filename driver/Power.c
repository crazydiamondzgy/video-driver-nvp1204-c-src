#include "NVP1204.h"

//=========================================================================================================================//
NTSTATUS AL_DispatchPower(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    PDEVICE_EXTENSION   deviceInfo;
    
    deviceInfo = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    // If the device has been removed, the driver should not pass 
    // the IRP down to the next lower driver.
    if (deviceInfo->m_bRemoved) 
	 {
        PoStartNextPowerIrp(Irp);
        Irp->IoStatus.Status =  STATUS_DELETE_PENDING;
        IoCompleteRequest(Irp, IO_NO_INCREMENT );
        return STATUS_DELETE_PENDING;
    }
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(deviceInfo->m_pLowerDeviceObject, Irp);
}
