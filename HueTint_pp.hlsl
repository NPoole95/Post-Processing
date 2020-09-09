//--------------------------------------------------------------------------------------
// Colour Tint Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------
// Just samples a pixel from the scene texture and multiplies it by a fixed colour to tint the scene

#include "Common.hlsli"


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

float3 HUEtoRGB(in float H);

float3 HSLtoRGB(in float3 HSL);

float3 RGBtoHCV(in float3 RGB);

float3 RGBtoHSL(in float3 RGB);

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
	// Sample a pixel from the scene texture and multiply it with the tint colour (comes from a constant buffer defined in Common.hlsli)
    
    float3 topColour;
    float3 bottomColour;
    float3 RGB;
    float3 HSL;
    float sinY;
    
    float3 outputColour = float3(1.0, 0.0, 0.0);
    
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
    /////// top colour  /////////////  
        RGB.r = gtintTopColour.r;
        RGB.g = gtintTopColour.g;
        RGB.b = gtintTopColour.b;
	
        HSL = RGBtoHSL(RGB);
    
    
        sinY = sin(gHueWiggle * 0.3);
        HSL.r += (0.314f * sinY);
    
        if (HSL.r > 1.0f)
        {
            HSL.r = 0.0f;
        }
    
        RGB = HSLtoRGB(HSL);
    
        topColour.r = RGB.r;
        topColour.g = RGB.g;
        topColour.b = RGB.b;
    
    ////////// bottom colour//////////
        RGB.r = gtintBottomColour.r;
        RGB.g = gtintBottomColour.g;
        RGB.b = gtintBottomColour.b;
	
        HSL = RGBtoHSL(RGB);
     
        sinY = sin(gHueWiggle * 0.3);
        HSL.r += (0.314f * sinY);
    
        if (HSL.r > 1.0f)
        {
            HSL.r = 0.0f;
        }
    
        RGB = HSLtoRGB(HSL);
    
        bottomColour.r = RGB.r;
        bottomColour.g = RGB.g;
        bottomColour.b = RGB.b;
    
    //////// final colour //////////////////
        float3 finalColour;
        finalColour.r = lerp(topColour.r, bottomColour.r, input.sceneUV.y);
        finalColour.g = lerp(topColour.g, bottomColour.g, input.sceneUV.y);
        finalColour.b = lerp(topColour.b, bottomColour.b, input.sceneUV.y);
    
        outputColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb * finalColour;
    
    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        outputColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }
	// Got the RGB from the scene texture, set alpha to 1 for final output
    float outputAlpha = 1.0f;
    return float4(outputColour, outputAlpha);
}

//http://www.chilliant.com/rgb2hsv.html

float3 HUEtoRGB(in float H)
{
    float R = abs(H * 6 - 3) - 1;
    float G = 2 - abs(H * 6 - 2);
    float B = 2 - abs(H * 6 - 4);
    return saturate(float3(R, G, B));
}

float3 HSLtoRGB(in float3 HSL)
{
    float3 RGB = HUEtoRGB(HSL.x);
    float C = (1 - abs(2 * HSL.z - 1)) * HSL.y;
    return (RGB - 0.5) * C + HSL.z;
}

float Epsilon = 1e-10;
 
float3 RGBtoHCV(in float3 RGB)
{
    // Based on work by Sam Hocevar and Emil Persson
    float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0 / 3.0) : float4(RGB.gb, 0.0, -1.0 / 3.0);
    float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
    return float3(H, C, Q.x);
}

float3 RGBtoHSL(in float3 RGB)
{
    float3 HCV = RGBtoHCV(RGB);
    float L = HCV.z - HCV.y * 0.5;
    float S = HCV.y / (1 - abs(L * 2 - 1) + Epsilon);
    return float3(HCV.x, S, L);
}
