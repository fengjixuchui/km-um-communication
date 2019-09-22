#include <windows.h>
#include <vector>
#include <cstdint> 
#include "offsets.h"

#include "..\core\kernel.h"
#include "..\util\process.h"
#include "..\vector.h"

class r6manager
{
private:
	void* m_driver_control;			// driver control function
	unsigned long m_pid;			// process id
	uintptr_t m_base;				// game m_base (as integer for arithmetic)
	uintptr_t m_game_manager;		// address of R6GameManager* (as integer for arithmetic)
	uintptr_t m_game_camera;       // address of R6CameraManager* (as integer for arithmetic)
	uintptr_t m_game_status;       // address of R6CameraManager* (as integer for arithmetic)
	
public:

	r6manager( ) { }
	bool verify_game( );
	HWND TargetHWND;

	__forceinline BOOLEAN KeRtlCopyMemory(const DWORD64 Address, const PVOID Buffer, const DWORD_PTR Size, const BOOLEAN Write) {

		if (!this->m_pid || !Address || !Buffer || !Size)
			return false;

		E_COMMAND_CODE ioctl;

		if (Write)
			ioctl = E_COMMAND_CODE::ID_WRITE_PROCESS_MEMORY;
		else
			ioctl = E_COMMAND_CODE::ID_READ_PROCESS_MEMORY;

		MEMORY_STRUCT memory_struct = { 0 };
		memory_struct.process_id = this->m_pid;
		memory_struct.address = reinterpret_cast<void*>(Address);
		memory_struct.size = Size;
		memory_struct.buffer = Buffer;

		return (call_driver_control(this->m_driver_control, ioctl, &memory_struct)) == 0 ? true : false;
	}

	template <typename Type>
	Type read(const DWORD64 address)
	{
		if (!address)
			return Type();

		Type buffer;
		return KeRtlCopyMemory(address, &buffer, sizeof(Type), FALSE) ? buffer : Type();
	};

	template <class Type>
	void write(const DWORD64 address, Type data)
	{
		if (!address)
			return;

		KeRtlCopyMemory(address, &data, sizeof(Type), TRUE);
	};

	Vector2 GetDisplaySize()
	{
		RECT rect;
		GetClientRect(TargetHWND, &rect);
		return Vector2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
	}

	bool IsInGame()
	{
		if (!m_game_status)
			return false;

		return read<bool>(m_game_status + 0x324);
	}


	Vector3 GetViewRight()
	{
		if (!m_game_camera)
			return Vector3();

		return read<Vector3>(m_game_camera + OFFSET_CAMERA_RIGHT);
	}

	Vector3 GetViewUp()
	{
		if (m_game_camera)
			return Vector3();

		return read<Vector3>(m_game_camera + OFFSET_CAMERA_UP);
	}

	Vector3 GetViewForward()
	{
		if (!m_game_camera)
			return Vector3();

		return read<Vector3>(m_game_camera + OFFSET_CAMERA_FORWARD);
	}

	Vector3 GetViewTranslation()
	{
		if (!m_game_camera)
			return Vector3();

		return read<Vector3>(m_game_camera + OFFSET_CAMERA_TRANSLATION);
	}

	float GetViewFovX()
	{
		if (!m_game_camera)
			return 0.f;

		return read<float>(m_game_camera + OFFSET_CAMERA_FOVX);
	}

	float GetViewFovY()
	{
		if (!m_game_camera)
			return 0.f;

		return read<float>(m_game_camera + OFFSET_CAMERA_FOVY);
	}

	bool WorldToScreen(Vector3 position, Vector2* Screen)
	{
		if (!m_game_camera)
			return false;

		Vector3 temp = position - GetViewTranslation();

		float x = temp.Dot(GetViewRight());
		float y = temp.Dot(GetViewUp());
		float z = temp.Dot(GetViewForward() * -1.f);

		Vector2 DisplaySize = GetDisplaySize();

		Screen->x = (DisplaySize.x / 2.f) * (1.f + x / GetViewFovX() / z);
		Screen->y = (DisplaySize.y / 2.f) * (1.f - y / GetViewFovY() / z);

		return z >= 1.0f ? true : false;
	}


