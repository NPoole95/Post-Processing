//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here
#include "ColourRGBA.h" 

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <array>
#include <sstream>
#include <memory>


//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------

//********************
// Available post-processes 
enum class PostProcess
{
	None,
	Copy,
	Tint,
	GreyNoise,
	Burn,
	Distort,
	Spiral,
	HeatHaze,
	HueTint,
	BlurH,
	BlurV,
	Underwater,
	Inverted,
	NightVision,
	Retro,
	Bloom1,
	Bloom2,
	DepthOfField
};

enum class PostProcessMode
{
	Fullscreen,
	Area,
	Polygon,
};

struct ProcessAndMode
{
	PostProcess process;
	PostProcessMode mode;
};

struct Constants
{
	// Tint post-process settings
	CVector3 tintTopColour = { 0, 0, 1 };
	CVector3 tintBottomColour = { 0, 1, 0 };

	// HueTint post-process settings
	float    HueWiggle = 0.0f;
	float    HueWiggleSpeed = 0.0f;

	// Underwater post-process settiings
	CVector3 waterColour = { 0.0f,0.0f,0.0f };
	float    Wiggle = 0.0f;
	float    WiggleSpeed = 0.0f;

	// Bloom post processing effects
	float bloomThreshold = 0.8f;

	// DOF post processing effects
	float depthThreshold = 0.0f;

	// Blur post-process settings
	int blurStrength = 7;
};

std::vector<Constants> gConstantsList;

auto gCurrentPostProcess = PostProcess::None;
auto gCurrentSecondPostProcess = PostProcess::None;
auto gCurrentPostProcessMode = PostProcessMode::Fullscreen;

std::vector<ProcessAndMode> gPostProcessList;

//********************


// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 1.5f;  // Radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // Units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)

// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;


// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
Mesh* gStarsMesh;
Mesh* gGroundMesh;
Mesh* gCubeMesh;
Mesh* gCrateMesh;
Mesh* gLightMesh;
Mesh* gWallMesh;

Model* gStars;
Model* gGround;
Model* gCube;
Model* gCrate;
Model* gWall;

Camera* gCamera;


// Store lights in an array in this exercise
const int NUM_LIGHTS = 2;
struct Light
{
	Model* model;
	CVector3 colour;
	float    strength;
};
Light gLights[NUM_LIGHTS];


// Additional light information
CVector3 gAmbientColour = { 0.3f, 0.3f, 0.4f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

ColourRGBA gBackgroundColor = { 0.3f, 0.3f, 0.4f, 1.0f };

// Variables controlling light1's orbiting of the cube
const float gLightOrbitRadius = 20.0f;
const float gLightOrbitSpeed = 0.7f;



//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants (settings) that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer* gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constants (settings) that change per-model (e.g. world matrix)
ID3D11Buffer* gPerModelConstantBuffer; // --"--

//**************************
PostProcessingConstants gPostProcessingConstants;       // As above, but constants (settings) for each post-process
ID3D11Buffer* gPostProcessingConstantBuffer; // --"--
//**************************


//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// DirectX objects controlling textures used in this lab
ID3D11Resource* gStarsDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gStarsDiffuseSpecularMapSRV = nullptr;
ID3D11Resource* gGroundDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;
ID3D11Resource* gCrateDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gCrateDiffuseSpecularMapSRV = nullptr;
ID3D11Resource* gCubeDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gCubeDiffuseSpecularMapSRV = nullptr;
ID3D11Resource* gWallDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gWallDiffuseSpecularMapSRV = nullptr;

ID3D11Resource* gLightDiffuseMap = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;



//****************************
// Post processing textures

// This texture will have the scene renderered on it. Then the texture is then used for post-processing
ID3D11Texture2D* gSceneTexture = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView* gSceneRenderTarget = nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gSceneTextureSRV = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

// This texture will have the scene renderered on it. Then the texture is then used for post-processing
ID3D11Texture2D* gSceneTextureOne = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView* gSceneRenderTargetCopy = nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gSceneTextureOneSRV = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

// This texture will have the scene renderered on it. Then the texture is then used for post-processing
ID3D11Texture2D* gSceneTextureTwo = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView* gSceneRenderTargetTwo = nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gSceneTextureTwoSRV = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)


// Additional textures used for specific post-processes
ID3D11Resource* gNoiseMap = nullptr;
ID3D11ShaderResourceView* gNoiseMapSRV = nullptr;
ID3D11Resource* gBurnMap = nullptr;
ID3D11ShaderResourceView* gBurnMapSRV = nullptr;
ID3D11Resource* gDistortMap = nullptr;
ID3D11ShaderResourceView* gDistortMapSRV = nullptr;


//****************************



