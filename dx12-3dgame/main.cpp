#include "Application.h"


#ifdef _DEBUG
int main() {
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif // DEBUG
	// ����������Ȃ���COM��������WIC������ɓ��삵�Ȃ����Ƃ�����H
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);

	Application& app = Application::GetInstance();
	if (!app.Init()) {
		return -1;
	}
	app.Run();
	app.Terminate();
	return 0;
}