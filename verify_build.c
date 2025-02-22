
#include "cubiomes/finders.h"
#include <stdio.h>

// Biome ID to name mapping
const char* getBiomeName(int id) {
    switch(id) {
        case 0: return "Ocean";
        case 1: return "Plains";
        case 2: return "Desert";
        case 3: return "Windswept Hills";
        case 4: return "Forest";
        case 5: return "Taiga";
        case 6: return "Swamp";
        case 7: return "River";
        case 10: return "Frozen Ocean";
        case 11: return "Frozen River";
        case 12: return "Snowy Plains";
        case 13: return "Snowy Mountains";
        case 14: return "Mushroom Fields";
        case 15: return "Mushroom Fields Shore";
        case 16: return "Beach";
        case 17: return "Desert Hills";
        case 18: return "Windswept Forest";
        case 19: return "Taiga Hills";
        case 20: return "Mountain Edge";
        case 21: return "Jungle";
        case 22: return "Jungle Hills";
        case 23: return "Sparse Jungle";
        case 24: return "Deep Ocean";
        case 25: return "Stony Shore";
        case 26: return "Snowy Beach";
        case 27: return "Birch Forest";
        case 28: return "Birch Forest Hills";
        case 29: return "Dark Forest";
        case 30: return "Snowy Taiga";
        case 31: return "Snowy Taiga Hills";
        case 32: return "Old Growth Pine Taiga";
        case 33: return "Giant Tree Taiga Hills";
        case 34: return "Wooded Mountains";
        case 35: return "Savanna";
        case 36: return "Savanna Plateau";
        case 37: return "Badlands";
        case 38: return "Wooded Badlands";
        case 39: return "Badlands Plateau";
        case 44: return "Warm Ocean";
        case 45: return "Lukewarm Ocean";
        case 46: return "Cold Ocean";
        case 47: return "Deep Warm Ocean";
        case 48: return "Deep Lukewarm Ocean";
        case 49: return "Deep Cold Ocean";
        case 50: return "Deep Frozen Ocean";
        case 129: return "Sunflower Plains";
        case 130: return "Desert Lakes";
        case 131: return "Windswept Gravelly Hills";
        case 132: return "Flower Forest";
        case 133: return "Taiga Mountains";
        case 134: return "Swamp Hills";
        case 140: return "Ice Spikes";
        case 149: return "Modified Jungle";
        case 151: return "Modified Jungle Edge";
        case 155: return "Old Growth Birch Forest";
        case 156: return "Tall Birch Hills";
        case 157: return "Dark Forest Hills";
        case 158: return "Snowy Taiga Mountains";
        case 160: return "Old Growth Spruce Taiga";
        case 161: return "Giant Spruce Taiga Hills";
        case 162: return "Gravelly Mountains+";
        case 163: return "Windswept Savanna";
        case 164: return "Shattered Savanna Plateau";
        case 165: return "Eroded Badlands";
        case 166: return "Modified Wooded Badlands Plateau";
        case 167: return "Modified Badlands Plateau";
        case 168: return "Bamboo Jungle";
        case 169: return "Bamboo Jungle Hills";
        case 174: return "Dripstone Caves";
        case 175: return "Lush Caves";
        case 177: return "Meadow";
        case 178: return "Grove";
        case 179: return "Snowy Slopes";
        case 180: return "Frozen Peaks";
        case 181: return "Jagged Peaks";
        case 182: return "Stony Peaks";
        case 183: return "Deep Dark";
        case 184: return "Mangrove Swamp";
        case 185: return "Cherry Grove";
        case 186: return "Pale Garden";  // Corrected missing biome ID 163 duplication
        default: return "Unknown Biome";
    }
}