//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
	////--------------- Load meshes ---------------////

	// Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
	try
	{
		gStarsMesh = new Mesh("Stars.x");
		gGroundMesh = new Mesh("Hills.x");
		gCubeMesh = new Mesh("Cube.x");
		gCrateMesh = new Mesh("CargoContainer.x");
		gLightMesh = new Mesh("Light.x");
		gWallMesh = new Mesh("Wall2.x");
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}


	////--------------- Load / prepare textures & GPU states ---------------////

	// Load textures and create DirectX objects for them
	// The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
	// texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
	// The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.
	if (!LoadTexture("Stars.jpg", &gStarsDiffuseSpecularMap, &gStarsDiffuseSpecularMapSRV) ||
		!LoadTexture("GrassDiffuseSpecular.dds", &gGroundDiffuseSpecularMap, &gGroundDiffuseSpecularMapSRV) ||
		!LoadTexture("StoneDiffuseSpecular.dds", &gCubeDiffuseSpecularMap, &gCubeDiffuseSpecularMapSRV) ||
		!LoadTexture("CargoA.dds", &gCrateDiffuseSpecularMap, &gCrateDiffuseSpecularMapSRV) ||
		!LoadTexture("brick_35.jpg", &gWallDiffuseSpecularMap, &gWallDiffuseSpecularMapSRV) ||
		!LoadTexture("Flare.jpg", &gLightDiffuseMap, &gLightDiffuseMapSRV) ||
		!LoadTexture("Noise.png", &gNoiseMap, &gNoiseMapSRV) ||
		!LoadTexture("Burn.png", &gBurnMap, &gBurnMapSRV) ||
		!LoadTexture("Distort.png", &gDistortMap, &gDistortMapSRV))
	{
		gLastError = "Error loading textures";
		return false;
	}


	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}


	////--------------- Prepare shaders and constant buffers to communicate with them ---------------////

	// Load the shaders required for the geometry we will use (see Shader.cpp / .h)
	if (!LoadShaders())
	{
		gLastError = "Error loading shaders";
		return false;
	}

	// Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
	// These allow us to pass data from CPU to shaders such as lighting information or matrices
	// See the comments above where these variable are declared and also the UpdateScene function
	gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
	gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));
	gPostProcessingConstantBuffer = CreateConstantBuffer(sizeof(gPostProcessingConstants));
	if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr || gPostProcessingConstantBuffer == nullptr)
	{
		gLastError = "Error creating constant buffers";
		return false;
	}



	//********************************************
	//**** Create Scene Texture

	// We will render the scene to this texture instead of the back-buffer (screen), then we post-process the texture onto the screen
	// This is exactly the same code we used in the graphics module when we were rendering the scene onto a cube using a texture

	// Using a helper function to load textures from files above. Here we create the scene texture manually
	// as we are creating a special kind of texture (one that we can render to). Many settings to prepare:
	D3D11_TEXTURE2D_DESC sceneTextureDesc = {};
	sceneTextureDesc.Width = gViewportWidth;  // Full-screen post-processing - use full screen size for texture
	sceneTextureDesc.Height = gViewportHeight;
	sceneTextureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	sceneTextureDesc.ArraySize = 1;
	sceneTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	sceneTextureDesc.SampleDesc.Count = 1;
	sceneTextureDesc.SampleDesc.Quality = 0;
	sceneTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	sceneTextureDesc.CPUAccessFlags = 0;
	sceneTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &gSceneTexture)))
	{
		gLastError = "Error creating scene texture";
		return false;
	}
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &gSceneTextureOne)))
	{
		gLastError = "Error creating scene texture";
		return false;
	}
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &gSceneTextureTwo)))
	{
		gLastError = "Error creating scene texture";
		return false;
	}

	// We created the scene texture above, now we get a "view" of it as a render target, i.e. get a special pointer to the texture that
	// we use when rendering to it (see RenderScene function below)
	if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneTexture, NULL, &
		gSceneRenderTarget)))
	{
		gLastError = "Error creating scene render target view";
		return false;
	}
	if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneTextureOne, NULL, &gSceneRenderTargetCopy)))
	{
		gLastError = "Error creating scene render target view";
		return false;
	}
	if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneTextureTwo, NULL, &gSceneRenderTargetTwo)))
	{
		gLastError = "Error creating scene render target Two view";
		return false;
	}

	// We also need to send this texture (resource) to the shaders. To do that we must create a shader-resource "view"
	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc = {};
	srDesc.Format = sceneTextureDesc.Format;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneTexture, &srDesc, &gSceneTextureSRV)))
	{
		gLastError = "Error creating scene shader resource view";
		return false;
	}
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneTextureOne, &srDesc, &gSceneTextureOneSRV)))
	{
		gLastError = "Error creating scene shader resource view";
		return false;
	}
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneTextureTwo, &srDesc, &gSceneTextureTwoSRV)))
	{
		gLastError = "Error creating scene shader resource view";
		return false;
	}


	return true;
}


