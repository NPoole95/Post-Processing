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

// Post-processing shader that Blurs the scene texture
float4 main(PostProcessingInput input) : SV_Target
{
    
    float3 finalColour = float3(1.0, 0.0, 0.0);
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
   
        float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    
        float pixelWidth = (1 / gViewportWidth);

        float2 samp = input.sceneUV;
        samp.y = input.sceneUV.y;

        finalColour = (0.0f, 0.0f, 0.0f);
    
        for (int i = 0; i < gBlurStrength; i++)
        {
            samp.x = input.sceneUV.x + gKernel[i].x * pixelWidth;
            finalColour += SceneTexture.Sample(PointSample, samp.xy).rgb * gWeight[i].x;
        }
    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }
    
    float outputAlpha = 1.0f;
	// Got the RGB from the scene texture, set alpha to 1 for final output
    return float4(finalColour, outputAlpha);
}