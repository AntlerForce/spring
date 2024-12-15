#include "ModelsDataUploader.h"

#include <limits>
#include <cassert>

#include "System/float4.h"
#include "System/Matrix44f.h"
#include "System/Log/ILog.h"
#include "System/SpringMath.h"
#include "System/TimeProfiler.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Objects/SolidObject.h"
#include "Sim/Projectiles/Projectile.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Rendering/GL/VBO.h"
#include "Rendering/Models/3DModel.h"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Rendering/Models/ModelsLock.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Game/GlobalUnsynced.h"


////////////////////////////////////////////////////////////////////

namespace Impl {
	template<
		typename SSBO,
		typename Uploader,
		typename MemStorage
	>
	void UpdateCommon(Uploader& uploader, std::unique_ptr<SSBO>& ssbo, MemStorage& memStorage, const char* className, const char* funcName)
	{
		//resize
		const uint32_t elemCount = uploader.GetElemsCount();
		const uint32_t storageElemCount = memStorage.GetSize();
		if (storageElemCount > elemCount) {
			ssbo->UnbindBufferRange(uploader.GetBindingIdx());

			const uint32_t newElemCount = AlignUp(storageElemCount, uploader.GetElemCountIncr());
			LOG_L(L_DEBUG, "[%s::%s] sizing SSBO %s. New elements count = %u, elemCount = %u, storageElemCount = %u", className, funcName, "up", newElemCount, elemCount, storageElemCount);
			ssbo->Resize(newElemCount);
			// ssbo->Resize() doesn't copy the data, force the update
			memStorage.SetUpdateListUpdateAll();
		}

		const auto& ul = memStorage.GetUpdateList();
		if (!ul.NeedUpdate())
			return;

		// may have been already unbound above, not a big deal
		ssbo->UnbindBufferRange(uploader.GetBindingIdx());

		// get the data
		const auto* clientPtr = memStorage.GetData().data();

		// iterate over contiguous regions of values that need update on the GPU
		for (auto itPair = ul.GetNext(); itPair.has_value(); itPair = ul.GetNext(itPair)) {
			auto [idxOffset, idxSize] = ul.GetOffsetAndSize(itPair.value());

			auto* mappedPtr = ssbo->Map(clientPtr, idxOffset, idxSize);

			if (!ssbo->HasClientPtr())
				memcpy(mappedPtr, clientPtr, storageElemCount * sizeof(decltype(*clientPtr)));

			ssbo->Unmap();
		}

		ssbo->BindBufferRange(uploader.GetBindingIdx());
		ssbo->SwapBuffer();

		memStorage.SetUpdateListReset();
	}

	template<typename SSBO, typename DataType>
	std::unique_ptr<SSBO> InitCommon(uint32_t bindingIdx, uint32_t elemCount0, uint32_t elemCountIncr, uint8_t type, bool coherent, uint32_t numBuffers, const char* className)
	{
		if (!globalRendering->haveGL4)
			return;

		assert(bindingIdx < -1u);

		IStreamBufferConcept::StreamBufferCreationParams p;
		p.target = GL_SHADER_STORAGE_BUFFER;
		p.numElems = elemCount0;
		p.name = std::string(className);
		p.type = static_cast<IStreamBufferConcept::Types>(type);
		p.resizeAble = true;
		p.coherent = coherent;
		p.numBuffers = numBuffers;
		p.optimizeForStreaming = true;

		auto ssbo = IStreamBuffer<DataType>::CreateInstance(p);
		ssbo->BindBufferRange(bindingIdx);

		return std::move(ssbo);
	}

	template<typename SSBO>
	void KillCommon(std::unique_ptr<SSBO>& ssbo, uint32_t bindingIdx)
	{
		if (!globalRendering->haveGL4)
			return;

		ssbo->UnbindBufferRange(bindingIdx);
		ssbo = nullptr;
	}

	template<typename SSBO, typename DataType>
	inline uint32_t GetElemsCount(std::unique_ptr<SSBO>& ssbo)
	{
		return ssbo->GetByteSize() / sizeof(DataType);
	}

}

template<typename T, typename Derived>
void TypedStorageBufferUploader<T, Derived>::InitImpl(uint32_t bindingIdx_, uint32_t elemCount0_, uint32_t elemCountIncr_, uint8_t type, bool coherent, uint32_t numBuffers)
{
	if (!globalRendering->haveGL4)
		return;

	bindingIdx = bindingIdx_;
	elemCount0 = elemCount0_;
	elemCountIncr = elemCountIncr_;

	assert(bindingIdx < -1u);

	IStreamBufferConcept::StreamBufferCreationParams p;
	p.target = GL_SHADER_STORAGE_BUFFER;
	p.numElems = elemCount0;
	p.name = std::string(className);
	p.type = static_cast<IStreamBufferConcept::Types>(type);
	p.resizeAble = true;
	p.coherent = coherent;
	p.numBuffers = numBuffers;
	p.optimizeForStreaming = true;

	ssbo = IStreamBuffer<T>::CreateInstance(p);
	ssbo->BindBufferRange(bindingIdx);
}

