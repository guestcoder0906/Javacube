
#include "cubiomes/finders.h"
#include <stdio.h>

int main() {
    int mc = MC_1_21; // Latest MC version supported
    uint64_t seed;
    int x0, z0;
    int useSpawn = 0;

    printf("Enter seed: ");
    scanf("%lu", &seed);
    printf("Use spawn as origin? (1=yes, 0=no): ");
    scanf("%d", &useSpawn);

    // Set up main generator
    Generator g;
    setupGenerator(&g, mc, 0);
    applySeed(&g, DIM_OVERWORLD, seed);

    // Get spawn point if needed
    Pos spawn = {0, 0};
    if (useSpawn) {
        spawn = getSpawn(&g);
        x0 = spawn.x - 500;
        z0 = spawn.z - 500;
        printf("Spawn point: x=%d z=%d\n", spawn.x, spawn.z);
    } else {
        x0 = -500;
        z0 = -500;
    }

    int x1 = x0 + 1000; // 500 blocks in each direction
    int z1 = z0 + 1000;

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

        // Select appropriate generator based on dimension
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

                // Get structure variant info
                int id = getBiomeAt(curr_gen, 4, pos.x>>2, 320>>2, pos.z>>2);
                StructureVariant sv;
                getVariant(&sv, structureType, mc, seed, pos.x, pos.z, id);

                // Get surface height at structure position
                float height[1];
                Range r = {4, pos.x>>2, pos.z>>2, 1, 1, 320>>2, 1};
                mapEndSurfaceHeight(height, curr_gen, curr_sn, r.x, r.z, r.sx, r.sz, r.scale, 0);
                int surface_y = (int)height[0];

                // Get biome at structure position
                int biome_id = getBiomeAt(curr_gen, 1, pos.x + sv.x, sv.y >= 0 ? sv.y : surface_y, pos.z + sv.z);

                // Print structure info
                const char *struct_names[] = {
                    "Feature", "Desert_Pyramid", "Jungle_Temple", "Swamp_Hut", 
                    "Igloo", "Village", "Ocean_Ruin", "Shipwreck", "Monument",
                    "Mansion", "Outpost", "Ruined_Portal", "Ruined_Portal_N",
                    "Ancient_City", "Treasure", "Mineshaft", "Desert_Well",
                    "Geode", "Fortress", "Bastion", "End_City", "End_Gateway",
                    "End_Island", "Trail_Ruins", "Trial_Chambers"
                };

                printf("Found %s\n", struct_names[structureType]);
                printf("  Position: x:%d y:%d z:%d\n", 
                    pos.x + sv.x, 
                    sv.y >= 0 ? sv.y : surface_y,
                    pos.z + sv.z);
                printf("  Surface Height: %d\n", surface_y);
                printf("  Biome ID: %d\n", biome_id);
                printf("  Distance from %s: %.1f blocks\n\n",
                    useSpawn ? "spawn" : "origin",
                    sqrt((pos.x-spawn.x)*(pos.x-spawn.x) + (pos.z-spawn.z)*(pos.z-spawn.z)));
            }
        }
    }

    return 0;
}
