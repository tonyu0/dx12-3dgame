#include <Windows.h>

class TWindowManager {
public:
	TWindowManager(unsigned int windowWidth, unsigned int windowHeight);
	~TWindowManager();
	void Initialize();
	const HWND& GetHandle() const { return windowHandle; }
	unsigned int GetWidth() const { return windowWidth; }
	unsigned int GetHeight() const { return windowHeight; }
private:
	static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	unsigned int windowWidth;
	unsigned int windowHeight;
	WNDCLASSEX windowClass;
	HWND windowHandle;
};