/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "AuxGeomSharedDrawFunctions.h"

#include <AzCore/base.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Obb.h>

#include <AzCore/Casting/numeric_cast.h>

namespace AtomSampleViewer
{
    using namespace AZ;
    using namespace RPI;
    using namespace Colors;

    // Create some semi-transparent colors
    const AZ::Color BlackAlpha     (0.0f, 0.0f, 0.0f, 0.5f);
    const AZ::Color WhiteAlpha     (1.0f, 1.0f, 1.0f, 0.5f);

    const AZ::Color RedAlpha       (1.0f, 0.0f, 0.0f, 0.5f);
    const AZ::Color GreenAlpha     (0.0f, 1.0f, 0.0f, 0.5f);
    const AZ::Color BlueAlpha      (0.0f, 0.0f, 1.0f, 0.5f);

    const AZ::Color YellowAlpha    (0.5f, 0.5f, 0.0f, 0.5f);
    const AZ::Color CyanAlpha      (0.0f, 0.5f, 0.5f, 0.5f);
    const AZ::Color MagentaAlpha   (0.5f, 0.0f, 0.5f, 0.5f);

    const AZ::Color LightGray      (0.8f, 0.8f, 0.8, 1.0f);
    const AZ::Color DarkGray       (0.2f, 0.2f, 0.2, 1.0f);

    void DrawBackgroundBox(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        // Draw a big cube using DrawTriangles to create a background for the other tests.
        // Use triangles rather than an AABB because triangles have back-face culling disabled.
        // All other test geometries are drawn inside this big cube.
        float cubeHalfWidth = 80.0f;
        float left = -cubeHalfWidth;
        float right = cubeHalfWidth;
        float top = cubeHalfWidth;
        float bottom = -cubeHalfWidth;
        float front = cubeHalfWidth;
        float back = -cubeHalfWidth;
        const uint32_t NumCubePoints = 8;
        AZ::Vector3 cubePoints[NumCubePoints] =
        {
            AZ::Vector3(left, front, top), AZ::Vector3(right, front, top), AZ::Vector3(right, front, bottom), AZ::Vector3(left, front, bottom),
            AZ::Vector3(left, back, top), AZ::Vector3(right, back, top), AZ::Vector3(right, back, bottom), AZ::Vector3(left, back, bottom),
        };
        const uint32_t NumCubeIndicies = 36;
        uint32_t cubeIndicies[NumCubeIndicies] =
        {
            0, 1, 2,  2, 3, 0,  // front face
            4, 5, 6,  6, 7, 4,  // back face (no back-face culling)
            0, 3, 7,  7, 4, 0,  // left
            1, 2, 6,  6, 5, 1,  // right
            0, 1, 5,  5, 4, 0,  // top
            2, 3, 7,  7, 6, 2,  // bottom
        };

        // Make the cube dark gray on the top face, blending to light gray on the bottom face
        AZ::Color cubeColors[NumCubePoints] = { DarkGray, DarkGray, LightGray, LightGray, DarkGray, DarkGray, LightGray, LightGray };

        // Draw as opaque cube with multiple colors and shared vertices
        AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments drawArgs;
        drawArgs.m_verts = cubePoints;
        drawArgs.m_vertCount = NumCubePoints;
        drawArgs.m_indices = cubeIndicies;
        drawArgs.m_indexCount = NumCubeIndicies;
        drawArgs.m_colors = cubeColors;
        drawArgs.m_colorCount = NumCubePoints;
        drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Opaque;
        auxGeom->DrawTriangles(drawArgs);
    }

