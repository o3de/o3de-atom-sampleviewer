/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>

#include <AzFramework/Entity/EntityContext.h>

#include <Atom/RPI.Public/WindowContext.h>

namespace AtomSampleViewer
{
    static constexpr const char DefaultPbrMaterialPath[] = "materials/defaultpbr.azmaterial";    
    static constexpr const char BunnyModelFilePath[] = "objects/bunny.azmodel";
    static constexpr const char ShaderBallModelFilePath[] = "objects/shaderball_simple.azmodel";
    static constexpr const char CubeModelFilePath[] = "testdata/objects/cube/cube.azmodel";

    class SampleComponentConfig
        : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(SampleComponentConfig, "{FBC7F31B-1E43-4A0F-9280-D1B62D81E83B}", AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(SampleComponentConfig, AZ::SystemAllocator, 0);

        static void Reflect(AZ::ReflectContext* context);

        SampleComponentConfig() {}

        explicit SampleComponentConfig(AZStd::shared_ptr<AZ::RPI::WindowContext> windowContext, AZ::EntityId cameraEntityId, AzFramework::EntityContextId entityContextId)
            : m_windowContext(AZStd::move(windowContext))
            , m_cameraEntityId(cameraEntityId)
            , m_entityContextId(entityContextId)
        {}

        bool IsValid() const;

        AZStd::shared_ptr<AZ::RPI::WindowContext> m_windowContext;
        AZ::EntityId m_cameraEntityId;
        AzFramework::EntityContextId m_entityContextId;
    };
} // namespace AZ
