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

#include "basicusb.h"

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
#pragma prefast(suppress:28101, "DRIVER_INITIALIZE declared above")
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS          status;

#if DBG
    DbgPrint("\nOSR BasicUsb Driver -- Compiled %s %s\n",__DATE__, __TIME__);
#endif

    //
    // Provide pointer to our EvtDeviceAdd event processing callback
    // function
    //
    WDF_DRIVER_CONFIG_INIT(&config, BasicUsbEvtDeviceAdd);


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
//  BasicUsbEvtDeviceAdd
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
BasicUsbEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                     status;
    WDF_OBJECT_ATTRIBUTES        objAttributes;
    WDFDEVICE                    device;
    WDF_IO_QUEUE_CONFIG          queueConfig;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

#if DBG
    DbgPrint("BasicUsbEvtDeviceAdd\n");
#endif
   
    
    //
    // Our "internal" (native) and user-accessible device names
    //
    DECLARE_CONST_UNICODE_STRING(nativeDeviceName, L"\\Device\\BasicUsb");
    DECLARE_CONST_UNICODE_STRING(userDeviceName, L"\\Global??\\BasicUsb");
    
    UNREFERENCED_PARAMETER(Driver);

    //
    // Life is a bit more complicated in this driver...
    //
    // We need an EvtPrepareHardware to configure our device. In addition, we
    // must handle EvtD0Entry and EvtD0Exit in order to manage our continuous
    // reader.
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
                                        BASICUSB_DEVICE_CONTEXT);

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
    // Set our I/O type to DIRECT, meaning that we want to receive
    // MDLs for both read and write requests.
    //
    // While we are not obligated to choose direct, USB drivers
    // typically do so because the USB bus driver needs MDLs to
    // actually perform the transfer. If we select DIRECT I/O, the
    // bus driver can just use the MDL that we're given as opposed
    // to creating his own.
    //
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
    
    //
    // In this driver we need to be notified of some Pnp/Power
    // events, so initialize a pnp power callbacks structure.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    //
    // USB drivers configure their device within prepare hardware,
    // so register an EvtDevicePrepareHardware callback.
    //
    pnpPowerCallbacks.EvtDevicePrepareHardware 
                                     = BasicUsbEvtDevicePrepareHardware;

    //
    // Our driver uses a continuous reader on the interrupt pipe to
    // be notified asynchronously of changes to the OSRFX2's
    // switchpack. We need to start the reader in D0Entry and stop
    // it in D0Exit, so register for EvtDeviceD0Entry and
    // EvtDeviceD0Exit callbacks
    //
    pnpPowerCallbacks.EvtDeviceD0Entry = BasicUsbEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit  = BasicUsbEvtDeviceD0Exit;  

    //
    // Update the DeviceInit structure to contain the new callbacks.
    //
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, 
                                           &pnpPowerCallbacks);


    //
    // We want to send control transfers synchronously from within
    // EvtIoDeviceControl, so we'll apply a PASSIVE_LEVEL constraint
    // to our device
    //
    objAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    //
    // Now let's create our device object
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
    // ALSO create a device interface for the device
    // This allows usage of the lovely SetupApiXxxx functions to locate
    // the device
    //
    status = WdfDeviceCreateDeviceInterface(device,
                                    &GUID_DEVINTERFACE_BASICUSB,
                                    NULL);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateDeviceInterface failed 0x%0x\n", status);
