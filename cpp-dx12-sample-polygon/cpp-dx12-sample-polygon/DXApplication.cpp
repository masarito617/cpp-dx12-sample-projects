#include "DXApplication.h"

DXApplication::DXApplication(unsigned int width, unsigned int height, std::wstring title)
	: title_(title)
	, windowWidth_(width)
	, windowHeight_(height)
	, viewport_(0.0f, 0.0f, static_cast<float>(windowWidth_), static_cast<float>(windowHeight_))
	, scissorrect_(0, 0, static_cast<LONG>(windowWidth_), static_cast<LONG>(windowHeight_))
	, vertexBufferView_({})
	, indexBufferView_({})
	, fenceValue_(0)
	, fenceEvent_(nullptr)
{
}

// ����������
void DXApplication::OnInit(HWND hwnd)
{
	LoadPipeline(hwnd);
	LoadAssets();
}

void DXApplication::LoadPipeline(HWND hwnd)
{
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	{
		// �f�o�b�O���C���[��L���ɂ���
		ComPtr<ID3D12Debug> debugLayer;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
			debugLayer->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	// DXGIFactory�̏�����
	ComPtr<IDXGIFactory6> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	// �f�o�C�X�̏�����
	CreateD3D12Device(dxgiFactory.Get(), device_.ReleaseAndGetAddressOf());

	// �R�}���h�֘A�̏�����
	{
		// �R�}���h�L���[
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // �^�C���A�E�g����
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // �R�}���h���X�g�ƍ��킹��
		ThrowIfFailed(device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.ReleaseAndGetAddressOf())));
		// �R�}���h�A���P�[�^
		ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.ReleaseAndGetAddressOf())));
		// �R�}���h���X�g
		ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.ReleaseAndGetAddressOf())));
		ThrowIfFailed(commandList_->Close());
	}

	// �X���b�v�`�F�[���̏�����
	{
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.BufferCount = kFrameCount;
		swapchainDesc.Width = windowWidth_;
		swapchainDesc.Height = windowHeight_;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.SampleDesc.Count = 1;
		ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
			commandQueue_.Get(),
			hwnd,
			&swapchainDesc,
			nullptr,
			nullptr,
			(IDXGISwapChain1**)swapchain_.ReleaseAndGetAddressOf()));
	}

	// �f�B�X�N���v�^�q�[�v�̏�����
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kFrameCount;            //�\���̂Q��
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;   //�����_�[�^�[�Q�b�g�r���[
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //�w��Ȃ�
		ThrowIfFailed(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeaps_.ReleaseAndGetAddressOf())));
	}

	// �X���b�v�`�F�[���Ɗ֘A�t���ă����_�[�^�[�Q�b�g�r���[�𐶐�
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeaps_->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < kFrameCount; ++i)
		{
			ThrowIfFailed(swapchain_->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(renderTargets_[i].ReleaseAndGetAddressOf())));
			device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		}
	}

	// �t�F���X�̐���
	{
		ThrowIfFailed(device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf())));
		fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}
}

void DXApplication::LoadAssets()
{
	// ���[�g�V�O�l�`���̐���
	{
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob));
		ThrowIfFailed(device_->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(rootsignature_.ReleaseAndGetAddressOf())));
	}

	// �p�C�v���C���X�e�[�g�̐���
	{
		// �V�F�[�_�[�I�u�W�F�N�g�̐���
#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		ComPtr<ID3DBlob> vsBlob;
		ComPtr<ID3DBlob> psBlob;
		D3DCompileFromFile(L"BasicVertexShader.hlsl", nullptr, nullptr, "BasicVS", "vs_5_0", compileFlags, 0, &vsBlob, nullptr);
		D3DCompileFromFile(L"BasicPixelShader.hlsl", nullptr, nullptr, "BasicPS", "ps_5_0", compileFlags, 0, &psBlob, nullptr);

		// ���_���C�A�E�g�̐���
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// �p�C�v���C���X�e�[�g�I�u�W�F�N�g(PSO)�𐶐�
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = rootsignature_.Get();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) }; // ���̓��C�A�E�g�̐ݒ�
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());                       // ���_�V�F�[�_
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());                       // �s�N�Z���V�F�[�_
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);         // ���X�^���C�U�[�X�e�[�g
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                   // �u�����h�X�e�[�g
		psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;                           // �T���v���}�X�N�̐ݒ�
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;   // �g�|���W�^�C�v
		psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;    // �X�g���b�v���̃J�b�g�ݒ�
		psoDesc.NumRenderTargets = 1;                                             // �����_�[�^�[�Q�b�g��
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                       // �����_�[�^�[�Q�b�g�t�H�[�}�b�g
		psoDesc.SampleDesc.Count = 1;                                             // �}���`�T���v�����O�̐ݒ�
		ThrowIfFailed(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelinestate_.ReleaseAndGetAddressOf())));
	}

	// ���_�o�b�t�@�r���[�̐���
	{
		// ���_��`
		DirectX::XMFLOAT3 vertices[] = {
			{-0.4f,-0.7f, 0.0f} , //����
			{-0.4f, 0.7f, 0.0f} , //����
			{ 0.4f,-0.7f, 0.0f} , //�E��
			{ 0.4f, 0.7f, 0.0f} , //�E��
		};
		const UINT vertexBufferSize = sizeof(vertices);
		auto vertexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		// ���_�o�b�t�@�[�̐���
		ThrowIfFailed(device_->CreateCommittedResource(
			&vertexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&vertexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(vertexBuffer_.ReleaseAndGetAddressOf())));
		// ���_���̃R�s�[
		DirectX::XMFLOAT3* vertexMap = nullptr;
		ThrowIfFailed(vertexBuffer_->Map(0, nullptr, (void**)&vertexMap));
		std::copy(std::begin(vertices), std::end(vertices), vertexMap);
		vertexBuffer_->Unmap(0, nullptr);
		// ���_�o�b�t�@�[�r���[�̐���
		vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = vertexBufferSize;
		vertexBufferView_.StrideInBytes = sizeof(vertices[0]);
	}


	// �C���f�b�N�X�o�b�t�@�r���[�̐���
	{
		// �C���f�b�N�X��`
		unsigned short indices[] = {
			0, 1, 2,
			2, 1, 3
		};
		const UINT indexBufferSize = sizeof(indices);
		auto indexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto indexResDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		// �C���f�b�N�X�o�b�t�@�̐���
		ThrowIfFailed(device_->CreateCommittedResource(
			&indexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&indexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(indexBuffer_.ReleaseAndGetAddressOf())));
		// �C���f�b�N�X���̃R�s�[
		unsigned short* indexMap = nullptr;
		indexBuffer_->Map(0, nullptr, (void**)&indexMap);
		std::copy(std::begin(indices), std::end(indices), indexMap);
		indexBuffer_->Unmap(0, nullptr);
		// �C���f�b�N�X�o�b�t�@�r���[�̐���
		indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = indexBufferSize;
		indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
	}
}

