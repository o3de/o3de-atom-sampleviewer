/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Component/EntityBus.h>

#include <Atom/RPI.Reflect/Shader/ShaderAsset.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

#include <CrySystemBus.h>

namespace AtomSampleViewer
{
    using HighResTimer = AZStd::chrono::high_resolution_clock;

    class AtomSampleViewerSystemComponent final
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public AZ::EntitySystemBus::Handler
        , protected CrySystemEventBus::Handler
    {
    public:
        AZ_COMPONENT(AtomSampleViewerSystemComponent, "{714873AD-70FC-47A8-A609-46103CB8DABA}");

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        AtomSampleViewerSystemComponent();
        ~AtomSampleViewerSystemComponent() override;

        void Activate() override;
        void Deactivate() override;

        // AZ::EntitySystemBus::Handler
        void OnEntityDestroyed(const AZ::EntityId& entityId) override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // CrySystemEventBus::Handler
        void OnCrySystemInitialized(ISystem& system, const SSystemInitParams&) override;

    private:
        void TrackPerfMetrics(const float deltaTime);
        void LogPerfMetrics() const;

        struct PerfMetrics
        {
            AZ_CLASS_ALLOCATOR(PerfMetrics, AZ::SystemAllocator, 0);
            AZ_TYPE_INFO(PerfMetrics, "{D333FB60-DC06-42CF-9124-B1EF90690A16}");

            static void Reflect(AZ::ReflectContext* context);

            virtual ~PerfMetrics() = default;

            float m_averageDeltaSeconds = 0.0f;
            float m_timeToFirstRenderSeconds = 0.0f;
            float m_timingTargetSeconds = 10.0f;
        };

        AZStd::chrono::time_point<HighResTimer> m_timestamp;
        float m_accumulatedDeltaSeconds = 0.0f;
        uint32_t m_frameCount = 0;
        bool m_testsLogged = false;

        PerfMetrics m_perfMetrics;
        AZ::Entity* m_atomSampleViewerEntity = nullptr;
        AZStd::vector<AZ::Name> m_passesToRemove;

        void ReadTimeoutShutdown();
        void TickTimeoutShutdown(float deltaTimeInSeconds);
        float m_secondsBeforeShutdown = 0.f; // >0.f If timeout shutdown is enabled, this will count down the time until quit() is called.
    };
} // namespace AtomSampleViewer
