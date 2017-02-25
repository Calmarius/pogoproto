/* Compile this file with a C++11 compiler. */

#include <stdio.h>
#include <math.h>
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

/* Represents a data buffer to be parsed as protobuff. */
class ProtoBuf
{
    Buffer buf;
    size_t ptr;

    /* Reads single byte from the buffer. */
    uint8_t getByte()
    {
        if (ptr >= buf.n) throw BufferOverflowException();

        return buf.buf[ptr++];
    }

public:

    /* Reads more bytes */
    void readBytes(uint8_t *bytes, size_t n)
    {
        for (size_t i = 0; i < n; i++)
        {
            bytes[i] = getByte();
        }
    }

    /* Reads varint.
        When the high bit is set, it indicates there are more bytes to be read.
        The low 7 bits encode the the payload in little endian order.
     */
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

    /* Construct from pointer and length. */
    ProtoBuf(uint8_t *buf, size_t n)
    {
        this->buf.buf = buf;
        this->buf.n = n;
        ptr = 0;
    }

    /* Construct from protobuff message. */
    ProtoBuf(const Message &msg)
    {
        if (msg.type != WireType::LENGTH_PREFIXED) throw InvalidArgumentException("Not a length prefixed message.");

        this->buf.buf = msg.data.subMessage.buf;
        this->buf.n = msg.data.subMessage.n;
        ptr = 0;
    }

    /* Number of bytes left  to read. */
    size_t getBytesLeft() {return buf.n - ptr; }

    /* Position in buffer.  */
    size_t getBufPos() {return ptr;}

    /* Reads a message from the buffer. */
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

    /* Gets the address the current byte (for debugging). */
    uint8_t *getptr() {return buf.buf + ptr;}


};

/* To dump message for debugging. */
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

/* Represents the info about the pokémon. */
struct PokemonInfo
{
    int id;
    std::string name;
    int baseAtk;
    int baseDef;
    int baseStamina;
    std::vector<int> fastMoves; // Ids of fast moves
    std::vector<int> chargedMoves; // Ids of charged moves
    size_t nAvailableFastMoves;
    size_t nAvailableChargedMoves;

    std::vector<int> pokemonTypes; // Ids of the two pokémon types.
    //------ Computed info
    double maxCP;
    double tankiness; // base attack times base defense (perfect IV assumed)
    double trueStrength; // product of all the 3 base stats.  (perfect IV assumed)
    double prestigerCPMultiplier;
};

/* Info about the moves. */
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

std::string removeFast(const std::string &name)
{
    std::string result = name;

    for (int i = 0; i < 5; i++) result.pop_back();

    return result;
}

std::string normalizeName(const std::string &name)
{
    std::string result;

    for (size_t i = 0; i < name.size(); i++)
    {
        if (i == 0) result.push_back(name[i]);
        else if (name[i] == '_') result.push_back(' ');
        else result.push_back(name[i] + 32);
    }

    return result;
}

std::map<int, PokemonInfo> pokemonList; // List of pokémon
std::map<int, MoveInfo> moveList; // List of moves
std::map<int, std::string> typeNames; // Names of types
std::map<int, std::map<int, float>> typeChart; // Type chart

std::map<std::string, bool> filtered; // Pokémon to ignore in the calculations.

std::map<std::string, int> pokemonNameToId; // Map pokémon names to Ids.
std::map<std::string, int> moveNameToId; // Map moves to Ids.

/* Pokémon + moveset tuple and their properties. */
struct MovesetDPS
{
    int pokemonId;
    int fastId;
    int chargedId;
    bool isLegacy;
    bool dodging;
    double DPS; // Moveset DPS * attack
    double msDPS; // Moveset DPS
    double truePower; // Moveset DPS * trueStrength
    double prestigePower; // Moveset DPS * prestigePotential
    int fastAttacksPerTurn;

    void populate(const double rawDPS, const PokemonInfo &pi)
    {
        msDPS = rawDPS;
        DPS = rawDPS * (pi.baseAtk + 15);
        truePower = rawDPS * pi.trueStrength * (dodging ? 1 : 0.25);
        prestigePower = rawDPS * pi.trueStrength * pow(pi.prestigerCPMultiplier, 3);
    }

