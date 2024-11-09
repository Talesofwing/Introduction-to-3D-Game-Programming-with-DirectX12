#pragma once

#include "D3DUtil.h"

template<typename T>
class UploadBuffer {
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : _isConstantBuffer(isConstantBuffer) {
		_elementByteSize = sizeof(T);

		// Constant buffer elements need to be multiples of 256 bytes.
		// This is because the hardware can only view constant data
		// at m * 256 byte offsets and of n * 256 byte lengths.

		if (isConstantBuffer)
			_elementByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(T));

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(_elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_uploadBuffer)
		));

		ThrowIfFailed(_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_mappedData)));

		// We do not need to unmap until we are done with the resource. However, we must not write to
		// the resource while it is in use by the GPU (so we must use synchronization techniques).
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer() {
		if (_uploadBuffer != nullptr)
			_uploadBuffer->Unmap(0, nullptr);

		_mappedData = nullptr;
	}

	ID3D12Resource* Resource() const {
		return _uploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) {
		memcpy(&_mappedData[elementIndex * _elementByteSize], &data, sizeof(T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer;
	BYTE* _mappedData = nullptr;

	UINT _elementByteSize = 0;
	bool _isConstantBuffer = false;
};
