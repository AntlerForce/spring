#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "ModelsMemStorageDefs.h"
#include "ModelsLock.h"
#include "System/Matrix44f.h"
#include "System/MemPoolTypes.h"
#include "System/FreeListMap.h"
#include "System/Threading/SpringThreading.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Objects/SolidObjectDef.h"

class TransformsMemStorage : public StablePosAllocator<CMatrix44f> {
private:
	using MyType = StablePosAllocator::Type;
public:
	explicit TransformsMemStorage()
		: StablePosAllocator<MyType>(INIT_NUM_ELEMS)
		, dirtyMap(INIT_NUM_ELEMS, BUFFERING)
	{}
	void Reset() override {
		assert(Threading::IsMainThread());
		StablePosAllocator<MyType>::Reset();
		dirtyMap.resize(GetSize(), BUFFERING);
	}

	size_t Allocate(size_t numElems) override {
		auto lock = CModelsLock::GetScopedLock();
		size_t res = StablePosAllocator<MyType>::Allocate(numElems);
		dirtyMap.resize(GetSize(), BUFFERING);

		return res;
	}
	void Free(size_t firstElem, size_t numElems, const MyType* T0 = nullptr) override {
		auto lock = CModelsLock::GetScopedLock();
		StablePosAllocator<MyType>::Free(firstElem, numElems, T0);
		dirtyMap.resize(GetSize(), BUFFERING);
	}

	const MyType& operator[](std::size_t idx) const override
	{
		auto lock = CModelsLock::GetScopedLock();
		return StablePosAllocator<MyType>::operator[](idx);
	}
	MyType& operator[](std::size_t idx) override
	{
		auto lock = CModelsLock::GetScopedLock();
		return StablePosAllocator<MyType>::operator[](idx);
	}
private:
	std::vector<uint8_t> dirtyMap;
public:
	const decltype(dirtyMap)& GetDirtyMap() const
	{
		return dirtyMap;
	}
	decltype(dirtyMap)& GetDirtyMap()
	{
		return dirtyMap;
	}

	void SetAllDirty();
public:
	//need to update buffer with matrices BUFFERING times, because the actual buffer is made of BUFFERING number of parts
	static constexpr uint8_t BUFFERING = 3u;
private:
	static constexpr int INIT_NUM_ELEMS = 1 << 16u;
};

extern TransformsMemStorage transformsMemStorage;


////////////////////////////////////////////////////////////////////

class ScopedTransformMemAlloc {
public:
	ScopedTransformMemAlloc() : ScopedTransformMemAlloc(0u) {};
	ScopedTransformMemAlloc(std::size_t numElems_)
		: numElems{numElems_}
	{
		firstElem = transformsMemStorage.Allocate(numElems);
	}

	ScopedTransformMemAlloc(const ScopedTransformMemAlloc&) = delete;
	ScopedTransformMemAlloc(ScopedTransformMemAlloc&& smma) noexcept { *this = std::move(smma); }

	~ScopedTransformMemAlloc() {
		if (firstElem == TransformsMemStorage::INVALID_INDEX)
			return;

		transformsMemStorage.Free(firstElem, numElems, &CMatrix44f::Zero());
	}

	bool Valid() const { return firstElem != TransformsMemStorage::INVALID_INDEX;	}
	std::size_t GetOffset(bool assertInvalid = true) const {
		if (assertInvalid)
			assert(Valid());

		return firstElem;
	}

	ScopedTransformMemAlloc& operator= (const ScopedTransformMemAlloc&) = delete;
	ScopedTransformMemAlloc& operator= (ScopedTransformMemAlloc&& smma) noexcept {
		//swap to prevent dealloc on dying object, yet enable destructor to do its thing on valid object
		std::swap(firstElem, smma.firstElem);
		std::swap(numElems , smma.numElems );

		return *this;
	}

	const CMatrix44f& operator[](std::size_t offset) const {
		assert(firstElem != TransformsMemStorage::INVALID_INDEX);
		assert(offset >= 0 && offset < numElems);

		return transformsMemStorage[firstElem + offset];
	}
	CMatrix44f& operator[](std::size_t offset) {
		assert(firstElem != TransformsMemStorage::INVALID_INDEX);
		assert(offset >= 0 && offset < numElems);

		transformsMemStorage.GetDirtyMap().at(firstElem + offset) = TransformsMemStorage::BUFFERING;
		return transformsMemStorage[firstElem + offset];
	}
public:
	static const ScopedTransformMemAlloc& Dummy() {
		static ScopedTransformMemAlloc dummy;

		return dummy;
	};
private:
	std::size_t firstElem = TransformsMemStorage::INVALID_INDEX;
	std::size_t numElems  = 0u;
};

////////////////////////////////////////////////////////////////////

class CWorldObject;
class ModelUniformsStorage {
private:
	using MyType = ModelUniformData;
public:
	ModelUniformsStorage();
public:
	size_t AddObjects(const CWorldObject* o);
	void   DelObjects(const CWorldObject* o);
	size_t GetObjOffset(const CWorldObject* o);
	MyType& GetObjUniformsArray(const CWorldObject* o);

	size_t AddObjects(const SolidObjectDef* o) { return INVALID_INDEX; }
	void   DelObjects(const SolidObjectDef* o) {}
	size_t GetObjOffset(const SolidObjectDef* o) { return INVALID_INDEX; }
	auto& GetObjUniformsArray(const SolidObjectDef* o) { return dummy; }

	size_t AddObjects(const S3DModel* o) { return INVALID_INDEX; }
	void   DelObjects(const S3DModel* o) {}
	size_t GetObjOffset(const S3DModel* o) { return INVALID_INDEX; }
	auto& GetObjUniformsArray(const S3DModel* o) { return dummy; }

	size_t Size() const { return storage.GetData().size(); }
	const auto& GetData() const { return storage.GetData(); }
public:
	static constexpr size_t INVALID_INDEX = 0;
private:
	inline static MyType dummy = {};

	std::unordered_map<CWorldObject*, size_t> objectsMap;
	spring::FreeListMap<MyType> storage;
};

extern ModelUniformsStorage modelUniformsStorage;
