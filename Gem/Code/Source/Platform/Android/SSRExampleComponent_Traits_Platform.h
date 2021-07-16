/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Mali devices have a memory limitation of vertex buffer (180MB), so low resolution mesh model is needed.
// https://community.arm.com/developer/tools-software/graphics/b/blog/posts/memory-limits-with-vulkan-on-mali-gpus
// [ATOM-14947]
#define ATOMSAMPLEVIEWER_TRAIT_SSR_SAMPLE_LUCY_MODEL_NAME                       "objects/lucy/lucy_low.azmodel"