    void printEntry(FILE *f, double value) const
    {
        fprintf(f, "- %s: %s + %s : %g  (msDPS: %g) %s %s (Fast attacks per turn: %d)\n",
            normalizeName(pokemonList[pokemonId].name).c_str(),
            normalizeName(removeFast(moveList[fastId].name)).c_str(),
            normalizeName(moveList[chargedId].name).c_str(),
            value,
            msDPS,
            isLegacy ? "(*)" : "",
            dodging ? "" : "(cannot dodge)",
            fastAttacksPerTurn
        );
    }
};

enum class PogoProtoTag
{
    ITEM_TEMPLATE = 2,
};

enum class ItemTemplateTag
{
    ITEM_NAME = 1,
    POKEMON_DETAILS = 2,
    MOVE_DETAILS = 4,
    POKEMON_TYPE_DETAILS = 8
};

enum class PokemonDetailsTag
{
    PRIMARY_TYPE = 4,
    SECONDARY_TYPE = 5,
    BASE_STATS = 8,
    QUICK_MOVES = 9,
    CHARGED_MOVES = 10
};

enum class BaseStatsTag
{
    STAMINA = 1,
    ATTACK = 2,
    DEFENSE = 3
};

enum class MoveDetailsTag
{
    TYPE = 3,
    POWER = 4,
    DURATION = 12,
    ENERGY = 15
};

enum class TypeDetailsTag
{
    TYPE_CHART = 1,
    ID = 2
};

void addLegacyMove(
    const char *pokemonName,
    const char *moveName
)
{
    if (pokemonNameToId.find(pokemonName) == pokemonNameToId.end())
    {
        printf("No such pokemon: %s\n", pokemonName);
        return;
    }
    if (moveNameToId.find(moveName) == moveNameToId.end())
    {
        printf("No such move: %s\n", moveName);
        return;
    }

    const MoveInfo &moveInfo = moveList[moveNameToId[moveName]];

    if (moveInfo.energy <= 0)
    {
        pokemonList[pokemonNameToId[pokemonName]].chargedMoves.push_back(moveNameToId[moveName]);
    }
    else
    {
        pokemonList[pokemonNameToId[pokemonName]].fastMoves.push_back(moveNameToId[moveName]);
    }
}

const double LEVEL30_CP_MULTIPLIER = 0.7317;
const double LEVEL40_CP_MULTIPLIER = 0.79030001;
const char *POKEMON_LIST_FILE = "pokemonlist.txt";

const double ATTACKER_CPM = LEVEL30_CP_MULTIPLIER; // Corresponding CP multiplier for level 30 pokémon.

struct
{
    const char *gameMasterFile;
    double roundLength; // How often the opponent strikes.
    double battleTime; // How long life assumed.
    double prestigerCP; // The desired CP of the prestiger.
    const char *filteredPokemon; // List of pokemon to filter out. eg. legendaries or other unobtainable stuff.
} conf = {0};

struct Option
{
    int nParameters;
    std::string helpText;
    int (*handler)(char **argv);
};

typedef int (*OptHandlerFunc)(const char *option);

std::map<std::string, Option> options;

void printHelp()
{
    printf("USAGE:\n\npogoproto filename [options]\n\n");
    printf("OPTIONS:\n\n");

    for (const auto &opt : options)
    {
        puts(opt.second.helpText.c_str());
    }
}

