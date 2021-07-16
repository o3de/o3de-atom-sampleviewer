/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/std/string/string_view.h>
#include <AzCore/Component/Entity.h>
#include <AzFramework/Entity/EntityContext.h>

namespace AtomSampleViewer
{
    AZ::Entity* CreateEntity(const AZStd::string_view& name, AzFramework::EntityContextId entityContextId);
    void DestroyEntity(AZ::Entity*& entity, AzFramework::EntityContextId entityContextId);
    void DestroyEntity(AZ::Entity*& entity);
}