int main() {
    // Configuration
    const int mc = MC_1_21;           // Latest MC version supported
    const uint64_t seed = 12345;      // Your seed here
    const int useSpawn = 1;           // Use spawn point (1) instead of custom coords
    const int customX = 0;            // Custom X coordinate if not using spawn
    const int customZ = 0;            // Custom Z coordinate if not using spawn
    const int searchRadius = 500;     // Search radius in blocks

    // Set up main generator
    Generator g;
    setupGenerator(&g, mc, 0);
    applySeed(&g, DIM_OVERWORLD, seed);

    // Get spawn point if needed
    Pos spawn = {0, 0};
    int x0, z0;
    
    if (useSpawn) {
        spawn = getSpawn(&g);
        x0 = spawn.x - searchRadius;
        z0 = spawn.z - searchRadius;
        printf("Spawn point: x=%d z=%d\n", spawn.x, spawn.z);
    } else {
        x0 = customX - searchRadius;
        z0 = customZ - searchRadius;
    }

    int x1 = x0 + (searchRadius * 2);
    int z1 = z0 + (searchRadius * 2);

    printf("\nSearching area from (%d,%d) to (%d,%d)\n\n", x0, z0, x1, z1);

    SurfaceNoise sn;
    initSurfaceNoise(&sn, DIM_OVERWORLD, seed);

    // For nether structures
    Generator ng;
    setupGenerator(&ng, mc, 0);
    applySeed(&ng, DIM_NETHER, seed);

    // For end structures
    Generator eg;
    setupGenerator(&eg, mc, 0);
    applySeed(&eg, DIM_END, seed);

    EndNoise en;
    setEndSeed(&en, mc, seed);
    SurfaceNoise esn;
    initSurfaceNoise(&esn, DIM_END, seed);

    // Check all structure types
    for (int structureType = 0; structureType < FEATURE_NUM; structureType++) {
        StructureConfig sconf;
        if (!getStructureConfig(structureType, mc, &sconf))
            continue;

        Generator *curr_gen = &g;
        SurfaceNoise *curr_sn = &sn;

        if (sconf.dim == DIM_NETHER) {
            curr_gen = &ng;
        } else if (sconf.dim == DIM_END) {
            curr_gen = &eg;
            curr_sn = &esn;
        }

        double blocksPerRegion = sconf.regionSize * 16.0;
        int rx0 = (int)floor(x0 / blocksPerRegion);
        int rz0 = (int)floor(z0 / blocksPerRegion);
        int rx1 = (int)ceil(x1 / blocksPerRegion);
        int rz1 = (int)ceil(z1 / blocksPerRegion);

        for (int j = rz0; j <= rz1; j++) {
            for (int i = rx0; i <= rx1; i++) {
                Pos pos;
                if (!getStructurePos(structureType, mc, seed, i, j, &pos))
                    continue;

                if (pos.x < x0 || pos.x > x1 || pos.z < z0 || pos.z > z1)
                    continue;

                if (!isViableStructurePos(structureType, curr_gen, pos.x, pos.z, 0))
                    continue;

                int id = getBiomeAt(curr_gen, 4, pos.x>>2, 320>>2, pos.z>>2);
                if (id == -1) { // if unknown, try surface
                    float height[256];
                    int w = 16, h = 16;
                    Range r = {4, pos.x>>2, pos.z>>2, w, h, 320>>2, 1};
                    mapApproxHeight(height, NULL, curr_gen, curr_sn, r.x, r.z, w, h);

                    int lx = (pos.x & 15);
                    int lz = (pos.z & 15);
                    int surface_y = (int)height[lz * w + lx];
                    id = getBiomeAt(curr_gen, 4, pos.x>>2, surface_y>>2, pos.z>>2);
                }

                StructureVariant sv;
                getVariant(&sv, structureType, mc, seed, pos.x, pos.z, id);

                float height[256];
                int w = 16, h = 16;
                Range r = {4, pos.x>>2, pos.z>>2, w, h, 320>>2, 1};
                mapApproxHeight(height, NULL, curr_gen, curr_sn, r.x, r.z, w, h);

                int lx = (pos.x & 15);
                int lz = (pos.z & 15);
                int surface_y = (int)height[lz * w + lx];
                
                // Use provided Y if available, otherwise use surface height
                int y_pos = sv.y != 320 && sv.y >= -64 ? sv.y : surface_y;
                int check_y = y_pos;  // Use same Y for biome check

                int biome_id = getBiomeAt(curr_gen, 1, pos.x + sv.x, check_y, pos.z + sv.z);
                if (biome_id == -1 || (biome_id >= 170 && biome_id <= 172)) { // if unknown or trial/trail biome, try surface
                    biome_id = getBiomeAt(curr_gen, 1, pos.x + sv.x, surface_y, pos.z + sv.z);
                }

                const char *struct_names[] = {
                    "Feature", "Desert_Pyramid", "Jungle_Temple", "Swamp_Hut",
                    "Igloo", "Village", "Ocean_Ruin", "Shipwreck", "Monument",
                    "Mansion", "Outpost", "Ruined_Portal", "Ruined_Portal_N",
                    "Ancient_City", "Treasure", "Mineshaft", "Desert_Well",
                    "Geode", "Trail_Ruins", "Trial_Chambers"
                };

                printf("Found %s\n", struct_names[structureType]);
                printf("  Position: x:%d y:%d z:%d\n", 
                    pos.x + sv.x, 
                    y_pos,
                    pos.z + sv.z);
                printf("  Biome: %s (ID: %d)\n", getBiomeName(biome_id), biome_id);
                printf("  Distance from %s: %.1f blocks\n\n",
                    useSpawn ? "spawn" : "origin",
                    sqrt((pos.x-spawn.x)*(pos.x-spawn.x) + (pos.z-spawn.z)*(pos.z-spawn.z)));
            }
        }
    }

    return 0;
}
