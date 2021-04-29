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


g_shaderballModel = 'materialeditor/viewportmodels/shaderball.azmodel'
g_cubeModel = 'materialeditor/viewportmodels/cube.azmodel'
g_beveledCubeModel = 'materialeditor/viewportmodels/beveledcube.azmodel'
g_modelWithoutLayerMask = 'objects/bunny.azmodel'
g_modelWithLayerMask = 'objects/com_envi_me_wall-2x2_testcorner.azmodel'
g_modelLucy = 'materialeditor/viewportmodels/lucy.azmodel'

function GenerateMaterialScreenshot(imageComparisonThresholdLevel, materialName, options)
    if options == nil then options = {} end
    if not (type(options.model) == "string") then
        options.model = g_shaderballModel
    end
    if not (type(options.cameraPitch) == "number") then
        options.cameraPitch = 20
    end
    if not (type(options.cameraHeading) == "number") then
        options.cameraHeading = -30
    end
    if not (type(options.cameraDistance) == "number") then
        options.cameraDistance = 2.0
    end
    if not (type(options.cameraZ) == "number") then
        options.cameraZ = 0.0
    end
    if not (type(options.screenshotFilename) == "string") then
        options.screenshotFilename = materialName
    end

    Print("MODEL: " .. options.model)

    materialName = string.lower(materialName)
    SetImguiValue('Models/##Available', options.model)
    SetImguiValue('Materials/##Available', g_testMaterialsFolder .. materialName .. '.azmaterial')

    if (options.lighting ~= nil) then
        SetImguiValue('Lighting Preset##SampleBase/' .. options.lighting, true)
    else
        SetImguiValue('Lighting Preset##SampleBase/Neutral Urban (Alt)', true)
    end

    -- The sample resets the camera position after loading the model and we need to
    -- make sure that happens before moving the camera below
    IdleFrames(1) 
    
    ArcBallCameraController_SetHeading(DegToRad(options.cameraHeading))
    ArcBallCameraController_SetPitch(DegToRad(-options.cameraPitch))
    ArcBallCameraController_SetDistance(options.cameraDistance)
    ArcBallCameraController_SetPan(Vector3(0.0, 0.0, options.cameraZ))
    
    -- Give some extra frames for the textures to finish streaming in. 
    -- [GFX TODO][ATOM-4819] Hopefully at some point we can make this deterministic
    IdleFrames(10) 

    options.screenshotFilename = string.lower(options.screenshotFilename)
    if type(options.uniqueSuffix) == "string" then
        filename = g_screenshotOutputFolder .. options.screenshotFilename .. '.' .. options.uniqueSuffix .. '.ppm'
    else
        filename = g_screenshotOutputFolder .. options.screenshotFilename .. '.ppm'
    end
    
    SelectImageComparisonToleranceLevel(imageComparisonThresholdLevel)
    CaptureScreenshot(filename)
end

OpenSample('RPI/Mesh')
ResizeViewport(999, 999)

----------------------------------------------------------------------
-- StandardPBR Materials...

g_testMaterialsFolder = 'testdata/materials/standardpbrtestcases/'
g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/StandardPBR/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

