#pragma once

#include "D3DUtil.h"

template<typename T>
class UploadBuffer {
public:
	UploadBuffer (ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : m_IsConstantBuffer (isConstantBuffer) {
		m_ElementByteSize = sizeof (T);
		
		// Constant buffer elements need to be multiples of 256 bytes.
		// This is because the hardware can only view constant data
		// at m * 256 byte offsets and of n * 256 byte lengths.
		
		if (isConstantBuffer)
			m_ElementByteSize = D3DUtil::CalcConstantBufferByteSize (sizeof (T));

		ThrowIfFailed (device->CreateCommittedResource (
			&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer (m_ElementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS (&m_UploadBuffer)
		));

		ThrowIfFailed (m_UploadBuffer->Map (0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

		// We do not need to unmap until we are done with the resource. However, we must not write to
		// the resource while it is in use by the GPU (so we must use synchronization techniques).
	}

	UploadBuffer (const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer () {
		if (m_UploadBuffer != nullptr)
			m_UploadBuffer->Unmap (0, nullptr);

		m_MappedData = nullptr;
	}

	ID3D12Resource* Resource () const {
		return m_UploadBuffer.Get ();
	}

	void CopyData (int elementIndex, const T& data) {
		memcpy (&m_MappedData[elementIndex * m_ElementByteSize], &data, sizeof (T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_UploadBuffer;
	BYTE* m_MappedData = nullptr;

	UINT m_ElementByteSize = 0;
	bool m_IsConstantBuffer = false;
};