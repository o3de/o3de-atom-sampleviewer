/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <RayTracingIntersectionShaderExampleComponent.h>

#include <Atom/Bootstrap/BootstrapNotificationBus.h>
#include <Atom/Component/DebugCamera/ArcBallControllerComponent.h>
#include <Atom/Feature/SpecularReflections/SpecularReflectionsFeatureProcessorInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <AzCore/Math/Random.h>
#include <AzFramework/Components/TransformComponent.h>
#include <DebugDraw/DebugDrawBus.h>
#include <EntityUtilityFunctions.h>

namespace AtomSampleViewer
{
    using namespace AZ;

    namespace
    {
        struct UniformRandomFloat
        {
            SimpleLcgRandom m_rng;
            float m_min;
            float m_max;

            UniformRandomFloat(SimpleLcgRandom& rng, float min, float max)
                : m_rng(rng)
                , m_min(min)
                , m_max(max)
            {
            }

            float operator()()
            {
                return m_rng.GetRandomFloat() * (m_max - m_min) + m_min;
            }
        };
    } // namespace

    static const char* PlaneMeshPath{ "objects/plane.fbx.azmodel" };
    static const char* MirrorMaterialPath{ "materials/SSRExample/GroundPlaneMirror.azmaterial" };

    static const float DefaultCameraHeadingDegrees{ 93.f };
    static const float DefaultCameraPitchDegrees{ -30.f };
    static const float DefaultCameraDistance{ 10.0f };
    static const Vector3 DefaultCameraPan{ 0.8f, -0.4f, -1.f };

    void RayTracingIntersectionShaderExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext * serializeContext{ azrtti_cast<AZ::SerializeContext*>(context) })
        {
            serializeContext->Class<RayTracingIntersectionShaderExampleComponent, AZ::Component>()->Version(0);
        }
    }

    void RayTracingIntersectionShaderExampleComponent::Activate()
    {
        m_mirrorplaneModelAsset =
            RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(PlaneMeshPath, RPI::AssetUtils::TraceLevel::Assert);
        m_mirrorMaterialAsset =
            RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(MirrorMaterialPath, RPI::AssetUtils::TraceLevel::Assert);
        m_mirrorMaterialInstance = RPI::Material::Create(m_mirrorMaterialAsset);
        m_mirrorplaneMeshHandle =
            GetMeshFeatureProcessor()->AcquireMesh(Render::MeshHandleDescriptor(m_mirrorplaneModelAsset, m_mirrorMaterialInstance));
        GetMeshFeatureProcessor()->SetTransform(
            m_mirrorplaneMeshHandle, Transform{ Vector3{ 0, 0, -1 }, Quaternion::CreateIdentity(), 14 });

        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Enable, azrtti_typeid<Debug::ArcBallControllerComponent>());
        using ArcBallBus = Debug::ArcBallControllerRequestBus;
        ArcBallBus::Event(GetCameraEntityId(), &ArcBallBus::Events::SetDistance, DefaultCameraDistance);
        ArcBallBus::Event(GetCameraEntityId(), &ArcBallBus::Events::SetHeading, DegToRad(DefaultCameraHeadingDegrees));
        ArcBallBus::Event(GetCameraEntityId(), &ArcBallBus::Events::SetPitch, DegToRad(DefaultCameraPitchDegrees));
        ArcBallBus::Event(GetCameraEntityId(), &ArcBallBus::Events::SetPan, DefaultCameraPan);

        InitLightingPresets(true);

        SimpleLcgRandom rng;
        UniformRandomFloat sphereRadiusRng{ rng, 0.2f, 0.8f };
        UniformRandomFloat obbSizeRng{ rng, 0.2f, 0.6f };
        UniformRandomFloat obbRotationRng{ rng, 0.f, Constants::TwoPi };

        for (int i{ 0 }; i < 100; i++)
        {
            float x{ aznumeric_cast<float>(i % 10) - 5 };
            float y{ aznumeric_cast<float>(i / 10) - 5 };

            Entity* entity{ CreateEntity(AZStd::string::format("Procedural mesh %i", i), GetEntityContextId()) };

            Component* transformComponent{ nullptr };
            ComponentDescriptorBus::EventResult(
                transformComponent, azrtti_typeid<AzFramework::TransformComponent>(), &ComponentDescriptorBus::Events::CreateComponent);
            azrtti_cast<AzFramework::TransformComponent*>(transformComponent)->SetLocalTM(Transform::CreateTranslation(Vector3{ x, y, 0 }));
            entity->AddComponent(transformComponent);

            Color color{ rng.GetRandomFloat(), rng.GetRandomFloat(), rng.GetRandomFloat(), 1.f };

            bool drawSphere{ rng.GetRandomFloat() < 0.5f };
            if (drawSphere)
            {
                float sphereRadius{ sphereRadiusRng() };
                DebugDraw::DebugDrawRequestBus::Broadcast(
                    &DebugDraw::DebugDrawRequests::DrawSphereOnEntity, entity->GetId(), sphereRadius, color, true, -1.f);
            }
            else // Draw obb
            {
                Obb obb{ Obb::CreateFromPositionRotationAndHalfLengths(
                    Vector3{ x, y, 0 }, Quaternion::CreateRotationX(obbRotationRng()) * Quaternion::CreateRotationY(obbRotationRng()),
                    AZ::Vector3{ obbSizeRng(), obbSizeRng(), obbSizeRng() }) };
                DebugDraw::DebugDrawRequestBus::Broadcast(
                    &DebugDraw::DebugDrawRequests::DrawObbOnEntity, entity->GetId(), obb, color, true, -1.f);
            }

            DebugDraw::DebugDrawInternalRequestBus::Broadcast(
                &DebugDraw::DebugDrawInternalRequestBus::Events::RegisterDebugDrawComponent, transformComponent);

            entity->Activate();
            m_entities.push_back(entity);
        }

        Render::SpecularReflectionsFeatureProcessorInterface* specularReflectionsFeatureProcessor{
            m_scene->GetFeatureProcessorForEntityContextId<Render::SpecularReflectionsFeatureProcessorInterface>(GetEntityContextId())
        };
        AZ_Assert(specularReflectionsFeatureProcessor, "SpecularReflectionsFeatureProcessor not available.");

        Render::SSROptions ssrOptions;
        ssrOptions.m_enable = true;
        ssrOptions.m_reflectionMethod = Render::SSROptions::ReflectionMethod::RayTracing;
        ssrOptions.m_temporalFiltering = false;
        specularReflectionsFeatureProcessor->SetSSROptions(ssrOptions);

        Render::Bootstrap::NotificationBus::Broadcast(&Render::Bootstrap::NotificationBus::Handler::OnBootstrapSceneReady, m_scene);
    };

    void RayTracingIntersectionShaderExampleComponent::Deactivate()
    {
        GetMeshFeatureProcessor()->ReleaseMesh(m_mirrorplaneMeshHandle);

        for (Entity* entity : m_entities)
        {
            DestroyEntity(entity);
        }

        Debug::CameraControllerRequestBus::Event(GetCameraEntityId(), &Debug::CameraControllerRequestBus::Events::Disable);
    }
} // namespace AtomSampleViewer