	Vector4 CreateFromYawPitchRoll(float yaw, float pitch, float roll)
	{
		Vector4 result;
		float cy = cos(yaw * 0.5);
		float sy = sin(yaw * 0.5);
		float cr = cos(roll * 0.5);
		float sr = sin(roll * 0.5);
		float cp = cos(pitch * 0.5);
		float sp = sin(pitch * 0.5);

		result.mData[3] = cy * cr * cp + sy * sr * sp;
		result.mData[0] = cy * sr * cp - sy * cr * sp;
		result.mData[1] = cy * cr * sp + sy * sr * cp;
		result.mData[2] = sy * cr * cp - cy * sr * sp;

		return result;
	}

	void set_angles(uint64_t local_player, Vector3& angles)
	{
		float d2r = 0.01745329251f;
		Vector4 new_angles = CreateFromYawPitchRoll(angles.z * d2r, 0.f, angles.x * d2r);

		uint64_t pSkeleton = read<uint64_t>(local_player + 0x20);

		if (pSkeleton)
		{
			uint64_t viewAngle2 = read<uint64_t>(pSkeleton + 0x11B8);
			write(viewAngle2 + 0xC0, new_angles);
		}
	}

	unsigned long get_player_count( )
	{
		uint64_t game_manager = read<uint64_t>( m_base + OFFSET_GAME_MANAGER );

		if ( !game_manager )
			return NULL;

		return read<unsigned long>( game_manager + OFFSET_GAME_MANAGER_ENTITY_COUNT );
	}
	
	uint64_t get_player_by_id( unsigned int i )
	{
		uint64_t game_manager = read<uint64_t>( m_base + OFFSET_GAME_MANAGER );

		if ( !game_manager )
			return NULL;

		uint64_t entity_list = read<uint64_t>( game_manager + OFFSET_GAME_MANAGER_ENTITY_LIST2 );

		if ( !entity_list )
			return NULL;

		if ( i > get_player_count( ) )
			return NULL;

		uint64_t entity = read<uint64_t>( entity_list + ( sizeof( PVOID ) * i ) );

		if ( !entity )
			return NULL;

		return read<uint64_t>( entity + OFFSET_STATUS_MANAGER_LOCALENTITY );
	}
	
	unsigned short get_player_team( uint64_t player )
	{
		if ( !player )
			return 0xFF;

		uint64_t replication = read<uint64_t>( player + OFFSET_ENTITY_REPLICATION );

		if ( !replication )
			return 0xFF;

		unsigned long online_team_id = read<unsigned long>( replication + OFFSET_ENTITY_REPLICATION_TEAM );
		return LOWORD( online_team_id );
	}
	
	Vector3 get_player_head(uint64_t player)
	{
		if (!player)
			return Vector3();

		uint64_t pSkeleton = read<uint64_t>(player + 0x20);

		if (!pSkeleton)
			return Vector3();

		return read<Vector3>(pSkeleton + 0x6A0);
	}

	uint64_t get_local_player(  )
	{
		uint64_t status_manager = read<uint64_t>( m_base + OFFSET_STATUS_MANAGER );

		if ( !status_manager )
			return NULL;

		uint64_t entity_container = read<uint64_t>( status_manager + OFFSET_STATUS_MANAGER_CONTAINER );

		if ( !entity_container )
			return NULL;

		entity_container = read<uint64_t>( entity_container );

		if ( !entity_container )
			return NULL;

		return read<uint64_t>( entity_container + OFFSET_STATUS_MANAGER_LOCALENTITY );
	}


	bool get_enemy_players( std::vector<uint64_t>& players )
	{
		uint64_t local_player = get_local_player( );

		if ( !_VALID( local_player ) )
			return FALSE;

		unsigned short local_team = get_player_team( local_player );

		unsigned int count = get_player_count( );

		if ( count > 255 )
			return false;

		for ( unsigned int i = 0; i < count; i++ )
		{
			uint64_t target_player = get_player_by_id( i );

			if ( !_VALID( target_player ) )
				continue;

			if ( target_player == local_player )
				continue;

			if ( get_player_team( target_player ) == local_team )
				continue;

			players.push_back( target_player );
		}

		return true;
	}