#endif
        return(status);
    }

    //
    // Configure our queue of incoming requests
    //
    // We only use the default queue, and we set it for parallel processing.
    // We chose this because we may have, say, a bulk read hanging on the bus
    // driver that won't get completed until we send down a bulk write.
    //
    // If we chose a sequential queue we wouldn't be presented the write from
    // the user until the read completed, but the read wouldn't complete
    // until we were presented the write from the user - resulting in an "I/O
    // deadlock".
    //
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchParallel);

    //
    // Declare our I/O Event Processing callbacks
    //
    // We handle, read, write, and device control requests.
    //
    // WDF will automagically handle Create and Close requests for us and will
    // will complete any OTHER request types with STATUS_INVALID_DEVICE_REQUEST.    
    //
    queueConfig.EvtIoRead = BasicUsbEvtRead;
    queueConfig.EvtIoWrite = BasicUsbEvtWrite;
    queueConfig.EvtIoDeviceControl = BasicUsbEvtDeviceControl;

    //
    // Because this is a queue for a real hardware
    // device, indicate that the queue needs to be
    // power managed
    //
    queueConfig.PowerManaged = WdfTrue;

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
//  BasicUsbEvtDevicePrepareHardware
//
//    This routine is called by the framework when a device of
//    the type we support is coming online. Our job will be to
//    create our WDFUSBDEVICE and configure it.
//
//  INPUTS:
//
//      Device       - One of our WDFDEVICE objects
//
//      ResourceList - We're a USB device, so not used
//
//      ResourceListTranslated - We're a USB device, so not
//                               used
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
BasicUsbEvtDevicePrepareHardware(
    IN WDFDEVICE Device,
    IN WDFCMRESLIST ResourceList,
    IN WDFCMRESLIST ResourceListTranslated
    ) {

    NTSTATUS                            status;
    PBASICUSB_DEVICE_CONTEXT            devContext;   
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS selectConfigParams;
    WDFUSBINTERFACE                     configuredInterface;
    WDF_USB_PIPE_INFORMATION            pipeInfo;
    UCHAR                               numPipes;
    UCHAR                               pipeIndex;
    WDFUSBPIPE                          configuredPipe;
    WDF_USB_CONTINUOUS_READER_CONFIG    contReaderConfig;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

#if DBG
    DbgPrint("BasicUsbEvtDevicePrepareHardware\n");
#endif

    devContext = BasicUsbGetContextFromDevice(Device);

    //
    // First thing to do is create our WDFUSBDEVICE. This is the
    // special USB I/O target that we'll be using to configure our
    // device and to send control requests.
    //
    // Under very rare cirumstances (i.e. resource rebalance of the
    // host controller) it's possible to come through here multiple
    // times. We could handle this by having an
    // EvtDeviceReleaseHardware and cleaning up the USB device
    // target, but we'll just leave it around and avoid creating it
    // multiple times with this check. No race condition as our
    // Prepare and Release can't run in parallel for the same device
    // 
    if (devContext->BasicUsbUsbDevice == NULL) {

        status = WdfUsbTargetDeviceCreate(Device, 
                                          WDF_NO_OBJECT_ATTRIBUTES,
                                          &devContext->BasicUsbUsbDevice);
        if (!NT_SUCCESS(status)) {
    #if DBG
            DbgPrint("WdfUsbTargetDeviceCreate failed 0x%0x\n", status);
    #endif
            return status;
        }

    }


    //
    // Now that our WDFUSBDEVICE is created, it's time to select
    // our configuration and enable our interface.
    //

    //
    // The OSRFX2 device only has a single interface, so we'll
    // initialize our select configuration parameters structure
    // using the specially provided macro
    //
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(
                                                &selectConfigParams);

    //
    // And actually select our configuration.
    //
    status = WdfUsbTargetDeviceSelectConfig(devContext->BasicUsbUsbDevice,
                                            WDF_NO_OBJECT_ATTRIBUTES,
                                            &selectConfigParams);
    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfUsbTargetDeviceSelectConfig failed 0x%0x\n", status);
#endif
        return status;

    }

    //
    // Our single interface has been configured. Let's grab the
    // WDFUSBINTERFACE object so that we can get our pipes.
    //
    configuredInterface 
            = selectConfigParams.Types.SingleInterface.ConfiguredUsbInterface;

    //
    // How many pipes were configure?
    //
    numPipes = selectConfigParams.Types.SingleInterface.NumberConfiguredPipes;

    //
    // For all the pipes that were configured....
    //
    for(pipeIndex = 0; pipeIndex < numPipes; pipeIndex++) {

        //
        // We'll need to find out the type the pipe, which we'll do
        // by supplying a pipe information structure when calling
        // WdfUsbInterfaceGetConfiguredPipe
        //
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

        //
        // Get the configured pipe.
        //
        configuredPipe = WdfUsbInterfaceGetConfiguredPipe(configuredInterface,
                                                          pipeIndex,
                                                          &pipeInfo);

        //
        // This device has three pipes
        //
        // 1) A Bulk IN pipe
        // 2) A Bulk OUT pipe
        // 3) An Interrupt IN pipe
        //
        // This implementation of the driver just works with the bulk out
        // and interrupt in pipes
        //
        
        //
        // First, let's see what type of pipe it is...
        //
        switch (pipeInfo.PipeType) {
            case WdfUsbPipeTypeBulk: {

                //
                // Bulk pipe. Determine if it's an OUT pipe
                //
                if (WdfUsbTargetPipeIsOutEndpoint(configuredPipe)) {

                    //
                    // Bulk OUT pipe. Should only ever get one of these...
                    //
                    ASSERT(devContext->BulkOutPipe == NULL);

                    devContext->BulkOutPipe = configuredPipe;
                }
                break;
            }
            case WdfUsbPipeTypeInterrupt: {
                //
                // We're only expecting an IN interrupt pipe
                //
                ASSERT(WdfUsbTargetPipeIsInEndpoint(configuredPipe));

                //
                // And we're only expected one of them
                //
                ASSERT(devContext->InterruptInPipe == NULL);

                devContext->InterruptInPipe = configuredPipe;
                break;
            }
            default: {
                //
                // Don't know what it is, don't care what it is...
                //
#if DBG
                DbgPrint("Unexpected pipe type? 0x%x\n", pipeInfo.PipeType);
#endif                
                break;
            }

        }

    }

    //
    // We hopefully have found everything we need...
    //
    if (devContext->BulkOutPipe == NULL ||
        devContext->InterruptInPipe == NULL) {
#if DBG
        DbgPrint("Didn't find expected pipes. BOUT=0x%p, IIN=0x%p\n",
                    devContext->BulkOutPipe,
                    devContext->InterruptInPipe);

#endif
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // By default, KMDF will not allow any non-MaxPacketSize
    // aligned I/O to be done against IN pipes. This is to avoid
    // hitting "babble" conditions, which occur when the device
    // sends more data than what you've asked it for.
    //
    // Our device doesn't babble, so we don't need this check on
    // our IN pipes.
    //
    WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(devContext->InterruptInPipe);


    //
    // For fun, we're going to hang a continuous reader out on the
    // interrupt endpoint. By doing so, we'll get called at
    // BasicUsbInterruptPipeReadComplete every time someone toggles
    // the switches on the switch pack.
    //


    //
    // Initialize the continuous reader config structure, specifying
    // our callback, our context, and the size of the transfers.
    //
    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&contReaderConfig,
                                          BasicUsbInterruptPipeReadComplete,
                                          devContext,
                                          sizeof(UCHAR));

    //
    // And create the continuous reader.
    //
    // Note that the continuous reader is not started by default, so
    // we'll need to manually start it when we are called at
    // EvtD0Entry.
    //
    status = WdfUsbTargetPipeConfigContinuousReader(devContext->InterruptInPipe,
                                                    &contReaderConfig);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfUsbTargetPipeConfigContinuousReader failed 0x%0x\n", 
                 status);
