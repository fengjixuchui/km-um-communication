#include "client.h"

DWORD WINAPI main_thread(LPVOID)
{
	while (TRUE)
	{
		if (game.IsInGame())
		{
			if (esp)
				game.esp();

			if (clip)
			{
				game.jum();
				game.set_clip(Vector3());
			}

			if (recoil)
				game.no_recoil(game.get_local_player());

			if (speed)
				game.speedrack(game.get_local_player());

			if (damage)
				game.damagemultiplier();

			if (fov)
				game.fovgun();

			if (aimassist)
			{
				game.Aim1();
				game.Crosshairid();
			}

			if (glow)
				game.glowaaa();
		}
	}
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if (game.verify_game())
	{
		HWND wnd{};
		MessageBox(wnd, "Start Rainbow Six Siege before starting the cheat!", "yoinkers", MB_ICONERROR);
		return 0;
	}

	CreateThread(NULL, 0, main_thread, NULL, NULL, NULL);

	while ((GetAsyncKeyState(VK_END) & 1) == 0)
		Sleep(100);

	return 0;
}