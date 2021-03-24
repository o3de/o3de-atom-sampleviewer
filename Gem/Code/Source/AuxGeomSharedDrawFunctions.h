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

#pragma once

#include <Atom/RPI.Public/AuxGeom/AuxGeomDraw.h>

namespace AtomSampleViewer
{
    // Create some semi-transparent colors
    extern const AZ::Color BlackAlpha;
    extern const AZ::Color WhiteAlpha;

    extern const AZ::Color RedAlpha;
    extern const AZ::Color GreenAlpha;
    extern const AZ::Color BlueAlpha;

    extern const AZ::Color YellowAlpha;
    extern const AZ::Color CyanAlpha;
    extern const AZ::Color MagentaAlpha;

    extern const AZ::Color LightGray;
    extern const AZ::Color DarkGray;

    void DrawBackgroundBox(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawThreeGridsOfPoints(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawAxisLines(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawLines(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawTriangles(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawShapes(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawBoxes(AZ::RPI::AuxGeomDrawPtr auxGeom, float x = 10.0f);
    void DrawManyPrimitives(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void DrawDepthTestPrimitives(AZ::RPI::AuxGeomDrawPtr auxGeom);
    void Draw2DWireRect(AZ::RPI::AuxGeomDrawPtr auxGeom, const AZ::Color& color, float z = 0.99f);
} // namespace AtomSampleViewer
