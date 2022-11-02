----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

RunScript("scripts/TestEnvironment.luac")

g_shaderballModel = 'materialeditor/viewportmodels/shaderball.azmodel'
g_cubeModel = 'materialeditor/viewportmodels/cube.azmodel'
g_beveledCubeModel = 'materialeditor/viewportmodels/beveledcube.azmodel'
g_modelWithoutLayerMask = 'objects/bunny.azmodel'
g_modelWithLayerMask = 'testdata/objects/paintedplane.azmodel'
g_modelHermanubis = 'materialeditor/viewportmodels/hermanubis.azmodel'
g_modelTube = 'testdata/objects/tube.azmodel'
g_modelGrass = 'objects/grass_tile_large.azmodel'

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
    if not (type(options.showGroundPlane) == "boolean") then
        options.showGroundPlane = false
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
    
    SetImguiValue('Show Ground Plane', options.showGroundPlane)

    -- The sample resets the camera position after loading the model and we need to
    -- make sure that happens before moving the camera below
    IdleFrames(1) 
    
    ArcBallCameraController_SetHeading(DegToRad(options.cameraHeading))
    ArcBallCameraController_SetPitch(DegToRad(-options.cameraPitch))
    ArcBallCameraController_SetDistance(options.cameraDistance)
    ArcBallCameraController_SetPan(Vector3(0.0, 0.0, options.cameraZ))
    
    -- Give some extra frames for the textures to finish streaming in.
    IdleFrames(10) 

    options.screenshotFilename = string.lower(options.screenshotFilename)
    if type(options.uniqueSuffix) == "string" then
        filename = g_testCaseFolder .. '/' .. options.screenshotFilename .. '.' .. options.uniqueSuffix .. '.png'
    else
        filename = g_testCaseFolder .. '/' .. options.screenshotFilename .. '.png'
    end
    
    SelectImageComparisonToleranceLevel(imageComparisonThresholdLevel)
    CaptureScreenshot(filename)
end

OpenSample('RPI/Mesh')
ResizeViewport(999, 999)

-- This is just quick regression test for a crash that happened when turning on the ground plane with no model
SetImguiValue('Show Ground Plane', true)
IdleFrames(1) 
SetImguiValue('Show Ground Plane', false)

----------------------------------------------------------------------
-- StandardPBR Materials...

g_testMaterialsFolder = 'testdata/materials/standardpbrtestcases/'
g_testCaseFolder = 'StandardPBR'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

