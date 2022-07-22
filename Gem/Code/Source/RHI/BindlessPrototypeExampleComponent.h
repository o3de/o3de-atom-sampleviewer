/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI.Reflect/Handle.h>
#include <Atom/RHI/PipelineState.h>

#include <Atom/RPI.Public/Model/ModelLod.h>
#include <Atom/RPI.Public/Model/Model.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Random.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <RHI/BasicRHIComponent.h>

#include <Utils/ImGuiSidebar.h>

namespace AZ
{
    namespace RHI
    {
        class BufferView;
    };
};

namespace AtomSampleViewer
{
    
    //! This sample tests a naive approach of achieving bindless resources.
    //! This sample utilizes a FloatBuffer, which is a StructuredBuffer with the element type of Float, which
    //! is 4-byte aligned.
    //! All data that is used in this sample is allocated in this buffer, like: materials, objects, 
    //! transforms, etc. This allows for various types of data to be stored within the same buffer. The data
    //! is accessible with handles, which acts as an offset within the FloatBuffer to access data.
    //!
    //! The texture views are stored in a descriptor which holds an image view array. 
    //!
    class BindlessPrototypeExampleComponent final
        : public BasicRHIComponent
        , public AZ::TickBus::Handler
    {
    public:

        // Helper structure to manage multiple SRGs
        struct BindlessSrg 
        {
            BindlessSrg(AZ::Data::Instance<AZ::RPI::Shader> shader, AZStd::vector<const char*> srgArray);
            ~BindlessSrg();

            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> GetSrg(const AZStd::string srgName);
            void CompileSrg(const AZStd::string srgName);
            bool SetBufferView(const AZStd::string srgName, const AZStd::string srgId, const AZ::RHI::BufferView* bufferView);

        private:
            AZStd::unordered_map<AZ::Name, AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>> m_srgMap;
        };

        // Handle that has the offset into the FloatBuffer
        using FloatBufferHandle = AZ::RHI::Handle<uint32_t>;

        // Per object data
        struct PerObject
        {
            AZ::Matrix4x4 m_localToWorldMatrix;
        };

        // Intermediate structure that holds a range of indices of submesh instances, which represents a mesh instance
        struct ObjectInterval
        {
            // Sub-mesh range
            uint32_t m_min = 0u;
            uint32_t m_max = 0u;

            // Object index
            FloatBufferHandle m_objectHandle;
        };

        // Simple linear allocated buffer of floats
        struct FloatBuffer
        {
            static const uint32_t FloatSizeInBytes = static_cast<uint32_t>(sizeof(float));
        public:
            FloatBuffer(AZ::RHI::Ptr<AZ::RHI::BufferPool> bufferPool, const uint32_t sizeInBytes);
            ~FloatBuffer();

            // Allocates data if the provided handle is empty, else it updates it
            bool AllocateOrUpdateBuffer(FloatBufferHandle& handle, const void* data, const uint32_t sizeInBytes);

            // Allocates data on the FloatBuffer
            bool AllocateFromBuffer(FloatBufferHandle& handle, const void* data, const uint32_t sizeInBytes);

            // Updates already existing data on the FloatBuffer
            bool UpdateBuffer(const FloatBufferHandle& handle, const void* data, const uint32_t sizeInBytes);

            // Maps host data to the device
            bool MapData(const AZ::RHI::BufferMapRequest& mapRequest, const void* data);

            // Create the buffer
            void CreateBufferFromPool(const uint32_t byteCount);

            // Buffer resource
            AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool = nullptr;
            AZ::RHI::Ptr<AZ::RHI::Buffer> m_buffer = nullptr;

            // Number of bytes that are allocated
            uint32_t m_allocatedInBytes = 0;

            // Total available data in bytes
            uint32_t m_totalSizeInBytes = 0;
        };

        // Simple intermediate structure that represents a submesh instance
        struct SubMeshInstance
        {
            AZ::RHI::ShaderInputNameIndex m_viewHandleIndex = "m_perViewHandle";
            AZ::RHI::ShaderInputNameIndex m_objecHandleIndex = "m_perObjectHandle";
            AZ::RHI::ShaderInputNameIndex m_materialHandleIndex = "m_materialHandle";
            AZ::RHI::ShaderInputNameIndex m_lightHandleIndex = "m_lightHandle";

            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_perSubMeshSrg;

            const AZ::RPI::ModelLod::Mesh* m_mesh;
            AZ::RPI::ModelLod::StreamBufferViewList bufferStreamViewArray;

            AZ::Matrix4x4 m_modelMatrix;
        };

    public:
        AZ_COMPONENT(BindlessPrototypeExampleComponent, "{E8AE0BC3-E076-4966-B01D-ED237E7D00E1}", BasicRHIComponent);
        AZ_DISABLE_COPY(BindlessPrototypeExampleComponent);

        BindlessPrototypeExampleComponent();
        ~BindlessPrototypeExampleComponent() final = default;

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component overrides...
        void Activate() override;
        void Deactivate() override;

        // AZ::RPI::RHISystemNotificationBus::Handler
        void OnFramePrepare(AZ::RHI::FrameGraphBuilder& frameGraphBuilder) final;

    private:
        // AZ::TickBus::Handler overrides...
        void OnTick(float deltaTime, AZ::ScriptTimePoint scriptTime) final;
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) final;

