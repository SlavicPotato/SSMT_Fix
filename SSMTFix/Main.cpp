#include "pch.h"

#include "Main.h"

namespace SSMTF
{
	static bool s_enableNPC = false;

	using lookupMT_t = BGSMovementType* (*)(const BSFixedString&);

	static lookupMT_t s_Character_GetCurrentMovementTypeData_LookupMT_o = nullptr;

	template <class T>
	static constexpr T* rel_member(T* a_v) noexcept
	{
		return IAL::ver() >= VER_1_6_629 ?
		           reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(a_v) + 8) :
		           a_v;
	}

	static constexpr bool is_sprinting(Actor* a_actor) noexcept
	{
		const auto player = *g_thePlayer;

		return rel_member(std::addressof(a_actor->actorState1))->sprinting &&
		       (a_actor != player || rel_member(std::addressof(player->flagBDD))->test(PlayerCharacter::FlagBDD::kWantsSprint));
	}

	static constexpr bool is_valid_movement_type_name(const BSFixedString& a_type) noexcept
	{
		return hash::stricmp_ascii(a_type.c_str(), "NPC", 3) == 0;
	}

	static const BSFixedString& get_mt_string(
		const BSFixedString& a_default,
		Actor*               a_actor) noexcept
	{
		if ((a_actor == *g_thePlayer || s_enableNPC) &&
		    is_valid_movement_type_name(a_default))
		{
			if (rel_member(std::addressof(a_actor->actorState1))->sneaking)
			{
				static BSFixedString result("NPCSneaking");
				return result;
			}

			if (is_sprinting(a_actor))
			{
				static BSFixedString result("NPCSprinting");
				return result;
			}
		}

		return a_default;
	}

	static BGSMovementType* Character_GetCurrentMovementTypeData_LookupMT(
		const BSFixedString& a_movementTypeName,
		Actor*               a_actor)
	{
		return s_Character_GetCurrentMovementTypeData_LookupMT_o(get_mt_string(a_movementTypeName, a_actor));
	}

	static bool Patch()
	{
		const IAL::Address<std::uintptr_t> addr(36919, 37944, 0x8E, 0x133);

		if (!addr.get())
		{
			return false;
		}

		struct Assembly : JITASM::JITASM
		{
			Assembly() :
				JITASM(ISKSE::GetLocalTrampoline())
			{
				Xbyak::Label callLabel;

				mov(rdx, IAL::IsAE() ? rbp : rsi);  // Actor
				jmp(ptr[rip + callLabel]);

				L(callLabel);
				dq(std::uintptr_t(Character_GetCurrentMovementTypeData_LookupMT));
			};
		};

		Assembly code;

		return hook::call5(
			ISKSE::GetBranchTrampoline(),
			addr.get(),
			code.get(),
			s_Character_GetCurrentMovementTypeData_LookupMT_o);
	}

	static void LoadSettings()
	{
		INIConfReader reader(PLUGIN_INI_FILE_NOEXT);

		s_enableNPC = reader.GetBoolValue("", "ApplyToNPC", true);
	}

	bool Initialize(const SKSEInterface* a_skse)
	{
		LoadSettings();

		gLog.Message("NPC support %s", s_enableNPC ? "ENABLED" : "disabled");

		const bool result = Patch();

		if (!result)
		{
			gLog.Error("Patch failed");
		}

		return result;
	}
}