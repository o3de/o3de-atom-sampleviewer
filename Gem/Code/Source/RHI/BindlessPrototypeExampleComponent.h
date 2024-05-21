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

#include <BindlessPrototype_Traits_Platform.h>

namespace AZ
{
    namespace RHI
    {
        class BufferView;
    };
};

namespace AtomSampleViewer
{
    
    //! This sample tests various resource types: read only textures, read only buffers, read only cube 
    //! map textures, read write textures and read write buffers.
    //! This sample utilizes a FloatBuffer, which is a StructuredBuffer with the element type of Float, which
    //! is 4-byte aligned.
    //! All data that is used in this sample is allocated in this buffer, like: materials, objects, 
    //! transforms, etc. This allows for various types of data to be stored within the same buffer. The data
    //! is accessible with handles, which acts as an offset within the FloatBuffer to access data.
    //!
    //! All the bindless heap indices for views to various resource types are passed in via an indirect buffer.
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
            AZ::RHI::ShaderInputNameIndex m_uvBufferHandleIndex = "m_uvBufferIndex";
            AZ::RHI::ShaderInputNameIndex m_uvBufferByteOffsetHandleIndex = "m_uvBufferByteOffset";

            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_perSubMeshSrg;

            const AZ::RPI::ModelLod::Mesh* m_mesh;
            AZ::RPI::ModelLod::StreamBufferViewList bufferStreamViewArray;

            AZ::Matrix4x4 m_modelMatrix;

            uint32_t m_uvBufferIndex = 0;
            uint32_t m_uvBufferByteOffset = 0;
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

        // Create read only buffers that has color values
        void CreateColorBuffer(
            const AZ::Name& bufferName,
            const AZ::Vector4& colorVal,
            AZ::RHI::Ptr<AZ::RHI::Buffer>& buffer,
            AZ::RHI::Ptr<AZ::RHI::BufferView>& bufferView);

        // Create indirect buffer that will contain index of the view into the bindless heap
        void CreateIndirectBuffer(
            const AZ::Name& bufferName,
            AZ::RHI::Ptr<AZ::RHI::Buffer>& indirectionBuffer,
            AZ::RHI::Ptr<AZ::RHI::BufferView>& bufferView,
            size_t byteSize);

        // Helper function to allocate or update data in the FloatBuffer
        template<typename T>
        void AllocateMaterial(FloatBufferHandle& handle)
        {
            T material;
            [[maybe_unused]] bool result = m_floatBuffer->AllocateOrUpdateBuffer(handle, static_cast<void*>(&material), sizeof(T));
            AZ_Assert(result, "Failed to allocate FloatBuffer");
        }

        // Create all the needed pools
        void CreatePools();

        // Recreate objects
        void Recreate();
        
        // Load compute shaders related to writing to rwtexture and rwbuffer
        void LoadComputeShader(const char* shaderFilePath);
        void LoadComputeShaders();

        // Create compute passes for writing to rwbuffer and rwtexture
        void CreateBufferComputeScope();
        void CreateImageComputeScope();

        // Create the pass that will read all the various resources and use them to render meshes via a raster pass
        void CreateBindlessScope();

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
        // Image array holding all of the Stream cubemap images
        AZStd::vector<AZ::Data::Instance<AZ::RPI::StreamingImage>> m_cubemapImages;

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
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_imageIndirectionBuffer = nullptr;
        // Indirection buffer holding uint indices of buffer resources
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_bufferIndirectionBuffer = nullptr;
        // View associated with the buffer holding indices to bindless images
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_imageIndirectionBufferView = nullptr;
        // View associated with the buffer holding indices to bindless buffers
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_bufferIndirectionBufferView = nullptr;

        // Color buffer holding color related floats
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_colorBuffer1 = nullptr;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_colorBuffer2 = nullptr;
        // Views related to the buffers declared above
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_colorBuffer1View = nullptr;
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_colorBuffer2View = nullptr;

        // Thread count for compute shaders.
        int m_bufferNumThreadsX = 1;
        int m_bufferNumThreadsY = 1;
        int m_bufferNumThreadsZ = 1;

        int m_imageNumThreadsX = 1;
        int m_imageNumThreadsY = 1;
        int m_imageNumThreadsZ = 1;

        // Pool size in bytes, 1MB
        static constexpr uint32_t m_poolSizeInBytes = 1u << 20u;

        // Every render-able mesh object (All SubMeshes)
        AZStd::vector<FloatBufferHandle> m_materialHandleArray;
        static constexpr uint32_t m_modelLod = 0u;

        AZ::RHI::ScopeId m_scopeId;
        static constexpr const char* m_samplerSrgName = "SamplerSrg";
        static constexpr const char* m_floatBufferSrgName = "FloatBufferSrg";
        static constexpr const char* m_indirectionBufferSrgName = "IndirectionBufferSrg";
#if ATOMSAMPLEVIEWER_TRAIT_BINDLESS_PROTOTYPE_SUPPORTS_DIRECT_BOUND_UNBOUNDED_ARRAY
        static constexpr const char* m_imageSrgName = "ImageSrg";
#endif

        // Material count
        static constexpr uint32_t m_materialCount = 1024u;
        // FloatBuffer Size, float count
        static constexpr uint32_t m_bufferFloatCount = 163840u;
        // Maximum object per axis
        static constexpr int32_t m_maxObjectPerAxis = 10u;
        // Total object count
        static constexpr uint32_t m_objectCount = m_maxObjectPerAxis * m_maxObjectPerAxis * m_maxObjectPerAxis;

        // Compute pass related PSOs
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_bufferDispatchPipelineState;
        AZ::RHI::ConstPtr<AZ::RHI::PipelineState> m_imageDispatchPipelineState;
        
        // Compute pass related buffer pool which will create a rwbuffer
        AZ::RHI::Ptr<AZ::RHI::BufferPool> m_computeBufferPool;
        AZ::RHI::Ptr<AZ::RHI::Buffer> m_computeBuffer;
        AZ::RHI::Ptr<AZ::RHI::BufferView> m_computeBufferView;
        AZ::RHI::BufferViewDescriptor m_rwBufferViewDescriptor;

         // Compute pass related image pool which will create a rwimage
        AZ::RHI::Ptr<AZ::RHI::ImagePool> m_rwImagePool;
        AZ::RHI::Ptr<AZ::RHI::Image> m_computeImage;
        AZ::RHI::Ptr<AZ::RHI::ImageView> m_computeImageView;
        AZ::RHI::ImageViewDescriptor m_rwImageViewDescriptor;

        // Compute pass related SRGs
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_bufferDispatchSRG;
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_imageDispatchSRG;
        AZ::RHI::AttachmentId m_bufferAttachmentId = AZ::RHI::AttachmentId("bufferAttachmentId");
        AZ::RHI::AttachmentId m_imageAttachmentId = AZ::RHI::AttachmentId("imageAttachmentId");
        
    };

} // namespace AtomSampleViewer