GenerateMaterialScreenshot('Level C', '001_DefaultWhite')
GenerateMaterialScreenshot('Level G', '002_BaseColorLerp')
GenerateMaterialScreenshot('Level H', '002_BaseColorLinearLight')
GenerateMaterialScreenshot('Level G', '002_BaseColorMultiply')
GenerateMaterialScreenshot('Level E', '003_MetalMatte')
GenerateMaterialScreenshot('Level F', '003_MetalPolished')
GenerateMaterialScreenshot('Level E', '004_MetalMap')
GenerateMaterialScreenshot('Level E', '005_RoughnessMap')
GenerateMaterialScreenshot('Level E', '006_SpecularF0Map')
GenerateMaterialScreenshot('Level D', '007_MultiscatteringCompensationOff')
GenerateMaterialScreenshot('Level D', '007_MultiscatteringCompensationOn')
GenerateMaterialScreenshot('Level H', '008_NormalMap')
GenerateMaterialScreenshot('Level E', '008_NormalMap_Bevels')
GenerateMaterialScreenshot('Level F', '009_Opacity_Blended', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level F', '009_Opacity_Blended_Alpha_Affects_Specular', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level H', '009_Opacity_Cutout_PackedAlpha_DoubleSided', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level H', '009_Opacity_Cutout_SplitAlpha_DoubleSided', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level G', '009_Opacity_Cutout_SplitAlpha_SingleSided', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level F', '009_Opacity_Opaque_DoubleSided', {lighting="Neutral Urban", model=g_modelTube, cameraDistance=4.0, cameraPitch=45.0})
GenerateMaterialScreenshot('Level F', '009_Opacity_TintedTransparent', {lighting="Neutral Urban", model=g_beveledCubeModel})
GenerateMaterialScreenshot('Level H', '010_AmbientOcclusion', {lighting="Palermo Sidewalk (Alt)"})
GenerateMaterialScreenshot('Level G', '010_SpecularOcclusion', {lighting="Palermo Sidewalk (Alt)"})
GenerateMaterialScreenshot('Level G', '010_BothOcclusion', {lighting="Palermo Sidewalk (Alt)"})
GenerateMaterialScreenshot('Level G', '011_Emissive')
GenerateMaterialScreenshot('Level I', '012_Parallax_POM', {model=g_cubeModel, cameraHeading=-35.0, cameraPitch=35.0})
GenerateMaterialScreenshot('Level I', '012_Parallax_POM_Cutout', {model=g_cubeModel, cameraHeading=-130.0, cameraPitch=35.0, cameraDistance=3.5, lighting="Goegap (Alt)", showGroundPlane=true})
GenerateMaterialScreenshot('Level I', '013_SpecularAA_Off', {lighting="Test"})
GenerateMaterialScreenshot('Level J', '013_SpecularAA_On', {lighting="Test"})
GenerateMaterialScreenshot('Level G', '014_ClearCoat', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level H', '014_ClearCoat_NormalMap', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level H', '014_ClearCoat_NormalMap_2ndUv', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level F', '014_ClearCoat_RoughnessMap', {lighting="Neutral Urban"})
GenerateMaterialScreenshot('Level F', '015_SubsurfaceScattering', {lighting="Test", cameraHeading = 45.0, cameraPitch=30.0, cameraDistance=1.5})
GenerateMaterialScreenshot('Level D', '015_SubsurfaceScattering_Transmission', {lighting="Test", cameraHeading = 45.0, cameraPitch=-30.0, cameraDistance=1.5})
GenerateMaterialScreenshot('Level E', '015_SubsurfaceScattering_Transmission_Thin', {model=g_modelGrass, lighting="Goegap (Alt)", cameraHeading = -150.0, cameraPitch=5.0, cameraDistance=1.5})
GenerateMaterialScreenshot('Level I', '100_UvTiling_AmbientOcclusion')
GenerateMaterialScreenshot('Level I', '100_UvTiling_BaseColor')
GenerateMaterialScreenshot('Level I', '100_UvTiling_Emissive')
GenerateMaterialScreenshot('Level H', '100_UvTiling_Metallic')
GenerateMaterialScreenshot('Level I', '100_UvTiling_Normal')
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_Rotate20',      {model=g_cubeModel, lighting="Test", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_Rotate90',      {model=g_cubeModel, lighting="Test", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_ScaleOnlyU',    {model=g_cubeModel, lighting="Test", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_ScaleOnlyV',    {model=g_cubeModel, lighting="Test", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_ScaleUniform',  {model=g_cubeModel, lighting="Test", cameraHeading=225.0})
GenerateMaterialScreenshot('Level G', '100_UvTiling_Normal_Dome_TransformAll',  {model=g_cubeModel, lighting="Test", cameraHeading=225.0})
GenerateMaterialScreenshot('Level M', '100_UvTiling_Opacity',    { lighting="Neutral Urban" })
GenerateMaterialScreenshot('Level L', '100_UvTiling_Parallax_A', { uniqueSuffix="Angle1", model=g_cubeModel, cameraHeading=35.0, cameraPitch=35.0 })
GenerateMaterialScreenshot('Level M', '100_UvTiling_Parallax_A', { uniqueSuffix="Angle2", model=g_cubeModel, cameraHeading=125.0, cameraPitch=35.0 })
GenerateMaterialScreenshot('Level L', '100_UvTiling_Parallax_B', { uniqueSuffix="Angle1", model=g_cubeModel, cameraHeading=0.0, cameraPitch=45.0 })
GenerateMaterialScreenshot('Level L', '100_UvTiling_Parallax_B', { uniqueSuffix="Angle2", model=g_cubeModel, cameraHeading=90.0, cameraPitch=45.0 })
GenerateMaterialScreenshot('Level H', '100_UvTiling_Roughness')
GenerateMaterialScreenshot('Level F', '100_UvTiling_SpecularF0')

GenerateMaterialScreenshot('Level F', '101_DetailMaps_BaseNoDetailMaps',        {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level H', '102_DetailMaps_All',                     {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level G', '103_DetailMaps_BaseColor',               {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level G', '103_DetailMaps_BaseColorWithMask',       {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level H', '104_DetailMaps_Normal',                  {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level H', '104_DetailMaps_NormalWithMask',          {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})
GenerateMaterialScreenshot('Level H', '105_DetailMaps_BlendMaskUsingDetailUVs', {model=g_modelHermanubis, lighting="Blouberg Sunrise 1 (Alt)", cameraHeading=175.0, cameraPitch=5.0, cameraDistance=0.75, cameraZ=0.5})

----------------------------------------------------------------------
-- StandardMultilayerPBR Materials...

g_testMaterialsFolder = 'testdata/materials/standardmultilayerpbrtestcases/'
g_testCaseFolder = 'StandardMultilayerPBR'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

GenerateMaterialScreenshot('Level I', '001_ManyFeatures', {model=g_cubeModel, cameraHeading=-125.0, cameraPitch=-16.0, cameraZ=0.12})
GenerateMaterialScreenshot('Level I', '001_ManyFeatures_Layer2Off', {model=g_cubeModel, cameraHeading=-125.0, cameraPitch=-16.0, cameraZ=0.12})
GenerateMaterialScreenshot('Level I', '001_ManyFeatures_Layer3Off', {model=g_cubeModel, cameraHeading=-125.0, cameraPitch=-16.0, cameraZ=0.12})
GenerateMaterialScreenshot('Level J', '002_ParallaxPdo', {model=g_cubeModel, cameraHeading=15.0, cameraPitch=28.0, lighting="Goegap (Alt)"})
GenerateMaterialScreenshot('Level G', '003_Debug_BlendMask', {model=g_cubeModel})
GenerateMaterialScreenshot('Level G', '003_Debug_Displacement', {model=g_cubeModel})
GenerateMaterialScreenshot('Level G', '003_Debug_BlendWeights', {model=g_cubeModel})
GenerateMaterialScreenshot('Level I', '004_UseVertexColors', {model=g_modelWithLayerMask, cameraHeading=0.0, cameraPitch=45.0, cameraDistance=30.0})
GenerateMaterialScreenshot('Level I', '004_UseVertexColors', {model=g_modelWithoutLayerMask, cameraHeading=145.0, cameraPitch=7.0, cameraZ=-0.1, cameraDistance=3.0, screenshotFilename="004_UseVertexColors_modelHasNoVertexColors"})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement',                            {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=-35.0, cameraPitch=33.0, cameraDistance=9.0, cameraZ=0.9})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement_Layer2Off',                  {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=-35.0, cameraPitch=33.0, cameraDistance=9.0, cameraZ=0.9})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement_Layer3Off',                  {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=-35.0, cameraPitch=33.0, cameraDistance=9.0, cameraZ=0.9})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement_With_BlendMaskVertexColors', {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=0.0, cameraPitch=80.0, cameraDistance=27.0})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement_With_BlendMaskTexture',               {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=180.0, cameraPitch=45.0, cameraDistance=20.0, cameraZ=-0.5})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement_With_BlendMaskTexture_NoHeightmaps',  {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=180.0, cameraPitch=45.0, cameraDistance=20.0, cameraZ=-0.5})
GenerateMaterialScreenshot('Level I', '005_UseDisplacement_With_BlendMaskTexture_AllSameHeight', {lighting="Blouberg Sunrise 1 (Alt)", model=g_modelWithLayerMask, cameraHeading=180.0, cameraPitch=45.0, cameraDistance=20.0, cameraZ=-0.5})

----------------------------------------------------------------------
-- Skin Materials...

g_testMaterialsFolder = 'testdata/materials/skintestcases/'
g_testCaseFolder = 'Skin'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

-- We should eventually replace this with realistic skin test cases, these are just placeholders for regression testing
GenerateMaterialScreenshot('Level D', '001_hermanubis_regression_test',    {model=g_modelHermanubis, cameraHeading=-26.0, cameraPitch=15.0, cameraDistance=2.0, cameraZ=0.7})
GenerateMaterialScreenshot('Level G', '002_wrinkle_regression_test', {model=g_modelWithLayerMask, cameraHeading=0.0, cameraPitch=45.0, cameraDistance=30.0})

----------------------------------------------------------------------
-- MinimalPBR Materials...

g_testMaterialsFolder = 'materials/minimalpbr/'
g_testCaseFolder = 'MinimalPBR'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

GenerateMaterialScreenshot('Level C', 'MinimalPbr_Default')
GenerateMaterialScreenshot('Level F', 'MinimalPbr_BlueMetal')
GenerateMaterialScreenshot('Level D', 'MinimalPbr_RedDielectric')

----------------------------------------------------------------------
-- MinimalMultilayerPBR Materials...
-- This test is here only temporarily for regression testing until StandardMultilayerPBR is refactored to use reused nested property groups.

g_testMaterialsFolder = 'materials/types/'
g_testCaseFolder = 'MinimalPBR'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

GenerateMaterialScreenshot('Level G', 'MinimalMultilayerExample')

----------------------------------------------------------------------
-- AutoBrick Materials...

g_testCaseFolder = 'AutoBrick'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

g_testMaterialsFolder = 'testdata/materials/autobrick/'

GenerateMaterialScreenshot('Level C', 'Brick', {model=g_cubeModel})
GenerateMaterialScreenshot('Level E', 'Tile', {model=g_cubeModel})

----------------------------------------------------------------------
-- Low End Pipeline...

SetImguiValue('Use Low End Pipeline', true)

-- Toggle a couple extra times to test a specific issue where the render pipelines would become disabled.
IdleFrames(2) 
SetImguiValue('Use Low End Pipeline', false)
IdleFrames(2) 
SetImguiValue('Use Low End Pipeline', true)
IdleFrames(2) 

g_testMaterialsFolder = 'testdata/materials/standardpbrtestcases/'
g_testCaseFolder = 'LowEndPipeline'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

-- We're not really getting full coverage here, but just some cursory regression tests to make sure the low-end pipeline doesn't break.
GenerateMaterialScreenshot('Level E', '004_MetalMap')
GenerateMaterialScreenshot('Level F', '009_Opacity_Blended', {lighting="Neutral Urban", model=g_beveledCubeModel})

SetImguiValue('Use Low End Pipeline', false)

----------------------------------------------------------------------
-- Material Pipeline Abstraction...

-- These test materials are temporary, specifically for regression testing the new material pipeline system while it is in development.
-- Once the core material types like StandardPBR are ported to use material pipelines, we can switch these over to use core material 
-- types, and maybe remove some cases if they end up being redundant.

g_testMaterialsFolder = 'materials/materialpipelinetest/'
g_testCaseFolder = 'MaterialPipelineSystem'
Print('Saving screenshots to ' .. NormalizePath(g_screenshotOutputFolder .. g_testCaseFolder))

SetImguiValue('Use Low End Pipeline', false)
SetImguiValue('Use Deferred Pipeline', false)
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Basic', {screenshotFilename="MaterialPipelineTest_Basic_MainPipeline"})
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Standard', {screenshotFilename="MaterialPipelineTest_Standard_MainPipeline"})
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Enhanced', {screenshotFilename="MaterialPipelineTest_Enhanced_MainPipeline", lighting="Test"})

SetImguiValue('Use Low End Pipeline', true)
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Basic', {screenshotFilename="MaterialPipelineTest_Basic_LowEndPipeline"})
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Standard', {screenshotFilename="MaterialPipelineTest_Standard_LowEndPipeline"})
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Enhanced', {screenshotFilename="MaterialPipelineTest_Enhanced_LowEndPipeline", lighting="Test"})
SetImguiValue('Use Low End Pipeline', false)

SetImguiValue('Use Deferred Pipeline', true)
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Basic', {screenshotFilename="MaterialPipelineTest_Basic_DeferredPipeline"})
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Standard', {screenshotFilename="MaterialPipelineTest_Standard_DeferredPipeline"})
GenerateMaterialScreenshot('Level E', 'MaterialPipelineTest_Enhanced', {screenshotFilename="MaterialPipelineTest_Enhanced_DeferredPipeline", lighting="Test"})
SetImguiValue('Use Deferred Pipeline', false)
