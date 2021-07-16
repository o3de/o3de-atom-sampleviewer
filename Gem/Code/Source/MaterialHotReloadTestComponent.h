/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CommonSampleComponentBase.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/FileIOErrorHandler.h>
#include <AzCore/Component/TickBus.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <AzFramework/Asset/AssetSystemTypes.h>

namespace AtomSampleViewer
{
    //! This test renders a simple material and exposes controls that can update the source data for that material and its shaders
    //! to demonstrate and test hot-reloading. It works by copying entire files from a test data folder into a material source folder
    //! and waiting for the Asset Processor to build the updates files.
    class MaterialHotReloadTestComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
        , public AZ::Data::AssetBus::Handler
    {
    public:
        AZ_COMPONENT(MaterialHotReloadTestComponent, "{EA684B21-9E39-4210-A640-AFBC28B2E683}", CommonSampleComponentBase);
        MaterialHotReloadTestComponent();

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

    private:

        AZ_DISABLE_COPY_MOVE(MaterialHotReloadTestComponent);

        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) override;
        
        // Finds the paths m_testDataFolder and m_tempSourceFolder
        void InitTestDataFolders();

        // Deletes a file from m_tempSourceFolder
        void DeleteTestFile(const char* tempSourceFile);

        // Copies a file from m_testDataFolder to m_tempSourceFolder
        void CopyTestFile(const AZStd::string& testDataFile, const AZStd::string& tempSourceFile);

        // Returns the AssetStatus of a file in m_tempSourceFolder 
        AzFramework::AssetSystem::AssetStatus GetTestAssetStatus(const char* tempSourceFile) const;

        // Draws ImGui indicating the Asset Processor status of a file in m_tempSourceFolder 
        void DrawAssetStatus(const char* tempSourceFile, bool includeFileName = false);

        AZ::Data::AssetId GetAssetId(const char* productFilePath);

        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        enum class ShaderVariantStatus
        {
            None,
            Root,
            PartiallyBaked,
            FullyBaked
        };

        ShaderVariantStatus GetShaderVariantStatus() const;

        static constexpr float LongTimeout = 30.0f;

        // Tracks initialization that starts when the component is activated
        enum class InitStatus
        {
            None,
            ClearingTestAssets,
            CopyingDefaultShaderTestFile,
            CopyingDefaultMaterialTypeTestFile,
            WaitingForDefaultMaterialToRegister,
            WaitingForDefaultMaterialToLoad,
            Ready
        };
        InitStatus m_initStatus = InitStatus::None;

        AZStd::string m_testDataFolder;   //< Stores several txt files with contents to be copied over various source asset files.
        AZStd::string m_tempSourceFolder; //< Folder for temp source asset files. These are what the sample edits and reloads.

        ImGuiSidebar m_imguiSidebar;

        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;

        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZ::Data::Asset<AZ::RPI::ModelAsset> m_modelAsset;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_meshChangedHandler;

        // These are used to render a secondary mesh that indicates which shader variant is being used to render the primary mesh
        AZ::Transform m_shaderVariantIndicatorMeshTransform;
        AZ::Vector3 m_shaderVariantIndicatorMeshNonUniformScale = AZ::Vector3::CreateOne();
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_shaderVariantIndicatorMeshHandle;
        AZ::Data::Instance<AZ::RPI::Material> m_shaderVariantIndicatorMaterial_root;
        AZ::Data::Instance<AZ::RPI::Material> m_shaderVariantIndicatorMaterial_fullyBaked;
        AZ::Data::Instance<AZ::RPI::Material>  m_shaderVariantIndicatorMaterial_current;

        FileIOErrorHandler m_fileIoErrorHandler;

        float m_clearAssetsTimeout = 0.0f;
    };
} // namespace AtomSampleViewer
