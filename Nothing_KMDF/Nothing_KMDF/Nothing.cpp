//
// Copyright 2007-2017 OSR Open Systems Resources, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE
// 

#include "nothing.h"

///////////////////////////////////////////////////////////////////////////////
//
//  DriverEntry
//
//    This routine is called by Windows when the driver is first loaded.  It
//    is the responsibility of this routine to create the WDFDRIVER
//
//  INPUTS:
//
//      DriverObject - Address of the DRIVER_OBJECT created by Windows for this
//                     driver.
//
//      RegistryPath - UNICODE_STRING which represents this driver's key in the
//                     Registry.  
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      STATUS_SUCCESS, otherwise an error indicating why the driver could not
//                      load.
//
//  IRQL:
//
//      This routine is called at IRQL == PASSIVE_LEVEL.
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

#if DBG
    DbgPrint("\nOSR Nothing Driver -- Compiled %s %s\n",__DATE__, __TIME__);
#endif

    //
    // Provide pointer to our EvtDeviceAdd event processing callback
    // function
    //
    WDF_DRIVER_CONFIG_INIT(&config, NothingEvtDeviceAdd);

    //
    // Create our WDFDriver instance
    //
    status = WdfDriverCreate(DriverObject,
                        RegistryPath,
                        WDF_NO_OBJECT_ATTRIBUTES, 
                        &config,     
                        WDF_NO_HANDLE 
                        );

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDriverCreate failed 0x%0x\n", status);
#endif
    }

    return(status);
}

///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtDeviceAdd
//
//    This routine is called by the framework when a device of
//    the type we support is found in the system.
//
//  INPUTS:
//
//      DriverObject - Our WDFDRIVER object
//
//      DeviceInit   - The device iniitalization structure we'll
//                     be using to create our WDFDEVICE
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      STATUS_SUCCESS, otherwise an error indicating why the driver could not
//                      load.
//
//  IRQL:
//
//      This routine is called at IRQL == PASSIVE_LEVEL.
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
NTSTATUS
NothingEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES objAttributes;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    
    //
    // Our "internal" (native) and user-accessible device names
    //
    DECLARE_CONST_UNICODE_STRING(nativeDeviceName, L"\\Device\\Nothing");
    DECLARE_CONST_UNICODE_STRING(userDeviceName, L"\\Global??\\Nothing");
    
    UNREFERENCED_PARAMETER(Driver);

    //
    // Life is nice and simple in this driver... 
    //
    // We don't need ANY PnP/Power Event Processing callbacks. There's no
    // hardware, thus we don't need EvtPrepareHardware or EvtReleaseHardware 
    // There's no power state to handle so we don't need EvtD0Entry or EvtD0Exit.
    //
    
    //
    // Prepare for WDFDEVICE creation
    //
    // Initialize standard WDF Object Attributes structure
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objAttributes);

    //
    // Specify our device context
    //
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objAttributes,
                                        NOTHING_DEVICE_CONTEXT);

    //
    // We want our device object NAMED, thank you very much
    //
    status = WdfDeviceInitAssignName(DeviceInit, &nativeDeviceName);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceInitAssignName failed 0x%0x\n", status);
#endif
        return(status);
    }

    //
    // Let's just create our device object
    //
    status = WdfDeviceCreate(&DeviceInit,
                            &objAttributes, 
                            &device);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreate failed 0x%0x\n", status);
#endif
        return status;
    }

    //
    // Create a symbolic link for the control object so that usermode can open
    // the device by name.
    //
    status = WdfDeviceCreateSymbolicLink(device, &userDeviceName);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateSymbolicLink failed 0x%0x\n", status);
#endif
        return(status);
    }


    //
    // Configure our queue of incoming requests
    //
    // We only use the default queue, and we set it for sequential processing.
    // This means that the driver will only receive one request at a time
    // from the queue, and will not get another request until it completes
    // the previous one.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                             WdfIoQueueDispatchSequential);

    //
    // Declare our I/O Event Processing callbacks
    //
    // We handle, read, write, and device control requests.
    //
    // WDF will automagically handle Create and Close requests for us and will
    // will complete any OTHER request types with STATUS_INVALID_DEVICE_REQUEST.    
    //
    queueConfig.EvtIoRead = NothingEvtRead;
    queueConfig.EvtIoWrite = NothingEvtWrite;
    queueConfig.EvtIoDeviceControl = NothingEvtDeviceControl;

    //
    // Because this is a queue for a software-only device, indicate
    // that the queue doesn't need to be power managed
    //
    queueConfig.PowerManaged = WdfFalse;

    status = WdfIoQueueCreate(device,
                            &queueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            NULL); // optional pointer to receive queue handle

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfIoQueueCreate for default queue failed 0x%0x\n", status);
#endif
        return(status);
    }

    return(status);
}

///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtRead
//
//    This routine is called by the framework when there is a
//    read request for us to process
//
//  INPUTS:
//
//      Queue    - Our default queue
//
//      Request  - A read request
//
//      Length   - The length of the read operation
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None.
//
//  IRQL:
//
//      This routine is called at IRQL <= DISPATCH_LEVEL
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
VOID
NothingEvtRead(WDFQUEUE  Queue, WDFREQUEST  Request, size_t  Length)
{
    PNOTHING_DEVICE_CONTEXT devContext;

    UNREFERENCED_PARAMETER(Length);

#if DBG
    DbgPrint("NothingEvtRead\n");
#endif

    //
    // Get a pointer to our device extension, just to show how it's done.
    // (get the WDFDEVICE from the WDFQUEUE, and the extension from the device)
    //
    devContext = NothingGetContextFromDevice(
                                WdfIoQueueGetDevice(Queue) );

    //
    // Nothing to do...
    // We're returning zero bytes, so fill that into the information field
    //
    WdfRequestCompleteWithInformation(Request,
                                    STATUS_SUCCESS,
                                    0);    
}

///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtWrite
//
//    This routine is called by the framework when there is a
//    read request for us to process
//
//  INPUTS:
//
//      Queue    - Our default queue
//
//      Request  - A read request
//
//      Length   - The length of the read operation
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None.
//
//  IRQL:
//
//      This routine is called at IRQL <= DISPATCH_LEVEL
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
VOID
NothingEvtWrite(WDFQUEUE  Queue, WDFREQUEST  Request, size_t  Length)
{

    UNREFERENCED_PARAMETER(Queue);

#if DBG
    DbgPrint("NothingEvtWrite\n");
#endif

    //
    // Nothing to do...
    //
    WdfRequestCompleteWithInformation(Request,
                                    STATUS_SUCCESS,
                                    Length);    
    
}

///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtDeviceControl
//
//    This routine is called by the framework when there is a
//    device control request for us to process
//
//  INPUTS:
//
//      Queue    - Our default queue
//
//      Request  - A device control request
//
//      OutputBufferLength - The length of the output buffer
//
//      InputBufferLength  - The length of the input buffer
//
//      IoControlCode      - The operation being performed
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None.
//
//  IRQL:
//
//      This routine is called at IRQL == PASSIVE_LEVEL, due to
//      our PASSIVE_LEVEL execution level contraint
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
VOID
NothingEvtDeviceControl(WDFQUEUE Queue,
            WDFREQUEST Request,
            size_t OutputBufferLength,
            size_t InputBufferLength,
            ULONG IoControlCode)
{

    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(Queue);

#if DBG
    DbgPrint("NothingEvtDeviceControl\n");
#endif


    //
    // Nothing to do...
    // In this case, we return an info field of zero
    //
    WdfRequestCompleteWithInformation(Request,
                                    STATUS_SUCCESS,
                                    0);    
}
