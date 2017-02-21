/* Compile this file with a C++11 compiler. */

#include <stdio.h>
#include <fstream>
#include <vector>
#include <stdint.h>
#include <regex>
#include <typeinfo>
#include <map>
#include <math.h>

/* Protobuff wire types */
enum class WireType
{
    VARINT,
    BIT64,
    LENGTH_PREFIXED,
    START_GROUP,
    END_GROUP,
    BIT32,
    UNKNOWN_TYPE = 255
};

/* Protobuff message header*/
struct MessageHeader
{
    WireType type;
    int tag;
};

/* Reads protobuff message header. */
MessageHeader readMesageHeader(FILE *f)
{
    MessageHeader mh;
    uint8_t byte;

    byte = fgetc(f);
    mh.type = (WireType)(byte & 7);
    mh.tag = byte >> 3;

    return mh;
}

class FileNotFoundException : public std::exception
{
public:
    virtual const char *what() const throw() {return "File not found."; }
};

/* File class that auto closes standard C files. */
class AutoFile
{
    FILE *f;
public:
    AutoFile(FILE *f)
    {
        this->f = f;
        if (!f)
        {
            throw FileNotFoundException();
        }
    }
    ~AutoFile() {if (f) fclose(f); }
    operator FILE*() {return f;}
};

/* Generic buffer struct.  */
struct Buffer
{
    uint8_t *buf;
    size_t n;
};

/* Represents a protobuff message */
struct Message
{
    WireType type;
    int tag;
    union
    {
        uint64_t varInt;
        Buffer subMessage;
        uint8_t fixed[8];
    } data;

    Message() {type = WireType::UNKNOWN_TYPE;}
};

class BufferOverflowException : public std::exception
{
public:
    virtual const char *what() const throw() {return "Buffer overflow happened."; }
};

class InvalidMessageException : public std::exception
{
public:
    virtual const char *what() const throw() {return "Invalid message."; }
};

class InvalidArgumentException : public std::exception
{
    const char *reason;
public:
    InvalidArgumentException() {reason = "Invalid argument.";}
    InvalidArgumentException(const char *r) {reason = r;}
    virtual const char *what() const throw() {return reason; }
};

class UnsupportedTypeException : public std::exception
{
public:
    virtual const char *what() const throw() {return "Unsupported wire type"; }
};

/* */
int64_t zigzagDecode(uint64_t n)
{
    if (n & 1)
    {
        // Negative
        return -(n >> 1) - 1;
    }
    else
    {
        // Positive
        return n >> 1;
    }
}

class ProtoBuf
{
    Buffer buf;
    size_t ptr;

    uint8_t getByte()
    {
        if (ptr >= buf.n) throw BufferOverflowException();

        return buf.buf[ptr++];
    }

public:

    void readBytes(uint8_t *bytes, size_t n)
    {
        for (size_t i = 0; i < n; i++)
        {
            bytes[i] = getByte();
        }
    }

    uint64_t readVarInt()
    {
        uint64_t result = 0;

        for (int i = 0; i < 10; i++)
        {
            uint8_t byte = getByte();

            result |= (uint64_t)(byte & 0x7F) << (7*i);

            if (!(byte & 0x80)) break;
        }

        return result;
    }

    ProtoBuf(uint8_t *buf, size_t n)
    {
        this->buf.buf = buf;
        this->buf.n = n;
        ptr = 0;
    }

    ProtoBuf(const Message &msg)
    {
        if (msg.type != WireType::LENGTH_PREFIXED) throw InvalidArgumentException("Not a length prefixed message.");

        this->buf.buf = msg.data.subMessage.buf;
        this->buf.n = msg.data.subMessage.n;
        ptr = 0;
    }

    size_t getBytesLeft() {return buf.n - ptr; }

    size_t getBufPos() {return ptr;}