template<typename T, typename Derived>
void TypedStorageBufferUploader<T, Derived>::KillImpl()
{
	if (!globalRendering->haveGL4)
		return;

	ssbo->UnbindBufferRange(bindingIdx);
	ssbo = nullptr;
}

template<typename T, typename Derived>
inline uint32_t TypedStorageBufferUploader<T, Derived>::GetElemsCount() const
{
	return ssbo->GetByteSize() / sizeof(T);
}

template<typename T, typename Derived>
size_t TypedStorageBufferUploader<T, Derived>::GetUnitDefElemOffset(int32_t unitDefID) const
{
	return GetDefElemOffsetImpl(unitDefHandler->GetUnitDefByID(unitDefID));
}

template<typename T, typename Derived>
size_t TypedStorageBufferUploader<T, Derived>::GetFeatureDefElemOffset(int32_t featureDefID) const
{
	return GetDefElemOffsetImpl(featureDefHandler->GetFeatureDefByID(featureDefID));
}

template<typename T, typename Derived>
size_t TypedStorageBufferUploader<T, Derived>::GetUnitElemOffset(int32_t unitID) const
{
	return GetElemOffsetImpl(unitHandler.GetUnit(unitID));
}

template<typename T, typename Derived>
size_t TypedStorageBufferUploader<T, Derived>::GetFeatureElemOffset(int32_t featureID) const
{
	return GetElemOffsetImpl(featureHandler.GetFeature(featureID));
}

template<typename T, typename Derived>
size_t TypedStorageBufferUploader<T, Derived>::GetProjectileElemOffset(int32_t syncedProjectileID) const
{
	return GetElemOffsetImpl(projectileHandler.GetProjectileBySyncedID(syncedProjectileID));
}

void TransformsUploader::InitDerived()
{
	if (!globalRendering->haveGL4)
		return;

	InitImpl(MATRIX_SSBO_BINDING_IDX, ELEM_COUNT0, ELEM_COUNTI, IStreamBufferConcept::Types::SB_BUFFERSUBDATA, true, 1);
}

void TransformsUploader::KillDerived()
{
	if (!globalRendering->haveGL4)
		return;

	KillImpl();
}

void TransformsUploader::UpdateDerived()
{
	if (!globalRendering->haveGL4)
		return;

	SCOPED_TIMER("TransformsUploader::Update");

	// TODO why the lock?
	auto lock = CModelsLock::GetScopedLock();

	Impl::UpdateCommon(*this, ssbo, transformsMemStorage, className, __func__);
}

size_t TransformsUploader::GetDefElemOffsetImpl(const S3DModel* model) const
{
	if (model == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr S3DModel", className, __func__);
		return TransformsMemStorage::INVALID_INDEX;
	}

	return model->GetMatAlloc().GetOffset(false);
}

size_t TransformsUploader::GetDefElemOffsetImpl(const UnitDef* def) const
{
	if (def == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr UnitDef", className, __func__);
		return TransformsMemStorage::INVALID_INDEX;
	}

	return GetDefElemOffsetImpl(def->LoadModel());
}

size_t TransformsUploader::GetDefElemOffsetImpl(const FeatureDef* def) const
{
	if (def == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr FeatureDef", className, __func__);
		return TransformsMemStorage::INVALID_INDEX;
	}

	return GetDefElemOffsetImpl(def->LoadModel());
}

size_t TransformsUploader::GetElemOffsetImpl(const CUnit* unit) const
{
	if (unit == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr CUnit", className, __func__);
		return TransformsMemStorage::INVALID_INDEX;
	}

	if (size_t offset = CUnitDrawer::GetTransformMemAlloc(unit).GetOffset(false); offset != TransformsMemStorage::INVALID_INDEX) {
		return offset;
	}

	LOG_L(L_ERROR, "[%s::%s] Supplied invalid CUnit (id:%d)", className, __func__, unit->id);
	return TransformsMemStorage::INVALID_INDEX;
}

