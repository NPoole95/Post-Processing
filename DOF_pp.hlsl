//--------------------------------------------------------------------------------------
// Depth Of Field Post-Processing Pixel Shader
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
Texture2D DepthTexture : register(t2);
SamplerState PointClamp : register(s2); // This sampler switches off filtering (e.g. bilinear, trilinear) when accessing depth buffer

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
    float3 finalColour = float3(1.0, 0.0, 0.0);
    
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV); //this takes our sampler and turns the rgba into floats between 0 and 1

        float depthValue = DepthTexture.Sample(PointClamp, input.sceneUV).r;
    
        if (depthValue >= gDepthThreshold)
        {
            finalColour *= float3(1.0f, 0.0f, 0.0f);
        }
    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }
    
    float alpha = 1.0f;
    return float4(finalColour, alpha);

}