// Prepare the scene
// Returns true on success
bool InitScene()
{
	////--------------- Set up scene ---------------////

	gStars = new Model(gStarsMesh);
	gGround = new Model(gGroundMesh);
	gCube = new Model(gCubeMesh);
	gCrate = new Model(gCrateMesh);
	gWall = new Model(gWallMesh);

	// Initial positions
	gCube->SetPosition({ 42, 5, -10 });
	gCube->SetRotation({ 0.0f, ToRadians(-110.0f), 0.0f });
	gCube->SetScale(1.5f);
	gCrate->SetPosition({ -10, 0, 90 });
	gCrate->SetRotation({ 0.0f, ToRadians(40.0f), 0.0f });
	gCrate->SetScale(6.0f);
	gStars->SetScale(8000.0f);
	gWall->SetPosition({ 50, 0, -50 });
	gWall->SetRotation({ 0.0f, ToRadians(-180.0f), 0.0f });
	gWall->SetScale(50.0f);



	// Light set-up - using an array this time
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		gLights[i].model = new Model(gLightMesh);
	}

	gLights[0].colour = { 0.8f, 0.8f, 1.0f };
	gLights[0].strength = 10;
	gLights[0].model->SetPosition({ 30, 10, 0 });
	gLights[0].model->SetScale(pow(gLights[0].strength, 1.0f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.

	gLights[1].colour = { 1.0f, 0.8f, 0.2f };
	gLights[1].strength = 40;
	gLights[1].model->SetPosition({ -70, 30, 100 });
	gLights[1].model->SetScale(pow(gLights[1].strength, 1.0f));


	////--------------- Set up camera ---------------////

	gCamera = new Camera();
	gCamera->SetPosition({ 25, 18, -45 });
	gCamera->SetRotation({ ToRadians(10.0f), ToRadians(7.0f), 0.0f });

	// An array of four points in world space - a tapered square centred at the origin

	ProcessAndMode window1 = { PostProcess::NightVision, PostProcessMode::Polygon };
	Constants constants1 = Constants();
	gConstantsList.push_back(constants1);
	ProcessAndMode window2 = { PostProcess::Underwater, PostProcessMode::Polygon };
	Constants constants2 = Constants();
	gConstantsList.push_back(constants2);
	ProcessAndMode window3 = { PostProcess::Inverted, PostProcessMode::Polygon };
	Constants constants3 = Constants();
	gConstantsList.push_back(constants3);
	ProcessAndMode window4 = { PostProcess::HueTint, PostProcessMode::Polygon };
	Constants constants4 = Constants();
	gConstantsList.push_back(constants4);

	gPostProcessingConstants.tintTopColour = { 0, 0, 1 };
	gPostProcessingConstants.tintBottomColour = { 0, 1, 0 };
	gPostProcessingConstants.blurStrength = 13;
	gPostProcessingConstants.MidLine = 0.5f;
	//gPostProcessingConstants.MidLineEnabled = true;
	//gPostProcessingConstants.IsFullScreen = true;

	gPostProcessList.push_back(window1);
	gPostProcessList.push_back(window2);
	gPostProcessList.push_back(window3);
	gPostProcessList.push_back(window4);

	return true;
}


// Release the geometry and scene resources created above
void ReleaseResources()
{
	ReleaseStates();

	if (gSceneTextureSRV)              gSceneTextureSRV->Release();
	if (gSceneRenderTarget)            gSceneRenderTarget->Release();
	if (gSceneTexture)                 gSceneTexture->Release();

	if (gSceneTextureOneSRV)              gSceneTextureOneSRV->Release();
	if (gSceneRenderTargetCopy)            gSceneRenderTargetCopy->Release();
	if (gSceneTextureOne)                 gSceneTextureOne->Release();

	if (gSceneTextureTwoSRV)              gSceneTextureTwoSRV->Release();
	if (gSceneRenderTargetTwo)            gSceneRenderTargetTwo->Release();
	if (gSceneTextureTwo)                 gSceneTextureTwo->Release();

	if (gDistortMapSRV)                gDistortMapSRV->Release();
	if (gDistortMap)                   gDistortMap->Release();
	if (gBurnMapSRV)                   gBurnMapSRV->Release();
	if (gBurnMap)                      gBurnMap->Release();
	if (gNoiseMapSRV)                  gNoiseMapSRV->Release();
	if (gNoiseMap)                     gNoiseMap->Release();

	if (gLightDiffuseMapSRV)           gLightDiffuseMapSRV->Release();
	if (gLightDiffuseMap)              gLightDiffuseMap->Release();
	if (gCrateDiffuseSpecularMapSRV)   gCrateDiffuseSpecularMapSRV->Release();
	if (gCrateDiffuseSpecularMap)      gCrateDiffuseSpecularMap->Release();
	if (gCubeDiffuseSpecularMapSRV)    gCubeDiffuseSpecularMapSRV->Release();
	if (gCubeDiffuseSpecularMap)       gCubeDiffuseSpecularMap->Release();
	if (gWallDiffuseSpecularMapSRV)    gWallDiffuseSpecularMapSRV->Release();
	if (gWallDiffuseSpecularMap)       gWallDiffuseSpecularMap->Release();
	if (gGroundDiffuseSpecularMapSRV)  gGroundDiffuseSpecularMapSRV->Release();
	if (gGroundDiffuseSpecularMap)     gGroundDiffuseSpecularMap->Release();
	if (gStarsDiffuseSpecularMapSRV)   gStarsDiffuseSpecularMapSRV->Release();
	if (gStarsDiffuseSpecularMap)      gStarsDiffuseSpecularMap->Release();

	if (gPostProcessingConstantBuffer)  gPostProcessingConstantBuffer->Release();
	if (gPerModelConstantBuffer)        gPerModelConstantBuffer->Release();
	if (gPerFrameConstantBuffer)        gPerFrameConstantBuffer->Release();

	ReleaseShaders();

	// See note in InitGeometry about why we're not using unique_ptr and having to manually delete
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		delete gLights[i].model;  gLights[i].model = nullptr;
	}
	delete gCamera;  gCamera = nullptr;
	delete gCrate;   gCrate = nullptr;
	delete gCube;    gCube = nullptr;
	delete gWall;    gWall = nullptr;
	delete gGround;  gGround = nullptr;
	delete gStars;   gStars = nullptr;

	delete gLightMesh;   gLightMesh = nullptr;
	delete gCrateMesh;   gCrateMesh = nullptr;
	delete gCubeMesh;    gCubeMesh = nullptr;
	delete gWallMesh;    gWallMesh = nullptr;
	delete gGroundMesh;  gGroundMesh = nullptr;
	delete gStarsMesh;   gStarsMesh = nullptr;
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

void RenderDepthBufferFromCamera(Camera* camera)
{

	gPerFrameConstants.cameraMatrix = camera->WorldMatrix();
	gPerFrameConstants.viewMatrix = camera->ViewMatrix();
	gPerFrameConstants.projectionMatrix = camera->ProjectionMatrix();
	gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
	UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

	// Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
	gD3DContext->VSSetConstantBuffers(1, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
	gD3DContext->PSSetConstantBuffers(1, 1, &gPerFrameConstantBuffer);

	// Use special depth-only rendering shaders
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gDepthOnlyPixelShader, nullptr, 0);

	// States - no blending, normal depth buffer and culling
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
	gD3DContext->RSSetState(gCullBackState);

	// Render models - no state changes required between each object in this situation (no textures used in this step)
	gGround->Render();
	gCrate->Render();
	gCube->Render();
	gWall->Render();
}

// Render everything in the scene from the given camera
void RenderSceneFromCamera(Camera* camera)
{
	//IMGUI
	//*******************************
	// Prepare ImGUI for this frame
	//*******************************

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Set camera matrices in the constant buffer and send over to GPU
	gPerFrameConstants.cameraMatrix = camera->WorldMatrix();
	gPerFrameConstants.viewMatrix = camera->ViewMatrix();
	gPerFrameConstants.projectionMatrix = camera->ProjectionMatrix();
	gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
	UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

	// Indicate that the constant buffer we just updated is for use in the vertex shader (VS), geometry shader (GS) and pixel shader (PS)
	gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
	gD3DContext->GSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
	gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);

	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);


	////--------------- Render ordinary models ---------------///

	// Select which shaders to use next
	gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)

	// States - no blending, normal depth buffer and back-face culling (standard set-up for opaque models)
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
	gD3DContext->RSSetState(gCullBackState);

	// Render lit models, only change textures for each onee
	gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

	gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gGround->Render();

	gD3DContext->PSSetShaderResources(0, 1, &gCrateDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gCrate->Render();

	gD3DContext->PSSetShaderResources(0, 1, &gCubeDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gCube->Render();

	gD3DContext->PSSetShaderResources(0, 1, &gWallDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gWall->Render();



	////--------------- Render sky ---------------////

	// Select which shaders to use next
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gTintedTexturePixelShader, nullptr, 0);

	// Using a pixel shader that tints the texture - don't need a tint on the sky so set it to white
	gPerModelConstants.objectColour = { 1, 1, 1 };

	// Stars point inwards
	gD3DContext->RSSetState(gCullNoneState);

	// Render sky
	gD3DContext->PSSetShaderResources(0, 1, &gStarsDiffuseSpecularMapSRV);
	gStars->Render();



	////--------------- Render lights ---------------////

	// Select which shaders to use next (actually same as before, so we could skip this)
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gTintedTexturePixelShader, nullptr, 0);

	// Select the texture and sampler to use in the pixel shader
	gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV); // First parameter must match texture slot number in the shaer

	// States - additive blending, read-only depth buffer and no culling (standard set-up for blending)
	gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);

	// Render all the lights in the array
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		gPerModelConstants.objectColour = gLights[i].colour; // Set any per-model constants apart from the world matrix just before calling render (light colour here)
		gLights[i].model->Render();
	}


}



//**************************

