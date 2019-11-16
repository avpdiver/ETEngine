#include "stdafx.h"
#include "OutlineRenderer.h"

#include "ShadedSceneRenderer.h"
#include "Gbuffer.h"

#include <EtCore/Content/ResourceManager.h>

#include <Engine/Graphics/TextureData.h>
#include <Engine/Graphics/Shader.h>
#include <Engine/Graphics/FrameBuffer.h>
#include <Engine/Materials/ColorMaterial.h>
#include <Engine/GlobalRenderingSystems/GlobalRenderingSystems.h>
#include <Engine/GraphicsHelper/RenderScene.h>



//====================
// Outline Renderer
//====================


//---------------------------------
// OutlineRenderer::d-tor
//
OutlineRenderer::~OutlineRenderer()
{
	DestroyRenderTarget();

	if (m_EventDispatcher != nullptr)
	{
		m_EventDispatcher->Unregister(m_CallbackId);
		m_EventDispatcher = nullptr;
	}
}

//---------------------------------
// OutlineRenderer::Initialize
//
void OutlineRenderer::Initialize(render::RenderEventDispatcher* const eventDispatcher)
{
	m_SobelShader = ResourceManager::Instance()->GetAssetData<ShaderData>("PostSobel.glsl"_hash);

	CreateRenderTarget();

	Viewport::GetCurrentViewport()->GetResizeEvent().AddListener( std::bind( &OutlineRenderer::OnWindowResize, this ) );

	m_EventDispatcher = eventDispatcher;
	m_CallbackId = m_EventDispatcher->Register(render::E_RenderEvent::RenderOutlines, render::T_RenderEventCallback(
		[this](render::RenderEventData const* const evnt) -> void
		{
			if (evnt->renderer->GetType() == typeid(render::ShadedSceneRenderer))
			{
				render::ShadedSceneRenderer const* const renderer = static_cast<render::ShadedSceneRenderer const*>(evnt->renderer);
				render::I_SceneExtension const* const ext = renderer->GetScene()->GetExtension("OutlineExtension"_hash);
				if (ext == nullptr)
				{
					LOG("render scene does not have an outline extension");
					return;
				}

				OutlineExtension const* const outlineExt = static_cast<OutlineExtension const*>(ext);
				Draw(evnt->targetFb, *outlineExt, renderer->GetScene()->GetNodes(), renderer->GetCamera(), renderer->GetGBuffer());
			}
			else
			{
				ET_ASSERT(true, "Cannot retrieve outline info from unhandled renderer!");
			}
		}));
}

//---------------------------------
// OutlineRenderer::GenerateRenderTarget
//
void OutlineRenderer::CreateRenderTarget()
{
	I_GraphicsApiContext* const api = Viewport::GetCurrentApiContext();
	ivec2 const dim = Viewport::GetCurrentViewport()->GetDimensions();

	TextureParameters params(false);
	params.minFilter = E_TextureFilterMode::Linear;
	params.magFilter = E_TextureFilterMode::Linear;
	params.wrapS = E_TextureWrapMode::ClampToBorder;
	params.wrapT = E_TextureWrapMode::ClampToBorder;
	params.borderColor = vec4(vec3(0.f), 1.f);

	//Generate texture and fbo and rbo as initial postprocessing target
	api->GenFramebuffers(1, &m_DrawTarget);
	api->BindFramebuffer(m_DrawTarget);
	m_DrawTex = new TextureData(dim, E_ColorFormat::RGB16f, E_ColorFormat::RGB, E_DataType::Float);
	m_DrawTex->Build();
	m_DrawTex->SetParameters(params);
	api->LinkTextureToFbo2D(0, m_DrawTex->GetHandle(), 0);

	api->GenRenderBuffers(1, &m_DrawDepth);
	api->BindRenderbuffer(m_DrawDepth);
	api->SetRenderbufferStorage(E_RenderBufferFormat::Depth24, dim);
	api->LinkRenderbufferToFbo(E_RenderBufferFormat::Depth24, m_DrawDepth);

	api->BindFramebuffer(0);
}

