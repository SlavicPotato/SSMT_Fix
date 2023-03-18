#include "pch.h"

#include "Main.h"

namespace SSMTF
{
	static bool s_enableNPC   = false;
	static bool s_enableExtra = false;

	using lookupMT_t = BGSMovementType* (*)(const BSFixedString&);

	static const IAL::Address<lookupMT_t> LookupMTByName(23268, 23727);

	template <class T>
	static constexpr T& rel_member(T& a_v) noexcept
	{
		return IAL::ver() >= VER_1_6_629 ?
		           *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(std::addressof(a_v)) + 8) :
		           a_v;
	}

	template <class T>
	static constexpr const T& rel_member(const T& a_v) noexcept
	{
		return rel_member(const_cast<T&>(a_v));
	}

	static constexpr bool is_valid_movement_type_name(
		const BSFixedString& a_type) noexcept
	{
		return hash::stricmp_ascii(a_type.c_str(), "NPC", 3) == 0;
	}

#if 0
	enum class ANIM_EQUIPPED_WEAPON_TYPE : std::int32_t
	{
		kNone = -1,

		kHandToHandMelee = 0,
		kOneHandSword    = 1,
		kOneHandDagger   = 2,
		kOneHandAxe      = 3,
		kOneHandMace     = 4,
		kTwoHandSword    = 5,
		kTwoHandAxe      = 6,
		kBow             = 7,
		kStaff           = 8,
		kMagic           = 9,
		kShield          = 10,
		kCrossbow        = 12,
	};

	static constexpr bool is_1hm(
		ANIM_EQUIPPED_WEAPON_TYPE a_t) noexcept
	{
		switch (a_t)
		{
		case ANIM_EQUIPPED_WEAPON_TYPE::kHandToHandMelee:
		case ANIM_EQUIPPED_WEAPON_TYPE::kOneHandSword:
		case ANIM_EQUIPPED_WEAPON_TYPE::kOneHandDagger:
		case ANIM_EQUIPPED_WEAPON_TYPE::kOneHandAxe:
		case ANIM_EQUIPPED_WEAPON_TYPE::kOneHandMace:
		case ANIM_EQUIPPED_WEAPON_TYPE::kShield:
			return true;
		default:
			return false;
		}
	}
