----------------------------------------------------------------------------------------------------
--
-- All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
-- its licensors.
--
-- For complete copyright and license terms please see the LICENSE at the root of this
-- distribution (the "License"). All use of this software is governed by the License,
-- or, if provided, by the license below or the license accompanying this file. Do not
-- remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--
--
----------------------------------------------------------------------------------------------------

-- "Main" suite tests for the automated review process.
-- These tests run on the nightly builds currently due to insufficient GPU node count (see SPEC-6031).
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
RunScript('scripts/shadowedbistrotest.bv.luac')
RunScript('scripts/PassTree.bv.luac')
