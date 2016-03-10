/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/

#include "Particles/ParticleEmitterInstance.h"

namespace DAVA
{
ParticleEmitterInstance::ParticleEmitterInstance(ParticleEffectComponent* _owner)
    : owner(_owner)
{
}

ParticleEmitterInstance::ParticleEmitterInstance(ParticleEffectComponent* _owner, ParticleEmitter* _emitter)
    : owner(_owner)
    , emitter(_emitter)
{
}

void ParticleEmitterInstance::SetEmitter(ParticleEmitter* _emitter)
{
    emitter.Set(_emitter);
}

void ParticleEmitterInstance::SetFilePath(const FilePath& _filePath)
{
    filePath = _filePath;
}

void ParticleEmitterInstance::SetSpawnPosition(const Vector3& _position)
{
    spawnPosition = _position;
}

ParticleEmitterInstance* ParticleEmitterInstance::Clone() const
{
    ParticleEmitterInstance* result = new ParticleEmitterInstance(owner, emitter->Clone());
    result->SetFilePath(GetFilePath());
    result->SetSpawnPosition(GetSpawnPosition());
    return result;
}
}