	uint64_t get_spotted_marker( uint64_t player )
	{
		if ( !_VALID( player ) )
			return FALSE;

		uint64_t component_chain = read<uint64_t>( player + OFFSET_ENTITY_COMPONENT ); //

		if ( !_VALID( component_chain ) )
			return NULL;

		uint64_t component_list = read<uint64_t>( component_chain + OFFSET_ENTITY_COMPONENT_LIST );

		if ( !_VALID( component_list ) )
			return NULL;

		// loop through components
		// look for PlayerMarkerComponent object

		// uint64_t marker_component = NULL;

		for ( unsigned int i = 15; i < 22; i++ )
		{
			uint64_t component = read<uint64_t>( component_list + i * sizeof( uint64_t ) );

			if ( !_VALID( component ) )
				continue;

			const uint64_t vt_marker = ENTITY_MARKER_VT_OFFSET;

			uint64_t vt_table = read<uint64_t>( component );
			uint64_t vt_offset = vt_table - m_base;

			if ( vt_offset == vt_marker )
				return component;

			//marker_component = component;
		}

		//if ( marker_component )
		//{
		//	unsigned int enable = 65793;
		//	write_virtual_memory( marker_component + ENTITY_MARKER_ENABLED_OFFSET, &enable, sizeof( unsigned int ) );
		//}

		return NULL;
	}

	bool esp(  )
	{
		std::vector<uint64_t> enemy_players;

		if ( !get_enemy_players( enemy_players ) )
		{
			printf( "> no enemy players\n" );
			return false;
		}

		printf( "> players: %llu\n", enemy_players.size( ) );

		std::vector<uint64_t> enemy_marker_components;

		for ( uint64_t player : enemy_players )
		{
			uint64_t marker_component = get_spotted_marker( player );

			printf( "    %llx marker=%llx\n", player, marker_component );

			if ( _VALID( marker_component ) )
				enemy_marker_components.push_back( marker_component );
		}

		printf( "> markers: %llu\n", enemy_marker_components.size( ) );

		for ( uint64_t marker : enemy_marker_components )
		{
			printf( "    %llx\n", marker );
			write<unsigned int>( marker + ENTITY_MARKER_ENABLED_OFFSET, 65793 );
		}

		return true;
	}

	bool glowaaa() //glow/chams
	{
		float g = 0.00000000000001;

		float R, G, B;
		R = 0;
		G = 255;
		B = 255;

		uint64_t glowbase = read<uint64_t>(m_base + OFFSET_ENVIRONMENT_AREA);

		if (!glowbase)
			return false;

		uint64_t glowbase1 = read<uint64_t>(glowbase + 0xB8);
		write<float>(glowbase1 + 0x110, R);
		write<float>(glowbase1 + 0x114, G);
		write<float>(glowbase1 + 0x118, B);

		write<float>(glowbase1 + 0x138, g);
		write<float>(glowbase1 + 0x130, g);
		write<float>(glowbase1 + 0x130 + 0x4, g);

		return true;
	}

	/*void Box3D(int HeadX, int HeadY, int bottomX, int bottomY, float Distance_Player, DWORD Color)
	{
		float drawW = 245 / Distance_Player;
		float drawW2 = 135 / Distance_Player;
		float drawW3 = 55 / Distance_Player;

		DrawLine(bottomX - drawW, bottomY, bottomX + drawW, bottomY, 1, Color);
		DrawLine(HeadX - drawW, HeadY, HeadX + drawW, HeadY, 1, Color);
		DrawLine(HeadX - drawW, HeadY, bottomX - drawW, bottomY, 1, Color);
		DrawLine(HeadX + drawW, HeadY, bottomX + drawW, bottomY, 1, Color);
		DrawLine(HeadX - drawW2, HeadY - drawW3, bottomX - drawW2, bottomY - drawW3, 1, Color);
		DrawLine(bottomX - drawW, bottomY, bottomX - drawW2, bottomY - drawW3, 1, Color);
		DrawLine(HeadX - drawW, HeadY, HeadX - drawW2, HeadY - drawW3, 1, Color);
		DrawLine((HeadX + drawW) + drawW2, HeadY - drawW3, (bottomX + drawW) + drawW2, bottomY - drawW3, 1, Color);
		DrawLine(bottomX + drawW, bottomY, (bottomX + drawW) + drawW2, bottomY - drawW3, 1, Color);
		DrawLine(HeadX + drawW, HeadY, (HeadX + drawW) + drawW2, HeadY - drawW3, 1, Color);
		DrawLine(HeadX - drawW2, HeadY - drawW3, (HeadX + drawW) + drawW2, HeadY - drawW3, 1, Color);
		DrawLine(bottomX - drawW2, bottomY - drawW3, (bottomX + drawW) + drawW2, bottomY - drawW3, 1, Color);
	}


	/*bool Box3D(int local_player)
	{
		float Distance = GetDistance(pPlayer Object origin, pPlayer Object Pos);

		uint64_t FunESPBox3D = read<uint64_t>(m_base + 0x370);

		if (FunESPBox3D)
			Box3D((int)HeadScreen.x, (int)HeadScreen.y, (int)FootScreen.x, (int)FootScreen.y, Distance, ColorESP);

		return;
	}
	*/


