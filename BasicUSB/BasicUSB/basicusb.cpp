//
// Copyright 2007-2023 OSR Open Systems Resources, Inc.
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
NTSTATUS
DriverEntry(PDRIVER_OBJECT  DriverObject,
            PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS          status;

#ifdef _KERNEL_MODE

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);
#endif

#if DBG
    DbgPrint("\nOSR BasicUsb Driver -- Compiled %s %s\n",
             __DATE__,
             __TIME__);
#endif

    //
    // Provide pointer to our EvtDeviceAdd event processing callback
    // function
    //
    WDF_DRIVER_CONFIG_INIT(&config,
                           BasicUsbEvtDeviceAdd);

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
        DbgPrint("WdfDriverCreate failed 0x%0x\n",
                 status);
#endif
    }

    return (status);
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
BasicUsbEvtDeviceAdd(WDFDRIVER       Driver,
                     PWDFDEVICE_INIT DeviceInit)
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
    // Our user-accessible device names
    //
#ifdef _KERNEL_MODE

    DECLARE_CONST_UNICODE_STRING(userDeviceName,
                                 L"\\DosDevices\\BasicUsb");
#else

    DECLARE_CONST_UNICODE_STRING(userDeviceName,
                                 L"\\DosDevices\\Global\\BasicUsb");
#endif


    UNREFERENCED_PARAMETER(Driver);

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

#if _KERNEL_MODE
    //
    // Set our I/O type to DIRECT, meaning that we want do not want
    // to copy data sent with read/write.
    //
    WdfDeviceInitSetIoType(DeviceInit,
                           WdfDeviceIoDirect);
#endif

    //
    // In this driver we need to be notified of some Pnp/Power
    // events, so initialize a pnp power callbacks structure.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    //
    // USB drivers create their I/O Targets in within prepare hardware
    //
    pnpPowerCallbacks.EvtDevicePrepareHardware = BasicUsbEvtDevicePrepareHardware;

    //
    // Our driver uses a continuous reader on the interrupt pipe to
    // be notified asynchronously of changes to the OSRFX2's
    // switchpack. The reader is started in D0Entry and stopped
    // in D0Exit.
    //
    pnpPowerCallbacks.EvtDeviceD0Entry = BasicUsbEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit  = BasicUsbEvtDeviceD0Exit;

    //
    // Copy the PnP/Power related callbacks to the DeviceInit structure
    //
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit,
                                           &pnpPowerCallbacks);


    //
    // We want to send control transfers synchronously from within
    // EvtIoDeviceControl. This involves waiting with that EvtIo callback
    // so we need to apply a PASSIVE_LEVEL constraint.
    //
    objAttributes.ExecutionLevel = WdfExecutionLevelPassive;

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
    // Create a symbolic link for the control object so that usermode can open
    // the device by name.
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
    // This allows usage of the lovely SetupApiXxxx functions to locate
    // the device
    //
    status = WdfDeviceCreateDeviceInterface(device,
                                            &GUID_DEVINTERFACE_BASICUSB,
                                            nullptr);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfDeviceCreateDeviceInterface failed 0x%0x\n",
                 status);
