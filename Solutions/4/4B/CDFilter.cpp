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

#include "CDFilter.h"

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
    DbgPrint("CDFilter: OSR CDFILTER Filter Driver...Compiled %s %s\n",
             __DATE__,
             __TIME__);
#endif

    //
    // Initialize our driver config structure, specifying our 
    // EvtDeviceAdd Event Processing Callback.
    //
    WDF_DRIVER_CONFIG_INIT(&driverConfig,
                           CDFilterEvtDeviceAdd);

    //
    // Create the framework driver object...
    //
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &driverConfig,
                             WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDriverCreate failed - 0x%x\n",
                 status);
#endif
    }

    return status;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CDFilterEvtDeviceAdd
//
//    This routine is called by the framework when a device of
//    the type we filter is found in the system.
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
CDFilterEvtDeviceAdd(WDFDRIVER       Driver,
                     PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS               status;
    WDF_OBJECT_ATTRIBUTES  objAtttributes;
    WDFDEVICE              wdfDevice;
    PFILTER_DEVICE_CONTEXT filterContext;
    WDF_IO_QUEUE_CONFIG    queueConfig;


    UNREFERENCED_PARAMETER(Driver);

#if DBG
    DbgPrint("CDFilter: Adding device...\n");
#endif

    //
    // Indicate that we're creating a FILTER device.  This will cause
    // KMDF to attach us correctly to the device stack and auto-forward
    // any requests we don't explicitly handle.
    //
    WdfFdoInitSetFilter(DeviceInit);

    //
    // Setup our device attributes to have our context type
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAtttributes,
                                            FILTER_DEVICE_CONTEXT);

    //
    // And create our WDF device
    //
    status = WdfDeviceCreate(&DeviceInit,
                             &objAtttributes,
                             &wdfDevice);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreate failed - 0x%x\n",
                 status);
#endif
        goto Done;
    }

    filterContext = CDFilterGetDeviceContext(wdfDevice);

    filterContext->WdfDevice = wdfDevice;


    //
    //  Create a Queue that will allow us to pick off any I/O requests
    //  that we may be interested in before they are forwarded
    //  to the target device.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchParallel);

    //
    // For this exercise we want to see all the READ Requests.
    // Note that all OTHER Requests (such as write and device control Requests)
    // will be automatically forwarded to our Local I/O Target by the Framework.
    //
    queueConfig.EvtIoRead = CDFilterEvtRead;

    //
    // Create the queue...
    //
    status = WdfIoQueueCreate(wdfDevice,
                              &queueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              nullptr);

    if (!NT_SUCCESS(status)) {

#if DBG
        DbgPrint("WdfIoQueueCreate failed - 0x%x\n",
                 status);
#endif
        goto Done;
    }


    status = STATUS_SUCCESS;

Done:

    return status;
}

///////////////////////////////////////////////////////////////////////////////
//
//  CDFilterEvtRead
//
//    This routine is called by the framework for read requests
//    being sent to the device we're filtering
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
CDFilterEvtRead(WDFQUEUE   Queue,
                WDFREQUEST Request,
                size_t     Length)
{
    WDF_REQUEST_SEND_OPTIONS options;
    NTSTATUS                 status;
    PFILTER_DEVICE_CONTEXT   devContext;

#if DBG
    DbgPrint("CDFilterEvtRead: Processing read. Length = 0x%x\n",
             Length);
#endif

    //
    // Get the context that we setup during DeviceAdd processing
    //
    devContext = CDFilterGetDeviceContext(WdfIoQueueGetDevice(Queue));

    //
    // We're just going to be passing this Request on with 
    // zero regard for what happens to it and we don't want to handle
    // it at all after the I/O Target completes it.  Therefore, we'll
    // use the WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET option
    //
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    //
    // And send it!
    // 
    if (!WdfRequestSend(Request,
                        WdfDeviceGetIoTarget(devContext->WdfDevice),
                        &options)) {

        //
        // Oops! Something bad happened and the Request was NOT sent
        // to the local I/O Target. That means we're responsible for
        // completing the Request
        //
        status = WdfRequestGetStatus(Request);
#if DBG
        DbgPrint("WdfRequestSend failed - 0x%x\n",
                 status);
#endif
        WdfRequestComplete(Request,
                           status);
    }
}