// Select the appropriate shader plus any additional textures required for a given post-process
// Helper function shared by full-screen, area and polygon post-processing functions below
void SelectPostProcessShaderAndTextures(PostProcess postProcess, float frameTime, int i)
{
	if (postProcess == PostProcess::Copy)
	{
		gD3DContext->PSSetShader(gCopyPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcess::Tint)
	{
		gPostProcessingConstants.tintBottomColour = gConstantsList[i].tintBottomColour;
		gPostProcessingConstants.tintTopColour = gConstantsList[i].tintTopColour;
		gD3DContext->PSSetShader(gTintPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcess::GreyNoise)
	{
		gD3DContext->PSSetShader(gGreyNoisePostProcess, nullptr, 0);

		// Noise scaling adjusts how fine the noise is.
		const float grainSize = 140; // Fineness of the noise grain
		gPostProcessingConstants.noiseScale = { gViewportWidth / grainSize, gViewportHeight / grainSize };

		// The noise offset is randomised to give a constantly changing noise effect (like tv static)
		gPostProcessingConstants.noiseOffset = { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };

		// Give pixel shader access to the noise texture
		gD3DContext->PSSetShaderResources(1, 1, &gNoiseMapSRV);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcess::Burn)
	{
		gD3DContext->PSSetShader(gBurnPostProcess, nullptr, 0);

		// Set and increase the burn level (cycling back to 0 when it reaches 1.0f)
		const float burnSpeed = 0.2f;
		gPostProcessingConstants.burnHeight = fmod(gPostProcessingConstants.burnHeight + burnSpeed * frameTime, 1.0f);

		// Give pixel shader access to the burn texture (basically a height map that the burn level ascends)
		gD3DContext->PSSetShaderResources(1, 1, &gBurnMapSRV);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcess::Distort)
	{
		gD3DContext->PSSetShader(gDistortPostProcess, nullptr, 0);

		// Give pixel shader access to the distortion texture (containts 2D vectors (in R & G) to shift the texture UVs to give a cut-glass impression)
		gD3DContext->PSSetShaderResources(1, 1, &gDistortMapSRV);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcess::Spiral)
	{
		gD3DContext->PSSetShader(gSpiralPostProcess, nullptr, 0);

		static float spiral = 0.0f;
		const float spiralSpeed = 1.0f;

		// Set and increase the amount of spiral - use a tweaked cos wave to animate
		gPostProcessingConstants.spiralLevel = ((1.0f - cos(spiral)) * 4.0f);
		spiral += spiralSpeed * frameTime;

		gD3DContext->PSSetShader(gSpiralPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcess::HeatHaze)
	{
		gD3DContext->PSSetShader(gHeatHazePostProcess, nullptr, 0);

		gPostProcessingConstants.heatHazeTimer += frameTime;
	}

	else if (postProcess == PostProcess::HueTint)
	{
		gD3DContext->PSSetShader(gHueTintPostProcess, nullptr, 0);

		gPostProcessingConstants.HueWiggle += gConstantsList[i].HueWiggleSpeed * frameTime;
	}

	else if (postProcess == PostProcess::Underwater)
	{
		gD3DContext->PSSetShader(gUnderwaterPostProcess, nullptr, 0);
		gPostProcessingConstants.waterColour = { 0.2, 0.4, 1 };
		gConstantsList[i].Wiggle += gConstantsList[i].WiggleSpeed * frameTime;
		gPostProcessingConstants.Wiggle = gConstantsList[i].Wiggle;

	}

	else if (postProcess == PostProcess::Inverted)
	{
		gD3DContext->PSSetShader(gInvertedColourPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcess::NightVision)
	{
		gD3DContext->PSSetShader(gNightVisionPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcess::BlurH)
	{
		gD3DContext->PSSetShader(gBlurHPostProcess, nullptr, 0);

		/*int blurStrength = 5;

		std::array<float,300> kernels;
		std::array<float, 300> weights;*/
		//const int arraySize = 100;

		gPostProcessingConstants.blurStrength = gConstantsList[i].blurStrength;

		if (gPostProcessingConstants.blurStrength % 2 == 0)
		{
			gPostProcessingConstants.blurStrength -= 1;
		}

		double sigma = 40;

		double mean = (gPostProcessingConstants.blurStrength - 1) / 2;
		double sum = 0.0; // For accumulating the kernel values

		for (int x = 0; x < gPostProcessingConstants.blurStrength; ++x)
		{
			gPostProcessingConstants.weight[x].x = exp(-0.5 * (pow((x - mean) / sigma, 2.0) + pow((x - mean) / sigma, 2.0))) / (2 * PI * sigma * sigma);

			// Accumulate the kernel values
			sum += gPostProcessingConstants.weight[x].x;
		}

		// Normalize the kernel
		for (int x = 0; x < gPostProcessingConstants.blurStrength; ++x)
		{
			gPostProcessingConstants.weight[x].x /= sum;
		}

		//int kernel[arraySize];
		int midpoint = (gPostProcessingConstants.blurStrength - 1) / 2;

		gPostProcessingConstants.kernel[midpoint].x = 0;
		for (int x = 1; x <= midpoint; x++)
		{
			gPostProcessingConstants.kernel[midpoint + x].x = x;
			gPostProcessingConstants.kernel[midpoint - x].x = -x;
		}
	}
	else if (postProcess == PostProcess::BlurV)
	{
		gD3DContext->PSSetShader(gBlurVPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Retro)
	{
		gD3DContext->PSSetShader(gRetroPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Bloom1)
	{
		gPostProcessingConstants.bloomThreshold = gConstantsList[i].bloomThreshold;
		gD3DContext->PSSetShader(gBloom1PostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Bloom2)
	{
		gD3DContext->PSSetShader(gBloom2PostProcess, nullptr, 0);
		gD3DContext->PSSetShaderResources(1, 1, &gSceneTextureOneSRV);
	}
	else if (postProcess == PostProcess::DepthOfField)
	{
		gPostProcessingConstants.depthThreshold = gConstantsList[i].depthThreshold;
		gD3DContext->PSSetShader(gDepthOfFieldPostProcess, nullptr, 0);
		// gD3DContext->PSSetShaderResources(2, 1, &gDepthShaderView);
		// gD3DContext->PSSetSamplers(2, 1, &gPointSampler);
	}

}



// Perform a full-screen post process from "scene texture" to back buffer
void FullScreenPostProcess(PostProcess postProcess, float frameTime, int i)
{

	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


	// States - no blending, don't write to depth buffer and ignore back-face culling
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);


	// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
	gD3DContext->IASetInputLayout(NULL); // No vertex data
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);
	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all
	if (i % 2 == 0)
	{
		gD3DContext->OMSetRenderTargets(1, &gSceneRenderTargetTwo, gDepthStencil);
		gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureSRV);
		gD3DContext->PSSetSamplers(0, 1, &gPointSampler);
	}
	else
	{
		gD3DContext->OMSetRenderTargets(1, &gSceneRenderTarget, gDepthStencil);
		gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureTwoSRV);
		gD3DContext->PSSetSamplers(0, 1, &gPointSampler);
	}


	// Select shader and textures needed for the required post-processes (helper function above)
	SelectPostProcessShaderAndTextures(postProcess, frameTime, i);


	// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
	gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
	gPostProcessingConstants.area2DSize = { 1, 1 }; // Full size of screen
	gPostProcessingConstants.area2DDepth = 0;        // Depth buffer value for full screen is as close as possible


	// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);


	// Draw a quad
	gD3DContext->Draw(4, 0);

	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

	gD3DContext->Draw(4, 0);
}


void SaveBaseSceneTexture(int i)
{
	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


	// States - no blending, don't write to depth buffer and ignore back-face culling
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);


	// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
	gD3DContext->IASetInputLayout(NULL); // No vertex data
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);
	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all

	gD3DContext->OMSetRenderTargets(1, &gSceneRenderTargetCopy, gDepthStencil);
	gD3DContext->PSSetSamplers(0, 1, &gPointSampler);

	if (i % 2 == 0)
	{
		gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureSRV);
	}
	else
	{
		gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureTwoSRV);
	}


	gD3DContext->PSSetShader(gCopyPostProcess, nullptr, 0);

	// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
	gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
	gPostProcessingConstants.area2DSize = { 1, 1 }; // Full size of screen
	gPostProcessingConstants.area2DDepth = 0;        // Depth buffer value for full screen is as close as possible


	// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);

	// Draw a quad
	gD3DContext->Draw(4, 0);

	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

	gD3DContext->Draw(4, 0);
}