#endif

	struct StringHolder
	{
		enum class CMT
		{
			kNPCDefault,
			kNPC1HM,
			kNPC2HM,
			kNPCBow,
			kNPCMagic
		};

		StringHolder() :
			commonMTs{
				"NPCDefault",
				"NPC1HM",
				"NPC2HM",
				"NPCBow",
				"NPCMagic"
			},
			sneakIgnoreMTs{
				"NPCBlocking",
				"NPCBowDrawn"
			}
		{
		}

		[[nodiscard]] inline static const auto& GetSingleton()
		{
			static StringHolder result;
			return result;
		}

		[[nodiscard]] constexpr auto* Get(CMT a_index) const noexcept
		{
			return std::addressof(commonMTs[stl::underlying(a_index)]);
		}

		[[nodiscard]] constexpr bool IsCommonMT(
			const BSFixedString& a_name) const noexcept
		{
			return std::find(
					   commonMTs.begin(),
					   commonMTs.end(),
					   a_name) != commonMTs.end();
		}

		[[nodiscard]] constexpr bool IsSneakIgnoredMT(
			const BSFixedString& a_name) const noexcept
		{
			return std::find(
					   sneakIgnoreMTs.begin(),
					   sneakIgnoreMTs.end(),
					   a_name) != sneakIgnoreMTs.end();
		}

		BSFixedString NPCSneaking{ "NPCSneaking" };
		BSFixedString NPCSprinting{ "NPCSprinting" };
		BSFixedString NPCAttacking{ "NPCAttacking" };

		std::array<BSFixedString, 5> commonMTs;
		std::array<BSFixedString, 2> sneakIgnoreMTs;
	};

	static constexpr bool is_1h_melee_weapon(const TESForm* a_form) noexcept
	{
		if (const auto weap = a_form->As<TESObjectWEAP>())
		{
			switch (weap->type())
			{
			case WEAPON_TYPE::kHandToHandMelee:
			case WEAPON_TYPE::kOneHandSword:
			case WEAPON_TYPE::kOneHandDagger:
			case WEAPON_TYPE::kOneHandAxe:
			case WEAPON_TYPE::kOneHandMace:
				return true;
			}
		}

		return false;
	}

	static const BSFixedString* get_eq_mt_name(
		const Actor*                   a_actor,
		const ActorState::ActorState1& a_actorState1) noexcept
	{
		const auto* const pm = rel_member(a_actor->processManager);
		if (!pm)
		{
			return nullptr;
		}

		// Handle cases where incorrect MT is used during left attack
		if (a_actor == *g_thePlayer &&
		    a_actorState1.meleeAttackState != ActorState::ATTACK_STATE_ENUM::kNone)
		{
			if (const auto* const high = pm->high)
			{
				if (const auto& ad = high->attackData)
				{
					if (ad->data.flags.test(RE::AttackData::AttackFlag::kChargeAttack))
					{
						const auto* const left = pm->equippedObject[ActorProcessManager::kEquippedHand_Left];

						if (!left || is_1h_melee_weapon(left))
						{
							return std::addressof(StringHolder::GetSingleton().NPCAttacking);
						}
					}
				}
			}
		}

		// Use NPCMagic if a spell or staff is equipped in either hand (vanilla behavior)
		for (auto& object : pm->equippedObject)
		{
			if (!object)
			{
				continue;
			}

			switch (object->formType)
			{
			case SpellItem::kTypeID:

				return StringHolder::GetSingleton().Get(StringHolder::CMT::kNPCMagic);

			case TESObjectWEAP::kTypeID:

				if (static_cast<const TESObjectWEAP*>(object)->type() == WEAPON_TYPE::kStaff)
				{
					return StringHolder::GetSingleton().Get(StringHolder::CMT::kNPCMagic);
				}

				break;
			}
		}

		std::uint32_t equippedCount = 0;

		// Weapons/shield, prioritize the right-hand slot
		for (auto it = std::rbegin(pm->equippedObject); it != std::rend(pm->equippedObject); ++it)
		{
			const auto* const object = *it;

			if (!object)
			{
				continue;
			}

			equippedCount++;

			switch (object->formType)
			{
			case TESObjectWEAP::kTypeID:

				switch (static_cast<const TESObjectWEAP*>(object)->type())
				{
				case WEAPON_TYPE::kBow:
				case WEAPON_TYPE::kCrossbow:
					return StringHolder::GetSingleton().Get(StringHolder::CMT::kNPCBow);
				case WEAPON_TYPE::kTwoHandAxe:
				case WEAPON_TYPE::kTwoHandSword:
					return StringHolder::GetSingleton().Get(StringHolder::CMT::kNPC2HM);
				case WEAPON_TYPE::kHandToHandMelee:
				case WEAPON_TYPE::kOneHandSword:
				case WEAPON_TYPE::kOneHandDagger:
				case WEAPON_TYPE::kOneHandAxe:
				case WEAPON_TYPE::kOneHandMace:
					return StringHolder::GetSingleton().Get(StringHolder::CMT::kNPC1HM);
				}

				break;

			case TESObjectARMO::kTypeID:

				if (static_cast<const TESObjectARMO*>(object)->IsShield())
				{
					return StringHolder::GetSingleton().Get(StringHolder::CMT::kNPC1HM);
				}

				break;
			}
		}

		return !equippedCount ?
		           StringHolder::GetSingleton().Get(StringHolder::CMT::kNPC1HM) :
		           nullptr;
	}

	static const BSFixedString& get_mt_name(
		const BSFixedString& a_default,
		const Actor*         a_actor) noexcept
	{
		const auto* const player = *g_thePlayer;

		if ((a_actor == player || s_enableNPC) &&
		    is_valid_movement_type_name(a_default))
		{
			auto& actorState1 = rel_member(a_actor->actorState1);

			if (actorState1.sneaking)
			{
				auto& sh = StringHolder::GetSingleton();

				if (sh.IsSneakIgnoredMT(a_default))
				{
					return a_default;
				}

				return sh.NPCSneaking;
			}

			if (actorState1.sprinting &&
			    (a_actor != player || rel_member(player->flagBDD).test(PlayerCharacter::FlagBDD::kWantsSprint)))
			{
				return StringHolder::GetSingleton().NPCSprinting;
			}

			if (s_enableExtra)
			{
				auto& actorState2 = rel_member(a_actor->actorState2);

				if (actorState2.weaponState >= ActorState::WEAPON_STATE::kDrawing &&
				    actorState2.weaponState <= ActorState::WEAPON_STATE::kSheathing &&
				    (rel_member(a_actor->stateFlags08) & 0xF) != 0 &&
				    StringHolder::GetSingleton().IsCommonMT(a_default))
				{
#if 0

					/*
						This method would be preferable but I'm not sure if it's safe to call 
						GetGraphVariableImpl here (or anywhere) on 1.5.97. On AE, an additional 
						lock was added to the variable cache that gets acquired on reads.
					*/

					std::int32_t r;
					std::int32_t l;

					static const BSFixedString iRightHandEquipped("iRightHandEquipped");
					static const BSFixedString iLeftHandEquipped("iLeftHandEquipped");

					if (a_actor->GetGraphVariableImpl2(iRightHandEquipped, r) &&
					    a_actor->GetGraphVariableImpl2(iLeftHandEquipped, l))
					{
						switch (static_cast<ANIM_EQUIPPED_WEAPON_TYPE>(r))
						{
						case ANIM_EQUIPPED_WEAPON_TYPE::kBow:
						case ANIM_EQUIPPED_WEAPON_TYPE::kCrossbow:
							{
								static const BSFixedString result("NPCBow");
								return result;
							}
						case ANIM_EQUIPPED_WEAPON_TYPE::kTwoHandAxe:
						case ANIM_EQUIPPED_WEAPON_TYPE::kTwoHandSword:
							{
								static const BSFixedString result("NPC2HM");
								return result;
							}
						}

						if (is_1hm(static_cast<ANIM_EQUIPPED_WEAPON_TYPE>(r)) ||
						    is_1hm(static_cast<ANIM_EQUIPPED_WEAPON_TYPE>(l)))
						{
							static const BSFixedString result("NPC1HM");
							return result;
						}
					}

#else

					if (const auto result = get_eq_mt_name(a_actor, actorState1))
					{
						return *result;
					}

#endif
				}
			}
		}

		return a_default;
	}

	static BGSMovementType* Character_GetCurrentMovementTypeData_LookupMT_hook(
		const BSFixedString& a_movementTypeName,
		Actor*               a_actor)
	{
		return LookupMTByName(get_mt_name(a_movementTypeName, a_actor));
	}

	static bool Patch()
	{
		const IAL::Address<std::uintptr_t> addr(36919, 37944, 0x8E, 0x133);

		if (!hook::check_dst5<0xE8>(addr.get()))
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
				dq(std::uintptr_t(Character_GetCurrentMovementTypeData_LookupMT_hook));
			};
		};

		Assembly code;

		ISKSE::GetBranchTrampoline().Write5Call(addr.get(), code.get());

		return true;
	}

	static void LoadSettings()
	{
		const INIConfReader reader(PLUGIN_INI_FILE_NOEXT);

		s_enableNPC   = reader.GetBoolValue("", "ApplyToNPC", false);
		s_enableExtra = reader.GetBoolValue("", "WieldingMovementTypeFixes", true);
	}

	bool Initialize([[maybe_unused]] const SKSEInterface* a_skse)
	{
		LoadSettings();

		gLog.Message("NPC support %s", s_enableNPC ? "ENABLED" : "disabled");
		gLog.Message("Wielding MT fixes %s", s_enableExtra ? "ENABLED" : "disabled");

		const bool result = Patch();

		if (!result)
		{
			gLog.Error("Patch failed");
		}

		return result;
	}
}