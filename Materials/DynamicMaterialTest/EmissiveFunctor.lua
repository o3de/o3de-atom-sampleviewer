----------------------------------------------------------------------------------------------------
--
-- All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
-- its licensors.
--
-- For complete copyright and license terms please see the LICENSE at the root of this
-- distribution (the "License"). All use of this software is governed by the License,
-- or, if provided, by the license below or the license accompanying this file. Do not
-- remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--
--
----------------------------------------------------------------------------------------------------

function GetMaterialPropertyDependencies()
    return {"emissive.useTexture", "emissive.textureMap", "emissive.intensity", "emissive.unit"}
end

function GetShaderOptionDependencies()
    return {"o_emissive_useTexture"}
end

 
-- These values are based on the Intensity enum defined in the .materialtype file
LightUnitProperty_EV100 = 0
LightUnitProperty_Nit = 1

function Process(context)

    local useTexture = context:GetMaterialPropertyValue_bool("emissive.useTexture")
    local textureMap = context:GetMaterialPropertyValue_image("emissive.textureMap")
    
    context:SetShaderOptionValue_bool("o_emissive_useTexture", useTexture and textureMap ~= nil)

    local lightUnit = context:GetMaterialPropertyValue_enum("emissive.unit")
    local intensity = context:GetMaterialPropertyValue_float("emissive.intensity")
    
    -- [GFX TODO][ATOM-6014]: this doesn't work yet because I need a way for the PhotometricUnit code (which is in the Common Feature gem)
    -- to reflect itself into the LuaMaterialFunctor's BehaviorContext (which is in the RPI gem). In the meantime, we replicate the math from ConvertIntensityBetweenUnits
    --local sourcePhotometricUnit
    --if(LightUnitProperty_EV100 == lightUnit) then
    --    sourcePhotometricUnit = PhotometricUnit_Ev100_Luminance
    --elseif(LightUnitProperty_Nit == lightUnit) then
    --    sourcePhotometricUnit = PhotometricUnit_Nit
    --else
    --    Error("Unrecognized light unit.")
    --end
    
    --intensity = PhotometricValue_ConvertIntensityBetweenUnits(sourceType, PhotometricUnit_Nit, intensity)

    if(LightUnitProperty_EV100 == lightUnit) then
        local Ev100LightMeterConstantLuminance = 12.5
        local Ev100Iso = 100.0
        local Ev100ShutterSpeed = 1.0
        intensity = (Ev100LightMeterConstantLuminance * Math.Pow(2.0, intensity)) / (Ev100Iso * Ev100ShutterSpeed)
    end

    context:SetShaderConstant_float("m_emissiveIntensity", intensity)
end

function ProcessEditor(context)
    
    local textureMap = context:GetMaterialPropertyValue_image("emissive.textureMap")

    if(nil == textureMap) then
        context:SetMaterialPropertyVisibility("emissive.useTexture", MaterialPropertyVisibility_Disabled)
    else
        context:SetMaterialPropertyVisibility("emissive.useTexture", MaterialPropertyVisibility_Enabled)
    end

    -- Update range values based on light units
    
    local lightUnit = context:GetMaterialPropertyValue_enum("emissive.unit")
    
    if(LightUnitProperty_EV100 == lightUnit) then
        context:SetMaterialPropertyMinValue_float("emissive.intensity", -10)
        context:SetMaterialPropertyMaxValue_float("emissive.intensity", 20)
        context:SetMaterialPropertySoftMinValue_float("emissive.intensity", -6)
        context:SetMaterialPropertySoftMaxValue_float("emissive.intensity", 16)
    elseif(LightUnitProperty_Nit == lightUnit) then
        context:SetMaterialPropertyMinValue_float("emissive.intensity", 0.0)
        context:SetMaterialPropertyMaxValue_float("emissive.intensity", 100000.0)
        context:SetMaterialPropertySoftMinValue_float("emissive.intensity", 0.001)
        context:SetMaterialPropertySoftMaxValue_float("emissive.intensity", 20.0)
    else
        Error("Unrecognized light unit.")
    end
    
end

