
#define PI 3.1415
#define PI_X2 6.2831
#define PI_X4 12.5663

static const float InvPi = 1.0f / PI;

half GetLuminance(half3 color)
{
    return dot(color, half3(0.2126h, 0.7152h, 0.0722h));
}

half SmoothnessToRoughness(half smoothness)
{
    return (1.0f - smoothness) * (1.0f - smoothness);
}

half RoughnessToSmoothness(half roughness)
{
    return 1.0f - sqrt(roughness);
}

half BlinnBRDF(half3 N, half3 V, half3 L, half Gloss)
{
    half3 H = normalize(V + L);

    // Compute perceptually linear exponent in range 2-2048
    half power = exp2(10.h * Gloss + 1.h);

    half fNormFactor = power * (1.0 / 8.0) + (2.0 / 8.0);
    return fNormFactor * pow(saturate(dot(N, H)), power);
}

half3 SpecularBRDF(half3 N, half3 V, half3 L, half m, half3 f0, half NormalizationFactor)
{
    half epsilon = 1e-5h;   // epsilon to prevent division by 0
    half m2 = m * m;
    half3 H = normalize(V + L);

    // GGX NDF
    half NdotH = saturate(dot(N, H));
    half spec = (NdotH * m2 - NdotH) * NdotH + 1;
    spec = m2 / max((spec * spec) * NormalizationFactor, epsilon);

    // Correlated Smith Visibility Term (including Cook-Torrance denominator)
    half NdotL = saturate(dot(N, L));
    half NdotV = abs(dot(N, V)) + epsilon;
    half Gv = NdotL * sqrt((-NdotV * m2 + NdotV) * NdotV + m2);
    half Gl = NdotV * sqrt((-NdotL * m2 + NdotL) * NdotL + m2);
    spec *= 0.5h / max(Gv + Gl, epsilon);

    // Fresnel (Schlick approximation)
    half f90 = saturate(dot(f0, 0.33333h) / 0.02h);  // Assume micro-occlusion when reflectance is below 2%
    half3 fresnel = lerp(f0, f90, pow(1 - saturate(dot(L, H)), 5));

    return fresnel * spec;
}

half3 SpecularBRDF(half3 N, half3 V, half3 L, half Gloss, half3 SpecCol)
{
    half m = max(SmoothnessToRoughness(Gloss), 0.001);  // Prevent highlights from getting too tiny without area lights
    return SpecularBRDF(N, V, L, m, SpecCol, 1.0f);
}

////////////////////////////////////////////////////////////////////////////
// Anisotropic Kajiya Kay model

half KajiyaKayAnisotropic(half3 T, half3 H, half Exp)
{
    half TdotH = dot(T, H);
    half fSpec = sqrt(max(1.0 - TdotH * TdotH, 0.01));

    return pow(fSpec, Exp);
}


#define AREA_LIGHT_SPHERE		0
#define AREA_LIGHT_RECTANGLE	1

float SphereNormalization(float len, float lightSize, float m)
{
    // Compute the normalization factors.
    // Note: just using sphere normalization (todo: come up with proper disk/plane normalization)
    float dist = saturate(lightSize / len);
    float normFactor = m / saturate(m + 0.5 * dist);
    return normFactor * normFactor;
}

float3 SphereLight(float3 R, float3 L, float m, half4x4 mAreaLightMatr)
{
    // Intersect the sphere.
    float3 centerDir = L - dot(L, R) * R;
    L = L - centerDir * saturate(mAreaLightMatr[3].y / (length(centerDir) + 1e-6));
    return L.xyz;
}

float3 RectangleLight(float3 R, float3 L, float m, half4x4 mAreaLightMatr)
{
    // Intersect the plane.
    float RdotN = dot(mAreaLightMatr[0].xyz, R.xyz) + 1e-6;
    float intersectLen = dot(mAreaLightMatr[0].xyz, L) / RdotN;
    float3 I = R.xyz * intersectLen - L.xyz;

    // Project the intersection to 2D and clamp it to the light radius to get a point inside or on the edge of the light.
    half2 lightPos2D = half2(dot(I.xyz, mAreaLightMatr[1].xyz), dot(I.xyz, mAreaLightMatr[2].xyz));
    half2 lightClamp2D = clamp(lightPos2D, -mAreaLightMatr[3].xy, mAreaLightMatr[3].xy);

    // New light direction.
    L = L + (mAreaLightMatr[1].xyz * lightClamp2D.x) + mAreaLightMatr[2].xyz * lightClamp2D.y;
    return L.xyz;
}

