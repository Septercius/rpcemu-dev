/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2017 Peter Howkins

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <QSettings>

#include "models.h"
#include "paths.h"
#include "rpcemu.h"

#ifdef FEATURE_MULTI_HOSTFS
#include "hostfs-multi.h"
#endif /* FEATURE_MULTI_HOSTFS */

#define MEMORY_ARRAYSIZE 7
#define VIDEO_ARRAYSIZE 2

static unsigned memoryValues[MEMORY_ARRAYSIZE] = {4, 8, 16, 32, 64, 128, 256};
static unsigned videoValues[VIDEO_ARRAYSIZE] = {0, 2};

char *config_get_string(QSettings &settings, QAnyStringView key, const QVariant &defaultValue)
{
    QString value = settings.value(key, defaultValue).toString();

    QByteArray ba = value.toUtf8();
    
    return strdup(ba.constData());
}

void config_set_string(QSettings &settings, QAnyStringView key, const char *value)
{
    if (value == NULL)
    {
        settings.setValue(key, "");
    }
    else
    {
        settings.setValue(key, QString(value));
    }
}

#ifdef FEATURE_MULTI_HOSTFS


/**
 * Parse and load HostFS configuration
 */
static void config_hostfs_load(QSettings &settings, Config *config)
{
    int driveNumber;
    QByteArray ba;
    QString sText;
    HostFSDrive *drive;

    settings.beginReadArray("HostFS");

    for (int i = 0; i < HOSTFS_DRIVE_MAX; i += 1)
    {
        settings.setArrayIndex(i);

        driveNumber = HOSTFS_DRIVE_BASE + i;

        drive = &config->hostfs_drive[i];
        drive->id = driveNumber;

        // Read the enabled flag and the boot option.
        drive->enabled = settings.value("enabled", (i == 0 ? "1" : "0")).toInt();
        drive->bootOption = settings.value("bootOption", (i == 0 ? "2" : "0")).toInt();

        // Read the drive name.
        sText = settings.value("name", (i == 0 ? "HostFS" : "")).toString();

        ba = sText.toUtf8();

        if (strlen(ba.data()) == 0)
        {
            drive->driveName = NULL;
            drive->enabled = 0;
        }
        else
        {
            drive->driveName = strdup(ba.data());
        }

        // Read the mapped folder.
#ifdef RPCEMU_PLATFORM_WIN32
        sText = settings.value("path", (i == 0 ? ".\\hostfs" : "")).toString();
#else
        sText = settings.value("path", (i == 0 ? "./hostfs" : "")).toString();
#endif /* RPCEMU_PLATFORM_WIN32 */
        ba = sText.toUtf8();

        if (strlen(ba.data()) == 0)
        {
            drive->hostPath = NULL;
            drive->enabled = 0;
        }
        else
        {
            drive->hostPath = strdup(ba.data());
            drive->resolvedHostPath = path_make_absolute(drive->hostPath);

            // Ensure the path is valid.
            if (!path_directory_exists(drive->resolvedHostPath))
            {
                drive->enabled = 0;
            }
        }

        if (drive->enabled)
        {
            fprintf(stderr, "HostFS: '%s' mapped to drive %d, '%s' (boot option = %d)\n", drive->resolvedHostPath, drive->id, drive->driveName, drive->bootOption);
        }
        else
        {
            fprintf(stderr, "HostFS: drive %d disabled.\n", drive->id);
        }
    }

    settings.endArray();
}

/**
 * Store HostFS configuration.
 */
static void config_hostfs_save(QSettings &settings, Config *config)
{
    HostFSDrive *drive;

    settings.beginWriteArray("HostFS");

    for (int i = 0; i < HOSTFS_DRIVE_MAX; i += 1)
    {
        settings.setArrayIndex(i);

        drive = &config->hostfs_drive[i];

        settings.setValue("enabled", (drive == NULL ? 0 : drive->enabled));
        if (drive == NULL || !drive->enabled)
        {
            continue;
        }

        settings.setValue("boot_option", drive->bootOption);
        settings.setValue("name", QString(drive->driveName));
        settings.setValue("path", QString(drive->hostPath));
    }

    settings.endArray();
}

#endif /* FEATURE_MULTI_HOSTFS */

/**
 * Parse and load NAT port forwarding rules into the global list
 */