#endif
        return status;
    }
        
    return STATUS_SUCCESS;

}

///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbEvtDeviceD0Entry
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
BasicUsbEvtDeviceD0Entry(
    IN WDFDEVICE Device,
    IN WDF_POWER_DEVICE_STATE PreviousState
    ) {

    PBASICUSB_DEVICE_CONTEXT devContext;   
    NTSTATUS                 status;
    WDFIOTARGET              interruptIoTarget;

    UNREFERENCED_PARAMETER(PreviousState);


#if DBG
    DbgPrint("BasicUsbEvtDeviceD0Entry\n");
#endif

    devContext = BasicUsbGetContextFromDevice(Device);

    //
    // We created our continuous reader in PrepareHardware, but we
    // never started it. So, we'll start it now with
    // WdfIoTargetStart
    //

    //
    // First, get the I/O target associated with our INT IN pipe
    //
    interruptIoTarget = 
            WdfUsbTargetPipeGetIoTarget(devContext->InterruptInPipe);

    //
    // And start the continuous reader!
    //
    status = WdfIoTargetStart(interruptIoTarget);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfIoTargetStart failed 0x%0x\n", status);
#endif
        return status;
    }

    return STATUS_SUCCESS;

}


///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbEvtDeviceD0Exit
//
//    This routine is called by the framework when a device of
//    the type we support is leaving the D0 state
//
//  INPUTS:
//
//      Device       - One of our WDFDEVICE objects
//
//      PreviousState - The D-State we're entering
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
BasicUsbEvtDeviceD0Exit(
    IN WDFDEVICE Device,
    IN WDF_POWER_DEVICE_STATE TargetState
    ) {

    PBASICUSB_DEVICE_CONTEXT devContext;   
    WDFIOTARGET              interruptIoTarget;

    UNREFERENCED_PARAMETER(TargetState);

#if DBG
    DbgPrint("BasicUsbEvtDeviceD0Exit\n");
#endif

    devContext = BasicUsbGetContextFromDevice(Device);

    //
    // It is our responsibility to stop the continous reader before
    // leaving D0.
    //

    //
    // First, ge the I/O target associated with our INT IN pipe.
    //
    interruptIoTarget = 
            WdfUsbTargetPipeGetIoTarget(devContext->InterruptInPipe);

    //
    // And stop the I/O target, leaving any outstanding requests
    // pending.
    //
    // Note that if we're leaving D0 because we're being removed
    // the framework will cancel all the requests and clean up
    // the I/O target for us.
    //
    WdfIoTargetStop(interruptIoTarget,
                    WdfIoTargetLeaveSentIoPending);

    return STATUS_SUCCESS;

}

