/*
 * Copyright (c) Contributors to the Open 3D Engine Project
 * 
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RPI.Public/Material/Material.h>

namespace AtomSampleViewer
{
    namespace ImGuiShaderUtils
    {
        //! Draws an ImGui table that compares the shader options for a shader variant that was requested and the shader variant that was found.
        void DrawShaderVariantTable(const AZ::RPI::ShaderOptionGroupLayout* layout, AZ::RPI::ShaderVariantId requestedVariantId, AZ::RPI::ShaderVariantId selectedVariantId);
    } // namespace Utils
} // namespace AtomSampleViewer
