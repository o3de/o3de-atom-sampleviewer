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

-- "Main" suite tests for the automated review process.
-- These tests run on the nightly builds currently due to GPU node availability.
-- In the future when we have more GPU nodes, these tests will run as part of the gating process for automated review.
-- Failures in that gating process will block submission of your code in the automated review process and require a bug to be filed.

RunScript('scripts/decals.bv.luac')
RunScript('scripts/dynamicdraw.bv.luac')
RunScript('scripts/dynamicmaterialtest.bv.luac')
RunScript('scripts/materialscreenshottests.bv.luac')
RunScript('scripts/materialhotreloadtest.bv.luac')
RunScript('scripts/msaa_rpi_test.bv.luac')
RunScript('scripts/cullingandlod.bv.luac')
RunScript('scripts/multirenderpipeline.bv.luac')
RunScript('scripts/lightculling.bv.luac')
RunScript('scripts/transparenttest.bv.luac')
RunScript('scripts/streamingimagetest.bv.luac')
RunScript('scripts/parallaxtest.bv.luac')
RunScript('scripts/checkerboardtest.bv.luac')
RunScript('scripts/scenereloadsoaktest.bv.luac')
RunScript('scripts/diffusegitest.bv.luac')
RunScript('scripts/arealighttest.bv.luac')
RunScript('scripts/multiscene.bv.luac')
RunScript('scripts/shadowtest.bv.luac')
RunScript('scripts/shadowedsponzatest.bv.luac')
RunScript('scripts/PassTree.bv.luac')
