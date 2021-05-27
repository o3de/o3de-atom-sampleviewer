/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

// Mali devices have a memory limitation of vertex buffer (180MB), so low resolution mesh model is needed.
// https://community.arm.com/developer/tools-software/graphics/b/blog/posts/memory-limits-with-vulkan-on-mali-gpus
#define ENTITY_LATTICE_TEST_COMPONENT_WIDTH                     2
#define ENTITY_LATTICE_TEST_COMPONENT_HEIGHT                    2
#define ENTITY_LATTICE_TEST_COMPONENT_DEPTH                     2
#define ENTITY_LATTEST_TEST_COMPONENT_MAX                       4