///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbInterruptPipeReadComplete
//
//    This is the callback we supplied for the continuous reader
//    on the interrupt IN pipe. It is called whenever the user
//    changes the state of the switches on the switch pack
//
//  INPUTS:
//
//      Pipe    - Our interrupt IN pipe
//
//      Buffer  - The WDFMEMORY object associated with the
//                 transfer. The buffer of thie memory contains
//                 the state of the switch pack
//
//      NumBytesTransferred - Self explanatory
//
//      Context - One of our per device context structures
//                (passed as a parameter to
//                 WDF_USB_CONTINUOUS_READER_CONFIG_INIT)
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
//      This routine is called at IRQL <= DISPATCH_LEVEL.
//
//  NOTES:
//
//      Even though we've applied a PASSIVE_LEVEL execution
//      level constraint on our device, this callback falls
//      outstide of the callbacks that the constraint is
//      enforced on.
//
///////////////////////////////////////////////////////////////////////////////
VOID
BasicUsbInterruptPipeReadComplete(
    IN WDFUSBPIPE Pipe,
    IN WDFMEMORY Buffer,
    IN size_t NumBytesTransferred,
    IN WDFCONTEXT Context
    ) {

    PUCHAR dataBuffer;

    UNREFERENCED_PARAMETER(Pipe);
    UNREFERENCED_PARAMETER(NumBytesTransferred);
    UNREFERENCED_PARAMETER(Context);

    //
    // Someone toggled the switch pack. Print out the new state
    //
    dataBuffer = (PUCHAR)WdfMemoryGetBuffer(Buffer, NULL);

#if DBG
    DbgPrint("Interrupt read complete. Bytes transferred = 0x%x, Data = 0x%x\n",
             (ULONG)NumBytesTransferred, *dataBuffer);
#endif

    return;
}


///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbEvtRead
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
//      This routine is called at IRQL == PASSIVE_LEVEL, due to
//      our PASSIVE_LEVEL execution level contraint
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
VOID
BasicUsbEvtRead(WDFQUEUE  Queue, WDFREQUEST  Request, size_t  Length)
{
    PBASICUSB_DEVICE_CONTEXT devContext;

    UNREFERENCED_PARAMETER(Length);

#if DBG
    DbgPrint("BasicUsbEvtRead\n");
#endif

    //
    // Get a pointer to our device extension, just to show how it's done.
    // (get the WDFDEVICE from the WDFQUEUE, and the extension from the device)
    //
    devContext = BasicUsbGetContextFromDevice(
                                WdfIoQueueGetDevice(Queue) );

    //
    // Nothing to do yet... We're returning zero bytes, so fill that
    // into the information field
    //
    WdfRequestCompleteWithInformation(Request,
                                    STATUS_SUCCESS,
                                    0);    
}


