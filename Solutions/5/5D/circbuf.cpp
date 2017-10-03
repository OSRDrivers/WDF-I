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
//  BasicUsbInitializeCircBuf
//
//    Initializes a basic circular buffer structure
//
//  INPUTS:
//
//      CircBuf - The circular buffer to initialize
//
//  OUTPUTS:
//
//      None.
//
//  RETURNS:
//
//      STATUS_SUCCESS if successful, an appropriate error
//      otherwise
//
//  IRQL:
//
//      This routine can be called at IRQL <= DISPATCH_LEVEL
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
NTSTATUS
BasicUsbInitializeCircBuf(
    PBASIC_USB_CIRC_BUF CircBuf
    ) {

    NTSTATUS status;

    //
    // By default the parent of the spinlock is the driver, this
    // will automatically be cleaned up for us when the driver
    // unloads.
    //
    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               &CircBuf->Lock);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    CircBuf->HeadIndex = 0;
    CircBuf->TailIndex = 0;
    CircBuf->ValidDataSize = 0;
    return STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbAddBulkInData
//
//    Adds data to a basic circular buffer
//
//  INPUTS:
//
//      CircBuf - The circular buffer to add data to
//
//      Data    - The data to add
//
//      Length  - The length of the data
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
//      This routine can be called at IRQL <= DISPATCH_LEVEL
//
//  NOTES:
//      
//      Suppress spurious warning about not releasing spinlock, we do!
//
///////////////////////////////////////////////////////////////////////////////
VOID
#pragma warning(suppress: 28167)
BasicUsbAddBulkInData(
    PBASIC_USB_CIRC_BUF CircBuf,
    PUCHAR Data,
    ULONG Length
    ) {

    ASSERT(Length <= CIRC_BUF_SIZE);

    WdfSpinLockAcquire(CircBuf->Lock);

    //
    // Is this going to wrap?
    //
    if ((CircBuf->TailIndex + Length) > CIRC_BUF_SIZE) {
        //
        // Yup...Copy as much as we can up to the end.
        //
        RtlCopyMemory(&CircBuf->Data[CircBuf->TailIndex],
                      Data,
                      CIRC_BUF_SIZE - CircBuf->TailIndex);

        //
        // Now copy the rest into the start of the circular buffer
        //
        RtlCopyMemory(&CircBuf->Data[0],
                      &Data[CIRC_BUF_SIZE - CircBuf->TailIndex],
                      Length - (CIRC_BUF_SIZE - CircBuf->TailIndex));


        CircBuf->TailIndex = (Length - (CIRC_BUF_SIZE - CircBuf->TailIndex));
        
    } else {

        RtlCopyMemory(&CircBuf->Data[CircBuf->TailIndex],
                      Data,
                      Length);

        CircBuf->TailIndex += Length;

    }

    if (CircBuf->ValidDataSize != CIRC_BUF_SIZE) {
        CircBuf->ValidDataSize += Length;
    }
     
    WdfSpinLockRelease(CircBuf->Lock);
}

///////////////////////////////////////////////////////////////////////////////
//
//  BasicUsbRemoveBulkInData
//
//    Removes data from a basic circular buffer
//
//  INPUTS:
//
//      CircBuf - The circular buffer to remove data from
//
//      Data    - The data buffer to copy into
//
//      Length  - The length of the data
//
//  OUTPUTS:
//
//      ReturnedLength - Actual amount of data copied
//
//  RETURNS:
//
//      None.
//
//  IRQL:
//
//      This routine can be called at IRQL <= DISPATCH_LEVEL
//
//  NOTES:
//
//
///////////////////////////////////////////////////////////////////////////////
VOID
BasicUsbRemoveBulkInData(
    PBASIC_USB_CIRC_BUF CircBuf,
    PUCHAR Data,
    ULONG Length,
    PULONG ReturnedLength
    ) {

    ULONG copyAmount;

    WdfSpinLockAcquire(CircBuf->Lock);

    if (CircBuf->ValidDataSize == 0) {
        //
        // No data!
        //
        WdfSpinLockRelease(CircBuf->Lock);
        *ReturnedLength = 0;
        return;

    }

    if (Length > CircBuf->ValidDataSize) {
        copyAmount = CircBuf->ValidDataSize;
    } else {
        copyAmount = Length;
    }

    //
    // Is this going to wrap?
    //
    if ((CircBuf->HeadIndex + copyAmount) > CIRC_BUF_SIZE) {

        //
        // Yup...Copy as much as we can up to the end.
        //
        RtlCopyMemory(Data,
                      &CircBuf->Data[CircBuf->HeadIndex],
                      CIRC_BUF_SIZE - CircBuf->HeadIndex);

        //
        // Now copy the rest from the start of the circular buffer
        //
        RtlCopyMemory(&Data[CIRC_BUF_SIZE - CircBuf->HeadIndex],
                      &CircBuf->Data[0],
                      copyAmount - (CIRC_BUF_SIZE - CircBuf->TailIndex));


        CircBuf->HeadIndex = (copyAmount - 
                                      (CIRC_BUF_SIZE - CircBuf->HeadIndex));

    } else {

        RtlCopyMemory(Data,
                      &CircBuf->Data[CircBuf->HeadIndex],
                      copyAmount);

        CircBuf->HeadIndex += copyAmount;

    }

    CircBuf->ValidDataSize -= copyAmount;

    WdfSpinLockRelease(CircBuf->Lock);

    *ReturnedLength = copyAmount;

    return;
}



