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

#include <Automation/ScriptReporter.h>
#include <AzCore/Utils/Utils.h>
#include <OpenImageIO/imageio.h>

namespace AtomSampleViewer
{
    bool ScriptReporter::LoadPngData(ImageComparisonResult& imageComparisonResult, const AZStd::string& path, AZStd::vector<uint8_t>& buffer, AZ::RHI::Size& size, AZ::RHI::Format& format, TraceLevel traceLevel)
    {
        using namespace OIIO;

        const size_t maxFileSize = 1024 * 1024 * 25;

        auto readScreenshotFileResult = AZ::Utils::ReadFile<AZStd::vector<uint8_t>>(path, maxFileSize);
        if (!readScreenshotFileResult.IsSuccess())
        {
            ReportScriptIssue(AZStd::string::format("Screenshot check failed. %s", readScreenshotFileResult.GetError().c_str()), traceLevel);
            imageComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::FileNotFound;
            return false;
        }

        auto in = ImageInput::open(path.c_str());
        if (in)
        {
            const ImageSpec& spec = in->spec();
            size.m_width = spec.width;
            size.m_height = spec.height;
            size.m_depth = spec.depth;
            format = AZ::RHI::Format::R8G8B8A8_UNORM;
            buffer.resize(spec.width * spec.height * spec.nchannels);
            if (!in->read_image(TypeDesc::UINT8, &buffer[0]))
            {
                ReportScriptIssue(AZStd::string::format("Screenshot check failed. Failed to read file '%s'", path.c_str()), traceLevel);
                imageComparisonResult.m_resultCode = ImageComparisonResult::ResultCode::FileNotLoaded;
                return false;
            } 
            in->close();
        }

        return true;
    }
}
