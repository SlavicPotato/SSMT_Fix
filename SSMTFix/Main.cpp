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
		return a_actor == *g_thePlayer ?
		           (*rel_member(std::addressof((*g_thePlayer)->unkBDD)) & 0x1) == 0x1 :
		           rel_member(std::addressof(a_actor->actorState1))->sprinting;
	}

	static bool is_valid_movement_type_name(const BSFixedString& a_type) noexcept
	{
		return hash::stricmp_ascii(a_type.c_str(), "NPC", 3) == 0;
	}

	enum class MTExclusionEntryFlags
	{
		kNone = 0,

		kSprint = 1u << 0,
		kSneak  = 1u << 1,

		kAll = kSprint | kSneak
	};
	DEFINE_ENUM_CLASS_BITWISE(MTExclusionEntryFlags);

	static stl::flag<MTExclusionEntryFlags> s_bowExclusionFlags   = MTExclusionEntryFlags::kSprint;
	static stl::flag<MTExclusionEntryFlags> s_magicExclusionFlags = MTExclusionEntryFlags::kSprint;

	class ExcludedMovementTypes
	{
	public:
		ExcludedMovementTypes() :
			m_data{
				std::make_pair(s_bowExclusionFlags, "NPCBowDrawn"),
				std::make_pair(s_bowExclusionFlags, "NPCBowDrawnQuickShot"),
				std::make_pair(s_magicExclusionFlags, "NPCMagicCasting")
			}
		{
		}

		inline static auto& GetSingleton()
		{
			static ExcludedMovementTypes result;
			return result;
		}

		inline constexpr bool IsExcluded(
			const BSFixedString&  a_movementType,
			MTExclusionEntryFlags a_from) noexcept
		{
			for (auto& e : m_data)
			{
				if (e.second == a_movementType)
				{
					return e.first.test_any(a_from);
				}
			}

			return false;
		}

	private:
		std::array<std::pair<stl::flag<MTExclusionEntryFlags>, BSFixedString>, 3> m_data;
	};

	static bool should_override_mt(
		const BSFixedString&  a_type,
		MTExclusionEntryFlags a_flags)
	{
		return !ExcludedMovementTypes::GetSingleton().IsExcluded(a_type, a_flags);
	}

	static BSFixedString get_mt_string(
		const BSFixedString& a_default,
		Actor*               a_actor) noexcept
	{
		if ((a_actor == *g_thePlayer || s_enableNPC) &&
		    is_valid_movement_type_name(a_default))
		{
			if (is_sprinting(a_actor) &&
			    should_override_mt(a_default, MTExclusionEntryFlags::kSprint))
			{
				static BSFixedString result("NPCSprinting");
				return result;
			}

			if (rel_member(std::addressof(a_actor->actorState1))->sneaking &&
			    should_override_mt(a_default, MTExclusionEntryFlags::kSneak))
			{
				static BSFixedString result("NPCSneaking");
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
		s_magicExclusionFlags.set(MTExclusionEntryFlags::kSprint, !reader.GetBoolValue("", "EnableSprintCasting", false));
	}

	bool Initialize(const SKSEInterface* a_skse)
	{
		LoadSettings();

		gLog.Message("NPC support %s", s_enableNPC ? "ENABLED" : "disabled");
		gLog.Message("Sprint casting %s", !s_magicExclusionFlags.test(MTExclusionEntryFlags::kSprint) ? "ENABLED" : "disabled");

		const bool result = Patch();

		if (!result)
		{
			gLog.Error("Patch failed");
		}

		return result;
	}
}