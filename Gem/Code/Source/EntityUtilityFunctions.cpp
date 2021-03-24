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
