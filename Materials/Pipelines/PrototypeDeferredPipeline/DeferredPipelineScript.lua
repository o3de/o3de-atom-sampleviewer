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

    if(lightingModel == "Base" or lightingModel == "Standard" or lightingModel == "Enhanced") then
        context:IncludeShader("DeferredMaterialPass")
    else
        Warning("The deferred pipeline does not support the " .. lightingModel .. " lighting model. This combination should not be used at runtime.")
        -- This allows returning 'true' to pass the build, but the surface won't be rendered at runtime.
        -- TODO(MaterialPipeline): Instead of rendering nothing, either render an error shader (like a magenta surface) or fall back to StandardLighting.
        --                         For an error shader, .materialtype needs to have new field for an ObjectSrg azsli file separate from "materialShaderCode", so that
        --                         the error shader can use the same ObjectSrg as the other shaders (depth/shadow) without including the unsupported materialShaderCode.
        --                         Using StandardLighting as a fallback is even more difficult because it requires some kind of adapter to move data from the Surface that
        --                         the material type wants to use, to the Surface that the lighting model supports. (It's a natural fit for downgrading from Enhanced to
        --                         Standard but there is compatibility issues between Skin and Standard).
    end
    
    -- Unlike Standard and Enhanced, BaseLighting doesn't have "_CustomZ" just because BasePBR.materialtype doesn't use alpha cutout or POM PDO, so we don't need it right now.
    -- But that could be added if some other material type wants to use BaseLighting with one of these features, as they don't really relate to the lighting model.
    if(lightingModel ~= "Base" and lightingModel ~= "Skin") then
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