    Message getMessage()
    {
        uint64_t messageTag = readVarInt();
        Message msg;

        msg.type = (WireType)(messageTag & 7);
        msg.tag = messageTag >> 3;

        switch (msg.type)
        {
            case WireType::VARINT: msg.data.varInt = readVarInt(); break;
            case WireType::BIT32:
                msg.data.fixed[0] = getByte();
                msg.data.fixed[1] = getByte();
                msg.data.fixed[2] = getByte();
                msg.data.fixed[3] = getByte();
                break;
            case WireType::BIT64:
                msg.data.fixed[0] = getByte();
                msg.data.fixed[1] = getByte();
                msg.data.fixed[2] = getByte();
                msg.data.fixed[3] = getByte();
                msg.data.fixed[4] = getByte();
                msg.data.fixed[5] = getByte();
                msg.data.fixed[6] = getByte();
                msg.data.fixed[7] = getByte();
                break;
            case WireType::LENGTH_PREFIXED:
                {
                    uint64_t length = readVarInt();

                    if (length > getBytesLeft())
                    {
                        throw InvalidMessageException();
                    }
                    msg.data.subMessage.buf = buf.buf + ptr;
                    msg.data.subMessage.n = length;
                    ptr += length;
                }
                break;
            default:
                throw UnsupportedTypeException();
        }

        return msg;
    }

    uint8_t *getptr() {return buf.buf + ptr;}


};

void dumpMessage(const Message &msg)
{
    printf("Message tag: %d\n", msg.tag);
    printf("Message type: %d\n", (int)msg.type);

    switch (msg.type)
    {
        case WireType::LENGTH_PREFIXED:
            printf("%zd bytes long submessage.\n", msg.data.subMessage.n);
            break;
        case WireType::VARINT:
            printf("Varint: %llu\n", (unsigned long long)msg.data.varInt);
            break;
        case WireType::BIT32:
            printf("32 bit data: ");
            for (int i = 0; i < 4; i++) printf("%02x ", msg.data.fixed[i]);
            printf("\n");
            break;
        case WireType::BIT64:
            printf("32 bit data: ");
            for (int i = 0; i < 8; i++) printf("%02x ", msg.data.fixed[i]);
            printf("\n");
            break;
        default:;
    }
}

struct PokemonInfo
{
    int id;
    std::string name;
    int baseAtk;
    int baseDef;
    int baseStamina;
    std::vector<int> fastMoves;
    std::vector<int> chargedMoves;
    int types[2];
    //------ Computed info
    double maxCP;
    double tankiness;
    double trueStrength;
};

struct MoveInfo
{
    int id;
    std::string name;
    float power;
    double duration; // In seconds
    int energy;
    int moveType;
    // --------
    double eps; // Energy per second
    double dps; // Damage per second
    double dpe; // Damage per energy
};

struct MovesetDPS
{
    int pokemonId;
    int fastId;
    int chargedId;
    double DPS;
    double msDPS;
    double truePower;
};

std::map<int, PokemonInfo> pokemonList;
std::map<int, MoveInfo> moveList;
std::map<int, std::string> typeNames;
std::map<int, std::map<int, float>> typeChart;

std::map<std::string, bool> filtered;