#endif
        goto Done;
    }

    //
    // Configure our queue of incoming requests
    //
    // We only use the default queue, and we set it for parallel dispatching.
    // This allows us to have multiple Requests (including both writes and reads)
    // in-progress simultaneously from our single default Queue.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
                                           WdfIoQueueDispatchParallel);

    //
    // Declare our I/O Event Processing callbacks
    //
    // We handle read, write, and device control requests.
    //
    // The Framework automagically handles Create and Close requests for us and will
    // will complete any OTHER request types with STATUS_INVALID_DEVICE_REQUEST.
    //
    queueConfig.EvtIoRead          = BasicUsbEvtRead;
    queueConfig.EvtIoWrite         = BasicUsbEvtWrite;
    queueConfig.EvtIoDeviceControl = BasicUsbEvtDeviceControl;

    //
    // Because this is a queue for a real hardware device, indicate that the
    // queue needs to be power managed (which is the default, but it's nice to
    // be explicit)
    //
    queueConfig.PowerManaged = WdfTrue;

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

    DbgPrint("DeviceAdd completes with status 0x%lx\n", status);

    DbgBreakPoint();


    return (status);
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
BasicUsbEvtDevicePrepareHardware(WDFDEVICE    Device,
                                 WDFCMRESLIST ResourceList,
                                 WDFCMRESLIST ResourceListTranslated)
{
    NTSTATUS                            status;
    PBASICUSB_DEVICE_CONTEXT            devContext;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS selectConfigParams;
    WDFUSBINTERFACE                     configuredInterface;
    WDF_USB_PIPE_INFORMATION            pipeInfo;
    UCHAR                               numPipes;
    WDFUSBPIPE                          configuredPipe;
    WDF_USB_CONTINUOUS_READER_CONFIG    contReaderConfig;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

#if DBG
    DbgPrint("BasicUsbEvtDevicePrepareHardware\n");
#endif

    devContext = BasicUsbGetContextFromDevice(Device);

    //
    // First thing to do is create our WDFUSBDEVICE Target.
    // This is the USB Special I/O target that we'll be using to configure
    // our device and to send Vendor Commands.
    //
    // Under very rare cirumstances (i.e. resource rebalance of the
    // host controller) it's possible for PrepareHardware to be called
    // multiple times without the system being rebooted.  We could handle
    // this by having an EvtDeviceReleaseHardware and cleaning up the USB
    // device target, but we'll just leave it around and avoid creating it
    // multiple times with this check.
    //
    if (devContext->UsbDeviceTarget == nullptr) {

#if NTDDI_VERSION >= NTDDI_WIN8

        WDF_USB_DEVICE_CREATE_CONFIG        deviceCreateConfig;

        //
        // Use the features of the "newer" USB stack. This requires building the
        // driver to target & run on Win 8 or later, where the new contract is supported.
        //
        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&deviceCreateConfig,
                                          USBD_CLIENT_CONTRACT_VERSION_602);

        status = WdfUsbTargetDeviceCreateWithParameters(Device,
                                                        &deviceCreateConfig,
                                                        WDF_NO_OBJECT_ATTRIBUTES,
                                                        &devContext->UsbDeviceTarget);
#else

        status = WdfUsbTargetDeviceCreate(Device,
                                          WDF_NO_OBJECT_ATTRIBUTES,
                                          &devContext->UsbDeviceTarget);
#endif

        if (!NT_SUCCESS(status)) {
#if DBG
            DbgPrint("WdfUsbTargetDeviceCreate failed 0x%0x\n",
                     status);
#endif
            goto Done;
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
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&selectConfigParams);

    //
    // And select our configuration...
    //
    status = WdfUsbTargetDeviceSelectConfig(devContext->UsbDeviceTarget,
                                            WDF_NO_OBJECT_ATTRIBUTES,
                                            &selectConfigParams);
    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfUsbTargetDeviceSelectConfig failed 0x%0x\n",
                 status);
