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
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES objAtttributes;
    WDFDEVICE             wdfDevice;
    PFILTER_DEVICE_CONTEXT filterContext;

    UNREFERENCED_PARAMETER(Driver);

#if DBG
    DbgPrint("CDFilter: Adding device...\n");
#endif

    //
    // Indicate that we're creating a FILTER device.  This will cause
    // KMDF to attach us correctly to the device stack, to automagically
    // set us to NOT be the Power Policy Owner and to auto-forward any
    // requests we don't explicitly handle down to our Local I/O Target.
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

    status = STATUS_SUCCESS;

Done:

    return status;
}
