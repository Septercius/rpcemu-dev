#include <stdlib.h>
#include "models.h"
#include "rpcemu.h"

ModelDetails machine; /**< The details of the current machine being emulated */

/** Array of details of models the emulator can emulate, must be kept in sync with
    Model enum in rpcemu.h */
const ModelDetails models[] = {
    { Model_A7000, "A7000", "A7000", ARMArchitecture_V3, CPUModel_ARM7500, IOMDType_ARM7500, MouseController_IOMD, MouseType_PS2, SuperIOType_FDC37C665GT, I2CBitField_PCF8583, MemoryFlags_RAMVariable | MemoryFlags_VRAMNone, 0, 0 },
    { Model_A7000Plus, "A7000+ (experimental)", "A7000+", ARMArchitecture_V3, CPUModel_ARM7500FE, IOMDType_ARM7500FE, MouseController_IOMD, MouseType_PS2, SuperIOType_FDC37C665GT, I2CBitField_PCF8583, MemoryFlags_RAMVariable | MemoryFlags_VRAMNone, 0, 0 },
    { Model_RPCARM610, "Risc PC - ARM610", "RPC610", ARMArchitecture_V3, CPUModel_ARM610, IOMDType_IOMD, MouseController_IOMD, MouseType_Quadrature, SuperIOType_FDC37C665GT, I2CBitField_PCF8583, MemoryFlags_RAMVariable | MemoryFlags_VRAMVariable, 0, 0 },
    { Model_RPCARM710, "Risc PC - ARM710", "RPC710", ARMArchitecture_V3, CPUModel_ARM710, IOMDType_IOMD, MouseController_IOMD, MouseType_Quadrature, SuperIOType_FDC37C665GT, I2CBitField_PCF8583, MemoryFlags_RAMVariable | MemoryFlags_VRAMVariable, 0, 0 },
    { Model_RPCARM810, "Risc PC - ARM810 (experimental)", "RPC810", ARMArchitecture_V4, CPUModel_ARM810, IOMDType_IOMD, MouseController_IOMD, MouseType_Quadrature, SuperIOType_FDC37C665GT, I2CBitField_PCF8583, MemoryFlags_RAMVariable | MemoryFlags_VRAMVariable, 0, 0 },
    { Model_RPCSA110, "Risc PC - StrongARM", "RPCSA", ARMArchitecture_V4, CPUModel_SA110, IOMDType_IOMD, MouseController_IOMD, MouseType_Quadrature, SuperIOType_FDC37C665GT, I2CBitField_PCF8583, MemoryFlags_RAMVariable | MemoryFlags_VRAMVariable, 0, 0 },
    { Model_Phoebe, "Phoebe (Risc PC2)", "Phoebe", ARMArchitecture_V4, CPUModel_SA110, IOMDType_IOMD2, MouseController_I8042, MouseType_PS2, SuperIOType_FDC37C672, I2CBitField_PCF8583 | I2CBitField_SPD_DIMM0 }, MemoryFlags_RAMFixed | MemoryFlags_VRAMFixed, 256, 4 };

const char *models_get_config_name(Model model)
{
    for (int i = 0; i < Model_MAX; i += 1)
    {
        if (models[i].model == model)
        {
            return models[i].name_config;
        }
    }
    
    return NULL;
}


ModelDetails *models_find(Model model)
{
    for (int i = 0; i < Model_MAX; i += 1)
    {
        if (models[i].model == model)
        {
            return &models[i];
        }
    }
    
    fatal("Model %d not found", model);
}
