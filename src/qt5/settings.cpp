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

#include "rpcemu.h"

/**
 * Parse and load NAT port forwarding rules into the global list
 */
static void
config_nat_rules_load(QSettings &settings)
{
	const int size = settings.beginReadArray("nat_port_forward_rules");

	for (int i = 0; i < size; i++) {
		PortForwardRule rule;

		settings.setArrayIndex(i);

		QString rule_type_name = settings.value("type", "").toString();
		if (rule_type_name == "TCP") {
			rule.type = PORT_FORWARD_TCP;
		} else if (rule_type_name == "UDP") {
			rule.type = PORT_FORWARD_UDP;
		} else {
			error("Unknown port forward type, must be TCP or UDP");
			continue; // Give up on this entry
		}

		const unsigned emu_port = settings.value("emu_port", 0).toUInt();
		const unsigned host_port = settings.value("host_port", 0).toUInt();

		if (emu_port == 0 || emu_port > 65535) {
			error("Invalid port forward emu port");
			continue;
		}
		if (host_port == 0 || host_port > 65535) {
			error("Invalid port forward host port");
			continue;
		}

		rule.emu_port  = (uint16_t) emu_port;
		rule.host_port = (uint16_t) host_port;

		rpcemu_nat_forward_add(rule);
	}
	settings.endArray();
}

/**
 * Store NAT port forwarding rules from the global list
 */
