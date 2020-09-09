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
// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
	// Sample a pixel from the scene texture and multiply it with the tint colour (comes from a constant buffer defined in Common.hlsli)
    float3 finalColour = float3(1.0, 0.0, 0.0);
    
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
        finalColour.r = lerp(gtintTopColour.r, gtintBottomColour.r, input.sceneUV.y);
        finalColour.g = lerp(gtintTopColour.g, gtintBottomColour.g, input.sceneUV.y);
        finalColour.b = lerp(gtintTopColour.b, gtintBottomColour.b, input.sceneUV.y);
    
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb * finalColour;
    
    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }
    
    float outputAlpha = 1.0f;
	// Got the RGB from the scene texture, set alpha to 1 for final output
    return float4(finalColour, outputAlpha);
}
