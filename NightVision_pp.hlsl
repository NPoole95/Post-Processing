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

float4 main(PostProcessingInput input) : SV_Target
{
    float3 finalColour = float3(1.0, 0.0, 0.0);
    
    if (gMidLineEnabled == false || gIsFullScreen == false || gMidLineEnabled == true && input.sceneUV.x < (gMidLine - 0.002))
    {
        float3 vAdd = (0.1, 0.1, 0.1); // just a float4 for use later
	
        float3 colour = SceneTexture.Sample(PointSample, input.sceneUV); //this takes our sampler and turns the rgba into floats between 0 and 1
    
        colour += SceneTexture.Sample(PointSample, input.sceneUV.xy);
        colour += SceneTexture.Sample(PointSample, input.sceneUV.xy);
        colour += SceneTexture.Sample(PointSample, input.sceneUV.xy);
    
        if (((colour.r + colour.g + colour.b) / 3) < 1.2) // if the pixel is bright leave it bright (lowering this will increase the effect of lights)
        {
            colour = colour / 4; //otherwise set it to an average color of the 4 images we just added together
        }

        finalColour = colour + vAdd; //adds the floats together

        finalColour.g = ((colour.r + colour.g + colour.b) / 3); // sets green the the average of rgb
        finalColour.r = 0; //removes red and blue colors
        finalColour.b = 0;
    
    
    }
    else if (input.sceneUV.x >= (gMidLine + 0.002))
    {
        finalColour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    }
    
    float outputAlpha = 1.0f;
    return float4(finalColour, outputAlpha); // brighten the final image and return it
    
}