// Perform an area post process from "scene texture" to back buffer at a given point in the world, with a given size (world units)
void AreaPostProcess(PostProcess postProcess, CVector3 worldPoint, CVector2 areaSize, float frameTime, int i)
{
	// First perform a full-screen copy of the scene to back-buffer
	FullScreenPostProcess(PostProcess::Copy, frameTime, i);


	// Now perform a post-process of a portion of the scene to the back-buffer (overwriting some of the copy above)
	// Note: The following code relies on many of the settings that were prepared in the FullScreenPostProcess call above, it only
	//       updates a few things that need to be changed for an area process. If you tinker with the code structure you need to be
	//       aware of all the work that the above function did that was also preparation for this post-process area step

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess, frameTime, i);

	// Enable alpha blending - area effects need to fade out at the edges or the hard edge of the area is visible
	// A couple of the shaders have been updated to put the effect into a soft circle
	// Alpha blending isn't enabled for fullscreen and polygon effects so it doesn't affect those (except heat-haze, which works a bit differently)
	gD3DContext->OMSetBlendState(gAlphaBlendingState, nullptr, 0xffffff);


	// Use picking methods to find the 2D position of the 3D point at the centre of the area effect
	auto worldPointTo2D = gCamera->PixelFromWorldPt(worldPoint, gViewportWidth, gViewportHeight);
	CVector2 area2DCentre = { worldPointTo2D.x, worldPointTo2D.y };
	float areaDistance = worldPointTo2D.z;

	// Nothing to do if given 3D point is behind the camera
	if (areaDistance < gCamera->NearClip())  return;

	// Convert pixel coordinates to 0->1 coordinates as used by the shader
	area2DCentre.x /= gViewportWidth;
	area2DCentre.y /= gViewportHeight;



	// Using new helper function here - it calculates the world space units covered by a pixel at a certain distance from the camera.
	// Use this to find the size of the 2D area we need to cover the world space size requested
	CVector2 pixelSizeAtPoint = gCamera->PixelSizeInWorldSpace(areaDistance, gViewportWidth, gViewportHeight);
	CVector2 area2DSize = { areaSize.x / pixelSizeAtPoint.x, areaSize.y / pixelSizeAtPoint.y };

	// Again convert the result in pixels to a result to 0->1 coordinates
	area2DSize.x /= gViewportWidth;
	area2DSize.y /= gViewportHeight;



	// Send the area top-left and size into the constant buffer - the 2DQuad vertex shader will use this to create a quad in the right place
	gPostProcessingConstants.area2DTopLeft = area2DCentre - 0.5f * area2DSize; // Top-left of area is centre - half the size
	gPostProcessingConstants.area2DSize = area2DSize;

	// Manually calculate depth buffer value from Z distance to the 3D point and camera near/far clip values. Result is 0->1 depth value
	// We've never seen this full calculation before, it's occasionally useful. It is derived from the material in the Picking lecture
	// Having the depth allows us to have area effects behind normal objects
	gPostProcessingConstants.area2DDepth = gCamera->FarClip() * (areaDistance - gCamera->NearClip()) / (gCamera->FarClip() - gCamera->NearClip());
	gPostProcessingConstants.area2DDepth /= areaDistance;

	// Pass over this post-processing area to shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);


	// Draw a quad
	gD3DContext->Draw(4, 0);
}


// Perform an post process from "scene texture" to back buffer within the given four-point polygon and a world matrix to position/rotate/scale the polygon
void PolygonPostProcess(PostProcess postProcess, const std::array<CVector3, 4>& points, const CMatrix4x4& worldMatrix, float frameTime, int i)
{
	// First perform a full-screen copy of the scene to back-buffer
	FullScreenPostProcess(PostProcess::Copy, frameTime, i);


	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);
	if (i % 2 == 0)
	{
		gD3DContext->OMSetRenderTargets(1, &gSceneRenderTargetTwo, gDepthStencil);
		gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureSRV);
		gD3DContext->PSSetSamplers(0, 1, &gPointSampler);
	}
	else
	{
		gD3DContext->OMSetRenderTargets(1, &gSceneRenderTarget, gDepthStencil);
		gD3DContext->PSSetShaderResources(0, 1, &gSceneTextureTwoSRV);
		gD3DContext->PSSetSamplers(0, 1, &gPointSampler);
	}

	// Now perform a post-process of a portion of the scene to the back-buffer (overwriting some of the copy above)
	// Note: The following code relies on many of the settings that were prepared in the FullScreenPostProcess call above, it only
	//       updates a few things that need to be changed for an area process. If you tinker with the code structure you need to be
	//       aware of all the work that the above function did that was also preparation for this post-process area step

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess, frameTime, i);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < points.size(); ++i)
	{
		CVector4 modelPosition = CVector4(points[i], 1);
		CVector4 worldPosition = modelPosition * worldMatrix;
		CVector4 viewportPosition = worldPosition * gCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the polygon points to the shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);

	// Select the special 2D polygon post-processing vertex shader and draw the polygon
	gD3DContext->VSSetShader(g2DPolygonVertexShader, nullptr, 0);
	gD3DContext->Draw(4, 0);
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

	gD3DContext->Draw(4, 0);
}

