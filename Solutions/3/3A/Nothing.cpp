//
// Copyright 2007-2022 OSR Open Systems Resources, Inc.
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
//    This software is supplied for instructional purposes only.  It is not
//    complete, and it is not suitable for use in any production environment.
//
//    OSR Open Systems Resources, Inc. (OSR) expressly disclaims any warranty
//    for this software.  THIS SOFTWARE IS PROVIDED  "AS IS" WITHOUT WARRANTY
//    OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION,
//    THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
//    PURPOSE.  THE ENTIRE RISK ARISING FROM THE USE OF THIS SOFTWARE REMAINS
//    WITH YOU.  OSR's entire liability and your exclusive remedy shall not
//    exceed the price paid for this material.  In no event shall OSR or its
//    suppliers be liable for any damages whatsoever (including, without
//    limitation, damages for loss of business profit, business interruption,
//    loss of business information, or any other pecuniary loss) arising out
//    of the use or inability to use this software, even if OSR has been
//    advised of the possibility of such damages.  Because some states/
//    jurisdictions do not allow the exclusion or limitation of liability for
//    consequential or incidental damages, the above limitation may not apply
//    to you.
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
NTSTATUS
DriverEntry(PDRIVER_OBJECT  DriverObject,
            PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG driverConfig;
    NTSTATUS          status;

#if DBG
    DbgPrint("\nOSR Nothing Driver -- Compiled %s %s\n",
             __DATE__,
             __TIME__);
#endif

    //
    // Provide pointer to our EvtDeviceAdd event processing callback
    // function
    //
    WDF_DRIVER_CONFIG_INIT(&driverConfig,
                           NothingEvtDeviceAdd);

    //
    // Create our WDFDriver instance
    //
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &driverConfig,
                             WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDriverCreate failed 0x%0x\n",
                 status);
#endif
    }

    return (status);
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
//      DeviceInit   - The device initialization structure we'll
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
NothingEvtDeviceAdd(WDFDRIVER       Driver,
                    PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES objAttributes;
    WDFDEVICE             device;
    WDF_IO_QUEUE_CONFIG   queueConfig;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    DECLARE_CONST_UNICODE_STRING(userDeviceName,
                                 L"\\DosDevices\\Nothing");

    UNREFERENCED_PARAMETER(Driver);

    //
    // Life is nice and simple in this driver... 
    //
    // We don't need ANY PnP/Power Event Processing callbacks. There's no
    // hardware, thus we don't need EvtPrepareHardware or EvtReleaseHardware 
    // There's no power state to handle so we don't need EvtD0Entry or EvtD0Exit.
    //
    // However, for this exercise we'll be adding D0Entry and D0Exit callbacks
    // for fun.
    //


    //
    // Prepare for WDFDEVICE creation
    //

    WDF_OBJECT_ATTRIBUTES_INIT(&objAttributes);

    //
    // Specify our device context
    //
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objAttributes,
                                           NOTHING_DEVICE_CONTEXT);


    //
    // We're adding D0Entry/D0Exit callbacks, so we need to
    // initialize out PnP power event callbacks structure.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    //
    // Fill in the D0Entry callback.
    //
    pnpPowerCallbacks.EvtDeviceD0Entry = NothingEvtDeviceD0Entry;

    //
    // Fill in the D0Exit callback
    //
    pnpPowerCallbacks.EvtDeviceD0Exit = NothingEvtDeviceD0Exit;

    //
    // Update the DeviceInit structure to contain the new callbacks.
    //
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, 
                                           &pnpPowerCallbacks);

    //
    // Create our device object
    //
    status = WdfDeviceCreate(&DeviceInit,
                             &objAttributes,
                             &device);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreate failed 0x%0x\n",
                 status);
#endif
        goto Done;
    }

    //
    // Create a symbolic link to our Device Object.  This allows apps
    // to open our device by name.  Note that we use a constant name,
    // so this driver will support only a single instance of our device.
    //
    status = WdfDeviceCreateSymbolicLink(device,
                                         &userDeviceName);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateSymbolicLink failed 0x%0x\n",
                 status);
