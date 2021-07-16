/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <MultiViewSingleSceneAuxGeomExampleComponent.h>
#include <AuxGeomSharedDrawFunctions.h>

#include <Atom/Component/DebugCamera/CameraComponent.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>

#include <Atom/Feature/PostProcessing/PostProcessingConstants.h>

#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>
#include <Atom/RPI.Public/AuxGeom/AuxGeomFeatureProcessorInterface.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>

#include <AzCore/Math/MatrixUtils.h>

#include <AzCore/Component/Entity.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Scene/SceneSystemInterface.h>

#include <SampleComponentConfig.h>
#include <SampleComponentManager.h>
#include <EntityUtilityFunctions.h>

#include <RHI/BasicRHIComponent.h>
#include <AtomSampleViewerOptions.h>

namespace AtomSampleViewer
{
    //////////////////////////////////////////////////////////////////////////
    // WindowedView

    //! A simple example for how to set up a second window, view, renderPipeline, and basic entities.
    class WindowedView final
        : public AzFramework::WindowNotificationBus::Handler
    {
        friend MultiViewSingleSceneAuxGeomExampleComponent;
    protected:
        AZStd::unique_ptr<AzFramework::NativeWindow> m_nativeWindow;
        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZ::RPI::RenderPipelinePtr m_pipeline;
        AZ::Entity* m_cameraEntity = nullptr;
        AZ::RPI::ViewPtr m_view;
        double m_simulateTime = 0.0f;
        float m_deltaTime = 0.0f;
        MultiViewSingleSceneAuxGeomExampleComponent* m_parent;

    public:
        WindowedView(AzFramework::EntityContextId contextId, MultiViewSingleSceneAuxGeomExampleComponent *parent)
        : m_parent(parent)
        {
            m_parent = parent;

            // Create a NativeWindow and WindowContext
            m_nativeWindow = AZStd::make_unique<AzFramework::NativeWindow>("Multi View Single Scene: Second Window", AzFramework::WindowGeometry(0, 0, 640, 480));
            m_nativeWindow->Activate();
            AZ::RHI::Ptr<AZ::RHI::Device> device = AZ::RHI::RHISystemInterface::Get()->GetDevice();
            m_windowContext = AZStd::make_shared<AZ::RPI::WindowContext>();
            m_windowContext->Initialize(*device, m_nativeWindow->GetWindowHandle());

            auto scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();

            // Create a custom pipeline descriptor
            AZ::RPI::RenderPipelineDescriptor pipelineDesc;
            pipelineDesc.m_mainViewTagName = "MainCamera";          //Surface shaders render to the "MainCamera" tag
            pipelineDesc.m_name = "SecondPipeline";                 //Sets the debug name for this pipeline
            pipelineDesc.m_rootPassTemplate = "MainPipeline";    //References a template in AtomSampleViewer\Passes\PassTemplates.azasset
            pipelineDesc.m_renderSettings.m_multisampleState.m_samples = 4;
            m_pipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *m_windowContext);

            scene->AddRenderPipeline(m_pipeline);

            // Create a camera entity, hook it up to the RenderPipeline
            m_cameraEntity = CreateEntity("WindowedViewCamera", contextId);
            AZ::Debug::CameraComponentConfig cameraConfig(m_windowContext);
            cameraConfig.m_fovY = AZ::Constants::QuarterPi;
            AZ::Debug::CameraComponent* camComponent = static_cast<AZ::Debug::CameraComponent*>(m_cameraEntity->CreateComponent(azrtti_typeid<AZ::Debug::CameraComponent>()));
            camComponent->SetConfiguration(cameraConfig);
            m_cameraEntity->CreateComponent(azrtti_typeid<AzFramework::TransformComponent>());
            m_cameraEntity->CreateComponent(azrtti_typeid<AZ::Debug::NoClipControllerComponent>());
            m_cameraEntity->Activate();
            m_pipeline->SetDefaultViewFromEntity(m_cameraEntity->GetId());
            m_view = camComponent->GetView();

            AzFramework::WindowNotificationBus::Handler::BusConnect(m_nativeWindow->GetWindowHandle());
        }

        ~WindowedView()
        {
            AzFramework::WindowNotificationBus::Handler::BusDisconnect(m_nativeWindow->GetWindowHandle());

            DestroyEntity(m_cameraEntity);

            m_pipeline->RemoveFromScene();
            m_pipeline = nullptr;

            m_windowContext->Shutdown();
            m_windowContext = nullptr;
        }

