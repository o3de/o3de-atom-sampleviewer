/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// [ATOM-15367] On more limited devices the large lattice size would run into out of memory error, specifically "device lost" error
#define ATOMSAMPLEVIEWER_TRAIT_SCENE_RELOAD_SOAK_TEST_COMPONENT_LATTICE_SIZE       2
