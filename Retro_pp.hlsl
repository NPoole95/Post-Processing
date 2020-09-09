//--------------------------------------------------------------------------------------
// Retro Post-Processing Pixel Shader
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
//uniform float vx_offset = gViewportWidth / 2; // offset of the vertical red line

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------
float4 main(PostProcessingInput input) : SV_Target
{
    float pixelWidth = 15; // low res pixel width
    float pixelHeight = 10; // low res pixel height
    
    float3 finalColour = float3(1.0, 0.0, 0.0);
       
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
        // Perform Post Process
        float pixelXPos = pixelWidth * (1.0f / gViewportWidth);
        float pixelYPos = pixelHeight * (1.0f / gViewportHeight);
        
        float2 coord = float2(pixelXPos * floor(input.sceneUV.x / pixelXPos), pixelYPos * floor(input.sceneUV.y / pixelYPos));
        
        finalColour = SceneTexture.Sample(PointSample, coord).rgb;
        finalColour.r = (floor(finalColour.r * 10) / 10) * 1.3;
        finalColour.g = (floor(finalColour.g * 10) / 10) * 1.3;
        finalColour.b = (floor(finalColour.b * 10) / 10) * 1.3;
    
    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }
    
    float outputAlpha = 1.0f;
    return float4(finalColour, outputAlpha);
}