        // AzFramework::WindowNotificationBus::Handler overrides ...
        void OnWindowClosed() override
        {
            m_parent->OnChildWindowClosed();
        }

        AzFramework::NativeWindowHandle GetNativeWindowHandle()
        {
            if (m_nativeWindow)
            {
                return m_nativeWindow->GetWindowHandle();
            }
            else
            {
                return nullptr;
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // MultiViewSingleSceneAuxGeomExampleComponent

    void MultiViewSingleSceneAuxGeomExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MultiViewSingleSceneAuxGeomExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }    

    MultiViewSingleSceneAuxGeomExampleComponent::MultiViewSingleSceneAuxGeomExampleComponent()
    {
    }

    void MultiViewSingleSceneAuxGeomExampleComponent::Activate()
    {
        // Setup primary camera controls
        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<AZ::Debug::NoClipControllerComponent>());

        OpenSecondSceneWindow();

        AZ::TickBus::Handler::BusConnect();
    }

    void MultiViewSingleSceneAuxGeomExampleComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();

        AZ::Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &AZ::Debug::CameraControllerRequestBus::Events::Disable);

        if(m_windowedView)
        {
            m_windowedView = nullptr;
        }
    }

    void MultiViewSingleSceneAuxGeomExampleComponent::OnChildWindowClosed()
    {
        m_windowedView = nullptr;
    }

    void MultiViewSingleSceneAuxGeomExampleComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint timePoint)
    {
        DrawAuxGeom();

        if (SupportsMultipleWindows() && ImGui::Begin("Multi View Panel"))
        {
            if(m_windowedView)
            {
                if (ImGui::Button("Close Second View Window"))
                {
                    m_windowedView = nullptr;
                }
            }
            else
            {
                if (ImGui::Button("Open Second View Window"))
                {
                    OpenSecondSceneWindow();
                }
            }
            ImGui::End();
        }

        if (m_windowedView)
        {
            // duplicate first camera changes to the 2nd camera
            AZ::Transform mainCameraTransform;
            AZ::TransformBus::EventResult(mainCameraTransform, GetCameraEntityId(), &AZ::TransformBus::Events::GetWorldTM);
            Camera::CameraComponentRequests* cameraInterface = Camera::CameraRequestBus::FindFirstHandler(GetCameraEntityId());
            float fovRadians = cameraInterface->GetFovRadians();
            float nearClipDistance = cameraInterface->GetNearClipDistance();
            float farClipDistance = cameraInterface->GetFarClipDistance();

            AZ::EntityId secondCameraEntityId = m_windowedView->m_cameraEntity->GetId();
            AZ::TransformBus::Event(secondCameraEntityId, &AZ::TransformBus::Events::SetWorldTM, mainCameraTransform);
            Camera::CameraComponentRequests* secondCamInterface = Camera::CameraRequestBus::FindFirstHandler(secondCameraEntityId);
            secondCamInterface->SetFovRadians(fovRadians);
            secondCamInterface->SetNearClipDistance(nearClipDistance);
            secondCamInterface->SetFarClipDistance(farClipDistance);
        }
    }

    void MultiViewSingleSceneAuxGeomExampleComponent::OpenSecondSceneWindow()
    {
        if (SupportsMultipleWindows() && !m_windowedView)
        {
            m_windowedView = AZStd::make_unique<WindowedView>(GetEntityContextId(), this);
        }
    }

    void MultiViewSingleSceneAuxGeomExampleComponent::DrawAuxGeom() const
    {
        auto scene = AZ::RPI::RPISystemInterface::Get()->GetDefaultScene();
        auto auxGeomFP = scene->GetFeatureProcessor<AZ::RPI::AuxGeomFeatureProcessorInterface>();
        if (auto auxGeom = auxGeomFP->GetDrawQueue())
        {
            DrawBackgroundBox(auxGeom);

            DrawThreeGridsOfPoints(auxGeom);

            DrawAxisLines(auxGeom);

            DrawLines(auxGeom);

            DrawBoxes(auxGeom, -20.0f);

            Draw2DWireRect(auxGeom, AZ::Colors::Red, 1.0f);
        }

        if (m_windowedView)
        {
            if (auto auxGeom = auxGeomFP->GetDrawQueueForView(m_windowedView->m_view.get()))
            {
                DrawTriangles(auxGeom);

                DrawShapes(auxGeom);

                DrawBoxes(auxGeom, 10.0f);

                DrawDepthTestPrimitives(auxGeom);

                Draw2DWireRect(auxGeom, AZ::Colors::Yellow, 0.9f);
            }
        }
    }

    

} // namespace AtomSampleViewer
