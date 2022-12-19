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
    
    -- TODO(MaterialPipeline): The deferred pipeline only supports a proof-of-concept set of g-buffers
    -- which are pretty limited, only a subset of the "Base" lighting model. The same shader and g-buffers
    -- are used regardless. For better demonstration, we should try to support the various lighting models.
    
    context:IncludeShader("DepthPass")
    context:IncludeShader("ShadowmapPass")
    context:IncludeShader("DeferredMaterialPass")
    
    -- Unlike Standard and Enhanced, BaseLighting doesn't have "_CustomZ" just because BasePBR.materialtype doesn't use alpha cutout or POM PDO, so we don't need it right now.
    -- But that could be added if some other material type wants to use BaseLighting with one of these features, as they don't really relate to the lighting model.
    if(lightingModel ~= "Base") then
        context:IncludeShader("DeferredMaterialPass_CustomZ")
        context:IncludeShader("DepthPass_CustomZ")
        context:IncludeShader("ShadowmapPass_CustomZ")
    end

    -- Even though the enhanced lighting model is not supported by the deferred pass, since this pipeline 
    -- uses the same transparent pass shaders as the main pipeline, we can actually support the Standard
    -- and Enhanced lighting models when they are transparent.

    if(lightingModel == "Standard") then
        context:IncludeShader("Transparent_StandardLighting")
        context:IncludeShader("TintedTransparent_StandardLighting")
    end
    
    if(lightingModel == "Enhanced") then
        context:IncludeShader("Transparent_EnhancedLighting")
        context:IncludeShader("TintedTransparent_EnhancedLighting")
    end
    
    return true
end