size_t TransformsUploader::GetElemOffsetImpl(const CFeature* feature) const
{
	if (feature == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr CFeature", className, __func__);
		return TransformsMemStorage::INVALID_INDEX;
	}

	if (size_t offset = CFeatureDrawer::GetTransformMemAlloc(feature).GetOffset(false); offset != TransformsMemStorage::INVALID_INDEX) {
		return offset;
	}

	LOG_L(L_ERROR, "[%s::%s] Supplied invalid CFeature (id:%d)", className, __func__, feature->id);
	return TransformsMemStorage::INVALID_INDEX;
}

size_t TransformsUploader::GetElemOffsetImpl(const CProjectile* p) const
{
	if (p == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr CProjectile", className, __func__);
		return TransformsMemStorage::INVALID_INDEX;
	}

	if (!p->synced) {
		LOG_L(L_ERROR, "[%s::%s] Supplied non-synced CProjectile (id:%d)", className, __func__, p->id);
		return TransformsMemStorage::INVALID_INDEX;
	}

	if (!p->weapon || !p->piece) {
		LOG_L(L_ERROR, "[%s::%s] Supplied non-weapon or non-piece CProjectile (id:%d)", className, __func__, p->id);
		return TransformsMemStorage::INVALID_INDEX;
	}
	/*
	if (size_t offset = p->GetMatAlloc().GetOffset(false); offset != TransformsMemStorage::INVALID_INDEX) {
		return offset;
	}
	*/

	LOG_L(L_ERROR, "[%s::%s] Supplied invalid CProjectile (id:%d)", className, __func__, p->id);
	return TransformsMemStorage::INVALID_INDEX;
}

void ModelUniformsUploader::InitDerived()
{
	if (!globalRendering->haveGL4)
		return;

	InitImpl(MATUNI_SSBO_BINDING_IDX, ELEM_COUNT0, ELEM_COUNTI, IStreamBufferConcept::Types::SB_BUFFERSUBDATA, true, 1);
}

void ModelUniformsUploader::KillDerived()
{
	if (!globalRendering->haveGL4)
		return;

	KillImpl();
}

void ModelUniformsUploader::UpdateDerived()
{
	if (!globalRendering->haveGL4)
		return;

	SCOPED_TIMER("ModelUniformsUploader::Update");

	Impl::UpdateCommon(*this, ssbo, modelUniformsStorage, className, __func__);
}

size_t ModelUniformsUploader::GetDefElemOffsetImpl(const S3DModel* model) const
{
	assert(false);
	LOG_L(L_ERROR, "[%s::%s] Invalid call", className, __func__);
	return ModelUniformsStorage::INVALID_INDEX;
}

size_t ModelUniformsUploader::GetDefElemOffsetImpl(const UnitDef* def) const
{
	assert(false);
	LOG_L(L_ERROR, "[%s::%s] Invalid call", className, __func__);
	return ModelUniformsStorage::INVALID_INDEX;
}

size_t ModelUniformsUploader::GetDefElemOffsetImpl(const FeatureDef* def) const
{
	assert(false);
	LOG_L(L_ERROR, "[%s::%s] Invalid call", className, __func__);
	return ModelUniformsStorage::INVALID_INDEX;
}


size_t ModelUniformsUploader::GetElemOffsetImpl(const CUnit* unit) const
{
	if (unit == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr CUnit", className, __func__);
		return ModelUniformsStorage::INVALID_INDEX;
	}

	if (size_t offset = modelUniformsStorage.GetObjOffset(unit); offset != size_t(-1)) {
		return offset;
	}

	LOG_L(L_ERROR, "[%s::%s] Supplied invalid CUnit (id:%d)", className, __func__, unit->id);
	return ModelUniformsStorage::INVALID_INDEX;
}

size_t ModelUniformsUploader::GetElemOffsetImpl(const CFeature* feature) const
{
	if (feature == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr CFeature", className, __func__);
		return ModelUniformsStorage::INVALID_INDEX;
	}

	if (size_t offset = modelUniformsStorage.GetObjOffset(feature); offset != size_t(-1)) {
		return offset;
	}

	LOG_L(L_ERROR, "[%s::%s] Supplied invalid CFeature (id:%d)", className, __func__, feature->id);
	return ModelUniformsStorage::INVALID_INDEX;
}

size_t ModelUniformsUploader::GetElemOffsetImpl(const CProjectile* p) const
{
	if (p == nullptr) {
		LOG_L(L_ERROR, "[%s::%s] Supplied nullptr CProjectile", className, __func__);
		return ModelUniformsStorage::INVALID_INDEX;
	}

	if (size_t offset = modelUniformsStorage.GetObjOffset(p); offset != size_t(-1)) {
		return offset;
	}

	LOG_L(L_ERROR, "[%s::%s] Supplied invalid CProjectile (id:%d)", className, __func__, p->id);
	return ModelUniformsStorage::INVALID_INDEX;
}