//---------------------------------
// OutlineRenderer::DestroyRenderTarget
//
void OutlineRenderer::DestroyRenderTarget()
{
	I_GraphicsApiContext* const api = Viewport::GetCurrentApiContext();

	api->DeleteRenderBuffers(1, &m_DrawDepth);
	SafeDelete(m_DrawTex);
	api->DeleteFramebuffers(1, &m_DrawTarget);
}

//---------------------------------
// OutlineRenderer::Draw
//
void OutlineRenderer::Draw(T_FbLoc const targetFb, 
	OutlineExtension const& outlines, 
	core::slot_map<mat4> const& nodes, 
	Camera const& cam, 
	Gbuffer const& gbuffer)
{
	if (outlines.GetOutlineLists().empty())
	{
		return;
	}

	I_GraphicsApiContext* const api = Viewport::GetCurrentApiContext();
	ivec2 const dim = Viewport::GetCurrentViewport()->GetDimensions();

	api->SetViewport(ivec2(0), dim);

	// draw the shapes as colors to the intermediate rendertarget
	//------------------------------------------------------------
	api->BindFramebuffer(m_DrawTarget);

	api->SetClearColor(vec4(vec3(0.f), 1.f));
	api->Clear(E_ClearFlag::Color | E_ClearFlag::Depth);

	ColorMaterial* const mat = RenderingSystems::Instance()->GetColorMaterial();
	AssetPtr<ShaderData> const shader = mat->GetShader();

	api->SetShader(shader.get());
	shader->Upload("worldViewProj"_hash, cam.GetViewProj());
	shader->Upload("uViewSize"_hash, etm::vecCast<float>(dim));

	shader->Upload("uOcclusionFactor"_hash, 0.15f);

	// bind the gbuffers depth texture
	shader->Upload("texGBufferA"_hash, 0);
	api->LazyBindTexture(0, gbuffer.GetTextures()[0]->GetTargetType(), gbuffer.GetTextures()[0]->GetHandle());

	api->SetDepthEnabled(true); 

	for (OutlineExtension::OutlineList const& list : outlines.GetOutlineLists())
	{
		shader->Upload("uColor"_hash, list.color);

		for (render::MaterialCollection::Mesh const& mesh : list.meshes)
		{
			api->BindVertexArray(mesh.m_VAO);
			for (render::T_NodeId const node : mesh.m_Instances)
			{
				// #todo: collect a list of transforms and draw this instanced
				mat4 const& transform = nodes[node];
				Sphere instSphere = Sphere((transform * vec4(mesh.m_BoundingVolume.pos, 1.f)).xyz,
					etm::length(etm::decomposeScale(transform)) * mesh.m_BoundingVolume.radius);

				if (cam.GetFrustum().ContainsSphere(instSphere) != VolumeCheck::OUTSIDE)
				{
					mat->UploadModelOnly(transform);
					api->DrawElements(E_DrawMode::Triangles, mesh.m_IndexCount, mesh.m_IndexDataType, 0);
				}
			}
		}
	}

	// apply a sobel shader to the colored shapes and render it to the target framebuffer
	//------------------------------------------------------------------------------------

	api->BindFramebuffer(targetFb);

	api->SetDepthEnabled(false);
	api->SetBlendEnabled(true);
	api->SetBlendEquation(E_BlendEquation::Add);
	api->SetBlendFunction(E_BlendFactor::One, E_BlendFactor::One);

	api->SetShader(m_SobelShader.get());

	m_SobelShader->Upload("inColorTex"_hash, 1);
	api->LazyBindTexture(1, E_TextureType::Texture2D, m_DrawTex->GetHandle());

	RenderingSystems::Instance()->GetPrimitiveRenderer().Draw<primitives::Quad>();

	api->SetBlendEnabled(false);
}

//---------------------------------
// OutlineRenderer::OnWindowResize
//
void OutlineRenderer::OnWindowResize()
{
	DestroyRenderTarget();
	CreateRenderTarget();
}
