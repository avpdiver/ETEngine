#include "stdafx.h"
#include "ShadedSceneRenderer.h"

#include <EtCore/Content/ResourceManager.h>

#include <Engine/GlobalRenderingSystems/GlobalRenderingSystems.h>
#include <Engine/GraphicsHelper/RenderScene.h>
#include <Engine/Graphics/Material.h>


namespace render {


//=======================
// Shaded Scene Renderer
//=======================

//---------------------------------
// ShadedSceneRenderer::GetCurrent
//
// Utility function to retrieve the scene renderer for the currently active viewport
//
ShadedSceneRenderer* ShadedSceneRenderer::GetCurrent()
{
	Viewport* const viewport = Viewport::GetCurrentViewport();
	if (viewport == nullptr)
	{
		return nullptr;
	}

	I_ViewportRenderer* const viewRenderer = viewport->GetViewportRenderer();
	if (viewRenderer == nullptr)
	{
		return nullptr;
	}

	ET_ASSERT(viewRenderer->GetType() == typeid(ShadedSceneRenderer));

	return static_cast<ShadedSceneRenderer*>(viewRenderer);
}

//--------------------------------------------------------------------------


//---------------------------------
// ShadedSceneRenderer::c-tor
//
ShadedSceneRenderer::ShadedSceneRenderer(render::Scene const* const renderScene)
	: I_ViewportRenderer()
	, m_RenderScene(renderScene)
{ }

//---------------------------------
// ShadedSceneRenderer::d-tor
//
// make sure all the singletons this system requires are uninitialized
//
ShadedSceneRenderer::~ShadedSceneRenderer()
{
	RenderingSystems::RemoveReference();
}

//-------------------------------------------
// ShadedSceneRenderer::InitRenderingSystems
//
// Create required buffers and subrendering systems etc
//
void ShadedSceneRenderer::InitRenderingSystems()
{
	RenderingSystems::AddReference();

	m_PostProcessing.Initialize();

	m_GBuffer.Initialize();
	m_GBuffer.Enable(true);

	m_SSR.Initialize();

	m_ClearColor = vec3(200.f / 255.f, 114.f / 255.f, 200.f / 255.f)*0.0f;

	m_SkyboxShader = ResourceManager::Instance()->GetAssetData<ShaderData>("FwdSkyboxShader.glsl"_hash);

	m_IsInitialized = true;
}

//---------------------------------
// ShadedSceneRenderer::OnResize
//
void ShadedSceneRenderer::OnResize(ivec2 const dim)
{
	m_Dimensions = dim;

	if (!m_IsInitialized)
	{
		return;
	}

	m_PostProcessing.~PostProcessingRenderer();
	new(&m_PostProcessing) PostProcessingRenderer();
	m_PostProcessing.Initialize();

	m_SSR.~ScreenSpaceReflections();
	new(&m_SSR) ScreenSpaceReflections();
	m_SSR.Initialize();
}

//---------------------------------
// ShadedSceneRenderer::OnRender
//
// Main scene drawing function
//
void ShadedSceneRenderer::OnRender(T_FbLoc const targetFb)
{
	I_GraphicsApiContext* const api = Viewport::GetCurrentApiContext();

	//Shadow Mapping
	//**************
	// #todo: update shadow maps

	//Deferred Rendering
	//******************
	//Step one: Draw the data onto gBuffer
	m_GBuffer.Enable();

	//reset viewport
	api->SetViewport(ivec2(0), m_Dimensions);
	api->SetDepthEnabled(true);
	api->SetCullEnabled(true);

	api->SetClearColor(vec4(m_ClearColor, 1.f));
	api->Clear(E_ClearFlag::Color | E_ClearFlag::Depth);

	// #todo: draw terrain

	// render opaque objects to GBuffer
	for (MaterialCollection const& collection : m_RenderScene->GetOpaqueRenderables())
	{
		api->SetShader(collection.m_Shader.get());
		for (MaterialCollection::MaterialInstance const& material : collection.m_Materials)
		{
			material.m_Material->UploadNonInstanceVariables();
			for (MaterialCollection::Mesh const& mesh : material.m_Meshes)
			{
				api->BindVertexArray(mesh.m_VAO);
				for (T_NodeId const node : mesh.m_Instances)
				{
					// #todo: collect a list of transforms and draw this instanced
					mat4 const& transform = m_RenderScene->GetNodes()[node];
					Sphere instSphere = Sphere((transform * vec4(mesh.m_BoundingVolume.pos, 1.f)).xyz, 
						etm::length(etm::decomposeScale(transform)) * mesh.m_BoundingVolume.radius);

					if (m_Camera.GetFrustum().ContainsSphere(instSphere) != VolumeCheck::OUTSIDE)
					{
						material.m_Material->UploadModelOnly(transform);
						api->DrawElements(E_DrawMode::Triangles, mesh.m_IndexCount, mesh.m_IndexDataType, 0);
					}
				}
			}
		}
	}

	// render ambient IBL
	api->SetFaceCullingMode(E_FaceCullMode::Back);
	api->SetCullEnabled(false);

	m_SSR.EnableInput();
	m_GBuffer.Draw();

	//copy Z-Buffer from gBuffer
	api->BindReadFramebuffer(m_GBuffer.Get());
	api->BindDrawFramebuffer(m_SSR.GetTargetFBO());
	api->CopyDepthReadToDrawFbo(m_Dimensions, m_Dimensions);

	// Render Light Volumes
	api->SetDepthEnabled(false);
	api->SetBlendEnabled(true);
	api->SetBlendEquation(E_BlendEquation::Add);
	api->SetBlendFunction(E_BlendFactor::One, E_BlendFactor::One);

	api->SetCullEnabled(true);
	api->SetFaceCullingMode(E_FaceCullMode::Front);

	// direct
	for (DirectionalLight const& dirLight : m_RenderScene->GetDirectionalLights())
	{
		vec3 col = dirLight.m_Color * dirLight.m_Brightness;
		RenderingSystems::Instance()->GetDirectLightVolume().Draw(dirLight.m_Direction, col);
	}

	// #todo: direct with shadow
	// #todo: pointlights

	api->SetFaceCullingMode(E_FaceCullMode::Back);
	api->SetBlendEnabled(false);

	api->SetCullEnabled(false);

	// draw SSR
	m_PostProcessing.EnableInput();
	m_SSR.Draw();
	// copy depth again
	api->BindReadFramebuffer(m_SSR.GetTargetFBO());
	api->BindDrawFramebuffer(m_PostProcessing.GetTargetFBO());
	api->CopyDepthReadToDrawFbo(m_Dimensions, m_Dimensions);

	api->SetDepthEnabled(true);

	//Forward Rendering
	//******************

	// draw skybox
	Skybox const& skybox = m_RenderScene->GetSkybox();
	if (skybox.m_EnvironmentMap != nullptr)
	{
		api->SetShader(m_SkyboxShader.get());
		m_SkyboxShader->Upload("skybox"_hash, 0);
		api->LazyBindTexture(0, E_TextureType::CubeMap, skybox.m_EnvironmentMap->GetRadianceHandle());

		m_SkyboxShader->Upload("numMipMaps"_hash, skybox.m_EnvironmentMap->GetNumMipMaps());
		m_SkyboxShader->Upload("roughness"_hash, skybox.m_Roughness);

		m_SkyboxShader->Upload("viewProj"_hash, m_Camera.GetStatViewProj());

		api->SetDepthFunction(E_DepthFunc::LEqual);
		RenderingSystems::Instance()->GetPrimitiveRenderer().Draw<primitives::Cube>();
	}

	// #todo: draw stars
	// #todo: draw atmosphere
	// #todo: forward rendering

	// post processing
	api->SetCullEnabled(false);
	m_PostProcessing.Draw(targetFb, m_RenderScene->GetPostProcessingSettings()); // #todo: draw overlays
}


} // namespace render
