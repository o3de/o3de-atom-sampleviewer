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

#pragma once

#include <AzFramework/Windowing/WindowBus.h>

struct ImGuiContext;

namespace AtomSampleViewer
{
    //! Shows a simple message or confirmation dialog.
    class ImGuiMessageBox
    {
    public:

        void OpenPopupMessage(AZStd::string title, AZStd::string message);
        void OpenPopupConfirmation(AZStd::string title, AZStd::string message, AZStd::function<void()> okAction, AZStd::string okButton = "OK", AZStd::string cancelButton = "Cancel");

        void TickPopup();

    private:
        enum class Type
        {
            Ok,
            OkCancel
        } m_type;

        enum class State
        {
            Closed,
            Opening,
            Open
        };

        AZStd::string m_title;
        AZStd::string m_message;
        AZStd::string m_okButtonLabel;
        AZStd::string m_cancelButtonLabel;
        AZStd::function<void()> m_okAction;
        State m_state = State::Closed;
    };

} // namespace AtomSampleViewer