#endif
        goto Done;
    }

    //
    // ALSO create a device interface for the device
    // This allows usage of the SetupApiXxxx functions to locate
    // the device
    //
    status = WdfDeviceCreateDeviceInterface(device,
                                    &GUID_DEVINTERFACE_NOTHING,
                                    NULL);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateDeviceInterface failed 0x%0x\n", status);
#endif
        return(status);
    }

    //
    // Configure our Queue of incoming requests
    //
    // We use only the default Queue, and we set it for sequential processing.
    // This means that the driver will only receive one request at a time
    // from the Queue, and will not get another request until it completes
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
    // will complete any other request types with STATUS_INVALID_DEVICE_REQUEST.    
    //
    queueConfig.EvtIoRead          = NothingEvtRead;
    queueConfig.EvtIoWrite         = NothingEvtWrite;
    queueConfig.EvtIoDeviceControl = NothingEvtDeviceControl;

    //
    // Because this is a queue for a software-only device, indicate
    // that the queue doesn't need to be power managed.
    //
    queueConfig.PowerManaged = WdfFalse;

    status = WdfIoQueueCreate(device,
                              &queueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfIoQueueCreate for default queue failed 0x%0x\n",
                 status);
#endif
        goto Done;
    }

    status = STATUS_SUCCESS;

Done:

    return (status);
}

