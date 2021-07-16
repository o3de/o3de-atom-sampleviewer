/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Utils/FileIOErrorHandler.h>

namespace AtomSampleViewer
{
    void FileIOErrorHandler::OnError([[maybe_unused]] const AZ::IO::SystemFile* file, [[maybe_unused]] const char* fileName, int errorCode)
    {
        m_ioErrorCode = errorCode;
    }

    void FileIOErrorHandler::BusConnect()
    {
        Base::BusConnect();
        m_ioErrorCode = ErrorCodeNotSet;
    }

    void FileIOErrorHandler::ReportLatestIOError(AZStd::string message)
    {
        AZ_Assert(BusIsConnected(), "FileIOErrorHandler must be connected while calling ReportLatestIOError");

        if (m_ioErrorCode != ErrorCodeNotSet)
        {
            message += AZStd::string::format(" IO error code %d.", m_ioErrorCode);
        }

        AZ_Error("FileIOErrorHandler", false, "%s", message.c_str());
    }

    void FileIOErrorHandler::BusDisconnect()
    {
        Base::BusDisconnect();
        m_ioErrorCode = ErrorCodeNotSet;
    }

} // namespace AtomSampleViewer
