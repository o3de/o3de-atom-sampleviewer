--------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

function MaterialTypeSetup(context)
    lightingModel = context:GetLightingModelName()
    Print('Material type uses lighting model "' .. lightingModel .. '".')

    context:ExcludeAllShaders()
    
    context:IncludeShader("DepthPass")
    context:IncludeShader("ShadowmapPass")
    context:IncludeShader("VisibilityPass")

    if(lightingModel == "Base") then
        context:IncludeShader("DeferredPass_BaseLighting")
    end
    
    if (lightingModel == "Standard") then
        context:IncludeShader("VisibilityPass_CustomZ")
        context:IncludeShader("DeferredPass_StandardLighting")
        context:IncludeShader("Transparent_StandardLighting")
        context:IncludeShader("TintedTransparent_StandardLighting")
    end

    if (lightingModel == "Enhanced") then
        context:IncludeShader("VisibilityPass_CustomZ")
        context:IncludeShader("DeferredPass_EnhancedLighting")
        context:IncludeShader("Transparent_EnhancedLighting")
        context:IncludeShader("TintedTransparent_EnhancedLighting")
    end

    if (lightingModel == "Skin") then
        context:IncludeShader("DeferredPass_SkinLighting")
    end
    return true
end