	bool no_recoil(uintptr_t local_player)
	{
		uintptr_t lpVisualCompUnk = read<uintptr_t>(local_player + 0x98);

		if (!lpVisualCompUnk)
			return false;

		uintptr_t lpWeapon = read<uintptr_t>(lpVisualCompUnk + 0xC8);

		if (!lpWeapon)
			return false;

		uintptr_t lpCurrentDisplayWeapon = read<uintptr_t>(lpWeapon + 0x208);

		if (!lpCurrentDisplayWeapon)
			return false;

		float almost_zero = 0.002f;
		write<float>(lpCurrentDisplayWeapon + 0x50, almost_zero);
		write<float>(lpCurrentDisplayWeapon + 0xA0, almost_zero);

		return true;
	}

	uint64_t net_ptr()
	{
		uint64_t r1 = read<uint64_t>(m_base + OFFSET_NETWORK_MANAGER);
		if (!r1)
			return 0;

		uint64_t r2 = read<uint64_t>(r1 + 0xF0);
		if (!r2)
			return 0;

		return read<uint64_t>(r2 + 0x8);
	}

	void set_clip(Vector3 set)
	{
		uint64_t tmp = net_ptr();
		if (!tmp)
			return;

		write<Vector3>(tmp + 0x530, set);
	}

	bool Aim1()
	{
		uint64_t Aim0 = read<uint64_t>(m_base + OFFSET_GAME_MANAGER);

		if (!Aim0)
			return false;


		uint64_t Aim = read<uint64_t>(Aim0 + 0x5180);
		uint64_t Aim1 = read<uint64_t>(Aim + 0x7190);
		uint64_t Aim2 = read<uint64_t>(Aim1 = 0x5180);
		uint64_t Aim4 = read<uint64_t>(Aim2 = 0x5180);
		uint64_t Aim3 = read<uint64_t>(Aim4 + 0x55);

		write<unsigned int>(Aim + 0x5180, 10000);
		write<unsigned int>(Aim1 + 0x7190, 10000);
		write<unsigned int>(Aim2 + 0x5180, 10000);
		write<unsigned int>(Aim2 + 0x5180, 10000);
		write<unsigned int>(Aim3 + 0x55, 10000);

		return true;
	}

	bool jum()
	{
		uint64_t jump = read<uint64_t>(m_base + 0x04E20A50);

		if (!jump)
			return false;

		uint64_t jum1 = read<uint64_t>(jump + 0x50);
		uint64_t jum2 = read<uint64_t>(jum1 + 0x8);
		uint64_t jum3 = read<uint64_t>(jum2 + 0x68);
		uint64_t jum4 = read<uint64_t>(jum3 + 0x40);
		uint64_t jum5 = read<uint64_t>(jum4 + 0xC0);
		uint64_t jum6 = read<uint64_t>(jum5 + 0x38);
		uint64_t jum7 = read<uint64_t>(jum6 + 0x10);
		uint64_t jum8 = read<uint64_t>(jum7 + 0x3F8);

		write<byte>(jum8, 1);

		return true;
	}