GenerateMaterialScreenshot('Level B', '001_DefaultWhite')
GenerateMaterialScreenshot('Level G', '002_BaseColorLerp')
GenerateMaterialScreenshot('Level H', '002_BaseColorLinearLight')
GenerateMaterialScreenshot('Level G', '002_BaseColorMultiply')
GenerateMaterialScreenshot('Level E', '003_MetalMatte')
GenerateMaterialScreenshot('Level F', '003_MetalPolished')
GenerateMaterialScreenshot('Level E', '004_MetalMap')
GenerateMaterialScreenshot('Level E', '005_RoughnessMap')
GenerateMaterialScreenshot('Level E', '006_SpecularF0Map')
GenerateMaterialScreenshot('Level C', '007_MultiscatteringCompensationOff')
GenerateMaterialScreenshot('Level C', '007_MultiscatteringCompensationOn')
GenerateMaterialScreenshot('Level H', '008_NormalMap')
GenerateMaterialScreenshot('Level E', '008_NormalMap_Bevels')
GenerateMaterialScreenshot('Level F', '009_Opacity_Blended', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level H', '009_Opacity_Cutout_PackedAlpha_DoubleSided', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level H', '009_Opacity_Cutout_SplitAlpha_DoubleSided', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level G', '009_Opacity_Cutout_SplitAlpha_SingleSided', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level G', '010_AmbientOcclusion', {lighting="Palermo Sidewalk (Alt)"})
GenerateMaterialScreenshot('Level G', '010_SpecularOcclusion', {lighting="Palermo Sidewalk (Alt)"})
GenerateMaterialScreenshot('Level G', '010_BothOcclusion', {lighting="Palermo Sidewalk (Alt)"})
GenerateMaterialScreenshot('Level G', '011_Emissive')
GenerateMaterialScreenshot('Level I', '012_Parallax_POM', {model=g_cubeModel, cameraHeading=-35.0, cameraPitch=35.0})
GenerateMaterialScreenshot('Level I', '013_SpecularAA_Off', {lighting="Dark Test Lighting"})
GenerateMaterialScreenshot('Level J', '013_SpecularAA_On', {lighting="Dark Test Lighting"})
GenerateMaterialScreenshot('Level G', '014_ClearCoat', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level H', '014_ClearCoat_NormalMap', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level H', '014_ClearCoat_NormalMap_2ndUv', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level F', '014_ClearCoat_RoughnessMap', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level F', '015_SubsurfaceScattering', {lighting="Dark Test Lighting", cameraHeading = 45.0, cameraPitch=30.0, cameraDistance=1.5})
GenerateMaterialScreenshot('Level D', '015_SubsurfaceScattering_Transmission', {lighting="Dark Test Lighting", cameraHeading = 45.0, cameraPitch=-30.0, cameraDistance=1.5})
GenerateMaterialScreenshot('Level I', '100_UvTiling_AmbientOcclusion')
GenerateMaterialScreenshot('Level I', '100_UvTiling_BaseColor')
GenerateMaterialScreenshot('Level I', '100_UvTiling_Emissive')
GenerateMaterialScreenshot('Level H', '100_UvTiling_Metallic')
GenerateMaterialScreenshot('Level I', '100_UvTiling_Normal')
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_Rotate20',      {model=g_cubeModel, lighting="Dark Test Lighting", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_Rotate90',      {model=g_cubeModel, lighting="Dark Test Lighting", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_ScaleOnlyU',    {model=g_cubeModel, lighting="Dark Test Lighting", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_ScaleOnlyV',    {model=g_cubeModel, lighting="Dark Test Lighting", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_ScaleUniform',  {model=g_cubeModel, lighting="Dark Test Lighting", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_TransformAll',  {model=g_cubeModel, lighting="Dark Test Lighting", cameraHeading=225.0})
GenerateMaterialScreenshot('Level M', '100_UvTiling_Opacity',    { lighting="Neutral Urban" })
GenerateMaterialScreenshot('Level L', '100_UvTiling_Parallax_A', { uniqueSuffix="Angle1", model=g_cubeModel, cameraHeading=35.0, cameraPitch=35.0 })
GenerateMaterialScreenshot('Level M', '100_UvTiling_Parallax_A', { uniqueSuffix="Angle2", model=g_cubeModel, cameraHeading=125.0, cameraPitch=35.0 })
GenerateMaterialScreenshot('Level L', '100_UvTiling_Parallax_B', { uniqueSuffix="Angle1", model=g_cubeModel, cameraHeading=0.0, cameraPitch=45.0 })
GenerateMaterialScreenshot('Level L', '100_UvTiling_Parallax_B', { uniqueSuffix="Angle2", model=g_cubeModel, cameraHeading=90.0, cameraPitch=45.0 })
GenerateMaterialScreenshot('Level H', '100_UvTiling_Roughness')
GenerateMaterialScreenshot('Level F', '100_UvTiling_SpecularF0')

GenerateMaterialScreenshot('Level E', '101_DetailMaps_LucyBaseNoDetailMaps',         {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level F', '102_DetailMaps_All',                          {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level F', '103_DetailMaps_BaseColor',                    {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level F', '103_DetailMaps_BaseColorWithMask',            {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level F', '104_DetailMaps_Normal',                       {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level F', '104_DetailMaps_NormalWithMask',               {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level F', '105_DetailMaps_BlendMaskUsingDetailUVs',      {model=g_modelLucy, cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})

----------------------------------------------------------------------
-- StandardMultilayerPBR Materials...

g_testMaterialsFolder = 'testdata/materials/standardmultilayerpbrtestcases/'
g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/StandardMultilayerPBR/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

GenerateMaterialScreenshot('Level I', '001_ManyFeatures', {model=g_cubeModel, cameraHeading=-125.0, cameraPitch=-16.0, cameraZ=0.12})
GenerateMaterialScreenshot('Level I', '002_ParallaxPdo', {model=g_cubeModel, cameraHeading=15.0, cameraPitch=28.0, lighting="Goegap (Alt)"})
GenerateMaterialScreenshot('Level G', '003_Debug_BlendMaskValues')
GenerateMaterialScreenshot('Level F', '003_Debug_DepthMaps')
GenerateMaterialScreenshot('Level I', '004_UseVertexColors', {model=g_modelWithLayerMask, cameraHeading=-135.0, cameraPitch=15.0, cameraZ=-0.3, cameraDistance=3.5})
GenerateMaterialScreenshot('Level I', '004_UseVertexColors', {model=g_modelWithoutLayerMask, cameraHeading=145.0, cameraPitch=7.0, cameraZ=-0.1, cameraDistance=3.0, screenshotFilename="004_UseVertexColors_modelHasNoVertexColors"})

----------------------------------------------------------------------
-- Skin Materials...

g_testMaterialsFolder = 'testdata/materials/skintestcases/'
g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/Skin/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

-- We should eventually replace this with realistic skin test cases, these are just placeholders for regression testing
GenerateMaterialScreenshot('Level D', '001_lucy_regression_test',    {model=g_modelLucy, cameraHeading=-26.0, cameraPitch=15.0, cameraDistance=2.0, cameraZ=0.7})
GenerateMaterialScreenshot('Level E', '002_wrinkle_regression_test', {model=g_modelWithLayerMask, cameraHeading=-135.0, cameraPitch=15.0, cameraZ=-0.3, cameraDistance=3.5})

----------------------------------------------------------------------
-- MinimalPBR Materials...

g_testMaterialsFolder = 'materials/minimalpbr/'
g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/MinimalPBR/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

GenerateMaterialScreenshot('Level B', 'MinimalPbr_Default')
GenerateMaterialScreenshot('Level F', 'MinimalPbr_BlueMetal')
GenerateMaterialScreenshot('Level D', 'MinimalPbr_RedDielectric')

----------------------------------------------------------------------
-- AutoBrick Materials...

g_screenshotOutputFolder = ResolvePath('@user@/Scripts/Screenshots/AutoBrick/')
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder))

g_testMaterialsFolder = 'testdata/materials/autobrick/'

GenerateMaterialScreenshot('Level C', 'Brick', {model=g_cubeModel})
GenerateMaterialScreenshot('Level D', 'Tile', {model=g_cubeModel})