void MergeTextures(float frameTime)
{

	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all
	gD3DContext->OMSetRenderTargets(1, /*MISSING, 2nd pass specify back buffer as render target (note: needs an &)*/&gBackBufferRenderTarget, gDepthStencil);


	// Give the pixel shader (post-processing shader) access to the scene texture 
	gD3DContext->PSSetShaderResources(0, 1, /* MISSING select the scene texture shader resource view (note: needs an &)*/&gSceneTextureOneSRV);
	gD3DContext->PSSetShaderResources(1, 1, /* MISSING select the scene texture shader resource view (note: needs an &)*/&gSceneTextureTwoSRV);
	gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)


	// Using special vertex shader than creates its own data for a full screen quad
	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


	// States - no blending, ignore depth buffer and culling
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gNoDepthBufferState, 0);
	gD3DContext->RSSetState(gCullNoneState);


	// No need to set vertex/index buffer (see fullscreen quad vertex shader), just indicate that the quad will be created as a triangle strip
	gD3DContext->IASetInputLayout(NULL); // No vertex data
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// Prepare custom settings for current post-process

	gD3DContext->PSSetShader(gMergeTextures, nullptr, 0);

	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);

	// Draw a quad
	gD3DContext->Draw( /*MISSING - Post-process pass renderes a quad*/ 4, 0);

	// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);
}
//**************************


