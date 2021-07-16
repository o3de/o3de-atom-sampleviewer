/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <EntityUtilityFunctions.h>
#include <AzFramework/Entity/EntityContextBus.h>

namespace AtomSampleViewer
{
    AZ::Entity* CreateEntity(const AZStd::string_view& name, AzFramework::EntityContextId entityContextId)
    {
        AZ::Entity* entity = nullptr;
        AzFramework::EntityContextRequestBus::EventResult(entity, entityContextId, &AzFramework::EntityContextRequestBus::Events::CreateEntity, name.data());
        AZ_Assert(entity != nullptr, "Failed to create \"%s\" entity.", name.data());
        return entity;
    }

    void DestroyEntity(AZ::Entity*& entity, AzFramework::EntityContextId entityContextId)
    {
        if (entity != nullptr)
        {
            AzFramework::EntityContextRequestBus::Event(entityContextId, &AzFramework::EntityContextRequestBus::Events::DestroyEntity, entity);
            entity = nullptr;
        }
    }

    void DestroyEntity(AZ::Entity*& entity)
    {
        if (entity != nullptr)
        {
            AzFramework::EntityContextId entityContextId = AzFramework::EntityContextId::CreateNull();
            AzFramework::EntityIdContextQueryBus::EventResult(entityContextId, entity->GetId(), &AzFramework::EntityIdContextQueryBus::Events::GetOwningContextId);
            DestroyEntity(entity, entityContextId);
        }
    }

} //namespace AtomSampleViewer