// �X�V����
void DXApplication::OnUpdate()
{
}

// �`�揈��
void DXApplication::OnRender()
{
	// �R�}���h���X�g�̃��Z�b�g
	{
		ThrowIfFailed(commandAllocator_->Reset());
		ThrowIfFailed(commandList_->Reset(commandAllocator_.Get(), pipelinestate_.Get()));
	}

	// �R�}���h���X�g�̐���
	{
		// �o�b�N�o�b�t�@�̃C���f�b�N�X���擾
		auto frameIndex = swapchain_->GetCurrentBackBufferIndex();

		// ���\�[�X�o���A�̐ݒ� (PRESENT -> RENDER_TARGET)
		auto startResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList_->ResourceBarrier(1, &startResourceBarrier);

		// �p�C�v���C���X�e�[�g�ƕK�v�ȃI�u�W�F�N�g��ݒ�
		commandList_->SetPipelineState(pipelinestate_.Get());         // �p�C�v���C���X�e�[�g
		commandList_->SetGraphicsRootSignature(rootsignature_.Get()); // ���[�g�V�O�l�`��
		commandList_->RSSetViewports(1, &viewport_);                  // �r���[�|�[�g
		commandList_->RSSetScissorRects(1, &scissorrect_);            // �V�U�[�Z�`

		// �����_�[�^�[�Q�b�g�̐ݒ�
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeaps_->GetCPUDescriptorHandleForHeapStart(), frameIndex, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		commandList_->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

		// ��ʃN���A
		float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };  // ���F
		commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		// �`�揈���̐ݒ�
		commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // �v���~�e�B�u�g�|���W�̐ݒ� (�O�p�|���S��)
		commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);                // ���_�o�b�t�@
		commandList_->IASetIndexBuffer(&indexBufferView_);                         // �C���f�b�N�X�o�b�t�@
		commandList_->DrawIndexedInstanced(6, 1, 0, 0, 0);                         // �`��

		// ���\�[�X�o���A�̐ݒ� (RENDER_TARGET -> PRESENT)
		auto endResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList_->ResourceBarrier(1, &endResourceBarrier);

		// ���߂̃N���[�Y
		commandList_->Close();
	}

	// �R�}���h���X�g�̎��s
	{
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
		// ��ʂ̃X���b�v
		ThrowIfFailed(swapchain_->Present(1, 0));
	}

	// GPU�����̏I����ҋ@
	{
		ThrowIfFailed(commandQueue_->Signal(fence_.Get(), ++fenceValue_));
		if (fence_->GetCompletedValue() < fenceValue_) {
			ThrowIfFailed(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_));
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}
}

// �I������
void DXApplication::OnDestroy()
{
	CloseHandle(fenceEvent_);
}

// D3D12Device�̐���
void DXApplication::CreateD3D12Device(IDXGIFactory6* dxgiFactory, ID3D12Device** d3d12device)
{
	ID3D12Device* tmpDevice = nullptr;

	// �O���t�B�b�N�X�{�[�h�̑I��
	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; SUCCEEDED(dxgiFactory->EnumAdapters(i, &tmpAdapter)); ++i)
	{
		adapters.push_back(tmpAdapter);
	}
	for (auto adapter : adapters)
	{
		DXGI_ADAPTER_DESC adapterDesc;
		adapter->GetDesc(&adapterDesc);
		// AMD���܂ރA�_�v�^�[�I�u�W�F�N�g��T���Ċi�[�i������Ȃ����nullptr�Ńf�t�H���g�j
		// ���i�ł̏ꍇ�́A�I�v�V������ʂ���I�������Đݒ肷��K�v������
		std::wstring strAdapter = adapterDesc.Description;
		if (strAdapter.find(L"AMD") != std::string::npos)
		{
			tmpAdapter = adapter;
			break;
		}
	}

	// Direct3D�f�o�C�X�̏�����
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	for (auto level : levels) {
		// �����\�ȃo�[�W���������������烋�[�v��ł��؂�
		if (SUCCEEDED(D3D12CreateDevice(tmpAdapter, level, IID_PPV_ARGS(&tmpDevice)))) {
			break;
		}
	}
	*d3d12device = tmpDevice;
}

void DXApplication::ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		// hr�̃G���[���e��throw����
		char s_str[64] = {};
		sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
		std::string errMessage = std::string(s_str);
		throw std::runtime_error(errMessage);
	}
}