int main(int argc, char **argv)
{
    // Check args.
    if (argc < 2)
    {
        printf("Usage: %s game_master_filename\n", argv[0]);
        return 1;
    }

    // Filter legendaries.
    filtered["ENTEI"] = true;
    filtered["LUGIA"] = true;
    filtered["SUICINE"] = true;
    filtered["ARTICUNO"] = true;
    filtered["MOLTRES"] = true;
    filtered["ZAPDOS"] = true;
    filtered["LUGIA"] = true;
    filtered["HO_OH"] = true;
    filtered["MEW"] = true;
    filtered["MEWTWO"] = true;
    filtered["RAIKOU"] = true;
    filtered["CELEBI"] = true;

    // Load file to a vector
    AutoFile f = fopen(argv[1], "rb");
    std::vector<uint8_t> message;

    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    message.resize(fileSize);
    fread(&message[0], fileSize, 1, f);

    // Parse protobuf and read pokémon data.
    ProtoBuf pb(&message[0], message.size());

    std::regex pokemonPattern("^V(\\d+)_POKEMON_(.*)$");
    std::regex movePattern("^V(\\d+)_MOVE_(.*)$");
    std::regex typePattern("^POKEMON_TYPE_(.*)$");

    while (pb.getBytesLeft())
    {
        Message msg = pb.getMessage();

        if ((msg.type == WireType::LENGTH_PREFIXED) && (msg.tag == 2))
        {
            ProtoBuf subProto(msg);
            Message name;
            Message details;

            while (subProto.getBytesLeft())
            {
                Message msg2 = subProto.getMessage();

                switch (msg2.tag)
                {
                    case 1: name = msg2; break;
                    case 2:
                    case 4:
                    case 8:
                        details = msg2; break;
                }
            }

            if ((name.type != WireType::LENGTH_PREFIXED) || (details.type != WireType::LENGTH_PREFIXED)) continue;

            printf("%.*s\n", (int)name.data.subMessage.n, name.data.subMessage.buf);

            std::string template_str((const char *)name.data.subMessage.buf, name.data.subMessage.n);
            std::smatch match;
            if (std::regex_search(template_str, match, pokemonPattern))
            {
                // Pokemon found.
                if (filtered.find(match[2].str()) != filtered.end()) continue;

                int id = strtol(match[1].str().c_str(), NULL, 10);
                PokemonInfo pi;

                pi.name = match[2].str();
                printf("Pokémon found: id: #%d, name: %s\n", id, pi.name.c_str());

                ProtoBuf pokemonInfoBuf(details);

                while (pokemonInfoBuf.getBytesLeft())
                {
                    Message msg3 = pokemonInfoBuf.getMessage();

                    switch (msg3.tag)
                    {
                        case 4: // Primary type.
                            pi.types[0] = msg3.data.varInt;
                            break;
                        case 5: // Secondary type.
                            pi.types[1] = msg3.data.varInt;
                            break;
                        case 8: // Base stats proto.
                        {
                            ProtoBuf baseStatsBuf(msg3);

                            while (baseStatsBuf.getBytesLeft())
                            {
                                Message msg4 = baseStatsBuf.getMessage();
                                if (msg4.type == WireType::VARINT)
                                {
                                    switch (msg4.tag)
                                    {
                                        case 1: pi.baseStamina = msg4.data.varInt; break;
                                        case 2: pi.baseAtk = msg4.data.varInt; break;
                                        case 3: pi.baseDef = msg4.data.varInt; break;
                                    }
                                }
                            }
                            break;
                        }
                        case 9: // Quick moves.
                        {
                            ProtoBuf fastMoves(msg3);

                            // Series of repeated varints.
                            while (fastMoves.getBytesLeft())
                            {
                                pi.fastMoves.push_back(fastMoves.readVarInt());
                            }
                            break;
                        }
                        case 10: // Charged moves.
                        {
                            ProtoBuf chargedMoves(msg3);

                            // Series of repeated varints.
                            while (chargedMoves.getBytesLeft())
                            {
                                pi.chargedMoves.push_back(chargedMoves.readVarInt());
                            }
                            break;
                        }
                    }
                }

                pi.id = id;
                pi.maxCP = ((pi.baseAtk + 15) * sqrt(pi.baseDef  + 15) * sqrt(pi.baseStamina + 15) * 0.79030001 * 0.79030001)/10.0;
                pi.tankiness = (pi.baseDef + 15) * (pi.baseStamina + 15);
                pi.trueStrength = (pi.baseAtk + 15) * pi.tankiness / 10000.0;

                printf("Name: %s\n", pi.name.c_str());
                printf("STA: %d\n", pi.baseStamina);
                printf("ATK: %d\n", pi.baseAtk);
                printf("DEF: %d\n", pi.baseDef);
                printf("Fast moves: ");
                for (unsigned i = 0; i < pi.fastMoves.size(); i++) printf("%d ", pi.fastMoves[i]);
                printf("\n");
                printf("Charged moves: ");
                for (unsigned i = 0; i < pi.chargedMoves.size(); i++) printf("%d ", pi.chargedMoves[i]);
                printf("\n");
                pokemonList[id] = pi;
            }

            if (std::regex_search(template_str, match, movePattern))
            {
                // Move description found!
                int id = strtol(match[1].str().c_str(), NULL, 10);

                ProtoBuf moveDetails(details);
                MoveInfo mi;

                mi.name = match[2].str();

                printf("Move found: id: #%d, name: %s\n", id, mi.name.c_str());

                while (moveDetails.getBytesLeft())
                {
                    Message msg3 = moveDetails.getMessage();

                    switch (msg3.tag)
                    {
                        case 3: // Type of the move.
                            mi.moveType = msg3.data.varInt;
                            break;
                        case 4: // The power of the move.
                            memcpy(&mi.power, msg3.data.fixed, 4);
                            break;
                        case 12:  // Duration
                            mi.duration = msg3.data.varInt / 1000.0;
                            break;
                        case 15: // Energy
                            mi.energy = (int64_t)msg3.data.varInt;
                            break;

                    }
                }

                mi.id = id;
                mi.eps = mi.energy / mi.duration;
                mi.dps = mi.power / mi.duration;
                mi.dpe = mi.power / mi.energy;

                printf("name: %s\n", mi.name.c_str());
                printf("power: %g\n", mi.power);
                printf("duration: %g\n", mi.duration);
                printf("erergy: %d\n", mi.energy);
                printf("moveType: %d\n", mi.moveType);
                moveList[id] = mi;
            }

            if (std::regex_search(template_str, match, typePattern))
            {
                // Type found

                ProtoBuf typeDetails(details);
                int id = -1;
                std::map<int, float> typeEffeciveness;

                while (typeDetails.getBytesLeft())
                {
                    Message msg3 = typeDetails.getMessage();

                    switch (msg3.tag)
                    {
                        case 1: // Type chart
                            {
                                int index = 1;
                                ProtoBuf damageTable(msg3);

                                while (damageTable.getBytesLeft())
                                {
                                    float effectiveness;
                                    uint8_t bytes[4];

                                    damageTable.readBytes(bytes, 4);
                                    memcpy(&effectiveness, bytes, 4);
                                    printf("Effectiveness %d, %g\n", index, effectiveness);
                                    typeEffeciveness[index] = effectiveness;

                                    index++;
                                }
                            }
                            break;
                        case 2: // Type id
                            id = msg3.data.varInt;
                            printf("It's id: %d\n", id);
                            break;
                    }
                }

                typeNames[id] = std::string((const char*)name.data.subMessage.buf, name.data.subMessage.n);
                typeChart[id] = typeEffeciveness;
            }
        }
    }

    //pokemonList[149].fastMoves.push_back(204); // Inject dragon breath for dragonite.
    //pokemonList[149].chargedMoves.push_back(83); // Inject dragon claw for dragonite.

    // Type chart
    for (const auto &i : typeChart)
    {
        for (const auto &j : i.second)
        {
            if (j.second == 1) continue;
            printf("(%d)%s -> (%d)%s: %g\n", i.first, typeNames[i.first].c_str(), j.first, typeNames[j.first].c_str(), j.second);
        }
    }

    // Pokemon by CP
    {
        std::vector<PokemonInfo> pis;

        for (const auto &pi : pokemonList)
        {
            pis.push_back(pi.second);
        }

        std::sort(pis.begin(), pis.end(), [](PokemonInfo a, PokemonInfo b) {return a.maxCP > b.maxCP; } ) ;

        {
            AutoFile cpFile = fopen("cplist.txt", "w");

            for (const auto &pi : pis)
            {
                fprintf(cpFile, "%s: %g\n", pi.name.c_str(), pi.maxCP);
            }
        }

        std::sort(pis.begin(), pis.end(), [](PokemonInfo a, PokemonInfo b) {return a.tankiness > b.tankiness; });

        {
            AutoFile tankinessFile = fopen("tankiness.txt", "w");

            for (const auto &pi : pis)
            {
                fprintf(tankinessFile, "%s:  %g\n", pi.name.c_str(), pi.tankiness);
            }
        }

        std::sort(pis.begin(), pis.end(), [](PokemonInfo a, PokemonInfo b) {return a.trueStrength > b.trueStrength; });

        {
            AutoFile trueStrengthFile = fopen("truestrength.txt", "w");

            for (const auto &pi : pis)
            {
                fprintf(trueStrengthFile, "%s:  %g\n", pi.name.c_str(), pi.trueStrength);
            }
        }
    }

    // Moves list

    AutoFile moves = fopen("moves.txt", "w");

    fprintf(moves, "%-5s%-30s %-30s %-10s %-10s %-10s %-10s %-10s %-10s\n",
        "Id",
        "Name",
        "Type",
        "Power",
        "Energy",
        "Duration",
        "EPS",
        "DPS",
        "DPE"
    );

    std::vector<MoveInfo> moveByName;
    for (const auto &mip : moveList)
    {
        moveByName.push_back(mip.second);
    }
    std::sort(moveByName.begin(), moveByName.end(), [](MoveInfo a, MoveInfo b){return a.name < b.name;});

    for (const auto &mi : moveByName)
    {
        fprintf(moves, "%-5d%-30s %-30s %-10g %-10d %-10g %-10g %-10g %-10g\n",
            mi.id,
            mi.name.c_str(),
            typeNames[mi.moveType].c_str(),
            mi.power,
            mi.energy,
            mi.duration,
            mi.eps,
            mi.dps,
            mi.dpe
        );
    }

    // Pokemon info and moves

    std::vector<MovesetDPS> overallMovesetStats;
    AutoFile pokemons = fopen("pokemonlist.txt", "w");

    std::map<int, std::vector<MovesetDPS>> movesetStatsByType;
    std::map<int, std::map<int, std::vector<MovesetDPS>>> bestCounters;

    for (const auto &kv : pokemonList)
    {
        const PokemonInfo &pi = kv.second;

        fprintf(pokemons, "#%d %s (Type: %s, %s) (Max CP: %g, ATK: %d, DEF: %d, STA: %d)\n",
            pi.id,
            pi.name.c_str(),
            typeNames[pi.types[0]].c_str(),
            typeNames[pi.types[1]].c_str(),
            pi.maxCP,
            pi.baseAtk,
            pi.baseDef,
            pi.baseStamina
        );
        fprintf(pokemons, "Fast moves: \n");

        std::vector<MovesetDPS> movesetStats;

        for (const auto &fmi : pi.fastMoves)
        {
            for (const auto &cmi : pi.chargedMoves)
            {
                MoveInfo &fastMove = moveList[fmi];
                MoveInfo &chargedMove = moveList[cmi];
                double energy = 0;
                double time = 0;
                double damage = 0;
                double primaryDamage = 0;
                double secondaryDamage = 0;

                while (time < 1000)
                {
                    MoveInfo *moveToUse;
                    double *damageToRaise;
                    double stab = 1;

                    if (energy >= -chargedMove.energy)
                    {
                        // Do charged move.
                        moveToUse = &chargedMove;
                        damageToRaise = &secondaryDamage;
                    }
                    else
                    {
                        // Do fast move
                        moveToUse = &fastMove;
                        damageToRaise = &primaryDamage;
                    }

                    if (
                        (moveToUse->moveType == pi.types[0]) ||
                        (moveToUse->moveType == pi.types[1])
                    )
                    {
                        stab = 1.25;
                    }

                    damage += moveToUse->power * stab;
                    *damageToRaise += moveToUse->power * stab;
                    time += moveToUse->duration;
                    energy += moveToUse->energy;
                }

                MovesetDPS mDPS;

                double rawDps = damage / time;

                mDPS.pokemonId = kv.first;
                mDPS.fastId = fmi;
                mDPS.chargedId = cmi;
                mDPS.DPS = rawDps * pi.baseAtk;
                mDPS.msDPS = rawDps;
                mDPS.truePower = rawDps * pi.trueStrength;

                movesetStats.push_back(mDPS);
                overallMovesetStats.push_back(mDPS);

                int fastMoveType = fastMove.moveType;

                if (chargedMove.moveType == fastMoveType)
                {
                    // Same type of damage
                    movesetStatsByType[fastMoveType].push_back(mDPS);
                }
                else
                {
                    // TODO: Copypaste
                    // Fast and charged are different.
                    MovesetDPS primaryDPS = mDPS;
                    rawDps = primaryDamage / time;
                    primaryDPS.DPS = rawDps * pi.baseAtk;
                    primaryDPS.truePower = rawDps * pi.trueStrength;
                    primaryDPS.msDPS = rawDps;
                    movesetStatsByType[fastMoveType].push_back(primaryDPS);

                    MovesetDPS secondaryDPS = mDPS;
                    rawDps = secondaryDamage / time;
                    secondaryDPS.DPS = rawDps * pi.baseAtk;
                    secondaryDPS.truePower = rawDps * pi.trueStrength;
                    secondaryDPS.msDPS = rawDps;
                    movesetStatsByType[chargedMove.moveType].push_back(secondaryDPS);
                }

                for (const auto &tnp1 : typeChart)
                {
                    for (const auto &tnp2: typeChart)
                    {
                        double theDamage;

                        if (tnp1.first == tnp2.first)
                        {
                            theDamage = primaryDamage * typeChart[fastMove.moveType][tnp1.first]
                            + secondaryDamage * typeChart[chargedMove.moveType][tnp1.first];
                        }
                        else
                        {
                            theDamage =
                                primaryDamage * typeChart[fastMove.moveType][tnp1.first] * typeChart[fastMove.moveType][tnp2.first]
                                + secondaryDamage * typeChart[chargedMove.moveType][tnp1.first] * typeChart[chargedMove.moveType][tnp2.first];
                        }
                        MovesetDPS dps = mDPS;
                        rawDps = theDamage / time;
                        dps.DPS = rawDps * pi.baseAtk;
                        dps.truePower = rawDps * pi.trueStrength;
                        dps.msDPS = rawDps;
                        movesetStatsByType[fastMoveType].push_back(dps);

                        bestCounters[tnp1.first][tnp2.first].push_back(dps);
                    }
                }
            }
        }

        std::sort(movesetStats.begin(), movesetStats.end(), [](MovesetDPS a, MovesetDPS b){return a.DPS > b.DPS; });
        for (const auto &mdps : movesetStats)
        {
            fprintf(pokemons, "%s + %s : %g (%g)\n", moveList[mdps.fastId].name.c_str(), moveList[mdps.chargedId].name.c_str(), mdps.DPS, mdps.msDPS);
        }
        fprintf(pokemons, "\n");
    }

    AutoFile dpsList = fopen("dpslist.txt", "w");
    std::sort(overallMovesetStats.begin(), overallMovesetStats.end(), [](MovesetDPS a, MovesetDPS b){return a.DPS > b.DPS; });
    for (const auto &mdps : overallMovesetStats)
    {
        fprintf(dpsList, "%s: %s + %s : %g (%g)\n",
            pokemonList[mdps.pokemonId].name.c_str(),
            moveList[mdps.fastId].name.c_str(),
            moveList[mdps.chargedId].name.c_str(),
            mdps.DPS,
            mdps.msDPS
        );
    }

    AutoFile tpsList = fopen("truepowerlist.txt", "w");
    std::sort(overallMovesetStats.begin(), overallMovesetStats.end(), [](MovesetDPS a, MovesetDPS b){return a.truePower > b.truePower; });
    for (const auto &mdps : overallMovesetStats)
    {
        fprintf(tpsList, "%s: %s + %s : %g\n",
            pokemonList[mdps.pokemonId].name.c_str(),
            moveList[mdps.fastId].name.c_str(),
            moveList[mdps.chargedId].name.c_str(),
            mdps.truePower
        );
    }

    AutoFile bestAttackersByType = fopen("bestDPSbyType.txt", "w");
    for (auto &typeVecPair : movesetStatsByType)
    {
        auto &typeVec = typeVecPair.second;

        std::sort(typeVec.begin(), typeVec.end(), [](MovesetDPS a, MovesetDPS b){return a.DPS > b.DPS; });
    }

    for (const auto &typeVecPair : movesetStatsByType)
    {
        fprintf(bestAttackersByType, "Best attackers of %s type:\n\n", typeNames[typeVecPair.first].c_str());
        for (const auto &mdps : typeVecPair.second)
        {
            fprintf(bestAttackersByType, "%s: %s + %s : %g\n",
                pokemonList[mdps.pokemonId].name.c_str(),
                moveList[mdps.fastId].name.c_str(),
                moveList[mdps.chargedId].name.c_str(),
                mdps.DPS /*/ typeVecPair.second[0].DPS * 100*/
            );
        }
        fprintf(bestAttackersByType, "\n\n");
    }


    AutoFile bestTruePowerByType = fopen("bestTruePowerByType.txt", "w");
    for (auto &typeVecPair : movesetStatsByType)
    {
        auto &typeVec = typeVecPair.second;

        std::sort(typeVec.begin(), typeVec.end(), [](MovesetDPS a, MovesetDPS b){return a.truePower > b.truePower; });
    }

    for (const auto &typeVecPair : movesetStatsByType)
    {
        fprintf(bestTruePowerByType, "Best attackers of %s type:\n\n", typeNames[typeVecPair.first].c_str());
        for (const auto &mdps : typeVecPair.second)
        {
            fprintf(bestTruePowerByType, "%s: %s + %s : %g\n",
                pokemonList[mdps.pokemonId].name.c_str(),
                moveList[mdps.fastId].name.c_str(),
                moveList[mdps.chargedId].name.c_str(),
                mdps.truePower /*/ typeVecPair.second[0].truePower * 100*/
            );
        }
        fprintf(bestTruePowerByType, "\n\n");
    }

    AutoFile bestDPSCountersFile = fopen("bestDPSCounters.txt", "w");
    for (auto &t1 : bestCounters)
    {
        for (auto &t2 : t1.second)
        {
            auto &vec = t2.second;

            std::sort(vec.begin(), vec.end(), [](MovesetDPS a, MovesetDPS b){return a.DPS > b.DPS; });
        }
    }

    for (const auto &t1 : bestCounters)
    {
        for (const auto &t2 : t1.second)
        {
            const auto &vec = t2.second;

            fprintf(bestDPSCountersFile, "Best counters of %s-%s\n", typeNames[t1.first].c_str(), typeNames[t2.first].c_str());
            for (const auto &mdps : vec)
            {
                fprintf(bestDPSCountersFile, "%s: %s + %s : %g\n",
                    pokemonList[mdps.pokemonId].name.c_str(),
                    moveList[mdps.fastId].name.c_str(),
                    moveList[mdps.chargedId].name.c_str(),
                    mdps.DPS /*/ typeVecPair.second[0].truePower * 100*/
                );
            }
            fprintf(bestDPSCountersFile, "\n\n");
        }
    }

    AutoFile bestTPCountersFile = fopen("bestTruePowerCounters.txt", "w");
    for (auto &t1 : bestCounters)
    {
        for (auto &t2 : t1.second)
        {
            auto &vec = t2.second;

            std::sort(vec.begin(), vec.end(), [](MovesetDPS a, MovesetDPS b){return a.truePower > b.truePower; });
        }
    }

    for (const auto &t1 : bestCounters)
    {
        for (const auto &t2 : t1.second)
        {
            const auto &vec = t2.second;

            fprintf(bestTPCountersFile, "Best counters of %s-%s\n", typeNames[t1.first].c_str(), typeNames[t2.first].c_str());
            for (const auto &mdps : vec)
            {
                fprintf(bestTPCountersFile, "%s: %s + %s : %g\n",
                    pokemonList[mdps.pokemonId].name.c_str(),
                    moveList[mdps.fastId].name.c_str(),
                    moveList[mdps.chargedId].name.c_str(),
                    mdps.truePower /*/ typeVecPair.second[0].truePower * 100*/
                );
            }
            fprintf(bestTPCountersFile, "\n\n");
        }
    }


    return 0;
}
