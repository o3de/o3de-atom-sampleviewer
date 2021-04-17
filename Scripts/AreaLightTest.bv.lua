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

function SetupPointLights()
    SetImguiValue('AreaLightSample/LightType/Point', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, 0.0, 0.0))
    SetImguiValue('AreaLightSample/Radius', 1.0)
end

function SetupDiskLights()
    SetImguiValue('AreaLightSample/LightType/Disk', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, 0.0, 0.0))
    SetImguiValue('AreaLightSample/Radius', 1.0)
    SetImguiValue('AreaLightSample/X rotation', -1.5)
    SetImguiValue('AreaLightSample/Y rotation', -0.125)
    SetImguiValue('AreaLightSample/Emit Both Directions', false)
end

function SetupCapsuleLights()
    SetImguiValue('AreaLightSample/LightType/Capsule', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, 0.0, 0.0))
    SetImguiValue('AreaLightSample/Radius', 0.5)
    SetImguiValue('AreaLightSample/Capsule Height', 2.0)
    SetImguiValue('AreaLightSample/Y rotation', -3.25)
    SetImguiValue('AreaLightSample/X rotation', -0.75)
end

function SetupQuadLights()
    SetImguiValue('AreaLightSample/LightType/Quad', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, 0.0, 0.0))
    SetImguiValue('AreaLightSample/Emit Both Directions', false)
    SetImguiValue('AreaLightSample/Quad Dimensions', Vector2(1.0, 3.0))
    SetImguiValue('AreaLightSample/X rotation', 1.4)
    SetImguiValue('AreaLightSample/Y rotation', -2.25)
    SetImguiValue('AreaLightSample/Z rotation', -0.25)
    SetImguiValue('AreaLightSample/Use Fast Approximation', false)
end

function SetupQuadLightsDoubleSided()
    SetImguiValue('AreaLightSample/LightType/Quad', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, -0.5, -1.25))
    SetImguiValue('AreaLightSample/Emit Both Directions', true)
    SetImguiValue('AreaLightSample/Quad Dimensions', Vector2(1.5, 0.6))
    SetImguiValue('AreaLightSample/X rotation', -1.45)
    SetImguiValue('AreaLightSample/Y rotation', -0.5)
    SetImguiValue('AreaLightSample/Z rotation', 0.85)
    SetImguiValue('AreaLightSample/Use Fast Approximation', false)
end

function SetupPolygonLights()
    SetImguiValue('AreaLightSample/LightType/Polygon', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, 0.0, 0.0))
    SetImguiValue('AreaLightSample/Emit Both Directions', false)
    SetImguiValue('AreaLightSample/Star Points', 5)
    SetImguiValue('AreaLightSample/Star Min-Max Radius', Vector2(0.5, 1.0))
    SetImguiValue('AreaLightSample/X rotation', 0.5)
    SetImguiValue('AreaLightSample/Y rotation', -0.25)
    SetImguiValue('AreaLightSample/Z rotation', 0.75)
end

function SetupPolygonLightsDoubleSided()
    SetImguiValue('AreaLightSample/LightType/Polygon', true)
    SetImguiValue('AreaLightSample/Position Offset', Vector3(0.0, -0.5, -1.25))
    SetImguiValue('AreaLightSample/Emit Both Directions', true)
    SetImguiValue('AreaLightSample/Star Points', 32)
    SetImguiValue('AreaLightSample/Star Min-Max Radius', Vector2(0.5, 0.75))
    SetImguiValue('AreaLightSample/X rotation', -1.45)
    SetImguiValue('AreaLightSample/Y rotation', -0.5)
    SetImguiValue('AreaLightSample/Z rotation', 0.85)
end

function SetupQuadLightsApprox()
    SetupQuadLights()
    SetImguiValue('AreaLightSample/Use Fast Approximation', true)
end

function SetupQuadLightsDoubleSidedApprox()
    SetupQuadLightsDoubleSided()
    SetImguiValue('AreaLightSample/Use Fast Approximation', true)
end

function VaryRoughnessOnly()
    SetImguiValue('AreaLightSample/Vary Metallic Across Models', false)
    SetImguiValue('AreaLightSample/Vary Roughness Across Models', true)
    SetImguiValue('AreaLightSample/Min Max Roughness', Vector2(0.0, 1.0))
end

function VaryRoughnessNonMetallic()
    VaryRoughnessOnly()
    SetImguiValue('AreaLightSample/Metallic', 0.0)
end

function VaryRoughnessMetallic()
    VaryRoughnessOnly()
    SetImguiValue('AreaLightSample/Metallic', 1.0)
end

function VaryMetallicOnly()
    SetImguiValue('AreaLightSample/Vary Roughness Across Models', false)
    SetImguiValue('AreaLightSample/Vary Metallic Across Models', true)
    SetImguiValue('AreaLightSample/Roughness', 0.5)
    SetImguiValue('AreaLightSample/Min Max Metallic', Vector2(0.0, 1.0))
end

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/AreaLights/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))
SelectImageComparisonToleranceLevel("Level E")

OpenSample('Features/AreaLight')
ResizeViewport(1600, 400)
NoClipCameraController_SetPosition(Vector3(0.0, 0.0, 0.0))
NoClipCameraController_SetFov(DegToRad(40.0))
SetImguiValue('AreaLightSample/Count', 10)

-- Make sure material / model has had a chance to load
IdleSeconds(0.1)

lightSetups = 
{
    point = SetupPointLights,
    disk = SetupDiskLights,
    capsule = SetupCapsuleLights,
    quad = SetupQuadLights,
    quad_double_sided = SetupQuadLightsDoubleSided,
    quad_approx = SetupQuadLightsApprox,
    quad_double_sided_approx = SetupQuadLightsDoubleSidedApprox,
    polygon = SetupPolygonLights,
    polygon_double_sided = SetupPolygonLightsDoubleSided,
}

materialSetups = 
{
    vary_rough_nonmetal = VaryRoughnessNonMetallic,
    vary_rough_metal = VaryRoughnessMetallic,
    vary_metal = VaryMetallicOnly,
}

-- Loop through all light setups and material setups taking screenshots.

for lightName, lightSetupFunction in pairs(lightSetups)
do
    lightSetupFunction()
    for materialName, materialSetupFunction in pairs(materialSetups)
    do
        materialSetupFunction()
        
        IdleFrames(1) 
        CaptureScreenshot(g_screenshotOutputFolder .. '/' .. lightName .. '_' .. materialName ..'.ppm')
    end
end

OpenSample(nil)