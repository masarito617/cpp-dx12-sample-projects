#include "DXApplication.h"
#include "Win32Application.h"

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
		// �����_�[�^�[�Q�b�g�r���[
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kFrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf())));
		// �[�x�o�b�t�@�[�r���[
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		ID3D12DescriptorHeap* dsvHeap = nullptr;
		ThrowIfFailed(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf())));
		// ��{���̎󂯓n���p
		D3D12_DESCRIPTOR_HEAP_DESC basicHeapDesc = {};
		basicHeapDesc.NumDescriptors = 3; // 1SRV + 2CBV
		basicHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		basicHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device_->CreateDescriptorHeap(&basicHeapDesc, IID_PPV_ARGS(basicHeap_.ReleaseAndGetAddressOf())));
	}

	// �X���b�v�`�F�[���Ɗ֘A�t���ă����_�[�^�[�Q�b�g�r���[�𐶐�
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < kFrameCount; ++i)
		{
			ThrowIfFailed(swapchain_->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(renderTargets_[i].ReleaseAndGetAddressOf())));
			device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		}
	}

	// �[�x�o�b�t�@�[�r���[����
	{
		// �[�x�o�b�t�@�[�쐬
		D3D12_RESOURCE_DESC depthResDesc = {};
		depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthResDesc.Width = windowWidth_;
		depthResDesc.Height = windowHeight_;
		depthResDesc.DepthOrArraySize = 1;
		depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthResDesc.SampleDesc.Count = 1;
		depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthResDesc.MipLevels = 1;
		depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthResDesc.Alignment = 0;
		auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		// �N���A�o�����[�̐ݒ�
		D3D12_CLEAR_VALUE _depthClearValue = {};
		_depthClearValue.DepthStencil.Depth = 1.0f;      //�[���P(�ő�l)�ŃN���A
		_depthClearValue.Format = DXGI_FORMAT_D32_FLOAT; //32bit�[�x�l�Ƃ��ăN���A
		ThrowIfFailed(device_->CreateCommittedResource(
			&depthHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&depthResDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, //�f�v�X�������݂Ɏg�p
			&_depthClearValue,
			IID_PPV_ARGS(depthBuffer_.ReleaseAndGetAddressOf())));
		// �[�x�o�b�t�@�[�r���[�쐬
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		device_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
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
		// ���[�g�p�����[�^�̐���
		// �f�B�X�N���v�^�e�[�u���̎���
		CD3DX12_DESCRIPTOR_RANGE1 discriptorRanges[2];
		discriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // CBV
		discriptorRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // SRV
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsDescriptorTable(2, discriptorRanges, D3D12_SHADER_VISIBILITY_ALL); // ����p�����[�^�ŕ����w��
		// �T���v���[�̐���
		// �e�N�X�`���f�[�^����ǂ��F�����o���������߂邽�߂̐ݒ�
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		// ���[�g�p�����[�^�A�T���v���[���烋�[�g�V�O�l�`���𐶐�
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob));
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
		D3DCompileFromFile(L"Shaders/LambertShaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, nullptr);
		D3DCompileFromFile(L"Shaders/LambertShaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &psBlob, nullptr);

		// ���_���C�A�E�g�̐���
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// ���ʕ`��ɂ���ꍇ�A�R�����g���O���ׂ�
		auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		// �p�C�v���C���X�e�[�g�I�u�W�F�N�g(PSO)�𐶐�
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = rootsignature_.Get();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) }; // ���̓��C�A�E�g�̐ݒ�
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());                       // ���_�V�F�[�_
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());                       // �s�N�Z���V�F�[�_
		psoDesc.RasterizerState = rasterizerState;                                // ���X�^���C�U�[�X�e�[�g
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                   // �u�����h�X�e�[�g
		psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;                           // �T���v���}�X�N�̐ݒ�
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;   // �g�|���W�^�C�v
		psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;    // �X�g���b�v���̃J�b�g�ݒ�
		psoDesc.NumRenderTargets = 1;                                             // �����_�[�^�[�Q�b�g��
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                       // �����_�[�^�[�Q�b�g�t�H�[�}�b�g
		psoDesc.SampleDesc.Count = 1;                                             // �}���`�T���v�����O�̐ݒ�
		// �[�x�X�e���V�� 
		psoDesc.DepthStencilState.DepthEnable = true;                             // �[�x�o�b�t�@�[���g�p���邩
		psoDesc.DepthStencilState.StencilEnable = false;                          // �X�e���V���e�X�g���s����
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;                                // �[�x�o�b�t�@�[�Ŏg�p����t�H�[�}�b�g
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;         // ��������
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;    // �����������̗p����
		ThrowIfFailed(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelinestate_.ReleaseAndGetAddressOf())));
	}

	// fbx���f���̃��[�h
	{
		if (!FbxLoader::Load("Assets/goloyan.fbx", &fbxVertexInfo_))
		{
			ThrowMessage("failed load fbx file.");
		}
	}

	// ���_�o�b�t�@�r���[�̐���
	{
		// ���_���W
		std::vector<FbxLoader::Vertex> vertices = fbxVertexInfo_.vertices;
		const UINT vertexBufferSize = sizeof(FbxLoader::Vertex) * vertices.size();
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
		FbxLoader::Vertex* vertexMap = nullptr;
		ThrowIfFailed(vertexBuffer_->Map(0, nullptr, (void**)&vertexMap));
		std::copy(std::begin(vertices), std::end(vertices), vertexMap);
		vertexBuffer_->Unmap(0, nullptr);
		// ���_�o�b�t�@�[�r���[�̐���
		vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = vertexBufferSize;
		vertexBufferView_.StrideInBytes = sizeof(FbxLoader::Vertex);
	}

	// �C���f�b�N�X�o�b�t�@�r���[�̐���
	{
		// �C���f�b�N�X���W
		std::vector<unsigned short> indices = fbxVertexInfo_.indices;
		const UINT indexBufferSize = sizeof(unsigned short) * indices.size();
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

	// �e�N�X�`���̃��[�h����
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};
	std::vector<D3D12_SUBRESOURCE_DATA> textureSubresources;
	{
		ThrowIfFailed(LoadFromWICFile(L"Assets/tex_goloyan.png", DirectX::WIC_FLAGS_NONE, &metadata, scratchImg));
		ThrowIfFailed(PrepareUpload(device_.Get(), scratchImg.GetImages(), scratchImg.GetImageCount(), metadata, textureSubresources));
	}

	// �f�B�X�N���v�^�q�[�v���n���h�������O�Ɏ擾
	auto basicHeapHandle = basicHeap_->GetCPUDescriptorHandleForHeapStart();

	// ���W�ϊ��}�g���N�X(CBV)�̐���
	{
		// �萔�o�b�t�@�[�̐���
		auto constHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto constDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(mapMatricesData_) + 0xff) & ~0xff); // 256�A���C�����g�ŃT�C�Y���w��
		ThrowIfFailed(device_->CreateCommittedResource(
			&constHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&constDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(constMatricesBuffer_.ReleaseAndGetAddressOf())));
		// �萔�o�b�t�@�[�r���[�̐���
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constMatricesBuffer_->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<UINT>(constMatricesBuffer_->GetDesc().Width);
		device_->CreateConstantBufferView(&cbvDesc, basicHeapHandle);
		// 3D���W�p�̍s��𐶐�
		worldMatrix_ = DirectX::XMMatrixScaling(scale_, scale_, scale_)
			* DirectX::XMMatrixRotationY(angle_ * (DirectX::XM_PI / 180.0f))
			* DirectX::XMMatrixTranslation(translate_.x, translate_.y, translate_.z);
		DirectX::XMFLOAT3 eye(0, 0, -5);
		DirectX::XMFLOAT3 target(0, 0, 0);
		DirectX::XMFLOAT3 up(0, 1, 0);
		viewMatrix_ = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
		projMatrix_ = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XM_PIDIV2, // ��p: 90�x
			static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_), // �A�X�y�N�g��
			1.0f, // near
			10.0f // far
		);
		// �萔���̃R�s�[
		constMatricesBuffer_->Map(0, nullptr, (void**)&mapMatricesData_);
		mapMatricesData_->world = worldMatrix_;
		mapMatricesData_->viewproj = viewMatrix_ * projMatrix_;
		constMatricesBuffer_->Unmap(0, nullptr);
	}

	basicHeapHandle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// ���C�e�B���O�p�f�[�^(CBV)�̐���
	{
		// �萔�o�b�t�@�[�̐���
		auto constHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto constDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(mapLightingData_) + 0xff) & ~0xff);
		ThrowIfFailed(device_->CreateCommittedResource(
			&constHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&constDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(constLightingBuffer_.ReleaseAndGetAddressOf())));
		// �萔�o�b�t�@�[�r���[�̐���
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constLightingBuffer_->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<UINT>(constLightingBuffer_->GetDesc().Width);
		device_->CreateConstantBufferView(&cbvDesc, basicHeapHandle);
		// �萔���̃R�s�[
		constLightingBuffer_->Map(0, nullptr, (void**)&mapLightingData_);
		mapLightingData_->ambientLight = ambientLight_;
		mapLightingData_->lightColor = lightColor_;
		mapLightingData_->lightDirection = lightDirection_;
		constLightingBuffer_->Unmap(0, nullptr);
	}

	basicHeapHandle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// �V�F�[�_�[���\�[�X�r���[(SRV)�̐���
	{
		// �e�N�X�`���o�b�t�@�̐���
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Format = metadata.format;
		textureDesc.Width = static_cast<UINT>(metadata.width);
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		textureDesc.MipLevels = metadata.mipLevels;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		auto textureHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device_->CreateCommittedResource(
			&textureHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(textureBuffer_.ReleaseAndGetAddressOf())));
		// �V�F�[�_�[���\�[�X�r���[�̐���
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device_->CreateShaderResourceView(textureBuffer_.Get(), &srvDesc, basicHeapHandle);
	}

	// �e�N�X�`���A�b�v���[�h�p�o�b�t�@�̐���
	ComPtr<ID3D12Resource> textureUploadBuffer;
	{
		const UINT64 textureBufferSize = GetRequiredIntermediateSize(textureBuffer_.Get(), 0, 1);
		auto textureUploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto textureUploadDesc = CD3DX12_RESOURCE_DESC::Buffer(textureBufferSize);
		ThrowIfFailed(device_->CreateCommittedResource(
			&textureUploadHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&textureUploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadBuffer)));
	}

	// �R�}���h���X�g�̐���
	{
		// �e�N�X�`���o�b�t�@�̓]��
		UpdateSubresources(commandList_.Get(), textureBuffer_.Get(), textureUploadBuffer.Get(), 0, 0, textureSubresources.size(), textureSubresources.data());
		auto uploadResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList_->ResourceBarrier(1, &uploadResourceBarrier);

		// ���߂̃N���[�Y
		commandList_->Close();
	}

	// �R�}���h���X�g�̎��s
	{
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
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

// �X�V����
void DXApplication::OnUpdate()
{
	// ��]�����Ă݂�
	angle_ += 1.0f;
	worldMatrix_ = worldMatrix_ = DirectX::XMMatrixScaling(scale_, scale_, scale_)
		* DirectX::XMMatrixRotationY(angle_ * (DirectX::XM_PI / 180.0f))
		* DirectX::XMMatrixTranslation(translate_.x, translate_.y, translate_.z);
	mapMatricesData_->world = worldMatrix_;
	mapMatricesData_->viewproj = viewMatrix_ * projMatrix_;
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
		// �f�B�X�N���v�^�e�[�u��
		// ���[�g�p�����[�^�ƃf�B�X�N���v�^�q�[�v��R�Â���
		ID3D12DescriptorHeap* ppHeaps[] = { basicHeap_.Get() };
		commandList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		commandList_->SetGraphicsRootDescriptorTable(0, basicHeap_->GetGPUDescriptorHandleForHeapStart());

		// �����_�[�^�[�Q�b�g�̐ݒ�
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), frameIndex, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		auto dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
		commandList_->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
		float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };  // ���F
		commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// �`�揈���̐ݒ�
		commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // �v���~�e�B�u�g�|���W�̐ݒ� (�O�p�|���S��)
		commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);                // ���_�o�b�t�@
		commandList_->IASetIndexBuffer(&indexBufferView_);                         // �C���f�b�N�X�o�b�t�@
		commandList_->DrawIndexedInstanced(fbxVertexInfo_.indices.size(), 1, 0, 0, 0);                        // �`��

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
		if (D3D12CreateDevice(tmpAdapter, level, IID_PPV_ARGS(&tmpDevice)) == S_OK) {
			break; // �����\�ȃo�[�W���������������烋�[�v��ł��؂�
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

void DXApplication::ThrowMessage(std::string message)
{
	throw std::runtime_error(message);
}