#endif
        goto Done;
    }

    //
    // Our single interface has been configured. Let's grab the
    // WDFUSBINTERFACE so that we can get our pipes.
    //
    configuredInterface
            = selectConfigParams.Types.SingleInterface.ConfiguredUsbInterface;

    //
    // How many pipes were configured?
    //
    numPipes = selectConfigParams.Types.SingleInterface.NumberConfiguredPipes;

    //
    // For each pipe that was configured....
    //
    for (UCHAR pipeIndex = 0; pipeIndex < numPipes; pipeIndex++) {

        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

        //
        // Get the USB Pipe Target and information about that pipe/endpoint
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
#if DBG
        DbgPrint("EP Addr = 0x%x, Type = 0x%x\n",
                 pipeInfo.EndpointAddress,
                 pipeInfo.PipeType);
#endif

        //
        // Let's identify our Pipe Targets using the Endpoint Address
        // (which we know from the OSR USBFX2 Data Sheet)
        //
        switch (pipeInfo.EndpointAddress) {

                //
                // Endpoint 1 IN is the OSR USB FX2 Interrupt Endpoint
                //
                // (The 0x80 bit being set means this is an IN endpoint)
                //
            case (0x01 | 0x80): {

                //
                // And we're only expected one of them
                //
                ASSERT(devContext->InterruptInPipe == nullptr);

                devContext->InterruptInPipe = configuredPipe;
                break;
            }

                //
                // Endpoint 6 OUT is an OSR USB FX2 BULK endpoint
                //
            case 0x06: {

                //
                // Bulk OUT pipe. Should only ever get one of these...
                //
                ASSERT(devContext->BulkOutPipe == nullptr);

                devContext->BulkOutPipe = configuredPipe;

                break;
            }


            default: {

                //
                // Not handling any other Endpoints at this time
                //
#if DBG
                DbgPrint("EP 0x%x type 0x%x is not used\n",
                         pipeInfo.EndpointAddress,
                         pipeInfo.PipeType);
#endif
                break;
            }

        }

    }

    //
    // We hopefully have found everything we need...
    //
    if (devContext->BulkOutPipe == nullptr ||
        devContext->InterruptInPipe == nullptr) {
#if DBG
        DbgPrint("Didn't find expected pipes. BOUT=0x%p, IIN=0x%p\n",
                 devContext->BulkOutPipe,
                 devContext->InterruptInPipe);

#endif
        status = STATUS_DEVICE_CONFIGURATION_ERROR;

        goto Done;
    }

    //
    // By default, the Framework requires all Reads to be integral
    // multiples of the max packet size.  We don't require this
    // check, so we disable it.
    //
    WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(devContext->InterruptInPipe);

    //
    // Setup a WDF Continuous Reader on the Interrupt IN Endpoint.
    // We'll get called at the provided callback every time someone
    // changes any of the switches on the switch pack.
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
    // And create the Continuous Reader.
    //
    // Note that the Continuous Reader is not started by default, so
    // we'll need to start it in EvtD0Entry.
    //
    status = WdfUsbTargetPipeConfigContinuousReader(devContext->InterruptInPipe,
                                                    &contReaderConfig);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfUsbTargetPipeConfigContinuousReader failed 0x%0x\n",
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
BasicUsbEvtDeviceD0Entry(WDFDEVICE              Device,
                         WDF_POWER_DEVICE_STATE PreviousState)
{
    PBASICUSB_DEVICE_CONTEXT devContext;
    NTSTATUS                 status;
    WDFIOTARGET              interruptIoTarget;

    UNREFERENCED_PARAMETER(PreviousState);

#if DBG
    DbgPrint("BasicUsbEvtDeviceD0Entry\n");
#endif

    devContext = BasicUsbGetContextFromDevice(Device);

    interruptIoTarget =
            WdfUsbTargetPipeGetIoTarget(devContext->InterruptInPipe);

    //
    // Start the WDF Continuous Reader on this Pipe Target
    //
    status = WdfIoTargetStart(interruptIoTarget);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfIoTargetStart failed 0x%0x\n",
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
BasicUsbEvtDeviceD0Exit(WDFDEVICE              Device,
                        WDF_POWER_DEVICE_STATE TargetState)
{
    PBASICUSB_DEVICE_CONTEXT devContext;
    WDFIOTARGET              interruptIoTarget;

    UNREFERENCED_PARAMETER(TargetState);

#if DBG
    DbgPrint("BasicUsbEvtDeviceD0Exit\n");
#endif

    devContext = BasicUsbGetContextFromDevice(Device);

    //
    // It is our responsibility to stop the Continuous Reader before
    // leaving D0.
    //
    interruptIoTarget =
            WdfUsbTargetPipeGetIoTarget(devContext->InterruptInPipe);

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
BasicUsbInterruptPipeReadComplete(WDFUSBPIPE Pipe,
                                  WDFMEMORY  Buffer,
                                  size_t     NumBytesTransferred,
                                  WDFCONTEXT Context)
{
    PUCHAR dataBuffer;

    UNREFERENCED_PARAMETER(Pipe);
    UNREFERENCED_PARAMETER(Context);

    if (NumBytesTransferred != 0) {


        //
        // Someone toggled the switch pack. Print out the new state
        //
        dataBuffer = (PUCHAR)WdfMemoryGetBuffer(Buffer,
                                                nullptr);

#if DBG
        DbgPrint("Interrupt read complete. Bytes transferred = 0x%x, Data = 0x%x\n",
                 (ULONG)NumBytesTransferred,
                 *dataBuffer);
#endif

    }
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
BasicUsbEvtRead(WDFQUEUE   Queue,
                WDFREQUEST Request,
                size_t     Length)
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
                                              WdfIoQueueGetDevice(Queue));

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
BasicUsbEvtWrite(WDFQUEUE   Queue,
                 WDFREQUEST Request,
                 size_t     Length)
{
    PBASICUSB_DEVICE_CONTEXT devContext;
    WDFMEMORY                requestMemory;
    NTSTATUS                 status;
    ULONG_PTR                bytesWritten;

    UNREFERENCED_PARAMETER(Length);

#if DBG
    DbgPrint("BasicUsbEvtWrite\n");
#endif

    devContext = BasicUsbGetContextFromDevice(
                                              WdfIoQueueGetDevice(Queue));

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
    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfRequestRetrieveInputMemory failed 0x%0x\n",
                 status);
#endif
        bytesWritten = 0;

        goto Done;
    }

    //
    // Take the user Write request and format it into a Bulk OUT
    // request.
    //
    status = WdfUsbTargetPipeFormatRequestForWrite(devContext->BulkOutPipe,
                                                   Request,
                                                   requestMemory,
                                                   nullptr);
    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfUsbTargetPipeFormatRequestForWrite failed 0x%0x\n",
                 status);
