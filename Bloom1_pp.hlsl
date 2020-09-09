//--------------------------------------------------------------------------------------
// Bloom Post-Processing Pixel Shader
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

float4 main(PostProcessingInput input) : SV_Target
{
    
    float4 finalColour = float4(1.0, 0.0, 0.0, 1.0f);
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
    
        float3 vAdd = (0.1, 0.1, 0.1); // just a float3 for use later
	
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV); //this takes our sampler and turns the rgba into floats between 0 and 1  
    
        if (((finalColour.r + finalColour.g + finalColour.b) / 3)  < gBloomThreshold) // check to see if the pixels brightness is below the threshold
        {
            finalColour.rgb = float3(0.0f, 0.0f, 0.0f); //if it is, set it to black
        }

    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        finalColour.rgb = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }

    float outputAlpha = 1.0f;
    return float4(finalColour.rgb, outputAlpha); // brighten the final image and return it
    
}
