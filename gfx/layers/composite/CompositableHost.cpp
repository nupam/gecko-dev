/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableHost.h"
#include <map>        // for _Rb_tree_iterator, map, etc
#include <utility>    // for pair
#include "Effects.h"  // for EffectMask, Effect, etc
#include "gfxUtils.h"
#include "Layers.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"     // for TextureHost, etc
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/RefPtr.h"   // for nsRefPtr
#include "nsDebug.h"          // for NS_WARNING
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "gfxPlatform.h"      // for gfxPlatform
#include "IPDLActor.h"

namespace mozilla {

using namespace gfx;

namespace layers {

class Compositor;

CompositableHost::CompositableHost(const TextureInfo& aTextureInfo)
    : mTextureInfo(aTextureInfo),
      mCompositorBridgeID(0),
      mLayer(nullptr),
      mAttached(false),
      mKeepAttached(false) {
  MOZ_COUNT_CTOR(CompositableHost);
}

CompositableHost::~CompositableHost() { MOZ_COUNT_DTOR(CompositableHost); }

void CompositableHost::UseTextureHost(const nsTArray<TimedTexture>& aTextures) {
  if (mTextureSourceProvider) {
    for (auto& texture : aTextures) {
      texture.mTexture->SetTextureSourceProvider(mTextureSourceProvider);
    }
  }
}

void CompositableHost::UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                                 TextureHost* aTextureOnWhite) {
  MOZ_ASSERT(aTextureOnBlack && aTextureOnWhite);
  if (mTextureSourceProvider) {
    aTextureOnBlack->SetTextureSourceProvider(mTextureSourceProvider);
    aTextureOnWhite->SetTextureSourceProvider(mTextureSourceProvider);
  }
}

void CompositableHost::RemoveTextureHost(TextureHost* aTexture) {}

void CompositableHost::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  MOZ_ASSERT(aProvider);
  mTextureSourceProvider = aProvider;
}

bool CompositableHost::AddMaskEffect(EffectChain& aEffects,
                                     const gfx::Matrix4x4& aTransform) {
  CompositableTextureSourceRef source;
  RefPtr<TextureHost> host = GetAsTextureHost();

  if (!host) {
    NS_WARNING("Using compositable with no valid TextureHost as mask");
    return false;
  }

  if (!host->Lock()) {
    NS_WARNING("Failed to lock the mask texture");
    return false;
  }

  if (!host->BindTextureSource(source)) {
    NS_WARNING(
        "The TextureHost was successfully locked but can't provide a "
        "TextureSource");
    host->Unlock();
    return false;
  }
  MOZ_ASSERT(source);

  RefPtr<EffectMask> effect =
      new EffectMask(source, source->GetSize(), aTransform);
  aEffects.mSecondaryEffects[EffectTypes::MASK] = effect;
  return true;
}

void CompositableHost::RemoveMaskEffect() {
  RefPtr<TextureHost> host = GetAsTextureHost();
  if (host) {
    host->Unlock();
  }
}

/* static */
already_AddRefed<CompositableHost> CompositableHost::Create(
    const TextureInfo& aTextureInfo) {
  RefPtr<CompositableHost> result;
  switch (aTextureInfo.mCompositableType) {
    case CompositableType::IMAGE:
      result = new WebRenderImageHost(aTextureInfo);
      break;
    default:
      NS_ERROR("Unknown CompositableType");
  }
  return result.forget();
}

void CompositableHost::DumpTextureHost(std::stringstream& aStream,
                                       TextureHost* aTexture) {
  if (!aTexture) {
    return;
  }
  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  if (!dSurf) {
    return;
  }
  aStream << gfxUtils::GetAsDataURI(dSurf).get();
}

TextureSourceProvider* CompositableHost::GetTextureSourceProvider() const {
  return mTextureSourceProvider;
}

}  // namespace layers
}  // namespace mozilla