///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtDeviceD0Entry
//
//    This routine is called by the framework when a device of
//    the type we support is entering the D0 state
//
//  INPUTS:
//
//      Device       - One of our WDFDEVICE objects
//
//      PreviousState - The D-State we're entering D0 from
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
NothingEvtDeviceD0Entry(WDFDEVICE              Device,
                        WDF_POWER_DEVICE_STATE PreviousState)
{

    UNREFERENCED_PARAMETER(Device);

#if DBG
    DbgPrint("NothingEvtDeviceD0Entry - Entering D0 from state %s (%d)\n",
             NothingPowerDeviceStateToString(PreviousState),
             PreviousState);
#endif

    return STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtDeviceD0Exit
//
//    This routine is called by the framework when a device of
//    the type we support is leaving the D0 state
//
//  INPUTS:
//
//      Device       - One of our WDFDEVICE objects
//
//      TargetState - The D-State we're entering
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
NothingEvtDeviceD0Exit(WDFDEVICE              Device,
                       WDF_POWER_DEVICE_STATE TargetState)
{


    UNREFERENCED_PARAMETER(Device);

#if DBG
    DbgPrint("NothingEvtDeviceD0Exit - Entering state %s (%d)\n",
             NothingPowerDeviceStateToString(TargetState),
             TargetState);
#endif

    return STATUS_SUCCESS;
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
NothingEvtRead(WDFQUEUE   Queue,
               WDFREQUEST Request,
               size_t     Length)
{
    PNOTHING_DEVICE_CONTEXT devContext;
    NTSTATUS                status;
    PVOID                   outBuffer;
    size_t                  outLength;

    UNREFERENCED_PARAMETER(Length);

#if DBG
    DbgPrint("NothingEvtRead\n");
#endif

    //
    // Get a pointer to our device extension.
    // (get the WDFDEVICE from the WDFQUEUE, and the extension from the device)
    //
    devContext = NothingGetContextFromDevice(
                                WdfIoQueueGetDevice(Queue) );

    //
    // Any data waiting?
    //
    if(devContext->BytesStored == 0) {

#if DBG
        DbgPrint("No data waiting. Completing with 0 bytes transferred\n");
#endif
        //
        // Well, THAT was easy...
        //
        status = STATUS_SUCCESS;
        outLength = 0;

        goto done;
    }

    status =  WdfRequestRetrieveOutputBuffer(Request,
                                             1,              // min size
                                             &outBuffer,
                                             &outLength);     
    if(!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("Failed to get output buffer - 0x%x\n", status);
#endif

        //
        // Bad read buffer... I guess.
        // Complete the Read with whatever status we got back from
        // WdfRetrieveOutputBuffer
        //
        outLength = 0;

        goto done;
    }

    //
    // Limit number of bytes returned to the number of bytes in the data storage area
    //
    if(outLength > devContext->BytesStored) {
        outLength = devContext->BytesStored;
    }

    //
    // Copy the data from our storage area to the user's data buffer
    //
    RtlCopyMemory(outBuffer,
                  devContext->Storage,
                  outLength);

    //
    // Given that the data's been retrieved we set the
    // count of bytes stored to zero
    //
    devContext->BytesStored = 0;

    status = STATUS_SUCCESS;

done:

#if DBG
    DbgPrint("Completing with 0x%Ix bytes transferred\n", outLength);
#endif

    //
    // Simple, simple...
    //
    WdfRequestCompleteWithInformation(Request,
                                      status,
                                      outLength);

    return;
}

///////////////////////////////////////////////////////////////////////////////
//
//  NothingEvtWrite
//
//    This routine is called by the framework when there is a
//    write request for us to process
//
//  INPUTS:
//
//      Queue    - Our default queue
//
//      Request  - A write request
//
//      Length   - The length of the write operation
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
NothingEvtWrite(WDFQUEUE   Queue,
                WDFREQUEST Request,
                size_t     Length)
{
    PNOTHING_DEVICE_CONTEXT devContext;
    NTSTATUS                status;
    PVOID                   inBuffer;
    size_t                  inLength;

#if DBG
    DbgPrint("NothingEvtWrite\n");
#endif

    //
    // Get a pointer to our device extension.
    //
    devContext = NothingGetContextFromDevice(
                                WdfIoQueueGetDevice(Queue) );

    if(devContext->BytesStored != 0) {

#if DBG
        DbgPrint("Data already stored. Completing with 0 bytes transferred\n");
#endif

        //
        // There are already data stored in the buffer, complete
        // the write with an error.
        //
        status = STATUS_DEVICE_BUSY;    // Translates to Win32 ERROR_BUSY
        inLength = 0;

        goto done;
    }

    status =  WdfRequestRetrieveInputBuffer(Request,
                                            1,              // minimum acceptable buffer size
                                            &inBuffer,
                                            &inLength);     
    if(!NT_SUCCESS(status)) {

#if DBG
        DbgPrint("Failed to get input buffer - 0x%x\n", status);
#endif

        //
        // Hmmm... we failed to get a pointer to the data the user sent us.
        // Odd.  Well, complete the Request with an error and Information field
        // of zero.
        //
        inLength = 0;

        goto done;
    }

    //
    // Limit number of bytes stored to length of data storage area
    //
    if(inLength > NOTHING_BUFFER_MAX_LENGTH) {
        inLength = NOTHING_BUFFER_MAX_LENGTH;
    }

    //
    // Store the data from the write into our storage area
    //
    RtlCopyMemory(devContext->Storage,
                  inBuffer,
                  inLength);

    //
    // Remember how many bytes of data we have waiting for the next
    // lucky reader
    //
    devContext->BytesStored = inLength;

    status = STATUS_SUCCESS;

done:

#if DBG
    DbgPrint("Completing with 0x%Ix bytes transferred\n", inLength);
#endif

    //
    // And we're done...
    //
    WdfRequestCompleteWithInformation(Request,
                                      status,
                                      inLength);
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
NothingEvtDeviceControl(WDFQUEUE   Queue,
                        WDFREQUEST Request,
                        size_t     OutputBufferLength,
                        size_t     InputBufferLength,
                        ULONG      IoControlCode)
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

CHAR const *
NothingPowerDeviceStateToString(
    WDF_POWER_DEVICE_STATE DeviceState
    ) {

    switch (DeviceState) {
    case WdfPowerDeviceInvalid:
        return "WdfPowerDeviceInvalid";
    case WdfPowerDeviceD0:
        return "WdfPowerDeviceD0";
    case WdfPowerDeviceD1:
        return "WdfPowerDeviceD1";
    case WdfPowerDeviceD2:
        return "WdfPowerDeviceD2";
    case WdfPowerDeviceD3:
        return "WdfPowerDeviceD3";
    case WdfPowerDeviceD3Final:
        return "WdfPowerDeviceD3Final";
    case WdfPowerDevicePrepareForHibernation:
        return "WdfPowerDevicePrepareForHibernation";
    case WdfPowerDeviceMaximum:
        return "WdfPowerDeviceMaximum";
    default:
        break;
    }
    return "Unknown";

}
