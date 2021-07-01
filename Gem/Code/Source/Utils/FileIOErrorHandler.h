/*
 * Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/IO/FileIOEventBus.h>
#include <AzCore/std/string/string.h>

namespace AtomSampleViewer
{
    //! Use this to report AZ::IO errors and include the OS error code in the message.
    //! The general pattern is to call:
    //!     FileIOErrorHandler::BusConnect()
    //!     if(!someAzIoOperation())
    //!         FileIOErrorHandler::ReportLatestIOError(myMessage)
    //!     FileIOErrorHandler::BusDisconnect()
    class FileIOErrorHandler
        : public AZ::IO::FileIOEventBus::Handler
    {
        using Base = AZ::IO::FileIOEventBus::Handler;
    public:

        void BusConnect(); //!< Also resets m_ioErrorCode
        void ReportLatestIOError(AZStd::string message);
        void BusDisconnect(); //!< Also resets m_ioErrorCode

    private:

        // AZ::IO::FileIOEventBus::Handler overrides...
        void OnError(const AZ::IO::SystemFile* file, const char* fileName, int errorCode) override;

        static constexpr int ErrorCodeNotSet = -1;
        int m_ioErrorCode = ErrorCodeNotSet;
    };
} // namespace AtomSampleViewer
