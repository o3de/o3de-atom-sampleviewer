----------------------------------------------------------------------------------------------------
--
-- Copyright (c) Contributors to the Open 3D Engine Project
-- 
-- SPDX-License-Identifier: Apache-2.0 OR MIT
--
--
--
----------------------------------------------------------------------------------------------------

-- "Periodic" suite tests for the automated review process.
-- These tests run as part of the periodic suite of tests and do not block any code submissions.
-- The purpose of this periodic test is to ensure the "_FullTestSuite_.bv.lua" script continues to function.
-- This is because if no developer ran the "_FullTestSuite_.bv.lua" script locally we would not know of any failures, so this script ensures we catch them.

RunScript('scripts/_FullTestSuite_.bv.lua')
