/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <MaterialHotReloadTestComponent.h>
#include <SampleComponentManager.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/Feature/Material/MaterialAssignment.h>

#include <AzCore/IO/Path/Path.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <AzCore/Utils/Utils.h>

#include <RHI/BasicRHIComponent.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;

    namespace
    {
        namespace Sources
        {
            static constexpr const char MaterialFileName[] = "HotReloadTest.material";
            static constexpr const char MaterialTypeFileName[] = "HotReloadTest.materialtype";
            static constexpr const char ShaderFileName[] = "HotReloadTest.shader";
            static constexpr const char AzslFileName[] = "HotReloadTest.azsl";
            static constexpr const char ShaderVariantListFileName[] = "HotReloadTest.shadervariantlist";
        }

        namespace Products
        {

            static constexpr const char ModelFilePath[] = "objects/plane.azmodel";
            static constexpr const char MaterialFilePath[] = "materials/hotreloadtest/temp/hotreloadtest.azmaterial";
            static constexpr const char MaterialTypeFilePath[] = "materials/hotreloadtest/temp/hotreloadtest.azmaterialtype";
            static constexpr const char ShaderFilePath[] = "materials/hotreloadtest/temp/hotreloadtest.azshader";

            static constexpr const char ShaderVariantTreeFileName[] = "hotreloadtest.azshadervarianttree";

            static AZStd::string GetShaderVariantFileName(RPI::ShaderVariantStableId stableId)
            {
                RPISystemInterface* rpiSystem = RPISystemInterface::Get();
                Name apiName = rpiSystem->GetRenderApiName();
                AZStd::string filePath = AZStd::string::format("hotreloadtest_%s_%u.azshadervariant", apiName.GetCStr(), stableId.GetIndex());
                return filePath;
            }

            // These materials are used to communicate which shader variant is being used, for screenshot comparison tests.
            static constexpr const char RootVariantIndicatorMaterialFilePath[] = "materials/hotreloadtest/testdata/variantselection_root.azmaterial";
            static constexpr const char FullyBakedVariantIndicatorMaterialFilePath[] = "materials/hotreloadtest/testdata/variantselection_fullybaked.azmaterial";
        }

        namespace TestData
        {
            namespace MaterialFileNames
            {
                static constexpr const char Default[] = "Material_UseDefaults.txt";
                static constexpr const char ChangePrimaryToRed[] = "Material_ChangePrimaryToRed.txt";
                static constexpr const char ChangePrimaryToBlue[] = "Material_ChangePrimaryToBlue.txt";
            }

            namespace MaterialTypeFileNames
            {
                static constexpr const char StraightLines[] = "MaterialType_StraightLines.txt";
                static constexpr const char WavyLines[] = "MaterialType_WavyLines.txt";
            }

            namespace ShaderFileNames
            {
                static constexpr const char BlendingOff[] = "Shader_BlendingOff.txt";
                static constexpr const char BlendingOn[] = "Shader_BlendingOn.txt";
            }

            namespace AzslFileNames
            {
                static constexpr const char HorizontalPattern[] = "AZSL_HorizontalPattern.txt";
                static constexpr const char VerticalPattern[] = "AZSL_VerticalPattern.txt";
            }

            namespace ShaderVariantListFileNames
            {
                static constexpr const char All[] = "ShaderVariantList_All.txt";
                static constexpr const char OnlyStraightLines[] = "ShaderVariantList_OnlyStraightLines.txt";
                static constexpr const char OnlyWavyLines[] = "ShaderVariantList_OnlyWavyLines.txt";
            }
        }
    }

    void MaterialHotReloadTestComponent::Reflect(ReflectContext* context)
    {
        if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<MaterialHotReloadTestComponent, CommonSampleComponentBase>()
                ->Version(0)
                ;
        }
    }

    MaterialHotReloadTestComponent::MaterialHotReloadTestComponent()
        : m_imguiSidebar{"@user@/MaterialHotReloadTestComponent/sidebar.xml"}
    {
    }
    
    void MaterialHotReloadTestComponent::InitTestDataFolders()
    {
        auto io = AZ::IO::LocalFileIO::GetInstance();

        auto projectPath = AZ::Utils::GetProjectPath();
        AZStd::string mainTestFolder;
        AzFramework::StringFunc::Path::Join(projectPath.c_str(), "Materials/HotReloadTest/", mainTestFolder);
        AzFramework::StringFunc::Path::Join(mainTestFolder.c_str(), "TestData/", m_testDataFolder);
        if (!io->Exists(m_testDataFolder.c_str()))
        {
            AZ_Error("MaterialHotReloadTestComponent", false, "Could not find source folder '%s'. This sample can only be used on dev platforms.", m_testDataFolder.c_str());
            m_testDataFolder.clear();
            return;
        }

        AzFramework::StringFunc::Path::Join(mainTestFolder.c_str(), "Temp/", m_tempSourceFolder);
        if (!io->CreatePath(m_tempSourceFolder.c_str()))
        {
            AZ_Error("MaterialHotReloadTestComponent", false, "Could not create temp folder '%s'.", m_tempSourceFolder.c_str());
        }
    }

    void MaterialHotReloadTestComponent::Activate()
    {
        m_initStatus = InitStatus::None;
        
        TickBus::Handler::BusConnect();
        m_imguiSidebar.Activate();

        InitTestDataFolders();

        // Delete any existing temp files and wait for the assets to disappear, to ensure we have a clean slate.
        // (If we were to just replace existing temp source files with the default ones without fully
        //  removing them first, it would be tricky to figure out whether the assets loaded assets are the new
        //  ones or stale ones left over from a prior instance of this sample).
        DeleteTestFile(Sources::MaterialFileName);
        DeleteTestFile(Sources::MaterialTypeFileName);
        DeleteTestFile(Sources::ShaderFileName);
        DeleteTestFile(Sources::AzslFileName);
        DeleteTestFile(Sources::ShaderVariantListFileName);

        m_initStatus = InitStatus::ClearingTestAssets;
        m_clearAssetsTimeout = 5.0f;

        // Wait until the test material is fully initialized. Use a long timeout because it can take a while for the shaders to compile.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, LongTimeout);

        Data::AssetId modelAssetId;
        Data::AssetCatalogRequestBus::BroadcastResult(modelAssetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, Products::ModelFilePath, nullptr, false);
        AZ_Assert(modelAssetId.IsValid(), "Failed to get model asset id: %s", Products::ModelFilePath);
        m_modelAsset.Create(modelAssetId);

        const Transform cameraTransform = Transform::CreateFromQuaternionAndTranslation(
            Quaternion::CreateFromAxisAngle(Vector3::CreateAxisZ(), AZ::Constants::Pi),
            Vector3{0.0f, 2.0f, 0.0f}
        );
        AZ::TransformBus::Event(GetCameraEntityId(), &AZ::TransformBus::Events::SetWorldTM, cameraTransform);

        m_meshFeatureProcessor = Scene::GetFeatureProcessorForEntityContextId<Render::MeshFeatureProcessorInterface>(GetEntityContextId());

        // The shader variant indicator banner will appear right below the main test mesh
        m_shaderVariantIndicatorMeshTransform = Transform::CreateIdentity();
        m_shaderVariantIndicatorMeshTransform.SetTranslation(Vector3{0.0f, 0.0f, -0.6f});
        m_shaderVariantIndicatorMeshNonUniformScale = Vector3(1.0f, 0.125f, 1.0f);
        m_shaderVariantIndicatorMeshTransform.SetRotation(Quaternion::CreateFromAxisAngle(Vector3::CreateAxisX(), -AZ::Constants::HalfPi));

        // Load materials that will be used to indicate which shader variant is active...
        Data::Asset<MaterialAsset> indicatorMaterialAsset;
        indicatorMaterialAsset = AssetUtils::LoadAssetByProductPath<MaterialAsset>(Products::RootVariantIndicatorMaterialFilePath, AssetUtils::TraceLevel::Assert);
        m_shaderVariantIndicatorMaterial_root = Material::FindOrCreate(indicatorMaterialAsset);
        indicatorMaterialAsset = AssetUtils::LoadAssetByProductPath<MaterialAsset>(Products::FullyBakedVariantIndicatorMaterialFilePath, AssetUtils::TraceLevel::Assert);
        m_shaderVariantIndicatorMaterial_fullyBaked = Material::FindOrCreate(indicatorMaterialAsset);
    }

    void MaterialHotReloadTestComponent::Deactivate()
    {
        m_meshFeatureProcessor->ReleaseMesh(m_meshHandle);
        m_meshFeatureProcessor->ReleaseMesh(m_shaderVariantIndicatorMeshHandle);
        m_shaderVariantIndicatorMaterial_root.reset();
        m_shaderVariantIndicatorMaterial_fullyBaked.reset();
        m_shaderVariantIndicatorMaterial_current.reset();

        Data::AssetBus::Handler::BusDisconnect();
        TickBus::Handler::BusDisconnect();
        m_imguiSidebar.Deactivate();
        
        m_initStatus = InitStatus::None;

    }

    void MaterialHotReloadTestComponent::DeleteTestFile(const char* tempSourceFile)
    {
        AZ::IO::Path deletePath = AZ::IO::Path(m_tempSourceFolder).Append(tempSourceFile);

        if (AZ::IO::LocalFileIO::GetInstance()->Exists(deletePath.c_str()))
        {
            m_fileIoErrorHandler.BusConnect();

            if (!AZ::IO::LocalFileIO::GetInstance()->Remove(deletePath.c_str()))
            {
                m_fileIoErrorHandler.ReportLatestIOError(AZStd::string::format("Failed to delete '%s'.", deletePath.c_str()));
            }

            m_fileIoErrorHandler.BusDisconnect();
        }
    }

    void MaterialHotReloadTestComponent::CopyTestFile(const AZStd::string& testDataFile, const AZStd::string& tempSourceFile)
    {
        // Instead of copying the file using AZ::IO::LocalFileIO, we load the file and write out a new file over top
        // the destination. This is necessary to make the AP reliably detect the changes (if we just copy the file,
        // sometimes it recognizes the OS level copy as an updated file and sometimes not).

        AZ::IO::Path copyFrom = AZ::IO::Path(m_testDataFolder).Append(testDataFile);
        AZ::IO::Path copyTo = AZ::IO::Path(m_tempSourceFolder).Append(tempSourceFile);

        m_fileIoErrorHandler.BusConnect();

        auto readResult = AZ::Utils::ReadFile(copyFrom.c_str());
        if (!readResult.IsSuccess())
        {
            m_fileIoErrorHandler.ReportLatestIOError(readResult.GetError());
            return;
        }

        auto writeResult = AZ::Utils::WriteFile(readResult.GetValue(), copyTo.c_str());
        if (!writeResult.IsSuccess())
        {
            m_fileIoErrorHandler.ReportLatestIOError(writeResult.GetError());
            return;
        }

        m_fileIoErrorHandler.BusDisconnect();
    }

    const char* ToString(AzFramework::AssetSystem::AssetStatus status)
    {
        switch (status)
        {
            case AzFramework::AssetSystem::AssetStatus_Missing: return "Missing";
            case AzFramework::AssetSystem::AssetStatus_Queued: return "Queued...";
            case AzFramework::AssetSystem::AssetStatus_Compiling: return "Compiling...";
            case AzFramework::AssetSystem::AssetStatus_Compiled: return "Compiled";
            case AzFramework::AssetSystem::AssetStatus_Failed: return "Failed";
            default: return "Unknown";
        }
    }

    AzFramework::AssetSystem::AssetStatus MaterialHotReloadTestComponent::GetTestAssetStatus(const char* tempSourceFile) const
    {
        AZStd::string filePath = AZStd::string("materials/hotreloadtest/temp/") + tempSourceFile;

        AzFramework::AssetSystem::AssetStatus status = AzFramework::AssetSystem::AssetStatus::AssetStatus_Unknown;
        AzFramework::AssetSystemRequestBus::BroadcastResult(status, &AzFramework::AssetSystem::AssetSystemRequests::GetAssetStatusSearchType,
            filePath.c_str(), AzFramework::AssetSystem::RequestAssetStatus::SearchType::Exact);

        return status;
    }

    void MaterialHotReloadTestComponent::DrawAssetStatus(const char* tempSourceFile, bool includeFileName)
    {
        AzFramework::AssetSystem::AssetStatus status = GetTestAssetStatus(tempSourceFile);

        if (includeFileName)
        {
            ImGui::Text("%s", tempSourceFile);
            ImGui::Indent();
        }

        ImGui::Text("Status:");
        ImGui::SameLine();
        ImGui::Text(ToString(status));

        if (includeFileName)
        {
            ImGui::Unindent();
        }
    }

    Data::AssetId MaterialHotReloadTestComponent::GetAssetId(const char* productFilePath)
    {
        Data::AssetId assetId;
        Data::AssetCatalogRequestBus::BroadcastResult(assetId, &Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, productFilePath, nullptr, false);
        return assetId;
    }

    void MaterialHotReloadTestComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (m_initStatus == InitStatus::WaitingForDefaultMaterialToLoad && asset.GetId() == m_materialAsset.GetId())
        {
            m_materialAsset = asset;
            Data::AssetBus::Handler::BusDisconnect(asset.GetId());
            m_initStatus = InitStatus::Ready;

            // Now that we finally have the material asset, we can add the model to the scene...

            m_material = Material::Create(m_materialAsset);

            Transform meshTransform = Transform::CreateFromQuaternion(Quaternion::CreateFromAxisAngle(Vector3::CreateAxisX(), -AZ::Constants::HalfPi));
            m_meshHandle = m_meshFeatureProcessor->AcquireMesh(AZ::Render::MeshHandleDescriptor{ m_modelAsset }, m_material);
            m_meshFeatureProcessor->SetTransform(m_meshHandle, meshTransform);

            Data::Instance<RPI::Model> model = GetMeshFeatureProcessor()->GetModel(m_meshHandle);
            if (model)
            {
                // Both the model and material are ready so scripts can continue
                ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
            }
            else
            {
                // The material is ready but the model isn't ready yet; wait until it's ready before allowing scripts to continue
                m_meshChangedHandler = AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler
                    {
                        [](AZ::Data::Instance<AZ::RPI::Model> model) { ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript); }
                    };
                GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_meshHandle, m_meshChangedHandler);
            }
        }
    }

    MaterialHotReloadTestComponent::ShaderVariantStatus MaterialHotReloadTestComponent::GetShaderVariantStatus() const
    {
        ShaderVariantStatus shaderVariantStatus = ShaderVariantStatus::None;

        if (m_material)
        {
            const ShaderVariantId variantId = m_material->GetShaderCollection()[0].GetShaderVariantId();
            auto searchResult = m_material->GetShaderCollection()[0].GetShaderAsset()->FindVariantStableId(variantId);
            if (searchResult.IsFullyBaked())
            {
                shaderVariantStatus = ShaderVariantStatus::FullyBaked;
            }
            else if (searchResult.IsRoot())
            {
                shaderVariantStatus = ShaderVariantStatus::Root;
            }
            else
            {
                shaderVariantStatus = ShaderVariantStatus::PartiallyBaked;
            }
        }

        return shaderVariantStatus;
    }

    void MaterialHotReloadTestComponent::OnTick([[maybe_unused]] float deltaTime, ScriptTimePoint /*scriptTime*/)
    {
        if (m_initStatus == InitStatus::ClearingTestAssets)
        {
            m_clearAssetsTimeout -= deltaTime;

            Data::AssetId materialAssetId = GetAssetId(Products::MaterialFilePath);
            Data::AssetId materialTypeAssetId = GetAssetId(Products::MaterialTypeFilePath);
            Data::AssetId shaderAssetId = GetAssetId(Products::ShaderFilePath);

            bool proceed = !materialAssetId.IsValid() && !materialTypeAssetId.IsValid() && !shaderAssetId.IsValid();

            if (!proceed && m_clearAssetsTimeout < 0)
            {
                // There was a specific bug in the Asset Processor where deleting a source file does not always remove the corresponding product
                // from the cache and from the asset database.
                AZ_Error("MaterialHotReloadTestComponent", false, "Timed out while waiting for test assets to be removed.");
                proceed = true;
            }

            if (proceed)
            {
                // [GFX TODO] [ATOM-5899] Once this ticket is addressed, This block can call all required CopyTestFile() at once,
                //            and the states InitStatus::CopyingDefault***TestFile won't be needed.
                // Any stale assets have been cleared, now we can create the new ones.
                // Copy a default set files into the temp folder.
                CopyTestFile(TestData::AzslFileNames::HorizontalPattern, Sources::AzslFileName);
                CopyTestFile(TestData::ShaderFileNames::BlendingOff, Sources::ShaderFileName);
                m_initStatus = InitStatus::CopyingDefaultShaderTestFile;
            }
        }

        if (m_initStatus == InitStatus::CopyingDefaultShaderTestFile)
        {
            AzFramework::AssetSystem::AssetStatus status = GetTestAssetStatus(Sources::ShaderFileName);
            if (status == AzFramework::AssetSystem::AssetStatus::AssetStatus_Compiled)
            {
                CopyTestFile(TestData::MaterialTypeFileNames::StraightLines, Sources::MaterialTypeFileName);
                m_initStatus = InitStatus::CopyingDefaultMaterialTypeTestFile;
            }
        }
        else if (m_initStatus == InitStatus::CopyingDefaultMaterialTypeTestFile)
        {
            AzFramework::AssetSystem::AssetStatus status = GetTestAssetStatus(Sources::MaterialTypeFileName);
            if (status == AzFramework::AssetSystem::AssetStatus::AssetStatus_Compiled)
            {
                CopyTestFile(TestData::MaterialFileNames::Default, Sources::MaterialFileName);
                m_initStatus = InitStatus::WaitingForDefaultMaterialToRegister;
            }
        }

        if (m_initStatus == InitStatus::WaitingForDefaultMaterialToRegister)
        {
            Data::AssetId materialAssetId = GetAssetId(Products::MaterialFilePath);
            if (materialAssetId.IsValid())
            {
                m_initStatus = InitStatus::WaitingForDefaultMaterialToLoad;
                Data::AssetBus::Handler::BusConnect(materialAssetId);
                m_materialAsset.Create(materialAssetId, true);
            }
        }

        auto shaderVariantStatus = GetShaderVariantStatus();
        if (shaderVariantStatus == ShaderVariantStatus::None)
        {
            m_meshFeatureProcessor->ReleaseMesh(m_shaderVariantIndicatorMeshHandle);
            m_shaderVariantIndicatorMaterial_current.reset();
        }
        else
        {
            bool updateIndicatorMesh = false;
            if (shaderVariantStatus == ShaderVariantStatus::Root)
            {
                if (m_shaderVariantIndicatorMaterial_current != m_shaderVariantIndicatorMaterial_root)
                {
                    m_shaderVariantIndicatorMaterial_current = m_shaderVariantIndicatorMaterial_root;
                    updateIndicatorMesh = true;
                }
            }
            else if(shaderVariantStatus == ShaderVariantStatus::FullyBaked)
            {
                if (m_shaderVariantIndicatorMaterial_current != m_shaderVariantIndicatorMaterial_fullyBaked)
                {
                    m_shaderVariantIndicatorMaterial_current = m_shaderVariantIndicatorMaterial_fullyBaked;
                    updateIndicatorMesh = true;
                }
            }
            else
            {
                AZ_Assert(false, "Unsupported ShaderVariantStatus");
            }

            if (updateIndicatorMesh)
            {
                m_meshFeatureProcessor->ReleaseMesh(m_shaderVariantIndicatorMeshHandle);
                m_shaderVariantIndicatorMeshHandle = m_meshFeatureProcessor->AcquireMesh(Render::MeshHandleDescriptor{ m_modelAsset }, m_shaderVariantIndicatorMaterial_current);
                m_meshFeatureProcessor->SetTransform(m_shaderVariantIndicatorMeshHandle, m_shaderVariantIndicatorMeshTransform,
                    m_shaderVariantIndicatorMeshNonUniformScale);
            }
        }

        if (m_imguiSidebar.Begin())
        {
            if (m_initStatus != InitStatus::Ready)
            {
                ImGui::Text("Waiting for assets...");
                ImGui::Separator();
            }

            ImGui::Text(Sources::MaterialFileName);
            ImGui::Indent();
            {
                DrawAssetStatus(Sources::MaterialFileName);

                if (m_initStatus == InitStatus::Ready)
                {
                    if (ScriptableImGui::Button("Default Colors"))
                    {
                        CopyTestFile(TestData::MaterialFileNames::Default, Sources::MaterialFileName);
                    }

                    if (ScriptableImGui::Button("ColorA = Red"))
                    {
                        CopyTestFile(TestData::MaterialFileNames::ChangePrimaryToRed, Sources::MaterialFileName);
                    }

                    if (ScriptableImGui::Button("ColorA = Blue"))
                    {
                        CopyTestFile(TestData::MaterialFileNames::ChangePrimaryToBlue, Sources::MaterialFileName);
                    }
                }
            }
            ImGui::Unindent();

            ImGui::Text(Sources::MaterialTypeFileName);
            ImGui::Indent();
            {
                DrawAssetStatus(Sources::MaterialTypeFileName);

                if (m_initStatus == InitStatus::Ready)
                {
                    if (ScriptableImGui::Button("Straight Lines"))
                    {
                        CopyTestFile(TestData::MaterialTypeFileNames::StraightLines, Sources::MaterialTypeFileName);
                    }

                    if (ScriptableImGui::Button("Wavy Lines"))
                    {
                        CopyTestFile(TestData::MaterialTypeFileNames::WavyLines, Sources::MaterialTypeFileName);
                    }
                }
            }
            ImGui::Unindent();

            ImGui::Text(Sources::ShaderFileName);
            ImGui::Indent();
            {
                DrawAssetStatus(Sources::ShaderFileName);

                if (m_initStatus == InitStatus::Ready)
                {
                    if (ScriptableImGui::Button("Blending Off"))
                    {
                        CopyTestFile(TestData::ShaderFileNames::BlendingOff, Sources::ShaderFileName);
                    }

                    if (ScriptableImGui::Button("Blending On"))
                    {
                        CopyTestFile(TestData::ShaderFileNames::BlendingOn, Sources::ShaderFileName);
                    }
                }
            }
            ImGui::Unindent();

            ImGui::Text(Sources::AzslFileName);
            ImGui::Indent();
            {
                // The AZSL file is a source dependency of the .shader file, so display the same asset status
                DrawAssetStatus(Sources::AzslFileName);

                if (m_initStatus == InitStatus::Ready)
                {
                    if (ScriptableImGui::Button("Horizontal Pattern"))
                    {
                        CopyTestFile(TestData::AzslFileNames::HorizontalPattern, Sources::AzslFileName);
                    }

                    if (ScriptableImGui::Button("Vertical Pattern"))
                    {
                        CopyTestFile(TestData::AzslFileNames::VerticalPattern, Sources::AzslFileName);
                    }
                }
            }
            ImGui::Unindent();

            ImGui::Text(Sources::ShaderVariantListFileName);
            ImGui::Indent();
            {
                // The AZSL file is a source dependency of the .shader file, so display the same asset status
                DrawAssetStatus(Sources::ShaderVariantListFileName, false);
                DrawAssetStatus(Products::ShaderVariantTreeFileName, true);
                DrawAssetStatus(Products::GetShaderVariantFileName(ShaderVariantStableId{0}).c_str(), true);
                DrawAssetStatus(Products::GetShaderVariantFileName(ShaderVariantStableId{1}).c_str(), true);
                DrawAssetStatus(Products::GetShaderVariantFileName(ShaderVariantStableId{2}).c_str(), true);

                if (m_initStatus == InitStatus::Ready)
                {
                    ScriptableImGui::PushNameContext("ShaderVariantList");

                    if (ScriptableImGui::Button("None"))
                    {
                        DeleteTestFile(Sources::ShaderVariantListFileName);
                    }

                    if (ScriptableImGui::Button("All"))
                    {
                        CopyTestFile(TestData::ShaderVariantListFileNames::All, Sources::ShaderVariantListFileName);
                    }

                    if (ScriptableImGui::Button("Only Wavy Lines"))
                    {
                        CopyTestFile(TestData::ShaderVariantListFileNames::OnlyWavyLines, Sources::ShaderVariantListFileName);
                    }

                    if (ScriptableImGui::Button("Only Straight Lines"))
                    {
                        CopyTestFile(TestData::ShaderVariantListFileNames::OnlyStraightLines, Sources::ShaderVariantListFileName);
                    }

                    ScriptableImGui::PopNameContext();
                }
            }
            ImGui::Unindent();

            m_imguiSidebar.End();
        }

    }

} // namespace AtomSampleViewer
