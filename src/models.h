#ifndef MODELS_H
#define MODELS_H

#include <stdint.h>

/** ARM architecture type */
typedef enum
{
    ARMArchitecture_V3,
    ARMArchitecture_V4
} ARMArchitecture;

/** The type of processor configured */
typedef enum
{
    CPUModel_ARM610,
    CPUModel_ARM710,
    CPUModel_SA110,
    CPUModel_ARM7500,
    CPUModel_ARM7500FE,
    CPUModel_ARM810
} CPUModel;

/* I2C bit fields */
typedef enum
{
    I2CBitField_PCF8583 = (1 << 0),
    I2CBitField_SPD_DIMM0 = (1 << 1)
} I2CBitField;

/** The type of IOMD chip */
typedef enum
{
    IOMDType_IOMD,
    IOMDType_ARM7500,
    IOMDType_ARM7500FE,
    IOMDType_IOMD2
} IOMDType;

/** Memory flags */
typedef enum
{
    MemoryFlags_RAMFixed = (1 << 0),
    MemoryFlags_RAMVariable = (1 << 1),
    MemoryFlags_VRAMNone = (1 << 2),
    MemoryFlags_VRAMFixed = (1 << 3),
    MemoryFlags_VRAMVariable = (1 << 4)
} MemoryFlags;

/** Selection of models that the emulator can emulate. */
typedef enum
{
    Model_RPCARM610,
    Model_RPCARM710,
    Model_RPCSA110,
    Model_A7000,
    Model_A7000Plus,
    Model_RPCARM810,
    Model_Phoebe,
    Model_MAX /**< Always last entry */
} Model;

/* Type of mouse controller */
typedef enum
{
    MouseController_I8042,
    MouseController_IOMD
} MouseController;

/* Type of mouse */
typedef enum
{
    MouseType_PS2,
    MouseType_Quadrature
} MouseType;

/** The type of networking configured */
typedef enum
{
    NetworkType_Off,
    NetworkType_NAT,
    NetworkType_EthernetBridging,
    NetworkType_IPTunnelling,
} NetworkType;

/** The type of SuperIO chip */
typedef enum
{
    SuperIOType_FDC37C665GT,
    SuperIOType_FDC37C672
} SuperIOType;

/** Structure to hold details about a model that the emulator can emulate */
typedef struct
{
    Model model;                            /**< Model code */
    const char *name_gui;                   /**< String used in the GUI */
    const char *name_config;                /**< String used in the Config file to select model */
    ARMArchitecture arm_architecture;       /**< ARM architecture version */
    CPUModel cpu_model;                     /**< CPU used in this model */
    IOMDType iomd_type;                     /**< IOMD used in this model */
    MouseController mouse_controller;       /**< Type of mouse controller */
    MouseType mouse_type;                   /**< Type of mouse in this model */
    SuperIOType super_type;                 /**< SuperIO chip used in this model */
    uint32_t i2c_devices;                   /**< Bitfield of devices on the I2C bus */
    MemoryFlags memory_flags;               /**< Flags for memory */
    int ram_fixed_size;                     /**< Fixed RAM size (in megabytes) */
    int vram_fixed_size;                    /**< Fixed VRAM size (in megabytes) */
} ModelDetails;

extern const ModelDetails models[]; /**< array of details of models the emulator can emulate */

extern ModelDetails machine; /**< The details of the current model being emulated */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
extern const char *models_get_config_name(Model model);
extern const ModelDetails *models_find(Model model);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