static void config_nat_rules_load(QSettings &settings)
{
    const int size = settings.beginReadArray("nat_port_forward_rules");

    for (int i = 0; i < size; i++)
    {
        PortForwardRule rule;

        settings.setArrayIndex(i);

        QString rule_type_name = settings.value("type", "").toString();
        if (rule_type_name == "TCP")
        {
            rule.type = PORT_FORWARD_TCP;
        }
        else if (rule_type_name == "UDP")
        {
            rule.type = PORT_FORWARD_UDP;
        }
        else
        {
            error("Unknown port forward type, must be TCP or UDP");
            continue; // Give up on this entry
        }

        const unsigned emu_port = settings.value("emu_port", 0).toUInt();
        const unsigned host_port = settings.value("host_port", 0).toUInt();

        if (emu_port == 0 || emu_port > 65535)
        {
            error("Invalid port forward emu port");
            continue;
        }
        if (host_port == 0 || host_port > 65535)
        {
            error("Invalid port forward host port");
            continue;
        }

        rule.emu_port = (uint16_t) emu_port;
        rule.host_port = (uint16_t) host_port;

        rpcemu_nat_forward_add(rule);
    }
    settings.endArray();
}

/**
 * Store NAT port forwarding rules from the global list
 */
static void config_nat_rules_save(QSettings &settings)
{
    int itemnum = 0;

    settings.beginWriteArray("nat_port_forward_rules");
    for (int i = 0; i < MAX_PORT_FORWARDS; i++)
    {
        if (port_forward_rules[i].type != PORT_FORWARD_NONE)
        {
            settings.setArrayIndex(itemnum);
            settings.setValue("type", port_forward_rules[i].type == PORT_FORWARD_TCP ? "TCP" : "UDP");
            settings.setValue("emu_port", port_forward_rules[i].emu_port);
            settings.setValue("host_port", port_forward_rules[i].host_port);
            itemnum++;
        }
    }
    settings.endArray();
}

/**
 * Load the user's previous chosen configuration. Will fill in sensible
 * defaults if any configuration values are absent.
 *
 * Called on program startup.
 *
 * @param config
 */
void config_load(Config *config)
{
    char configpath[PATH_MAX];
    const char *p;
    Model model;
    int size;
    QString sText;
    QByteArray ba;
    bool valid;

    path_append_data(configpath, "rpc.cfg");

    QSettings settings(configpath, QSettings::IniFormat);

    /* Copy the contents of the configfile to the log */
    QStringList keys = settings.childKeys();
    foreach (const QString &key, settings.childKeys())
    {
        sText = QString("config_load: %1 = \"%2\"\n").arg(key, settings.value(key).toString());
        rpclog("%s", sText.toLocal8Bit().constData());
    }
    
    sText = settings.value("model", "").toString();
    ba = sText.toUtf8();
    p = ba.data();
    
    model = models_find_type(p, Model_RPCARM710);
    rpcemu_model_changed(model);
    
    // Set main memory.
    if ((machine.memory_flags & MemoryFlags_RAMFixed) != 0)
    {
        // Fixed main memory.
        config->mem_size = machine.ram_fixed_size;
    }
    else
    {
        // Configurable main memory.
        size = settings.value("mem_size", "16").toInt(&valid);
        if (valid)
        {
            valid = false;
            
            for (int i = 0; i < MEMORY_ARRAYSIZE; i += 1)
            {
                if (memoryValues[i] == size)
                {
                    config->mem_size = size;
                    
                    valid = true;
                    break;
                }
            }
        }
        
        if (!valid)
        {
            config->mem_size = 16;
        }
    }

    // Set video memory.
    if ((machine.memory_flags & MemoryFlags_VRAMNone) != 0)
    {
        // No VRAM.
        config->vram_size = 0;
    }
    else if ((machine.memory_flags & MemoryFlags_VRAMFixed) != 0)
    {
        // Fixed VRAM.
        config->vram_size = machine.vram_fixed_size;
    }
    else
    {
        // Configurable VRAM.
        size = settings.value("vram_size", "8").toInt(&valid);
        
        if (valid)
        {
            config->vram_size = (size == 0 ? 0 : 8);
        }
        else
        {
            config->vram_size = 8;
        }
    }

    config->soundenabled = settings.value("sound_enabled", "1").toInt();
    config->refresh = settings.value("refresh_rate", "60").toInt();
    config->cdromenabled = settings.value("cdrom_enabled", "0").toInt();
    config->cdromtype = settings.value("cdrom_type", "0").toInt();

    sText = settings.value("cdrom_iso", "").toString();
    ba = sText.toUtf8();
    if (snprintf(config->isoname, sizeof(config->isoname), "%s", ba.constData()) >= (int) sizeof(config->isoname))
    {
        // Path in config file longer then buffer
        rpclog("config_load: cdrom_iso path too long - ignored\n");
        config->isoname[0] = '\0';
    }

    config->mousehackon = settings.value("mouse_following", "1").toInt();
    config->mousetwobutton = settings.value("mouse_twobutton", "0").toInt();

    sText = settings.value("network_type", "off").toString();
    if (!QString::compare(sText, "off", Qt::CaseInsensitive))
    {
        config->network_type = NetworkType_Off;
    }
    else if (!QString::compare(sText, "nat", Qt::CaseInsensitive))
    {
        config->network_type = NetworkType_NAT;
    }
    else if (!QString::compare(sText, "iptunnelling", Qt::CaseInsensitive))
    {
        config->network_type = NetworkType_IPTunnelling;
    }
    else if (!QString::compare(sText, "ethernetbridging", Qt::CaseInsensitive))
    {
        config->network_type = NetworkType_EthernetBridging;
    }
    else
    {
        QByteArray ba = sText.toUtf8();
        rpclog("Unknown network_type '%s', defaulting to off\n", ba.data());
        config->network_type = NetworkType_Off;
    }

    config->username = config_get_string(settings, "username", "");
    config->ipaddress = config_get_string(settings, "ipaddress", "");
    config->macaddress = config_get_string(settings, "macaddress", "");
    config->bridgename = config_get_string(settings, "bridgename", "");
    config->network_capture = config_get_string(settings, "network_capture", "");

    config->cpu_idle = settings.value("cpu_idle", "0").toInt();

    config->show_fullscreen_message = settings.value("show_fullscreen_message", "1").toInt();


#ifdef FEATURE_MULTI_HOSTFS
    config->confirm_reset = settings.value("confirm_reset", "1").toInt();
    config->confirm_quit = settings.value("confirm_quit", "1").toInt();

    config->show_dotfiles = settings.value("show_dotfiles", "0").toInt();
    config->show_systemfiles = settings.value("show_systemfiles", "0").toInt();

    config_hostfs_load(settings, config);
#endif /* FEATURE_MULTI_HOSTFS */
    
    settings.beginGroup("lastDirectory");
    
    config->last_cdrom_directory = config_get_string(settings, "cdrom", "");
    config->last_floppy_directory = config_get_string(settings, "floppy", "");
    config->last_screenshot_directory = config_get_string(settings, "screenshot", "");
    
    settings.endGroup();

    config_nat_rules_load(settings);
}

