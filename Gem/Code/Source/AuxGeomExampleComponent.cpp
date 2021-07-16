/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AuxGeomExampleComponent.h>

#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/RPI.Public/AuxGeom/AuxGeomFeatureProcessorInterface.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Matrix3x3.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Components/CameraBus.h>

#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>

#include <AuxGeomSharedDrawFunctions.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    void AuxGeomExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AuxGeomExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    AuxGeomExampleComponent::AuxGeomExampleComponent()
    {
    }

    void AuxGeomExampleComponent::LoadConfigFiles()
    {
    }

    void AuxGeomExampleComponent::Activate()
    {
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        Camera::CameraRequestBus::Event(GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            200.f);

        m_imguiSidebar.Activate();

        AZ::TickBus::Handler::BusConnect();
    }

    void AuxGeomExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);
    }
    
    void AuxGeomExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_imguiSidebar.Begin())
        {
            ImGui::Text("Draw Options");

            if (ScriptableImGui::Button("Select All"))
            {
                m_drawBackgroundBox = true;
                m_drawThreeGridsOfPoints = true;
                m_drawAxisLines = true;
                m_drawLines = true;
                m_drawTriangles = true;
                m_drawShapes = true;
                m_drawBoxes = true;
                m_drawManyPrimitives = true;
                m_drawDepthTestPrimitives = true;
                m_draw2DWireRect = true;
            }

            if (ScriptableImGui::Button("Unselect All"))
            {
                m_drawBackgroundBox = false;
                m_drawThreeGridsOfPoints = false;
                m_drawAxisLines = false;
                m_drawLines = false;
                m_drawTriangles = false;
                m_drawShapes = false;
                m_drawBoxes = false;
                m_drawManyPrimitives = false;
                m_drawDepthTestPrimitives = false;
                m_draw2DWireRect = false;
            }
            ImGui::Indent();

            ScriptableImGui::Checkbox("Draw background box", &m_drawBackgroundBox);
            ScriptableImGui::Checkbox("Draw three grids of points", &m_drawThreeGridsOfPoints);
            ScriptableImGui::Checkbox("Draw axis lines", &m_drawAxisLines);
            ScriptableImGui::Checkbox("Draw lines", &m_drawLines);
            ScriptableImGui::Checkbox("Draw triangles", &m_drawTriangles);
            ScriptableImGui::Checkbox("Draw shapes", &m_drawShapes);
            ScriptableImGui::Checkbox("Draw boxes", &m_drawBoxes);
            ScriptableImGui::Checkbox("Draw many primitives", &m_drawManyPrimitives);
            ScriptableImGui::Checkbox("Draw depth test primitives", &m_drawDepthTestPrimitives);
            ScriptableImGui::Checkbox("Draw 2d wire rect", &m_draw2DWireRect);

            ImGui::Unindent();

            m_imguiSidebar.End();
        }


        // Currently this does this one thing, the intention is that this sample has a sidebar to allow selection of different
        // examples/tests
        DrawSampleOfAllAuxGeom();
    }

    void AuxGeomExampleComponent::DrawSampleOfAllAuxGeom() const
    {
        auto defaultScene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        if (auto auxGeom = AZ::RPI::AuxGeomFeatureProcessorInterface::GetDrawQueueForScene(defaultScene))
        {
            if (m_drawBackgroundBox)
            {
                DrawBackgroundBox(auxGeom);
            }

            if (m_drawThreeGridsOfPoints)
            {
                DrawThreeGridsOfPoints(auxGeom);
            }

            if (m_drawAxisLines)
            {
                DrawAxisLines(auxGeom);
            }

            if (m_drawLines)
            {
                DrawLines(auxGeom);
            }

            if (m_drawTriangles)
            {
                DrawTriangles(auxGeom);
            }

            if (m_drawShapes)
            {
                DrawShapes(auxGeom);
            }

            if (m_drawBoxes)
            {
                DrawBoxes(auxGeom);
            }

            if (m_drawManyPrimitives)
            {
                DrawManyPrimitives(auxGeom);
            }

            if (m_drawDepthTestPrimitives)
            {
                DrawDepthTestPrimitives(auxGeom);
            }

            if (m_draw2DWireRect)
            {
                Draw2DWireRect(auxGeom, AZ::Colors::Red, 1.0f);
            }
        }
    }

} // namespace AtomSampleViewer