// Rendering the scene
void RenderScene(float frameTime)
{
	//// Common settings ////

	// Set up the light information in the constant buffer
	// Don't send to the GPU yet, the function RenderSceneFromCamera will do that
	gPerFrameConstants.light1Colour = gLights[0].colour * gLights[0].strength;
	gPerFrameConstants.light1Position = gLights[0].model->Position();
	gPerFrameConstants.light2Colour = gLights[1].colour * gLights[1].strength;
	gPerFrameConstants.light2Position = gLights[1].model->Position();

	gPerFrameConstants.ambientColour = gAmbientColour;
	gPerFrameConstants.specularPower = gSpecularPower;
	gPerFrameConstants.cameraPosition = gCamera->Position();

	gPerFrameConstants.viewportWidth = static_cast<float>(gViewportWidth);
	gPerFrameConstants.viewportHeight = static_cast<float>(gViewportHeight);



	////--------------- Main scene rendering ---------------////

	// Set the target for rendering and select the main depth buffer.
	// If using post-processing then render to the scene texture, otherwise to the usual back buffer
	// Also clear the render target to a fixed colour and the depth buffer to the far distance
	// Setup the viewport to the size of the main window

	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(gViewportWidth);
	vp.Height = static_cast<FLOAT>(gViewportHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	gD3DContext->RSSetViewports(1, &vp);

	gD3DContext->OMSetRenderTargets(0, nullptr, gDepthStencil);
	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

	RenderDepthBufferFromCamera(gCamera);

	if (gPostProcessList.size() != 0)
	{
		gD3DContext->OMSetRenderTargets(1, &gSceneRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(gSceneRenderTarget, &gBackgroundColor.r);
	}
	else
	{
		gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
	}

	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

	gD3DContext->PSSetShaderResources(2, 1, &gDepthShaderView);
	gD3DContext->PSSetSamplers(2, 1, &gPointSampler);
	// Render the scene from the main camera
	RenderSceneFromCamera(gCamera);


	////--------------- Scene completion ---------------////


	// run the polygon post processing


	//polyMatrix = MatrixRotationY(ToRadians(1)) * polyMatrix;
	//SaveBaseSceneTexture();
	// Pass an array of 4 points and a matrix. Only supports 4 points.

	std::array<std::array<CVector3, 4>, 4> points;
	points[0] = { CVector3{22,25,-50}, {22,5,-50}, {33,25,-50}, {33,5,-50} }; // C++ strangely needs an extra pair of {} here... only for std:array...
	points[1] = { CVector3{36,25,-50}, {36,5,-50}, {49,25,-50}, {49,5,-50} }; // C++ strangely needs an extra pair of {} here... only for std:array...
	points[2] = { CVector3{50,25,-50}, {50,5,-50}, {63,25,-50}, {63,5,-50} }; // C++ strangely needs an extra pair of {} here... only for std:array...
	points[3] = { CVector3{64,25,-50}, {64,5,-50}, {78,25,-50}, {78,5,-50} }; // C++ strangely needs an extra pair of {} here... only for std:array...

	// A rotating matrix placing the model above in the scene
	static CMatrix4x4 polyMatrix = MatrixTranslation({ 0, 0, 0 });

	// Run any full screen post-processing steps
	///////////////////////////////////////////////////////////

	// Perform the polygon post processing first so the base scene can be saved in a texture
	gPostProcessingConstants.IsFullScreen = false;
	if (gPostProcessList.size() != 0)
	{
		for (int i = 0; i < gPostProcessList.size(); i++)
		{
			if (gPostProcessList[i].mode == PostProcessMode::Polygon)
			{
				PolygonPostProcess(gPostProcessList[i].process, points[i], polyMatrix, frameTime, i);
			}
		}
	}

	// Save the scene in a texture so it can be referenced for bloom effects
	/*SaveBaseSceneTexture();*/

	//Perform full screen post processing effects
	gPostProcessingConstants.IsFullScreen = true;
	if (gPostProcessList.size() != 0)
	{
		for (int i = 0; i < gPostProcessList.size(); i++)
		{
			if (gPostProcessList[i].mode == PostProcessMode::Fullscreen && gPostProcessList[i].process != PostProcess::Bloom1)
			{
				gCurrentPostProcess = gPostProcessList[i].process;

				FullScreenPostProcess(gPostProcessList[i].process, frameTime, i);
			}
		}
	}

	if (gPostProcessList.size() != 0)
	{
		for (int i = 0; i < gPostProcessList.size(); i++)
		{
			if (gPostProcessList[i].process == PostProcess::Bloom1)
			{

				int j = i;
				SaveBaseSceneTexture(i);

				gCurrentPostProcess = gPostProcessList[i].process;
				FullScreenPostProcess(gPostProcessList[i].process, frameTime, j);
				j++;

				//int temp = gPostProcessingConstants.blurStrength;
				gConstantsList[i].blurStrength = 90;

				/////////////////

				//double sigma = 40;
				//const int arraySize = 100;
				//// double weight[arraySize];
				////double mean = arraySize / 2;
				//double mean = (gConstantsList[i].blurStrength - 1) / 2;
				//double sum = 0.0; // For accumulating the kernel values

				//for (int x = 0; x < gConstantsList[i].blurStrength; ++x)
				//{
				//	gPostProcessingConstants.weight[x].x = exp(-0.5 * (pow((x - mean) / sigma, 2.0) + pow((x - mean) / sigma, 2.0))) / (2 * PI * sigma * sigma);

				//	// Accumulate the kernel values
				//	sum += gPostProcessingConstants.weight[x].x;
				//}

				//// Normalize the kernel
				//for (int x = 0; x < gConstantsList[i].blurStrength; ++x)
				//{
				//	gPostProcessingConstants.weight[x].x /= sum;
				//}

				////int kernel[arraySize];
				//int midpoint = (gConstantsList[i].blurStrength - 1) / 2;

				//gPostProcessingConstants.kernel[midpoint].x = 0;
				//for (int x = 1; x <= midpoint; x++)
				//{
				//	gPostProcessingConstants.kernel[midpoint + x].x = x;
				//	gPostProcessingConstants.kernel[midpoint - x].x = -x;
				//}

				//gPostProcessingConstants.blurStrength = gConstantsList[i].blurStrength;
				/////////////

				gCurrentPostProcess = PostProcess::BlurH;
				FullScreenPostProcess(PostProcess::BlurH, frameTime, j);
				j++;

				gCurrentPostProcess = PostProcess::BlurV;
				FullScreenPostProcess(PostProcess::BlurV, frameTime, j);
				j++;

				/*gPostProcessingConstants.blurStrength = temp;*/

				gCurrentPostProcess = PostProcess::Bloom2;
				FullScreenPostProcess(PostProcess::Bloom2, frameTime, j);
				j++;

			    FullScreenPostProcess(PostProcess::Copy, frameTime, i); // this copy is used to make the number of calls Odd so that bouncing between the two textures lines up again
			}
		}
	}




	//IMGUI
	//*******************************
	// Draw ImGUI interface
	//*******************************
	// You can draw ImGUI elements at any time between the frame preparation code at the top
	// of this function, and the finalisation code below

	//ImGui::ShowDemoWindow();

	int windowNumber = 0;
	std::string str;

	ImGui::Begin("Post Process Controls", 0, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::BeginGroup();
	ImGui::Text("Light controls:");
	ImGui::SliderFloat("Light 1 Strength", &gLights[0].strength, 5, 20);
	ImGui::SliderFloat("Light 2 Strength", &gLights[1].strength, 30, 80);
	ImGui::EndGroup();

	ImGui::BeginGroup();
	ImGui::Text("Window effect controls:");
	for (int i = 0; i < gPostProcessList.size(); i++)
	{
		if (gPostProcessList[i].mode == PostProcessMode::Polygon)
		{
			switch (gPostProcessList[i].process) {
			case PostProcess::Tint:
			{
				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Top Colour ";
				const char* name = str.c_str();
				ImGui::ColorPicker3(name, &gConstantsList[i].tintTopColour.x);

				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Bottom Colour ";
				name = str.c_str();
				ImGui::ColorPicker3(name, &gConstantsList[i].tintBottomColour.x);
			}
				break;
			case PostProcess::HueTint:
			{
				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Hue Speed";
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].HueWiggleSpeed, 1, 5);
			}
				break;
			case PostProcess::BlurH:
			{
				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Blur Strength";
				const char* name = str.c_str();
				ImGui::SliderInt(name, &gConstantsList[i].blurStrength, 7, 21);
			}
				break;
			case PostProcess::Underwater:
			{
				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Water Wiggle Strength";
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].WiggleSpeed, 1, 5);
			}
				break;
			case PostProcess::Burn:
				break;
			case PostProcess::Distort:
				break;
			case PostProcess::GreyNoise:
				break;
			case PostProcess::Spiral:
				break;
			case PostProcess::Retro:
				break;
			case PostProcess::Bloom1:
			{
				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Bloom Threshold";
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].bloomThreshold, 0.0f, 2.0f);
			}
				break;
			case PostProcess::Bloom2:
				break;
			case PostProcess::DepthOfField:
			{
				str = "Window ";
				str += std::to_string(windowNumber);
				str += " Depth Threshold";
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].depthThreshold, 0.0f, 1.0f);
			}
				break;
			}
			windowNumber++;
		}
	}
	ImGui::EndGroup();

	ImGui::BeginGroup();
	ImGui::Text("Screen effect controls:");
	ImGui::Checkbox("Enable Midline", &gPostProcessingConstants.MidLineEnabled);
	if (gPostProcessingConstants.MidLineEnabled)
	{
		ImGui::SliderFloat("Mid Position", &gPostProcessingConstants.MidLine, 0.0f, 1.0f);
	}

	int tintNumber=0;
	int hueNumber=0;
	int blurNumber=0;
	int UWNumber=0;
	int bloomNumber=0;
	int depthNumber=0;

	for (int i = 0; i < gPostProcessList.size(); i++)
	{
		if (gPostProcessList[i].mode == PostProcessMode::Fullscreen)
		{
			switch (gPostProcessList[i].process) {
			case PostProcess::Tint:
			{
				str = "Top Colour ";
				str += std::to_string(tintNumber);
				const char* name = str.c_str();
				ImGui::ColorPicker3(name, &gConstantsList[i].tintTopColour.x);

				str = "Bottom Colour ";
				str += std::to_string(tintNumber);
				name = str.c_str();
				ImGui::ColorPicker3(name, &gConstantsList[i].tintBottomColour.x);
				tintNumber++;
			}
				break;
			case PostProcess::HueTint:
			{
				str = "Hue Speed ";
				str += std::to_string(hueNumber);
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].HueWiggleSpeed, 1, 5); 
				hueNumber++;
			}
			break;
			case PostProcess::BlurH:
			{
				if (blurNumber == 1)
				{
					int g = 1;
				}
				str = "Blur Strength ";
				str += std::to_string(blurNumber);
				const char* name = str.c_str();
				ImGui::SliderInt(name, &gConstantsList[i].blurStrength, 7, 21);
				blurNumber++;
			}
				break;
			case PostProcess::BlurV:
				 break;
			case PostProcess::Underwater:
			{
				str = "Water Wiggle Strength ";
				str += std::to_string(UWNumber);
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].WiggleSpeed, 1, 5);
				UWNumber++;
			}
				break;
			case PostProcess::Burn:
				break;
			case PostProcess::Distort:
				break;
			case PostProcess::GreyNoise:
				break;
			case PostProcess::Spiral:
				break;
			case PostProcess::Retro:
				break;
			case PostProcess::Bloom1:
			{
				str = "Bloom Threshold ";
				str += std::to_string(bloomNumber);
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].bloomThreshold, 0.0f, 2.0f);
				if (gConstantsList[i].bloomThreshold != 0.0f)
				{
					int g = 4;
				}
				bloomNumber++;
			}
				break;
			case PostProcess::Bloom2:
				break;
			case PostProcess::DepthOfField:
			{
				str = "Depth Threshold ";
				str += std::to_string(depthNumber);
				const char* name = str.c_str();
				ImGui::SliderFloat(name, &gConstantsList[i].depthThreshold, 0.0f, 1.0f);
				depthNumber++;
			}
				break;
			}
		}
	}
	ImGui::EndGroup();
	ImGui::End();



	//*******************************



	////--------------- Scene completion ---------------////

	//IMGUI
	//*******************************
	// Finalise ImGUI for this frame
	//*******************************
	ImGui::Render();
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);


	// When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
	// Set first parameter to 1 to lock to vsync
	gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------


