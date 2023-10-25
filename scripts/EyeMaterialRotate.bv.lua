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

--[[
    This is not a formal automation test. Instead, it is a helper/utility
    script that can be used to automatically change the Heading angle of the
    ArcBall camera, and being able to appreciate the Eye mesh being
    rendered with the Eye material from different angles.

    In particular, it was used to capture videos of the original Eye shader vs
    the updated version.
]]

-- Animates the Heading angle of the ArcBall camera starting from @startHeadingDegrees
-- until it reaches @stopHeadingDegrees in @numSteps. At each step (delta angle) it waits
-- @waitTimePerStepSeconds seconds before changing to the next angle.
function MyAnimateCamera(startHeadingDegrees, stopHeadingDegrees, numSteps, waitTimePerStepSeconds)
    local headingDegrees = startHeadingDegrees
    local deltaDegrees = (stopHeadingDegrees - startHeadingDegrees) / numSteps
    local stepCount = numSteps+1
    for step=1, stepCount  do
        ArcBallCameraController_SetHeading(DegToRad(headingDegrees));
        IdleSeconds(waitTimePerStepSeconds);
        headingDegrees = headingDegrees + deltaDegrees
    end
end

OpenSample('Features/EyeMaterial')
-- in 18 steps of 10 degrees each, rotate the camera
-- from 90 degrees  to 270 degrees. at each step it will wait
-- 1 second.
MyAnimateCamera(90.0, 270.0, 18, 1.0)

-- Here are two examples of leaving the camera at a particular angle.
-- MyAnimateCamera(129.6, 129.6, 1, 1.0)
-- MyAnimateCamera(90.0, 90.0, 1, 1.0)
