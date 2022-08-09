#include "WindowManager.h"
#include <tchar.h>

TWindowManager::TWindowManager(unsigned int windowWidth, unsigned int windowHeight) : windowWidth(windowWidth), windowHeight(windowHeight)
{
	Initialize();
}
TWindowManager::~TWindowManager() {
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

void TWindowManager::Initialize() {
	windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;//�R�[���o�b�N�֐��̎w��
	windowClass.lpszClassName = _T("DirectXTest");//�A�v���P�[�V�����N���X��
	windowClass.hInstance = GetModuleHandle(0);//�n���h���̎擾
	RegisterClassEx(&windowClass);//�A�v���P�[�V�����N���X(���������̍�邩���낵������OS�ɗ\������)
	RECT windowRect = { 0,0, static_cast<LONG>(windowWidth), static_cast<LONG>(windowHeight) };

	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);//�E�B���h�E�̃T�C�Y�͂�����Ɩʓ|�Ȃ̂Ŋ֐����g���ĕ␳����
//�E�B���h�E�I�u�W�F�N�g�̐���
	windowHandle = CreateWindow(windowClass.lpszClassName,//�N���X���w��
		_T("DX12�e�X�g"),//�^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,//�^�C�g���o�[�Ƌ��E��������E�B���h�E�ł�
		CW_USEDEFAULT,//�\��X���W��OS�ɂ��C�����܂�
		CW_USEDEFAULT,//�\��Y���W��OS�ɂ��C�����܂�
		windowRect.right - windowRect.left,//�E�B���h�E��
		windowRect.bottom - windowRect.top,//�E�B���h�E��
		nullptr,//�e�E�B���h�E�n���h��
		nullptr,//���j���[�n���h��
		windowClass.hInstance,//�Ăяo���A�v���P�[�V�����n���h��
		nullptr);//�ǉ��p�����[�^
}

// LRESULT? HWND?
// �E�B���h�E���o�����Ă�ԃ}�C�t���[���Ă΂��H
// Window�������ɓn���R�[���o�b�N�֐�
LRESULT TWindowManager::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// �E�B���h�E�j����
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // ����̏���
}
