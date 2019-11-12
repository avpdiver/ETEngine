#include "stdafx.h"
#include "SkyboxMaterial.h"

#include <EtCore/Content/ResourceManager.h>

#include <Engine/Graphics/Shader.h>
#include <Engine/Graphics/TextureData.h>
#include <Engine/Graphics/EnvironmentMap.h>
#include <Engine/Components/TransformComponent.h>
#include <Engine/SceneRendering/ShadedSceneRenderer.h>


SkyboxMaterial::SkyboxMaterial(T_Hash const assetId) 
	: Material("Shaders/FwdSkyboxShader.glsl")
	, m_AssetId(assetId)
{ 
	m_DrawForward = true;
	m_StandardTransform = false;
}

void SkyboxMaterial::LoadTextures()
{
	m_EnvironmentMap = ResourceManager::Instance()->GetAssetData<EnvironmentMap>(m_AssetId);
}

void SkyboxMaterial::UploadDerivedVariables()
{
	m_Shader->Upload("skybox"_hash, 0);
	Viewport::GetCurrentApiContext()->LazyBindTexture(0, E_TextureType::CubeMap, m_EnvironmentMap->GetRadianceHandle());

	m_Shader->Upload("numMipMaps"_hash, m_EnvironmentMap->GetNumMipMaps());
	m_Shader->Upload("roughness"_hash, m_Roughness);

	m_Shader->Upload("viewProj"_hash, render::ShadedSceneRenderer::GetCurrent()->GetCamera().GetStatViewProj());
}