static void
config_nat_rules_save(QSettings &settings)
{
	int itemnum = 0;

	settings.beginWriteArray("nat_port_forward_rules");
	for (int i = 0; i < MAX_PORT_FORWARDS; i++) {
		if (port_forward_rules[i].type != PORT_FORWARD_NONE) {
			settings.setArrayIndex(itemnum);
			settings.setValue("type",      port_forward_rules[i].type == PORT_FORWARD_TCP ? "TCP" : "UDP");
			settings.setValue("emu_port",  port_forward_rules[i].emu_port);
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
void
config_load(Config * config)
{
	char filename[512];
	const char *p;
	Model model;
	int i;
	QString sText;
	QByteArray ba;

	snprintf(filename, sizeof(filename), "%srpc.cfg", rpcemu_get_datadir());

	QSettings settings(filename, QSettings::IniFormat);

	/* Copy the contents of the configfile to the log */
	QStringList keys = settings.childKeys();
	foreach (const QString &key, settings.childKeys()) {
		sText = QString("config_load: %1 = \"%2\"\n").arg(key, settings.value(key).toString());
		rpclog("%s", sText.toLocal8Bit().constData());
	}

	sText = settings.value("mem_size", "16").toString();
	ba = sText.toUtf8();
	p = ba.data();
	if (!strcmp(p, "4")) {
		config->mem_size = 4;
	} else if (!strcmp(p, "8")) {
		config->mem_size = 8;
	} else if (!strcmp(p, "32")) {
		config->mem_size = 32;
	} else if (!strcmp(p, "64")) {
		config->mem_size = 64;
	} else if (!strcmp(p, "128")) {
		config->mem_size = 128;
	} else if (!strcmp(p, "256")) {
		config->mem_size = 256;
	} else {
		config->mem_size = 16;
	}

	sText = settings.value("vram_size", "").toString();
	if (!QString::compare(sText, "", Qt::CaseInsensitive)) {
		config->vram_size = 8;
	} else if (!QString::compare(sText, "0", Qt::CaseInsensitive)) {
		config->vram_size = 0;
	} else {
		config->vram_size = 8;
	}

	sText = settings.value("model", "").toString();
	ba = sText.toUtf8();
	p = ba.data();
	model = Model_RPCARM710;
	if (p != NULL) {
		for (i = 0; i < Model_MAX; i++) {
			if (strcasecmp(p, models[i].name_config) == 0) {
				model = (Model) i;
				break;
			}
		}
	}

	rpcemu_model_changed(model);

	/* A7000 and A7000+ have no VRAM */
	if (model == Model_A7000 || model == Model_A7000plus) {
		config->vram_size = 0;
	}

	/* If Phoebe, override some settings */
	if (model == Model_Phoebe) {
		config->mem_size = 256;
		config->vram_size = 4;
	}

	config->soundenabled = settings.value("sound_enabled", "1").toInt();
	config->refresh      = settings.value("refresh_rate", "60").toInt();
	config->cdromenabled = settings.value("cdrom_enabled", "0").toInt();
	config->cdromtype    = settings.value("cdrom_type", "0").toInt();

	sText = settings.value("cdrom_iso", "").toString();
	ba = sText.toUtf8();
	if (snprintf(config->isoname, sizeof(config->isoname), "%s", ba.constData()) >= (int) sizeof(config->isoname)) {
		// Path in config file longer then buffer
		rpclog("config_load: cdrom_iso path too long - ignored\n");
		config->isoname[0] = '\0';
	}

	config->mousehackon = settings.value("mouse_following", "1").toInt();
	config->mousetwobutton = settings.value("mouse_twobutton", "0").toInt();

	sText = settings.value("network_type", "off").toString();
	if (!QString::compare(sText, "off", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_Off;
	} else if (!QString::compare(sText, "nat", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_NAT;
	} else if (!QString::compare(sText, "iptunnelling", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_IPTunnelling;
	} else if (!QString::compare(sText, "ethernetbridging", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_EthernetBridging;
	} else {
		QByteArray ba = sText.toUtf8();
		rpclog("Unknown network_type '%s', defaulting to off\n", ba.data());
		config->network_type = NetworkType_Off;
	}

	/* Take a copy of the string config values, to allow dynamic alteration
	   later */
	sText = settings.value("username", "").toString();
	ba = sText.toUtf8();
	if(strlen(ba.data()) != 0) {
		config->username = strdup(ba.data());
	} else {
		config->username = NULL;
	}
	
	sText = settings.value("ipaddress", "").toString();
	ba = sText.toUtf8();
	if(strlen(ba.data()) != 0) {
		config->ipaddress = strdup(ba.data());
	} else {
		config->ipaddress = NULL;
	}

	sText = settings.value("macaddress", "").toString();
	ba = sText.toUtf8();
	if(strlen(ba.data()) != 0) {
		config->macaddress = strdup(ba.data());
	} else {
		config->macaddress = NULL;
	}

	sText = settings.value("bridgename", "").toString();
	ba = sText.toUtf8();
	if(strlen(ba.data()) != 0) {
		config->bridgename = strdup(ba.data());
	} else {
		config->bridgename = NULL;
	}

	config->cpu_idle = 0;
	config->cpu_idle = settings.value("cpu_idle", "0").toInt();

	config->show_fullscreen_message = settings.value("show_fullscreen_message", "1").toInt();

	sText = settings.value("network_capture", "").toString();
	if (sText != "") {
		ba = sText.toUtf8();
		config->network_capture = strdup(ba.constData());
	} else {
		config->network_capture = NULL;
	}

	config_nat_rules_load(settings);
}


/**
 * Store the user's most recently chosen configuration to disc, for use next
 * time the program starts.
 *
 * Called on program exit.
 */
void
config_save(Config *config)
{
	char filename[512];
	QString sText;

	snprintf(filename, sizeof(filename), "%srpc.cfg", rpcemu_get_datadir());

	QSettings settings(filename, QSettings::IniFormat);
	settings.clear();

	char s[256];

	sprintf(s, "%u", config->mem_size);
	settings.setValue("mem_size", s);

	sprintf(s, "%s", models[machine.model].name_config);
	settings.setValue("model", s);

	if (config->vram_size != 0) {
		settings.setValue("vram_size", "2");
	} else {
		settings.setValue("vram_size", "0");
	}

	settings.setValue("sound_enabled",   config->soundenabled);
	settings.setValue("refresh_rate",    config->refresh);
	settings.setValue("cdrom_enabled",   config->cdromenabled);
	settings.setValue("cdrom_type",      config->cdromtype);
	settings.setValue("cdrom_iso",       config->isoname);
	settings.setValue("mouse_following", config->mousehackon);
	settings.setValue("mouse_twobutton", config->mousetwobutton);


	switch (config->network_type) {
	case NetworkType_Off:              sprintf(s, "off"); break;
	case NetworkType_NAT:              sprintf(s, "nat"); break;
	case NetworkType_EthernetBridging: sprintf(s, "ethernetbridging"); break;
	case NetworkType_IPTunnelling:     sprintf(s, "iptunnelling"); break;
	}
	settings.setValue("network_type", s);

	if (config->username) {
		settings.setValue("username", config->username);
	} else {
		settings.setValue("username", "");
	}
	if (config->ipaddress) {
		settings.setValue("ipaddress", config->ipaddress);
	} else {
		settings.setValue("ipaddress", "");
	}
	if (config->macaddress) {
		settings.setValue("macaddress", config->macaddress);
	} else {
		settings.setValue("macaddress", "");
	}
	if (config->bridgename) {
		settings.setValue("bridgename", config->bridgename);
	} else {
		settings.setValue("bridgename", "");
	}

	settings.setValue("cpu_idle", config->cpu_idle);
	settings.setValue("show_fullscreen_message", config->show_fullscreen_message);

	if (config->network_capture) {
		settings.setValue("network_capture", config->network_capture);
	}

	config_nat_rules_save(settings);
}