    void DrawThreeGridsOfPoints(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        AZ::u8 pointSize = 10;   // DX12 API ignores point size, works on Vulkan

        // draw three grids of points

        const uint32_t NumPlanePointsPerAxis = 16; // must be even
        const uint32_t NumPlanePoints = NumPlanePointsPerAxis * NumPlanePointsPerAxis;
        float gridHalfWidth = 0.1f;
        float gridSpacing = gridHalfWidth/aznumeric_cast<float>(NumPlanePointsPerAxis/2);
        AZ::Vector3 origin(0.0f, 0.0f, 2.0f);

        ///////////////////////////////////////////////////////////////////////
        //  1st grid of points is in plane of x = 0, draw in red
        float x, y, z;
        y = -gridHalfWidth;
        for (int yIndex = 0; yIndex < NumPlanePointsPerAxis; ++yIndex, y += gridSpacing)
        {
            z = -gridHalfWidth;
            for (int zIndex = 0; zIndex <= NumPlanePointsPerAxis; ++zIndex, z += gridSpacing)
            {
                AZ::Vector3 vert = origin + AZ::Vector3(0.0f, y, z);
                AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
                drawArgs.m_verts = &vert;
                drawArgs.m_vertCount = 1;
                drawArgs.m_colors = &Red;
                drawArgs.m_colorCount = 1;
                drawArgs.m_size = pointSize;
                auxGeom->DrawPoints(drawArgs);
            }
        }

        ///////////////////////////////////////////////////////////////////////
        // 2nd grid of points is in plane of y = 0, draw in green with one draw call
        AZ::Vector3 planePoints[NumPlanePointsPerAxis * NumPlanePointsPerAxis];
        uint32_t pointIndex = 0;
        x = -gridHalfWidth;
        for (int xIndex = 0; xIndex < NumPlanePointsPerAxis; ++xIndex, x += gridSpacing)
        {
           z = -gridHalfWidth;
           for (int zIndex = 0; zIndex < NumPlanePointsPerAxis; ++zIndex, z += gridSpacing)
           {
               planePoints[pointIndex++] = origin + AZ::Vector3(x, 0.0f, z);
           }
        }
        AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
        drawArgs.m_verts = planePoints;
        drawArgs.m_vertCount = NumPlanePoints;
        drawArgs.m_colors = &Green;
        drawArgs.m_colorCount = 1;
        drawArgs.m_size = pointSize;
        auxGeom->DrawPoints(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        // 3rd grid of points is in plane of z = 0, draw in multiple colors with one draw call
        AZ::Color pointColors[NumPlanePointsPerAxis * NumPlanePointsPerAxis];
        pointIndex = 0;
        float opacity = 0.0f;
        x = -gridHalfWidth;
        for (int xIndex = 0; xIndex < NumPlanePointsPerAxis; ++xIndex, x += gridSpacing)
        {
            y = -gridHalfWidth;
            for (int yIndex = 0; yIndex < NumPlanePointsPerAxis; ++yIndex, y += gridSpacing)
            {
                planePoints[pointIndex] = origin + AZ::Vector3(x, y, 0.0f);
                pointColors[pointIndex] = AZ::Color(0.0f, 0.0f, 1.0f, opacity);
                ++pointIndex;
                opacity += 1.0f / NumPlanePoints;
            }
        }
        drawArgs.m_verts = planePoints;
        drawArgs.m_vertCount = NumPlanePoints;
        drawArgs.m_colors = pointColors;
        drawArgs.m_colorCount = NumPlanePoints;
        drawArgs.m_size = pointSize;
        drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
        auxGeom->DrawPoints(drawArgs);
    }

    void DrawAxisLines(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        // draw a line for each axis with triangles indicating direction and spheres on the ends

        AZ::u8 lineWidth = 1;   // currently we don't support width on lines

        ///////////////////////////////////////////////////////////////////////
        // Draw three lines for the axes
        float axisLength = 30.0f;
        AZ::Vector3 verts[3] = {AZ::Vector3( -axisLength, 0, 0 ), AZ::Vector3( axisLength, 0, 0 )};
        AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
        drawArgs.m_verts = verts;
        drawArgs.m_vertCount = 2;
        drawArgs.m_colors = &Red;
        drawArgs.m_colorCount = 1;
        drawArgs.m_size = lineWidth;
        auxGeom->DrawLines(drawArgs);

        verts[0] = AZ::Vector3( 0, -axisLength, 0 ); verts[1] = AZ::Vector3( 0, axisLength, 0 );
        drawArgs.m_colors = &Green;
        auxGeom->DrawLines(drawArgs);

        verts[0] = AZ::Vector3( 0, 0, -axisLength ); verts[1] = AZ::Vector3( 0, 0, axisLength );
        drawArgs.m_colors = &Blue;
        auxGeom->DrawLines(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        // Next, draw a couple of triangles on each axis to indicate increasing direction
        float triLength = 1.0f;
        float triHalfWidth = 0.3f;
        float start = 2.5f;
        verts[0] = AZ::Vector3(start + triLength, 0, 0); verts[1] = AZ::Vector3(start, -triHalfWidth, 0); verts[2] = AZ::Vector3(start, triHalfWidth, 0);
        drawArgs.m_colors = &Red;
        drawArgs.m_vertCount = 3;
        auxGeom->DrawTriangles(drawArgs);

        verts[1] = AZ::Vector3(start, 0, triHalfWidth); verts[2] = AZ::Vector3(start, 0, -triHalfWidth);
        auxGeom->DrawTriangles(drawArgs);

        verts[0] = AZ::Vector3(0, start + triLength, 0); verts[1] = AZ::Vector3(0, start, -triHalfWidth); verts[2] = AZ::Vector3(0, start, triHalfWidth);
        drawArgs.m_colors = &Green;
        auxGeom->DrawTriangles(drawArgs);

        verts[1] = AZ::Vector3(triHalfWidth, start, 0); verts[2] = AZ::Vector3(-triHalfWidth, start, 0);
        auxGeom->DrawTriangles(drawArgs);

        verts[0] = AZ::Vector3(0, 0, start + triLength); verts[1] = AZ::Vector3(-triHalfWidth, 0, start); verts[2] = AZ::Vector3(triHalfWidth, 0, start);
        drawArgs.m_colors = &Blue;
        auxGeom->DrawTriangles(drawArgs);

        verts[1] = AZ::Vector3(0, triHalfWidth, start); verts[2] = AZ::Vector3(0, -triHalfWidth, start);
        auxGeom->DrawTriangles(drawArgs);
    }

    void DrawLines(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        float halfLength = 0.25f;
        AZ::Vector3 xVec(halfLength, 0.0f, 0.0f);
        AZ::Vector3 yVec(0.0f, halfLength, 0.0f);
        AZ::Vector3 zVec(0.0f, 0.0f, halfLength);

        ///////////////////////////////////////////////////////////////////////
        // Draw a 3d cross all one color using DrawLines (opaque)
        {
            AZ::Vector3 center(-1.0f, 1.0f, 0.0f);
            AZ::Vector3 points[] = { center - xVec, center + xVec, center - zVec, center + zVec, center - yVec, center + yVec };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 6;
            drawArgs.m_colors = &Black;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // Draw a 3d cross all one color using DrawLines (translucent)
        {
            AZ::Vector3 center(-2.0f, 1.0f, 0.0f);
            AZ::Vector3 points[] = { center - xVec, center + xVec, center - zVec, center + zVec, center - yVec, center + yVec };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 6;
            drawArgs.m_colors = &BlackAlpha;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // Draw a 3d cross in three colors using DrawLines (opaque)
        {
            AZ::Vector3 center(-1.0f, 2.0f, 0.0f);
            AZ::Vector3 points[] = { center - xVec, center + xVec, center - zVec, center + zVec, center - yVec, center + yVec };
            AZ::Color colors[] = { Red, Yellow, Green, Cyan, Blue, Magenta };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 6;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 6;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // Draw a 3d cross in three colors using DrawLines (translucent)
        {
            AZ::Vector3 center(-2.0f, 2.0f, 0.0f);
            AZ::Vector3 points[] = { center - xVec, center + xVec, center - zVec, center + zVec, center - yVec, center + yVec };
            AZ::Color colors[] = { RedAlpha, Yellow, GreenAlpha, Cyan, BlueAlpha, Magenta };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 6;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 6;
            drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a wireframe pyramid using 5 points and 16 indices in one color (opaque)
        {
            AZ::Vector3 baseCenter(-1.0f, 3.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec, baseCenter + zVec };
            uint32_t indices[] = { 0, 1, 1, 2, 2, 3, 3, 0,   0, 4, 1, 4, 2, 4, 3, 4 };
            AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 5;
            drawArgs.m_indices = indices;
            drawArgs.m_indexCount = 16;
            drawArgs.m_colors = &Black;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a wireframe pyramid using 5 points and 16 indices in one color (translucent)
        {
            AZ::Vector3 baseCenter(-2.0f, 3.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec, baseCenter + zVec };
            uint32_t indices[] = { 0, 1, 1, 2, 2, 3, 3, 0,   0, 4, 1, 4, 2, 4, 3, 4 };
            AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 5;
            drawArgs.m_indices = indices;
            drawArgs.m_indexCount = 16;
            drawArgs.m_colors = &BlackAlpha;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a wireframe pyramid using 5 points and 16 indices in many colors (opaque)
        {
            AZ::Vector3 baseCenter(-1.0f, 4.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec, baseCenter + zVec };
            uint32_t indices[] = { 0, 1, 1, 2, 2, 3, 3, 0,   0, 4, 1, 4, 2, 4, 3, 4 };
            AZ::Color colors[] = { Red, Green, Blue, Yellow, Black };
            AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 5;
            drawArgs.m_indices = indices;
            drawArgs.m_indexCount = 16;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 5;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a wireframe pyramid using 5 points and 16 indices in many colors (translucent)
        {
            AZ::Vector3 baseCenter(-2.0f, 4.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec, baseCenter + zVec };
            uint32_t indices[] = { 0, 1, 1, 2, 2, 3, 3, 0,   0, 4, 1, 4, 2, 4, 3, 4 };
            AZ::Color colors[] = { RedAlpha, GreenAlpha, BlueAlpha, YellowAlpha, Black };
            AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 5;
            drawArgs.m_indices = indices;
            drawArgs.m_indexCount = 16;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 5;
            drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
            auxGeom->DrawLines(drawArgs);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a closed square using a polyline using 4 points in one color (opaque)
        {
            AZ::Vector3 baseCenter(-1.0f, 5.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = &Black;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Closed);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a closed square using a polyline using 4 points in one color (translucent)
        {
            AZ::Vector3 baseCenter(-2.0f, 5.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = &BlackAlpha;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Closed);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw an open square using a polyline using 4 points in one color (opaque)
        {
            AZ::Vector3 baseCenter(-1.0f, 6.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = &Black;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Open);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw an open square using a polyline using 4 points in one color (translucent)
        {
            AZ::Vector3 baseCenter(-2.0f, 6.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = &BlackAlpha;
            drawArgs.m_colorCount = 1;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Open);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a closed square using a polyline using 4 points in many colors (opaque)
        {
            AZ::Vector3 baseCenter(-1.0f, 7.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AZ::Color colors[] = { Red, Green, Blue, Yellow };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 4;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Closed);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw a closed square using a polyline using 4 points in many colors (translucent)
        {
            AZ::Vector3 baseCenter(-2.0f, 7.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AZ::Color colors[] = { RedAlpha, GreenAlpha, BlueAlpha, YellowAlpha };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 4;
            drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Closed);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw an open square using a polyline using 4 points in many colors (opaque)
        {
            AZ::Vector3 baseCenter(-1.0f, 8.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - yVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AZ::Color colors[] = { Red, Green, Blue, Yellow };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 4;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Open);
        }

        ///////////////////////////////////////////////////////////////////////
        // draw an open square using a polyline using 4 points in many colors (translucent)
        {
            AZ::Vector3 baseCenter(-2.0f, 8.0f, 0.0f);
            AZ::Vector3 points[] = { baseCenter - xVec - zVec, baseCenter + xVec - yVec, baseCenter + xVec + yVec, baseCenter - xVec + yVec };
            AZ::Color colors[] = { RedAlpha, GreenAlpha, BlueAlpha, YellowAlpha };
            AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
            drawArgs.m_verts = points;
            drawArgs.m_vertCount = 4;
            drawArgs.m_colors = colors;
            drawArgs.m_colorCount = 4;
            drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
            auxGeom->DrawPolylines(drawArgs, AuxGeomDraw::PolylineEnd::Open);
        }
    }

    void DrawTriangles(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        // Draw a mixture of opaque and translucent triangles to test distance sorting of primitives
        float width = 2.0f;
        float left = -5.0f;
        float right = left + width;
        float bottom = 2.0f;
        float top = bottom + width;
        float spacing = 1.0f;
        float yStart = -10.0f;
        float y = yStart;
        ///////////////////////////////////////////////////////////////////////
        AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
        AZ::Vector3 verts[3] = {AZ::Vector3(left, y, top), AZ::Vector3(right, y, top), AZ::Vector3(right, y, bottom)};
        drawArgs.m_verts = verts;
        drawArgs.m_vertCount = 3;
        drawArgs.m_colors = &Black;
        drawArgs.m_colorCount = 1;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(right, y, top); verts[1] = AZ::Vector3(right, y, bottom); verts[2] = AZ::Vector3(left, y, bottom);
        drawArgs.m_colors = &BlackAlpha;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(left, y, top); verts[1] = AZ::Vector3(right, y, top); verts[2] = AZ::Vector3(right, y, bottom);
        drawArgs.m_colors = &Red;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(right, y, top); verts[1] = AZ::Vector3(right, y, bottom); verts[2] = AZ::Vector3(left, y, bottom);
        drawArgs.m_colors = &RedAlpha;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(left, y, top); verts[1] = AZ::Vector3(right, y, top); verts[2] = AZ::Vector3(right, y, bottom);
        drawArgs.m_colors = &Green;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(right, y, top); verts[1] = AZ::Vector3(right, y, bottom); verts[2] = AZ::Vector3(left, y, bottom);
        drawArgs.m_colors = &GreenAlpha;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(left, y, top); verts[1] = AZ::Vector3(right, y, top); verts[2] = AZ::Vector3(right, y, bottom);
        drawArgs.m_colors = &Blue;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        y += spacing;
        verts[0] = AZ::Vector3(right, y, top); verts[1] = AZ::Vector3(right, y, bottom); verts[2] = AZ::Vector3(left, y, bottom);
        drawArgs.m_colors = &BlueAlpha;
        auxGeom->DrawTriangles(drawArgs);


        ///////////////////////////////////////////////////////////////////////
        // do the same thing but in 2 AuxGeom draw calls - one for the opaque and one for the translucent
        // Note that this will mean that the 5 translucent depth sort together and not separately
        left = -8.0f;
        right = left + width;
        const uint32_t NumPoints = 12;
        AZ::Vector3 opaquePoints[NumPoints] =
        {
            AZ::Vector3(left, yStart + spacing * 0, top), AZ::Vector3(right, yStart + spacing * 0, top), AZ::Vector3(right, yStart + spacing * 0, bottom),
            AZ::Vector3(left, yStart + spacing * 2, top), AZ::Vector3(right, yStart + spacing * 2, top), AZ::Vector3(right, yStart + spacing * 2, bottom),
            AZ::Vector3(left, yStart + spacing * 4, top), AZ::Vector3(right, yStart + spacing * 4, top), AZ::Vector3(right, yStart + spacing * 4, bottom),
            AZ::Vector3(left, yStart + spacing * 6, top), AZ::Vector3(right, yStart + spacing * 6, top), AZ::Vector3(right, yStart + spacing * 6, bottom),
        };
        AZ::Vector3 transPoints[NumPoints] =
        {
            AZ::Vector3(right, yStart + spacing * 1, top), AZ::Vector3(right, yStart + spacing * 1, bottom), AZ::Vector3(left, yStart + spacing * 1, bottom),
            AZ::Vector3(right, yStart + spacing * 3, top), AZ::Vector3(right, yStart + spacing * 3, bottom), AZ::Vector3(left, yStart + spacing * 3, bottom),
            AZ::Vector3(right, yStart + spacing * 5, top), AZ::Vector3(right, yStart + spacing * 5, bottom), AZ::Vector3(left, yStart + spacing * 5, bottom),
            AZ::Vector3(right, yStart + spacing * 7, top), AZ::Vector3(right, yStart + spacing * 7, bottom), AZ::Vector3(left, yStart + spacing * 7, bottom),
        };
        AZ::Color opaqueColors[NumPoints] = {
            Black, Black, Black,
            Red, Red, Red,
            Green, Green, Green,
            Blue, Blue, Blue,
        };
        AZ::Color transColors[NumPoints] = {
            BlackAlpha, BlackAlpha, BlackAlpha,
            RedAlpha, RedAlpha, RedAlpha,
            GreenAlpha, GreenAlpha, GreenAlpha,
            BlueAlpha, BlueAlpha, BlueAlpha,
        };
        ///////////////////////////////////////////////////////////////////////
        // opaque triangles
        drawArgs.m_verts = opaquePoints;
        drawArgs.m_vertCount = NumPoints;
        drawArgs.m_colors = opaqueColors;
        drawArgs.m_colorCount = NumPoints;
        drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Opaque;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        // translucent triangles
        drawArgs.m_verts = transPoints;
        drawArgs.m_colors = transColors;
        drawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////
        // Draw cubes using indexed draws to test shared vertices
        left = -12.0f;
        right = left + width;
        float front = yStart;
        float back = front + width;
        const uint32_t NumCubePoints = 8;
        AZ::Vector3 cubePoints[NumCubePoints] =
        {
            AZ::Vector3(left, front, top), AZ::Vector3(right, front, top), AZ::Vector3(right, front, bottom), AZ::Vector3(left, front, bottom),
            AZ::Vector3(left, back, top), AZ::Vector3(right, back, top), AZ::Vector3(right, back, bottom), AZ::Vector3(left, back, bottom),
        };
        const uint32_t NumCubeIndicies = 36;
        uint32_t cubeIndicies[NumCubeIndicies] =
        {
            0, 1, 2,  2, 3, 0,  // front face
            4, 5, 6,  6, 7, 4,  // back face (no back-face culling)
            0, 3, 7,  7, 4, 0,  // left
            1, 2, 6,  6, 5, 1,  // right
            0, 1, 5,  5, 4, 0,  // top
            2, 3, 7,  7, 6, 2,  // bottom
        };
        AZ::Color cubeColors[NumCubePoints] = { Red, Green, Blue, Black, RedAlpha, GreenAlpha, BlueAlpha, BlackAlpha };

        ///////////////////////////////////////////////////////////////////////
        // Opaque cube all one color
        AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments indexedDrawArgs;
        indexedDrawArgs.m_verts = cubePoints;
        indexedDrawArgs.m_vertCount = NumCubePoints;
        indexedDrawArgs.m_indices = cubeIndicies;
        indexedDrawArgs.m_indexCount = NumCubeIndicies;
        indexedDrawArgs.m_colors = &Red;
        indexedDrawArgs.m_colorCount = 1;
        auxGeom->DrawTriangles(indexedDrawArgs);

        ///////////////////////////////////////////////////////////////////////
        // Move all the points along the positive Y axis and draw another cube
         AZ::Vector3 offset(0.0f, 4.0f, 0.0f);
        for (int pointIndex = 0; pointIndex < NumCubePoints; ++pointIndex)
        {
            cubePoints[pointIndex] += offset;
        }

        // Translucent cube all one color 
        indexedDrawArgs.m_colors = &RedAlpha;
        auxGeom->DrawTriangles(indexedDrawArgs);

        ///////////////////////////////////////////////////////////////////////
        // Move all the points along the positive Z axis and draw another cube
        for (int pointIndex = 0; pointIndex < NumCubePoints; ++pointIndex)
        {
            cubePoints[pointIndex] += offset;
        }

        // Opaque cube with multiple colors
        indexedDrawArgs.m_colors = cubeColors;
        indexedDrawArgs.m_colorCount = NumCubePoints;
        indexedDrawArgs.m_opacityType = AuxGeomDraw::OpacityType::Opaque;
        auxGeom->DrawTriangles(indexedDrawArgs);

        ///////////////////////////////////////////////////////////////////////
        // Move all the points along the positive Z axis and draw another cube
        for (int pointIndex = 0; pointIndex < NumCubePoints; ++pointIndex)
        {
            cubePoints[pointIndex] += offset;
        }

        // Translucent cube with multiple colors
        indexedDrawArgs.m_opacityType = AuxGeomDraw::OpacityType::Translucent;
        auxGeom->DrawTriangles(indexedDrawArgs);
    }

    void DrawShapes(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        // Draw a mixture of opaque and translucent shapes to test distance sorting of objects

        const int numRows = 4;
        AZ::Color opaqueColors[numRows] = { Red, Green, Blue, Yellow };
        AZ::Color translucentColors[numRows] = { RedAlpha, GreenAlpha, BlueAlpha, YellowAlpha };

        const int numDrawStyles = 4;
        AuxGeomDraw::DrawStyle drawStyles[numDrawStyles] = { AuxGeomDraw::DrawStyle::Point, AuxGeomDraw::DrawStyle::Line, AuxGeomDraw::DrawStyle::Solid, AuxGeomDraw::DrawStyle::Shaded };

        AZ::Vector3 direction[numRows] =
        {
            AZ::Vector3( 0.0f, -1.0f,  0.0f),
            AZ::Vector3(-1.0f,  0.0f,  0.0f),
            AZ::Vector3( 1.0f,  1.0f,  0.0f),
            AZ::Vector3(-1.0f,  1.0f, -1.0f)
        };

        float radius = 1.0f;
        float height = 1.0f;

        float x = 5.0f;
        float y = -8.0f;
        float z = 2.0f;

        Vector3 translucentOffset(2.0f, 0.0f, 0.5f);
        Vector3 shapeOffset(0.0f, 0.0f, 3.0f);
        Vector3 styleOffset(8.0f, 0.0f, 0.0f);

        float colorYOffset = -8.0f;
        auxGeom->SetPointSize(5.0f);

        // Each row is drawn in a different color (colors changing along the Z axis)
        // Within each row we draw every shape in all three draw styles, both opaque and translucent
        for (int rowIndex = 0; rowIndex < numRows; ++rowIndex)
        {
            Vector3 basePosition = Vector3(x, y, z);

            // Spread the draw style out along the X axis
            for (int style = 0; style < numDrawStyles; ++style)
            {
                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Draw Spheres
                // Two spheres, one opaque, one translucent, One DepthTest On & DepthWrite off, One DepthTest Off & DepthWrite On
                Vector3 shapePosition = basePosition;
                auxGeom->DrawSphere(shapePosition, radius, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);
                auxGeom->DrawSphere(shapePosition + 1.0f * translucentOffset, radius, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::Back);

                // Two spheres, One DepthTest On & DepthWrite off, One DepthTest Off & DepthWrite On
                shapePosition += shapeOffset;
                auxGeom->DrawSphere(shapePosition, radius, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::Back);
                auxGeom->DrawSphere(shapePosition + translucentOffset, radius, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::Off, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);

                // Three spheres, One no face cull, One front face cull, One back face cull
                shapePosition += shapeOffset;
                auxGeom->DrawSphere(shapePosition, radius, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::None);
                auxGeom->DrawSphere(shapePosition + translucentOffset, radius, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Front);
                auxGeom->DrawSphere(shapePosition + 2.0f * translucentOffset, radius, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);


                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Draw Cones
                // Two cones, one opaque, one translucent
                shapePosition += 2.0f * shapeOffset;
                auxGeom->DrawCone(shapePosition, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);
                auxGeom->DrawCone(shapePosition + translucentOffset, direction[rowIndex], radius, height, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::Back);

                // Two cones, One DepthTest On & DepthWrite off, One DepthTest Off & DepthWrite On
                shapePosition += shapeOffset;
                auxGeom->DrawCone(shapePosition, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::Back);
                auxGeom->DrawCone(shapePosition + translucentOffset, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::Off, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);

                // Three cones, One no face cull, One front face cull, One back face cull
                shapePosition += shapeOffset;
                auxGeom->DrawCone(shapePosition, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::None);
                auxGeom->DrawCone(shapePosition + translucentOffset, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Front);
                auxGeom->DrawCone(shapePosition + 2.0f * translucentOffset, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);


                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Draw Cylinders
                // Two cylinders, one opaque, one translucent
                shapePosition += 2.0f * shapeOffset;
                auxGeom->DrawCylinder(shapePosition, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);
                auxGeom->DrawCylinder(shapePosition + translucentOffset, direction[rowIndex], radius, height, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::Back);

                // Two cylinders, One DepthTest On & DepthWrite off, One DepthTest Off & DepthWrite On
                shapePosition += shapeOffset;
                auxGeom->DrawCylinder(shapePosition, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::Back);
                auxGeom->DrawCylinder(shapePosition + translucentOffset, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::Off, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);

                // Three cylinders, One no face cull, One front face cull, One back face cull
                shapePosition += shapeOffset;
                auxGeom->DrawCylinder(shapePosition, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::None);
                auxGeom->DrawCylinder(shapePosition + translucentOffset, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Front);
                auxGeom->DrawCylinder(shapePosition + 2.0f * translucentOffset, direction[rowIndex], radius, height, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::Back);

                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                basePosition += styleOffset;
            }

            // move along Z axis for next color
            y += colorYOffset;
            radius *= 0.75f;
            height *= 1.25f;
        }
    }

    void DrawBoxes(AZ::RPI::AuxGeomDrawPtr auxGeom, float x)
    {
        // Draw a mixture of opaque and translucent boxes
        const int numRows = 4;
        AZ::Color opaqueColors[numRows] = { Red, Green, Blue, Yellow };
        AZ::Color translucentColors[numRows] = { RedAlpha, GreenAlpha, BlueAlpha, YellowAlpha };

        const int numStyles = 4;
        AuxGeomDraw::DrawStyle drawStyles[numStyles] = { AuxGeomDraw::DrawStyle::Point, AuxGeomDraw::DrawStyle::Line, AuxGeomDraw::DrawStyle::Solid, AuxGeomDraw::DrawStyle::Shaded };

        // The size of the box for each row
        AZ::Vector3 halfExtents[numRows] =
        {
            AZ::Vector3( 0.5f,  2.0f,  1.0f),
            AZ::Vector3( 1.0f,  1.5f,  1.0f),
            AZ::Vector3( 2.0f,  0.5f,  0.5f),
            AZ::Vector3( 0.1f,  1.0f,  2.0f)
        };

        // The euler rotations for each row that has a rotation
        AZ::Vector3 rotation[numRows] =
        {
            AZ::Vector3( 90.0f,  0.0f,  90.0f),
            AZ::Vector3( 90.0f,  0.0f,   0.0f),
            AZ::Vector3(  0.0f,  0.0f, -45.0f),
            AZ::Vector3(  0.0f, 45.0f, -45.0f)
        };

        float y = 5.0f;
        float z = 2.0f;

        Vector3 translucentOffset(2.0f, 0.0f, 0.5f);
        Vector3 typeOffset(0.0f, 0.0f, 3.0f);
        Vector3 styleOffset(8.0f, 0.0f, 0.0f);

        // each translucent box is positioned like its opaque partner but with this additional scale
        Vector3 transScale(1.5f, 0.5f, 1.0f);

        float colorYOffset = 8.0f;

        auxGeom->SetPointSize(10.0f);

        // Each row is drawn in a different color (colors changing along the Z axis)
        // Within each row we draw every box type in all three draw styles, both opaque and translucent
        for (int rowIndex = 0; rowIndex < numRows; ++rowIndex)
        {
            Vector3 basePosition = Vector3(x, y, z);

            AZ::Transform rotationTransform;
            rotationTransform.SetFromEulerDegrees(rotation[rowIndex]);
            AZ::Quaternion rotationQuaternion = rotationTransform.GetRotation();
            AZ::Matrix3x3 rotationMatrix = AZ::Matrix3x3::CreateFromTransform(rotationTransform);

            // Spread the draw style out along the X axis
            for (int style = 0; style < numStyles; ++style)
            {
                Vector3 boxPosition = basePosition;

                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Layer 0: AABBs with no transform
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                AZ::Aabb aabb = AZ::Aabb::CreateCenterHalfExtents(basePosition, halfExtents[rowIndex]);
                auxGeom->DrawAabb(aabb, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On);

                AZ::Vector3 scaledHalfExtents = halfExtents[rowIndex] * transScale;
                for (int index = 0; index < 3; ++index)
                aabb = AZ::Aabb::CreateCenterHalfExtents(basePosition + translucentOffset, scaledHalfExtents);
                auxGeom->DrawAabb(aabb, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off);

                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Layer 1: AABBs with transforms that rotate and scale
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromMatrix3x3AndTranslation(rotationMatrix, basePosition + typeOffset);
                aabb = AZ::Aabb::CreateCenterHalfExtents(AZ::Vector3::CreateZero(), halfExtents[rowIndex]);
                auxGeom->DrawAabb(aabb, transform, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On);

                // for the transparent version apply an additional scale to the transform
                transform = AZ::Matrix3x4::CreateFromMatrix3x3AndTranslation(rotationMatrix, basePosition + typeOffset + translucentOffset);
                transform.MultiplyByScale(transScale);
                auxGeom->DrawAabb(aabb, transform, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off);

                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Layer 2: OBB with all position, rotation scale defined in the OBB itself
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                // position the OOB using Obb::SetPosition and rotate it using Obb::SetAxis
                AZ::Obb obb = AZ::Obb::CreateFromAabb(aabb);
                obb.SetPosition(basePosition + 2 * typeOffset);
                obb.SetRotation(rotationQuaternion);
                auxGeom->DrawObb(obb, AZ::Matrix3x4::CreateIdentity(), opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On);

                // for the transparent version apply an additional scale to the transform
                obb.SetPosition(basePosition + 2 * typeOffset + translucentOffset);
                for (int index = 0; index < 3; ++index)
                {
                    obb.SetHalfLength(index, obb.GetHalfLength(index) * transScale.GetElement(index));
                }
                auxGeom->DrawObb(obb, AZ::Matrix3x4::CreateIdentity(), translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On);

                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Layer 3: OBB with rotation and scale defined in the OBB itself but position passed to DrawObb separately
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                // position the OOB using position param to DrawObb, rotate it using  Obb::SetAxis
                obb = AZ::Obb::CreateFromAabb(aabb);
                obb.SetRotation(rotationQuaternion);
                auxGeom->DrawObb(obb, basePosition + 3 * typeOffset, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On);

                // For the translucent version apply the additional scale in the half extents
                for (int index = 0; index < 3; ++index)
                {
                    obb.SetHalfLength(index, obb.GetHalfLength(index) * transScale.GetElement(index));
                }
                auxGeom->DrawObb(obb, basePosition + 3 * typeOffset + translucentOffset, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off);

                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // Layer 4: OBB with rotation and scale and translation defined in the transform passed to DrawObb
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                // position and rotate the OOB using transform param to DrawObb
                obb = AZ::Obb::CreateFromAabb(aabb);

                transform = AZ::Matrix3x4::CreateFromMatrix3x3AndTranslation(rotationMatrix, basePosition + 4 * typeOffset);
                auxGeom->DrawObb(obb, transform, opaqueColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On);

                // for the transparent version apply an additional scale to the transform
                transform = AZ::Matrix3x4::CreateFromMatrix3x3AndTranslation(rotationMatrix, basePosition + 4 * typeOffset + translucentOffset);
                transform.MultiplyByScale(transScale);
                auxGeom->DrawObb(obb, transform, translucentColors[rowIndex], drawStyles[style], AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off);

                basePosition += styleOffset;
            }

            // move along Z axis for next color
            y += colorYOffset;
        }
    }

    void DrawManyPrimitives(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        // Draw a grid of 300 x 200 quads (as triangle pairs - no shared verts)

        const float y = 20.0f;
        const float xOrigin = -30.0f;
        const float zOrigin = 0.0f;
        const float width = 0.1f;
        const float height = 0.1f;

        // we will draw 300 by 200 (60,000) quads as triangle pairs = 120,000 triangles = 360,000 vertices
        const int widthInQuads = 300;
        const int heightInQuads = 200;
        AZ::RPI::AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
        drawArgs.m_vertCount = 3;
        drawArgs.m_colorCount = 1;

        for (int xIndex = 0; xIndex < widthInQuads; ++xIndex)
        {
            for (int zIndex = 0; zIndex < heightInQuads; ++zIndex)
            {
                const float xMin = xOrigin + xIndex * width;
                const float xMax = xMin + width;
                const float zMin = zOrigin + zIndex * height;
                const float zMax = zMin + height;

                AZ::Color color(static_cast<float>(xIndex) / (widthInQuads - 1), static_cast<float>(zIndex) / (heightInQuads - 1), 0.0f, 1.0f);
                AZ::Vector3 verts[3] = {AZ::Vector3(xMin, y, zMax), AZ::Vector3(xMax, y, zMax), AZ::Vector3(xMax, y, zMin)};

                drawArgs.m_verts = verts;
                drawArgs.m_colors = &color;
                auxGeom->DrawTriangles(drawArgs);

                AZStd::swap(verts[0], verts[2]);
                verts[1] = AZ::Vector3(xMin, y, zMin);
                auxGeom->DrawTriangles(drawArgs);
            }
        }
    }

    void DrawDepthTestPrimitives(AZ::RPI::AuxGeomDrawPtr auxGeom)
    {
        float width = 2.0f;
        float left = -20.0f;
        float right = left + width;
        float bottom = -1.0f;
        float top = bottom + width;
        float yStart = -1.0f;

        //Draw opaque cube using DrawTriangles
        right = left + width;
        float front = yStart;
        float back = front + width;
        const uint32_t NumCubePoints = 8;
        AZ::Vector3 cubePoints[NumCubePoints] =
        {
            AZ::Vector3(left, front, top), AZ::Vector3(right, front, top), AZ::Vector3(right, front, bottom), AZ::Vector3(left, front, bottom),
            AZ::Vector3(left, back, top), AZ::Vector3(right, back, top), AZ::Vector3(right, back, bottom), AZ::Vector3(left, back, bottom),
        };
        const uint32_t NumCubeIndicies = 36;
        uint32_t cubeIndicies[NumCubeIndicies] =
        {
            0, 1, 2,  2, 3, 0,  // front face
            4, 5, 6,  6, 7, 4,  // back face (no back-face culling)
            0, 3, 7,  7, 4, 0,  // left
            1, 2, 6,  6, 5, 1,  // right
            0, 1, 5,  5, 4, 0,  // top
            2, 3, 7,  7, 6, 2,  // bottom
        };
        ///////////////////////////////////////////////////////////////////////
        // Opaque cube all one color, depth write & depth test on
        AZ::RPI::AuxGeomDraw::AuxGeomDynamicIndexedDrawArguments drawArgs;
        drawArgs.m_verts = cubePoints;
        drawArgs.m_vertCount = NumCubePoints;
        drawArgs.m_indices = cubeIndicies;
        drawArgs.m_indexCount = NumCubeIndicies;
        drawArgs.m_colors = &Red;
        drawArgs.m_colorCount = 1;
        drawArgs.m_depthTest = AuxGeomDraw::DepthTest::On;
        drawArgs.m_depthWrite = AuxGeomDraw::DepthWrite::On;
        auxGeom->DrawTriangles(drawArgs);

        AZ::Vector3 offset = AZ::Vector3(-5.0f, 0.0f, 0.0f);
        AZ::Vector3 scale = AZ::Vector3(1.0f, 3.0f, 3.0f);
        for (int pointIndex = 0; pointIndex < NumCubePoints; ++pointIndex)
        {
            cubePoints[pointIndex] *= scale;
            cubePoints[pointIndex] += offset;
        }
        ///////////////////////////////////////////////////////////////////////
        // Opaque cube all one color, depth write off
        drawArgs.m_colors = &Green;
        drawArgs.m_depthTest = AuxGeomDraw::DepthTest::On;
        drawArgs.m_depthWrite = AuxGeomDraw::DepthWrite::Off;
        auxGeom->DrawTriangles(drawArgs);

        offset = AZ::Vector3(-5.0f, 1.0f, 0.0f);
        scale = AZ::Vector3(1.0f, 0.3333f, 0.3333f);
        for (int pointIndex = 0; pointIndex < NumCubePoints; ++pointIndex)
        {
            cubePoints[pointIndex] *= scale;
            cubePoints[pointIndex] += offset;
        }
        ///////////////////////////////////////////////////////////////////////
        // Opaque cube all one color, depth Test off
        drawArgs.m_colors = &Blue;
        drawArgs.m_depthTest = AuxGeomDraw::DepthTest::Off;
        drawArgs.m_depthWrite = AuxGeomDraw::DepthWrite::On;
        auxGeom->DrawTriangles(drawArgs);

        offset = AZ::Vector3(-5.0f, 1.0f, 0.0f);
        for (int pointIndex = 0; pointIndex < NumCubePoints; ++pointIndex)
        {
            cubePoints[pointIndex] += offset;
        }
        ///////////////////////////////////////////////////////////////////////
        // Opaque cube all one color, depth Test on, depth Write on
        drawArgs.m_colors = &Yellow;
        drawArgs.m_depthTest = AuxGeomDraw::DepthTest::On;
        drawArgs.m_depthWrite = AuxGeomDraw::DepthWrite::On;
        auxGeom->DrawTriangles(drawArgs);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Repeat with AABB shapes
        float radius = width / 2.0f;
        AZ::Vector3 basePosition = AZ::Vector3(-20.0f + radius, -7.0f, 0.0f); // adjust x pos by +radius to account for AABB's being centered relative to the coordinate.

        AZ::Aabb aabb = AZ::Aabb::CreateCenterRadius(basePosition, radius);
        auxGeom->DrawAabb(aabb, Red, AuxGeomDraw::DrawStyle::Solid, AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::None, -1);

        aabb = AZ::Aabb::CreateCenterHalfExtents(basePosition + 1.0f * offset, AZ::Vector3(radius, 3.0f * radius, 3.0f * radius));
        auxGeom->DrawAabb(aabb, Green, AuxGeomDraw::DrawStyle::Solid, AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::Off, AuxGeomDraw::FaceCullMode::None, -1);

        aabb = AZ::Aabb::CreateCenterRadius(basePosition + 2.0f * offset, radius);
        auxGeom->DrawAabb(aabb, Blue, AuxGeomDraw::DrawStyle::Solid, AuxGeomDraw::DepthTest::Off, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::None, -1);

        aabb = AZ::Aabb::CreateCenterRadius(basePosition + 3.0f * offset, radius);
        auxGeom->DrawAabb(aabb, Yellow, AuxGeomDraw::DrawStyle::Solid, AuxGeomDraw::DepthTest::On, AuxGeomDraw::DepthWrite::On, AuxGeomDraw::FaceCullMode::None, -1);
    }

    void Draw2DWireRect(AZ::RPI::AuxGeomDrawPtr auxGeom, const AZ::Color& color, float z)
    {
        float vertOffset = z * 0.45f;
        AZ::Vector3 verts[4] = {
            AZ::Vector3(0.5f - vertOffset, 0.5f - vertOffset, z), 
            AZ::Vector3(0.5f - vertOffset, 0.5f + vertOffset, z), 
            AZ::Vector3(0.5f + vertOffset, 0.5f + vertOffset, z), 
            AZ::Vector3(0.5f + vertOffset, 0.5f - vertOffset, z) };
        int32_t viewProjOverrideIndex = auxGeom->GetOrAdd2DViewProjOverride();
        AZ::RPI::AuxGeomDraw::AuxGeomDynamicDrawArguments drawArgs;
        drawArgs.m_verts = verts;
        drawArgs.m_vertCount = 4;
        drawArgs.m_colors = &color;
        drawArgs.m_colorCount = 1;
        drawArgs.m_viewProjectionOverrideIndex = viewProjOverrideIndex;
        auxGeom->DrawPolylines(drawArgs, AZ::RPI::AuxGeomDraw::PolylineEnd::Closed);
    }
}