#endif
        bytesWritten = 0;

        goto Done;
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
                                   nullptr);
    //
    // Send the request!
    //
    if (!WdfRequestSend(Request,
                        WdfUsbTargetPipeGetIoTarget(devContext->BulkOutPipe),
                        nullptr)) {

        //
        // Bad news. The target didn't get the request, so get the
        // failure status and complete the request ourselves..
        //
        status = WdfRequestGetStatus(Request);
#if DBG
        DbgPrint("WdfRequestSend failed 0x%0x\n",
                 status);
#endif
        bytesWritten = 0;

        goto Done;
    }

    goto DoneWithoutComplete;

Done:

    WdfRequestCompleteWithInformation(Request,
                                      status,
                                      bytesWritten);
DoneWithoutComplete:

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
BasicUsbEvtRequestWriteCompletionRoutine(WDFREQUEST                     Request,
                                         WDFIOTARGET                    Target,
                                         PWDF_REQUEST_COMPLETION_PARAMS Params,
                                         WDFCONTEXT                     Context)
{
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
BasicUsbEvtDeviceControl(WDFQUEUE   Queue,
                         WDFREQUEST Request,
                         size_t     OutputBufferLength,
                         size_t     InputBufferLength,
                         ULONG      IoControlCode)
{
    NTSTATUS                 status;
    PBASICUSB_DEVICE_CONTEXT devContext;
    ULONG_PTR                bytesReadOrWritten;

#if DBG
    DbgPrint("BasicUsbEvtDeviceControl\n");
#endif

    UNREFERENCED_PARAMETER(OutputBufferLength);

    devContext = BasicUsbGetContextFromDevice(WdfIoQueueGetDevice(Queue));

    switch (IoControlCode) {

        case IOCTL_OSR_BASICUSB_SET_BAR_GRAPH: {

            //
            // InputBufferLength - According to the data sheet the device
            // uses a 1 byte bitmask to determine which bars to set.  So,
            // the input buffer must be at least 1 byte long.
            //
            if (InputBufferLength < 1) {
#if DBG
                DbgPrint("No input buffer supplied in SET_BAR_GRAPH IOCTL\n");
#endif
                status             = STATUS_BUFFER_TOO_SMALL;
                bytesReadOrWritten = 0;

                goto Done;
            }

            //
            // Process the Request on the device
            //
            status = BasicUsbDoSetBarGraph(devContext,
                                           Request,
                                           &bytesReadOrWritten);

            goto Done;
        }

            //
            // Other IOCTL cases would be handled here
            //

        default: {
#if DBG
            DbgPrint("Unsupported IOCTL: 0x%x\n",
                     IoControlCode);
#endif
            status             = STATUS_INVALID_DEVICE_REQUEST;
            bytesReadOrWritten = 0;

        }

    }  // end of switch(IoControlCode)


Done:

    WdfRequestCompleteWithInformation(Request,
                                      status,
                                      bytesReadOrWritten);
}


///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbDoSetBarGraph
//
//      Process a received SET_BARGRAPH IOCTL Request by synchronously
//      sending it to the device.
//
//  INPUTS:
//
//      DevContext         - Pointer to our Device Context
//
//      Request            - A SET_BARGRAPH device control request
//
//  OUTPUTS:
//
//      BytesWritten       - Pointer into which to return the number of
//                           data bytes written.
//
//  RETURNS:
//
//      NTSTATUS indicating the overall result of the operation.
//
//  IRQL:
//
//      This routine is called at IRQL == PASSIVE_LEVEL, due to
//      our PASSIVE_LEVEL execution level contraint
//
///////////////////////////////////////////////////////////////////////////////
_Use_decl_annotations_

NTSTATUS
BasicUsbDoSetBarGraph(PBASICUSB_DEVICE_CONTEXT DevContext,
                      WDFREQUEST               Request,
                      PULONG_PTR               BytesWritten)
{
    NTSTATUS                     status;
    WDFMEMORY                    inputMemory;
    WDF_MEMORY_DESCRIPTOR        inputMemoryDescriptor;
    WDF_USB_CONTROL_SETUP_PACKET controlSetupPacket;
    ULONG_PTR                    byteCount;

    ASSERT(BytesWritten != nullptr);

    //
    // We need a descriptor of the Request's input buffer (that
    // contains the bitmask indicating which LED bars to illuminate).
    // This description is a WDFMEMORY object, that we will use to
    // format the Request to be sent to our WDFUSBDEVICE Target.
    //
    status = WdfRequestRetrieveInputMemory(Request,
                                           &inputMemory);

    if (!NT_SUCCESS(status)) {
#if DBG
        DbgPrint("WdfRequestRetrieveInputMemory failed 0x%0x\n",
                 status);
#endif
        byteCount = 0;

        goto Done;
    }

    //
    // The routine we want to call takes a WDF Memory Descriptor, so
    // initialize that now with the handle to the WDFMEMORY Object
    // describing the user input buffer.
    //
    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&inputMemoryDescriptor,
                                      inputMemory,
                                      nullptr);

    //
    // Initialize the Vendor Command (defined by the device) that
    // allows us to light particular segments of the bar graph LED.
    //
    WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
                                             BmRequestHostToDevice,
                                             BmRequestToDevice,
                                             USBFX2LK_SET_BARGRAPH_DISPLAY,
                                             0,
                                             0);


    //
    // Build a new Request for the Vendor Command, send it to our
    // WDFUSBDEVICE Target, and wait for it to complete.
    //
    // Note that we CAN wait because we established an IRQL PASSIVE_LEVEL
    // constraint on our Device-level Callbacks in EvtDriverDeviceAdd.
    //
    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

    status = WdfUsbTargetDeviceSendControlTransferSynchronously(DevContext->UsbDeviceTarget,
                                                                WDF_NO_HANDLE,
                                                                nullptr,
                                                                &controlSetupPacket,
                                                                &inputMemoryDescriptor,
                                                                nullptr);
    //
    // Conveniently, when SendControlTransferSynchronously works, it returns
    // the ultimate I/O status
    //
    if (!NT_SUCCESS(status)) {

        //
        // Bad news! The SET_BARGRAPH_DISPLAY operation failed.
        //
#if DBG
        DbgPrint("WdfUsbTargetDeviceSendControlTransferSynchronously "
                 "failed 0x%0x\n",
                 status);
#endif
        byteCount = 0;

        goto Done;
    }

    status    = STATUS_SUCCESS;
    byteCount = sizeof(UCHAR);

Done:

    *BytesWritten = byteCount;

    return status;
}