/**
 * Store the user's most recently chosen configuration to disc, for use next
 * time the program starts.
 *
 * Called on program exit.
 */
void config_save(Config *config)
{
    char configpath[PATH_MAX];

    path_append_data(configpath, "rpc.cfg");

    QSettings settings(configpath, QSettings::IniFormat);
    settings.clear();

    const char *model_name = models_get_name(machine.model);
    config_set_string(settings, "model", model_name);
    
    settings.setValue("mem_size", config->mem_size);
    settings.setValue("vram_size", (config->vram_size != 0 ? 2 : 0));
    settings.setValue("sound_enabled", config->soundenabled);
    settings.setValue("refresh_rate", config->refresh);
    settings.setValue("cdrom_enabled", config->cdromenabled);
    settings.setValue("cdrom_type", config->cdromtype);
    settings.setValue("cdrom_iso", QString(config->isoname));
    settings.setValue("mouse_following", config->mousehackon);
    settings.setValue("mouse_twobutton", config->mousetwobutton);

    switch (config->network_type)
    {
        case NetworkType_Off:
            config_set_string(settings, "network_type", "off");
            break;

        case NetworkType_NAT:
            config_set_string(settings, "network_type", "nat");
            break;

        case NetworkType_EthernetBridging:
            config_set_string(settings, "network_type", "ethernetbridging");
            break;

        case NetworkType_IPTunnelling:
            config_set_string(settings, "network_type", "iptunnelling");
            break;
    }
    
    config_set_string(settings, "username", config->username);
    config_set_string(settings, "ipaddress", config->ipaddress);
    config_set_string(settings, "macaddress", config->macaddress);
    config_set_string(settings, "bridgename", config->bridgename);

    settings.setValue("cpu_idle", config->cpu_idle);
    settings.setValue("show_fullscreen_message", config->show_fullscreen_message);

    if (config->network_capture)
    {
        settings.setValue("network_capture", QString(config->network_capture));
    }
    
    settings.beginGroup("lastDirectory");

    config_set_string(settings, "cdrom", config->last_cdrom_directory);
    config_set_string(settings, "floppy", config->last_floppy_directory);
    config_set_string(settings, "screenshot", config->last_screenshot_directory);
    
    settings.endGroup();

#ifdef FEATURE_MULTI_HOSTFS
    settings.setValue("confirm_quit", config->confirm_quit);
    settings.setValue("confirm_reset", config->confirm_reset);

    settings.setValue("show_dotfiles", config->show_dotfiles);
    settings.setValue("show_systemfiles", config->show_systemfiles);

    config_hostfs_save(settings, config);
#endif

    config_nat_rules_save(settings);
}
