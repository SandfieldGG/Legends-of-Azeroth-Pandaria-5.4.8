/*
* This file is part of the Pandaria 5.4.8 Project. See THANKS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TRINITY_CREATURE_TEXT_MGR_H
#define TRINITY_CREATURE_TEXT_MGR_H

#include "Creature.h"
#include "GridNotifiers.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "Opcodes.h"
#include "Group.h"

class Creature;
class Player;
class Unit;
class WorldObject;
class WorldPacket;

enum CreatureTextRange
{
    TEXT_RANGE_NORMAL   = 0,
    TEXT_RANGE_AREA     = 1,
    TEXT_RANGE_ZONE     = 2,
    TEXT_RANGE_MAP      = 3,
    TEXT_RANGE_WORLD    = 4,
    TEXT_RANGE_PERSONAL = 5
};

enum class CreatureTextSoundType : uint32
{
    DirectSound = 0,
    ObjectSound = 1,
    Music       = 2
};

struct CreatureTextEntry
{
    uint32 creatureId;
    uint8 groupId;
    uint8 id;
    std::string text;
    ChatMsg type;
    Language lang;
    float probability;
    Emote emote;
    uint32 duration;
    uint32 sound;
    CreatureTextSoundType soundType;
    uint32 BroadcastTextId;
    CreatureTextRange TextRange;
};

struct CreatureTextLocale
{
    std::vector<std::string> Text;
};

struct CreatureTextId
{
    CreatureTextId(uint32 e, uint32 g, uint32 i) :  entry(e), textGroup(g), textId(i) { }

    bool operator<(CreatureTextId const& right) const
    {
        return std::tie(entry, textGroup, textId) < std::tie(right.entry, right.textGroup, right.textId);
    }

    uint32 entry;
    uint32 textGroup;
    uint32 textId;
};

typedef std::vector<CreatureTextEntry> CreatureTextGroup;                   // texts in a group
typedef std::unordered_map<uint8, CreatureTextGroup> CreatureTextHolder;    // groups for a creature by text_group
typedef std::unordered_map<uint32, CreatureTextHolder> CreatureTextMap;     // all creatures by entry

typedef std::map<CreatureTextId, CreatureTextLocale> LocaleCreatureTextMap;

typedef std::unordered_map<uint64, CreatureTextRepeatGroup> CreatureTextRepeatMap; // guid based

class CreatureTextMgr
{
    private:
        CreatureTextMgr() { }
        ~CreatureTextMgr() { }    

    public:
        static CreatureTextMgr* instance();
        void LoadCreatureTexts();
        void LoadCreatureTextLocales();
        CreatureTextMap  const& GetTextMap() const { return mTextMap; }

        void SendSound(Creature* source, uint32 sound, ChatMsg msgType, WorldObject const* whisperTarget, CreatureTextRange range, Team team, bool gmOnly);
        void SendEmote(Unit* source, uint32 emote);

        //if sent, returns the 'duration' of the text else 0 if error
        uint32 SendChat(Creature* source, uint8 textGroup, WorldObject const* whisperTarget = nullptr, ChatMsg msgType = CHAT_MSG_ADDON, Language language = LANG_ADDON, CreatureTextRange range = TEXT_RANGE_NORMAL, uint32 sound = 0, Team team = TEAM_OTHER, bool gmOnly = false, Player* srcPlr = nullptr);
        bool TextExist(uint32 sourceEntry, uint8 textGroup);
        std::string GetLocalizedChatString(uint32 entry, uint8 gender, uint8 textGroup, uint32 id, LocaleConstant locale) const;

        template<class Builder>
        void SendChatPacket(WorldObject* source, Builder const& builder, ChatMsg msgType, WorldObject const* whisperTarget = nullptr, CreatureTextRange range = TEXT_RANGE_NORMAL, Team team = TEAM_OTHER, bool gmOnly = false) const;
    private:
        void SendNonChatPacket(WorldObject* source, WorldPacket* data, ChatMsg msgType, WorldObject const* whisperTarget, CreatureTextRange range, Team team, bool gmOnly) const;
        float GetRangeForChatType(ChatMsg msgType) const;

        CreatureTextMap mTextMap;
        LocaleCreatureTextMap mLocaleTextMap;
};

#define sCreatureTextMgr CreatureTextMgr::instance()

template<class Builder>
class CreatureTextLocalizer
{
    public:
        CreatureTextLocalizer(Builder const& builder, ChatMsg msgType) : _builder(builder), _msgType(msgType)
        {
            _packetCache.resize(TOTAL_LOCALES);
        }

        ~CreatureTextLocalizer()
        {
            for (size_t i = 0; i < _packetCache.size(); ++i)
            {
                if (_packetCache[i])
                    delete _packetCache[i]->first;
                delete _packetCache[i];
            }
        }

        void operator()(Player* player)
        {
            // LocaleConstant loc_idx = player->GetSession()->GetSessionDbLocaleIndex();
            // WorldPacket* messageTemplate;

            // uint64 guid = 0LL;
            // switch (_msgType)
            // {
            //     case CHAT_MSG_MONSTER_WHISPER:
            //     case CHAT_MSG_RAID_BOSS_WHISPER:
            //         guid = player->GetGUID();
            //         break;
            //     default:
            //         break;
            // }

            // // create if not cached yet
            // if (!_packetCache[loc_idx])
            // {
            //     messageTemplate = new WorldPacket();
            //     _builder(messageTemplate, loc_idx, guid);
            //      ASSERT(messageTemplate->GetOpcode() != MSG_NULL_ACTION);
            //     _packetCache[loc_idx].reset(messageTemplate);
            // }
            // else
            //     messageTemplate = _packetCache[loc_idx].get();

            // WorldPacket data(*messageTemplate);



            LocaleConstant loc_idx = player->GetSession()->GetSessionDbLocaleIndex();
            WorldPacket* messageTemplate;
            size_t whisperGUIDpos;

            // create if not cached yet
            if (!_packetCache[loc_idx])
            {
                messageTemplate = new WorldPacket();
                whisperGUIDpos = _builder(messageTemplate, loc_idx);
                _packetCache[loc_idx] = new std::pair<WorldPacket*, size_t>(messageTemplate, whisperGUIDpos);
            }
            else
            {
                messageTemplate = _packetCache[loc_idx]->first;
                whisperGUIDpos = _packetCache[loc_idx]->second;
            }

            WorldPacket data(*messageTemplate);
            switch (_msgType)
            {
                case CHAT_MSG_MONSTER_WHISPER:
                case CHAT_MSG_RAID_BOSS_WHISPER:
                    // data.put<uint64>(whisperGUIDpos, player->GetGUID()); //.GetRawValue()
                    // data.put<uint64>(whisperGUIDpos + (8 + 3 +8 + 2 + 12 + 1+ 1 + 1 + 8 + 8 + 1 + 8), player->GetGUID());
                    break;
                default:
                    break;
            }

            player->SendDirectMessage(&data);
        }

    private:
        //std::vector<std::unique_ptr<WorldPacket>> _packetCache;
        mutable std::vector<std::pair<WorldPacket*, size_t>*> _packetCache;
        Builder const& _builder;
        ChatMsg _msgType;
};

template<class Builder>
void CreatureTextMgr::SendChatPacket(WorldObject* source, Builder const& builder, ChatMsg msgType, WorldObject const* whisperTarget /*= nullptr*/, CreatureTextRange range, Team team, bool gmOnly) const
{
    if (!source)
        return;

    CreatureTextLocalizer<Builder> localizer(builder, msgType);

    switch (msgType)
    {
        case CHAT_MSG_MONSTER_PARTY:
        {
            if (!whisperTarget)
                return;

            if (Player* whisperPlayer = const_cast<Player*>(whisperTarget->ToPlayer()))
            {
                if (Group* group = whisperPlayer->GetGroup())
                    group->BroadcastWorker(localizer);
            }
            return;
        }
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_RAID_BOSS_WHISPER:
        {
            if (range == TEXT_RANGE_NORMAL) //ignores team and gmOnly
            {
                if (!whisperTarget || whisperTarget->GetTypeId() != TYPEID_PLAYER)
                    return;

                localizer(const_cast<Player*>(whisperTarget->ToPlayer()));
                return;
            }
            break;
        }
        default:
            break;
    }

    switch (range)
    {
        case TEXT_RANGE_AREA:
        {
            uint32 areaId = source->GetAreaId();
            Map::PlayerList const& players = source->GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                if (itr->GetSource()->GetAreaId() == areaId && (!team || Team(itr->GetSource()->GetTeam()) == team) && (!gmOnly || itr->GetSource()->IsGameMaster()))
                    localizer(itr->GetSource());
            return;
        }
        case TEXT_RANGE_ZONE:
        {
            uint32 zoneId = source->GetZoneId();
            Map::PlayerList const& players = source->GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                if (itr->GetSource()->GetZoneId() == zoneId && (!team || Team(itr->GetSource()->GetTeam()) == team) && (!gmOnly || itr->GetSource()->IsGameMaster()))
                    localizer(itr->GetSource());
            return;
        }
        case TEXT_RANGE_MAP:
        {
            Map::PlayerList const& players = source->GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                if ((!team || Team(itr->GetSource()->GetTeam()) == team) && (!gmOnly || itr->GetSource()->IsGameMaster()))
                    localizer(itr->GetSource());
            return;
        }
        case TEXT_RANGE_WORLD:
        {
            SessionMap const& smap = sWorld->GetAllSessions();
            for (SessionMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
                if (Player* player = iter->second->GetPlayer())
                    if ((!team || Team(player->GetTeam()) == team) && (!gmOnly || player->IsGameMaster()))
                        localizer(player);
            return;
        }
        case TEXT_RANGE_NORMAL:
        default:
            break;
    }

    float dist = GetRangeForChatType(msgType);
    Trinity::PlayerDistWorker<CreatureTextLocalizer<Builder> > worker(source, dist, localizer);
    source->VisitNearbyWorldObject(dist, worker);
}

#endif
