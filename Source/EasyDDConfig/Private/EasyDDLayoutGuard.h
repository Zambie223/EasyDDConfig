#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

#include <initializer_list>

/**
 * Startup check that the game and engine types this mod reads or writes directly still have the layout the mod was
 * compiled against.
 *
 * Inline getters and direct member access compile a field's position into this mod. A game patch can move fields
 * without renaming anything (1.2.5.1 moved UNetConnection's fields by 32 bytes and every factory building's by 16), and
 * the mod then reads or writes the wrong memory. The running game's reflection data always has the current layout, so
 * each feature compares what it was compiled with (sizeof, offsetof of reflected fields) against it before it starts,
 * and stays off with a warning if anything differs instead of crashing or corrupting memory.
 */
namespace EasyDD::Layout
{
	struct FField
	{
		const TCHAR* Name;
		int32 CompiledOffset;
	};

	/**
	 * True if Type's size in the running game equals CompiledSize and each listed reflected field sits at its compiled
	 * offset. Otherwise logs one warning naming the feature and the first difference, and returns false.
	 */
	bool Matches(const UStruct* Type, int32 CompiledSize, std::initializer_list<FField> Fields, const TCHAR* Feature);

	/** FChatMessageStruct, which the mod fills for its chat lines. Checked once per process; if false, chat lines are only logged. */
	bool ChatMessageMatches();

	/** UFGRemoteCallObject, the base of the mod's admin-sync RCO class. If false, admin menu sync is off. */
	bool RemoteCallObjectMatches();

	/**
	 * AActor (the base of the mod's subsystem, through SML's AModSubsystem) and UWorld (read inline throughout: time,
	 * timers, net mode). If false, the mod doesn't start at all.
	 */
	bool EngineBaseMatches();
}

/** Compiled offset of a reflected (UPROPERTY) member, for EasyDD::Layout::Matches. */
#define EASYDD_FIELD(Type, Member) EasyDD::Layout::FField{ TEXT(#Member), static_cast<int32>(STRUCT_OFFSET(Type, Member)) }
