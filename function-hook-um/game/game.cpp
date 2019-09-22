#include "game.h"

bool r6manager::verify_game( )
{
	// get the hooked function

	m_driver_control = kernel_control_function( );

	if ( !m_driver_control )
		return false;

	// get process id

	m_pid = GetProcId( "RainbowSix.exe" );

	if ( !m_pid )
		return false;

	// get base address

	m_base = call_driver_control(
		m_driver_control, ID_GET_PROCESS_BASE, m_pid );

	printf( "> pid: %d base: %llx\n", m_pid, m_base );

	if ( !m_base )
		return false;

	// get r6 game manager

	m_game_manager = read<UINT_PTR>( m_base + OFFSET_GAME_MANAGER );

	if ( !m_game_manager )
		return false;

	printf("> manager: %llx\n", m_game_manager);

	m_game_status = read<UINT_PTR>(m_base + OFFSET_GAME_STAUS_MANAGER);

	if (!m_game_status)
		return false;

	printf("> m_game_status: %llx\n", m_game_status);

	uint64_t GameRenderer = read<UINT_PTR>(m_base + OFFSET_CAMERA_MANAGER);

	if (!GameRenderer)
		return false;

	auto pGameRenderer = read<UINT_PTR>(GameRenderer + 0x0);

	if (!pGameRenderer)
		return false;

	auto pEngineLink = read<UINT_PTR>(pGameRenderer + OFFSET_CAMERA_ENGINELINK);


	if (!pGameRenderer)
		return false;

	auto pEngine = read<UINT_PTR>(pEngineLink + OFFSET_CAMERA_ENGINE);

	if (!pEngine)
		return false;

	m_game_camera = read<UINT_PTR>(pEngine + OFFSET_CAMERA_ENGINE_CAMERA);

	if (!m_game_camera)
		return false;

	printf("> m_game_camera: %llx\n", m_game_camera);

	TargetHWND = FindWindowA("R6Game", NULL);

	return true;
}