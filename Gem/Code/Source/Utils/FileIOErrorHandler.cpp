/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
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
