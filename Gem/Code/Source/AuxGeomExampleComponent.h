/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>

namespace AtomSampleViewer
{
    class AuxGeomExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(AuxGeomExampleComponent, "{C7AC0D17-84D9-42A2-9EBA-2358C0F13074}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        AuxGeomExampleComponent();
        ~AuxGeomExampleComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private: // functions

        void LoadConfigFiles();

        // Functions for each display option (currently there is only one
        void DrawSampleOfAllAuxGeom() const;

        // Functions used by DrawSampleOfAllAuxGeom

    private: // data
                
        ImGuiSidebar m_imguiSidebar;

        // draw options
        bool m_drawBackgroundBox = true;
        bool m_drawThreeGridsOfPoints = true;
        bool m_drawAxisLines = true;
        bool m_drawLines = true;
        bool m_drawTriangles = true;
        bool m_drawShapes = true;
        bool m_drawBoxes = true;
        bool m_drawManyPrimitives = true;
        bool m_drawDepthTestPrimitives = true;
        bool m_draw2DWireRect = true;
    };
} // namespace AtomSampleViewer