	bool Friend()
	{
		uint64_t spec = read<uint64_t>(m_base + OFFSET_SPOOF_SPECTATE);
		write<char>(spec + 0x5d, 1);

		return true;
	}
	bool damagemultiplier()
	{
		int damagevalue = 30;//damage  value

		uint64_t damgebase = read<uint64_t>(m_base + OFFSET_GAME_MANAGER);

		if (!damgebase)
			return false;

		uint64_t dmage = read<uint64_t>(damgebase + 0x1f8);
		uint64_t dmage1 = read<uint64_t>(dmage + 0xd8);
		uint64_t dmage2 = read<uint64_t>(dmage1 + 0x48);
		uint64_t dmage3 = read<uint64_t>(dmage2 + 0x130);
		uint64_t dmage4 = read<uint64_t>(dmage3 + 0x120);
		uint64_t dmage5 = read<uint64_t>(dmage4 + 0x0);

		write<unsigned int>(dmage5 + 0x40, 30);


		return true;
	}

	bool localTeam()
	{
		while (true)
		{

			int TeamLocal = 4;

			if (!TeamLocal)
				return false;

			uint64_t Local = read<uint64_t>(TeamLocal + OFFSET_LOCAL);
			uint64_t Local1 = read<uint64_t>(Local + OFFSET_LOCAL1);
			uint64_t Local2 = read<uint64_t>(Local1 + OFFSET_LOCAL2);
			uint64_t Local3 = read<uint64_t>(Local2 + OFFSET_LOCAL3);

			write<int>(Local3 + OFFSET_LOCAL4, 4);

			return true;
		}
	}

	bool Crosshairid()
	{
		int cross = 200; //


		uint64_t trigger = read<uint64_t>(m_base + 0x4FF0F30);

		if (!trigger)
			return false;

		uint64_t id = read<uint64_t>(trigger + 0x50);
		uint64_t id1 = read<uint64_t>(id + 0x80);
		uint64_t id2 = read<uint64_t>(id1 + 0x58);
		uint64_t id3 = read<uint64_t>(id2 + 0x418);


		write<unsigned int>(id3 + 0x304, 200);




		return true;
	}

	bool speedrack(uintptr_t local_player)
	{
		uintptr_t speed = read<uintptr_t>(local_player + 0x50);

		if (!speed)
			return false;

		float value = 1.500;
		write<float>(speed + 0x48, value);

	}


	bool fovgun()
	{
		float W, P;
		W = 1.800f; //Weapon Fov

		uint64_t  OFFSET_FOV = read<uint64_t>(m_base + 0x5017730);

		if (!OFFSET_FOV)
			return false;

		uint64_t weapon_fov = read<uint64_t>(OFFSET_FOV + 0x28);
		uint64_t class1_unknown = read<uint64_t>(weapon_fov + 0x0);

		write<float>(class1_unknown + 0x3C, W);

		return true;
	}

	bool Triggerbot()
	{

		while (true)
		{

			byte value = 1;
			byte value1 = 0;

			uint64_t triggerbot_offset = read<uint64_t>(m_base + OFFSET_TRIGGERBOT);
			uint64_t class1 = read<uint64_t>(triggerbot_offset + OFFSET_TRIGGERBOT1);
			uint64_t class2 = read<uint64_t>(class1 + OFFSET_TRIGGERBOT2);
			uint64_t class3 = read<uint64_t>(class2 + OFFSET_TRIGGERBOT3);
			uint64_t class4 = read<uint64_t>(class3 + OFFSET_TRIGGERBOT4);

			int class5 = read<int>(class4 + OFFSET_TRIGGERBOT5);

			while (class5 == 1)
			{
				if (GetAsyncKeyState(VK_MENU))
				{
					mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
					mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
					break;
				}
			}

			if (GetAsyncKeyState(VK_F12))
				break;
		}

		return 0;
	}

	//bool fov()
	//{
	//	float W = 0.0f;
	//	float P = 0.0f;

	//	P = 2.000f; // Player Fov

	//	uint64_t  OFFSET_FOV = read<uint64_t>(m_base + 0x5017730);

	//	if (!OFFSET_FOV)
	//		return false;

	//	uint64_t weapon_fov = read<uint64_t>(OFFSET_FOV + 0x28);
	//	uint64_t class1_unknown = read<uint64_t>(weapon_fov + 0x0);


	//	write<float>(class1_unknown + 0x38, P);

	//	return true;
	//}
};