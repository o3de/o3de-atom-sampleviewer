/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Automation/ScriptReporter.h>
#include <AzCore/Utils/Utils.h>
#include <Atom/Utils/PngFile.h>

namespace AtomSampleViewer
{
    bool ScriptReporter::LoadPngData(ImageComparisonResult& imageComparisonResult, const AZStd::string& path, AZStd::vector<uint8_t>& buffer, AZ::RHI::Size& size, AZ::RHI::Format& format, TraceLevel traceLevel)
    {
        using namespace AZ;
        using namespace AZ::Utils;

        const auto errorHandler = [path, traceLevel, &imageComparisonResult](const char* errorMessage)
        {
            ReportScriptIssue(AZStd::string::format("Failed to load PNG file '%s' with error '%s'\n", path.c_str(), errorMessage),
            traceLevel);

            imageComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::FileNotLoaded;
        };

        PngFile::LoadSettings loadSettings;
        loadSettings.m_stripAlpha = false;
        loadSettings.m_errorHandler = errorHandler;

        PngFile file = PngFile::Load(path.c_str(), loadSettings);
        if (file.IsValid())
        {
            size = RHI::Size(file.GetWidth(), file.GetHeight(), 1);
            format = AZ::RHI::Format::R8G8B8A8_UNORM;
            buffer = file.TakeBuffer();
            return true;
        }
        else
        {
            return false;                
        }
    }
}
