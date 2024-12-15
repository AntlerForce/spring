#pragma once

#include <string>
#include <string_view>
#include <stddef.h>
#include <memory>

#include "System/Matrix44f.h"
#include "System/TypeToStr.h"
#include "Rendering/GL/StreamBuffer.h"
#include "Rendering/Models/ModelsMemStorageDefs.h"


class CUnit;
class CFeature;
class CProjectile;
struct UnitDef;
struct FeatureDef;
struct S3DModel;

template<typename T, typename Derived>
class TypedStorageBufferUploader {
public:
	static Derived& GetInstance() {
		static Derived instance;
		return instance;
	};
public:
	void Init() { static_cast<Derived*>(this)->InitDerived(); }
	void Kill() { static_cast<Derived*>(this)->KillDerived(); }
	void Update() { static_cast<Derived*>(this)->UpdateDerived(); }

	uint32_t GetElemsCount() const;
	auto GetElemCountIncr() const { return elemCountIncr; }
	auto GetBindingIdx() const { return bindingIdx; }
public:
	// Defs
	size_t GetElemOffset(const UnitDef* def) const { return GetDefElemOffsetImpl(def); }
	size_t GetElemOffset(const FeatureDef* def) const { return GetDefElemOffsetImpl(def); }
	size_t GetElemOffset(const S3DModel* model) const { return GetDefElemOffsetImpl(model); }
	size_t GetUnitDefElemOffset(int32_t unitDefID) const;
	size_t GetFeatureDefElemOffset(int32_t featureDefID) const;

	// Objs
	size_t GetElemOffset(const CUnit* unit) const { return GetElemOffsetImpl(unit); }
	size_t GetElemOffset(const CFeature* feature) const { return GetElemOffsetImpl(feature); }
	size_t GetElemOffset(const CProjectile* proj) const { return GetElemOffsetImpl(proj); }
	size_t GetUnitElemOffset(int32_t unitID) const;
	size_t GetFeatureElemOffset(int32_t featureID) const;
	size_t GetProjectileElemOffset(int32_t syncedProjectileID) const;
protected:
	void InitImpl(uint32_t bindingIdx_, uint32_t elemCount0_, uint32_t elemCountIncr_, uint8_t type, bool coherent, uint32_t numBuffers);
	void KillImpl();

	virtual size_t GetDefElemOffsetImpl(const S3DModel* model) const = 0;
	virtual size_t GetDefElemOffsetImpl(const UnitDef* def) const = 0;
	virtual size_t GetDefElemOffsetImpl(const FeatureDef* def) const = 0;
	virtual size_t GetElemOffsetImpl(const CUnit* so) const = 0;
	virtual size_t GetElemOffsetImpl(const CFeature* so) const = 0;
	virtual size_t GetElemOffsetImpl(const CProjectile* p) const = 0;

protected:
	uint32_t bindingIdx = -1u;
	uint32_t elemCount0 = 0;
	uint32_t elemCountIncr = 0;
	static constexpr const char* className = spring::TypeToCStr<Derived>();

	std::unique_ptr<IStreamBuffer<T>> ssbo;
};

class TransformsUploader : public TypedStorageBufferUploader<CMatrix44f, TransformsUploader> {
public:
	void InitDerived();
	void KillDerived();
	void UpdateDerived();
protected:
	size_t GetDefElemOffsetImpl(const S3DModel* model) const override;
	size_t GetDefElemOffsetImpl(const UnitDef* def) const override;
	size_t GetDefElemOffsetImpl(const FeatureDef* def) const override;
	size_t GetElemOffsetImpl(const CUnit* so) const override;
	size_t GetElemOffsetImpl(const CFeature* so) const override;
	size_t GetElemOffsetImpl(const CProjectile* p) const override;
private:
	static constexpr uint32_t MATRIX_SSBO_BINDING_IDX = 0;
	static constexpr uint32_t ELEM_COUNT0 = 1u << 16;
	static constexpr uint32_t ELEM_COUNTI = 1u << 14;
};

class ModelUniformsUploader : public TypedStorageBufferUploader<ModelUniformData, ModelUniformsUploader> {
public:
	void InitDerived();
	void KillDerived();
	void UpdateDerived();
protected:
	virtual size_t GetDefElemOffsetImpl(const S3DModel* model) const override;
	virtual size_t GetDefElemOffsetImpl(const UnitDef* def) const override;
	virtual size_t GetDefElemOffsetImpl(const FeatureDef* def) const override;
	virtual size_t GetElemOffsetImpl(const CUnit* so) const override;
	virtual size_t GetElemOffsetImpl(const CFeature* so) const override;
	virtual size_t GetElemOffsetImpl(const CProjectile* p) const override;
private:
	static constexpr uint32_t MATUNI_SSBO_BINDING_IDX = 1;
	static constexpr uint32_t ELEM_COUNT0 = 1u << 12;
	static constexpr uint32_t ELEM_COUNTI = 1u << 11;
};

#define transformsUploader TransformsUploader::GetInstance()
#define modelUniformsUploader ModelUniformsUploader::GetInstance()