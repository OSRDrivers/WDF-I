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
extern "C"
NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

#if DBG
    DbgPrint("CDFilter: OSR CDFILTER Filter Driver...Compiled %s %s\n" ,__DATE__, __TIME__);
#endif

    //
    // Initialize our driver config structure, specifying our 
    // EvtDeviceAdd event processing callback.
    //
    WDF_DRIVER_CONFIG_INIT(&config,
                           CDFilterEvtDeviceAdd);

    //
    // Create the framework driver object...
    //
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             NULL);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDriverCreate failed - 0x%x\n", status);
#endif
        return status;
    }

    return STATUS_SUCCESS;
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
CDFilterEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{

    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES wdfObjectAttr;
    WDFDEVICE wdfDevice;
    PFILTER_DEVICE_CONTEXT devContext;
    WDF_IO_QUEUE_CONFIG ioQueueConfig;

#if DBG
    DbgPrint("CDFilterEvtDeviceAdd: Adding device...\n");
#endif

   UNREFERENCED_PARAMETER(Driver);

    //
    // Indicate that we're creating a FILTER device.  This will cause
    // KMDF to attach us correctly to the device stack and auto-forward
    // any requests we don't explicitly handle.
    //
    WdfFdoInitSetFilter(DeviceInit);


    //
    // Setup our device attributes to have our context type
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&wdfObjectAttr, 
                                            FILTER_DEVICE_CONTEXT);


    //
    // And create our WDF device. This does a multitude of things,
    //  including:
    //
    //  1) Creating a WDM device object
    //  2) Attaching the device object to the filtered device object
    //  3) Propogates all of the flags and characteristics of the 
    //     target device to our filter device. So, for example, if 
    //     the target device is setup for direct I/O our filter 
    //     device will also be setup for direct I/O
    //
    status = WdfDeviceCreate(&DeviceInit, 
                             &wdfObjectAttr, 
                             &wdfDevice);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreate failed - 0x%x\n", status);
#endif
        return status;
    }

    //
    // Get our filter context
    //
    devContext = CDFilterGetDeviceContext(wdfDevice);

   //
    // Get our local (aka "in-stack") I/O Target. This is the device
    // to which we'll be forwarding requests that we intercept (and
    // hence aren't automagically forwarded).
    //
    devContext->TargetToSendRequestsTo = WdfDeviceGetIoTarget(wdfDevice);

    //
    // Now that that's over with, we can create our default queue.
    //  This queue will allow us to pick off any I/O requests
    //  that we may be interested in before they are forwarded
    //  to the target device.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
                                           WdfIoQueueDispatchParallel);

    //
    // For this exercise we'll intercept READ requests
    //
    ioQueueConfig.EvtIoRead = CDFilterEvtRead;

    //
    // Create the queue...
    //
    status = WdfIoQueueCreate(wdfDevice,
                              &ioQueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              NULL);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfIoQueueCreate failed - 0x%x\n", status);
#endif
        return status;
    }

   //
    // Success!
    //
    return STATUS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//
//  WdfFltrReadComplete
//
//      This routine is our completion routine for read requests sent to the 
//      filtered device
//
//  INPUTS:
//
//      Queue   - Our filter device's default WDF queue
//
//      Target  - The I/O target of our default queue (i.e. an I/O target that
//                targets the filtered device
//
//      Params  - The completion information for the request
//
//      Context - Whatever context we supplied to WdfRequestSetCompletionRoutine
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      None
//
//  IRQL:
//
//      Depends entirely on the device that you're filtering. Typically
//      <= DISPATCH_LEVEL
//
//  NOTES:
//
///////////////////////////////////////////////////////////////////////////////
VOID
CDFilterReadComplete(
    IN WDFREQUEST Request,
    IN WDFIOTARGET Target,
    IN PWDF_REQUEST_COMPLETION_PARAMS Params,
    IN WDFCONTEXT Context
    ) 
{

   UNREFERENCED_PARAMETER(Context);
   UNREFERENCED_PARAMETER(Target);

   //
    // Print the information to the debugger
    //
#if DBG
    DbgPrint("CDFilterReadComplete: Status-0x%x; Information-0x%x\n",
        Params->IoStatus.Status, 
        Params->IoStatus.Information);
#endif

    //
    // Restart completion processing.
    //
    WdfRequestComplete(Request, Params->IoStatus.Status);
    
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
CDFilterEvtRead(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
    NTSTATUS status;
    PFILTER_DEVICE_CONTEXT devContext;

#if DBG
    DbgPrint("CDFilterEvtRead: Processing read. Length = 0x%x\n", Length);
#endif

    //
    // Get the context that we setup during DeviceAdd processing
    //
    devContext = CDFilterGetDeviceContext(WdfIoQueueGetDevice(Queue));

    //
    // Setup the request for the next driver
    //
    WdfRequestFormatRequestUsingCurrentType(Request);

    //
    // Set the completion routine...
    //
    WdfRequestSetCompletionRoutine(Request,
                                   CDFilterReadComplete,
                                   NULL);

    //
    // And send it!
    // 
    if (!WdfRequestSend(Request, 
                        devContext->TargetToSendRequestsTo, 
                        WDF_NO_SEND_OPTIONS)) {

        //
        // Oops! Something bad happened, complete the request
        //
        status = WdfRequestGetStatus(Request);
#if DBG
        DbgPrint("WdfRequestSend failed - 0x%x\n", status);
#endif
        WdfRequestComplete(Request, status);
    }

    return;

}