        // Draw ImGui sidebar
        void DrawImgui();

        // Add object for rendering
        void AddObjectForRender(const PerObject& perObject);

        // Clears all objects in the scene
        void ClearObjects();

        // Creates all the objects
        void CreateObjects();

        // Creates the materials
        void CreateMaterials();

        // Helper function to allocate or update data in the FloatBuffer
        template<typename T>
        void AllocateMaterial(FloatBufferHandle& handle)
        {
            T material;
            [[maybe_unused]] bool result = m_floatBuffer->AllocateOrUpdateBuffer(handle, static_cast<void*>(&material), sizeof(T));
            AZ_Assert(result, "Failed to allocate FloatBuffer");
        }

        // Create the BufferPol
        void CreateBufferPool();

        //Recreate objects
        void Recreate();
        
        // ImGui sidebar
        ImGuiSidebar m_imguiSidebar;

        // Depth stencil used by the sample
        AZ::RHI::AttachmentId m_depthStencilID;

        // The shader and their srg 
        AZStd::unique_ptr<BindlessSrg> m_bindlessSrg;

        // Holds all float buffer data
        AZStd::unique_ptr<FloatBuffer> m_floatBuffer;

        // Perspective matrix
        AZ::Matrix4x4 m_viewToClipMatrix;

        // Camera
        AZ::EntityId m_cameraEntityId;

        // Shader used for this sample
        AZ::Data::Instance<AZ::RPI::Shader> m_shader;

        // Indices of the sub meshes that 
        AZStd::vector<ObjectInterval> m_objectIntervalArray;

        // NOTE: Assume the same pipeline for every draw item
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_pipelineState;

        // Handle holding the world matrix
        FloatBufferHandle m_worldToClipHandle;

        // Handle array holding the FloatBuffer handles for all objects
        AZStd::vector<FloatBufferHandle> m_objectHandleArray;

        // Image array holding all of the StreamImages
        AZStd::vector<AZ::Data::Instance<AZ::RPI::StreamingImage>> m_images;

        AZStd::vector<const AZ::RHI::ImageView*> m_imageViews;

        // Light direction handle
        FloatBufferHandle m_lightDirectionHandle;

        // Light direction
        AZ::Vector3 m_lightDir;
        float m_lightAngle = 0.0f;

        // Mesh dimension count
        int32_t m_objectCountWidth = 1;
        int32_t m_objectCountHeight = 1;
        int32_t m_objectCountDepth = 1;
        static constexpr float m_meshPositionOffset = 6.0f;

        // Array of sub mesh instances
        AZStd::vector<SubMeshInstance> m_subMeshInstanceArray;

        // The model used for this example
        AZ::Data::Instance<AZ::RPI::Model> m_model = nullptr;

        // BufferPool used to allocate buffers in this example
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_bufferPool = nullptr;

        // Indirection buffer holding uint indices of texture resources
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_indirectionBuffer = nullptr;

        // View associated with the indirection buffer
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_indirectionBufferView = nullptr;
        
        // Pool size in bytes, 1MB
        static constexpr uint32_t m_poolSizeInBytes = 1u << 20u;

        // Every render-able mesh object (All SubMeshes)
        AZStd::vector<FloatBufferHandle> m_materialHandleArray;
        static constexpr uint32_t m_modelLod = 0u;

        AZ::RHI::ScopeId m_scopeId;
        static constexpr const char* m_samplerSrgName = "SamplerSrg";
        static constexpr const char* m_floatBufferSrgName = "FloatBufferSrg";
        static constexpr const char* m_indirectionBufferSrgName = "IndirectionBufferSrg";
        static constexpr const char* m_imageSrgName = "ImageSrg";

        // Material count
        static constexpr uint32_t m_materialCount = 1024u;
        // FloatBuffer Size, float count
        static constexpr uint32_t m_bufferFloatCount = 163840u;
        // Maximum object per axis
        static constexpr int32_t m_maxObjectPerAxis = 10u;
        // Total object count
        static constexpr uint32_t m_objectCount = m_maxObjectPerAxis * m_maxObjectPerAxis * m_maxObjectPerAxis;
    };

} // namespace AtomSampleViewer