// Update models and camera. frameTime is the time passed since the last frame
void UpdateScene(float frameTime)
{
	//***********

	/*if (KeyHit(Key_F1))  gCurrentPostProcessMode = PostProcessMode::Fullscreen;
	if (KeyHit(Key_F2))  gCurrentPostProcessMode = PostProcessMode::Area;
	if (KeyHit(Key_F3))  gCurrentPostProcessMode = PostProcessMode::Polygon;*/

	if (KeyHit(Key_1))
	{
		ProcessAndMode process = { PostProcess::HueTint, PostProcessMode::Fullscreen };
		gPostProcessList.push_back(process);
		gPostProcessingConstants.tintTopColour = { 0, 0, 1 };
		gPostProcessingConstants.tintBottomColour = { 0, 1, 0 };
		Constants constants = Constants(); gConstantsList.push_back(constants);
	}
	else if (KeyHit(Key_2)) { ProcessAndMode process = { PostProcess::BlurH, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); process = { PostProcess::BlurV, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants); gConstantsList.push_back(constants);
	}
	else if (KeyHit(Key_3)) { ProcessAndMode process = { PostProcess::Underwater, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants); }
	else if (KeyHit(Key_4)) { ProcessAndMode process = { PostProcess::Distort, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_5)) { ProcessAndMode process = { PostProcess::Spiral, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_6)) { ProcessAndMode process = { PostProcess::HeatHaze, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_7)) { ProcessAndMode process = { PostProcess::Burn, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_8)) { ProcessAndMode process = { PostProcess::DepthOfField, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants); }
	else if (KeyHit(Key_I)) { ProcessAndMode process = { PostProcess::Inverted, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_N)) { ProcessAndMode process = { PostProcess::NightVision, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_T)) { ProcessAndMode process = { PostProcess::Tint, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants); }
	else if (KeyHit(Key_R)) { ProcessAndMode process = { PostProcess::Retro, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_G)) { ProcessAndMode process = { PostProcess::GreyNoise, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); gConstantsList.push_back(constants);}
	else if (KeyHit(Key_B)) { ProcessAndMode process = { PostProcess::Bloom1, PostProcessMode::Fullscreen }; gPostProcessList.push_back(process); Constants constants = Constants(); constants.blurStrength = 90; gConstantsList.push_back(constants); gConstantsList.push_back(constants); gConstantsList.push_back(constants); gConstantsList.push_back(constants); }
	else if (KeyHit(Key_0))
	{
		gConstantsList.clear();
		gPostProcessList.clear();
		gCurrentPostProcess = PostProcess::None;
		gCurrentSecondPostProcess = PostProcess::None;
		gPostProcessingConstants.bloomThreshold = 1.3f;
		gPostProcessingConstants.blurStrength = 13;
		gPostProcessingConstants.MidLine = 0.5f;

		ProcessAndMode window1 = { PostProcess::NightVision, PostProcessMode::Polygon };
		Constants constants1 = Constants(); 
		gConstantsList.push_back(constants1);
		ProcessAndMode window2 = { PostProcess::Underwater, PostProcessMode::Polygon };
		Constants constants2 = Constants();
		gConstantsList.push_back(constants2);
		ProcessAndMode window3 = { PostProcess::Inverted, PostProcessMode::Polygon };
		Constants constants3 = Constants();
		gConstantsList.push_back(constants3);
		ProcessAndMode window4 = { PostProcess::HueTint, PostProcessMode::Polygon };
		Constants constants4 = Constants();
		gConstantsList.push_back(constants4);

		gPostProcessingConstants.tintTopColour = { 0, 0, 1 };
		gPostProcessingConstants.tintBottomColour = { 0, 1, 0 };

		gPostProcessList.push_back(window1);
		gPostProcessList.push_back(window2);
		gPostProcessList.push_back(window3);
		gPostProcessList.push_back(window4);
	}


	// Post processing settings - all data for post-processes is updated every frame whether in use or not (minimal cost)

	//// Colour for tint shader
	//gPostProcessingConstants.tintColour = { 1, 0, 0 };

	//// Noise scaling adjusts how fine the grey noise is.
	//const float grainSize = 140; // Fineness of the noise grain
	//gPostProcessingConstants.noiseScale  = { gViewportWidth / grainSize, gViewportHeight / grainSize };

	//// The noise offset is randomised to give a constantly changing noise effect (like tv static)
	//gPostProcessingConstants.noiseOffset = { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };

	//// Set and increase the burn level (cycling back to 0 when it reaches 1.0f)
	//const float burnSpeed = 0.2f;
	//gPostProcessingConstants.burnHeight = fmod(gPostProcessingConstants.burnHeight + burnSpeed * frameTime, 1.0f);

	//// Set the level of distortion
	//gPostProcessingConstants.distortLevel = 0.03f;

	//// Set and increase the amount of spiral - use a tweaked cos wave to animate
	//static float wiggle = 0.0f;
	//const float wiggleSpeed = 1.0f;
	//gPostProcessingConstants.spiralLevel = ((1.0f - cos(wiggle)) * 4.0f );
	//wiggle += wiggleSpeed * frameTime;

	//// Update heat haze timer
	//gPostProcessingConstants.heatHazeTimer += frameTime;

	////***********


	// Orbit one light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float lightRotate = 0.0f;
	static bool go = true;
	gLights[0].model->SetPosition({ 20 + cos(lightRotate) * gLightOrbitRadius, 10, 20 + sin(lightRotate) * gLightOrbitRadius });
	if (go)  lightRotate -= gLightOrbitSpeed * frameTime;
	if (KeyHit(Key_L))  go = !go;

	// Control of camera
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);

	// Toggle FPS limiting
	if (KeyHit(Key_P))  lockFPS = !lockFPS;

	// Show frame time / FPS in the window title //
	const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
	static float totalFrameTime = 0;
	static int frameCount = 0;
	totalFrameTime += frameTime;
	++frameCount;
	if (totalFrameTime > fpsUpdateTime)
	{
		// Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
		float avgFrameTime = totalFrameTime / frameCount;
		std::ostringstream frameTimeMs;
		frameTimeMs.precision(2);
		frameTimeMs << std::fixed << avgFrameTime * 1000;
		std::string windowTitle = "CO3303 Week 14: Area Post Processing - Frame Time: " + frameTimeMs.str() +
			"ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
		SetWindowTextA(gHWnd, windowTitle.c_str());
		totalFrameTime = 0;
		frameCount = 0;
	}
}
