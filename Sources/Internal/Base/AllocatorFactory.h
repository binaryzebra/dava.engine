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



#ifndef __DAVAENGINE_ALLOCATOR_FACTORY_H__
#define __DAVAENGINE_ALLOCATOR_FACTORY_H__

#include "Base/BaseTypes.h"
#include "Base/Singleton.h"
#include "Base/FixedSizePoolAllocator.h"

#define IMPLEMENT_POOL_ALLOCATOR(TYPE, poolSize) \
	void * operator new(std::size_t size) \
	{ \
		static FixedSizePoolAllocator * alloc = AllocatorFactory::Instance()->GetAllocator<TYPE>(poolSize); \
		return alloc->New(); \
	} \
	 \
	void operator delete(void * ptr) \
	{ \
		static FixedSizePoolAllocator * alloc = AllocatorFactory::Instance()->GetAllocator<TYPE>(poolSize); \
		alloc->Delete(ptr); \
	} \

namespace DAVA
{

class AllocatorFactory : public Singleton<AllocatorFactory>
{
public:
	AllocatorFactory();
	virtual ~AllocatorFactory();

	template<class T>
	FixedSizePoolAllocator * GetAllocator(int32 poolLength);

	void Dump();

private:
	Map<String, FixedSizePoolAllocator*> allocators;
};

template<class T>
FixedSizePoolAllocator * AllocatorFactory::GetAllocator(int32 poolLength)
{
	String className = typeid(T).name();
	FixedSizePoolAllocator * alloc = allocators[className];
	if(0 == alloc)
	{
		alloc = new FixedSizePoolAllocator(sizeof(T), poolLength);
		allocators[className] = alloc;
	}

	return alloc;
}

};

#endif //__DAVAENGINE_ALLOCATOR_FACTORY_H__