half4 AreaLightIntersection(float3 N, float3 V, float3 L, float m, half4x4 areaLightMatrix, int lightType)
{
    half4 lightVec = half4(L.xyz, 1.0f);
    float3 R = reflect(V, N);

    [branch] if (lightType == AREA_LIGHT_RECTANGLE)
        lightVec.xyz = RectangleLight(R, L, m, areaLightMatrix);
    else
        lightVec.xyz = SphereLight(R, L, m, areaLightMatrix);

    // Normalize.
    float len = max(length(lightVec.xyz), 1e-6);
    lightVec.xyz /= len;

    // Energy normalization
    lightVec.w = SphereNormalization(len, lightType == AREA_LIGHT_RECTANGLE ? length(areaLightMatrix[3].xy) : areaLightMatrix[3].y, m);

    return lightVec;
}

float3 AreaLightGGX(float3 N, float3 V, float3 L, float Gloss, float3 SpecCol, half4x4 areaLightMatrix, int lightType)
{
    float m = max(SmoothnessToRoughness(Gloss), 0.001);
    half4 lightVec = AreaLightIntersection(N, V, L, m, areaLightMatrix, lightType);
    return SpecularBRDF(N, V, lightVec.xyz, m, SpecCol, lightVec.w);
}

// Diffuse BRDFs

float3 ComputeNearestLightOnRectangle(float3 vPosition, float3 vLightPoint, float4x4 mAreaLightMatr)
{
    // Calculate light space plane.
    float3 vLightDir = dot(mAreaLightMatr[0].xyz, vLightPoint.xyz) * mAreaLightMatr[0].xyz - vLightPoint.xyz;

    // Calculate the nearest point.
    half2 vSurfArea = float2(dot(vLightDir.xyz, mAreaLightMatr[1].xyz), dot(vLightDir.xyz, mAreaLightMatr[2].xyz));
    half2 vSurfAreaClamp = clamp(vSurfArea.xy, -mAreaLightMatr[3].xy, mAreaLightMatr[3].xy); // 1 alu
    float3 vNearestPoint = mAreaLightMatr[1].xyz * vSurfAreaClamp.x + (mAreaLightMatr[2].xyz * vSurfAreaClamp.y);

    return vLightPoint.xyz + vNearestPoint.xyz;
}

float OrenNayarBRDF(float3 N, float3 V, float3 L, float Gloss, float NdotL)
{
    float m = SmoothnessToRoughness(Gloss);
    m *= m * m;  // Map GGX to Oren-Nayar roughness (purely empirical remapping)

                 // Approximation of the full quality Oren-Nayar model
    float s = dot(L, V) - dot(N, L) * dot(N, V);
    float t = s <= 0 ? 1 : max(max(dot(N, L), dot(N, V)), 1e-6);
    float A = 1.0h / (1.0h + (0.5h - 2.0h / (3.0h * PI)) * m);
    float B = m * A;

    return NdotL * max(A + B * (s / t), 0);
}

float BurleyBRDF(float NdotL, float NdotV, float VdotH, float roughness)
{
    NdotV = max(NdotV, 0.1);  // Prevent overly dark edges

                              // Burley BRDF with renormalization to conserve energy
    float energyBias = 0.5 * roughness;
    float energyFactor = lerp(1, 1 / 1.51, roughness);
    float fd90 = energyBias + 2.0 * VdotH * VdotH * roughness;
    float scatterL = lerp(1, fd90, pow(1 - NdotL, 5));
    float scatterV = lerp(1, fd90, pow(1 - NdotV, 5));

    return scatterL * scatterV * energyFactor * NdotL;
}

float DiffuseBRDF(float3 N, float3 V, float3 L, float Gloss, float NdotL)
{
    // TODO: Share computations with Specular BRDF
    float m = SmoothnessToRoughness(min(Gloss, 1));
    float VdotH = saturate(dot(V, normalize(V + L)));
    float NdotV = abs(dot(N, V)) + 1e-5h;

    // Burley BRDF with renormalization to conserve energy
    return BurleyBRDF(NdotL, NdotV, VdotH, m);
}