int main(int argc, char **argv)
{
    // Check endianness to warn the user the the program is not prepared to run on big endian.
    {
        uint8_t endiannessCheck[4] = {0, 1, 2, 3};
        uint32_t test;

        memcpy(&test, endiannessCheck, 4);

        if (test != 0x03020100)
        {
            printf("ERROR: Your machine is not little endian. This program is not perpared to run on machines with different endianness.\n");
            return 1;
        }
    }

    // Set up options.
    conf.gameMasterFile = NULL;
    conf.roundLength = 2.5;
    conf.battleTime = 100;
    conf.prestigerCP = 1500;
    conf.filteredPokemon = NULL;

    {
        Option *option;

        option = &options["-rl"];
        option->nParameters = 1;
        option->handler = [](char **argv)
        {
            conf.roundLength = strtod(argv[1], NULL);
            printf("Using round length: %g\n", conf.roundLength);
            return 0;
        };
        {
            std::stringstream tmp;
            tmp << "-rl roundLength\n\n";
            tmp << "\tSpecify how fast the opponent pokémon attacks in seconds. \n\n";
            tmp << "\tThe simulation assumes the players dodges the attacks. This determines how often the attacks come.\n";
            tmp << "\tDefault: " << conf.roundLength << "\n";
            option->helpText = tmp.str();
        }

        option = &options["-bt"];
        option->nParameters = 1;
        option->handler = [](char **argv)
        {
            conf.battleTime = strtol(argv[1], NULL, 10);
            printf("Using battle time: %g\n", conf.battleTime);
            return 0;
        };
        {
            std::stringstream tmp;
            tmp << "-bt battleTime\n\n";
            tmp << "\tSpecify how long lifetime do you expect for your pokémon during battle.\n\n";
            tmp << "\tThis is important when dealing with the energy received from the damage your pokémon take.\n";
            tmp << "\tDefault: " << conf.battleTime << "\n";
            option->helpText = tmp.str();
        }

        option = &options["-pcp"];
        option->nParameters = 1;
        option->handler = [](char **argv)
        {
            conf.prestigerCP = strtod(argv[1], NULL);
            printf("Preferred prestiger CP: %g\n", conf.prestigerCP);
            return 0;
        };
        {
            std::stringstream tmp;
            tmp << "-pcp prestigerCP\n\n";
            tmp << "\tThe preferred prestiger CP you want to use, when comparing prestigers.\n\n";
            tmp << "\tPokémon that cannot reach the specified CP will not be listed in the prestiger list.\n";
            tmp << "\tDefault: " <<  conf.prestigerCP << "\n";
            option->helpText = tmp.str();
        }

        option = &options["-filtered"];
        option->nParameters = 1;
        option->handler = [](char **argv)
        {
            conf.filteredPokemon = argv[1];
            printf("Filtering unwanted pokemon using file: %s\n", conf.filteredPokemon);
            return 0;
        };
        {
            std::stringstream tmp;
            tmp << "-filtered file\n\n";
            tmp << "\tList of pokemon to filter out.\n\n";
            tmp << "\tEach line in the file must contain a pokemon name as it appears in the decoded protobuff.\n";
            tmp << "\tSee " << POKEMON_LIST_FILE << " for the possible names.\n";
            option->helpText = tmp.str();
        }
    }

    // Check args.
    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    // Parse options.
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        auto opt = options.find(arg);

        if (opt == options.end())
        {
            if (conf.gameMasterFile)
            {
                fprintf(stderr, "Unknown option: %s\n", arg);
                return 1;
            }
            conf.gameMasterFile = arg;
            printf("Will read from game master file: %s\n", conf.gameMasterFile);
        }
        else
        {
            if (i + opt->second.nParameters >= argc)
            {
                fprintf(stderr, "Missing parameter for option %s\n", opt->first.c_str());
                return 1;
            }
            else
            {
                if (opt->second.handler(argv + i))
                {
                    fprintf(stderr, "Error in option %s\n", opt->first.c_str());
                    return 1;
                }
                i += opt->second.nParameters;
            }
        }
    }

    // Filter legendaries.
    if (conf.filteredPokemon)
    {
        AutoFile f = fopen(conf.filteredPokemon, "r");

        // Continue here.


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
        filtered["SUICUNE"] = true;
    }

    // Load file to a vector
    AutoFile f = fopen(conf.gameMasterFile, "rb");
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

        if ((msg.type == WireType::LENGTH_PREFIXED) && ((PogoProtoTag)msg.tag == PogoProtoTag::ITEM_TEMPLATE))
        {
            ProtoBuf subProto(msg);
            Message name;
            Message details;

            while (subProto.getBytesLeft())
            {
                Message msg2 = subProto.getMessage();

                switch ((ItemTemplateTag)msg2.tag)
                {
                    case ItemTemplateTag::ITEM_NAME: name = msg2; break;
                    case ItemTemplateTag::POKEMON_DETAILS:
                    case ItemTemplateTag::MOVE_DETAILS:
                    case ItemTemplateTag::POKEMON_TYPE_DETAILS:
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

                    switch ((PokemonDetailsTag)msg3.tag)
                    {
                        case PokemonDetailsTag::PRIMARY_TYPE:
                        case PokemonDetailsTag::SECONDARY_TYPE:
                            pi.pokemonTypes.push_back(msg3.data.varInt);
                            break;
                        case PokemonDetailsTag::BASE_STATS:
                        {
                            ProtoBuf baseStatsBuf(msg3);

                            while (baseStatsBuf.getBytesLeft())
                            {
                                Message msg4 = baseStatsBuf.getMessage();
                                if (msg4.type == WireType::VARINT)
                                {
                                    switch ((BaseStatsTag)msg4.tag)
                                    {
                                        case BaseStatsTag::STAMINA: pi.baseStamina = msg4.data.varInt; break;
                                        case BaseStatsTag::ATTACK: pi.baseAtk = msg4.data.varInt; break;
                                        case BaseStatsTag::DEFENSE: pi.baseDef = msg4.data.varInt; break;
                                    }
                                }
                            }
                            break;
                        }
                        case PokemonDetailsTag::QUICK_MOVES:
                        {
                            ProtoBuf fastMoves(msg3);

                            // Series of repeated varints.
                            while (fastMoves.getBytesLeft())
                            {
                                pi.fastMoves.push_back(fastMoves.readVarInt());
                            }
                            break;
                        }
                        case PokemonDetailsTag::CHARGED_MOVES:
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

                pi.nAvailableChargedMoves = pi.chargedMoves.size();
                pi.nAvailableFastMoves = pi.fastMoves.size();

                pi.id = id;
                double CPBase = (pi.baseAtk + 15) * sqrt((pi.baseDef + 15) * (pi.baseStamina + 15));
                pi.maxCP = CPBase * LEVEL40_CP_MULTIPLIER * LEVEL40_CP_MULTIPLIER / 10.0;
                if (pi.maxCP < conf.prestigerCP)
                {
                    pi.prestigerCPMultiplier = 0;
                }
                else
                {
                    pi.prestigerCPMultiplier = sqrt(conf.prestigerCP * 10 / CPBase);
                }
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

                pokemonNameToId[pi.name] = id;
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

                    switch ((MoveDetailsTag)msg3.tag)
                    {
                        case MoveDetailsTag::TYPE:
                            mi.moveType = msg3.data.varInt;
                            break;
                        case MoveDetailsTag::POWER:
                            memcpy(&mi.power, msg3.data.fixed, 4);
                            break;
                        case MoveDetailsTag::DURATION:
                            mi.duration = msg3.data.varInt / 1000.0;
                            break;
                        case MoveDetailsTag::ENERGY:
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

                moveNameToId[mi.name] = id;
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

                    switch ((TypeDetailsTag)msg3.tag)
                    {
                        case TypeDetailsTag::TYPE_CHART: // Type chart
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
                        case TypeDetailsTag::ID: // Type id
                            id = msg3.data.varInt;
                            printf("It's id: %d\n", id);
                            break;
                    }
                }

                typeNames[id] = match[1].str();
                typeChart[id] = typeEffeciveness;
            }
        }
    }

    // Set up legacy movesets.

    addLegacyMove("CHARMELEON", "SCRATCH_FAST");
    addLegacyMove("CHARIZARD", "WING_ATTACK_FAST");
    addLegacyMove("CHARIZARD", "EMBER_FAST");
    addLegacyMove("CHARIZARD", "FLAMETHROWER");
    addLegacyMove("BUTTERFREE", "BUG_BITE_FAST");
    addLegacyMove("BEEDRILL", "BUG_BITE_FAST");
    addLegacyMove("PIDGEOT", "WING_ATTACK_FAST");
    addLegacyMove("PIDGEOT", "AIR_CUTTER");
    addLegacyMove("SPEAROW", "TWISTER");
    addLegacyMove("FEAROW", "TWISTER");
    addLegacyMove("EKANS", "GUNK_SHOT");
    addLegacyMove("PIKACHU", "THUNDER");
    addLegacyMove("RAICHU", "THUNDER_SHOCK_FAST");
    addLegacyMove("RAICHU", "THUNDER");
    addLegacyMove("SANDSHREW", "ROCK_TOMB");
    addLegacyMove("NIDOKING", "FURY_CUTTER_FAST");
    addLegacyMove("CLEFABLE", "POUND_FAST");
    addLegacyMove("NINETALES", "EMBER_FAST");
    addLegacyMove("NINETALES", "FLAMETHROWER");
    addLegacyMove("NINETALES", "FIRE_BLAST");
    addLegacyMove("ZUBAT", "SLUDGE_BOMB");
    addLegacyMove("GOLBAT", "OMINOUS_WIND");
    addLegacyMove("PERSIAN", "NIGHT_SLASH");
    addLegacyMove("PRIMEAPE", "KARATE_CHOP_FAST");
    addLegacyMove("PRIMEAPE", "CROSS_CHOP");
    addLegacyMove("ARCANINE", "BITE_FAST");
    addLegacyMove("ARCANINE", "BULLDOZE");
    addLegacyMove("ARCANINE", "FLAMETHROWER");
    addLegacyMove("POLIWHIRL", "SCALD");
    addLegacyMove("POLIWRATH", "MUD_SHOT_FAST");
    addLegacyMove("POLIWRATH", "SUBMISSION");
    addLegacyMove("ALAKAZAM", "PSYCHIC");
    addLegacyMove("ALAKAZAM", "DAZZLING_GLEAM");
    addLegacyMove("MACHOP", "LOW_KICK_FAST");
    addLegacyMove("MACHOKE", "CROSS_CHOP");
    addLegacyMove("MACHAMP", "CROSS_CHOP");
    addLegacyMove("MACHAMP", "SUBMISSION");
    addLegacyMove("MACHAMP", "STONE_EDGE");
    addLegacyMove("MACHAMP", "KARATE_CHOP_FAST");
    addLegacyMove("WEEPINBELL", "RAZOR_LEAF_FAST");
    addLegacyMove("GRAVELER", "ROCK_SLIDE");
    addLegacyMove("GOLEM", "ANCIENT_POWER");
    addLegacyMove("PONYTA", "FIRE_BLAST");
    addLegacyMove("RAPIDASH", "EMBER_FAST");
    addLegacyMove("MAGNETON", "THUNDER_SHOCK_FAST");
    addLegacyMove("MAGNETON", "DISCHARGE");
    addLegacyMove("FARFETCHD", "CUT_FAST");
    addLegacyMove("DODUO", "SWIFT");
    addLegacyMove("DODUO", "SWIFT");
    addLegacyMove("DODRIO", "AIR_CUTTER");
    addLegacyMove("SEEL", "AQUA_JET");
    addLegacyMove("DEWGONG", "ICE_SHARD_FAST");
    addLegacyMove("DEWGONG", "AQUA_JET");
    addLegacyMove("DEWGONG", "ICY_WIND");
    addLegacyMove("MUK", "LICK_FAST");
    addLegacyMove("CLOYSTER", "ICY_WIND");
    addLegacyMove("CLOYSTER", "BLIZZARD");
    addLegacyMove("GASTLY", "SUCKER_PUNCH_FAST");
    addLegacyMove("GASTLY", "OMINOUS_WIND");
    addLegacyMove("HAUNTER", "LICK_FAST");
    addLegacyMove("HAUNTER", "SHADOW_BALL");
    addLegacyMove("GENGAR", "SHADOW_CLAW_FAST");
    addLegacyMove("GENGAR", "DARK_PULSE");
    addLegacyMove("ONIX", "IRON_HEAD");
    addLegacyMove("ONIX", "ROCK_SLIDE");
    addLegacyMove("HYPNO", "PSYSHOCK");
    addLegacyMove("HYPNO", "SHADOW_BALL");
    addLegacyMove("KINGLER", "MUD_SHOT_FAST");
    addLegacyMove("VOLTORB", "SIGNAL_BEAM");
    addLegacyMove("ELECTRODE", "TACKLE_FAST");
    addLegacyMove("EXEGGUTOR", "ZEN_HEADBUTT_FAST");
    addLegacyMove("EXEGGUTOR", "CONFUSION_FAST");
    addLegacyMove("HITMONLEE", "BRICK_BREAK");
    addLegacyMove("HITMONCHAN", "BRICK_BREAK");
    addLegacyMove("HITMONCHAN", "ROCK_SMASH_FAST");
    addLegacyMove("TANGELA", "POWER_WHIP");
    addLegacyMove("SCYTHER", "STEEL_WING_FAST");
    addLegacyMove("SCYTHER", "BUG_BUZZ");
    addLegacyMove("JYNX", "POUND_FAST");
    addLegacyMove("JYNX", "ICE_PUNCH");
    addLegacyMove("PINSIR", "FURY_CUTTER_FAST");
    addLegacyMove("PINSIR", "SUBMISSION");
    addLegacyMove("GYARADOS", "TWISTER");
    addLegacyMove("GYARADOS", "DRAGON_PULSE");
    addLegacyMove("LAPRAS", "DRAGON_PULSE");
    addLegacyMove("LAPRAS", "ICE_SHARD_FAST");
    addLegacyMove("EEVEE", "BODY_SLAM");
    addLegacyMove("FLAREON", "HEAT_WAVE");
    addLegacyMove("PORYGON", "TACKLE_FAST");
    addLegacyMove("PORYGON", "ZEN_HEADBUTT_FAST");
    addLegacyMove("PORYGON", "DISCHARGE");
    addLegacyMove("PORYGON", "SIGNAL_BEAM");
    addLegacyMove("PORYGON", "PSYBEAM");
    addLegacyMove("OMANYTE", "ROCK_TOMB");
    addLegacyMove("OMANYTE", "BRINE");
    addLegacyMove("OMASTAR", "ROCK_SLIDE");
    addLegacyMove("KABUTOPS", "FURY_CUTTER_FAST");
    addLegacyMove("SNORLAX", "BODY_SLAM");
    addLegacyMove("DRAGONITE", "DRAGON_BREATH_FAST");
    addLegacyMove("DRAGONITE", "DRAGON_CLAW");
    addLegacyMove("DRAGONITE", "DRAGON_PULSE");

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
        std::vector<PokemonInfo> pis; // A temporary vector to sort.

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

    std::vector<MoveInfo> moveByName; // To store the list of moves alphabetically.
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

    std::vector<MovesetDPS> overallMovesetStats; // Single bucket to sort all moveset stats
    AutoFile pokemons = fopen(POKEMON_LIST_FILE, "w");

    std::map<int, std::vector<MovesetDPS>> movesetStatsByType; // Moveset stats for each type
    std::map<int, std::map<int, std::vector<MovesetDPS>>> bestCounters; // Moveset stats for each type combination (FIXME: the key should be a int, int tuple instead of this)

    // For each pokémon...
    for (const auto &kv : pokemonList)
    {
        const PokemonInfo &pi = kv.second;

        std::stringstream str;
        for (auto tid : pi.pokemonTypes)
        {
            str << typeNames[tid] << " ";
        }

        fprintf(pokemons, "#%d %s (Type: %s) (Max CP: %g, ATK: %d, DEF: %d, STA: %d), prestiger CP multiplier: %g\n",
            pi.id,
            pi.name.c_str(),
            str.str().c_str(),
            pi.maxCP,
            pi.baseAtk,
            pi.baseDef,
            pi.baseStamina,
            pi.prestigerCPMultiplier
        );
        fprintf(pokemons, "Fast moves: \n");

        std::vector<MovesetDPS> pokemonMovesets; // Movesets of the pokémon.

        // For each moveset combination...
        for (size_t i = 0; i < pi.fastMoves.size(); i++)
        {
            int fmi = pi.fastMoves[i];
            for (size_t j = 0; j < pi.chargedMoves.size(); j++)
            {
                int cmi = pi.chargedMoves[j];
                // Simulate hitting a punching bag for 1000 seconds.
                MoveInfo &fastMove = moveList[fmi];
                MoveInfo &chargedMove = moveList[cmi];
                double energy = 0;
                double time = 0;
                double damage = 0;
                double primaryDamage = 0;
                double secondaryDamage = 0;

                int expectedHitsPerTurn = floor((conf.roundLength - 0.5) / fastMove.duration);
                bool dodging = expectedHitsPerTurn > 0;

                if (!dodging) continue;

                //printf("ExpectedHitsPerTurn: %d\n", expectedHitsPerTurn);

                bool legacy = (i >= pi.nAvailableFastMoves) || (j >= pi.nAvailableChargedMoves);

                double extraEnergy = 0.5*((pi.baseStamina + 15) * ATTACKER_CPM);

                //printf("extraEnergy: %g\n", extraEnergy);

                while (time < conf.battleTime)
                {
                    MoveInfo *moveToUse;
                    double *damageToRaise;
                    double stab = 1;
                    int nConsecutiveHits;
                    double remTime;

                    if (energy >= -chargedMove.energy)
                    {
                        // Do charged move.
                        moveToUse = &chargedMove;
                        damageToRaise = &secondaryDamage;
                        nConsecutiveHits = 1;
                    }
                    else
                    {
                        // Do fast move
                        moveToUse = &fastMove;
                        damageToRaise = &primaryDamage;
                        remTime = conf.roundLength - fmod(time, conf.roundLength);
                        if (dodging)
                        {
                            nConsecutiveHits = floor(remTime / fastMove.duration);
                            if (nConsecutiveHits > expectedHitsPerTurn) nConsecutiveHits = expectedHitsPerTurn;
                        }
                        else
                        {
                            nConsecutiveHits = 1;
                        }
                    }

                    for (int tid : pi.pokemonTypes)
                    {
                        if (moveToUse->moveType == tid)
                        {
                            stab = 1.25;
                            break;
                        }
                    }

                    damage += moveToUse->power * stab * nConsecutiveHits;
                    *damageToRaise += moveToUse->power * stab * nConsecutiveHits;
                    time += moveToUse->duration * nConsecutiveHits;
                    energy += moveToUse->energy * nConsecutiveHits;
                    energy += (moveToUse->duration / conf.battleTime) * extraEnergy * nConsecutiveHits;
                    if (energy > 100) energy = 100;
                    //printf("%s used %s %d times (damage: %g, energy: %d, staminaEnergy: %g)\n", pi.name.c_str(), moveToUse->name.c_str(), nConsecutiveHits, moveToUse->power, moveToUse->energy, (moveToUse->duration / BATTLE_TIME) * extraEnergy);
                    //printf("t: %g, primary dmg: %g, secondary dmg: %g, energy: %g\n", time, primaryDamage, secondaryDamage, energy);
                    if (dodging && moveToUse == &fastMove)
                    {
                        remTime -= moveToUse->duration * nConsecutiveHits;
                        if (remTime < 0.5) remTime = 0.5;
                        time += remTime; // Time spent dodging.
                        //printf("Then dodged for %g seconds.\n", remTime);
                        //printf("t: %g, primary dmg: %g, secondary dmg: %g, energy: %g\n", time, primaryDamage, secondaryDamage, energy);
                    }
                    //fgetc(stdin);
                }

                MovesetDPS mDPS;

                // Get the overall DPS.
                double rawDps = damage / time;

                /* printf("Moveset DPS: %g\n", rawDps); */

                mDPS.pokemonId = kv.first;
                mDPS.fastId = fmi;
                mDPS.chargedId = cmi;
                mDPS.populate(rawDps, pi);
                mDPS.isLegacy = legacy;
                mDPS.dodging = dodging;
                mDPS.fastAttacksPerTurn = expectedHitsPerTurn;

                // Store them for the pokémon and the overall bucket.
                pokemonMovesets.push_back(mDPS);
                overallMovesetStats.push_back(mDPS);

                // TODO: Hidden power of all type.
                // Put them into the typed buckets to find out
                if (chargedMove.moveType == fastMove.moveType)
                {
                    // Same type of damage
                    movesetStatsByType[fastMove.moveType].push_back(mDPS);
                }
                else
                {
                    // FIXME: Copypaste
                    // Fast and charged are different.
                    MovesetDPS primaryDPS = mDPS;
                    primaryDPS.populate(primaryDamage / time, pi);
                    movesetStatsByType[fastMove.moveType].push_back(primaryDPS);

                    MovesetDPS secondaryDPS = mDPS;
                    secondaryDPS.populate(secondaryDamage / time, pi);
                    movesetStatsByType[chargedMove.moveType].push_back(secondaryDPS);
                }

                // For each type combination...
                for (const auto &tnp1 : typeChart)
                {
                    for (const auto &tnp2: typeChart)
                    {
                        if (tnp1.first > tnp2.first) continue; // To avoid duplicates.

                        // Find out how much damage each moveset does against each combination of moves.
                        double theDamage;

                        if (tnp1.first == tnp2.first)
                        {
                            // Single typed pokémon are stored as double typed of the same type. Mind this.
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
                        dps.populate(theDamage / time, pi);
                        bestCounters[tnp1.first][tnp2.first].push_back(dps);
                    }
                }
            }
        }

        // Write out each pokémon and their respective moveset.
        std::sort(pokemonMovesets.begin(), pokemonMovesets.end(), [](MovesetDPS a, MovesetDPS b){return a.DPS > b.DPS; });
        for (const auto &mdps : pokemonMovesets)
        {
            mdps.printEntry(pokemons, mdps.DPS);
        }
        fprintf(pokemons, "\n");
    }

    // Write the overall DPS list.
    AutoFile dpsList = fopen("DPS.txt", "w");
    std::sort(overallMovesetStats.begin(), overallMovesetStats.end(), [](MovesetDPS a, MovesetDPS b){return a.DPS > b.DPS; });
    for (const auto &mdps : overallMovesetStats)
    {
        mdps.printEntry(dpsList, mdps.DPS);
    }

    // Write the true power list.
    AutoFile dtfList = fopen("DTF.txt", "w");
    std::sort(overallMovesetStats.begin(), overallMovesetStats.end(), [](MovesetDPS a, MovesetDPS b){return a.truePower > b.truePower; });
    for (const auto &mdps : overallMovesetStats)
    {
        mdps.printEntry(dtfList, mdps.truePower);
    }

    // Best DPS by Type
    AutoFile bestAttackersByType = fopen("DPSbyType.txt", "w");
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
            mdps.printEntry(bestAttackersByType, mdps.DPS);
        }
        fprintf(bestAttackersByType, "\n\n");
    }

    // Write best true power by type.
    AutoFile bestDTFByType = fopen("DTFbyType.txt", "w");
    for (auto &typeVecPair : movesetStatsByType)
    {
        auto &typeVec = typeVecPair.second;

        std::sort(typeVec.begin(), typeVec.end(), [](MovesetDPS a, MovesetDPS b){return a.truePower > b.truePower; });
    }

    for (const auto &typeVecPair : movesetStatsByType)
    {
        fprintf(bestDTFByType, "Best attackers of %s type:\n\n", typeNames[typeVecPair.first].c_str());
        for (const auto &mdps : typeVecPair.second)
        {
            mdps.printEntry(bestDTFByType, mdps.truePower);
        }
        fprintf(bestDTFByType, "\n\n");
    }

    // Write best counters by DPS
    AutoFile bestDPSCountersFile = fopen("DPSCounters.txt", "w");
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

            fprintf(bestDPSCountersFile, "Best counters of %s-%s\n\n", typeNames[t1.first].c_str(), typeNames[t2.first].c_str());
            for (const auto &mdps : vec)
            {
                mdps.printEntry(bestDPSCountersFile, mdps.DPS);
            }
            fprintf(bestDPSCountersFile, "\n\n");
        }
    }

    // Write Best counters by True power
    AutoFile bestDTFCountersFile = fopen("DTFCounters.txt", "w");
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

            fprintf(bestDTFCountersFile, "Best counters of %s-%s\n\n", typeNames[t1.first].c_str(), typeNames[t2.first].c_str());
            for (const auto &mdps : vec)
            {
                mdps.printEntry(bestDTFCountersFile, mdps.truePower);
            }
            fprintf(bestDTFCountersFile, "\n\n");
        }
    }

    // Best prestigers
    AutoFile prestigersFile = fopen("prestigers.txt", "w");
    for (auto &t1 : bestCounters)
    {
        for (auto &t2 : t1.second)
        {
            auto &vec = t2.second;

            std::sort(vec.begin(), vec.end(), [](MovesetDPS a, MovesetDPS b){return a.prestigePower > b.prestigePower; });
        }
    }

    for (const auto &t1 : bestCounters)
    {
        for (const auto &t2 : t1.second)
        {
            const auto &vec = t2.second;

            fprintf(prestigersFile, "Best counters of %s-%s\n\n", typeNames[t1.first].c_str(), typeNames[t2.first].c_str());
            for (const auto &mdps : vec)
            {
                mdps.printEntry(prestigersFile, mdps.prestigePower);
            }
            fprintf(bestDTFCountersFile, "\n\n");
        }
    }


    return 0;
}
