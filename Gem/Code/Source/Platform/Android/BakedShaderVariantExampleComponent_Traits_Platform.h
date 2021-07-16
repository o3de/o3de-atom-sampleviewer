/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// [ATOM-15361] Mobile pipeline uses no-msaa forward pass, need to differentiate this while getting the pass in the sample
#define ATOMSAMPLEVIEWER_TRAIT_BAKED_SHADERVARIANT_SAMPLE_PASS_NAME     "ForwardPass"