///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbEvtWrite
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
//      This routine is called at IRQL == PASSIVE_LEVEL, due to
//      our PASSIVE_LEVEL execution level contraint
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
VOID
BasicUsbEvtWrite(WDFQUEUE  Queue, WDFREQUEST  Request, size_t  Length)
{
    PBASICUSB_DEVICE_CONTEXT devContext;
    WDFMEMORY                requestMemory;
    NTSTATUS                 status;
    

    UNREFERENCED_PARAMETER(Length);

#if DBG
    DbgPrint("BasicUsbEvtWrite\n");
#endif

    devContext = BasicUsbGetContextFromDevice(
                                WdfIoQueueGetDevice(Queue) );

    //
    // The purpose of this routine will be to convert the write
    // that we received from the user into a USB request and send
    // it to the underlying bus driver.
    //


    //
    // First thing to do is get the input memory associated with the
    // request (only because it's a required parameter to
    // WdfUsbTargetPipeFormatRequestForWrite)
    //
    status = WdfRequestRetrieveInputMemory(Request, 
                                           &requestMemory);
    if(!NT_SUCCESS(status)){
#if DBG
        DbgPrint("WdfRequestRetrieveInputMemory failed 0x%0x\n", status);
#endif
        WdfRequestComplete(Request, status);
        return;
    }

    //
    // Take the user Write request and format it into a Bulk OUT
    // request.
    //
    status = WdfUsbTargetPipeFormatRequestForWrite(devContext->BulkOutPipe,
                                                   Request,
                                                   requestMemory,
                                                   NULL);
    if(!NT_SUCCESS(status)){
#if DBG
        DbgPrint("WdfUsbTargetPipeFormatRequestForWrite failed 0x%0x\n", 
                 status);
#endif
        WdfRequestComplete(Request, status);
        return;
    }


    //
    // We'd like to asynchronously send this newly formatted request
    // to the underlying bus driver. In order to do that, we *must*
    // supply a completion routine that calls WdfRequestComplete.
    // Failure to do so is not architecturally defined, which means
    // it ain't gonna work.
    //
    // Note that because we have modified the request, we are not
    // allowed to use the WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET
    // flag as a cheap shortcut, we MUST supply the completion
    // routine.
    //
    WdfRequestSetCompletionRoutine(Request,
                                   BasicUsbEvtRequestWriteCompletionRoutine,
                                   NULL);

    //
    // Send the request!
    //
    if (!WdfRequestSend(Request,
                        WdfUsbTargetPipeGetIoTarget(devContext->BulkOutPipe),
                        NULL)) {

        //
        // Bad news. The target didn't get the request, so get the
        // failure status and complete the request ourselves..
        //
        status = WdfRequestGetStatus(Request);
#if DBG
        DbgPrint("WdfRequestSend failed 0x%0x\n", status);
#endif
        WdfRequestComplete(Request, status);
        return;
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbEvtRequestWriteCompletionRoutine
//
//    This routine is called by the framework when a write
//    request has been completed by the device
//
//  INPUTS:
//
//      Request  - The write request
//
//      Target   - The I/O target we sent the write to
//
//      Params   - Parameter information from the completed
//                 request
//
//      Context  - The context supplied to
//                 WdfRequestSetCompletionRoutine (NULL in
//                 our case)
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
//      This routine is called at IRQL <= DISPATCH_LEVEL.
//
//  NOTES:
//
//      Even though we've applied a PASSIVE_LEVEL execution
//      level constraint on our device, this callback falls
//      outstide of the callbacks that the constraint is
//      enforced on.
//
///////////////////////////////////////////////////////////////////////////////
VOID
BasicUsbEvtRequestWriteCompletionRoutine(
    IN WDFREQUEST Request,
    IN WDFIOTARGET Target,
    IN PWDF_REQUEST_COMPLETION_PARAMS Params,
    IN WDFCONTEXT Context
    ) {

    PWDF_USB_REQUEST_COMPLETION_PARAMS usbParams;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    //
    // The transfer has been completed on the USB. Get a pointer to
    // the USB specific completion information.
    //
    usbParams = Params->Parameters.Usb.Completion;

#if DBG
    DbgPrint("USB write completed with 0x%0x. Bytes transferred 0x%x\n", 
             Params->IoStatus.Status,
             (ULONG)usbParams->Parameters.PipeWrite.Length);
#endif

    //
    // Now complete the request back to the user, specifying the
    // result of the operation.
    //
    WdfRequestCompleteWithInformation(Request, 
                                      Params->IoStatus.Status, 
                                      usbParams->Parameters.PipeWrite.Length);

    return;
}


///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbEvtDeviceControl
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
BasicUsbEvtDeviceControl(WDFQUEUE Queue,
            WDFREQUEST Request,
            size_t OutputBufferLength,
            size_t InputBufferLength,
            ULONG IoControlCode)
{

    NTSTATUS                     status;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    WDFMEMORY                    inputMemory;
    WDF_MEMORY_DESCRIPTOR        inputMemoryDescriptor;
    PBASICUSB_DEVICE_CONTEXT     devContext;
   
#if DBG
    DbgPrint("BasicUsbEvtDeviceControl\n");
#endif

    devContext = BasicUsbGetContextFromDevice(
                                WdfIoQueueGetDevice(Queue) );

    switch (IoControlCode) {
        case IOCTL_OSR_BASICUSB_SET_BAR_GRAPH: {

            //
            // Validate the buffers for this request:
            //
            // OutputBufferLength - Must be zero
            //
            if (OutputBufferLength != 0) {
#if DBG
                DbgPrint("Invalid parameter - output buffer supplied in "\
                         "SET_BAR_GRAPH IOCTL (%u bytes)\n",
                         (ULONG)OutputBufferLength); 
#endif
                WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
                return;
            }

            //
            // InputBufferLength  - Must be at least 1 byte
            //
            if (InputBufferLength == 0) {
#if DBG
                DbgPrint("No input buffer supplied in SET_BAR_GRAPH IOCTL\n");
#endif
                WdfRequestCompleteWithInformation(Request,
                                                  STATUS_BUFFER_TOO_SMALL,
                                                  sizeof(UCHAR));
                return;
            }

            //
            // We need the input memory from the request so that we can pass
            // it to the bus driver.
            //
            status = WdfRequestRetrieveInputMemory(Request,
                                                   &inputMemory);

            if (!NT_SUCCESS(status)) {
#if DBG
                DbgPrint("WdfRequestRetrieveInputMemory failed 0x%0x\n", status);
#endif
                WdfRequestComplete(Request, status);
                return;
            }

            //
            // The routine we want to call takes a memory descriptor, so
            // initialize that now with the handle to the user memory.
            //
            WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&inputMemoryDescriptor,
                                              inputMemory,
                                              NULL);

            //
            // Initialize the vendor command (defined by the device) that
            // allows us to light the bar graph.
            //
            WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
                                               &controlSetupPacket,
                                               BmRequestHostToDevice,
                                               BmRequestToDevice,
                                               USBFX2LK_SET_BARGRAPH_DISPLAY,
                                               0,
                                               0);


            //
            // And send the vendor command as a control transfer. This
            // shouldn't take very long, so we'll just send it synchronously
            // to the device.
            //

            //
            // We've specified an execution level restraint of
            // PASSIVE_LEVEL, so we're allowed to send this request
            // synchronously.
            //
            ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
            status = WdfUsbTargetDeviceSendControlTransferSynchronously(
                                        devContext->BasicUsbUsbDevice,
                                        WDF_NO_HANDLE,
                                        NULL,
                                        &controlSetupPacket,
                                        &inputMemoryDescriptor,
                                        NULL);
            if (NT_SUCCESS(status)) {
                //
                // If the request succeeded, complete the request with success
                // and indicate to the user how much data was transferred (in
                // this case a single byte was sent to the device)
                //
                WdfRequestCompleteWithInformation(Request,
                                                  status,
                                                  sizeof(UCHAR));

            } else {
                //
                // Bad news! Just complete the request with the failure status.
                //
#if DBG
                DbgPrint("WdfUsbTargetDeviceSendControlTransferSynchronously "\
                         "failed 0x%0x\n", status); 
#endif

                WdfRequestComplete(Request, status);
            }
            return;
        }
        default:  {
#if DBG
            DbgPrint("Unknown IOCTL: 0x%x\n", IoControlCode);
#endif
            WdfRequestCompleteWithInformation(Request,
                                              STATUS_INVALID_DEVICE_REQUEST,
                                              0);
            return;
        }
    }

    return;
}
